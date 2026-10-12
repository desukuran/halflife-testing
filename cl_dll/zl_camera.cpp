/***
*
*	Zelda 64 port: the camera (z_camera.c).
*
*	The engine's view angles are the camera's angles. The mouse orbits the camera, the
*	camera's own logic swings it behind Link, and whatever it ends up at is written back with
*	SetViewAngles, so the next usercmd carries the camera yaw. pm_zelda uses that yaw for
*	camera relative control, exactly like Camera_GetInputDirYaw.
*
*	Modes, named after the camera functions they copy:
*		NORMAL		Camera_Normal1   drags behind Link, swings round when he runs sideways
*		PARALLEL	Camera_Parallel1 Z with nothing targeted: snap behind Link
*		BATTLE		Camera_Battle1   locked on an enemy: behind Link, looking past him at it
*		KEEPON		Camera_KeepOn1   locked on someone friendly: closer
*		TALK		Camera_KeepOn3   talking: frames Link and whoever is talking
*		SUBJECT		Camera_Subj3     first person bow / hookshot / C-up
*
*	Distances come from sSetNormal0Mode*Data (z_camera_data.inc.c), scaled by
*	CAM_DATA_SCALED( playerHeight 68 ) and then ZL_UNIT.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "pm_zelda.h"
#include "zl_client.h"
#include "studio.h"
#include "r_studioint.h"
#include "event_api.h"
#include "entity_types.h"
#include "triangleapi.h"
#include "zl_sounds.h"

#include <math.h>

extern engine_studio_api_t IEngineStudio;

extern vec3_t v_angles, v_origin;	// view.cpp

enum
{
	ZCAM_NORMAL,
	ZCAM_PARALLEL,
	ZCAM_BATTLE,
	ZCAM_KEEPON,
	ZCAM_TALK,
	ZCAM_SUBJECT,
	ZCAM_CHEST,
	ZCAM_DOOR,
};

// CAM_FUNCDATA_* values * 0.68 (68 / 100) * ZL_UNIT
#define CAM_SCALE			( 0.68f * ZL_UNIT )
#define NORM1_DIST_MIN		( 200.0f * CAM_SCALE )
#define NORM1_DIST_MAX		( 300.0f * CAM_SCALE )
#define NORM1_PITCH			10.0f
#define PARA1_DIST			( 250.0f * CAM_SCALE )
#define BATT1_DIST			( 180.0f * CAM_SCALE )
#define KEEP1_DIST			( 130.0f * CAM_SCALE )
#define CAM_AT_HEIGHT		18.0f	// above the hull centre: Link's height + yOffset (-20)

struct zl_camera_t
{
	int		initialized;
	int		mode;
	int		lastMode;
	float	modeTime;

	Vector	eye;
	Vector	lookAt;
	float	atZ;			// smoothed vertical follow (rwData->yOffset)

	float	orbitYaw;		// direction from Link to the eye
	float	orbitPitch;		// elevation of the eye, degrees
	float	dist;

	float	viewYaw;		// what we last wrote with SetViewAngles
	float	viewPitch;
	float	lastMouseTime;

	float	letterbox;		// current, in 240 line units
	int		thirdPerson;
	int		firstPerson;
};

static zl_camera_t s_cam;

// Mouse movement waiting for the next frame, from ZInput (see ZCam_AddMouse)
static float s_pendingYaw, s_pendingPitch;

/*
 The camera owns the view angles, but it can't hand them to the engine: SetViewAngles from
 V_CalcRefdef doesn't reliably reach the next usercmd. Instead ZInput_CreateMove reads how far
 the mouse moved the engine's angles since the last command (ZCam_AddMouse) and writes the
 camera's angles straight into the usercmd (ZCam_CommandAngles), which is where pm_zelda reads
 "which way is up on the stick" (Camera_GetInputDirYaw in OoT).
*/
void ZCam_AddMouse( float yaw, float pitch )
{
	s_pendingYaw += yaw;
	s_pendingPitch += pitch;
}

int ZCam_CommandAngles( float *angles )
{
	if ( !s_cam.initialized || !ZL_IsLink() )
		return 0;
	angles[0] = s_cam.viewPitch;
	angles[1] = s_cam.viewYaw;
	angles[2] = 0;
	return 1;
}

static cvar_t *zl_cam_style;
static cvar_t *zl_cam_follow;
static cvar_t *zl_cam_mouse;
static cvar_t *zl_cam_dist;

void ZCam_Init( void )
{
	memset( &s_cam, 0, sizeof( s_cam ) );
	zl_cam_style = CVAR_CREATE( "zl_cam_style", "1", FCVAR_ARCHIVE );	// 1 mouse camera, 0 OoT camera (drags / swings by itself)
	zl_cam_follow = CVAR_CREATE( "zl_cam_follow", "1", FCVAR_ARCHIVE );	// OoT camera: auto swing behind Link
	zl_cam_mouse = CVAR_CREATE( "zl_cam_mouse", "1", FCVAR_ARCHIVE );	// mouse orbits the camera
	zl_cam_dist = CVAR_CREATE( "zl_cam_dist", "1", FCVAR_ARCHIVE );		// distance multiplier
}

int ZCam_IsThirdPerson( void )
{
	return ZL_IsLink() && s_cam.thirdPerson;
}

int ZCam_IsFirstPerson( void )
{
	return ZL_IsLink() && s_cam.firstPerson;
}

float ZCam_ViewPitch( void )
{
	return s_cam.viewPitch;
}

float ZCam_Letterbox( void )
{
	return s_cam.letterbox;
}

static float ZCam_AngleDiff( float a, float b )
{
	float d = fmodf( a - b, 360.0f );
	if ( d > 180.0f )
		d -= 360.0f;
	else if ( d < -180.0f )
		d += 360.0f;
	return d;
}

static float ZCam_Approach( float cur, float target, float rate, float dt )
{
	float k = 1.0f - expf( -rate * dt );
	return cur + ( target - cur ) * k;
}

static float ZCam_ApproachAngle( float cur, float target, float rate, float dt )
{
	float k = 1.0f - expf( -rate * dt );
	return cur + ZCam_AngleDiff( target, cur ) * k;
}

static Vector ZCam_OrbitPoint( const Vector &at, float yaw, float pitch, float dist )
{
	float y = yaw * ( M_PI / 180.0f );
	float p = pitch * ( M_PI / 180.0f );
	return at + Vector( cosf( y ) * cosf( p ), sinf( y ) * cosf( p ), sinf( p ) ) * dist;
}

// Camera_BGCheck: pull the eye in front of walls
static Vector ZCam_Collide( const Vector &at, const Vector &eye )
{
	pmtrace_t *tr = gEngfuncs.PM_TraceLine( (float *)&at.x, (float *)&eye.x, PM_TRACELINE_PHYSENTSONLY, 2, -1 );
	if ( !tr || tr->fraction >= 1.0f )
		return eye;

	Vector dir = eye - at;
	float len = dir.Length();
	if ( len < 1 )
		return eye;
	dir = dir / len;

	float hit = max( 8.0f, tr->fraction * len - 6.0f );
	return at + dir * hit;
}

