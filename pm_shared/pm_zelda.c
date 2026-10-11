/***
*
*	Link's movement, ported from OoT's Player actor (z_player.c) onto Half-Life's shared
*	player physics. Runs on both the server and the client (prediction), so everything that
*	changes movement lives here and only reads/writes predicted fields:
*
*		fuser1	facing yaw (actor.shape.rot.y)
*		fuser4	time in current action
*		iuser4	ZL_STATE_* bits
*		iuser3	ZSF_* flags from the server (read only)
*
*	Camera relative control: cmd.viewangles is the camera, cmd.forwardmove/sidemove is the
*	control stick, exactly like Camera_GetInputDirYaw() + sControlStickAngle in OoT.
*
*	Speeds below are written in OoT units and converted with ZL_VEL/ZL_ACC (zl_shared.h).
*
****/

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "mathlib.h"
#include "const.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_shared.h"
#include "pm_movevars.h"
#include "zl_shared.h"
#include "zl_sounds.h"
#include "pm_zelda.h"
#include "pm_materials.h"

#ifndef PITCH
#define PITCH	0
#define YAW		1
#define ROLL	2
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern playermove_t *pmove;
void PM_CatagorizeTextureType( void );
int PM_FlyMove( void );

pmz_predicted_t g_pmzPredicted;

typedef struct pmz_s
{
	int		active;
	int		state;
	int		sflags;
	float	facing;
	float	time;

	int		pad;			// ZPAD_* currently held
	int		pressed;		// went down this move
	int		released;		// went up this move

	float	stickMag;		// 0..60 like sControlStickMagnitude
	float	stickYaw;		// world yaw the stick points at (degrees)
	int		stickDir;		// ZDIR_* relative to facing

	int		hasTarget;
	vec3_t	target;

	float	savedGravity;
	int		wasOnGround;
	float	preMoveSpeed;
} pmz_t;

static pmz_t z;

/*
=============================================================================
 Small math helpers
=============================================================================
*/
static float PMZ_AngleMod( float a )
{
	a = fmodf( a, 360.0f );
	if ( a < 0 )
		a += 360.0f;
	return a;
}

// Signed shortest difference a - b in [-180, 180]
static float PMZ_AngleDiff( float a, float b )
{
	float d = fmodf( a - b, 360.0f );
	if ( d > 180.0f )
		d -= 360.0f;
	else if ( d < -180.0f )
		d += 360.0f;
	return d;
}

// Math_ScaledStepToS for degrees per second
static float PMZ_StepAngle( float cur, float target, float degPerSec )
{
	float d = PMZ_AngleDiff( target, cur );
	float step = degPerSec * pmove->frametime;

	if ( fabsf( d ) <= step )
		return PMZ_AngleMod( target );

	return PMZ_AngleMod( cur + ( d > 0 ? step : -step ) );
}

// Math_AsymStepToF
static float PMZ_AsymStep( float cur, float target, float incPerSec, float decPerSec )
{
	float step;

	if ( cur < target )
	{
		step = incPerSec * pmove->frametime;
		cur += step;
		if ( cur > target )
			cur = target;
	}
	else if ( cur > target )
	{
		step = decPerSec * pmove->frametime;
		cur -= step;
		if ( cur < target )
			cur = target;
	}
	return cur;
}

static float PMZ_Yaw( const float *v )
{
	if ( v[0] == 0 && v[1] == 0 )
		return z.facing;
	return PMZ_AngleMod( (float)( atan2( v[1], v[0] ) * 180.0 / M_PI ) );
}

static float PMZ_Speed2D( void )
{
	return (float)sqrt( pmove->velocity[0] * pmove->velocity[0] + pmove->velocity[1] * pmove->velocity[1] );
}

static void PMZ_SetVelocity2D( float yaw, float speed )
{
	float r = yaw * (float)( M_PI / 180.0 );
	pmove->velocity[0] = cosf( r ) * speed;
	pmove->velocity[1] = sinf( r ) * speed;
}

/*
=============================================================================
 State helpers
=============================================================================
*/
static int PMZ_Action( void )
{
	return ZL_GetAction( z.state );
}

static void PMZ_SetAction( int action )
{
	z.state = ( z.state & ~( ZL_STATE_ACTION_MASK | ZL_STATE_SUB_MASK | ZL_STATE_CHARGED ) ) | action;
	z.time = 0;
}

static void PMZ_SetSub( int sub )
{
	z.state = ( z.state & ~ZL_STATE_SUB_MASK ) | ( ( sub << ZL_STATE_SUB_SHIFT ) & ZL_STATE_SUB_MASK );
}

static void PMZ_SetMWA( int mwa )
{
	z.state = ( z.state & ~ZL_STATE_MWA_MASK ) | ( ( mwa << ZL_STATE_MWA_SHIFT ) & ZL_STATE_MWA_MASK );
}

static void PMZ_SetCombo( int combo )
{
	z.state = ( z.state & ~ZL_STATE_COMBO_MASK ) | ( ( combo << ZL_STATE_COMBO_SHIFT ) & ZL_STATE_COMBO_MASK );
}

static int PMZ_IsZTargeting( void )
{
	return ( z.state & ( ZL_STATE_LOCKED | ZL_STATE_PARALLEL ) ) != 0;
}

static int PMZ_OnGround( void )
{
	return pmove->onground != -1;
}

// freq: OoT's caller side frequency scale; the sound's own random pitch (zl_sounds.h) goes on top
static void PMZ_SoundEx( int channel, int id, float vol, float freq )
{
	char path[96];
	const char *sample;

	if ( id < 0 || id >= ZS_COUNT )
		return;

	// Prediction replays commands; only the first run of each one makes noise
	if ( !pmove->runfuncs )
		return;

	sample = ZL_SoundSample( path, sizeof( path ), pmove->PM_Info_ValueForKey( pmove->physinfo, "zv" ), id,
							 pmove->RandomLong( 0, 0x7FFF ) );

	pmove->PM_PlaySound( channel, sample, vol, ATTN_NORM, 0, ZL_SoundPitch( id, freq, pmove->RandomLong( 0, 0x7FFF ) ) );
}

static void PMZ_Sound( int channel, int id, float vol )
{
	PMZ_SoundEx( channel, id, vol, 1.0f );
}

