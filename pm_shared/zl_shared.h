/***
*
*	Zelda 64 -> GoldSrc port: definitions shared by the player movement code (pm_shared),
*	the server (dlls) and the client (cl_dll).
*
*	Everything in here has to stay plain C because pm_shared is compiled as C.
*
*	Reference: zeldaret/oot decompilation. Function names in comments refer to that project
*	(src/overlays/actors/ovl_player_actor/z_player.c unless noted otherwise).
*
****/
#ifndef ZL_SHARED_H
#define ZL_SHARED_H

#include "in_buttons.h"

/*
=============================================================================
 Units

 OoT game logic runs at 20 Hz (NTSC, R_UPDATE_RATE 3). Actor_UpdatePos() moves an actor
 by velocity * (R_UPDATE_RATE * 0.5) every logic frame, so one OoT "speed unit" is
 1.5 world units per 1/20th of a second.

 Adult Link is 68 units tall (Player_GetHeight) and the HL player hull is 72, so OoT world
 units are scaled by 72/68 to land on HL units.
=============================================================================
*/
#define ZL_UNIT			( 72.0f / 68.0f )
#define ZL_FPS			20.0f
#define ZL_POS_RATE		1.5f

// OoT speed (units per logic frame, before the 1.5 position rate) -> HL units per second
#define ZL_VEL( v )		( (v) * ZL_POS_RATE * ZL_FPS * ZL_UNIT )
// OoT per-frame speed step -> HL units per second squared
#define ZL_ACC( a )		( (a) * ZL_POS_RATE * ZL_FPS * ZL_FPS * ZL_UNIT )
// OoT logic frames -> seconds
#define ZL_FRAMES( f )	( (f) / ZL_FPS )
// OoT world distance -> HL units
#define ZL_DIST( d )	( (d) * ZL_UNIT )
// OoT binary angle (0x10000 = 360 degrees) -> degrees
#define ZL_BINANG( b )	( (b) * ( 360.0f / 65536.0f ) )

// sBootData[PLAYER_BOOTS_KOKIRI]: REG(68) gravity -1.0, R_RUN_SPEED_LIMIT 6.0, REG(19) accel 2.0
#define ZL_GRAVITY			ZL_ACC( 1.0f )
#define ZL_MAX_FALL_SPEED	ZL_VEL( 20.0f )		// actor.minVelocityY
#define ZL_RUN_SPEED		6.0f				// OoT speed units
#define ZL_ACCEL_STEP		2.0f				// REG(19) / 100
#define ZL_DECEL_STEP		1.5f

// The 29 frame walk cycle's step a frame (func_8084029C, before the x1.5) at OoT speed s.
// Walking and running (func_80841EE4): REG(35) 0.55 + REG(36) 0.27 * s until the run animation is
// fully blended in, REG(37) 0.6 * (s - REG(48) 3.7) >= 1, then 1.2 + REG(38) 0.35 * (s - 3.7).
// Side stepping while Z-targeting (func_80841860): REG(30) 1.0 + REG(32) 0.3 * s.
#define ZL_RUN_CYCLE_STEP( s )	( ( (s) - 3.7f ) * 0.6f < 1.0f ? 0.55f + 0.27f * (s) : 1.2f + 0.35f * ( (s) - 3.7f ) )
#define ZL_SIDE_CYCLE_STEP( s )	( 1.0f + 0.3f * (s) )
// Where the walk / run blend passes half: (s - 3.7) * 0.6 = 0.5
#define ZL_RUN_ANIM_SPEED		4.53f

/*
=============================================================================
 Buttons. N64 pad -> usercmd_t button bits (in_buttons.h).
 The client binds +zbtn_a / +zbtn_b / ... (see cl_dll/input.cpp).
=============================================================================
*/
#define ZBTN_A			IN_JUMP		// Action: roll / sidehop / backflip / jump attack / talk / open / throw
#define ZBTN_B			IN_ATTACK	// Sword
#define ZBTN_Z			IN_ALT1		// Z-target
#define ZBTN_R			IN_DUCK		// Shield
#define ZBTN_CLEFT		IN_ATTACK2
#define ZBTN_CDOWN		IN_RELOAD
#define ZBTN_CRIGHT		IN_RUN
#define ZBTN_CUP		IN_SCORE	// First person look

// usercmd_t.impact_index is set to this when impact_position carries the Z-target focus point
#define ZL_CMD_TARGET_VALID	1

