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
*	Tracks have names in resource/zl_music.txt (name, loop or once, file), so maps and the game
*	ask for "kokiri_forest" or "fanfare_chest" rather than a file. A file path works too.
*
*	Fanfares (Audio_PlayFanfare: opening a chest, getting an item) play over the music, which
*	carries on silently under them and comes back when they end.
*
*	Console:
*		zmus_play <name or file> [fade seconds]	file is relative to the mod folder, e.g.
*										music/oot/06 Kokiri Forest.miniusf
*		zmus_fanfare <name or file>
*		zmus_stop [fade seconds]
*		zmus_list						the names in resource/zl_music.txt
*		zmus_reload						read resource/zl_music.txt again
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
static zl_track_t			s_fan;		// a fanfare over s_cur
static float				s_duck = 1.0f;	// s_cur's gain under a fanfare
static Sint16				*s_mixBuf;
static int					s_mixFrames;
static float				s_masterVolume = 0.8f;
static int					s_initialized;

static cvar_t *zmus_volume;
static cvar_t *zmus_enable;
static cvar_t *zmus_hle;

/*
=============================================================================
 The music list (resource/zl_music.txt)

	// comment
	name	loop|once	"file relative to the mod folder"
	name	= other_name		an alias
=============================================================================
*/
struct zl_music_entry_t
{
	char	name[48];
	char	file[160];		// or the aliased name
	int		loop;			// -1 = alias
};

#define ZMUS_MAX_LIST	256
static zl_music_entry_t	s_list[ZMUS_MAX_LIST];
static int				s_listCount;
static int				s_listLoaded;

