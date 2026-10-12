/***
*
*	Zelda 64 port: sword effects, drawn with the triangle API.
*
*	Sword trail		EffectBlure (Player_InitCommon's D_8085470C) from the blade's base to its tip
*					while a swing's weapon info is live, every piece fading out over 4 frames
*	Spin attack		EnMThunder: the glow along the blade while charging, the ring when he lets
*					go, and its light at his waist
*	Hit effects		EffectSsHitMark and CollisionCheck_SpawnShieldParticles, sent by the server
*					(ZFx, zl_combat.cpp)
*
*	The trail and the spin attack are worked out from what each Link model is showing (its
*	sequence and bones, StudioDrawPlayer), so they work the same for every Link in view. The
*	textures are sprites baked frame by frame by tools/zelda/make_hud_sprites.py, or from your
*	ROM by rip_from_decomp.py.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "r_efx.h"
#include "dlight.h"
#include "parsemsg.h"
#include "zl_client.h"

#include <string.h>
#include <math.h>

extern vec3_t v_angles;		// view.cpp, where zl_camera.cpp puts the camera

#define ZFX_MAX_LINKS	32
#define ZFX_TRAIL_MAX	64
#define ZFX_MAX_MARKS	16

// OoT model space (the hand limb's) -> Half-Life units (make_link_model.py's SCALE)
#define ZFX_MODEL_SCALE	( 0.01f * ZL_UNIT )

static struct model_s *s_sprWhite;
static struct model_s *s_sprRing[2];		// blue, orange (zl_spinring_w / _s)
static struct model_s *s_sprCharge[2];		// zl_spincharge_b / _o
static struct model_s *s_sprHitmark;

struct zfx_sample_t
{
	vec3_t	tip, base;
	float	time;
	int		stroke;
};

struct zfx_link_t
{
	float	drawn;				// client time it was last drawn
	float	hand[3][4];			// PLAYER_LIMB_L_HAND, world (mf_9E0)
	vec3_t	waist;				// PLAYER_BODYPART_WAIST

	// Trail
	zfx_sample_t trail[ZFX_TRAIL_MAX];
	int		head, count;
	int		stroke;
	int		emitting;

	// Spin attack
	float	chargeStart;		// when the charge started, 0 = not charging
	float	charge;				// unk_858
	int		lit;				// had the magic for it (EnMThunder isn't empty)
	float	release;			// when he let go of a lit charge (the ring), 0 = none
	int		strong;				// the great spin's orange
	vec3_t	ringPos;
	float	ringYaw;
};

static zfx_link_t s_links[ZFX_MAX_LINKS];

struct zfx_mark_t
{
	vec3_t	pos;
	float	start;				// 0 = free
	int		metal;
};

static zfx_mark_t s_marks[ZFX_MAX_MARKS];

/*
=============================================================================
 What the model is doing
=============================================================================
*/

// Swings, and the last frame their weapon info is live (D_80854190 unk_0D): func_8084285C
// keeps meleeWeaponState up to there, and the trail gets a piece every frame it is
static const struct { const char *label; float lastFrame; } s_swings[] =
{
	{ "link_fighter_normal_kiru",		4 },
	{ "link_fighter_normal_kiru_finsh",	5 },
	{ "link_fighter_Lside_kiru",		4 },
	{ "link_fighter_Lside_kiru_finsh",	8 },
	{ "link_fighter_Rside_kiru",		4 },
	{ "link_fighter_Rside_kiru_finsh",	6 },
	{ "link_fighter_pierce_kiru",		3 },
	{ "link_fighter_pierce_kiru_finsh",	9 },
	{ "link_fighter_Lpower_jump_kiru",	11 },
	{ "link_fighter_Lpower_jump_kiru_h", 2 },	// _hit, labels are cut to 31 characters
	{ "link_fighter_rolling_kiru",		12 },
	{ "link_fighter_Wrolling_kiru",		16 },
};

static float ZFx_SwingLastFrame( const char *label )
{
	for ( int i = 0; i < (int)( sizeof( s_swings ) / sizeof( s_swings[0] ) ); i++ )
	{
		if ( !strcmp( label, s_swings[i].label ) )
			return s_swings[i].lastFrame;
	}
	return -1;
}

