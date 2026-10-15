/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
/*
** SDL_INPUT.C
**
** SDL2 keyboard and mouse input, replaces the X11 event code in
** unix/linux_glimp.c
*/

#include <SDL.h>

#include "../client/client.h"
#include "sdl_local.h"

static cvar_t	*in_mouse;
static cvar_t	*in_nograb;

static qboolean	mouseAvailable = qfalse;
static qboolean	mouseActive = qfalse;
static qboolean	inputInitialized = qfalse;

/*
===============
IN_TranslateSDLKey

Map an SDL key to a quake key number, 0 if it has no mapping
===============
*/
static int IN_TranslateSDLKey( const SDL_Keysym *keysym )
{
	SDL_Keycode	sym = keysym->sym;

	// the console key is the one below escape regardless of keyboard layout
	if ( keysym->scancode == SDL_SCANCODE_GRAVE ) {
		return '`';
	}

	switch ( sym ) {
	case SDLK_TAB:			return K_TAB;
	case SDLK_RETURN:		return K_ENTER;
	case SDLK_ESCAPE:		return K_ESCAPE;
	case SDLK_SPACE:		return K_SPACE;
	case SDLK_BACKSPACE:	return K_BACKSPACE;
	case SDLK_CAPSLOCK:		return K_CAPSLOCK;
	case SDLK_PAUSE:		return K_PAUSE;
	case SDLK_POWER:		return K_POWER;

	case SDLK_UP:			return K_UPARROW;
	case SDLK_DOWN:			return K_DOWNARROW;
	case SDLK_LEFT:			return K_LEFTARROW;
	case SDLK_RIGHT:		return K_RIGHTARROW;

	case SDLK_LALT:
	case SDLK_RALT:			return K_ALT;
	case SDLK_LCTRL:
	case SDLK_RCTRL:		return K_CTRL;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:		return K_SHIFT;
	case SDLK_LGUI:
	case SDLK_RGUI:			return K_COMMAND;

	case SDLK_INSERT:		return K_INS;
	case SDLK_DELETE:		return K_DEL;
	case SDLK_PAGEDOWN:		return K_PGDN;
	case SDLK_PAGEUP:		return K_PGUP;
	case SDLK_HOME:			return K_HOME;
	case SDLK_END:			return K_END;

	case SDLK_F1:			return K_F1;
	case SDLK_F2:			return K_F2;
	case SDLK_F3:			return K_F3;
	case SDLK_F4:			return K_F4;
	case SDLK_F5:			return K_F5;
	case SDLK_F6:			return K_F6;
	case SDLK_F7:			return K_F7;
	case SDLK_F8:			return K_F8;
	case SDLK_F9:			return K_F9;
	case SDLK_F10:			return K_F10;
	case SDLK_F11:			return K_F11;
	case SDLK_F12:			return K_F12;
	case SDLK_F13:			return K_F13;
	case SDLK_F14:			return K_F14;
	case SDLK_F15:			return K_F15;

	case SDLK_KP_7:			return K_KP_HOME;
	case SDLK_KP_8:			return K_KP_UPARROW;
	case SDLK_KP_9:			return K_KP_PGUP;
	case SDLK_KP_4:			return K_KP_LEFTARROW;
	case SDLK_KP_5:			return K_KP_5;
	case SDLK_KP_6:			return K_KP_RIGHTARROW;
	case SDLK_KP_1:			return K_KP_END;
	case SDLK_KP_2:			return K_KP_DOWNARROW;
	case SDLK_KP_3:			return K_KP_PGDN;
	case SDLK_KP_ENTER:		return K_KP_ENTER;
	case SDLK_KP_0:			return K_KP_INS;
	case SDLK_KP_PERIOD:	return K_KP_DEL;
	case SDLK_KP_DIVIDE:	return K_KP_SLASH;
	case SDLK_KP_MINUS:		return K_KP_MINUS;
	case SDLK_KP_PLUS:		return K_KP_PLUS;
	case SDLK_NUMLOCKCLEAR:	return K_KP_NUMLOCK;
	case SDLK_KP_MULTIPLY:	return K_KP_STAR;
	case SDLK_KP_EQUALS:	return K_KP_EQUALS;

	default:
		break;
	}

	// printable keys use their (lower case) character
	if ( sym > 32 && sym < 127 ) {
		return sym;
	}

	// keys on non-US layouts that produce latin-1 characters
	if ( sym >= 160 && sym < 256 ) {
		return sym;
	}

	return 0;
}