static const char *ZMus_Token( const char *p, char *out, int size )
{
	int n = 0;
	while ( *p == ' ' || *p == '\t' )
		p++;
	if ( *p == '"' )
	{
		p++;
		while ( *p && *p != '"' && *p != '\n' )
		{
			if ( n < size - 1 )
				out[n++] = *p;
			p++;
		}
		if ( *p == '"' )
			p++;
	}
	else
	{
		while ( *p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' )
		{
			if ( n < size - 1 )
				out[n++] = *p;
			p++;
		}
	}
	out[n] = 0;
	return p;
}

static void ZMus_LoadList( void )
{
	int length = 0;
	s_listCount = 0;
	s_listLoaded = 1;

	char *data = (char *)gEngfuncs.COM_LoadFile( (char *)"resource/zl_music.txt", 5, &length );
	if ( !data )
	{
		gEngfuncs.Con_Printf( "zmus: no resource/zl_music.txt, tracks have to be given as files\n" );
		return;
	}

	char *copy = (char *)malloc( length + 1 );
	memcpy( copy, data, length );
	copy[length] = 0;
	gEngfuncs.COM_FreeFile( data );

	for ( char *line = copy; line && *line; )
	{
		char *next = strchr( line, '\n' );
		if ( next )
			*next++ = 0;

		char name[48], how[160], file[160];
		const char *p = ZMus_Token( line, name, sizeof( name ) );
		if ( name[0] && strncmp( name, "//", 2 ) && name[0] != '#' && s_listCount < ZMUS_MAX_LIST )
		{
			p = ZMus_Token( p, how, sizeof( how ) );
			zl_music_entry_t *e = &s_list[s_listCount];
			strncpy( e->name, name, sizeof( e->name ) - 1 );
			e->name[sizeof( e->name ) - 1] = 0;
			if ( !strcmp( how, "=" ) )
			{
				ZMus_Token( p, file, sizeof( file ) );
				e->loop = -1;
			}
			else
			{
				ZMus_Token( p, file, sizeof( file ) );
				e->loop = !stricmp( how, "once" ) ? 0 : 1;
			}
			strncpy( e->file, file, sizeof( e->file ) - 1 );
			e->file[sizeof( e->file ) - 1] = 0;
			if ( e->file[0] )
				s_listCount++;
		}
		line = next;
	}
	free( copy );
}

// A name from the list (following aliases), or NULL
static const zl_music_entry_t *ZMus_Find( const char *name )
{
	if ( !s_listLoaded )
		ZMus_LoadList();

	for ( int depth = 0; depth < 8; depth++ )
	{
		const zl_music_entry_t *found = NULL;
		for ( int i = 0; i < s_listCount; i++ )
		{
			if ( !stricmp( s_list[i].name, name ) )
			{
				found = &s_list[i];
				break;
			}
		}
		if ( !found || found->loop >= 0 )
			return found;
		name = found->file;
	}
	return NULL;
}

// What to load for a name or a file; *loop is the list's setting when it's a name
static const char *ZMus_Resolve( const char *track, int *loop )
{
	const zl_music_entry_t *e = ZMus_Find( track );
	if ( e )
	{
		if ( loop )
			*loop = e->loop;
		return e->file;
	}
	return track;
}

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

// Render a track, step its fade, and scale it by its gain (times extra, ramping extra0 -> extra1)
static void ZMus_RenderGain( zl_track_t *t, Sint16 *out, int frames, float extra0, float extra1 )
{
	ZMus_Render( t, out, frames );

	float seconds = (float)frames / s_spec.freq;
	t->playedSec += seconds;

	// Non looping tracks fade out at the end of their length tag
	if ( !t->loop && t->lengthSec > 0 && t->playedSec >= t->lengthSec && t->fadeTarget > 0 )
	{
		t->fadeTarget = 0;
		t->fadeRate = 1.0f / 2.0f;
	}

	float gain0 = t->fade;
	if ( t->fade < t->fadeTarget )
		t->fade = min( t->fadeTarget, t->fade + t->fadeRate * seconds );
	else if ( t->fade > t->fadeTarget )
		t->fade = max( t->fadeTarget, t->fade - t->fadeRate * seconds );
	float gain1 = t->fade;

	float vol = s_masterVolume * t->volume;
	gain0 *= extra0;
	gain1 *= extra1;
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

static void SDLCALL ZMus_Callback( void *userdata, Uint8 *stream, int len )
{
	Sint16 *out = (Sint16 *)stream;
	int frames = len / ( sizeof( Sint16 ) * ZMUS_CHANNELS );
	float seconds = (float)frames / s_spec.freq;

	memset( stream, 0, len );

	// The music ducks out under a fanfare (it keeps playing) and comes back after
	float duck0 = s_duck;
	if ( s_fan.kind )
		s_duck = max( 0.0f, s_duck - seconds / 0.1f );
	else
		s_duck = min( 1.0f, s_duck + seconds / 0.5f );

	if ( s_cur.kind )
		ZMus_RenderGain( &s_cur, out, frames, duck0, s_duck );

	if ( s_fan.kind && s_mixBuf && frames <= s_mixFrames )
	{
		ZMus_RenderGain( &s_fan, s_mixBuf, frames, 1.0f, 1.0f );
		for ( int i = 0; i < frames * ZMUS_CHANNELS; i++ )
		{
			int v = out[i] + s_mixBuf[i];
			out[i] = (Sint16)( v > 32767 ? 32767 : v < -32768 ? -32768 : v );
		}
		// A fanfare ends at its length tag (ZMusic_Frame frees it)
		if ( s_fan.lengthSec > 0 && s_fan.playedSec >= s_fan.lengthSec )
			s_fan.kind = 0;
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

	// For mixing a fanfare in; SDL asks for s_spec.samples frames at a time
	s_mixFrames = max( (int)s_spec.samples, 4096 );
	s_mixBuf = (Sint16 *)malloc( s_mixFrames * ZMUS_CHANNELS * sizeof( Sint16 ) );

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

	// A name from resource/zl_music.txt: its file, and whether it loops unless told not to
	int listLoop = loop;
	track = ZMus_Resolve( track, &listLoop );
	if ( loop )
		loop = listLoop;

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

// Audio_PlayFanfare: once, over the music, which comes back after. Replaces a fanfare already
// playing. Returns 0 if it couldn't be played.
int ZMusic_Fanfare( const char *track )
{
	if ( !s_initialized || ( zmus_enable && !zmus_enable->value ) )
		return 0;
	if ( !ZMus_OpenDevice() )
		return 0;

	track = ZMus_Resolve( track, NULL );

	zl_track_t t, old;
	if ( !ZMus_Load( &t, track ) )
		return 0;
	t.loop = 0;
	t.volume = 1.0f;
	t.fade = t.fadeTarget = 1.0f;
	t.fadeRate = 1.0f;

	SDL_LockAudioDevice( s_device );
	old = s_fan;
	s_fan = t;
	SDL_UnlockAudioDevice( s_device );

	ZMus_FreeTrack( &old );
	return 1;
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

	// A fanfare that ended: free it
	if ( !s_fan.kind && ( s_fan.usf || s_fan.wav ) )
	{
		zl_track_t old;
		SDL_LockAudioDevice( s_device );
		old = s_fan;
		memset( &s_fan, 0, sizeof( s_fan ) );
		SDL_UnlockAudioDevice( s_device );
		ZMus_FreeTrack( &old );
	}

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

static void ZMus_Cmd_Fanfare( void )
{
	if ( gEngfuncs.Cmd_Argc() < 2 )
	{
		gEngfuncs.Con_Printf( "zmus_fanfare <name or file>\n" );
		return;
	}
	ZMusic_Fanfare( gEngfuncs.Cmd_Argv( 1 ) );
}

static void ZMus_Cmd_List( void )
{
	if ( !s_listLoaded )
		ZMus_LoadList();
	for ( int i = 0; i < s_listCount; i++ )
	{
		if ( s_list[i].loop < 0 )
			gEngfuncs.Con_Printf( "%-28s = %s\n", s_list[i].name, s_list[i].file );
		else
			gEngfuncs.Con_Printf( "%-28s %s  %s\n", s_list[i].name, s_list[i].loop ? "loop" : "once", s_list[i].file );
	}
	gEngfuncs.Con_Printf( "%d tracks in resource/zl_music.txt\n", s_listCount );
}

static void ZMus_Cmd_Reload( void )
{
	ZMus_LoadList();
	gEngfuncs.Con_Printf( "zmus: %d tracks\n", s_listCount );
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
	if ( s_fan.kind )
		gEngfuncs.Con_Printf( "zmus: fanfare %s %.1fs / %.1fs\n", s_fan.name, s_fan.playedSec, s_fan.lengthSec );
}

void ZMusic_Init( void )
{
	zmus_volume = CVAR_CREATE( "zmus_volume", "0.8", FCVAR_ARCHIVE );
	zmus_enable = CVAR_CREATE( "zmus_enable", "1", FCVAR_ARCHIVE );
	zmus_hle = CVAR_CREATE( "zmus_hle", "1", FCVAR_ARCHIVE );

	gEngfuncs.pfnAddCommand( "zmus_play", ZMus_Cmd_Play );
	gEngfuncs.pfnAddCommand( "zmus_stop", ZMus_Cmd_Stop );
	gEngfuncs.pfnAddCommand( "zmus_info", ZMus_Cmd_Info );
	gEngfuncs.pfnAddCommand( "zmus_fanfare", ZMus_Cmd_Fanfare );
	gEngfuncs.pfnAddCommand( "zmus_list", ZMus_Cmd_List );
	gEngfuncs.pfnAddCommand( "zmus_reload", ZMus_Cmd_Reload );

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
	ZMus_FreeTrack( &s_fan );
	if ( s_mixBuf )
	{
		free( s_mixBuf );
		s_mixBuf = NULL;
	}
	if ( s_haveNext )
		ZMus_FreeTrack( &s_next );
	s_haveNext = 0;
}
