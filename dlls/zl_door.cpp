/***
*
*	Zelda 64 port: doors (En_Door, Door_Shutter).
*
*	zl_door			the wooden door with a handle (EnDoor). Stand in front of it, face it, A:
*					Link takes the handle and walks through on his animation
*					(Player_ActionHandler_1, link_demo_doorA / doorB) while the door swings
*					(gDoorAdultOpening*Anim) and the camera cuts to the other side (Camera_Special9).
*		angles		which way it faces (either side opens it)
*		spawnflags	1 = locked: takes a small key (EnDoor DOOR_LOCKED). Chained up with a padlock on
*					both faces (Actor_DrawDoorLock); without a key Navi calls (C-up, text 0x0203)
*		target		fired when it opens
*
*	zl_door_shutter	the dungeon door that slides up (Door_Shutter, DOORSHUTTER_GFX_GENERIC). A:
*					it rises 200, Link walks on under it (Player_Action_80845CA4) and it comes
*					down behind him with a thud and a little quake (DoorShutter_Close).
*		angles, spawnflags 1, target as above
*
*	The models are models/zelda/door.mdl and door_shutter.mdl (tools/zelda/make_object_models.py),
*	the door facing +X. Each is about 60 / 80 units across: the doorway in the map wants to be at
*	least 64 wide for Half-Life's hull. The cut to the other side is the client's (zl_camera.cpp,
*	ZCAM_DOOR); this sets Link's ZACT_DOOR and puts him where OoT does.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "zl_player.h"

#define ZL_MDL_DOOR			"models/zelda/door.mdl"
#define ZL_MDL_SHUTTER		"models/zelda/door_shutter.mdl"

#define ZL_MDL_CHAINS		"models/zelda/door_chains.mdl"
#define ZL_MDL_LOCK			"models/zelda/door_lock.mdl"

#define SF_ZDOOR_LOCKED		1

// Navi's "the door is locked" (player->naviTextId = -0x203)
#define ZL_NAVI_DOOR_LOCKED	"0x0203"

/*
=============================================================================
 The chains and padlock on a locked door (Actor_DrawDoorLock), one of each on each face.
 Unlocking (lockTimer 10 -> 0) the chains slide off along their length and the padlock
 shrinks away (scale lockTimer / 10, through the renderer's fuser1 / fuser2 squash).
=============================================================================
*/
class CZeldaLockPart : public CBaseAnimating
{
public:
	void Spawn( void ) {}
	void EXPORT UnlockThink( void );
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	void Unlock( void );

	BOOL m_fPadlock;
	float m_flUnlockStart;
};

LINK_ENTITY_TO_CLASS( zl_door_lockpart, CZeldaLockPart );

