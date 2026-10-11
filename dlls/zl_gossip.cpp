/***
*
*	Zelda 64 port: the Gossip Stone (En_Gs, src/overlays/actors/ovl_En_Gs/z_en_gs.c).
*
*	monster_gossip_stone. Link can Z-target it and read it with A ("Check"). What it does when
*	hit depends on what hit it, like EnGs's damage table:
*
*		sword (DMG_SLASH / DMG_CLUB)		"boing", wobbles, tells the time	(func_80A4E910)
*		arrow, hookshot, bullet				"boing", wobbles side to side		(func_80A4EA08)
*		crushing (the Megaton Hammer's job)	squashed flat for 5 s, pops back	(func_80A4EB3C)
*		explosion							counts down, smokes, blasts off		(func_80A4ED34)
*		fire / shock / energy				spins up and stretches				(func_80A4F13C)
*
*	Everything runs at OoT's 20 frames a second with its numbers. The squash and stretch goes
*	to the client as fuser1 (width) / fuser2 (height), see StudioSetUpTransform.
*
*	Model: models/zelda/gossip_stone.mdl, made by tools/zelda/make_object_models.py from your
*	ROM. Without it a Half-Life rock stands in.
*
***/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "player.h"
#include "zl_player.h"
#include "zl_sounds.h"

#define GS_MODEL			"models/zelda/gossip_stone.mdl"
#define GS_MODEL_FALLBACK	"models/rockgibs.mdl"
#define GS_FRAME			0.05f		// one OoT frame

enum gs_action_e
{
	GS_IDLE = 0,
	GS_SWORD,		// func_80A4E910
	GS_ARROW,		// func_80A4EA08
	GS_HAMMER,		// func_80A4EB3C
	GS_BOMB,		// func_80A4ED34
	GS_SPIN,		// func_80A4F13C
	GS_GONE,		// func_80A4F700
};

// Math_SmoothStepToF (z_lib.c), returns how far is left
static float GS_SmoothStepToF( float *pValue, float target, float fraction, float step, float minStep )
{
	if ( *pValue != target )
	{
		float stepSize = ( target - *pValue ) * fraction;

		if ( stepSize >= minStep || stepSize <= -minStep )
		{
			if ( stepSize > step )
				stepSize = step;
			if ( stepSize < -step )
				stepSize = -step;
			*pValue += stepSize;
		}
		else
		{
			if ( stepSize < minStep )
			{
				*pValue += minStep;
				stepSize = minStep;
				if ( target < *pValue )
					*pValue = target;
			}
			if ( stepSize > -minStep )
			{
				*pValue += -minStep;
				if ( *pValue < target )
					*pValue = target;
			}
		}
	}
	return fabs( target - *pValue );
}

class CGossipStone : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_NONE; }
	int ObjectCaps( void ) { return ( CBaseMonster::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ) | FCAP_IMPULSE_USE; }
	int BloodColor( void ) { return DONT_BLEED; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void EXPORT StoneThink( void );

	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

private:
	void Reset( void );
	float Wobble( float fraction, float step, float minStep, int period );
	void Sound( int sound, float volume = 1.0f );
	void Sword( void );
	void Arrow( void );
	void Hammer( void );
	void Bomb( void );
	void Spin( void );

	int		m_iAction;
	int		m_iStep;		// unk_19F
	int		m_iTimer;		// unk_200
	float	m_flA, m_flB, m_flC, m_flD;	// unk_1E8 / 1EC / 1F0 / 1F4
	float	m_flScaleX, m_flScaleY;		// unk_1B4[0]
	float	m_flSpin, m_flRoll;			// unk_1A0[0].y / .z, degrees
	float	m_flBaseYaw;
	float	m_flRiseSpeed;				// OoT units / frame while flying
	float	m_flStartZ;
	EHANDLE	m_hHitter;					// who to tell the time
	EHANDLE	m_hFire;
};

LINK_ENTITY_TO_CLASS( monster_gossip_stone, CGossipStone );

TYPEDESCRIPTION CGossipStone::m_SaveData[] =
{
	DEFINE_FIELD( CGossipStone, m_flBaseYaw, FIELD_FLOAT ),
};
IMPLEMENT_SAVERESTORE( CGossipStone, CBaseMonster );

void CGossipStone::Precache( void )
{
	PRECACHE_MODEL( (char *)( ZL_FileExists( GS_MODEL ) ? GS_MODEL : GS_MODEL_FALLBACK ) );
	PRECACHE_MODEL( "sprites/fire.spr" );
}

