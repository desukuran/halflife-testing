/***
*
*	Zelda 64 port: music entities. The client plays the tracks (cl_dll/zl_music.cpp, lazyusf2).
*
*	ambient_zmusic
*		track		file under the mod folder, e.g. "music/oot/kokiri_forest.miniusf"
*		fadein		seconds to fade in (0 = cut in)
*		fadeout		seconds to fade out when stopped or replaced
*		volume		0-10 (10 = full)
*		spawnflags	1 = play on level start
*					2 = don't loop (USF rips loop forever otherwise; uses the length tag)
*					4 = stop when triggered again (toggle) instead of restarting
*
*	Triggering it starts the track (or toggles with flag 4). Triggering with USE_OFF fades out.
*
*	trigger_zmusic_stop
*		fadeout		seconds
*		Triggering it fades out whatever is playing.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "zl_player.h"

#define SF_ZMUSIC_START_ON		1
#define SF_ZMUSIC_NO_LOOP		2
#define SF_ZMUSIC_TOGGLE		4

// The last track asked for, so players joining / loading a save hear it too
static char		g_szZMusicTrack[128];
static float	g_flZMusicVolume = 1.0f;
static int		g_iZMusicLoop = 1;

static void ZL_SendMusic( edict_t *pTo, int cmd, const char *track, float fade, float volume, int loop )
{
	if ( !gmsgZMusic )
		return;

	if ( pTo )
		MESSAGE_BEGIN( MSG_ONE, gmsgZMusic, NULL, pTo );
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgZMusic );

	WRITE_BYTE( cmd );
	WRITE_STRING( track ? track : "" );
	WRITE_SHORT( (int)( fade * 100 ) );
	WRITE_BYTE( (int)( volume * 255 ) );
	WRITE_BYTE( loop );
	MESSAGE_END();
}

void ZL_MusicPlay( const char *track, float fadein, float volume, int loop )
{
	strncpy( g_szZMusicTrack, track, sizeof( g_szZMusicTrack ) - 1 );
	g_szZMusicTrack[sizeof( g_szZMusicTrack ) - 1] = 0;
	g_flZMusicVolume = volume;
	g_iZMusicLoop = loop;
	ZL_SendMusic( NULL, ZMUS_PLAY, track, fadein, volume, loop );
}

void ZL_MusicStop( float fadeout )
{
	g_szZMusicTrack[0] = 0;
	ZL_SendMusic( NULL, fadeout > 0 ? ZMUS_FADEOUT : ZMUS_STOP, "", fadeout, 0, 0 );
}

// New client (or save loaded): resend what should be playing
void ZL_MusicResend( edict_t *pPlayer )
{
	if ( g_szZMusicTrack[0] )
		ZL_SendMusic( pPlayer, ZMUS_PLAY, g_szZMusicTrack, 0.5f, g_flZMusicVolume, g_iZMusicLoop );
}

class CAmbientZMusic : public CPointEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT StartThink( void );
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	string_t	m_iszTrack;
	float		m_flFadeIn;
	float		m_flFadeOut;
	float		m_flVolume;
	BOOL		m_fPlaying;
};

LINK_ENTITY_TO_CLASS( ambient_zmusic, CAmbientZMusic );

TYPEDESCRIPTION CAmbientZMusic::m_SaveData[] =
{
	DEFINE_FIELD( CAmbientZMusic, m_iszTrack, FIELD_STRING ),
	DEFINE_FIELD( CAmbientZMusic, m_flFadeIn, FIELD_FLOAT ),
	DEFINE_FIELD( CAmbientZMusic, m_flFadeOut, FIELD_FLOAT ),
	DEFINE_FIELD( CAmbientZMusic, m_flVolume, FIELD_FLOAT ),
	DEFINE_FIELD( CAmbientZMusic, m_fPlaying, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CAmbientZMusic, CPointEntity );

void CAmbientZMusic::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "track" ) )
	{
		m_iszTrack = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fadein" ) )
	{
		m_flFadeIn = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fadeout" ) )
	{
		m_flFadeOut = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "volume" ) )
	{
		m_flVolume = atof( pkvd->szValue ) / 10.0f;
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CAmbientZMusic::Spawn( void )
{
	if ( m_flVolume <= 0 )
		m_flVolume = 1.0f;

	if ( pev->spawnflags & SF_ZMUSIC_START_ON )
	{
		// Wait for the client to be connected and its user messages registered
		SetThink( &CAmbientZMusic::StartThink );
		pev->nextthink = gpGlobals->time + 1.0f;
	}
}

void CAmbientZMusic::StartThink( void )
{
	if ( !UTIL_PlayerByIndex( 1 ) )
	{
		pev->nextthink = gpGlobals->time + 0.5f;
		return;
	}

	Use( this, this, USE_ON, 0 );
	SetThink( NULL );
}

void CAmbientZMusic::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FStringNull( m_iszTrack ) )
		return;

	BOOL turnOn;
	if ( useType == USE_ON )
		turnOn = TRUE;
	else if ( useType == USE_OFF )
		turnOn = FALSE;
	else
		turnOn = ( pev->spawnflags & SF_ZMUSIC_TOGGLE ) ? !m_fPlaying : TRUE;

	if ( turnOn )
	{
		ZL_MusicPlay( STRING( m_iszTrack ), m_flFadeIn, m_flVolume, ( pev->spawnflags & SF_ZMUSIC_NO_LOOP ) ? 0 : 1 );
		m_fPlaying = TRUE;
	}
	else
	{
		ZL_MusicStop( m_flFadeOut );
		m_fPlaying = FALSE;
	}
}

class CTriggerZMusicStop : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd )
	{
		if ( FStrEq( pkvd->szKeyName, "fadeout" ) )
		{
			pev->dmg = atof( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else
			CPointEntity::KeyValue( pkvd );
	}

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		ZL_MusicStop( pev->dmg );
	}
};

LINK_ENTITY_TO_CLASS( trigger_zmusic_stop, CTriggerZMusicStop );
