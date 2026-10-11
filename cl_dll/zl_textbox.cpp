/***
*
*	Zelda 64 port: the message box (z_message.c Message_Decode / Message_Update / Message_Draw).
*
*	Messages live in zelda/resource/zl_messages.txt:
*
*		@NAME [BLACK|WOODEN|BLUE|NONE] [VARIABLE|TOP|MIDDLE|BOTTOM]
*		Text. Line breaks are kept like OoT's NEWLINE.
*		{BOX} starts the next box.
*
*	Several entries named NAME#1, NAME#2 ... make a pool; one is picked at random.
*
*	Control codes, written the way the decomp's message_data_fmt.h macros read:
*
*		{BOX}				BOX_BREAK
*		{BOX:n}				BOX_BREAK_DELAYED: next box by itself after n frames
*		{RED} {GREEN} {BLUE} {LIGHTBLUE} {PURPLE} {YELLOW} {BLACK} {WHITE}
*							COLOR (GREEN is TEXT_COLOR_ADJUSTABLE, WHITE is DEFAULT)
*		{NAME}				the player's name (zl_name, "Link")
*		{SHIFT:n}			SHIFT n pixels
*		{INSTANT} {/INSTANT} QUICKTEXT_ENABLE / _DISABLE
*		{SPEED:n}			TEXT_SPEED: n frames per character
*		{WAIT}				AWAIT_BUTTON_PRESS
*		{FADE:n}			FADE: close by itself after n frames
*		{CHOICE2} {CHOICE3}	TWO_CHOICE / THREE_CHOICE: the last 2 / 3 lines are the options
*		{SFX:NA_SE_...}		SFX: play a sound (names from pm_shared/zl_sounds.h)
*		{UNSKIPPABLE}		UNSKIPPABLE
*
*	The text is encoded into OoT's own byte codes (MESSAGE_* in message_data_fmt.h) and
*	then played back the same way the game does.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "zl_client.h"
#include <time.h>
#include "zl_sounds.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

// MESSAGE_* (non-wide)
#define MSG_NEWLINE				0x01
#define MSG_END					0x02
#define MSG_BOX_BREAK			0x04
#define MSG_COLOR				0x05
#define MSG_SHIFT				0x06
#define MSG_QUICKTEXT_ENABLE	0x08
#define MSG_QUICKTEXT_DISABLE	0x09
#define MSG_BOX_BREAK_DELAYED	0x0C
#define MSG_AWAIT_BUTTON_PRESS	0x0D
#define MSG_FADE				0x0E
#define MSG_SFX					0x12
#define MSG_TEXT_SPEED			0x14
#define MSG_UNSKIPPABLE			0x1A
#define MSG_TWO_CHOICE			0x1B
#define MSG_THREE_CHOICE		0x1C

enum
{
	TEXT_COLOR_DEFAULT,
	TEXT_COLOR_RED,
	TEXT_COLOR_ADJUSTABLE,
	TEXT_COLOR_BLUE,
	TEXT_COLOR_LIGHTBLUE,
	TEXT_COLOR_PURPLE,
	TEXT_COLOR_YELLOW,
	TEXT_COLOR_BLACK,
};

enum
{
	TEXTBOX_TYPE_BLACK,
	TEXTBOX_TYPE_WOODEN,
	TEXTBOX_TYPE_BLUE,
	TEXTBOX_TYPE_NONE,
};

enum
{
	TEXTBOX_POS_VARIABLE,
	TEXTBOX_POS_TOP,
	TEXTBOX_POS_MIDDLE,
	TEXTBOX_POS_BOTTOM,
};

/*
=============================================================================
 Message database
=============================================================================
*/
struct zl_message_t
{
	char	name[64];
	int		type;
	int		pos;
	char	*text;		// raw, codes still in {braces} (malloc'd)
};

#define MAX_ZL_MESSAGES 4096

static zl_message_t s_messages[MAX_ZL_MESSAGES];
static int s_numMessages;

static int ZText_ParseType( const char *s )
{
	if ( !stricmp( s, "WOODEN" ) )	return TEXTBOX_TYPE_WOODEN;
	if ( !stricmp( s, "BLUE" ) )	return TEXTBOX_TYPE_BLUE;
	if ( !stricmp( s, "NONE" ) )	return TEXTBOX_TYPE_NONE;
	return TEXTBOX_TYPE_BLACK;
}

