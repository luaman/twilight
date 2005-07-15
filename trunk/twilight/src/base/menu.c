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
#include "renderer/draw.h"
#include "host.h"
#include "keys.h"
#include "mathlib.h"
#include "menu.h"
#include "strlib.h"
#include "sys.h"
#include "fs/wad.h"
#include "lh_parser.h"
#include "common.h"


static int			menu_errors;
menu_t		*m_menu;
static menu_t		*m_first;
static memzone_t	*m_zone;
qboolean	m_entersound;

void
Menu_Delete_Item (menu_item_t *item)
{
	int i;

	if (!item)
		return;
	if (item->label)
		Zone_Free (item->label);
	if (item->control)
		Zone_Free (item->control);

	switch (item->type) {
		case m_command:
			if (item->u.command)
				Zone_Free (item->u.command);
			break;
		case m_multi_select:
			if (item->u.multi.values) {
				for (i = 0; i < item->u.multi.n_values; i++) {
					if (item->u.multi.values[i].value)
						Zone_Free(item->u.multi.values[i].value);
					if (item->u.multi.values[i].label)
						Zone_Free(item->u.multi.values[i].label);
				}
				Zone_Free(item->u.multi.values);
			}
			break;
		case m_img:
			if (item->u.img.trans)
				Zone_Free(item->u.img.trans);
			break;
		default:
			break;
	}
}

void
Menu_Delete_Menu (menu_t *menu)
{
	int i;

	if (!menu)
		return;
	if (menu->id)
		Zone_Free (menu->id);
	if (menu->title)
		Zone_Free (menu->title);
	if (!menu->items)
		return;
	for (i = 0; menu->items[i]; i++)
		Menu_Delete_Item(menu->items[i]);

	Zone_Free(menu->items);
}

static qboolean
Menu_Parse_Img_Trans (codetree_t *tree_base, menu_img_trans_t *trans)
{
	codetree_t			*code;
	codeword_t			*word, *word2;

#define MENU_ERROR()		do {												\
	Com_Printf("ERROR: Parse error when building img trans. (%d %d)\n", __LINE__, code->linenumber);	\
	return false;														\
} while (0)

    while (tree_base->linenumber < 0 && tree_base->child)
	        tree_base = tree_base->child;

	for (code = tree_base; code; code = code->next) {
		if (!(word = code->words)) MENU_ERROR ();
		if (!(word2 = word->next)) MENU_ERROR ();
		if (!strcmp(word->string, "from") && !code->child) {
			if (word2 && (word2->flags & WORDFLAG_STRING))
				trans->from_cvar = Cvar_Find(word2->string);
			else if (word2 && (word2->flags & WORDFLAG_INTEGER))
				trans->from = word2->intvalue;
			else MENU_ERROR ();
		} else if (!strcmp(word->string, "from_bits") && !code->child) {
			if (word2 && (word2->flags & WORDFLAG_INTEGER))
				trans->from_bits = word2->intvalue;
			else MENU_ERROR ();
		} else if (!strcmp(word->string, "from_shift") && !code->child) {
			if (word2 && (word2->flags & WORDFLAG_INTEGER))
				trans->from_shift = word2->intvalue;
			else MENU_ERROR ();
		} else if (!strcmp(word->string, "to") && !code->child) {
			if (word2 && (word2->flags & WORDFLAG_STRING))
				trans->to_cvar = Cvar_Find(word2->string);
			else if (word2 && (word2->flags & WORDFLAG_INTEGER))
				trans->to = word2->intvalue;
			else MENU_ERROR ();
		} else if (!strcmp(word->string, "to_bits") && !code->child) {
			if (word2 && (word2->flags & WORDFLAG_INTEGER))
				trans->to_bits = word2->intvalue;
			else MENU_ERROR ();
		} else if (!strcmp(word->string, "to_shift") && !code->child) {
			if (word2 && (word2->flags & WORDFLAG_INTEGER))
				trans->to_shift = word2->intvalue;
			else MENU_ERROR ();
		} else
			MENU_ERROR ();
	}
#undef MENU_ERROR
	return true;
}

