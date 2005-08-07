/*
	$RCSfile$

	Copyright (C) 2004-2005 Thad Ward

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
#ifdef _WIN32
static const char rcsid[] =
"$Id$";

#include "twiconfig.h"

#include "quakedef.h"
#include "sys.h"

# include <windows.h>
# include <richedit.h>
# include "resource.h"
// the win32 console needs cmd.h so it can directly stuff commands into the command
// buffer from the message handlers.
# include "cmd.h"

HWND hMainWnd = NULL;
HWND hLogWnd = NULL;
HWND hEntryWnd = NULL;
HWND hClearBtn = NULL;
HWND hQuitBtn = NULL;
HINSTANCE hOurInstance = NULL;
HFONT console_font = NULL;
WNDPROC prev_EntryProc = NULL;
HMODULE hRichEditDLL = NULL;

#define IDC_CLEAR	1025
#define IDC_QUIT	1026

static const COLORREF sys_colors[4] =
{
	RGB(255,255,255),
	RGB(213,107,0),
	RGB(202,198,0),
	RGB(190,95,0)
};

static const char sys_colormap[256] =
{
	3, 3, 3, 3, 3, 3, 3, 3,	3, 0, 0, 3, 3, 0, 3, 3,
	2, 2, 2, 2, 2, 2, 2, 2,	2, 2, 2, 2, 2, 2, 2, 2,
	0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0,

	3, 3, 3, 3, 3, 3, 3, 3,	3, 0, 0, 3, 3, 0, 3, 3,
	2, 2, 2, 2, 2, 2, 2, 2,	2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1,	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,	1, 1, 1, 1, 1, 1, 1, 1,
};

static const char sys_charmap[256] =
{
	' ', '#', '#', '#', '#', '.', '#', '#',
	'#', '\t', '\n', '#', ' ', '\n', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<',

	'<', '=', '>', '#', '#', '.', '#', '#',
	'#', '#', ' ', '#', ' ', '>', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<'
};

void
WinCon_Printf(const char *text)
{
	Uint8		*p;

	if (hLogWnd)
	{
		char buf[4096];
		Uint8 *pbuf = buf;
		static int	bufsize;
		int			len;
		CHARRANGE	sel;
		int			lastcolor = -1;
		CHARFORMAT	charfmt;

		memset(&charfmt, 0, sizeof(charfmt));
		charfmt.cbSize = sizeof(charfmt);
		charfmt.dwMask = CFM_COLOR;		// only changing the color

		len = strlen( text );
		bufsize += len;
		if( bufsize > 0xFFFFFF ) {
			sel.cpMin = 0;
			sel.cpMax = 0xFFFFFFFF;
			bufsize = len;
		}
		else
			sel.cpMin = sel.cpMax = 0xFFFFFFFF;

		SendMessage( hLogWnd, EM_EXSETSEL, 0, (LPARAM)&sel);
		SendMessage( hLogWnd, EM_LINESCROLL, 0, 0xFFFF );
		SendMessage( hLogWnd, EM_SCROLLCARET, 0, 0 );

		for(p = (Uint8*)text; *p; p++)
		{
			// if we hit a new color
			if(sys_colormap[*p] != lastcolor)
			{
				if(lastcolor != -1)
				{
					// flush the current buffer
					*pbuf = 0;
					SendMessage( hLogWnd, EM_REPLACESEL, FALSE, (LPARAM)buf );
					// reset the buffer
					pbuf = buf;
				}

				// send the new color to the window and store it.
				lastcolor = sys_colormap[*p];
				charfmt.crTextColor = sys_colors[lastcolor];
				SendMessage(hLogWnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&charfmt);
			}

			if (*p == '\n')
			{
				*pbuf = '\r';
				pbuf++;
			}

			*pbuf = sys_charmap[*p];
			pbuf++;
		}

		// flush the last bit
		*pbuf = 0;
		if(strlen(buf) > 0)
			SendMessage( hLogWnd, EM_REPLACESEL, FALSE, (LPARAM)buf );
	}
}

void
WinCon_Shutdown()
{
	ShowWindow(hMainWnd, SW_HIDE);
	DestroyWindow(hLogWnd);
	DestroyWindow(hEntryWnd);
	DestroyWindow(hClearBtn);
	DestroyWindow(hQuitBtn);
	DestroyWindow(hMainWnd);
	DeleteObject(console_font);
	FreeLibrary(hRichEditDLL);
}

LRESULT CALLBACK MainWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	NOTIFYICONDATA nd;
	HRESULT res;
	static int minimized = 0;

	switch( uMsg ) {
		// grabbing WM_SETTEXT so that we can change the tooltip
		// on the tray icon if we are minimized (while still only
		// calling SetWindowText() )
		// note: DO NOT return from this message, since we want to
		// call the default window proc.
		case WM_SETTEXT:
			if(minimized)
			{
				memset(&nd, 0, sizeof(nd));
				nd.cbSize = sizeof(nd);
				nd.hWnd = hMainWnd;
				nd.uID = 1;
				strncpy(nd.szTip, (char*)lParam, sizeof(nd.szTip));
				Shell_NotifyIcon(NIM_MODIFY, &nd);
			}
			break;
		case WM_ACTIVATE:
			if( LOWORD( wParam ) != WA_INACTIVE ) {
				SetFocus( hEntryWnd );
			}
			return 0;
			break;
		case WM_CLOSE:
			Cbuf_AddText("quit");
			return 0;
			break;
		case WM_SYSCOMMAND:
			switch( wParam) {
				case SC_MINIMIZE:
					// do minimize to tray here
					memset(&nd, 0, sizeof(nd));
					nd.cbSize = sizeof(nd);
					nd.hWnd = hMainWnd;
					nd.hIcon = LoadIcon (hOurInstance, MAKEINTRESOURCE( IDI_TWILIGHT ));
					nd.uID = 1;
					nd.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
					nd.uCallbackMessage = WM_USER;
					GetWindowText(hMainWnd, nd.szTip, sizeof(nd.szTip));
					res = Shell_NotifyIcon(NIM_ADD, &nd);
					DestroyIcon(nd.hIcon);

					// only hide the window if the tray icon was added sucessfully.
					if(res)
					{
						ShowWindow(hMainWnd, SW_HIDE);
						minimized = 1;
						return 0;
					}
					break;
			}
			break;
		case WM_USER:
			switch(lParam) {
				case WM_LBUTTONDOWN:
				case WM_RBUTTONDOWN:
				case WM_MBUTTONDOWN:
					if(minimized)
					{
						ShowWindow(hMainWnd, SW_SHOW);
						minimized = 0;
						memset(&nd, 0, sizeof(nd));
						nd.cbSize = sizeof(nd);
						nd.hWnd = hMainWnd;
						nd.uID = 1;
						Shell_NotifyIcon(NIM_DELETE, &nd);
						return 0;
					}
					break;
			}
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_CLEAR:
				SetWindowText(hLogWnd, "");
				return 0;
				break;
			case IDC_QUIT:
				Cbuf_AddText("quit");
				return 0;
				break;
			}
			break;
	}

	return DefWindowProc( hwnd, uMsg, wParam, lParam );
}

LRESULT CALLBACK EntryWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char buf[1024];

	switch( uMsg ) {
		case WM_CHAR:
			/* this is a switch in case we want to do command history, completetion, etc. */
			switch(wParam) {
			case '\r':
				GetWindowText( hEntryWnd, buf, sizeof(buf) );
				
				/* clear commandline */
				SetWindowText( hEntryWnd, "" );

				/* add to command buffer */
				Cbuf_AddText(buf);

				/* print command */
				Sys_Printf( "]%s\n", buf );
				return 0;
				break;
			}
			break;
	}

	return CallWindowProc( prev_EntryProc, hwnd, uMsg, wParam, lParam );
}

