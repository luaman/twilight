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

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>		// struct timeval
#endif
#ifdef _WIN32
# include <windows.h>
# include <winsock.h>
# define EWOULDBLOCK WSAEWOULDBLOCK
# define ECONNREFUSED WSAECONNREFUSED

# define MAXHOSTNAMELEN		256
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# include <sys/param.h>
# include <sys/ioctl.h>
# include <sys/uio.h>
# include <arpa/inet.h>
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
#endif
#include <errno.h>
// LordHavoc: winsock uses WSAGetLastError instead of errno, errno is never set by winsock functions
#ifdef _WIN32
# ifdef errno
#  undef errno
# endif
# define errno WSAGetLastError()
#endif

#ifdef __sun__
# include <sys/filio.h>
#endif

#include "quakedef.h"
#include "common.h"
#include "net.h"
#include "strlib.h"
#include "sys.h"
#include "server.h"


netadr_t    net_local_adr;

netadr_t    net_from;
sizebuf_t   net_message;

static qboolean net_initialized = false;

static int	ip_sockets[2] = { -1, -1 };	// non blocking, for receives

#define	MAX_UDP_PACKET	8192
Uint8       net_message_buffer[MAX_UDP_PACKET];

//int         gethostname (char *, int);
int         close (int);

#define	MAX_LOOPBACK	4	// must be a power of two

typedef struct
{
	Uint8	data[MAX_UDP_PACKET];
	int		datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t	msgs[MAX_LOOPBACK];
	unsigned int	get, send;
} loopback_t;

loopback_t	loopbacks[2];

//=============================================================================

static void
NetadrToSockadr (netadr_t *a, struct sockaddr_in *s)
{
	memset (s, 0, sizeof (*s));
	s->sin_family = AF_INET;

	*(int *) &s->sin_addr = *(int *) &a->ip;
	s->sin_port = a->port;
}

static void
SockadrToNetadr (struct sockaddr_in *s, netadr_t *a)
{
	a->type = NA_IP;
	*(int *) &a->ip = *(int *) &s->sin_addr;
	a->port = s->sin_port;
}

qboolean
NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
	return ((a.type == NA_LOOPBACK && b.type == NA_LOOPBACK) ||
		(*(unsigned *)a.ip == *(unsigned *)b.ip));
}


qboolean
NET_CompareAdr (netadr_t a, netadr_t b)
{
	return ((a.type == NA_LOOPBACK && b.type == NA_LOOPBACK) ||
		(*(unsigned *)a.ip == *(unsigned *)b.ip && a.port == b.port));
}

const char       *
NET_AdrToString (netadr_t a)
{
	static char s[64];

	if (a.type == NA_LOOPBACK)
		return "loopback";

	snprintf (s, sizeof (s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2],
			  a.ip[3], ntohs (a.port));

	return s;
}

const char       *
NET_BaseAdrToString (netadr_t a)
{
	static char s[64];

	snprintf (s, sizeof (s), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);

	return s;
}