/*
=============================================================================
 Camera_Demo3 (CAM_SET_SLOW_CHEST_CS): opening a chest

 Run at OoT's 20 frames a second and drawn interpolated. It starts low beside Link looking up
 at him, swings round behind him over about seven seconds while the lid opens, rises to his
 head and pushes in over his shoulder as the light comes out, then pulls back to 80 units and
 holds there until he has put the item away. Distances are OoT units (x ZL_UNIT); OoT's
 geographic vectors (r, pitch from the horizon, yaw) are ZCam_OrbitPoint's.
=============================================================================
*/
struct zl_geo_t { float r, pitch, yaw; };

// D_8011D658 / D_8011D678 (z_camera_data.inc.c), binangs as degrees
static const zl_geo_t sDemo3Eye[4] =
{
	{ 50.0f, -25.0f, -60.0f },	// { 50, 0xEE3A, 0xD558 }
	{ 75.0f, 0.0f, -180.0f },	// { 75, 0x0000, 0x8008 }
	{ 80.0f, -25.0f, -180.0f },	// { 80, 0xEE3A, 0x8008 }
	{ 15.0f, -25.0f, -180.0f },	// { 15, 0xEE3A, 0x8008 }
};
static const float sDemo3At[4][2] =		// (forward, up) from Link's feet
{
	{ 20.0f, 40.0f },
	{ 0.0f, 40.0f },
	{ -3.0f, 3.0f },
	{ -3.0f, 3.0f },
};
#define DEMO3_PLAYER_HEIGHT		68.0f	// Player_GetHeight, adult

struct zl_demo3_t
{
	int		active;
	int		frame;			// rwData->animFrame
	float	accum;
	float	yaw;			// Link's facing when it started
	float	yawDir;
	float	step;			// rwData->unk_0C
	Vector	feet;			// rwData->initialAt
	Vector	at, eye, prevAt, prevEye;
};

static zl_demo3_t s_demo3;
static int s_demo3Flip;

static float ZCam_Lerp( float a, float b, float t ) { return a + ( b - a ) * t; }

static zl_geo_t ZCam_ToGeo( const Vector &v )
{
	zl_geo_t g;
	g.r = v.Length();
	g.pitch = ( g.r > 0.001f ) ? asinf( max( -1.0f, min( 1.0f, v.z / g.r ) ) ) * ( 180.0f / M_PI ) : 0.0f;
	g.yaw = atan2f( v.y, v.x ) * ( 180.0f / M_PI );
	return g;
}

// Camera_LERPCeilF / Camera_LERPCeilS: step toward the target, snap once close
static float ZCam_LerpCeil( float target, float cur, float scale, float minDiff, int angle )
{
	float diff = angle ? ZCam_AngleDiff( target, cur ) : target - cur;
	if ( fabsf( diff ) < minDiff )
		return target;
	return cur + diff * scale;
}

static Vector ZCam_Demo3At( float fwd, float up )
{
	float y = s_demo3.yaw * ( M_PI / 180.0f );
	return s_demo3.feet + Vector( cosf( y ), sinf( y ), 0.0f ) * ( fwd * ZL_UNIT ) + Vector( 0.0f, 0.0f, up * ZL_UNIT );
}

static void ZCam_Demo3Start( const Vector &feet, float yaw )
{
	s_demo3.active = 1;
	s_demo3.frame = 0;
	s_demo3.accum = 0;
	s_demo3.feet = feet;
	s_demo3.yaw = yaw;

	// Swing round whichever side isn't blocked (alternating like play->state.frames & 1)
	s_demo3Flip = !s_demo3Flip;
	s_demo3.yawDir = s_demo3Flip ? 1.0f : -1.0f;
	Vector from = ZCam_Demo3At( 40.0f, 40.0f );
	Vector to = ZCam_OrbitPoint( from, yaw + 90.0f * s_demo3.yawDir, 0.0f, sDemo3Eye[1].r * ZL_UNIT );
	to.z = feet.z + 5.0f * ZL_UNIT;
	pmtrace_t *tr = gEngfuncs.PM_TraceLine( (float *)&from.x, (float *)&to.x, PM_TRACELINE_PHYSENTSONLY, 2, -1 );
	if ( tr && tr->fraction < 1.0f )
		s_demo3.yawDir = -s_demo3.yawDir;

	s_demo3.at = ZCam_Demo3At( sDemo3At[0][0], sDemo3At[0][1] );
	s_demo3.eye = ZCam_OrbitPoint( s_demo3.at, sDemo3Eye[0].yaw * s_demo3.yawDir + yaw, sDemo3Eye[0].pitch, sDemo3Eye[0].r * ZL_UNIT );
	s_demo3.step = 1.0f;
	s_demo3.prevAt = s_demo3.at;
	s_demo3.prevEye = s_demo3.eye;
	s_demo3.frame = 1;
}

// One OoT frame of Camera_Demo3
static void ZCam_Demo3Step( void )
{
	int f = s_demo3.frame;
	zl_geo_t cur = ZCam_ToGeo( s_demo3.eye - s_demo3.at );	// eyeAtOffset
	zl_geo_t want = cur;
	bool update = true;
	int seg = -1;
	float t = 0;

	s_demo3.prevAt = s_demo3.at;
	s_demo3.prevEye = s_demo3.eye;

	if ( f >= 2 && f < 148 )		{ seg = 0; t = ( f - 2 ) * ( 1.0f / 146.0f ); s_demo3.step -= 1.0f / 365.0f; }
	else if ( f >= 148 && f < 158 )	{ seg = 1; t = ( f - 148 ) * 0.1f; s_demo3.step -= 0.04f; }
	else if ( f >= 159 && f < 168 )	{ seg = 2; t = ( f - 159 ) * ( 1.0f / 9.0f ); s_demo3.step += 4.0f / 45.0f; }
	else if ( f >= 228 )
	{
		// Hold 80 units back, level
		want.r = 80.0f * ZL_UNIT;
		want.pitch = 0;
		s_demo3.step = 0.1f;
	}
	else
		update = false;			// animState 10 / 20 / 30

	if ( seg >= 0 )
	{
		float fwd = ZCam_Lerp( sDemo3At[seg][0], sDemo3At[seg + 1][0], t );
		float up;
		if ( seg == 0 )
			up = ZCam_Lerp( sDemo3At[0][1], sDemo3At[1][1], t );
		else if ( seg == 1 )
			up = ZCam_Lerp( sDemo3At[1][1] - DEMO3_PLAYER_HEIGHT, sDemo3At[2][1], t ) + DEMO3_PLAYER_HEIGHT;
		else
			up = ZCam_Lerp( sDemo3At[2][1], sDemo3At[3][1], t ) + DEMO3_PLAYER_HEIGHT;
		s_demo3.at = ZCam_Demo3At( fwd, up );

		want.r = ZCam_Lerp( sDemo3Eye[seg].r, sDemo3Eye[seg + 1].r, t ) * ZL_UNIT;
		want.pitch = ZCam_Lerp( sDemo3Eye[seg].pitch, sDemo3Eye[seg + 1].pitch, t );
		want.yaw = ZCam_Lerp( sDemo3Eye[seg].yaw, sDemo3Eye[seg + 1].yaw, t ) * s_demo3.yawDir + s_demo3.yaw;
	}

	if ( update )
	{
		float s = max( 0.0f, min( 1.0f, s_demo3.step ) );
		zl_geo_t e;
		e.r = ZCam_LerpCeil( want.r, cur.r, s, 2.0f, 0 );
		e.pitch = ZCam_LerpCeil( want.pitch, cur.pitch, s, 0.055f, 1 );
		e.yaw = ZCam_LerpCeil( want.yaw, cur.yaw, s, 0.055f, 1 );
		s_demo3.eye = ZCam_OrbitPoint( s_demo3.at, e.yaw, e.pitch, e.r );
	}

	s_demo3.frame++;
}

