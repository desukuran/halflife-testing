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
#include "animation.h"

#include <math.h>
#include <ctype.h>

#define ZL_MDL_ARROW		"models/crossbow_bolt.mdl"	// OOT: object_link_boy arrow (gLinkAdultArrowDL)
#define ZL_MDL_BOMB			"models/w_grenade.mdl"		// OOT: object_bomb / gameplay_keep gBombBodyDL
#define ZL_MDL_BOMB_OOT		"models/zelda/bomb.mdl"		// make_object_models.py: gBombCapDL + gBombBodyDL
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

static void ZL_PrecacheGetItemModels( void );

void ZL_PrecacheItems( void )
{
	PRECACHE_MODEL( ZL_MDL_ARROW );
	PRECACHE_MODEL( ZL_MDL_BOMB );
	if ( ZL_FileExists( ZL_MDL_BOMB_OOT ) )
		PRECACHE_MODEL( ZL_MDL_BOMB_OOT );
	ZL_PrecacheGetItemModels();
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
	void FuseFrame( void );

	EHANDLE m_hHolder;		// set while Link carries it over his head
	BOOL	m_fOoT;			// models/zelda/bomb.mdl: flash skins, camera facing (client), growth
	int		m_iTimer;		// EnBom timer, OoT frames left
	int		m_iFlashSpeed;	// flashSpeedScale
	float	m_flFlash;		// flashIntensity 0..140
	float	m_flNextFrame;
};

#define ZL_BOMB_FLASH_SKINS	8	// make_object_models.py FLASH_LEVELS

LINK_ENTITY_TO_CLASS( zl_bomb, CZeldaBomb );

#define ZL_BOMB_FUSE	ZL_FRAMES( 70 )	// EnBom timer

void CZeldaBomb::Spawn( void )
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->friction = 0.6f;
	pev->gravity = 0.8f;
	m_fOoT = ZL_FileExists( ZL_MDL_BOMB_OOT );
	SET_MODEL( ENT( pev ), m_fOoT ? ZL_MDL_BOMB_OOT : ZL_MDL_BOMB );
	if ( m_fOoT )
	{
		// The model is centred on the bomb; OoT draws it 7 units up from the floor (shape.yOffset 700)
		UTIL_SetSize( pev, Vector( -ZL_DIST( 5 ), -ZL_DIST( 5 ), -ZL_DIST( 7 ) ), Vector( ZL_DIST( 5 ), ZL_DIST( 5 ), ZL_DIST( 7 ) ) );
		pev->effects |= EF_NODRAW;		// scale 0 until the timer reaches 67
	}
	else
		UTIL_SetSize( pev, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ) );
	m_iTimer = 70;
	m_iFlashSpeed = 7;
	m_flFlash = 0;
	m_flNextFrame = gpGlobals->time + ZL_FRAMES( 1 );
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
		// Between Link's hands over his head (PLAYER_STATE1_CARRYING_ACTOR: the held actor sits
		// at sGetItemRefPos), the bomb's centre 7 units above that (shape.yOffset 700). Only
		// roughly: the server poses link.mdl without the client's prediction and leg blending,
		// so the client draws it at the hands it actually drew (pev->playerclass = holder).
		CLinkPlayer *pLink = ZL_GetLink( pHolder );
		Vector hands;
		if ( m_fOoT && pLink && pLink->ZHandsMidpoint( hands ) )
			UTIL_SetOrigin( pev, hands + Vector( 0, 0, ZL_DIST( 7 ) ) );
		else
			UTIL_SetOrigin( pev, pHolder->pev->origin + Vector( 0, 0, 44 ) );
		pev->velocity = g_vecZero;
	}

	while ( gpGlobals->time >= m_flNextFrame && m_iTimer > 0 )
	{
		m_flNextFrame += ZL_FRAMES( 1 );
		FuseFrame();
	}

	if ( m_iTimer <= 0 || pev->dmgtime <= gpGlobals->time )
	{
		Explode();
		return;
	}

	pev->nextthink = gpGlobals->time + 0.01f;
}

