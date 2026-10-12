/***
*
*	Zelda 64 port: sound table.
*
*	Every sound the port plays goes through this table. The first column is a stock Half-Life
*	sound that ships with the game; the second is the OoT sound effect it stands in for.
*	When sound/zelda/oot/<NAME>.wav exists (tools/zelda/extract_sfx.py makes them from your
*	own ROM; they are not distributed with this mod) that sound is used instead, unless
*	zl_oot_sounds is 0.
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
	ZS_PL_BODY_HIT,
	ZS_PL_DAMAGE,
	ZS_PL_CLIMB_CLIFF,
	ZS_PL_SLIP,

	// Footsteps, jumps and landings, one per floor (ZL_SURF_*, in that order)
	ZS_PL_WALK_GROUND,
	ZS_PL_WALK_SAND,
	ZS_PL_WALK_STONE,
	ZS_PL_WALK_JABU,
	ZS_PL_WALK_WATER,
	ZS_PL_WALK_GRASS,
	ZS_PL_WALK_WOOD,
	ZS_PL_WALK_ICE,
	ZS_PL_JUMP,
	ZS_PL_JUMP_SAND,
	ZS_PL_JUMP_STONE,
	ZS_PL_JUMP_JABU,
	ZS_PL_JUMP_WATER,
	ZS_PL_JUMP_GRASS,
	ZS_PL_JUMP_WOOD,
	ZS_PL_JUMP_ICE,
	ZS_PL_LAND,
	ZS_PL_LAND_SAND,
	ZS_PL_LAND_STONE,
	ZS_PL_LAND_JABU,
	ZS_PL_LAND_WATER,
	ZS_PL_LAND_GRASS,
	ZS_PL_LAND_WOOD,
	ZS_PL_LAND_ICE,

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
	ZS_IT_WALL_HIT_HARD,
	ZS_IT_WALL_HIT_SOFT,
	ZS_IT_SWORD_STRIKE_HARD,
	ZS_IT_REFLECTION_WOOD,
	ZS_IT_ROLLING_CUT_LV1,
	ZS_IT_ROLLING_CUT_LV2,
	ZS_PL_SWORD_CHARGE,
	ZS_VO_SNEEZE,			// idle fidgets (sFidgetAnimSfxLists)
	ZS_VO_SWEAT,
	ZS_VO_BREATH_REST,
	ZS_VO_RELAX,
	ZS_PL_CALM_HIT,
	ZS_IT_HAMMER_SWING,
	ZS_EV_PLANT_BROKEN,		// cutting grass (En_Kusa)
	ZS_EV_TORCH,			// a lit torch (Obj_Syokudai)
	ZS_EV_FLAME_IGNITION,
	ZS_OC_DOOR_OPEN,		// doors (En_Door, Door_Shutter)
	ZS_EV_DOOR_CLOSE,
	ZS_EV_SLIDE_DOOR_OPEN,
	ZS_EV_SLIDE_DOOR_CLOSE,
	ZS_EV_STONE_BOUND,
	ZS_EV_CHAIN_KEY_UNLOCK,
	ZS_VO_FREEZE,			// taking hits (func_80837C0C and the knockdown)
	ZS_VO_FALL_S,
	ZS_VO_TAKEN_AWAY,
	ZS_PL_BOUND,

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
	ZS_EV_CHEST_UNLOCK,
	ZS_NAVI_HELLO,
	ZS_NAVI_HEY,

	// Gossip Stone (En_Gs)
	ZS_GS_BOING,
	ZS_GS_LAUNCH,
	ZS_GS_FIRE,
	ZS_GS_COUNT,
	ZS_GS_COUNT_LAST,
	ZS_GS_GROW,
	ZS_GS_ROLL,

	ZS_COUNT
};

// Floors with their own footsteps: OoT's SURFACE_SFX_OFFSET_* that Half-Life materials map to
enum zl_surface_e
{
	ZL_SURF_DIRT = 0,	// SURFACE_SFX_OFFSET_DIRT (OoT's default floor)
	ZL_SURF_SAND,		// _SAND
	ZL_SURF_STONE,		// _STONE
	ZL_SURF_JABU,		// _JABU (squelchy)
	ZL_SURF_WATER,		// _WATER_SHALLOW
	ZL_SURF_GRASS,		// _GRASS
	ZL_SURF_WOOD,		// _WOOD
	ZL_SURF_ICE,		// _ICE
	ZL_SURF_COUNT
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
	{ "common/bodydrop3.wav",			"NA_SE_PL_BODY_HIT", 0, 0 },
	{ "player/pl_pain5.wav",			"NA_SE_PL_DAMAGE", 0, 0 },
	{ "player/pl_ladder1.wav",			"NA_SE_PL_CLIMB_CLIFF", 0, 0 },
	{ "player/pl_tile3.wav",			"NA_SE_PL_SLIP_LEVEL", 0, 0 },		// sliding on ice, repeated while it lasts

	// NA_SE_PL_WALK_GROUND + SURFACE_SFX_OFFSET_*. OoT's names for the offsets don't match what
	// they're used for (offset 3, Jabu-Jabu's belly, is "_DIRT"; offset 10, wood, is "_LADDER").
	{ "player/pl_dirt1.wav",			"NA_SE_PL_WALK_GROUND", 2, 0 },		// SURFACE_SFX_OFFSET_DIRT
	{ "player/pl_dirt2.wav",			"NA_SE_PL_WALK_SAND", 2, 0 },			// _SAND
	{ "player/pl_step1.wav",			"NA_SE_PL_WALK_CONCRETE", 2, 0 },		// _STONE
	{ "player/pl_slosh1.wav",			"NA_SE_PL_WALK_DIRT", 2, 0 },			// _JABU
	{ "player/pl_slosh2.wav",			"NA_SE_PL_WALK_WATER0", 1, 0 },		// _WATER_SHALLOW
	{ "player/pl_dirt3.wav",			"NA_SE_PL_WALK_GRASS", 2, 0 },		// _GRASS
	{ "player/pl_tile1.wav",			"NA_SE_PL_WALK_LADDER", 2, 0 },		// _WOOD
	{ "player/pl_tile2.wav",			"NA_SE_PL_WALK_ICE", 0, 0 },			// _ICE
	{ "player/pl_dirt2.wav",			"NA_SE_PL_JUMP", 0, 0 },
	{ "player/pl_dirt3.wav",			"NA_SE_PL_JUMP_SAND", 0, 0 },
	{ "player/pl_step2.wav",			"NA_SE_PL_JUMP_CONCRETE", 0, 0 },
	{ "player/pl_slosh3.wav",			"NA_SE_PL_JUMP_DIRT", 0, 0 },
	{ "player/pl_slosh3.wav",			"NA_SE_PL_JUMP_WATER0", 0, 0 },
	{ "player/pl_dirt4.wav",			"NA_SE_PL_JUMP_GRASS", 0, 0 },
	{ "player/pl_tile3.wav",			"NA_SE_PL_JUMP_LADDER", 0, 0 },
	{ "player/pl_tile4.wav",			"NA_SE_PL_JUMP_ICE", 0, 0 },
	{ "player/pl_dirt4.wav",			"NA_SE_PL_LAND", 1, 0 },
	{ "player/pl_dirt1.wav",			"NA_SE_PL_LAND_SAND", 1, 0 },
	{ "player/pl_step3.wav",			"NA_SE_PL_LAND_CONCRETE", 1, 0 },
	{ "player/pl_slosh4.wav",			"NA_SE_PL_LAND_DIRT", 1, 0 },
	{ "player/pl_slosh4.wav",			"NA_SE_PL_LAND_WATER0", 1, 0 },
	{ "player/pl_dirt2.wav",			"NA_SE_PL_LAND_GRASS", 1, 0 },
	{ "player/pl_tile5.wav",			"NA_SE_PL_LAND_LADDER", 1, 0 },
	{ "player/pl_tile1.wav",			"NA_SE_PL_LAND_ICE", 1, 0 },

	// ZS_IT_*
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_SWORD_SWING", 1, 0 },
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_SWORD_SWING_HARD", 0, 0 },
	{ "weapons/cbar_hitbod1.wav",		"NA_SE_IT_SWORD_STRIKE", 0, 0 },		// sword into an enemy (CollisionCheck_SwordHitAudio)
	{ "weapons/cbar_hitbod1.wav",		"NA_SE_IT_SWORD_IMPACT", 1, 0 },
	{ "items/suitcharge1.wav",			"NA_SE_IT_SWORD_CHARGE", 0, 0 },		// spin attack charge hum, held while it's lit
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_ROLLING_CUT", 0, 0 },		// spin attack without the ring
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
	{ "weapons/cbar_hit1.wav",			"NA_SE_IT_WALL_HIT_HARD", 0, 0 },		// sword into a wall: the clang (func_80842DF4)
	{ "player/pl_dirt1.wav",			"NA_SE_IT_WALL_HIT_SOFT", 0, 0 },		// ... into soft dirt
	{ "weapons/cbar_hit2.wav",			"NA_SE_IT_SWORD_STRIKE_HARD", 0, 0 },	// hitting something armoured (ELEM_MATERIAL_UNK1)
	{ "debris/wood1.wav",				"NA_SE_IT_REFLECTION_WOOD", 2, 0 },		// sword on wood
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_ROLLING_CUT_LV1", 0, 0 },		// spin attack with the blue ring (EnMThunder)
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_ROLLING_CUT_LV2", 0, 0 },		// great spin, the orange ring
	{ "items/suitchargeok1.wav",		"NA_SE_PL_SWORD_CHARGE", 0, 0 },		// the sword lights up (Audio_PlaySwordChargeSfx)
	{ "common/null.wav",				"NA_SE_VO_LI_SNEEZE", 0, 0 },			// idle in the cold
	{ "common/null.wav",				"NA_SE_VO_LI_SWEAT", 0, 0 },			// idle in the heat
	{ "player/breathe1.wav",			"NA_SE_VO_LI_BREATH_REST", 1, 0 },		// idle at critical health: panting
	{ "common/null.wav",				"NA_SE_VO_LI_RELAX", 0, 0 },			// stretching
	{ "player/pl_duct3.wav",			"NA_SE_PL_CALM_HIT", 2, 0 },			// patting his tunic
	{ "weapons/cbar_miss1.wav",			"NA_SE_IT_HAMMER_SWING", 1, 0 },		// the idle sword twirl
	{ "debris/wood3.wav",				"NA_SE_EV_PLANT_BROKEN", 1, 0 },		// grass / a bush cut
	{ "ambience/burning1.wav",			"NA_SE_EV_TORCH", 0, 0 },				// a lit torch crackling
	{ "ambience/flameburst1.wav",		"NA_SE_EV_FLAME_IGNITION", 2, 0 },		// a torch catching
	{ "doors/doormove2.wav",			"NA_SE_OC_DOOR_OPEN", 0, 0 },			// a door swinging open
	{ "doors/doorstop2.wav",			"NA_SE_EV_DOOR_CLOSE", 0, 0 },			// and shutting behind Link
	{ "doors/doormove3.wav",			"NA_SE_EV_SLIDE_DOOR_OPEN", 0, 0 },		// a dungeon door sliding up
	{ "doors/doormove3.wav",			"NA_SE_EV_SLIDE_DOOR_CLOSE", 0, 0 },	// and coming down
	{ "doors/doorstop1.wav",			"NA_SE_EV_STONE_BOUND", 0, 0 },			// its thud at the bottom
	{ "doors/latchunlocked1.wav",		"NA_SE_EV_CHAIN_KEY_UNLOCK", 0, 0 },	// a small key in a locked door
	{ "player/pl_pain7.wav",			"NA_SE_VO_LI_FREEZE", 0, 0 },			// knocked down: hitting the ground
	{ "player/pl_pain4.wav",			"NA_SE_VO_LI_FALL_S", 0, 0 },			// starting a long drop
	{ "player/pl_pain6.wav",			"NA_SE_VO_LI_TAKEN_AWAY", 0, 0 },		// electrocuted
	{ "common/bodydrop4.wav",			"NA_SE_PL_BOUND", 0, 0 },				// knocked down: body on the ground

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
	{ "doors/latchunlocked1.wav",		"NA_SE_EV_TBOX_UNLOCK", 0, 0 },
	{ "buttons/blip2.wav",				"NA_SE_VO_NA_HELLO_3", 0, 0 },			// Navi: "Hey!" (Object_Kankyo; NA_SE_VO_NAVY_HELLO is silent in OoT)
	{ "buttons/blip1.wav",				"NA_SE_VO_NA_HELLO_2", 1, 0 },			// Navi: "Listen!" (played right after "Hey!")

	// ZS_GS_*: the Gossip Stone
	{ "debris/wood2.wav",				"NA_SE_EN_STALKID_ATTACK", 0, 0 },		// hit with a sword: "boing" and wobble
	{ "weapons/rocket1.wav",			"NA_SE_EV_STONE_LAUNCH", 0, 0 },		// blasting off after a bomb
	{ "ambience/burning1.wav",			"NA_SE_EV_FIRE_PILLAR", 0, 0 },			// smoking before lift-off
	{ "buttons/blip1.wav",				"NA_SE_SY_WARNING_COUNT_N", 0, 0 },		// countdown beep (blue)
	{ "buttons/blip2.wav",				"NA_SE_SY_WARNING_COUNT_E", 0, 0 },		// last beeps (red)
	{ "debris/concrete1.wav",			"NA_SE_EV_STONE_GROW_UP", 0, 0 },		// stretching up after fire / light
	{ "debris/concrete3.wav",			"NA_SE_EV_STONE_ROLLING", 0, 0 },		// each turn while spinning
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
 Which file each sound plays. The server decides once per map, when it precaches
 (ZL_PrecacheAll), and publishes it as the "zv" physinfo key: one character per sound id,
 '0' = the Half-Life stand-in, '1'..'9' = how many OoT takes there are (OoT picks between
 several takes of Link's voice clips; NA_SE_VO_LI_SWORD_N has four, written as NAME.wav,
 NAME_2.wav ... by extract_sfx.py). The movement code, the client and the server all build
 the sample name from it, so only what was precached is ever played.
*/
static int ZL_SoundTakes( const char *zv, int id )
{
	if ( !zv || id < 0 || id >= (int)strlen( zv ) )
		return 0;
	return ( zv[id] >= '1' && zv[id] <= '9' ) ? zv[id] - '0' : 0;
}

static void ZL_OoTSoundPath( char *out, int size, int id, int take )
{
	if ( take > 1 )
		snprintf( out, size, "zelda/oot/%s_%d.wav", g_zlSounds[id].oot, take );
	else
		snprintf( out, size, "zelda/oot/%s.wav", g_zlSounds[id].oot );
}

// The sample to play for sound id; rnd picks the take. out must hold 96 characters.
static const char *ZL_SoundSample( char *out, int size, const char *zv, int id, int rnd )
{
	int takes = ZL_SoundTakes( zv, id );
	if ( id < 0 || id >= ZS_COUNT )
		return "common/null.wav";
	if ( !takes )
		return g_zlSounds[id].hl;
	ZL_OoTSoundPath( out, size, id, 1 + ( rnd & 0x7FFF ) % takes );
	return out;
}

// Half-Life pitch (100 = normal) for a sound played at freqScale
static int ZL_SoundPitch( int id, float freqScale, int rnd )
{
	int pitch = (int)( 100.0f * freqScale * ZL_SoundFreq( id, rnd ) + 0.5f );
	return pitch < 1 ? 1 : ( pitch > 255 ? 255 : pitch );
}

#endif // ZL_SOUNDS_H