static int ZFx_IsCharging( const char *label )
{
	// power_kiru_wait / power_kiru_walk (zl_anim.cpp ZANIM_SPIN_CHARGE*)
	return !strncmp( label, "link_fighter_power_kiru_w", 25 );
}

static void ZFx_HandPoint( const float (*m)[4], float x, float y, float z, float *out )
{
	x *= ZFX_MODEL_SCALE;
	y *= ZFX_MODEL_SCALE;
	z *= ZFX_MODEL_SCALE;
	for ( int k = 0; k < 3; k++ )
		out[k] = m[k][0] * x + m[k][1] * y + m[k][2] * z + m[k][3];
}

// Has this Link the magic to light the sword (ZL_SPIN_MAGIC)? Other players' flags aren't sent,
// so they're taken to have it
static int ZFx_HasSpinMagic( int index )
{
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	if ( local && local->index - 1 == index )
		return ( ZL_PredictedFlags() & ZSF_MAGIC ) != 0;
	return 1;
}

void ZFx_LinkDrawn( int index, struct cl_entity_s *ent, const char *label, float frame, const float (*hand)[4], const float *waist )
{
	if ( index < 0 || index >= ZFX_MAX_LINKS || !label )
		return;

	zfx_link_t *l = &s_links[index];
	float now = gEngfuncs.GetClientTime();

	if ( now == l->drawn )
		return;
	if ( now < l->drawn || now - l->drawn > 0.5f )
	{
		// Back in view (or a new map): nothing from before carries on
		l->count = 0;
		l->emitting = 0;
		l->chargeStart = 0;
		l->release = 0;
	}
	l->drawn = now;
	memcpy( l->hand, hand, sizeof( l->hand ) );
	VectorCopy( waist, l->waist );

	// Charging: unk_858 goes up 0.02 a frame (func_80844E3C) once he has raised the sword. Letting
	// go of it lit (past 0.15, with the magic) makes the ring: rolling_kiru the blue one,
	// Wrolling_kiru (the great spin) the orange one.
	if ( ZFx_IsCharging( label ) )
	{
		if ( !l->chargeStart )
			l->chargeStart = now;
		l->charge = ( now - l->chargeStart - ZL_SPIN_CHARGE_START ) * ZL_FPS * 0.02f;
		if ( l->charge < 0 )
			l->charge = 0;
		if ( l->charge > 1.0f )
			l->charge = 1.0f;
		l->lit = ZFx_HasSpinMagic( index );
		l->release = 0;
	}
	else if ( l->chargeStart )
	{
		l->chargeStart = 0;
		if ( l->lit && l->charge > 0.15f && ( !strcmp( label, "link_fighter_rolling_kiru" ) || !strcmp( label, "link_fighter_Wrolling_kiru" ) ) )
		{
			l->release = now;
			l->strong = !strcmp( label, "link_fighter_Wrolling_kiru" );
			VectorCopy( waist, l->ringPos );
			l->ringYaw = ent->angles[1] + 180.0f;		// shape.rot.y = Link's + 0x8000
		}
	}

	// EnMThunder_SpinAttacking: the ring follows his waist across the floor for 8 frames
	if ( l->release && ( now - l->release ) * ZL_FPS < 8.0f )
	{
		l->ringPos[0] = waist[0];
		l->ringPos[1] = waist[1];
	}

	// Trail: sMeleeWeaponBaseOffsetFromLeftHand0 (0, 400, 0) to the tip, sMeleeWeaponLengths
	// (the model holds the Master Sword: 4000) along the hand's x
	float last = ZFx_SwingLastFrame( label );
	int emit = last >= 0 && frame <= last;
	if ( emit )
	{
		if ( !l->emitting )
			l->stroke++;

		zfx_sample_t *prev = l->count ? &l->trail[( l->head + ZFX_TRAIL_MAX - 1 ) % ZFX_TRAIL_MAX] : NULL;
		if ( !prev || prev->stroke != l->stroke || now - prev->time >= 1.0f / 200.0f )
		{
			zfx_sample_t *s = &l->trail[l->head];
			ZFx_HandPoint( hand, 4000, 400, 0, s->tip );
			ZFx_HandPoint( hand, 0, 400, 0, s->base );
			s->time = now;
			s->stroke = l->stroke;
			l->head = ( l->head + 1 ) % ZFX_TRAIL_MAX;
			if ( l->count < ZFX_TRAIL_MAX )
				l->count++;
		}
	}
	l->emitting = emit;
}

