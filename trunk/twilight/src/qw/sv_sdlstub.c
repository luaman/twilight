
#include "sv_sdlstub.h"
#include <unistd.h>
#include <sys/time.h>

struct timeval start;

// this function came from SDL timer/linux/SDL_systimer.c
void SDL_StartTicks(void)
{
	/* Set first ticks value */
	gettimeofday(&start, NULL);
}

// this function came from SDL timer/linux/SDL_systimer.c
Uint32 SDL_GetTicks (void)
{
	struct timeval now;
	Uint32 ticks;

	gettimeofday(&now, NULL);
	ticks=(now.tv_sec-start.tv_sec)*1000+(now.tv_usec-start.tv_usec)/1000;
	return(ticks);
}

// this function is original (but dumb)
void SDL_Delay(Uint32 ms)
{
	usleep(ms * 1000);
}

// this function is original (but dumb)
int SDL_Init(Uint32 flags)
{
	SDL_StartTicks();
	return 0;
}

// this function is original (but dumb)
void SDL_Quit(void)
{
	exit(0);
}
