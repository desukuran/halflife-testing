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

const char *ZL_SoundPath( int sound )
{
	static char path[ZS_COUNT][96];

	if ( sound < 0 || sound >= ZS_COUNT )
		return "common/null.wav";

	if ( zl_sfx_oot.value )
	{
		if ( !path[sound][0] )
			snprintf( path[sound], sizeof( path[sound] ), "zelda/oot/%s.wav", g_zlSounds[sound].oot );
		return path[sound];
	}

	return g_zlSounds[sound].hl;
}

void ZL_EmitSound( edict_t *pEdict, int channel, int sound, float volume, float attn, int pitch )
{
	EMIT_SOUND_DYN( pEdict, channel, ZL_SoundPath( sound ), volume, attn, 0, pitch );
}

void CLinkPlayer::ZSound( int channel, int sound, float volume, float attn, int pitch )
{
	ZL_EmitSound( edict(), channel, sound, volume, attn, pitch );
}

// Called from ClientPrecache (world spawn), the only time precaching is allowed
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

void ZL_PrecacheAll( void )
{
	if ( !ZL_Enabled() )
		return;

	for ( int i = 0; i < ZS_COUNT; i++ )
		PRECACHE_SOUND( (char *)ZL_SoundPath( i ) );

	PRECACHE_MODEL( (char *)ZL_PlayerModel() );

	ZL_PrecacheItems();
}