/*
=============================================================================
 Predicted player state, carried in pev/pmove/clientdata fields:

   fuser1  facing yaw in degrees (actor.shape.rot.y)
   fuser4  time spent in the current action, seconds
   iuser4  ZL_STATE_* packed bits below (written by the movement code)
   iuser3  ZSF_* server flags (read-only for the movement code, also blocks HL ducking)

 These fields are added to clientdata_t in zelda/delta.lst.
=============================================================================
*/

// Actions. Each maps to one or more Player_Action_* functions in z_player.c.
enum zl_action_e
{
	ZACT_NONE = 0,		// Not in Link mode
	ZACT_STAND,			// Player_Action_Idle / _80840450 (walk) / _808407CC (run) / _80840DE4 (Z sidewalk)
	ZACT_ROLL,			// Player_Action_Roll
	ZACT_BONK,			// Player_Action_Roll -> av2.bonked (rolled into a wall at speed >= 7)
	ZACT_SIDEHOP,		// func_8083BCD0 -> Player_Action_8084411C (sub = stick direction)
	ZACT_BACKFLIP,		// func_8083BCD0 with PLAYER_STICK_DIR_BACKWARD
	ZACT_JUMP,			// func_8083A4A8 auto jump off a ledge
	ZACT_FALL,			// Player_Action_8084411C falling without a jump
	ZACT_LAND,			// func_80837B9C landing recovery
	ZACT_CLIMB,			// Player_Action_80845668 ledge climb (link_normal_100step_up / 150step_up)
	ZACT_SLASH,			// func_80837948 -> Player_Action_808502D0 (mwa = attack type)
	ZACT_JUMPSLASH,		// func_8083BA90 -> Player_Action_80844AF4 (PLAYER_MWA_JUMPSLASH_START)
	ZACT_JUMPSLASH_END,	// PLAYER_MWA_JUMPSLASH_FINISH
	ZACT_SPIN_CHARGE,	// Player_Action_80842180 / _8084227C (power_kiru_wait / walk)
	ZACT_SPIN,			// PLAYER_MWA_SPIN_ATTACK_1H / BIG_SPIN_1H
	ZACT_SHIELD,		// Player_Action_80843188 (standing defense)
	ZACT_AIM,			// First person bow / hookshot aim (CAM_MODE_AIM_ADULT)
	ZACT_CUP,			// C-up first person look (CAM_MODE_FIRST_PERSON)
	ZACT_HOOKFLY,		// Player_Action_HookshotFly
	ZACT_DAMAGE,		// Player_Action_8084370C (flinch)
	ZACT_KNOCKDOWN,		// Player_Action_8084377C / _80843954 (knocked down and getting up)
	ZACT_TALK,			// Player_Action_Talk
	ZACT_GETITEM,		// Player_Action_8084E3C4 (holding an item up over his head; sub 1 = out of a chest)
	ZACT_DEAD,
	ZACT_CHEST,			// Player_ActionHandler_2 -> link_demo_Tbox_open (EnBox; sub unused)

	ZACT_COUNT
};

#define ZL_STATE_ACTION_MASK	0x0000003F
#define ZL_STATE_SUB_SHIFT		6
#define ZL_STATE_SUB_MASK		0x000000C0
#define ZL_STATE_MWA_SHIFT		8
#define ZL_STATE_MWA_MASK		0x00001F00
#define ZL_STATE_COMBO_SHIFT	13
#define ZL_STATE_COMBO_MASK		0x00006000
#define ZL_STATE_LOCKED			0x00010000	// Locked onto an actor (PLAYER_STATE1_HOSTILE_LOCK_ON / focusActor)
#define ZL_STATE_PARALLEL		0x00020000	// Z held with nothing to target (PLAYER_STATE1_PARALLEL)
#define ZL_STATE_SHIELDING		0x00040000	// PLAYER_STATE1_SHIELDING
#define ZL_STATE_ONGROUND		0x00080000	// Was on ground at the end of the last move
#define ZL_STATE_CHARGED		0x00100000	// Spin attack fully charged
#define ZL_STATE_RECOIL			0x00200000	// Sword bounced off a wall: speedXZ -14, slowing 5 a frame (func_80842DF4)
#define ZL_STATE_BUTTONS_SHIFT	24			// Previous frame's N64 buttons, for edge detection
#define ZL_STATE_BUTTONS_MASK	0xFF000000

