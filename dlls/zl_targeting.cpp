/***
*
*	Zelda 64 port: Z-targeting, the attention system (z_actor.c Attention_*,
*	z_player.c Player_UpdateZTargeting / Player_UpdateHostileLockOn).
*
*	The server picks the actor. The client draws the arrow / reticle and feeds the focus
*	point back through usercmd impact_position so pm_zelda steers toward it.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "zl_player.h"

// sAttentionRanges: lock-on range and leash multiplier (attentionRangeSq, leashScale)
struct zl_attention_range_t
{
	float range;
	float leash;
};

static const zl_attention_range_t sAttentionRanges[] =
{
	{ 70, 140 },		// ATTENTION_RANGE_0
	{ 170, 255 },		// ATTENTION_RANGE_1
	{ 280, 5600 },		// ATTENTION_RANGE_2
	{ 350, 525 },		// ATTENTION_RANGE_3 (actor default)
	{ 700, 1050 },		// ATTENTION_RANGE_4
	{ 1000, 1500 },		// ATTENTION_RANGE_5
	{ 100, 105.36842f },// ATTENTION_RANGE_6
	{ 140, 163.33333f },// ATTENTION_RANGE_7
	{ 240, 576 },		// ATTENTION_RANGE_8
	{ 280, 280000 },	// ATTENTION_RANGE_9
};

// Which range type a Half-Life monster gets. Big things can be locked from further away.
static int ZL_AttentionRangeType( CBaseEntity *pEnt, BOOL hostile )
{
	const char *cls = STRING( pEnt->pev->classname );

	if ( !hostile )
		return 1;	// NPCs: ATTENTION_RANGE_1, you walk up to talk

	if ( !strcmp( cls, "monster_gargantua" ) || !strcmp( cls, "monster_bigmomma" ) ||
		 !strcmp( cls, "monster_apache" ) || !strcmp( cls, "monster_osprey" ) ||
		 !strcmp( cls, "monster_nihilanth" ) || !strcmp( cls, "monster_tentacle" ) ||
		 !strcmp( cls, "monster_ichthyosaur" ) )
		return 5;	// bosses

	if ( !strcmp( cls, "monster_human_grunt" ) || !strcmp( cls, "monster_alien_grunt" ) ||
		 !strcmp( cls, "monster_human_assassin" ) || !strcmp( cls, "monster_alien_controller" ) ||
		 !strcmp( cls, "monster_turret" ) || !strcmp( cls, "monster_miniturret" ) ||
		 !strcmp( cls, "monster_sentry" ) )
		return 4;	// ranged enemies

	return 3;
}

// Things we never lock onto
static BOOL ZL_IgnoredForTargeting( CBaseEntity *pEnt )
{
	const char *cls = STRING( pEnt->pev->classname );

	return !strcmp( cls, "monster_cockroach" ) || !strcmp( cls, "monster_flyer" ) ||
		   !strcmp( cls, "monster_furniture" ) || !strcmp( cls, "monster_generic" ) ||
		   !strcmp( cls, "monster_tripmine" ) || !strcmp( cls, "monster_satchel" ) ||
		   !strcmp( cls, "monster_snark" ) || !strcmp( cls, "hornet" ) ||
		   ( !strcmp( cls, "monster_barnacle" ) && pEnt->pev->deadflag != DEAD_NO );
}

BOOL CLinkPlayer::ZIsTargetable( CBaseEntity *pEnt, float *pRange, float *pLeash )
{
	if ( !pEnt || pEnt == this || pEnt->IsPlayer() )
		return FALSE;
	if ( !( pEnt->pev->flags & FL_MONSTER ) || pEnt->pev->takedamage == DAMAGE_NO )
		return FALSE;
	if ( !pEnt->IsAlive() || ( pEnt->pev->effects & EF_NODRAW ) )
		return FALSE;
	if ( pEnt->pev->rendermode != kRenderNormal && pEnt->pev->renderamt < 30 )
		return FALSE;
	if ( ZL_IgnoredForTargeting( pEnt ) )
		return FALSE;

	BOOL hostile = IRelationship( pEnt ) > R_NO;
	const zl_attention_range_t &r = sAttentionRanges[ZL_AttentionRangeType( pEnt, hostile )];

	if ( pRange )
		*pRange = ZL_DIST( r.range );
	if ( pLeash )
		*pLeash = ZL_DIST( r.range ) * ( r.leash / r.range );

	return TRUE;
}

// Attention_FindActorInCategory + Attention_WeightedDistToPlayerSq
CBaseEntity *CLinkPlayer::ZFindTarget( CBaseEntity *pExclude, BOOL locked )
{
	CBaseEntity *pBest = NULL;
	float bestDistSq = 1e18f;
	float facing = ZFacing();
	Vector eye = pev->origin + pev->view_ofs;
	Vector camFwd, camRight, camUp;

	UTIL_MakeVectorsPrivate( Vector( 0, pev->v_angle.y, 0 ), camFwd, camRight, camUp );

	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = UTIL_FindEntityInSphere( pEnt, pev->origin, ZL_DIST( 1000 ) ) ) != NULL )
	{
		float range, leash;

		if ( pEnt == pExclude || !ZIsTargetable( pEnt, &range, &leash ) )
			continue;

		Vector focus = pEnt->Center();
		Vector delta = focus - pev->origin;
		float distSq = delta.Length() * delta.Length();

		if ( distSq > range * range )
			continue;

		// Yaw from the actor's point of view, against Link's shape yaw
		float yawTo = UTIL_VecToYaw( delta );
		float diff = fabs( UTIL_AngleDiff( yawTo, facing ) );
		float weighted;

		if ( locked )
		{
			// Switching targets: anything within 90 degrees, closer to straight ahead wins
			if ( diff > 90 )
				continue;
			weighted = distSq - distSq * 0.8f * ( ( 90.0f - diff ) / 180.0f );
		}
		else
		{
			// Fresh lock: has to be within about 60 degrees of where Link is facing
			if ( diff > 60 )
				continue;
			weighted = distSq;
		}

		// Attention_ActorOnScreen: roughly inside the camera's view
		Vector flat = delta;
		flat.z = 0;
		if ( flat.Length() > 32 && DotProduct( flat.Normalize(), camFwd ) < 0.2f )
			continue;

		// BgCheck_CameraLineTest1: nothing solid in the way
		TraceResult tr;
		UTIL_TraceLine( eye, focus, ignore_monsters, ignore_glass, edict(), &tr );
		if ( tr.flFraction < 1.0f && tr.pHit != pEnt->edict() )
			continue;

		if ( weighted < bestDistSq )
		{
			bestDistSq = weighted;
			pBest = pEnt;
		}
	}

	return pBest;
}

void CLinkPlayer::ZReleaseTarget( BOOL playSound )
{
	if ( m_hZTarget != NULL && playSound )
		ZSound( CHAN_STATIC, ZS_SY_LOCK_OFF, 0.6f, ATTN_NONE );
	m_hZTarget = NULL;
}

void CLinkPlayer::ZUpdateTargeting( void )
{
	CBaseEntity *pTarget = m_hZTarget;
	BOOL zHeld = ZHeld( ZPAD_Z ) != 0;
	BOOL zPressed = ZPressed( ZPAD_Z ) != 0;
	BOOL toggle = zl_ztoggle.value != 0;
	int act = ZAction();

	// Attention_ShouldReleaseLockOn: died, vanished or ran past the leash
	if ( pTarget )
	{
		float range, leash;
		if ( !ZIsTargetable( pTarget, &range, &leash ) || ( pTarget->Center() - pev->origin ).Length() > leash )
		{
			ZReleaseTarget( TRUE );
			pTarget = NULL;
		}
	}

	if ( act == ZACT_TALK || act == ZACT_DEAD || act == ZACT_AIM || act == ZACT_CUP )
	{
		m_hZHover = NULL;
	}
	else
	{
		// The arrow over the nearest thing Link could target (Navi hovering)
		m_hZHover = pTarget ? NULL : ZFindTarget( NULL, FALSE );
	}

	if ( zPressed )
	{
		if ( pTarget && ( toggle || gpGlobals->time - m_flZLockTime < 0.6f ) )
		{
			// Tapping Z again while locked moves to the next actor
			CBaseEntity *pNext = ZFindTarget( pTarget, TRUE );
			if ( pNext )
			{
				m_hZTarget = pNext;
				m_flZLockTime = gpGlobals->time;
				ZSound( CHAN_STATIC, IRelationship( pNext ) > R_NO ? ZS_SY_LOCK_ON : ZS_SY_LOCK_ON_HUMAN, 0.7f, ATTN_NONE );
			}
			else if ( toggle )
			{
				ZReleaseTarget( TRUE );
			}
		}
		else
		{
			CBaseEntity *pNew = m_hZHover;
			if ( !pNew )
				pNew = ZFindTarget( NULL, FALSE );

			if ( pNew )
			{
				m_hZTarget = pNew;
				m_flZLockTime = gpGlobals->time;
				ZSound( CHAN_STATIC, IRelationship( pNew ) > R_NO ? ZS_SY_LOCK_ON : ZS_SY_LOCK_ON_HUMAN, 0.7f, ATTN_NONE );
			}
			else
			{
				// Parallel: the camera swings behind Link (CAM_MODE_Z_PARALLEL)
				ZSound( CHAN_STATIC, ZS_SY_CAMERA_ZOOM_UP, 0.4f, ATTN_NONE );
			}
		}
	}
	else if ( !zHeld && !toggle && m_hZTarget != NULL )
	{
		// Hold mode: letting go of Z drops the lock
		m_hZTarget = NULL;
	}

	pTarget = m_hZTarget;

	int lockedIdx = pTarget ? pTarget->entindex() : 0;
	int hoverIdx = m_hZHover != NULL ? ( (CBaseEntity *)m_hZHover )->entindex() : 0;
	int flags = 0;

	if ( pTarget )
	{
		flags |= ZTGT_LOCKED;
		if ( IRelationship( pTarget ) > R_NO )
			flags |= ZTGT_HOSTILE;
	}
	if ( m_hZHover != NULL && IRelationship( m_hZHover ) > R_NO )
		flags |= ZTGT_HOVER_HOSTILE;

	if ( lockedIdx != m_iZTargetSent || hoverIdx != m_iZHoverSent || flags != m_iZTargetFlagsSent )
	{
		m_iZTargetSent = lockedIdx;
		m_iZHoverSent = hoverIdx;
		m_iZTargetFlagsSent = flags;

		MESSAGE_BEGIN( MSG_ONE, gmsgZTarget, NULL, pev );
			WRITE_SHORT( lockedIdx );
			WRITE_SHORT( hoverIdx );
			WRITE_BYTE( flags );
			// Height of the focus point above the entity origin, so the client aims the same place
			WRITE_COORD( pTarget ? ( pTarget->EyePosition().z - pTarget->pev->origin.z ) : 0 );
		MESSAGE_END();
	}
}
