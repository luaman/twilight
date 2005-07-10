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

#ifndef __MENU_H
#define __MENU_H

#include "qtypes.h"
#include "cvar.h"
#include "lh_parser.h"
#include "fs/wad.h"
#include "mathlib.h"
#include "strlib.h"
#include "image/image.h"

typedef enum {
	m_command, m_slider, m_toggle, m_multi_select, m_text_entry, m_text,
	m_step_float, m_loop_int, m_img,
} item_type_t;

typedef struct menu_img_trans_s {
	cvar_t		*from_cvar;
	int			 from_shift, from_bits, from;
	cvar_t		*to_cvar;
	int			 to_shift, to_bits, to;
} menu_img_trans_t;

typedef struct menu_item_img_s {
	image_t				*img;
	int					x, y;		// FIXME: HACK HACK HACK!
	menu_img_trans_t	*trans;
	int					n_trans;
} menu_item_img_t;

typedef struct menu_item_loop_int_s {
	cvar_t		*cvar;
	int			 max, shift, bits;
} menu_item_loop_int_t;

typedef struct menu_item_step_float_s {
	cvar_t		*cvar;
	float		 min, step;
	qboolean	 bound;	
} menu_item_step_float_t;

typedef struct menu_item_slider_s {
	cvar_t	*cvar;
	float	 min, max, step;
} menu_item_slider_t;

typedef struct multi_item_s {
	char	*value;
	char	*label;
} multi_item_t;

typedef struct menu_item_multi_s {
	cvar_t			*cvar;
	multi_item_t	*values;
	int				 n_values;
} menu_item_multi_t;

typedef struct menu_item_text_entry_s {
	cvar_t	*cvar;
	int		 max_len;
	char	 min_valid, max_valid;
} menu_item_text_entry_t;

#define MITEM_SELECTABLE			BIT(0)
#define MITEM_DRAW					BIT(1)
#define MITEM_DRAW_LABEL			BIT(2)
#define MITEM_DRAW_VALUE			BIT(3)

typedef enum {
	c_none, c_grey, c_kill, c_kill_load,
} control_type_t;

typedef struct menu_control_s {
	cvar_t			*cvar;
	char			*svalue;
	int				 ivalue;
	qboolean		 invert;
	control_type_t	 type;
} menu_control_t;

typedef struct menu_item_s {
	char			*label;
	int				 n_control;
	menu_control_t	*control;
	int				 flags;
	int				 height;	// FIXME: HACK HACK HACK!
	item_type_t	 type;
	union {
		menu_item_slider_t		 slider;
		char					*command;
		cvar_t					*toggle;
		char					*text;
		menu_item_multi_t		 multi;
		menu_item_text_entry_t	 text_entry;
		menu_item_step_float_t	 step_float;
		menu_item_loop_int_t	 loop_int;
		menu_item_img_t		 img;
	} u;
} menu_item_t;

typedef struct menu_s {
	char			*id, *title;
	char			*on_enter, *on_exit;
	menu_item_t		**items;
	int				 item;
	struct menu_s	*next;
} menu_t;

extern menu_t		*m_menu;
extern qboolean		m_entersound;


void M_DrawImg(int x, int y, image_t *img);
void M_DrawTextBox(int x, int y, int width, int lines);
void M_SetKeyDest(void);
void M_ToggleMenu_f(void);
void M_Draw(void);
void M_Keydown(int key, char ascii);
void M_Renderer_Init_Cvars(void);
void M_Renderer_Init(void);

void Menu_Delete_Item(menu_item_t *item);
void Menu_Delete_Menu(menu_t *menu);
void M_Exit(qboolean new_menu);
void M_Base_Init_Cvars(void);
void M_Base_Init(void);

void M_Init_Cvars(void);
void M_Init(void);

extern inline control_type_t
MItem_Control (menu_item_t *item)
{
	cvar_t			*cvar;
	int				 i;
	qboolean		 match;
	control_type_t	 worst = c_none;

	for (i = 0; i < item->n_control; i++) {
		match = false;
		cvar = item->control[i].cvar;

		if (item->control[i].svalue) {
			if (!strcasecmp(item->control[i].svalue, cvar->svalue))
				match = true;
		} else
			if (cvar->ivalue == item->control[i].ivalue)
				match = true;

		if ((!item->control[i].invert && !match) ||
				(item->control[i].invert && match))
			if (worst < item->control[i].type)
				worst = item->control[i].type;
	}

	return worst;
}

extern inline qboolean
MItem_Selectable (menu_item_t *item)
{
	if (!(item->flags & MITEM_SELECTABLE))
		return 0;
	if (MItem_Control(item) != c_none)
		return 0;
	return 1;
}

extern inline int
MItem_Draw (menu_item_t *item)
{
	control_type_t	state;

	if (!(item->flags & MITEM_DRAW))
		return 0;
	state = MItem_Control(item);
	if (state == c_none)
		return 1;
	else if (state == c_grey)
		return 2;
	else if (state == c_kill)
		return 0;
	else
		return 1;
}

extern inline int
MItem_Draw_Label (menu_item_t *item)
{
	control_type_t	state;

	if (!(item->flags & MITEM_DRAW_LABEL))
		return 0;
	if (!item->label)
		return 0;

	state = MItem_Control(item);
	if (state == c_none)
		return 1;
	else if (state == c_grey)
		return 2;
	else if (state == c_kill)
		return 0;
	else
		return 1;
}

extern inline int
MItem_Draw_Value (menu_item_t *item)
{
	control_type_t	state;
	qboolean		good;

	if (!(item->flags & MITEM_DRAW_VALUE))
		return 0;
	switch (item->type) {
		case m_toggle:
			good = !!item->u.toggle;
		case m_text:
			good = !!item->u.text;
		case m_img:
			good = !!item->u.img.img;
		case m_loop_int:
			good = !!item->u.loop_int.cvar;
		case m_step_float:
			good = !!item->u.step_float.cvar;
		case m_slider:
			good = !!item->u.slider.cvar;
		case m_multi_select:
			good = !!item->u.multi.cvar;
		case m_text_entry:
			good = !!item->u.text_entry.cvar;
		default:
			good = 1;
	}
	if (!good)
		return 0;

	state = MItem_Control(item);
	if (state == c_none)
		return 1;
	else if (state == c_grey)
		return 2;
	else if (state == c_kill)
		return 0;
	else
		return 1;
}


#endif // __MENU_H

