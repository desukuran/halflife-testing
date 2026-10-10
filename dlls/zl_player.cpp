/***
*
*	Zelda 64 port: Link, the player class. See zl_player.h.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "talkmonster.h"
#include "zl_player.h"

extern BOOL g_fGameOver;

#include <math.h>

LINK_ENTITY_TO_CLASS( player, CLinkPlayer );

cvar_t zl_enable		= { "zl_enable", "1", FCVAR_SERVER };
cvar_t zl_dmg_in_scale	= { "zl_dmg_in_scale", "0.8", FCVAR_SERVER };	// HL damage -> OoT health units
cvar_t zl_dmg_out_scale	= { "zl_dmg_out_scale", "10", FCVAR_SERVER };	// OoT damage -> HL damage
cvar_t zl_ztoggle		= { "zl_ztoggle", "0", FCVAR_ARCHIVE };		// OoT options menu: Z Target "Switch" instead of "Hold"
cvar_t zl_model			= { "zl_model", "models/player.mdl", FCVAR_ARCHIVE };	// models/player/link/link.mdl once built
cvar_t zl_sfx_oot		= { "zl_sfx_oot", "0", FCVAR_ARCHIVE };		// use ripped OoT sounds from sound/zelda/oot/
cvar_t zl_hl_suit		= { "zl_hl_suit", "0", FCVAR_SERVER };		// also give the HEV suit (HL HUD + voice)
cvar_t zl_drops			= { "zl_drops", "1", FCVAR_SERVER };		// monsters drop rupees / hearts
cvar_t zl_hookshot_any	= { "zl_hookshot_any", "1", FCVAR_SERVER };	// hookshot sticks to anything, not just wood
cvar_t zl_start_items	= { "zl_start_items", "1", FCVAR_SERVER };	// 0 nothing, 1 sword + shield, 2 everything

BOOL ZL_Enabled( void )
{
	return zl_enable.value != 0;
}

void ZL_RegisterCvars( void )
{
	CVAR_REGISTER( &zl_enable );
	CVAR_REGISTER( &zl_dmg_in_scale );
	CVAR_REGISTER( &zl_dmg_out_scale );
	CVAR_REGISTER( &zl_ztoggle );
	CVAR_REGISTER( &zl_model );
	CVAR_REGISTER( &zl_sfx_oot );
	CVAR_REGISTER( &zl_hl_suit );
	CVAR_REGISTER( &zl_drops );
	CVAR_REGISTER( &zl_hookshot_any );
	CVAR_REGISTER( &zl_start_items );
}

CLinkPlayer *ZL_GetLink( CBaseEntity *pEnt )
{
	if ( !pEnt || !pEnt->IsPlayer() )
		return NULL;
	return static_cast<CLinkPlayer *>( pEnt );
}

TYPEDESCRIPTION CLinkPlayer::m_SaveData[] =
{
	DEFINE_FIELD( CLinkPlayer, m_iZHealthCapacity, FIELD_INTEGER ),
	DEFINE_FIELD( CLinkPlayer, m_iZMagic, FIELD_INTEGER ),
	DEFINE_FIELD( CLinkPlayer, m_iZMagicCapacity, FIELD_INTEGER ),
	DEFINE_FIELD( CLinkPlayer, m_iZRupees, FIELD_INTEGER ),
	DEFINE_FIELD( CLinkPlayer, m_iZWallet, FIELD_INTEGER ),
	DEFINE_FIELD( CLinkPlayer, m_fZDoubleDefense, FIELD_BOOLEAN ),
	DEFINE_FIELD( CLinkPlayer, m_fZInitialized, FIELD_BOOLEAN ),
	DEFINE_FIELD( CLinkPlayer, m_ZInv.items, FIELD_INTEGER ),
	DEFINE_FIELD( CLinkPlayer, m_ZInv.equipment, FIELD_INTEGER ),
	DEFINE_ARRAY( CLinkPlayer, m_ZInv.ammo, FIELD_INTEGER, ZITEM_COUNT ),
	DEFINE_ARRAY( CLinkPlayer, m_ZInv.cButton, FIELD_INTEGER, 3 ),
	DEFINE_ARRAY( CLinkPlayer, m_ZInv.cWeapon, FIELD_INTEGER, 3 ),
	DEFINE_FIELD( CLinkPlayer, m_ZInv.quiver, FIELD_INTEGER ),
	DEFINE_FIELD( CLinkPlayer, m_ZInv.bombBag, FIELD_INTEGER ),
	DEFINE_FIELD( CLinkPlayer, m_ZInv.smallKeys, FIELD_INTEGER ),
};

int CLinkPlayer::Save( CSave &save )
{
	if ( !CBasePlayer::Save( save ) )
		return 0;
	return save.WriteFields( "LINK", this, m_SaveData, ARRAYSIZE( m_SaveData ) );
}

int CLinkPlayer::Restore( CRestore &restore )
{
	if ( !CBasePlayer::Restore( restore ) )
		return 0;

	int status = restore.ReadFields( "LINK", this, m_SaveData, ARRAYSIZE( m_SaveData ) );

	// Force the HUD to resend everything
	memset( m_iZStatsSent, -1, sizeof( m_iZStatsSent ) );
	memset( m_iZButtonsSent, -1, sizeof( m_iZButtonsSent ) );
	m_iZTargetSent = m_iZHoverSent = m_iZTargetFlagsSent = -1;
	m_hZTalkTo = NULL;
	m_hZHeldBomb = NULL;
	m_hZHookshot = NULL;
	m_iZHeldButton = -1;

	return status;
}

BOOL CLinkPlayer::LinkActive( void )
{
	return ZL_Enabled() && !IsObserver();
}

void CLinkPlayer::Precache( void )
{
	CBasePlayer::Precache();
}

/*
=============================================================================
 playerData
=============================================================================
*/
void CLinkPlayer::ZResetPlayerData( void )
{
	m_fZInitialized = TRUE;

	m_iZHealthCapacity = ZL_START_HEARTS * ZL_HEART;
	m_iZMagic = 0;
	m_iZMagicCapacity = 0;
	m_iZRupees = 0;
	m_iZWallet = ZL_WALLET_CHILD;
	m_fZDoubleDefense = FALSE;

	memset( &m_ZInv, 0, sizeof( m_ZInv ) );
	m_ZInv.quiver = 30;
	m_ZInv.bombBag = 20;

	if ( zl_start_items.value >= 1 )
		m_ZInv.equipment = ZEQ_SWORD_KOKIRI | ZEQ_SHIELD_DEKU;

	if ( zl_start_items.value >= 2 )
	{
		m_ZInv.equipment |= ZEQ_SWORD_MASTER | ZEQ_SHIELD_HYLIAN;
		m_iZHealthCapacity = 8 * ZL_HEART;
		m_iZMagicCapacity = ZL_MAGIC_SINGLE;
		m_iZMagic = ZL_MAGIC_SINGLE;
		m_iZWallet = ZL_WALLET_ADULT;
		m_iZRupees = 50;
		ZGiveItem( ZITEM_BOW );
		ZGiveItem( ZITEM_BOMB );
		ZGiveItem( ZITEM_HOOKSHOT );
		ZGiveItem( ZITEM_DEKU_NUT );
	}
}

