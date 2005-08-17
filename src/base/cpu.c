/*
	$RCSfile$

	Copyright (C) 2002  Zephaniah E. Hull.

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
/* crc.c */
static const char rcsid[] =
    "$Id$";

#include "twiconfig.h"

#include <string.h>
#include "qtypes.h"
#include "mathlib.h"
#include "common.h"
#include "cpu.h"

Uint32	cpu_flags;
char	*cpu_id;

void
check_cpuid ()
{
	Uint32	raw_flags = 0, raw_eflags = 0;
	char	raw_id[13] = {0};
	Uint32	*tmp = (Uint32 *) &raw_id[0];

#ifdef HAVE_GCC_ASM_X86_CPUID
	asm ("\n"
#ifdef HAVE_GCC_ASM_X86_EFLAGS
		"pushfl\n"
		"pop					%%eax\n"
		"mov					%%eax, %%ebx\n"
		"xor					$0x200000, %%eax\n"
		"push					%%eax\n"
		"popfl\n"
		"pushfl\n"
		"pop					%%eax\n"
		"push					%%ebx\n"
		"popfl\n"
		"xorl					%%ebx, %%eax\n"
		"je						end\n"
#endif
		"mov					$0, %%eax\n"
		"cpuid\n"
		"mov					%%ebx, %2\n"
		"mov					%%edx, 4%2\n"
		"mov					%%ecx, 8%2\n"
		"cmp					$0, %%eax\n"
		"je						end\n"
		"mov					$1, %%eax\n"
		"cpuid\n"
		"mov					%%edx, %0\n"
		"mov					$0x80000000, %%eax\n"
		"cpuid\n"
		"cmp					$0, %%eax\n"
		"je						end\n"
		"mov					$0x80000001, %%eax\n"
		"cpuid\n"
		"mov					%%edx, %1\n"
		"end:\n"
		: "=m" (raw_flags), "=m" (raw_eflags), "=m" (*tmp)
		: : "eax", "ebx", "ecx", "edx", "memory");
#endif

	if (raw_flags)
		cpu_flags |= CPU_CPUID;
	if (raw_eflags)
		cpu_flags |= CPU_CPUID_EXT;

	switch (*tmp) {
		case 0x756e6547:	// Intel
			if (raw_flags & BIT(23))
				cpu_flags |= CPU_MMX;
			if (raw_flags & BIT(25)) {
				cpu_flags |= CPU_MMX_EXT;
				cpu_flags |= CPU_SSE;
			}
			if (raw_flags & BIT(26)) {
				cpu_flags |= CPU_MMX_EXT;
				cpu_flags |= CPU_SSE2;
			}
			break;
		case 0x68747541:	// AMD
			if (raw_flags & BIT(25))
				cpu_flags |= CPU_SSE;
			if (raw_eflags & BIT(22))
				cpu_flags |= CPU_MMX_EXT;
			if (raw_eflags & BIT(23))
				cpu_flags |= CPU_MMX;
			if (raw_eflags & BIT(30))
				cpu_flags |= CPU_3DNOW_EXT;
			if (raw_eflags & BIT(31))
				cpu_flags |= CPU_3DNOW;
			break;
		case 0x69727943:	// Cyrix
			if (raw_eflags & BIT(23))
				cpu_flags |= CPU_MMX;
			if (raw_eflags & BIT(24))
				cpu_flags |= CPU_MMX_EXT;
			if (raw_eflags & BIT(31))
				cpu_flags |= CPU_3DNOW;
			break;
	}

	cpu_id = strdup(raw_id);

#ifndef HAVE_GCC_ASM_X86_MMX
	if (raw_flags & BIT(23))	// Bit 23 is universally used for MMX.
		cpu_flags &= ~CPU_MMX;
#endif
}

void
CPU_Init()
{
	if (COM_CheckParm ("-nocpuid"))
		return;

	Com_Printf("Checking CPUID.\n");
	check_cpuid();

	if (cpu_flags)
	{
		Com_Printf ("CPU maker: %s\n", cpu_id);
		Com_Printf ("CPU flags:");
		if (cpu_flags & CPU_CPUID)
			Com_Printf(" CPUID");
		if (cpu_flags & CPU_CPUID_EXT)
			Com_Printf(" CPUID_EXT");
		if (cpu_flags & CPU_MMX)
			Com_Printf(" MMX");
		if (cpu_flags & CPU_MMX_EXT)
			Com_Printf(" MMX_EXT");
		if (cpu_flags & CPU_SSE)
			Com_Printf(" SSE");
		if (cpu_flags & CPU_SSE2)
			Com_Printf(" SSE2");
		if (cpu_flags & CPU_3DNOW)
			Com_Printf(" 3DNOW");
		if (cpu_flags & CPU_3DNOW_EXT)
			Com_Printf(" 3DNOW_EXT");
		Com_Printf(".\n");
	}
}

