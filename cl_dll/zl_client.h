/***
*
*	Zelda 64 port: client side declarations.
*
*	zl_draw.cpp		2D drawing at OoT's 320x240 layout, scaled to the window
*	zl_hud.cpp		hearts, magic, rupees, A/B/C buttons, Z-target reticle, letterbox
*	zl_textbox.cpp	the message box (z_message.c)
*	zl_camera.cpp	the camera (z_camera.c)
*	zl_input.cpp	N64 buttons, Z-target focus into usercmd
*	zl_music.cpp	music through lazyusf2 + SDL2 audio
*	zl_effects.cpp	sword trail, spin attack ring and glow, hit marks
*
****/
#ifndef ZL_CLIENT_H
#define ZL_CLIENT_H

#include "zl_shared.h"

// MSVC's math.h only defines M_PI with _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct cl_entity_s;
struct ref_params_s;
struct usercmd_s;

// What the server has told us, plus a few client side bits
struct zl_client_t
{
	int		active;			// got ZStats from a Zelda server

	// ZStats
	int		health;
	int		healthCap;
	int		magic;
	int		magicCap;
	int		rupees;
	int		wallet;
	int		keys;
	int		doubleDefense;
	int		equipment;

	// ZButtons
	int		swordB;			// 0 none, 1 Kokiri, 2 Master, 3 Biggoron
	int		cItem[3];
	int		cWeapon[3];
	int		cAmmo[3];
	int		doAction;		// ZDO_*
	int		holding;

	// ZTarget
	int		lockIdx;
	int		hoverIdx;
	int		targetFlags;
	float	focusHeight;
	float	lockTime;		// when the current lock started (reticle converge animation)

	// Item get
	int		itemGetIcon;
	float	itemGetTime;
	int		itemGet3D;		// the server shows the item's own model over Link's head instead
};

extern zl_client_t g_zl;

// Is the local player Link right now? (server flag in the predicted state)
int ZL_IsLink( void );
int ZL_PredictedState( void );
int ZL_PredictedAction( void );
float ZL_PredictedFacing( void );
int ZL_PredictedFlags( void );

// zl_draw.cpp: virtual 320x240 screen. Elements are anchored left / right / centre so
// widescreen keeps OoT's margins.
enum zl_anchor_e
{
	ZA_LEFT = 0,
	ZA_CENTER,
	ZA_RIGHT,
};

void	ZDraw_Init( void );
void	ZDraw_VidInit( void );
float	ZDraw_Scale( void );
float	ZDraw_X( float x, int anchor );
float	ZDraw_Y( float y );
void	ZDraw_Begin( void );
void	ZDraw_End( void );
void	ZDraw_Rect( float x, float y, float w, float h, int r, int g, int b, int a );			// screen pixels
void	ZDraw_Sprite( struct model_s *spr, int frame, float x, float y, float w, float h, int r, int g, int b, int a );
void	ZDraw_SpriteRotated( struct model_s *spr, int frame, float cx, float cy, float w, float h, float deg, int r, int g, int b, int a );
float	ZDraw_TextWidth( const char *text, float height );
float	ZDraw_Char( int c, float x, float y, float height, int r, int g, int b, int a );		// returns advance
float	ZDraw_Text( const char *text, float x, float y, float height, int r, int g, int b, int a );
void	ZDraw_SetTextYScale( float scale );
void	ZDraw_TextShadow( const char *text, float x, float y, float height, int r, int g, int b, int a );
struct model_s *ZDraw_LoadSprite( const char *name );
void	ZDraw_SpriteUV( struct model_s *spr, int frame, float x, float y, float w, float h, float u0, float v0, float u1, float v1, int r, int g, int b, int a );

extern struct model_s *g_zlSprWhite;
extern struct model_s *g_zlSprFont;
extern struct model_s *g_zlSprHeart;
extern struct model_s *g_zlSprButton;
extern struct model_s *g_zlSprReticle;
extern struct model_s *g_zlSprHookDot;
extern struct model_s *g_zlSprTextbox;
extern struct model_s *g_zlSprHudIcons;
extern struct model_s *g_zlSprIcons;
extern struct model_s *g_zlSprMagic;		// meter end, end rim, middle, middle rim, fill

// Frames in sprites/zl_icons.spr (tools/zelda/make_hud_sprites.py)
enum zl_icon_e
{
	ZICON_SWORD_KOKIRI = 0,
	ZICON_SWORD_MASTER,
	ZICON_SWORD_BIGGORON,
	ZICON_BOW,
	ZICON_BOMB,
	ZICON_HOOKSHOT,
	ZICON_LONGSHOT,
	ZICON_DEKU_NUT,
	ZICON_DEKU_STICK,
	ZICON_BOTTLE,
	ZICON_HLWEAPON,
	ZICON_RUPEE,
	ZICON_KEY,
	ZICON_ARROW,
	ZICON_HEART_CONTAINER,
	ZICON_SHIELD,
	ZICON_COUNT
};

// zl_textbox.cpp
void	ZText_Init( void );
void	ZText_VidInit( void );
void	ZText_Open( const char *name, int speaker );
void	ZText_Close( void );
int		ZText_IsOpen( void );
void	ZText_Draw( float time );
void	ZText_Input( int pressed, float stickY );	// ZPAD_* edges + stick for choices
int		ZText_Speaker( void );

// zl_camera.cpp
void	ZCam_Init( void );
int		ZCam_IsThirdPerson( void );
void	ZCam_CalcRefdef( struct ref_params_s *pparams );
void	ZCam_AddMouse( float yaw, float pitch );
int		ZCam_CommandAngles( float *angles );
float	ZCam_Letterbox( void );		// 0..32 in 240 line units
int		ZCam_IsFirstPerson( void );
float	ZCam_ViewPitch( void );
int		ZCam_HookDot( float *pos );	// aiming the hookshot: where its red dot is

// zl_input.cpp
void	ZInput_Init( void );
void	ZInput_PreCreateMove( void );
void	ZInput_CreateMove( float frametime, struct usercmd_s *cmd, int active );
int		ZInput_TranslateWeaponButtons( int buttons );

// zl_music.cpp
void	ZMusic_Init( void );
void	ZMusic_Shutdown( void );
void	ZMusic_Frame( float time );
void	ZMusic_Play( const char *track, float fadein, float volume, int loop );
void	ZMusic_Stop( float fadeout );
int		ZMusic_Fanfare( const char *track );

// zl_hud.cpp
void	ZHud_Init( void );
void	ZHud_VidInit( void );
void	ZL_PlaySound( int sound, float volume );

// zl_effects.cpp: sword trail, spin attack, hit effects
void	ZFx_Init( void );
void	ZFx_VidInit( void );
void	ZFx_Draw( void );
void	ZFx_LinkDrawn( int index, struct cl_entity_s *ent, const char *label, float frame, const float (*hand)[4], const float *waist );

#endif // ZL_CLIENT_H