/*
=============================================================================
 Hit effects from the server
=============================================================================
*/
static void ZFx_Spawn( int type, const float *pos )
{
	float now = gEngfuncs.GetClientTime();

	if ( type == ZFX_HITMARK_WHITE || type == ZFX_HITMARK_METAL )
	{
		// EffectSsHitMark_SpawnFixedScale: 8 frames, one texture each
		int best = 0;
		for ( int i = 0; i < ZFX_MAX_MARKS; i++ )
		{
			if ( !s_marks[i].start || now - s_marks[i].start > 8.0f / ZL_FPS )
			{
				best = i;
				break;
			}
			if ( s_marks[i].start < s_marks[best].start )
				best = i;
		}
		VectorCopy( pos, s_marks[best].pos );
		s_marks[best].start = now;
		s_marks[best].metal = ( type == ZFX_HITMARK_METAL );
		return;
	}

	if ( type == ZFX_SPARKS )
	{
		// CollisionCheck_SpawnShieldParticles: 16 streaks of light, flying out at up to 35 a
		// frame, and a blue light (0, 128, 255) over 300 units for their 8 frames. OoT's point
		// lights only light actors, like Half-Life's entity lights.
		gEngfuncs.pEfxAPI->R_SparkStreaks( (float *)pos, 16, -350, 350 );

		dlight_t *el = gEngfuncs.pEfxAPI->CL_AllocElight( 0 );
		if ( el )
		{
			VectorCopy( pos, el->origin );
			el->radius = 300.0f * ZL_UNIT;
			el->color.r = 0;
			el->color.g = 128;
			el->color.b = 255;
			el->die = now + 8.0f / ZL_FPS;
			el->decay = el->radius / ( 8.0f / ZL_FPS );
		}
	}
}

static int __MsgFunc_ZFx( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int type = READ_BYTE();
	float pos[3];
	pos[0] = READ_COORD();
	pos[1] = READ_COORD();
	pos[2] = READ_COORD();
	ZFx_Spawn( type, pos );
	return 1;
}

/*
=============================================================================
 Drawing
=============================================================================
*/

// EffectBlure, D_8085470C: each piece lives elemDuration 4 frames, the tip end going from
// (255, 255, 255, 255) to clear and the base end from alpha 64
static void ZFx_DrawTrail( zfx_link_t *l, float now )
{
	const float life = 4.0f / ZL_FPS;
	triangleapi_t *tri = gEngfuncs.pTriAPI;

	if ( l->count < 2 || !s_sprWhite || !tri->SpriteTexture( s_sprWhite, 0 ) )
		return;

	tri->RenderMode( kRenderTransAlpha );
	tri->Begin( TRI_QUADS );
	for ( int i = 0; i < l->count - 1; i++ )
	{
		zfx_sample_t *a = &l->trail[( l->head + ZFX_TRAIL_MAX - l->count + i ) % ZFX_TRAIL_MAX];
		zfx_sample_t *b = &l->trail[( l->head + ZFX_TRAIL_MAX - l->count + i + 1 ) % ZFX_TRAIL_MAX];
		float fa = 1.0f - ( now - a->time ) / life;
		float fb = 1.0f - ( now - b->time ) / life;

		if ( a->stroke != b->stroke || fa <= 0 )
			continue;
		if ( fb > 1.0f )
			fb = 1.0f;

		tri->Color4f( 1, 1, 1, fa );
		tri->TexCoord2f( 0, 0 );
		tri->Vertex3fv( a->tip );
		tri->Color4f( 1, 1, 1, fb );
		tri->TexCoord2f( 1, 0 );
		tri->Vertex3fv( b->tip );
		tri->Color4f( 1, 1, 1, fb * ( 64.0f / 255.0f ) );
		tri->TexCoord2f( 1, 1 );
		tri->Vertex3fv( b->base );
		tri->Color4f( 1, 1, 1, fa * ( 64.0f / 255.0f ) );
		tri->TexCoord2f( 0, 1 );
		tri->Vertex3fv( a->base );
	}
	tri->End();
}

