/***
*
*	Zelda 64 port: input.
*
*	N64 pad -> Half-Life buttons (see ZBTN_* in pm_shared/zl_shared.h):
*
*		+zbtn_a			A		IN_JUMP		roll, sidehop, backflip, jump attack, talk, open, throw
*		+zbtn_b			B		IN_ATTACK	sword
*		+zbtn_z			Z		IN_ALT1		Z-target
*		+zbtn_r			R		IN_DUCK		shield
*		+zbtn_cleft		C<		IN_ATTACK2	item
*		+zbtn_cdown		Cv		IN_RELOAD	item
*		+zbtn_cright	C>		IN_RUN		item
*		+zbtn_cup		C^		IN_SCORE	first person look
*
*	Movement keys / a joystick are the control stick. The camera is the mouse.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "usercmd.h"
#include "kbutton.h"
#include "cl_entity.h"
#include "zl_client.h"

extern kbutton_t in_jump;
extern kbutton_t in_attack;
extern kbutton_t in_alt1;
extern kbutton_t in_duck;
extern kbutton_t in_attack2;
extern kbutton_t in_reload;
kbutton_t in_zcright;
kbutton_t in_zcup;

void KeyDown( kbutton_t *b );
void KeyUp( kbutton_t *b );

static void ZBtn_ADown( void )		{ KeyDown( &in_jump ); }
static void ZBtn_AUp( void )		{ KeyUp( &in_jump ); }
static void ZBtn_BDown( void )		{ KeyDown( &in_attack ); }
static void ZBtn_BUp( void )		{ KeyUp( &in_attack ); }
static void ZBtn_ZDown( void )		{ KeyDown( &in_alt1 ); }
static void ZBtn_ZUp( void )		{ KeyUp( &in_alt1 ); }
static void ZBtn_RDown( void )		{ KeyDown( &in_duck ); }
static void ZBtn_RUp( void )		{ KeyUp( &in_duck ); }
static void ZBtn_CLeftDown( void )	{ KeyDown( &in_attack2 ); }
static void ZBtn_CLeftUp( void )	{ KeyUp( &in_attack2 ); }
static void ZBtn_CDownDown( void )	{ KeyDown( &in_reload ); }
static void ZBtn_CDownUp( void )	{ KeyUp( &in_reload ); }
static void ZBtn_CRightDown( void )	{ KeyDown( &in_zcright ); }
static void ZBtn_CRightUp( void )	{ KeyUp( &in_zcright ); }
static void ZBtn_CUpDown( void )	{ KeyDown( &in_zcup ); }
static void ZBtn_CUpUp( void )		{ KeyUp( &in_zcup ); }

void ZInput_Init( void )
{
	gEngfuncs.pfnAddCommand( "+zbtn_a", ZBtn_ADown );
	gEngfuncs.pfnAddCommand( "-zbtn_a", ZBtn_AUp );
	gEngfuncs.pfnAddCommand( "+zbtn_b", ZBtn_BDown );
	gEngfuncs.pfnAddCommand( "-zbtn_b", ZBtn_BUp );
	gEngfuncs.pfnAddCommand( "+zbtn_z", ZBtn_ZDown );
	gEngfuncs.pfnAddCommand( "-zbtn_z", ZBtn_ZUp );
	gEngfuncs.pfnAddCommand( "+zbtn_r", ZBtn_RDown );
	gEngfuncs.pfnAddCommand( "-zbtn_r", ZBtn_RUp );
	gEngfuncs.pfnAddCommand( "+zbtn_cleft", ZBtn_CLeftDown );
	gEngfuncs.pfnAddCommand( "-zbtn_cleft", ZBtn_CLeftUp );
	gEngfuncs.pfnAddCommand( "+zbtn_cdown", ZBtn_CDownDown );
	gEngfuncs.pfnAddCommand( "-zbtn_cdown", ZBtn_CDownUp );
	gEngfuncs.pfnAddCommand( "+zbtn_cright", ZBtn_CRightDown );
	gEngfuncs.pfnAddCommand( "-zbtn_cright", ZBtn_CRightUp );
	gEngfuncs.pfnAddCommand( "+zbtn_cup", ZBtn_CUpDown );
	gEngfuncs.pfnAddCommand( "-zbtn_cup", ZBtn_CUpUp );
}

// Extra button bits for CL_ButtonBits
int ZInput_ButtonBits( int bResetState )
{
	int bits = 0;

	if ( in_zcright.state & 3 )
		bits |= ZBTN_CRIGHT;
	if ( in_zcup.state & 3 )
		bits |= ZBTN_CUP;

	if ( bResetState )
	{
		in_zcright.state &= ~2;
		in_zcup.state &= ~2;
	}
	return bits;
}

// Same translation as CLinkPlayer::ZTranslateHLButtons, for client side weapon prediction
int ZInput_TranslateWeaponButtons( int buttons )
{
	static const int cBits[3] = { ZBTN_CLEFT, ZBTN_CDOWN, ZBTN_CRIGHT };
	int out = buttons & ~( IN_ATTACK | IN_ATTACK2 | IN_RELOAD | IN_RUN | IN_JUMP | IN_DUCK | IN_ALT1 | IN_SCORE );

	for ( int i = 0; i < 3; i++ )
	{
		if ( g_zl.cItem[i] == ZITEM_HLWEAPON && ( buttons & cBits[i] ) )
			out |= IN_ATTACK;
	}
	return out;
}

static int ZInput_Pad( int buttons )
{
	int pad = 0;
	if ( buttons & ZBTN_A )			pad |= ZPAD_A;
	if ( buttons & ZBTN_B )			pad |= ZPAD_B;
	if ( buttons & ZBTN_Z )			pad |= ZPAD_Z;
	if ( buttons & ZBTN_R )			pad |= ZPAD_R;
	if ( buttons & ZBTN_CLEFT )		pad |= ZPAD_CLEFT;
	if ( buttons & ZBTN_CDOWN )		pad |= ZPAD_CDOWN;
	if ( buttons & ZBTN_CRIGHT )	pad |= ZPAD_CRIGHT;
	if ( buttons & ZBTN_CUP )		pad |= ZPAD_CUP;
	return pad;
}

void ZInput_CreateMove( float frametime, struct usercmd_s *cmd, int active )
{
	static int lastPad;

	if ( !ZL_IsLink() )
	{
		lastPad = 0;
		return;
	}

	int pad = ZInput_Pad( cmd->buttons );
	int pressed = pad & ~lastPad;
	lastPad = pad;

	// Text box eats the pad; Link is frozen anyway
	if ( ZText_IsOpen() )
	{
		float stick = 0;
		float maxs = 400.0f;
		if ( cmd->forwardmove > maxs * 0.3f )
			stick = 1;
		else if ( cmd->forwardmove < -maxs * 0.3f )
			stick = -1;

		ZText_Input( pressed, stick );

		cmd->buttons &= ~( IN_ATTACK | IN_JUMP | IN_ATTACK2 | IN_RELOAD | IN_RUN | IN_SCORE | IN_ALT1 | IN_DUCK | IN_USE );
		cmd->forwardmove = cmd->sidemove = cmd->upmove = 0;
	}

	// Where the locked actor is, for pm_zelda's facing / strafing (see PMZ_UpdateTargeting)
	cmd->impact_index = 0;
	if ( g_zl.lockIdx > 0 )
	{
		cl_entity_t *ent = gEngfuncs.GetEntityByIndex( g_zl.lockIdx );
		cl_entity_t *local = gEngfuncs.GetLocalPlayer();
		if ( ent && local && ent->model && ent->curstate.messagenum >= local->curstate.messagenum )
		{
			cmd->impact_index = ZL_CMD_TARGET_VALID;
			cmd->impact_position[0] = ent->origin[0];
			cmd->impact_position[1] = ent->origin[1];
			cmd->impact_position[2] = ent->origin[2] + g_zl.focusHeight;
		}
	}
}
