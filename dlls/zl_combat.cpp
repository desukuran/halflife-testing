/***
*
*	Zelda 64 port: combat.
*
*	- The "function in the middle" that turns Half-Life damage into OoT hearts and Link's
*	  OoT damage values into Half-Life damage (ZL_HLDamageToHealth / ZL_OoTDamageToHL).
*	- Taking hits: invincibility frames, roll i-frames, shield blocks, knockback
*	  (func_80837C0C, Player_UpdateDamage).
*	- OoT fall damage (func_80843E64).
*	- Sword swings (meleeWeaponQuads in OoT, a swept arc here).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "gamerules.h"
#include "decals.h"
#include "zl_player.h"

#include <math.h>

/*
=============================================================================
 Damage translation
=============================================================================
*/

// Half-Life damage -> OoT health units (16 per heart). Hearts drop in quarters on the meter,
// so every hit is rounded up to a quarter heart. Capped at four hearts so a Gargantua stomp
// is a Ganon-sized hit, not an instant death at full health.
int ZL_HLDamageToHealth( float flDamage, int bitsDamageType )
{
	if ( flDamage <= 0 )
		return 0;

	float units = flDamage * zl_dmg_in_scale.value;
	int quarters = (int)ceil( units / 4.0f );
	int result = quarters * 4;

	if ( result < 4 )
		result = 4;
	if ( result > 4 * ZL_HEART )
		result = 4 * ZL_HEART;

	return result;
}

// Link's OoT damage (sword 1/2/4, arrows 2, bombs 2...) -> Half-Life damage
float ZL_OoTDamageToHL( int ootDamage )
{
	return ootDamage * zl_dmg_out_scale.value;
}

/*
=============================================================================
 Taking damage
=============================================================================
*/

// Shield (PLAYER_STATE1_SHIELDING): frontal hits that a shield can stop
BOOL CLinkPlayer::ZTryBlock( entvars_t *pevInflictor, int bitsDamageType )
{
	if ( !( pev->iuser4 & ZL_STATE_SHIELDING ) && ZAction() != ZACT_SHIELD )
		return FALSE;

	if ( !pevInflictor || pevInflictor == pev )
		return FALSE;

	// Explosions, falls, drowning, radiation and sound go around a shield
	if ( !( bitsDamageType & ( DMG_SLASH | DMG_CLUB | DMG_BULLET | DMG_ACID | DMG_SHOCK | DMG_ENERGYBEAM | DMG_GENERIC ) ) ||
		 ( bitsDamageType & ( DMG_BLAST | DMG_FALL | DMG_DROWN | DMG_RADIATION | DMG_SONIC | DMG_NERVEGAS | DMG_POISON ) ) )
		return FALSE;

	// The Deku Shield burns (DMG_BURN never gets here because it is not in the list above)
	Vector delta = pevInflictor->origin - pev->origin;
	delta.z = 0;
	if ( delta.Length() < 1 )
		return FALSE;

	if ( DotProduct( delta.Normalize(), ZForward() ) < 0.34f )	// ~70 degrees either side
		return FALSE;

	ZSound( CHAN_ITEM, ZS_IT_SHIELD_REFLECT_SW );
	UTIL_Sparks( pev->origin + ZForward() * 20 + Vector( 0, 0, 10 ) );
	return TRUE;
}

// func_80837C0C: flinch or get knocked down
void CLinkPlayer::ZApplyKnockback( entvars_t *pevInflictor, int healthUnits, int bitsDamageType )
{
	Vector away = -ZForward();

	if ( pevInflictor && pevInflictor != pev )
	{
		away = pev->origin - pevInflictor->origin;
		away.z = 0;
		if ( away.Length() < 1 )
			away = -ZForward();
		away = away.Normalize();
	}

	int act = ZAction();
	BOOL locked = ( pev->iuser4 & ZL_STATE_LOCKED ) != 0;

	// Big hits and explosions knock Link off his feet (link_normal_front_downA)
	if ( ( bitsDamageType & DMG_BLAST ) || healthUnits >= ZL_HEART * 2 )
	{
		ZSetAction( ZACT_KNOCKDOWN );
		pev->velocity = away * ZL_VEL( 6.0f );
		pev->velocity.z = ZL_VEL( 6.0f );
		pev->flags &= ~FL_ONGROUND;
		ZSound( CHAN_VOICE, ZS_VO_FALL_L );
		return;
	}

	// Running full tilt and not locked on: keep running (unk_890 = 20)
	Vector flat = pev->velocity;
	flat.z = 0;
	if ( act == ZACT_STAND && flat.Length() > ZL_VEL( 4.0f ) && !locked )
	{
		ZSound( CHAN_VOICE, ZS_VO_DAMAGE_S );
		return;
	}

	if ( act == ZACT_STAND || act == ZACT_SLASH || act == ZACT_SPIN_CHARGE || act == ZACT_LAND || act == ZACT_SHIELD ||
		 act == ZACT_AIM || act == ZACT_CUP || act == ZACT_DAMAGE )
	{
		ZSetAction( ZACT_DAMAGE );
		// colChkInfo.damage >= 5 gives the big shove (speedXZ 23, toned down here)
		pev->velocity = away * ZL_VEL( healthUnits >= 5 ? 12.0f : 3.0f );
	}

	ZSound( CHAN_VOICE, ZS_VO_DAMAGE_S );
}