/*
 The charging glow, gSpinAttackChargingDL: a six sided tube along the blade, in the hand's space
 through EnMThunder_Draw's Matrix_Translate(0, 300, -100), Matrix_Scale(-1.2, -1.0, -0.7),
 Matrix_RotateX(0x4000) (the Master Sword's), and pulsing in thickness with
 sSpinChargeScale[gameplayFrames & 7] (x6 once it's orange, x2 before).
*/
static const float s_glowRing[2][7][3] =
{
	// x, y, z of the tube's ends; the seventh closes it
	{ { 411, 0, 480 }, { 411, 312, 240 }, { 411, 312, -240 }, { 411, 0, -480 }, { 411, -312, -240 }, { 411, -312, 240 }, { 411, 0, 480 } },
	{ { -3725, 0, 307 }, { -3725, 175, 154 }, { -3725, 175, -154 }, { -3725, 0, -307 }, { -3725, -175, -154 }, { -3725, -175, 154 }, { -3725, 0, 307 } },
};
static const float s_glowU[2][7] =
{
	{ 0.0f, 0.128f, 0.372f, 0.5f, 0.628f, 0.872f, 1.0f },
	{ 0.0f, 0.117f, 0.383f, 0.5f, 0.617f, 0.883f, 1.0f },
};
static const float s_spinChargeScale[8] = { 0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.25f, 0.2f, 0.15f };

static void ZFx_DrawCharge( zfx_link_t *l, int index, float now )
{
	triangleapi_t *tri = gEngfuncs.pTriAPI;
	float alpha;
	float charge = l->charge;

	// chargeAlpha: up from 0.1 to full at 0.15; after he lets go it stays for followPlayerTimer
	// and then drops 40 a frame
	if ( l->chargeStart )
	{
		if ( charge <= 0.1f || !l->lit )
			return;
		alpha = charge < 0.15f ? ( charge - 0.1f ) * 20.0f : 1.0f;
	}
	else if ( l->release )
	{
		float k = ( now - l->release ) * ZL_FPS;
		alpha = k < 7.0f ? 1.0f : 1.0f - ( k - 6.0f ) * ( 40.0f / 255.0f );
		if ( alpha <= 0 )
			return;
	}
	else
		return;

	int strong = charge >= 0.85f;
	int gameFrame = (int)( now * ZL_FPS );
	float k = s_spinChargeScale[gameFrame & 7] * ( strong ? 6.0f : 2.0f ) + 1.0f;

	struct model_s *spr = s_sprCharge[strong];
	if ( !spr || !tri->SpriteTexture( spr, gameFrame % 128 ) )
		return;

	float pts[2][7][3];
	for ( int e = 0; e < 2; e++ )
	{
		for ( int j = 0; j < 7; j++ )
		{
			const float *v = s_glowRing[e][j];
			// (x, y, z) -> Matrix_Scale(1, k, k), RotateX 90: (x, -k z, k y), then the scale
			// and translation: (-1.2 x, k z + 300, -0.7 k y - 100)
			ZFx_HandPoint( (const float (*)[4])l->hand, -1.2f * v[0], k * v[2] + 300.0f, -0.7f * k * v[1] - 100.0f, pts[e][j] );
		}
	}

	tri->RenderMode( kRenderTransAdd );
	tri->Color4f( 1, 1, 1, alpha );
	tri->Begin( TRI_QUADS );
	for ( int j = 0; j < 6; j++ )
	{
		// The texture runs from the tip (t -0x1E) to the hilt (t 0x806, twice its height)
		tri->TexCoord2f( s_glowU[0][j], 1 );		tri->Vertex3fv( pts[0][j] );
		tri->TexCoord2f( s_glowU[0][j + 1], 1 );	tri->Vertex3fv( pts[0][j + 1] );
		tri->TexCoord2f( s_glowU[1][j + 1], 0 );	tri->Vertex3fv( pts[1][j + 1] );
		tri->TexCoord2f( s_glowU[1][j], 0 );		tri->Vertex3fv( pts[1][j] );
	}
	tri->End();
}

/*
 The ring, gSpinAttack1DL + 2DL (blue) or 3DL + 4DL (orange): a flat ring and a band around him,
 each a strip of 12, the inner / upper edge turned 15 degrees from the outer / lower one. The
 actor's scale grows from 0.1 to 4 (8 for the great spin) by Math_SmoothStepToF(0.6, 0.8), and
 EnMThunder_Draw scales everything by 0.02 more.
*/
struct zfx_ring_shape_t
{
	float r0, y0;		// outer / lower edge (t = 1)
	float r1, y1;		// inner / upper edge (t = 0)
};