void CLinkPlayer::ZAddRupees( int amount )
{
	m_iZRupees = max( 0, min( m_iZWallet, m_iZRupees + amount ) );
}

BOOL CLinkPlayer::ZUseMagic( int amount )
{
	if ( m_iZMagic < amount )
		return FALSE;
	m_iZMagic -= amount;
	return TRUE;
}

void CLinkPlayer::ZAddMagic( int amount )
{
	m_iZMagic = max( 0, min( m_iZMagicCapacity, m_iZMagic + amount ) );
}

void CLinkPlayer::ZAddHeartContainer( void )
{
	if ( m_iZHealthCapacity < ZL_MAX_HEARTS * ZL_HEART )
		m_iZHealthCapacity += ZL_HEART;
	pev->max_health = m_iZHealthCapacity;
	pev->health = m_iZHealthCapacity;
}

void CLinkPlayer::ZGiveEquipment( int bits )
{
	m_ZInv.equipment |= bits;
}

void CLinkPlayer::ZGiveItem( int item )
{
	if ( item <= ZITEM_NONE || item >= ZITEM_COUNT )
		return;

	BOOL isNew = !( m_ZInv.items & ( 1 << item ) );
	m_ZInv.items |= ( 1 << item );

	switch ( item )
	{
	case ZITEM_BOW:			m_ZInv.ammo[item] = max( m_ZInv.ammo[item], m_ZInv.quiver ); break;
	case ZITEM_BOMB:		m_ZInv.ammo[item] = max( m_ZInv.ammo[item], m_ZInv.bombBag ); break;
	case ZITEM_DEKU_NUT:	m_ZInv.ammo[item] = max( m_ZInv.ammo[item], 20 ); break;
	case ZITEM_DEKU_STICK:	m_ZInv.ammo[item] = max( m_ZInv.ammo[item], 10 ); break;
	case ZITEM_LONGSHOT:	m_ZInv.items |= ( 1 << ZITEM_HOOKSHOT ); break;
	}

	// Put new items on an empty C button, like picking one up and assigning it in the subscreen
	if ( isNew )
	{
		for ( int i = 0; i < 3; i++ )
		{
			if ( m_ZInv.cButton[i] == item )
				return;
		}
		for ( int i = 0; i < 3; i++ )
		{
			if ( m_ZInv.cButton[i] == ZITEM_NONE )
			{
				m_ZInv.cButton[i] = item;
				return;
			}
		}
	}
}