// One OoT frame of EnBom_Update's fuse
void CZeldaBomb::FuseFrame( void )
{
	m_iTimer--;

	if ( m_iTimer == 67 )
		pev->effects &= ~EF_NODRAW;		// Actor_SetScale( 0.01f )

	// The flash doubles its speed at 40, 20 and 3 frames left, and pulses between 0 and 140 red
	if ( m_iTimer == 3 || m_iTimer == 20 || m_iTimer == 40 )
		m_iFlashSpeed >>= 1;
	int speed = max( m_iFlashSpeed, 1 );
	float step = 140.0f / speed;
	if ( m_iTimer & ( m_iFlashSpeed + 1 ) )
		m_flFlash = min( 140.0f, m_flFlash + step );
	else
		m_flFlash = max( 0.0f, m_flFlash - step );

	if ( m_fOoT )
	{
		pev->skin = (int)( m_flFlash / 140.0f * ( ZL_BOMB_FLASH_SKINS - 1 ) + 0.5f );

		// Swells by 0.002 a frame (20%) for the last 3 frames
		if ( m_iTimer < 3 )
			pev->scale = 1.0f + ( 3 - m_iTimer ) * 0.2f;

		// The fuse: a spark on its tip every other frame (EffectSsGSpk_SpawnFuse, 17 units up
		// from the bomb's base)
		if ( m_iTimer < 63 && !( m_iTimer & 1 ) )
		{
			Vector tip = pev->origin + Vector( 0, 0, ZL_DIST( 17 - 7 ) );
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tip );
				WRITE_BYTE( TE_SPRITE );
				WRITE_COORD( tip.x );
				WRITE_COORD( tip.y );
				WRITE_COORD( tip.z );
				WRITE_SHORT( MODEL_INDEX( ZL_SPR_FLASH ) );
				WRITE_BYTE( 1 );		// scale in 10ths
				WRITE_BYTE( 200 );		// brightness
			MESSAGE_END();
		}
	}
	else if ( m_flFlash > 70.0f )
	{
		// Stock model: a red glow stands in for the flash
		pev->renderfx = kRenderFxGlowShell;
		pev->rendercolor = Vector( 255, 30, 30 );
		pev->renderamt = 10;
	}
	else
		pev->renderfx = kRenderFxNone;
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
				pev->ltime = gpGlobals->time;	// grace period before "stuck" can end the pull
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
	case ZITEM_BOW:			return TRUE;
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

// link.mdl's L_Hand and R_Hand bones, posed at Link's facing: the server's angles are the
// camera's, the client draws him at fuser1 (ZL_LocalLinkFacing)
BOOL CLinkPlayer::ZHandsMidpoint( Vector &out )
{
	if ( !strstr( STRING( pev->model ), "/link/" ) )
		return FALSE;

	void *pmodel = GET_MODEL_PTR( ENT( pev ) );
	int lhand = LookupBone( pmodel, "L_Hand" ), rhand = LookupBone( pmodel, "R_Hand" );
	if ( lhand < 0 || rhand < 0 )
		return FALSE;

	Vector saved = pev->angles, l, r, ang;
	pev->angles = Vector( 0, ZFacing(), 0 );
	GetBonePosition( lhand, l, ang );
	GetBonePosition( rhand, r, ang );
	pev->angles = saved;

	out = ( l + r ) * 0.5f;
	return TRUE;
}

Vector CLinkPlayer::ZHandPosition( void )
{
	return pev->origin + ZForward() * 14 + Vector( 0, 0, 22 );
}

// First person: the camera. Locked on: straight at the target. Otherwise Link's facing.
// First person this tick: aiming now, or aiming going into the move that just let go of C
BOOL CLinkPlayer::ZFirstPersonAim( void )
{
	return ZAction() == ZACT_AIM || m_iZPreMoveAction == ZACT_AIM;
}

Vector CLinkPlayer::ZAimVector( void )
{
	int act = ZAction();
	CBaseEntity *pTarget = m_hZTarget;

	if ( ZFirstPersonAim() || act == ZACT_CUP )
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
	Vector src = ZFirstPersonAim() ? ( pev->origin + pev->view_ofs + dir * 16 ) : ZHandPosition();

	CZeldaArrow::Shoot( this, src, dir );
	ZSound( CHAN_WEAPON, ZS_IT_ARROW_SHOT );
	m_flZNextItemUse = gpGlobals->time + 0.3f;
}

