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
** SDL_GLIMP.C
**
** SDL2 window and OpenGL context creation, replaces the X11/GLX code in
** unix/linux_glimp.c.  This lets the game run natively on X11 and Wayland.
*/

#include <SDL.h>

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "sdl_local.h"

SDL_Window		*SDL_window = NULL;
static SDL_GLContext	SDL_glContext = NULL;

static qboolean		gammaSaved = qfalse;
static Uint16		savedGammaRamp[3][256];

static cvar_t		*r_allowResize;

/*
** GLimp_SetGamma
**
** This routine should only be called if glConfig.deviceSupportsGamma is TRUE
*/
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
	Uint16	table[3][256];
	int		i;

	if ( !SDL_window || !glConfig.deviceSupportsGamma ) {
		return;
	}

	for ( i = 0; i < 256; i++ ) {
		table[0][i] = ( (Uint16) red[i] << 8 ) | red[i];
		table[1][i] = ( (Uint16) green[i] << 8 ) | green[i];
		table[2][i] = ( (Uint16) blue[i] << 8 ) | blue[i];
	}

	if ( SDL_SetWindowGammaRamp( SDL_window, table[0], table[1], table[2] ) < 0 ) {
		ri.Printf( PRINT_DEVELOPER, "SDL_SetWindowGammaRamp() failed: %s\n", SDL_GetError() );
	}
}

/*
** GLimp_Shutdown
*/
void GLimp_Shutdown( void )
{
	IN_DeactivateMouse();

	if ( SDL_window && gammaSaved ) {
		SDL_SetWindowGammaRamp( SDL_window, savedGammaRamp[0], savedGammaRamp[1], savedGammaRamp[2] );
	}
	gammaSaved = qfalse;

	if ( SDL_glContext ) {
		SDL_GL_DeleteContext( SDL_glContext );
		SDL_glContext = NULL;
	}

	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	SDL_QuitSubSystem( SDL_INIT_VIDEO );

	Com_Memset( &glConfig, 0, sizeof( glConfig ) );
	Com_Memset( &glState, 0, sizeof( glState ) );
}

/*
** GLimp_LogComment
*/
void GLimp_LogComment( char *comment )
{
}

/*
** GLimp_DesktopMode
**
** Size of the display the window will open on
*/
static qboolean GLimp_DesktopMode( SDL_DisplayMode *mode )
{
	int display = 0;

	if ( SDL_window ) {
		display = SDL_GetWindowDisplayIndex( SDL_window );
		if ( display < 0 ) {
			display = 0;
		}
	}

	if ( SDL_GetDesktopDisplayMode( display, mode ) < 0 ) {
		ri.Printf( PRINT_WARNING, "Cannot get desktop display mode: %s\n", SDL_GetError() );
		return qfalse;
	}

	return qtrue;
}

/*
** GLimp_CreateWindow
*/
static qboolean GLimp_CreateWindow( int width, int height, qboolean fullscreen, qboolean desktopFullscreen,
									int colorBits, int depthBits, int stencilBits )
{
	Uint32	flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
	int		channelBits = ( colorBits == 16 ) ? 5 : 8;

	if ( fullscreen ) {
		flags |= desktopFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
	} else if ( r_allowResize->integer ) {
		flags |= SDL_WINDOW_RESIZABLE;
	}

	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, channelBits );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, channelBits );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, channelBits );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, depthBits );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, stencilBits );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	// the renderer uses the fixed function pipeline, ask for a compatibility context
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, 0 );

	SDL_window = SDL_CreateWindow( CLIENT_WINDOW_TITLE,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags );
	if ( !SDL_window ) {
		ri.Printf( PRINT_ALL, "...SDL_CreateWindow failed: %s\n", SDL_GetError() );
		return qfalse;
	}

	if ( fullscreen && !desktopFullscreen ) {
		SDL_DisplayMode	wanted, closest;

		Com_Memset( &wanted, 0, sizeof( wanted ) );
		wanted.w = width;
		wanted.h = height;
		if ( SDL_GetClosestDisplayMode( SDL_GetWindowDisplayIndex( SDL_window ), &wanted, &closest ) ) {
			SDL_SetWindowDisplayMode( SDL_window, &closest );
		}
	}

	SDL_glContext = SDL_GL_CreateContext( SDL_window );
	if ( !SDL_glContext ) {
		ri.Printf( PRINT_ALL, "...SDL_GL_CreateContext failed: %s\n", SDL_GetError() );
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
		return qfalse;
	}

	return qtrue;
}

