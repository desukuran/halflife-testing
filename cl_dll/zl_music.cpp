/***
*
*	Zelda 64 port: music player.
*
*	OoT's music is sequenced: the N64 plays it with its own audio microcode. The standard rip
*	format is USF / miniUSF (a save state of the game's sound driver plus a tiny bit of code
*	that starts one song), so playing it means emulating the N64. That's lazyusf2's job
*	(external/lazyusf2, psflib for the PSF container, zlib to unpack it).
*
*	Plain .wav files work too (SDL_LoadWAV), for pre-rendered or non OoT tracks.
*
*	Output goes through its own SDL2 audio device, separate from the engine's sound.
*
*	Console:
*		zmus_play <file> [fade seconds]	file is relative to the mod folder, e.g.
*										music/oot/kokiri_forest.miniusf
*		zmus_stop [fade seconds]
*		zmus_info
*		zmus_volume 0..1				(multiplied by the game's MP3Volume)
*		zmus_enable 0/1
*		zmus_hle 0/1					lazyusf2 HLE audio (fast). 0 = low level RSP (slow, exact)
*
*	Built with ZL_NO_USF defined, lazyusf2 is left out and only .wav plays.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "zl_client.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "SDL2/SDL.h"

#ifndef ZL_NO_USF
#include "lazyusf2/usf/usf.h"
#include "psflib/psflib.h"
#endif

#define ZMUS_CHANNELS	2

struct zl_track_t
{
	char		name[128];
	int			kind;			// 0 none, 1 usf, 2 wav
	void		*usf;			// lazyusf2 state
	Sint16		*wav;			// converted to the device format
	int			wavSamples;		// frames
	int			wavPos;
	int			loop;
	double		lengthSec;		// from the length tag (+ fade tag), 0 = unknown
	double		playedSec;
	float		volume;			// entity volume
	float		fade;			// current fade gain
	float		fadeTarget;
	float		fadeRate;		// gain per second
	int			enableCompare;
	int			enableFifoFull;
};

static SDL_AudioDeviceID	s_device;
static SDL_AudioSpec		s_spec;
static zl_track_t			s_cur;
static zl_track_t			s_next;		// waits for s_cur to fade out
static int					s_haveNext;
static float				s_masterVolume = 0.8f;
static int					s_initialized;

static cvar_t *zmus_volume;
static cvar_t *zmus_enable;
static cvar_t *zmus_hle;

/*
=============================================================================
 Loading
=============================================================================
*/
#ifndef ZL_NO_USF

struct zl_usf_load_t
{
	void	*state;
	int		enableCompare;
	int		enableFifoFull;
	double	length;
	double	fade;
};

static void *ZMus_fopen( void *context, const char *path )		{ return fopen( path, "rb" ); }
static size_t ZMus_fread( void *p, size_t size, size_t count, void *f ) { return fread( p, size, count, (FILE *)f ); }
static int ZMus_fseek( void *f, int64_t offset, int whence )	{ return fseek( (FILE *)f, (long)offset, whence ); }
static int ZMus_fclose( void *f )								{ return fclose( (FILE *)f ); }
static long ZMus_ftell( void *f )								{ return ftell( (FILE *)f ); }

static const psf_file_callbacks s_psfCallbacks =
{
	"\\/|:",
	NULL,
	ZMus_fopen,
	ZMus_fread,
	ZMus_fseek,
	ZMus_fclose,
	ZMus_ftell,
};

static int ZMus_UsfLoader( void *context, const uint8_t *exe, size_t exeSize, const uint8_t *reserved, size_t reservedSize )
{
	zl_usf_load_t *ld = (zl_usf_load_t *)context;
	if ( exe && exeSize > 0 )
		return -1;
	return usf_upload_section( ld->state, reserved, reservedSize );
}

