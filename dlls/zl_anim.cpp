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
	ZANIM_CARRY,
	ZANIM_CARRY_WALK,
	ZANIM_SWIM,
	ZANIM_SWIM_IDLE,
	ZANIM_LEGS_WAIT,	// legs under an upper body animation (link.mdl's gait), never picked directly
	ZANIM_LEGS_WALK,
	ZANIM_LEGS_RUN,
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
};

static const zl_anim_def_t sAnims[ZANIM_COUNT] =
{
	{ "gPlayerAnim_link_normal_wait",				{ "link_normal_wait", "ref_aim_crowbar" },				ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_walk",				{ "link_normal_walk", "ref_aim_crowbar" },				ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_run",				{ "link_normal_run", "ref_aim_crowbar" },				ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_anchor_waitR",				{ "link_anchor_waitR", "crouch_aim_crowbar" },			"deep_idle", TRUE },
	{ "gPlayerAnim_link_anchor_side_walkL",			{ "link_anchor_side_walkL", "ref_aim_crowbar" },		ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_anchor_side_walkR",			{ "link_anchor_side_walkR", "ref_aim_crowbar" },		ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_anchor_back_walk",			{ "link_anchor_back_walk", "ref_aim_crowbar" },			ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_landing_roll",		{ "link_normal_landing_roll", "longjump", "jump" },		ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_normal_hip_down",			{ "link_normal_hip_down", "die_backwards1", "die_backwards" }, ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_fighter_Lside_jump",		{ "link_fighter_Lside_jump", "jump" },					ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_fighter_Rside_jump",		{ "link_fighter_Rside_jump", "jump" },					ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_fighter_backturn_jump",		{ "link_fighter_backturn_jump", "longjump", "jump" },	ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_normal_run_jump",			{ "link_normal_run_jump", "jump" },						ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_normal_landing_wait",		{ "link_normal_landing_wait", "jump" },					ZGAIT_NONE, TRUE },
	{ "gPlayerAnim_link_normal_landing",			{ "link_normal_landing", "crouch_idle" },				ZGAIT_NONE, FALSE },
	{ "gPlayerAnim_link_normal_150step_up",			{ "link_normal_150step_up", "crouch_idle", "jump" },		ZGAIT_NONE, FALSE },
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
	{ "gPlayerAnim_link_hook_fly_wait",				{ "link_hook_fly_wait", "longjump", "jump" },			ZGAIT_NONE, TRUE },
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
	// Carrying (Player_UpperAction_CarryActor): carryB_wait on the upper body only, the legs
	// keep running the normal *_free cycle (PLAYER_ANIMTYPE_5). carryB is the lift itself.
	{ "gPlayerAnim_link_normal_carryB_wait",		{ "link_normal_carryB_wait", "ref_aim_squeak", "ref_aim_trip" }, ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_carryB_wait",		{ "link_normal_carryB_wait", "ref_aim_squeak", "ref_aim_trip" }, ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_swimer_swim",				{ "link_swimer_swim", "swim" },							ZGAIT_NONE, TRUE },
	{ "gPlayerAnim_link_swimer_swim_wait",			{ "link_swimer_swim_wait", "treadwater" },				ZGAIT_NONE, TRUE },
	// ZANIM_LEGS_*: PLAYER_ANIMGROUP_wait / walk / run for PLAYER_ANIMTYPE_5 (holding something)
	{ "gPlayerAnim_link_normal_wait_free",			{ "link_normal_wait_free", "deep_idle" },				ZGAIT_NONE, TRUE },
	{ "gPlayerAnim_link_normal_walk_free",			{ "link_normal_walk_free", "walk" },					ZGAIT_NONE, TRUE },
	{ "gPlayerAnim_link_normal_run_free",			{ "link_normal_run_free", "run" },						ZGAIT_NONE, TRUE },
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

static int ZL_PickAnim( CLinkPlayer *pLink )
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
	case ZACT_JUMP:			return ZANIM_JUMP;
	case ZACT_FALL:			return ZANIM_FALL;
	case ZACT_LAND:			return ZANIM_LAND;
	case ZACT_CLIMB:		return ZANIM_CLIMB;
	case ZACT_JUMPSLASH:	return ZANIM_JUMPSLASH;
	case ZACT_JUMPSLASH_END: return ZANIM_JUMPSLASH_END;
	case ZACT_SPIN_CHARGE:	return speed > 10 ? ZANIM_SPIN_CHARGE_WALK : ZANIM_SPIN_CHARGE;
	case ZACT_SPIN:			return mwa == ZMWA_BIG_SPIN_1H ? ZANIM_BIG_SPIN : ZANIM_SPIN;
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

	// Standing / moving
	if ( pLink->m_hZHeldBomb != NULL )
		return speed > 10 ? ZANIM_CARRY_WALK : ZANIM_CARRY;

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
		return ZANIM_IDLE;
	if ( speed < ZL_VEL( 3.0f ) )
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
	case ZANIM_Z_BACK:
	case ZANIM_SPIN_CHARGE_WALK:
		return TRUE;
	default:
		return FALSE;
	}
}

void CLinkPlayer::ZAnimate( void )
{
	if ( pev->modelindex != sAnimCache.modelindex )
		ZL_BuildAnimCache( this );

	int anim = ZL_PickAnim( this );
	const zl_anim_def_t &def = sAnims[anim];

	// One shots restart when the action restarts (combos, chained rolls)
	int key = anim | ( ZL_GetCombo( pev->iuser4 ) << 8 ) | ( ZAction() << 12 );
	BOOL restart = ( key != m_iZAnimState ) || ( !def.loop && ZActionTime() < 0.02f && m_iZAnim == anim );

	m_iZAnimState = key;
	m_iZAnim = anim;

	// Legs
	Vector vel = pev->velocity;
	vel.z = 0;
	float speed = vel.Length();

	if ( sAnimCache.full[anim] && ( anim == ZANIM_CARRY || anim == ZANIM_CARRY_WALK ) && sAnimCache.full[ZANIM_LEGS_RUN] )
	{
		// Upper body only: the renderer takes everything below "Bip01 Spine" (link.mdl's
		// UpperControl limb) from the gait sequence
		if ( speed > ZL_VEL( 3.5f ) )
			pev->gaitsequence = sAnimCache.seq[ZANIM_LEGS_RUN];
		else if ( speed > 5 )
			pev->gaitsequence = sAnimCache.seq[ZANIM_LEGS_WALK];
		else
			pev->gaitsequence = sAnimCache.seq[ZANIM_LEGS_WAIT];
	}
	else if ( sAnimCache.full[anim] )
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

		// Fit HL's stand-in one shots to the OoT action length so they don't loop or cut off
		if ( !sAnimCache.full[anim] && !def.loop )
		{
			float t = 0;
			switch ( ZAction() )
			{
			case ZACT_SLASH:	t = ZL_SLASH_TIME; break;
			case ZACT_ROLL:		t = ZL_ROLL_TIME; break;
			case ZACT_SPIN:		t = ZL_SPIN_TIME; break;
			case ZACT_LAND:		t = ZL_LAND_TIME * 2; break;
			case ZACT_DAMAGE:	t = ZL_DAMAGE_TIME; break;
			case ZACT_KNOCKDOWN: t = ZL_KNOCKDOWN_TIME * 0.6f; break;
			case ZACT_CLIMB:	t = ZL_CLIMB_TIME; break;
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
		float rate = ( oot * 0.3f + 1.0f ) * 1.5f;	// cycle frames per OoT frame
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

	// One shots (knockdowns, item get, the chest) hold their last frame instead of looping
	if ( !def.loop && m_fSequenceFinished )
	{
		pev->frame = 255;
		pev->framerate = 0;
	}
}