/*
** GLimp_SetMode
*/
static qboolean GLimp_SetMode( void )
{
	SDL_DisplayMode	desktop;
	int		width, height;
	float	aspect;
	int		colorBits, depthBits, stencilBits;
	int		value;
	int		attempt;
	qboolean fullscreen = r_fullscreen->integer ? qtrue : qfalse;
	qboolean desktopFullscreen = qfalse;
	qboolean haveDesktop = GLimp_DesktopMode( &desktop );

	ri.Printf( PRINT_ALL, "...setting mode %d:", r_mode->integer );

	if ( r_mode->integer == -2 ) {
		// native desktop resolution
		if ( !haveDesktop ) {
			desktop.w = 1920;
			desktop.h = 1080;
		}
		width = desktop.w;
		height = desktop.h;
		desktopFullscreen = qtrue;
	} else if ( !R_GetModeInfo( &width, &height, &aspect, r_mode->integer ) ) {
		ri.Printf( PRINT_ALL, " invalid mode\n" );
		return qfalse;
	}

	// a fullscreen mode that matches the desktop doesn't need a mode switch,
	// which avoids flicker and works properly on Wayland
	if ( haveDesktop && width == desktop.w && height == desktop.h ) {
		desktopFullscreen = qtrue;
	}

	ri.Printf( PRINT_ALL, " %d %d %s\n", width, height, fullscreen ? "fullscreen" : "windowed" );

	colorBits = r_colorbits->integer;
	if ( colorBits != 16 ) {
		colorBits = 24;
	}
	depthBits = r_depthbits->integer ? r_depthbits->integer : 24;
	stencilBits = r_stencilbits->integer;

	// try the requested settings first and fall back to safer ones
	for ( attempt = 0; attempt < 4; attempt++ ) {
		int tryDepth = depthBits, tryStencil = stencilBits, tryColor = colorBits;

		if ( attempt >= 1 ) {
			tryStencil = 0;
		}
		if ( attempt >= 2 ) {
			tryDepth = 16;
		}
		if ( attempt >= 3 ) {
			tryColor = 16;
		}

		ri.Printf( PRINT_ALL, "...attempting color %d depth %d stencil %d\n", tryColor, tryDepth, tryStencil );
		if ( GLimp_CreateWindow( width, height, fullscreen, desktopFullscreen, tryColor, tryDepth, tryStencil ) ) {
			break;
		}
	}

	if ( !SDL_window ) {
		return qfalse;
	}

	SDL_GL_GetDrawableSize( SDL_window, &glConfig.vidWidth, &glConfig.vidHeight );
	glConfig.windowAspect = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
	glConfig.isFullscreen = fullscreen;

	SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &value );
	glConfig.colorBits = value * 3;
	SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &value );
	glConfig.depthBits = value;
	SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &value );
	glConfig.stencilBits = value;
	glConfig.stereoEnabled = qfalse;

	if ( SDL_GetWindowDisplayMode( SDL_window, &desktop ) == 0 && fullscreen ) {
		glConfig.displayFrequency = desktop.refresh_rate;
	} else if ( GLimp_DesktopMode( &desktop ) ) {
		glConfig.displayFrequency = desktop.refresh_rate;
	}

	if ( SDL_GL_SetSwapInterval( r_swapInterval->integer ) < 0 ) {
		ri.Printf( PRINT_ALL, "SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError() );
	}
	r_swapInterval->modified = qfalse;

	ri.Printf( PRINT_ALL, "...window %dx%d, drawable %dx%d\n", width, height, glConfig.vidWidth, glConfig.vidHeight );

	return qtrue;
}

