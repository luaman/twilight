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

#include "SDL.h"

#include "quakedef.h"
#include "client.h"
#include "cmd.h"
#include "cvar.h"
#include "input.h"
#include "mathlib.h"
#include "strlib.h"
#include "crc.h"
#include "host.h"

static cvar_t	*cl_nodelta;

// cvars
static cvar_t	*in_key_repeat_delay;
static cvar_t	*in_key_repeat_interval;

static cvar_t	*lookspring;
static cvar_t	*lookstrafe;
static cvar_t	*sensitivity;

static cvar_t	*m_filter;
static cvar_t	*m_pitch;
static cvar_t	*m_yaw;
static cvar_t	*m_forward;
static cvar_t	*m_side;
static cvar_t	*m_freelook;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/


static kbutton_t in_klook;
static kbutton_t in_left, in_right, in_forward, in_back;
static kbutton_t in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t in_speed, in_use, in_jump, in_attack;
static kbutton_t in_up, in_down;
static kbutton_t in_strafe, in_mlook;

static int in_impulse;


static void
KeyDown (kbutton_t *b)
{
	int			k;
	const char	*c;

	c = Cmd_Argv (1);
	if (c[0])
		k = Q_atoi (c);
	else
		// typed manually at the console for continuous down
		k = -1;

	if (k == b->down[0] || k == b->down[1])
		// repeating key
		return;

	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else {
		Com_Printf ("Three keys down for a button!\n");
		return;
	}

	if (b->state & 1)
		// still down
		return;

	// down + impulse down
	b->state |= 1 + 2;
}