TYPEDESCRIPTION CZeldaLockPart::m_SaveData[] =
{
	DEFINE_FIELD( CZeldaLockPart, m_fPadlock, FIELD_BOOLEAN ),
	DEFINE_FIELD( CZeldaLockPart, m_flUnlockStart, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CZeldaLockPart, CBaseAnimating );

static void ZL_PrecacheLock( void )
{
	if ( ZL_FileExists( ZL_MDL_CHAINS ) )
		PRECACHE_MODEL( (char *)ZL_MDL_CHAINS );
	if ( ZL_FileExists( ZL_MDL_LOCK ) )
		PRECACHE_MODEL( (char *)ZL_MDL_LOCK );
}

static CZeldaLockPart *ZL_SpawnLockPart( const char *model, BOOL padlock, const Vector &origin, float yaw )
{
	if ( !ZL_FileExists( model ) )
		return NULL;
	CZeldaLockPart *pPart = GetClassPtr( (CZeldaLockPart *)NULL );
	pPart->pev->classname = MAKE_STRING( "zl_door_lockpart" );
	SET_MODEL( pPart->edict(), model );
	pPart->pev->movetype = MOVETYPE_NONE;
	pPart->pev->solid = SOLID_NOT;
	pPart->pev->angles = Vector( 0, yaw, 0 );
	UTIL_SetOrigin( pPart->pev, origin );
	pPart->m_fPadlock = padlock;
	pPart->pev->sequence = pPart->LookupSequence( "idle" );
	pPart->pev->frame = 0;
	pPart->ResetSequenceInfo();
	pPart->pev->framerate = 0;
	return pPart;
}

void CZeldaLockPart::Unlock( void )
{
	m_flUnlockStart = gpGlobals->time;
	if ( !m_fPadlock )
	{
		pev->sequence = LookupSequence( "unlock" );
		pev->frame = 0;
		ResetSequenceInfo();
		pev->framerate = 1.0f;		// one frame a frame, lockTimer 10 .. 0
	}
	SetThink( &CZeldaLockPart::UnlockThink );
	pev->nextthink = gpGlobals->time + ZL_FRAMES( 1.0f );
}

void CZeldaLockPart::UnlockThink( void )
{
	pev->nextthink = gpGlobals->time + ZL_FRAMES( 1.0f );
	float left = 1.0f - ( gpGlobals->time - m_flUnlockStart ) / ZL_DOOR_LOCK_TIME;
	if ( left <= 0.05f )
	{
		UTIL_Remove( this );
		return;
	}
	if ( m_fPadlock )
		pev->fuser1 = pev->fuser2 = left;
	else
		StudioFrameAdvance();
}

// Both faces of a door: the lock 50 up its middle and depth out from it (Door_Shutter draws it
// 500 x 0.025 out; EnDoor's own would sit inside its thinner door, so it's put just off the face)
static void ZL_SpawnLock( CBaseEntity *pDoor, float depth, EHANDLE *parts )
{
	Vector fwd;
	UTIL_MakeVectorsPrivate( Vector( 0, pDoor->pev->angles.y, 0 ), fwd, NULL, NULL );
	Vector mid = pDoor->pev->origin + Vector( 0, 0, ZL_DIST( 50 ) );
	for ( int side = 0; side < 2; side++ )
	{
		float sign = side ? -1.0f : 1.0f;
		float yaw = pDoor->pev->angles.y + ( side ? 180.0f : 0.0f );
		Vector at = mid + fwd * ( sign * ZL_DIST( depth ) );
		parts[side * 2] = ZL_SpawnLockPart( ZL_MDL_CHAINS, FALSE, at, yaw );
		parts[side * 2 + 1] = ZL_SpawnLockPart( ZL_MDL_LOCK, TRUE, at, yaw );
	}
}

static void ZL_UnlockParts( EHANDLE *parts )
{
	for ( int i = 0; i < 4; i++ )
	{
		CZeldaLockPart *pPart = (CZeldaLockPart *)(CBaseEntity *)parts[i];
		if ( pPart )
			pPart->Unlock();
		parts[i] = NULL;
	}
}

/*
 Where Link is from the door, in OoT units (Actor_WorldToActorCoords): x across it, y up from its
 foot, z out of its front. Returns the side he's on, facing it, close enough to open it
 (EnDoor_Idle / DoorShutter_GetPlayerSide): 1 in front, -1 behind, 0 not.
*/
static int ZL_DoorPlayerSide( CBaseEntity *pDoor, CLinkPlayer *pLink, float rangeSides, float rangeY )
{
	Vector fwd, right, up;
	UTIL_MakeVectorsPrivate( Vector( 0, pDoor->pev->angles.y, 0 ), fwd, right, up );

	Vector feet = pLink->pev->origin;
	feet.z += pLink->pev->mins.z;
	Vector d = feet - pDoor->pev->origin;

	float x = DotProduct( d, right ) / ZL_UNIT;
	float y = d.z / ZL_UNIT;
	float z = DotProduct( d, fwd ) / ZL_UNIT;

	if ( fabs( x ) > rangeSides || fabs( y ) > rangeY || fabs( z ) >= 50.0f )
		return 0;

	// Facing the door: shape.rot.y about opposite to the door's in front of it, along it behind
	float diff = UTIL_AngleDiff( pLink->ZFacing(), pDoor->pev->angles.y );
	if ( z > 0 )
		diff = UTIL_AngleDiff( 180.0f, diff );
	if ( fabs( diff ) >= 67.5f )		// 0x3000
		return 0;

	return z >= 0 ? 1 : -1;
}

// A door the player can't get through without a key he hasn't got
static BOOL ZL_DoorLockedOut( CLinkPlayer *pLink, BOOL locked )
{
	return locked && pLink->m_ZInv.smallKeys <= 0;
}

// The door's box: thin along its facing, axis aligned (doors in the map should face along an axis)
static void ZL_DoorSetSize( CBaseEntity *pDoor, float halfWidth, float halfDepth, float height )
{
	float yaw = UTIL_AngleMod( pDoor->pev->angles.y );
	BOOL alongY = ( yaw > 45 && yaw < 135 ) || ( yaw > 225 && yaw < 315 );
	float hx = alongY ? halfWidth : halfDepth;
	float hy = alongY ? halfDepth : halfWidth;
	UTIL_SetSize( pDoor->pev, Vector( -hx, -hy, 0 ), Vector( hx, hy, height ) );
}

// Put Link on the spot and start him through (Player_ActionHandler_1)
static void ZL_DoorStartLink( CLinkPlayer *pLink, const Vector &spot, float facing, int sub, BOOL unlocking )
{
	if ( spot != pLink->pev->origin )
	{
		TraceResult tr;
		UTIL_TraceHull( pLink->pev->origin, spot, dont_ignore_monsters, human_hull, pLink->edict(), &tr );
		if ( !tr.fStartSolid )
			UTIL_SetOrigin( pLink->pev, tr.vecEndPos );
	}

	pLink->pev->velocity = g_vecZero;
	pLink->pev->fuser1 = UTIL_AngleMod( facing );	// Link's facing (predicted)
	pLink->ZSetAction( ZACT_DOOR );
	pLink->pev->iuser4 = ( pLink->pev->iuser4 & ~( ZL_STATE_SUB_MASK | ZL_STATE_MWA_MASK ) ) |
						 ( ( sub << ZL_STATE_SUB_SHIFT ) & ZL_STATE_SUB_MASK ) |
						 ( ( ( unlocking ? 1 : 0 ) << ZL_STATE_MWA_SHIFT ) & ZL_STATE_MWA_MASK );
}

/*
=============================================================================
 zl_door (En_Door)
=============================================================================
*/
class CZeldaDoor : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT OpenThink( void );
	int ObjectCaps( void ) { return CBaseAnimating::ObjectCaps() | FCAP_IMPULSE_USE; }
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int PlayerSide( CLinkPlayer *pLink, BOOL anyway = FALSE );

	BOOL m_fLocked;
	float m_flOpenStart;
	float m_flWait;			// lockTimer: the swing starts once the chains are off
	int m_iOpenStep;
	EHANDLE m_hLock[4];
};