void CLinkPlayer::ZFireHookshot( void )
{
	if ( m_hZHookshot != NULL )
		return;

	Vector dir = ZAimVector();
	Vector src = ZFirstPersonAim() ? ( pev->origin + pev->view_ofs + dir * 16 ) : ZHandPosition();
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
	pBomb->pev->playerclass = entindex();	// the client puts it between his drawn hands (StudioModelRenderer.cpp)
	pBomb->pev->movetype = MOVETYPE_NOCLIP;
	pBomb->pev->solid = SOLID_NOT;
	Vector hands;
	if ( pBomb->m_fOoT && ZHandsMidpoint( hands ) )
		UTIL_SetOrigin( pBomb->pev, hands + Vector( 0, 0, ZL_DIST( 7 ) ) );
	else
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
	pBomb->pev->playerclass = 0;
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

	if ( act == ZACT_TALK || act == ZACT_GETITEM || act == ZACT_CHEST || act == ZACT_DEAD || act == ZACT_HOOKFLY )
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
		// OoT aims the bow with no arrows too (only explosives, sticks and beans check their
		// ammo up front); there's just nothing to fire
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
	// PMZ_ActAim drops back to STAND on the command that lets go of the button, which runs
	// before this: what counts is whether Link was aiming going into that move
	int act = ZFirstPersonAim() ? ZACT_AIM : ZAction();
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

// The item's own get-item model (make_object_models.py, sDrawItemTable), per zl_pickup_e.
// Shown over Link's head when he gets it, and lying in the world instead of the stand-in.
static const char *sPickupGI[] =
{
	"models/zelda/gi_rupee_green.mdl",
	"models/zelda/gi_rupee_blue.mdl",
	"models/zelda/gi_rupee_red.mdl",
	"models/zelda/gi_rupee_purple.mdl",
	"models/zelda/gi_heart.mdl",
	"models/zelda/gi_arrows.mdl",
	"models/zelda/gi_bomb.mdl",
	"models/zelda/gi_nuts.mdl",
	"models/zelda/gi_magic_small.mdl",
	"models/zelda/gi_magic_large.mdl",
	"models/zelda/gi_heart_container.mdl",
	"models/zelda/gi_key.mdl",
	"models/zelda/gi_bow.mdl",
	"models/zelda/gi_bombbag.mdl",
	"models/zelda/gi_hookshot.mdl",
	"models/zelda/gi_longshot.mdl",
	"models/zelda/gi_sword.mdl",			// OoT has no get-item Master Sword: the Kokiri Sword's
	"models/zelda/gi_shield_hylian.mdl",
	"models/zelda/gi_magic_large.mdl",		// magic meter (the Great Fairy gives it without a model)
	"models/zelda/gi_heart_container.mdl",	// double defense (same)
};
static BOOL sPickupGIExists[ARRAYSIZE( sPickupGI )];

// EnItem00_DrawCollectible: bombs, arrows, nuts and magic jars lying around are billboards
// (sItemDropTex, ripped into zl_drops.spr), at their actor scale over gItemDropVtx (500 square)
// and lifted by shape.yOffset
#define ZL_DROPS_SPRITE		"sprites/oot/zl_drops.spr"
static BOOL sDropsSprite;

struct zl_drop_sprite_t { int type; int frame; float scale; float yOffset; };
static const zl_drop_sprite_t sDropSprites[] =
{
	{ ZPICK_BOMBS,			1, 0.03f, 320.0f },		// ITEM00_BOMBS_A
	{ ZPICK_ARROWS,			2, 0.035f, 250.0f },	// ITEM00_ARROWS_SMALL
	{ ZPICK_NUTS,			6, 0.03f, 320.0f },
	{ ZPICK_MAGIC_LARGE,	8, 0.045f, 320.0f },
	{ ZPICK_MAGIC_SMALL,	9, 0.03f, 320.0f },
};

static const zl_drop_sprite_t *ZL_DropSprite( int type )
{
	if ( !sDropsSprite )
		return NULL;
	for ( int i = 0; i < (int)ARRAYSIZE( sDropSprites ); i++ )
	{
		if ( sDropSprites[i].type == type )
			return &sDropSprites[i];
	}
	return NULL;
}

static void ZL_PrecacheGetItemModels( void )
{
	sDropsSprite = ZL_FileExists( ZL_DROPS_SPRITE );
	if ( sDropsSprite )
		PRECACHE_MODEL( (char *)ZL_DROPS_SPRITE );

	for ( int i = 0; i < (int)ARRAYSIZE( sPickupGI ); i++ )
	{
		sPickupGIExists[i] = ZL_FileExists( sPickupGI[i] );
		if ( sPickupGIExists[i] )
			PRECACHE_MODEL( (char *)sPickupGI[i] );
	}
}

static const char *ZL_PickupGIModel( int type )
{
	if ( type < 0 || type >= (int)ARRAYSIZE( sPickupGI ) || !sPickupGIExists[type] )
		return NULL;
	return sPickupGI[type];
}

/*
=============================================================================
 zl_gi_model: the item Link holds up (Player_DrawGetItemImpl)

 Between his hands, 14 units up and 3.3 forward, turning 1000 binangs a frame, until he
 puts it away.
=============================================================================
*/
class CZeldaGetItemModel : public CBaseEntity
{
public:
	void Spawn( void ) {}
	void EXPORT FollowThink( void );
	static CZeldaGetItemModel *Show( CLinkPlayer *pLink, const char *model );
	EHANDLE m_hLink;
};

LINK_ENTITY_TO_CLASS( zl_gi_model, CZeldaGetItemModel );

CZeldaGetItemModel *CZeldaGetItemModel::Show( CLinkPlayer *pLink, const char *model )
{
	CZeldaGetItemModel *p = GetClassPtr( (CZeldaGetItemModel *)NULL );
	p->pev->classname = MAKE_STRING( "zl_gi_model" );
	SET_MODEL( p->edict(), model );
	p->pev->movetype = MOVETYPE_NOCLIP;
	p->pev->solid = SOLID_NOT;
	p->pev->angles = Vector( 0, pLink->ZFacing(), 0 );
	p->pev->avelocity = Vector( 0, 1000.0f * ( 360.0f / 65536.0f ) * ZL_FPS, 0 );
	p->m_hLink = pLink;
	p->SetThink( &CZeldaGetItemModel::FollowThink );
	p->FollowThink();
	return p;
}

void CZeldaGetItemModel::FollowThink( void )
{
	CLinkPlayer *pLink = ZL_GetLink( m_hLink );
	if ( !pLink || !pLink->IsAlive() || pLink->ZAction() != ZACT_GETITEM )
	{
		UTIL_Remove( this );
		return;
	}

	Vector at;
	if ( !pLink->ZHandsMidpoint( at ) )
		at = pLink->pev->origin + Vector( 0, 0, pLink->pev->maxs.z );	// no link.mdl: the top of his head
	at = at + pLink->ZForward() * ZL_DIST( 3.3f ) + Vector( 0, 0, ZL_DIST( 14 ) );
	UTIL_SetOrigin( pev, at );
	pev->nextthink = gpGlobals->time + 0.02f;
}

class CZeldaPickup : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void ) {}
	void EXPORT PickupTouch( CBaseEntity *pOther );
	void EXPORT PickupThink( void );
	void EXPORT CollectedThink( void );
	BOOL Give( CLinkPlayer *pLink );

	int m_iType;
	float m_flSpawnTime;
	EHANDLE m_hCollector;
	int m_iCollectTimer;
};