static void
KeyUp (kbutton_t *b)
{
	int			k;
	const char	*c;

	c = Cmd_Argv (1);
	if (c[0])
		k = Q_atoi (c);
	else {
		// typed manually at the console, assume for unsticking, clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		// key up without coresponding down (menu pass through)
		return;
	if (b->down[0] || b->down[1])
		// some other key is still holding it down
		return;

	if (!(b->state & 1))
		return;							// still up (this should not happen)
	b->state &= ~1;						// now up
	b->state |= 4;						// impulse up
}

static void
IN_KLookDown (void)
{
	KeyDown (&in_klook);
}

static void
IN_KLookUp (void)
{
	KeyUp (&in_klook);
}

static void
IN_MLookDown (void)
{
	KeyDown (&in_mlook);
}

static void
IN_MLookUp (void)
{
	KeyUp (&in_mlook);
	if (!freelook && lookspring->ivalue)
		V_StartPitchDrift ();
}

static void
IN_UpDown (void)
{
	KeyDown (&in_up);
}

static void
IN_UpUp (void)
{
	KeyUp (&in_up);
}

static void
IN_DownDown (void)
{
	KeyDown (&in_down);
}

static void
IN_DownUp (void)
{
	KeyUp (&in_down);
}

static void
IN_LeftDown (void)
{
	KeyDown (&in_left);
}

static void
IN_LeftUp (void)
{
	KeyUp (&in_left);
}

static void
IN_RightDown (void)
{
	KeyDown (&in_right);
}

static void
IN_RightUp (void)
{
	KeyUp (&in_right);
}

static void
IN_ForwardDown (void)
{
	KeyDown (&in_forward);
}

static void
IN_ForwardUp (void)
{
	KeyUp (&in_forward);
}

static void
IN_BackDown (void)
{
	KeyDown (&in_back);
}

static void
IN_BackUp (void)
{
	KeyUp (&in_back);
}

static void
IN_LookupDown (void)
{
	KeyDown (&in_lookup);
}

static void
IN_LookupUp (void)
{
	KeyUp (&in_lookup);
}

static void
IN_LookdownDown (void)
{
	KeyDown (&in_lookdown);
}

static void
IN_LookdownUp (void)
{
	KeyUp (&in_lookdown);
}

static void
IN_MoveleftDown (void)
{
	KeyDown (&in_moveleft);
}

static void
IN_MoveleftUp (void)
{
	KeyUp (&in_moveleft);
}

static void
IN_MoverightDown (void)
{
	KeyDown (&in_moveright);
}

static void
IN_MoverightUp (void)
{
	KeyUp (&in_moveright);
}

static void
IN_SpeedDown (void)
{
	KeyDown (&in_speed);
}

static void
IN_SpeedUp (void)
{
	KeyUp (&in_speed);
}

static void
IN_StrafeDown (void)
{
	KeyDown (&in_strafe);
}

static void
IN_StrafeUp (void)
{
	KeyUp (&in_strafe);
}

static void
IN_AttackDown (void)
{
	KeyDown (&in_attack);
}

static void
IN_AttackUp (void)
{
	KeyUp (&in_attack);
}

static void
IN_UseDown (void)
{
	KeyDown (&in_use);
}

static void
IN_UseUp (void)
{
	KeyUp (&in_use);
}

static void
IN_JumpDown (void)
{
	KeyDown (&in_jump);
}

static void
IN_JumpUp (void)
{
	KeyUp (&in_jump);
}

static void
IN_Impulse (void)
{
	in_impulse = Q_atoi (Cmd_Argv (1));
}

static void
IN_CenterView (void)
{
	cl.viewangles[PITCH] = 0;
}

/*
===============
Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
static float
CL_KeyState (kbutton_t *key)
{
	float       val;
	qboolean    impulsedown, impulseup, down;

	impulsedown = key->state & 2;
	impulseup = key->state & 4;
	down = key->state & 1;
	val = 0;

	if (impulsedown && !impulseup) {
		if (down)
			val = 0.5;					// pressed and held this frame
		else
			val = 0;					// I_Error ();
	}
	if (impulseup && !impulsedown) {
		if (down)
			val = 0;					// I_Error ();
		else
			val = 0;					// released this frame
	}
	if (!impulsedown && !impulseup) {
		if (down)
			val = 1.0;					// held the entire frame
		else
			val = 0;					// up the entire frame
	}
	if (impulsedown && impulseup) {
		if (down)
			val = 0.75;					// released and re-pressed this frame
		else
			val = 0.25;					// pressed and released this frame
	}

	key->state &= 1;					// clear impulses

	return val;
}




//==========================================================================

cvar_t *cl_upspeed;
cvar_t *cl_forwardspeed;
cvar_t *cl_backspeed;
cvar_t *cl_sidespeed;

cvar_t *cl_movespeedkey;

cvar_t *cl_yawspeed;
cvar_t *cl_pitchspeed;

cvar_t *cl_anglespeedkey;


/*
================
Moves the local angle positions
================
*/
static void
CL_AdjustAngles (void)
{
	float       speed;
	float       up, down;

	if (in_speed.state & 1)
		speed = host.frametime * cl_anglespeedkey->fvalue;
	else
		speed = host.frametime;

	if (!(in_strafe.state & 1)) {
		cl.viewangles[YAW] -=
			speed * cl_yawspeed->fvalue * CL_KeyState (&in_right);
		cl.viewangles[YAW] +=
			speed * cl_yawspeed->fvalue * CL_KeyState (&in_left);
		cl.viewangles[YAW] = ANGLEMOD (cl.viewangles[YAW]);
	}
	if (in_klook.state & 1) {
		V_StopPitchDrift ();
		cl.viewangles[PITCH] -=
			speed * cl_pitchspeed->fvalue * CL_KeyState (&in_forward);
		cl.viewangles[PITCH] +=
			speed * cl_pitchspeed->fvalue * CL_KeyState (&in_back);
	}

	up = CL_KeyState (&in_lookup);
	down = CL_KeyState (&in_lookdown);

	cl.viewangles[PITCH] -= speed * cl_pitchspeed->fvalue * up;
	cl.viewangles[PITCH] += speed * cl_pitchspeed->fvalue * down;

	if (up || down)
		V_StopPitchDrift ();

	// KB: Allow looking straight up/down
	cl.viewangles[PITCH] = bound (-90, cl.viewangles[PITCH], 90 - ANG16_DELTA);
	cl.viewangles[ROLL] = bound (-50, cl.viewangles[ROLL], 50);
}

/*
================
Send the intended movement message to the server
================
*/
static void
CL_BaseMove (usercmd_t *cmd)
{
	CL_AdjustAngles ();

	memset (cmd, 0, sizeof (*cmd));

	VectorCopy (cl.viewangles, cmd->angles);
	if (in_strafe.state & 1) {
		cmd->sidemove += cl_sidespeed->fvalue * CL_KeyState (&in_right);
		cmd->sidemove -= cl_sidespeed->fvalue * CL_KeyState (&in_left);
	}

	cmd->sidemove += cl_sidespeed->fvalue * CL_KeyState (&in_moveright);
	cmd->sidemove -= cl_sidespeed->fvalue * CL_KeyState (&in_moveleft);

	cmd->upmove += cl_upspeed->fvalue * CL_KeyState (&in_up);
	cmd->upmove -= cl_upspeed->fvalue * CL_KeyState (&in_down);

	if (!(in_klook.state & 1)) {
		cmd->forwardmove += cl_forwardspeed->fvalue
			* CL_KeyState (&in_forward);
		cmd->forwardmove -= cl_backspeed->fvalue * CL_KeyState (&in_back);
	}
	//
	// adjust for speed key
	//
	if (in_speed.state & 1) {
		cmd->forwardmove *= cl_movespeedkey->fvalue;
		cmd->sidemove *= cl_movespeedkey->fvalue;
		cmd->upmove *= cl_movespeedkey->fvalue;
	}
}

static int
MakeChar (int i)
{
	i &= ~3;
	i = bound ((-127 * 4), i, (127 * 4));

	return i;
}

static void
CL_FinishMove (usercmd_t *cmd)
{
	int         i;
	int         ms;

	//
	// always dump the first two message, because it may contain leftover inputs
	// from the last level
	//
	if (++cl.movemessages <= 2)
		return;
	//
	// figure button bits
	//  
	if (in_attack.state & 3)
		cmd->buttons |= 1;
	in_attack.state &= ~2;

	if (in_jump.state & 3)
		cmd->buttons |= 2;
	in_jump.state &= ~2;

	if (in_use.state & 3)
		cmd->buttons |= 4;
	in_use.state &= ~2;

	// send milliseconds of time to apply the move
	ms = host.frametime * 1000;
	if (ms > 200)
		ms = 200;						// time was unreasonable
	cmd->msec = ms;

	VectorCopy (cl.viewangles, cmd->angles);

	cmd->impulse = in_impulse;
	in_impulse = 0;


	//
	// chop down so no extra bits are kept that the server wouldn't get
	//
	cmd->forwardmove = MakeChar (cmd->forwardmove);
	cmd->sidemove = MakeChar (cmd->sidemove);
	cmd->upmove = MakeChar (cmd->upmove);

	for (i = 0; i < 3; i++)
		cmd->angles[i] = ((int) (cmd->angles[i] * (65536.0f / 360.0f) + 0.5f) & 65535)
			* (360.0 / 65536.0);
}

void
CL_SendCmd (void)
{
	sizebuf_t   buf;
	Uint8       data[128];
	int         i;
	usercmd_t  *cmd, *oldcmd;
	int         checksumIndex;
	int         lost;
	int         seq_hash;

	if (ccls.demoplayback)
		return;							// sendcmds come from the demo

	// save this command off for prediction
	i = cls.netchan.outgoing_sequence & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	cl.frames[i].senttime = ccl.time;
	cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet

//  seq_hash = (cls.netchan.outgoing_sequence & 0xffff) ; // ^ QW_CHECK_HASH;
	seq_hash = cls.netchan.outgoing_sequence;

	// get basic movement from keyboard
	CL_BaseMove (cmd);

	// allow mice or other external controllers to add to the move
	IN_Move (cmd);

	// if we are spectator, try autocam
	if (cl.spectator)
		Cam_Track (cmd);

	CL_FinishMove (cmd);

	Cam_FinishMove (cmd);

	// send this and the previous cmds in the message, so
	// if the last packet was dropped, it can be recovered
	SZ_Init (&buf, data, sizeof(data));

	MSG_WriteByte (&buf, clc_move);

	// save the position for a checksum byte
	checksumIndex = buf.cursize;
	MSG_WriteByte (&buf, 0);

	// write our lossage percentage
	lost = CL_CalcNet ();
	MSG_WriteByte (&buf, (Uint8) lost);

	i = (cls.netchan.outgoing_sequence - 2) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	MSG_WriteDeltaUsercmd (&buf, &nullcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence - 1) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);

	// calculate a checksum over the move commands
	buf.data[checksumIndex] =
		COM_BlockSequenceCRCByte (buf.data + checksumIndex + 1,
								  buf.cursize - checksumIndex - 1, seq_hash);

	// request delta compression of entities
	if (cls.netchan.outgoing_sequence - cl.validsequence >= UPDATE_BACKUP - 1)
		cl.validsequence = 0;

	if (cl.validsequence && !cl_nodelta->ivalue &&
			ccls.state == ca_active && !ccls.demorecording) {
		cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].delta_sequence =
			cl.validsequence;
		MSG_WriteByte (&buf, clc_delta);
		MSG_WriteByte (&buf, cl.validsequence & 255);
	} else
		cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].delta_sequence =
			-1;

	if (ccls.demorecording)
		CL_WriteDemoCmd (cmd);

	//
	// deliver the message
	//
	Netchan_Transmit (&cls.netchan, buf.cursize, buf.data);
}