// "m:ss.ms" style PSF time tags
static double ZMus_ParseTime( const char *s )
{
	double total = 0, part = 0;
	const char *p = s;

	while ( *p )
	{
		if ( *p == ':' )
		{
			total = ( total + part ) * 60.0;
			part = 0;
		}
		else if ( ( *p >= '0' && *p <= '9' ) || *p == '.' || *p == ',' )
		{
			part = atof( p );
			while ( *p && *p != ':' )
				p++;
			continue;
		}
		p++;
	}
	return total + part;
}

static int ZMus_UsfInfo( void *context, const char *name, const char *value )
{
	zl_usf_load_t *ld = (zl_usf_load_t *)context;

	if ( !stricmp( name, "_enablecompare" ) && *value )
		ld->enableCompare = 1;
	else if ( !stricmp( name, "_enablefifofull" ) && *value )
		ld->enableFifoFull = 1;
	else if ( !stricmp( name, "length" ) )
		ld->length = ZMus_ParseTime( value );
	else if ( !stricmp( name, "fade" ) )
		ld->fade = ZMus_ParseTime( value );
	return 0;
}

static int ZMus_LoadUsf( zl_track_t *t, const char *path )
{
	zl_usf_load_t ld;
	memset( &ld, 0, sizeof( ld ) );

	ld.state = malloc( usf_get_state_size() );
	if ( !ld.state )
		return 0;
	usf_clear( ld.state );

	if ( psf_load( path, &s_psfCallbacks, 0x21, ZMus_UsfLoader, &ld, ZMus_UsfInfo, &ld, 1, NULL, NULL ) < 0 )
	{
		usf_shutdown( ld.state );
		free( ld.state );
		gEngfuncs.Con_Printf( "zmus: couldn't load USF %s\n", path );
		return 0;
	}

	usf_set_compare( ld.state, ld.enableCompare );
	usf_set_fifo_full( ld.state, ld.enableFifoFull );
	usf_set_hle_audio( ld.state, zmus_hle ? (int)zmus_hle->value : 1 );

	t->usf = ld.state;
	t->kind = 1;
	t->lengthSec = ld.length > 0 ? ld.length + ld.fade : 0;
	return 1;
}

static void ZMus_FreeUsf( zl_track_t *t )
{
	if ( t->usf )
	{
		usf_shutdown( t->usf );
		free( t->usf );
		t->usf = NULL;
	}
}

#endif // ZL_NO_USF

static int ZMus_LoadWav( zl_track_t *t, const char *path )
{
	SDL_AudioSpec spec;
	Uint8 *buf = NULL;
	Uint32 len = 0;

	if ( !SDL_LoadWAV( path, &spec, &buf, &len ) )
	{
		gEngfuncs.Con_Printf( "zmus: couldn't load %s: %s\n", path, SDL_GetError() );
		return 0;
	}

	SDL_AudioCVT cvt;
	if ( SDL_BuildAudioCVT( &cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, ZMUS_CHANNELS, s_spec.freq ) < 0 )
	{
		SDL_FreeWAV( buf );
		return 0;
	}

	cvt.len = len;
	cvt.buf = (Uint8 *)SDL_malloc( len * ( cvt.len_mult > 0 ? cvt.len_mult : 1 ) );
	memcpy( cvt.buf, buf, len );
	SDL_FreeWAV( buf );

	if ( cvt.needed )
		SDL_ConvertAudio( &cvt );

	t->wav = (Sint16 *)cvt.buf;
	t->wavSamples = cvt.len_cvt / ( sizeof( Sint16 ) * ZMUS_CHANNELS );
	t->wavPos = 0;
	t->kind = 2;
	t->lengthSec = (double)t->wavSamples / s_spec.freq;
	return 1;
}

static void ZMus_FreeTrack( zl_track_t *t )
{
#ifndef ZL_NO_USF
	ZMus_FreeUsf( t );
#endif
	if ( t->wav )
	{
		SDL_free( t->wav );
		t->wav = NULL;
	}
	memset( t, 0, sizeof( *t ) );
}

