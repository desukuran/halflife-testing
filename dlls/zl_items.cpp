/***
*
*	Zelda 64 port: items.
*
*	C button items (Fairy Bow, Bombs, Hookshot / Longshot, Deku Nuts, borrowed Half-Life
*	weapons), the projectiles they make, and the pickups found in the world.
*
*	Models are stock Half-Life ones for now. The comment next to each one is the OoT object
*	it stands in for, so a ripped model can be dropped in under the same name later.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "customentity.h"
#include "gamerules.h"
#include "decals.h"
#include "soundent.h"
#include "zl_player.h"

#include <math.h>
#include <ctype.h>

#define ZL_MDL_ARROW		"models/crossbow_bolt.mdl"	// OOT: object_link_boy arrow (gLinkAdultArrowDL)
#define ZL_MDL_BOMB			"models/w_grenade.mdl"		// OOT: object_bomb / gameplay_keep gBombBodyDL
#define ZL_MDL_HOOK_TIP		"models/crossbow_bolt.mdl"	// OOT: object_link_boy gLinkAdultHookshotTipDL
#define ZL_MDL_NUT			"models/w_battery.mdl"		// OOT: gameplay_keep gDekuNutDL
#define ZL_SPR_CHAIN		"sprites/laserbeam.spr"		// OOT: gLinkAdultHookshotChainDL (drawn as links)
#define ZL_SPR_FLASH		"sprites/flare1.spr"			// OOT: Deku Nut flash (EnArrow ARROW_NUT -> effect)

// Pickups
#define ZL_MDL_RUPEE		"models/w_battery.mdl"		// OOT: gameplay_keep gRupeeGreenTex / gRupeeDL
#define ZL_MDL_HEART		"models/w_medkit.mdl"		// OOT: gameplay_keep gHeartPieceDL / recovery heart
#define ZL_MDL_ARROWS		"models/w_crossbow_clip.mdl"	// OOT: object_gi_arrow (bundle of arrows)
#define ZL_MDL_BOMBS		"models/w_grenade.mdl"		// OOT: object_gi_bomb_1
#define ZL_MDL_MAGIC		"models/w_antidote.mdl"		// OOT: object_gi_magicpot (small magic jar)
#define ZL_MDL_CONTAINER	"models/w_medkit.mdl"		// OOT: object_gi_hearts (heart container)
#define ZL_MDL_BOW			"models/w_crossbow.mdl"		// OOT: object_gi_bow
#define ZL_MDL_BOMBBAG		"models/w_satchel.mdl"		// OOT: object_gi_bomb_2 (bomb bag)
#define ZL_MDL_HOOKSHOT		"models/w_357.mdl"			// OOT: object_gi_hookshot
#define ZL_MDL_SWORD		"models/w_crowbar.mdl"		// OOT: object_gi_sword_1 / gi_msword
#define ZL_MDL_SHIELD		"models/w_weaponbox.mdl"	// OOT: object_gi_shield_2 (Hylian Shield)
#define ZL_MDL_KEY			"models/w_security.mdl"		// OOT: object_gi_key (small key)
#define ZL_MDL_CHEST		"models/w_weaponbox.mdl"	// OOT: object_box (treasure chest)

void ZL_PrecacheItems( void )
{
	PRECACHE_MODEL( ZL_MDL_ARROW );
	PRECACHE_MODEL( ZL_MDL_BOMB );
	PRECACHE_MODEL( ZL_MDL_HOOK_TIP );
	PRECACHE_MODEL( ZL_MDL_NUT );
	PRECACHE_MODEL( ZL_SPR_CHAIN );
	PRECACHE_MODEL( ZL_SPR_FLASH );
	PRECACHE_MODEL( ZL_MDL_RUPEE );
	PRECACHE_MODEL( ZL_MDL_HEART );
	PRECACHE_MODEL( ZL_MDL_ARROWS );
	PRECACHE_MODEL( ZL_MDL_BOMBS );
	PRECACHE_MODEL( ZL_MDL_MAGIC );
	PRECACHE_MODEL( ZL_MDL_CONTAINER );
	PRECACHE_MODEL( ZL_MDL_BOW );
	PRECACHE_MODEL( ZL_MDL_BOMBBAG );
	PRECACHE_MODEL( ZL_MDL_HOOKSHOT );
	PRECACHE_MODEL( ZL_MDL_SWORD );
	PRECACHE_MODEL( ZL_MDL_SHIELD );
	PRECACHE_MODEL( ZL_MDL_KEY );
	PRECACHE_MODEL( ZL_MDL_CHEST );
}

static const char *ZL_ItemName( int item )
{
	switch ( item )
	{
	case ZITEM_BOW:			return "Fairy Bow";
	case ZITEM_BOMB:		return "Bombs";
	case ZITEM_HOOKSHOT:	return "Hookshot";
	case ZITEM_LONGSHOT:	return "Longshot";
	case ZITEM_DEKU_NUT:	return "Deku Nuts";
	case ZITEM_DEKU_STICK:	return "Deku Sticks";
	case ZITEM_BOTTLE:		return "Bottle";
	case ZITEM_HLWEAPON:	return "Half-Life weapon";
	default:				return "nothing";
	}
}

/*
=============================================================================
 CZeldaArrow: EnArrow (ARROW_NORMAL)
=============================================================================
*/
class CZeldaArrow : public CBaseEntity
{
public:
	void Spawn( void );
	void EXPORT ArrowTouch( CBaseEntity *pOther );
	void EXPORT ArrowThink( void );

	static CZeldaArrow *Shoot( CLinkPlayer *pOwner, const Vector &src, const Vector &dir );
};

LINK_ENTITY_TO_CLASS( zl_arrow, CZeldaArrow );

void CZeldaArrow::Spawn( void )
{
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->gravity = 0.5f;
	SET_MODEL( ENT( pev ), ZL_MDL_ARROW );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	SetTouch( &CZeldaArrow::ArrowTouch );
	SetThink( &CZeldaArrow::ArrowThink );
	pev->nextthink = gpGlobals->time + 0.1f;
	pev->dmgtime = gpGlobals->time + 3.0f;
}

CZeldaArrow *CZeldaArrow::Shoot( CLinkPlayer *pOwner, const Vector &src, const Vector &dir )
{
	CZeldaArrow *pArrow = GetClassPtr( (CZeldaArrow *)NULL );
	pArrow->pev->classname = MAKE_STRING( "zl_arrow" );
	pArrow->Spawn();
	UTIL_SetOrigin( pArrow->pev, src );
	// EnArrow speed 150 is too fast for HL's tracing at 20 Hz; still very quick
	pArrow->pev->velocity = dir * 2400;
	pArrow->pev->angles = UTIL_VecToAngles( dir );
	pArrow->pev->owner = pOwner->edict();
	pArrow->pev->dmg = ZL_OoTDamageToHL( 2 );	// DMG_ARROW_NORMAL
	return pArrow;
}