static menu_item_t *
Menu_Parse_Item (const char *type, codetree_t *tree_base)
{
	menu_item_t		*item;
	codetree_t		*code = tree_base;
	codeword_t		*word, *word2, *word3, *word4, *word5;
	int				i;

	item = Zone_Alloc(m_zone, sizeof(menu_item_t));
#define MENU_ERROR()		do {												\
	Com_Printf("ERROR: Parse error when building menu item. (%d %d)\n", __LINE__, code->linenumber);	\
	menu_errors++;														\
	Menu_Delete_Item(item);												\
	return NULL;														\
} while (0)

    while (tree_base->linenumber < 0 && tree_base->child)
	        tree_base = tree_base->child;

	if (!strcmp(type, "command")) {
		item->type = m_command;
	} else if (!strcmp(type, "toggle")) {
		item->type = m_toggle;
	} else if (!strcmp(type, "slider")) {
		item->type = m_slider;
	} else if (!strcmp(type, "text_entry")) {
		item->type = m_text_entry;
		item->u.text_entry.min_valid = 0x20;
		item->u.text_entry.max_valid = 0x7E;
	} else if (!strcmp(type, "text")) {
		item->type = m_text;
	} else if (!strcmp(type, "step_float")) {
		item->type = m_step_float;
	} else if (!strcmp(type, "loop_int")) {
		item->type = m_loop_int;
	} else if (!strcmp(type, "multi_select")) {
		item->type = m_multi_select;
	} else if (!strcmp(type, "qpic") || !strcmp(type, "img")) {
		item->type = m_img;
	} else
		MENU_ERROR();

	item->flags = MITEM_SELECTABLE | MITEM_DRAW | MITEM_DRAW_LABEL
				| MITEM_DRAW_VALUE;
	item->height = 8;

	for (code = tree_base; code; code = code->next) {
		if (!(word = code->words)) MENU_ERROR ();
		if ((word2 = word->next))
			if ((word3 = word->next))
				if ((word4 = word->next))
					word5 = word->next;
		if (!strcmp(word->string, "label") && !code->child) {
			if (word2 && (word2->flags & WORDFLAG_STRING)) {
				item->label = Zstrdup(m_zone, word2->string);
			} else MENU_ERROR ();
		} else if (!strcmp(word->string, "control") && !code->child) {
			item->n_control++;
		} else if (!strcmp(word->string, "flags") && !code->child) {
			for (; word2; word2 = word2->next) {
				if (!strcmp(word2->string, "selectable"))
					item->flags |= MITEM_SELECTABLE;
				else if (!strcmp(word2->string, "~selectable"))
					item->flags &= ~MITEM_SELECTABLE;
				else if (!strcmp(word2->string, "draw"))
					item->flags |= MITEM_DRAW;
				else if (!strcmp(word2->string, "~draw"))
					item->flags &= ~MITEM_DRAW;
				else if (!strcmp(word2->string, "draw_label"))
					item->flags |= MITEM_DRAW_LABEL;
				else if (!strcmp(word2->string, "~draw_label"))
					item->flags &= ~MITEM_DRAW_LABEL;
				else if (!strcmp(word2->string, "draw_value"))
					item->flags |= MITEM_DRAW_VALUE;
				else if (!strcmp(word2->string, "~draw_value"))
					item->flags &= ~MITEM_DRAW_VALUE;
				else
					MENU_ERROR ();
			}
		} else if (!strcmp(word->string, "height") && !code->child) {
			if (word2 && (word2->flags & WORDFLAG_INTEGER))
				item->height = word2->intvalue;
			else MENU_ERROR ();
		} else {
			switch (item->type) {
				case m_command:
					if (!strcmp(word->string, "command") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_STRING))
							item->u.command = Zstrdup(m_zone, word2->string);
						else MENU_ERROR ();
					} else MENU_ERROR ();
					break;
				case m_toggle:
					if (!strcmp(word->string, "cvar") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_STRING))
							item->u.toggle = Cvar_Find(word2->string);
						else MENU_ERROR ();
					} else MENU_ERROR ();
					break;
				case m_slider:
					if (!strcmp(word->string, "cvar") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_STRING))
							item->u.slider.cvar = Cvar_Find(word2->string);
						else MENU_ERROR ();
					} else if (!strcmp(word->string, "min") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_DOUBLE))
							item->u.slider.min = word2->doublevalue;
						else MENU_ERROR ();
					} else if (!strcmp(word->string, "max") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_DOUBLE))
							item->u.slider.max = word2->doublevalue;
						else MENU_ERROR ();
					} else if (!strcmp(word->string, "step") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_DOUBLE))
							item->u.slider.step = word2->doublevalue;
						else MENU_ERROR ();
					} else MENU_ERROR ();
					break;
				case m_text_entry:
					if (!strcmp(word->string, "cvar") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_STRING))
							item->u.text_entry.cvar = Cvar_Find(word2->string);
						else MENU_ERROR ();
					} else if (!strcmp(word->string,"max_len")&& !code->child) {
						if (word2 && (word2->flags & WORDFLAG_INTEGER))
							item->u.text_entry.max_len = word2->intvalue;
						else MENU_ERROR ();
					} else if (!strcmp(word->string,"min_valid")&&!code->child){
						if (word2 && (word2->flags & WORDFLAG_INTEGER))
							item->u.text_entry.min_valid = word2->intvalue;
						else MENU_ERROR ();
					} else if (!strcmp(word->string,"max_valid")&&!code->child){
						if (word2 && (word2->flags & WORDFLAG_INTEGER))
							item->u.text_entry.max_valid = word2->intvalue;
						else MENU_ERROR ();
					} else MENU_ERROR ();
					break;
				case m_multi_select:
					if (!strcmp(word->string, "cvar") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_STRING))
							item->u.multi.cvar = Cvar_Find(word2->string);
						else MENU_ERROR ();
					} else if (!strcmp(word->string,"value") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_STRING))
							item->u.multi.n_values++;
						else MENU_ERROR ();
					} else MENU_ERROR ();
					break;
				case m_img:
					if ((!strcmp(word->string, "qpic") || !strcmp(word->string, "img")) && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_STRING)) {
							if (!(item->u.img.img = Image_Load(va("gfx/%s", word2->string), TEX_UPLOAD | TEX_ALPHA | TEX_KEEPRAW)))
								MENU_ERROR ();
						} else MENU_ERROR ();
					} else if (!strcmp(word->string, "x") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_INTEGER))
							item->u.img.x = word2->intvalue;
						else MENU_ERROR ();
					} else if (!strcmp(word->string, "y") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_INTEGER))
							item->u.img.y = word2->intvalue;
						else MENU_ERROR ();
					} else if (!strcmp(word->string,"trans") && code->child)
						item->u.img.n_trans++;
					else MENU_ERROR ();
					break;
				case m_text:
					if (!strcmp(word->string, "text") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_STRING))
							item->u.text = Zstrdup(m_zone, word2->string);
						else MENU_ERROR ();
					} else MENU_ERROR ();
					break;
				case m_step_float:
					if (!strcmp(word->string, "cvar") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_STRING))
							item->u.step_float.cvar = Cvar_Find(word2->string);
						else MENU_ERROR ();
					} else if (!strcmp(word->string, "min") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_DOUBLE))
							item->u.step_float.min = word2->doublevalue;
						else MENU_ERROR ();
					} else if (!strcmp(word->string, "step") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_DOUBLE))
							item->u.step_float.step = word2->doublevalue;
						else MENU_ERROR ();
					} else if (!strcmp(word->string, "bound") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_INTEGER))
							item->u.step_float.bound = !!word2->intvalue;
						else MENU_ERROR ();
					} else MENU_ERROR ();
					break;
				case m_loop_int:
					if (!strcmp(word->string, "cvar") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_STRING))
							item->u.loop_int.cvar = Cvar_Find(word2->string);
						else MENU_ERROR ();
					} else if (!strcmp(word->string, "max") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_DOUBLE))
							item->u.loop_int.max = word2->intvalue;
						else MENU_ERROR ();
					} else if (!strcmp(word->string, "shift") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_DOUBLE))
							item->u.loop_int.shift = word2->intvalue;
						else MENU_ERROR ();
					} else if (!strcmp(word->string, "bits") && !code->child) {
						if (word2 && (word2->flags & WORDFLAG_INTEGER))
							item->u.loop_int.bits = word2->intvalue;
						else MENU_ERROR ();
					} else MENU_ERROR ();
					break;
			}
		}
	}

	/*
	 * Second pass.
	 */
	if (item->n_control) {
		item->control =
			Zone_Alloc(m_zone, sizeof(menu_control_t)*item->n_control);
		for (code = tree_base, i = 0; code; code = code->next) {
			if (!(word = code->words)) MENU_ERROR ();
			if (!strcmp(word->string, "control") && !code->child) {
				if ((!(word2 = word->next)) || word2->flags) MENU_ERROR ();
				if ((!(word3 = word2->next)) ||
						!(word3->flags & WORDFLAG_STRING))
					MENU_ERROR ();
				if ((!(word4 = word3->next)) ||
						!(word4->flags & (WORDFLAG_STRING | WORDFLAG_DOUBLE)))
					MENU_ERROR ();

				if ((word5 = word4->next) && (word5->flags & WORDFLAG_INTEGER))
					item->control[i].invert = !!word5->intvalue;

				if (!strcasecmp(word2->string, "grey"))
					item->control[i].type = c_grey;
				else if (!strcasecmp(word2->string, "kill"))
					item->control[i].type = c_kill;
				else if (!strcasecmp(word2->string, "kill_load"))
					item->control[i].type = c_kill_load;
				else
					MENU_ERROR ();

				if (!(item->control[i].cvar = Cvar_Find(word3->string)))
					MENU_ERROR ();

				if (word4->flags & WORDFLAG_INTEGER)
					item->control[i].ivalue = word4->intvalue;
				else
					item->control[i].svalue = Zstrdup(m_zone, word4->string);
				i++;
			}
		}
	}

	if (MItem_Control(item) == c_kill_load) {
		Menu_Delete_Item(item);
		return NULL;
	}

	switch (item->type) {
		case m_multi_select:
			if (!item->u.multi.n_values)
				MENU_ERROR ();

			item->u.multi.values = Zone_Alloc(m_zone,
					sizeof(multi_item_t) * item->u.multi.n_values);

			for (code = tree_base, i = 0; code; code = code->next) {
				if (!(word = code->words)) MENU_ERROR ();
				if (!(word2 = word->next)) MENU_ERROR ();
				word3 = word2->next;
				if (!strcmp(word->string,"value")&& !code->child) {
					if (word2 && (word2->flags & WORDFLAG_STRING)) {
						item->u.multi.values[i].value =
							Zstrdup(m_zone,word2->string);
						if (word3 && (word3->flags & WORDFLAG_STRING))
							item->u.multi.values[i].label =
								Zstrdup(m_zone,word3->string);
						i++;
					} else MENU_ERROR ();
				}
			}
			break;
		case m_img:
			if (!item->u.img.n_trans)
				break;

			item->u.img.trans = Zone_Alloc(m_zone,
					sizeof(menu_img_trans_t) * item->u.img.n_trans);

			for (code = tree_base, i = 0; code; code = code->next) {
				if (!(word = code->words)) MENU_ERROR ();
				if (!strcmp(word->string, "trans") && code->child) {
					if (!Menu_Parse_Img_Trans (code->child, &item->u.img.trans[i++]))
						MENU_ERROR ();
				}
			}
			break;
		default:
			break;
	}
