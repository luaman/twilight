/*
	$RCSfile$

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:
	
		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
static const char rcsid[] =
    "$Id$";

#include "twiconfig.h"

/* common */
#include "quakedef.h"
#include "client.h"
#include "cmd.h"
#include "cvar.h"
#include "dlight.h"
#include "mathlib.h"
#include "renderer/screen.h"
#include "strlib.h"
#include "host.h"
#include "renderer/alias.h"

/* QW specific */
#include "pmove.h"

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

static cvar_t *cl_rollspeed;
static cvar_t *cl_rollangle;

static cvar_t *cl_bob;
static cvar_t *cl_bobcycle;
static cvar_t *cl_bobup;

static cvar_t *v_kicktime;
static cvar_t *v_kickroll;
static cvar_t *v_kickpitch;

static cvar_t *v_iyaw_cycle;
static cvar_t *v_iroll_cycle;
static cvar_t *v_ipitch_cycle;
static cvar_t *v_iyaw_level;
static cvar_t *v_iroll_level;
static cvar_t *v_ipitch_level;

static cvar_t *v_idlescale;
static cvar_t *v_zoom;

cvar_t *crosshair;
cvar_t *crosshaircolor;

static cvar_t *cl_crossx;
static cvar_t *cl_crossy;

cvar_t *gl_cshiftpercent;

static cvar_t *v_contentblend;

static cvar_t *v_centermove;
static cvar_t *v_centerspeed;

static float v_dmg_time, v_dmg_roll, v_dmg_pitch;

static frame_t *view_frame;
static player_state_t *view_message;


float
V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	vec3_t      forward, right, up;
	float       sign;
	float       side;
	float       value;

	AngleVectors (angles, forward, right, up);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);

	value = cl_rollangle->fvalue;

	if (side < cl_rollspeed->fvalue)
		side = side * value / cl_rollspeed->fvalue;
	else
		side = value;

	return side * sign;
}



static float
V_CalcBob (void)
{
	static double	bobtime;
	static float	bob;
	float			cycle;
	float			*vel;

	if (cl.spectator)
		return 0;

	if (!pmove.groundent)
		return bob;		/* just use old value */

	if (!cl_bobcycle->fvalue)
		return bob = 0;

	bobtime += ccl.frametime;
	cycle = bobtime - (int) (bobtime / cl_bobcycle->fvalue)
		* cl_bobcycle->fvalue;
	cycle /= cl_bobcycle->fvalue;
	if (cycle < cl_bobup->fvalue)
		cycle = M_PI * cycle / cl_bobup->fvalue;
	else
		cycle = M_PI + M_PI * (cycle - cl_bobup->fvalue)
			/ (1.0 - cl_bobup->fvalue);

	/* bob is proportional to simulated velocity in the xy plane
       (don't count Z, or jumping messes it up) */
	vel = cl.frames[(cls.netchan.incoming_sequence)&UPDATE_MASK].playerstate[ccl.player_num].velocity;

	bob = VectorLength2(vel) * cl_bob->fvalue;
	bob = bob * (0.3 + 0.7 * Q_sin (cycle));
	bob = bound(-7, bob, 4);

	return bob;
}


/* ============================================================================= */

void
V_StartPitchDrift (void)
{
	if (ccl.laststop == ccl.time)
		return;		/* something else is keeping it from drifting */

	if (ccl.nodrift || !ccl.pitchvel) {
		ccl.pitchvel = v_centerspeed->fvalue;
		ccl.nodrift = false;
		ccl.driftmove = 0;
	}
}

void
V_StopPitchDrift (void)
{
	ccl.laststop = ccl.time;
	ccl.nodrift = true;
	ccl.pitchvel = 0;
}

/*
===============
If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0
===============
*/
static void
V_DriftPitch (void)
{
	float	delta, move;

	if (!view_message->groundent || ccls.demoplayback) {
		ccl.driftmove = 0;
		ccl.pitchvel = 0;
		return;
	}

	/* don't count small mouse motion */
	if (ccl.nodrift) {
		if (fabs(cl.frames[(cls.netchan.outgoing_sequence - 1) & UPDATE_MASK].cmd.forwardmove) < 200)
			ccl.driftmove = 0;
		else
			ccl.driftmove += ccl.frametime;

		if (ccl.driftmove > v_centermove->fvalue)
			V_StartPitchDrift();

		return;
	}

	delta = 0 - cl.viewangles[PITCH];

	if (!delta) {
		ccl.pitchvel = 0;
		return;
	}

	move = ccl.frametime * ccl.pitchvel;
	ccl.pitchvel += ccl.frametime * v_centerspeed->fvalue;

	if (delta > 0) {
		if (move > delta) {
			ccl.pitchvel = 0;
			move = delta;
		}
		cl.viewangles[PITCH] += move;
	} else if (delta < 0) {
		if (move > -delta) {
			ccl.pitchvel = 0;
			move = -delta;
		}
		cl.viewangles[PITCH] -= move;
	}
}