void
CL_Input_Init (void)
{
	Cmd_AddCommand ("+moveup", IN_UpDown);
	Cmd_AddCommand ("-moveup", IN_UpUp);
	Cmd_AddCommand ("+movedown", IN_DownDown);
	Cmd_AddCommand ("-movedown", IN_DownUp);
	Cmd_AddCommand ("+left", IN_LeftDown);
	Cmd_AddCommand ("-left", IN_LeftUp);
	Cmd_AddCommand ("+right", IN_RightDown);
	Cmd_AddCommand ("-right", IN_RightUp);
	Cmd_AddCommand ("+forward", IN_ForwardDown);
	Cmd_AddCommand ("-forward", IN_ForwardUp);
	Cmd_AddCommand ("+back", IN_BackDown);
	Cmd_AddCommand ("-back", IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+strafe", IN_StrafeDown);
	Cmd_AddCommand ("-strafe", IN_StrafeUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	Cmd_AddCommand ("+attack", IN_AttackDown);
	Cmd_AddCommand ("-attack", IN_AttackUp);
	Cmd_AddCommand ("+use", IN_UseDown);
	Cmd_AddCommand ("-use", IN_UseUp);
	Cmd_AddCommand ("+jump", IN_JumpDown);
	Cmd_AddCommand ("-jump", IN_JumpUp);
	Cmd_AddCommand ("impulse", IN_Impulse);
	Cmd_AddCommand ("+klook", IN_KLookDown);
	Cmd_AddCommand ("-klook", IN_KLookUp);
	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);
	Cmd_AddCommand ("force_centerview", IN_CenterView);
	SDL_EnableKeyRepeat(in_key_repeat_delay->ivalue,
			in_key_repeat_interval->ivalue);
}

static void
InputSetRepeatDelay (struct cvar_s *var)
{
	SDL_EnableKeyRepeat(var->ivalue, (in_key_repeat_interval) ?
			in_key_repeat_interval->ivalue : SDL_DEFAULT_REPEAT_INTERVAL);
}

static void
InputSetRepeatInterval (struct cvar_s *var)
{
	SDL_EnableKeyRepeat((in_key_repeat_delay) ? in_key_repeat_delay->ivalue
			: SDL_DEFAULT_REPEAT_DELAY, var->ivalue);
}

void
CL_Input_Init_Cvars (void)
{
	cl_upspeed = Cvar_Get ("cl_upspeed", "200", CVAR_NONE, NULL);
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", CVAR_ARCHIVE, NULL);
	cl_backspeed = Cvar_Get ("cl_backspeed", "200", CVAR_ARCHIVE, NULL);
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "350", CVAR_NONE, NULL);
	cl_movespeedkey = Cvar_Get ("cl_movespeedkey", "2.0", CVAR_NONE, NULL);
	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", CVAR_NONE, NULL);
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", CVAR_NONE, NULL);
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", CVAR_NONE, NULL);

	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE, NULL);
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE, NULL);
	sensitivity = Cvar_Get ("sensitivity", "3", CVAR_ARCHIVE, NULL);

	m_filter = Cvar_Get ("m_filter", "0", CVAR_NONE, NULL);
	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE, NULL);
	m_yaw = Cvar_Get ("m_yaw", "0.022", CVAR_ARCHIVE, NULL);
	m_forward = Cvar_Get ("m_forward", "1", CVAR_ARCHIVE, NULL);
	m_side = Cvar_Get ("m_side", "0.8", CVAR_ARCHIVE, NULL);
	m_freelook = Cvar_Get ("freelook", "1", CVAR_ARCHIVE, NULL);

	cl_nodelta = Cvar_Get ("cl_nodelta", "0", CVAR_NONE, NULL);
	in_key_repeat_delay = Cvar_Get ("in_key_repeat_delay",
			va ("%i", SDL_DEFAULT_REPEAT_DELAY), CVAR_ARCHIVE,
			InputSetRepeatDelay);
	in_key_repeat_interval = Cvar_Get ("in_key_repeat_interval",
			va ("%i", SDL_DEFAULT_REPEAT_INTERVAL), CVAR_ARCHIVE,
			InputSetRepeatInterval);
}

