/***
*
*	Zelda 64 port: talking to people, user messages and the client commands they answer with.
*
*	The text itself lives on the client (zelda/resource/zl_messages.txt). The server only says
*	"open message X, spoken by entity Y" and waits for "zmsg_done <choice>".
*
*	Scientists and security guards talk through the OoT box when you press A in front of them.
*	Following works like OoT's two choice prompts: "Follow me" / "Stay here".
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "talkmonster.h"
#include "zl_player.h"

int gmsgZStats = 0;
int gmsgZButtons = 0;
int gmsgZTarget = 0;
int gmsgZText = 0;
int gmsgZMusic = 0;
int gmsgZItemGet = 0;
int gmsgZFx = 0;

BOOL ZL_ItemCommand( CLinkPlayer *pLink, const char *pcmd );

void ZL_RegisterMessages( void )
{
	gmsgZStats = REG_USER_MSG( ZMSG_STATS, -1 );
	gmsgZButtons = REG_USER_MSG( ZMSG_BUTTONS, -1 );
	gmsgZTarget = REG_USER_MSG( ZMSG_TARGET, -1 );
	gmsgZText = REG_USER_MSG( ZMSG_TEXT, -1 );
	gmsgZMusic = REG_USER_MSG( ZMSG_MUSIC, -1 );
	gmsgZItemGet = REG_USER_MSG( ZMSG_ITEMGET, -1 );
	gmsgZFx = REG_USER_MSG( ZMSG_FX, -1 );
}

// A hit effect (ZFX_*) for everyone who can see pos
void ZL_SendFx( int type, const Vector &pos )
{
	if ( !gmsgZFx )
		return;
	MESSAGE_BEGIN( MSG_PVS, gmsgZFx, pos );
		WRITE_BYTE( type );
		WRITE_COORD( pos.x );
		WRITE_COORD( pos.y );
		WRITE_COORD( pos.z );
	MESSAGE_END();
}

// What the open message was asking, so the answer can be acted on
enum
{
	ZTALK_PLAIN = 0,
	ZTALK_FOLLOW_ASK,	// "Should I come with you?"  Yes / No
	ZTALK_STAY_ASK,		// "Want me to wait here?"     Yes / No
	ZTALK_TRIGGER,		// trigger_zmessage / zl_sign: fire targets when done
};

void CLinkPlayer::ZStartMessage( const char *pszMessage, CBaseEntity *pSpeaker, int kind )
{
	m_hZTalkTo = pSpeaker ? pSpeaker : (CBaseEntity *)this;
	m_iZTalkKind = kind;
	m_flZTalkStart = gpGlobals->time;

	if ( ZAction() != ZACT_GETITEM )
		ZSetAction( ZACT_TALK );

	pev->velocity.x = pev->velocity.y = 0;
	ZUpdateServerFlags();

	MESSAGE_BEGIN( MSG_ONE, gmsgZText, NULL, pev );
		WRITE_BYTE( ZTEXT_OPEN );
		WRITE_STRING( pszMessage );
		WRITE_SHORT( pSpeaker ? pSpeaker->entindex() : 0 );
	MESSAGE_END();
}

// Sentence group prefix (SC_ / BA_) picks the text for that kind of NPC
static const char *ZL_TalkPrefix( CBaseEntity *pNPC )
{
	if ( FClassnameIs( pNPC->pev, "monster_barney" ) )
		return "BA";
	if ( FClassnameIs( pNPC->pev, "monster_scientist" ) || FClassnameIs( pNPC->pev, "monster_sitting_scientist" ) )
		return "SC";
	return "NPC";
}

void CLinkPlayer::ZTalkTo( CBaseEntity *pNPC )
{
	CTalkMonster *pTalker = NULL;
	const char *prefix = ZL_TalkPrefix( pNPC );
	char msg[64];
	int kind = ZTALK_PLAIN;

	if ( !strcmp( prefix, "BA" ) || !strcmp( prefix, "SC" ) )
		pTalker = static_cast<CTalkMonster *>( pNPC );

	// Mapper text wins
	if ( !FStringNull( pNPC->pev->message ) )
	{
		ZStartMessage( STRING( pNPC->pev->message ), pNPC, ZTALK_PLAIN );
	}
	else
	{
		if ( pTalker && pTalker->IsFollowing() )
		{
			snprintf( msg, sizeof( msg ), "%s_STAY_ASK", prefix );
			kind = ZTALK_STAY_ASK;
		}
		else if ( pTalker && pTalker->CanFollow() )
		{
			snprintf( msg, sizeof( msg ), "%s_FOLLOW_ASK", prefix );
			kind = ZTALK_FOLLOW_ASK;
		}
		else
		{
			// A random line from the idle pool (the client picks the variant)
			snprintf( msg, sizeof( msg ), "%s_IDLE", prefix );
		}

		ZStartMessage( msg, pNPC, kind );
	}

	if ( pTalker )
	{
		// Turn to face Link and say hello out loud, like Player_StartTalking setting focusActor
		pTalker->m_hTalkTarget = this;
		pTalker->pev->ideal_yaw = UTIL_VecToYaw( pev->origin - pTalker->pev->origin );
		pTalker->IdleHeadTurn( pev->origin );

		if ( pTalker->FOkToSpeak() && pTalker->m_szGrp[TLK_HELLO] )
			pTalker->PlaySentence( pTalker->m_szGrp[TLK_HELLO], 3, VOL_NORM, ATTN_IDLE );
	}

	// Link looks at who he is talking to
	pev->fuser1 = UTIL_VecToYaw( pNPC->pev->origin - pev->origin );
}

/*
 OoT holds the world still while you read: talking puts Link in a cutscene state that most
 actors check before they act. Here every monster stops thinking and animating until the box
 closes, and whoever Link is talking to turns its body and head to him (Player_StartTalking /
 the NPC's own "face the player" update).
*/
#define ZL_MAX_FROZEN 256
static EHANDLE s_hFrozen[ZL_MAX_FROZEN];
static float s_flFrozenRate[ZL_MAX_FROZEN];
static int s_iNumFrozen;

