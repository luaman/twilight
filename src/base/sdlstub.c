/*
	$RCSfile$

	Copyright (C) 2003       Forest Hale
	Copyright (C) 1997-2001  Sam Lantinga

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

#ifndef HAVE_SDL_H

/*
 * If SDL is not available, we can still (probably) build a server for UNIX
 * systems by pretending we have just enough of SDL to get away with it.  We
 * provide this stub for that purpose.
 */

#include "sdlstub.h"

#include <sys/time.h>

struct timeval start;

// this function came from SDL timer/linux/SDL_systimer.c
void SDL_StartTicks(void)
{
	/* Set first ticks value */
	gettimeofday(&start, NULL);
}

// this function came from SDL timer/linux/SDL_systimer.c
Uint32 SDL_GetTicks (void)
{
	struct timeval now;
	Uint32 ticks;

	gettimeofday(&now, NULL);
	ticks=(now.tv_sec-start.tv_sec)*1000+(now.tv_usec-start.tv_usec)/1000;
	return(ticks);
}

// this function is original (but dumb)
void SDL_Delay(Uint32 ms)
{
	usleep(ms * 1000);
}

// this function is original (but dumb)
int SDL_Init(Uint32 flags)
{
	flags = flags;
	SDL_StartTicks();
	return 0;
}

void SDL_Quit(void)
{
}

#endif // !HAVE_SDL_H