void
IN_Move (usercmd_t *cmd)
{
	if (m_filter->fvalue) {
		static float old_mouse_x, old_mouse_y;
		if ((mouse_x != old_mouse_x) || (mouse_y != old_mouse_y)) {
			mouse_x = (mouse_x + old_mouse_x) * 0.5;
			mouse_y = (mouse_y + old_mouse_y) * 0.5;
		}

		old_mouse_x = mouse_x;
		old_mouse_y = mouse_y;
	}

	mouse_x *= sensitivity->fvalue * ccl.viewzoom;
	mouse_y *= sensitivity->fvalue * ccl.viewzoom;

	if ((in_strafe.state & 1) || (lookstrafe->ivalue && freelook))
		cmd->sidemove += m_side->fvalue * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw->fvalue * mouse_x;

	if (freelook)
		V_StopPitchDrift ();

	if (freelook && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch->fvalue * mouse_y;
		// KB: Allow looking straight up/down
		cl.viewangles[PITCH] = bound (-90, cl.viewangles[PITCH],
				90 - ANG16_DELTA);
	}
	else
	{
		if (in_strafe.state & 1)
			cmd->upmove -= m_forward->fvalue * mouse_y;
		else
			cmd->forwardmove -= m_forward->fvalue * mouse_y;
	}

	mouse_x = mouse_y = 0.0;
}

