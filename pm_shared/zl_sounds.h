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
	int rand;			// OoT sfx table "rand" (0-3): up to +8 / +16 / +33% random pitch (Audio_ComputeSfxFreqScale)
	int lower;			// SFX_FLAG_14: up to -8% random pitch
} zl_sound_t;

static const zl_sound_t g_zlSounds[ZS_COUNT] =
{
	// ZS_VO_*
	{ "player/pl_jump1.wav",			"NA_SE_VO_LI_SWORD_N", 0, 0 },		// "Hup!" on sidehops, slashes
	{ "player/pl_jump2.wav",			"NA_SE_VO_LI_SWORD_L", 0, 0 },		// "Hyah!" jump attack, spin
	{ "player/pl_jump1.wav",			"NA_SE_VO_LI_AUTO_JUMP", 0, 0 },		// running off a ledge
	{ "player/pl_jump2.wav",			"NA_SE_VO_LI_CLIMB_END", 0, 0 },		// pulling up onto a ledge
	{ "player/pl_pain2.wav",			"NA_SE_VO_LI_DAMAGE_S", 0, 0 },		// small hit
	{ "player/pl_pain4.wav",			"NA_SE_VO_LI_FALL_L", 0, 0 },			// knocked down
	{ "player/pl_fallpain3.wav",		"NA_SE_VO_LI_LAND_DAMAGE_S", 0, 0 },	// fall damage
	{ "player/pl_pain6.wav",			"NA_SE_VO_LI_DOWN", 0, 0 },
	{ "player/pl_pain7.wav",			"NA_SE_VO_LI_DOWN", 0, 0 },			// Link's death scream: NA_SE_VO_LI_DOWN + game over jingle
	{ "player/pl_jump1.wav",			"NA_SE_VO_LI_LASH", 0, 0 },

	// ZS_PL_*
	{ "player/pl_duct1.wav",			"NA_SE_PL_ROLL", 0, 0 },
	{ "player/pl_duct2.wav",			"NA_SE_PL_SKIP", 0, 0 },
	{ "player/pl_step2.wav",			"NA_SE_PL_JUMP", 0, 0 },
	{ "player/pl_step3.wav",			"NA_SE_PL_LAND", 1, 0 },
	{ "common/bodydrop3.wav",			"NA_SE_PL_BODY_HIT", 0, 0 },
	{ "player/pl_pain5.wav",			"NA_SE_PL_DAMAGE", 0, 0 },
	{ "player/pl_ladder1.wav",			"NA_SE_PL_CLIMB_CLIFF", 0, 0 },
	{ "player/pl_step1.wav",			"NA_SE_PL_WALK_GROUND", 2, 0 },

	// ZS_IT_*
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_SWORD_SWING", 1, 0 },
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_SWORD_SWING_HARD", 0, 0 },
	{ "weapons/cbar_hit1.wav",			"NA_SE_IT_SWORD_STRIKE", 0, 0 },		// sword on a wall: "clink" and sparks
	{ "weapons/cbar_hitbod1.wav",		"NA_SE_IT_SWORD_IMPACT", 1, 0 },
	{ "items/suitcharge1.wav",			"NA_SE_IT_SWORD_CHARGE", 0, 0 },		// spin attack charge loop
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_ROLLING_CUT", 0, 0 },		// spin attack release
	{ "items/gunpickup1.wav",			"NA_SE_IT_SHIELD_POSTURE", 1, 0 },
	{ "weapons/cbar_hit2.wav",			"NA_SE_IT_SHIELD_REFLECT_SW", 1, 0 },	// blocked a hit
	{ "weapons/cbar_hit2.wav",			"NA_SE_IT_SHIELD_BOUND", 2, 0 },
	{ "weapons/xbow_reload1.wav",		"NA_SE_IT_BOW_DRAW", 1, 0 },
	{ "weapons/xbow_fire1.wav",			"NA_SE_IT_ARROW_SHOT", 1, 0 },
	{ "weapons/xbow_hit1.wav",			"NA_SE_IT_ARROW_STICK_OBJ", 0, 0 },
	{ "weapons/xbow_hitbod1.wav",		"NA_SE_IT_ARROW_STICK_CRE", 0, 0 },
	{ "weapons/mine_activate.wav",		"NA_SE_IT_BOMB_IGNIT", 0, 0 },			// lit fuse (loops on the bomb)
	{ "weapons/explode3.wav",			"NA_SE_IT_BOMB_EXPLOSION", 0, 0 },
	{ "weapons/grenade_hit1.wav",		"NA_SE_EV_BOMB_BOUND", 2, 0 },
	{ "weapons/357_cock1.wav",			"NA_SE_IT_HOOKSHOT_READY", 0, 0 },
	{ "weapons/xbow_fly1.wav",			"NA_SE_IT_HOOKSHOT_CHAIN", 0, 0 },
	{ "weapons/xbow_hit1.wav",			"NA_SE_IT_HOOKSHOT_STICK_OBJ", 1, 0 },
	{ "items/gunpickup2.wav",			"NA_SE_IT_HOOKSHOT_RECEIVE", 0, 0 },
	{ "weapons/cbar_hit2.wav",			"NA_SE_IT_HOOKSHOT_REFLECT", 0, 0 },
	{ "debris/glass1.wav",				"NA_SE_IT_EXPLOSION_LIGHT", 0, 0 },	// Deku Nut flash
	{ "common/wpn_hudoff.wav",			"NA_SE_PL_CHANGE_ARMS", 0, 0 },

	// ZS_SY_*
	{ "buttons/blip2.wav",				"NA_SE_SY_LOCK_ON", 0, 0 },
	{ "common/wpn_hudoff.wav",			"NA_SE_SY_LOCK_OFF", 0, 0 },
	{ "buttons/blip1.wav",				"NA_SE_SY_LOCK_ON_HUMAN", 0, 0 },
	{ "common/wpn_hudon.wav",			"NA_SE_SY_CAMERA_ZOOM_UP", 0, 0 },
	{ "buttons/blip1.wav",				"NA_SE_SY_MESSAGE_PASS", 0, 0 },
	{ "common/wpn_select.wav",			"NA_SE_SY_MESSAGE_END", 0, 0 },
	{ "common/wpn_moveselect.wav",		"NA_SE_SY_CURSOR", 0, 0 },
	{ "common/wpn_select.wav",			"NA_SE_SY_DECIDE", 0, 0 },
	{ "common/wpn_denyselect.wav",		"NA_SE_SY_ERROR", 0, 0 },
	{ "items/smallmedkit1.wav",			"NA_SE_SY_HP_RECOVER", 0, 0 },
	{ "fvox/beep.wav",					"NA_SE_SY_HITPOINT_ALARM", 0, 0 },
	{ "items/9mmclip1.wav",				"NA_SE_SY_GET_RUPY", 0, 0 },
	{ "items/gunpickup2.wav",			"NA_SE_SY_GET_ITEM", 0, 0 },
	{ "items/suitchargeok1.wav",		"NA_SE_SY_GET_BOXITEM", 0, 0 },
	{ "items/smallmedkit1.wav",			"NA_SE_SY_HP_RECOVER", 0, 0 },
	{ "buttons/bell1.wav",				"NA_SE_SY_FSEL_DECIDE_L", 0, 0 },

	// Enemies / environment
	{ "weapons/cbar_hitbod2.wav",		"NA_SE_EN_STAL_DAMAGE", 0, 0 },				// OoT has no generic enemy hurt; a Stalchild's
	{ "debris/bustcrate1.wav",			"NA_SE_EV_POT_BROKEN", 0, 0 },
	{ "doors/doormove2.wav",			"NA_SE_EV_TBOX_OPEN", 0, 0 },
	{ "buttons/blip2.wav",				"NA_SE_VO_NA_HELLO_3", 0, 0 },			// Navi: "Hey!" (Object_Kankyo; NA_SE_VO_NAVY_HELLO is silent in OoT)
	{ "buttons/blip1.wav",				"NA_SE_VO_NA_HELLO_2", 1, 0 },			// Navi: "Listen!" (played right after "Hey!")
};

