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
** SDL_SND.C
**
** SDL2 audio output, replaces the OSS /dev/dsp code in unix/linux_snd.c.
** The mixer paints into a ring buffer (dma.buffer) and the SDL audio
** callback streams it to PulseAudio / PipeWire / ALSA.
*/

#include <SDL.h>

#include "../client/snd_local.h"

static qboolean			snd_inited = qfalse;
static SDL_AudioDeviceID	audioDevice = 0;

// read position of the audio callback in the dma buffer, in bytes
static int				dmapos = 0;
static int				dmasize = 0;

static cvar_t			*s_sdlBits;
static cvar_t			*s_sdlSpeed;
static cvar_t			*s_sdlChannels;
static cvar_t			*s_sdlDevSamps;
static cvar_t			*s_sdlMixSamps;

/*
===============
SNDDMA_AudioCallback

Runs on the SDL audio thread, copies the mixed samples to the device
===============
*/
static void SNDDMA_AudioCallback( void *userdata, Uint8 *stream, int len )
{
	int		pos = dmapos;
	int		tobufend, len1, len2;

	if ( !snd_inited || !dma.buffer ) {
		SDL_memset( stream, 0, len );
		return;
	}

	tobufend = dmasize - pos;
	len1 = len;
	len2 = 0;

	if ( len1 > tobufend ) {
		len1 = tobufend;
		len2 = len - len1;
	}

	SDL_memcpy( stream, dma.buffer + pos, len1 );
	if ( len2 <= 0 ) {
		dmapos += len1;
	} else {
		SDL_memcpy( stream + len1, dma.buffer, len2 );
		dmapos = len2;
	}

	if ( dmapos >= dmasize ) {
		dmapos = 0;
	}
}

/*
===============
SNDDMA_Init
===============
*/
qboolean SNDDMA_Init( void )
{
	SDL_AudioSpec	desired, obtained;
	int				tmp;

	if ( snd_inited ) {
		return qtrue;
	}

	s_sdlBits = Cvar_Get( "s_sdlBits", "16", CVAR_ARCHIVE );
	s_sdlSpeed = Cvar_Get( "s_sdlSpeed", "0", CVAR_ARCHIVE );
	s_sdlChannels = Cvar_Get( "s_sdlChannels", "2", CVAR_ARCHIVE );
	s_sdlDevSamps = Cvar_Get( "s_sdlDevSamps", "0", CVAR_ARCHIVE );
	s_sdlMixSamps = Cvar_Get( "s_sdlMixSamps", "0", CVAR_ARCHIVE );

	Com_Printf( "SDL_Init( SDL_INIT_AUDIO )... " );

	if ( !SDL_WasInit( SDL_INIT_AUDIO ) ) {
		if ( SDL_InitSubSystem( SDL_INIT_AUDIO ) < 0 ) {
			Com_Printf( "FAILED (%s)\n", SDL_GetError() );
			return qfalse;
		}
	}

	Com_Printf( "OK\n" );
	Com_Printf( "SDL audio driver is \"%s\".\n", SDL_GetCurrentAudioDriver() );

	Com_Memset( &desired, 0, sizeof( desired ) );
	Com_Memset( &obtained, 0, sizeof( obtained ) );

	tmp = s_sdlBits->integer;
	if ( tmp != 16 && tmp != 8 ) {
		tmp = 16;
	}
	desired.format = ( tmp == 16 ) ? AUDIO_S16SYS : AUDIO_U8;

	desired.freq = s_sdlSpeed->integer;
	if ( !desired.freq ) {
		// the mixer resamples everything to the device rate
		desired.freq = 44100;
	}

	// a reasonably small device buffer keeps the latency down
	if ( s_sdlDevSamps->integer ) {
		desired.samples = s_sdlDevSamps->integer;
	} else if ( desired.freq <= 11025 ) {
		desired.samples = 256;
	} else if ( desired.freq <= 22050 ) {
		desired.samples = 512;
	} else {
		desired.samples = 1024;
	}

	desired.channels = s_sdlChannels->integer == 1 ? 1 : 2;
	desired.callback = SNDDMA_AudioCallback;

	audioDevice = SDL_OpenAudioDevice( NULL, 0, &desired, &obtained,
		SDL_AUDIO_ALLOW_FREQUENCY_CHANGE );
	if ( !audioDevice ) {
		Com_Printf( "SDL_OpenAudioDevice() failed: %s\n", SDL_GetError() );
		SDL_QuitSubSystem( SDL_INIT_AUDIO );
		return qfalse;
	}

	// the mix buffer is a power of two, several times the device buffer
	tmp = s_sdlMixSamps->integer;
	if ( !tmp ) {
		tmp = ( obtained.samples * obtained.channels ) * 10;
	}

	if ( tmp & ( tmp - 1 ) ) {
		int val = 1;
		while ( val < tmp ) {
			val <<= 1;
		}
		tmp = val;
	}

	dmapos = 0;
	dma.samplebits = SDL_AUDIO_BITSIZE( obtained.format );
	dma.channels = obtained.channels;
	dma.samples = tmp;
	dma.submission_chunk = 1;
	dma.speed = obtained.freq;
	dmasize = dma.samples * ( dma.samplebits / 8 );
	dma.buffer = calloc( 1, dmasize );
	if ( !dma.buffer ) {
		Com_Printf( "Failed to allocate the sound mix buffer\n" );
		SDL_CloseAudioDevice( audioDevice );
		audioDevice = 0;
		SDL_QuitSubSystem( SDL_INIT_AUDIO );
		return qfalse;
	}

	// silence for unsigned 8 bit samples is 0x80
	if ( dma.samplebits == 8 ) {
		Com_Memset( dma.buffer, 0x80, dmasize );
	}

	Com_Printf( "SDL audio: %d Hz, %d bit, %d channels, %d samples buffered\n",
		dma.speed, dma.samplebits, dma.channels, dma.samples );

	snd_inited = qtrue;
	SDL_PauseAudioDevice( audioDevice, 0 );

	return qtrue;
}

/*
===============
SNDDMA_GetDMAPos

Return the current sample position (in mono samples read) inside the
recirculating dma buffer, so the mixing code will know how many sample
are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos( void )
{
	return dmapos / ( dma.samplebits / 8 );
}

/*
===============
SNDDMA_Shutdown
===============
*/
void SNDDMA_Shutdown( void )
{
	if ( audioDevice ) {
		SDL_PauseAudioDevice( audioDevice, 1 );
		SDL_CloseAudioDevice( audioDevice );
		audioDevice = 0;
	}

	SDL_QuitSubSystem( SDL_INIT_AUDIO );

	free( dma.buffer );
	dma.buffer = NULL;
	dmapos = dmasize = 0;
	snd_inited = qfalse;

	Com_Printf( "SDL audio device shut down.\n" );
}

/*
===============
SNDDMA_BeginPainting

The mixer is about to write into the dma buffer, keep the callback out
===============
*/
void SNDDMA_BeginPainting( void )
{
	if ( audioDevice ) {
		SDL_LockAudioDevice( audioDevice );
	}
}

/*
===============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit( void )
{
	if ( audioDevice ) {
		SDL_UnlockAudioDevice( audioDevice );
	}
}