static int ZText_ParsePos( const char *s )
{
	if ( !stricmp( s, "TOP" ) )		return TEXTBOX_POS_TOP;
	if ( !stricmp( s, "MIDDLE" ) )	return TEXTBOX_POS_MIDDLE;
	if ( !stricmp( s, "BOTTOM" ) )	return TEXTBOX_POS_BOTTOM;
	return TEXTBOX_POS_VARIABLE;
}

static void ZText_FreeMessages( void )
{
	for ( int i = 0; i < s_numMessages; i++ )
	{
		if ( s_messages[i].text )
			free( s_messages[i].text );
	}
	s_numMessages = 0;
}

// Parse one message file and append its entries. Lines starting with // are comments.
static void ZText_ParseFile( const char *filename, int required )
{
	int length = 0;
	byte *file = gEngfuncs.COM_LoadFile( (char *)filename, 5, &length );
	if ( !file )
	{
		if ( required )
			gEngfuncs.Con_Printf( "Zelda: %s not found\n", filename );
		return;
	}

	char *data = (char *)malloc( length + 1 );
	memcpy( data, file, length );
	data[length] = 0;
	gEngfuncs.COM_FreeFile( file );

	zl_message_t *cur = NULL;
	char *text = NULL;
	int textLen = 0, textCap = 0;
	char *p = data;

	while ( 1 )
	{
		char *eol = strchr( p, '\n' );
		int lineLen = eol ? (int)( eol - p ) : (int)strlen( p );
		if ( lineLen > 0 && p[lineLen - 1] == '\r' )
			lineLen--;

		bool isHeader = lineLen > 0 && p[0] == '@';
		bool isComment = lineLen > 1 && p[0] == '/' && p[1] == '/';
		bool atEnd = ( *p == 0 );

		if ( ( isHeader || atEnd ) && cur )
		{
			// Finish the previous entry: trim trailing blank lines
			while ( textLen > 0 && ( text[textLen - 1] == '\n' || text[textLen - 1] == ' ' || text[textLen - 1] == '\t' ) )
				textLen--;
			text[textLen] = 0;
			cur->text = text;
			cur = NULL;
			text = NULL;
		}

		if ( atEnd )
			break;

		if ( isHeader )
		{
			if ( s_numMessages >= MAX_ZL_MESSAGES )
				break;

			cur = &s_messages[s_numMessages++];
			memset( cur, 0, sizeof( *cur ) );

			char header[256];
			int len = min( lineLen - 1, (int)sizeof( header ) - 1 );
			memcpy( header, p + 1, len );
			header[len] = 0;

			char name[64] = "", type[32] = "", pos[32] = "";
			sscanf( header, "%63s %31s %31s", name, type, pos );
			strncpy( cur->name, name, sizeof( cur->name ) - 1 );
			cur->type = ZText_ParseType( type );
			cur->pos = ZText_ParsePos( pos );

			textCap = 256;
			textLen = 0;
			text = (char *)malloc( textCap );
			text[0] = 0;
		}
		else if ( cur && !isComment )
		{
			// Skip leading blank lines
			if ( textLen > 0 || lineLen > 0 )
			{
				if ( textLen + lineLen + 2 >= textCap )
				{
					textCap = ( textLen + lineLen + 2 ) * 2;
					text = (char *)realloc( text, textCap );
				}
				memcpy( text + textLen, p, lineLen );
				textLen += lineLen;
				text[textLen++] = '\n';
				text[textLen] = 0;
			}
		}

		if ( !eol )
		{
			p += strlen( p );
			continue;
		}
		p = eol + 1;
	}

	free( data );
}

static void ZText_LoadMessages( void )
{
	ZText_FreeMessages();
	// Your own OoT text, converted by tools/zelda/rip_from_decomp.py (not distributed).
	// Loaded first so its ITEM_* entries win over the stand-ins.
	ZText_ParseFile( "resource/zl_oot_messages.txt", 0 );
	ZText_ParseFile( "resource/zl_messages.txt", 1 );
	gEngfuncs.Con_DPrintf( "Zelda: loaded %d messages\n", s_numMessages );
}

