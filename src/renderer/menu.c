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
static const char rcsid[] =
    "$Id$";

#include "twiconfig.h"

#include <stdio.h>

#include "quakedef.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "gl_draw.h"
#include "host.h"
#include "keys.h"
#include "mathlib.h"
#include "menu.h"
#include "strlib.h"
#include "sys.h"
#include "wad.h"
#include "video.h"
#include "lh_parser.h"
#include "common.h"
#include "gl_info.h"
#include "sound.h"
#include "cclient.h"
#include "gl_main.h"

/*
================
Draws one solid graphics character
================
*/
static void
M_DrawCharacter (int cx, int line, int num)
{
	Draw_Character (cx + ((vid.width_2d - 320) >> 1), line, num, 8);
}

static void
M_Print (float cx, float cy, char *str)
{
	Draw_String (cx + ((vid.width_2d - 320) >> 1), cy, str, 8);
}

static void
M_PrintAlt (float cx, float cy, char *str)
{
	Draw_Alt_String (cx + ((vid.width_2d - 320) >> 1), cy, str, 8);
}

void
M_DrawImg (int x, int y, image_t *img)
{
	Draw_Img (x + ((vid.width_2d - 320) >> 1), y, img);
}


void
M_DrawTextBox (int x, int y, int width, int lines)
{
	image_t     *p;
	int         cx, cy;
	int         n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CacheImg ("gfx/box_tl");
	M_DrawImg (cx, cy, p);
	p = Draw_CacheImg ("gfx/box_ml");
	for (n = 0; n < lines; n++) {
		cy += 8;
		M_DrawImg (cx, cy, p);
	}
	p = Draw_CacheImg ("gfx/box_bl");
	M_DrawImg (cx, cy + 8, p);

	// draw middle
	cx += 8;
	while (width > 0) {
		cy = y;
		p = Draw_CacheImg ("gfx/box_tm");
		M_DrawImg (cx, cy, p);
		p = Draw_CacheImg ("gfx/box_mm");
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = Draw_CacheImg ("gfx/box_mm2");
			M_DrawImg (cx, cy, p);
		}
		p = Draw_CacheImg ("gfx/box_bm");
		M_DrawImg (cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CacheImg ("gfx/box_tr");
	M_DrawImg (cx, cy, p);
	p = Draw_CacheImg ("gfx/box_mr");
	for (n = 0; n < lines; n++) {
		cy += 8;
		M_DrawImg (cx, cy, p);
	}
	p = Draw_CacheImg ("gfx/box_br");
	M_DrawImg (cx, cy + 8, p);
}

//=============================================================================

#define	SLIDER_RANGE	10

static void
M_DrawSlider (int x, int y, int width, float min, float max, float step,
		float value)
{
	int	i;
	float units, where;

	units = (max - min) / step;
	where = (value - min) / step;
	where /= units;
	where *= width - 2;
	where = bound(0, where, width - 2);

	M_DrawCharacter (x, y, 128);
	x += 8;
	for (i = 0; i < (width - 2); i++)
		M_DrawCharacter (x + i * 8, y, 129);
	M_DrawCharacter (x + (i * 8), y, 130);
	M_DrawCharacter (x + (where * 8), y, 131);
}

static void
M_DrawCheckbox (int x, int y, int on)
{
//	M_Print (x, y, (on) ? "on" : "off");
	M_Print (x, y, (on) ? "[*]" : "[ ]");
}

static Uint8	trans_table_ident[256];
static Uint8	trans_table_cur[256];

static void
M_DrawTransImgTranslate (int x, int y, image_t *img)
{
	Draw_TransImgTranslate (x + ((vid.width_2d - 320) >> 1), y, img,
			trans_table_cur);
}

static void
M_Trans_Table_New (void)
{
	int i;
	for (i = 0; i < 256; i++)
		trans_table_cur[i] = trans_table_ident[i];
}

static void
M_Trans_Table_Trans (int from, int to)
{
	int		i;
	Uint8	*source = trans_table_ident, *dest = trans_table_cur;

	from *= 16;
	to *= 16;

	if ((((from < 128) && (to < 128))) || ((from >= 128) && (to >= 128))) {
		memcpy(dest + to, source + from, 16);
	} else {
		for (i = 0; i < 16; i++)
			dest[to + i] = source[from + (15 - i)];
	}
}

//=============================================================================
/* Menu Subsystem */

void
M_SetKeyDest (void)
{
	if (m_menu)
		key_dest = key_menu;
	else if (r.worldmodel)
		key_dest = key_game;
	else
		key_dest = key_console;
}

void
M_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest == key_menu) {
		if (!m_menu) {	// XXX: IMPOSSIBLE! :XXX
			Com_Printf("IMPOSSIBLE at %s %d (%s)\n",
					__FILE__, __LINE__, __FUNCTION__);
			M_SetKeyDest ();
			return;
		}

		M_Exit (false);
		M_SetKeyDest ();
		return;
	} else if (key_dest == key_console && r.worldmodel)
		Con_ToggleConsole_f ();
	else
		Cbuf_InsertText("menu Main\n");
}

