/***
*
*	Zelda 64 port: grass and bushes to cut (En_Kusa), and OoT's random drops.
*
*	zl_grass
*		type		0 = bush (gFieldBushDL): solid, leaves its stump when cut, drops from table 0
*					1 = tuft of grass (object_kusa): walk through it, gone when cut, drops a
*						recovery heart or (adult Link's stand-in for seeds) arrows half the time
*
*	Anything that hurts it cuts it: the sword, a spin, a bomb, an arrow. Models from
*	tools/zelda/make_object_models.py (bush.mdl, bush_stump.mdl, grass.mdl); without them it's
*	still there to cut, just not drawn.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "zl_player.h"

#define ZL_MDL_BUSH			"models/zelda/bush.mdl"
#define ZL_MDL_BUSH_STUMP	"models/zelda/bush_stump.mdl"
#define ZL_MDL_GRASS		"models/zelda/grass.mdl"

/*
=============================================================================
 Drops (Item_DropCollectibleRandom, func_8001F404)
=============================================================================
*/
enum
{
	ZDROP_NONE,
	ZDROP_RUPEE_GREEN,
	ZDROP_RUPEE_BLUE,
	ZDROP_RUPEE_RED,
	ZDROP_MAGIC_SMALL,
	ZDROP_MAGIC_LARGE,
	ZDROP_HEART,
	ZDROP_SEEDS,
	ZDROP_ARROWS,
	ZDROP_BOMBS,
	ZDROP_FLEXIBLE,
};

// sItemDropIds, COLLECTIBLE_DROP_TABLE_0
static const int sDropTable0[16] =
{
	ZDROP_RUPEE_GREEN, ZDROP_RUPEE_BLUE, ZDROP_NONE, ZDROP_NONE,
	ZDROP_RUPEE_BLUE, ZDROP_RUPEE_GREEN, ZDROP_MAGIC_SMALL, ZDROP_HEART,
	ZDROP_HEART, ZDROP_NONE, ZDROP_MAGIC_SMALL, ZDROP_FLEXIBLE,
	ZDROP_SEEDS, ZDROP_SEEDS, ZDROP_NONE, ZDROP_RUPEE_BLUE,
};

// Spawn one (Item_DropCollectible), after func_8001F404 has checked Link can use it
static void ZL_DropOne( CLinkPlayer *pLink, int drop, const Vector &pos )
{
	// Adult Link: seeds become arrows
	if ( drop == ZDROP_SEEDS )
		drop = ZDROP_ARROWS;

	if ( ( drop == ZDROP_BOMBS && !( pLink->m_ZInv.items & ( 1 << ZITEM_BOMB ) ) ) ||
		 ( drop == ZDROP_ARROWS && !( pLink->m_ZInv.items & ( 1 << ZITEM_BOW ) ) ) ||
		 ( ( drop == ZDROP_MAGIC_SMALL || drop == ZDROP_MAGIC_LARGE ) && pLink->m_iZMagicCapacity <= 0 ) )
		return;

	if ( drop == ZDROP_HEART && pLink->pev->health >= pLink->m_iZHealthCapacity )
		drop = ZDROP_RUPEE_GREEN;

	const char *cls = NULL;
	switch ( drop )
	{
	case ZDROP_RUPEE_GREEN:	cls = "item_zl_rupee"; break;
	case ZDROP_RUPEE_BLUE:	cls = "item_zl_rupee_blue"; break;
	case ZDROP_RUPEE_RED:	cls = "item_zl_rupee_red"; break;
	case ZDROP_MAGIC_SMALL:	cls = "item_zl_magic"; break;
	case ZDROP_MAGIC_LARGE:	cls = "item_zl_magic_large"; break;
	case ZDROP_HEART:		cls = "item_zl_heart"; break;
	case ZDROP_ARROWS:		cls = "item_zl_arrows"; break;
	case ZDROP_BOMBS:		cls = "item_zl_bombs"; break;
	}
	if ( !cls )
		return;

	// Owned by the world, so it blinks out after a while like a dropped EnItem00
	CBaseEntity *pItem = CBaseEntity::Create( (char *)cls, pos + Vector( 0, 0, 8 ), g_vecZero, ENT( 0 ) );
	if ( pItem )
		pItem->pev->velocity = Vector( RANDOM_FLOAT( -30, 30 ), RANDOM_FLOAT( -30, 30 ), ZL_VEL( 8.0f ) * 0.6f );
}

// Item_DropCollectibleRandom( COLLECTIBLE_DROP_TABLE_0 ): one of 16, ITEM00_FLEXIBLE being what
// Link is short of
void ZL_DropCollectibleRandom( CLinkPlayer *pLink, const Vector &pos )
{
	if ( !pLink )
		return;

	int drop = sDropTable0[RANDOM_LONG( 0, 15 )];

	if ( drop == ZDROP_FLEXIBLE )
	{
		float health = pLink->pev->health;
		if ( health <= 0x50 )		// 5 hearts or less (a fairy at one heart in OoT; there are none)
			drop = ZDROP_HEART;
		else if ( pLink->m_iZMagicCapacity > 0 && pLink->m_iZMagic == 0 )
			drop = ZDROP_MAGIC_LARGE;
		else if ( pLink->m_iZMagicCapacity > 0 && pLink->m_iZMagic <= pLink->m_iZMagicCapacity / 2 )
			drop = ZDROP_MAGIC_SMALL;
		else if ( ( pLink->m_ZInv.items & ( 1 << ZITEM_BOW ) ) && pLink->m_ZInv.ammo[ZITEM_BOW] < 6 )
			drop = ZDROP_ARROWS;
		else if ( ( pLink->m_ZInv.items & ( 1 << ZITEM_BOMB ) ) && pLink->m_ZInv.ammo[ZITEM_BOMB] < 6 )
			drop = ZDROP_BOMBS;
		else if ( pLink->m_iZRupees < 11 )
			drop = ZDROP_RUPEE_RED;
		else
			return;
	}

	ZL_DropOne( pLink, drop, pos );
}

