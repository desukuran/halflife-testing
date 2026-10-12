/***
*
*	Zelda 64 port: the HUD (z_parameter.c Interface_Draw, z_lifemeter.c Health_DrawMeter,
*	z_actor.c Attention_Draw for the lock-on reticle and Navi's arrow).
*
*	Positions and colours are OoT's, on its 320x240 screen. Colours default to the N64
*	(NTSC 1.0) set: blue A, green B. zl_hud_gc 1 switches to the GameCube green A / red B.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "parsemsg.h"
#include "pm_zelda.h"
#include "zl_client.h"
#include "zl_sounds.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

zl_client_t g_zl;

extern vec3_t v_angles, v_origin;	// view.cpp: the final render angles and eye

static cvar_t *zl_hud_gc;
static cvar_t *zl_hud;
static cvar_t *zl_hud_scale;

/*
=============================================================================
 Predicted state helpers
=============================================================================
*/
int ZL_PredictedFlags( void )
{
	if ( g_pmzPredicted.valid )
		return g_pmzPredicted.sflags;
	return 0;
}

int ZL_IsLink( void )
{
	return ( ZL_PredictedFlags() & ZSF_ACTIVE ) != 0 && g_zl.active;
}

// In single player the engine draws the player with models/player.mdl whatever the server
// set. When the server gave Link his own model (zl_model), draw that one instead.
struct model_s *ZL_PlayerModelOverride( cl_entity_t *ent )
{
	if ( !ent || !ent->model || !ZL_IsLink() )
		return NULL;
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	if ( !local || ent->index != local->index )
		return NULL;
	if ( ent->model->type != mod_studio || !strcmp( ent->model->name, "models/player.mdl" ) )
		return NULL;
	return ent->model;
}

// The local player while he's Link: his predicted shape yaw, for StudioDrawPlayer
int ZL_LocalLinkFacing( cl_entity_t *ent, float *yaw )
{
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	if ( !ent || !local || ent->index != local->index || !ZL_IsLink() )
		return 0;
	*yaw = ZL_PredictedFacing();
	return 1;
}

// Climbing a ledge (pm_zelda.c PMZ_StartClimb): he's on top, and drawn this much lower, the height
// the animation doesn't rise through (kept in the MWA bits), stepping back 1.5 a frame
// (Math_StepToF( shape.yOffset, 0, 150 )) from frame 5 of 150step_up, at once for 100step_up
float ZL_LocalLinkDrawOffset( void )
{
	int state = ZL_PredictedState();
	if ( ZL_GetAction( state ) != ZACT_CLIMB )
		return 0;

	int sub = ZL_GetSub( state );
	float extra = (float)ZL_GetMWA( state );
	float start = sub ? ZL_FRAMES( 5.0f / ZL_CLIMB_RATE ) : 0.0f;
	float left = extra - max( 0.0f, g_pmzPredicted.time - start ) * ZL_VEL( 1.0f );
	return left > 0 ? left : 0;
}

int ZL_PredictedState( void )
{
	return g_pmzPredicted.valid ? g_pmzPredicted.state : 0;
}

int ZL_PredictedAction( void )
{
	return ZL_GetAction( ZL_PredictedState() );
}

float ZL_PredictedFacing( void )
{
	return g_pmzPredicted.facing;
}

void ZL_PlaySound( int sound, float volume )
{
	char path[128];

	if ( sound < 0 || sound >= ZS_COUNT )
		return;

	// The server decided at precache which sounds are OoT ones ("zv", see zl_sounds.h)
	const char *sample = ZL_SoundSample( path, sizeof( path ), gEngfuncs.PhysInfo_ValueForKey( "zv" ), sound,
										 gEngfuncs.pfnRandomLong( 0, 0x7FFF ) );
	gEngfuncs.pfnPlaySoundByName( (char *)sample, volume );
}

