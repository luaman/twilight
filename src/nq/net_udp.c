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

#ifdef _WIN32
# include <winsock.h>
# include <io.h>
# define EWOULDBLOCK WSAEWOULDBLOCK
# define ECONNREFUSED WSAECONNREFUSED
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <sys/param.h>
# include <sys/ioctl.h>
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
#endif
#include <errno.h>
// LordHavoc: winsock uses WSAGetLastError instead of errno, errno is never set by winsock functions

#ifdef __sun__
# include <sys/filio.h>
#endif

#include <stdio.h>

#include "quakedef.h"
#include "cvar.h"
#include "net.h"
#include "strlib.h"
#include "sys.h"

#ifdef _WIN32
# ifdef errno
#  undef errno
# endif
# define errno WSAGetLastError()
#endif

static int  net_acceptsocket = -1;		// socket for fielding new connections
static int  net_controlsocket;
static int  net_broadcastsocket = 0;
static struct qsockaddr broadcastaddr;

static unsigned long myAddr;

#include "net_udp.h"

//=============================================================================

#ifdef _WIN32
# define MAXHOSTNAMELEN		256
#endif

int
UDP_Init (void)
{
	struct hostent *local;
	char        buf[MAXHOSTNAMELEN];

#ifdef _WIN32
	WSADATA     winsockdata;
	WORD        wVersionRequested;
	int         r;

	wVersionRequested = MAKEWORD (1, 1);

	r = WSAStartup (MAKEWORD (1, 1), &winsockdata);
	if (r)
		Sys_Error ("Winsock initialization failed.");
#endif /* _WIN32 */

	// determine my name & address
	gethostname (buf, MAXHOSTNAMELEN);
	if (!(local = gethostbyname (buf))) {
		Com_Printf ("Unable to resolve our own hostname: '%s'\n", buf);
		myAddr = inet_addr ("0.0.0.0");
		snprintf(buf, MAXHOSTNAMELEN, "not_a_damn_clue");
	} else
		myAddr = *(int *) local->h_addr_list[0];

	// if the quake hostname isn't set, set it to the machine name
	if (strcmp (hostname->svalue, "UNNAMED") == 0) {
		buf[15] = 0;
		Cvar_Set (hostname, buf);
	}

	if ((net_controlsocket = UDP_OpenSocket (0)) == -1)
		Sys_Error ("UDP_Init: Unable to open control socket\n");

	((struct sockaddr_in *) &broadcastaddr)->sin_family = AF_INET;
	((struct sockaddr_in *) &broadcastaddr)->sin_addr.s_addr = INADDR_BROADCAST;
	((struct sockaddr_in *) &broadcastaddr)->sin_port =
		htons ((unsigned short) net_hostport);

	Com_Printf ("UDP Initialized\n");
	tcpipAvailable = true;

	return net_controlsocket;
}

//=============================================================================

void
UDP_Shutdown (void)
{
	UDP_Listen (false);
	UDP_CloseSocket (net_controlsocket);
#ifdef _WIN32
	WSACleanup();
#endif
}

//=============================================================================

void
UDP_Listen (qboolean state)
{
	// enable listening
	if (state) {
		if (net_acceptsocket != -1)
			return;
		if ((net_acceptsocket = UDP_OpenSocket (net_hostport)) == -1)
			Sys_Error ("UDP_Listen: Unable to open accept socket\n");
		return;
	}
	// disable listening
	if (net_acceptsocket == -1)
		return;
	UDP_CloseSocket (net_acceptsocket);
	net_acceptsocket = -1;
}

//=============================================================================

// LordHavoc: grabbed QW's superior UDP_OpenSocket, and adjusted to fit
int
UDP_OpenSocket (int port)
{
	int         newsocket;
	struct sockaddr_in address;
	Uint         i;

#ifdef _WIN32
# define ioctl ioctlsocket
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
	address.sin_port = htons ((short) port);
	if (bind (newsocket, (void *) &address, sizeof (address)) == -1)
		Sys_Error ("UDP_OpenSocket: bind: %s", strerror (errno));

	return newsocket;
}

//=============================================================================

int
UDP_CloseSocket (int socket)
{
	if (socket == net_broadcastsocket)
		net_broadcastsocket = 0;
#ifdef _WIN32
	return closesocket (socket);
#else
	return close (socket);
#endif
}


//=============================================================================

int
UDP_Connect (int socket, struct qsockaddr *addr)
{
	socket = socket;
	addr = addr;
	return 0;
}

//=============================================================================

int
UDP_CheckNewConnections (void)
{
	unsigned long available;

	if (net_acceptsocket == -1)
		return -1;

	if (ioctl (net_acceptsocket, FIONREAD, &available) == -1)
		Sys_Error ("UDP: ioctlsocket (FIONREAD) failed\n");
	if (available)
		return net_acceptsocket;
	return -1;
}

//=============================================================================

int
UDP_Read (int socket, Uint8 *buf, int len, struct qsockaddr *addr)
{
	unsigned int addrlen = sizeof (struct qsockaddr);
	int          ret;

	ret = recvfrom (socket, (void *) buf, len, 0, (struct sockaddr *) addr, &addrlen);
	if (ret == -1 && (errno == EWOULDBLOCK || errno == ECONNREFUSED))
		return 0;
	return ret;
}