// D_80854488: dmgFlags for each sword; values from the default actor damage table
int CLinkPlayer::ZSwordDamage( int mwa )
{
	int sword = 1;	// Kokiri

	if ( m_ZInv.equipment & ZEQ_SWORD_BIGGORON )
		sword = 4;
	else if ( m_ZInv.equipment & ZEQ_SWORD_MASTER )
		sword = 2;

	switch ( mwa )
	{
	case ZMWA_JUMPSLASH_START:
	case ZMWA_JUMPSLASH_FINISH:
	case ZMWA_FLIPSLASH_START:
	case ZMWA_FLIPSLASH_FINISH:
		return sword * 2;				// DMG_JUMP_*
	case ZMWA_SPIN_ATTACK_1H:
	case ZMWA_SPIN_ATTACK_2H:
		return sword * 2;				// DMG_SPIN_*
	case ZMWA_BIG_SPIN_1H:
	case ZMWA_BIG_SPIN_2H:
		return sword * 4;
	default:
		return sword;					// DMG_SLASH_*
	}
}

void CLinkPlayer::ZSetAction( int action )
{
	pev->iuser4 = ( pev->iuser4 & ~( ZL_STATE_ACTION_MASK | ZL_STATE_SUB_MASK | ZL_STATE_CHARGED ) ) | action;
	pev->fuser4 = 0;
}

Vector CLinkPlayer::ZForward( void )
{
	float r = ZFacing() * ( M_PI / 180.0 );
	return Vector( cos( r ), sin( r ), 0 );
}