void CZeldaPickup::Spawn( void )
{
	const zl_pickup_info_t &info = sPickups[m_iType];

	const char *gi = ZL_PickupGIModel( m_iType );
	const zl_drop_sprite_t *spr = ZL_DropSprite( m_iType );

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;

	if ( spr )
	{
		SET_MODEL( ENT( pev ), ZL_DROPS_SPRITE );
		pev->frame = spr->frame;
		pev->scale = 500.0f * spr->scale * ZL_UNIT / 32.0f;
		pev->rendermode = kRenderTransAlpha;
		pev->renderamt = 255;
		// Sits shape.yOffset above the floor
		float lift = spr->yOffset * spr->scale * ZL_UNIT;
		UTIL_SetSize( pev, Vector( -12, -12, -lift ), Vector( 12, 12, 24 - lift ) );
		UTIL_SetOrigin( pev, pev->origin );
		m_flSpawnTime = gpGlobals->time;
		SetTouch( &CZeldaPickup::PickupTouch );
		if ( !info.major && pev->owner != NULL )
		{
			SetThink( &CZeldaPickup::PickupThink );
			pev->nextthink = gpGlobals->time + 1.0f;
		}
		return;
	}

	SET_MODEL( ENT( pev ), gi ? gi : info.model );
	// The get-item models are centred on their origin: lift them off the floor
	UTIL_SetSize( pev, Vector( -12, -12, gi ? -10 : 0 ), Vector( 12, 12, gi ? 14 : 24 ) );
	UTIL_SetOrigin( pev, pev->origin );
	pev->scale = gi ? 1.0f : info.scale;

	if ( !gi && ( info.r || info.g || info.b ) )
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
		// Player_Action_8084E3C4: the item get fanfare (NA_BGM_HEART_GET for a heart container)
		if ( info.major )
			ZL_MusicFanfare( m_iType == ZPICK_HEART_CONTAINER ? "fanfare_heart" : "fanfare_item", ZS_SY_GET_BOXITEM );
		else
			pLink->ZSound( CHAN_ITEM, ZS_SY_GET_ITEM );
		break;
	}

	if ( info.major && info.message )
	{
		// Link holds it over his head (Player_Action_8084E3C4) while the text box is up
		pLink->ZSetAction( ZACT_GETITEM );
		pLink->ZStartMessage( info.message, NULL, 0 );

		// Over his head in 3D when the model is there, otherwise the HUD shows the icon
		const char *gi = ZL_PickupGIModel( m_iType );
		if ( gi )
			CZeldaGetItemModel::Show( pLink, gi );

		MESSAGE_BEGIN( MSG_ONE, gmsgZItemGet, NULL, pLink->pev );
			WRITE_BYTE( m_iType );
			WRITE_BYTE( gi ? 1 : 0 );
		MESSAGE_END();
	}

	SUB_UseTargets( pLink, USE_TOGGLE, 0 );

	if ( info.major )
	{
		UTIL_Remove( this );
		return;
	}

	// EnItem00_Collected: it bobs over his head for 15 frames, then goes
	m_hCollector = pLink;
	m_iCollectTimer = 15;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	pev->velocity = g_vecZero;
	pev->effects &= ~EF_NODRAW;
	pev->owner = NULL;
	if ( m_iType == ZPICK_HEART )
	{
		pev->avelocity = g_vecZero;
		pev->angles.y = 0;
	}
	SetTouch( NULL );
	SetThink( &CZeldaPickup::CollectedThink );
	CollectedThink();
}