static zl_message_t *ZText_Find( const char *name )
{
	for ( int i = 0; i < s_numMessages; i++ )
	{
		if ( !stricmp( s_messages[i].name, name ) )
			return &s_messages[i];
	}

	// Pool: NAME#1, NAME#2 ...
	zl_message_t *pool[64];
	int count = 0;
	int len = (int)strlen( name );

	for ( int i = 0; i < s_numMessages && count < 64; i++ )
	{
		if ( !strnicmp( s_messages[i].name, name, len ) && s_messages[i].name[len] == '#' )
			pool[count++] = &s_messages[i];
	}

	if ( count )
		return pool[gEngfuncs.pfnRandomLong( 0, count - 1 )];

	return NULL;
}

/*
=============================================================================
 Encoding {CODES} to OoT bytes
=============================================================================
*/
static int ZText_SoundByName( const char *name )
{
	for ( int i = 0; i < ZS_COUNT; i++ )
	{
		if ( !stricmp( g_zlSounds[i].oot, name ) )
			return i;
	}
	return -1;
}

static int ZText_Encode( const char *src, unsigned char *out, int max )
{
	int n = 0;
	cvar_t *nameVar = gEngfuncs.pfnGetCvarPointer( "zl_name" );
	const char *playerName = ( nameVar && nameVar->string && nameVar->string[0] ) ? nameVar->string : "Link";

#define PUT( b ) do { if ( n < max - 1 ) out[n++] = (unsigned char)( b ); } while ( 0 )

	while ( *src )
	{
		if ( *src == '\n' )
		{
			PUT( MSG_NEWLINE );
			src++;
			continue;
		}

		if ( *src == '{' )
		{
			const char *close = strchr( src, '}' );
			if ( !close )
				break;

			char code[64];
			int len = min( (int)( close - src - 1 ), (int)sizeof( code ) - 1 );
			memcpy( code, src + 1, len );
			code[len] = 0;
			src = close + 1;

			char *arg = strchr( code, ':' );
			if ( arg )
				*arg++ = 0;

			// A box break swallows the newline that follows it in the file
			bool eatNewline = false;

			if ( !stricmp( code, "BOX" ) )
			{
				if ( arg )
				{
					PUT( MSG_BOX_BREAK_DELAYED );
					PUT( atoi( arg ) );
				}
				else
					PUT( MSG_BOX_BREAK );
				eatNewline = true;
			}
			else if ( !stricmp( code, "RED" ) )			{ PUT( MSG_COLOR ); PUT( TEXT_COLOR_RED ); }
			else if ( !stricmp( code, "GREEN" ) )		{ PUT( MSG_COLOR ); PUT( TEXT_COLOR_ADJUSTABLE ); }
			else if ( !stricmp( code, "BLUE" ) )		{ PUT( MSG_COLOR ); PUT( TEXT_COLOR_BLUE ); }
			else if ( !stricmp( code, "LIGHTBLUE" ) )	{ PUT( MSG_COLOR ); PUT( TEXT_COLOR_LIGHTBLUE ); }
			else if ( !stricmp( code, "PURPLE" ) )		{ PUT( MSG_COLOR ); PUT( TEXT_COLOR_PURPLE ); }
			else if ( !stricmp( code, "YELLOW" ) )		{ PUT( MSG_COLOR ); PUT( TEXT_COLOR_YELLOW ); }
			else if ( !stricmp( code, "BLACK" ) )		{ PUT( MSG_COLOR ); PUT( TEXT_COLOR_BLACK ); }
			else if ( !stricmp( code, "WHITE" ) || !stricmp( code, "DEFAULT" ) ) { PUT( MSG_COLOR ); PUT( TEXT_COLOR_DEFAULT ); }
			else if ( !stricmp( code, "NAME" ) )
			{
				for ( const char *c = playerName; *c; c++ )
					PUT( *c );
			}
			else if ( !stricmp( code, "TIME" ) )
			{
				// MESSAGE_TIME prints the time of day; Half-Life has no clock of its own, so it's yours
				time_t now = time( NULL );
				struct tm *lt = localtime( &now );
				char clock[16];
				snprintf( clock, sizeof( clock ), "%d:%02d", lt ? lt->tm_hour : 0, lt ? lt->tm_min : 0 );
				for ( const char *c = clock; *c; c++ )
					PUT( *c );
			}
			else if ( !stricmp( code, "SHIFT" ) && arg )	{ PUT( MSG_SHIFT ); PUT( atoi( arg ) ); }
			else if ( !stricmp( code, "INSTANT" ) )		PUT( MSG_QUICKTEXT_ENABLE );
			else if ( !stricmp( code, "/INSTANT" ) )	PUT( MSG_QUICKTEXT_DISABLE );
			else if ( !stricmp( code, "SPEED" ) && arg ) { PUT( MSG_TEXT_SPEED ); PUT( atoi( arg ) ); }
			else if ( !stricmp( code, "WAIT" ) )		PUT( MSG_AWAIT_BUTTON_PRESS );
			else if ( !stricmp( code, "FADE" ) && arg )	{ PUT( MSG_FADE ); PUT( atoi( arg ) ); }
			else if ( !stricmp( code, "CHOICE2" ) )		PUT( MSG_TWO_CHOICE );
			else if ( !stricmp( code, "CHOICE3" ) )		PUT( MSG_THREE_CHOICE );
			else if ( !stricmp( code, "UNSKIPPABLE" ) )	PUT( MSG_UNSKIPPABLE );
			else if ( !stricmp( code, "SFX" ) && arg )
			{
				int snd = ZText_SoundByName( arg );
				if ( snd >= 0 )
				{
					PUT( MSG_SFX );
					PUT( snd );
				}
			}

			if ( eatNewline && *src == '\n' )
				src++;
			continue;
		}

		PUT( *src );
		src++;
	}

	PUT( MSG_END );
	out[n] = 0;
	return n;

#undef PUT
}

