/***
*
*	Zelda 64 port: picking player model animations for Link's actions.
*
*	Every entry names the OoT animation it should be (gPlayerAnim_* in the decomp,
*	object link_animetion), then a list of sequence names to try in order:
*
*	  1. the OoT name without "gPlayerAnim_", which is what a ripped link.mdl will use
*	  2. stock Half-Life player.mdl sequences that are the closest stand-in
*
*	Half-Life player models split the body: "sequence" plays on the torso, "gaitsequence"
*	on the legs (blended by the client from velocity). When a sequence from (1) is found the
*	gait is turned off so the full body plays the OoT animation.
*
*	link.mdl plays every animation at one frame per OoT frame (PLAYER_ANIM_ADJUSTED_SPEED);
*	"rate" is for the ones OoT plays faster (see ZL_ANIM_RATE in zl_shared.h).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "animation.h"
#include "zl_player.h"

enum zl_anim_e
{
	ZANIM_IDLE,
	ZANIM_WALK,
	ZANIM_RUN,
	ZANIM_Z_IDLE,
	ZANIM_Z_SIDE_L,
	ZANIM_Z_SIDE_R,
	ZANIM_Z_BACK,
	ZANIM_ROLL,
	ZANIM_BONK,
	ZANIM_SIDEHOP_L,
	ZANIM_SIDEHOP_R,
	ZANIM_BACKFLIP,
	ZANIM_JUMP,
	ZANIM_FALL,
	ZANIM_LAND,
	ZANIM_CLIMB,
	ZANIM_CLIMB_LOW,		// link_normal_100step_up
	ZANIM_SLASH_RIGHT,
	ZANIM_SLASH_LEFT,
	ZANIM_SLASH_FORWARD,
	ZANIM_STAB,
	ZANIM_SLASH_RIGHT_COMBO,
	ZANIM_SLASH_LEFT_COMBO,
	ZANIM_SLASH_FORWARD_COMBO,
	ZANIM_STAB_COMBO,
	ZANIM_JUMPSLASH,
	ZANIM_JUMPSLASH_END,
	ZANIM_SPIN_CHARGE,
	ZANIM_SPIN_CHARGE_WALK,
	ZANIM_SPIN,
	ZANIM_BIG_SPIN,
	ZANIM_SHIELD,
	ZANIM_AIM_BOW,
	ZANIM_AIM_HOOK,
	ZANIM_HOOKFLY,
	ZANIM_DAMAGE,
	ZANIM_KNOCKDOWN,
	ZANIM_TALK,
	ZANIM_TALK_WAIT,
	ZANIM_GETITEM,
	ZANIM_GETITEM_B,
	ZANIM_CHEST,
	ZANIM_HOP_LAND_L,		// landing out of a hop, D_80853D4C[dir][1] / [2] (locked onto an enemy)
	ZANIM_HOP_LAND_L_LOCK,
	ZANIM_HOP_LAND_R,
	ZANIM_HOP_LAND_R_LOCK,
	ZANIM_FLIP_LAND,
	ZANIM_FLIP_LAND_LOCK,
	ZANIM_PUTAWAY,			// upper body, legs from the gait like carrying
	ZANIM_PUTAWAY_NOSHIELD,
	ZANIM_CARRY,			// zl_carry_*: carryB_wait on the upper body, *_free legs (make_link_model.py)
	ZANIM_CARRY_WALK,
	ZANIM_CARRY_RUN,
	ZANIM_SWIM,
	ZANIM_SWIM_IDLE,
	ZANIM_JUMP_NORMAL,		// func_8083A4A8: off a ledge slow or not quite where he faces
	ZANIM_RUN_JUMP_END,		// landings out of the air (Player_Action_8084411C), standing until he moves
	ZANIM_SHORT_LANDING,
	ZANIM_ANCHOR_LANDING,
	ZANIM_END_FORWARD,		// after a swing (D_80854190 unk_04 / unk_08 when locked onto an enemy)
	ZANIM_END_FORWARD_R,
	ZANIM_END_FORWARD_COMBO,
	ZANIM_END_FORWARD_COMBO_R,
	ZANIM_END_RIGHT,
	ZANIM_END_RIGHT_R,
	ZANIM_END_RIGHT_COMBO,
	ZANIM_END_RIGHT_COMBO_R,
	ZANIM_END_LEFT,
	ZANIM_END_LEFT_R,
	ZANIM_END_LEFT_COMBO,
	ZANIM_END_LEFT_COMBO_R,
	ZANIM_END_STAB,
	ZANIM_END_STAB_R,
	ZANIM_END_STAB_COMBO,
	ZANIM_END_STAB_COMBO_R,
	ZANIM_END_SPIN,
	ZANIM_END_SPIN_R,
	ZANIM_END_BIG_SPIN,
	ZANIM_END_JUMPSLASH,
	ZANIM_IDLE_FREE,		// PLAYER_ANIMTYPE_0: nothing in his hands, or the sword without a shield
	ZANIM_WALK_FREE,		// (ZL_Typed); the ones above are PLAYER_ANIMTYPE_1, sword and shield out
	ZANIM_RUN_FREE,
	ZANIM_Z_IDLE_FREE,
	ZANIM_Z_SIDE_L_FREE,
	ZANIM_Z_SIDE_R_FREE,
	ZANIM_FALL_FREE,
	ZANIM_LAND_FREE,
	ZANIM_SHORT_LANDING_FREE,
	ZANIM_ROLL_FREE,
	ZANIM_BONK_FREE,
	ZANIM_FIDGET_LOOK,		// sFidgetAnimations: sword and shield in hand / otherwise (_F)
	ZANIM_FIDGET_LOOK_F,
	ZANIM_FIDGET_COLD,
	ZANIM_FIDGET_COLD_F,
	ZANIM_FIDGET_WARM,
	ZANIM_FIDGET_WARM_F,
	ZANIM_FIDGET_STRETCH,
	ZANIM_FIDGET_STRETCH_F,
	ZANIM_FIDGET_HEAT1,
	ZANIM_FIDGET_HEAT1_F,
	ZANIM_FIDGET_HEAT2,
	ZANIM_FIDGET_HEAT2_F,
	ZANIM_FIDGET_SWORD,
	ZANIM_FIDGET_TUNIC,
	ZANIM_FIDGET_TUNIC_F,
	ZANIM_FIDGET_FEET,
	ZANIM_FIDGET_FEET_F,
	ZANIM_FIDGET_SHIELD,
	ZANIM_COUNT
};

#define ZGAIT_AUTO	NULL		// legs pick walk / run / idle from speed
#define ZGAIT_NONE	""			// full body, no gait

struct zl_anim_def_t
{
	const char	*oot;			// gPlayerAnim_* in OoT
	const char	*seq[4];		// sequence names to try, OoT name first
	const char	*gait;			// HL leg sequence for stock models
	BOOL		loop;
	float		rate;			// OoT frames a frame when OoT plays it faster than PLAYER_ANIM_ADJUSTED_SPEED
};

#define ZRATE_ONCE	ZL_ANIM_RATE	// Player_AnimPlayOnce / PlayLoop / func_8083328C: playSpeed 1
#define ZRATE_HOLD	-1.0f			// stays on its first frame

static const zl_anim_def_t sAnims[ZANIM_COUNT] =
{
	{ "gPlayerAnim_link_normal_wait",				{ "link_normal_wait", "ref_aim_crowbar" },				ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_walk",				{ "link_normal_walk", "ref_aim_crowbar" },				ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_fighter_run",				{ "link_fighter_run", "ref_aim_crowbar" },				ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_anchor_waitR",				{ "link_anchor_waitR", "crouch_aim_crowbar" },			"deep_idle", TRUE },
	{ "gPlayerAnim_link_anchor_side_walkL",			{ "link_anchor_side_walkL", "ref_aim_crowbar" },		ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_anchor_side_walkR",			{ "link_anchor_side_walkR", "ref_aim_crowbar" },		ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_anchor_back_walk",			{ "link_anchor_back_walk", "ref_aim_crowbar" },			ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_landing_roll",		{ "link_normal_landing_roll", "longjump", "jump" },		ZGAIT_NONE, FALSE, ZL_ROLL_RATE },
	{ "gPlayerAnim_link_normal_hip_down",			{ "link_normal_hip_down", "die_backwards1", "die_backwards" }, ZGAIT_NONE, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_Lside_jump",		{ "link_fighter_Lside_jump", "jump" },					ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_fighter_Rside_jump",		{ "link_fighter_Rside_jump", "jump" },					ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_fighter_backturn_jump",		{ "link_fighter_backturn_jump", "longjump", "jump" },	ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_normal_run_jump",			{ "link_normal_run_jump", "jump" },						ZGAIT_NONE, FALSE },
	// Falling: Player_Action_8084411C turns to the first frame of landing once he's under where he
	// left the ground (or at once when he walked off)
	{ "gPlayerAnim_link_normal_landing",			{ "link_normal_landing", "jump" },						ZGAIT_NONE, FALSE, ZRATE_HOLD },
	{ "gPlayerAnim_link_normal_landing",			{ "link_normal_landing", "crouch_idle" },				ZGAIT_NONE, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_normal_150step_up",			{ "link_normal_150step_up", "crouch_idle", "jump" },		ZGAIT_NONE, FALSE, ZL_CLIMB_RATE },
	{ "gPlayerAnim_link_normal_100step_up",			{ "link_normal_100step_up", "crouch_idle", "jump" },		ZGAIT_NONE, FALSE, ZL_CLIMB_RATE },
	// Sword swings: D_80854190
	{ "gPlayerAnim_link_fighter_Lside_kiru",		{ "link_fighter_Lside_kiru", "ref_shoot_crowbar" },		ZGAIT_AUTO, FALSE },
	{ "gPlayerAnim_link_fighter_Rside_kiru",		{ "link_fighter_Rside_kiru", "ref_shoot_crowbar" },		ZGAIT_AUTO, FALSE },
	{ "gPlayerAnim_link_fighter_normal_kiru",		{ "link_fighter_normal_kiru", "ref_shoot_crowbar" },	ZGAIT_AUTO, FALSE },
	{ "gPlayerAnim_link_fighter_pierce_kiru",		{ "link_fighter_pierce_kiru", "ref_shoot_onehanded", "ref_shoot_crowbar" }, ZGAIT_AUTO, FALSE },
	{ "gPlayerAnim_link_fighter_Lside_kiru_finsh",	{ "link_fighter_Lside_kiru_finsh", "crouch_shoot_crowbar", "ref_shoot_crowbar" }, ZGAIT_AUTO, FALSE },
	{ "gPlayerAnim_link_fighter_Rside_kiru_finsh",	{ "link_fighter_Rside_kiru_finsh", "crouch_shoot_crowbar", "ref_shoot_crowbar" }, ZGAIT_AUTO, FALSE },
	{ "gPlayerAnim_link_fighter_normal_kiru_finsh",	{ "link_fighter_normal_kiru_finsh", "crouch_shoot_crowbar", "ref_shoot_crowbar" }, ZGAIT_AUTO, FALSE },
	{ "gPlayerAnim_link_fighter_pierce_kiru_finsh",	{ "link_fighter_pierce_kiru_finsh", "ref_shoot_onehanded", "ref_shoot_crowbar" }, ZGAIT_AUTO, FALSE },
	{ "gPlayerAnim_link_fighter_Lpower_jump_kiru",	{ "link_fighter_Lpower_jump_kiru", "ref_shoot_crowbar" }, "jump", FALSE },
	{ "gPlayerAnim_link_fighter_Lpower_jump_kiru_hit", { "link_fighter_Lpower_jump_kiru_hit", "crouch_shoot_crowbar" }, "crouch_idle", FALSE },
	{ "gPlayerAnim_link_fighter_power_kiru_wait",	{ "link_fighter_power_kiru_wait", "ref_aim_crowbar" },	"crouch_idle", TRUE },
	{ "gPlayerAnim_link_fighter_power_kiru_walk",	{ "link_fighter_power_kiru_walk", "ref_aim_crowbar" },	"crouchrun", TRUE },
	{ "gPlayerAnim_link_fighter_rolling_kiru",		{ "link_fighter_rolling_kiru", "ref_shoot_crowbar" },	"deep_idle", FALSE },
	{ "gPlayerAnim_link_fighter_Wrolling_kiru",		{ "link_fighter_Wrolling_kiru", "ref_shoot_crowbar" },	"deep_idle", FALSE },
	{ "gPlayerAnim_link_normal_defense_wait",		{ "link_normal_defense_wait", "crouch_aim_crowbar" },	"crouch_idle", TRUE },
	{ "gPlayerAnim_link_bow_bow_wait",				{ "link_bow_bow_wait", "ref_aim_bow" },					"deep_idle", TRUE },
	{ "gPlayerAnim_link_hook_wait",					{ "link_hook_wait", "ref_aim_onehanded", "ref_aim_bow" }, "deep_idle", TRUE },
	{ "gPlayerAnim_link_hook_fly_wait",				{ "link_hook_fly_wait", "longjump", "jump" },			ZGAIT_NONE, TRUE, ZRATE_ONCE },
	{ "gPlayerAnim_link_normal_front_shit",			{ "link_normal_front_shit", "ref_aim_crowbar" },		"crouch_idle", FALSE },
	{ "gPlayerAnim_link_normal_front_downA",		{ "link_normal_front_downA", "die_backwards1", "die_backwards" }, ZGAIT_NONE, FALSE },
	// Player_Action_Talk: talk_free once (hand to his side), then talk_free_wait loops (breathing)
	{ "gPlayerAnim_link_normal_talk_free",			{ "link_normal_talk_free", "ref_aim_crowbar" },			"deep_idle", FALSE },
	{ "gPlayerAnim_link_normal_talk_free_wait",		{ "link_normal_talk_free_wait", "ref_aim_crowbar" },	"deep_idle", TRUE },
	// Item get (Player_Action_8084E3C4): both play once and hold their last frame while the text
	// box is up. itemA comes after a chest, itemB when an item is picked up off the ground.
	{ "gPlayerAnim_link_demo_get_itemA",			{ "link_demo_get_itemA", "ref_aim_squeak", "ref_aim_trip" }, "deep_idle", FALSE },
	{ "gPlayerAnim_link_demo_get_itemB",			{ "link_demo_get_itemB", "ref_aim_squeak", "ref_aim_trip" }, "deep_idle", FALSE },
	// Opening a chest (ageProperties->unk_98)
	{ "gPlayerAnim_link_demo_Tbox_open",			{ "link_demo_Tbox_open", "ref_aim_crowbar" },			"crouch_idle", FALSE },
	// Hop landings (D_80853D4C): the backflip finishes its turn in backturn_jump_end
	{ "gPlayerAnim_link_fighter_Lside_jump_end",	{ "link_fighter_Lside_jump_end", "crouch_idle" },		ZGAIT_NONE, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_Lside_jump_endL",	{ "link_fighter_Lside_jump_endL", "crouch_idle" },		ZGAIT_NONE, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_Rside_jump_end",	{ "link_fighter_Rside_jump_end", "crouch_idle" },		ZGAIT_NONE, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_Rside_jump_endR",	{ "link_fighter_Rside_jump_endR", "crouch_idle" },		ZGAIT_NONE, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_backturn_jump_end",	{ "link_fighter_backturn_jump_end", "crouch_idle" },	ZGAIT_NONE, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_backturn_jump_endR", { "link_fighter_backturn_jump_endR", "crouch_idle" },	ZGAIT_NONE, FALSE, ZRATE_ONCE },
	// A "Put Away" (sItemChangeInfo[PLAYER_ITEM_CHG_5]); free2fighter_free when there's no shield
	// over the standing legs: whole body sequences zl_putaway / _noshield (make_link_model.py), since
	// Half-Life's gaitsequence split didn't show it
	{ "gPlayerAnim_link_normal_fighter2free",		{ "zl_putaway", "ref_aim_crowbar" },					ZGAIT_AUTO, FALSE, ZL_PUTAWAY_RATE },
	{ "gPlayerAnim_link_normal_free2fighter_free",	{ "zl_putaway_noshield", "ref_aim_crowbar" },			ZGAIT_AUTO, FALSE, ZL_PUTAWAY_RATE },
	// Carrying (Player_UpperAction_CarryActor): carryB_wait on the upper body only, the legs
	// keep running the normal *_free cycle (PLAYER_ANIMTYPE_5). make_link_model.py builds the
	// two together as whole body sequences (zl_carry_wait / walk / run), so the legs walk in step
	// with the footsteps like the normal run cycle. carryB is the lift itself.
	{ "gPlayerAnim_link_normal_carryB_wait",		{ "zl_carry_wait", "ref_aim_squeak", "ref_aim_trip" },	ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_carryB_wait",		{ "zl_carry_walk", "ref_aim_squeak", "ref_aim_trip" },	ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_carryB_wait",		{ "zl_carry_run", "ref_aim_squeak", "ref_aim_trip" },	ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_swimer_swim",				{ "link_swimer_swim", "swim" },							ZGAIT_NONE, TRUE },
	{ "gPlayerAnim_link_swimer_swim_wait",			{ "link_swimer_swim_wait", "treadwater" },				ZGAIT_NONE, TRUE },
	{ "gPlayerAnim_link_normal_jump",				{ "link_normal_jump", "jump" },							ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_normal_run_jump_end",		{ "link_normal_run_jump_end", "crouch_idle" },			ZGAIT_NONE, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_normal_short_landing",		{ "link_normal_short_landing", "crouch_idle" },			ZGAIT_NONE, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_anchor_landingR",			{ "link_anchor_landingR", "crouch_idle" },				ZGAIT_NONE, FALSE, ZRATE_ONCE },
	// Bringing the sword back after a swing: Player_Action_808502D0 plays it with func_8083A098
	// (playSpeed 1) as he goes back to standing, and moving off cuts it short
	{ "gPlayerAnim_link_fighter_normal_kiru_end",	{ "link_fighter_normal_kiru_end", "ref_aim_crowbar" },	ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_normal_kiru_endR",	{ "link_fighter_normal_kiru_endR", "ref_aim_crowbar" },	ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_normal_kiru_finsh_end", { "link_fighter_normal_kiru_finsh_end", "ref_aim_crowbar" }, ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_anchor_normal_kiru_finsh_endR", { "link_anchor_normal_kiru_finsh_endR", "ref_aim_crowbar" }, ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_Lside_kiru_end",	{ "link_fighter_Lside_kiru_end", "ref_aim_crowbar" },	ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_anchor_Lside_kiru_endR",	{ "link_anchor_Lside_kiru_endR", "ref_aim_crowbar" },	ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_Lside_kiru_finsh_end", { "link_fighter_Lside_kiru_finsh_end", "ref_aim_crowbar" }, ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_anchor_Lside_kiru_finsh_endR", { "link_anchor_Lside_kiru_finsh_endR", "ref_aim_crowbar" }, ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_Rside_kiru_end",	{ "link_fighter_Rside_kiru_end", "ref_aim_crowbar" },	ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_anchor_Rside_kiru_endR",	{ "link_anchor_Rside_kiru_endR", "ref_aim_crowbar" },	ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_Rside_kiru_finsh_end", { "link_fighter_Rside_kiru_finsh_end", "ref_aim_crowbar" }, ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_anchor_Rside_kiru_finsh_endR", { "link_anchor_Rside_kiru_finsh_endR", "ref_aim_crowbar" }, ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_pierce_kiru_end",	{ "link_fighter_pierce_kiru_end", "ref_aim_crowbar" },	ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_anchor_pierce_kiru_endR",	{ "link_anchor_pierce_kiru_endR", "ref_aim_crowbar" },	ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_pierce_kiru_finsh_end", { "link_fighter_pierce_kiru_finsh_end", "ref_aim_crowbar" }, ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_anchor_pierce_kiru_finsh_endR", { "link_anchor_pierce_kiru_finsh_endR", "ref_aim_crowbar" }, ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_rolling_kiru_end",	{ "link_fighter_rolling_kiru_end", "ref_aim_crowbar" },	ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_anchor_rolling_kiru_endR",	{ "link_anchor_rolling_kiru_endR", "ref_aim_crowbar" },	ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_Wrolling_kiru_end",	{ "link_fighter_Wrolling_kiru_end", "ref_aim_crowbar" },	ZGAIT_AUTO, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_fighter_power_jump_kiru_end", { "link_fighter_power_jump_kiru_end", "crouch_idle" },	ZGAIT_NONE, FALSE, ZRATE_ONCE },
	// PLAYER_ANIMTYPE_0 (D_80853914's first column): the sword away, or out without a shield
	{ "gPlayerAnim_link_normal_wait_free",			{ "link_normal_wait_free", "ref_aim_crowbar" },			ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_walk_free",			{ "link_normal_walk_free", "ref_aim_crowbar" },			ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_run_free",			{ "link_normal_run_free", "ref_aim_crowbar" },			ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_waitR_free",			{ "link_normal_waitR_free", "crouch_aim_crowbar" },		"deep_idle", TRUE },
	{ "gPlayerAnim_link_normal_side_walkL_free",	{ "link_normal_side_walkL_free", "ref_aim_crowbar" },	ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_side_walkR_free",	{ "link_normal_side_walkR_free", "ref_aim_crowbar" },	ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_landing_free",		{ "link_normal_landing_free", "jump" },					ZGAIT_NONE, FALSE, ZRATE_HOLD },
	{ "gPlayerAnim_link_normal_landing_free",		{ "link_normal_landing_free", "crouch_idle" },			ZGAIT_NONE, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_normal_short_landing_free",	{ "link_normal_short_landing_free", "crouch_idle" },	ZGAIT_NONE, FALSE, ZRATE_ONCE },
	{ "gPlayerAnim_link_normal_landing_roll_free",	{ "link_normal_landing_roll_free", "longjump", "jump" },	ZGAIT_NONE, FALSE, ZL_ROLL_RATE },
	{ "gPlayerAnim_link_normal_hip_down_free",		{ "link_normal_hip_down_free", "die_backwards1", "die_backwards" }, ZGAIT_NONE, FALSE, ZRATE_ONCE },
	// Idle fidgets (sFidgetAnimations), played once at PLAYER_ANIM_ADJUSTED_SPEED
	{ "gPlayerAnim_link_normal_wait_typeA_20f",		{ "link_normal_wait_typeA_20f", "deep_idle" },			ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_normal_waitF_typeA_20f",	{ "link_normal_waitF_typeA_20f", "deep_idle" },			ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_normal_wait_typeC_20f",		{ "link_normal_wait_typeC_20f", "deep_idle" },			ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_normal_waitF_typeC_20f",	{ "link_normal_waitF_typeC_20f", "deep_idle" },			ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_normal_wait_typeB_20f",		{ "link_normal_wait_typeB_20f", "deep_idle" },			ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_normal_waitF_typeB_20f",	{ "link_normal_waitF_typeB_20f", "deep_idle" },			ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_wait_typeD_20f",			{ "link_wait_typeD_20f", "deep_idle" },					ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_waitF_typeD_20f",			{ "link_waitF_typeD_20f", "deep_idle" },				ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_wait_heat1_20f",			{ "link_wait_heat1_20f", "deep_idle" },					ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_waitF_heat1_20f",			{ "link_waitF_heat1_20f", "deep_idle" },				ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_wait_heat2_20f",			{ "link_wait_heat2_20f", "deep_idle" },					ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_waitF_heat2_20f",			{ "link_waitF_heat2_20f", "deep_idle" },				ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_wait_itemD1_20f",			{ "link_wait_itemD1_20f", "deep_idle" },				ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_wait_itemA_20f",			{ "link_wait_itemA_20f", "deep_idle" },					ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_waitF_itemA_20f",			{ "link_waitF_itemA_20f", "deep_idle" },				ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_wait_itemB_20f",			{ "link_wait_itemB_20f", "deep_idle" },					ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_waitF_itemB_20f",			{ "link_waitF_itemB_20f", "deep_idle" },				ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_wait_itemC_20f",			{ "link_wait_itemC_20f", "deep_idle" },					ZGAIT_NONE, FALSE },
};

struct zl_anim_cache_t
{
	int modelindex;
	int seq[ZANIM_COUNT];
	BOOL full[ZANIM_COUNT];		// found an OoT named sequence: no gait
	int gait[ZANIM_COUNT];
	int gaitIdle, gaitWalk, gaitRun, gaitCrouch;
};

static zl_anim_cache_t sAnimCache;

static void ZL_BuildAnimCache( CBaseAnimating *pEnt )
{
	void *pmodel = GET_MODEL_PTR( ENT( pEnt->pev ) );

	sAnimCache.modelindex = pEnt->pev->modelindex;

	for ( int i = 0; i < ZANIM_COUNT; i++ )
	{
		const zl_anim_def_t &def = sAnims[i];
		sAnimCache.seq[i] = 0;
		sAnimCache.full[i] = FALSE;

		for ( int j = 0; j < 4 && def.seq[j]; j++ )
		{
			// Studio sequence labels hold 31 characters; make_link_model.py cuts longer OoT names
			char label[32];
			strncpy( label, def.seq[j], sizeof( label ) - 1 );
			label[sizeof( label ) - 1] = 0;
			int s = LookupSequence( pmodel, label );
			if ( s >= 0 )
			{
				sAnimCache.seq[i] = s;
				sAnimCache.full[i] = ( j == 0 );
				break;
			}
		}

		if ( def.gait == ZGAIT_AUTO )
			sAnimCache.gait[i] = -1;
		else if ( def.gait[0] == 0 )
			sAnimCache.gait[i] = 0;
		else
			sAnimCache.gait[i] = max( 0, LookupSequence( pmodel, def.gait ) );
	}

	sAnimCache.gaitIdle = max( 0, LookupSequence( pmodel, "deep_idle" ) );
	sAnimCache.gaitWalk = max( 0, LookupSequence( pmodel, "walk" ) );
	sAnimCache.gaitRun = max( 0, LookupSequence( pmodel, "run" ) );
	sAnimCache.gaitCrouch = max( 0, LookupSequence( pmodel, "crouch_idle" ) );
}

// A melee attack's swing (D_80854190 unk_00)
static int ZL_SwingAnim( int act, int mwa )
{
	if ( act == ZACT_SPIN )
		return mwa == ZMWA_BIG_SPIN_1H ? ZANIM_BIG_SPIN : ZANIM_SPIN;
	if ( act == ZACT_JUMPSLASH_END )
		return ZANIM_JUMPSLASH_END;

	switch ( mwa )
	{
	case ZMWA_RIGHT_SLASH_1H:	return ZANIM_SLASH_RIGHT;
	case ZMWA_LEFT_SLASH_1H:	return ZANIM_SLASH_LEFT;
	case ZMWA_FORWARD_SLASH_1H:	return ZANIM_SLASH_FORWARD;
	case ZMWA_STAB_1H:			return ZANIM_STAB;
	case ZMWA_RIGHT_COMBO_1H:	return ZANIM_SLASH_RIGHT_COMBO;
	case ZMWA_LEFT_COMBO_1H:	return ZANIM_SLASH_LEFT_COMBO;
	case ZMWA_FORWARD_COMBO_1H:	return ZANIM_SLASH_FORWARD_COMBO;
	case ZMWA_STAB_COMBO_1H:	return ZANIM_STAB_COMBO;
	default:					return ZANIM_SLASH_RIGHT;
	}
}

// ... and the sword coming back after it (unk_04, or unk_08 when locked onto an enemy), 0 if none
static int ZL_SwingEndAnim( int swing, BOOL hostile )
{
	switch ( swing )
	{
	case ZANIM_SLASH_FORWARD:		return hostile ? ZANIM_END_FORWARD_R : ZANIM_END_FORWARD;
	case ZANIM_SLASH_FORWARD_COMBO:	return hostile ? ZANIM_END_FORWARD_COMBO_R : ZANIM_END_FORWARD_COMBO;
	case ZANIM_SLASH_RIGHT:			return hostile ? ZANIM_END_RIGHT_R : ZANIM_END_RIGHT;
	case ZANIM_SLASH_RIGHT_COMBO:	return hostile ? ZANIM_END_RIGHT_COMBO_R : ZANIM_END_RIGHT_COMBO;
	case ZANIM_SLASH_LEFT:			return hostile ? ZANIM_END_LEFT_R : ZANIM_END_LEFT;
	case ZANIM_SLASH_LEFT_COMBO:	return hostile ? ZANIM_END_LEFT_COMBO_R : ZANIM_END_LEFT_COMBO;
	case ZANIM_STAB:				return hostile ? ZANIM_END_STAB_R : ZANIM_END_STAB;
	case ZANIM_STAB_COMBO:			return hostile ? ZANIM_END_STAB_COMBO_R : ZANIM_END_STAB_COMBO;
	case ZANIM_SPIN:				return hostile ? ZANIM_END_SPIN_R : ZANIM_END_SPIN;
	case ZANIM_BIG_SPIN:			return hostile ? ZANIM_END_SPIN_R : ZANIM_END_BIG_SPIN;
	case ZANIM_JUMPSLASH_END:		return ZANIM_END_JUMPSLASH;
	}
	return 0;
}

static BOOL ZL_IsSwingEnd( int anim )
{
	return anim >= ZANIM_END_FORWARD && anim <= ZANIM_END_JUMPSLASH;
}

/*
 Which set of animations (sPlayerModelTypes' anim type): with the sword and the shield in his hands
 Link stands and runs ready to fight (PLAYER_ANIMTYPE_1, link_normal_wait, fighter_run); with them
 put away, or the sword out on its own, relaxed (PLAYER_ANIMTYPE_0, the *_free ones).
*/
static BOOL ZL_SwordAndShield( CLinkPlayer *pLink )
{
	return pLink->m_fZSwordOut && ( pLink->pev->iuser3 & ZSF_HASSHIELD );
}