LINK_ENTITY_TO_CLASS( zl_door, CZeldaDoor );

TYPEDESCRIPTION CZeldaDoor::m_SaveData[] =
{
	DEFINE_FIELD( CZeldaDoor, m_fLocked, FIELD_BOOLEAN ),
	DEFINE_FIELD( CZeldaDoor, m_flOpenStart, FIELD_TIME ),
	DEFINE_FIELD( CZeldaDoor, m_flWait, FIELD_FLOAT ),
	DEFINE_FIELD( CZeldaDoor, m_iOpenStep, FIELD_INTEGER ),
	DEFINE_ARRAY( CZeldaDoor, m_hLock, FIELD_EHANDLE, 4 ),
};
IMPLEMENT_SAVERESTORE( CZeldaDoor, CBaseAnimating );

void CZeldaDoor::Precache( void )
{
	if ( ZL_FileExists( ZL_MDL_DOOR ) )
		PRECACHE_MODEL( (char *)ZL_MDL_DOOR );
	ZL_PrecacheLock();
}

void CZeldaDoor::Spawn( void )
{
	Precache();

	if ( ZL_FileExists( ZL_MDL_DOOR ) )
	{
		SET_MODEL( ENT( pev ), ZL_MDL_DOOR );
		pev->sequence = LookupSequence( "idle" );
		pev->frame = 0;
		ResetSequenceInfo();
		pev->framerate = 0;
	}
	else
		ALERT( at_console, "zl_door: no %s (tools/zelda/make_object_models.py)\n", ZL_MDL_DOOR );

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;
	// The door: 60 across, 100 high, its faces 7 either side of the middle
	ZL_DoorSetSize( this, ZL_DIST( 30 ), ZL_DIST( 8 ), ZL_DIST( 100 ) );
	UTIL_SetOrigin( pev, pev->origin );

	m_fLocked = ( pev->spawnflags & SF_ZDOOR_LOCKED ) != 0;
	m_flOpenStart = 0;
	if ( m_fLocked )
		ZL_SpawnLock( this, 9.75f, m_hLock );
}