void
WinCon_Init()
{
	WNDCLASS wc;
	RECT rect;
	HDC hDC;
	int w, h, width, height;
	
	hOurInstance = GetModuleHandle(NULL);

	/* we need to make sure we load the richedit control.
	   we will only be using functionality from RichEdit 1.0, so
	   it doesn't matter what gets loaded, but the dll name is different
	   with each version of the control (except between 2.0 and 3.0)
	*/
	/* richedit 1.0 */
	hRichEditDLL = LoadLibrary("riched32.dll");

	/* richedit 2.0/3.0 */
	if(!hRichEditDLL)
		hRichEditDLL = LoadLibrary("riched20.dll");

	/* richedit 4.1 */
	if(!hRichEditDLL)
		hRichEditDLL = LoadLibrary("msftedit.dll");

	wc.style = 0;
	wc.lpfnWndProc = MainWinProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hOurInstance;
	wc.hIcon = LoadIcon (hOurInstance, MAKEINTRESOURCE( IDI_TWILIGHT ) );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszMenuName =  NULL;
	wc.lpszClassName = "TwiConsole";

	if (!RegisterClass (&wc)) 
		Sys_Error ("Unable to register Console window class.");

	rect.left = rect.top = 0;
	rect.right = 540;
	rect.bottom = 450;

	AdjustWindowRect( &rect, WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_GROUP, FALSE);

	hDC = GetDC(GetDesktopWindow());
	w = GetDeviceCaps(hDC, HORZRES);
	h = GetDeviceCaps(hDC, VERTRES);
	ReleaseDC(GetDesktopWindow(), hDC);

	width = rect.right - rect.left + 1;
	height = rect.bottom - rect.top + 1;

	hMainWnd = CreateWindow (	"TwiConsole",
								"Twilight Console",
								WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_GROUP,
								(w - width) / 2,
								(h - height) / 2,
								width,
								height,
								(HWND) NULL,
								(HMENU) NULL,
								hOurInstance,
								(LPVOID) NULL); 	

	if (!hMainWnd) 
		Sys_Error("Unable to create Console window."); 

	hDC = GetDC( hMainWnd);
	console_font = CreateFont( -MulDiv(8, GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, 0,
					FW_LIGHT, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH|FF_MODERN, "Courier New");
	ReleaseDC(hMainWnd, hDC);

	hEntryWnd = CreateWindowEx(0,"edit", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, 6,
					400, 528, 20, hMainWnd, NULL, hOurInstance, NULL);
	hLogWnd = CreateWindowEx(0, "richedit", NULL, WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|ES_READONLY|
					ES_AUTOVSCROLL|ES_MULTILINE|ES_LEFT|ES_NOHIDESEL, 6, 40, 526, 354, hMainWnd, NULL, hOurInstance, NULL);
	SendMessage(hLogWnd, EM_SETBKGNDCOLOR, 0, RGB(0,0,0));
	SendMessage(hLogWnd, WM_SETFONT, (WPARAM)console_font, FALSE);
	SendMessage(hEntryWnd, WM_SETFONT, (WPARAM)console_font, FALSE);
	prev_EntryProc = (WNDPROC)SetWindowLong(hEntryWnd, GWL_WNDPROC, (long)EntryWinProc);

	hClearBtn = CreateWindowEx( 0, "button", "Clear", WS_CHILD|WS_VISIBLE, 6, 425, 72, 24, 
					hMainWnd, (HMENU)IDC_CLEAR, hOurInstance, NULL );

	hQuitBtn = CreateWindowEx( 0, "button", "Quit", WS_CHILD|WS_VISIBLE, 462, 425, 72, 24, 
					hMainWnd, (HMENU)IDC_QUIT, hOurInstance, NULL );

	ShowWindow(hMainWnd, SW_SHOWDEFAULT);
	UpdateWindow(hMainWnd);
	SetForegroundWindow(hMainWnd);
	SetFocus(hEntryWnd);
}

static char WC_map[1024] = "";
static Uint WC_maxclients = 0;
static Uint WC_clients = 0;
static Uint WC_port = 0;
static qboolean WC_update = 0;

void
WinCon_SetMapname(const char *mapname)
{
	strncpy(WC_map, mapname, sizeof(WC_map));
	WC_update = 1;
}

void
WinCon_SetMaxClients(Uint maxclients)
{
	WC_maxclients = maxclients;
	WC_update = 1;
}

void
WinCon_SetConnectedClients(Uint clients)
{
	WC_clients = clients;
	WC_update = 1;
}

void
WinCon_SetPort(Uint port)
{
	WC_port = port;
	WC_update = 1;
}

void
WinCon_PumpMessages()
{
	MSG msg;
/*
	char *tmp = NULL;
	Uint i;

	tmp = Info_ValueForKey(svs.info, "maxclients");
	p = Q_atoi(tmp);
	if(lastmaxclients != p)
	{
		lastmaxclients = p;
		updatetitle = 1;
	}

	p = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		client_t   *cl;
		cl = &svs.clients[i];
		if (cl->state == cs_connected || cl->state == cs_spawned || cl->spectator) 
			p++;
	}
	if(lastclients != p)
	{
		lastclients = p;
		updatetitle = 1;
	}
*/
	// if some titlebar info changed, update the titlebar
	if(WC_update)
	{
		char wintitle[1024];
		snprintf(wintitle, sizeof(wintitle), "Twilight - port %d, clients %d/%d, map %s", WC_port, WC_clients, WC_maxclients, WC_map);
		SetWindowText(hMainWnd, wintitle);
		WC_update = 0;
	}

	// pump the messages
	while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg); 
		DispatchMessage(&msg); 
	}
}
#endif