static int ZMus_Load( zl_track_t *t, const char *track )
{
	char path[512];
	const char *ext = strrchr( track, '.' );

	memset( t, 0, sizeof( *t ) );
	strncpy( t->name, track, sizeof( t->name ) - 1 );
	snprintf( path, sizeof( path ), "%s/%s", gEngfuncs.pfnGetGameDirectory(), track );

	if ( ext && !stricmp( ext, ".wav" ) )
		return ZMus_LoadWav( t, path );

#ifndef ZL_NO_USF
	if ( ext && ( !stricmp( ext, ".usf" ) || !stricmp( ext, ".miniusf" ) ) )
		return ZMus_LoadUsf( t, path );
	gEngfuncs.Con_Printf( "zmus: %s: expected .miniusf, .usf or .wav\n", track );
#else
	gEngfuncs.Con_Printf( "zmus: %s: this build only plays .wav (built with ZL_NO_USF)\n", track );
#endif
	return 0;
}

/*
=============================================================================
 Audio thread
=============================================================================
*/
static void ZMus_Render( zl_track_t *t, Sint16 *out, int frames )
{
	switch ( t->kind )
	{
#ifndef ZL_NO_USF
	case 1:
		if ( usf_render_resampled( t->usf, out, frames, s_spec.freq ) )
		{
			memset( out, 0, frames * ZMUS_CHANNELS * sizeof( Sint16 ) );
			t->kind = 0;	// emulation error: go quiet
		}
		break;
#endif
	case 2:
	{
		int written = 0;
		while ( written < frames )
		{
			int avail = t->wavSamples - t->wavPos;
			if ( avail <= 0 )
			{
				if ( !t->loop )
				{
					memset( out + written * ZMUS_CHANNELS, 0, ( frames - written ) * ZMUS_CHANNELS * sizeof( Sint16 ) );
					t->kind = 0;
					return;
				}
				t->wavPos = 0;
				continue;
			}
			int n = min( avail, frames - written );
			memcpy( out + written * ZMUS_CHANNELS, t->wav + t->wavPos * ZMUS_CHANNELS, n * ZMUS_CHANNELS * sizeof( Sint16 ) );
			t->wavPos += n;
			written += n;
		}
		break;
	}
	default:
		memset( out, 0, frames * ZMUS_CHANNELS * sizeof( Sint16 ) );
		break;
	}
}

static void SDLCALL ZMus_Callback( void *userdata, Uint8 *stream, int len )
{
	Sint16 *out = (Sint16 *)stream;
	int frames = len / ( sizeof( Sint16 ) * ZMUS_CHANNELS );

	if ( s_cur.kind == 0 )
	{
		memset( stream, 0, len );
		return;
	}

	ZMus_Render( &s_cur, out, frames );

	float seconds = (float)frames / s_spec.freq;
	s_cur.playedSec += seconds;

	// Non looping tracks fade out at the end of their length tag
	if ( !s_cur.loop && s_cur.lengthSec > 0 && s_cur.playedSec >= s_cur.lengthSec && s_cur.fadeTarget > 0 )
	{
		s_cur.fadeTarget = 0;
		s_cur.fadeRate = 1.0f / 2.0f;
	}

	float gain0 = s_cur.fade;
	if ( s_cur.fade < s_cur.fadeTarget )
		s_cur.fade = min( s_cur.fadeTarget, s_cur.fade + s_cur.fadeRate * seconds );
	else if ( s_cur.fade > s_cur.fadeTarget )
		s_cur.fade = max( s_cur.fadeTarget, s_cur.fade - s_cur.fadeRate * seconds );
	float gain1 = s_cur.fade;

	float vol = s_masterVolume * s_cur.volume;
	for ( int i = 0; i < frames; i++ )
	{
		float g = vol * ( gain0 + ( gain1 - gain0 ) * ( (float)i / frames ) );
		for ( int c = 0; c < ZMUS_CHANNELS; c++ )
		{
			int v = (int)( out[i * ZMUS_CHANNELS + c] * g );
			out[i * ZMUS_CHANNELS + c] = (Sint16)( v > 32767 ? 32767 : v < -32768 ? -32768 : v );
		}
	}
}

