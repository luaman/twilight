#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "strlib.h"
#include "zone.h"
#include "common.h"
#include "qtypes.h"
#include "mathlib.h"
#include "lh_parser.h"

qboolean
LHP_integer(char *text, int *value)
{
	int negative;
	*value = 0;
	negative = 0;
	if (*text == '+' || *text == '-')
	{
		if (*text == '-')
			negative = 1;
		text++;
	}
	if (*text < '0' || *text > '9')
		return false;
	while (*text >= '0' && *text <= '9')
		*value = *value * 10 + (*text++ - '0');
	if (negative)
		*value = -*value;
	if (*text > ' ')
		return false;
	return true;
}

qboolean
LHP_double(char *text, double *value)
{
	int negative, negative2;
	double d, n;
	*value = 0;
	negative = 0;
	if (*text == '+' || *text == '-')
	{
		if (*text == '-')
			negative = 1;
		text++;
	}
	if ((*text < '0' || *text > '9') && *text != '.')
		return false;
	while (*text >= '0' && *text <= '9')
		*value = *value * 10 + (*text++ - '0');
	if (*text == '.')
	{
		text++;
		n = 0;
		d = 1;
		while (*text >= '0' && *text <= '9')
		{
			d *= 10;
			n = n * 10 + (*text++ - '0');
		}
		// preserve as much accuracy as possible by counting in integer
		// and then dividing to make it a fraction
		*value += n / d;
	}
	if (*text == 'e' || *text == 'E')
	{
		text++;
		negative2 = 0;
		n = 0;
		if (*text == '+' || *text == '-')
		{
			if (*text == '-')
				negative2 = 1;
			text++;
		}
		while (*text >= '0' && *text <= '9')
			n = n * 10 + (*text++ - '0');
		if (negative2)
			n = -n;
		*value *= pow(10, n);
	}
	if (negative)
		*value = -*value;
	if (*text > ' ')
		return false;
	return true;
}


static int
wordclassify(char *text, int *intvalue, double *doublevalue)
{
	int flags = 0;
	while (*text && *text <= ' ')
		text++;
	if (LHP_integer(text, intvalue))
		flags |= WORDFLAG_INTEGER;
	if (LHP_double(text, doublevalue))
		flags |= WORDFLAG_DOUBLE;
	return flags;
}

void
LHP_freecodewords(codeword_t *word)
{
	codeword_t *next;
	while (word)
	{
		next = word->next;
		if (word->string)
			Zone_Free(word->string);
		Zone_Free(word);
		word = next;
	}
}

void
LHP_freecodetree(codetree_t *code)
{
	while(code)
	{
		if (code->child)
			LHP_freecodetree(code->child);
		LHP_freecodewords(code->words);
		code = code->next;
	}
}

void
LHP_printcodetree_c(int indentlevel, codetree_t *code)
{
	int i;
	char tabs[64];
	codeword_t *word;
	if (indentlevel > 63) {
		Com_Printf("printcodetree_c: only up to 63 indent levels supported\n");
		return;
	}
	for (i = 0;i < indentlevel;i++)
		tabs[i] = '\t';
	tabs[i] = 0;
	while (code->linenumber < 0 && code->child)
		code = code->child;
	if (code->linenumber < 0)
		return;
	while (code)
	{
		Com_Printf("%d%s", code->linenumber, tabs);
		word = code->words;
		while (word)
		{
			if (word->flags & WORDFLAG_STRING)
				Com_Printf("\"%s\"", word->string);
			else if (word->flags & WORDFLAG_INTEGER)
				Com_Printf("%d", word->intvalue);
			else if (word->flags & WORDFLAG_DOUBLE)
				Com_Printf("%g", word->doublevalue);
			else
				Com_Printf("%s", word->string);
			if (word->next)
				Com_Printf(" ");
			word = word->next;
		}
		if (code->child)
		{
			Com_Printf(":\n");
			LHP_printcodetree_c(indentlevel + 1, code->child);
		} else
			Com_Printf("\n");
		code = code->next;
	}
}

#define MAX_WORD_SIZE	128

