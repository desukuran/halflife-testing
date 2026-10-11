/***
*
*	Link's movement (Zelda 64 port). Shared by the client and server through pm_shared.
*
****/
#ifndef PM_ZELDA_H
#define PM_ZELDA_H

#ifdef __cplusplus
extern "C" {
#endif

// Called by PM_Move around PM_PlayerMove
void PMZ_Begin( void );
void PMZ_End( void );

// Called from PM_PlayerMove
int PMZ_Active( void );
int PMZ_Think( void );		// returns non zero if it did the whole move itself
void PMZ_PostMove( void );
int PMZ_Airborne( void );	// PM_CatagorizePosition: Link is jumping, don't snap him to the floor

// The last predicted Link state, filled in on the client only. The camera and HUD read this
// because it is ahead of what the server has sent back.
typedef struct pmz_predicted_s
{
	int		valid;
	int		state;		// ZL_STATE_* bits (iuser4)
	int		sflags;		// ZSF_* (iuser3)
	float	facing;		// fuser1
	float	time;		// fuser4
	float	origin[3];
	float	velocity[3];
	int		onground;
} pmz_predicted_t;

extern pmz_predicted_t g_pmzPredicted;

#ifdef __cplusplus
}
#endif

#endif // PM_ZELDA_H