// The floor under Link as one of OoT's SURFACE_SFX_OFFSET_* (SurfaceType_GetSfxOffset), from
// the Half-Life material of the texture he stands on
static int PMZ_Surface( void )
{
	if ( pmove->waterlevel == 1 )
		return ZL_SURF_WATER;

	PM_CatagorizeTextureType();

	switch ( pmove->chtexturetype )
	{
	case CHAR_TEX_DIRT:		return ZL_SURF_DIRT;
	case CHAR_TEX_SNOW:		return ZL_SURF_SAND;
	case CHAR_TEX_SLOSH:	return ZL_SURF_WATER;
	case CHAR_TEX_WOOD:		return ZL_SURF_WOOD;
	case CHAR_TEX_GLASS:	return ZL_SURF_ICE;
	case CHAR_TEX_FLESH:	return ZL_SURF_JABU;
	default:				return ZL_SURF_STONE;	// concrete, metal, tile, vents, grates, computers
	}
}

// Player_PlayJumpingSfx / Player_PlayLandingSfx: NA_SE_PL_JUMP / NA_SE_PL_LAND + the floor
static void PMZ_FloorSound( int first, float vol )
{
	PMZ_Sound( CHAN_BODY, first + PMZ_Surface(), vol );
}

/*
=============================================================================
 Control stick
=============================================================================
*/
static void PMZ_ReadPad( void )
{
	int buttons = pmove->cmd.buttons;
	int old = ( z.state & ZL_STATE_BUTTONS_MASK ) >> ZL_STATE_BUTTONS_SHIFT;

	z.pad = 0;
	if ( buttons & ZBTN_A )			z.pad |= ZPAD_A;
	if ( buttons & ZBTN_B )			z.pad |= ZPAD_B;
	if ( buttons & ZBTN_Z )			z.pad |= ZPAD_Z;
	if ( buttons & ZBTN_R )			z.pad |= ZPAD_R;
	if ( buttons & ZBTN_CLEFT )		z.pad |= ZPAD_CLEFT;
	if ( buttons & ZBTN_CDOWN )		z.pad |= ZPAD_CDOWN;
	if ( buttons & ZBTN_CRIGHT )	z.pad |= ZPAD_CRIGHT;
	if ( buttons & ZBTN_CUP )		z.pad |= ZPAD_CUP;

	if ( z.sflags & ZSF_FROZEN )
		z.pad = 0;

	z.pressed = z.pad & ~old;
	z.released = old & ~z.pad;
}

// Player_CalcSpeedAndYawFromControlStick
static void PMZ_ReadStick( void )
{
	float f = pmove->cmd.forwardmove;
	float s = pmove->cmd.sidemove;
	float mag = (float)sqrt( f * f + s * s );
	float maxs = pmove->maxspeed > 0 ? pmove->maxspeed : 320.0f;
	float frac = mag / maxs;

	if ( frac > 1.0f )
		frac = 1.0f;

	if ( ( z.sflags & ZSF_FROZEN ) || frac < 0.02f )
	{
		z.stickMag = 0;
		z.stickYaw = z.facing;
		z.stickDir = ZDIR_NONE;
		return;
	}

	// Keyboards are all or nothing; spread whatever we get over the stick range above the
	// curved mode dead zone (20) so walk keys still creep.
	z.stickMag = 20.0f + 40.0f * frac;
	z.stickYaw = PMZ_AngleMod( pmove->cmd.viewangles[YAW] + (float)( atan2( -s, f ) * 180.0 / M_PI ) );

	{
		// controlStickDirections: quadrant relative to shape yaw
		float rel = PMZ_AngleDiff( z.stickYaw, z.facing );

		if ( rel > -45.0f && rel <= 45.0f )
			z.stickDir = ZDIR_FORWARD;
		else if ( rel > 45.0f && rel <= 135.0f )
			z.stickDir = ZDIR_LEFT;
		else if ( rel < -45.0f && rel >= -135.0f )
			z.stickDir = ZDIR_RIGHT;
		else
			z.stickDir = ZDIR_BACKWARD;
	}
}

// SPEED_MODE_CURVED target speed, in OoT units
static float PMZ_CurvedSpeed( void )
{
	float t = z.stickMag - 20.0f;
	float c, target;

	if ( t <= 0 )
		return 0;

	// Math_CosS( t * 450 )
	c = 1.0f - cosf( t * 450.0f * (float)( 2.0 * M_PI / 65536.0 ) );
	target = ( ( c * c ) * 30.0f + 7.0f ) * 0.14f;

	if ( target > ZL_RUN_SPEED )
		target = ZL_RUN_SPEED;
	return target;
}

// SPEED_MODE_LINEAR
static float PMZ_LinearSpeed( void )
{
	float target = z.stickMag * 0.8f * 0.14f;

	if ( target > ZL_RUN_SPEED )
		target = ZL_RUN_SPEED;
	return target;
}

/*
=============================================================================
 Z-targeting (Player_UpdateZTargeting / Player_UpdateHostileLockOn)

 The server picks the actor (it can see every entity), the client sends where it is
 through usercmd impact_position so both sides steer the same way.
=============================================================================
*/
static void PMZ_UpdateTargeting( void )
{
	z.hasTarget = ( pmove->cmd.impact_index == ZL_CMD_TARGET_VALID );
	VectorCopy( pmove->cmd.impact_position, z.target );

	if ( !( z.pad & ZPAD_Z ) )
	{
		z.state &= ~( ZL_STATE_LOCKED | ZL_STATE_PARALLEL );
		return;
	}

	if ( z.hasTarget )
	{
		z.state |= ZL_STATE_LOCKED;
		z.state &= ~ZL_STATE_PARALLEL;
	}
	else
	{
		// Once parallel, stay parallel until Z is released (PLAYER_STATE1_PARALLEL)
		z.state &= ~ZL_STATE_LOCKED;
		z.state |= ZL_STATE_PARALLEL;
	}
}

static float PMZ_TargetYaw( void )
{
	vec3_t d;
	VectorSubtract( z.target, pmove->origin, d );
	return PMZ_Yaw( d );
}

// Shape yaw while Z-targeting: toward the actor at 4000 binang/frame, parallel stays put
static void PMZ_FaceTarget( void )
{
	if ( z.state & ZL_STATE_LOCKED )
		z.facing = PMZ_StepAngle( z.facing, PMZ_TargetYaw(), ZL_BINANG( 4000.0f * ZL_POS_RATE * ZL_FPS ) );
}