codetree_t *
LHP_parse(char *text, char *name, memzone_t *zone)
{
	int			line, lastindent, indent, characters, spaces, c, tmp_len = 0;
	char		*t, *textstart, *textend, *spacepos, *linestart, *lineend;
	char		tmp_word[MAX_WORD_SIZE] = { '\0' };
	codetree_t	*code, *newcode, *startcode;
	codeword_t	*word, *newword;

	startcode = code = Zone_Alloc(zone, sizeof(codetree_t));
	code->beginsindent = 1;
	code->temporarybeginsindent = 1;
	code->linenumber = -1;
	lastindent = -1;
	textstart = text;
	textend = text + strlen(text);
	for (line = 1;*text;line++)
	{
		if (text >= textend)
		{
			LHP_printcodetree_c(0, startcode);
			Com_Printf("%s:%d LHP: internal parsing error - %d characters past end of file!\n", name, line, textend - text);
			LHP_freecodetree (startcode);
			return NULL;
		}

		linestart = text;
		t = text;
		characters = 0;
		while (*t && *t != '\n' && *t != '\r')
			if (*t++ > ' ') // non-whitespace
				characters++;
		if (*t == '\r')
			t++;
		if (*t == '\n')
			t++;
		lineend = t;
		// skip blank lines
		if (characters == 0)
		{
			text = t;
			continue;
		}

		indent = 0;
		spaces = 0;
		spacepos = NULL;
		while (*text == '\t' || *text == ' ')
		{
			if (*text == ' ')
			{
				if (spacepos == NULL)
					spacepos = text;
				spaces++;
			}
			indent++;
			text++;
		}

		// ignore whitespace type preceeding a comment
		if (*text == '#')
		{
			text = t;
			continue;
		}
		if (*text == ':') {
			Com_Printf("%s:%d LHP: can't begin a statement with :\n", name, line);
			LHP_freecodetree (startcode);
			return NULL;
		}

		if (spaces)
		{
		//	printcodetree_c(0, startcode);
			Com_Printf("%s:%d LHP: space used for indenting, this is not allowed, use only tabs.\n", name, line);
			LHP_freecodetree (startcode);
			return NULL;
		}

		// compare indent level
		while (indent < lastindent && code->parent != NULL)
		{
			code = code->parent;
			lastindent--;
		}
		if (indent > lastindent)
		{
			if (indent - lastindent != 1) {
				Com_Printf("%s:%d LHP: indent level increased more than one step\n", name, line);
				LHP_freecodetree (startcode);
				return NULL;
			}
			if (!code->temporarybeginsindent)
			{
				Com_Printf("%s:%d LHP: indent level increased without the previous line ending in :\n", name, line);
				LHP_freecodetree (startcode);
				return NULL;
			}
			code->temporarybeginsindent = 0;
		}
		else if (code->temporarybeginsindent && code->parent != NULL)
		{
		//	printcodetree_c(0, startcode);
			Com_Printf("%s:%d LHP: previous statement ended in : but indent level did not increase\n", name, line);
			LHP_freecodetree (startcode);
			return NULL;
		}

		newcode = Zone_Alloc(zone, sizeof(codetree_t));
		newcode->linenumber = line;

		if (indent > lastindent)
		{
			newcode->parent = code;
			code->child = newcode;
		}
		else
		{
			newcode->parent = code->parent;
			newcode->prev = code;
			code->next = newcode;
		}

		lastindent = indent;

		// parse the words now
		word = NULL;
		while (*text && *text != ':' && *text != '\n' && *text != '\r')
		{
			// comment ends the line
			if (*text == '#')
			{
				while (*text && *text != '\n' && *text != '\r')
					text++;
				break;
			}
			if (*text == '\\')
			{
				text++;
				switch(*text)
				{
				case '\\':
					break;
				case '\r':
					text++;
					if (*text == '\n')
						text++;
					while(*text <= ' ' && *text != '\r' && *text != '\n')
						text++;
					line++;
					break;
				case '\n':
					text++;
					while(*text <= ' ' && *text != '\r' && *text != '\n')
						text++;
					line++;
					break;
				default:
					Com_Printf("%s:%d LHP: unsupported escape character \\%c outside string\n", name, line, *text);
					LHP_freecodetree (startcode);
					return NULL;
				}
			}
			newword = Zone_Alloc(zone, sizeof(codeword_t));
			newword->parent = newcode;
			newword->prev = word;
			if (word)
				word->next = newword;
			else
				newcode->words = newword;
			newword->flags = 0;
			if (*text == '"')
			{
				newword->flags |= WORDFLAG_STRING;
				text++;
				while (*text && *text != '"')
				{
					c = *text++;
					if (c == '\r' || c == '\n') {
						Com_Printf("%s:%d LHP: newline during string\n", name, line);
						LHP_freecodetree (startcode);
						return NULL;
					}
					if (c == '\\')
					{
						switch(*text)
						{
						case '\\':
							break;
						case '\n':
							text++;
							line++;
							break;
						case '\r':
							text++;
							if (*text == '\n')
								text++;
							line++;
							break;
						case 'n':
							text++;
							c = '\n';
							break;
						case 'r':
							text++;
							c = '\r';
							break;
						case 't':
							text++;
							c = '\t';
							break;
						case '0':
							text++;
							switch(*text)
							{
							case '0':
							case '1':
							case '2':
							case '3':
							case '4':
							case '5':
							case '6':
							case '7':
								c = 0;
								while (*text >= '0' && *text <= '7')
									c = (c * 8) + *text;
								if (*text >= '8' && *text <= '9') {
									Com_Printf("%s:%d LHP: %c found during octal character in string\n", name, line, *text);
									LHP_freecodetree (startcode);
									return NULL;
								}
							case '8':
							case '9':
								Com_Printf("%s:%d LHP: %c found during octal character in string\n", name, line, *text);
								LHP_freecodetree (startcode);
								return NULL;
							case 'x':
								c = 0;
								while ((*text >= '0' && *text <= '9') || (*text >= 'a' && *text <= 'f') || (*text >= 'A' && *text <= 'F'))
								{
									if (*text >= '0' && *text <= '9')
										c = (c * 16) + *text - '0';
									else if (*text >= 'a' && *text <= 'f')
										c = (c * 16) + *text - 'a' + 10;
									else if (*text >= 'A' && *text <= 'F')
										c = (c * 16) + *text - 'A' + 10;
								}
								break;
							default:
								Com_Printf("%s:%d LHP: unknown number format 0%c\n", name, line, *text);
								LHP_freecodetree (startcode);
								return NULL;
							}
							if (c > 255) {
								Com_Printf("%s:%d LHP: only 8bit characters supported, sorry\n", name, line);
								LHP_freecodetree (startcode);
								return NULL;
							}
							if (c <= 0) {
								Com_Printf("%s:%d LHP: character zero (NULL) is not allowed in strings\n", name, line);
								LHP_freecodetree (startcode);
								return NULL;
							}
							break;
						case 0:
							Com_Printf("%s:%d LHP: end of file during string\n", name, line);
							LHP_freecodetree (startcode);
							return NULL;
						default:
							Com_Printf("%s:%d LHP: unknown escape code \\%c", name, line, *text);
							LHP_freecodetree (startcode);
							return NULL;
						}
					}
					tmp_word[tmp_len++] = c;
					if ((tmp_len + 2) > MAX_WORD_SIZE) {
						Com_Printf("%s:%d LHP: Word too long!\n", name, line);
						LHP_freecodetree (startcode);
						return NULL;
					}
				}
				if (*text != '"') {
					Com_Printf("%s:%d LHP: string missing closing quote\n", name, line);
					LHP_freecodetree (startcode);
					return NULL;
				}
				text++;
				if (*text > ' ' && *text != ':') {
					Com_Printf("%s:%d LHP: text directly following quoted string\n", name, line);
					LHP_freecodetree (startcode);
					return NULL;
				}
			}
			else
				while (*text > ' ' && *text != ':') {
					tmp_word[tmp_len++] = *text++;
					if ((tmp_len + 2) > MAX_WORD_SIZE) {
						Com_Printf("%s:%d LHP: Word too long!\n", name, line);
						LHP_freecodetree (startcode);
						return NULL;
					}
				}
			if (*text == ':')
			{
				text++;
				t = text;
				while (*t && *t != '\n' && *t != '\r') {
					if ((*t != ' ') && (*t != '\t')) {
						Com_Printf("%s:%d LHP: : can only be the final character on a line\n", name, line);
						LHP_freecodetree (startcode);
						return NULL;
					}
					t++;
				}
				newcode->beginsindent = 1;
				newcode->temporarybeginsindent = 1;
			}
			tmp_word[++tmp_len] = '\0';
			newword->string = Zone_Alloc(zone, tmp_len);
			strlcpy (newword->string, tmp_word, tmp_len);
			tmp_len = 0;
			newword->flags |= wordclassify(newword->string, &newword->intvalue, &newword->doublevalue);
			word = newword;

			while (*text && *text <= ' ' && *text != '\n' && *text != '\r')
				text++;
		}

		code = newcode;

		if (*text && *text != '\n' && *text != '\r') {
			Com_Printf("%s:%d parseshader: parsing line did not leave end of line as current character\n", name, line);
			LHP_freecodetree (startcode);
			return NULL;
		}
		if (*text == '\r')
			text++;
		if (*text == '\n')
			text++;
	}

	return startcode;
}