static cl_entity_t *ZCam_Entity( int idx )
{
	if ( idx <= 0 )
		return NULL;
	cl_entity_t *ent = gEngfuncs.GetEntityByIndex( idx );
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	if ( !ent || !local || !ent->model || ent == local )
		return NULL;
	if ( ent->curstate.messagenum < local->curstate.messagenum )
		return NULL;
	return ent;
}

// The last view drawn, where the door camera starts from
static Vector s_viewEye, s_viewAt;

static void ZCam_SetView( struct ref_params_s *pparams, const Vector &eye, const Vector &lookAt )
{
	Vector dir = lookAt - eye;
	Vector ang;
	float flat = sqrtf( dir.x * dir.x + dir.y * dir.y );
	ang[0] = -atan2f( dir.z, flat ) * ( 180.0f / M_PI );	// HL view pitch: looking down is positive
	ang[1] = atan2f( dir.y, dir.x ) * ( 180.0f / M_PI );
	ang[2] = 0;

	s_viewEye = eye;
	s_viewAt = lookAt;

	VectorCopy( eye, pparams->vieworg );
	pparams->viewangles[0] = ang[0];
	pparams->viewangles[1] = ang[1];
	pparams->viewangles[2] = 0;

	// view.cpp stored v_angles / v_origin before we moved the camera; the HUD's reticle
	// projection reads them
	VectorCopy( pparams->viewangles, v_angles );
	VectorCopy( pparams->vieworg, v_origin );

	// ZInput_CreateMove puts these in the usercmd
	s_cam.viewYaw = ang[1];
	s_cam.viewPitch = ang[0];
}

/*
=============================================================================
 First person arms (Player_OverrideLimbDrawGameplayFirstPerson)

 Aiming the bow or hookshot, OoT draws only Link's arms and what he holds, from the aiming
 camera. models/player/link/v_link_aim.mdl (tools/zelda/make_link_model.py, from your ROM) is
 built in that camera's space, so it is the view model here. The upper body animation comes up
 once (link_bow_bow_ready / link_hook_shot_ready, func_80834D2C) then idles (D_808543D4). C-up's
 first person draws nothing, and without the model there are no arms.
=============================================================================
*/
static struct model_s *s_aimModel;
static int s_aimModelTried;
static int s_aimSeq = -1;
static int s_aimHook = -1;
static float s_aimStart;

static int ZCam_FindSequence( struct model_s *mod, const char *name, float *seconds )
{
	studiohdr_t *hdr = (studiohdr_t *)IEngineStudio.Mod_Extradata( mod );
	if ( !hdr )
		return -1;
	mstudioseqdesc_t *seq = (mstudioseqdesc_t *)( (byte *)hdr + hdr->seqindex );
	for ( int i = 0; i < hdr->numseq; i++ )
	{
		if ( !stricmp( seq[i].label, name ) )
		{
			if ( seconds )
				*seconds = seq[i].fps > 0 ? ( seq[i].numframes - 1 ) / seq[i].fps : 0;
			return i;
		}
	}
	return -1;
}

/*
 Player_DrawHookshotReticle: aiming the hookshot, a red dot sits wherever it would hit within
 its reach (38600 / 77600 model units from the hand; here the server's range from the eye, where
 the hook really leaves from). Traced here, while prediction's physents are set up; the HUD draws it.
*/
static int s_hookDotValid;
static Vector s_hookDot;

int ZCam_HookDot( float *pos )
{
	if ( !s_hookDotValid || !ZCam_IsFirstPerson() )
		return 0;
	VectorCopy( s_hookDot, pos );
	return 1;
}

static void ZCam_TraceHookDot( struct ref_params_s *pparams, int item )
{
	s_hookDotValid = 0;
	if ( item != ZITEM_HOOKSHOT && item != ZITEM_LONGSHOT )
		return;

	Vector fwd, right, up;
	AngleVectors( pparams->viewangles, fwd, right, up );
	Vector start = pparams->vieworg;
	Vector end = start + fwd * ( ZL_HOOK_RANGE * ( item == ZITEM_LONGSHOT ? 2.0f : 1.0f ) + 16.0f );

	pmtrace_t *tr = gEngfuncs.PM_TraceLine( (float *)&start.x, (float *)&end.x, PM_TRACELINE_PHYSENTSONLY, 2, -1 );
	if ( tr && tr->fraction < 1.0f && !tr->startsolid )
	{
		s_hookDot = Vector( tr->endpos[0], tr->endpos[1], tr->endpos[2] );
		s_hookDotValid = 1;
	}
}

/*
 The arms are drawn as an entity of their own (HUD_CreateEntities adds it, this places it once the
 view is known): the engine's view model slot belongs to Half-Life's weapons and wasn't drawing them.
*/
static cl_entity_t s_aimEnt;
static int s_aimActive;

void ZCam_AddEntities( void )
{
	if ( s_aimActive && s_aimEnt.model )
		gEngfuncs.CL_CreateVisibleEntity( ET_NORMAL, &s_aimEnt );
}