/*
 EnItem00_Collected: on the player, 40 + sin(despawnTimer * 15000) * despawnTimer * 0.3 above his
 feet (20 more for adult Link), rupees still turning 960 a frame, hearts facing front
*/
void CZeldaPickup::CollectedThink( void )
{
	CBaseEntity *pLink = m_hCollector;
	if ( !pLink || m_iCollectTimer <= 0 )
	{
		UTIL_Remove( this );
		return;
	}

	float bob = sin( m_iCollectTimer * 15000.0f * ( M_PI / 32768.0f ) ) * ( m_iCollectTimer * 0.3f );
	Vector pos = pLink->pev->origin;
	pos.z += pLink->pev->mins.z + ZL_DIST( 40.0f + 20.0f + bob ) - pev->mins.z;
	UTIL_SetOrigin( pev, pos );

	m_iCollectTimer--;
	pev->nextthink = gpGlobals->time + ZL_FRAMES( 1.0f );
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

 The map's angle is the way the chest's front (the latch) faces. Opening it is OoT's slow
 chest: Link steps up to the latch (Player_ActionHandler_2 puts him 29.43 units out, facing
 the chest), plays link_demo_Tbox_open while the lid's animation runs at 1.5x
 (EnBox_WaitOpen / EnBox_Open, NA_SE_EV_TBOX_UNLOCK on its frame 30 and NA_SE_EV_TBOX_OPEN on
 frame 90), the light pours out (Demo_Tre_Lgt), and then he holds the item up
 (link_demo_get_itemA). The client runs the matching camera (Camera_Demo3, ZCAM_CHEST).

 models/zelda/treasure_chest.mdl comes from your ROM (tools/zelda/make_object_models.py);
 without it the chest is a weapon box that pops open.
=============================================================================
*/
#define ZL_MDL_CHEST_OOT	"models/zelda/treasure_chest.mdl"
#define SF_CHEST_SMALL		1	// ENBOX_TYPE_SMALL: half size (actor scale 0.005)

class CZeldaChest : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT OpenThink( void );
	void GiveItem( CLinkPlayer *pLink );
	int ObjectCaps( void ) { return CBaseAnimating::ObjectCaps() | FCAP_IMPULSE_USE; }
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	string_t m_iszItem;
	EHANDLE m_hOpener;
	float m_flOpenStart;
	int m_iOpenStep;
	BOOL m_fOoTModel;
};