void CZeldaArrow::ArrowThink( void )
{
	if ( gpGlobals->time > pev->dmgtime )
	{
		UTIL_Remove( this );
		return;
	}
	if ( pev->movetype == MOVETYPE_FLY )
		pev->angles = UTIL_VecToAngles( pev->velocity );
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CZeldaArrow::ArrowTouch( CBaseEntity *pOther )
{
	SetTouch( NULL );

	if ( pOther->pev->takedamage )
	{
		TraceResult tr = UTIL_GetGlobalTrace();
		entvars_t *pevOwner = pev->owner ? VARS( pev->owner ) : pev;

		ClearMultiDamage();
		pOther->TraceAttack( pevOwner, pev->dmg, pev->velocity.Normalize(), &tr, DMG_BULLET | DMG_NEVERGIB );
		ApplyMultiDamage( pev, pevOwner );

		ZL_EmitSound( edict(), CHAN_BODY, ZS_IT_ARROW_STICK_CRE, 1, ATTN_NORM, PITCH_NORM );
		pev->velocity = g_vecZero;
		UTIL_Remove( this );
		return;
	}

	// Stick in the wall for a bit, like EnArrow's "stuck" state
	ZL_EmitSound( edict(), CHAN_BODY, ZS_IT_ARROW_STICK_OBJ, 1, ATTN_NORM, PITCH_NORM );
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	pev->dmgtime = gpGlobals->time + 8.0f;

	if ( UTIL_PointContents( pev->origin ) != CONTENTS_WATER )
		UTIL_Sparks( pev->origin );
}

/*
=============================================================================
 CZeldaBomb: EnBom
=============================================================================
*/
class CZeldaBomb : public CBaseEntity
{
public:
	void Spawn( void );
	void EXPORT FuseThink( void );
	void EXPORT BombTouch( CBaseEntity *pOther );
	void Explode( void );

	EHANDLE m_hHolder;		// set while Link carries it over his head
};

LINK_ENTITY_TO_CLASS( zl_bomb, CZeldaBomb );

#define ZL_BOMB_FUSE	ZL_FRAMES( 70 )	// EnBom timer

void CZeldaBomb::Spawn( void )
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->friction = 0.6f;
	pev->gravity = 0.8f;
	SET_MODEL( ENT( pev ), ZL_MDL_BOMB );
	UTIL_SetSize( pev, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ) );
	pev->dmgtime = gpGlobals->time + ZL_BOMB_FUSE;
	pev->dmg = ZL_OoTDamageToHL( 4 );
	SetThink( &CZeldaBomb::FuseThink );
	SetTouch( &CZeldaBomb::BombTouch );
	pev->nextthink = gpGlobals->time + 0.01f;
	ZL_EmitSound( edict(), CHAN_WEAPON, ZS_IT_BOMB_IGNIT, 0.6f, ATTN_NORM, PITCH_NORM );
}

void CZeldaBomb::BombTouch( CBaseEntity *pOther )
{
	if ( pOther->edict() == pev->owner )
		return;
	if ( pev->velocity.Length() > 60 )
		ZL_EmitSound( edict(), CHAN_BODY, ZS_IT_BOMB_BOUND, 0.5f, ATTN_NORM, PITCH_NORM );
	pev->velocity = pev->velocity * 0.7f;
}

void CZeldaBomb::FuseThink( void )
{
	CBaseEntity *pHolder = m_hHolder;

	if ( pHolder )
	{
		// Over Link's head (PLAYER_STATE1_CARRYING_ACTOR)
		UTIL_SetOrigin( pev, pHolder->pev->origin + Vector( 0, 0, 44 ) );
		pev->velocity = g_vecZero;
	}

	float left = pev->dmgtime - gpGlobals->time;

	// Flash red faster as the fuse burns down
	if ( left < 1.5f )
	{
		BOOL on = ( (int)( left * ( left < 0.6f ? 16 : 8 ) ) & 1 ) != 0;
		pev->renderfx = on ? kRenderFxGlowShell : kRenderFxNone;
		pev->rendercolor = Vector( 255, 30, 30 );
		pev->renderamt = 10;
	}

	if ( left <= 0 )
	{
		Explode();
		return;
	}

	pev->nextthink = gpGlobals->time + 0.01f;
}

void CZeldaBomb::Explode( void )
{
	CBaseEntity *pHolder = m_hHolder;
	if ( pHolder )
	{
		CLinkPlayer *pLink = ZL_GetLink( pHolder );
		if ( pLink && pLink->m_hZHeldBomb == this )
			pLink->m_hZHeldBomb = NULL;
	}

	TraceResult tr;
	UTIL_TraceLine( pev->origin + Vector( 0, 0, 8 ), pev->origin - Vector( 0, 0, 32 ), ignore_monsters, edict(), &tr );

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 16 );
		WRITE_SHORT( g_sModelIndexFireball );
		WRITE_BYTE( 20 );
		WRITE_BYTE( 15 );
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();

	ZL_EmitSound( edict(), CHAN_VOICE, ZS_IT_BOMB_EXPLOSION, 1.0f, ATTN_NORM, PITCH_NORM );
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );

	entvars_t *pevOwner = pev->owner ? VARS( pev->owner ) : pev;
	pev->owner = NULL;

	// EnBom explosion collider grows to ~100 units
	::RadiusDamage( pev->origin, pev, pevOwner, pev->dmg, ZL_DIST( 150 ), CLASS_NONE, DMG_BLAST );

	if ( tr.flFraction < 1.0f )
		UTIL_DecalTrace( &tr, RANDOM_LONG( 0, 1 ) ? DECAL_SCORCH1 : DECAL_SCORCH2 );

	UTIL_Remove( this );
}

/*
=============================================================================
 CZeldaHookshot: ArmsHook
=============================================================================
*/
class CZeldaHookshot : public CBaseEntity
{
public:
	void Spawn( void );
	void EXPORT HookThink( void );
	void Retract( void );
	void Finish( void );
	BOOL Hookable( TraceResult *ptr );
	void UpdateRemove( void ) {}

	static CZeldaHookshot *Shoot( CLinkPlayer *pOwner, const Vector &src, const Vector &dir, BOOL longshot );

	EHANDLE		m_hOwner;
	EHANDLE		m_hGrabbed;		// item being reeled in
	CBeam		*m_pChain;
	Vector		m_vecDir;
	float		m_flRange;
	int			m_iState;		// 0 out, 1 retracting, 2 pulling Link
	Vector		m_vecStart;
};

LINK_ENTITY_TO_CLASS( zl_hookshot, CZeldaHookshot );

// ArmsHook_Shoot: speed 20 per frame, 13 frames (26 for the Longshot)
#define ZL_HOOK_SPEED		ZL_VEL( 20.0f )
#define ZL_HOOK_RANGE		ZL_DIST( 20.0f * ZL_POS_RATE * 13.0f )
#define ZL_HOOK_FLY_SPEED	700.0f

void CZeldaHookshot::Spawn( void )
{
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;
	SET_MODEL( ENT( pev ), ZL_MDL_HOOK_TIP );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	m_pChain = NULL;
	m_iState = 0;
	SetThink( &CZeldaHookshot::HookThink );
	pev->nextthink = gpGlobals->time + 0.01f;
}