void CGossipStone::Spawn( void )
{
	Precache();

	SET_MODEL( ENT( pev ), ZL_FileExists( GS_MODEL ) ? GS_MODEL : GS_MODEL_FALLBACK );
	// Collider: radius 21, height 48 OoT units
	UTIL_SetSize( pev, Vector( -ZL_DIST( 21 ), -ZL_DIST( 21 ), 0 ), Vector( ZL_DIST( 21 ), ZL_DIST( 21 ), ZL_DIST( 48 ) ) );

	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_YES;
	pev->health = 1000;
	pev->max_health = 1000;
	pev->flags |= FL_MONSTER;		// Z-targetable (ZIsTargetable)
	pev->deadflag = DEAD_NO;
	pev->sequence = 0;
	m_bloodColor = DONT_BLEED;
	m_flBaseYaw = pev->angles.y;

	DROP_TO_FLOOR( ENT( pev ) );
	Reset();

	SetThink( &CGossipStone::StoneThink );
	pev->nextthink = gpGlobals->time + GS_FRAME;
}

// func_80A4F77C: back to rest before a new reaction
void CGossipStone::Reset( void )
{
	m_iAction = GS_IDLE;
	m_iStep = 0;
	m_iTimer = 0;
	m_flScaleX = m_flScaleY = 1.0f;
	m_flSpin = m_flRoll = 0;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 0;
}

void CGossipStone::Sound( int sound, float volume )
{
	ZL_EmitSound( edict(), CHAN_BODY, sound, volume, ATTN_NORM, PITCH_NORM );
}

// A: "Check". 0x2053 with OoT's text when you've ripped it, the port's own words otherwise.
void CGossipStone::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CLinkPlayer *pLink = ZL_GetLink( pActivator );
	if ( pLink && m_iAction != GS_BOMB && m_iAction != GS_GONE )
		pLink->ZStartMessage( "0x2053|GOSSIP_LOOK", this, 0 );
}

void CGossipStone::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
}

// EnGs_Update: the damage table's reaction decides what happens; the stone itself can't be hurt
int CGossipStone::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( m_iAction == GS_BOMB || m_iAction == GS_GONE )
		return 0;

	int action;
	if ( bitsDamageType & DMG_BLAST )
		action = GS_BOMB;
	else if ( bitsDamageType & ( DMG_BURN | DMG_SLOWBURN | DMG_SHOCK | DMG_ENERGYBEAM ) )
		action = GS_SPIN;
	else if ( bitsDamageType & DMG_CRUSH )
		action = GS_HAMMER;
	else if ( bitsDamageType & ( DMG_SLASH | DMG_CLUB ) )
		action = GS_SWORD;
	else if ( bitsDamageType & ( DMG_BULLET | DMG_NEVERGIB ) )
		action = GS_ARROW;
	else
		return 0;

	Reset();
	m_iAction = action;
	m_hHitter = pevAttacker ? CBaseEntity::Instance( pevAttacker ) : NULL;
	pev->health = pev->max_health;
	return 0;
}

// func_80A4E754: squash one way and the other on a sine, amplitude easing to 0
float CGossipStone::Wobble( float fraction, float step, float minStep, int period )
{
	float left = GS_SmoothStepToF( &m_flA, m_flB, fraction, step, minStep );
	left = GS_SmoothStepToF( &m_flA, m_flB, fraction, step, minStep );
	float s = sinf( ( ( m_iTimer % period ) * ( 1.0f / period ) ) * 2.0f * M_PI ) * m_flA;
	m_flScaleX = 1.0f + s;
	m_flScaleY = 1.0f - s;
	m_iTimer++;
	return left;
}

void CGossipStone::Sword( void )
{
	if ( m_iStep == 0 )
	{
		Sound( ZS_GS_BOING );
		m_iTimer = 0;
		m_iStep = 1;
		m_flA = 0.5f;
		m_flB = 0.0f;
	}
	else if ( Wobble( 0.8f, 0.007f, 0.001f, 7 ) == 0.0f )
	{
		// 0x71B1: "BOINNG! BOINNG! The current time is: ..."
		CLinkPlayer *pLink = ZL_GetLink( m_hHitter );
		if ( pLink )
			pLink->ZStartMessage( "0x71B1|GOSSIP_TIME", this, 0 );
		Reset();
	}
}

void CGossipStone::Arrow( void )
{
	if ( m_iStep == 0 )
	{
		Sound( ZS_GS_BOING );
		m_flA = 0.3f;
		m_flB = 0.0f;
		m_iTimer = 0;
		m_iStep = 1;
	}
	else
	{
		// OoT turns the squash axis round once every 8 frames; a rocking roll stands in
		m_flRoll = sinf( ( ( m_iTimer % 8 ) / 8.0f ) * 2.0f * M_PI ) * m_flA * 40.0f;
		if ( Wobble( 0.8f, 0.005f, 0.001f, 7 ) == 0.0f )
			Reset();
	}
}