LINK_ENTITY_TO_CLASS( zl_chest, CZeldaChest );

TYPEDESCRIPTION CZeldaChest::m_SaveData[] =
{
	DEFINE_FIELD( CZeldaChest, m_iszItem, FIELD_STRING ),
	DEFINE_FIELD( CZeldaChest, m_hOpener, FIELD_EHANDLE ),
	DEFINE_FIELD( CZeldaChest, m_flOpenStart, FIELD_TIME ),
	DEFINE_FIELD( CZeldaChest, m_iOpenStep, FIELD_INTEGER ),
	DEFINE_FIELD( CZeldaChest, m_fOoTModel, FIELD_BOOLEAN ),
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

void CZeldaChest::Precache( void )
{
	if ( FStringNull( pev->model ) )
	{
		m_fOoTModel = ZL_FileExists( ZL_MDL_CHEST_OOT );
		pev->model = MAKE_STRING( m_fOoTModel ? ZL_MDL_CHEST_OOT : ZL_MDL_CHEST );
	}
	PRECACHE_MODEL( (char *)STRING( pev->model ) );
}

void CZeldaChest::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	float size = ( pev->spawnflags & SF_CHEST_SMALL ) ? 0.5f : 1.0f;
	if ( m_fOoTModel )
	{
		// OoT's model faces away from Link: its latch is on the back
		pev->angles.y = UTIL_AngleMod( pev->angles.y + 180.0f );
		pev->scale = size;
		pev->sequence = LookupSequence( "open" );
		pev->frame = 0;
		pev->framerate = 0;
		ResetSequenceInfo();
		pev->framerate = 0;
	}
	else if ( !pev->scale )
	{
		pev->scale = 1.6f * size;
	}

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;
	UTIL_SetSize( pev, Vector( -20, -20, 0 ) * size, Vector( 20, 20, 32 ) * size );
	UTIL_SetOrigin( pev, pev->origin );
	DROP_TO_FLOOR( ENT( pev ) );
}

void CZeldaChest::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->frame != 0 || m_flOpenStart > 0 )
		return;

	CLinkPlayer *pLink = ZL_GetLink( pActivator );
	if ( !pLink )
		return;

	if ( !m_fOoTModel )
	{
		// No ripped chest: pop the lid and hand the item straight over
		pev->frame = 1;
		pev->renderfx = kRenderFxGlowShell;
		pev->rendercolor = Vector( 255, 230, 120 );
		pev->renderamt = 2;
		ZL_EmitSound( edict(), CHAN_BODY, ZS_EV_CHEST_OPEN, 1, ATTN_NORM, PITCH_NORM );
		GiveItem( pLink );
		SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
		return;
	}

	// Link steps up to the latch, facing the chest
	float front = UTIL_AngleMod( pev->angles.y + 180.0f );
	Vector fwd;
	UTIL_MakeVectorsPrivate( Vector( 0, front, 0 ), fwd, NULL, NULL );
	// OoT's 29.43 units would put Half-Life's wider hull inside the chest's box
	float stand = max( ZL_CHEST_STAND_DIST, 20.0f * pev->scale + 17.0f );
	Vector spot = pev->origin + fwd * stand;
	spot.z = pLink->pev->origin.z;

	TraceResult tr;
	UTIL_TraceHull( pLink->pev->origin, spot, dont_ignore_monsters, human_hull, pLink->edict(), &tr );
	if ( !tr.fStartSolid )
		UTIL_SetOrigin( pLink->pev, tr.vecEndPos );

	pLink->pev->velocity = g_vecZero;
	pLink->pev->fuser1 = UTIL_AngleMod( front + 180.0f );	// Link's facing (predicted)
	pLink->ZSetAction( ZACT_CHEST );

	m_hOpener = pLink;
	m_flOpenStart = gpGlobals->time;
	m_iOpenStep = 0;

	// EnBox_WaitOpen: Animation_Change( anim, 1.5f, ... ) the frame after Link starts
	pev->sequence = LookupSequence( "open" );
	pev->frame = 0;
	ResetSequenceInfo();
	pev->framerate = 1.5f;

	SetThink( &CZeldaChest::OpenThink );
	pev->nextthink = gpGlobals->time + 0.05f;

	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
}