static void
M_Do_Draw (menu_t *menu, int current)
{
	int			 i, x, div_x, len;
	float		 y, y_add;
	menu_item_t	*item;

	x = 8;
	div_x = x + (18 * 8);
	M_Print(x, 4, menu->title);

	y = 32;

	for (i = 0; menu->items[i]; i++, y += y_add) {
		item = menu->items[i];
		y_add = 0;
		if (!MItem_Draw(item))
			continue;

		if (item->height) {
			y += item->height / 2;
			y_add = item->height;
		}

#define PULSE		(sin(host.time * 5) * 0.2)

		if (MItem_Draw_Label(item)) {
			len = strlen(item->label);
			x = div_x - ((len + 1) * 8);
			M_PrintAlt(x, y, item->label);
			if (i == current) {

				M_PrintAlt(div_x - 8, y, "<=>");
				qglBlendFunc (GL_ONE, GL_SRC_ALPHA);
				qglColor4f(1, 1, 1, 0.8 + PULSE);
				M_PrintAlt(x, y, item->label);
				qglColor4f(1, 1, 1, 1);
				qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			} else {
				M_PrintAlt(div_x, y, "|");
			}
		}

		if (!MItem_Draw_Value(item))
			continue;

		switch (item->type) {
			case m_command:
				break;
			case m_loop_int:
				{
					menu_item_loop_int_t	*loop = &item->u.loop_int;
					int						 n, bit_mask;

					bit_mask = (BIT(loop->bits) - 1) << loop->shift;
					n = (loop->cvar->ivalue & bit_mask) >> loop->shift;
					M_DrawSlider (div_x + 16, y, 12, 0, loop->max, 1, n);
					M_PrintAlt(div_x + 16 + (8 * 13), y, va("%d", n));
				}
				break;
			case m_slider:
				M_DrawSlider (div_x + 16, y, 12,
						item->u.slider.min, item->u.slider.max,
						item->u.slider.step, item->u.slider.cvar->fvalue);
				M_PrintAlt(div_x + 16 + (8 * 13), y,
						va("%g", item->u.slider.cvar->fvalue));
				break;
			case m_toggle:
				M_DrawCheckbox (div_x + 16, y, item->u.toggle->ivalue);
				break;
			case m_text:
				M_Print(div_x + 16, y, item->u.text);
				break;
			case m_step_float:
				M_Print(div_x + 16, y,va("%g",item->u.step_float.cvar->fvalue));
				break;
			case m_multi_select:
				{
					menu_item_multi_t	*multi = &item->u.multi;
					cvar_t				*cvar = multi->cvar;
					char				*cur = cvar->svalue;
					int					 j;
					qboolean			 found = false;

					M_Print(div_x + 16, y, "]");

					for (j = 0; j < multi->n_values; j++) {
						if (!strcasecmp(cur, multi->values[j].value)) {
							if (multi->values[j].label)
								M_Print(div_x + 28, y, multi->values[j].label);
							else
								M_Print(div_x + 28, y, cur);
							found = true;
							break;
						}
					}

					if (!found)
						M_Print(div_x + 28, y, cur);
				}
				break;
			case m_text_entry:
				M_Print(div_x + 16, y, "[");
				M_Print(div_x + 28, y, item->u.text_entry.cvar->svalue);
				M_Print(div_x + 32 + (8 * item->u.text_entry.max_len), y, "]");
				break;
			case m_img:
				{
					int					j, from, to;
					menu_item_img_t		*img = &item->u.img;
					menu_img_trans_t	*trans;

					M_Trans_Table_New ();
					for (j = 0; j < img->n_trans; j++) {
						trans = &img->trans[j];
						if (trans->from_cvar) from = trans->from_cvar->ivalue;
						else from = trans->from;
						from >>= trans->from_shift;
						from &= BIT(trans->from_bits) - 1;

						if (trans->to_cvar) to = trans->to_cvar->ivalue;
						else to = trans->to;
						to >>= trans->to_shift;
						to &= BIT(trans->to_bits) - 1;

						M_Trans_Table_Trans(from, to);
					}
					M_DrawTransImgTranslate(div_x + img->x, y + img->y, img->img);
				}
				break;
		}
	}
}