//=============================================================================

static int
UDP_MakeSocketBroadcastCapable (int socket)
{
	int         i = 1;

	// make this socket broadcast capable
	if (setsockopt (socket, SOL_SOCKET, SO_BROADCAST, (char *) &i, sizeof (i)) <
		0)
		return -1;
	net_broadcastsocket = socket;

	return 0;
}

//=============================================================================

int
UDP_Broadcast (int socket, Uint8 *buf, int len)
{
	int         ret;

	if (socket != net_broadcastsocket) {
		if (net_broadcastsocket != 0)
			Sys_Error ("Attempted to use multiple broadcasts sockets\n");
		ret = UDP_MakeSocketBroadcastCapable (socket);
		if (ret == -1) {
			Com_Printf ("Unable to make socket broadcast capable\n");
			return ret;
		}
	}

	return UDP_Write (socket, buf, len, &broadcastaddr);
}

//=============================================================================

int
UDP_Write (int socket, Uint8 *buf, int len, struct qsockaddr *addr)
{
	int         ret;

	ret =
		sendto (socket, (const void *) buf, len, 0, (struct sockaddr *) addr,
				sizeof (struct qsockaddr));
	if (ret == -1 && errno == EWOULDBLOCK)
		return 0;
	return ret;
}

//=============================================================================

char       *
UDP_AddrToString (struct qsockaddr *addr)
{
	static char buffer[22];
	int         haddr;

	haddr = ntohl (((struct sockaddr_in *) addr)->sin_addr.s_addr);
	snprintf (buffer, sizeof (buffer), "%d.%d.%d.%d:%d", (haddr >> 24) & 0xff,
			  (haddr >> 16) & 0xff, (haddr >> 8) & 0xff, haddr & 0xff,
			  ntohs (((struct sockaddr_in *) addr)->sin_port));
	return buffer;
}

//=============================================================================

int
UDP_StringToAddr (const char *string, struct qsockaddr *addr)
{
	int         ha1, ha2, ha3, ha4, hp;
	int         ipaddr;

	sscanf (string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
	ipaddr = (ha1 << 24) | (ha2 << 16) | (ha3 << 8) | ha4;

	addr->sa_family = AF_INET;
	((struct sockaddr_in *) addr)->sin_addr.s_addr = htonl (ipaddr);
	((struct sockaddr_in *) addr)->sin_port = htons ((unsigned short) hp);
	return 0;
}

//=============================================================================

int
UDP_GetSocketAddr (int socket, struct qsockaddr *addr)
{
	unsigned int addrlen = sizeof (struct qsockaddr);
	unsigned int a;

	memset (addr, 0, sizeof (struct qsockaddr));
	getsockname (socket, (struct sockaddr *) addr, &addrlen);
	a = ((struct sockaddr_in *) addr)->sin_addr.s_addr;
	if (a == 0 || a == inet_addr ("127.0.0.1"))
		((struct sockaddr_in *) addr)->sin_addr.s_addr = myAddr;

	return 0;
}

//=============================================================================

int
UDP_GetNameFromAddr (struct qsockaddr *addr, char *name)
{
	struct hostent *hostentry;

	hostentry =
		gethostbyaddr ((char *) &((struct sockaddr_in *) addr)->sin_addr,
					   sizeof (struct in_addr), AF_INET);
	if (hostentry) {
		strlcpy (name, (char *) hostentry->h_name, NET_NAMELEN);
		return 0;
	}

	strcpy (name, UDP_AddrToString (addr));
	return 0;
}

//=============================================================================

int
UDP_GetAddrFromName (const char *name, struct qsockaddr *addr)
{
	struct hostent *hostentry;

	hostentry = gethostbyname (name);
	if (!hostentry)
		return -1;

	addr->sa_family = AF_INET;
	((struct sockaddr_in *) addr)->sin_port =
		htons ((unsigned short) net_hostport);
	((struct sockaddr_in *) addr)->sin_addr.s_addr =
		*(int *) hostentry->h_addr_list[0];

	return 0;
}

//=============================================================================

int
UDP_AddrCompare (struct qsockaddr *addr1, struct qsockaddr *addr2)
{
	if (addr1->sa_family != addr2->sa_family)
		return -1;

	if (((struct sockaddr_in *) addr1)->sin_addr.s_addr !=
		((struct sockaddr_in *) addr2)->sin_addr.s_addr)
		return -1;

	if (((struct sockaddr_in *) addr1)->sin_port !=
		((struct sockaddr_in *) addr2)->sin_port)
		return 1;

	return 0;
}

//=============================================================================

int
UDP_GetSocketPort (struct qsockaddr *addr)
{
	return ntohs (((struct sockaddr_in *) addr)->sin_port);
}


int
UDP_SetSocketPort (struct qsockaddr *addr, int port)
{
	((struct sockaddr_in *) addr)->sin_port = htons ((unsigned short) port);
	return 0;
}

//=============================================================================

