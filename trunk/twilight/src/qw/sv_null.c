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

#include <stdio.h>

#include "qtypes.h"
#include "cvar.h"

/*
 * Null server
 *
 * Basically, anything required to make the client compile without a server
 * should go in here.  It's just a stub.
 */

// Cvars which don't actually exist
cvar_t *sv_highchars = NULL;

// QW's stdio "console"
qboolean do_stdin = true;
qboolean stdin_ready;

// Functions that don't actually exist

qboolean
ServerPaused (void)
{
	return true;
}