void
M_Draw (void)
{
	extern float scr_con_current;

	if (!m_menu)
		return;
	if (key_dest != key_menu)
		return;

	if (scr_con_current) {
		Draw_ConsoleBackground (vid.height_2d);
		S_ExtraUpdate ();
	} else
		Draw_FadeScreen ();

	if (m_entersound) {
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	S_ExtraUpdate ();

	M_Do_Draw (m_menu, m_menu->item);
}


static void
M_Handle_Key (menu_item_t *item, int key)
{
	S_LocalSound ("misc/menu3.wav");
	switch (item->type) {
		case m_command:
			switch (key) {
				case K_ENTER:
				case K_LEFTARROW:
				case K_RIGHTARROW:
					Cbuf_InsertText(item->u.command);
					break;
				default:
				break;
			}
			break;
		case m_toggle:
			switch (key) {
				case K_ENTER:
				case K_LEFTARROW:
				case K_RIGHTARROW:

				Cvar_Set(item->u.toggle, va ("%d", !item->u.toggle->ivalue));
				break;

				default: break;
			}
			break;
		case m_slider:
			{
				menu_item_slider_t	*slider = &item->u.slider;
				cvar_t				*cvar = slider->cvar;
				float				tmp = cvar->fvalue;

				switch (key) {
					case K_LEFTARROW:
						tmp -= slider->step;
						break;
					case K_RIGHTARROW:
						tmp += slider->step;
						break;

					default: break;
				}
				tmp = bound(slider->min, tmp, slider->max);
				Cvar_Set(cvar, va("%f", tmp));
			}
			break;
		case m_step_float:
			{
				menu_item_step_float_t	*step = &item->u.step_float;
				cvar_t					*cvar = step->cvar;
				float					 tmp = cvar->fvalue;

				switch (key) {
					case K_LEFTARROW:
						tmp -= step->step;
						break;
					case K_RIGHTARROW:
						if (tmp < step->min)
							tmp = step->min;
						else
							tmp += step->step;
						break;

					default: break;
				}
				if (!step->bound)
					if (tmp < step->min)
						tmp = -1;
				Cvar_Set(cvar, va("%f", tmp));
			}
			break;
		case m_loop_int:
			{
				menu_item_loop_int_t	*loop = &item->u.loop_int;
				cvar_t					*cvar = loop->cvar;
				int						 value, tmp, bit_mask;

				value = cvar->ivalue;
				bit_mask = (BIT(loop->bits) - 1) << loop->shift;
				tmp = (value & bit_mask) >> loop->shift;
				value &= ~bit_mask;

				switch (key) {
					case K_LEFTARROW:
						tmp--;
						break;
					case K_RIGHTARROW:
						tmp++;
						break;

					default: break;
				}
				if (tmp < 0)
					tmp = loop->max;
				if (tmp > loop->max)
					tmp = 0;

				value |= tmp << loop->shift;

				Cvar_Set(cvar, va("%d", value));
			}
			break;
		case m_multi_select:
			{
				menu_item_multi_t		*multi = &item->u.multi;
				cvar_t					*cvar = multi->cvar;
				char					*cur = cvar->svalue;
				int						 i;
				qboolean				 found = false;

				i = 0;
				while (1) {
					if (!strcasecmp(cur, multi->values[i].value)) {
						found = true;
						break;
					}
					if (++i >= multi->n_values)
						break;
				}

				if (!found)
					i = 0;

				switch (key) {
					case K_LEFTARROW:
						i--;
						break;
					case K_ENTER:
					case K_RIGHTARROW:
						i++;
						break;
					default: break;
				}

				if (i < 0) {
					i = multi->n_values - 1;
				}

				if (i >= multi->n_values)
					i = 0;

				Cvar_Set (cvar, multi->values[i].value);
			}
			break;
		case m_text_entry:
			{
				int						 i;
				char					*str;
				menu_item_text_entry_t	*text = &item->u.text_entry;

				i = text->cvar->s_len;
				if ((key >= text->min_valid) && (key <= text->max_valid)) {
					if ((i + 1) <= text->max_len)
						Cvar_Set(text->cvar, va("%s%c",text->cvar->svalue,key));
				} else if (key == K_BACKSPACE || key == K_LEFTARROW) {
					if (i >= 1) {
						str = strdup(text->cvar->svalue);
						str[i - 1] = '\0';
						Cvar_Set(text->cvar, str);
						free(str);
					}
				}
			}
			break;
		case m_text:
		case m_img:
			break;
	}
}

void
M_Keydown (int key)
{
	menu_t		*menu = m_menu;
	menu_item_t	*item = menu->items[menu->item];
	int			 start;

	if (!menu)
		return;

	switch (key) {
		case K_ESCAPE:
			M_ToggleMenu_f ();
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			start = menu->item;
down_start:
			if (!menu->items[++menu->item])
				menu->item = 0;
			if ((menu->item != start) &&
					!MItem_Selectable(menu->items[menu->item]))
				goto down_start;
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			start = menu->item;
up_start:
			if (--menu->item < 0) {
				while (menu->items[++menu->item]);
				menu->item--;
			}
			if ((menu->item != start) &&
					!MItem_Selectable(menu->items[menu->item]))
				goto up_start;
			break;

		default:
			M_Handle_Key (item, key);
			break;
	}
}

void
M_Renderer_Init_Cvars(void)
{
}

void
M_Renderer_Init (void)
{
	int i;

	for (i = 0; i < 256; i++)
		trans_table_ident[i] = i;

	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);
}