int CLinkPlayer::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( !LinkActive() )
		return CBasePlayer::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

	if ( pev->takedamage == DAMAGE_NO || !IsAlive() )
		return 0;

	int act = ZAction();

	// Nobody hits Link while he is reading
	if ( act == ZACT_TALK || act == ZACT_GETITEM || m_hZTalkTo != NULL )
		return 0;

	// Intangibility after a hit (invincibilityTimer)
	if ( m_flZInvincibleUntil > gpGlobals->time && !( bitsDamageType & DMG_DROWN ) )
		return 0;

	// Player_Action_Roll: Player_SetInvulnerability at frame 8
	if ( act == ZACT_ROLL && ZActionTime() >= ZL_ROLL_INVULN_START &&
		 ZActionTime() < ZL_ROLL_INVULN_START + ZL_ROLL_INVULN_TIME && !( bitsDamageType & ( DMG_FALL | DMG_DROWN ) ) )
		return 0;

	if ( ZTryBlock( pevInflictor, bitsDamageType ) )
		return 0;

	int units = m_fZDirectDamage ? (int)flDamage : ZL_HLDamageToHealth( flDamage, bitsDamageType );

	// Health_ChangeBy: double defense halves every hit
	if ( m_fZDoubleDefense )
		units = ( units + 1 ) / 2;

	if ( units <= 0 )
		return 0;

	// Let Half-Life do the bookkeeping (death, time based damage, HUD damage bits), in OoT units
	int ret = CBasePlayer::TakeDamage( pevInflictor, pevAttacker, (float)units, bitsDamageType );

	if ( ret && IsAlive() )
	{
		m_flZInvincibleUntil = gpGlobals->time + ZL_INVINCIBILITY_TIME;

		if ( m_hZHeldBomb != NULL )
			ZThrowBomb( TRUE );

		if ( !( bitsDamageType & ( DMG_FALL | DMG_DROWN ) ) )
			ZApplyKnockback( pevInflictor, units, bitsDamageType );

		ZSound( CHAN_BODY, ZS_PL_DAMAGE, 0.7f );
	}

	return ret;
}

// func_80843E64 / D_80854600: half a heart from 400 units, a full heart from 800
void CLinkPlayer::ZFallDamageThink( void )
{
	BOOL onground = ( pev->flags & FL_ONGROUND ) != 0;

	if ( !onground )
	{
		if ( m_fZWasOnGround || pev->waterlevel > 0 || ZAction() == ZACT_HOOKFLY || ZAction() == ZACT_CLIMB )
			m_flZFallStartZ = pev->origin.z;
		else if ( pev->origin.z > m_flZFallStartZ )
			m_flZFallStartZ = pev->origin.z;
	}
	else if ( !m_fZWasOnGround )
	{
		float fallDistance = ( m_flZFallStartZ - pev->origin.z ) / ZL_UNIT;
		int damage = 0;

		if ( pev->waterlevel == 0 )
		{
			if ( fallDistance >= 800 )
				damage = 16;
			else if ( fallDistance >= 400 )
				damage = 8;
		}

		if ( damage )
		{
			m_fZDirectDamage = TRUE;
			TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), damage, DMG_FALL );
			m_fZDirectDamage = FALSE;

			// Player_SetIntangibility( this, 40 )
			m_flZInvincibleUntil = gpGlobals->time + ZL_FRAMES( 40 );
			UTIL_ScreenShake( pev->origin, 4, 30, 0.5f, 64 );
			ZSound( CHAN_BODY, ZS_PL_BODY_HIT );
			ZSound( CHAN_VOICE, ZS_VO_LAND_DAMAGE_S );
		}
	}

	m_fZWasOnGround = onground;

	// Half-Life's own fall damage is replaced by the above
	m_flFallVelocity = 0;
}

/*
=============================================================================
 Sword
=============================================================================
*/

// Reach of each sword from Link's centre, in HL units
static float ZL_SwordReach( int equipment )
{
	if ( equipment & ZEQ_SWORD_BIGGORON )
		return 112;
	if ( equipment & ZEQ_SWORD_MASTER )
		return 84;
	return 72;
}