static void ZCam_AimArms( struct ref_params_s *pparams, int act, int state )
{
	cl_entity_t *vm = &s_aimEnt;

	int item = ( act == ZACT_AIM ) ? g_zl.cItem[min( 2, max( 0, ZL_GetSub( state ) ) )] : ZITEM_NONE;
	int hook = ( item == ZITEM_HOOKSHOT || item == ZITEM_LONGSHOT );

	ZCam_TraceHookDot( pparams, item );

	if ( !s_aimModelTried )
	{
		int index;
		s_aimModelTried = 1;
		s_aimModel = gEngfuncs.CL_LoadModel( "models/player/link/v_link_aim.mdl", &index );
	}

	if ( !s_aimModel || ( item != ZITEM_BOW && !hook ) )
	{
		s_aimActive = 0;
		s_aimSeq = -1;
		return;
	}
	s_aimActive = 1;

	// Ready once, then the idle loop
	float readyTime = 0;
	int ready = ZCam_FindSequence( s_aimModel, hook ? "link_hook_shot_ready" : "link_bow_bow_ready", &readyTime );
	int idle = ZCam_FindSequence( s_aimModel, hook ? "link_hook_wait" : "link_bow_bow_wait", NULL );
	int want;
	if ( s_aimSeq < 0 || hook != s_aimHook )
	{
		s_aimStart = pparams->time;
		s_aimHook = hook;
		want = ready >= 0 ? ready : idle;
	}
	else
		want = ( pparams->time - s_aimStart >= readyTime || ready < 0 ) ? idle : ready;

	vm->model = s_aimModel;
	vm->index = 0;
	vm->curstate.modelindex = 0;
	vm->curstate.body = hook ? 1 : 0;
	vm->curstate.rendermode = kRenderNormal;
	vm->curstate.renderamt = 255;
	vm->curstate.scale = 1.0f;
	if ( want >= 0 && want != s_aimSeq )
	{
		s_aimSeq = want;
		vm->curstate.sequence = want;
		vm->curstate.frame = 0;
		vm->curstate.framerate = 1.0f;
		vm->curstate.animtime = pparams->time;
		vm->latched.sequencetime = 0;
	}

	// The model is built around OoT's eye: put it on ours, turned with the view
	VectorCopy( pparams->vieworg, vm->origin );
	vm->angles[0] = -pparams->viewangles[0];
	vm->angles[1] = pparams->viewangles[1];
	vm->angles[2] = 0;
	VectorCopy( vm->origin, vm->curstate.origin );
	VectorCopy( vm->angles, vm->curstate.angles );
}

/*
=============================================================================
 Camera_Special9 (CAM_SET_DOORC): through a door

 Camera_ChangeDoorCam's three timers, in OoT frames. It holds where it was while Link takes the
 handle (38; 12 for a sliding door), cuts to the far side 200 out along the way he's going, 20
 degrees up and 15 to one side (the other side when that's in a wall), and watches him come
 through (26 / 15), then swings round behind him and in to 60 over 10 frames and stays there
 until he moves.
=============================================================================
*/
struct zl_doorcam_t
{
	int		active;
	int		frame;
	float	accum;
	int		t1, t2, t3;
	float	targetYaw;
	Vector	at, eye, prevAt, prevEye;
};

static zl_doorcam_t s_doorCam;
static int s_doorCamFlip;
static int s_doorCamAct;		// Link's action last frame

// at: Player_GetHeight (68) + roData->yOffset (-5% of it) over his feet, from the hull centre
#define DOORCAM_AT_HEIGHT	( ( 68.0f - 3.4f ) * ZL_UNIT - 36.0f )

static int ZCam_DoorFrames( void )
{
	return s_doorCam.t1 + s_doorCam.t2 + s_doorCam.t3 - 2;
}

int ZCam_DoorHidesHud( void )
{
	// CAM_HUD_VISIBILITY_NOTHING_ALT until it's done
	return s_doorCam.active && s_doorCam.frame < ZCam_DoorFrames();
}

static void ZCam_DoorStart( int sliding, int unlocking, float facing )
{
	s_doorCam.active = 1;
	s_doorCam.frame = 0;
	s_doorCam.accum = 0;
	// A sliding door's camera starts when it starts up, after the chains are off (unlockTimer)
	s_doorCam.t1 = sliding ? 12 + ( unlocking ? 10 : 0 ) : 38;
	s_doorCam.t2 = sliding ? 15 : 26;
	s_doorCam.t3 = 10;
	// The door's way through, which is the way Link faces going through it
	s_doorCam.targetYaw = facing;
	s_doorCam.at = s_doorCam.prevAt = s_viewAt;
	s_doorCam.eye = s_doorCam.prevEye = s_viewEye;
}

// Camera_LERPCeilVec3f
static Vector ZCam_LerpCeilVec( const Vector &target, const Vector &cur, float yScale, float xzScale, float minDiff )
{
	return Vector( ZCam_LerpCeil( target.x, cur.x, xzScale, minDiff, 0 ),
				   ZCam_LerpCeil( target.y, cur.y, xzScale, minDiff, 0 ),
				   ZCam_LerpCeil( target.z, cur.z, yScale, minDiff, 0 ) );
}

// One OoT frame of Camera_Special9
static void ZCam_DoorStep( const Vector &origin )
{
	Vector focus = origin + Vector( 0.0f, 0.0f, DOORCAM_AT_HEIGHT );
	int f = ++s_doorCam.frame;
	int cut = s_doorCam.t1;					// timer1 runs out
	int turn = cut + s_doorCam.t2 - 1;		// timer2
	int done = turn + s_doorCam.t3 - 1;		// timer3

	s_doorCam.prevAt = s_doorCam.at;
	s_doorCam.prevEye = s_doorCam.eye;

	if ( f < cut || f > done )
		return;

	if ( f == cut )
	{
		// 0xE38 up, targetYaw +-0xAAA (by play->state.frames & 1), 200 out; Camera_CheckOOB
		// puts it on the other side when that's out of the room
		s_doorCamFlip = !s_doorCamFlip;
		float side = s_doorCamFlip ? 15.0f : -15.0f;
		Vector eye = ZCam_OrbitPoint( s_doorCam.at, s_doorCam.targetYaw + side, 20.0f, 200.0f * ZL_UNIT );
		pmtrace_t *tr = gEngfuncs.PM_TraceLine( (float *)&focus.x, (float *)&eye.x, PM_TRACELINE_PHYSENTSONLY, 2, -1 );
		if ( tr && tr->fraction < 1.0f )
			eye = ZCam_OrbitPoint( s_doorCam.at, s_doorCam.targetYaw - side, 20.0f, 200.0f * ZL_UNIT );
		s_doorCam.eye = s_doorCam.prevEye = eye;
	}

	if ( f < turn )
	{
		// Watching him come through
		s_doorCam.at = ZCam_LerpCeilVec( focus, s_doorCam.at, 0.25f, 0.25f, 0.1f );
		return;
	}

	if ( f == turn )
		s_doorCam.targetYaw += 180.0f;		// rwData->targetYaw -= 0x7FFF

	// Round behind him and in
	s_doorCam.at = ZCam_LerpCeilVec( focus, s_doorCam.at, 0.5f, 0.5f, 0.1f );
	zl_geo_t g = ZCam_ToGeo( s_doorCam.eye - s_doorCam.at );
	g.pitch = ZCam_LerpCeil( 15.0f, g.pitch, 0.3f, 0.055f, 1 );
	g.yaw = ZCam_LerpCeil( s_doorCam.targetYaw, g.yaw, 0.3f, 0.055f, 1 );
	g.r = ZCam_LerpCeil( 60.0f * ZL_UNIT, g.r, 0.3f, 1.0f, 0 );
	s_doorCam.eye = ZCam_OrbitPoint( s_doorCam.at, g.yaw, g.pitch, g.r );
}

