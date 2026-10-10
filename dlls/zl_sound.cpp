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

static int s_zlTakes[ZS_COUNT];		// takes per OoT sound, from sound/zelda/oot/variants.txt
static char s_zlTakesKey[ZS_COUNT + 1];	// the same as the "zv" physinfo string
static BOOL s_zlOoTInstalled;			// variants.txt found: extract_sfx.py has been run

BOOL ZL_UseOoTSounds( void )
{
	if ( zl_sfx_oot.value < 0 )
		return s_zlOoTInstalled;
	return zl_sfx_oot.value != 0;
}

const char *ZL_SoundPath( int sound )
{
	static char path[96];

	if ( sound < 0 || sound >= ZS_COUNT )
		return "common/null.wav";

	if ( ZL_UseOoTSounds() )
	{
		int takes = s_zlTakes[sound] > 1 ? s_zlTakes[sound] : 1;
		ZL_OoTSoundPath( path, sizeof( path ), sound, takes > 1 ? RANDOM_LONG( 1, takes ) : 1 );
		return path;
	}

	return g_zlSounds[sound].hl;
}

const char *ZL_SoundTakesKey( void )
{
	return s_zlTakesKey;
}

// sound/zelda/oot/variants.txt: "NA_SE_VO_LI_SWORD_N 4" lines
static void ZL_LoadSoundTakes( void )
{
	for ( int i = 0; i < ZS_COUNT; i++ )
	{
		s_zlTakes[i] = 1;
		s_zlTakesKey[i] = '1';
	}
	s_zlTakesKey[ZS_COUNT] = 0;

	int len = 0;
	char *data = (char *)LOAD_FILE_FOR_ME( (char *)"sound/zelda/oot/variants.txt", &len );
	s_zlOoTInstalled = data != NULL;
	if ( !data )
		return;

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
		{
			if ( !strcmp( g_zlSounds[i].oot, name ) )
			{
				s_zlTakes[i] = max( 1, min( 9, count ) );
				s_zlTakesKey[i] = '0' + s_zlTakes[i];
			}
		}
	}
	free( text );
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
const char *ZL_PlayerModel( void )
{
	static char checked[128];
	static BOOL exists;
	const char *want = ( zl_model.string && zl_model.string[0] ) ? zl_model.string : "models/player.mdl";

	if ( strcmp( checked, want ) )
	{
		int len = 0;
		byte *data = LOAD_FILE_FOR_ME( (char *)want, &len );
		exists = data != NULL;
		if ( data )
			FREE_FILE( data );
		strncpy( checked, want, sizeof( checked ) - 1 );
		if ( !exists )
			ALERT( at_console, "Zelda: %s not found, using models/player.mdl\n", want );
	}
	return exists ? want : "models/player.mdl";
}

// Called from ClientPrecache (world spawn), the only time precaching is allowed
void ZL_PrecacheAll( void )
{
	if ( !ZL_Enabled() )
		return;

	ZL_LoadSoundTakes();
	for ( int i = 0; i < ZS_COUNT; i++ )
	{
		if ( ZL_UseOoTSounds() )
		{
			// Every take; the path buffers have to outlive the precache call
			for ( int t = 1; t <= s_zlTakes[i]; t++ )
			{
				char path[96];
				ZL_OoTSoundPath( path, sizeof( path ), i, t );
				PRECACHE_SOUND( (char *)STRING( ALLOC_STRING( path ) ) );
			}
		}
		else
			PRECACHE_SOUND( (char *)g_zlSounds[i].hl );
	}

	PRECACHE_MODEL( (char *)ZL_PlayerModel() );

	ZL_PrecacheItems();
}
