/*
	teamplay.c

	Teamplay enhancements ("proxy features")

	Copyright (C) 2000       Anton Gavrilov (tonik@quake.ru)

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

	$Id$
*/

#include "twiconfig.h"
#include "quakedef.h"
#include "qtypes.h"
#include "mathlib.h"
#include "client.h"
#include "cvar.h"
#include "cmd.h"
#include "locs.h"
#include "strlib.h"
#include "sys.h"

cvar_t	*cl_parsesay;
static qboolean died = false, recorded_location = false;
static vec3_t death_location, last_recorded_location;

/*
===================
Team_ParseSay
===================
*/
char *
Team_ParseSay (const char *s)
{
	static char			buf[1024];
	unsigned int		i;
	int					bracket;
	char				chr, *t1, t2[128], t3[128];
	location_t			*location;

	i = 0;

	while (*s && (i <= sizeof (buf))) {
		if ((*s == '%') && (s[1] != '\0')) {
			t1 = NULL;
			memset (t2, '\0', sizeof (t2));
			memset (t3, '\0', sizeof (t3));

			if ((s[1] == '[') && (s[3] == ']')) {
				bracket = 1;
				chr = s[2];
				s += 4;
			} else {
				bracket = 0;
				chr = s[1];
				s += 2;
			}

			switch (chr) {
				case '%':
					t2[0] = '%';
					t2[1] = 0;
					t1 = t2;
					break;
				case 'd':
					bracket = 0;
					if (died) {
						location = loc_search (death_location);
						if (location) {
							recorded_location = true;
							VectorCopy (death_location,
									last_recorded_location);
							t1 = location->name;
							break;
						}
					}
					goto location;
				case 'r':
					bracket = 0;
					if (recorded_location) {
						location = loc_search (last_recorded_location);
						if (location) {
							t1 = location->name;
							break;
						}
					}
					goto location;
				case 'l':
location:
					bracket = 0;
					location = loc_search (cl_entities[cl.viewentity].common.origin);
					if (location) {
						recorded_location = true;
						VectorCopy (cl_entities[cl.viewentity].common.origin, last_recorded_location);
						t1 = location->name;
					} else
						snprintf (t2, sizeof (t2), "Unknown!\n");
					break;
				case 'a':
					if (bracket) {
						if (cl.stats[STAT_ARMOR] > 50)
							bracket = 0;

						if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
							t3[0] = 'R' | 0x80;
						else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
							t3[0] = 'Y' | 0x80;
						else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
							t3[0] = 'G' | 0x80;
						else {
							t2[0] = 'N' | 0x80;
							t2[1] = 'O' | 0x80;
							t2[2] = 'N' | 0x80;
							t2[3] = 'E' | 0x80;
							t2[4] = '!' | 0x80;
							break;
						}

						snprintf (t2, sizeof (t2), "%sa:%i", t3,
								cl.stats[STAT_ARMOR]);
					} else
						snprintf (t2, sizeof (t2), "%i",
								cl.stats[STAT_ARMOR]);
					break;
				case 'A':
					bracket = 0;
					if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
						t2[0] = 'R' | 0x80;
					else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
						t2[0] = 'Y' | 0x80;
					else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
						t2[0] = 'G' | 0x80;
					else {
						t2[0] = 'N' | 0x80;
						t2[1] = 'O' | 0x80;
						t2[2] = 'N' | 0x80;
						t2[3] = 'E' | 0x80;
						t2[4] = '!' | 0x80;
					}
					break;
				case 'h':
					if (bracket) {
						if (cl.stats[STAT_HEALTH] > 50)
							bracket = 0;
						snprintf (t2, sizeof (t2), "h:%i",
								cl.stats[STAT_HEALTH]);
					} else
						snprintf (t2, sizeof (t2), "%i",
								cl.stats[STAT_HEALTH]);
					break;
				default:
					bracket = 0;
			}

			if (!t1) {
				if (!t2[0]) {
					t2[0] = '%';
					t2[1] = chr;
				}

				t1 = t2;
			}

			if (bracket)
				buf[i++] = 0x90;		// '['

			if (t1) {
				int			len;

				len = strlen (t1);
				if (i + len >= sizeof (buf))
					continue;			// No more space in buffer, icky
				strncpy (buf + i, t1, len);
				i += len;
			}

			if (bracket)
				buf[i++] = 0x91;		// ']'

			continue;
		}

		buf[i++] = *s++;
	}

	buf[i] = 0;
	return buf;
}

void
Team_Dead (void)
{
	died = true;
	VectorCopy (cl_entities[cl.viewentity].common.origin, death_location);
}

void
Team_NewMap (void)
{
	loc_newmap (cl.worldmodel->name);

	died = false;
	recorded_location = false;
}

void
Team_Init_Cvars (void)
{
	cl_parsesay = Cvar_Get ("cl_parsesay", "0", CVAR_NONE, NULL);
}

static void
Team_loc (void)
{
	char		*desc = NULL;
	location_t	*loc;

	if (Cmd_Argc () == 1) {
		Com_Printf ("loc <add|delete|rename|move|save|zsave> [<description>] :Modifies location data, add|rename take <description> parameter\n");
		return;
	}

	if (Cmd_Argc () >= 3)
		desc = Cmd_Args () + strlen (Cmd_Argv (1)) + 1;
	
	if (!strcasecmp (Cmd_Argv (1), "save")) {
		if (Cmd_Argc () == 2) {
			loc_write (cl.worldmodel->name);
		} else {
			Com_Printf ("loc save :saves locs from memory into a .loc file\n");
		}
	}
	
	if (cls.state != ca_connected) {
		Com_Printf ("Not connected.\n");
		return;
	}
	
	if (!strcasecmp (Cmd_Argv (1), "add")) {
		if (Cmd_Argc () >= 3)
			loc_new (cl_entities[cl.viewentity].common.origin, desc);
		else
			Com_Printf ("loc add <description> :marks the current location with the description and records the information into a loc file.\n");
	}

	if (!strcasecmp (Cmd_Argv (1), "rename")) {
		if (Cmd_Argc () >= 3) {
			loc = loc_search (cl_entities[cl.viewentity].common.origin);
			if (loc)
				strlcpy (loc->name, desc, sizeof(loc->name));
		} else
			Com_Printf ("loc rename <description> :changes the description of the nearest location marker\n");
	}
	
	if (!strcasecmp (Cmd_Argv (1), "delete")) {
		if (Cmd_Argc () == 2) {
			loc = loc_search (cl_entities[cl.viewentity].common.origin);
			if (loc)
				loc_delete (loc);
		} else
			Com_Printf ("loc delete :removes nearest location marker\n");
	}
	
	if (!strcasecmp (Cmd_Argv (1), "move")) {
		if (Cmd_Argc () == 2) {
			loc = loc_search (cl_entities[cl.viewentity].common.origin);
			if (loc)
				VectorCopy(cl_entities[cl.viewentity].common.origin, loc->where);
		} else
			Com_Printf ("loc move :moves the nearest location marker to your current location\n");
	}
}

void
Team_Init (void)
{
	loc_init ();
	Cmd_AddCommand ("loc", Team_loc);
}