/*
===============
IN_ActivateMouse
===============
*/
static void IN_ActivateMouse( void )
{
	if ( !mouseAvailable || !SDL_window ) {
		return;
	}

	if ( !mouseActive ) {
		if ( !in_nograb->integer ) {
			SDL_SetRelativeMouseMode( SDL_TRUE );
			SDL_SetWindowGrab( SDL_window, SDL_TRUE );
		}
		// throw away any motion that happened while we weren't active
		SDL_GetRelativeMouseState( NULL, NULL );
		mouseActive = qtrue;
	}
}

/*
===============
IN_DeactivateMouse
===============
*/
void IN_DeactivateMouse( void )
{
	if ( !mouseActive ) {
		return;
	}

	SDL_SetRelativeMouseMode( SDL_FALSE );
	if ( SDL_window ) {
		SDL_SetWindowGrab( SDL_window, SDL_FALSE );
		// put the cursor somewhere sensible when releasing it
		if ( !( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_FULLSCREEN ) ) {
			int w, h;
			SDL_GetWindowSize( SDL_window, &w, &h );
			SDL_WarpMouseInWindow( SDL_window, w / 2, h / 2 );
		}
	}
	mouseActive = qfalse;
}

/*
===============
IN_Init
===============
*/
void IN_Init( void )
{
	Com_Printf( "\n------- Input Initialization -------\n" );

	in_mouse = Cvar_Get( "in_mouse", "1", CVAR_ARCHIVE );
	// developer feature, allows to break without loosing mouse pointer
	in_nograb = Cvar_Get( "in_nograb", "0", 0 );

	if ( !SDL_WasInit( SDL_INIT_EVENTS ) ) {
		if ( SDL_InitSubSystem( SDL_INIT_EVENTS ) < 0 ) {
			Com_Error( ERR_FATAL, "IN_Init: SDL_Init(SDL_INIT_EVENTS) failed: %s", SDL_GetError() );
		}
	}

	mouseAvailable = in_mouse->integer ? qtrue : qfalse;
	mouseActive = qfalse;

	// we generate the characters for the console from text input events
	SDL_StartTextInput();

	inputInitialized = qtrue;
	Com_Printf( "------------------------------------\n" );
}

/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown( void )
{
	IN_DeactivateMouse();
	SDL_StopTextInput();
	mouseAvailable = qfalse;
	inputInitialized = qfalse;
}