// anyway: whether he'd be in place to open it if it weren't for the key
int CZeldaDoor::PlayerSide( CLinkPlayer *pLink, BOOL anyway )
{
	if ( m_flOpenStart > 0 || !pLink || ( !anyway && ZL_DoorLockedOut( pLink, m_fLocked ) ) )
		return 0;
	return ZL_DoorPlayerSide( this, pLink, 20.0f, 20.0f );
}

void CZeldaDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CLinkPlayer *pLink = ZL_GetLink( pActivator );
	int side = PlayerSide( pLink );
	if ( !side )
		return;

	// EnDoor_Idle / _Open: a small key goes into the lock, the chains come off over 10 frames
	// (lockTimer) and Link waits for them (doorTimer)
	BOOL unlocking = m_fLocked;
	if ( m_fLocked )
	{
		pLink->m_ZInv.smallKeys--;
		m_fLocked = FALSE;
		ZL_EmitSound( edict(), CHAN_ITEM, ZS_EV_CHAIN_KEY_UNLOCK, 1, ATTN_NORM, PITCH_NORM );
		ZL_UnlockParts( m_hLock );
	}
	m_flWait = unlocking ? ZL_DOOR_LOCK_TIME : 0.0f;

	// From behind (doorDirection < 0) it's DOOR_OPEN_ANIM_ADULT_L and link_demo_doorA, facing the
	// way the door does; from in front, _R and doorB. He stands 22 from it.
	Vector fwd;
	UTIL_MakeVectorsPrivate( Vector( 0, pev->angles.y, 0 ), fwd, NULL, NULL );
	Vector spot = pev->origin + fwd * ( side * ZL_DIST( ZL_DOOR_STAND_DIST ) );
	spot.z = pLink->pev->origin.z;

	// It's open while he goes through (no collision until it has swung shut)
	pev->solid = SOLID_NOT;
	UTIL_SetOrigin( pev, pev->origin );

	ZL_DoorStartLink( pLink, spot, side < 0 ? pev->angles.y : pev->angles.y + 180.0f,
					  side < 0 ? ZDOOR_HINGED_L : ZDOOR_HINGED_R, unlocking );

	if ( pev->modelindex )
	{
		pev->sequence = LookupSequence( side < 0 ? "open_l" : "open_r" );
		pev->frame = 0;
		ResetSequenceInfo();
		pev->framerate = unlocking ? 0.0f : ZL_DOOR_ANIM_RATE;
	}

	m_flOpenStart = gpGlobals->time;
	m_iOpenStep = 0;
	SetThink( &CZeldaDoor::OpenThink );
	pev->nextthink = gpGlobals->time + 0.05f;

	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
}

