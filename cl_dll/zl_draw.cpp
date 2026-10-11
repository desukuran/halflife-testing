/***
*
*	Zelda 64 port: 2D drawing.
*
*	OoT lays its HUD out on a 320x240 screen. Everything here takes those coordinates and
*	scales them to the window height, anchoring to the left / right edge so widescreen
*	keeps the same margins. Drawing goes through TriAPI so sprites can be scaled, tinted
*	and alpha blended (the stock SPR_Draw* calls can do none of those together).
*
****/

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "triangleapi.h"
#include "zl_client.h"
#include "zl_fontwidths.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

struct model_s *g_zlSprWhite;
struct model_s *g_zlSprFont;
struct model_s *g_zlSprHeart;
struct model_s *g_zlSprButton;
struct model_s *g_zlSprReticle;
struct model_s *g_zlSprTextbox;
struct model_s *g_zlSprHudIcons;
struct model_s *g_zlSprIcons;

static int s_drawing;
static void ZDraw_LoadFontWidths( void );
static cvar_t *zl_hud_draw;		// 0 = TriAPI (scaled, tinted, alpha, rotated), 1 = SPR_DrawGeneric fallback

// Which HSPRITE each loaded model came from, for the SPR_DrawGeneric path
#define ZL_MAX_SPRITES 32
static struct { struct model_s *model; HSPRITE handle; } s_sprites[ZL_MAX_SPRITES];
static int s_numSprites;

static int ZDraw_FileExists( const char *name )
{
	int length = 0;
	byte *file = gEngfuncs.COM_LoadFile( (char *)name, 5, &length );
	if ( !file )
		return 0;
	gEngfuncs.COM_FreeFile( file );
	return 1;
}

static int s_usingOotFont;

struct model_s *ZDraw_LoadSprite( const char *name )
{
	// Ripped OoT textures (tools/zelda/rip_from_decomp.py) live in sprites/oot/ and win
	char oot[128];
	const char *slash = strrchr( name, '/' );
	snprintf( oot, sizeof( oot ), "sprites/oot/%s", slash ? slash + 1 : name );
	if ( ZDraw_FileExists( oot ) )
	{
		if ( strstr( name, "zl_font" ) )
			s_usingOotFont = 1;
		name = oot;
	}
	else if ( strstr( name, "zl_font" ) )
		s_usingOotFont = 0;

	HSPRITE h = SPR_Load( name );
	if ( !h )
		return NULL;

	struct model_s *m = (struct model_s *)gEngfuncs.GetSpritePointer( h );

	for ( int i = 0; i < s_numSprites; i++ )
	{
		if ( s_sprites[i].model == m )
		{
			s_sprites[i].handle = h;
			return m;
		}
	}
	if ( s_numSprites < ZL_MAX_SPRITES )
	{
		s_sprites[s_numSprites].model = m;
		s_sprites[s_numSprites].handle = h;
		s_numSprites++;
	}
	return m;
}

static HSPRITE ZDraw_Handle( struct model_s *m )
{
	for ( int i = 0; i < s_numSprites; i++ )
	{
		if ( s_sprites[i].model == m )
			return s_sprites[i].handle;
	}
	return 0;
}

void ZDraw_Init( void )
{
	zl_hud_draw = CVAR_CREATE( "zl_hud_draw", "0", FCVAR_ARCHIVE );
}

void ZDraw_VidInit( void )
{
	g_zlSprWhite	= ZDraw_LoadSprite( "sprites/zl_white.spr" );
	g_zlSprFont		= ZDraw_LoadSprite( "sprites/zl_font.spr" );
	g_zlSprHeart	= ZDraw_LoadSprite( "sprites/zl_heart.spr" );
	g_zlSprButton	= ZDraw_LoadSprite( "sprites/zl_button.spr" );
	g_zlSprReticle	= ZDraw_LoadSprite( "sprites/zl_reticle.spr" );
	g_zlSprTextbox	= ZDraw_LoadSprite( "sprites/zl_textbox.spr" );
	g_zlSprHudIcons	= ZDraw_LoadSprite( "sprites/zl_hudicons.spr" );
	g_zlSprIcons	= ZDraw_LoadSprite( "sprites/zl_icons.spr" );

	ZDraw_LoadFontWidths();
}