void CLinkPlayer::ZFreezeWorld( BOOL freeze )
{
	if ( !freeze )
	{
		for ( int i = 0; i < s_iNumFrozen; i++ )
		{
			CBaseEntity *pEnt = s_hFrozen[i];
			if ( !pEnt )
				continue;
			pEnt->pev->framerate = s_flFrozenRate[i];
			if ( pEnt->pev->nextthink > 0 )
				pEnt->pev->nextthink = gpGlobals->time + 0.1f;
		}
		s_iNumFrozen = 0;
		m_fZWorldFrozen = FALSE;
		return;
	}

	m_fZWorldFrozen = TRUE;
	CBaseEntity *pSpeaker = m_hZTalkTo;

	for ( int e = 1; e < gpGlobals->maxEntities; e++ )
	{
		edict_t *ed = INDEXENT( e );
		if ( !ed || ed->free || !( ed->v.flags & FL_MONSTER ) || ( ed->v.flags & FL_CLIENT ) )
			continue;
		CBaseEntity *pEnt = CBaseEntity::Instance( ed );
		if ( !pEnt || !pEnt->IsAlive() )
			continue;

		int slot = -1;
		for ( int i = 0; i < s_iNumFrozen; i++ )
			if ( (CBaseEntity *)s_hFrozen[i] == pEnt )
				slot = i;
		if ( slot < 0 )
		{
			if ( s_iNumFrozen >= ZL_MAX_FROZEN )
				continue;
			slot = s_iNumFrozen++;
			s_hFrozen[slot] = pEnt;
			s_flFrozenRate[slot] = pEnt->pev->framerate;
		}

		// No AI, no animation, no drifting
		if ( pEnt->pev->nextthink > 0 )
			pEnt->pev->nextthink = gpGlobals->time + 0.1f;
		pEnt->pev->velocity.x = pEnt->pev->velocity.y = 0;

		CBaseMonster *pMon = pEnt->MyMonsterPointer();
		if ( pEnt == pSpeaker && pMon && !FClassnameIs( pEnt->pev, "monster_gossip_stone" ) )
		{
			// The one Link talks to keeps its talking animation and looks at him
			pEnt->pev->framerate = s_flFrozenRate[slot];
			pMon->pev->ideal_yaw = UTIL_VecToYaw( pev->origin - pEnt->pev->origin );
			pMon->ChangeYaw( pMon->pev->yaw_speed > 0 ? (int)pMon->pev->yaw_speed : 20 );
			if ( FClassnameIs( pEnt->pev, "monster_scientist" ) || FClassnameIs( pEnt->pev, "monster_barney" ) )
				static_cast<CTalkMonster *>( pMon )->IdleHeadTurn( pev->origin );
			pMon->StudioFrameAdvance();
		}
		else
		{
			pEnt->pev->framerate = 0;
		}
	}
}