// EnDoor_Open
void CZeldaDoor::OpenThink( void )
{
	pev->nextthink = gpGlobals->time + 0.05f;

	float t = gpGlobals->time - m_flOpenStart - m_flWait;
	if ( t < 0 )
		return;
	if ( pev->modelindex && pev->framerate == 0 && m_iOpenStep == 0 )
	{
		pev->frame = 0;
		ResetSequenceInfo();
		pev->framerate = ZL_DOOR_ANIM_RATE;
	}
	if ( pev->modelindex )
		StudioFrameAdvance();

	if ( m_iOpenStep == 0 && t >= ZL_DOOR_OPEN_TIME )
	{
		ZL_EmitSound( edict(), CHAN_BODY, ZS_OC_DOOR_OPEN, 1, ATTN_NORM, PITCH_NORM );
		m_iOpenStep = 1;
	}
	else if ( m_iOpenStep == 1 && t >= ZL_DOOR_CLOSE_TIME )
	{
		ZL_EmitSound( edict(), CHAN_ITEM, ZS_EV_DOOR_CLOSE, 1, ATTN_NORM, PITCH_NORM );
		m_iOpenStep = 2;
	}

	// Shut, and Link through (or out of the way of) the doorway
	if ( m_iOpenStep == 2 && t >= ZL_DOOR_SWING_TIME )
	{
		BOOL clear = TRUE;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CLinkPlayer *pLink = ZL_GetLink( UTIL_PlayerByIndex( i ) );
			if ( pLink && pLink->ZAction() == ZACT_DOOR && pLink->ZActionTime() < ZL_DOOR_TIME + m_flWait &&
				 ( pLink->pev->origin - pev->origin ).Length2D() < ZL_DIST( 40 ) )
				clear = FALSE;
		}
		if ( !clear )
			return;

		if ( pev->modelindex )
		{
			pev->sequence = LookupSequence( "idle" );
			pev->frame = 0;
			ResetSequenceInfo();
			pev->framerate = 0;
		}
		pev->solid = SOLID_BBOX;
		UTIL_SetOrigin( pev, pev->origin );
		m_flOpenStart = 0;
		SetThink( NULL );
		pev->nextthink = 0;
	}
}

/*
=============================================================================
 zl_door_shutter (Door_Shutter)
=============================================================================
*/
enum
{
	ZSHUTTER_IDLE,
	ZSHUTTER_OPEN,		// DoorShutter_Open: up 200, waiting for Link to be through
	ZSHUTTER_CLOSE,		// DoorShutter_Close
};

class CZeldaShutter : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ShutterThink( void );
	int ObjectCaps( void ) { return CBaseAnimating::ObjectCaps() | FCAP_IMPULSE_USE; }
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int PlayerSide( CLinkPlayer *pLink, BOOL anyway = FALSE );

	BOOL m_fLocked;
	int m_iState;
	float m_flWait;			// unlockTimer: when it starts up (DECR( unlockTimer ) == 0)
	EHANDLE m_hLock[4];
	float m_flSpeed;		// actor.velocity.y, OoT units a frame
	float m_flHomeZ;
	EHANDLE m_hOpener;
};

LINK_ENTITY_TO_CLASS( zl_door_shutter, CZeldaShutter );

TYPEDESCRIPTION CZeldaShutter::m_SaveData[] =
{
	DEFINE_FIELD( CZeldaShutter, m_fLocked, FIELD_BOOLEAN ),
	DEFINE_FIELD( CZeldaShutter, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CZeldaShutter, m_flSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CZeldaShutter, m_flHomeZ, FIELD_FLOAT ),
	DEFINE_FIELD( CZeldaShutter, m_hOpener, FIELD_EHANDLE ),
	DEFINE_FIELD( CZeldaShutter, m_flWait, FIELD_TIME ),
	DEFINE_ARRAY( CZeldaShutter, m_hLock, FIELD_EHANDLE, 4 ),
};
IMPLEMENT_SAVERESTORE( CZeldaShutter, CBaseAnimating );

void CZeldaShutter::Precache( void )
{
	if ( ZL_FileExists( ZL_MDL_SHUTTER ) )
		PRECACHE_MODEL( (char *)ZL_MDL_SHUTTER );
	ZL_PrecacheLock();
}