/*
=============================================================================
 Starting actions
=============================================================================
*/
static void PMZ_StartRoll( void )
{
	PMZ_SetAction( ZACT_ROLL );
	PMZ_Sound( CHAN_BODY, ZS_PL_ROLL, 1.0f );
}

// func_8083BCD0: sidehop (left/right) or backflip
static void PMZ_StartHop( int dir )
{
	float yaw = z.facing + dir * 90.0f;
	float speed = ( dir & 1 ) ? 8.5f : 6.0f;
	float vy = ( dir & 1 ) ? 3.5f : 5.8f;

	PMZ_SetAction( ( dir & 1 ) ? ZACT_SIDEHOP : ZACT_BACKFLIP );
	PMZ_SetSub( dir );

	PMZ_SetVelocity2D( yaw, ZL_VEL( speed ) );
	pmove->velocity[2] = ZL_VEL( vy );
	pmove->onground = -1;

	// func_808389E8: the floor's jump sound and "Hup!"
	PMZ_FloorSound( ZS_PL_JUMP, 0.8f );
	PMZ_Sound( CHAN_VOICE, ZS_VO_SWORD_N, 1.0f );
	PMZ_Sound( CHAN_ITEM, ( dir == ZDIR_BACKWARD ) ? ZS_PL_ROLL : ZS_PL_SKIP, 0.8f );
}

// func_8083BA90( PLAYER_MWA_JUMPSLASH_START, 5.0, 5.0 )
static void PMZ_StartJumpSlash( void )
{
	PMZ_SetAction( ZACT_JUMPSLASH );
	PMZ_SetMWA( ZMWA_JUMPSLASH_START );

	PMZ_SetVelocity2D( z.facing, ZL_VEL( 5.0f ) );
	pmove->velocity[2] = ZL_VEL( 5.0f );
	pmove->onground = -1;

	PMZ_Sound( CHAN_VOICE, ZS_VO_SWORD_L, 1.0f );
}

// func_80837818: which swing does B give us?
static int PMZ_PickSwing( void )
{
	static const int dirTable[4] = { ZMWA_STAB_1H, ZMWA_RIGHT_SLASH_1H, ZMWA_RIGHT_SLASH_1H, ZMWA_LEFT_SLASH_1H };
	int mwa;

	if ( z.stickDir == ZDIR_NONE )
	{
		mwa = PMZ_IsZTargeting() ? ZMWA_FORWARD_SLASH_1H : ZMWA_RIGHT_SLASH_1H;
	}
	else
	{
		mwa = dirTable[z.stickDir];
		if ( mwa == ZMWA_STAB_1H && !PMZ_IsZTargeting() )
			mwa = ZMWA_FORWARD_SLASH_1H;
	}

	return mwa;
}

// func_80837948: start a swing, counting combos (unk_845)
static void PMZ_StartSlash( void )
{
	int mwa = PMZ_PickSwing();
	int lastBase = ZL_GetMWA( z.state );
	int combo = ZL_GetCombo( z.state );
	int wasSlashing = PMZ_Action() == ZACT_SLASH;

	// The third swing of the same kind turns into its finisher (arg2 += 2)
	if ( lastBase >= ZMWA_FORWARD_COMBO_1H && ( lastBase & 2 ) && lastBase <= ZMWA_STAB_COMBO_2H )
		lastBase -= 2;

	if ( !wasSlashing || mwa != lastBase || combo >= 3 )
		combo = 0;

	combo++;

	// When swinging from a standstill, face the stick first
	if ( !PMZ_IsZTargeting() && z.stickDir != ZDIR_NONE )
		z.facing = z.stickYaw;

	PMZ_SetAction( ZACT_SLASH );
	PMZ_SetCombo( combo );
	PMZ_SetMWA( combo >= 3 ? mwa + 2 : mwa );

	// Root motion lunge from the animation (ANIM_FLAG_UPDATE_XZ): stabs push forward
	if ( mwa == ZMWA_STAB_1H )
		PMZ_SetVelocity2D( z.facing, ZL_VEL( 6.0f ) );
	else
	{
		float spd = PMZ_Speed2D();
		if ( spd > ZL_VEL( 3.0f ) )
			spd = ZL_VEL( 3.0f );
		PMZ_SetVelocity2D( z.facing, spd );
	}

	PMZ_Sound( CHAN_WEAPON, combo >= 3 ? ZS_IT_SWORD_SWING_HARD : ZS_IT_SWORD_SWING, 1.0f );
	PMZ_Sound( CHAN_VOICE, combo >= 3 ? ZS_VO_SWORD_L : ZS_VO_SWORD_N, 0.8f );
}

static void PMZ_StartSpin( int big )
{
	PMZ_SetAction( ZACT_SPIN );
	PMZ_SetMWA( big ? ZMWA_BIG_SPIN_1H : ZMWA_SPIN_ATTACK_1H );
	PMZ_SetVelocity2D( z.facing, 0 );
	PMZ_Sound( CHAN_WEAPON, ZS_IT_ROLLING_CUT, 1.0f );
	PMZ_Sound( CHAN_VOICE, ZS_VO_SWORD_L, 1.0f );
}

/*
=============================================================================
 Ledge climbing (Player_ActionHandler_5 / func_8083DF68 wall checks)

 Returns the height of a climbable ledge in front of Link, or 0.
=============================================================================
*/
static float PMZ_LedgeHeight( float yaw, float *wallDist )
{
	pmtrace_t tr;
	vec3_t fwd, start, end;
	float r = yaw * (float)( M_PI / 180.0 );
	float height;

	fwd[0] = cosf( r );
	fwd[1] = sinf( r );
	fwd[2] = 0;

	// Is there a wall right in front of us?
	VectorMA( pmove->origin, 8.0f, fwd, end );
	tr = pmove->PM_PlayerTrace( pmove->origin, end, PM_NORMAL, -1 );
	if ( tr.fraction == 1.0f || tr.startsolid || fabsf( tr.plane.normal[2] ) > 0.3f )
		return 0;

	if ( wallDist )
		*wallDist = tr.fraction * 8.0f;

	// Drop a hull down onto the ledge from above
	VectorMA( pmove->origin, 20.0f, fwd, start );
	start[2] += ZL_CLIMB_MAX + 1.0f;
	VectorCopy( start, end );
	end[2] = pmove->origin[2];

	// Room above our head first
	{
		vec3_t up;
		VectorCopy( pmove->origin, up );
		up[2] += ZL_CLIMB_MAX + 1.0f;
		tr = pmove->PM_PlayerTrace( pmove->origin, up, PM_NORMAL, -1 );
		if ( tr.fraction < 1.0f )
			return 0;
	}

	tr = pmove->PM_PlayerTrace( start, end, PM_NORMAL, -1 );
	if ( tr.startsolid || tr.allsolid || tr.fraction == 1.0f || tr.plane.normal[2] < 0.7f )
		return 0;

	height = tr.endpos[2] - pmove->origin[2];
	if ( height <= pmove->movevars->stepsize || height > ZL_CLIMB_MAX )
		return 0;

	return height;
}