#undef MENU_ERROR
	return item;
}

static void
Menu_Parse_Menu (codetree_t *tree_base)
{
	menu_t			*menu, *tmenu;
	int				 i = 0;
	codetree_t		*code = tree_base;
	codeword_t		*word;

	menu_errors = 0;

#define MENU_ERROR()		do {											\
	Com_Printf("ERROR: Parse error when building menu. (%d %d '%s')\n", __LINE__, code->linenumber, word->string);	\
	menu_errors++;													\
} while (0)

	menu = Zone_Alloc(m_zone, sizeof(menu_t));
    while (tree_base->linenumber < 0 && tree_base->child)
	        tree_base = code->child;

	for (code = tree_base; code; code = code->next) {
		word = code->words;
		if (!strcmp(word->string, "id") && !code->child) {
			if ((word = word->next) && word->flags & WORDFLAG_STRING)
				menu->id = Zstrdup(m_zone, word->string);
			else
				MENU_ERROR ();
		} else if (!strcmp(word->string, "title") && !code->child) {
			if ((word = word->next) && word->flags & WORDFLAG_STRING)
				menu->title = Zstrdup(m_zone, word->string);
			else
				MENU_ERROR ();
		} else if (!strcmp(word->string, "on_enter") && !code->child) {
			if ((word = word->next) && word->flags & WORDFLAG_STRING)
				menu->on_enter = Zstrdup(m_zone, word->string);
			else
				MENU_ERROR ();
		} else if (!strcmp(word->string, "on_exit") && !code->child) {
			if ((word = word->next) && word->flags & WORDFLAG_STRING)
				menu->on_exit = Zstrdup(m_zone, word->string);
			else
				MENU_ERROR ();
		} else if (!strcmp(word->string, "item") && code->child) {
			if ((word = word->next))
				i++;
		} else
			MENU_ERROR ();
	}

	menu->items = Zone_Alloc(m_zone, sizeof(menu_item_t *) * ++i);
	i = 0;

	for (code = tree_base; code; code = code->next) {
		word = code->words;
		if (!strcmp(word->string, "item") && code->child) {
			if ((word = word->next)) {
				menu->items[i] = Menu_Parse_Item (word->string, code->child);
				if (menu->items[i])
					i++;
			} else
				MENU_ERROR ();
		}
	}
	menu->items[i] = NULL;

	for (tmenu = m_first; tmenu; tmenu = tmenu->next) {
		if (!strcasecmp(menu->id, tmenu->id)) {
			Com_Printf("MENU_ERROR! Menu '%s' already defined!\n", menu->id);
			Menu_Delete_Menu(menu);
			return;
		}
	}

	menu->next = m_first;
	m_first = menu;