static const zfx_ring_shape_t s_ringShapes[2][2] =
{
	{ { 1500, 0, 1019, 0 }, { 1230, -250, 1272, 250 } },	// gSpinAttack1Vtx, 2Vtx
	{ { 1650, 0, 916, 0 }, { 1230, -500, 1272, 500 } },		// gSpinAttack3Vtx, 4Vtx
};

static float ZFx_RingScale( int strong, float frames )
{
	float target = strong ? 8.0f : 4.0f;
	float s = 0.1f, prev = 0.1f;
	int n = (int)frames;
	for ( int i = 0; i <= n; i++ )
	{
		prev = s;
		float step = ( target - s ) * 0.6f;
		if ( step > 0.8f )
			step = 0.8f;
		s += step;
	}
	return prev + ( s - prev ) * ( frames - n );
}

static void ZFx_DrawRing( zfx_link_t *l, float now )
{
	triangleapi_t *tri = gEngfuncs.pTriAPI;

	if ( !l->release )
		return;

	// spinAttackTimer 1 -> 0 at 1/16 a frame; drawn 15 frames, full until it's down to 0.6
	float frames = ( now - l->release ) * ZL_FPS;
	int f = (int)frames;
	if ( f >= 15 )
		return;
	float timer = 1.0f - ( f + 1 ) / 16.0f;
	float alpha = timer > 0.6f ? 1.0f : timer * ( 5.0f / 3.0f );
	float scale = ZFx_RingScale( l->strong, frames ) * 0.02f * ZL_UNIT;

	struct model_s *spr = s_sprRing[l->strong];
	if ( !spr )
		return;

	float yaw = l->ringYaw * ( M_PI / 180.0f );
	float cy = cos( yaw ), sy = sin( yaw );

	tri->RenderMode( kRenderTransAdd );
	tri->Color4f( 1, 1, 1, alpha );

	for ( int layer = 0; layer < 2; layer++ )
	{
		const zfx_ring_shape_t *sh = &s_ringShapes[l->strong][layer];
		if ( !tri->SpriteTexture( spr, layer * 15 + f ) )
			continue;

		tri->Begin( TRI_QUADS );
		for ( int i = 0; i < 12; i++ )
		{
			float pts[4][3], uv[4][2];
			for ( int c = 0; c < 4; c++ )
			{
				int seg = i + ( c == 1 || c == 2 );
				int inner = ( c >= 2 );
				// The outer edge at 90 - 30 * seg degrees, u = seg / 12 (0.25 - angle / 360)
				float ang = ( 90.0f - 30.0f * seg + ( inner ? 15.0f : 0.0f ) ) * ( M_PI / 180.0f );
				float r = inner ? sh->r1 : sh->r0;
				float ox = r * cos( ang ), oz = r * sin( ang ), oy = inner ? sh->y1 : sh->y0;
				// OoT +z is the actor's forward, +x its left, +y up
				pts[c][0] = l->ringPos[0] + ( cy * oz - sy * ox ) * scale;
				pts[c][1] = l->ringPos[1] + ( sy * oz + cy * ox ) * scale;
				pts[c][2] = l->ringPos[2] + oy * scale;
				uv[c][0] = seg / 12.0f;
				uv[c][1] = inner ? 0.0f : 1.0f;
			}
			for ( int c = 0; c < 4; c++ )
			{
				tri->TexCoord2f( uv[c][0], uv[c][1] );
				tri->Vertex3fv( pts[c] );
			}
		}
		tri->End();
	}
}

