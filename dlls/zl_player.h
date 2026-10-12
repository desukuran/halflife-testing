/***
*
*	Zelda 64 port: Link, the player class.
*
*	CLinkPlayer replaces CBasePlayer as the "player" entity. With zl_enable 0 it behaves
*	exactly like the Half-Life player. With it on, movement comes from pm_shared/pm_zelda.c
*	and this class handles everything the server owns:
*
*		- hearts / magic / rupees (gSaveContext.save.info.playerData)
*		- the attention system: Z-targeting (z_actor.c Attention_*)
*		- sword hits, shield blocks, damage in and out (zl_combat.cpp)
*		- C button items (zl_items.cpp)
*		- the A button do-action: talk, open, check (Player_ActionHandler_Talk)
*		- picking player model sequences for Link's actions (zl_anim.cpp)
*
****/
#ifndef ZL_PLAYER_H
#define ZL_PLAYER_H

#include "player.h"
#include "zl_shared.h"
#include "zl_sounds.h"

class CZeldaHookshot;

// Inventory (gSaveContext.save.info.inventory, cut down)
struct zl_inventory_t
{
	int		items;				// bit per zl_item_e that Link owns
	int		equipment;			// ZEQ_* bits
	int		ammo[ZITEM_COUNT];	// arrows, bombs, nuts, sticks
	int		cButton[3];			// item on C-left, C-down, C-right
	int		cWeapon[3];			// HL weapon id when cButton is ZITEM_HLWEAPON
	int		quiver;				// max arrows (30 / 40 / 50)
	int		bombBag;			// max bombs (20 / 30 / 40)
	int		smallKeys;
};