float ZDraw_Scale( void )
{
	// zl_hud_scale multiplies the N64 -> window scale (1 = OoT's size relative to the screen)
	static cvar_t *zl_hud_scale;
	if ( !zl_hud_scale )
		zl_hud_scale = gEngfuncs.pfnGetCvarPointer( "zl_hud_scale" );
	float mul = ( zl_hud_scale && zl_hud_scale->value > 0 ) ? zl_hud_scale->value : 1.0f;
	return ScreenHeight / 240.0f * mul;
}

float ZDraw_X( float x, int anchor )
{
	float s = ZDraw_Scale();

	// zl_hud_wide 0 (default): the N64's 4:3 picture, centred, so the buttons sit as close to
	// the hearts as they do on a TV. 1: hearts on the left edge, buttons on the right edge.
	static cvar_t *zl_hud_wide;
	if ( !zl_hud_wide )
		zl_hud_wide = gEngfuncs.pfnGetCvarPointer( "zl_hud_wide" );
	if ( ( !zl_hud_wide || !zl_hud_wide->value ) && ScreenWidth > 320.0f * s )
		return ( ScreenWidth - 320.0f * s ) * 0.5f + x * s;

	switch ( anchor )
	{
	case ZA_RIGHT:	return ScreenWidth - ( 320.0f - x ) * s;
	case ZA_CENTER:	return ScreenWidth * 0.5f + ( x - 160.0f ) * s;
	default:		return x * s;
	}
}

float ZDraw_Y( float y )
{
	// Top half hangs from the top edge, bottom half from the bottom edge, so a zl_hud_scale
	// above 1 keeps the rupee counter on screen. Identical to y * scale when zl_hud_scale is 1.
	float s = ZDraw_Scale();
	if ( y >= 160.0f )
		return ScreenHeight - ( 240.0f - y ) * s;
	return y * s;
}

void ZDraw_Begin( void )
{
	s_drawing = 1;
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
}

void ZDraw_End( void )
{
	s_drawing = 0;
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	gEngfuncs.pTriAPI->Color4f( 1, 1, 1, 1 );
}

#define ZL_GL_SRC_ALPHA				0x0302
#define ZL_GL_ONE_MINUS_SRC_ALPHA	0x0303

static void ZDraw_Quad( struct model_s *spr, int frame, const float *xs, const float *ys, int r, int g, int b, int a )
{
	triangleapi_t *tri = gEngfuncs.pTriAPI;

	if ( !spr )
		return;

	if ( zl_hud_draw && zl_hud_draw->value )
	{
		// Fallback: axis aligned bounding box of the quad, no per-draw alpha
		HSPRITE h = ZDraw_Handle( spr );
		if ( !h || a <= 0 )
			return;
		float minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0];
		for ( int i = 1; i < 4; i++ )
		{
			minx = min( minx, xs[i] ); maxx = max( maxx, xs[i] );
			miny = min( miny, ys[i] ); maxy = max( maxy, ys[i] );
		}
		SPR_Set( h, r * a / 255, g * a / 255, b * a / 255 );
		gEngfuncs.pfnSPR_DrawGeneric( frame, (int)minx, (int)miny, NULL, ZL_GL_SRC_ALPHA, ZL_GL_ONE_MINUS_SRC_ALPHA,
			(int)( maxx - minx + 0.5f ), (int)( maxy - miny + 0.5f ) );
		return;
	}

	if ( !tri->SpriteTexture( spr, frame ) )
		return;

	tri->RenderMode( kRenderTransAlpha );
	tri->Color4ub( r, g, b, a );
	tri->Begin( TRI_QUADS );
		tri->TexCoord2f( 0, 0 );	tri->Vertex3f( xs[0], ys[0], 0 );
		tri->TexCoord2f( 1, 0 );	tri->Vertex3f( xs[1], ys[1], 0 );
		tri->TexCoord2f( 1, 1 );	tri->Vertex3f( xs[2], ys[2], 0 );
		tri->TexCoord2f( 0, 1 );	tri->Vertex3f( xs[3], ys[3], 0 );
	tri->End();
}

void ZDraw_Rect( float x, float y, float w, float h, int r, int g, int b, int a )
{
	if ( !g_zlSprWhite )
	{
		gEngfuncs.pfnFillRGBABlend( (int)x, (int)y, (int)w, (int)h, r, g, b, a );
		return;
	}
	ZDraw_Sprite( g_zlSprWhite, 0, x, y, w, h, r, g, b, a );
}