/*
=============================================================================
 Spawn / think
=============================================================================
*/
void CLinkPlayer::Spawn( void )
{
	CBasePlayer::Spawn();

	m_iZHeldButton = -1;
	m_hZHeldBomb = NULL;
	m_hZHookshot = NULL;
	m_hZTalkTo = NULL;
	m_hZTarget = NULL;
	m_hZHover = NULL;
	m_flZInvincibleUntil = 0;
	m_iZPad = m_iZPadLast = 0;
	memset( m_iZStatsSent, -1, sizeof( m_iZStatsSent ) );
	memset( m_iZButtonsSent, -1, sizeof( m_iZButtonsSent ) );
	m_iZTargetSent = m_iZHoverSent = m_iZTargetFlagsSent = -1;
	m_flZFallStartZ = pev->origin.z;
	m_iZAnimState = -1;

	if ( !ZL_Enabled() )
	{
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "zl", "0" );
		pev->iuser3 = 0;
		pev->iuser4 = 0;
		return;
	}

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "zl", "1" );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "zs", zl_sfx_oot.value ? "1" : "0" );

	if ( zl_model.string && zl_model.string[0] && strcmp( zl_model.string, "models/player.mdl" ) )
	{
		SET_MODEL( ENT( pev ), zl_model.string );
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
	}

	if ( !m_fZInitialized )
		ZResetPlayerData();

	pev->max_health = m_iZHealthCapacity;
	pev->health = m_iZHealthCapacity;

	if ( zl_hl_suit.value )
		pev->weapons |= ( 1 << WEAPON_SUIT );

	pev->iuser4 = 0;
	pev->fuser1 = pev->angles.y;
	pev->fuser4 = 0;

	ZUpdateServerFlags();
}

static int ZL_PadFromButtons( int buttons )
{
	int pad = 0;
	if ( buttons & ZBTN_A )			pad |= ZPAD_A;
	if ( buttons & ZBTN_B )			pad |= ZPAD_B;
	if ( buttons & ZBTN_Z )			pad |= ZPAD_Z;
	if ( buttons & ZBTN_R )			pad |= ZPAD_R;
	if ( buttons & ZBTN_CLEFT )		pad |= ZPAD_CLEFT;
	if ( buttons & ZBTN_CDOWN )		pad |= ZPAD_CDOWN;
	if ( buttons & ZBTN_CRIGHT )	pad |= ZPAD_CRIGHT;
	if ( buttons & ZBTN_CUP )		pad |= ZPAD_CUP;
	return pad;
}

// Strip the N64 buttons out of what HL's player / weapon code sees. A C button with a
// Half-Life weapon on it becomes +attack for that weapon.
int CLinkPlayer::ZTranslateHLButtons( int raw )
{
	static const int cBits[3] = { ZBTN_CLEFT, ZBTN_CDOWN, ZBTN_CRIGHT };
	int out = raw & ~( IN_ATTACK | IN_ATTACK2 | IN_RELOAD | IN_RUN | IN_JUMP | IN_DUCK | IN_ALT1 | IN_SCORE );

	for ( int i = 0; i < 3; i++ )
	{
		if ( m_ZInv.cButton[i] == ZITEM_HLWEAPON && ( raw & cBits[i] ) )
			out |= IN_ATTACK;
	}
	return out;
}

void CLinkPlayer::ZUpdateServerFlags( void )
{
	int f = ZSF_ACTIVE;

	if ( m_ZInv.equipment & ( ZEQ_SWORD_KOKIRI | ZEQ_SWORD_MASTER | ZEQ_SWORD_BIGGORON ) )
		f |= ZSF_HASSWORD;
	if ( m_ZInv.equipment & ( ZEQ_SHIELD_DEKU | ZEQ_SHIELD_HYLIAN | ZEQ_SHIELD_MIRROR ) )
		f |= ZSF_HASSHIELD;
	if ( m_iZMagic >= 12 )
		f |= ZSF_MAGIC;

	if ( m_iZDoAction != ZDO_NONE && m_iZDoAction != ZDO_ATTACK && m_iZDoAction != ZDO_THROW )
		f |= ZSF_DOACTION;
	if ( m_hZHeldBomb != NULL )
		f |= ZSF_HOLDING;
	if ( m_flZInvincibleUntil > gpGlobals->time )
		f |= ZSF_INVULN;

	if ( ZItemAimable( m_ZInv.cButton[0] ) ) f |= ZSF_AIM_CLEFT;
	if ( ZItemAimable( m_ZInv.cButton[1] ) ) f |= ZSF_AIM_CDOWN;
	if ( ZItemAimable( m_ZInv.cButton[2] ) ) f |= ZSF_AIM_CRIGHT;

	if ( m_hZTalkTo != NULL || ZAction() == ZACT_TALK || ZAction() == ZACT_GETITEM || ( pev->flags & FL_FROZEN ) )
		f |= ZSF_FROZEN;

	if ( m_hZHookshot != NULL && ZAction() != ZACT_HOOKFLY )
		f |= ZSF_HOOKSHOT;

	pev->iuser3 = f;
}

