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

	// CollisionCheck_SpawnShieldParticlesMetal: the streaks and NA_SE_IT_SHIELD_REFLECT_SW
	ZSound( CHAN_ITEM, ZS_IT_SHIELD_REFLECT_SW );
	ZL_SendFx( ZFX_SPARKS, pev->origin + ZForward() * 20 + Vector( 0, 0, 10 ) );
	return TRUE;
}

/*
 func_80837C0C: how Link takes a hit. Half-Life damage picks OoT's hit response the way the
 attacker's collider would (Player_UpdateDamage, func_808382DC):
   DMG_SHOCK                          PLAYER_HIT_RESPONSE_ELECTRIFIED
   DMG_BLAST, or a hit of 2 hearts+   HIT_SPECIAL_EFFECT_KNOCKBACK: KNOCKBACK_LARGE at 4 / 5
   anything else                      PLAYER_HIT_RESPONSE_NONE at 4 / 5
 DMG_CLUB of a heart or more        KNOCKBACK_SMALL at 3 / 6
 (the 2 heart and club rules are the port's: OoT knocks Link back with the attack's own
 effect or Actor_SetPlayerKnockback*, which Half-Life's monsters don't have).
*/
enum
{
	ZHR_NONE,
	ZHR_KNOCKBACK_LARGE,
	ZHR_KNOCKBACK_SMALL,
	ZHR_ELECTRIFIED,
};

void CLinkPlayer::ZApplyKnockback( entvars_t *pevInflictor, int healthUnits, int bitsDamageType )
{
	// yRot: the way the hit pushes him (Actor_WorldYawTowardActor( ac, player ))
	Vector away = -ZForward();
	if ( pevInflictor && pevInflictor != pev )
	{
		away = pev->origin - pevInflictor->origin;
		away.z = 0;
		if ( away.Length() < 1 )
			away = -ZForward();
		away = away.Normalize();
	}
	float pushYaw = UTIL_VecToYaw( away );
	float rel = UTIL_AngleDiff( pushYaw, ZFacing() );		// yRot -= shape.rot.y
	BOOL fromFront = fabs( rel ) > 90.0f;					// ABS( yRot ) > 0x4000

	int response = ZHR_NONE;
	if ( bitsDamageType & DMG_SHOCK )
		response = ZHR_ELECTRIFIED;
	else if ( ( bitsDamageType & DMG_BLAST ) || healthUnits >= ZL_HEART * 2 )
		response = ZHR_KNOCKBACK_LARGE;
	else if ( ( bitsDamageType & DMG_CLUB ) && healthUnits >= ZL_HEART )
		response = ZHR_KNOCKBACK_SMALL;

	BOOL onground = ( pev->flags & FL_ONGROUND ) != 0;
	int act = ZAction();

	if ( m_hZHeldBomb != NULL )
		ZThrowBomb( TRUE );			// func_80832564: Player_DetachHeldActor

	if ( response == ZHR_ELECTRIFIED )
	{
		// Player_AnimPlayLoopAdjusted( electric_shock ), 20 frames, NA_SE_VO_LI_TAKEN_AWAY all along
		ZSetAction( ZACT_SHOCK );
		pev->velocity.x = pev->velocity.y = 0;
		ZSound( CHAN_VOICE, ZS_VO_TAKEN_AWAY );
		m_flZShockUntil = gpGlobals->time + ZL_SHOCK_TIME + ZL_SHOCK_END_TIME;
		return;
	}

	if ( response == ZHR_KNOCKBACK_LARGE || response == ZHR_KNOCKBACK_SMALL || !onground ||
		 act == ZACT_CLIMB || act == ZACT_HOOKFLY )
	{
		// Player_Action_8084377C: off his feet
		ZSetAction( ZACT_KNOCKDOWN );
		int sub = fromFront ? 0 : ZKD_BACK;
		float speed = 4.0f, yVel = 5.0f;
		if ( response == ZHR_KNOCKBACK_SMALL )
		{
			sub |= ZKD_SMALL;
			speed = 3.0f;
			yVel = 6.0f;
			ZSound( CHAN_VOICE, ZS_VO_DAMAGE_S );
		}
		else
			ZSound( CHAN_VOICE, ZS_VO_FALL_L );
		pev->iuser4 = ( pev->iuser4 & ~( ZL_STATE_SUB_MASK | ZL_STATE_MWA_MASK ) ) |
					  ( ( sub << ZL_STATE_SUB_SHIFT ) & ZL_STATE_SUB_MASK ) |
					  ( ( ZKD_PHASE_AIR << ZL_STATE_MWA_SHIFT ) & ZL_STATE_MWA_MASK );

		pev->velocity = away * ZL_VEL( speed );
		pev->velocity.z = ZL_VEL( yVel );
		pev->flags &= ~FL_ONGROUND;
	}
	else
	{
		// Running hard and not locked onto an enemy: he runs on (unk_890 = 20)
		Vector flat = pev->velocity;
		flat.z = 0;
		BOOL hostile = ( pev->iuser3 & ZSF_HOSTILE ) != 0;
		if ( flat.Length() > ZL_VEL( 4.0f ) && !hostile && act == ZACT_STAND )
		{
			ZSound( CHAN_VOICE, ZS_VO_DAMAGE_S );
			return;
		}

		// Player_Action_8084370C: a flinch from D_808544B0
		ZSetAction( ZACT_DAMAGE );
		int sub = ( fromFront ? 0 : ZHIT_BACK ) | ( hostile ? ZHIT_LOCKED : 0 );
		BOOL big = healthUnits >= 5;
		pev->iuser4 = ( pev->iuser4 & ~( ZL_STATE_SUB_MASK | ZL_STATE_MWA_MASK ) ) |
					  ( ( sub << ZL_STATE_SUB_SHIFT ) & ZL_STATE_SUB_MASK ) |
					  ( ( ( big ? 1 : 0 ) << ZL_STATE_MWA_SHIFT ) & ZL_STATE_MWA_MASK );

		// speedXZ along the push: 23 for the big ones, what he had for the small ones
		float speed = big ? ZL_VEL( ZL_DAMAGE_BIG_SPEED ) : flat.Length();
		pev->velocity.x = away.x * speed;
		pev->velocity.y = away.y * speed;
		ZSound( CHAN_VOICE, ZS_VO_DAMAGE_S );
	}

	// shape.rot.y turns to the push, and round to face where it came from when that was in front
	pev->fuser1 = UTIL_AngleMod( fromFront ? pushYaw + 180.0f : pushYaw );
}

