/*
	$RCSfile$ -- player eye positioning

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
#include "light.h"
#include "mathlib.h"
#include "screen.h"
#include "strlib.h"
#include "sys.h"
#include "gl_alias.h"

/* NQ specific */
#include "host.h"
#include "console.h"
#include "cl_console.h"

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

cvar_t *scr_ofsx;
cvar_t *scr_ofsy;
cvar_t *scr_ofsz;

cvar_t *cl_rollspeed;
cvar_t *cl_rollangle;

cvar_t *cl_bob;
cvar_t *cl_bobcycle;
cvar_t *cl_bobup;

cvar_t *v_kicktime;
cvar_t *v_kickroll;
cvar_t *v_kickpitch;

cvar_t *v_iyaw_cycle;
cvar_t *v_iroll_cycle;
cvar_t *v_ipitch_cycle;
cvar_t *v_iyaw_level;
cvar_t *v_iroll_level;
cvar_t *v_ipitch_level;

cvar_t *v_idlescale;
cvar_t *v_zoom;

cvar_t *crosshair;
cvar_t *crosshaircolor;

cvar_t *cl_crossx;
cvar_t *cl_crossy;

cvar_t *gl_cshiftpercent;

cvar_t *v_contentblend;

cvar_t *v_centermove;
cvar_t *v_centerspeed;

float v_dmg_time, v_dmg_roll, v_dmg_pitch;

extern int in_forward, in_forward2, in_back;

/*
===============
V_CalcRoll

===============
*/
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


/*
===============
V_CalcBob

===============
*/
float
V_CalcBob (void)
{
	float       bob;
	float       cycle;

	if (!cl_bobcycle->fvalue)
		return 0;

	cycle = cl.time - (int)(cl.time / cl_bobcycle->fvalue)
		* cl_bobcycle->fvalue;
	cycle /= cl_bobcycle->fvalue;
	if (cycle < cl_bobup->fvalue)
		cycle = M_PI * cycle / cl_bobup->fvalue;
	else
		cycle = M_PI + M_PI * (cycle - cl_bobup->fvalue)
			/ (1.0 - cl_bobup->fvalue);

	/* bob is proportional to velocity in the xy plane
	   (don't count Z, or jumping messes it up) */

	bob = Q_sqrt (cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1]) * cl_bob->fvalue;
	bob = bob * 0.3 + bob * 0.7 * Q_sin (cycle);
	bob = bound (-7, bob, 4);

	return bob;
}

// ==========================================================================

void
V_StartPitchDrift (void)
{
	if (cl.laststop == cl.time)
		return;		/* something else is keeping it from drifting */

	if (cl.nodrift || !cl.pitchvel) {
		cl.pitchvel = v_centerspeed->fvalue;
		cl.nodrift = false;
		cl.driftmove = 0;
	}
}

