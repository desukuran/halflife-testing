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

extern vec3_t v_angles;		// view.cpp: the final render angles

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
}

void ZHud_VidInit( void )
{
	g_HudZelda.VidInit();
}

int CHudZelda::Init( void )
{
	HOOK_MESSAGE( ZStats );
	HOOK_MESSAGE( ZButtons );
	HOOK_MESSAGE( ZTarget );
	HOOK_MESSAGE( ZText );
	HOOK_MESSAGE( ZMusic );
	HOOK_MESSAGE( ZItemGet );

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
void CHudZelda::DrawMagic( void )
{
	if ( g_zl.magicCap <= 0 )
		return;

	float y = ( g_zl.healthCap > 10 * ZL_HEART ) ? 42.0f : 34.0f;
	float x0 = ZDraw_X( 18.0f, ZA_LEFT );
	float y0 = ZDraw_Y( y );
	float s = ZDraw_Scale();
	float w = ( g_zl.magicCap + 8 ) * s;
	float h = 10.0f * s;

	// Frame (gMagicMeterEndTex / MidTex), dark border
	ZDraw_Rect( x0, y0, w, h, 255, 255, 255, 230 );
	ZDraw_Rect( x0 + s, y0 + s, w - 2 * s, h - 2 * s, 30, 30, 30, 230 );

	// Fill R_MAGIC_FILL_COLOR (0, 200, 0)
	float fill = g_zl.magic * s;
	ZDraw_Rect( x0 + 4 * s, y0 + 3 * s, fill, h - 6 * s, 0, 200, 0, 255 );
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

// A (186, 9), B (160, 17) width 29, C left (227, 18) / down (249, 34) / right (271, 18) width 27
void CHudZelda::DrawButtons( float time )
{
	float s = ZDraw_Scale();
	bool gc = zl_hud_gc && zl_hud_gc->value;
	int act = ZL_PredictedAction();

	int aR = gc ? 0 : 90, aG = gc ? 200 : 90, aB = gc ? 50 : 255;
	int bR = gc ? 255 : 0, bG = gc ? 30 : 150, bB = gc ? 30 : 0;

	// B: the sword
	{
		float d = 29 * s;
		float x = ZDraw_X( 160, ZA_RIGHT ), y = ZDraw_Y( 17 );
		ZDraw_Sprite( g_zlSprButton, 0, x, y, d, d, bR, bG, bB, 200 );
		if ( g_zl.swordB )
		{
			int icon = g_zl.swordB == 3 ? ZICON_SWORD_BIGGORON : g_zl.swordB == 2 ? ZICON_SWORD_MASTER : ZICON_SWORD_KOKIRI;
			int alpha = ( g_zl.holding || act == ZACT_AIM || act == ZACT_TALK ) ? 80 : 255;
			ZDraw_Sprite( g_zlSprIcons, icon, x + 2 * s, y + 2 * s, d - 4 * s, d - 4 * s, 255, 255, 255, alpha );
		}
	}

	// A: do-action label
	{
		float d = 29 * s;
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

		ZDraw_Sprite( g_zlSprButton, 0, x, y + ( d - dy ) * 0.5f, d, dy, aR, aG, aB, shown ? 220 : 120 );

		const char *label = ( shown > 0 && shown < ZDO_COUNT ) ? s_doActionNames[shown] : "";
		if ( label[0] && squash > 0.05f )
		{
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

	// C buttons (R_C_BTN_COLOR 255, 160, 0)
	static const float cx[3] = { 227, 249, 271 };
	static const float cy[3] = { 18, 34, 18 };
	static const float arrowDeg[3] = { -90, 180, 90 };

	for ( int i = 0; i < 3; i++ )
	{
		float d = 27 * s;
		float x = ZDraw_X( cx[i], ZA_RIGHT ), y = ZDraw_Y( cy[i] );

		ZDraw_Sprite( g_zlSprButton, 0, x, y, d, d, 255, 160, 0, 170 );

		if ( g_zl.cItem[i] == ZITEM_NONE )
		{
			ZDraw_SpriteRotated( g_zlSprButton, 2, x + d * 0.5f, y + d * 0.5f, d * 0.5f, d * 0.5f, arrowDeg[i], 120, 70, 0, 220 );
			continue;
		}

		int alpha = ( act == ZACT_TALK || act == ZACT_DEAD ) ? 90 : 255;
		DrawItemIcon( g_zl.cItem[i], g_zl.cWeapon[i], x + d * 0.5f, y + d * 0.5f, d * 0.85f, alpha );

		if ( g_zl.cAmmo[i] >= 0 )
		{
			int r = 255, g = 255, b = 255;
			if ( g_zl.cAmmo[i] == 0 )
				r = g = b = 130;
			DrawCount( g_zl.cAmmo[i], x + d + 1 * s, y + d + 1 * s, 10 * s, r, g, b );
		}
	}

	// C-up: first person view (C_UP_BUTTON 254, 16), small
	{
		float d = 12 * s;
		float x = ZDraw_X( 254, ZA_RIGHT ), y = ZDraw_Y( 4 );
		ZDraw_Sprite( g_zlSprButton, 0, x, y, d, d, 255, 160, 0, 170 );
		ZDraw_SpriteRotated( g_zlSprButton, 2, x + d * 0.5f, y + d * 0.5f, d * 0.6f, d * 0.6f, 0, 120, 70, 0, 230 );
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

// First person bow / hookshot: OoT shows a small reticle in the middle
void CHudZelda::DrawAimCrosshair( void )
{
	if ( ZL_PredictedAction() != ZACT_AIM )
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
	if ( ZL_PredictedAction() != ZACT_GETITEM || time - g_zl.itemGetTime > 30.0f )
		return;

	float s = ZDraw_Scale();
	float size = 36 * s;
	float bob = sinf( ( time - g_zl.itemGetTime ) * 3.0f ) * 2 * s;
	ZDraw_Sprite( g_zlSprIcons, g_zl.itemGetIcon, ScreenWidth * 0.5f - size * 0.5f, ZDraw_Y( 80 ) + bob, size, size, 255, 255, 255, 255 );
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

	bool talking = ZText_IsOpen();
	int act = ZL_PredictedAction();

	DrawReticles( flTime );
	DrawAimCrosshair();

	// CAM_HUD_VISIBILITY_A_HEARTS_MAGIC_FORCE while talking
	DrawHearts( flTime );
	DrawMagic();
	if ( !talking && act != ZACT_GETITEM )
		DrawCounters();
	DrawButtons( flTime );

	DrawItemGet( flTime );
	ZText_Draw( flTime );

	ZDraw_End();
	return 1;
}
