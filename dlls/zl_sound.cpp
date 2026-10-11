/***
*
*	Zelda 64 port: sound table glue (pm_shared/zl_sounds.h) and precaching.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "zl_player.h"
#include "zl_sounds.h"

void ZL_PrecacheItems( void );

static char s_zlTakesKey[ZS_COUNT + 1];	// the "zv" physinfo string (zl_sounds.h), decided at precache

const char *ZL_SoundPath( int sound )
{
	static char path[96];
	return ZL_SoundSample( path, sizeof( path ), s_zlTakesKey, sound, RANDOM_LONG( 0, 0x7FFF ) );
}

const char *ZL_SoundTakesKey( void )
{
	return s_zlTakesKey;
}

BOOL ZL_FileExists( const char *path )
{
	int len = 0;
	byte *data = LOAD_FILE_FOR_ME( (char *)path, &len );
	if ( !data )
		return FALSE;
	FREE_FILE( data );
	return TRUE;
}

// Per sound: the OoT file if it's there (and zl_oot_sounds isn't 0), else the Half-Life one.
// sound/zelda/oot/variants.txt ("NA_SE_VO_LI_SWORD_N 4" lines) gives the extra takes.
static void ZL_ChooseSounds( void )
{
	int takes[ZS_COUNT];
	for ( int i = 0; i < ZS_COUNT; i++ )
		takes[i] = 1;

	int len = 0;
	char *data = (char *)LOAD_FILE_FOR_ME( (char *)"sound/zelda/oot/variants.txt", &len );
	if ( data )
	{
		char *text = (char *)malloc( len + 1 );
		memcpy( text, data, len );
		text[len] = 0;
		FREE_FILE( data );

		for ( char *line = strtok( text, "\r\n" ); line; line = strtok( NULL, "\r\n" ) )
		{
			char name[64];
			int count;
			if ( line[0] == '/' || sscanf( line, "%63s %d", name, &count ) != 2 )
				continue;
			for ( int i = 0; i < ZS_COUNT; i++ )
				if ( !strcmp( g_zlSounds[i].oot, name ) )
					takes[i] = max( 1, min( 9, count ) );
		}
		free( text );
	}

	for ( int i = 0; i < ZS_COUNT; i++ )
	{
		char path[128];
		snprintf( path, sizeof( path ), "sound/zelda/oot/%s.wav", g_zlSounds[i].oot );
		BOOL oot = zl_oot_sounds.value != 0 && ZL_FileExists( path );
		s_zlTakesKey[i] = oot ? (char)( '0' + takes[i] ) : '0';
	}
	s_zlTakesKey[ZS_COUNT] = 0;
}

void ZL_EmitSound( edict_t *pEdict, int channel, int sound, float volume, float attn, int pitch )
{
	// OoT's per sound random pitch on top of whatever pitch the caller asked for
	pitch = ZL_SoundPitch( sound, pitch / 100.0f, RANDOM_LONG( 0, 0x7FFF ) );
	EMIT_SOUND_DYN( pEdict, channel, ZL_SoundPath( sound ), volume, attn, 0, pitch );
}

void CLinkPlayer::ZSound( int channel, int sound, float volume, float attn, int pitch )
{
	ZL_EmitSound( edict(), channel, sound, volume, attn, pitch );
}

// Precaching a model that isn't there is fatal, and link.mdl only exists once you have built it
// from your own ROM (tools/zelda/make_link_model.py)
// zl_model: "auto" (link.mdl if you've built it, else player.mdl), "hl" (player.mdl) or a path.
// Older builds saved "models/player.mdl" as the default into config.cfg; that counts as auto.
const char *ZL_PlayerModel( void )
{
	static char checked[128];
	static const char *result = "models/player.mdl";
	const char *want = zl_model.string ? zl_model.string : "";

	if ( strcmp( checked, want ) )
	{
		strncpy( checked, want, sizeof( checked ) - 1 );
		const char *path = want;
		if ( !want[0] || !stricmp( want, "auto" ) || !stricmp( want, "models/player.mdl" ) )
			path = "models/player/link/link.mdl";
		else if ( !stricmp( want, "hl" ) )
			path = "models/player.mdl";

		if ( ZL_FileExists( path ) )
			result = STRING( ALLOC_STRING( path ) );
		else
		{
			result = "models/player.mdl";
			ALERT( at_console, "Zelda: %s not found, using models/player.mdl\n", path );
		}
		ALERT( at_console, "Zelda: player model %s\n", result );
	}
	return result;
}

// Called from ClientPrecache (world spawn), the only time precaching is allowed
void ZL_PrecacheAll( void )
{
	if ( !ZL_Enabled() )
		return;

	// Decided once per map: switching zl_oot_sounds takes effect on the next map load, so a
	// sound that wasn't precached is never asked for
	ZL_ChooseSounds();
	int nOoT = 0, nHL = 0;
	for ( int i = 0; i < ZS_COUNT; i++ )
	{
		int takes = ZL_SoundTakes( s_zlTakesKey, i );
		if ( !takes )
		{
			PRECACHE_SOUND( (char *)g_zlSounds[i].hl );
			nHL++;
			continue;
		}
		for ( int t = 1; t <= takes; t++ )
		{
			char path[96];
			ZL_OoTSoundPath( path, sizeof( path ), i, t );
			PRECACHE_SOUND( (char *)STRING( ALLOC_STRING( path ) ) );	// the name has to outlive this call
			nOoT++;
		}
	}
	ALERT( at_console, "Zelda: precached %d OoT sound files and %d Half-Life stand-ins\n", nOoT, nHL );

	PRECACHE_MODEL( (char *)ZL_PlayerModel() );

	ZL_PrecacheItems();
}