// EnMThunder_Update's light: (255, 255, 100) x spinAttackTimer over 800 x it. While charging the
// timer only gets up to 0.5 (0.1 .. 0.15); letting go lit sets it to 1 and it runs down.
static void ZFx_SpinLight( zfx_link_t *l, int index, float now )
{
	float t = 0;
	const float *pos = l->waist;

	if ( l->chargeStart && l->charge > 0.1f && l->lit )
	{
		t = ( l->charge - 0.1f ) * 10.0f;
		if ( t > 0.5f )
			t = 0.5f;
	}
	else if ( l->release )
	{
		t = 1.0f - ( (int)( ( now - l->release ) * ZL_FPS ) + 1 ) / 16.0f;
		pos = l->ringPos;
	}
	if ( t <= 0 )
		return;

	dlight_t *el = gEngfuncs.pEfxAPI->CL_AllocElight( 0x5A00 + index );
	if ( !el )
		return;
	VectorCopy( pos, el->origin );
	el->radius = 800.0f * t * ZL_UNIT;
	el->color.r = (unsigned char)( t * 255 );
	el->color.g = (unsigned char)( t * 255 );
	el->color.b = (unsigned char)( t * 100 );
	el->die = now + 0.1f;
	el->decay = 0;
}

// EffectSsHitMark_Draw: a billboard, gEffHitMarkVtx (-11 .. 12) at scale 300 / 100
static void ZFx_DrawMarks( float now )
{
	triangleapi_t *tri = gEngfuncs.pTriAPI;
	vec3_t fwd, right, up;

	if ( !s_sprHitmark )
		return;

	gEngfuncs.pfnAngleVectors( v_angles, fwd, right, up );
	tri->RenderMode( kRenderTransAdd );
	tri->Color4f( 1, 1, 1, 1 );

	for ( int i = 0; i < ZFX_MAX_MARKS; i++ )
	{
		zfx_mark_t *m = &s_marks[i];
		if ( !m->start )
			continue;
		int k = (int)( ( now - m->start ) * ZL_FPS );
		if ( k < 0 || k >= 8 )
		{
			m->start = 0;
			continue;
		}
		if ( !tri->SpriteTexture( s_sprHitmark, m->metal * 8 + k ) )
			continue;

		const float lo = -11.0f * 3.0f * ZL_UNIT, hi = 12.0f * 3.0f * ZL_UNIT;
		const float corners[4][4] = { { lo, hi, 0, 0 }, { hi, hi, 1, 0 }, { hi, lo, 1, 1 }, { lo, lo, 0, 1 } };
		tri->Begin( TRI_QUADS );
		for ( int c = 0; c < 4; c++ )
		{
			vec3_t p;
			for ( int a = 0; a < 3; a++ )
				p[a] = m->pos[a] + right[a] * corners[c][0] + up[a] * corners[c][1];
			tri->TexCoord2f( corners[c][2], corners[c][3] );
			tri->Vertex3fv( p );
		}
		tri->End();
	}
}

void ZFx_Draw( void )
{
	triangleapi_t *tri = gEngfuncs.pTriAPI;
	float now = gEngfuncs.GetClientTime();

	if ( !g_zl.active )
		return;

	tri->CullFace( TRI_NONE );

	for ( int i = 0; i < ZFX_MAX_LINKS; i++ )
	{
		zfx_link_t *l = &s_links[i];
		if ( !l->drawn || now - l->drawn > 0.5f || now < l->drawn )
			continue;

		ZFx_DrawRing( l, now );
		if ( now - l->drawn < 0.1f )
		{
			ZFx_DrawTrail( l, now );
			ZFx_DrawCharge( l, i, now );
		}
		ZFx_SpinLight( l, i, now );

		if ( l->release && ( now - l->release ) * ZL_FPS >= 16.0f )
			l->release = 0;
	}

	ZFx_DrawMarks( now );

	tri->RenderMode( kRenderNormal );
	tri->Color4f( 1, 1, 1, 1 );
	tri->CullFace( TRI_FRONT );
}

void ZFx_Init( void )
{
	HOOK_MESSAGE( ZFx );
}

void ZFx_VidInit( void )
{
	s_sprWhite = ZDraw_LoadSprite( "sprites/zl_white.spr" );
	s_sprRing[0] = ZDraw_LoadSprite( "sprites/zl_spinring_w.spr" );
	s_sprRing[1] = ZDraw_LoadSprite( "sprites/zl_spinring_s.spr" );
	s_sprCharge[0] = ZDraw_LoadSprite( "sprites/zl_spincharge_b.spr" );
	s_sprCharge[1] = ZDraw_LoadSprite( "sprites/zl_spincharge_o.spr" );
	s_sprHitmark = ZDraw_LoadSprite( "sprites/zl_hitmark.spr" );

	memset( s_links, 0, sizeof( s_links ) );
	memset( s_marks, 0, sizeof( s_marks ) );
}