/*
** GLimp_InitExtensions
*/
static void GLimp_InitExtensions( void )
{
	if ( !r_allowExtensions->integer )
	{
		ri.Printf( PRINT_ALL, "*** IGNORING OPENGL EXTENSIONS ***\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );

	// GL_S3_s3tc
	glConfig.textureCompression = TC_NONE;
	if ( SDL_GL_ExtensionSupported( "GL_S3_s3tc" ) )
	{
		if ( r_ext_compressed_textures->value )
		{
			glConfig.textureCompression = TC_S3TC;
			ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_S3_s3tc\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_S3_s3tc not found\n" );
	}

	// GL_EXT_texture_env_add
	glConfig.textureEnvAddAvailable = qfalse;
	if ( SDL_GL_ExtensionSupported( "GL_EXT_texture_env_add" ) )
	{
		if ( r_ext_texture_env_add->integer )
		{
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_env_add\n" );
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n" );
	}

	// GL_ARB_multitexture
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	if ( SDL_GL_ExtensionSupported( "GL_ARB_multitexture" ) )
	{
		if ( r_ext_multitexture->value )
		{
			qglMultiTexCoord2fARB = SDL_GL_GetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = SDL_GL_GetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = SDL_GL_GetProcAddress( "glClientActiveTextureARB" );

			if ( qglActiveTextureARB && qglMultiTexCoord2fARB && qglClientActiveTextureARB )
			{
				GLint	maxTextures = 0;

				qglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &maxTextures );
				glConfig.maxActiveTextures = maxTextures;

				if ( glConfig.maxActiveTextures > 1 )
				{
					ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture\n" );
				}
				else
				{
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;
					ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n" );
				}
			}
			else
			{
				qglMultiTexCoord2fARB = NULL;
				qglActiveTextureARB = NULL;
				qglClientActiveTextureARB = NULL;
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );
	}

	// GL_EXT_compiled_vertex_array
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;
	if ( SDL_GL_ExtensionSupported( "GL_EXT_compiled_vertex_array" ) )
	{
		if ( r_ext_compiled_vertex_array->value )
		{
			qglLockArraysEXT = SDL_GL_GetProcAddress( "glLockArraysEXT" );
			qglUnlockArraysEXT = SDL_GL_GetProcAddress( "glUnlockArraysEXT" );
			if ( !qglLockArraysEXT || !qglUnlockArraysEXT )
			{
				qglLockArraysEXT = NULL;
				qglUnlockArraysEXT = NULL;
				ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array has no entry points\n" );
			}
			else
			{
				ri.Printf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n" );
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
	}
}

/*
** GLimp_InitGamma
*/
static void GLimp_InitGamma( void )
{
	glConfig.deviceSupportsGamma = qfalse;
	gammaSaved = qfalse;

	if ( r_ignorehwgamma->integer ) {
		ri.Printf( PRINT_ALL, "...ignoring hardware gamma\n" );
		return;
	}

	// Wayland and some X11 setups can't change the gamma ramp, the
	// renderer then falls back to brightening textures and lightmaps
	if ( SDL_GetWindowGammaRamp( SDL_window, savedGammaRamp[0], savedGammaRamp[1], savedGammaRamp[2] ) < 0 ) {
		ri.Printf( PRINT_ALL, "...hardware gamma not available: %s\n", SDL_GetError() );
		return;
	}

	if ( SDL_SetWindowGammaRamp( SDL_window, savedGammaRamp[0], savedGammaRamp[1], savedGammaRamp[2] ) < 0 ) {
		ri.Printf( PRINT_ALL, "...hardware gamma not available: %s\n", SDL_GetError() );
		return;
	}

	gammaSaved = qtrue;
	glConfig.deviceSupportsGamma = qtrue;
	ri.Printf( PRINT_ALL, "...using hardware gamma\n" );
}

/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.
*/
void GLimp_Init( void )
{
	ri.Printf( PRINT_ALL, "Initializing OpenGL display\n" );

	r_allowResize = ri.Cvar_Get( "r_allowResize", "0", CVAR_ARCHIVE | CVAR_LATCH );

	if ( !SDL_WasInit( SDL_INIT_VIDEO ) ) {
		if ( SDL_InitSubSystem( SDL_INIT_VIDEO ) < 0 ) {
			ri.Error( ERR_FATAL, "GLimp_Init() - SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError() );
		}
		ri.Printf( PRINT_ALL, "SDL video driver: %s\n", SDL_GetCurrentVideoDriver() );
	}

	if ( !GLimp_SetMode() ) {
		// fall back to a mode everything can do
		ri.Printf( PRINT_ALL, "...falling back to 640x480 windowed\n" );
		ri.Cvar_Set( "r_mode", "3" );
		ri.Cvar_Set( "r_fullscreen", "0" );
		r_mode->integer = 3;
		r_fullscreen->integer = 0;
		if ( !GLimp_SetMode() ) {
			ri.Error( ERR_FATAL, "GLimp_Init() - could not create an OpenGL window: %s\n", SDL_GetError() );
		}
	}

	// These values force the UI to disable driver selection
	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;

	// get our config strings
	Q_strncpyz( glConfig.vendor_string, (const char *)qglGetString( GL_VENDOR ), sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (const char *)qglGetString( GL_RENDERER ), sizeof( glConfig.renderer_string ) );
	if ( *glConfig.renderer_string && glConfig.renderer_string[strlen( glConfig.renderer_string ) - 1] == '\n' ) {
		glConfig.renderer_string[strlen( glConfig.renderer_string ) - 1] = 0;
	}
	Q_strncpyz( glConfig.version_string, (const char *)qglGetString( GL_VERSION ), sizeof( glConfig.version_string ) );
	// modern drivers report far more extensions than fit, it is only informational
	Q_strncpyz( glConfig.extensions_string, (const char *)qglGetString( GL_EXTENSIONS ), sizeof( glConfig.extensions_string ) );

	ri.Cvar_Set( "r_lastValidRenderer", glConfig.renderer_string );

	GLimp_InitExtensions();
	GLimp_InitGamma();

}

/*
** GLimp_EndFrame
**
** Responsible for doing a swapbuffers
*/
void GLimp_EndFrame( void )
{
	// don't flip if drawing to front buffer
	if ( Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) != 0 ) {
		SDL_GL_SwapWindow( SDL_window );
	}

	if ( r_swapInterval->modified ) {
		r_swapInterval->modified = qfalse;
		if ( SDL_GL_SetSwapInterval( r_swapInterval->integer ) < 0 ) {
			ri.Printf( PRINT_ALL, "SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError() );
		}
	}
}

/*
===========================================================

SMP acceleration is not supported, the render thread functions are stubs

===========================================================
*/

qboolean GLimp_SpawnRenderThread( void (*function)( void ) )
{
	ri.Printf( PRINT_WARNING, "ERROR: SMP support was disabled at compile time\n" );
	return qfalse;
}

void *GLimp_RendererSleep( void )
{
	return NULL;
}

void GLimp_FrontEndSleep( void )
{
}

void GLimp_WakeRenderer( void *data )
{
}