static int ZL_Typed( CLinkPlayer *pLink, int anim )
{
	if ( ZL_SwordAndShield( pLink ) )
		return anim;

	switch ( anim )
	{
	case ZANIM_IDLE:			return ZANIM_IDLE_FREE;
	case ZANIM_WALK:			return ZANIM_WALK_FREE;
	case ZANIM_RUN:				return ZANIM_RUN_FREE;
	case ZANIM_Z_IDLE:			return ZANIM_Z_IDLE_FREE;
	case ZANIM_Z_SIDE_L:		return ZANIM_Z_SIDE_L_FREE;
	case ZANIM_Z_SIDE_R:		return ZANIM_Z_SIDE_R_FREE;
	case ZANIM_FALL:			return ZANIM_FALL_FREE;
	case ZANIM_LAND:			return ZANIM_LAND_FREE;
	case ZANIM_SHORT_LANDING:	return ZANIM_SHORT_LANDING_FREE;
	case ZANIM_ROLL:			return ZANIM_ROLL_FREE;
	case ZANIM_BONK:			return ZANIM_BONK_FREE;
	}
	return anim;
}

static BOOL ZL_IsFidget( int anim )
{
	return anim >= ZANIM_FIDGET_LOOK && anim <= ZANIM_FIDGET_SHIELD;
}