void CGossipStone::Hammer( void )
{
	if ( m_iStep == 0 )
	{
		m_flA = m_flScaleY - 1.0f;
		m_flB = -0.8f;
		m_iStep++;
	}
	else if ( m_iStep == 1 )
	{
		float left = GS_SmoothStepToF( &m_flA, m_flB, 1.0f, 0.4f, 0.001f );
		m_flScaleY = m_flA + 1.0f;
		if ( left == 0.0f )
		{
			m_iTimer = 0;
			m_iStep++;
		}
	}
	else if ( m_iStep == 2 )
	{
		if ( ++m_iTimer >= 100 )
		{
			m_iStep++;
			m_flA = m_flScaleY - 1.0f;
			m_flB = 0.0f;
		}
	}
	else if ( m_iStep == 3 )
	{
		float left = GS_SmoothStepToF( &m_flA, m_flB, 1.0f, 0.5f, 0.001f );
		m_flScaleY = m_flA + 1.0f;
		if ( left == 0.0f )
		{
			m_flA = 0.5f;
			m_flB = 0.0f;
			m_iTimer = 0;
			Sound( ZS_GS_BOING );
			m_iStep++;
		}
	}
	else if ( Wobble( 1.0f, 0.03f, 0.001f, 5 ) == 0.0f )
	{
		Reset();
	}
}

void CGossipStone::Bomb( void )
{
	if ( m_iStep == 0 )
	{
		m_iTimer = 40;
		m_iStep++;
	}

	if ( m_iStep == 1 )
	{
		if ( m_iTimer-- <= 0 )
		{
			m_iTimer = 80;
			m_iStep++;

			// The flame on top (gEffFire1DL in EnGs_Draw)
			CSprite *pFire = CSprite::SpriteCreate( "sprites/fire.spr", pev->origin + Vector( 0, 0, ZL_DIST( 52 ) ), TRUE );
			if ( pFire )
			{
				pFire->SetTransparency( kRenderTransAdd, 255, 200, 50, 255, kRenderFxNone );
				pFire->SetScale( 0.6f );
				pFire->SetAttachment( edict(), 0 );
				m_hFire = pFire;
			}
		}
	}

	if ( m_iStep == 2 )
	{
		// Flash blue, then red for the last second, with the countdown beeps
		m_iTimer--;
		pev->renderfx = kRenderFxNone;
		if ( m_iTimer < 80 && ( m_iTimer % 20 ) < 8 )
		{
			pev->renderfx = kRenderFxGlowShell;
			pev->renderamt = 6;
			if ( m_iTimer < 20 )
				pev->rendercolor = Vector( 255, 50, 50 );
			else
				pev->rendercolor = Vector( 50, 50, 255 );
			if ( ( m_iTimer % 20 ) == 7 )
				Sound( m_iTimer < 20 ? ZS_GS_COUNT_LAST : ZS_GS_COUNT, 0.8f );
		}
		if ( m_iTimer <= 0 )
		{
			m_iTimer = 0;
			pev->renderfx = kRenderFxNone;
			m_iStep++;
			Sound( ZS_GS_FIRE );
		}
	}

	if ( m_iStep == 3 )
	{
		// Dust at the base while it builds up
		if ( ( m_iTimer & 3 ) == 0 )
			UTIL_Sparks( pev->origin + Vector( RANDOM_FLOAT( -16, 16 ), RANDOM_FLOAT( -16, 16 ), 4 ) );
		if ( m_iTimer++ >= 40 )
		{
			m_flRiseSpeed = 0;
			m_flStartZ = pev->origin.z;
			pev->solid = SOLID_NOT;
			Sound( ZS_GS_LAUNCH );
			m_iStep++;
		}
	}

	if ( m_iStep == 4 )
	{
		// actor.gravity = 0.3: it accelerates upwards until it hits something
		m_flRiseSpeed += 0.3f;
		float dz = ZL_VEL( m_flRiseSpeed ) * GS_FRAME;

		TraceResult tr;
		UTIL_TraceLine( pev->origin + Vector( 0, 0, ZL_DIST( 48 ) ), pev->origin + Vector( 0, 0, ZL_DIST( 48 ) + dz + 2 ),
						ignore_monsters, edict(), &tr );
		if ( tr.flFraction < 1.0f || tr.fStartSolid )
		{
			// EffectSsBomb2_SpawnLayered + NA_SE_IT_BOMB_EXPLOSION
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
				WRITE_BYTE( TE_EXPLOSION );
				WRITE_COORD( tr.vecEndPos.x );
				WRITE_COORD( tr.vecEndPos.y );
				WRITE_COORD( tr.vecEndPos.z - 16 );
				WRITE_SHORT( g_sModelIndexFireball );
				WRITE_BYTE( 30 );
				WRITE_BYTE( 15 );
				WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
			MESSAGE_END();
			STOP_SOUND( edict(), CHAN_BODY, ZL_SoundPath( ZS_GS_LAUNCH ) );
			ZL_EmitSound( edict(), CHAN_ITEM, ZS_IT_BOMB_EXPLOSION, 1.0f, ATTN_NORM, PITCH_NORM );
			pev->effects |= EF_NODRAW;
			m_iAction = GS_GONE;
			m_iTimer = 10;
			if ( m_hFire != NULL )
				UTIL_Remove( m_hFire );
			return;
		}

		UTIL_SetOrigin( pev, pev->origin + Vector( 0, 0, dz ) );
		if ( pev->origin.z - m_flStartZ > ZL_DIST( 12000 ) )
		{
			if ( m_hFire != NULL )
				UTIL_Remove( m_hFire );
			UTIL_Remove( this );
		}
	}
}