/*
=============================================================================
 zl_grass (En_Kusa)
=============================================================================
*/
class CZeldaGrass : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Cut( CBaseEntity *pAttacker );
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iType;
	BOOL m_fCut;
};

LINK_ENTITY_TO_CLASS( zl_grass, CZeldaGrass );

TYPEDESCRIPTION CZeldaGrass::m_SaveData[] =
{
	DEFINE_FIELD( CZeldaGrass, m_iType, FIELD_INTEGER ),
	DEFINE_FIELD( CZeldaGrass, m_fCut, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CZeldaGrass, CBaseEntity );

void CZeldaGrass::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "type" ) )
	{
		m_iType = atoi( pkvd->szValue ) ? 1 : 0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CZeldaGrass::Precache( void )
{
	if ( ZL_FileExists( ZL_MDL_BUSH ) )
		PRECACHE_MODEL( (char *)ZL_MDL_BUSH );
	if ( ZL_FileExists( ZL_MDL_BUSH_STUMP ) )
		PRECACHE_MODEL( (char *)ZL_MDL_BUSH_STUMP );
	if ( ZL_FileExists( ZL_MDL_GRASS ) )
		PRECACHE_MODEL( (char *)ZL_MDL_GRASS );
}

void CZeldaGrass::Spawn( void )
{
	Precache();

	const char *mdl = m_iType ? ZL_MDL_GRASS : ZL_MDL_BUSH;
	if ( ZL_FileExists( mdl ) )
		SET_MODEL( ENT( pev ), mdl );

	pev->movetype = MOVETYPE_NONE;
	// sCylinderInit: radius 12, height 44. The bush stands in the way (OC1_TYPE_PLAYER); the
	// grass is walked through
	pev->solid = m_iType ? SOLID_TRIGGER : SOLID_BBOX;
	UTIL_SetSize( pev, Vector( -ZL_DIST( 12 ), -ZL_DIST( 12 ), 0 ), Vector( ZL_DIST( 12 ), ZL_DIST( 12 ), ZL_DIST( 44 ) ) );
	UTIL_SetOrigin( pev, pev->origin );
	DROP_TO_FLOOR( ENT( pev ) );

	pev->takedamage = DAMAGE_YES;
	pev->health = 1;
	m_fCut = FALSE;
}

int CZeldaGrass::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( m_fCut )
		return 0;
	Cut( CBaseEntity::Instance( pevAttacker ) );
	return 1;
}

/*
 EnKusa's AC_HIT: NA_SE_EV_PLANT_BROKEN, the bits fly (EnKusa_SpawnFragments), the drop
 (EnKusa_DropCollectible), and the bush leaves its stump (object_kusa_DL_0002E0)
*/
void CZeldaGrass::Cut( CBaseEntity *pAttacker )
{
	m_fCut = TRUE;
	pev->takedamage = DAMAGE_NO;

	ZL_EmitSound( edict(), CHAN_BODY, ZS_EV_PLANT_BROKEN, 1.0f, ATTN_NORM, PITCH_NORM );

	if ( pev->modelindex )
	{
		Vector mid = pev->origin + Vector( 0, 0, ZL_DIST( 20 ) );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, mid );
			WRITE_BYTE( TE_BREAKMODEL );
			WRITE_COORD( mid.x );
			WRITE_COORD( mid.y );
			WRITE_COORD( mid.z );
			WRITE_COORD( 16 );		// size
			WRITE_COORD( 16 );
			WRITE_COORD( 16 );
			WRITE_COORD( 0 );		// velocity
			WRITE_COORD( 0 );
			WRITE_COORD( 120 );
			WRITE_BYTE( 10 );		// random velocity
			WRITE_SHORT( pev->modelindex );
			WRITE_BYTE( 6 );		// count
			WRITE_BYTE( 8 );		// life, 0.1 s
			WRITE_BYTE( 0 );		// flags
		MESSAGE_END();
	}

	CLinkPlayer *pLink = ZL_GetLink( pAttacker );
	if ( !pLink )
		pLink = ZL_GetLink( UTIL_PlayerByIndex( 1 ) );

	if ( m_iType == 0 )
	{
		ZL_DropCollectibleRandom( pLink, pev->origin );
		pev->solid = SOLID_NOT;
		if ( ZL_FileExists( ZL_MDL_BUSH_STUMP ) )
			SET_MODEL( ENT( pev ), ZL_MDL_BUSH_STUMP );
		else
			pev->effects |= EF_NODRAW;
		UTIL_SetSize( pev, Vector( -ZL_DIST( 12 ), -ZL_DIST( 12 ), 0 ), Vector( ZL_DIST( 12 ), ZL_DIST( 12 ), ZL_DIST( 8 ) ) );
	}
	else
	{
		// ENKUSA_TYPE_1: seeds or a recovery heart, one or the other
		if ( pLink )
			ZL_DropOne( pLink, RANDOM_LONG( 0, 1 ) ? ZDROP_SEEDS : ZDROP_HEART, pev->origin );
		pev->solid = SOLID_NOT;
		pev->effects |= EF_NODRAW;
	}
	UTIL_SetOrigin( pev, pev->origin );
}