static BOOL ZL_IsIdle( int anim )
{
	return anim == ZANIM_IDLE || anim == ZANIM_IDLE_FREE || ZL_IsFidget( anim );
}

// Health_IsCritical: a heart or less up to 5 heart containers, 1.5 up to 10, 2 up to 15, else 2.75
static BOOL ZL_HealthCritical( CLinkPlayer *pLink )
{
	int cap = pLink->m_iZHealthCapacity;
	int health = (int)pLink->pev->health;
	int crit = cap <= 0x50 ? 0x10 : ( cap <= 0xA0 ? 0x18 : ( cap <= 0xF0 ? 0x20 : 0x2C ) );
	return health > 0 && health <= crit;
}

// FidgetType (z_player.c), and sFidgetAnimations: [sword and shield in hand, otherwise]
enum
{
	ZFIDGET_LOOK_AROUND, ZFIDGET_COLD, ZFIDGET_WARM, ZFIDGET_HOT, ZFIDGET_STRETCH_1, ZFIDGET_STRETCH_2,
	ZFIDGET_STRETCH_3, ZFIDGET_CRIT_HEALTH_START, ZFIDGET_CRIT_HEALTH_LOOP, ZFIDGET_SWORD_SWING,
	ZFIDGET_ADJUST_TUNIC, ZFIDGET_TAP_FEET, ZFIDGET_ADJUST_SHIELD,
};

