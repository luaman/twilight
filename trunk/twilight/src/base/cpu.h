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

	$Id$
*/
/* crc.h */

#ifndef __CPU_H
#define __CPU_H

#include "mathlib.h"

void CPU_Init();
#define CPU_CPUID									BIT(0)
#define CPU_CPUID_EXT								BIT(1)
#define CPU_MMX										BIT(2)
#define CPU_MMX_EXT									BIT(3)
#define CPU_SSE										BIT(4)
#define CPU_SSE2									BIT(5)
#define CPU_3DNOW									BIT(6)
#define CPU_3DNOW_EXT								BIT(7)
extern Uint32 cpu_flags;

#endif // __CPU_H