#define ZL_GetAction( s )		( (s) & ZL_STATE_ACTION_MASK )
#define ZL_GetSub( s )			( ( (s) & ZL_STATE_SUB_MASK ) >> ZL_STATE_SUB_SHIFT )
#define ZL_GetMWA( s )			( ( (s) & ZL_STATE_MWA_MASK ) >> ZL_STATE_MWA_SHIFT )
#define ZL_GetCombo( s )		( ( (s) & ZL_STATE_COMBO_MASK ) >> ZL_STATE_COMBO_SHIFT )

// Compact button bits stored in ZL_STATE_BUTTONS
#define ZPAD_A			0x01
#define ZPAD_B			0x02
#define ZPAD_Z			0x04
#define ZPAD_R			0x08
#define ZPAD_CLEFT		0x10
#define ZPAD_CDOWN		0x20
#define ZPAD_CRIGHT		0x40
#define ZPAD_CUP		0x80

// Server flags (iuser3). Set by CLinkPlayer before the move runs.
#define ZSF_ACTIVE		0x0001	// Link mode on. Non zero iuser3 also stops PM_Duck.
#define ZSF_DOACTION	0x0002	// A has a context action (talk / open / check), don't roll
#define ZSF_HOLDING		0x0004	// Holding a bomb over his head (PLAYER_STATE1_CARRYING_ACTOR)
#define ZSF_INVULN		0x0008	// Damage invincibility frames, the client flickers the model
#define ZSF_AIM_CLEFT	0x0010	// Item on C-left is aimed in first person (bow, hookshot)
#define ZSF_AIM_CDOWN	0x0020
#define ZSF_AIM_CRIGHT	0x0040
#define ZSF_HASSWORD	0x0080
#define ZSF_HASSHIELD	0x0100
#define ZSF_FROZEN		0x0200	// Talking / cutscene: no input at all
#define ZSF_MAGIC		0x0400	// Has the magic to light the spin attack (ZL_SPIN_MAGIC)
#define ZSF_HOOKSHOT	0x0800	// Hookshot chain is out, Link can't move
#define ZSF_SWORDOUT	0x1000	// Sword in hand (Player_GetMeleeWeaponHeld), not in its sheath
#define ZSF_HOSTILE		0x2000	// Locked onto an enemy (PLAYER_STATE1_HOSTILE_LOCK_ON)

// Stick directions relative to the facing yaw (PLAYER_STICK_DIR_*)
#define ZDIR_NONE		-1
#define ZDIR_FORWARD	0
#define ZDIR_LEFT		1
#define ZDIR_BACKWARD	2
#define ZDIR_RIGHT		3

/*
=============================================================================
 Melee weapon attack types (PlayerMeleeWeaponAnimation in include/player.h)
=============================================================================
*/
enum zl_mwa_e
{
	ZMWA_FORWARD_SLASH_1H = 0,
	ZMWA_FORWARD_SLASH_2H,
	ZMWA_FORWARD_COMBO_1H,
	ZMWA_FORWARD_COMBO_2H,
	ZMWA_RIGHT_SLASH_1H,
	ZMWA_RIGHT_SLASH_2H,
	ZMWA_RIGHT_COMBO_1H,
	ZMWA_RIGHT_COMBO_2H,
	ZMWA_LEFT_SLASH_1H,
	ZMWA_LEFT_SLASH_2H,
	ZMWA_LEFT_COMBO_1H,
	ZMWA_LEFT_COMBO_2H,
	ZMWA_STAB_1H,
	ZMWA_STAB_2H,
	ZMWA_STAB_COMBO_1H,
	ZMWA_STAB_COMBO_2H,
	ZMWA_FLIPSLASH_START,
	ZMWA_JUMPSLASH_START,
	ZMWA_FLIPSLASH_FINISH,
	ZMWA_JUMPSLASH_FINISH,
	ZMWA_BACKSLASH_RIGHT,
	ZMWA_BACKSLASH_LEFT,
	ZMWA_HAMMER_FORWARD,
	ZMWA_HAMMER_SIDE,
	ZMWA_SPIN_ATTACK_1H,
	ZMWA_SPIN_ATTACK_2H,
	ZMWA_BIG_SPIN_1H,
	ZMWA_BIG_SPIN_2H,
	ZMWA_COUNT
};