static const int s_fidgetAnims[][2] =
{
	{ ZANIM_FIDGET_LOOK,	ZANIM_FIDGET_LOOK_F },
	{ ZANIM_FIDGET_COLD,	ZANIM_FIDGET_COLD_F },
	{ ZANIM_FIDGET_WARM,	ZANIM_FIDGET_WARM_F },
	{ ZANIM_FIDGET_WARM,	ZANIM_FIDGET_WARM_F },
	{ ZANIM_FIDGET_STRETCH,	ZANIM_FIDGET_STRETCH_F },
	{ ZANIM_FIDGET_STRETCH,	ZANIM_FIDGET_STRETCH_F },
	{ ZANIM_FIDGET_STRETCH,	ZANIM_FIDGET_STRETCH_F },
	{ ZANIM_FIDGET_HEAT1,	ZANIM_FIDGET_HEAT1_F },
	{ ZANIM_FIDGET_HEAT2,	ZANIM_FIDGET_HEAT2_F },
	{ ZANIM_FIDGET_SWORD,	ZANIM_FIDGET_SWORD },
	{ ZANIM_FIDGET_TUNIC,	ZANIM_FIDGET_TUNIC_F },
	{ ZANIM_FIDGET_FEET,	ZANIM_FIDGET_FEET_F },
	{ ZANIM_FIDGET_SHIELD,	ZANIM_FIDGET_SHIELD },
};