CZeldaHookshot *CZeldaHookshot::Shoot( CLinkPlayer *pOwner, const Vector &src, const Vector &dir, BOOL longshot )
{
	CZeldaHookshot *pHook = GetClassPtr( (CZeldaHookshot *)NULL );
	pHook->pev->classname = MAKE_STRING( "zl_hookshot" );
	pHook->Spawn();
	UTIL_SetOrigin( pHook->pev, src );
	pHook->m_vecStart = src;
	pHook->m_vecDir = dir;
	pHook->m_flRange = longshot ? ZL_HOOK_RANGE * 2 : ZL_HOOK_RANGE;
	pHook->m_hOwner = pOwner;
	pHook->pev->owner = pOwner->edict();
	pHook->pev->angles = UTIL_VecToAngles( dir );
	pHook->pev->velocity = dir * ZL_HOOK_SPEED;

	pHook->m_pChain = CBeam::BeamCreate( ZL_SPR_CHAIN, 6 );
	pHook->m_pChain->EntsInit( pOwner->entindex(), pHook->entindex() );
	pHook->m_pChain->SetColor( 200, 200, 210 );
	pHook->m_pChain->SetBrightness( 255 );
	pHook->m_pChain->SetNoise( 0 );
	pHook->m_pChain->SetFlags( BEAM_FSOLID );
	pHook->m_pChain->pev->spawnflags |= SF_BEAM_TEMPORARY;

	ZL_EmitSound( pHook->edict(), CHAN_WEAPON, ZS_IT_HOOKSHOT_CHAIN, 0.8f, ATTN_NORM, PITCH_NORM );
	return pHook;
}

// OoT only bites into wooden targets (SurfaceType_IsHookshotSurface); HL maps have none
// marked, so anything goes unless zl_hookshot_any is 0.
BOOL CZeldaHookshot::Hookable( TraceResult *ptr )
{
	CBaseEntity *pHit = CBaseEntity::Instance( ptr->pHit );
	if ( !pHit || !pHit->IsBSPModel() )
		return FALSE;

	const char *tex = TRACE_TEXTURE( ptr->pHit, pev->origin - m_vecDir * 8, pev->origin + m_vecDir * 32 );
	if ( tex && !stricmp( tex, "sky" ) )
		return FALSE;

	if ( zl_hookshot_any.value )
		return TRUE;

	if ( !tex )
		return FALSE;

	char name[32];
	strncpy( name, tex, sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = 0;
	for ( char *c = name; *c; c++ )
		*c = tolower( *c );

	return strstr( name, "wood" ) || strstr( name, "hook" ) || strstr( name, "crate" ) || strstr( name, "tree" );
}

void CZeldaHookshot::Retract( void )
{
	m_iState = 1;
	ZL_EmitSound( edict(), CHAN_WEAPON, ZS_IT_HOOKSHOT_CHAIN, 0.6f, ATTN_NORM, 90 );
}

void CZeldaHookshot::Finish( void )
{
	CLinkPlayer *pLink = ZL_GetLink( m_hOwner );

	if ( pLink )
	{
		if ( pLink->ZAction() == ZACT_HOOKFLY )
		{
			pLink->ZSetAction( ( pLink->pev->flags & FL_ONGROUND ) ? ZACT_STAND : ZACT_FALL );
			pLink->pev->velocity = pLink->pev->velocity * 0.2f;
		}
		if ( pLink->m_hZHookshot == this )
			pLink->m_hZHookshot = NULL;
		ZL_EmitSound( pLink->edict(), CHAN_ITEM, ZS_IT_HOOKSHOT_RECEIVE, 0.6f, ATTN_NORM, PITCH_NORM );
	}

	if ( m_pChain )
	{
		UTIL_Remove( m_pChain );
		m_pChain = NULL;
	}
	UTIL_Remove( this );
}

void CZeldaHookshot::HookThink( void )
{
	CLinkPlayer *pLink = ZL_GetLink( m_hOwner );
	if ( !pLink || !pLink->IsAlive() )
	{
		Finish();
		return;
	}

	Vector hand = pLink->ZHandPosition();
	pev->nextthink = gpGlobals->time + 0.01f;
	float dt = gpGlobals->frametime > 0 ? gpGlobals->frametime : 0.01f;

	switch ( m_iState )
	{
	case 0:	// flying out
	{
		Vector next = pev->origin + m_vecDir * ZL_HOOK_SPEED * dt;
		TraceResult tr;
		UTIL_TraceLine( pev->origin, next, dont_ignore_monsters, pLink->edict(), &tr );

		if ( tr.flFraction < 1.0f )
		{
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			UTIL_SetOrigin( pev, tr.vecEndPos );

			if ( pHit && pHit->pev->takedamage && !pHit->IsBSPModel() )
			{
				// Stuns most things; small damage for HL monsters that don't care about stun
				ClearMultiDamage();
				pHit->TraceAttack( pLink->pev, ZL_OoTDamageToHL( 1 ), m_vecDir, &tr, DMG_CLUB );
				ApplyMultiDamage( pev, pLink->pev );
				ZL_EmitSound( edict(), CHAN_BODY, ZS_IT_HOOKSHOT_STICK_OBJ, 1, ATTN_NORM, PITCH_NORM );
				Retract();
			}
			else if ( Hookable( &tr ) )
			{
				// Pull Link to the tip (Player_Action_HookshotFly)
				ZL_EmitSound( edict(), CHAN_BODY, ZS_IT_HOOKSHOT_STICK_OBJ, 1, ATTN_NORM, PITCH_NORM );
				UTIL_SetOrigin( pev, tr.vecEndPos - m_vecDir * 2 );
				m_iState = 2;
				pLink->ZSetAction( ZACT_HOOKFLY );
				pLink->pev->flags &= ~FL_ONGROUND;
			}
			else
			{
				ZL_EmitSound( edict(), CHAN_BODY, ZS_IT_HOOKSHOT_REFLECT, 1, ATTN_NORM, PITCH_NORM );
				UTIL_Sparks( tr.vecEndPos );
				Retract();
			}
			break;
		}

		UTIL_SetOrigin( pev, next );

		// Grab pickups on the way (the hookshot fetches rupees and hearts)
		CBaseEntity *pItem = NULL;
		while ( ( pItem = UTIL_FindEntityInSphere( pItem, pev->origin, 24 ) ) != NULL )
		{
			if ( !strncmp( STRING( pItem->pev->classname ), "item_zl_", 8 ) )
			{
				m_hGrabbed = pItem;
				Retract();
				break;
			}
		}

		if ( ( pev->origin - m_vecStart ).Length() >= m_flRange )
			Retract();
		break;
	}

	case 1:	// reeling back in
	{
		Vector toHand = hand - pev->origin;
		float dist = toHand.Length();
		float step = ZL_HOOK_SPEED * 1.4f * dt;

		if ( dist <= step + 8 )
		{
			CBaseEntity *pItem = m_hGrabbed;
			if ( pItem )
				pItem->Touch( pLink );
			Finish();
			return;
		}

		UTIL_SetOrigin( pev, pev->origin + toHand / dist * step );
		pev->angles = UTIL_VecToAngles( -toHand );

		CBaseEntity *pItem = m_hGrabbed;
		if ( pItem )
			UTIL_SetOrigin( pItem->pev, pev->origin );
		break;
	}

	case 2:	// pulling Link to the tip
	{
		Vector toTip = pev->origin - pLink->pev->origin;
		float dist = toTip.Length();

		if ( dist < 40 || pLink->ZAction() != ZACT_HOOKFLY ||
			 ( gpGlobals->time - pev->ltime > 0.2f && pLink->pev->velocity.Length() < 50 ) )
		{
			Finish();
			return;
		}

		if ( pLink->pev->velocity.Length() > 50 )
			pev->ltime = gpGlobals->time;

		pLink->pev->velocity = toTip / dist * ZL_HOOK_FLY_SPEED;
		break;
	}
	}
}

/*
=============================================================================
 Deku Nut: EnArrow ARROW_NUT, stuns everything nearby with a flash
=============================================================================
*/
class CZeldaStun : public CBaseEntity
{
public:
	void Spawn( void ) { SetThink( &CZeldaStun::Wake ); }
	void EXPORT Wake( void )
	{
		CBaseEntity *pVictim = m_hVictim;
		if ( pVictim && pVictim->IsAlive() )
		{
			pVictim->pev->framerate = m_flFramerate ? m_flFramerate : 1.0f;
			pVictim->pev->nextthink = gpGlobals->time + 0.1f;
			pVictim->pev->renderfx = kRenderFxNone;
		}
		UTIL_Remove( this );
	}

	EHANDLE	m_hVictim;
	float	m_flFramerate;
};

LINK_ENTITY_TO_CLASS( zl_stun, CZeldaStun );

static void ZL_StunMonster( CBaseMonster *pMon, float duration )
{
	if ( !pMon || !pMon->IsAlive() || pMon->IsPlayer() )
		return;

	CZeldaStun *pStun = GetClassPtr( (CZeldaStun *)NULL );
	pStun->pev->classname = MAKE_STRING( "zl_stun" );
	pStun->Spawn();
	pStun->m_hVictim = pMon;
	pStun->m_flFramerate = pMon->pev->framerate;
	pStun->pev->nextthink = gpGlobals->time + duration;

	// Freeze in place: no thinking, no animating, a pale shell like OoT's stun flash
	pMon->pev->framerate = 0;
	pMon->pev->velocity = g_vecZero;
	pMon->pev->nextthink = gpGlobals->time + duration;
	pMon->pev->renderfx = kRenderFxGlowShell;
	pMon->pev->rendercolor = Vector( 200, 200, 255 );
	pMon->pev->renderamt = 5;
}

class CZeldaNut : public CBaseEntity
{
public:
	void Spawn( void )
	{
		pev->movetype = MOVETYPE_TOSS;
		pev->solid = SOLID_BBOX;
		pev->gravity = 1.0f;
		SET_MODEL( ENT( pev ), ZL_MDL_NUT );
		pev->scale = 0.5f;
		UTIL_SetSize( pev, g_vecZero, g_vecZero );
		SetTouch( &CZeldaNut::NutTouch );
	}

	void EXPORT NutTouch( CBaseEntity *pOther )
	{
		if ( pOther->edict() == pev->owner )
			return;

		// Flash
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_DLIGHT );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_BYTE( 30 );
			WRITE_BYTE( 255 );
			WRITE_BYTE( 255 );
			WRITE_BYTE( 255 );
			WRITE_BYTE( 4 );
			WRITE_BYTE( 60 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SPRITE );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z + 8 );
			WRITE_SHORT( MODEL_INDEX( ZL_SPR_FLASH ) );
			WRITE_BYTE( 20 );
			WRITE_BYTE( 255 );
		MESSAGE_END();

		ZL_EmitSound( edict(), CHAN_BODY, ZS_IT_DEKU_NUT, 1, ATTN_NORM, PITCH_NORM );

		CBaseEntity *pEnt = NULL;
		while ( ( pEnt = UTIL_FindEntityInSphere( pEnt, pev->origin, ZL_DIST( 200 ) ) ) != NULL )
		{
			CBaseMonster *pMon = pEnt->MyMonsterPointer();
			if ( pMon && ( pEnt->pev->flags & FL_MONSTER ) && FVisible( pEnt ) )
				ZL_StunMonster( pMon, 4.0f );
		}

		UTIL_Remove( this );
	}
};

