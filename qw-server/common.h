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
// comndef.h  -- general definitions

#ifndef __COMMON_H
#define __COMMON_H

#include <SDL_byteorder.h>

#define	MAX_INFO_STRING	196
#define	MAX_SERVERINFO_STRING	512
#define	MAX_LOCALINFO_STRING	32768

extern struct cvar_s *fs_userconf;
extern struct cvar_s *fs_userpath;
extern struct cvar_s *fs_shareconf;
extern struct cvar_s *fs_sharepath;

//==========================================================================n

typedef struct sizebuf_s {
	qboolean    allowoverflow;			// if false, do a Sys_Error
	qboolean    overflowed;				// set to true if the buffer size
	// failed
	Uint8      *data;
	int         maxsize;
	int         cursize;
} sizebuf_t;

void        SZ_Clear (sizebuf_t *buf);
void       *SZ_GetSpace (sizebuf_t *buf, int length);
void        SZ_Write (sizebuf_t *buf, void *data, int length);
void        SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s {
	struct link_s *prev, *next;
} link_t;


void        ClearLink (link_t *l);
void        RemoveLink (link_t *l);
void        InsertLinkBefore (link_t *l, link_t *before);
void        InsertLinkAfter (link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((Uint8 *)l - (int)&(((t *)0)->m)))

//============================================================================

#ifndef NULL
#define NULL ((void *)0)
#endif

#define Q_MAXCHAR ((char)0x7f)
#define Q_MAXSHORT ((short)0x7fff)
#define Q_MAXINT	((int)0x7fffffff)
#define Q_MAXLONG ((int)0x7fffffff)
#define Q_MAXFLOAT ((int)0x7fffffff)

#define Q_MINCHAR ((char)0x80)
#define Q_MINSHORT ((short)0x8000)
#define Q_MININT 	((int)0x80000000)
#define Q_MINLONG ((int)0x80000000)
#define Q_MINFLOAT ((int)0x7fffffff)

//============================================================================
// Byte order macros

// These macros _MUST_ sign extend.

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
# define swap_short LittleShort
# define BigShort(x) ((Sint16)(x))

# define swap_long LittleLong
# define BigLong(x) ((Sint32)(x))

# define swap_float LittleFloat
# define BigFloat(x) (x)
#else
# define swap_short BigShort
# define LittleShort(x) ((Sint16)(x))

# define swap_long BigLong
# define LittleLong(x) ((Sint32)(x))

# define swap_float BigFloat
# define LittleFloat(x) (x)
#endif

static inline Sint16 swap_short(Sint16 x)
{
	return (x >> 8) | (x << 8);
}

static inline Sint32 swap_long(Sint32 x)
{
	return	(x >> 24) |
		((x & 0x00ff0000) >> 8) |
		((x & 0x0000ff00) << 8) |
		(x << 24);
}

static inline float swap_float(float f)
{
	Uint32 x;
	
	x = *(Uint32*)&f;
	x =	(x >> 24) |
		((x & 0x00ff0000) >> 8) |
		((x & 0x0000ff00) << 8) |
		(x << 24);
	f = *(float*)&x;
	return f;
}

#undef swap_short
#undef swap_long
#undef swap_float

//============================================================================

struct usercmd_s;

extern struct usercmd_s nullcmd;

void        MSG_WriteChar (sizebuf_t *sb, int c);
void        MSG_WriteByte (sizebuf_t *sb, int c);
void        MSG_WriteShort (sizebuf_t *sb, int c);
void        MSG_WriteLong (sizebuf_t *sb, int c);
void        MSG_WriteFloat (sizebuf_t *sb, float f);
void        MSG_WriteString (sizebuf_t *sb, char *s);
void        MSG_WriteCoord (sizebuf_t *sb, float f);
void        MSG_WriteAngle (sizebuf_t *sb, float f);
void        MSG_WriteAngle16 (sizebuf_t *sb, float f);
void        MSG_WriteDeltaUsercmd (sizebuf_t *sb, struct usercmd_s *from,
								   struct usercmd_s *cmd);

extern int  msg_readcount;
extern qboolean msg_badread;			// set if a read goes beyond end of

										// message

void        MSG_BeginReading (void);
int         MSG_GetReadCount (void);
int         MSG_ReadChar (void);
int         MSG_ReadByte (void);
int         MSG_ReadShort (void);
int         MSG_ReadLong (void);
float       MSG_ReadFloat (void);
char       *MSG_ReadString (void);
char       *MSG_ReadStringLine (void);

float       MSG_ReadCoord (void);
float       MSG_ReadAngle (void);
float       MSG_ReadAngle16 (void);
void        MSG_ReadDeltaUsercmd (struct usercmd_s *from,
								  struct usercmd_s *cmd);

//============================================================================

int         Q_atoi (char *str);
float       Q_atof (char *str);


//============================================================================

extern char com_token[1024];
extern qboolean com_eof;

char       *COM_Parse (char *data);


extern int  com_argc;
extern char **com_argv;

int         COM_CheckParm (char *parm);
void        COM_AddParm (char *parm);

void        COM_Init_Cvars (void);
void        COM_Init (void);
void        COM_InitArgv (int argc, char **argv);

char       *COM_SkipPath (char *pathname);
void        COM_StripExtension (char *in, char *out);
void        COM_FileBase (char *in, char *out);
void        COM_DefaultExtension (char *path, char *extension);

char       *va (char *format, ...);

// does a varargs printf into a temp buffer


//============================================================================

extern int  com_filesize;
struct cache_user_s;

extern char com_gamedir[MAX_OSPATH];

void        COM_WriteFile (char *filename, void *data, int len);
int         COM_FOpenFile (char *filename, FILE ** file);
void        COM_CloseFile (FILE * h);

Uint8      *COM_LoadStackFile (char *path, void *buffer, int bufsize);
Uint8      *COM_LoadTempFile (char *path);
Uint8      *COM_LoadHunkFile (char *path);
void        COM_LoadCacheFile (char *path, struct cache_user_s *cu);
void        COM_CreatePath (char *path);
void        COM_Gamedir (char *dir);

extern struct cvar_s *registered;
extern qboolean standard_quake, rogue, hipnotic;

char       *Info_ValueForKey (char *s, char *key);
void        Info_RemoveKey (char *s, char *key);
void        Info_RemovePrefixedKeys (char *start, char prefix);
void        Info_SetValueForKey (char *s, char *key, char *value, int maxsize);
void        Info_SetValueForStarKey (char *s, char *key, char *value,
									 int maxsize);
void        Info_Print (char *s);

Uint8       COM_BlockSequenceCheckByte (Uint8 *base, int length, int sequence,
										unsigned mapchecksum);
Uint8       COM_BlockSequenceCRCByte (Uint8 *base, int length, int sequence);

int         build_number (void);

#endif // __COMMON_H

