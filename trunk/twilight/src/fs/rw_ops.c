/*
	$RCSfile$

	Copyright (C) 2003  Zephaniah E. Hull.

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

#include <sys/types.h>
#include <sys/stat.h>
	
#include "common.h"
#include "strlib.h"
#include "qtypes.h"
#include "fs.h"
#include "fs_hash.h"
#include "rw_ops.h"

typedef struct {
	SDL_RWops	*base;
	int			start, end, cur, size;
} limit_t;

static int
limit_seek (SDL_RWops *rw, int offset, int whence)
{
	limit_t	*limit = rw->hidden.unknown.data1;
	int		where;

	switch (whence) {
		case SEEK_SET:
			where = limit->start + offset;
			break;
		case SEEK_CUR:
			where = SDL_RWseek (limit->base, offset, whence);
			if (where > limit->end)
				where = limit->end;
			else if (where < limit->start)
				where = limit->start;
			else
				return where - limit->start;
			break;
		case SEEK_END:
			where = limit->end + offset;
			break;
		default:
			return -1;
	}
	if (where > limit->end)
		where = limit->end;
	else if (where < limit->start)
		where = limit->start;

	where = SDL_RWseek (limit->base, where, SEEK_SET);
	return limit->cur = (where - limit->start);
}

static int
limit_read (SDL_RWops *rw, void *ptr, int size, int maxnum)
{
	limit_t	*limit = rw->hidden.unknown.data1;
	int		num = maxnum;

	if ((limit->cur + (size * num)) > limit->size)
		num = (limit->size - limit->cur) / size;
	num = SDL_RWread (limit->base, ptr, size, num);
	limit->cur += size * num;
	return num;
}

static int
limit_write (SDL_RWops *rw, const void *ptr, int size, int num)
{
	rw = rw;
	ptr = ptr;
	size = size;
	num = num;

	return -1;
}

static int
limit_close (SDL_RWops *rw)
{
	limit_t	*limit = rw->hidden.unknown.data1;

	SDL_RWclose (limit->base);
	Zone_Free (limit);
	Zone_Free (rw);

	return 0;
}

SDL_RWops *
LimitFromRW (SDL_RWops *rw, int start, int end)
{
	SDL_RWops	*new;
	limit_t		*limit;

	new = Zone_Alloc (fs_zone, sizeof (SDL_RWops));
	limit = Zone_Alloc (fs_zone, sizeof (limit_t));
	new->hidden.unknown.data1 = limit;
	
	new->seek = limit_seek;
	new->read = limit_read;
	new->write = limit_write;
	new->close = limit_close;

	limit->base = rw;

	limit->start = start;
	limit->end = end;
	limit->size = end - start;

	SDL_RWseek (new, 0, SEEK_SET);

	return new;
}


int
Wrap_Seek (SDL_RWops *rw, int offset, int whence)
{
	rw_wrap_t	*wrap = rw->hidden.unknown.data1;

	return SDL_RWseek (wrap->rw, offset, whence);
}

int
Wrap_Read (SDL_RWops *rw, void *ptr, int size, int maxnum)
{
	rw_wrap_t	*wrap = rw->hidden.unknown.data1;

	return SDL_RWread (wrap->rw, ptr, size, maxnum);
}

int
Wrap_Write (SDL_RWops *rw, const void *ptr, int size, int num)
{
	rw_wrap_t	*wrap = rw->hidden.unknown.data1;

	return SDL_RWwrite (wrap->rw, ptr, size, num);
}

int
Wrap_Close (SDL_RWops *rw)
{
	rw_wrap_t	*wrap = rw->hidden.unknown.data1;

	if (wrap->close)
		wrap->close (wrap->rw, wrap->data);
	else
		SDL_RWclose (wrap->rw);
	Zone_Free (wrap);
	Zone_Free (rw);

	return 0;
}

SDL_RWops *
WrapRW (SDL_RWops *rw, void *data, wrap_close_t *close)
{
	rw_wrap_t	*wrap;
	SDL_RWops	*new;

	new = Zone_Alloc (fs_zone, sizeof (SDL_RWops));
	wrap = Zone_Alloc (fs_zone, sizeof (rw_wrap_t));
	
	new->hidden.unknown.data1 = wrap;
	wrap->rw = rw;
	wrap->data = data;
	if (close)
		wrap->close = close;

	new->seek = Wrap_Seek;
	new->read = Wrap_Read;
	new->write = Wrap_Write;
	new->close = Wrap_Close;

	return new;
}


#include <stdarg.h>
int
RWprintf (SDL_RWops *rw, const char *format, ...)
{
	size_t length;
	va_list argptr;
	char *p;
	char text[4096] = {0};
	int n;

	va_start (argptr, format);
	length = vsnprintf (text, sizeof (text) - 1, format, argptr);
	// note: assumes Zone_Alloc clears memory to zero
	if (length > sizeof(text)) {
		p = Zone_Alloc (tempzone, length + 1);
		length = vsprintf (p, format, argptr);
		n = SDL_RWwrite (rw, p, length, 1);
		Zone_Free (p);
	} else
		n = SDL_RWwrite (rw, text, length, 1);
	va_end (argptr);

	return n;
}