/*
=============================================================================
 Timings (seconds). OoT animation lengths divided by playback speed at 20 Hz.

 Link's animations advance playSpeed * R_UPDATE_RATE * 0.5 frames a game frame
 (LinkAnimation_Once / _Loop), so 1.5 at playSpeed 1 and 1 at PLAYER_ANIM_ADJUSTED_SPEED (2/3).
=============================================================================
*/
#define ZL_ANIM_RATE			1.5f	// animation frames per OoT frame at playSpeed 1
#define ZL_ROLL_RATE			( 1.25f * ZL_ANIM_RATE )	// Player_SetupRoll: playSpeed 1.25
#define ZL_ROLL_TIME			ZL_FRAMES( 20.0f / ZL_ROLL_RATE )	// Player_Action_Roll ends at anim frame 20
#define ZL_ROLL_INVULN_START	ZL_FRAMES( 8.0f / ZL_ROLL_RATE )	// Player_SetInvulnerability at frame 8
#define ZL_ROLL_INVULN_TIME		ZL_FRAMES( 10.0f )
#define ZL_ROLL_CHAIN_TIME		ZL_FRAMES( 15.0f / ZL_ROLL_RATE )	// frame 15+: Player_ActionHandler_7 (the sword) may cut in
#define ZL_BONK_TIME			0.9f
#define ZL_LAND_TIME			0.20f
#define ZL_HARD_LAND_TIME		0.55f
// Landing out of a hop (D_80853D4C's *_jump_end / *_jump_endR / L, func_8083A098: playSpeed 1)
#define ZL_HOP_LAND_TIME		ZL_FRAMES( 8.0f / ZL_ANIM_RATE )	// Lside_jump_end, Rside_jump_end and their L / R
#define ZL_FLIP_LAND_TIME		ZL_FRAMES( 7.0f / ZL_ANIM_RATE )	// backturn_jump_end
#define ZL_FLIP_LAND_TIME_R		ZL_FRAMES( 12.0f / ZL_ANIM_RATE )	// backturn_jump_endR (locked onto an enemy)
// Landing from a fall (Player_Action_8084411C): fallDistance is how far under where he left the
// ground he lands. Up to 80 a short landing; under 800 with the stick forward he rolls.
#define ZL_SHORT_LANDING_DIST	ZL_DIST( 80.0f )
#define ZL_LANDING_ROLL_DIST	ZL_DIST( 800.0f )
// ZACT_JUMP's sub: which jump func_8083A4A8 picked
#define ZJUMP_RUN				0	// link_normal_run_jump: speedXZ > 4 and running where he faces
#define ZJUMP_NORMAL			1	// link_normal_jump
// Putting the sword away (sItemChangeInfo[PLAYER_ITEM_CHG_5]: fighter2free at 1.2x, the sword
// goes into the sheath on frame 10 of 20)
#define ZL_PUTAWAY_RATE			( 1.2f * 1.5f )		// frames per OoT frame
#define ZL_PUTAWAY_SWAP_TIME	ZL_FRAMES( 10.0f / ZL_PUTAWAY_RATE )
#define ZL_PUTAWAY_TIME			ZL_FRAMES( 20.0f / ZL_PUTAWAY_RATE )
#define ZL_SLASH_TIME			0.40f	// kiru + kiru_end
#define ZL_SLASH_COMBO_TIME		0.20f	// B pressed after this chains the next swing
#define ZL_SLASH_RECOVER_TIME	0.65f	// kiru_endR, interruptible by movement
#define ZL_JUMPSLASH_END_TIME	0.45f
// Charging the spin attack: unk_858 steps 0.02 a frame (func_80844E3C). Let go at 0.1 or more and
// he spins; past 0.15 the sword lights up blue (EnMThunder, Audio_PlaySwordChargeSfx level 1),
// past 0.85 it turns orange (level 2) and letting go is the great spin. The port keeps the great
// spin for when Link has magic.
// It starts once B has been held since the swing began for 7 frames (unk_844 counts down from 8,
// Player_ActionHandler_8), and unk_858 only counts after he raises the sword: power_kiru_start
// from frame 8 to its end at playSpeed 1, 2 frames.
// Lighting the sword needs ZL_SPIN_MAGIC (EnMThunder: params 0x200, Magic_RequestChange
// MAGIC_CONSUME_WAIT_PREVIEW): the meter shows it in yellow, its border flashes, and letting go
// lit takes it. Without it there's no light, no ring and no charge sound.
#define ZL_SPIN_MAGIC			2
#define ZL_SPIN_CHARGE_ENTER	ZL_FRAMES( 7.0f )
#define ZL_SPIN_CHARGE_START	ZL_FRAMES( 2.0f )
#define ZL_SPIN_CHARGE_TIME		( ZL_SPIN_CHARGE_START + ZL_FRAMES( 0.10f / 0.02f ) )
#define ZL_SPIN_GLOW_TIME		( ZL_SPIN_CHARGE_START + ZL_FRAMES( 0.15f / 0.02f ) )
#define ZL_SPIN_BIG_TIME		( ZL_SPIN_CHARGE_START + ZL_FRAMES( 0.85f / 0.02f ) )
#define ZL_SPIN_CHARGE_FREQ1	1.12246f	// sSfxSwordChargeFreqLevels: two and five semitones up
#define ZL_SPIN_CHARGE_FREQ2	1.33484f
// rolling_kiru (13 frames) / Wrolling_kiru (17), played one frame a frame (Player_AnimPlayOnceAdjusted)
#define ZL_SPIN_TIME			ZL_FRAMES( 13.0f )
#define ZL_BIG_SPIN_TIME		ZL_FRAMES( 17.0f )
#define ZL_SpinTime( mwa )		( (mwa) == ZMWA_BIG_SPIN_1H ? ZL_BIG_SPIN_TIME : ZL_SPIN_TIME )
#define ZL_DAMAGE_TIME			0.35f
#define ZL_KNOCKDOWN_TIME		1.40f
#define ZL_LEDGE_PUSH_TIME		ZL_FRAMES( 6.0f )	// ledgeClimbDelayTimer >= 6
#define ZL_GETITEM_TIME			1.60f