void CLinkPlayer::PreThink( void )
{
	if ( !LinkActive() )
	{
		pev->iuser3 = 0;
		CBasePlayer::PreThink();
		return;
	}

	int raw = pev->button;
	m_iZPadLast = m_iZPad;
	m_iZPad = ZL_PadFromButtons( raw );

	pev->button = ZTranslateHLButtons( raw );

	CBasePlayer::PreThink();

	if ( IsAlive() && !g_fGameOver )
	{
		ZUpdateTargeting();
		ZUpdateDoAction();

		if ( ZPressed( ZPAD_A ) )
			ZDoActionPressed();
	}

	ZUpdateServerFlags();
}

void CLinkPlayer::PostThink( void )
{
	if ( !LinkActive() )
	{
		CBasePlayer::PostThink();
		return;
	}

	if ( IsAlive() )
		ZFallDamageThink();

	CBasePlayer::PostThink();

	if ( !IsAlive() || g_fGameOver )
		return;

	ZSwordThink();
	ZItemThink();

	// Link's body faces his shape yaw, not the camera
	pev->angles = Vector( 0, ZFacing(), 0 );

	// Talking: stay frozen until the client closes the box. Safety net for lost commands.
	if ( m_hZTalkTo == NULL && ZAction() == ZACT_TALK && gpGlobals->time - m_flZTalkStart > 60.0f )
		ZSetAction( ZACT_STAND );
}

void CLinkPlayer::Jump( void )
{
	if ( LinkActive() )
		return;		// A is not a jump button in Hyrule
	CBasePlayer::Jump();
}

void CLinkPlayer::Duck( void )
{
	if ( LinkActive() )
		return;		// R is the shield
	CBasePlayer::Duck();
}

void CLinkPlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	if ( !LinkActive() )
	{
		CBasePlayer::SetAnimation( playerAnim );
		return;
	}

	if ( playerAnim == PLAYER_DIE )
	{
		CBasePlayer::SetAnimation( playerAnim );
		return;
	}

	ZAnimate();
}

void CLinkPlayer::ImpulseCommands( void )
{
	CBasePlayer::ImpulseCommands();
}

int CLinkPlayer::TakeHealth( float flHealth, int bitsDamageType )
{
	if ( !LinkActive() )
		return CBasePlayer::TakeHealth( flHealth, bitsDamageType );

	if ( pev->takedamage == DAMAGE_NO || pev->health >= pev->max_health )
		return 0;

	// Hearts move in quarters on the meter; HL's 15 point medkit becomes one recovery heart
	int units = (int)flHealth;
	if ( units >= 4 )
		units = ( ( units + 2 ) / 4 ) * 4;
	if ( units >= 12 && units < ZL_HEART )
		units = ZL_HEART;

	int before = (int)pev->health;
	pev->health = min( pev->max_health, pev->health + units );

	if ( units >= 4 )
		ZSound( CHAN_ITEM, ZS_SY_HP_RECOVER, 0.8f );

	return (int)pev->health - before;
}

void CLinkPlayer::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( LinkActive() )
	{
		ZReleaseTarget( FALSE );
		if ( m_hZHeldBomb != NULL )
			ZThrowBomb( TRUE );
		m_hZTalkTo = NULL;

		ZSetAction( ZACT_DEAD );
		ZSound( CHAN_VOICE, ZS_VO_DEATH );

		MESSAGE_BEGIN( MSG_ONE, gmsgZText, NULL, pev );
			WRITE_BYTE( ZTEXT_CLOSE );
			WRITE_STRING( "" );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	CBasePlayer::Killed( pevAttacker, iGib );
}