void
V_StopPitchDrift (void)
{
	cl.laststop = cl.time;
	cl.nodrift = true;
	cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0
===============
*/
void
V_DriftPitch (void)
{
	float	delta, move;

	if (noclip_anglehack || !cl.onground || cls.demoplayback) {
		cl.driftmove = 0;
		cl.pitchvel = 0;
		return;
	}

	/* don't count small mouse motion */
	if (cl.nodrift) {
		if (fabs(cl.cmd.forwardmove) < cl_forwardspeed->fvalue)
			cl.driftmove = 0;
		else
			cl.driftmove += (cl.time - cl.oldtime);

		if (cl.driftmove > v_centermove->fvalue)
			V_StartPitchDrift();

		return;
	}

	delta = cl.idealpitch - cl.viewangles[PITCH];

	if (!delta) {
		cl.pitchvel = 0;
		return;
	}

	move = (cl.time - cl.oldtime) * cl.pitchvel;
	cl.pitchvel += (cl.time - cl.oldtime) * v_centerspeed->fvalue;

	if (delta > 0) {
		if (move > delta) {
			cl.pitchvel = 0;
			move = delta;
		}
		cl.viewangles[PITCH] += move;
	} else if (delta < 0) {
		if (move > -delta) {
			cl.pitchvel = 0;
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
cshift_t    cshift_empty = { {130, 80, 50}, 0 };
cshift_t    cshift_water = { {130, 80, 50}, 128 };
cshift_t    cshift_slime = { {0, 25, 5}, 150 };
cshift_t    cshift_lava = { {255, 80, 0}, 150 };

float       v_blend[4];					// rgba 0.0 - 1.0

/*
===============
V_ParseDamage
===============
*/
void
V_ParseDamage (void)
{
	int         armor, blood;
	vec3_t      from;
	int         i;
	vec3_t      forward, right, up;
	entity_t	*ent;
	float       side;
	float       count;

	armor = MSG_ReadByte();
	blood = MSG_ReadByte();

	for (i = 0; i < 3; i++)
		from[i] = MSG_ReadCoord();

	count = (blood + armor) * 0.5;
	if (count < 10)
		count = 10;

	cl.faceanimtime = cl.time + 0.2;	/* butt sbar face into pain frame */

	cl.cshifts[CSHIFT_DAMAGE].percent += 3 * count;
	if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
		cl.cshifts[CSHIFT_DAMAGE].percent = 150;

	if (armor > blood) {
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	} else if (armor) {
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	} else {
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

	/* calculate view angle kicks */
	ent = &cl_entities[cl.viewentity];

	VectorSubtract(from, ent->common.origin, from);
	VectorNormalizeFast(from);

	AngleVectors(ent->common.angles, forward, right, up);

	side = DotProduct(from, right);
	v_dmg_roll = count * side * v_kickroll->fvalue;

	side = DotProduct(from, forward);
	v_dmg_pitch = count * side * v_kickpitch->fvalue;

	v_dmg_time = v_kicktime->fvalue;
}

/*
==================
V_cshift_f
==================
*/
void
V_cshift_f (void)
{
	cshift_empty.destcolor[0] = Q_atoi(Cmd_Argv (1));
	cshift_empty.destcolor[1] = Q_atoi(Cmd_Argv (2));
	cshift_empty.destcolor[2] = Q_atoi(Cmd_Argv (3));
	cshift_empty.percent = Q_atoi(Cmd_Argv (4));
}

/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void
V_BonusFlash_f (void)
{
	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	cl.cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void
V_SetContentsColor (int contents)
{
	if (!v_contentblend->ivalue) {
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		return;
	}

	switch (contents) {
		case CONTENTS_EMPTY:
			cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
			break;
		case CONTENTS_LAVA:
			cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
			break;
		case CONTENTS_SOLID:
		case CONTENTS_SLIME:
			cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
			break;
		default:
			cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
	}
}

/*
=============
V_CalcPowerupCshift
=============
*/
void
V_CalcPowerupCshift (void)
{
	if (cl.items & IT_QUAD) {
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	} else if (cl.items & IT_SUIT) {
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 20;
	} else if (cl.items & IT_INVISIBILITY) {
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cl.cshifts[CSHIFT_POWERUP].percent = 100;
	} else if (cl.items & IT_INVULNERABILITY) {
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	} else
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
}

/*
=============
V_CalcBlend
=============
*/
void
V_CalcBlend (void)
{
	float	r = 0, g = 0, b = 0, a = 0, a2;
	int		j;

	if (gl_cshiftpercent->ivalue) {
		for (j = 0; j < NUM_CSHIFTS; j++) {
			a2 = cl.cshifts[j].percent * gl_cshiftpercent->ivalue * (1.0f / 25500.0f);

			if (!a2)
				continue;
			if (a2 > 1)
				a2 = 1;
			r += (cl.cshifts[j].destcolor[0]-r) * a2;
			g += (cl.cshifts[j].destcolor[1]-g) * a2;
			b += (cl.cshifts[j].destcolor[2]-b) * a2;
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

/*
=============
V_UpdatePalette
=============
*/
void
V_UpdatePalette (void)
{
	int			i, j;

	V_CalcPowerupCshift();

	for (i = 0; i < NUM_CSHIFTS; i++) {
		if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent) {
			cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j = 0; j < 3; j++)
			if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j]) {
				cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
	}

	/* drop the damage value */
	cl.cshifts[CSHIFT_DAMAGE].percent -= (cl.time - cl.oldtime) * 150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

	/* drop the bonus value */
	cl.cshifts[CSHIFT_BONUS].percent -= (cl.time - cl.oldtime) * 100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	V_CalcBlend();
}

/*
==============================================================================

						VIEW RENDERING

==============================================================================
*/

/*
==============
V_BoundOffsets
==============
*/
void
V_BoundOffsets (void)
{
	vec3_t org;
	
	VectorCopy (cl_entities[cl.viewentity].common.origin, org);

	/* absolutely bound refresh relative to entity clipping hull
	   so the view can never be inside a solid wall */
	r_refdef.vieworg[0] = bound(org[0] - 14, r_refdef.vieworg[0], org[0] + 14);
	r_refdef.vieworg[1] = bound(org[1] - 14, r_refdef.vieworg[1], org[1] + 14);
	r_refdef.vieworg[2] = bound(org[2] - 22, r_refdef.vieworg[2], org[2] + 30);
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void
V_AddIdle (void)
{
	r_refdef.viewangles[ROLL] +=
		v_idlescale->fvalue * Q_sin (cl.time * v_iroll_cycle->fvalue) *
		v_iroll_level->fvalue;
	r_refdef.viewangles[PITCH] +=
		v_idlescale->fvalue * Q_sin (cl.time * v_ipitch_cycle->fvalue) *
		v_ipitch_level->fvalue;
	r_refdef.viewangles[YAW] +=
		v_idlescale->fvalue * Q_sin (cl.time * v_iyaw_cycle->fvalue) *
		v_iyaw_level->fvalue;

	/*
	cl.viewent.angles[ROLL] -=
		v_idlescale->fvalue * Q_sin (cl.time * v_iroll_cycle->fvalue) *
		v_iroll_level->fvalue;
	cl.viewent.angles[PITCH] -=
		v_idlescale->fvalue * Q_sin (cl.time * v_ipitch_cycle->fvalue) *
		v_ipitch_level->fvalue;
	cl.viewent.angles[YAW] -=
		v_idlescale->fvalue * Q_sin (cl.time * v_iyaw_cycle->fvalue) *
		v_iyaw_level->fvalue;
		*/
}

/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void
V_CalcViewRoll (void)
{
	float       side;

	side = V_CalcRoll (cl_entities[cl.viewentity].common.angles, cl.velocity);
	r_refdef.viewangles[ROLL] += side;

	if (v_dmg_time > 0) {
		r_refdef.viewangles[ROLL] += v_dmg_time / v_kicktime->fvalue * v_dmg_roll;
		r_refdef.viewangles[PITCH] += v_dmg_time / v_kicktime->fvalue * v_dmg_pitch;
		v_dmg_time -= (cl.time - cl.oldtime);
	}

	if (cl.stats[STAT_HEALTH] <= 0)
		r_refdef.viewangles[ROLL] = 80; // dead view angle
}

/*
==================
V_CalcIntermissionRefdef

==================
*/
void
V_CalcIntermissionRefdef (void)
{
	/* ent is the player model (visible when out of body) */
	entity_t   *ent = &cl_entities[cl.viewentity];

	VectorCopy(ent->common.origin, r_refdef.vieworg);
	VectorCopy(ent->common.angles, r_refdef.viewangles);
	cl.viewent.common.model = NULL;

	/* always idle in intermission */
	r_refdef.viewangles[ROLL] += Q_sin (cl.time * v_iroll_cycle->fvalue) *
		v_iroll_level->fvalue;
	r_refdef.viewangles[PITCH] += Q_sin (cl.time * v_ipitch_cycle->fvalue) *
		v_ipitch_level->fvalue;
	r_refdef.viewangles[YAW] += Q_sin (cl.time * v_iyaw_cycle->fvalue) *
		v_iyaw_level->fvalue;
}

/*
==================
V_AddEntity
==================
*/
void
V_AddEntity ( entity_t *ent )
{
	if ( r_refdef.num_entities >= MAX_VISEDICTS ) {
		Sys_Error ("ERROR! Out of entitys!");
		return;
	}

	r_refdef.entities[r_refdef.num_entities++] = &ent->common;
}

/*
==================
V_ClearEntities
==================
*/
void
V_ClearEntities ( void )
{
	r_refdef.num_entities = 0;
}

/*
==================
V_CalcRefdef

==================
*/
void
V_CalcRefdef (void)
{
	entity_t		*ent;
	entity_t		*view;
	int				i;
	vec3_t			forward, right, up;
	vec3_t			angles, origin;
	float			bob;
	static float	oldz = 0;

	V_DriftPitch();

	/* ent is the player model (visible when out of body) */
	ent = &cl_entities[cl.viewentity];

	/* view is the weapon model (only visible from inside body) */
	view = &cl.viewent;

	/* Keep the colormap correct. */
	cl.colormap = ent->common.colormap;

	/* transform the view offset by the model's matrix to get the offset from
	   model origin for the view */
	ent->common.angles[YAW] = cl.viewangles[YAW];		/* the model should */
	ent->common.angles[PITCH] = -cl.viewangles[PITCH];	/* face the view dir */

	bob = V_CalcBob();

	/* refresh position */
	VectorCopy (ent->common.origin, r_refdef.vieworg);
	r_refdef.vieworg[2] += cl.viewheight + bob;

	/* never let it sit exactly on a node line, because a water plane can
	   dissapear when viewed with the eye exactly on it.
	   the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis */
	r_refdef.vieworg[0] += 1.0 / 32;
	r_refdef.vieworg[1] += 1.0 / 32;
	r_refdef.vieworg[2] += 1.0 / 32;

	VectorCopy(cl.viewangles, r_refdef.viewangles);
	V_CalcViewRoll();
	V_AddIdle();

	if (v_zoom->fvalue != cl.viewzoom)
		cl.viewzoom = SLIDE (cl.viewzoom, v_zoom->fvalue, 4 * host_frametime);

	/* offsets */
	angles[PITCH] = -ent->common.angles[PITCH]; /* because entity pitches are actually backward */
	angles[YAW] = ent->common.angles[YAW];
	angles[ROLL] = ent->common.angles[ROLL];

	AngleVectors (angles, forward, right, up);
	for (i = 0; i < 3; i++)
		r_refdef.vieworg[i] +=
			scr_ofsx->ivalue * forward[i] +
			scr_ofsy->ivalue * right[i] +
			scr_ofsz->ivalue * up[i];

	V_BoundOffsets();

	/* set up gun position */
	VectorCopy (cl.viewangles, angles);

	/* set up gun angles */
	angles[YAW] = r_refdef.viewangles[YAW];
	angles[PITCH] = -r_refdef.viewangles[PITCH];

	VectorCopy (ent->common.origin, origin);
	origin[2] += cl.viewheight;

	for (i = 0; i < 3; i++) {
		origin[i] += forward[i] * bob * 0.4;
	}

	origin[2] += bob;
	if (view->common.model != cl.model_precache[cl.stats[STAT_WEAPON]])
	{
		memset(view, 0, sizeof(*view));
		view->common.model = cl.model_precache[cl.stats[STAT_WEAPON]];
	}

	/* set up the refresh position */
	VectorAdd (r_refdef.viewangles, cl.punchangle, r_refdef.viewangles);

	/* smooth out stair step ups */
	if (cl.onground && (ent->common.origin[2] - oldz > 0)) {
		float	steptime;

		steptime = (cl.time - cl.oldtime);

		oldz += steptime * 80;
		if (oldz > ent->common.origin[2])
			oldz = ent->common.origin[2];
		if (ent->common.origin[2] - oldz > 12)
			oldz = ent->common.origin[2] - 12;
		r_refdef.vieworg[2] += oldz - ent->common.origin[2];
		origin[2] += oldz - ent->common.origin[2];
	} else
		oldz = ent->common.origin[2];

	CL_Update_OriginAngles(view, origin, angles, cl.mtime[1]);
	CL_Update_Frame(view, cl.stats[STAT_WEAPONFRAME], cl.mtime[1]);

	if (chase_active->ivalue)
		Chase_Update();
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/

void
V_RenderView (void)
{
	if (con_forcedup)
		return;

	/* don't allow cheats in multiplayer */
	if (cl.maxclients > 1)
	{
		Cvar_Set (scr_ofsx, "0");
		Cvar_Set (scr_ofsy, "0");
		Cvar_Set (scr_ofsz, "0");
	}

	if (cl.intermission)
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


/*
=============
V_Init_Cvars
=============
*/
void
V_Init_Cvars (void)
{
	scr_ofsx = Cvar_Get ("scr_ofsx", "0", CVAR_NONE, NULL);
	scr_ofsy = Cvar_Get ("scr_ofsy", "0", CVAR_NONE, NULL);
	scr_ofsz = Cvar_Get ("scr_ofsz", "0", CVAR_NONE, NULL);

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

/*
=============
V_Init
=============
*/
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
	cl.viewent.common.real_ent = &cl.viewent;

	if (!r_drawviewmodel->ivalue || chase_active->ivalue ||
			!r_drawentities->ivalue || cl.items & IT_INVISIBILITY ||
			(cl.stats[STAT_HEALTH] <= 0) || !cl.viewent.common.model)
		return;

	// hack the depth range to prevent view model from poking into walls
	qglDepthRange (0.0f, 0.3f);
	R_DrawOpaqueAliasModels(&ent_pointer, 1, true);
	qglDepthRange (0.0f, 1.0f);
}
					

