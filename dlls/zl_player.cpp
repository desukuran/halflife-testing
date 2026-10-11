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

extern DLL_GLOBAL ULONG g_ulModelIndexPlayer;

cvar_t zl_enable		= { "zl_enable", "1", FCVAR_SERVER };
cvar_t zl_dmg_in_scale	= { "zl_dmg_in_scale", "0.8", FCVAR_SERVER };	// HL damage -> OoT health units
cvar_t zl_dmg_out_scale	= { "zl_dmg_out_scale", "10", FCVAR_SERVER };	// OoT damage -> HL damage
cvar_t zl_ztoggle		= { "zl_ztoggle", "0", FCVAR_ARCHIVE };		// OoT options menu: Z Target "Switch" instead of "Hold"
cvar_t zl_model			= { "zl_model", "auto", FCVAR_ARCHIVE };	// auto: models/player/link/link.mdl (make_link_model.py) when it exists; hl: player.mdl
cvar_t zl_oot_sounds	= { "zl_oot_sounds", "1", FCVAR_ARCHIVE };	// 1: OoT sounds from sound/zelda/oot/ where they exist, 0: Half-Life only
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
	CVAR_REGISTER( &zl_oot_sounds );
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

	if ( strcmp( ZL_PlayerModel(), "models/player.mdl" ) )
	{
		SET_MODEL( ENT( pev ), ZL_PlayerModel() );
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
		// CheckPowerups() puts g_ulModelIndexPlayer back on every frame ("don't use eyes")
		g_ulModelIndexPlayer = pev->modelindex;
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
	m_flZTalkEndTime = -1.0f;
	m_flZPutAwayStart = 0;
	m_fZWorldFrozen = FALSE;

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
	if ( m_fZSwordOut )
		f |= ZSF_SWORDOUT;
	if ( m_hZTarget != NULL && IRelationship( m_hZTarget ) > R_NO )
		f |= ZSF_HOSTILE;

	if ( m_iZDoAction != ZDO_NONE && m_iZDoAction != ZDO_ATTACK && m_iZDoAction != ZDO_THROW )
		f |= ZSF_DOACTION;
	if ( m_hZHeldBomb != NULL )
		f |= ZSF_HOLDING;
	if ( m_flZInvincibleUntil > gpGlobals->time )
		f |= ZSF_INVULN;

	if ( ZItemAimable( m_ZInv.cButton[0] ) ) f |= ZSF_AIM_CLEFT;
	if ( ZItemAimable( m_ZInv.cButton[1] ) ) f |= ZSF_AIM_CDOWN;
	if ( ZItemAimable( m_ZInv.cButton[2] ) ) f |= ZSF_AIM_CRIGHT;

	if ( m_hZTalkTo != NULL || ZAction() == ZACT_TALK || ZAction() == ZACT_GETITEM || ZAction() == ZACT_CHEST || ( pev->flags & FL_FROZEN ) )
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
	m_iZPreMoveAction = ZAction();	// the move below can end AIM on the very command that releases C
	m_iZPad = ZL_PadFromButtons( raw );

	pev->button = ZTranslateHLButtons( raw );

	CBasePlayer::PreThink();

	if ( IsAlive() && !g_fGameOver )
	{
		ZUpdateTargeting();
		ZUpdateDoAction();

		// The A that closed a text box isn't a new press: wait until it has been let go
		if ( ZPressed( ZPAD_A ) && gpGlobals->time - m_flZTalkEndTime > 0.3f )
			ZDoActionPressed();
	}

	ZUpdateServerFlags();

	// Which sounds this map precached (zl_sounds.h). Kept up to date here rather than in Spawn
	// so loading a saved game or changing maps can't leave a stale list behind.
	const char *zv = g_engfuncs.pfnGetPhysicsKeyValue( edict(), "zv" );
	if ( !zv || strcmp( zv, ZL_SoundTakesKey() ) )
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "zv", ZL_SoundTakesKey() );
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
	ZUpdateModel();

	// While a text box is up the world holds still and the speaker turns to Link
	if ( ZAction() == ZACT_TALK || ZAction() == ZACT_GETITEM )
		ZFreezeWorld( TRUE );
	else if ( m_fZWorldFrozen )
		ZFreezeWorld( FALSE );

	// Link's body faces his shape yaw, not the camera
	pev->angles = Vector( 0, ZFacing(), 0 );

	// Talking: stay frozen until the client closes the box. Safety net for lost commands.
	if ( m_hZTalkTo == NULL && ZAction() == ZACT_TALK && gpGlobals->time - m_flZTalkStart > 60.0f )
		ZSetAction( ZACT_STAND );
}

