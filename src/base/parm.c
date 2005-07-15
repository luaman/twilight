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

#include "qtypes.h"
#include "strlib.h"
#include "sys.h"

#define MAX_NUM_ARGVS   50
#define NUM_SAFE_ARGVS  6

static char *largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];
static char *argvdummy = " ";
static char *safeargvs[NUM_SAFE_ARGVS] =
	{ "-nocdaudio", "-nolan", "-nomouse", "-nosound", "-window", "-nocpuid"};

int com_argc;
char **com_argv;

void
Cmdline_Init (int argc, char *argv[])
{
    qboolean	safe = false;
    int			i;

	if (argc > MAX_NUM_ARGVS)
	{
		Sys_Printf ("Cmdline_Init: %i parameters, can only handle %i\n",
				argc, MAX_NUM_ARGVS);
		argc = MAX_NUM_ARGVS;
	}

    for (i = 0; i < argc; i++)
	{
        largv[i] = argv[i];
        if (!strcmp ("-safe", argv[i]))
            safe = true;
    }

	com_argc = i;
    com_argv = largv;

    if (safe)
	{
        // force all the safe-mode switches. Note that we reserved extra space
        // in case we need to add these, so we don't need an overflow check
        for (i = 0; i < NUM_SAFE_ARGVS; i++)
		{
            largv[com_argc] = safeargvs[i];
            com_argc++;
        }
    }

    largv[com_argc] = argvdummy;
}


/*
================
Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int
COM_CheckParm (const char *parm)
{
    int			i;

    for (i = 1; i < com_argc; i++)
	{
        if (!com_argv[i])
            // NEXTSTEP sometimes clears appkit vars.
            continue;
        if (!strcmp (parm, com_argv[i]))
            return i;
    }

    return 0;
}