/*
============================================================================== 
 
						PALETTE FLASHES 
 
============================================================================== 
*/
static cshift_t    cshift_empty = { {130, 80, 50}, 0 };
static cshift_t    cshift_water = { {130, 80, 50}, 128 };
static cshift_t    cshift_slime = { {0, 25, 5}, 150 };
static cshift_t    cshift_lava = { {255, 80, 0}, 150 };

float       v_blend[4];					// rgba 0.0 - 1.0


void
V_ParseDamage (void)
{
	int         armor, blood;
	vec3_t      from;
	int         i;
	vec3_t      forward, right, up;

	float       side;
	float       count;

	armor = MSG_ReadByte();
	blood = MSG_ReadByte();

	for (i = 0; i < 3; i++)
		from[i] = MSG_ReadCoord();

	count = (blood + armor) * 0.5;
	if (count < 10)
		count = 10;

	ccl.faceanimtime = ccl.time + 0.2;	/* butt sbar face into pain frame */

	ccl.cshifts[CSHIFT_DAMAGE].percent += 3 * count;
	if (ccl.cshifts[CSHIFT_DAMAGE].percent < 0)
		ccl.cshifts[CSHIFT_DAMAGE].percent = 0;
	if (ccl.cshifts[CSHIFT_DAMAGE].percent > 150)
		ccl.cshifts[CSHIFT_DAMAGE].percent = 150;

	if (armor > blood) {
		ccl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		ccl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		ccl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	} else if (armor) {
		ccl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		ccl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		ccl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	} else {
		ccl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		ccl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		ccl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

	/* calculate view angle kicks */
	VectorSubtract(from, ccl.player_origin, from);
	VectorNormalizeFast(from);

	AngleVectors(ccl.player_angles, forward, right, up);

	side = DotProduct(from, right);
	v_dmg_roll = count * side * v_kickroll->fvalue;

	side = DotProduct(from, forward);
	v_dmg_pitch = count * side * v_kickpitch->fvalue;

	v_dmg_time = v_kicktime->fvalue;
}


static void
V_cshift_f (void)
{
	cshift_empty.destcolor[0] = Q_atoi(Cmd_Argv (1));
	cshift_empty.destcolor[1] = Q_atoi(Cmd_Argv (2));
	cshift_empty.destcolor[2] = Q_atoi(Cmd_Argv (3));
	cshift_empty.percent = Q_atoi(Cmd_Argv (4));
}

/*
==================
When you run over an item, the server sends this command
==================
*/
static void
V_BonusFlash_f (void)
{
	ccl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	ccl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	ccl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	ccl.cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
Underwater, lava, etc each has a color shift
=============
*/
void
V_SetContentsColor (int contents)
{
	if (!v_contentblend->ivalue) {
		ccl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		return;
	}

	switch (contents) {
		case CONTENTS_EMPTY:
			ccl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
			break;
		case CONTENTS_LAVA:
			ccl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
			break;
		case CONTENTS_SOLID:
		case CONTENTS_SLIME:
			ccl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
			break;
		default:
			ccl.cshifts[CSHIFT_CONTENTS] = cshift_water;
	}
}


static void
V_CalcPowerupCshift (void)
{
	if (ccl.stats[STAT_ITEMS] & IT_QUAD) {
		ccl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		ccl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		ccl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		ccl.cshifts[CSHIFT_POWERUP].percent = 30;
	} else if (ccl.stats[STAT_ITEMS] & IT_SUIT) {
		ccl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		ccl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		ccl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		ccl.cshifts[CSHIFT_POWERUP].percent = 20;
	} else if (ccl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		ccl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		ccl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		ccl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		ccl.cshifts[CSHIFT_POWERUP].percent = 100;
	} else if (ccl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		ccl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		ccl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		ccl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		ccl.cshifts[CSHIFT_POWERUP].percent = 30;
	} else
		ccl.cshifts[CSHIFT_POWERUP].percent = 0;
}


void
V_CalcBlend (void)
{
	float	r = 0, g = 0, b = 0, a = 0, a2;
	int		j;

	if (gl_cshiftpercent->fvalue) {
		for (j = 0; j < NUM_CSHIFTS; j++)	 {
			a2 = ccl.cshifts[j].percent * gl_cshiftpercent->fvalue * (1.0f / 25500.0f);

			if (!a2)
				continue;
			if (a2 > 1)
				a2 = 1;
			r += (ccl.cshifts[j].destcolor[0]-r) * a2;
			g += (ccl.cshifts[j].destcolor[1]-g) * a2;
			b += (ccl.cshifts[j].destcolor[2]-b) * a2;
			a = 1 - (1 - a) * (1 - a2);
		}

		/* saturate color (to avoid blending in black) */
		if (a) {
			a2 = 1 / a;
			r *= a2;
			g *= a2;
			b *= a2;
		}
	}

	v_blend[0] = bound(0, r * (1.0/255.0), 1);
	v_blend[1] = bound(0, g * (1.0/255.0), 1);
	v_blend[2] = bound(0, b * (1.0/255.0), 1);
	v_blend[3] = bound(0, a				 , 1);
}


void
V_UpdatePalette (void)
{
	int			i, j;

	V_CalcPowerupCshift();

	for (i = 0; i < NUM_CSHIFTS; i++) {
		if (ccl.cshifts[i].percent != ccl.prev_cshifts[i].percent) {
			ccl.prev_cshifts[i].percent = ccl.cshifts[i].percent;
		}
		for (j = 0; j < 3; j++)
			if (ccl.cshifts[i].destcolor[j] != ccl.prev_cshifts[i].destcolor[j]) {
				ccl.prev_cshifts[i].destcolor[j] = ccl.cshifts[i].destcolor[j];
			}
	}

	/* drop the damage value */
	ccl.cshifts[CSHIFT_DAMAGE].percent -= ccl.frametime * 150;
	if (ccl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		ccl.cshifts[CSHIFT_DAMAGE].percent = 0;

	/* drop the bonus value */
	ccl.cshifts[CSHIFT_BONUS].percent -= ccl.frametime * 100;
	if (ccl.cshifts[CSHIFT_BONUS].percent <= 0)
		ccl.cshifts[CSHIFT_BONUS].percent = 0;

	V_CalcBlend();
}

/* 
============================================================================== 
 
						VIEW RENDERING 
 
============================================================================== 
*/

/*
==============
Idle swaying
==============
*/
static void
V_AddIdle (void)
{
	r.angles[ROLL] +=
		v_idlescale->fvalue * Q_sin (ccl.time * v_iroll_cycle->fvalue) *
		v_iroll_level->fvalue;
	r.angles[PITCH] +=
		v_idlescale->fvalue * Q_sin (ccl.time * v_ipitch_cycle->fvalue) *
		v_ipitch_level->fvalue;
	r.angles[YAW] +=
		v_idlescale->fvalue * Q_sin (ccl.time * v_iyaw_cycle->fvalue) *
		v_iyaw_level->fvalue;

	cl.viewent_angles[ROLL] -=
		v_idlescale->fvalue * Q_sin (ccl.time * v_iroll_cycle->fvalue) *
		v_iroll_level->fvalue;
	cl.viewent_angles[PITCH] -=
		v_idlescale->fvalue * Q_sin (ccl.time * v_ipitch_cycle->fvalue) *
		v_ipitch_level->fvalue;
	cl.viewent_angles[YAW] -=
		v_idlescale->fvalue * Q_sin (ccl.time * v_iyaw_cycle->fvalue) *
		v_iyaw_level->fvalue;
}

/*
==============
Roll is induced by movement and damage
==============
*/
static void
V_CalcViewRoll (void)
{
	float       side;

	side = V_CalcRoll (ccl.player_angles, ccl.player_velocity);
	r.angles[ROLL] += side;

	if (v_dmg_time > 0) {
		r.angles[ROLL] += v_dmg_time / v_kicktime->fvalue * v_dmg_roll;
		r.angles[PITCH] += v_dmg_time / v_kicktime->fvalue * v_dmg_pitch;
		v_dmg_time -= ccl.frametime;
	}
}


static void
V_CalcIntermissionRefdef (void)
{
	/* view is the weapon model (only visible from inside body) */
	memset (&cl.viewent, 0, sizeof (entity_t));
	cl.viewent.times = -1;		// FIXME: HACK! DO NOT COPY ELSEWHERE!

	VectorCopy(ccl.player_origin, r.origin);
	VectorCopy(ccl.player_angles, r.angles);

	/* always idle in intermission */
	r.angles[ROLL] += Q_sin (ccl.time * v_iroll_cycle->fvalue) *
		v_iroll_level->fvalue;
	r.angles[PITCH] += Q_sin (ccl.time * v_ipitch_cycle->fvalue) *
		v_ipitch_level->fvalue;
	r.angles[YAW] += Q_sin (ccl.time * v_iyaw_cycle->fvalue) *
		v_iyaw_level->fvalue;
}


static void
V_CalcRefdef (void)
{
	int				i;
	vec3_t			forward, right, up;

	float			bob;
	static float	oldz = 0;

	/* Make sure we have an uptodate colormap. */
	ccl.colormap = &ccl.users[ccl.player_num].color_map;

	V_DriftPitch();

	bob = V_CalcBob();

// refresh position from simulated origin
	VectorCopy (ccl.player_origin, r.origin);

	r.origin[2] += bob;

	/* never let it sit exactly on a node line, because a water plane can
	   dissapear when viewed with the eye exactly on it.
	   the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis */
	r.origin[0] += 1.0 / 16;
	r.origin[1] += 1.0 / 16;
	r.origin[2] += 1.0 / 16;

	VectorCopy(ccl.player_angles, r.angles);
	V_CalcViewRoll();
	V_AddIdle();

	if (view_message->flags & PF_GIB)
		r.origin[2] += 8;		/* gib view height */
	else if (view_message->flags & PF_DEAD)
		r.origin[2] -= 16;		/* corpse view height */
	else
		r.origin[2] += 22;		/* view height */

	if (view_message->flags & PF_DEAD)	/* PF_GIB will also set PF_DEAD */
		r.angles[ROLL] = 80;	/*   dead view angle */

	if (v_zoom->fvalue != ccl.viewzoom)
		ccl.viewzoom = SLIDE (ccl.viewzoom, v_zoom->fvalue, 4 * host.frametime);

	/* offsets */
	AngleVectors (ccl.player_angles, forward, right, up);

	/* set up gun position */
	VectorCopy (ccl.player_angles, cl.viewent_angles);

	/* set up gun angles */
	cl.viewent_angles[YAW] = r.angles[YAW];
	cl.viewent_angles[PITCH] = -r.angles[PITCH];

	VectorCopy (ccl.player_origin, cl.viewent_origin);
	cl.viewent_origin[2] += 22;

	for (i = 0; i < 3; i++) {
		cl.viewent_origin[i] += forward[i] * bob * 0.4;
	}

	cl.viewent_origin[2] += bob;

	/* fudge position around to keep amount of weapon visible
	   roughly equal with different FOV */
	cl.viewent_origin[2] += 2;

	if (view_message->flags & (PF_GIB | PF_DEAD)) {
		memset (&cl.viewent, 0, sizeof (entity_t));
		cl.viewent.times = -1;		// FIXME: HACK! DO NOT COPY ELSEWHERE!
	} else {
		model_t *model = ccl.model_precache[ccl.stats[STAT_WEAPON]];

		if (cl.viewent.common.model != model) {
			memset (&cl.viewent, 0, sizeof (entity_t));
			cl.viewent.common.model = model;
			cl.viewent.times = -1;		// FIXME: HACK! DO NOT COPY ELSEWHERE!
		}
	}

	cl.viewent_frame = view_message->weaponframe;

	/* set up the refresh position */
	r.angles[PITCH] += cl.punchangle;

	/* smooth out stair step ups */
	if (view_message->groundent && (ccl.player_origin[2] - oldz > 0)) {
		float	steptime;

		steptime = ccl.frametime;

		oldz += steptime * 80;
		if (oldz > ccl.player_origin[2])
			oldz = ccl.player_origin[2];
		if (ccl.player_origin[2] - oldz > 12)
			oldz = ccl.player_origin[2] - 12;
		r.origin[2] += oldz - ccl.player_origin[2];
		cl.viewent_origin[2] += oldz - ccl.player_origin[2];
	} else
		oldz = ccl.player_origin[2];
}


static void
DropPunchAngle (void)
{
	cl.punchangle -= 10 * ccl.frametime;
	if (cl.punchangle < 0)
		cl.punchangle = 0;
}

/*
==================
The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
void
V_RenderView (void)
{
	ccl.player_angles[ROLL] = 0;				// FIXME @@@ 

	if (ccls.state != ca_active)
		return;

	view_frame = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];
	view_message = &view_frame->playerstate[ccl.player_num];

	DropPunchAngle ();
	if (ccl.intermission)
		/* intermission / finale rendering */
		V_CalcIntermissionRefdef ();
	else
		V_CalcRefdef ();

	R_BuildLightList ();
	R_RenderView ();
}

/* ============================================================================ */

static void
V_Zoom_CB (cvar_t *var)
{
	if (var->fvalue > 1.5f)
		Cvar_Set (var, "1.5");
	else if (var->fvalue < 0.05f)
		Cvar_Set (var, "0.05");
}



void
V_Init_Cvars (void)
{
	cl_rollspeed = Cvar_Get ("cl_rollspeed", "200", CVAR_NONE, NULL);
	cl_rollangle = Cvar_Get ("cl_rollangle", "2.0", CVAR_NONE, NULL);

	cl_bob = Cvar_Get ("cl_bob", "0.02", CVAR_NONE, NULL);
	cl_bobcycle = Cvar_Get ("cl_bobcycle", "0.6", CVAR_NONE, NULL);
	cl_bobup = Cvar_Get ("cl_bobup", "0.5", CVAR_NONE, NULL);

	v_kicktime = Cvar_Get ("v_kicktime", "0.5", CVAR_NONE, NULL);
	v_kickroll = Cvar_Get ("v_kickroll", "0.6", CVAR_NONE, NULL);
	v_kickpitch = Cvar_Get ("v_kickpitch", "0.6", CVAR_NONE, NULL);

	v_iyaw_cycle = Cvar_Get ("v_iyaw_cycle", "2", CVAR_NONE, NULL);
	v_iroll_cycle = Cvar_Get ("v_iroll_cycle", "0.5", CVAR_NONE, NULL);
	v_ipitch_cycle = Cvar_Get ("v_ipitch_cycle", "1", CVAR_NONE, NULL);
	v_iyaw_level = Cvar_Get ("v_iyaw_level", "0.3", CVAR_NONE, NULL);
	v_iroll_level = Cvar_Get ("v_iroll_level", "0.1", CVAR_NONE, NULL);
	v_ipitch_level = Cvar_Get ("v_ipitch_level", "0.3", CVAR_NONE, NULL);

	v_idlescale = Cvar_Get ("v_idlescale", "0", CVAR_NONE, NULL);

	v_zoom = Cvar_Get ("v_zoom", "1", CVAR_NONE, V_Zoom_CB);

	crosshair = Cvar_Get ("crosshair", "0", CVAR_ARCHIVE, NULL);
	crosshaircolor = Cvar_Get ("crosshaircolor", "79", CVAR_ARCHIVE, NULL);

	cl_crossx = Cvar_Get ("cl_crossx", "0", CVAR_ARCHIVE, NULL);
	cl_crossy = Cvar_Get ("cl_crossy", "0", CVAR_ARCHIVE, NULL);

	gl_cshiftpercent = Cvar_Get ("gl_cshiftpercent", "100", CVAR_NONE, NULL);

	v_contentblend = Cvar_Get ("v_contentblend", "1", CVAR_NONE, NULL);

	v_centermove = Cvar_Get ("v_centermove", "0.15", CVAR_NONE, NULL);
	v_centerspeed = Cvar_Get ("v_centerspeed", "500", CVAR_NONE, NULL);
}


void
V_Init (void)
{
	Cmd_AddCommand ("v_cshift", V_cshift_f);
	Cmd_AddCommand ("bf", V_BonusFlash_f);
	Cmd_AddCommand ("centerview", V_StartPitchDrift);
}


void
R_DrawViewModel (void)
{
	entity_common_t *ent_pointer;

	ent_pointer = &cl.viewent.common;

	cl.viewent.times++;

	if (!r_drawviewmodel->ivalue ||
		!Cam_DrawViewModel () ||
		!r_drawentities->ivalue ||
		(ccl.stats[STAT_ITEMS] & IT_INVISIBILITY) ||
		(ccl.stats[STAT_HEALTH] <= 0) ||
		!cl.viewent.common.model) {
		return;
	}

	CL_Update_OriginAngles(&cl.viewent, cl.viewent_origin, cl.viewent_angles, ccl.time);
	CL_Update_Frame(&cl.viewent, cl.viewent_frame, ccl.time);

	// hack the depth range to prevent view model from poking into walls
	qglDepthRange (0.0f, 0.3f);
	R_DrawOpaqueAliasModels(&ent_pointer, 1, true);
	qglDepthRange (0.0f, 1.0f);
}