// Opening a chest (EnBox_Open / link_demo_Tbox_open, played at one frame per OoT frame).
// The lid's animation runs at 1.5 frames per OoT frame: the latch clicks on its frame 30,
// the lid swings on frame 90.
#define ZL_CHEST_TIME			ZL_FRAMES( 133.0f )
#define ZL_CHEST_UNLOCK_TIME	ZL_FRAMES( 30.0f / 1.5f )
#define ZL_CHEST_OPEN_TIME		ZL_FRAMES( 90.0f / 1.5f )
#define ZL_CHEST_STAND_DIST		ZL_DIST( 29.4343f )	// Link stands this far from the chest

// Ledges (Player_UpdateBgCheck, Player_ActionHandler_12, sAgeProperties[adult]): a ray drops
// 10 past the wall from unk_0C 111 above his feet. Under unk_1C 41 he hops up (link_normal_jump);
// up to unk_18 59 link_normal_100step_up, up to unk_14 79.4 link_normal_150step_up, both at
// playSpeed 1.3. He's put on top at once; the animation carries the body up 41 / 59 from below
// and back 25 from the edge, and shape.yOffset makes up the rest, stepping back 150 model units
// a frame (after frame 5 of 150step_up).
#define ZL_CLIMB_MIN			ZL_DIST( 41.0f )
#define ZL_CLIMB_LOW			ZL_DIST( 59.0f )
#define ZL_CLIMB_MAX			ZL_DIST( 79.4f )
#define ZL_LEDGE_CHECK			ZL_DIST( 111.0f )
#define ZL_CLIMB_RATE			( 1.3f * 1.5f )
#define ZL_CLIMB_BACK			ZL_DIST( 25.28f )		// 150step_up's root starts 2528 model units back
#define ZL_ClimbRise( sub )		( (sub) ? ZL_CLIMB_LOW : ZL_CLIMB_MIN )
#define ZL_ClimbTime( sub )		ZL_FRAMES( ( (sub) ? 44.0f : 27.0f ) / ZL_CLIMB_RATE )

// How far the hookshot reaches (the longshot twice as far). The hookshot's red dot looks this far.
#define ZL_HOOK_RANGE			ZL_DIST( 20.0f * ZL_POS_RATE * 13.0f )

// Running into a wall (Player_ProcessSceneCollision): the stick's speed is capped at
// R_RUN_SPEED_LIMIT x (angle between Link's yaw and straight into the wall, binang) x 0.00008,
// at least 0.1, so head on he stops and at a glance he keeps most of his speed
#define ZL_WALL_SPEED_SCALE		0.00008f
#define ZL_WALL_SPEED_MIN		0.1f

// First person aim (Camera_Subj3, sSetNormal0ModeAimAdultData / FirstPersonData): fov 45,
// N64 vertical, which is 58 horizontal at 4:3 like Half-Life's fov
#define ZL_AIM_FOV				58