/*
=============
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
qboolean
NET_StringToAdr (const char *s, netadr_t *a)
{
	struct hostent *h;
	struct sockaddr_in sadr;
	char       *colon;
	char        copy[128];

	if (!strcmp(s, "local"))
	{
		memset(a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}

	memset (&sadr, 0, sizeof (sadr));
	sadr.sin_family = AF_INET;

	sadr.sin_port = 0;

	strlcpy_s (copy, s);
	// strip off a trailing :port if present
	for (colon = copy; *colon; colon++)
		if (*colon == ':') {
			*colon = 0;
			sadr.sin_port = htons ((unsigned short) Q_atoi (colon + 1));
		}

	if (copy[0] >= '0' && copy[0] <= '9') {
		*(int *) &sadr.sin_addr = inet_addr (copy);
	} else {
		if (!(h = gethostbyname (copy)))
			return 0;
		*(int *) &sadr.sin_addr = *(int *) h->h_addr_list[0];
	}

	SockadrToNetadr (&sadr, a);

	return true;
}

/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/

static qboolean
NET_GetLoopPacket (netsrc_t sock)
{
	int     i;
	loopback_t  *loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if ((int)(loop->send - loop->get) <= 0)
		return false;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	memcpy (net_message.data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message.cursize = loop->msgs[i].datalen;
	net_from = net_local_adr;
	net_from.type = NA_LOOPBACK;
	return true;
}

static void
NET_SendLoopPacket (netsrc_t sock, unsigned int length, void *data, netadr_t to)
{
	int     i;
	loopback_t  *loop = &loopbacks[sock^1];

	to = to;

	i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	if (length > sizeof(loop->msgs[i].data))
		Sys_Error ("NET_SendLoopPacket: length > MAX_UDP_PACKET");

	memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}



//=============================================================================

qboolean
NET_GetPacket (netsrc_t sock)
{
	int         ret;
	struct sockaddr_in from;
	Uint        fromlen;
	int			net_socket = ip_sockets[sock];

	if (NET_GetLoopPacket (sock))
		return true;

	if (net_socket == -1)
		return false;

	fromlen = sizeof (from);
	ret =
		recvfrom (net_socket, net_message_buffer, sizeof (net_message_buffer),
				  0, (struct sockaddr *) &from, &fromlen);
	if (ret == -1) {
		if (errno == EWOULDBLOCK)
			return false;
		if (errno == ECONNREFUSED)
			return false;
#ifdef _WIN32
		// LordHavoc: never could figure out why recvfrom was returning this
		// (no such file or directory), but had to workaround it...
		if (errno == ENOENT)
			return false;
#endif
		Sys_Printf ("NET_GetPacket: %s\n", strerror (errno));
		return false;
	}

	net_message.cursize = ret;
	SockadrToNetadr (&from, &net_from);

	return ret;
}

//=============================================================================

void
NET_SendPacket (netsrc_t sock, unsigned int length, void *data, netadr_t to)
{
	int         ret;
	struct sockaddr_in addr;
	int			net_socket = ip_sockets[sock];

	if (to.type == NA_LOOPBACK)
	{
		NET_SendLoopPacket (sock, length, data, to);
		return;
	}

	if (net_socket == -1)
		return;

	NetadrToSockadr (&to, &addr);

	ret =
		sendto (net_socket, data, length, 0, (struct sockaddr *) &addr,
				sizeof (addr));
	if (ret == -1) {
		if (errno == EWOULDBLOCK)
			return;
		if (errno == ECONNREFUSED)
			return;
		Sys_Printf ("NET_SendPacket: %s\n", strerror (errno));
	}
}

//=============================================================================

static int
UDP_OpenSocket (int port)
{
	int         newsocket;
	struct sockaddr_in address;
	Uint        i;

#ifdef _WIN32
#define ioctl ioctlsocket
	unsigned long _true = true;
#else
	int         _true = 1;
#endif

	memset (&address, 0, sizeof (address));

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		Sys_Error ("UDP_OpenSocket: socket:%s", strerror (errno));
	if (ioctl (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO:", strerror (errno));
	address.sin_family = AF_INET;
//ZOID -- check for interface binding option
	if ((i = COM_CheckParm ("-ip")) != 0 && i < com_argc) {
		address.sin_addr.s_addr = inet_addr (com_argv[i + 1]);
		Com_Printf ("Binding to IP Interface Address of %s\n",
					inet_ntoa (address.sin_addr));
	} else
		address.sin_addr.s_addr = INADDR_ANY;
	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons ((short) port);
	if (bind (newsocket, (void *) &address, sizeof (address)) == -1)
		Sys_Error ("UDP_OpenSocket: bind: %s", strerror (errno));

	return newsocket;
}

void
NET_OpenSocket (netsrc_t sock, int port)
{
	// 
	// open the single socket to be used for all communications
	// 
	ip_sockets[sock] = UDP_OpenSocket (port);
}

void
NET_Init (void)
{
	if (net_initialized)
		return;

#ifdef _WIN32
	{
		WSADATA     winsockdata;
		WORD        wVersionRequested;
		int         r;

		wVersionRequested = MAKEWORD (1, 1);

		r = WSAStartup (MAKEWORD (1, 1), &winsockdata);
		if (r)
			Sys_Error ("Winsock initialization failed.");
	}
#endif /* _WIN32 */

	// 
	// init the message buffer
	// 
	SZ_Init (&net_message, net_message_buffer, sizeof(net_message_buffer));

	net_initialized = true;
}

/*
====================
Only used by the server
FIXME: Abstract the nonblocking stdin stuff
====================
*/
void 
NET_Sleep (int msec)
{
	fd_set			fdset;
	struct timeval	timeout;

	FD_ZERO (&fdset);

#ifndef _WIN32
		if (do_stdin)
			FD_SET (0, &fdset);
#endif

	FD_SET (ip_sockets[NS_SERVER], &fdset);
	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;

	select (ip_sockets[NS_SERVER] + 1, &fdset, NULL, NULL, &timeout);

#ifndef _WIN32
		stdin_ready = FD_ISSET (0, &fdset);
#endif
}

void
NET_Shutdown (void)
{
	if (ip_sockets[NS_CLIENT] != -1)
#ifdef _WIN32
		closesocket (ip_sockets[NS_CLIENT]);
#else
		close (ip_sockets[NS_CLIENT]);
#endif

	if (ip_sockets[NS_SERVER] != -1)
#ifdef _WIN32
		closesocket (ip_sockets[NS_SERVER]);
#else
		close (ip_sockets[NS_SERVER]);
#endif

#ifdef _WIN32
		WSACleanup();
#endif
}