#undef MENU_ERROR
	if (menu_errors)
		LHP_printcodetree_c(1, tree_base);
}

static void
Menu_Parse_Menus (codetree_t *tree_base)
{
	codetree_t		*code = tree_base;
	codeword_t		*word;

#define MENU_ERROR()		do {											\
	Com_Printf("ERROR: Parse error when parsing menus. (%d %d '%s')\n", __LINE__, code->linenumber, word->string);	\
	return;															\
} while (0)

    while (tree_base->linenumber < 0 && tree_base->child)
	        tree_base = code->child;

	for (code = tree_base; code; code = code->next) {
		word = code->words;
		if (!strcmp(word->string, "menu") && code->child) {
			Menu_Parse_Menu (code->child);
		} else
			MENU_ERROR ();
	}
#undef MENU_ERROR
}

static void
M_Deletemenu_f (void)
{
	menu_t		*menu, **last;
	const char	*id;

	id = Cmd_Args ();
	last = &m_first;

	for (menu = m_first; menu; last = &menu->next, menu = menu->next) {
		if (!strcasecmp(id, menu->id)) {
			if (m_menu == menu)
				M_ToggleMenu_f ();

			*last = menu->next;
			Menu_Delete_Menu(menu);
			return;
		}
	}

	Com_Printf("ERROR: Menu %s not found!\n", id);
}