/*
===============
IN_ProcessEvents
===============
*/
static void IN_ProcessEvents( void )
{
	SDL_Event	e;
	int			key;

	if ( !SDL_WasInit( SDL_INIT_EVENTS ) ) {
		return;
	}

	while ( SDL_PollEvent( &e ) ) {
		switch ( e.type ) {
		case SDL_KEYDOWN:
			key = IN_TranslateSDLKey( &e.key.keysym );

			// alt-enter toggles fullscreen
			if ( key == K_ENTER && ( e.key.keysym.mod & KMOD_ALT ) ) {
				Cvar_SetValue( "r_fullscreen", !Cvar_VariableIntegerValue( "r_fullscreen" ) );
				Cbuf_AddText( "vid_restart\n" );
				break;
			}

			if ( key ) {
				Sys_QueEvent( 0, SE_KEY, key, qtrue, 0, NULL );
			}

			// the console line editor expects these as characters
			if ( key == K_BACKSPACE ) {
				Sys_QueEvent( 0, SE_CHAR, '\b', 0, 0, NULL );
			} else if ( ( e.key.keysym.mod & KMOD_CTRL ) && e.key.keysym.sym >= SDLK_a && e.key.keysym.sym <= SDLK_z ) {
				// ctrl-v paste, ctrl-c clear, etc.
				Sys_QueEvent( 0, SE_CHAR, e.key.keysym.sym - SDLK_a + 1, 0, 0, NULL );
			}
			break;

		case SDL_KEYUP:
			key = IN_TranslateSDLKey( &e.key.keysym );
			if ( key ) {
				Sys_QueEvent( 0, SE_KEY, key, qfalse, 0, NULL );
			}
			break;

		case SDL_TEXTINPUT:
			{
				const unsigned char	*c = (const unsigned char *)e.text.text;

				// decode utf-8, quake only understands latin-1
				while ( *c ) {
					int ch = *c++;

					if ( ch >= 0xC0 && ch < 0xE0 && ( *c & 0xC0 ) == 0x80 ) {
						ch = ( ( ch & 0x1F ) << 6 ) | ( *c++ & 0x3F );
					} else if ( ch >= 0x80 ) {
						// skip multibyte characters we can't show
						while ( ( *c & 0xC0 ) == 0x80 ) {
							c++;
						}
						continue;
					}

					// the console key toggles the console, don't type it
					if ( ch == '`' || ch == '~' ) {
						continue;
					}

					if ( ch >= 32 && ch < 256 && ch != 127 ) {
						Sys_QueEvent( 0, SE_CHAR, ch, 0, 0, NULL );
					}
				}
			}
			break;

		case SDL_MOUSEMOTION:
			if ( mouseActive && ( e.motion.xrel || e.motion.yrel ) ) {
				Sys_QueEvent( 0, SE_MOUSE, e.motion.xrel, e.motion.yrel, 0, NULL );
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if ( mouseActive ) {
				switch ( e.button.button ) {
				case SDL_BUTTON_LEFT:	key = K_MOUSE1; break;
				case SDL_BUTTON_RIGHT:	key = K_MOUSE2; break;
				case SDL_BUTTON_MIDDLE:	key = K_MOUSE3; break;
				case SDL_BUTTON_X1:		key = K_MOUSE4; break;
				case SDL_BUTTON_X2:		key = K_MOUSE5; break;
				default:				key = 0; break;
				}
				if ( key ) {
					Sys_QueEvent( 0, SE_KEY, key, e.type == SDL_MOUSEBUTTONDOWN, 0, NULL );
				}
			} else if ( e.type == SDL_MOUSEBUTTONDOWN ) {
				// clicking the window takes the mouse back
				IN_ActivateMouse();
			}
			break;

		case SDL_MOUSEWHEEL:
			if ( e.wheel.y > 0 ) {
				Sys_QueEvent( 0, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
				Sys_QueEvent( 0, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
			} else if ( e.wheel.y < 0 ) {
				Sys_QueEvent( 0, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
				Sys_QueEvent( 0, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
			}
			break;

		case SDL_WINDOWEVENT:
			switch ( e.window.event ) {
			case SDL_WINDOWEVENT_FOCUS_LOST:
				IN_DeactivateMouse();
				// don't leave keys stuck down
				Key_ClearStates();
				break;
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				if ( SDL_window && !( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_FULLSCREEN ) &&
					Cvar_VariableIntegerValue( "r_allowResize" ) ) {
					int w, h;
					SDL_GL_GetDrawableSize( SDL_window, &w, &h );
					if ( w != cls.glconfig.vidWidth || h != cls.glconfig.vidHeight ) {
						Cvar_Set( "r_mode", "-1" );
						Cvar_SetValue( "r_customwidth", w );
						Cvar_SetValue( "r_customheight", h );
						Cbuf_AddText( "vid_restart\n" );
					}
				}
				break;
			}
			break;

		case SDL_QUIT:
			Cbuf_ExecuteText( EXEC_NOW, "quit Closed window\n" );
			break;

		default:
			break;
		}
	}
}

/*
===============
IN_Frame
===============
*/
void IN_Frame( void )
{
	qboolean	wantMouse;

	if ( !inputInitialized ) {
		return;
	}

	if ( !SDL_window ) {
		mouseActive = qfalse;
		return;
	}

	wantMouse = qtrue;

	// release the mouse when the console is down and we're on the desktop
	if ( ( cls.keyCatchers & KEYCATCH_CONSOLE ) &&
		!( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_FULLSCREEN ) ) {
		wantMouse = qfalse;
	}

	// don't steal the mouse while we're in the background
	if ( !( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_INPUT_FOCUS ) ) {
		wantMouse = qfalse;
	}

	if ( wantMouse ) {
		IN_ActivateMouse();
	} else {
		IN_DeactivateMouse();
	}
}

/*
===============
IN_Activate
===============
*/
void IN_Activate( void )
{
}

/*
===============
Sys_SendKeyEvents

Called from Sys_GetEvent to pump the event loop
===============
*/
void Sys_SendKeyEvents( void )
{
	IN_ProcessEvents();
}

/*
===============
Sys_GetClipboardData

The caller frees the returned string with Z_Free
===============
*/
char *Sys_GetClipboardData( void )
{
	char	*text, *data;

	if ( !SDL_HasClipboardText() ) {
		return NULL;
	}

	text = SDL_GetClipboardText();
	if ( !text ) {
		return NULL;
	}

	data = NULL;
	if ( text[0] ) {
		int		len = strlen( text ) + 1;

		data = Z_Malloc( len );
		Q_strncpyz( data, text, len );
		strtok( data, "\n\r\b" );
	}
	SDL_free( text );

	return data;
}