class CLinkPlayer : public CBasePlayer
{
public:
	void	Spawn( void ) override;
	void	Precache( void ) override;
	void	PreThink( void ) override;
	void	PostThink( void ) override;
	void	Jump( void ) override;
	void	Duck( void ) override;
	int		TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType ) override;
	int		TakeHealth( float flHealth, int bitsDamageType ) override;
	void	Killed( entvars_t *pevAttacker, int iGib ) override;
	void	UpdateClientData( void ) override;
	void	SetAnimation( PLAYER_ANIM playerAnim ) override;
	void	ImpulseCommands( void ) override;
	int		Save( CSave &save ) override;
	int		Restore( CRestore &restore ) override;

	static TYPEDESCRIPTION m_SaveData[];

	// Is the Zelda layer running for this player?
	BOOL	LinkActive( void );

	// Predicted state (written by the movement code)
	int		ZState( void ) const		{ return pev->iuser4; }
	int		ZAction( void ) const		{ return ZL_GetAction( pev->iuser4 ); }
	float	ZActionTime( void ) const	{ return pev->fuser4; }
	float	ZFacing( void ) const		{ return pev->fuser1; }
	void	ZSetAction( int action );
	Vector	ZForward( void );

	// Buttons in N64 terms (ZPAD_*), from the raw usercmd buttons
	int		m_iZPad;
	int		m_iZPadLast;
	int		ZPressed( int bits ) const	{ return ( m_iZPad & ~m_iZPadLast ) & bits; }
	int		ZReleased( int bits ) const	{ return ( m_iZPadLast & ~m_iZPad ) & bits; }
	int		ZHeld( int bits ) const		{ return m_iZPad & bits; }

	// playerData
	int		m_iZHealthCapacity;		// health units (16 per heart)
	int		m_iZMagic;
	int		m_iZMagicCapacity;		// 0, 48 or 96
	int		m_iZRupees;
	int		m_iZWallet;
	BOOL	m_fZDoubleDefense;
	zl_inventory_t m_ZInv;

	void	ZAddRupees( int amount );
	BOOL	ZUseMagic( int amount );
	void	ZAddMagic( int amount );
	void	ZAddHeartContainer( void );
	void	ZGiveItem( int item );
	void	ZGiveEquipment( int bits );
	int		ZSwordDamage( int mwa );	// OoT damage of the current sword for a swing type

	// Attention system (zl_targeting.cpp)
	EHANDLE	m_hZTarget;			// focusActor
	EHANDLE	m_hZHover;			// attention.naviHoverActor: candidate shown with the arrow
	float	m_flZLockTime;
	int		m_iZTargetSent;
	int		m_iZHoverSent;
	int		m_iZTargetFlagsSent;
	void	ZUpdateTargeting( void );
	CBaseEntity *ZFindTarget( CBaseEntity *pExclude, BOOL locked );
	BOOL	ZIsTargetable( CBaseEntity *pEnt, float *pRange, float *pLeash );
	void	ZReleaseTarget( BOOL playSound );

	// A button (Player_ActionHandler_Talk and friends)
	EHANDLE	m_hZDoAction;
	int		m_iZDoAction;		// ZDO_*
	void	ZUpdateDoAction( void );
	void	ZUpdateModel( void );		// link.mdl hands / sheath bodygroups and blinking
	void	ZFreezeWorld( BOOL freeze );	// monsters hold still while a text box is up
	void	ZDoActionPressed( void );

	// Talking (zl_message.cpp)
	EHANDLE	m_hZTalkTo;
	int		m_iZTalkKind;		// what we asked, so the choice answer means something
	float	m_flZTalkStart;
	void	ZStartMessage( const char *pszMessage, CBaseEntity *pSpeaker, int kind );
	void	ZTalkTo( CBaseEntity *pNPC );
	void	ZMessageDone( int choice );

	// Combat (zl_combat.cpp)
	float	m_flZInvincibleUntil;
	BOOL	m_fZDirectDamage;	// TakeDamage amount is already in OoT health units
	int		m_iZLastSwingId;	// action+mwa+combo of the swing whose hits we tracked
	EHANDLE	m_hZSwingHits[16];
	int		m_iZSwingHitCount;
	BOOL	m_fZSwingHitWorld;
	float	m_flZFallStartZ;
	BOOL	m_fZWasOnGround;
	float	m_flZLowHealthBeep;
	void	ZSwordThink( void );
	void	ZSwordSweep( float minDeg, float maxDeg, float reach, float height, int ootDamage, int dmgBits );
	BOOL	ZTryBlock( entvars_t *pevInflictor, int bitsDamageType );
	void	ZApplyKnockback( entvars_t *pevInflictor, int healthUnits, int bitsDamageType );
	void	ZFallDamageThink( void );

	// Items (zl_items.cpp)
	int		m_iZHeldItem;			// item the C button is using right now
	BOOL	m_fZSwordOut;			// sword (and shield) in hand until A "Put Away"
	float	m_flZPutAwayStart;		// A "Put Away" started (fighter2free plays), 0 = not putting away
	float	m_flZPutAwayReady;		// when "Put Away" came on offer (putAwayCooldownTimer), 0 = it isn't
	float	m_flZTalkEndTime;		// when the last text box closed (its A press mustn't start another)
	BOOL	m_fZWorldFrozen;
	float	m_flZBlinkNext;			// next blink (link.mdl eye skins)
	int		m_iZBlinkStep;
	int		m_iZHeldButton;			// 0..2 or -1
	float	m_flZItemTime;
	int		m_iZPreMoveAction;
	EHANDLE	m_hZHeldBomb;
	EHANDLE	m_hZHookshot;
	float	m_flZNextItemUse;
	void	ZItemThink( void );
	void	ZItemButtonDown( int slot );
	void	ZItemButtonUp( int slot );
	void	ZFireArrow( void );
	void	ZPullBomb( void );
	void	ZThrowBomb( BOOL drop );
	void	ZFireHookshot( void );
	void	ZThrowNut( void );
	BOOL	ZItemAimable( int item );
	int		ZItemAmmo( int item );
	BOOL	ZFirstPersonAim( void );
	Vector	ZAimVector( void );
	Vector	ZHandPosition( void );
	BOOL	ZHandsMidpoint( Vector &out );	// between link.mdl's hands (sGetItemRefPos), FALSE without it

	// HL weapons on C buttons
	int		ZTranslateHLButtons( int raw );

	// Animation (zl_anim.cpp)
	int		m_iZAnimState;			// what the last picked animation was keyed on
	int		m_iZAnim;
	int		m_iZLandAnim;			// landing that plays standing until he moves (func_8083A098), 0 = none
	int		m_iZLandNext;			// what follows it (a swing's end animation)
	int		m_iZIdleAnim;			// standing still: the idle or fidget playing (Player_ChooseNextIdleAnim)
	int		m_iZIdleType;			// idleType: every other idle is a fidget, -1 at critical health
	BOOL	m_fZIdleRestart;		// play m_iZIdleAnim again from the start
	float	m_flZIdleFrame;			// frame of it at the last ZAnimate, for its sounds
	float	m_flZAirStart;			// fallStartHeight while he's in the air (vuser3.z)
	float	m_flZLastSpeed;			// horizontal speed at the last ZAnimate
	void	ZAnimate( void );

	// HUD
	int		m_iZStatsSent[8];
	int		m_iZButtonsSent[10];
	void	ZSendStats( BOOL force );
	void	ZSendButtons( BOOL force );

	// iuser3 flags given to the movement code
	void	ZUpdateServerFlags( void );

	// zl_sound.cpp
	void	ZSound( int channel, int sound, float volume = 1.0f, float attn = ATTN_NORM, int pitch = PITCH_NORM );

	void	ZResetPlayerData( void );
	BOOL	m_fZInitialized;
};

