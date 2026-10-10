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
void ZL_PrecacheAll( void )
{
	if ( !ZL_Enabled() )
		return;

	for ( int i = 0; i < ZS_COUNT; i++ )
		PRECACHE_SOUND( (char *)ZL_SoundPath( i ) );

	if ( zl_model.string && zl_model.string[0] )
		PRECACHE_MODEL( (char *)zl_model.string );

	ZL_PrecacheItems();
}