LINK_ENTITY_TO_CLASS( zl_nut, CZeldaNut );

/*
=============================================================================
 CLinkPlayer item logic
=============================================================================
*/
BOOL CLinkPlayer::ZItemAimable( int item )
{
	switch ( item )
	{
	case ZITEM_BOW:			return m_ZInv.ammo[ZITEM_BOW] > 0;
	case ZITEM_HOOKSHOT:
	case ZITEM_LONGSHOT:	return m_hZHookshot == NULL;
	default:				return FALSE;
	}
}

int CLinkPlayer::ZItemAmmo( int item )
{
	switch ( item )
	{
	case ZITEM_BOW:
	case ZITEM_BOMB:
	case ZITEM_DEKU_NUT:
	case ZITEM_DEKU_STICK:
		return m_ZInv.ammo[item];
	default:
		return -1;
	}
}

Vector CLinkPlayer::ZHandPosition( void )
{
	return pev->origin + ZForward() * 14 + Vector( 0, 0, 22 );
}

// First person: the camera. Locked on: straight at the target. Otherwise Link's facing.
Vector CLinkPlayer::ZAimVector( void )
{
	int act = ZAction();
	CBaseEntity *pTarget = m_hZTarget;

	if ( act == ZACT_AIM || act == ZACT_CUP )
	{
		Vector fwd, right, up;
		UTIL_MakeVectorsPrivate( pev->v_angle, fwd, right, up );
		return fwd;
	}

	if ( pTarget )
		return ( pTarget->Center() - ZHandPosition() ).Normalize();

	return ZForward();
}

void CLinkPlayer::ZFireArrow( void )
{
	if ( m_ZInv.ammo[ZITEM_BOW] <= 0 )
	{
		ZSound( CHAN_ITEM, ZS_SY_ERROR );
		return;
	}

	m_ZInv.ammo[ZITEM_BOW]--;
	Vector dir = ZAimVector();
	Vector src = ( ZAction() == ZACT_AIM ) ? ( pev->origin + pev->view_ofs + dir * 16 ) : ZHandPosition();

	CZeldaArrow::Shoot( this, src, dir );
	ZSound( CHAN_WEAPON, ZS_IT_ARROW_SHOT );
	m_flZNextItemUse = gpGlobals->time + 0.3f;
}

void CLinkPlayer::ZFireHookshot( void )
{
	if ( m_hZHookshot != NULL )
		return;

	Vector dir = ZAimVector();
	Vector src = ( ZAction() == ZACT_AIM ) ? ( pev->origin + pev->view_ofs + dir * 16 ) : ZHandPosition();
	BOOL longshot = ( m_ZInv.items & ( 1 << ZITEM_LONGSHOT ) ) != 0;

	m_hZHookshot = CZeldaHookshot::Shoot( this, src, dir, longshot );

	// Hookshotting drops out of first person
	if ( ZAction() == ZACT_AIM )
		ZSetAction( ZACT_STAND );
}

void CLinkPlayer::ZPullBomb( void )
{
	if ( m_ZInv.ammo[ZITEM_BOMB] <= 0 )
	{
		ZSound( CHAN_ITEM, ZS_SY_ERROR );
		return;
	}

	m_ZInv.ammo[ZITEM_BOMB]--;

	CZeldaBomb *pBomb = GetClassPtr( (CZeldaBomb *)NULL );
	pBomb->pev->classname = MAKE_STRING( "zl_bomb" );
	pBomb->Spawn();
	pBomb->pev->owner = edict();
	pBomb->m_hHolder = this;
	pBomb->pev->movetype = MOVETYPE_NOCLIP;
	pBomb->pev->solid = SOLID_NOT;
	UTIL_SetOrigin( pBomb->pev, pev->origin + Vector( 0, 0, 44 ) );

	m_hZHeldBomb = pBomb;
	ZSound( CHAN_ITEM, ZS_IT_PUT_AWAY, 0.6f );
}