void CLinkPlayer::ZSwordSweep( float minDeg, float maxDeg, float reach, float height, int ootDamage, int dmgBits )
{
	Vector center = pev->origin + Vector( 0, 0, height );
	float facing = ZFacing();
	float hlDamage = ZL_OoTDamageToHL( ootDamage );

	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = UTIL_FindEntityInSphere( pEnt, pev->origin, reach + 64 ) ) != NULL )
	{
		if ( pEnt == this || pEnt->pev->takedamage == DAMAGE_NO || pEnt->pev->solid == SOLID_NOT )
			continue;
		if ( pEnt->pev->solid == SOLID_TRIGGER )
			continue;

		// One hit per swing per thing
		BOOL already = FALSE;
		for ( int i = 0; i < m_iZSwingHitCount; i++ )
		{
			if ( m_hZSwingHits[i] == pEnt )
				already = TRUE;
		}
		if ( already )
			continue;

		Vector targetCenter = pEnt->Center();
		Vector delta = targetCenter - pev->origin;
		delta.z = 0;

		// Distance to the edge of the target's box
		float radius = ( pEnt->pev->size.x + pEnt->pev->size.y ) * 0.25f;
		float dist = delta.Length() - radius;
		if ( dist > reach )
			continue;

		// Vertical overlap with Link's body
		if ( pEnt->pev->absmax.z < pev->absmin.z - 8 || pEnt->pev->absmin.z > pev->absmax.z + 16 )
			continue;

		// Inside the swing arc, widened by the target's angular size
		float yawTo = UTIL_VecToYaw( delta );
		float rel = UTIL_AngleDiff( yawTo, facing );
		float slack = delta.Length() > 1 ? atan( radius / delta.Length() ) * ( 180.0 / M_PI ) : 180.0f;
		if ( rel < minDeg - slack || rel > maxDeg + slack )
			continue;

		// No hitting through walls
		TraceResult tr;
		UTIL_TraceLine( center, targetCenter, dont_ignore_monsters, edict(), &tr );
		if ( tr.flFraction < 1.0f && tr.pHit != pEnt->edict() )
		{
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				continue;
		}

		if ( m_iZSwingHitCount < (int)ARRAYSIZE( m_hZSwingHits ) )
			m_hZSwingHits[m_iZSwingHitCount++] = pEnt;

		if ( tr.pHit != pEnt->edict() )
		{
			// Fake a trace onto the target so blood and decals land somewhere sensible
			tr.pHit = pEnt->edict();
			tr.vecEndPos = targetCenter;
			tr.iHitgroup = 0;
		}

		Vector dir = ( targetCenter - center ).Normalize();

		ClearMultiDamage();
		pEnt->TraceAttack( pev, hlDamage, dir, &tr, dmgBits );
		ApplyMultiDamage( pev, pev );

		if ( pEnt->Classify() != CLASS_NONE && pEnt->Classify() != CLASS_MACHINE && pEnt->MyMonsterPointer() )
			ZSound( CHAN_ITEM, ZS_IT_SWORD_HIT );
		else
			ZSound( CHAN_ITEM, ZS_IT_SWORD_STRIKE );
	}

	// Sword on a wall: sparks and the "clink" (NA_SE_IT_SWORD_STRIKE)
	if ( !m_fZSwingHitWorld )
	{
		float mid = ( minDeg + maxDeg ) * 0.5f;
		Vector ang( 0, facing + mid, 0 );
		Vector fwd, right, up;
		UTIL_MakeVectorsPrivate( ang, fwd, right, up );

		TraceResult tr;
		UTIL_TraceLine( center, center + fwd * reach, ignore_monsters, edict(), &tr );
		if ( tr.flFraction < 1.0f )
		{
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( pHit && pHit->IsBSPModel() && pHit->pev->takedamage == DAMAGE_NO )
			{
				const char *tex = TRACE_TEXTURE( tr.pHit, center, center + fwd * reach );
				if ( !tex || stricmp( tex, "sky" ) )
				{
					m_fZSwingHitWorld = TRUE;
					UTIL_Sparks( tr.vecEndPos );
					ZSound( CHAN_ITEM, ZS_IT_SWORD_STRIKE );
				}
			}
		}
	}
}