/*
=============================================================================
 link.mdl: what Link holds (gPlayerModelTypes) and blinking

 Bodygroups, in the order make_link_model.py writes them:
   1 lhand: 0 open, 1 fist, 2 master sword
   2 rhand: 0 open, 1 fist, 2 hylian shield, 3 bow, 4 hookshot
   3 back:  0 sword + shield, 1 shield + empty sheath, 2 empty sheath, 3 sword in sheath
 Skin: 0 eyes open, 1 half, 2 closed
=============================================================================
*/
void CLinkPlayer::ZUpdateModel( void )
{
	int act = ZAction();

	// Swinging draws the sword; items, climbing and grabbing put it away (Player_PutAwayHeldItem),
	// and so do talking, chests and item gets (Player_SetupWaitForPutAway)
	if ( act == ZACT_SLASH || act == ZACT_JUMPSLASH || act == ZACT_JUMPSLASH_END || act == ZACT_SPIN_CHARGE || act == ZACT_SPIN )
	{
		m_fZSwordOut = TRUE;
		m_flZPutAwayStart = 0;
	}
	else if ( act == ZACT_AIM || act == ZACT_CLIMB || act == ZACT_HOOKFLY || act == ZACT_TALK || act == ZACT_CHEST ||
			  act == ZACT_GETITEM || m_hZHeldBomb != NULL || !( pev->iuser3 & ZSF_HASSWORD ) )
	{
		m_fZSwordOut = FALSE;
		m_flZPutAwayStart = 0;
	}

	// A "Put Away": fighter2free on the upper body, the sword goes back in on its frame 10
	if ( m_flZPutAwayStart > 0 )
	{
		float t = gpGlobals->time - m_flZPutAwayStart;
		if ( m_fZSwordOut && t >= ZL_PUTAWAY_SWAP_TIME )
		{
			m_fZSwordOut = FALSE;
			ZSound( CHAN_ITEM, ZS_IT_PUT_AWAY );
		}
		if ( t >= ZL_PUTAWAY_TIME || ( act != ZACT_STAND && act != ZACT_SHIELD ) )
		{
			if ( m_fZSwordOut && t < ZL_PUTAWAY_SWAP_TIME )
				m_fZSwordOut = FALSE;		// interrupted before the swap: it goes away anyway
			m_flZPutAwayStart = 0;
		}
	}

	if ( !strstr( STRING( pev->model ), "/link/" ) )
		return;

	BOOL hasShield = ( pev->iuser3 & ZSF_HASSHIELD ) != 0;
	BOOL shielding = act == ZACT_SHIELD || ( ZState() & ZL_STATE_SHIELDING );
	int item = m_iZHeldItem;
	int lhand = 0, rhand = 0, back = hasShield ? 0 : 3;

	if ( item == ZITEM_BOW && ( act == ZACT_AIM || ( ZState() & ( ZL_STATE_LOCKED | ZL_STATE_PARALLEL ) ) ) )
	{
		lhand = 1;		// PLAYER_MODELGROUP_BOW_SLINGSHOT
		rhand = 3;
	}
	else if ( ( item == ZITEM_HOOKSHOT || item == ZITEM_LONGSHOT ) && ( act == ZACT_AIM || act == ZACT_HOOKFLY ||
				( ZState() & ( ZL_STATE_LOCKED | ZL_STATE_PARALLEL ) ) ) )
	{
		rhand = 4;		// PLAYER_MODELGROUP_HOOKSHOT
	}
	else if ( m_fZSwordOut )
	{
		lhand = 2;		// PLAYER_MODELGROUP_SWORD_AND_SHIELD
		rhand = hasShield ? 2 : 1;
		back = 2;
	}
	else if ( shielding && hasShield )
	{
		rhand = 2;		// Player_SetModelsForHoldingShield
		back = 3;
	}

	SetBodygroup( 1, lhand );
	SetBodygroup( 2, rhand );
	SetBodygroup( 3, back );

	// Blink every 1-4 seconds: half, closed, closed, half at 20 frames a second
	static const int blink[] = { 1, 2, 2, 1, 0 };
	if ( gpGlobals->time >= m_flZBlinkNext )
	{
		if ( m_iZBlinkStep < (int)( sizeof( blink ) / sizeof( blink[0] ) ) )
		{
			pev->skin = blink[m_iZBlinkStep++];
			m_flZBlinkNext = gpGlobals->time + 0.05f;
		}
		else
		{
			m_iZBlinkStep = 0;
			m_flZBlinkNext = gpGlobals->time + RANDOM_FLOAT( 1.0f, 4.0f );
		}
	}
	if ( act == ZACT_DEAD || act == ZACT_KNOCKDOWN )
		pev->skin = 2;
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

		if ( FClassnameIs( pEnt->pev, "monster_gossip_stone" ) && dist < 80 )
			what = ZDO_CHECK;		// EnGs: Actor_OfferTalk at 40 units
		else if ( ZL_IsTalker( pEnt ) && dist < 90 )
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
	else if ( best == ZDO_NONE && m_fZSwordOut && !m_flZPutAwayStart && act == ZACT_STAND && pev->velocity.Length2D() < 20 )
		m_iZDoAction = ZDO_PUTAWAY;	// standing still with the sword out (running, A still rolls)
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

	if ( m_iZDoAction == ZDO_PUTAWAY )
	{
		if ( !m_flZPutAwayStart )
			m_flZPutAwayStart = gpGlobals->time;	// ZUpdateModel plays it out
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