int CLinkPlayer::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( !LinkActive() )
		return CBasePlayer::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

	if ( pev->takedamage == DAMAGE_NO || !IsAlive() )
		return 0;

	int act = ZAction();

	// Nobody hits Link while he is reading
	if ( act == ZACT_TALK || act == ZACT_GETITEM || act == ZACT_CHEST || act == ZACT_DOOR || m_hZTalkTo != NULL )
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
		// Player_SetIntangibility( this, 20 ), NA_SE_PL_DAMAGE
		m_flZInvincibleUntil = gpGlobals->time + ZL_INVINCIBILITY_TIME;
		ZSound( CHAN_BODY, ZS_PL_DAMAGE, 0.7f );

		if ( !( bitsDamageType & ( DMG_FALL | DMG_DROWN ) ) )
			ZApplyKnockback( pevInflictor, units, bitsDamageType );
		else if ( m_hZHeldBomb != NULL )
			ZThrowBomb( TRUE );
	}

	return ret;
}

/*
 Falling (Player_Action_8084411C, func_80843E64)
   - starting down with the floor more than 800 below: NA_SE_VO_LI_FALL_S
   - 120 fallen with the floor still more than 280 below: NA_SE_VO_LI_FALL_L, once
   - landing from 400: half a heart, from 800 a whole one (D_80854600), 40 frames intangible, a
     quake, NA_SE_PL_BODY_HIT and NA_SE_VO_LI_LAND_DAMAGE_S, then link_normal_landing to frame 8
     and 10 / 20 frames stunned (fallDamageStunTimer)
   - landing in a roll (the stick forward, under 800) takes no damage at all, nor does being
     knocked down
*/
void CLinkPlayer::ZFallDamageThink( void )
{
	BOOL onground = ( pev->flags & FL_ONGROUND ) != 0;
	int act = ZAction();

	if ( !onground )
	{
		if ( m_fZWasOnGround || pev->waterlevel > 0 || act == ZACT_HOOKFLY || act == ZACT_CLIMB )
		{
			m_flZFallStartZ = pev->origin.z;
			m_iZFallVoice = 0;
		}
		else if ( pev->origin.z > m_flZFallStartZ )
			m_flZFallStartZ = pev->origin.z;

		// The screams, on the way down (not knocked flying, not swimming)
		if ( pev->velocity.z < 0 && act != ZACT_KNOCKDOWN && act != ZACT_HOOKFLY && act != ZACT_CLIMB &&
			 pev->waterlevel == 0 && m_iZFallVoice < 2 )
		{
			TraceResult tr;
			Vector feet = pev->origin + Vector( 0, 0, pev->mins.z );
			UTIL_TraceLine( feet, feet - Vector( 0, 0, ZL_DIST( 2000 ) ), ignore_monsters, edict(), &tr );
			float toFloor = tr.flFraction * 2000.0f;		// OoT units (sYDistToFloor)
			float fallen = ( m_flZFallStartZ - pev->origin.z ) / ZL_UNIT;

			if ( m_iZFallVoice == 0 )
			{
				if ( toFloor > 800.0f )
					ZSound( CHAN_VOICE, ZS_VO_FALL_S );
				m_iZFallVoice = 1;
			}
			else if ( fallen > 120.0f && toFloor > 280.0f )
			{
				ZSound( CHAN_VOICE, ZS_VO_FALL_L );
				m_iZFallVoice = 2;
			}
		}
	}
	else if ( !m_fZWasOnGround )
	{
		float fallDistance = ( m_flZFallStartZ - pev->origin.z ) / ZL_UNIT;
		int damage = 0;

		if ( pev->waterlevel == 0 && act != ZACT_KNOCKDOWN && act != ZACT_ROLL && act != ZACT_DEAD )
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

			if ( IsAlive() )
			{
				// Player_SetIntangibility( this, 40 ), Player_RequestQuake( play, 32967, 2, 30 )
				m_flZInvincibleUntil = gpGlobals->time + ZL_FRAMES( 40 );
				UTIL_ScreenShake( pev->origin, 2, 30, ZL_FRAMES( 30 ), 64 );
				ZSound( CHAN_BODY, ZS_PL_BODY_HIT );
				ZSound( CHAN_VOICE, ZS_VO_LAND_DAMAGE_S );

				// fallDamageStunTimer
				ZSetAction( ZACT_FALLSTUN );
				pev->iuser4 = ( pev->iuser4 & ~ZL_STATE_SUB_MASK ) |
							  ( ( ( damage >= 16 ? 1 : 0 ) << ZL_STATE_SUB_SHIFT ) & ZL_STATE_SUB_MASK );
			}
		}
		m_iZFallVoice = 0;
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
// The Half-Life material (CHAR_TEX_*) of the brush texture between start and end, 0 if none
static char ZL_TextureType( edict_t *pHit, const Vector &start, const Vector &end )
{
	const char *tex = TRACE_TEXTURE( pHit ? pHit : ENT( 0 ), (float *)&start.x, (float *)&end.x );
	char name[CBTEXTURENAMEMAX];

	if ( !tex )
		return 0;
	// strip leading '-0' or '+0~' or '{' or '!' (as TEXTURETYPE_PlaySound does)
	if ( *tex == '-' || *tex == '+' )
		tex += 2;
	if ( *tex == '{' || *tex == '!' || *tex == '~' || *tex == ' ' )
		tex++;
	strncpy( name, tex, sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = 0;
	return TEXTURETYPE_Find( name );
}

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
		if ( pEnt->pev->solid == SOLID_TRIGGER && !FClassnameIs( pEnt->pev, "zl_grass" ) )
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

		// What it sounds and looks like (CollisionCheck_HitEffects, sHitInfo): things that bleed
		// get the white hit mark and NA_SE_IT_SWORD_STRIKE; machines are COL_MATERIAL_METAL,
		// the metal hit mark and NA_SE_IT_SHIELD_BOUND; breakable brushes go by their texture
		// (wood: CollisionCheck_SpawnShieldParticlesWood)
		int fx = ZFX_HITMARK_WHITE;
		int snd = ZS_IT_SWORD_STRIKE;
		if ( pEnt->IsBSPModel() )
		{
			char mat = ZL_TextureType( pEnt->edict(), center, targetCenter );
			if ( mat == CHAR_TEX_WOOD )
			{
				fx = ZFX_SPARKS;
				snd = ZS_IT_REFLECTION_WOOD;
			}
			else if ( mat == CHAR_TEX_METAL || mat == CHAR_TEX_VENT || mat == CHAR_TEX_GRATE || mat == CHAR_TEX_COMPUTER )
			{
				fx = ZFX_HITMARK_METAL;
				snd = ZS_IT_SHIELD_BOUND;
			}
		}
		else if ( pEnt->Classify() == CLASS_MACHINE )
		{
			fx = ZFX_HITMARK_METAL;
			snd = ZS_IT_SHIELD_BOUND;
		}

		// On the near side of the target, at the swing's height
		Vector flat( dir.x, dir.y, 0 );
		Vector hitPos = targetCenter - flat.Normalize() * radius;
		hitPos.z = center.z;
		if ( hitPos.z < pEnt->pev->absmin.z )
			hitPos.z = pEnt->pev->absmin.z;
		if ( hitPos.z > pEnt->pev->absmax.z )
			hitPos.z = pEnt->pev->absmax.z;

		// Grass makes its own noise
		if ( !FClassnameIs( pEnt->pev, "zl_grass" ) )
		{
			ZSound( CHAN_ITEM, snd );
			ZL_SendFx( fx, hitPos );
		}
	}

	// Sword on a wall (func_80842DF4): the light streaks and NA_SE_IT_WALL_HIT_HARD, the clang
	// (_SOFT into soft dirt; wood is NA_SE_IT_REFLECTION_WOOD), and Link bounces back:
	// speedXZ -14. Spins don't hit walls.
	if ( !m_fZSwingHitWorld && ZAction() != ZACT_SPIN )
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
					char mat = ZL_TextureType( tr.pHit, center, center + fwd * reach );
					int snd = ZS_IT_WALL_HIT_HARD;
					if ( mat == CHAR_TEX_WOOD )
						snd = ZS_IT_REFLECTION_WOOD;
					else if ( mat == CHAR_TEX_DIRT || mat == CHAR_TEX_SNOW || mat == CHAR_TEX_SLOSH )
						snd = ZS_IT_WALL_HIT_SOFT;

					m_fZSwingHitWorld = TRUE;
					ZL_SendFx( ZFX_SPARKS, tr.vecEndPos + tr.vecPlaneNormal * 2 );
					ZSound( CHAN_ITEM, snd );

					// if ( speedXZ >= 0 ) speedXZ = -14; pm_zelda.c slows it back down
					if ( ZAction() == ZACT_SLASH && DotProduct( pev->velocity, ZForward() ) >= 0 )
					{
						Vector back = ZForward() * -ZL_VEL( 14.0f );
						pev->velocity.x = back.x;
						pev->velocity.y = back.y;
						pev->iuser4 |= ZL_STATE_RECOIL;
					}
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

			// Letting go of a lit spin takes its magic, once (MAGIC_STATE_CONSUME_SETUP)
			if ( act == ZACT_SPIN && ZL_GetSub( pev->iuser4 ) )
				ZUseMagic( ZL_SPIN_MAGIC );
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
		if ( t > 0.03f && t < ZL_SpinTime( mwa ) )
		{
			float scale = ( mwa == ZMWA_BIG_SPIN_1H ) ? 1.6f : 1.15f;

			// A lit spin also hits with EnMThunder's ring: radius scale * 25, the ring growing to
			// scale 4 (8 for the great spin) with the Master or Biggoron Sword, 2 (4) with the
			// Kokiri Sword
			if ( ZL_GetSub( pev->iuser4 ) )
			{
				BOOL kokiri = !( m_ZInv.equipment & ( ZEQ_SWORD_MASTER | ZEQ_SWORD_BIGGORON ) );
				float ring = ZL_DIST( 25.0f * ( kokiri ? 2.0f : 4.0f ) * ( mwa == ZMWA_BIG_SPIN_1H ? 2.0f : 1.0f ) );
				if ( ring > reach * scale )
					scale = ring / reach;
			}

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