/*
=============================================================================
 Dying (func_80836448): OnePointCutscene 9806, Camera_KeepOn4 with CAM_ITEM_TYPE_12

 The camera comes round over 30 frames to 120 degrees off behind Link (the side it's already on),
 2 degrees under level, 1.6 x his height out, looking at 0.4 of his height and on past him; the
 letterbox closes in and the HUD goes. In single player the world goes dark around him
 (Environment_FadeInGameOverLights: ambient and fog to black over 21 frames, his own two lights
 keeping him lit), here a black sheet just behind him, and then the Game Over screen.
=============================================================================
*/
struct zl_death_t
{
	int		active;
	float	start;			// client time he died
	int		mp;
	float	facing;
	int		frame;
	float	accum;
	float	targetYaw, targetPitch, stepYaw, stepPitch, dist;
	int		animTimer;
	Vector	at, eye, prevAt, prevEye;
	// The Game Over screen
	int		choice;			// 0 yes, 1 no
	float	stickPrev;
	int		answered;
};

static zl_death_t s_death;

#define DEATH_PLAYER_HEIGHT	68.0f
#define DEATH_AT_HEIGHT		( DEATH_PLAYER_HEIGHT * ( 1.0f - 0.6f ) )	// yOffset -0.6 x height
#define DEATH_EYE_DIST		( DEATH_PLAYER_HEIGHT * 1.6f )
#define DEATH_AT_FORWARD	20.0f		// atOffsetPlayerForwards
#define DEATH_CAM_FRAMES	30			// initTimer 0x1E

int ZDeath_Active( void ) { return s_death.active; }
int ZDeath_Multiplayer( void ) { return s_death.mp; }
float ZDeath_Frames( void ) { return s_death.active ? ( gEngfuncs.GetClientTime() - s_death.start ) * ZL_FPS : 0.0f; }
int ZDeath_Choice( void ) { return s_death.choice; }
int ZDeath_Answered( void ) { return s_death.answered; }

static Vector ZDeath_AtTarget( const Vector &origin )
{
	return origin + Vector( 0.0f, 0.0f, -36.0f + DEATH_AT_HEIGHT * ZL_UNIT );
}

static void ZDeath_Start( const Vector &origin, float facing )
{
	memset( &s_death, 0, sizeof( s_death ) );
	s_death.active = 1;
	s_death.start = gEngfuncs.GetClientTime();
	s_death.mp = gEngfuncs.GetMaxClients() > 1;
	s_death.facing = facing;
	s_death.at = s_death.prevAt = s_viewAt;
	s_death.eye = s_death.prevEye = s_viewEye;

	zl_geo_t cur = ZCam_ToGeo( s_death.eye - s_death.at );
	s_death.dist = cur.r;

	// atToEyeBaseYaw: behind him, 120 round on the side the camera's on; Camera_BGCheck and
	// sCamCheckAroundOffsets try others when that's in a wall
	float behind = facing + 180.0f;
	float side = ZCam_AngleDiff( behind, cur.yaw ) > 0 ? -1.0f : 1.0f;
	static const float tries[] = { 120.0f, -120.0f, 90.0f, -90.0f, 150.0f, -150.0f, 0.0f };
	Vector atT = ZDeath_AtTarget( origin );
	float yaw = behind + 120.0f * side;
	for ( int i = 0; i < (int)( sizeof( tries ) / sizeof( tries[0] ) ); i++ )
	{
		float y = behind + tries[i] * side;
		Vector eye = ZCam_OrbitPoint( atT, y, -2.0f, DEATH_EYE_DIST * ZL_UNIT );
		pmtrace_t *tr = gEngfuncs.PM_TraceLine( (float *)&atT.x, (float *)&eye.x, PM_TRACELINE_PHYSENTSONLY, 2, -1 );
		if ( !tr || tr->fraction >= 1.0f )
		{
			yaw = y;
			break;
		}
	}

	s_death.targetYaw = cur.yaw;
	s_death.targetPitch = cur.pitch;
	s_death.stepYaw = ZCam_AngleDiff( yaw, cur.yaw ) / DEATH_CAM_FRAMES;
	s_death.stepPitch = ( -2.0f - cur.pitch ) / DEATH_CAM_FRAMES;
	s_death.animTimer = DEATH_CAM_FRAMES;
}

// One OoT frame of Camera_KeepOn4
static void ZDeath_Step( const Vector &origin )
{
	s_death.prevAt = s_death.at;
	s_death.prevEye = s_death.eye;

	float f = s_death.facing * ( M_PI / 180.0f );
	Vector atT = ZDeath_AtTarget( origin );
	Vector at;
	at.x = ZCam_LerpCeil( atT.x, s_death.at.x, 0.5f, 0.2f, 0 );
	at.y = ZCam_LerpCeil( atT.y, s_death.at.y, 0.5f, 0.2f, 0 );
	at.z = ZCam_LerpCeil( atT.z, s_death.at.z, 0.5f, 0.2f, 0 );
	at = at + Vector( cosf( f ), sinf( f ), 0.0f ) * ( DEATH_AT_FORWARD * ZL_UNIT );

	zl_geo_t cur = ZCam_ToGeo( s_death.eye - at );
	s_death.dist = ZCam_LerpCeil( DEATH_EYE_DIST * ZL_UNIT, s_death.dist, 0.25f, 2.0f, 0 );
	if ( s_death.animTimer > 0 )
	{
		s_death.targetYaw += s_death.stepYaw;
		s_death.targetPitch += s_death.stepPitch;
		s_death.animTimer--;
	}
	float yaw = ZCam_LerpCeil( s_death.targetYaw, cur.yaw, 0.25f, 0.02f, 1 );
	float pitch = ZCam_LerpCeil( s_death.targetPitch, cur.pitch, 0.25f, 0.02f, 1 );

	s_death.at = at;
	s_death.eye = ZCam_OrbitPoint( at, yaw, pitch, s_death.dist );
}

static void ZDeath_View( struct ref_params_s *pparams, const Vector &origin )
{
	if ( !s_death.active )
		ZDeath_Start( origin, ZL_PredictedFacing() );

	float dt = pparams->frametime;
	if ( dt <= 0 || dt > 0.25f )
		dt = 0.016f;
	s_death.accum += dt;
	while ( s_death.accum >= 1.0f / ZL_FPS )
	{
		s_death.accum -= 1.0f / ZL_FPS;
		ZDeath_Step( origin );
	}

	float k = s_death.accum * ZL_FPS;
	Vector at = s_death.prevAt + ( s_death.at - s_death.prevAt ) * k;
	Vector eye = s_death.prevEye + ( s_death.eye - s_death.prevEye ) * k;
	ZCam_SetView( pparams, ZCam_Collide( at, eye ), at );
}

