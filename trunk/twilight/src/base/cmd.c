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

#include <stdlib.h>		/* For malloc() */

#include "quakedef.h"
#include "common.h"
#include "cmd.h"
#include "cvar.h"
#include "strlib.h"
#include "sys.h"
#include "zone.h"

// this gets allocated as soon as Cmd_AddCommand needs it
memzone_t *cmdzone = NULL;

#define MAX_ALIAS_NAME 32

typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	char				name[MAX_ALIAS_NAME];
	char				*value;
}
cmdalias_t;

cmdalias_t *cmd_alias;
qboolean cmd_wait;
cvar_t *cl_warncmd;

char cmd_tokenbuffer[MAX_TOKENBUFFER];

sizebuf_t *cmd_text;
Uint8 cmd_text_buf[8192];

//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void
Cmd_Wait_f (void)
{
	cmd_wait = true;
}

/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

/*
============
Cbuf_Init
============
*/
void
Cbuf_Init (void)
{
	cmd_text = Zone_Alloc (stringzone, sizeof (sizebuf_t));
	cmd_text->data = cmd_text_buf;
	cmd_text->maxsize = sizeof (cmd_text_buf);
}


/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void
Cbuf_AddText (char *text)
{
	size_t		l;

	l = strlen (text);
	if (cmd_text->cursize + l >= cmd_text->maxsize)
	{
		Com_Printf ("Cbuf_AddText: overflow\n");
		return;
	}

	SZ_Write (cmd_text, text, l);
}

/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void
Cbuf_InsertText (char *text)
{
	sizebuf_t	*p;
	char		*buf;
	size_t		len;

	if (!(len = strlen (text)))
		return;

	p = Zone_Alloc (stringzone, sizeof (sizebuf_t));
	buf = Zone_Alloc (stringzone, len + 2048);
	memcpy (buf, text, len);
	p->cursize = len;
	p->maxsize = len + 2048;
	p->data = buf;
	p->next = cmd_text;
	cmd_text = p;
}

static void
extract_line (char *line)
{
	size_t		i;
	int			quotes;
	char		*text;

	// find a \n or ; line break
	text = (char *) cmd_text->data;
	quotes = 0;
	for (i = 0; i < cmd_text->cursize; i++)
	{
		if (text[i] == '"')
			quotes++;
		if (!(quotes & 1) && text[i] == ';')
			// don't break if inside a quoted string
			break;
		if (text[i] == '\n' || text[i] == '\r')
			break;
	}

	memcpy (line, text, i);
	line[i] = '\0';

	// delete the text from the command buffer and move remaining commands
	// down this is necessary because commands (exec, alias) can insert
	// data at the beginning of the text buffer
	if (i == cmd_text->cursize)
		cmd_text->cursize = 0;
	else
	{
		i++;
		cmd_text->cursize -= i;
		memmove (text, text + i, cmd_text->cursize);
	}
}

/*
============
Cbuf_Execute_Sets
============
*/
void
Cbuf_Execute_Sets (void)
{
	char		line[1024] = { 0 };

	while (cmd_text->cursize)
	{
		extract_line (line);
		// execute the command line
		if (strncmp (line, "set", 3) == 0 && isspace ((int) line[3]))
			Cmd_ExecuteString (line, src_command);
	}
}

/*
============
Cbuf_Execute
============
*/
void
Cbuf_Execute (void)
{
	size_t		i;
	int			quotes;
	char		*text, line[1024];
	sizebuf_t	*p;

	while (1)
	{
		if (!cmd_text->cursize)
		{
			if (cmd_text->next)
			{
				p = cmd_text->next;
				Zone_Free (cmd_text->data);
				Zone_Free (cmd_text);
				cmd_text = p;
				continue;
			}
			else
				break;
		}

		// find a \n or ; line break
		text = (char *) cmd_text->data;

		quotes = 0;
		for (i = 0; i < cmd_text->cursize; i++)
		{
			if (text[i] == '"')
				quotes++;
			if (!(quotes & 1) && text[i] == ';')
				// don't break in a quoted string
				break;
			if (text[i] == '\n')
				break;
		}


		memcpy (line, text, i);
		line[i] = 0;

		// delete the text from the command buffer and move remaining commands
		// down this is necessary because commands (exec, alias) can insert
		// data at the beginning of the text buffer
		if (i == cmd_text->cursize)
			cmd_text->cursize = 0;
		else
		{
			i++;
			cmd_text->cursize -= i;
			memmove (text, text + i, cmd_text->cursize);
		}

		// execute the command line
		Cmd_ExecuteString (line, src_command);

		if (cmd_wait)
		{
			// skip out while text still remains in buffer, leaving it for next frame
			cmd_wait = false;
			break;
		}
	}
}