void CGossipStone::Spin( void )
{
	float before = m_flSpin;

	if ( m_iStep == 0 )
	{
		m_flA = 0;			// spin speed, degrees a frame
		m_flB = 90.0f;
		m_flC = 0.1f;
		m_flD = 2.0f;
		m_iStep = 1;
	}
	if ( m_iStep == 1 )
	{
		GS_SmoothStepToF( &m_flC, m_flD, 1.0f, 0.1f, 0.001f );
		float left = GS_SmoothStepToF( &m_flA, m_flB, 1.0f, m_flC, 0.001f );
		m_flSpin += m_flA;
		if ( left == 0.0f )
		{
			m_iTimer = 0;
			m_iStep = 2;
		}
	}
	if ( m_iStep == 2 )
	{
		m_flSpin += m_flA;
		if ( m_iTimer++ > 40 )
		{
			// Stretch up to 2.5 tall, 0.7 wide
			m_flA = m_flScaleY - 1.0f;
			m_flB = 1.5f;
			m_flC = m_flScaleX - 1.0f;
			m_flD = -0.3f;
			Sound( ZS_GS_GROW );
			m_iStep = 3;
		}
	}
	if ( m_iStep == 3 )
	{
		m_flSpin += 90.0f;
		float left = GS_SmoothStepToF( &m_flA, m_flB, 0.8f, 0.2f, 0.001f );
		GS_SmoothStepToF( &m_flC, m_flD, 0.8f, 0.2f, 0.001f );
		m_flScaleX = m_flC + 1.0f;
		m_flScaleY = m_flA + 1.0f;
		if ( left == 0.0f )
		{
			m_flA = 40.0f;		// slowing down
			m_iTimer = 0;
			m_iStep = 4;
		}
	}
	if ( m_iStep == 4 )
	{
		// Wind down and settle back to normal size
		m_flSpin += m_flA;
		m_flA = max( 0.0f, m_flA - 1.0f );
		GS_SmoothStepToF( &m_flScaleX, 1.0f, 0.2f, 0.05f, 0.001f );
		GS_SmoothStepToF( &m_flScaleY, 1.0f, 0.2f, 0.05f, 0.001f );
		if ( m_flA == 0.0f && m_flScaleX == 1.0f && m_flScaleY == 1.0f )
		{
			m_flSpin = 0;
			Reset();
		}
	}

	// NA_SE_EV_STONE_ROLLING each time it comes round
	if ( m_iAction == GS_SPIN && (int)( before / 360.0f ) != (int)( m_flSpin / 360.0f ) )
		Sound( ZS_GS_ROLL, 0.7f );
}

void CGossipStone::StoneThink( void )
{
	pev->nextthink = gpGlobals->time + GS_FRAME;

	switch ( m_iAction )
	{
	case GS_SWORD:	Sword();	break;
	case GS_ARROW:	Arrow();	break;
	case GS_HAMMER:	Hammer();	break;
	case GS_BOMB:	Bomb();		break;
	case GS_SPIN:	Spin();		break;
	case GS_GONE:
		if ( m_iTimer-- <= 0 )
		{
			UTIL_Remove( this );
			return;
		}
		break;
	default:
		break;
	}

	pev->angles = Vector( 0, m_flBaseYaw + fmod( m_flSpin, 360.0f ), m_flRoll );
	// 1.0 / 1.0 is "no scale" for the renderer too, but sending 0 keeps the entity delta small
	if ( m_flScaleX == 1.0f && m_flScaleY == 1.0f )
		pev->fuser1 = pev->fuser2 = 0;
	else
	{
		pev->fuser1 = max( 0.01f, m_flScaleX );
		pev->fuser2 = max( 0.01f, m_flScaleY );
	}
}
