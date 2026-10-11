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

#include <math.h>

extern vec3_t v_angles, v_origin;	// view.cpp

enum
{
	ZCAM_NORMAL,
	ZCAM_PARALLEL,
	ZCAM_BATTLE,
	ZCAM_KEEPON,
	ZCAM_TALK,
	ZCAM_SUBJECT,
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

static void ZCam_SetView( struct ref_params_s *pparams, const Vector &eye, const Vector &lookAt )
{
	Vector dir = lookAt - eye;
	Vector ang;
	float flat = sqrtf( dir.x * dir.x + dir.y * dir.y );
	ang[0] = -atan2f( dir.z, flat ) * ( 180.0f / M_PI );	// HL view pitch: looking down is positive
	ang[1] = atan2f( dir.y, dir.x ) * ( 180.0f / M_PI );
	ang[2] = 0;

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

	// Pick the mode (Player decides camera modes in OoT; this mirrors Player_UpdateCamAndSeqModes)
	int mode = ZCAM_NORMAL;
	cl_entity_t *target = NULL;
	cl_entity_t *speaker = NULL;

	if ( act == ZACT_AIM || act == ZACT_CUP )
		mode = ZCAM_SUBJECT;
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
	else if ( mode == ZCAM_TALK || act == ZACT_TALK || act == ZACT_GETITEM )
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