void CLinkPlayer::ZSwordThink( void )
{
	int act = ZAction();
	int mwa = ZL_GetMWA( pev->iuser4 );
	float t = ZActionTime();
	float reach = ZL_SwordReach( m_ZInv.equipment );

	// A new swing starts a new hit list. The id changes with the swing type and combo count.
	int swingId = act | ( mwa << 8 ) | ( ZL_GetCombo( pev->iuser4 ) << 16 );
	BOOL swinging = ( act == ZACT_SLASH || act == ZACT_SPIN || act == ZACT_JUMPSLASH || act == ZACT_JUMPSLASH_END );

	if ( !swinging )
	{
		m_iZLastSwingId = 0;
		return;
	}

	if ( swingId != m_iZLastSwingId || ( act == ZACT_SLASH && t < 0.03f ) )
	{
		if ( swingId != m_iZLastSwingId )
		{
			m_iZSwingHitCount = 0;
			m_fZSwingHitWorld = FALSE;
		}
		m_iZLastSwingId = swingId;
	}

	int dmg = ZSwordDamage( mwa );

	switch ( act )
	{
	case ZACT_SLASH:
		// D_80854190 active frames, roughly the first half of the swing
		if ( t < 0.04f || t > 0.24f )
			return;

		switch ( mwa )
		{
		case ZMWA_STAB_1H:
		case ZMWA_STAB_COMBO_1H:
			ZSwordSweep( -14, 14, reach * 1.25f, 24, dmg, DMG_SLASH );
			break;
		case ZMWA_FORWARD_SLASH_1H:
		case ZMWA_FORWARD_COMBO_1H:
			ZSwordSweep( -28, 28, reach * 1.1f, 20, dmg, DMG_SLASH );
			break;
		default:	// right / left horizontal slashes and their finishers
			ZSwordSweep( -85, 85, reach, 18, dmg, DMG_SLASH );
			break;
		}
		break;

	case ZACT_JUMPSLASH:
		// The downswing: hits on the way down
		if ( pev->velocity.z < 0 )
			ZSwordSweep( -40, 40, reach * 1.15f, 0, dmg, DMG_SLASH | DMG_CLUB );
		break;

	case ZACT_JUMPSLASH_END:
		if ( t < 0.15f )
			ZSwordSweep( -40, 40, reach * 1.15f, 0, dmg, DMG_SLASH | DMG_CLUB );
		break;

	case ZACT_SPIN:
		if ( t > 0.03f && t < ZL_SPIN_TIME * 0.8f )
		{
			float scale = ( mwa == ZMWA_BIG_SPIN_1H ) ? 1.6f : 1.15f;

			// Great spin costs magic once (Magic_RequestChange in OoT)
			if ( mwa == ZMWA_BIG_SPIN_1H && t < 0.1f && m_iZSwingHitCount == 0 && !m_fZSwingHitWorld )
				ZUseMagic( 12 );

			ZSwordSweep( -180, 180, reach * scale, 16, dmg, DMG_SLASH );
		}
		break;
	}
}

/*
=============================================================================
 Drops (Item_DropCollectibleRandom)
=============================================================================
*/
void ZL_MonsterKilled( CBaseMonster *pMonster, entvars_t *pevAttacker )
{
	if ( !ZL_Enabled() || !zl_drops.value || !pMonster || pMonster->IsPlayer() )
		return;
	if ( !( pMonster->pev->flags & FL_MONSTER ) )
		return;

	CLinkPlayer *pLink = ZL_GetLink( UTIL_PlayerByIndex( 1 ) );
	if ( !pLink )
		return;

	// Only things Link would have fought drop goodies
	if ( pLink->IRelationship( pMonster ) <= R_NO )
		return;

	const char *drop = NULL;
	int roll = RANDOM_LONG( 0, 99 );
	BOOL lowHealth = pLink->pev->health <= ZL_HEART;

	if ( lowHealth && roll < 50 )
		drop = "item_zl_heart";
	else if ( roll < 25 )
		drop = "item_zl_rupee";
	else if ( roll < 35 )
		drop = "item_zl_heart";
	else if ( roll < 41 )
		drop = "item_zl_rupee_blue";
	else if ( roll < 47 && ( pLink->m_ZInv.items & ( 1 << ZITEM_BOW ) ) )
		drop = "item_zl_arrows";
	else if ( roll < 53 && ( pLink->m_ZInv.items & ( 1 << ZITEM_BOMB ) ) )
		drop = "item_zl_bombs";
	else if ( roll < 58 && pLink->m_iZMagicCapacity > 0 )
		drop = "item_zl_magic";
	else if ( roll < 60 )
		drop = "item_zl_rupee_red";

	if ( !drop )
		return;

	CBaseEntity *pItem = CBaseEntity::Create( (char *)drop, pMonster->Center() + Vector( 0, 0, 8 ), g_vecZero, pMonster->edict() );
	if ( pItem )
	{
		// Pop up out of the body like OoT drops
		pItem->pev->velocity = Vector( RANDOM_FLOAT( -40, 40 ), RANDOM_FLOAT( -40, 40 ), 180 );
	}
}
