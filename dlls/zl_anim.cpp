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
	ZANIM_GETITEM,
	ZANIM_CARRY,
	ZANIM_CARRY_WALK,
	ZANIM_SWIM,
	ZANIM_SWIM_IDLE,
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
	{ "gPlayerAnim_link_normal_talk_free",			{ "link_normal_talk_free", "ref_aim_crowbar" },			"deep_idle", TRUE },
	{ "gPlayerAnim_link_demo_get_itemA",			{ "link_demo_get_itemA", "ref_aim_squeak", "ref_aim_trip" }, "deep_idle", TRUE },
	{ "gPlayerAnim_link_normal_carryB_wait",		{ "link_normal_carryB_wait", "ref_aim_squeak", "ref_aim_trip" }, ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_normal_carryB",				{ "link_normal_carryB", "ref_aim_squeak", "ref_aim_trip" }, ZGAIT_AUTO, TRUE },
	{ "gPlayerAnim_link_swimer_swim",				{ "link_swimer_swim", "swim" },							ZGAIT_NONE, TRUE },
	{ "gPlayerAnim_link_swimer_swim_wait",			{ "link_swimer_swim_wait", "treadwater" },				ZGAIT_NONE, TRUE },
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
			int s = LookupSequence( pmodel, def.seq[j] );
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
	case ZACT_TALK:			return ZANIM_TALK;
	case ZACT_GETITEM:		return ZANIM_GETITEM;

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

	// Freeze knockdowns on their last frame instead of looping
	if ( !def.loop && m_fSequenceFinished )
	{
		pev->frame = 255;
		pev->framerate = 0;
	}
}