void CZeldaShutter::Spawn( void )
{
	Precache();

	if ( ZL_FileExists( ZL_MDL_SHUTTER ) )
		SET_MODEL( ENT( pev ), ZL_MDL_SHUTTER );
	else
		ALERT( at_console, "zl_door_shutter: no %s (tools/zelda/make_object_models.py)\n", ZL_MDL_SHUTTER );

	// Moved a frame at a time through velocity (so it slides smoothly), into the wall above it
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_BBOX;
	// gDungeonDoorDL: 80 across, 110 high, 20 thick
	ZL_DoorSetSize( this, ZL_DIST( 40 ), ZL_DIST( 10 ), ZL_DIST( 110 ) );
	UTIL_SetOrigin( pev, pev->origin );

	m_fLocked = ( pev->spawnflags & SF_ZDOOR_LOCKED ) != 0;
	m_iState = ZSHUTTER_IDLE;
	m_flHomeZ = pev->origin.z;
	m_flSpeed = 0;
	// Matrix_Scale( 0.01, 0.01, 0.025 ) then 500 out
	if ( m_fLocked )
		ZL_SpawnLock( this, 12.5f, m_hLock );
}

int CZeldaShutter::PlayerSide( CLinkPlayer *pLink, BOOL anyway )
{
	if ( m_iState != ZSHUTTER_IDLE || !pLink || ( !anyway && ZL_DoorLockedOut( pLink, m_fLocked ) ) )
		return 0;
	// sGfxInfo[DOORSHUTTER_GFX_GENERIC]: 20 to the sides, 15 up and down
	return ZL_DoorPlayerSide( this, pLink, 20.0f, 15.0f );
}

void CZeldaShutter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CLinkPlayer *pLink = ZL_GetLink( pActivator );
	int side = PlayerSide( pLink );
	if ( !side )
		return;

	// DoorShutter_Idle: unlocking takes a small key; the chains come off over 10 frames and it
	// only starts up after that, Link waiting for it (doorTimer)
	BOOL unlocking = m_fLocked;
	if ( m_fLocked )
	{
		pLink->m_ZInv.smallKeys--;
		m_fLocked = FALSE;
		ZL_EmitSound( edict(), CHAN_ITEM, ZS_EV_CHAIN_KEY_UNLOCK, 1, ATTN_NORM, PITCH_NORM );
		ZL_UnlockParts( m_hLock );
	}
	m_flWait = gpGlobals->time + ( unlocking ? ZL_DOOR_LOCK_TIME : 0.0f );

	// He faces straight through (yaw = home.rot.y, turned round from in front) and walks on from
	// where he is
	ZL_DoorStartLink( pLink, pLink->pev->origin, side > 0 ? pev->angles.y + 180.0f : pev->angles.y, ZDOOR_SLIDING, unlocking );

	pev->solid = SOLID_NOT;
	UTIL_SetOrigin( pev, pev->origin );
	m_iState = ZSHUTTER_OPEN;
	m_flSpeed = 0;
	m_hOpener = pLink;

	SetThink( &CZeldaShutter::ShutterThink );
	pev->nextthink = gpGlobals->time + ZL_FRAMES( 1.0f );

	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
}

static float ZL_StepTo( float cur, float target, float step )
{
	if ( cur < target )
		return min( target, cur + step );
	return max( target, cur - step );
}