/*
=============================================================================
 HUD
=============================================================================
*/
void CLinkPlayer::UpdateClientData( void )
{
	BOOL force = m_fInitHUD;

	CBasePlayer::UpdateClientData();

	if ( !LinkActive() )
		return;

	ZSendStats( force );
	ZSendButtons( force );

	if ( force )
	{
		m_iZTargetSent = m_iZHoverSent = m_iZTargetFlagsSent = -1;
		ZL_MusicResend( edict() );
	}
}

void CLinkPlayer::ZSendStats( BOOL force )
{
	int stats[8];

	stats[0] = max( 0, (int)pev->health );
	stats[1] = m_iZHealthCapacity;
	stats[2] = m_iZMagic;
	stats[3] = m_iZMagicCapacity;
	stats[4] = m_iZRupees;
	stats[5] = m_iZWallet;
	stats[6] = m_ZInv.smallKeys;
	stats[7] = ( m_fZDoubleDefense ? 1 : 0 ) | ( ( m_ZInv.equipment & 0xFF ) << 1 );

	if ( !force && !memcmp( stats, m_iZStatsSent, sizeof( stats ) ) )
		return;

	memcpy( m_iZStatsSent, stats, sizeof( stats ) );

	MESSAGE_BEGIN( MSG_ONE, gmsgZStats, NULL, pev );
		WRITE_SHORT( stats[0] );
		WRITE_SHORT( stats[1] );
		WRITE_BYTE( stats[2] );
		WRITE_BYTE( stats[3] );
		WRITE_SHORT( stats[4] );
		WRITE_SHORT( stats[5] );
		WRITE_BYTE( stats[6] );
		WRITE_SHORT( stats[7] );
	MESSAGE_END();
}

void CLinkPlayer::ZSendButtons( BOOL force )
{
	int b[10];

	// B: which sword
	b[0] = ( m_ZInv.equipment & ZEQ_SWORD_BIGGORON ) ? 3 : ( m_ZInv.equipment & ZEQ_SWORD_MASTER ) ? 2 : ( m_ZInv.equipment & ZEQ_SWORD_KOKIRI ) ? 1 : 0;

	for ( int i = 0; i < 3; i++ )
	{
		b[1 + i * 2] = m_ZInv.cButton[i] | ( m_ZInv.cButton[i] == ZITEM_HLWEAPON ? ( m_ZInv.cWeapon[i] << 8 ) : 0 );
		b[2 + i * 2] = ZItemAmmo( m_ZInv.cButton[i] );
	}

	b[7] = m_iZDoAction;
	b[8] = m_hZHeldBomb != NULL ? 1 : 0;
	b[9] = 0;

	if ( !force && !memcmp( b, m_iZButtonsSent, sizeof( b ) ) )
		return;

	memcpy( m_iZButtonsSent, b, sizeof( b ) );

	MESSAGE_BEGIN( MSG_ONE, gmsgZButtons, NULL, pev );
		WRITE_BYTE( b[0] );
		for ( int i = 0; i < 3; i++ )
		{
			WRITE_SHORT( b[1 + i * 2] );
			WRITE_SHORT( b[2 + i * 2] );
		}
		WRITE_BYTE( b[7] );
		WRITE_BYTE( b[8] );
	MESSAGE_END();
}

/*
=============================================================================
 A button: do-action (interface.h DO_ACTION_*)
=============================================================================
*/
static BOOL ZL_IsTalker( CBaseEntity *pEnt )
{
	CBaseMonster *pMon = pEnt->MyMonsterPointer();
	if ( !pMon || !pMon->IsAlive() || pEnt->IsPlayer() )
		return FALSE;

	// Anything with text set by the mapper talks
	if ( !FStringNull( pEnt->pev->message ) && ( pEnt->pev->flags & FL_MONSTER ) )
		return TRUE;

	if ( !FClassnameIs( pEnt->pev, "monster_scientist" ) && !FClassnameIs( pEnt->pev, "monster_barney" ) &&
		 !FClassnameIs( pEnt->pev, "monster_sitting_scientist" ) )
		return FALSE;

	// Scared or angry NPCs won't chat
	return pMon->m_MonsterState != MONSTERSTATE_COMBAT && pMon->m_MonsterState != MONSTERSTATE_SCRIPT;
}