static int PMZ_ClimbMove( void )
{
	pmtrace_t tr;
	vec3_t fwd, end;
	float r = z.facing * (float)( M_PI / 180.0 );

	fwd[0] = cosf( r );
	fwd[1] = sinf( r );
	fwd[2] = 0;

	// Phase 1: straight up until the way forward is clear (link_normal_150step_up)
	VectorMA( pmove->origin, 12.0f, fwd, end );
	tr = pmove->PM_PlayerTrace( pmove->origin, end, PM_NORMAL, -1 );

	if ( tr.fraction < 1.0f && z.time < ZL_CLIMB_TIME )
	{
		pmove->velocity[0] = pmove->velocity[1] = 0;
		pmove->velocity[2] = ZL_CLIMB_MAX / ( ZL_CLIMB_TIME * 0.6f );
	}
	else
	{
		// Phase 2: step onto the ledge
		PMZ_SetVelocity2D( z.facing, ZL_VEL( 4.0f ) );
		pmove->velocity[2] = 0;
	}

	PM_FlyMove();

	if ( z.time >= ZL_CLIMB_TIME )
	{
		VectorClear( pmove->velocity );
		PMZ_SetAction( ZACT_STAND );
	}

	return 1;
}

/*
=============================================================================
 Ground movement for the standing action
=============================================================================
*/

// Player_Action_80840450 / _808407CC: free running, camera relative
static void PMZ_FreeMove( float speedScale )
{
	float speed = PMZ_Speed2D();
	float moveYaw = PMZ_Yaw( pmove->velocity );

	if ( z.stickMag > 0 )
	{
		float target = ZL_VEL( PMZ_CurvedSpeed() ) * speedScale;
		float d;

		if ( speed < 1.0f )
			moveYaw = z.stickYaw;

		d = PMZ_AngleDiff( z.stickYaw, moveYaw );

		if ( fabsf( d ) > 90.0f )
		{
			// Skid to a stop, then turn around (temp4 > 0x4000)
			speed = PMZ_AsymStep( speed, 0, 0, ZL_ACC( ZL_DECEL_STEP ) );
			if ( speed <= 0 )
				moveYaw = z.stickYaw;
		}
		else
		{
			// Math_ScaledStepToS( &yaw, yawTarget, |diff| * 0.1 ), scaled by 1.5 per frame
			float frac = 1.0f - powf( 1.0f - 0.15f, pmove->frametime * ZL_FPS );
			moveYaw = PMZ_AngleMod( moveYaw + d * frac );
			speed = PMZ_AsymStep( speed, target, ZL_ACC( ZL_ACCEL_STEP ), ZL_ACC( ZL_DECEL_STEP ) );
		}

		// Shape yaw follows movement yaw at 2000 binang/frame
		z.facing = PMZ_StepAngle( z.facing, moveYaw, ZL_BINANG( 2000.0f * ZL_POS_RATE * ZL_FPS ) );
	}
	else
	{
		// Player_DecelerateToZero: R_DECELERATE_RATE 8.0
		speed = PMZ_AsymStep( speed, 0, 0, ZL_ACC( 8.0f ) );
	}

	PMZ_SetVelocity2D( moveYaw, speed );
}

// Player_Action_80840DE4: Z-targeted strafing, Link keeps looking at the actor
static void PMZ_TargetMove( float speedScale )
{
	float speed = PMZ_Speed2D();
	float moveYaw = PMZ_Yaw( pmove->velocity );

	PMZ_FaceTarget();

	if ( z.stickMag > 0 )
	{
		static const float dirScale[4] = { 1.0f, 0.7f, 0.55f, 0.7f };
		float target = ZL_VEL( PMZ_LinearSpeed() ) * dirScale[z.stickDir] * speedScale;

		moveYaw = z.stickYaw;
		speed = PMZ_AsymStep( speed, target, ZL_ACC( 1.5f ), ZL_ACC( 1.5f ) );
	}
	else
	{
		speed = PMZ_AsymStep( speed, 0, 0, ZL_ACC( 8.0f ) );
	}

	PMZ_SetVelocity2D( moveYaw, speed );
}

static void PMZ_Decelerate( float stepPerFrame )
{
	float speed = PMZ_Speed2D();
	float yaw = PMZ_Yaw( pmove->velocity );
	speed = PMZ_AsymStep( speed, 0, 0, ZL_ACC( stepPerFrame ) );
	PMZ_SetVelocity2D( yaw, speed );
}

/*
=============================================================================
 Per action logic. Everything below runs before HL's own walk/air move, which then does
 the collision. Velocity xy is fully owned by Link, cmd moves are zeroed so HL's
 accelerate code leaves it alone.
=============================================================================
*/
static int PMZ_CanAct( void )
{
	return !( z.sflags & ( ZSF_FROZEN | ZSF_HOOKSHOT ) );
}

// Player_ActionHandler_10 / _Roll: what does A do right now?
static int PMZ_HandleA( void )
{
	if ( !( z.pressed & ZPAD_A ) || !PMZ_CanAct() || !PMZ_OnGround() )
		return 0;

	// The server owns talking / opening / throwing
	if ( z.sflags & ( ZSF_DOACTION | ZSF_HOLDING ) )
		return 0;

	if ( PMZ_IsZTargeting() )
	{
		if ( z.stickDir <= ZDIR_FORWARD )
		{
			// Only with the sword in hand (Player_GetMeleeWeaponHeld); sheathed, it's a roll
			if ( ( z.sflags & ZSF_HASSWORD ) && ( z.sflags & ZSF_SWORDOUT ) )
				PMZ_StartJumpSlash();
			else
				PMZ_StartRoll();
		}
		else
		{
			PMZ_StartHop( z.stickDir );
		}
		return 1;
	}

	// Player_TryRoll: needs the stick pushed
	if ( z.stickMag > 0 )
	{
		if ( fabsf( PMZ_AngleDiff( z.stickYaw, z.facing ) ) < 100.0f )
			z.facing = z.stickYaw;
		PMZ_StartRoll();
		return 1;
	}

	return 0;
}