/*
 Standing still (Player_Action_Idle): each idle animation plays once, then
 Player_ChooseNextIdleAnim picks the next. idleType flips every time, so it goes the plain idle,
 a fidget, the plain idle... At critical health it's the panting ones every time. A fidget is one
 of the four common ones 4 times in 5 (the sword twirl and the shield only with both in hand),
 else the room's own (zl_room_env, environmentType: looking around in an ordinary room).
*/
static int ZL_IdleAnim( CLinkPlayer *pLink )
{
	BOOL fighter = ZL_SwordAndShield( pLink );
	int plain = fighter ? ZANIM_IDLE : ZANIM_IDLE_FREE;

	// Just stopped: the plain idle
	if ( !ZL_IsIdle( pLink->m_iZAnim ) || !pLink->m_iZIdleAnim )
		return pLink->m_iZIdleAnim = plain;
	if ( pLink->m_iZAnim != pLink->m_iZIdleAnim || !pLink->m_fSequenceFinished )
		return pLink->m_iZIdleAnim;

	int next;
	BOOL crit = ZL_HealthCritical( pLink );
	if ( !crit && ( ( pLink->m_iZIdleType = ( pLink->m_iZIdleType + 1 ) & 1 ) != 0 ) )
	{
		next = plain;
	}
	else
	{
		int fidget = (int)zl_room_env.value;
		if ( fidget < ZFIDGET_LOOK_AROUND || fidget > ZFIDGET_STRETCH_3 )
			fidget = ZFIDGET_LOOK_AROUND;

		if ( crit )
		{
			if ( pLink->m_iZIdleType >= 0 )
			{
				fidget = ZFIDGET_CRIT_HEALTH_START;
				pLink->m_iZIdleType = -1;
			}
			else
				fidget = ZFIDGET_CRIT_HEALTH_LOOP;
		}
		else
		{
			int common = RANDOM_LONG( 0, 4 );
			if ( common < 4 && ( ( common != 0 && common != 3 ) || fighter ) )
				fidget = ZFIDGET_SWORD_SWING + common;
		}
		next = s_fidgetAnims[fidget][fighter ? 0 : 1];
	}

	pLink->m_fZIdleRestart = TRUE;
	return pLink->m_iZIdleAnim = next;
}