/*
=============================================================================
 Health, magic and rupees (gSaveContext.save.info.playerData)
=============================================================================
*/
#define ZL_HEART				16		// health units per heart (0x10)
#define ZL_START_HEARTS			3
#define ZL_MAX_HEARTS			20
#define ZL_MAGIC_SINGLE			48
#define ZL_MAGIC_DOUBLE			96
#define ZL_INVINCIBILITY_TIME	ZL_FRAMES( 20.0f )

// Wallet sizes (CUR_CAPACITY(UPG_WALLET))
#define ZL_WALLET_CHILD			99
#define ZL_WALLET_ADULT			200
#define ZL_WALLET_GIANT			500

/*
=============================================================================
 Inventory items (subset of ItemID from include/item.h, renumbered)
=============================================================================
*/
enum zl_item_e
{
	ZITEM_NONE = 0,
	ZITEM_BOW,
	ZITEM_BOMB,
	ZITEM_HOOKSHOT,
	ZITEM_LONGSHOT,
	ZITEM_DEKU_NUT,
	ZITEM_DEKU_STICK,
	ZITEM_BOTTLE,
	ZITEM_HLWEAPON,		// A borrowed Half-Life weapon put on a C button
	ZITEM_COUNT
};

// Equipment bits
#define ZEQ_SWORD_KOKIRI	0x0001
#define ZEQ_SWORD_MASTER	0x0002
#define ZEQ_SWORD_BIGGORON	0x0004
#define ZEQ_SHIELD_DEKU		0x0010
#define ZEQ_SHIELD_HYLIAN	0x0020
#define ZEQ_SHIELD_MIRROR	0x0040

// Do-action labels shown on the A button (DO_ACTION_* in include/interface.h)
enum zl_doaction_e
{
	ZDO_NONE = 0,
	ZDO_ATTACK,
	ZDO_CHECK,
	ZDO_ENTER,
	ZDO_RETURN,
	ZDO_OPEN,
	ZDO_JUMP,
	ZDO_DECIDE,
	ZDO_DIVE,
	ZDO_FASTER,
	ZDO_THROW,
	ZDO_CLIMB,
	ZDO_DROP,
	ZDO_DOWN,
	ZDO_SAVE,
	ZDO_SPEAK,
	ZDO_NEXT,
	ZDO_GRAB,
	ZDO_STOP,
	ZDO_PUTAWAY,
	ZDO_REEL,
	ZDO_COUNT
};

/*
=============================================================================
 User messages (server -> client). Registered in CLinkPlayer::LinkPrecache.
=============================================================================
*/
#define ZMSG_STATS		"ZStats"	// health, max health, magic, max magic, rupees, wallet, keys
#define ZMSG_BUTTONS	"ZButtons"	// B item, C items + ammo, A do-action
#define ZMSG_TARGET		"ZTarget"	// locked entindex, hover entindex, flags
#define ZMSG_TEXT		"ZText"		// message box: open / close
#define ZMSG_MUSIC		"ZMusic"	// play / stop / fade
#define ZMSG_ITEMGET	"ZItemGet"	// item get fanfare + icon
#define ZMSG_FX			"ZFx"		// a hit effect: type byte, position (cl_dll/zl_effects.cpp)

// ZFx types
#define ZFX_HITMARK_WHITE	0	// EffectSsHitMark EFFECT_HITMARK_WHITE: the sword cuts into something
#define ZFX_HITMARK_METAL	1	// EFFECT_HITMARK_METAL: it bounces off armour (CollisionCheck_HitSolid)
#define ZFX_SPARKS			2	// CollisionCheck_SpawnShieldParticles: sword on a wall, a blocked hit

// ZText commands
#define ZTEXT_OPEN		1
#define ZTEXT_CLOSE		2

// ZMusic commands
#define ZMUS_PLAY		1
#define ZMUS_STOP		2
#define ZMUS_FADEOUT	3
#define ZMUS_VOLUME		4
#define ZMUS_FANFARE	5	// track = a fanfare name, loop = sound to play instead without music (255: none)

// ZTarget flags
#define ZTGT_LOCKED		0x01
#define ZTGT_HOSTILE	0x02	// ACTORCAT_ENEMY colors
#define ZTGT_HOVER_HOSTILE 0x04

#endif // ZL_SHARED_H