static int PMZ_HandleB( void )
{
	if ( !( z.pressed & ZPAD_B ) || !PMZ_CanAct() || !( z.sflags & ZSF_HASSWORD ) )
		return 0;
	if ( z.sflags & ZSF_HOLDING )
		return 0;

	if ( !PMZ_OnGround() )
		return 0;

	PMZ_StartSlash();
	return 1;
}

static int PMZ_AimButton( void )
{
	if ( ( z.sflags & ZSF_AIM_CLEFT ) && ( z.pad & ZPAD_CLEFT ) )
		return 0;
	if ( ( z.sflags & ZSF_AIM_CDOWN ) && ( z.pad & ZPAD_CDOWN ) )
		return 1;
	if ( ( z.sflags & ZSF_AIM_CRIGHT ) && ( z.pad & ZPAD_CRIGHT ) )
		return 2;
	return -1;
}

static void PMZ_ActStand( void )
{
	int aim;

	// Leaving the ground without jumping (walked off something short enough not to auto jump)
	if ( !PMZ_OnGround() )
	{
		PMZ_SetAction( ZACT_FALL );
		return;
	}

	if ( PMZ_HandleA() || PMZ_HandleB() )
		return;

	if ( PMZ_CanAct() )
	{
		// Shield (Player_ActionHandler_11): standing guard unless Z-targeting
		if ( ( z.pad & ZPAD_R ) && ( z.sflags & ZSF_HASSHIELD ) && !( z.sflags & ZSF_HOLDING ) )
		{
			if ( !PMZ_IsZTargeting() )
			{
				PMZ_SetAction( ZACT_SHIELD );
				PMZ_Sound( CHAN_ITEM, ZS_IT_SHIELD_POSTURE, 0.7f );
				return;
			}
			z.state |= ZL_STATE_SHIELDING;
		}

		// First person aim with a bow / hookshot on a C button
		aim = PMZ_AimButton();
		if ( aim >= 0 && !PMZ_IsZTargeting() )
		{
			PMZ_SetAction( ZACT_AIM );
			PMZ_SetSub( aim );
			z.facing = PMZ_AngleMod( pmove->cmd.viewangles[YAW] );
			return;
		}

		if ( ( z.pressed & ZPAD_CUP ) && !PMZ_IsZTargeting() )
		{
			PMZ_SetAction( ZACT_CUP );
			z.facing = PMZ_AngleMod( pmove->cmd.viewangles[YAW] );
			return;
		}
	}

	if ( PMZ_IsZTargeting() )
	{
		PMZ_TargetMove( ( z.state & ZL_STATE_SHIELDING ) ? 0.6f : 1.0f );
		z.time = 0;
		return;
	}

	PMZ_FreeMove( ( z.sflags & ZSF_HOLDING ) ? 0.8f : 1.0f );

	// Ledge climb: push into a ledge for ledgeClimbDelayTimer frames (or press A)
	{
		float rel = PMZ_AngleDiff( z.stickYaw, z.facing );
		float height = 0;

		if ( z.stickMag > 0 && fabsf( rel ) < 35.0f && !( z.sflags & ZSF_HOLDING ) )
			height = PMZ_LedgeHeight( z.facing, NULL );

		if ( height > 0 )
		{
			// fuser4 doubles as the push timer while standing
			if ( z.time >= ZL_LEDGE_PUSH_TIME || ( z.pressed & ZPAD_A ) )
			{
				PMZ_SetAction( ZACT_CLIMB );
				PMZ_Sound( CHAN_VOICE, ZS_VO_CLIMB_END, 1.0f );
				PMZ_Sound( CHAN_BODY, ZS_PL_CLIMB_CLIFF, 0.8f );
			}
		}
		else
		{
			z.time = 0;
		}
	}
}

static void PMZ_ActRoll( void )
{
	float target;

	if ( z.time >= ZL_ROLL_TIME )
	{
		PMZ_SetAction( PMZ_OnGround() ? ZACT_STAND : ZACT_FALL );
		return;
	}

	// Player_ActionHandler_7 after frame 15: roll again
	if ( z.time >= ZL_ROLL_CHAIN_TIME && PMZ_OnGround() && ( z.pressed & ZPAD_A ) && z.stickMag > 0 )
	{
		PMZ_StartRoll();
		return;
	}

	// Rolling speed is 1.5x the walk speed for the stick, at least 3
	target = PMZ_CurvedSpeed() * 1.5f;
	if ( target < 3.0f || z.stickDir != ZDIR_FORWARD )
		target = 3.0f;

	{
		float speed = PMZ_AsymStep( PMZ_Speed2D(), ZL_VEL( target ), ZL_ACC( ZL_ACCEL_STEP ), ZL_ACC( 1.5f ) );
		PMZ_SetVelocity2D( z.facing, speed );
		z.preMoveSpeed = speed;
	}
}

static void PMZ_ActBonk( void )
{
	PMZ_Decelerate( 2.0f );
	if ( z.time >= ZL_BONK_TIME )
		PMZ_SetAction( ZACT_STAND );
}

static void PMZ_ActAir( void )
{
	// Horizontal speed is kept through the jump (speedXZ is not touched by 8084411C)
	if ( PMZ_OnGround() && z.time > 0.05f )
	{
		int act = PMZ_Action();

		if ( pmove->flFallVelocity >= ZL_MAX_FALL_SPEED * 0.95f )
		{
			PMZ_SetAction( ZACT_LAND );
			PMZ_SetSub( 1 );	// hard landing
			PMZ_SetMWA( 0 );
			PMZ_Sound( CHAN_VOICE, ZS_VO_LAND_DAMAGE_S, 1.0f );
		}
		else if ( act == ZACT_SIDEHOP || act == ZACT_BACKFLIP )
		{
			// D_80853D4C[dir][1 or 2]: each hop has its own landing; remember which (mwa = dir + 1)
			int dir = ZL_GetSub( z.state );
			PMZ_SetAction( ZACT_LAND );
			PMZ_SetMWA( dir + 1 );
		}
		else
		{
			PMZ_SetAction( ZACT_STAND );
		}
		PMZ_FloorSound( ZS_PL_LAND, 0.6f );
	}
}