void ZDraw_Sprite( struct model_s *spr, int frame, float x, float y, float w, float h, int r, int g, int b, int a )
{
	float xs[4] = { x, x + w, x + w, x };
	float ys[4] = { y, y, y + h, y + h };
	ZDraw_Quad( spr, frame, xs, ys, r, g, b, a );
}

void ZDraw_SpriteRotated( struct model_s *spr, int frame, float cx, float cy, float w, float h, float deg, int r, int g, int b, int a )
{
	float rad = deg * ( M_PI / 180.0f );
	float c = cosf( rad ), s = sinf( rad );
	float hx = w * 0.5f, hy = h * 0.5f;
	float lx[4] = { -hx, hx, hx, -hx };
	float ly[4] = { -hy, -hy, hy, hy };
	float xs[4], ys[4];

	for ( int i = 0; i < 4; i++ )
	{
		xs[i] = cx + lx[i] * c - ly[i] * s;
		ys[i] = cy + lx[i] * s + ly[i] * c;
	}

	ZDraw_Quad( spr, frame, xs, ys, r, g, b, a );
}

// Widths from sprites/zl_font.txt when present (a ripped OoT font), else the generated table
static unsigned char s_fontWidths[95];

static void ZDraw_LoadFontWidths( void )
{
	memcpy( s_fontWidths, g_zlFontWidths, sizeof( s_fontWidths ) );

	int length = 0;
	byte *file = gEngfuncs.COM_LoadFile( (char *)( s_usingOotFont ? "sprites/oot/zl_font.txt" : "sprites/zl_font.txt" ), 5, &length );
	if ( !file )
		return;

	char *text = (char *)malloc( length + 1 );
	memcpy( text, file, length );
	text[length] = 0;
	gEngfuncs.COM_FreeFile( file );

	int n = 0;
	char *p = text;
	while ( *p && n < 95 )
	{
		if ( p[0] == '/' && p[1] == '/' )
		{
			while ( *p && *p != '\n' )
				p++;
			continue;
		}
		if ( *p >= '0' && *p <= '9' )
		{
			s_fontWidths[n++] = (unsigned char)atoi( p );
			while ( *p >= '0' && *p <= '9' )
				p++;
			continue;
		}
		p++;
	}
	free( text );
}

static int ZDraw_GlyphWidth( int c )
{
	if ( c < 32 || c > 126 )
		c = '?';
	return s_fontWidths[c - 32] ? s_fontWidths[c - 32] : g_zlFontWidths[c - 32];
}

float ZDraw_TextWidth( const char *text, float height )
{
	float scale = height / ZL_FONT_CELL;
	float w = 0;

	for ( ; *text; text++ )
		w += ZDraw_GlyphWidth( (unsigned char)*text ) * scale;

	return w;
}

// Vertical squash for text (the A button label flipping over); 1 = normal
static float s_textYScale = 1.0f;
void ZDraw_SetTextYScale( float scale )
{
	s_textYScale = scale;
}

float ZDraw_Char( int c, float x, float y, float height, int r, int g, int b, int a )
{
	float scale = height / ZL_FONT_CELL;
	int adv = ZDraw_GlyphWidth( c );

	if ( c < 32 || c > 126 )
		c = '?';

	if ( c != ' ' )
	{
		// Glyphs are centred in their 32 px cell (make_hud_sprites.py)
		float offset = (float)(int)( ( ZL_FONT_CELL - adv ) / 2 );
		if ( offset < 0 )
			offset = 0;
		float hy = ZL_FONT_CELL * scale;
		ZDraw_Sprite( g_zlSprFont, c - 32, x - offset * scale, y + hy * ( 1.0f - s_textYScale ) * 0.5f, ZL_FONT_CELL * scale,
					  hy * s_textYScale, r, g, b, a );
	}

	return adv * scale;
}

float ZDraw_Text( const char *text, float x, float y, float height, int r, int g, int b, int a )
{
	float start = x;
	for ( ; *text; text++ )
		x += ZDraw_Char( (unsigned char)*text, x, y, height, r, g, b, a );
	return x - start;
}

void ZDraw_TextShadow( const char *text, float x, float y, float height, int r, int g, int b, int a )
{
	float off = height / 16.0f;
	ZDraw_Text( text, x + off, y + off, height, 0, 0, 0, a );
	ZDraw_Text( text, x, y, height, r, g, b, a );
}