void CZeldaShutter::ShutterThink( void )
{
	pev->nextthink = gpGlobals->time + ZL_FRAMES( 1.0f );

	float z = ( pev->origin.z - m_flHomeZ ) / ZL_UNIT;		// OoT units up

	if ( m_iState == ZSHUTTER_OPEN )
	{
		if ( gpGlobals->time < m_flWait - 0.001f )
			return;

		// DoorShutter_UpdateOpening's first frame
		if ( m_flSpeed == 0 && z <= 0 )
			ZL_EmitSound( edict(), CHAN_BODY, ZS_EV_SLIDE_DOOR_OPEN, 1, ATTN_NORM, PITCH_NORM );

		// Math_StepToF( &velocity.y, 15, 3 ), then up toward home + 200
		m_flSpeed = ZL_StepTo( m_flSpeed, 15.0f, 3.0f );
		float next = ZL_StepTo( z, 200.0f, m_flSpeed );
		pev->velocity = Vector( 0, 0, ( next - z ) * ZL_UNIT * ZL_FPS );
		if ( next < 200.0f )
			return;

		// Up: it shuts once Link is more than 50 away (xzDistToPlayer)
		CBaseEntity *pOpener = m_hOpener;
		if ( pOpener && pOpener->IsAlive() && ( pOpener->pev->origin - pev->origin ).Length2D() <= ZL_DIST( 50 ) )
			return;

		ZL_EmitSound( edict(), CHAN_BODY, ZS_EV_SLIDE_DOOR_CLOSE, 1, ATTN_NORM, PITCH_NORM );
		m_iState = ZSHUTTER_CLOSE;
		m_flSpeed = 0;
		pev->solid = SOLID_BBOX;
		UTIL_SetOrigin( pev, pev->origin );
		return;
	}

	if ( m_iState == ZSHUTTER_CLOSE )
	{
		// Math_StepToF( &velocity.y, 20, 8 ), down to home
		if ( m_flSpeed < 20.0f )
			m_flSpeed = ZL_StepTo( m_flSpeed, 20.0f, 8.0f );
		float next = ZL_StepTo( z, 0.0f, m_flSpeed );
		pev->velocity = Vector( 0, 0, ( next - z ) * ZL_UNIT * ZL_FPS );
		if ( next > 0.0f )
			return;

		// Down: NA_SE_EV_STONE_BOUND and Quake_Request( 3 ) for 10 frames
		ZL_EmitSound( edict(), CHAN_ITEM, ZS_EV_STONE_BOUND, 1, ATTN_NORM, PITCH_NORM );
		UTIL_ScreenShake( pev->origin, 2.0f, 40.0f, ZL_FRAMES( 10.0f ), ZL_DIST( 400 ) );

		pev->velocity = g_vecZero;
		pev->origin.z = m_flHomeZ;
		UTIL_SetOrigin( pev, pev->origin );
		m_iState = ZSHUTTER_IDLE;
		m_hOpener = NULL;
		SetThink( NULL );
		pev->nextthink = 0;
	}
}

/*
=============================================================================
 For the A button (CLinkPlayer::ZUpdateDoAction)
=============================================================================
*/
BOOL ZL_IsDoor( CBaseEntity *pEnt )
{
	return FClassnameIs( pEnt->pev, "zl_door" ) || FClassnameIs( pEnt->pev, "zl_door_shutter" );
}

// Navi's hint for a door Link is at but can't open: the name of her text, or NULL
const char *ZL_DoorNaviText( CBaseEntity *pEnt, CLinkPlayer *pLink )
{
	if ( !pLink || pLink->m_ZInv.smallKeys > 0 )
		return NULL;
	if ( FClassnameIs( pEnt->pev, "zl_door" ) )
	{
		CZeldaDoor *pDoor = (CZeldaDoor *)pEnt;
		return ( pDoor->m_fLocked && pDoor->PlayerSide( pLink, TRUE ) ) ? ZL_NAVI_DOOR_LOCKED : NULL;
	}
	if ( FClassnameIs( pEnt->pev, "zl_door_shutter" ) )
	{
		CZeldaShutter *pDoor = (CZeldaShutter *)pEnt;
		return ( pDoor->m_fLocked && pDoor->PlayerSide( pLink, TRUE ) ) ? ZL_NAVI_DOOR_LOCKED : NULL;
	}
	return NULL;
}

BOOL ZL_DoorCanOpen( CBaseEntity *pEnt, CLinkPlayer *pLink )
{
	if ( FClassnameIs( pEnt->pev, "zl_door" ) )
		return ( (CZeldaDoor *)pEnt )->PlayerSide( pLink ) != 0;
	if ( FClassnameIs( pEnt->pev, "zl_door_shutter" ) )
		return ( (CZeldaShutter *)pEnt )->PlayerSide( pLink ) != 0;
	return FALSE;
}