// The world going dark around him (single player): a black sheet across the view just past him,
// so he stays lit and everything beyond goes to black over 21 frames
void ZCam_DrawDeathFade( struct model_s *white )
{
	if ( !s_death.active || s_death.mp )
		return;

	float a = ZDeath_Frames() / 21.0f;
	if ( a <= 0 )
		return;
	if ( a > 1 )
		a = 1;

	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	if ( !player )
		return;

	Vector fwd, right, up;
	gEngfuncs.pfnAngleVectors( v_angles, fwd, right, up );
	Vector link = player->origin;
	float depth = DotProduct( link - v_origin, fwd ) + 24.0f;
	if ( depth < 16.0f )
		depth = 16.0f;
	Vector c = v_origin + fwd * depth;
	float s = depth * 4.0f;

	triangleapi_t *tri = gEngfuncs.pTriAPI;
	if ( !white || !tri->SpriteTexture( white, 0 ) )
		return;
	tri->RenderMode( kRenderTransAlpha );
	tri->CullFace( TRI_NONE );
	tri->Color4f( 0.0f, 0.0f, 0.0f, a );
	tri->Begin( TRI_QUADS );
	Vector p;
	p = c - right * s - up * s;	tri->TexCoord2f( 0, 0 ); tri->Vertex3fv( (float *)&p.x );
	p = c + right * s - up * s;	tri->TexCoord2f( 1, 0 ); tri->Vertex3fv( (float *)&p.x );
	p = c + right * s + up * s;	tri->TexCoord2f( 1, 1 ); tri->Vertex3fv( (float *)&p.x );
	p = c - right * s + up * s;	tri->TexCoord2f( 0, 1 ); tri->Vertex3fv( (float *)&p.x );
	tri->End();
	tri->RenderMode( kRenderNormal );
	tri->CullFace( TRI_FRONT );
}

/*
 KaleidoScope's PAUSE_STATE_GAME_OVER_CONTINUE_PROMPT: the stick picks, A answers. In
 multiplayer Half-Life's own respawn takes any button.
*/
#define GAMEOVER_PROMPT_FRAME	( 56.0f + 20.0f + 30.0f + 40.0f + 11.0f )

void ZDeath_Input( int pressed, float stickX )
{
	if ( !s_death.active || s_death.mp || s_death.answered || ZDeath_Frames() < GAMEOVER_PROMPT_FRAME )
	{
		s_death.stickPrev = stickX;
		return;
	}

	int dir = 0;
	if ( stickX > 0.5f && s_death.stickPrev <= 0.5f )
		dir = 1;
	else if ( stickX < -0.5f && s_death.stickPrev >= -0.5f )
		dir = -1;
	s_death.stickPrev = stickX;

	if ( dir && ( s_death.choice + dir == 0 || s_death.choice + dir == 1 ) )
	{
		s_death.choice += dir;
		ZL_PlaySound( ZS_SY_CURSOR, 0.6f );
	}

	if ( pressed & ZPAD_A )
	{
		ZL_PlaySound( ZS_SY_DECIDE, 0.7f );
		s_death.answered = 1;
		gEngfuncs.pfnClientCmd( (char *)( s_death.choice == 0 ? "zl_continue 1\n" : "zl_continue 0\n" ) );
	}
}