/*
=============================================================================
 Playback
=============================================================================
*/
enum
{
	ZT_CLOSED,
	ZT_OPENING,
	ZT_PRINTING,
	ZT_WAITING,		// box full, waiting for A / B
	ZT_CHOICE,
	ZT_CLOSING,
};

struct zl_textstate_t
{
	int				state;
	unsigned char	buf[2048];
	int				len;
	int				boxStart;		// offset of the current box
	int				boxEnd;			// offset of the code that ended it
	int				endCode;		// MSG_BOX_BREAK, MSG_END, choice, ...
	int				endArg;
	float			printed;		// characters revealed so far in this box (float for speed)
	int				totalChars;		// printable characters in this box
	int				lines;
	int				choiceCount;
	int				choice;
	int				type;
	int				pos;
	int				speaker;
	int				unskippable;
	float			stateTime;
	float			lastTime;
	float			stickPrev;
	int				sfxFired[16];
	int				numSfxFired;
};

static zl_textstate_t s_text;
static cvar_t *zl_text_speed;
static cvar_t *zl_name;
static cvar_t *zl_text_test;

/*
=============================================================================
 Testing: zl_text_test <name>, zl_text_next / zl_text_prev, zl_text_list [filter]

 The box is opened through the server (zmsg_test) so Link stops and the A/B buttons go to
 the text box exactly like a real conversation.
=============================================================================
*/
static int s_testIndex = -1;

static void ZText_TestOpen( const char *name )
{
	char cmd[96];
	snprintf( cmd, sizeof( cmd ), "zmsg_test \"%s\"\n", name );
	gEngfuncs.pfnClientCmd( cmd );
	gEngfuncs.Con_Printf( "zl_text_test: %s\n", name );
}

static void ZText_TestStep( int dir )
{
	if ( !s_numMessages )
		return;
	if ( gEngfuncs.Cmd_Argc() > 1 )
		dir *= max( 1, atoi( gEngfuncs.Cmd_Argv( 1 ) ) );
	s_testIndex = ( ( s_testIndex + dir ) % s_numMessages + s_numMessages ) % s_numMessages;
	gEngfuncs.Con_Printf( "[%d/%d] ", s_testIndex + 1, s_numMessages );
	ZText_TestOpen( s_messages[s_testIndex].name );
}

static void ZText_TestNext( void ) { ZText_TestStep( 1 ); }
static void ZText_TestPrev( void ) { ZText_TestStep( -1 ); }

static void ZText_TestList( void )
{
	const char *filter = gEngfuncs.Cmd_Argc() > 1 ? gEngfuncs.Cmd_Argv( 1 ) : "";
	int shown = 0;
	for ( int i = 0; i < s_numMessages; i++ )
	{
		if ( filter[0] && !strstr( s_messages[i].name, filter ) )
			continue;
		// First line of the text, codes and all, so you can find a message by what it says
		char preview[48];
		strncpy( preview, s_messages[i].text, sizeof( preview ) - 1 );
		preview[sizeof( preview ) - 1] = 0;
		for ( char *c = preview; *c; c++ )
			if ( *c == '\n' ) { *c = 0; break; }
		gEngfuncs.Con_Printf( "%4d %-24s %s\n", i + 1, s_messages[i].name, preview );
		shown++;
	}
	gEngfuncs.Con_Printf( "%d of %d messages\n", shown, s_numMessages );
}

