/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
===========================================================================
*/
/*
** linux_snd.c -- SDL2 audio backend (replaces OSS/ALSA)
** Works on modern Linux with PipeWire/PulseAudio/ALSA via SDL2
*/

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../game/q_shared.h"
#include "../client/snd_local.h"

static int dma_samples;
static SDL_AudioDeviceID audio_device = 0;
static qboolean snd_inited = 0;
static volatile int snd_dma_pos = 0;        // copy offset in the ring, wraps
static volatile int snd_samples_played = 0; // monotonic, in 16-bit samples

cvar_t *sndbits;
cvar_t *sndspeed;
cvar_t *sndchannels;

static qboolean use_custom_memset = qfalse;
extern int s_paintedtime;
void Snd_Memset(void* dest, const int val, const size_t count)
{
    int *pDest;
    int i, iterate;
    if (!use_custom_memset)
    {
        Com_Memset(dest, val, count);
        return;
    }
    iterate = count / sizeof(int);
    pDest = (int*)dest;
    for(i = 0; i < iterate; i++)
        pDest[i] = val;
}

static void Q3_AudioCallback(void *userdata, Uint8 *stream, int len)
{
    // copy len bytes from the dma buffer ring starting at snd_dma_pos, wrapping
    int ring_bytes = dma.samples * (dma.samplebits / 8);
    int copy = (len < ring_bytes) ? len : ring_bytes;
    int byte_pos = snd_dma_pos * (dma.samplebits / 8);
    int first = ring_bytes - byte_pos;
    if (first >= copy) {
        memcpy(stream, dma.buffer + byte_pos, copy);
    } else {
        memcpy(stream, dma.buffer + byte_pos, first);
        memcpy(stream + first, dma.buffer, copy - first);
    }
    if (copy < len)
        memset(stream + copy, 0, len - copy);
    snd_dma_pos = (snd_dma_pos + copy / (dma.samplebits / 8)) % dma.samples;
    snd_samples_played += copy / (dma.samplebits / 8);
}

qboolean SNDDMA_Init(void)
{
    SDL_AudioSpec desired, obtained;
    
    if (snd_inited)
        return 1;

    sndbits = Cvar_Get("sndbits", "16", CVAR_ARCHIVE);
    sndspeed = Cvar_Get("sndspeed", "0", CVAR_ARCHIVE);
    sndchannels = Cvar_Get("sndchannels", "2", CVAR_ARCHIVE);

    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            Com_Printf("SDL: cannot init audio: %s\n", SDL_GetError());
            return 0;
        }
    }

    dma.samplebits = 16;
    dma.speed = 44100;
    dma.channels = 2;
    dma.submission_chunk = 1;
    dma.samples = 8192; /* power of two; must exceed 2 * s_mixPreStep * speed (2*0.05*44100=4410) */
    dma_samples = dma.samples;

    dma.buffer = (unsigned char *)malloc(dma.samples * (dma.samplebits / 8));
    if (!dma.buffer) {
        Com_Printf("SDL: cannot allocate mix buffer\n");
        return 0;
    }

    memset(&desired, 0, sizeof(desired));
    desired.freq = dma.speed;
    desired.format = AUDIO_S16SYS;
    desired.channels = dma.channels;
    desired.samples = dma.samples / dma.channels / 4;  // frames per SDL buffer (quarter ring)
    desired.callback = Q3_AudioCallback;

    audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (audio_device == 0) {
        Com_Printf("SDL: cannot open audio device: %s\n", SDL_GetError());
        free(dma.buffer);
        dma.buffer = NULL;
        return 0;
    }

    SDL_PauseAudioDevice(audio_device, 0);
    snd_inited = 1;
    Com_Printf("SDL: opened audio %d Hz, %d ch, %d samples\n",
               obtained.freq, obtained.channels, obtained.samples);
    return 1;
}

int SNDDMA_GetDMAPos(void)
{
    if (!snd_inited || !audio_device) return 0;
    return snd_samples_played;
}

void SNDDMA_Shutdown(void)
{
    if (audio_device) {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }
    if (dma.buffer) {
        free(dma.buffer);
        dma.buffer = NULL;
    }
    snd_inited = 0;
}

void SNDDMA_Submit(void)
{
    // SDL callback handles mixing
}

void SNDDMA_BeginPainting(void)
{
    // No-op
}