// Throw forward when moving, set down at Link's feet when standing (Player_UpdateCarry)
void CLinkPlayer::ZThrowBomb( BOOL drop )
{
	CZeldaBomb *pBomb = (CZeldaBomb *)(CBaseEntity *)m_hZHeldBomb;
	m_hZHeldBomb = NULL;

	if ( !pBomb )
		return;

	pBomb->m_hHolder = NULL;
	pBomb->pev->movetype = MOVETYPE_BOUNCE;
	pBomb->pev->solid = SOLID_BBOX;

	if ( drop )
	{
		UTIL_SetOrigin( pBomb->pev, pev->origin + ZForward() * 24 + Vector( 0, 0, -20 ) );
		pBomb->pev->velocity = Vector( 0, 0, 0 );
	}
	else
	{
		Vector dir = ( m_hZTarget != NULL ) ? ZAimVector() : ZForward();
		dir.z = 0;
		dir = dir.Normalize();
		UTIL_SetOrigin( pBomb->pev, pev->origin + dir * 16 + Vector( 0, 0, 30 ) );
		pBomb->pev->velocity = dir * ZL_VEL( 12.0f ) + Vector( 0, 0, ZL_VEL( 6.0f ) );
		ZSound( CHAN_VOICE, ZS_VO_SWORD_N, 0.7f );
	}
}

void CLinkPlayer::ZThrowNut( void )
{
	if ( m_ZInv.ammo[ZITEM_DEKU_NUT] <= 0 )
	{
		ZSound( CHAN_ITEM, ZS_SY_ERROR );
		return;
	}
	m_ZInv.ammo[ZITEM_DEKU_NUT]--;

	CZeldaNut *pNut = GetClassPtr( (CZeldaNut *)NULL );
	pNut->pev->classname = MAKE_STRING( "zl_nut" );
	pNut->Spawn();
	pNut->pev->owner = edict();
	UTIL_SetOrigin( pNut->pev, ZHandPosition() );

	Vector dir = ZAimVector();
	pNut->pev->velocity = dir * ZL_VEL( 10.0f ) + Vector( 0, 0, ZL_VEL( 3.0f ) );
	m_flZNextItemUse = gpGlobals->time + 0.4f;
}

void CLinkPlayer::ZItemButtonDown( int slot )
{
	int item = m_ZInv.cButton[slot];
	int act = ZAction();

	if ( act == ZACT_TALK || act == ZACT_GETITEM || act == ZACT_DEAD || act == ZACT_HOOKFLY )
		return;

	if ( item == ZITEM_NONE )
	{
		ZSound( CHAN_ITEM, ZS_SY_ERROR, 0.6f );
		return;
	}

	// Pressing a C button while holding a bomb throws it
	if ( m_hZHeldBomb != NULL )
	{
		Vector vel = pev->velocity;
		vel.z = 0;
		ZThrowBomb( vel.Length() < 20 );
		return;
	}

	if ( gpGlobals->time < m_flZNextItemUse )
		return;

	switch ( item )
	{
	case ZITEM_BOW:
		if ( m_ZInv.ammo[ZITEM_BOW] <= 0 )
		{
			ZSound( CHAN_ITEM, ZS_SY_ERROR );
			return;
		}
		m_iZHeldItem = item;
		m_iZHeldButton = slot;
		m_flZItemTime = gpGlobals->time;
		ZSound( CHAN_ITEM, ZS_IT_BOW_DRAW, 0.8f );
		break;

	case ZITEM_HOOKSHOT:
	case ZITEM_LONGSHOT:
		if ( m_hZHookshot != NULL )
			return;
		m_iZHeldItem = item;
		m_iZHeldButton = slot;
		m_flZItemTime = gpGlobals->time;
		ZSound( CHAN_ITEM, ZS_IT_HOOKSHOT_READY, 0.8f );
		break;

	case ZITEM_BOMB:
		if ( ( pev->flags & FL_ONGROUND ) && act == ZACT_STAND )
			ZPullBomb();
		break;

	case ZITEM_DEKU_NUT:
		ZThrowNut();
		break;

	case ZITEM_HLWEAPON:
	{
		// Make the borrowed weapon active; holding the C button fires it (ZTranslateHLButtons)
		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			for ( CBasePlayerItem *pItem = m_rgpPlayerItems[i]; pItem; pItem = pItem->m_pNext )
			{
				if ( pItem->m_iId == m_ZInv.cWeapon[slot] && pItem != m_pActiveItem )
				{
					SwitchWeapon( pItem );
					return;
				}
			}
		}
		break;
	}

	default:
		ZSound( CHAN_ITEM, ZS_SY_ERROR, 0.6f );
		break;
	}
}

void CLinkPlayer::ZItemButtonUp( int slot )
{
	if ( slot != m_iZHeldButton )
		return;

	int item = m_iZHeldItem;
	int act = ZAction();
	BOOL locked = ( pev->iuser4 & ( ZL_STATE_LOCKED | ZL_STATE_PARALLEL ) ) != 0;
	BOOL drawn = gpGlobals->time - m_flZItemTime >= 0.15f;

	m_iZHeldButton = -1;
	m_iZHeldItem = ZITEM_NONE;

	// Release fires (func_808351D4: !sHeldItemButtonIsHeldDown). Needs to be aiming:
	// first person, or Z-targeting in third person.
	if ( !drawn || ( act != ZACT_AIM && !locked ) )
		return;

	switch ( item )
	{
	case ZITEM_BOW:			ZFireArrow(); break;
	case ZITEM_HOOKSHOT:
	case ZITEM_LONGSHOT:	ZFireHookshot(); break;
	}
}

void CLinkPlayer::ZItemThink( void )
{
	static const int cPad[3] = { ZPAD_CLEFT, ZPAD_CDOWN, ZPAD_CRIGHT };

	for ( int i = 0; i < 3; i++ )
	{
		if ( ZPressed( cPad[i] ) )
			ZItemButtonDown( i );
		else if ( ZReleased( cPad[i] ) )
			ZItemButtonUp( i );
	}

	// Lost the aim (hit, jumped...) while drawing: put it away
	if ( m_iZHeldButton >= 0 )
	{
		int act = ZAction();
		if ( act != ZACT_AIM && act != ZACT_STAND )
		{
			m_iZHeldButton = -1;
			m_iZHeldItem = ZITEM_NONE;
		}
	}

	// HL weapons keep their own ammo; refresh the C button count
	if ( m_pActiveItem )
	{
		for ( int i = 0; i < 3; i++ )
		{
			if ( m_ZInv.cButton[i] == ZITEM_HLWEAPON && m_ZInv.cWeapon[i] == m_pActiveItem->m_iId )
				m_ZInv.ammo[ZITEM_HLWEAPON] = m_pActiveItem->PrimaryAmmoIndex() >= 0 ? m_rgAmmo[m_pActiveItem->PrimaryAmmoIndex()] : -1;
		}
	}
}

/*
=============================================================================
 Pickups (EnItem00 for collectibles, ItemGive for the big ones)
=============================================================================
*/
enum zl_pickup_e
{
	ZPICK_RUPEE_GREEN,
	ZPICK_RUPEE_BLUE,
	ZPICK_RUPEE_RED,
	ZPICK_RUPEE_PURPLE,
	ZPICK_HEART,
	ZPICK_ARROWS,
	ZPICK_BOMBS,
	ZPICK_NUTS,
	ZPICK_MAGIC_SMALL,
	ZPICK_MAGIC_LARGE,
	ZPICK_HEART_CONTAINER,
	ZPICK_KEY,
	ZPICK_BOW,
	ZPICK_BOMBBAG,
	ZPICK_HOOKSHOT,
	ZPICK_LONGSHOT,
	ZPICK_MASTER_SWORD,
	ZPICK_HYLIAN_SHIELD,
	ZPICK_MAGIC_METER,
	ZPICK_DOUBLE_DEFENSE,
};