/*
=============================================================================
 CHudZelda
=============================================================================
*/
class CHudZelda : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );
	void InitHUDData( void );

	int MsgFunc_ZStats( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ZButtons( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ZTarget( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ZText( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ZMusic( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ZItemGet( const char *pszName, int iSize, void *pbuf );

	void DrawLetterbox( void );
	void DrawHearts( float time );
	void DrawMagic( void );
	void DrawCounters( void );
	void DrawCounterDigits( const char *text, float x, float y, int r, int g, int b );
	void DrawButtons( float time );
	void DrawItemIcon( int item, int weapon, float cx, float cy, float size, int alpha );
	void DrawCount( int value, float right, float bottom, float h, int r, int g, int b );
	void DrawReticles( float time );
	void DrawAimCrosshair( void );
	void DrawItemGet( float time );

	float m_flNextAlarm;
	float m_flHeartBeat;
	int m_iLastHealth;
};

static cvar_t *zl_bow_crosshair;
static CHudZelda g_HudZelda;

// DECLARE_MESSAGE assumes a CHud member; this element is a global
#define ZL_DECLARE_MESSAGE( x ) \
	static int __MsgFunc_##x( const char *pszName, int iSize, void *pbuf ) { return g_HudZelda.MsgFunc_##x( pszName, iSize, pbuf ); }

ZL_DECLARE_MESSAGE( ZStats )
ZL_DECLARE_MESSAGE( ZButtons )
ZL_DECLARE_MESSAGE( ZTarget )
ZL_DECLARE_MESSAGE( ZText )
ZL_DECLARE_MESSAGE( ZMusic )
ZL_DECLARE_MESSAGE( ZItemGet )

void ZHud_Init( void )
{
	g_HudZelda.Init();
	ZFx_Init();
}

void ZHud_VidInit( void )
{
	g_HudZelda.VidInit();
	ZFx_VidInit();
}

int CHudZelda::Init( void )
{
	HOOK_MESSAGE( ZStats );
	HOOK_MESSAGE( ZButtons );
	HOOK_MESSAGE( ZTarget );
	HOOK_MESSAGE( ZText );
	HOOK_MESSAGE( ZMusic );
	HOOK_MESSAGE( ZItemGet );

	zl_bow_crosshair = CVAR_CREATE( "zl_bow_crosshair", "0", FCVAR_ARCHIVE );
	zl_hud_gc = CVAR_CREATE( "zl_hud_gc", "0", FCVAR_ARCHIVE );		// GameCube button colours
	zl_hud = CVAR_CREATE( "zl_hud", "1", FCVAR_ARCHIVE );
	zl_hud_scale = CVAR_CREATE( "zl_hud_scale", "1", FCVAR_ARCHIVE );
	CVAR_CREATE( "zl_hud_wide", "0", FCVAR_ARCHIVE );	// 0: N64 4:3 layout, 1: stretch to the screen edges

	ZDraw_Init();
	ZText_Init();

	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem( this );

	Reset();
	return 1;
}

int CHudZelda::VidInit( void )
{
	ZDraw_VidInit();
	ZText_VidInit();
	return 1;
}

void CHudZelda::Reset( void )
{
	m_flNextAlarm = 0;
	m_iLastHealth = -1;
}

void CHudZelda::InitHUDData( void )
{
	memset( &g_zl, 0, sizeof( g_zl ) );
	ZText_Close();
}

int CHudZelda::MsgFunc_ZStats( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	g_zl.health = READ_SHORT();
	g_zl.healthCap = READ_SHORT();
	g_zl.magic = READ_BYTE();
	g_zl.magicCap = READ_BYTE();
	g_zl.rupees = READ_SHORT();
	g_zl.wallet = READ_SHORT();
	g_zl.keys = READ_BYTE();
	int flags = READ_SHORT();
	g_zl.doubleDefense = flags & 1;
	g_zl.equipment = flags >> 1;
	g_zl.active = 1;
	return 1;
}

int CHudZelda::MsgFunc_ZButtons( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	g_zl.swordB = READ_BYTE();
	for ( int i = 0; i < 3; i++ )
	{
		int v = READ_SHORT();
		g_zl.cItem[i] = v & 0xFF;
		g_zl.cWeapon[i] = ( v >> 8 ) & 0xFF;
		g_zl.cAmmo[i] = (short)READ_SHORT();
	}
	g_zl.doAction = READ_BYTE();
	g_zl.holding = READ_BYTE();
	return 1;
}

int CHudZelda::MsgFunc_ZTarget( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int locked = READ_SHORT();
	g_zl.hoverIdx = READ_SHORT();
	g_zl.targetFlags = READ_BYTE();
	g_zl.focusHeight = READ_COORD();

	if ( locked != g_zl.lockIdx )
		g_zl.lockTime = gHUD.m_flTime;
	g_zl.lockIdx = locked;
	return 1;
}

int CHudZelda::MsgFunc_ZText( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int cmd = READ_BYTE();
	char name[128];
	strncpy( name, READ_STRING(), sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = 0;
	int speaker = READ_SHORT();

	if ( cmd == ZTEXT_OPEN )
		ZText_Open( name, speaker );
	else
		ZText_Close();
	return 1;
}

int CHudZelda::MsgFunc_ZMusic( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int cmd = READ_BYTE();
	char track[128];
	strncpy( track, READ_STRING(), sizeof( track ) - 1 );
	track[sizeof( track ) - 1] = 0;
	float fade = READ_SHORT() / 100.0f;
	float volume = READ_BYTE() / 255.0f;
	int loop = READ_BYTE();

	switch ( cmd )
	{
	case ZMUS_PLAY:		ZMusic_Play( track, fade, volume, loop ); break;
	case ZMUS_STOP:		ZMusic_Stop( 0 ); break;
	case ZMUS_FADEOUT:	ZMusic_Stop( fade ); break;
	case ZMUS_FANFARE:
		// Without the music, the sound it stands in for (the "loop" byte)
		if ( !ZMusic_Fanfare( track ) && loop != 255 )
			ZL_PlaySound( loop, 1.0f );
		break;
	}
	return 1;
}

int CHudZelda::MsgFunc_ZItemGet( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int pickup = READ_BYTE();

	// zl_pickup_e (dlls/zl_items.cpp) -> icon
	static const int icons[] =
	{
		ZICON_RUPEE, ZICON_RUPEE, ZICON_RUPEE, ZICON_RUPEE, ZICON_HEART_CONTAINER, ZICON_ARROW, ZICON_BOMB,
		ZICON_DEKU_NUT, ZICON_BOTTLE, ZICON_BOTTLE, ZICON_HEART_CONTAINER, ZICON_KEY, ZICON_BOW, ZICON_BOMB,
		ZICON_HOOKSHOT, ZICON_LONGSHOT, ZICON_SWORD_MASTER, ZICON_SHIELD, ZICON_BOTTLE, ZICON_HEART_CONTAINER,
	};
	g_zl.itemGetIcon = ( pickup >= 0 && pickup < (int)( sizeof( icons ) / sizeof( icons[0] ) ) ) ? icons[pickup] : ZICON_RUPEE;
	g_zl.itemGetTime = gHUD.m_flTime;
	g_zl.itemGet3D = READ_BYTE();
	return 1;
}

/*
=============================================================================
 Drawing
=============================================================================
*/

// Camera_UpdateInterface letterbox: 26 / 27 / 32 lines out of 240
void CHudZelda::DrawLetterbox( void )
{
	float lb = ZCam_Letterbox();
	if ( lb <= 0.1f )
		return;

	float h = ZDraw_Y( lb );
	ZDraw_Rect( 0, 0, ScreenWidth, h, 0, 0, 0, 255 );
	ZDraw_Rect( 0, ScreenHeight - h, ScreenWidth, h, 0, 0, 0, 255 );
}

// Health_DrawMeter: centre (30 + 10i, 26 [+10 per row of 10]), 16 px texture at 0.68
void CHudZelda::DrawHearts( float time )
{
	int capacity = g_zl.healthCap / ZL_HEART;
	int full = g_zl.health / ZL_HEART;
	int frac = g_zl.health % ZL_HEART;
	float s = ZDraw_Scale();
	float size = 8.0f * 0.68f * s;	// halfHeartLength = 8 * 0.68

	// The last non empty heart beats (beatingHeartOscillator)
	int beating = ( frac != 0 ) ? full : full - 1;
	float beat = 1.0f + 0.15f * ( 0.5f + 0.5f * sinf( time * ( M_PI * 2.0f ) * 1.5f ) );

	int primR = 255, primG = 70, primB = 50;	// HEARTS_PRIM
	int envR = 50, envG = 40, envB = 60;		// HEARTS_ENV
	if ( g_zl.doubleDefense )
	{
		// Double defense: white outline, deeper red hearts (HEARTS_DD_*)
		primR = 200; primG = 0; primB = 0;
		envR = envG = envB = 255;
	}

	for ( int i = 0; i < capacity; i++ )
	{
		float cx = ZDraw_X( 30.0f + ( i % 10 ) * 10.0f, ZA_LEFT );
		float cy = ZDraw_Y( 26.0f + ( i / 10 ) * 10.0f );
		float sz = size * ( i == beating ? beat : 1.0f );

		// Container outline, always drawn
		ZDraw_Sprite( g_zlSprHeart, 4, cx - sz, cy - sz, sz * 2, sz * 2, envR, envG, envB, 255 );

		int frame = -1;
		if ( i < full )
			frame = 0;
		else if ( i == full && frac )
		{
			// gHeartQuarterTex .. gHeartThreeQuarterTex by sixteenths: >10 3/4, >5 1/2, else 1/4
			frame = frac > 10 ? 1 : frac > 5 ? 2 : 3;
		}

		if ( frame >= 0 )
			ZDraw_Sprite( g_zlSprHeart, frame, cx - sz, cy - sz, sz * 2, sz * 2, primR, primG, primB, 255 );
		else
			ZDraw_Sprite( g_zlSprHeart, 0, cx - sz, cy - sz, sz * 2, sz * 2, 0, 0, 0, 120 );
	}

	// Health_IsCritical -> NA_SE_SY_HITPOINT_ALARM
	int critical = g_zl.healthCap <= 0x50 ? 0x10 : g_zl.healthCap <= 0xA0 ? 0x18 : g_zl.healthCap <= 0xF0 ? 0x20 : 0x2C;
	if ( g_zl.health > 0 && g_zl.health <= critical )
	{
		if ( time > m_flNextAlarm )
		{
			ZL_PlaySound( ZS_SY_HITPOINT_ALARM, 0.6f );
			m_flNextAlarm = time + 1.0f;
		}
	}
}

// R_MAGIC_METER_X 18, R_MAGIC_METER_Y_HIGHER 34 / _LOWER 42 (more than 10 hearts)
/*
 Magic_DrawMeter at R_MAGIC_METER_X 18, y 34 (42 under two rows of hearts): the end cap, the
 middle as wide as the capacity, the end again mirrored, all lerp((100, 50, 50), border colour);
 then the fill from x 26, 7 high, R_MAGIC_FILL_COLOR (0, 200, 0) over black.
 Magic_Update: magic goes up 4 a frame (MAGIC_STATE_ADD) and down 2 (MAGIC_STATE_CONSUME). While
 the spin attack is lit its magic shows in yellow (MAGIC_STATE_METER_FLASH_2) and the border flashes
 white / grey (sMagicBorderColors, sMagicBorderIndices, YREG(40..43)), and keeps flashing after
 he lets go until the spin is over.
*/
void CHudZelda::DrawMagic( void )
{
	static const int borderColors[2][3] = { { 255, 255, 255 }, { 150, 150, 150 } };
	static const int borderIndices[4] = { 0, 1, 1, 0 };
	static const int borderRatios[4] = { 2, 1, 2, 1 };
	static int border[3] = { 255, 255, 255 };
	static int borderRatio = 2, borderStep = 1;
	static int shown = -1;
	static float tick;

	if ( g_zl.magicCap <= 0 )
	{
		shown = -1;
		return;
	}

	int act = ZL_PredictedAction();
	int preview = act == ZACT_SPIN_CHARGE && g_pmzPredicted.time >= ZL_SPIN_CHARGE_TIME &&
				   ( ZL_PredictedFlags() & ZSF_MAGIC ) && g_zl.magic >= ZL_SPIN_MAGIC;
	int flash = preview || ( act == ZACT_SPIN && ZL_GetSub( ZL_PredictedState() ) );

	float now = gEngfuncs.GetClientTime();
	if ( shown < 0 || now < tick || now - tick > 1.0f )
	{
		shown = g_zl.magic;
		tick = now;
	}
	while ( now - tick >= 1.0f / ZL_FPS )
	{
		tick += 1.0f / ZL_FPS;
		if ( shown < g_zl.magic )
			shown = min( g_zl.magic, shown + 4 );
		else if ( shown > g_zl.magic )
			shown = max( g_zl.magic, shown - 2 );

		if ( flash )
		{
			const int *target = borderColors[borderIndices[borderStep]];
			for ( int c = 0; c < 3; c++ )
			{
				int change = abs( border[c] - target[c] ) / borderRatio;
				border[c] += ( border[c] >= target[c] ) ? -change : change;
			}
			if ( --borderRatio == 0 )
			{
				for ( int c = 0; c < 3; c++ )
					border[c] = target[c];
				borderRatio = borderRatios[borderStep];
				borderStep = ( borderStep + 1 ) & 3;
			}
		}
		else
		{
			border[0] = border[1] = border[2] = 255;
			borderRatio = 2;
		}
	}

	float y = ( g_zl.healthCap > 10 * ZL_HEART ) ? 42.0f : 34.0f;
	float s = ZDraw_Scale();
	float cap = (float)g_zl.magicCap;
	int fillTo = preview ? max( 0, shown - ZL_SPIN_MAGIC ) : shown;

	if ( !g_zlSprMagic )
	{
		// No meter sprite: plain boxes
		float x0 = ZDraw_X( 18.0f, ZA_LEFT ), y0 = ZDraw_Y( y + 1 );
		ZDraw_Rect( x0, y0, ( cap + 16 ) * s, 12 * s, border[0], border[1], border[2], 230 );
		ZDraw_Rect( x0 + s, y0 + s, ( cap + 14 ) * s, 10 * s, 100, 50, 50, 230 );
	}
	else
	{
		struct { float x, w, u0, u1; int frame; } parts[3] =
		{
			{ 18.0f, 8.0f, 0, 1, 0 },
			{ 26.0f, cap, 0, 1, 2 },
			{ 26.0f + cap, 8.0f, 1, 0, 0 },		// G_TX_MIRROR: the end the other way round
		};
		for ( int i = 0; i < 3; i++ )
		{
			float px = ZDraw_X( parts[i].x, ZA_LEFT ), py = ZDraw_Y( y );
			ZDraw_SpriteUV( g_zlSprMagic, parts[i].frame, px, py, parts[i].w * s, 16 * s, parts[i].u0, 0, parts[i].u1, 1, 100, 50, 50, 255 );
			ZDraw_SpriteUV( g_zlSprMagic, parts[i].frame + 1, px, py, parts[i].w * s, 16 * s, parts[i].u0, 0, parts[i].u1, 1, border[0], border[1], border[2], 255 );
		}
	}

	// The fill: black, then the colour by the texture's intensity (its top 7 rows)
	float fx = ZDraw_X( 26.0f, ZA_LEFT ), fy = ZDraw_Y( y + 3 );
	if ( shown > 0 )
		ZDraw_Rect( fx, fy, shown * s, 7 * s, 0, 0, 0, 255 );
	if ( g_zlSprMagic )
	{
		if ( shown > fillTo )
			ZDraw_SpriteUV( g_zlSprMagic, 4, fx + fillTo * s, fy, ( shown - fillTo ) * s, 7 * s, 0, 0, 1, 7.0f / 16.0f, 250, 250, 0, 255 );
		if ( fillTo > 0 )
			ZDraw_SpriteUV( g_zlSprMagic, 4, fx, fy, fillTo * s, 7 * s, 0, 0, 1, 7.0f / 16.0f, 0, 200, 0, 255 );
	}
	else
	{
		if ( shown > fillTo )
			ZDraw_Rect( fx + fillTo * s, fy, ( shown - fillTo ) * s, 7 * s, 250, 250, 0, 255 );
		if ( fillTo > 0 )
			ZDraw_Rect( fx, fy, fillTo * s, 7 * s, 0, 200, 0, 255 );
	}
}

void CHudZelda::DrawCount( int value, float right, float bottom, float h, int r, int g, int b )
{
	char buf[16];
	snprintf( buf, sizeof( buf ), "%d", value );
	float w = ZDraw_TextWidth( buf, h );
	ZDraw_TextShadow( buf, right - w, bottom - h * 0.8f, h, r, g, b, 255 );
}

// gCounterDigit0Tex .. 9: 8x16, 8 apart (Interface_DrawCounters). Frames 2-11 of zl_hudicons;
// sprites from older tool versions don't have them, then the message font stands in.
void CHudZelda::DrawCounterDigits( const char *text, float x, float y, int r, int g, int b )
{
	float s = ZDraw_Scale();
	int hasDigits = g_zlSprHudIcons && g_zlSprHudIcons->numframes >= 12;

	for ( int i = 0; text[i]; i++, x += 8 )
	{
		if ( hasDigits && text[i] >= '0' && text[i] <= '9' )
		{
			// Dark drop shadow one pixel down-right, like the N64's
			ZDraw_Sprite( g_zlSprHudIcons, 2 + text[i] - '0', ZDraw_X( x + 1, ZA_LEFT ), ZDraw_Y( y + 1 ), 8 * s, 16 * s, 0, 0, 0, 160 );
			ZDraw_Sprite( g_zlSprHudIcons, 2 + text[i] - '0', ZDraw_X( x, ZA_LEFT ), ZDraw_Y( y ), 8 * s, 16 * s, r, g, b, 255 );
		}
		else
		{
			char c[2] = { text[i], 0 };
			ZDraw_TextShadow( c, ZDraw_X( x, ZA_LEFT ), ZDraw_Y( y - 2 ), 18 * s, r, g, b, 255 );
		}
	}
}

// Rupee icon at (26, 206), digits from x = 42; small keys one row up at y = 190
void CHudZelda::DrawCounters( void )
{
	float s = ZDraw_Scale();

	// Rupee icon: prim (200, 255, 100), env (0, 80, 0)
	ZDraw_Sprite( g_zlSprHudIcons, 0, ZDraw_X( 26, ZA_LEFT ), ZDraw_Y( 206 ), 16 * s, 16 * s, 120, 230, 60, 255 );

	int digits = g_zl.wallet >= 200 ? 3 : 2;
	char buf[8];
	snprintf( buf, sizeof( buf ), "%0*d", digits, g_zl.rupees );

	int r = 255, g = 255, b = 255;
	if ( g_zl.rupees == g_zl.wallet )
		r = 120, g = 255, b = 0;		// full wallet
	else if ( g_zl.rupees == 0 )
		r = g = b = 100;

	DrawCounterDigits( buf, 42, 206, r, g, b );

	if ( g_zl.keys > 0 )
	{
		ZDraw_Sprite( g_zlSprHudIcons, 1, ZDraw_X( 26, ZA_LEFT ), ZDraw_Y( 190 ), 16 * s, 16 * s, 200, 230, 255, 255 );
		snprintf( buf, sizeof( buf ), "%d", g_zl.keys );
		DrawCounterDigits( buf, 42, 190, 255, 255, 255 );
	}
}

static int ZL_ItemIcon( int item )
{
	switch ( item )
	{
	case ZITEM_BOW:			return ZICON_BOW;
	case ZITEM_BOMB:		return ZICON_BOMB;
	case ZITEM_HOOKSHOT:	return ZICON_HOOKSHOT;
	case ZITEM_LONGSHOT:	return ZICON_LONGSHOT;
	case ZITEM_DEKU_NUT:	return ZICON_DEKU_NUT;
	case ZITEM_DEKU_STICK:	return ZICON_DEKU_STICK;
	case ZITEM_BOTTLE:		return ZICON_BOTTLE;
	case ZITEM_HLWEAPON:	return ZICON_HLWEAPON;
	default:				return -1;
	}
}

void CHudZelda::DrawItemIcon( int item, int weapon, float cx, float cy, float size, int alpha )
{
	int icon = ZL_ItemIcon( item );
	if ( icon < 0 )
		return;
	ZDraw_Sprite( g_zlSprIcons, icon, cx - size * 0.5f, cy - size * 0.5f, size, size, 255, 255, 255, alpha );
}

static const char *s_doActionNames[ZDO_COUNT] =
{
	"", "Attack", "Check", "Enter", "Return", "Open", "Jump", "Decide", "Dive", "Faster",
	"Throw", "Climb", "Drop", "Down", "Save", "Speak", "Next", "Grab", "Stop", "Put Away", "Reel",
};

// An IA texture drawn as lerp(black, colour, I) with its alpha: the outline frame dark, then
// alpha x intensity in the colour (rip_two_layer)
static void ZL_DrawTwoLayer( struct model_s *spr, int pair, float x, float y, float w, float h, int r, int g, int b, int a )
{
	ZDraw_Sprite( spr, pair * 2, x, y, w, h, 0, 0, 0, a );
	ZDraw_Sprite( spr, pair * 2 + 1, x, y, w, h, r, g, b, a );
}

// Interface_DrawAmmoCount: gAmmoDigit textures, 8x8, the tens at x and the ones 6 to the right;
// grey at none (and green when full, which isn't sent)
static void ZL_DrawAmmo( int ammo, float vx, float vy )
{
	float s = ZDraw_Scale();
	int c = ammo == 0 ? 100 : 255;
	if ( !g_zlSprAmmo )
	{
		char buf[8];
		snprintf( buf, sizeof( buf ), "%d", ammo );
		ZDraw_TextShadow( buf, ZDraw_X( vx, ZA_RIGHT ), ZDraw_Y( vy - 2 ), 10 * s, c, c, c, 255 );
		return;
	}
	if ( ammo > 99 )
		ammo = 99;
	if ( ammo >= 10 )
		ZL_DrawTwoLayer( g_zlSprAmmo, ammo / 10, ZDraw_X( vx, ZA_RIGHT ), ZDraw_Y( vy ), 8 * s, 8 * s, c, c, c, 255 );
	ZL_DrawTwoLayer( g_zlSprAmmo, ammo % 10, ZDraw_X( vx + 6, ZA_RIGHT ), ZDraw_Y( vy ), 8 * s, 8 * s, c, c, c, 255 );
}

/*
 Interface_DrawItemButtons / Interface_DrawActionButton: gButtonBackgroundTex in the button's
 colour (its own alpha makes it see through), B (160, 17) 29 wide, C left (227, 18) / down
 (249, 34) / right (271, 18) 27 wide; item icons 30 / 24 wide from the button's corner; empty C
 buttons show their arrow textures; the A button (186, 9) with its do_action_static label, 48 x 16
 against the button's 28. C-up only comes up when Navi calls: (254, 16) 16 wide with "Navi" (32 x 8)
 7 left of it and 4 down, blinking every 10 frames (sCUpTimer).
*/
void CHudZelda::DrawButtons( float time )
{
	float s = ZDraw_Scale();
	bool gc = zl_hud_gc && zl_hud_gc->value;
	int act = ZL_PredictedAction();

	int aR = gc ? 0 : 90, aG = gc ? 200 : 90, aB = gc ? 50 : 255;
	int bR = gc ? 255 : 0, bG = gc ? 30 : 150, bB = gc ? 30 : 0;

	// B: the sword
	{
		float x = ZDraw_X( 160, ZA_RIGHT ), y = ZDraw_Y( 17 );
		ZDraw_Sprite( g_zlSprButton, 0, x, y, 29 * s, 29 * s, bR, bG, bB, 255 );
		if ( g_zl.swordB )
		{
			int icon = g_zl.swordB == 3 ? ZICON_SWORD_BIGGORON : g_zl.swordB == 2 ? ZICON_SWORD_MASTER : ZICON_SWORD_KOKIRI;
			int alpha = ( g_zl.holding || act == ZACT_AIM || act == ZACT_TALK ) ? 70 : 255;
			ZDraw_Sprite( g_zlSprIcons, icon, x, y, 30 * s, 30 * s, 255, 255, 255, alpha );
		}
	}

	// A: do-action label
	{
		float d = 28 * s;
		float x = ZDraw_X( 186, ZA_RIGHT ), y = ZDraw_Y( 9 );
		int doAction = g_zl.doAction;

		if ( ZText_IsOpen() )
			doAction = ZDO_NEXT;
		else if ( doAction == ZDO_NONE && ( ZL_PredictedState() & ( ZL_STATE_LOCKED | ZL_STATE_PARALLEL ) ) )
			doAction = ZDO_ATTACK;

		// Interface_SetDoAction: a new action flips the button over its horizontal axis,
		// 0 -> 90 degrees showing the old label, then -90 -> 0 with the new one, at
		// 31400 / 3 ten-thousandths of a radian per 20 Hz frame (about 0.15 s in all)
		static int shown = -1, target = -1;
		static float flipStart = -1.0f;
		if ( shown < 0 )
			shown = target = doAction;
		if ( doAction != target )
		{
			target = doAction;
			flipStart = time;
		}
		float angle = 0;	// radians, the button's tilt
		const float rate = 3.14f / 3.0f * 20.0f;
		if ( flipStart >= 0 )
		{
			angle = ( time - flipStart ) * rate;
			if ( angle >= 1.57f )
			{
				shown = target;
				angle -= 3.14f;
				if ( angle >= 0 )
				{
					angle = 0;
					flipStart = -1.0f;
				}
			}
		}
		float squash = fabsf( cosf( angle ) );
		float dy = d * squash;

		ZDraw_Sprite( g_zlSprButton, 0, x, y + ( d - dy ) * 0.5f, d, dy, aR, aG, aB, 255 );

		if ( shown > 0 && shown < ZDO_COUNT && squash > 0.05f )
		{
			if ( g_zlSprDoAction && g_zlSprDoAction->numframes >= ZDO_COUNT * 2 )
			{
				float lw = d * 48.0f / 28.0f, lh = d * 16.0f / 28.0f * squash;
				ZL_DrawTwoLayer( g_zlSprDoAction, shown, x + ( d - lw ) * 0.5f, y + ( d - lh ) * 0.5f, lw, lh, 255, 255, 255, 255 );
			}
			else
			{
				const char *label = s_doActionNames[shown];
				float h = 11 * s;
				float w = ZDraw_TextWidth( label, h );
				if ( w > d * 1.3f )
				{
					h *= d * 1.3f / w;
					w = ZDraw_TextWidth( label, h );
				}
				ZDraw_SetTextYScale( squash );
				ZDraw_TextShadow( label, x + ( d - w ) * 0.5f, y + d * 0.5f - h * 0.55f, h, 255, 255, 255, 255 );
				ZDraw_SetTextYScale( 1.0f );
			}
		}
	}

	// C buttons (R_C_BTN_COLOR 255, 160, 0)
	static const float cx[3] = { 227, 249, 271 };
	static const float cy[3] = { 18, 34, 18 };
	static const float arrowDeg[3] = { -90, 180, 90 };

	for ( int i = 0; i < 3; i++ )
	{
		float d = 27 * s;
		float x = ZDraw_X( cx[i], ZA_RIGHT ), y = ZDraw_Y( cy[i] );

		ZDraw_Sprite( g_zlSprButton, 0, x, y, d, d, 255, 160, 0, 255 );

		if ( g_zl.cItem[i] == ZITEM_NONE )
		{
			if ( g_zlSprButton && g_zlSprButton->numframes >= 6 )
				ZDraw_Sprite( g_zlSprButton, 3 + i, x, y, d, d, 255, 160, 0, 255 );
			else
				ZDraw_SpriteRotated( g_zlSprButton, 2, x + d * 0.5f, y + d * 0.5f, d * 0.5f, d * 0.5f, arrowDeg[i], 120, 70, 0, 220 );
			continue;
		}

		int alpha = ( act == ZACT_TALK || act == ZACT_DEAD ) ? 70 : 255;
		DrawItemIcon( g_zl.cItem[i], g_zl.cWeapon[i], x + 12 * s, y + 12 * s, 24 * s, alpha );

		if ( g_zl.cAmmo[i] >= 0 )
			ZL_DrawAmmo( g_zl.cAmmo[i], cx[i] + 1, cy[i] + 17 );
	}

	// C-up: Navi calling
	static float naviStart = -1.0f;
	if ( !( ZL_PredictedFlags() & ZSF_NAVI ) || ZText_IsOpen() )
	{
		naviStart = -1.0f;
		return;
	}
	if ( naviStart < 0 || naviStart > time )
		naviStart = time;
	if ( (int)( ( time - naviStart ) * ZL_FPS / 10.0f ) & 1 )
		return;		// sCUpInvisible
	{
		float d = 16 * s;
		ZDraw_Sprite( g_zlSprButton, 0, ZDraw_X( 254, ZA_RIGHT ), ZDraw_Y( 16 ), d, d, 255, 160, 0, 255 );
		float lx = ZDraw_X( 254 - 7, ZA_RIGHT ), ly = ZDraw_Y( 16 + 4 );
		if ( g_zlSprNavi )
			ZL_DrawTwoLayer( g_zlSprNavi, 0, lx, ly, 32 * s, 8 * s, 255, 255, 255, 255 );
		else
			ZDraw_TextShadow( "Navi", lx, ly - 2 * s, 8 * s, 255, 255, 255, 255 );
	}
}

static int ZL_ProjectToScreen( const Vector &world, float *sx, float *sy )
{
	float screen[3];
	if ( gEngfuncs.pTriAPI->WorldToScreen( (float *)&world.x, screen ) )
		return 0;	// behind the camera
	*sx = ( 1.0f + screen[0] ) * ScreenWidth * 0.5f;
	*sy = ( 1.0f - screen[1] ) * ScreenHeight * 0.5f;
	return 1;
}

static cl_entity_t *ZL_VisibleEntity( int idx )
{
	if ( idx <= 0 )
		return NULL;
	cl_entity_t *ent = gEngfuncs.GetEntityByIndex( idx );
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	if ( !ent || !local || !ent->model )
		return NULL;
	if ( ent->curstate.messagenum < local->curstate.messagenum )
		return NULL;	// not in the last packet
	return ent;
}

// Attention_Draw: four triangles spin around the locked actor and close in on lock-on;
// the arrow floats over whatever Z would lock onto
void CHudZelda::DrawReticles( float time )
{
	if ( ZCam_IsFirstPerson() || ZText_IsOpen() )
		return;

	cl_entity_t *locked = ZL_VisibleEntity( g_zl.lockIdx );
	if ( locked )
	{
		Vector focus = locked->origin + Vector( 0.0f, 0.0f, g_zl.focusHeight );
		float sx, sy, ex, ey;

		Vector right;
		AngleVectors( v_angles, NULL, right, NULL );

		if ( ZL_ProjectToScreen( focus, &sx, &sy ) && ZL_ProjectToScreen( focus + right * 28.0f, &ex, &ey ) )
		{
			float radius = sqrtf( ( ex - sx ) * ( ex - sx ) + ( ey - sy ) * ( ey - sy ) );
			radius = max( radius, 14.0f * ZDraw_Scale() );

			// Converge from far out over a quarter second (reticleRadius 500 -> 80)
			float t = ( time - g_zl.lockTime ) / 0.25f;
			if ( t < 1.0f )
				radius *= 1.0f + 3.0f * ( 1.0f - t ) * ( 1.0f - t );

			bool hostile = ( g_zl.targetFlags & ZTGT_HOSTILE ) != 0;
			int r = hostile ? 255 : 150, g = hostile ? 255 : 150, b = hostile ? 0 : 255;
			float spin = time * 120.0f;
			float tri = 12.0f * ZDraw_Scale();

			for ( int i = 0; i < 4; i++ )
			{
				float a = ( spin + i * 90.0f ) * ( M_PI / 180.0f );
				float px = sx + cosf( a ) * radius;
				float py = sy + sinf( a ) * radius;
				// triangle sprite points down (+y); rotate so it points at the centre
				ZDraw_SpriteRotated( g_zlSprReticle, 0, px, py, tri, tri, spin + i * 90.0f + 90.0f, r, g, b, 210 );
			}
		}
		return;
	}

	cl_entity_t *hover = ZL_VisibleEntity( g_zl.hoverIdx );
	if ( hover )
	{
		float top = hover->curstate.maxs[2] > 0 ? hover->curstate.maxs[2] : 72.0f;
		Vector above = hover->origin + Vector( 0.0f, 0.0f, top + 18.0f + 4.0f * sinf( time * 6.0f ) );
		float sx, sy;

		if ( ZL_ProjectToScreen( above, &sx, &sy ) )
		{
			bool hostile = ( g_zl.targetFlags & ZTGT_HOVER_HOSTILE ) != 0;
			int r = hostile ? 255 : 150, g = hostile ? 255 : 150, b = hostile ? 0 : 255;
			float size = 14.0f * ZDraw_Scale();
			ZDraw_Sprite( g_zlSprReticle, 1, sx - size * 0.5f, sy - size, size, size, r, g, b, 230 );
		}
	}
}

// First person bow / hookshot. The hookshot has OoT's red dot where it would hit
// (Player_DrawHookshotReticle); the bow gets a small cross in the middle.
void CHudZelda::DrawAimCrosshair( void )
{
	if ( ZL_PredictedAction() != ZACT_AIM )
		return;

	int item = g_zl.cItem[min( 2, max( 0, ZL_GetSub( ZL_PredictedState() ) ) )];
	if ( item == ZITEM_HOOKSHOT || item == ZITEM_LONGSHOT )
	{
		// gLinkAdultHookshotReticleDL's texture is 30 model units across, drawn at scale 0.08 out
		// to 200 units away and bigger further off, so it keeps its size on screen from there
		Vector dot, fwd, right, up;
		float sx, sy, ex, ey;
		if ( !ZCam_HookDot( dot ) )
			return;
		AngleVectors( v_angles, fwd, right, up );
		float depth = max( 1.0f, DotProduct( dot - v_origin, fwd ) );
		float size = ZL_DIST( 2.4f ) * max( 1.0f, depth / ZL_DIST( 200.0f ) );
		if ( !ZL_ProjectToScreen( dot, &sx, &sy ) || !ZL_ProjectToScreen( dot + right * size, &ex, &ey ) )
			return;
		float px = max( 2.0f, sqrtf( ( ex - sx ) * ( ex - sx ) + ( ey - sy ) * ( ey - sy ) ) );
		ZDraw_Sprite( g_zlSprHookDot, 0, sx - px * 0.5f, sy - px * 0.5f, px, px, 255, 0, 0, 255 );
		return;
	}

	// OoT has no crosshair for the bow: you aim along the arrow. zl_bow_crosshair 1 adds one.
	if ( !zl_bow_crosshair || !zl_bow_crosshair->value )
		return;

	float s = ZDraw_Scale();
	float cx = ScreenWidth * 0.5f, cy = ScreenHeight * 0.5f;
	ZDraw_Rect( cx - 6 * s, cy - 0.5f * s, 4 * s, 1 * s, 255, 255, 255, 220 );
	ZDraw_Rect( cx + 2 * s, cy - 0.5f * s, 4 * s, 1 * s, 255, 255, 255, 220 );
	ZDraw_Rect( cx - 0.5f * s, cy - 6 * s, 1 * s, 4 * s, 255, 255, 255, 220 );
	ZDraw_Rect( cx - 0.5f * s, cy + 2 * s, 1 * s, 4 * s, 255, 255, 255, 220 );
}

// Item get: Link holds the item up while the text box is open
void CHudZelda::DrawItemGet( float time )
{
	if ( ZL_PredictedAction() != ZACT_GETITEM || time - g_zl.itemGetTime > 30.0f || g_zl.itemGet3D )
		return;

	float s = ZDraw_Scale();
	float size = 36 * s;
	float bob = sinf( ( time - g_zl.itemGetTime ) * 3.0f ) * 2 * s;
	ZDraw_Sprite( g_zlSprIcons, g_zl.itemGetIcon, ScreenWidth * 0.5f - size * 0.5f, ZDraw_Y( 80 ) + bob, size, size, 255, 255, 255, 255 );
}

/*
 KaleidoScope_DrawGameOver and the continue prompt (single player). From when link_derth_rebirth
 stops (frame 56) and 20 more (GAMEOVER_DEATH_DELAY_MENU): "GAME OVER" at (64, 98), its colours
 coming round over 30 frames (PRIMITIVE 255,130,0 alpha 0 -> 30,0,0 alpha 255, ENVIRONMENT the
 other way), the shine scrolling half a row a frame; 40 frames on it rises 3 a frame to 66 and
 "Continue playing?" comes up with Yes / No. Multiplayer: a line saying how to respawn.
*/
static void ZL_DrawGameOver( float time )
{
	float f = ZDeath_Frames();
	float s = ZDraw_Scale();
	const float start = 56.0f + 20.0f;

	if ( ZDeath_Multiplayer() )
	{
		if ( f >= start )
		{
			const char *msg = "Press A to respawn";
			float h = 10 * s;
			ZDraw_TextShadow( msg, ScreenWidth * 0.5f - strlen( msg ) * h * 0.28f, ZDraw_Y( 200 ), h, 255, 255, 255, 255 );
		}
		return;
	}

	if ( f < start || !g_zlSprGameOver || g_zlSprGameOver->numframes < 8 )
		return;

	float k = min( 1.0f, ( f - start ) / 30.0f );
	int pr = (int)( 255 + ( 30 - 255 ) * k ), pg = (int)( 130 - 130 * k ), pa = (int)( 255 * k );
	int er = (int)( 30 + ( 255 - 30 ) * k ), eg = (int)( 130 * k );

	float rise = f - ( start + 30.0f + 40.0f );
	float ty = 98.0f - ( rise > 0 ? min( 32.0f, rise * 3.0f ) : 0.0f );
	float tx = ZDraw_X( 64, ZA_CENTER ), y = ZDraw_Y( ty );
	float w = 192 * s, h = 32 * s;

	ZDraw_Sprite( g_zlSprGameOver, 0, tx, y, w, h, pr, pg, 0, pa );
	ZDraw_Sprite( g_zlSprGameOver, 1, tx, y, w, h, er, eg, 0, pa );
	if ( g_zlSprGameOver->numframes >= 40 )
	{
		int shift = ( (int)( f * 0.5f ) ) % 32;
		ZDraw_Sprite( g_zlSprGameOver, 8 + ( 32 - shift ) % 32, tx, y, w, h, er, eg, 0, pa );
	}

	// The prompt
	if ( rise <= 0 )
		return;
	int a = (int)( 255 * min( 1.0f, rise / 11.0f ) );
	ZL_DrawTwoLayer( g_zlSprGameOver, 1, ZDraw_X( 84, ZA_CENTER ), ZDraw_Y( 128 ), 152 * s, 16 * s, 255, 255, 255, a );

	// The chosen one in the cursor's yellow, pulsing (sPromptChoice), the other white
	float pulse = 0.75f + 0.25f * sinf( time * 6.0f );
	for ( int i = 0; i < 2; i++ )
	{
		bool sel = ZDeath_Choice() == i;
		int r = 255, g = sel ? (int)( 255 * pulse ) : 255, b = sel ? 0 : 255;
		int ia = sel ? a : a * 2 / 3;
		ZL_DrawTwoLayer( g_zlSprGameOver, 2 + i, ZDraw_X( i ? 176 : 96, ZA_CENTER ), ZDraw_Y( 150 ), 48 * s, 16 * s, r, g, b, ia );
	}
}

int CHudZelda::Draw( float flTime )
{
	ZMusic_Frame( flTime );

	if ( !ZL_IsLink() || ( zl_hud && !zl_hud->value ) )
		return 1;

	if ( gHUD.m_iHideHUDDisplay & HIDEHUD_ALL )
		return 1;

	ZDraw_Begin();

	DrawLetterbox();

	// Dead: no HUD (CAM_HUD_VISIBILITY_NOTHING_ALT), then the Game Over screen
	if ( ZDeath_Active() )
	{
		ZL_DrawGameOver( flTime );
		ZDraw_End();
		return 1;
	}

	bool talking = ZText_IsOpen();
	int act = ZL_PredictedAction();

	// CAM_HUD_VISIBILITY_NOTHING_ALT while the door camera runs
	if ( ZCam_DoorHidesHud() )
	{
		ZDraw_End();
		return 1;
	}

	DrawReticles( flTime );
	DrawAimCrosshair();

	// CAM_HUD_VISIBILITY_A_HEARTS_MAGIC_FORCE while talking
	DrawHearts( flTime );
	DrawMagic();
	if ( !talking && act != ZACT_GETITEM && act != ZACT_CHEST )
		DrawCounters();
	DrawButtons( flTime );

	DrawItemGet( flTime );
	ZText_Draw( flTime );

	ZDraw_End();
	return 1;
}
