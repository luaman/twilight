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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# ifdef WIN32
#  include <win32conf.h>
# endif
#endif

#include "qtypes.h"
#include "strlib.h"

#define MAX_NUM_ARGVS   50
#define NUM_SAFE_ARGVS  5

static char *largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];
static char *argvdummy = " ";
static char *safeargvs[NUM_SAFE_ARGVS] =
	{ "-nocdaudio", "-nolan", "-nomouse", "-nosound", "-window" };

int         com_argc;
char      **com_argv;

/*
================
COM_InitArgv
================
*/
void
COM_InitArgv (int argc, char **argv)
{
    qboolean    safe;
    int         i;

    safe = false;

    for (com_argc = 0; (com_argc < MAX_NUM_ARGVS) && (com_argc < argc);
         com_argc++) {
        largv[com_argc] = argv[com_argc];
        if (!strcmp ("-safe", argv[com_argc]))
            safe = true;
    }

    if (safe) {
        // force all the safe-mode switches. Note that we reserved extra space
        // in case we need to add these, so we don't need an overflow check
        for (i = 0; i < NUM_SAFE_ARGVS; i++) {
            largv[com_argc] = safeargvs[i];
            com_argc++;
        }
    }

    largv[com_argc] = argvdummy;
    com_argv = largv;
}


/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int
COM_CheckParm (char *parm)
{
    int         i;

    for (i = 1; i < com_argc; i++) {
        if (!com_argv[i])
            // NEXTSTEP sometimes clears appkit vars.
            continue;
        if (!strcmp (parm, com_argv[i]))
            return i;
    }

    return 0;
}


