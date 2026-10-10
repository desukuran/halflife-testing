/***
*
*	Zelda 64 port: sound table.
*
*	Every sound the port plays goes through this table. The first column is a stock Half-Life
*	sound that ships with the game; the second is the OoT sound effect it stands in for.
*	When zl_sfx_oot is 1 the OoT column is used instead, as sound/zelda/oot/<NAME>.wav.
*	Those files come out of tools/zelda/rip_sfx.py and your own ROM. They are not
*	distributed with this mod.
*
*	OoT names are the SFX enum names from include/sfx.h / include/tables/sfx/ headers in the decomp.
*
****/
#ifndef ZL_SOUNDS_H
#define ZL_SOUNDS_H

enum zl_sound_e
{
	// Link voice (NA_SE_VO_LI_*). HL has no voiced protagonist, so these are grunts.
	ZS_VO_SWORD_N = 0,
	ZS_VO_SWORD_L,
	ZS_VO_AUTO_JUMP,
	ZS_VO_CLIMB_END,
	ZS_VO_DAMAGE_S,
	ZS_VO_FALL_L,
	ZS_VO_LAND_DAMAGE_S,
	ZS_VO_DOWN,
	ZS_VO_DEATH,
	ZS_VO_LASH,

	// Link body (NA_SE_PL_*)
	ZS_PL_ROLL,
	ZS_PL_SKIP,
	ZS_PL_JUMP,
	ZS_PL_LAND,
	ZS_PL_BODY_HIT,
	ZS_PL_DAMAGE,
	ZS_PL_CLIMB_CLIFF,
	ZS_PL_WALK_GROUND,

	// Items (NA_SE_IT_*)
	ZS_IT_SWORD_SWING,
	ZS_IT_SWORD_SWING_HARD,
	ZS_IT_SWORD_STRIKE,
	ZS_IT_SWORD_HIT,
	ZS_IT_SWORD_CHARGE,
	ZS_IT_ROLLING_CUT,
	ZS_IT_SHIELD_POSTURE,
	ZS_IT_SHIELD_REFLECT_SW,
	ZS_IT_SHIELD_BOUND,
	ZS_IT_BOW_DRAW,
	ZS_IT_ARROW_SHOT,
	ZS_IT_ARROW_STICK_OBJ,
	ZS_IT_ARROW_STICK_CRE,
	ZS_IT_BOMB_IGNIT,
	ZS_IT_BOMB_EXPLOSION,
	ZS_IT_BOMB_BOUND,
	ZS_IT_HOOKSHOT_READY,
	ZS_IT_HOOKSHOT_CHAIN,
	ZS_IT_HOOKSHOT_STICK_OBJ,
	ZS_IT_HOOKSHOT_RECEIVE,
	ZS_IT_HOOKSHOT_REFLECT,
	ZS_IT_DEKU_NUT,
	ZS_IT_PUT_AWAY,

	// System (NA_SE_SY_*)
	ZS_SY_LOCK_ON,
	ZS_SY_LOCK_OFF,
	ZS_SY_LOCK_ON_HUMAN,
	ZS_SY_CAMERA_ZOOM_UP,
	ZS_SY_MESSAGE_PASS,
	ZS_SY_MESSAGE_END,
	ZS_SY_CURSOR,
	ZS_SY_DECIDE,
	ZS_SY_ERROR,
	ZS_SY_HP_RECOVER,
	ZS_SY_HITPOINT_ALARM,
	ZS_SY_GET_RUPY,
	ZS_SY_GET_ITEM,
	ZS_SY_GET_BOXITEM,
	ZS_SY_GET_HEART,
	ZS_SY_FSEL_DECIDE_L,

	// Enemies / environment
	ZS_EN_DAMAGE,
	ZS_EV_POT_BROKEN,
	ZS_EV_CHEST_OPEN,
	ZS_NAVI_HELLO,
	ZS_NAVI_HEY,

	ZS_COUNT
};

typedef struct zl_sound_s
{
	const char *hl;		// stock Half-Life sound
	const char *oot;	// OoT SFX enum name (sound/zelda/oot/<oot>.wav once ripped)
} zl_sound_t;