// sFidgetAnimSfxLists: what each fidget sounds like, on its frames
static const struct { int anim, alt; int sound; int channel; float frame; } s_fidgetSfx[] =
{
	{ ZANIM_FIDGET_COLD,	ZANIM_FIDGET_COLD_F,	ZS_VO_SNEEZE,			CHAN_VOICE,	8 },
	{ ZANIM_FIDGET_WARM,	ZANIM_FIDGET_WARM_F,	ZS_VO_SWEAT,			CHAN_VOICE,	18 },
	{ ZANIM_FIDGET_STRETCH,	ZANIM_FIDGET_STRETCH_F,	ZS_VO_RELAX,			CHAN_VOICE,	20 },
	{ ZANIM_FIDGET_HEAT1,	ZANIM_FIDGET_HEAT1_F,	ZS_VO_BREATH_REST,		CHAN_VOICE,	13 },
	{ ZANIM_FIDGET_HEAT2,	ZANIM_FIDGET_HEAT2_F,	ZS_VO_BREATH_REST,		CHAN_VOICE,	10 },
	{ ZANIM_FIDGET_SWORD,	ZANIM_FIDGET_SWORD,		ZS_IT_HAMMER_SWING,		CHAN_ITEM,	10 },
	{ ZANIM_FIDGET_SWORD,	ZANIM_FIDGET_SWORD,		ZS_VO_AUTO_JUMP,		CHAN_VOICE,	10 },
	{ ZANIM_FIDGET_SWORD,	ZANIM_FIDGET_SWORD,		ZS_IT_SWORD_SWING,		CHAN_ITEM,	22 },
	{ ZANIM_FIDGET_SWORD,	ZANIM_FIDGET_SWORD,		ZS_VO_SWORD_N,			CHAN_VOICE,	22 },
	{ ZANIM_FIDGET_TUNIC,	ZANIM_FIDGET_TUNIC_F,	ZS_PL_CALM_HIT,			CHAN_BODY,	44 },
	{ ZANIM_FIDGET_TUNIC,	ZANIM_FIDGET_TUNIC_F,	ZS_PL_CALM_HIT,			CHAN_BODY,	48 },
	{ ZANIM_FIDGET_TUNIC,	ZANIM_FIDGET_TUNIC_F,	ZS_PL_CALM_HIT,			CHAN_BODY,	52 },
	{ ZANIM_FIDGET_TUNIC,	ZANIM_FIDGET_TUNIC_F,	ZS_PL_CALM_HIT,			CHAN_BODY,	56 },
	{ ZANIM_FIDGET_TUNIC,	ZANIM_FIDGET_TUNIC_F,	ZS_PL_CALM_HIT,			CHAN_BODY,	60 },
	{ ZANIM_FIDGET_FEET,	ZANIM_FIDGET_FEET_F,	ZS_PL_WALK_GROUND,		CHAN_BODY,	25 },
	{ ZANIM_FIDGET_FEET,	ZANIM_FIDGET_FEET_F,	ZS_PL_WALK_GROUND,		CHAN_BODY,	30 },
	{ ZANIM_FIDGET_FEET,	ZANIM_FIDGET_FEET_F,	ZS_PL_WALK_GROUND,		CHAN_BODY,	44 },
	{ ZANIM_FIDGET_FEET,	ZANIM_FIDGET_FEET_F,	ZS_PL_WALK_GROUND,		CHAN_BODY,	48 },
	{ ZANIM_FIDGET_FEET,	ZANIM_FIDGET_FEET_F,	ZS_PL_WALK_GROUND,		CHAN_BODY,	52 },
	{ ZANIM_FIDGET_FEET,	ZANIM_FIDGET_FEET_F,	ZS_PL_WALK_GROUND,		CHAN_BODY,	56 },
	{ ZANIM_FIDGET_SHIELD,	ZANIM_FIDGET_SHIELD,	ZS_IT_SHIELD_POSTURE,	CHAN_ITEM,	16 },
	{ ZANIM_FIDGET_SHIELD,	ZANIM_FIDGET_SHIELD,	ZS_IT_SHIELD_POSTURE,	CHAN_ITEM,	20 },
	{ ZANIM_FIDGET_SHIELD,	ZANIM_FIDGET_SHIELD,	ZS_IT_SHIELD_POSTURE,	CHAN_ITEM,	70 },
};

static int ZL_PickAnimBase( CLinkPlayer *pLink );

static int ZL_PickAnim( CLinkPlayer *pLink )
{
	return ZL_Typed( pLink, ZL_PickAnimBase( pLink ) );
}

