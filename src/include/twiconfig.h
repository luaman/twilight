/*
	$RCSfile$

	Copyright (C) 2001-2002  Joseph Carter

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

#ifndef __TWICONFIG_H
#define __TWICONFIG_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# ifdef _WIN32
#  include "win32config.h"
# endif
#endif

/*
 * Win32 uses an underscore at the beginning of the functions it provides for
 * POSIX "compatibility" - mangle things here if we need to.
 */
#if !defined(HAVE_SNPRINTF) && defined(HAVE__SNPRINTF)
# define snprintf _snprintf
#endif
#if !defined(HAVE_VSNPRINTF) && defined(HAVE__VSNPRINTF)
# define vsnprintf _vsnprintf
#endif
#if !defined(HAVE_MKDIR) && defined(HAVE__MKDIR)
# define mkdir(s) _mkdir((s))
#endif

#endif // __TWICONFIG_H

