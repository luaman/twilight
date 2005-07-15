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

#ifndef __NET_H
#define __NET_H

#define	MAX_MSGLEN		1450
#define	MAX_DATAGRAM	1450

#define	PORT_ANY	-1

typedef enum {NA_LOOPBACK, NA_BROADCAST, NA_IP, NA_IPX, NA_BROADCAST_IPX} netadrtype_t;

typedef struct {
	netadrtype_t	type;

	Uint8       ip[4];
	unsigned short port;
	unsigned short pad;
} netadr_t;

extern netadr_t net_local_adr;
extern netadr_t net_from;				// address of who sent the packet
extern sizebuf_t net_message;

typedef enum {NS_CLIENT, NS_SERVER} netsrc_t;

void NET_Init (void);
void NET_Sleep (int msec);
void NET_Shutdown (void);
qboolean NET_GetPacket (netsrc_t sock);
void NET_SendPacket (netsrc_t sock, unsigned int length, void *data, netadr_t to);
void NET_OpenSocket (netsrc_t sock, int port);

qboolean NET_CompareAdr (netadr_t a, netadr_t b);
qboolean NET_CompareBaseAdr (netadr_t a, netadr_t b);
const char *NET_AdrToString (netadr_t a);
const char *NET_BaseAdrToString (netadr_t a);
qboolean NET_StringToAdr (const char *s, netadr_t *a);

#define NET_IsLocalAddress(adr)	(NET_CompareAdr(adr,net_local_adr))

//============================================================================

#define	OLD_AVG		0.99				// total = oldtotal*OLD_AVG +
										// new*(1-OLD_AVG)

#define	MAX_LATENT	32

typedef struct {
	qboolean    fatal_error;

	float       last_received;			// for timeouts

// the statistics are cleared at each client begin, because
// the server connecting process gives a bogus picture of the data
	float       frame_latency;			// rolling average
	float       frame_rate;

	int         drop_count;				// dropped packets, cleared each level
	int         good_count;				// cleared each level

	netadr_t    remote_address;
	int         qport;

// bandwidth estimator
	double      cleartime;				// if realtime > nc->cleartime, free to 
	// go
	double      rate;					// seconds / byte

// sequencing variables
	int         incoming_sequence;
	int         incoming_acknowledged;
	int         incoming_reliable_acknowledged;	// single bit

	int         incoming_reliable_sequence;	// single bit, maintained local

	int         outgoing_sequence;
	int         reliable_sequence;		// single bit
	int         last_reliable_sequence;	// sequence number of last send

// reliable staging and holding areas
	sizebuf_t   message;				// writing buffer to send to server
	Uint8       message_buf[MAX_MSGLEN];

	int         reliable_length;
	Uint8       reliable_buf[MAX_MSGLEN];	// unacked reliable message

// time and size data to calculate bandwidth
	int         outgoing_size[MAX_LATENT];
	double      outgoing_time[MAX_LATENT];

	netsrc_t	sock;
} netchan_t;

extern int nopacketcount;
extern int net_drop;
extern cvar_t *qport;

void Netchan_Init_Cvars(void);
void Netchan_Init(void);
void Netchan_OutOfBandPrint(netsrc_t sock, netadr_t adr, const char *format, ...);
void Netchan_Setup(netsrc_t sock, netchan_t *chan, netadr_t adr, int qport);
qboolean Netchan_CanPacket(netchan_t *chan);
qboolean Netchan_CanReliable(netchan_t *chan);
void Netchan_Transmit(netchan_t *chan, size_t length, Uint8 *data);
qboolean Netchan_Process(netchan_t *chan);

#endif // __NET_H