void CLinkPlayer::ZUpdateDoAction( void )
{
	CBaseEntity *pBest = NULL;
	int best = ZDO_NONE;
	float bestDist = 1e9;
	Vector fwd = ZForward();

	m_hZDoAction = NULL;

	if ( m_hZHeldBomb != NULL )
	{
		m_iZDoAction = ZDO_THROW;
		return;
	}

	if ( m_hZTalkTo != NULL || ZAction() == ZACT_TALK )
	{
		m_iZDoAction = ZDO_NEXT;
		return;
	}

	int act = ZAction();
	if ( act != ZACT_STAND && act != ZACT_SHIELD )
	{
		m_iZDoAction = ZDO_NONE;
		return;
	}

	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = UTIL_FindEntityInSphere( pEnt, pev->origin, 96 ) ) != NULL )
	{
		if ( pEnt == this )
			continue;

		Vector delta = pEnt->Center() - pev->origin;
		delta.z = 0;
		float dist = delta.Length();
		if ( dist < 1 )
			continue;

		// Facing it (within ~60 degrees), Player_ActionHandler_Talk uses the shape yaw
		if ( DotProduct( delta / dist, fwd ) < 0.5f )
			continue;

		int what = ZDO_NONE;

		if ( ZL_IsTalker( pEnt ) && dist < 90 )
			what = ZDO_SPEAK;
		else if ( ( FClassnameIs( pEnt->pev, "zl_sign" ) || FClassnameIs( pEnt->pev, "func_zl_sign" ) ) && dist < 80 )
			what = ZDO_CHECK;
		else if ( FClassnameIs( pEnt->pev, "zl_chest" ) && dist < 80 && pEnt->pev->frame == 0 )
			what = ZDO_OPEN;
		else if ( ( FClassnameIs( pEnt->pev, "func_door" ) || FClassnameIs( pEnt->pev, "func_door_rotating" ) ) &&
				  ( pEnt->ObjectCaps() & FCAP_IMPULSE_USE ) && dist < 96 )
			what = ZDO_OPEN;
		else if ( ( pEnt->ObjectCaps() & ( FCAP_IMPULSE_USE | FCAP_ONOFF_USE ) ) && !( pEnt->pev->flags & FL_MONSTER ) &&
				  !pEnt->IsPlayer() && dist < 72 )
			what = ZDO_CHECK;

		if ( what != ZDO_NONE && dist < bestDist )
		{
			bestDist = dist;
			best = what;
			pBest = pEnt;
		}
	}

	m_iZDoAction = best;
	m_hZDoAction = pBest;

	// Nothing to interact with: the A button shows what A will do on its own
	if ( best == ZDO_NONE && ( m_iZPad & ZPAD_Z ) && ( pev->iuser3 & ZSF_HASSWORD ) )
		m_iZDoAction = ZDO_ATTACK;	// jump attack
}

void CLinkPlayer::ZDoActionPressed( void )
{
	if ( m_hZHeldBomb != NULL )
	{
		Vector vel = pev->velocity;
		vel.z = 0;
		ZThrowBomb( vel.Length() < 20 );
		return;
	}

	CBaseEntity *pEnt = m_hZDoAction;
	if ( !pEnt )
		return;

	switch ( m_iZDoAction )
	{
	case ZDO_SPEAK:
		ZTalkTo( pEnt );
		break;

	case ZDO_CHECK:
	case ZDO_OPEN:
		pEnt->Use( this, this, USE_TOGGLE, 1 );
		break;
	}
}