// Helpers
CLinkPlayer *ZL_GetLink( CBaseEntity *pEnt );
BOOL ZL_Enabled( void );
void ZL_RegisterCvars( void );
void ZL_PrecacheAll( void );
void ZL_EmitSound( edict_t *pEdict, int channel, int sound, float volume, float attn, int pitch );
const char *ZL_SoundPath( int sound );
void ZL_SendFx( int type, const Vector &pos );
const char *ZL_SoundTakesKey( void );
BOOL ZL_FileExists( const char *path );	// relative to the mod folder
	// "zv" physinfo: takes per sound id

// Damage translation (zl_combat.cpp)
int		ZL_HLDamageToHealth( float flDamage, int bitsDamageType );
float	ZL_OoTDamageToHL( int ootDamage );

// Messages (zl_message.cpp)
extern int gmsgZStats, gmsgZButtons, gmsgZTarget, gmsgZText, gmsgZMusic, gmsgZItemGet;
void ZL_RegisterMessages( void );
BOOL ZL_ClientCommand( CBasePlayer *pPlayer, const char *pcmd );

// Music (zl_music.cpp)
void ZL_MusicPlay( const char *track, float fadein, float volume, int loop );
void ZL_MusicStop( float fadeout );
void ZL_MusicFanfare( const char *track, int fallbackSound );
void ZL_MusicResend( edict_t *pPlayer );

// Monster hooks
void ZL_MonsterKilled( CBaseMonster *pMonster, entvars_t *pevAttacker );

extern cvar_t zl_enable;
extern cvar_t zl_dmg_in_scale;
extern cvar_t zl_dmg_out_scale;
extern cvar_t zl_ztoggle;
extern cvar_t zl_model;
const char *ZL_PlayerModel( void );	// zl_model if the file exists, else models/player.mdl
extern cvar_t zl_oot_sounds;
extern cvar_t zl_hl_suit;
extern cvar_t zl_drops;
extern cvar_t zl_hookshot_any;
extern cvar_t zl_room_env;
extern cvar_t zl_anim_debug;

#endif // ZL_PLAYER_H