static void PMZ_ActLand( void )
{
	float t = ( ZL_GetSub( z.state ) == 1 ) ? ZL_HARD_LAND_TIME : ZL_LAND_TIME;
	int hop = ZL_GetMWA( z.state );		// landing out of a hop: dir + 1

	if ( hop == ZDIR_BACKWARD + 1 )
		t = ( z.sflags & ZSF_HOSTILE ) ? ZL_FLIP_LAND_TIME_R : ZL_FLIP_LAND_TIME;
	else if ( hop )
		t = ZL_HOP_LAND_TIME;

	// func_80843E64: Math_StepToF( &speedXZ, 0, 1.0 )
	PMZ_Decelerate( 1.0f );

	if ( z.time >= t || ( z.time > 0.1f && ( PMZ_HandleA() || PMZ_HandleB() ) ) )
	{
		if ( PMZ_Action() == ZACT_LAND )
			PMZ_SetAction( ZACT_STAND );
	}
}

static void PMZ_ActSlash( void )
{
	// Movement during the swing is root motion only
	PMZ_Decelerate( 1.0f );

	if ( z.time >= ZL_SLASH_COMBO_TIME && ( z.pressed & ZPAD_B ) )
	{
		PMZ_StartSlash();
		return;
	}

	if ( z.time >= ZL_SLASH_TIME )
	{
		// Holding B after a swing charges the spin attack (func_8083C50C / unk_844)
		if ( ( z.pad & ZPAD_B ) && ( z.sflags & ZSF_HASSWORD ) )
		{
			PMZ_SetAction( ZACT_SPIN_CHARGE );
			PMZ_Sound( CHAN_ITEM, ZS_IT_SWORD_CHARGE, 0.5f );
		}
		else
		{
			PMZ_SetAction( ZACT_STAND );
		}
	}
}

static void PMZ_ActJumpSlash( void )
{
	if ( PMZ_OnGround() && z.time > 0.05f )
	{
		PMZ_SetAction( ZACT_JUMPSLASH_END );
		PMZ_SetMWA( ZMWA_JUMPSLASH_FINISH );
		PMZ_Sound( CHAN_WEAPON, ZS_IT_SWORD_SWING_HARD, 1.0f );
	}
}

static void PMZ_ActJumpSlashEnd( void )
{
	PMZ_Decelerate( 2.0f );
	if ( z.time >= ZL_JUMPSLASH_END_TIME )
		PMZ_SetAction( ZACT_STAND );
}

static void PMZ_ActSpinCharge( void )
{
	// power_kiru_walk: slow, facing locked
	float speed = PMZ_Speed2D();
	float target = 0;

	if ( z.stickMag > 0 )
	{
		target = ZL_VEL( PMZ_CurvedSpeed() ) * 0.3f;
		if ( target > ZL_VEL( 2.0f ) )
			target = ZL_VEL( 2.0f );
	}

	speed = PMZ_AsymStep( speed, target, ZL_ACC( 1.0f ), ZL_ACC( 2.0f ) );
	PMZ_SetVelocity2D( z.stickMag > 0 ? z.stickYaw : PMZ_Yaw( pmove->velocity ), speed );
	PMZ_FaceTarget();

	if ( z.time >= ZL_SPIN_CHARGE_TIME )
		z.state |= ZL_STATE_CHARGED;

	if ( !( z.pad & ZPAD_B ) )
	{
		if ( z.state & ZL_STATE_CHARGED )
			PMZ_StartSpin( z.time >= ZL_SPIN_BIG_TIME && ( z.sflags & ZSF_MAGIC ) );
		else
			PMZ_SetAction( ZACT_STAND );
	}
}

static void PMZ_ActSpin( void )
{
	// HL player models have no spin; turn the whole body once around instead
	z.facing = PMZ_AngleMod( z.facing - 360.0f * pmove->frametime / ZL_SPIN_TIME );
	PMZ_Decelerate( 4.0f );

	if ( z.time >= ZL_SPIN_TIME )
		PMZ_SetAction( ZACT_STAND );
}

static void PMZ_ActShield( void )
{
	PMZ_Decelerate( 8.0f );
	z.state |= ZL_STATE_SHIELDING;

	if ( !( z.pad & ZPAD_R ) || PMZ_IsZTargeting() || !PMZ_CanAct() )
	{
		z.state &= ~ZL_STATE_SHIELDING;
		PMZ_SetAction( ZACT_STAND );
	}
}

static void PMZ_ActAim( void )
{
	static const int padBits[3] = { ZPAD_CLEFT, ZPAD_CDOWN, ZPAD_CRIGHT };
	int sub = ZL_GetSub( z.state );

	PMZ_Decelerate( 8.0f );
	z.facing = PMZ_AngleMod( pmove->cmd.viewangles[YAW] );

	// Fired (by the server) or cancelled when the C button comes back up
	if ( sub > 2 || !( z.pad & padBits[sub] ) || ( z.pressed & ( ZPAD_A | ZPAD_B ) ) || PMZ_IsZTargeting() )
		PMZ_SetAction( ZACT_STAND );
}

static void PMZ_ActCUp( void )
{
	PMZ_Decelerate( 8.0f );
	z.facing = PMZ_AngleMod( pmove->cmd.viewangles[YAW] );

	if ( z.time > 0.1f && ( z.pressed & ( ZPAD_CUP | ZPAD_A | ZPAD_B | ZPAD_Z | ZPAD_CLEFT | ZPAD_CDOWN | ZPAD_CRIGHT ) ) )
		PMZ_SetAction( ZACT_STAND );
}

static void PMZ_ActHurt( void )
{
	int act = PMZ_Action();

	if ( PMZ_OnGround() )
		PMZ_Decelerate( act == ZACT_KNOCKDOWN ? 0.8f : 1.5f );

	if ( z.time >= ( act == ZACT_KNOCKDOWN ? ZL_KNOCKDOWN_TIME : ZL_DAMAGE_TIME ) )
		PMZ_SetAction( ZACT_STAND );
}

/*
=============================================================================
 Entry points
=============================================================================
*/
int PMZ_Active( void )
{
	return z.active;
}