/*
=============================================================================
 Control (main thread)
=============================================================================
*/
static int ZMus_OpenDevice( void )
{
	if ( s_device )
		return 1;

	if ( !SDL_WasInit( SDL_INIT_AUDIO ) && SDL_InitSubSystem( SDL_INIT_AUDIO ) < 0 )
	{
		gEngfuncs.Con_Printf( "zmus: SDL audio init failed: %s\n", SDL_GetError() );
		return 0;
	}

	SDL_AudioSpec want;
	memset( &want, 0, sizeof( want ) );
	want.freq = 44100;
	want.format = AUDIO_S16SYS;
	want.channels = ZMUS_CHANNELS;
	want.samples = 2048;
	want.callback = ZMus_Callback;

	s_device = SDL_OpenAudioDevice( NULL, 0, &want, &s_spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE );
	if ( !s_device )
	{
		gEngfuncs.Con_Printf( "zmus: couldn't open an audio device: %s\n", SDL_GetError() );
		return 0;
	}

	SDL_PauseAudioDevice( s_device, 0 );
	return 1;
}

static void ZMus_Swap( zl_track_t *incoming, float fadein )
{
	zl_track_t old;

	SDL_LockAudioDevice( s_device );
	old = s_cur;
	s_cur = *incoming;
	s_cur.fade = fadein > 0 ? 0.0f : 1.0f;
	s_cur.fadeTarget = 1.0f;
	s_cur.fadeRate = fadein > 0 ? 1.0f / fadein : 1.0f;
	SDL_UnlockAudioDevice( s_device );

	ZMus_FreeTrack( &old );
	memset( incoming, 0, sizeof( *incoming ) );
}

void ZMusic_Play( const char *track, float fadein, float volume, int loop )
{
	if ( !s_initialized || ( zmus_enable && !zmus_enable->value ) )
		return;
	if ( !ZMus_OpenDevice() )
		return;

	// Same track already playing: leave it alone (level transitions resend it)
	if ( s_cur.kind && !stricmp( s_cur.name, track ) && s_cur.fadeTarget > 0 )
	{
		s_cur.volume = volume;
		return;
	}

	zl_track_t t;
	if ( !ZMus_Load( &t, track ) )
		return;
	t.loop = loop;
	t.volume = volume > 0 ? volume : 1.0f;

	if ( s_cur.kind && fadein > 0 )
	{
		// Fade the old one out first, then start this one (ZMusic_Frame)
		if ( s_haveNext )
			ZMus_FreeTrack( &s_next );
		s_next = t;
		s_next.fadeRate = fadein;	// remembered fade in time
		s_haveNext = 1;

		SDL_LockAudioDevice( s_device );
		s_cur.fadeTarget = 0;
		s_cur.fadeRate = 1.0f / fadein;
		SDL_UnlockAudioDevice( s_device );
		return;
	}

	ZMus_Swap( &t, fadein );
}

void ZMusic_Stop( float fadeout )
{
	if ( !s_device )
		return;

	if ( s_haveNext )
	{
		ZMus_FreeTrack( &s_next );
		s_haveNext = 0;
	}

	if ( fadeout > 0 )
	{
		SDL_LockAudioDevice( s_device );
		s_cur.fadeTarget = 0;
		s_cur.fadeRate = 1.0f / fadeout;
		SDL_UnlockAudioDevice( s_device );
	}
	else
	{
		zl_track_t empty;
		memset( &empty, 0, sizeof( empty ) );
		ZMus_Swap( &empty, 0 );
	}
}