static const zl_sound_t g_zlSounds[ZS_COUNT] =
{
	// ZS_VO_*
	{ "player/pl_jump1.wav",			"NA_SE_VO_LI_SWORD_N" },		// "Hup!" on sidehops, slashes
	{ "player/pl_jump2.wav",			"NA_SE_VO_LI_SWORD_L" },		// "Hyah!" jump attack, spin
	{ "player/pl_jump1.wav",			"NA_SE_VO_LI_AUTO_JUMP" },		// running off a ledge
	{ "player/pl_jump2.wav",			"NA_SE_VO_LI_CLIMB_END" },		// pulling up onto a ledge
	{ "player/pl_pain2.wav",			"NA_SE_VO_LI_DAMAGE_S" },		// small hit
	{ "player/pl_pain4.wav",			"NA_SE_VO_LI_FALL_L" },			// knocked down
	{ "player/pl_fallpain3.wav",		"NA_SE_VO_LI_LAND_DAMAGE_S" },	// fall damage
	{ "player/pl_pain6.wav",			"NA_SE_VO_LI_DOWN" },
	{ "player/pl_pain7.wav",			"NA_SE_VO_LI_DOWN" },			// Link's death scream: NA_SE_VO_LI_DOWN + game over jingle
	{ "player/pl_jump1.wav",			"NA_SE_VO_LI_LASH" },

	// ZS_PL_*
	{ "player/pl_duct1.wav",			"NA_SE_PL_ROLL" },
	{ "player/pl_duct2.wav",			"NA_SE_PL_SKIP" },
	{ "player/pl_step2.wav",			"NA_SE_PL_JUMP" },
	{ "player/pl_step3.wav",			"NA_SE_PL_LAND" },
	{ "common/bodydrop3.wav",			"NA_SE_PL_BODY_HIT" },
	{ "player/pl_pain5.wav",			"NA_SE_PL_DAMAGE" },
	{ "player/pl_ladder1.wav",			"NA_SE_PL_CLIMB_CLIFF" },
	{ "player/pl_step1.wav",			"NA_SE_PL_WALK_GROUND" },

	// ZS_IT_*
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_SWORD_SWING" },
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_SWORD_SWING_HARD" },
	{ "weapons/cbar_hit1.wav",			"NA_SE_IT_SWORD_STRIKE" },		// sword on a wall: "clink" and sparks
	{ "weapons/cbar_hitbod1.wav",		"NA_SE_IT_SWORD_HIT" },
	{ "items/suitcharge1.wav",			"NA_SE_IT_SWORD_CHARGE" },		// spin attack charge loop
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_ROLLING_CUT" },		// spin attack release
	{ "items/gunpickup1.wav",			"NA_SE_IT_SHIELD_POSTURE" },
	{ "weapons/cbar_hit2.wav",			"NA_SE_IT_SHIELD_REFLECT_SW" },	// blocked a hit
	{ "weapons/cbar_hit2.wav",			"NA_SE_IT_SHIELD_BOUND" },
	{ "weapons/xbow_reload1.wav",		"NA_SE_IT_BOW_DRAW" },
	{ "weapons/xbow_fire1.wav",			"NA_SE_IT_ARROW_SHOT" },
	{ "weapons/xbow_hit1.wav",			"NA_SE_IT_ARROW_STICK_OBJ" },
	{ "weapons/xbow_hitbod1.wav",		"NA_SE_IT_ARROW_STICK_CRE" },
	{ "weapons/mine_activate.wav",		"NA_SE_IT_BOMB_IGNIT" },			// lit fuse (loops on the bomb)
	{ "weapons/explode3.wav",			"NA_SE_IT_BOMB_EXPLOSION" },
	{ "weapons/grenade_hit1.wav",		"NA_SE_IT_BOMB_BOUND" },
	{ "weapons/357_cock1.wav",			"NA_SE_IT_HOOKSHOT_READY" },
	{ "weapons/xbow_fly1.wav",			"NA_SE_IT_HOOKSHOT_CHAIN" },
	{ "weapons/xbow_hit1.wav",			"NA_SE_IT_HOOKSHOT_STICK_OBJ" },
	{ "items/gunpickup2.wav",			"NA_SE_IT_HOOKSHOT_RECEIVE" },
	{ "weapons/cbar_hit2.wav",			"NA_SE_IT_HOOKSHOT_REFLECT" },
	{ "debris/glass1.wav",				"NA_SE_IT_EXPLOSION_LIGHT" },	// Deku Nut flash
	{ "common/wpn_hudoff.wav",			"NA_SE_PL_CHANGE_ARMS" },

	// ZS_SY_*
	{ "buttons/blip2.wav",				"NA_SE_SY_LOCK_ON" },
	{ "common/wpn_hudoff.wav",			"NA_SE_SY_LOCK_OFF" },
	{ "buttons/blip1.wav",				"NA_SE_SY_LOCK_ON_HUMAN" },
	{ "common/wpn_hudon.wav",			"NA_SE_SY_CAMERA_ZOOM_UP" },
	{ "buttons/blip1.wav",				"NA_SE_SY_MESSAGE_PASS" },
	{ "common/wpn_select.wav",			"NA_SE_SY_MESSAGE_END" },
	{ "common/wpn_moveselect.wav",		"NA_SE_SY_CURSOR" },
	{ "common/wpn_select.wav",			"NA_SE_SY_DECIDE" },
	{ "common/wpn_denyselect.wav",		"NA_SE_SY_ERROR" },
	{ "items/smallmedkit1.wav",			"NA_SE_SY_HP_RECOVER" },
	{ "fvox/beep.wav",					"NA_SE_SY_HITPOINT_ALARM" },
	{ "items/9mmclip1.wav",				"NA_SE_SY_GET_RUPY" },
	{ "items/gunpickup2.wav",			"NA_SE_SY_GET_ITEM" },
	{ "items/suitchargeok1.wav",		"NA_SE_SY_GET_BOXITEM" },
	{ "items/smallmedkit1.wav",			"NA_SE_SY_GET_HEART" },
	{ "buttons/bell1.wav",				"NA_SE_SY_FSEL_DECIDE_L" },

	// Enemies / environment
	{ "weapons/cbar_hitbod2.wav",		"NA_SE_EN_DAMAGE" },				// generic enemy hurt
	{ "debris/bustcrate1.wav",			"NA_SE_EV_POT_BROKEN" },
	{ "doors/doormove2.wav",			"NA_SE_EV_TBOX_OPEN" },
	{ "buttons/blip2.wav",				"NA_SE_VO_NAVY_HELLO" },			// Navi: "Hello!"
	{ "buttons/blip1.wav",				"NA_SE_VO_NAVY_HEAR" },			// Navi: "Hey!" / "Listen!"
};

#endif // ZL_SOUNDS_H