// Called every HUD frame: setting zl_text_test opens that message once
static void ZText_PollTestCvar( void )
{
	if ( !zl_text_test || !zl_text_test->string || !zl_text_test->string[0] )
		return;
	char name[64];
	strncpy( name, zl_text_test->string, sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = 0;
	gEngfuncs.Cvar_Set( (char *)"zl_text_test", (char *)"" );

	for ( int i = 0; i < s_numMessages; i++ )
		if ( !stricmp( s_messages[i].name, name ) )
			s_testIndex = i;
	ZText_TestOpen( name );
}

void ZText_Init( void )
{
	zl_text_speed = CVAR_CREATE( "zl_text_speed", "45", FCVAR_ARCHIVE );	// characters per second
	zl_name = CVAR_CREATE( "zl_name", "Link", FCVAR_ARCHIVE | FCVAR_USERINFO );
	memset( &s_text, 0, sizeof( s_text ) );
	gEngfuncs.pfnAddCommand( "zl_reload_messages", ZText_LoadMessages );

	zl_text_test = CVAR_CREATE( "zl_text_test", "", 0 );	// set to a message name to show it
	gEngfuncs.pfnAddCommand( "zl_text_next", ZText_TestNext );
	gEngfuncs.pfnAddCommand( "zl_text_prev", ZText_TestPrev );
	gEngfuncs.pfnAddCommand( "zl_text_list", ZText_TestList );
}

void ZText_VidInit( void )
{
	ZText_LoadMessages();
}

int ZText_IsOpen( void )
{
	return s_text.state != ZT_CLOSED;
}

int ZText_Speaker( void )
{
	return s_text.speaker;
}

// Scan the current box: where it ends, how many lines and characters it has
static void ZText_MeasureBox( void )
{
	int i = s_text.boxStart;
	s_text.lines = 1;
	s_text.totalChars = 0;
	s_text.choiceCount = 0;
	s_text.endCode = MSG_END;
	s_text.endArg = 0;

	while ( i < s_text.len )
	{
		int c = s_text.buf[i];

		if ( c == MSG_END || c == MSG_BOX_BREAK )
		{
			s_text.endCode = c;
			break;
		}
		if ( c == MSG_BOX_BREAK_DELAYED || c == MSG_FADE )
		{
			s_text.endCode = c;
			s_text.endArg = s_text.buf[i + 1];
			break;
		}

		switch ( c )
		{
		case MSG_NEWLINE:			s_text.lines++; break;
		case MSG_TWO_CHOICE:		s_text.choiceCount = 2; break;
		case MSG_THREE_CHOICE:		s_text.choiceCount = 3; break;
		case MSG_COLOR:
		case MSG_SHIFT:
		case MSG_SFX:
		case MSG_TEXT_SPEED:		i++; break;
		case MSG_QUICKTEXT_ENABLE:
		case MSG_QUICKTEXT_DISABLE:
		case MSG_AWAIT_BUTTON_PRESS:
		case MSG_UNSKIPPABLE:		break;
		default:					s_text.totalChars++; break;
		}
		i++;
	}

	s_text.boxEnd = i;
	s_text.printed = 0;
	s_text.numSfxFired = 0;
	s_text.choice = 0;
}

static void ZText_StartBox( int offset )
{
	s_text.boxStart = offset;
	ZText_MeasureBox();
	s_text.state = ZT_PRINTING;
	s_text.stateTime = 0;
}

void ZText_Open( const char *name, int speaker )
{
	// "0x2053|GOSSIP_LOOK": the first one that exists (OoT's text when you've ripped it)
	zl_message_t *msg = NULL;
	char alts[160];
	strncpy( alts, name, sizeof( alts ) - 1 );
	alts[sizeof( alts ) - 1] = 0;
	for ( char *alt = strtok( alts, "|" ); alt && !msg; alt = strtok( NULL, "|" ) )
	{
		msg = ZText_Find( alt );
		name = alt;
	}
	char missing[160];
	const char *src;

	memset( &s_text, 0, sizeof( s_text ) );

	if ( msg )
	{
		src = msg->text;
		s_text.type = msg->type;
		s_text.pos = msg->pos;
	}
	else
	{
		snprintf( missing, sizeof( missing ), "{RED}%s{WHITE}\nisn't in zl_messages.txt.", name );
		src = missing;
	}

	s_text.len = ZText_Encode( src, s_text.buf, sizeof( s_text.buf ) );
	s_text.speaker = speaker;
	s_text.state = ZT_OPENING;
	s_text.lastTime = gHUD.m_flTime;
	s_text.boxStart = 0;
	ZText_MeasureBox();
}

static void ZText_Finish( int choice )
{
	char cmd[32];
	s_text.state = ZT_CLOSED;
	snprintf( cmd, sizeof( cmd ), "zmsg_done %d\n", choice );
	gEngfuncs.pfnClientCmd( cmd );
}

void ZText_Close( void )
{
	s_text.state = ZT_CLOSED;
}

static void ZText_Advance( void )
{
	switch ( s_text.endCode )
	{
	case MSG_BOX_BREAK:
	case MSG_BOX_BREAK_DELAYED:
	{
		int next = s_text.boxEnd + ( s_text.endCode == MSG_BOX_BREAK_DELAYED ? 2 : 1 );
		ZL_PlaySound( ZS_SY_MESSAGE_PASS, 0.7f );
		ZText_StartBox( next );
		break;
	}
	default:
		ZL_PlaySound( ZS_SY_MESSAGE_END, 0.6f );
		ZText_Finish( s_text.choiceCount ? s_text.choice : 0 );
		break;
	}
}

void ZText_Input( int pressed, float stickY )
{
	if ( s_text.state == ZT_CLOSED || s_text.state == ZT_OPENING )
		return;

	bool a = ( pressed & ZPAD_A ) != 0;
	bool b = ( pressed & ZPAD_B ) != 0;

	switch ( s_text.state )
	{
	case ZT_PRINTING:
		// A / B fill the box at once (unless UNSKIPPABLE)
		if ( ( a || b ) && !s_text.unskippable && s_text.stateTime > 0.1f )
			s_text.printed = (float)s_text.totalChars;
		break;

	case ZT_WAITING:
		if ( ( a || b ) && s_text.endCode != MSG_FADE && s_text.endCode != MSG_BOX_BREAK_DELAYED )
			ZText_Advance();
		break;

	case ZT_CHOICE:
	{
		// Control stick up / down moves the cursor (NA_SE_SY_CURSOR)
		int dir = 0;
		if ( stickY > 0.5f && s_text.stickPrev <= 0.5f )
			dir = -1;
		else if ( stickY < -0.5f && s_text.stickPrev >= -0.5f )
			dir = 1;
		s_text.stickPrev = stickY;

		if ( dir )
		{
			s_text.choice = ( s_text.choice + dir + s_text.choiceCount ) % s_text.choiceCount;
			ZL_PlaySound( ZS_SY_CURSOR, 0.6f );
		}

		if ( a )
		{
			ZL_PlaySound( ZS_SY_DECIDE, 0.7f );
			ZText_Finish( s_text.choice );
		}
		else if ( b )
		{
			// B picks the last option (usually "No")
			ZL_PlaySound( ZS_SY_DECIDE, 0.7f );
			ZText_Finish( s_text.choiceCount - 1 );
		}
		break;
	}
	}
}

static void ZText_Update( float time )
{
	float dt = time - s_text.lastTime;
	s_text.lastTime = time;
	if ( dt < 0 || dt > 0.25f )
		dt = 0.016f;

	s_text.stateTime += dt;

	switch ( s_text.state )
	{
	case ZT_OPENING:
		// The box grows open over 8 frames (sWidthCoefficients / sHeightCoefficients)
		if ( s_text.stateTime >= ZL_FRAMES( 4 ) )
			ZText_StartBox( 0 );
		break;

	case ZT_PRINTING:
	{
		float cps = zl_text_speed ? zl_text_speed->value : 45.0f;
		if ( cps <= 0 )
			cps = 1000;
		s_text.printed += cps * dt;

		if ( s_text.printed >= s_text.totalChars )
		{
			s_text.printed = (float)s_text.totalChars;
			s_text.state = s_text.choiceCount ? ZT_CHOICE : ZT_WAITING;
			s_text.stateTime = 0;
		}
		break;
	}

	case ZT_WAITING:
		if ( s_text.endCode == MSG_BOX_BREAK_DELAYED && s_text.stateTime >= ZL_FRAMES( s_text.endArg ) )
			ZText_Advance();
		else if ( s_text.endCode == MSG_FADE && s_text.stateTime >= ZL_FRAMES( s_text.endArg ) )
			ZText_Finish( 0 );
		break;
	}
}

static void ZText_ColorFor( int color, int type, int *r, int *g, int *b )
{
	// Message_SetTextColor
	bool wooden = ( type == TEXTBOX_TYPE_WOODEN );
	switch ( color )
	{
	case TEXT_COLOR_RED:		if ( wooden ) { *r = 255; *g = 120; *b = 0; } else { *r = 255; *g = 60; *b = 60; } break;
	case TEXT_COLOR_ADJUSTABLE:	*r = 70; *g = 255; *b = 80; break;
	case TEXT_COLOR_BLUE:		if ( wooden ) { *r = 80; *g = 110; *b = 255; } else { *r = 80; *g = 90; *b = 255; } break;
	case TEXT_COLOR_LIGHTBLUE:	if ( wooden ) { *r = 90; *g = 180; *b = 255; } else { *r = 100; *g = 180; *b = 255; } break;
	case TEXT_COLOR_PURPLE:		if ( wooden ) { *r = 210; *g = 100; *b = 255; } else { *r = 255; *g = 150; *b = 180; } break;
	case TEXT_COLOR_YELLOW:		if ( wooden ) { *r = 255; *g = 255; *b = 30; } else { *r = 225; *g = 255; *b = 50; } break;
	case TEXT_COLOR_BLACK:		*r = *g = *b = 0; break;
	default:					*r = *g = *b = 255; break;
	}
}

static void ZText_DrawBox( float x, float y, float w, float h, int r, int g, int b, int a )
{
	float rad = min( 8.0f * ZDraw_Scale(), min( w, h ) * 0.5f );

	ZDraw_Rect( x + rad, y, w - 2 * rad, h, r, g, b, a );
	ZDraw_Rect( x, y + rad, rad, h - 2 * rad, r, g, b, a );
	ZDraw_Rect( x + w - rad, y + rad, rad, h - 2 * rad, r, g, b, a );

	ZDraw_SpriteRotated( g_zlSprTextbox, 0, x + rad * 0.5f, y + rad * 0.5f, rad, rad, 0, r, g, b, a );
	ZDraw_SpriteRotated( g_zlSprTextbox, 0, x + w - rad * 0.5f, y + rad * 0.5f, rad, rad, 90, r, g, b, a );
	ZDraw_SpriteRotated( g_zlSprTextbox, 0, x + w - rad * 0.5f, y + h - rad * 0.5f, rad, rad, 180, r, g, b, a );
	ZDraw_SpriteRotated( g_zlSprTextbox, 0, x + rad * 0.5f, y + h - rad * 0.5f, rad, rad, 270, r, g, b, a );
}

void ZText_Draw( float time )
{
	ZText_PollTestCvar();

	if ( s_text.state == ZT_CLOSED )
		return;

	ZText_Update( time );
	if ( s_text.state == ZT_CLOSED )
		return;

	float s = ZDraw_Scale();

	// Box: 256 x 64 on the 320 x 240 screen, centred
	float boxY;
	switch ( s_text.pos )
	{
	case TEXTBOX_POS_TOP:		boxY = 38; break;
	case TEXTBOX_POS_MIDDLE:	boxY = 90; break;
	default:					boxY = 156; break;
	}

	float bw = 256 * s, bh = 64 * s;
	float bx = ZDraw_X( 32, ZA_CENTER );
	float by = ZDraw_Y( boxY );

	float open = 1.0f;
	if ( s_text.state == ZT_OPENING )
		open = min( 1.0f, s_text.stateTime / ZL_FRAMES( 4 ) );

	int br = 0, bg = 0, bb = 0, ba = 170;	// TEXTBOX_TYPE_BLACK
	if ( s_text.type == TEXTBOX_TYPE_WOODEN )
		br = 70, bg = 50, bb = 30, ba = 230;
	else if ( s_text.type == TEXTBOX_TYPE_BLUE )
		br = 0, bg = 10, bb = 50, ba = 170;

	if ( s_text.type != TEXTBOX_TYPE_NONE )
	{
		float w = bw * open, h = bh * open;
		ZText_DrawBox( bx + ( bw - w ) * 0.5f, by + ( bh - h ) * 0.5f, w, h, br, bg, bb, ba );
	}

	if ( s_text.state == ZT_OPENING )
		return;

	// Text: R_TEXT_INIT_XPOS 31 px in, 16 px lines, starting lower when there are fewer lines
	float lineH = 16.0f * s;
	float charH = 14.0f * s;	// ~27 characters per line, close to OoT
	float textX = bx + 26 * s;
	float textY = by + ( s_text.lines >= 4 ? 4 : s_text.lines == 3 ? 10 : 18 ) * s - 3 * s;
	float x = textX, y = textY;

	int color = TEXT_COLOR_DEFAULT;
	int r, g, b;
	ZText_ColorFor( color, s_text.type, &r, &g, &b );

	int shown = 0;
	int quick = 0;
	int line = 0;
	int choiceFirstLine = s_text.choiceCount ? s_text.lines - s_text.choiceCount : 99;
	float choiceY[3] = { y, y, y };
	bool shadow = s_text.type != TEXTBOX_TYPE_WOODEN;

	if ( choiceFirstLine == 0 )
		x += 14 * s;

	for ( int i = s_text.boxStart; i < s_text.boxEnd; i++ )
	{
		int c = s_text.buf[i];

		switch ( c )
		{
		case MSG_NEWLINE:
			line++;
			x = textX;
			y += lineH;
			if ( line >= choiceFirstLine && line - choiceFirstLine < 3 )
			{
				choiceY[line - choiceFirstLine] = y;
				x += 14 * s;	// room for the choice cursor
			}
			continue;
		case MSG_COLOR:
			color = s_text.buf[++i];
			ZText_ColorFor( color, s_text.type, &r, &g, &b );
			continue;
		case MSG_SHIFT:
			x += s_text.buf[++i] * s;
			continue;
		case MSG_SFX:
		{
			int snd = s_text.buf[++i];
			// Play once, when the text reaches it
			if ( shown <= (int)s_text.printed )
			{
				bool fired = false;
				for ( int k = 0; k < s_text.numSfxFired; k++ )
					if ( s_text.sfxFired[k] == i )
						fired = true;
				if ( !fired && s_text.numSfxFired < 16 )
				{
					s_text.sfxFired[s_text.numSfxFired++] = i;
					ZL_PlaySound( snd, 0.8f );
				}
			}
			continue;
		}
		case MSG_TEXT_SPEED:
			i++;
			continue;
		case MSG_QUICKTEXT_ENABLE:
			quick = 1;
			continue;
		case MSG_QUICKTEXT_DISABLE:
			quick = 0;
			continue;
		case MSG_UNSKIPPABLE:
			s_text.unskippable = 1;
			continue;
		case MSG_TWO_CHOICE:
		case MSG_THREE_CHOICE:
		case MSG_AWAIT_BUTTON_PRESS:
			continue;
		}

		if ( c < 0x20 )
			continue;

		// Quick text appears all at once
		if ( quick && shown >= (int)s_text.printed )
			s_text.printed = (float)( shown + 1 );

		if ( shown >= (int)s_text.printed )
			break;

		if ( shadow )
			ZDraw_Char( c, x + s, y + s, charH, 0, 0, 0, 255 );
		x += ZDraw_Char( c, x, y, charH, r, g, b, 255 );
		shown++;
	}

	// End icons: bouncing triangle for "more", square at the end (gMessageArrowTex / gMessageEndTex)
	float bob = fabsf( sinf( time * 6.0f ) ) * 2 * s;
	float iconSize = 8 * s;
	float ix = bx + bw * 0.5f - iconSize * 0.5f;
	float iy = by + bh - 11 * s;

	if ( s_text.state == ZT_WAITING )
	{
		if ( s_text.endCode == MSG_BOX_BREAK )
			ZDraw_Sprite( g_zlSprTextbox, 1, ix, iy + bob, iconSize, iconSize, 0, 200, 80, 255 );
		else if ( s_text.endCode == MSG_END )
			ZDraw_Sprite( g_zlSprTextbox, 2, ix, iy, iconSize, iconSize, 0, 200, 80, 255 );
	}

	if ( s_text.state == ZT_CHOICE && s_text.choice < 3 )
	{
		float cy = choiceY[s_text.choice];
		float pulse = 0.75f + 0.25f * sinf( time * 8.0f );
		ZDraw_Sprite( g_zlSprTextbox, 3, textX + 1 * s, cy + charH * 0.3f, iconSize, iconSize, (int)( 70 * pulse ), (int)( 255 * pulse ), (int)( 80 * pulse ), 255 );
	}
}