/*
 OoT's random pitch for a sound (Audio_ComputeSfxFreqScale): SFX_FLAG_14 lowers it by up to
 15/192, the table's rand field raises it by up to 15, 31 or 63 / 192. rnd is any random
 number; the caller's own frequency scale (footsteps by speed, ...) multiplies the result.
*/
static float ZL_SoundFreq( int id, int rnd )
{
	static const int masks[4] = { 0, 0xF, 0x1F, 0x3F };
	float freq = 1.0f;

	if ( id < 0 || id >= ZS_COUNT )
		return freq;
	if ( g_zlSounds[id].lower )
		freq = 1.0f - (float)( rnd & 0xF ) / 192.0f;
	freq += (float)( ( rnd >> 4 ) & masks[g_zlSounds[id].rand & 3] ) / 192.0f;
	return freq;
}

/*
 OoT picks between several takes of Link's voice clips (NA_SE_VO_LI_SWORD_N has four).
 tools/zelda/extract_sfx.py writes them as NAME.wav, NAME_2.wav ... and variants.txt; the
 server turns that into the "zv" physinfo key, one digit per sound id ('1' = one take), so
 the movement code and the client pick the same way. take is 1-based.
*/
static void ZL_OoTSoundPath( char *out, int size, int id, int take )
{
	if ( take > 1 )
		snprintf( out, size, "zelda/oot/%s_%d.wav", g_zlSounds[id].oot, take );
	else
		snprintf( out, size, "zelda/oot/%s.wav", g_zlSounds[id].oot );
}

static int ZL_SoundTakes( const char *zv, int id )
{
	if ( !zv || id < 0 || id >= (int)strlen( zv ) )
		return 1;
	return ( zv[id] >= '2' && zv[id] <= '9' ) ? zv[id] - '0' : 1;
}

// Half-Life pitch (100 = normal) for a sound played at freqScale
static int ZL_SoundPitch( int id, float freqScale, int rnd )
{
	int pitch = (int)( 100.0f * freqScale * ZL_SoundFreq( id, rnd ) + 0.5f );
	return pitch < 1 ? 1 : ( pitch > 255 ? 255 : pitch );
}

#endif // ZL_SOUNDS_H