struct zl_pickup_info_t
{
	const char	*model;
	float		scale;
	int			r, g, b;		// glow shell color, 0 = none
	BOOL		major;			// item get fanfare + text box
	const char	*message;		// zl_messages.txt entry shown on pickup
};

static const zl_pickup_info_t sPickups[] =
{
	{ ZL_MDL_RUPEE,		0.6f, 0, 255, 0,		FALSE, NULL },					// ITEM00_RUPEE_GREEN
	{ ZL_MDL_RUPEE,		0.6f, 0, 80, 255,		FALSE, NULL },					// ITEM00_RUPEE_BLUE
	{ ZL_MDL_RUPEE,		0.7f, 255, 30, 30,		FALSE, NULL },					// ITEM00_RUPEE_RED
	{ ZL_MDL_RUPEE,		0.8f, 200, 0, 255,		FALSE, NULL },					// ITEM00_RUPEE_PURPLE
	{ ZL_MDL_HEART,		0.6f, 255, 40, 40,		FALSE, NULL },					// ITEM00_RECOVERY_HEART
	{ ZL_MDL_ARROWS,	1.0f, 0, 0, 0,			FALSE, NULL },					// ITEM00_ARROWS_SMALL
	{ ZL_MDL_BOMBS,		1.0f, 0, 0, 0,			FALSE, NULL },					// ITEM00_BOMBS_A
	{ ZL_MDL_NUT,		0.6f, 160, 110, 40,		FALSE, NULL },					// ITEM00_NUTS
	{ ZL_MDL_MAGIC,		1.0f, 0, 200, 0,		FALSE, NULL },					// ITEM00_MAGIC_SMALL
	{ ZL_MDL_MAGIC,		1.4f, 0, 200, 0,		FALSE, NULL },					// ITEM00_MAGIC_LARGE
	{ ZL_MDL_CONTAINER,	1.4f, 255, 40, 40,		TRUE, "ITEM_HEART_CONTAINER" },	// GI_HEART_CONTAINER
	{ ZL_MDL_KEY,		1.0f, 220, 220, 255,	TRUE, "ITEM_SMALL_KEY" },		// GI_KEY_SMALL
	{ ZL_MDL_BOW,		1.0f, 0, 0, 0,			TRUE, "ITEM_BOW" },				// GI_BOW
	{ ZL_MDL_BOMBBAG,	1.0f, 0, 0, 0,			TRUE, "ITEM_BOMB_BAG" },		// GI_BOMB_BAG_20
	{ ZL_MDL_HOOKSHOT,	1.0f, 0, 0, 0,			TRUE, "ITEM_HOOKSHOT" },		// GI_HOOKSHOT
	{ ZL_MDL_HOOKSHOT,	1.2f, 120, 120, 255,	TRUE, "ITEM_LONGSHOT" },		// GI_LONGSHOT
	{ ZL_MDL_SWORD,		1.0f, 80, 120, 255,		TRUE, "ITEM_MASTER_SWORD" },	// GI_SWORD_MASTER
	{ ZL_MDL_SHIELD,	1.0f, 0, 0, 0,			TRUE, "ITEM_HYLIAN_SHIELD" },	// GI_SHIELD_HYLIAN
	{ ZL_MDL_MAGIC,		1.4f, 0, 255, 0,		TRUE, "ITEM_MAGIC_METER" },		// Great Fairy: magic meter
	{ ZL_MDL_CONTAINER,	1.4f, 255, 255, 255,	TRUE, "ITEM_DOUBLE_DEFENSE" },	// Great Fairy: double defense
};

class CZeldaPickup : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void ) {}
	void EXPORT PickupTouch( CBaseEntity *pOther );
	void EXPORT PickupThink( void );
	BOOL Give( CLinkPlayer *pLink );

	int m_iType;
	float m_flSpawnTime;
};

void CZeldaPickup::Spawn( void )
{
	const zl_pickup_info_t &info = sPickups[m_iType];

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	SET_MODEL( ENT( pev ), info.model );
	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 24 ) );
	UTIL_SetOrigin( pev, pev->origin );
	pev->scale = info.scale;

	if ( info.r || info.g || info.b )
	{
		pev->renderfx = kRenderFxGlowShell;
		pev->rendercolor = Vector( info.r, info.g, info.b );
		pev->renderamt = 4;
	}

	// Collectibles spin (EnItem00 shape.rot.y += 960)
	pev->avelocity = Vector( 0, 120, 0 );
	m_flSpawnTime = gpGlobals->time;

	SetTouch( &CZeldaPickup::PickupTouch );

	// Dropped collectibles (owner = the monster that dropped them) blink out after a while
	// (EnItem00 despawn timer). Mapper placed ones stay.
	if ( !info.major && pev->owner != NULL )
	{
		SetThink( &CZeldaPickup::PickupThink );
		pev->nextthink = gpGlobals->time + 1.0f;
	}
}

void CZeldaPickup::PickupThink( void )
{
	float age = gpGlobals->time - m_flSpawnTime;

	if ( age > 15.0f )
	{
		UTIL_Remove( this );
		return;
	}

	if ( age > 11.0f )
		pev->effects ^= EF_NODRAW;

	pev->nextthink = gpGlobals->time + ( age > 11.0f ? 0.1f : 1.0f );
}