/*
 Half-Life only lets go of the floor above 180 units/s upward (PM_CatagorizePosition), and
 snaps anything within 2 units back down. OoT's hops are slower than that (a side hop is 3.5,
 111 units/s; the jump attack 5.0, 159), so without this they never leave the ground: the
 floor zeroes their upward speed and Link slides along it.
*/
int PMZ_Airborne( void )
{
	int act;

	if ( !z.active || pmove->velocity[2] <= 0 )
		return 0;

	act = PMZ_Action();
	return act == ZACT_SIDEHOP || act == ZACT_BACKFLIP || act == ZACT_JUMP || act == ZACT_JUMPSLASH ||
		   act == ZACT_FALL;
}

void PMZ_Begin( void )
{
	memset( &z, 0, sizeof( z ) );

	z.sflags = pmove->iuser3;
	z.active = ( z.sflags & ZSF_ACTIVE ) && !pmove->spectator && pmove->iuser1 == 0;

	if ( !z.active )
		return;

	z.state = pmove->iuser4;
	z.facing = PMZ_AngleMod( pmove->fuser1 );
	z.time = pmove->fuser4 + pmove->cmd.msec * 0.001f;

	if ( PMZ_Action() == ZACT_NONE )
	{
		PMZ_SetAction( ZACT_STAND );
		z.facing = PMZ_AngleMod( pmove->cmd.viewangles[YAW] );
	}

	if ( pmove->dead && PMZ_Action() != ZACT_DEAD )
		PMZ_SetAction( ZACT_DEAD );

	z.wasOnGround = ( z.state & ZL_STATE_ONGROUND ) != 0;

	// Read input here rather than in PMZ_Think so water / ladder moves keep button edges right
	PMZ_ReadPad();
	PMZ_ReadStick();
	PMZ_UpdateTargeting();

	// OoT gravity (REG(68)) instead of sv_gravity, still scaled by trigger_gravity etc.
	z.savedGravity = pmove->gravity;
	pmove->gravity = ( pmove->gravity ? pmove->gravity : 1.0f ) * ( ZL_GRAVITY / pmove->movevars->gravity );

	// -1.2 instead of -1.0: always in the jump attack (Player_Action_80844AF4), and in any other
	// jump while locked onto an enemy (Player_Action_8084411C, Player_CheckHostileLockOn)
	{
		int act = PMZ_Action();
		if ( act == ZACT_JUMPSLASH || ( ( z.sflags & ZSF_HOSTILE ) &&
			 ( act == ZACT_SIDEHOP || act == ZACT_BACKFLIP || act == ZACT_JUMP || act == ZACT_FALL ) ) )
			pmove->gravity *= 1.2f;
	}
}

int PMZ_Think( void )
{
	int act;

	pmove->frametime = pmove->cmd.msec * 0.001f;

	z.state &= ~ZL_STATE_SHIELDING;
	z.preMoveSpeed = 0;

	act = PMZ_Action();

	// Hookshot chain out: Link stands still holding it
	if ( ( z.sflags & ZSF_HOOKSHOT ) && act != ZACT_HOOKFLY )
	{
		PMZ_Decelerate( 8.0f );
		pmove->cmd.forwardmove = pmove->cmd.sidemove = pmove->cmd.upmove = 0;
		return 0;
	}

	switch ( act )
	{
	case ZACT_STAND:		PMZ_ActStand(); break;
	case ZACT_ROLL:			PMZ_ActRoll(); break;
	case ZACT_BONK:			PMZ_ActBonk(); break;
	case ZACT_SIDEHOP:
	case ZACT_BACKFLIP:
	case ZACT_JUMP:
	case ZACT_FALL:			PMZ_ActAir(); break;
	case ZACT_LAND:			PMZ_ActLand(); break;
	case ZACT_SLASH:		PMZ_ActSlash(); break;
	case ZACT_JUMPSLASH:	PMZ_ActJumpSlash(); break;
	case ZACT_JUMPSLASH_END: PMZ_ActJumpSlashEnd(); break;
	case ZACT_SPIN_CHARGE:	PMZ_ActSpinCharge(); break;
	case ZACT_SPIN:			PMZ_ActSpin(); break;
	case ZACT_SHIELD:		PMZ_ActShield(); break;
	case ZACT_AIM:			PMZ_ActAim(); break;
	case ZACT_CUP:			PMZ_ActCUp(); break;
	case ZACT_DAMAGE:
	case ZACT_KNOCKDOWN:	PMZ_ActHurt(); break;

	case ZACT_CLIMB:
		pmove->cmd.forwardmove = pmove->cmd.sidemove = pmove->cmd.upmove = 0;
		return PMZ_ClimbMove();

	case ZACT_HOOKFLY:
		// The server steers velocity toward the hook; fly straight with no gravity
		pmove->cmd.forwardmove = pmove->cmd.sidemove = pmove->cmd.upmove = 0;
		PM_FlyMove();
		return 1;

	case ZACT_GETITEM:
		// link_demo_get_itemB (picked up, not out of a chest): turn to face the camera,
		// Math_ScaledStepToS( shape.rot.y, camYaw + 0x8000, 4000 ) a frame
		if ( ZL_GetSub( z.state ) == 0 )
			z.facing = PMZ_StepAngle( z.facing, pmove->cmd.viewangles[YAW] + 180.0f, 4000.0f * ( 360.0f / 65536.0f ) * 20.0f );
		PMZ_Decelerate( 8.0f );
		break;

	case ZACT_TALK:
	case ZACT_CHEST:
	case ZACT_DEAD:
	default:
		PMZ_Decelerate( 8.0f );
		break;
	}

	// Link owns the horizontal velocity now
	pmove->cmd.forwardmove = pmove->cmd.sidemove = pmove->cmd.upmove = 0;
	return 0;
}