void ZCam_CalcRefdef( struct ref_params_s *pparams )
{
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	if ( !player )
		return;

	float dt = pparams->frametime;
	if ( dt <= 0 || dt > 0.25f )
		dt = 0.016f;

	int state = ZL_PredictedState();
	int act = ZL_GetAction( state );
	float facing = ZL_PredictedFacing();
	Vector origin = pparams->simorg;
	Vector velocity = g_pmzPredicted.velocity;

	// Mouse movement since the last frame
	float mouseYaw = s_pendingYaw, mousePitch = s_pendingPitch;
	s_pendingYaw = s_pendingPitch = 0;
	if ( fabs( mouseYaw ) > 0.01f || fabs( mousePitch ) > 0.01f )
		s_cam.lastMouseTime = pparams->time;
	if ( zl_cam_mouse && !zl_cam_mouse->value )
		mouseYaw = mousePitch = 0;

	// Dead: the death camera until he's back (multiplayer) or the level reloads
	if ( pparams->health <= 0 )
	{
		s_cam.letterbox = ZCam_Approach( s_cam.letterbox, 32.0f, 10.0f, dt );
		s_cam.firstPerson = 0;
		s_cam.thirdPerson = 1;
		ZDeath_View( pparams, origin );
		return;
	}
	if ( s_death.active )
	{
		s_death.active = 0;
		s_cam.initialized = 0;		// back in behind him
	}

	// Pick the mode (Player decides camera modes in OoT; this mirrors Player_UpdateCamAndSeqModes)
	int mode = ZCAM_NORMAL;
	cl_entity_t *target = NULL;
	cl_entity_t *speaker = NULL;

	if ( act == ZACT_AIM || act == ZACT_CUP )
		mode = ZCAM_SUBJECT;
	else if ( act == ZACT_CHEST || ( act == ZACT_GETITEM && ZL_GetSub( state ) == 1 && s_demo3.active ) )
		mode = ZCAM_CHEST;
	else if ( act == ZACT_DOOR )
		mode = ZCAM_DOOR;
	else if ( s_doorCam.active && s_doorCam.frame >= ZCam_DoorFrames() && velocity.Length2D() < 1.0f &&
			  fabs( mouseYaw ) < 0.01f && fabs( mousePitch ) < 0.01f )
		mode = ZCAM_DOOR;		// done, it stays until he moves
	else if ( s_doorCam.active && s_doorCam.frame < ZCam_DoorFrames() )
		mode = ZCAM_DOOR;		// Link's through before the camera is
	else if ( ZText_IsOpen() && ( speaker = ZCam_Entity( ZText_Speaker() ) ) != NULL )
		mode = ZCAM_TALK;
	else if ( ( state & ZL_STATE_LOCKED ) && ( target = ZCam_Entity( g_zl.lockIdx ) ) != NULL )
		mode = ( g_zl.targetFlags & ZTGT_HOSTILE ) ? ZCAM_BATTLE : ZCAM_KEEPON;
	else if ( state & ( ZL_STATE_PARALLEL | ZL_STATE_LOCKED ) )
		mode = ZCAM_PARALLEL;

	if ( mode != s_cam.mode )
	{
		s_cam.lastMode = s_cam.mode;
		s_cam.mode = mode;
		s_cam.modeTime = 0;
	}
	s_cam.modeTime += dt;

	// Letterbox: CAM_LETTERBOX_MEDIUM (27) when Z-targeting, LARGE (32) when talking
	float lbTarget = 0;
	if ( mode == ZCAM_PARALLEL || mode == ZCAM_BATTLE || mode == ZCAM_KEEPON )
		lbTarget = 27;
	else if ( mode == ZCAM_TALK || mode == ZCAM_CHEST || act == ZACT_TALK || act == ZACT_GETITEM || ZCam_DoorHidesHud() )
		lbTarget = 32;
	s_cam.letterbox = ZCam_Approach( s_cam.letterbox, lbTarget, 10.0f, dt );

	// Link's body faces his shape yaw, not the camera
	Vector bodyAng( 0.0f, facing, 0.0f );
	VectorCopy( bodyAng, player->angles );
	VectorCopy( bodyAng, player->curstate.angles );
	VectorCopy( bodyAng, player->prevstate.angles );
	VectorCopy( bodyAng, player->latched.prevangles );

	/*
	------------------------------------------------------------------
	 First person (Camera_Subj3): look with the mouse from Link's eyes
	------------------------------------------------------------------
	*/
	if ( mode == ZCAM_SUBJECT )
	{
		if ( !s_cam.firstPerson )
		{
			// Start looking where Link faces
			s_cam.viewYaw = facing;
			s_cam.viewPitch = 0;
		}
		else
		{
			s_cam.viewYaw += mouseYaw;
			s_cam.viewPitch = max( -70.0f, min( 70.0f, s_cam.viewPitch + mousePitch ) );
		}

		s_cam.firstPerson = 1;
		s_cam.thirdPerson = 0;
		s_cam.initialized = 1;

		Vector eye = origin + Vector( pparams->viewheight[0], pparams->viewheight[1], pparams->viewheight[2] );
		VectorCopy( eye, pparams->vieworg );
		pparams->viewangles[0] = s_cam.viewPitch;
		pparams->viewangles[1] = s_cam.viewYaw;
		pparams->viewangles[2] = 0;
		VectorCopy( pparams->viewangles, v_angles );
		VectorCopy( pparams->vieworg, v_origin );

		ZCam_AimArms( pparams, act, state );

		// Leave first person behind Link
		s_cam.orbitYaw = facing + 180.0f;
		s_cam.orbitPitch = NORM1_PITCH;
		s_cam.dist = NORM1_DIST_MIN;
		s_cam.eye = ZCam_OrbitPoint( origin + Vector( 0.0f, 0.0f, CAM_AT_HEIGHT ), s_cam.orbitYaw, s_cam.orbitPitch, s_cam.dist );
		s_cam.atZ = origin.z + CAM_AT_HEIGHT;
		return;
	}

	s_cam.firstPerson = 0;
	s_cam.thirdPerson = 1;
	ZCam_AimArms( pparams, ZACT_STAND, state );

	if ( mode == ZCAM_CHEST )
	{
		if ( !s_demo3.active )
			ZCam_Demo3Start( origin - Vector( 0.0f, 0.0f, 36.0f ), facing );	// standing hull's feet

		s_demo3.accum += dt;
		while ( s_demo3.accum >= 1.0f / ZL_FPS )
		{
			s_demo3.accum -= 1.0f / ZL_FPS;
			ZCam_Demo3Step();
		}

		float k = s_demo3.accum * ZL_FPS;
		Vector at = s_demo3.prevAt + ( s_demo3.at - s_demo3.prevAt ) * k;
		Vector eye = s_demo3.prevEye + ( s_demo3.eye - s_demo3.prevEye ) * k;

		// Hand back to the normal camera from wherever this ends
		Vector d = eye - origin;
		s_cam.eye = eye;
		s_cam.dist = max( NORM1_DIST_MIN, min( NORM1_DIST_MAX, d.Length() ) );
		s_cam.orbitYaw = atan2f( d.y, d.x ) * ( 180.0f / M_PI );
		s_cam.orbitPitch = NORM1_PITCH;
		s_cam.atZ = origin.z + CAM_AT_HEIGHT;
		s_cam.initialized = 1;

		ZCam_SetView( pparams, eye, at );
		return;
	}
	s_demo3.active = 0;

	if ( mode == ZCAM_DOOR )
	{
		// (a new door straight after the last, its camera still waiting for him to move, starts over)
		if ( !s_doorCam.active || ( act == ZACT_DOOR && s_doorCamAct != ZACT_DOOR ) )
			ZCam_DoorStart( ZL_GetSub( state ) == ZDOOR_SLIDING, ZL_GetMWA( state ) != 0, facing );
		s_doorCamAct = act;

		s_doorCam.accum += dt;
		while ( s_doorCam.accum >= 1.0f / ZL_FPS )
		{
			s_doorCam.accum -= 1.0f / ZL_FPS;
			ZCam_DoorStep( origin );
		}

		float k = s_doorCam.accum * ZL_FPS;
		Vector at = s_doorCam.prevAt + ( s_doorCam.at - s_doorCam.prevAt ) * k;
		Vector eye = s_doorCam.prevEye + ( s_doorCam.eye - s_doorCam.prevEye ) * k;

		// Hand back to the normal camera from here
		Vector d = eye - origin;
		s_cam.eye = eye;
		s_cam.dist = max( NORM1_DIST_MIN, min( NORM1_DIST_MAX, d.Length() ) );
		s_cam.orbitYaw = atan2f( d.y, d.x ) * ( 180.0f / M_PI );
		s_cam.orbitPitch = NORM1_PITCH;
		s_cam.atZ = origin.z + CAM_AT_HEIGHT;
		s_cam.initialized = 1;

		ZCam_SetView( pparams, ZCam_Collide( at, eye ), at );
		return;
	}
	s_doorCam.active = 0;
	s_doorCamAct = act;

	// Vertical follow lags on jumps, catches up on the ground (rwData->yOffset)
	float atTargetZ = origin.z + CAM_AT_HEIGHT;
	if ( !s_cam.initialized || fabs( atTargetZ - s_cam.atZ ) > 200 )
		s_cam.atZ = atTargetZ;
	else
		s_cam.atZ = ZCam_Approach( s_cam.atZ, atTargetZ, g_pmzPredicted.onground ? 10.0f : 3.0f, dt );

	Vector at( origin.x, origin.y, s_cam.atZ );

	if ( !s_cam.initialized || ( s_cam.eye - at ).Length() > 800 )
	{
		s_cam.orbitYaw = facing + 180.0f;
		s_cam.orbitPitch = NORM1_PITCH;
		s_cam.dist = NORM1_DIST_MIN;
		s_cam.eye = ZCam_OrbitPoint( at, s_cam.orbitYaw, s_cam.orbitPitch, s_cam.dist );
		s_cam.initialized = 1;
	}

	float distScale = ( zl_cam_dist && zl_cam_dist->value > 0 ) ? zl_cam_dist->value : 1.0f;
	Vector eye, lookAt = at;

	switch ( mode )
	{
	default:
	case ZCAM_NORMAL:
	if ( !zl_cam_style || zl_cam_style->value )
	{
		// Mouse camera (zl_cam_style 1): only the mouse turns it, so the stick directions stay
		// put on screen: W runs away from the camera, A to its left, whatever Link is doing.
		// Distance, height and Z's snap behind Link are OoT's; the drag and auto swing below,
		// which make keyboard directions wander, are left out.
		s_cam.orbitYaw += mouseYaw;
		s_cam.orbitPitch = max( -40.0f, min( 75.0f, s_cam.orbitPitch + mousePitch ) );
		s_cam.dist = ZCam_Approach( s_cam.dist, ( NORM1_DIST_MIN + NORM1_DIST_MAX ) * 0.5f * distScale, 4.0f, dt );
		eye = ZCam_OrbitPoint( at, s_cam.orbitYaw, s_cam.orbitPitch, s_cam.dist );
		break;
	}
	else
	{
		// Derive the orbit from where the eye was: Link running away drags it, running at it
		// pushes it back (Camera_ClampDist)
		Vector d = s_cam.eye - at;
		float len = d.Length();
		if ( len > 1 )
		{
			s_cam.orbitYaw = atan2f( d.y, d.x ) * ( 180.0f / M_PI );
			float p = asinf( max( -1.0f, min( 1.0f, d.z / len ) ) ) * ( 180.0f / M_PI );
			// Only follow the eye's pitch when coming out of another mode
			if ( s_cam.modeTime < 0.5f )
				s_cam.orbitPitch = ZCam_Approach( s_cam.orbitPitch, p, 6.0f, dt );
		}

		s_cam.orbitYaw += mouseYaw;
		s_cam.orbitPitch = max( -40.0f, min( 75.0f, s_cam.orbitPitch + mousePitch ) );
		s_cam.dist = max( NORM1_DIST_MIN * distScale, min( NORM1_DIST_MAX * distScale, len ) );

		Vector flatVel = velocity;
		flatVel.z = 0;
		float speedRatio = min( 1.0f, flatVel.Length() / ZL_VEL( ZL_RUN_SPEED ) );
		bool mouseIdle = pparams->time - s_cam.lastMouseTime > 0.75f;

		// Camera_CalcDefaultYaw: swing behind Link, fastest when he runs across the screen,
		// not at all when he runs straight at the camera
		if ( zl_cam_follow && zl_cam_follow->value && speedRatio > 0.01f && mouseIdle )
		{
			float behind = facing + 180.0f;
			float diff = ZCam_AngleDiff( behind, s_cam.orbitYaw );
			float side = fabsf( sinf( diff * ( M_PI / 180.0f ) ) );
			float rate = speedRatio * ( 2.2f * side + ( fabsf( diff ) < 90.0f ? 0.6f : 0.0f ) );
			s_cam.orbitYaw += diff * ( 1.0f - expf( -rate * dt ) );

			// Ease the pitch back to Normal1's 10 degrees
			s_cam.orbitPitch = ZCam_Approach( s_cam.orbitPitch, NORM1_PITCH, 0.8f * speedRatio, dt );
		}

		eye = ZCam_OrbitPoint( at, s_cam.orbitYaw, s_cam.orbitPitch, s_cam.dist );
		break;
	}

	case ZCAM_PARALLEL:
	{
		// Camera_Parallel1: straight behind Link, level
		s_cam.orbitYaw = ZCam_ApproachAngle( s_cam.orbitYaw, facing + 180.0f, 12.0f, dt );
		s_cam.orbitPitch = ZCam_Approach( s_cam.orbitPitch, 5.0f, 8.0f, dt );
		s_cam.dist = ZCam_Approach( s_cam.dist, PARA1_DIST * distScale, 6.0f, dt );
		eye = ZCam_OrbitPoint( at, s_cam.orbitYaw, s_cam.orbitPitch, s_cam.dist );
		lookAt = at + Vector( cosf( facing * ( M_PI / 180.0f ) ), sinf( facing * ( M_PI / 180.0f ) ), 0.0f ) * 64.0f;
		break;
	}

	case ZCAM_BATTLE:
	case ZCAM_KEEPON:
	{
		Vector focus = target->origin + Vector( 0.0f, 0.0f, g_zl.focusHeight );
		Vector toLink = at - focus;
		toLink.z = 0;
		float sep = toLink.Length();
		float behind = ( sep > 1 ) ? atan2f( toLink.y, toLink.x ) * ( 180.0f / M_PI ) : facing + 180.0f;

		// Camera_Battle1 swings a little off to the side the closer the enemy is
		float swing = ( mode == ZCAM_BATTLE ) ? 10.0f + 25.0f * max( 0.0f, 1.0f - sep / 300.0f ) : 15.0f;
		float side = ZCam_AngleDiff( s_cam.orbitYaw, behind ) >= 0 ? 1.0f : -1.0f;

		float dist = ( mode == ZCAM_BATTLE ? BATT1_DIST : KEEP1_DIST ) * distScale;
		s_cam.orbitYaw = ZCam_ApproachAngle( s_cam.orbitYaw, behind + swing * side, 8.0f, dt );
		s_cam.orbitPitch = ZCam_Approach( s_cam.orbitPitch, mode == ZCAM_BATTLE ? 14.0f : 8.0f, 6.0f, dt );
		s_cam.dist = ZCam_Approach( s_cam.dist, dist, 6.0f, dt );

		eye = ZCam_OrbitPoint( at, s_cam.orbitYaw, s_cam.orbitPitch, s_cam.dist );

		// Look past Link at the target (atLERPStepScale)
		lookAt = at + ( focus - at ) * ( mode == ZCAM_BATTLE ? 0.45f : 0.5f );
		break;
	}

	case ZCAM_TALK:
	{
		// Camera_KeepOn3: side on, both of them in frame
		Vector other = speaker->origin + Vector( 0.0f, 0.0f, 18.0f );
		Vector mid = ( at + other ) * 0.5f;
		Vector between = other - at;
		between.z = 0;
		float sep = max( 32.0f, between.Length() );
		Vector perp = Vector( -between.y, between.x, 0.0f ) / sep;

		// Pick whichever side the camera is already on
		if ( DotProduct( s_cam.eye - mid, perp ) < 0 )
			perp = perp * -1.0f;

		Vector want = mid + perp * ( sep * 0.9f + 70.0f ) - between / sep * ( sep * 0.35f ) + Vector( 0.0f, 0.0f, 14.0f );
		eye = s_cam.eye + ( want - s_cam.eye ) * ( 1.0f - expf( -5.0f * dt ) );
		lookAt = mid;
		break;
	}
	}

	// Smooth the eye a bit except in Normal, which is already continuous
	if ( mode != ZCAM_NORMAL && mode != ZCAM_TALK )
		eye = s_cam.eye + ( eye - s_cam.eye ) * ( 1.0f - expf( -14.0f * dt ) );

	s_cam.eye = eye;

	// Collision is applied to the rendered eye only, so the orbit springs back after a wall
	Vector rendered = ZCam_Collide( at, eye );

	// Crit wiggle: the camera wobbles at low health (Camera_Normal1, health <= 16)
	if ( g_zl.health > 0 && g_zl.health <= ZL_HEART && mode == ZCAM_NORMAL )
		rendered.z += sinf( pparams->time * 2.0f ) * 0.6f;

	ZCam_SetView( pparams, rendered, lookAt );
}