BOOL CZeldaPickup::Give( CLinkPlayer *pLink )
{
	switch ( m_iType )
	{
	case ZPICK_RUPEE_GREEN:	if ( pLink->m_iZRupees >= pLink->m_iZWallet ) return FALSE; pLink->ZAddRupees( 1 ); break;
	case ZPICK_RUPEE_BLUE:	if ( pLink->m_iZRupees >= pLink->m_iZWallet ) return FALSE; pLink->ZAddRupees( 5 ); break;
	case ZPICK_RUPEE_RED:	if ( pLink->m_iZRupees >= pLink->m_iZWallet ) return FALSE; pLink->ZAddRupees( 20 ); break;
	case ZPICK_RUPEE_PURPLE: if ( pLink->m_iZRupees >= pLink->m_iZWallet ) return FALSE; pLink->ZAddRupees( 50 ); break;
	case ZPICK_HEART:
		pLink->pev->health = min( pLink->pev->max_health, pLink->pev->health + ZL_HEART );
		break;
	case ZPICK_ARROWS:
		if ( !( pLink->m_ZInv.items & ( 1 << ZITEM_BOW ) ) ) return FALSE;
		pLink->m_ZInv.ammo[ZITEM_BOW] = min( pLink->m_ZInv.quiver, pLink->m_ZInv.ammo[ZITEM_BOW] + 10 );
		break;
	case ZPICK_BOMBS:
		if ( !( pLink->m_ZInv.items & ( 1 << ZITEM_BOMB ) ) ) return FALSE;
		pLink->m_ZInv.ammo[ZITEM_BOMB] = min( pLink->m_ZInv.bombBag, pLink->m_ZInv.ammo[ZITEM_BOMB] + 5 );
		break;
	case ZPICK_NUTS:
		pLink->ZGiveItem( ZITEM_DEKU_NUT );
		pLink->m_ZInv.ammo[ZITEM_DEKU_NUT] = min( 40, pLink->m_ZInv.ammo[ZITEM_DEKU_NUT] + 5 );
		break;
	case ZPICK_MAGIC_SMALL:	if ( !pLink->m_iZMagicCapacity ) return FALSE; pLink->ZAddMagic( 12 ); break;
	case ZPICK_MAGIC_LARGE:	if ( !pLink->m_iZMagicCapacity ) return FALSE; pLink->ZAddMagic( 48 ); break;
	case ZPICK_HEART_CONTAINER: pLink->ZAddHeartContainer(); break;
	case ZPICK_KEY:			pLink->m_ZInv.smallKeys++; break;
	case ZPICK_BOW:			pLink->ZGiveItem( ZITEM_BOW ); break;
	case ZPICK_BOMBBAG:		pLink->ZGiveItem( ZITEM_BOMB ); break;
	case ZPICK_HOOKSHOT:	pLink->ZGiveItem( ZITEM_HOOKSHOT ); break;
	case ZPICK_LONGSHOT:
		pLink->ZGiveItem( ZITEM_HOOKSHOT );
		pLink->m_ZInv.items |= ( 1 << ZITEM_LONGSHOT );
		for ( int i = 0; i < 3; i++ )
			if ( pLink->m_ZInv.cButton[i] == ZITEM_HOOKSHOT )
				pLink->m_ZInv.cButton[i] = ZITEM_LONGSHOT;
		break;
	case ZPICK_MASTER_SWORD:	pLink->ZGiveEquipment( ZEQ_SWORD_MASTER ); break;
	case ZPICK_HYLIAN_SHIELD:	pLink->ZGiveEquipment( ZEQ_SHIELD_HYLIAN ); break;
	case ZPICK_MAGIC_METER:
		pLink->m_iZMagicCapacity = pLink->m_iZMagicCapacity ? ZL_MAGIC_DOUBLE : ZL_MAGIC_SINGLE;
		pLink->m_iZMagic = pLink->m_iZMagicCapacity;
		break;
	case ZPICK_DOUBLE_DEFENSE:
		pLink->m_fZDoubleDefense = TRUE;
		pLink->pev->health = pLink->pev->max_health;
		break;
	}
	return TRUE;
}

void CZeldaPickup::PickupTouch( CBaseEntity *pOther )
{
	CLinkPlayer *pLink = ZL_GetLink( pOther );
	if ( !pLink || !pLink->IsAlive() || !pLink->LinkActive() )
		return;

	// Fresh drops can't be grabbed in the same instant they pop out
	if ( gpGlobals->time - m_flSpawnTime < 0.3f && pev->velocity.z > 0 )
		return;

	if ( !Give( pLink ) )
		return;

	const zl_pickup_info_t &info = sPickups[m_iType];

	switch ( m_iType )
	{
	case ZPICK_RUPEE_GREEN:
	case ZPICK_RUPEE_BLUE:
	case ZPICK_RUPEE_RED:
	case ZPICK_RUPEE_PURPLE:
		pLink->ZSound( CHAN_ITEM, ZS_SY_GET_RUPY );
		break;
	case ZPICK_HEART:
		pLink->ZSound( CHAN_ITEM, ZS_SY_GET_HEART );
		break;
	default:
		pLink->ZSound( CHAN_ITEM, info.major ? ZS_SY_GET_BOXITEM : ZS_SY_GET_ITEM );
		break;
	}

	if ( info.major && info.message )
	{
		// Link holds it over his head (Player_Action_8084E3C4) while the text box is up
		pLink->ZSetAction( ZACT_GETITEM );
		pLink->ZStartMessage( info.message, NULL, 0 );

		MESSAGE_BEGIN( MSG_ONE, gmsgZItemGet, NULL, pLink->pev );
			WRITE_BYTE( m_iType );
		MESSAGE_END();
	}

	SUB_UseTargets( pLink, USE_TOGGLE, 0 );
	UTIL_Remove( this );
}

#define ZL_DEFINE_PICKUP( classname, type )							\
	class CZeldaPickup_##type : public CZeldaPickup					\
	{																\
	public:															\
		void Spawn( void ) { m_iType = type; CZeldaPickup::Spawn(); }	\
	};																\
	LINK_ENTITY_TO_CLASS( classname, CZeldaPickup_##type );

ZL_DEFINE_PICKUP( item_zl_rupee, ZPICK_RUPEE_GREEN )
ZL_DEFINE_PICKUP( item_zl_rupee_blue, ZPICK_RUPEE_BLUE )
ZL_DEFINE_PICKUP( item_zl_rupee_red, ZPICK_RUPEE_RED )
ZL_DEFINE_PICKUP( item_zl_rupee_purple, ZPICK_RUPEE_PURPLE )
ZL_DEFINE_PICKUP( item_zl_heart, ZPICK_HEART )
ZL_DEFINE_PICKUP( item_zl_arrows, ZPICK_ARROWS )
ZL_DEFINE_PICKUP( item_zl_bombs, ZPICK_BOMBS )
ZL_DEFINE_PICKUP( item_zl_nuts, ZPICK_NUTS )
ZL_DEFINE_PICKUP( item_zl_magic, ZPICK_MAGIC_SMALL )
ZL_DEFINE_PICKUP( item_zl_magic_large, ZPICK_MAGIC_LARGE )
ZL_DEFINE_PICKUP( item_zl_heartcontainer, ZPICK_HEART_CONTAINER )
ZL_DEFINE_PICKUP( item_zl_key, ZPICK_KEY )
ZL_DEFINE_PICKUP( item_zl_bow, ZPICK_BOW )
ZL_DEFINE_PICKUP( item_zl_bombbag, ZPICK_BOMBBAG )
ZL_DEFINE_PICKUP( item_zl_hookshot, ZPICK_HOOKSHOT )
ZL_DEFINE_PICKUP( item_zl_longshot, ZPICK_LONGSHOT )
ZL_DEFINE_PICKUP( item_zl_mastersword, ZPICK_MASTER_SWORD )
ZL_DEFINE_PICKUP( item_zl_hylianshield, ZPICK_HYLIAN_SHIELD )
ZL_DEFINE_PICKUP( item_zl_magicmeter, ZPICK_MAGIC_METER )
ZL_DEFINE_PICKUP( item_zl_doubledefense, ZPICK_DOUBLE_DEFENSE )

/*
=============================================================================
 zl_chest: treasure chest (EnBox). "item" keyvalue names the item_zl_* inside.
=============================================================================
*/
class CZeldaChest : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void ) { PRECACHE_MODEL( (char *)STRING( pev->model ) ); }
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return CBaseAnimating::ObjectCaps() | FCAP_IMPULSE_USE; }
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	string_t m_iszItem;
};

LINK_ENTITY_TO_CLASS( zl_chest, CZeldaChest );

TYPEDESCRIPTION CZeldaChest::m_SaveData[] =
{
	DEFINE_FIELD( CZeldaChest, m_iszItem, FIELD_STRING ),
};
IMPLEMENT_SAVERESTORE( CZeldaChest, CBaseAnimating );

void CZeldaChest::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "item" ) )
	{
		m_iszItem = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseAnimating::KeyValue( pkvd );
}