void CZeldaChest::OpenThink( void )
{
	pev->nextthink = gpGlobals->time + 0.05f;
	StudioFrameAdvance();
	if ( m_fSequenceFinished )
	{
		pev->frame = 255;
		pev->framerate = 0;
	}

	float t = gpGlobals->time - m_flOpenStart;
	CLinkPlayer *pLink = ZL_GetLink( m_hOpener );

	if ( m_iOpenStep == 0 && t >= ZL_CHEST_UNLOCK_TIME )
	{
		ZL_EmitSound( edict(), CHAN_BODY, ZS_EV_CHEST_UNLOCK, 1, ATTN_NORM, PITCH_NORM );
		m_iOpenStep = 1;
	}
	else if ( m_iOpenStep == 1 && t >= ZL_CHEST_OPEN_TIME )
	{
		ZL_EmitSound( edict(), CHAN_ITEM, ZS_EV_CHEST_OPEN, 1, ATTN_NORM, PITCH_NORM );
		// EnBox_Open: NA_BGM_OPEN_TRE_BOX, except for the small chests
		if ( !( pev->spawnflags & SF_CHEST_SMALL ) )
			ZL_MusicFanfare( "fanfare_chest", -1 );

		// Demo_Tre_Lgt: the light pouring out of the chest
		Vector light = pev->origin + Vector( 0, 0, 24 * pev->scale );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, light );
			WRITE_BYTE( TE_DLIGHT );
			WRITE_COORD( light.x );
			WRITE_COORD( light.y );
			WRITE_COORD( light.z );
			WRITE_BYTE( 18 );		// radius in 10s
			WRITE_BYTE( 255 );
			WRITE_BYTE( 230 );
			WRITE_BYTE( 140 );
			WRITE_BYTE( 60 );		// life in 10ths
			WRITE_BYTE( 0 );		// decay
		MESSAGE_END();
		m_iOpenStep = 2;
	}

	// Link finished link_demo_Tbox_open: the item comes out
	if ( m_iOpenStep == 2 && t >= ZL_CHEST_TIME )
	{
		m_iOpenStep = 3;
		if ( pLink && pLink->ZAction() == ZACT_CHEST )
			GiveItem( pLink );
	}

	if ( m_iOpenStep == 3 && m_fSequenceFinished )
	{
		SetThink( NULL );
		pev->nextthink = 0;
	}

	// Link was knocked out of it (died, level change...): the chest stays open regardless
	if ( m_iOpenStep < 3 && ( !pLink || !pLink->IsAlive() ) )
		m_iOpenStep = 2;
}

void CZeldaChest::GiveItem( CLinkPlayer *pLink )
{
	const char *cls = FStringNull( m_iszItem ) ? "item_zl_rupee_blue" : STRING( m_iszItem );
	CBaseEntity *pItem = CBaseEntity::Create( (char *)cls, pLink->pev->origin, g_vecZero, NULL );
	if ( pItem )
		pItem->Touch( pLink );

	if ( pLink->ZAction() == ZACT_GETITEM )
		pLink->pev->iuser4 = ( pLink->pev->iuser4 & ~ZL_STATE_SUB_MASK ) | ( 1 << ZL_STATE_SUB_SHIFT );	// get_itemA
	else if ( pLink->ZAction() == ZACT_CHEST )
		pLink->ZSetAction( ZACT_STAND );		// small things (rupees) have no text box
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