void CLinkPlayer::ZMessageDone( int choice )
{
	CBaseEntity *pSpeaker = m_hZTalkTo;
	int kind = m_iZTalkKind;

	m_hZTalkTo = NULL;
	m_iZTalkKind = ZTALK_PLAIN;
	m_flZTalkEndTime = gpGlobals->time;

	if ( ZAction() == ZACT_TALK || ZAction() == ZACT_GETITEM )
		ZSetAction( ZACT_STAND );

	ZUpdateServerFlags();

	if ( !pSpeaker || pSpeaker == this )
		return;

	CTalkMonster *pTalker = NULL;
	if ( FClassnameIs( pSpeaker->pev, "monster_barney" ) || FClassnameIs( pSpeaker->pev, "monster_scientist" ) )
		pTalker = static_cast<CTalkMonster *>( pSpeaker );

	switch ( kind )
	{
	case ZTALK_FOLLOW_ASK:
		if ( pTalker && choice == 0 && pTalker->CanFollow() )
		{
			pTalker->StartFollowing( this );
			if ( pTalker->m_szGrp[TLK_USE] )
				pTalker->PlaySentence( pTalker->m_szGrp[TLK_USE], 3, VOL_NORM, ATTN_IDLE );
		}
		break;

	case ZTALK_STAY_ASK:
		if ( pTalker && choice == 0 )
		{
			pTalker->StopFollowing( TRUE );
			if ( pTalker->m_szGrp[TLK_UNUSE] )
				pTalker->PlaySentence( pTalker->m_szGrp[TLK_UNUSE], 3, VOL_NORM, ATTN_IDLE );
		}
		break;

	case ZTALK_TRIGGER:
		pSpeaker->Use( this, this, choice == 0 ? USE_ON : USE_OFF, (float)choice );
		break;
	}
}

/*
=============================================================================
 trigger_zmessage: point entity that shows an OoT text box to the player when triggered.
	message		zl_messages.txt entry
	target		fired when the box closes (USE_ON for the first choice, USE_OFF otherwise)
	spawnflags	1 = only once
=============================================================================
*/
class CTriggerZMessage : public CPointEntity
{
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	BOOL m_fWaiting;
};

LINK_ENTITY_TO_CLASS( trigger_zmessage, CTriggerZMessage );

void CTriggerZMessage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CLinkPlayer *pLink = ZL_GetLink( pCaller );

	// Coming back from the box closing
	if ( pLink && m_fWaiting && pCaller == pActivator )
	{
		m_fWaiting = FALSE;
		SUB_UseTargets( pActivator, useType, value );
		if ( pev->spawnflags & 1 )
			UTIL_Remove( this );
		return;
	}

	if ( !pLink )
		pLink = ZL_GetLink( pActivator );
	if ( !pLink )
		pLink = ZL_GetLink( UTIL_PlayerByIndex( 1 ) );
	if ( !pLink || !pLink->LinkActive() || FStringNull( pev->message ) )
		return;

	m_fWaiting = TRUE;
	pLink->ZStartMessage( STRING( pev->message ), this, ZTALK_TRIGGER );
}

/*
=============================================================================
 zl_sign: something to read with A ("Check"). Point entity or brush.
=============================================================================
*/
class CZeldaSign : public CBaseEntity
{
public:
	void Spawn( void )
	{
		if ( !FStringNull( pev->model ) )
		{
			SET_MODEL( ENT( pev ), STRING( pev->model ) );
			pev->solid = SOLID_BSP;
			pev->movetype = MOVETYPE_PUSH;
		}
		else
		{
			pev->solid = SOLID_NOT;
			UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 48 ) );
		}
		UTIL_SetOrigin( pev, pev->origin );
	}

	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE; }

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CLinkPlayer *pLink = ZL_GetLink( pActivator );
		if ( !pLink || FStringNull( pev->message ) )
			return;

		// The second Use comes from ZMessageDone once the box is closed
		if ( m_fWaiting )
		{
			m_fWaiting = FALSE;
			SUB_UseTargets( pActivator, useType, value );
			return;
		}

		m_fWaiting = TRUE;
		pLink->ZStartMessage( STRING( pev->message ), this, ZTALK_TRIGGER );
	}

	BOOL m_fWaiting;
};

LINK_ENTITY_TO_CLASS( zl_sign, CZeldaSign );
LINK_ENTITY_TO_CLASS( func_zl_sign, CZeldaSign );	// brush version for the FGD

/*
=============================================================================
 Client commands
=============================================================================
*/
BOOL ZL_ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	CLinkPlayer *pLink = ZL_GetLink( pPlayer );
	if ( !pLink )
		return FALSE;

	if ( FStrEq( pcmd, "zmsg_done" ) )
	{
		int choice = CMD_ARGC() > 1 ? atoi( CMD_ARGV( 1 ) ) : 0;
		pLink->ZMessageDone( choice );
		return TRUE;
	}

	if ( FStrEq( pcmd, "zmsg_test" ) )
	{
		// zmsg_test <message name>: show any entry from zl_messages.txt
		if ( CMD_ARGC() > 1 )
			pLink->ZStartMessage( CMD_ARGV( 1 ), NULL, ZTALK_PLAIN );
		return TRUE;
	}

	return ZL_ItemCommand( pLink, pcmd );
}
