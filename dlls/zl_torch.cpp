/***
*
*	Zelda 64 port: the standing torch (Obj_Syokudai).
*
*	zl_torch
*		type		0 = golden (gGoldenTorchDL), 1 = timed (gTimedTorchDL), 2 = wooden (gWoodenTorchDL)
*		spawnflags	1 = starts out
*		Triggering it lights it (or puts it out when it's lit).
*
*	The stand is models/zelda/torch_*.mdl (tools/zelda/make_object_models.py). The flame, its
*	flickering light and the glow are drawn by the client (cl_dll/zl_effects.cpp), which finds lit
*	torches by their model; renderamt carries the flame's size (0 out, 255 full). While lit it
*	crackles (NA_SE_EV_TORCH); lighting it goes NA_SE_EV_FLAME_IGNITION.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "zl_player.h"

#define SF_TORCH_UNLIT		1

static const char *sTorchModels[3] =
{
	"models/zelda/torch_golden.mdl",
	"models/zelda/torch_timed.mdl",
	"models/zelda/torch_wooden.mdl",
};

// The ripped NA_SE_EV_TORCH is cut at a few seconds: start it over before it runs out. The
// Half-Life stand-in loops by itself.
#define ZL_TORCH_SOUND_PERIOD	3.3f

class CZeldaTorch : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT TorchThink( void );
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iType;
	BOOL m_fLit;
	float m_flLitTime;		// when it was lit (the flame grows over 10 frames)
	float m_flNextSound;
};

LINK_ENTITY_TO_CLASS( zl_torch, CZeldaTorch );

TYPEDESCRIPTION CZeldaTorch::m_SaveData[] =
{
	DEFINE_FIELD( CZeldaTorch, m_iType, FIELD_INTEGER ),
	DEFINE_FIELD( CZeldaTorch, m_fLit, FIELD_BOOLEAN ),
	DEFINE_FIELD( CZeldaTorch, m_flLitTime, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CZeldaTorch, CBaseEntity );

void CZeldaTorch::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "type" ) )
	{
		m_iType = atoi( pkvd->szValue );
		if ( m_iType < 0 || m_iType > 2 )
			m_iType = 0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CZeldaTorch::Precache( void )
{
	if ( ZL_FileExists( sTorchModels[m_iType] ) )
		PRECACHE_MODEL( (char *)sTorchModels[m_iType] );
}

void CZeldaTorch::Spawn( void )
{
	Precache();

	if ( ZL_FileExists( sTorchModels[m_iType] ) )
		SET_MODEL( ENT( pev ), sTorchModels[m_iType] );

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;
	// The stand: about 10 across and 60 tall
	UTIL_SetSize( pev, Vector( -ZL_DIST( 10 ), -ZL_DIST( 10 ), 0 ), Vector( ZL_DIST( 10 ), ZL_DIST( 10 ), ZL_DIST( 60 ) ) );
	UTIL_SetOrigin( pev, pev->origin );
	DROP_TO_FLOOR( ENT( pev ) );

	m_fLit = !( pev->spawnflags & SF_TORCH_UNLIT );
	m_flLitTime = gpGlobals->time - 1.0f;
	pev->renderamt = m_fLit ? 255 : 0;
	m_flNextSound = gpGlobals->time + RANDOM_FLOAT( 0.1f, 1.0f );

	SetThink( &CZeldaTorch::TorchThink );
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CZeldaTorch::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	BOOL light = ( useType == USE_ON ) ? TRUE : ( useType == USE_OFF ) ? FALSE : !m_fLit;
	if ( light == m_fLit )
		return;

	m_fLit = light;
	if ( m_fLit )
	{
		m_flLitTime = gpGlobals->time;
		m_flNextSound = gpGlobals->time;
		ZL_EmitSound( edict(), CHAN_ITEM, ZS_EV_FLAME_IGNITION, 1.0f, ATTN_NORM, PITCH_NORM );
	}
	else
	{
		pev->renderamt = 0;
		STOP_SOUND( edict(), CHAN_BODY, ZL_SoundPath( ZS_EV_TORCH ) );
	}
}

void CZeldaTorch::TorchThink( void )
{
	pev->nextthink = gpGlobals->time + ZL_FRAMES( 1.0f );

	if ( !m_fLit )
		return;

	// ObjSyokudai_Draw: lit, the flame grows from 1/10 over 10 frames
	float frames = ( gpGlobals->time - m_flLitTime ) * ZL_FPS;
	float scale = frames >= 9.0f ? 1.0f : ( frames + 1.0f ) / 10.0f;
	pev->renderamt = (int)( scale * 255.0f );

	if ( gpGlobals->time >= m_flNextSound )
	{
		ZL_EmitSound( edict(), CHAN_BODY, ZS_EV_TORCH, 0.8f, ATTN_STATIC, PITCH_NORM );
		m_flNextSound = gpGlobals->time + ZL_TORCH_SOUND_PERIOD;
	}
}