void ZMusic_Frame( float time )
{
	if ( !s_device )
		return;

	float mp3 = gEngfuncs.pfnGetCvarFloat( "MP3Volume" );
	s_masterVolume = ( zmus_volume ? zmus_volume->value : 0.8f ) * ( mp3 > 0 ? mp3 * 2.0f : 1.0f );
	if ( s_masterVolume > 1.5f )
		s_masterVolume = 1.5f;

	if ( zmus_enable && !zmus_enable->value && s_cur.kind )
		ZMusic_Stop( 0 );

	// Faded out: free it, start the queued one
	if ( s_cur.kind && s_cur.fade <= 0.0001f && s_cur.fadeTarget <= 0 )
	{
		if ( s_haveNext )
		{
			float fadein = s_next.fadeRate;
			s_next.fadeRate = 0;
			s_haveNext = 0;
			ZMus_Swap( &s_next, fadein );
		}
		else
		{
			zl_track_t empty;
			memset( &empty, 0, sizeof( empty ) );
			ZMus_Swap( &empty, 0 );
		}
	}
	else if ( !s_cur.kind && s_haveNext )
	{
		float fadein = s_next.fadeRate;
		s_next.fadeRate = 0;
		s_haveNext = 0;
		ZMus_Swap( &s_next, fadein );
	}
}

static void ZMus_Cmd_Play( void )
{
	if ( gEngfuncs.Cmd_Argc() < 2 )
	{
		gEngfuncs.Con_Printf( "zmus_play <file relative to the mod folder> [fade seconds]\n" );
		return;
	}
	float fade = gEngfuncs.Cmd_Argc() > 2 ? atof( gEngfuncs.Cmd_Argv( 2 ) ) : 0;
	ZMusic_Play( gEngfuncs.Cmd_Argv( 1 ), fade, 1.0f, 1 );
}

static void ZMus_Cmd_Stop( void )
{
	float fade = gEngfuncs.Cmd_Argc() > 1 ? atof( gEngfuncs.Cmd_Argv( 1 ) ) : 0;
	ZMusic_Stop( fade );
}

static void ZMus_Cmd_Info( void )
{
#ifndef ZL_NO_USF
	gEngfuncs.Con_Printf( "zmus: lazyusf2 built in\n" );
#else
	gEngfuncs.Con_Printf( "zmus: wav only\n" );
#endif
	if ( !s_device )
	{
		gEngfuncs.Con_Printf( "zmus: audio device not open\n" );
		return;
	}
	gEngfuncs.Con_Printf( "zmus: device %d Hz, %d ch\n", s_spec.freq, s_spec.channels );
	if ( s_cur.kind )
		gEngfuncs.Con_Printf( "zmus: playing %s (%s) %.1fs / %.1fs, fade %.2f -> %.2f, %s\n", s_cur.name,
			s_cur.kind == 1 ? "usf" : "wav", s_cur.playedSec, s_cur.lengthSec, s_cur.fade, s_cur.fadeTarget,
			s_cur.loop ? "looping" : "once" );
	else
		gEngfuncs.Con_Printf( "zmus: nothing playing\n" );
}

void ZMusic_Init( void )
{
	zmus_volume = CVAR_CREATE( "zmus_volume", "0.8", FCVAR_ARCHIVE );
	zmus_enable = CVAR_CREATE( "zmus_enable", "1", FCVAR_ARCHIVE );
	zmus_hle = CVAR_CREATE( "zmus_hle", "1", FCVAR_ARCHIVE );

	gEngfuncs.pfnAddCommand( "zmus_play", ZMus_Cmd_Play );
	gEngfuncs.pfnAddCommand( "zmus_stop", ZMus_Cmd_Stop );
	gEngfuncs.pfnAddCommand( "zmus_info", ZMus_Cmd_Info );

	memset( &s_cur, 0, sizeof( s_cur ) );
	memset( &s_next, 0, sizeof( s_next ) );
	s_initialized = 1;
}

void ZMusic_Shutdown( void )
{
	if ( s_device )
	{
		SDL_CloseAudioDevice( s_device );
		s_device = 0;
	}
	ZMus_FreeTrack( &s_cur );
	if ( s_haveNext )
		ZMus_FreeTrack( &s_next );
	s_haveNext = 0;
}