static int ZL_PickAnimBase( CLinkPlayer *pLink )
{
	int state = pLink->ZState();
	int act = ZL_GetAction( state );
	int mwa = ZL_GetMWA( state );
	Vector vel = pLink->pev->velocity;
	vel.z = 0;
	float speed = vel.Length();

	if ( pLink->pev->waterlevel >= 2 )
		return speed > 20 ? ZANIM_SWIM : ZANIM_SWIM_IDLE;

	switch ( act )
	{
	case ZACT_ROLL:			return ZANIM_ROLL;
	case ZACT_BONK:			return ZANIM_BONK;
	case ZACT_SIDEHOP:		return ZL_GetSub( state ) == ZDIR_LEFT ? ZANIM_SIDEHOP_L : ZANIM_SIDEHOP_R;
	case ZACT_BACKFLIP:		return ZANIM_BACKFLIP;
	case ZACT_JUMP:
		// Player_Action_8084411C: coming down under where he jumped from, he holds the first
		// frame of landing (fallDistance > 0)
		if ( pLink->pev->velocity.z < 0 && pLink->pev->origin.z < pLink->m_flZAirStart - 1.0f )
			return ZANIM_FALL;
		return ZL_GetSub( state ) == ZJUMP_NORMAL ? ZANIM_JUMP_NORMAL : ZANIM_JUMP;
	case ZACT_FALL:			return ZANIM_FALL;
	case ZACT_LAND:
	{
		// Out of a hop: its own landing, the battle one when locked onto an enemy
		BOOL lock = ( pLink->pev->iuser3 & ZSF_HOSTILE ) != 0;
		switch ( mwa - 1 )
		{
		case ZDIR_LEFT:		return lock ? ZANIM_HOP_LAND_L_LOCK : ZANIM_HOP_LAND_L;
		case ZDIR_RIGHT:	return lock ? ZANIM_HOP_LAND_R_LOCK : ZANIM_HOP_LAND_R;
		case ZDIR_BACKWARD:	return lock ? ZANIM_FLIP_LAND_LOCK : ZANIM_FLIP_LAND;
		default:			return ZANIM_LAND;
		}
	}
	case ZACT_CLIMB:		return ZL_GetSub( state ) ? ZANIM_CLIMB : ZANIM_CLIMB_LOW;
	case ZACT_JUMPSLASH:	return ZANIM_JUMPSLASH;
	case ZACT_SPIN_CHARGE:	return speed > 10 ? ZANIM_SPIN_CHARGE_WALK : ZANIM_SPIN_CHARGE;
	case ZACT_SHIELD:		return ZANIM_SHIELD;
	case ZACT_HOOKFLY:		return ZANIM_HOOKFLY;
	case ZACT_DAMAGE:		return ZANIM_DAMAGE;
	case ZACT_KNOCKDOWN:	return ZANIM_KNOCKDOWN;
	case ZACT_TALK:
		// talk_free plays once, then its wait loop takes over
		if ( pLink->m_iZAnim == ZANIM_TALK_WAIT || ( pLink->m_iZAnim == ZANIM_TALK && pLink->m_fSequenceFinished ) )
			return ZANIM_TALK_WAIT;
		return ZANIM_TALK;
	case ZACT_GETITEM:		return ZL_GetSub( state ) == 1 ? ZANIM_GETITEM : ZANIM_GETITEM_B;
	case ZACT_CHEST:		return ZANIM_CHEST;

	case ZACT_AIM:
	{
		int item = pLink->m_iZHeldItem;
		return ( item == ZITEM_HOOKSHOT || item == ZITEM_LONGSHOT ) ? ZANIM_AIM_HOOK : ZANIM_AIM_BOW;
	}

	case ZACT_SLASH:
	case ZACT_SPIN:
	case ZACT_JUMPSLASH_END:
	{
		// The swing plays once and the sword comes back with the end animation, which carries
		// on once he's standing again (ZAnimate). A new swing starts its action over, and no
		// swing is over in under 0.15 seconds (pierce_kiru, the shortest, is 4 frames).
		int swing = ZL_SwingAnim( act, mwa );
		int end = ZL_SwingEndAnim( swing, ( pLink->pev->iuser3 & ZSF_HOSTILE ) != 0 );
		if ( !end || pLink->ZActionTime() < 0.15f )
			return swing;
		if ( pLink->m_iZAnim == ZL_SwingEndAnim( swing, TRUE ) || pLink->m_iZAnim == ZL_SwingEndAnim( swing, FALSE ) )
			return pLink->m_iZAnim;
		if ( pLink->m_iZAnim == swing && pLink->m_fSequenceFinished )
			return end;
		return swing;
	}
	}

	// Standing / moving
	if ( pLink->m_flZPutAwayStart > 0 && gpGlobals->time - pLink->m_flZPutAwayStart < ZL_PUTAWAY_TIME && speed < 10 )
		return ( pLink->pev->iuser3 & ZSF_HASSHIELD ) ? ZANIM_PUTAWAY : ZANIM_PUTAWAY_NOSHIELD;

	if ( pLink->m_hZHeldBomb != NULL )
	{
		if ( speed < 10 )
			return ZANIM_CARRY;
		return speed < ZL_VEL( ZL_RUN_ANIM_SPEED ) ? ZANIM_CARRY_WALK : ZANIM_CARRY_RUN;
	}

	// Just landed, or bringing the sword back: that plays until it ends or he moves off
	// (Player_Action_Idle goes to running as soon as the stick asks for speed; slowing to a stop
	// isn't moving off). A swing still going when the attack ended is followed by its end.
	if ( pLink->m_iZLandAnim )
	{
		BOOL done = pLink->m_iZAnim == pLink->m_iZLandAnim && pLink->m_fSequenceFinished;
		if ( done && pLink->m_iZLandNext )
		{
			pLink->m_iZLandAnim = pLink->m_iZLandNext;
			pLink->m_iZLandNext = 0;
			done = FALSE;
		}
		BOOL moving = speed >= 10 && speed > pLink->m_flZLastSpeed - 0.5f;
		if ( !done && !moving && act == ZACT_STAND && !( state & ZL_STATE_SHIELDING ) )
			return pLink->m_iZLandAnim;
		pLink->m_iZLandAnim = 0;
		pLink->m_iZLandNext = 0;
	}

	if ( pLink->m_iZHeldItem == ZITEM_BOW && ( state & ( ZL_STATE_LOCKED | ZL_STATE_PARALLEL ) ) )
		return ZANIM_AIM_BOW;
	if ( ( pLink->m_iZHeldItem == ZITEM_HOOKSHOT || pLink->m_iZHeldItem == ZITEM_LONGSHOT ) && ( state & ( ZL_STATE_LOCKED | ZL_STATE_PARALLEL ) ) )
		return ZANIM_AIM_HOOK;

	if ( state & ZL_STATE_SHIELDING )
		return ZANIM_SHIELD;

	if ( state & ( ZL_STATE_LOCKED | ZL_STATE_PARALLEL ) )
	{
		if ( speed < 10 )
			return ZANIM_Z_IDLE;

		float rel = UTIL_AngleDiff( UTIL_VecToYaw( vel ), pLink->ZFacing() );
		if ( rel > 45 && rel < 135 )
			return ZANIM_Z_SIDE_L;
		if ( rel < -45 && rel > -135 )
			return ZANIM_Z_SIDE_R;
		if ( fabs( rel ) >= 135 )
			return ZANIM_Z_BACK;
	}

	if ( speed < 10 )
		return ZL_IdleAnim( pLink );
	if ( speed < ZL_VEL( ZL_RUN_ANIM_SPEED ) )
		return ZANIM_WALK;
	return ZANIM_RUN;
}

static BOOL ZL_IsCycleAnim( int anim )
{
	switch ( anim )
	{
	case ZANIM_WALK:
	case ZANIM_RUN:
	case ZANIM_Z_SIDE_L:
	case ZANIM_Z_SIDE_R:
	case ZANIM_WALK_FREE:
	case ZANIM_RUN_FREE:
	case ZANIM_Z_SIDE_L_FREE:
	case ZANIM_Z_SIDE_R_FREE:
	case ZANIM_Z_BACK:
	case ZANIM_SPIN_CHARGE_WALK:
	case ZANIM_CARRY_WALK:
	case ZANIM_CARRY_RUN:
		return TRUE;
	default:
		return FALSE;
	}
}