static void
M_Loadmenu_f (void)
{
	char		*menu_buffer;
	codetree_t	*menu_tree;

	if (Cmd_Argc() != 2) {
		Com_Printf("Not enough args.\n");
		return;
	}

	if (!(menu_buffer = (char *) COM_LoadTempFile(Cmd_Argv(1), true))) {
		Com_Printf("Could not load '%s'\n", Cmd_Argv(1));
		return;
	}
	if (!(menu_tree = LHP_parse(menu_buffer, "menu.mnu", m_zone))) {
		Com_Printf("Could not parse '%s'\n", Cmd_Argv(1));
		Zone_Free(menu_buffer);
		return;
	}
	Menu_Parse_Menus(menu_tree);
	LHP_freecodetree(menu_tree);
	Zone_Free(menu_buffer);
}

static void
M_First_Item (menu_t *menu)
{
	menu->item = 0;
	while(menu->items[menu->item] && !MItem_Selectable(menu->items[menu->item]))
		menu->item++;
	if (!menu->items[menu->item])
		menu->item = 0;
}

void
M_Exit (qboolean new_menu)
{
	if (!m_menu)
		return;

	if (!new_menu) {
		M_First_Item (m_menu);
		if (m_menu->on_exit)
			Cbuf_InsertText(m_menu->on_exit);
	}

	m_menu = NULL;
	M_SetKeyDest ();
}

static void
M_Enter (menu_t *menu)
{
	if (!menu)
		return;

	if (m_menu)
		M_Exit (true);

	if (!menu->item)
		M_First_Item (menu);

	if (menu->on_enter) {
		Cbuf_InsertText (menu->on_enter);
		Cbuf_Execute ();
	}

	m_entersound = true;
	m_menu = menu;
	M_SetKeyDest ();
}

static void
M_Menu_f (void)
{
	menu_t		*menu;
	const char	*id;

	id = Cmd_Args ();

	for (menu = m_first; menu; menu = menu->next) {
		if (!strcasecmp(id, menu->id)) {
			M_Enter (menu);
			return;
		}
	}

	Com_Printf("ERROR: Menu %s not found!\n", id);
}


void
M_Base_Init_Cvars(void)
{
}

void
M_Base_Init (void)
{
	Cmd_AddCommand ("menu", M_Menu_f);
	Cmd_AddCommand ("loadmenu", M_Loadmenu_f);
	Cmd_AddCommand ("deletemenu", M_Deletemenu_f);

	m_zone = Zone_AllocZone ("Menus");
}