/*
============
Cbuf_InsertFile
============
*/
void
Cbuf_InsertFile (char *path)
{
	FILE		*f;
	char		*text;
	size_t		size;

	f = fopen (path, "rb");
	if (f)
	{
		size = COM_filelength (f);
		if (size)
		{
			text = Zone_Alloc(tempzone, size);
			if (text)
			{
				fread (text, size, 1, f);
				Cbuf_InsertText (text);
				Zone_Free(text);
			}
			else
				Sys_Printf ("Cbuf_InsertFile: Allocation failed for %s\n", path);
		}
		else
			Sys_Printf ("Cbuf_InsertFile: Empty file %s\n", path);
		fclose (f);
	}
	else
		Sys_Printf ("Cbuf_InsertFile: Couldn't open %s\n", path);
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
void
Cmd_StuffCmds_f (void)
{
	size_t		s, i;
	int			j;
	char		*text, *build, c;
	qboolean	done;

	// build the combined string to parse from
	s = 0;
	for (i = 1; i < com_argc; i++)
	{
		if (!com_argv[i])
			// NEXTSTEP nulls out -NXHost
			continue;
		s += strlen (com_argv[i]) + 1;
	}
	if (!s)
		return;

	text = Z_Malloc (s + 1);
	text[0] = 0;
	for (i = 1; i < com_argc; i++)
	{
		if (!com_argv[i])
			// NEXTSTEP nulls out -NXHost
			continue;
		strcat (text, com_argv[i]);
		if (i != com_argc - 1)
			strcat (text, " ");
	}

	// pull out the commands
	build = Z_Malloc (s + 1);
	build[0] = 0;

	for (i = 0; i < s - 1; i++)
	{
		if (text[i] == '+')
		{
			i++;

			j = i;
			done = false;
			while (text[j] && !done) {
				if (text[j - 1] == ' ')
					switch (text[j]) {
						case '+':
						case '-':
							done = true;
							break;
						default:
							j++;
					}
				else
					j++;
			}

			c = text[j];
			text[j] = 0;

			strcat (build, text + i);
			strcat (build, "\n");
			text[j] = c;
			i = j - 1;
		}
	}

	if (build[0])
		Cbuf_InsertText (build);

	Z_Free (text);
	Z_Free (build);
}


/*
===============
Cmd_Exec_f
===============
*/
void
Cmd_Exec_f (void)
{
	char		*f;

	if (Cmd_Argc () != 2)
	{
		Com_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	f = COM_LoadTempFile (Cmd_Argv (1), true);
	if (!f)
	{
		Com_Printf ("couldn't exec %s\n", Cmd_Argv (1));
		return;
	}

	if (cl_warncmd->ivalue || developer->ivalue)
		Com_Printf ("execing %s\n", Cmd_Argv (1));

	Cbuf_InsertText (f);
	Zone_Free (f);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void
Cmd_Echo_f (void)
{
	int			i;

	for (i = 1; i < Cmd_Argc (); i++)
		Com_Printf ("%s ", Cmd_Argv (i));
	Com_Printf ("\n");
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
void
Cmd_Alias_f (void)
{
	cmdalias_t	*a;
	char		cmd[1024], *s;
	int			i, c;

	if (Cmd_Argc () == 1)
	{
		Com_Printf ("Current alias commands:\n");
		for (a = cmd_alias; a; a = a->next)
			Com_Printf ("%s : %s\n", a->name, a->value);
		return;
	}

	s = Cmd_Argv (1);
	if (strlen (s) >= MAX_ALIAS_NAME)
	{
		Com_Printf ("Alias name is too long\n");
		return;
	}
	// if the alias already exists, reuse it
	for (a = cmd_alias; a; a = a->next)
	{
		if (!strcmp (s, a->name))
		{
			Z_Free (a->value);
			break;
		}
	}

	if (!a)
	{
		a = Z_Malloc (sizeof (cmdalias_t));
		a->next = cmd_alias;
		cmd_alias = a;
	}
	strcpy (a->name, s);

	// copy the rest of the command line
	cmd[0] = 0;
	c = Cmd_Argc ();
	for (i = 2; i < c; i++)
	{
		strlcat (cmd, Cmd_Argv (i), sizeof (cmd));
		if (i != c)
			strlcat (cmd, " ", sizeof (cmd));
	}
	strlcat (cmd, "\n", sizeof (cmd));

	a->value = CopyString (cmd);
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char					*name;
	xcommand_t				function;
}
cmd_function_t;


#define	MAX_ARGS 80

static int cmd_argc;
static char *cmd_argv[MAX_ARGS];
static char *cmd_args = NULL;

cmd_source_t cmd_source;

static cmd_function_t *cmd_functions;	// possible commands to execute

/*
============
Cmd_Argc
============
*/
int
Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char *
Cmd_Argv (int arg)
{
	if (arg >= cmd_argc)
		return "";
	return cmd_argv[arg];
}

/*
============
Cmd_Args
============
*/
char *
Cmd_Args (void)
{
	return cmd_args;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
============
*/
void
Cmd_TokenizeString (char *text)
{
	int			i = 0;

	cmd_argc = 0;
	cmd_args = NULL;

	while (1)
	{
		// skip whitespace up to a \n
		while (*text && *text <= ' ' && *text != '\n')
			text++;

		// a newline seperates commands in the buffer
		if (*text == '\n')
		{
			text++;
			break;
		}

		if (!*text)
			return;

		if (cmd_argc == 1)
			cmd_args = text;

		text = COM_Parse (text);
		if (!text)
			return;

		if (cmd_argc < MAX_ARGS)
		{
			size_t length = strlen (com_token) + 1;
			if (i + length > MAX_TOKENBUFFER)
				Sys_Error ("Cmd_TokenizeString: token buffer too small for"
						" %i characters", i + length);
			cmd_argv[cmd_argc] = cmd_tokenbuffer + i;
			i += length;
			memcpy (cmd_argv[cmd_argc], com_token, length);
			cmd_argc++;
		}
	}
}


/*
============
Cmd_AddCommand
============
*/
void
Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
	cvar_t			*var;
	cmd_function_t	*cmd;

	// fail if the command is a variable name
	var = Cvar_Find (cmd_name);
	if (var)
	{
		Com_Printf ("\"%s\" already defined as a Cvar\n",
				cmd_name);
		return;
	}
	// fail if the command already exists
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strcmp (cmd_name, cmd->name))
		{
			Com_Printf ("\"%s\" already defined\n", cmd_name);
			return;
		}
	}

	if (!cmdzone)
		cmdzone = Zone_AllocZone("command");
	cmd = Zone_Alloc(cmdzone, sizeof (cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

/*
============
Cmd_Exists
============
*/
qboolean
Cmd_Exists (char *cmd_name)
{
	cmd_function_t	*cmd;

	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strcmp (cmd_name, cmd->name))
			return true;
	}

	return false;
}



/*
============
Cmd_CompleteCommand
============
*/
char *
Cmd_CompleteCommand (char *partial)
{
	size_t			len;
	cmdalias_t		*a;
	cmd_function_t	*cmd;

	len = strlen (partial);

	if (!len)
		return NULL;

	// check functions
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncmp (partial, cmd->name, len))
			return cmd->name;
	for (a = cmd_alias; a; a = a->next)
		if (!strcmp (partial, a->name))
			return a->name;

	// check for partial match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncmp (partial, cmd->name, len))
			return cmd->name;
	for (a = cmd_alias; a; a = a->next)
		if (!strncmp (partial, a->name, len))
			return a->name;

	return NULL;
}

/*
============
Cmd_CompleteCountPossible

Thanks for Taniwha's Help -EvilTypeGuy
============
*/
int
Cmd_CompleteCountPossible (char *partial)
{
	cmd_function_t	*cmd;
	int				h;
	size_t			len;
	
	len = strlen(partial);
	if (!len)
		return 0;

	h = 0;
	// Loop through the command list and count all partial matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp(partial, cmd->name, len))
			h++;

	return h;
}

/*
============
Cmd_CompleteBuildList

Thanks for Taniwha's Help -EvilTypeGuy
============
*/
char **
Cmd_CompleteBuildList (char *partial)
{
	cmd_function_t	*cmd;
	size_t			len, bpos;
	int				sizeofbuf = (Cmd_CompleteCountPossible (partial) + 1) * sizeof (char *);
	char			**buf;

	len = strlen(partial);
	buf = (char**)malloc(sizeofbuf + sizeof (char *));

	bpos = 0;
	// Loop through the alias list and print all matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp(partial, cmd->name, len))
			buf[bpos++] = cmd->name;

	buf[bpos] = NULL;
	return buf;
}

/*
============
Cmd_CompleteAlias

Thanks for Taniwha's Help -EvilTypeGuy
============
*/
char
*Cmd_CompleteAlias (char * partial)
{
	static cmdalias_t	*alias;
	size_t				len;

	len = strlen(partial);

	if (!len)
		return NULL;

	// Check aliases
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			return alias->name;

	return NULL;
}

/*
============
Cmd_CompleteAliasCountPossible

Thanks for Taniwha's Help -EvilTypeGuy
============
*/
int
Cmd_CompleteAliasCountPossible (char *partial)
{
	cmdalias_t	*alias;
	int			h;
	size_t		len;

	len = strlen(partial);

	if (!len)
		return 0;

	h = 0;
	// Loop through the command list and count all partial matches
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			h++;

	return h;
}

/*
============
Cmd_CompleteAliasBuildList

Thanks for Taniwha's Help -EvilTypeGuy
============
*/
char **
Cmd_CompleteAliasBuildList (char *partial)
{
	cmdalias_t	*alias;
	size_t		len, bpos;
	int			sizeofbuf;
	char		**buf;

	sizeofbuf = (Cmd_CompleteAliasCountPossible (partial) + 1)
		* sizeof (char *);

	len = strlen(partial);
	buf = (char**)malloc(sizeofbuf + sizeof (char *));

	bpos = 0;
	// Loop through the alias list and print all matches
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			buf[bpos++] = alias->name;

	buf[bpos] = NULL;
	return buf;
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void
Cmd_ExecuteString (char *text, cmd_source_t src)
{
	cmd_function_t	*cmd;
	cmdalias_t		*a;

	cmd_source = src;
	Cmd_TokenizeString (text);

	// execute the command line
	if (!Cmd_Argc ())
		return;	// no tokens

	// check functions
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strcasecmp (cmd_argv[0], cmd->name))
		{
			if (!cmd->function)
				Cmd_ForwardToServer ();
			else
				cmd->function ();
			return;
		}
	}

	// check alias
	for (a = cmd_alias; a; a = a->next)
	{
		if (!strcasecmp (cmd_argv[0], a->name))
		{
			Cbuf_InsertText (a->value);
			return;
		}
	}

	// check cvars
	if (!Cvar_LegacyCmd () && (cl_warncmd->ivalue || developer->ivalue))
		Com_Printf ("Unknown command \"%s\"\n", Cmd_Argv (0));
}

/*
============
Cmd_Init
============
*/
void
Cmd_Init (xcommand_t CmdForwardToServer)
{
	// register our commands
	Cmd_AddCommand ("stuffcmds", Cmd_StuffCmds_f);
	Cmd_AddCommand ("exec", Cmd_Exec_f);
	Cmd_AddCommand ("echo", Cmd_Echo_f);
	Cmd_AddCommand ("alias", Cmd_Alias_f);
	Cmd_AddCommand ("wait", Cmd_Wait_f);

	// and our cvars
	cl_warncmd = Cvar_Get ("cl_warncmd", "0", CVAR_NONE, NULL);

	if (CmdForwardToServer)
		Cmd_AddCommand ("cmd", CmdForwardToServer);
}