void CLinkPlayer::ZAnimate( void )
{
	if ( pev->modelindex != sAnimCache.modelindex )
		ZL_BuildAnimCache( this );

	// Out of the air: which landing (Player_Action_8084411C, func_8083A098). The hops, hard
	// landings and the roll are their own actions.
	int act = ZAction();
	int prevAct = m_iZAnimState >= 0 ? ( m_iZAnimState >> 12 ) : ZACT_NONE;
	if ( act == ZACT_JUMP || act == ZACT_FALL )
	{
		m_flZAirStart = pev->vuser3.z;
		m_iZLandAnim = 0;
		m_iZLandNext = 0;
	}
	else if ( act == ZACT_STAND && ( prevAct == ZACT_JUMP || prevAct == ZACT_FALL ) )
	{
		float fall = m_flZAirStart - pev->origin.z;

		if ( m_iZAnim == ZANIM_JUMP )
			m_iZLandAnim = ZANIM_RUN_JUMP_END;		// never came down past where he jumped from
		else if ( ( pev->iuser4 & ZL_STATE_LOCKED ) && ( pev->iuser3 & ZSF_HOSTILE ) )
			m_iZLandAnim = ZANIM_ANCHOR_LANDING;
		else if ( fall <= ZL_SHORT_LANDING_DIST )
			m_iZLandAnim = ZL_Typed( this, ZANIM_SHORT_LANDING );
		else
			m_iZLandAnim = ZL_Typed( this, ZANIM_LAND );
	}
	else if ( act == ZACT_STAND && prevAct == ZACT_ROLL && ( m_iZAnim == ZANIM_ROLL || m_iZAnim == ZANIM_ROLL_FREE ) )
	{
		// Player_Action_Roll hands over to standing on frame 20 of 32; the rest plays out unless
		// he moves off (func_8083A060)
		m_iZLandAnim = m_iZAnim;
		m_iZLandNext = 0;
	}
	else if ( act == ZACT_STAND && ( prevAct == ZACT_SLASH || prevAct == ZACT_SPIN || prevAct == ZACT_JUMPSLASH_END ) )
	{
		// The attack is over but the sword is still coming back
		int end = ZL_SwingEndAnim( m_iZAnim, ( pev->iuser3 & ZSF_HOSTILE ) != 0 );
		m_iZLandNext = 0;
		if ( ZL_IsSwingEnd( m_iZAnim ) )
			m_iZLandAnim = m_iZAnim;
		else if ( end && m_fSequenceFinished )
			m_iZLandAnim = end;
		else if ( end )
		{
			m_iZLandAnim = m_iZAnim;
			m_iZLandNext = end;
		}
	}

	int anim = ZL_PickAnim( this );
	const zl_anim_def_t &def = sAnims[anim];

	// One shots restart when the action restarts (combos, chained rolls)
	// ... but not what carries on from the attack into standing
	int key = anim | ( ZL_GetCombo( pev->iuser4 ) << 8 ) | ( ZAction() << 12 );
	BOOL carryOn = ( anim == m_iZAnim && anim == m_iZLandAnim );
	BOOL restart = !carryOn && ( ( key != m_iZAnimState ) || ( !def.loop && ZActionTime() < 0.02f && m_iZAnim == anim ) );
	if ( m_fZIdleRestart )
	{
		// The next idle, even when it's the same one again
		restart = TRUE;
		m_fZIdleRestart = FALSE;
	}
	if ( restart )
		m_flZIdleFrame = -1.0f;

	m_iZAnimState = key;
	m_iZAnim = anim;

	// Legs
	Vector vel = pev->velocity;
	vel.z = 0;
	float speed = vel.Length();

	m_flZLastSpeed = speed;

	if ( sAnimCache.full[anim] )
	{
		pev->gaitsequence = 0;
	}
	else if ( sAnimCache.gait[anim] >= 0 )
	{
		pev->gaitsequence = sAnimCache.gait[anim];
	}
	else
	{
		if ( speed > ZL_VEL( 3.5f ) )
			pev->gaitsequence = sAnimCache.gaitRun;
		else if ( speed > 5 )
			pev->gaitsequence = sAnimCache.gaitWalk;
		else
			pev->gaitsequence = sAnimCache.gaitIdle;
	}

	int seq = sAnimCache.seq[anim];

	if ( restart || pev->sequence != seq )
	{
		pev->sequence = seq;
		pev->frame = 0;
		ResetSequenceInfo();

		if ( zl_anim_debug.value )
			ALERT( at_console, "zl_anim: %s (sequence %d%s), action %d, speed %.0f\n", def.seq[0], seq,
				sAnimCache.full[anim] ? "" : ", stand-in", ZAction(), speed );

		if ( sAnimCache.full[anim] && def.rate == ZRATE_HOLD )
			pev->framerate = 0;
		else if ( sAnimCache.full[anim] && def.rate > 0 )
			pev->framerate = def.rate;

		// Fit HL's stand-in one shots to the OoT action length so they don't loop or cut off
		if ( !sAnimCache.full[anim] && !def.loop )
		{
			float t = 0;
			switch ( ZAction() )
			{
			case ZACT_SLASH:	t = ZL_SLASH_TIME; break;
			case ZACT_ROLL:		t = ZL_ROLL_TIME; break;
			case ZACT_SPIN:		t = ZL_SpinTime( ZL_GetMWA( pev->iuser4 ) ); break;
			case ZACT_LAND:		t = ZL_LAND_TIME * 2; break;
			case ZACT_DAMAGE:	t = ZL_DAMAGE_TIME; break;
			case ZACT_KNOCKDOWN: t = ZL_KNOCKDOWN_TIME * 0.6f; break;
			case ZACT_CLIMB:	t = ZL_ClimbTime( ZL_GetSub( pev->iuser4 ) ); break;
			case ZACT_CHEST:	t = ZL_CHEST_TIME; break;
			}

			if ( t > 0 )
			{
				void *pmodel = GET_MODEL_PTR( ENT( pev ) );
				float frameRate, groundSpeed;
				GetSequenceInfo( pmodel, pev, &frameRate, &groundSpeed );
				// frameRate is frames per second of the 256 frame normalised cycle
				if ( frameRate > 0 )
				{
					float natural = 256.0f / frameRate;
					pev->framerate = natural / t;
				}
			}
		}
	}

	// Walking and running all run off one cycle, 29 frames long, which PMZ_Footsteps advances at
	// OoT's rate and keeps in fuser2 (Player's unk_868, func_8084029C); each animation shows it
	// scaled to its own length (the 20 frame run at unk_868 * 20 / 29). Steps land on frames 10
	// and 24 of it, so the feet and the footstep sounds stay together.
	if ( sAnimCache.full[anim] && ZL_IsCycleAnim( anim ) && ( pev->flags & FL_ONGROUND ) )
	{
		void *pmodel = GET_MODEL_PTR( ENT( pev ) );
		float frameRate, groundSpeed;
		GetSequenceInfo( pmodel, pev, &frameRate, &groundSpeed );

		float oot = speed / ZL_VEL( 1.0f );
		BOOL side = ( anim == ZANIM_Z_SIDE_L || anim == ZANIM_Z_SIDE_R || anim == ZANIM_Z_SIDE_L_FREE || anim == ZANIM_Z_SIDE_R_FREE );
		float rate = ( side ? ZL_SIDE_CYCLE_STEP( oot ) : ZL_RUN_CYCLE_STEP( oot ) ) * 1.5f;	// cycle frames per OoT frame
		if ( rate > 7.25f )
			rate = 7.25f;

		float phase = pev->fuser2;
		if ( phase < 0 || phase >= 29.0f )
			phase = 0;

		// frameRate is 256ths of the sequence a second at framerate 1 (256 * fps / (frames - 1))
		pev->frame = phase * ( 256.0f / 29.0f );
		pev->framerate = ( frameRate > 0 ) ? rate * 20.0f * ( 256.0f / 29.0f ) / frameRate : 1.0f;
		pev->animtime = gpGlobals->time;
		m_fSequenceFinished = FALSE;
		return;
	}

	// Fidget sounds, by the frame it's on (one a frame: m_flFrameRate is 256 * 20 / (frames - 1))
	if ( ZL_IsFidget( anim ) && m_flFrameRate > 0 )
	{
		float frame = pev->frame * ZL_FPS / m_flFrameRate;
		for ( int i = 0; i < (int)ARRAYSIZE( s_fidgetSfx ); i++ )
		{
			if ( ( s_fidgetSfx[i].anim == anim || s_fidgetSfx[i].alt == anim ) &&
				 m_flZIdleFrame < s_fidgetSfx[i].frame && frame >= s_fidgetSfx[i].frame )
				ZSound( s_fidgetSfx[i].channel, s_fidgetSfx[i].sound, s_fidgetSfx[i].channel == CHAN_VOICE ? 1.0f : 0.6f );
		}
		m_flZIdleFrame = frame;
	}

	// One shots (knockdowns, item get, the chest) hold their last frame instead of looping
	if ( !def.loop && m_fSequenceFinished )
	{
		pev->frame = 255;
		pev->framerate = 0;
	}
}