/*
 Player_PlaySteppingSfx / func_8084029C: a step phase runs over the 29 frame walk cycle at
 (speed * 0.3 + 1) frames per game frame (x1.5 for the 20 Hz update, at most 7.25) and a step
 plays as it passes frames 10 and 24. func_800F3F84 then sets the step's frequency
 0.9 .. 1.1 and volume 0.775 .. 1.0 from the speed, and NA_SE_PL_WALK_GROUND's table entry adds
 up to +16% random pitch. The phase lives in fuser2 (predicted, see zelda/delta.lst).
*/
static void PMZ_Footsteps( int act, int onground )
{
	float spd, rate, phase, next;
	int i;
	static const float stepFrames[2] = { 10.0f, 24.0f };

	if ( !onground || ( act != ZACT_STAND && act != ZACT_SHIELD && act != ZACT_SPIN_CHARGE ) || pmove->waterlevel >= 2 )
		return;

	spd = (float)sqrt( pmove->velocity[0] * pmove->velocity[0] + pmove->velocity[1] * pmove->velocity[1] ) / ZL_VEL( 1.0f );
	if ( spd < 0.3f )
		return;

	rate = ( spd * 0.3f + 1.0f ) * 1.5f;
	if ( rate > 7.25f )
		rate = 7.25f;
	rate *= pmove->cmd.msec * 0.001f * 20.0f;

	phase = pmove->fuser2;
	if ( phase < 0 || phase >= 29.0f )
		phase = 0;
	next = phase + rate;

	for ( i = 0; i < 2; i++ )
	{
		float t = stepFrames[i];
		if ( ( phase < t && next >= t ) || ( phase < t + 29.0f && next >= t + 29.0f ) )
		{
			float r = spd > 6.0f ? 1.0f : spd / 6.0f;
			float freq = spd > 6.0f ? 1.1f : r * 0.2f + 0.9f;
			float vol = spd > 6.0f ? 1.0f : r * 0.225f + 0.775f;
			// Player_PlaySteppingSfx: NA_SE_PL_WALK_GROUND + the floor's offset
			PMZ_SoundEx( CHAN_BODY, ZS_PL_WALK_GROUND + PMZ_Surface(), vol * 0.7f, freq );
		}
	}

	while ( next >= 29.0f )
		next -= 29.0f;
	pmove->fuser2 = next;
}

void PMZ_PostMove( void )
{
	int act = PMZ_Action();
	int onground = PMZ_OnGround();

	PMZ_Footsteps( act, onground );

	// Clamp to minVelocityY
	if ( pmove->velocity[2] < -ZL_MAX_FALL_SPEED )
		pmove->velocity[2] = -ZL_MAX_FALL_SPEED;

	// func_8083A4A8: ran off a ledge, jump automatically
	if ( act == ZACT_STAND && z.wasOnGround && !onground && pmove->velocity[2] <= 0 && !PMZ_IsZTargeting() )
	{
		pmtrace_t tr;
		vec3_t end;
		float speedOoT = PMZ_Speed2D() / ZL_VEL( 1.0f );

		VectorCopy( pmove->origin, end );
		end[2] -= 40.0f;
		tr = pmove->PM_PlayerTrace( pmove->origin, end, PM_NORMAL, -1 );

		if ( tr.fraction == 1.0f && speedOoT > 1.0f )
		{
			// IREG(66..69): fast runs give the full 7.5, walking gives a hop
			float vy = ( speedOoT > 5.9f ) ? 7.5f : ( 1.25f + 0.2f * speedOoT );
			pmove->velocity[2] = ZL_VEL( vy );
			PMZ_SetAction( ZACT_JUMP );
			PMZ_FloorSound( ZS_PL_JUMP, 0.8f );		// func_80838940
			PMZ_Sound( CHAN_VOICE, ZS_VO_AUTO_JUMP, 1.0f );
		}
		else
		{
			PMZ_SetAction( ZACT_FALL );
		}
	}

	// Rolled into a wall at speed >= 7: bonk (Player_Action_Roll -> av2.bonked)
	if ( act == ZACT_ROLL && z.preMoveSpeed >= ZL_VEL( 7.0f ) && onground )
	{
		float after = PMZ_Speed2D();

		if ( after < z.preMoveSpeed * 0.35f && PMZ_LedgeHeight( z.facing, NULL ) == 0 )
		{
			pmtrace_t tr;
			vec3_t end;
			float r = z.facing * (float)( M_PI / 180.0 );

			end[0] = pmove->origin[0] + cosf( r ) * 4.0f;
			end[1] = pmove->origin[1] + sinf( r ) * 4.0f;
			end[2] = pmove->origin[2];
			tr = pmove->PM_PlayerTrace( pmove->origin, end, PM_NORMAL, -1 );

			if ( tr.fraction < 1.0f && fabsf( tr.plane.normal[2] ) < 0.3f )
			{
				PMZ_SetAction( ZACT_BONK );
				PMZ_SetVelocity2D( z.facing, -z.preMoveSpeed );
				PMZ_Sound( CHAN_BODY, ZS_PL_BODY_HIT, 1.0f );
				PMZ_Sound( CHAN_VOICE, ZS_VO_CLIMB_END, 1.0f );
				pmove->punchangle[0] = -6.0f;
			}
		}
	}

	if ( onground )
		z.state |= ZL_STATE_ONGROUND;
	else
		z.state &= ~ZL_STATE_ONGROUND;
}

void PMZ_End( void )
{
	if ( !z.active )
	{
		// Leaving Link mode: clear so it starts fresh next time
		if ( pmove->iuser4 )
			pmove->iuser4 = 0;
		return;
	}

	pmove->gravity = z.savedGravity;

	// Swimming / ladders run HL's movement; drop out of anything ground based
	if ( pmove->waterlevel >= 2 || pmove->movetype == MOVETYPE_FLY )
	{
		int act = PMZ_Action();
		if ( act != ZACT_STAND && act != ZACT_DEAD && act != ZACT_TALK && act != ZACT_HOOKFLY && act != ZACT_GETITEM && act != ZACT_CHEST )
			PMZ_SetAction( ZACT_STAND );
	}

	z.state = ( z.state & ~ZL_STATE_BUTTONS_MASK ) | ( ( z.pad << ZL_STATE_BUTTONS_SHIFT ) & ZL_STATE_BUTTONS_MASK );

	// fuser4 is networked with 22 bits at 1/128: keep it bounded
	if ( z.time > 1000.0f )
		z.time = 1000.0f;

	pmove->iuser4 = z.state;
	pmove->fuser1 = z.facing;
	pmove->fuser4 = z.time;

	if ( !pmove->server )
	{
		g_pmzPredicted.valid = 1;
		g_pmzPredicted.state = z.state;
		g_pmzPredicted.sflags = z.sflags;
		g_pmzPredicted.facing = z.facing;
		g_pmzPredicted.time = z.time;
		VectorCopy( pmove->origin, g_pmzPredicted.origin );
		VectorCopy( pmove->velocity, g_pmzPredicted.velocity );
		g_pmzPredicted.onground = pmove->onground != -1;
	}
}
