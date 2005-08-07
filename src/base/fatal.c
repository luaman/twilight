/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2004 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#include "twiconfig.h"

#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif

void
Twi_BackTrace (int fd)
{
#if HAVE_EXECINFO_H
	void        *array[128] = { 0 };
	int         size;
	size = backtrace (array, sizeof(array) / sizeof(array[0]));
	backtrace_symbols_fd (array, size, fd);
#else
	fd = fd;    // Make it referenced.
#endif
}

#ifndef HAVE_SIGACTION

/* No signals on this platform, nothing to do.. */

void Twi_InstallParachute(void)
{
	return;
}

void Twi_UninstallParachute(void)
{
	return;
}

#else

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "SDL.h"
#include "fatal.h"

/* This installs some signal handlers for the more common fatal signals,
   so that if the programmer is lazy, the app doesn't die so horribly if
   the program crashes.
*/

static void print_msg(const char *text)
{
	fputs (text, stderr);
}

static void Twi_Parachute(int sig, siginfo_t *siginfo, void *context)
{
	context = context;

	signal(sig, SIG_DFL);
	print_msg("Fatal signal: ");
	switch (sig) {
		case SIGSEGV:
			print_msg("Segmentation Fault");
			fprintf(stderr, " at %p", siginfo->si_addr);
			break;
#ifdef SIGBUS
#if SIGBUS != SIGSEGV
		case SIGBUS:
			print_msg("Bus Error");
			fprintf(stderr, " at %p", siginfo->si_addr);
			break;
#endif
#endif /* SIGBUS */
#ifdef SIGFPE
		case SIGFPE:
			print_msg("Floating Point Exception");
			fprintf(stderr, " at %p", siginfo->si_addr);
			break;
#endif /* SIGFPE */
#ifdef SIGQUIT
		case SIGQUIT:
			print_msg("Keyboard Quit");
			break;
#endif /* SIGQUIT */
#ifdef SIGPIPE
		case SIGPIPE:
			print_msg("Broken Pipe");
			break;
#endif /* SIGPIPE */
		default:
			fprintf(stderr, "# %d", sig);
			break;
	}
	print_msg(" (Twi Parachute Deployed)\n");
	Twi_BackTrace (2);
	SDL_Quit();
	exit(-sig);
}

static int Twi_fatal_signals[] = {
	SIGSEGV,
#ifdef SIGBUS
	SIGBUS,
#endif
#ifdef SIGFPE
	SIGFPE,
#endif
#ifdef SIGQUIT
	SIGQUIT,
#endif
	0
};

void Twi_InstallParachute(void)
{
	/* Set a handler for any fatal signal not already handled */
	int i;
	struct sigaction action;

	for ( i=0; Twi_fatal_signals[i]; ++i ) {
		sigaction(Twi_fatal_signals[i], NULL, &action);
		if ( action.sa_handler == SIG_DFL ) {
			action.sa_sigaction = Twi_Parachute;
			action.sa_flags |= SA_SIGINFO;
			sigaction(Twi_fatal_signals[i], &action, NULL);
		}
	}
#ifdef SIGALRM
	/* Set SIGALRM to be ignored -- necessary on Solaris */
	sigaction(SIGALRM, NULL, &action);
	if ( action.sa_handler == SIG_DFL ) {
		action.sa_handler = SIG_IGN;
		sigaction(SIGALRM, &action, NULL);
	}
#endif
	return;
}

void Twi_UninstallParachute(void)
{
	/* Remove a handler for any fatal signal handled */
	int i;
	struct sigaction action;

	for ( i=0; Twi_fatal_signals[i]; ++i ) {
		sigaction(Twi_fatal_signals[i], NULL, &action);
		if ( action.sa_sigaction == Twi_Parachute ) {
			action.sa_handler = SIG_DFL;
			action.sa_flags &= ~SA_SIGINFO;
			sigaction(Twi_fatal_signals[i], &action, NULL);
		}
	}
}

#endif /* NO_SIGNAL_H */
