/*
	$RCSfile$

	Copyright (C) 2002  Joseph Carter

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

#include <string.h>

#include "client.h"
#include "image/image.h"

/*
 * Null client
 *
 * Basically, anything required to make the server compile without a client
 * should go in here.  It's just a stub.
 */

// net_chan depends on cls (for now)
client_common_static_t ccls;
client_static_t cls;

// Functions that don't actually exist

void
Draw_Disc (void)
{
}

void
CL_Disconnect (void)
{
}

void
Con_Print (char *txt)
{
	txt = txt;
}

void
CL_Init (void)
{
	memset (&cls, 0, sizeof(client_static_t));
}

void
M_ToggleMenu_f(void)
{
}

void
M_SetKeyDest(void)
{
}

image_t *
Draw_CacheImg(char *path)
{
	path = path;
	
	return NULL;
}

image_t *
Image_Load (char *name, int flags)
{
	name = name;
	flags = flags;
	return NULL;
}