void CZeldaChest::Spawn( void )
{
	if ( FStringNull( pev->model ) )
		pev->model = MAKE_STRING( ZL_MDL_CHEST );
	Precache();
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	if ( !pev->scale )
		pev->scale = 1.6f;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;
	UTIL_SetSize( pev, Vector( -20, -14, 0 ), Vector( 20, 14, 28 ) );
	UTIL_SetOrigin( pev, pev->origin );
	DROP_TO_FLOOR( ENT( pev ) );
}

void CZeldaChest::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->frame != 0 )
		return;

	CLinkPlayer *pLink = ZL_GetLink( pActivator );
	if ( !pLink )
		return;

	pev->frame = 1;		// open
	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = Vector( 255, 230, 120 );
	pev->renderamt = 2;

	ZL_EmitSound( edict(), CHAN_BODY, ZS_EV_CHEST_OPEN, 1, ATTN_NORM, PITCH_NORM );

	const char *cls = FStringNull( m_iszItem ) ? "item_zl_rupee_blue" : STRING( m_iszItem );
	CBaseEntity *pItem = CBaseEntity::Create( (char *)cls, pLink->pev->origin, g_vecZero, NULL );
	if ( pItem )
		pItem->Touch( pLink );

	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
}

/*
=============================================================================
 Half-Life weapon pickups go on a free C button
=============================================================================
*/
void ZL_OnWeaponPickup( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	CLinkPlayer *pLink = ZL_GetLink( pPlayer );
	if ( !pLink || !pLink->LinkActive() || !pItem )
		return;

	for ( int i = 0; i < 3; i++ )
	{
		if ( pLink->m_ZInv.cButton[i] == ZITEM_HLWEAPON && pLink->m_ZInv.cWeapon[i] == pItem->m_iId )
			return;
	}

	for ( int i = 0; i < 3; i++ )
	{
		if ( pLink->m_ZInv.cButton[i] == ZITEM_NONE )
		{
			pLink->m_ZInv.cButton[i] = ZITEM_HLWEAPON;
			pLink->m_ZInv.cWeapon[i] = pItem->m_iId;
			return;
		}
	}
}

/*
=============================================================================
 Console: inventory management until there is a subscreen
=============================================================================
*/
BOOL ZL_ItemCommand( CLinkPlayer *pLink, const char *pcmd )
{
	if ( FStrEq( pcmd, "zl_equip" ) )
	{
		// zl_equip <0|1|2> <item number>   (0 C-left, 1 C-down, 2 C-right)
		if ( CMD_ARGC() < 3 )
		{
			ClientPrint( pLink->pev, HUD_PRINTCONSOLE, "zl_equip <slot 0-2> <item: 1 bow, 2 bombs, 3 hookshot, 4 longshot, 5 nuts>\n" );
			return TRUE;
		}
		int slot = atoi( CMD_ARGV( 1 ) );
		int item = atoi( CMD_ARGV( 2 ) );
		if ( slot < 0 || slot > 2 || item < 0 || item >= ZITEM_COUNT || item == ZITEM_HLWEAPON )
			return TRUE;
		if ( item != ZITEM_NONE && !( pLink->m_ZInv.items & ( 1 << item ) ) )
		{
			ClientPrint( pLink->pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "You don't have the %s.\n", ZL_ItemName( item ) ) );
			return TRUE;
		}
		for ( int i = 0; i < 3; i++ )
			if ( pLink->m_ZInv.cButton[i] == item && item != ZITEM_NONE )
				pLink->m_ZInv.cButton[i] = ZITEM_NONE;
		pLink->m_ZInv.cButton[slot] = item;
		pLink->ZSound( CHAN_ITEM, ZS_SY_DECIDE, 0.6f, ATTN_NONE );
		return TRUE;
	}

	if ( FStrEq( pcmd, "zl_inventory" ) )
	{
		ClientPrint( pLink->pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "Hearts %d/%d  Magic %d/%d  Rupees %d/%d  Keys %d\n",
			(int)pLink->pev->health, pLink->m_iZHealthCapacity, pLink->m_iZMagic, pLink->m_iZMagicCapacity,
			pLink->m_iZRupees, pLink->m_iZWallet, pLink->m_ZInv.smallKeys ) );
		for ( int i = 1; i < ZITEM_COUNT; i++ )
		{
			if ( pLink->m_ZInv.items & ( 1 << i ) )
				ClientPrint( pLink->pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "  %d %s (%d)\n", i, ZL_ItemName( i ), pLink->ZItemAmmo( i ) ) );
		}
		ClientPrint( pLink->pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "C buttons: %s / %s / %s\n",
			ZL_ItemName( pLink->m_ZInv.cButton[0] ), ZL_ItemName( pLink->m_ZInv.cButton[1] ), ZL_ItemName( pLink->m_ZInv.cButton[2] ) ) );
		return TRUE;
	}

	if ( FStrEq( pcmd, "zl_give" ) )
	{
		if ( !CVAR_GET_FLOAT( "sv_cheats" ) || CMD_ARGC() < 2 )
			return TRUE;
		const char *what = CMD_ARGV( 1 );
		if ( !strcmp( what, "all" ) )
		{
			pLink->m_ZInv.equipment |= ZEQ_SWORD_KOKIRI | ZEQ_SWORD_MASTER | ZEQ_SHIELD_DEKU | ZEQ_SHIELD_HYLIAN;
			pLink->ZGiveItem( ZITEM_BOW );
			pLink->ZGiveItem( ZITEM_BOMB );
			pLink->ZGiveItem( ZITEM_HOOKSHOT );
			pLink->ZGiveItem( ZITEM_DEKU_NUT );
			pLink->m_iZMagicCapacity = ZL_MAGIC_SINGLE;
			pLink->m_iZMagic = ZL_MAGIC_SINGLE;
			pLink->m_iZWallet = ZL_WALLET_ADULT;
			pLink->m_iZRupees = ZL_WALLET_ADULT;
			while ( pLink->m_iZHealthCapacity < 10 * ZL_HEART )
				pLink->ZAddHeartContainer();
		}
		else if ( !strcmp( what, "rupees" ) )
			pLink->ZAddRupees( CMD_ARGC() > 2 ? atoi( CMD_ARGV( 2 ) ) : 100 );
		else if ( !strcmp( what, "heart" ) )
			pLink->ZAddHeartContainer();
		else if ( !strcmp( what, "magic" ) )
		{
			pLink->m_iZMagicCapacity = ZL_MAGIC_SINGLE;
			pLink->m_iZMagic = ZL_MAGIC_SINGLE;
		}
		else
			pLink->ZGiveItem( atoi( what ) );
		return TRUE;
	}

	return FALSE;
}

// item_battery in Half-Life maps: a small magic jar, or a blue rupee before the magic meter
BOOL ZL_BatteryPickup( CBasePlayer *pPlayer )
{
	CLinkPlayer *pLink = ZL_GetLink( pPlayer );
	if ( !pLink || !pLink->LinkActive() )
		return FALSE;

	if ( pLink->m_iZMagicCapacity > 0 && pLink->m_iZMagic < pLink->m_iZMagicCapacity )
	{
		pLink->ZAddMagic( 12 );
		pLink->ZSound( CHAN_ITEM, ZS_SY_GET_ITEM );
		return TRUE;
	}

	if ( pLink->m_iZMagicCapacity == 0 && pLink->m_iZRupees < pLink->m_iZWallet )
	{
		pLink->ZAddRupees( 5 );
		pLink->ZSound( CHAN_ITEM, ZS_SY_GET_RUPY );
		return TRUE;
	}

	return FALSE;
}
