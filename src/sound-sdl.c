/*
 * Copyright (C) 2002 2003 2005, Magnus Hjorth
 *
 * This file is part of mhWaveEdit.
 *
 * mhWaveEdit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by        
 * the Free Software Foundation; either version 2 of the License, or  
 * (at your option) any later version.
 *
 * mhWaveEdit is distributed in the hope that it will be useful,   
 * but WITHOUT ANY WARRANTY; without even the implied warranty of  
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mhWaveEdit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "SDL.h"
#include "sound.h" 
#include "ringbuf.h"
#include "gettext.h"

static struct {
     Ringbuf *output_buffer;
} sdl_data;

static gboolean sdl_init(gboolean silent)
{
     gchar *c;
     if (SDL_Init(SDL_INIT_AUDIO) == -1) {
	  c = g_strdup_printf(_("Could not initialize SDL: %s"), 
			      SDL_GetError());
	  console_message(c);
	  g_free(c);
	  exit(1);
     }
     sdl_data.output_buffer = ringbuf_new ( 
	  inifile_get_guint32(INI_SETTING_SOUNDBUFSIZE,
                              INI_SETTING_SOUNDBUFSIZE_DEFAULT) );
     return TRUE;
}

static void sdl_quit(void)
{
     SDL_Quit();
}

static void sdl_output_callback(void *userdata, Uint8 *stream, int len)
{
     guint32 ui;
     ui = ringbuf_dequeue ( sdl_data.output_buffer, stream, len );
     if (ui < len) memset(stream+ui, 0, len-ui);
}

static gint sdl_output_select_format(Dataformat *format, gboolean silent,
				     GVoidFunc ready_func)
{
     gchar *c;
     SDL_AudioSpec desired;
     if (format->type == DATAFORMAT_FLOAT || format->samplesize > 2 || 
	 format->channels > 2) return -1;
     desired.freq = format->samplerate;
     if (format->sign) {
	  if (format->samplesize == 1)
	       desired.format = AUDIO_S8;
	  else if (format->bigendian)
	       desired.format = AUDIO_S16MSB;
	  else
	       desired.format = AUDIO_S16LSB;
     } else {
	  if (format->samplesize == 1)
	       desired.format = AUDIO_U8;
	  else if (format->bigendian)
	       desired.format = AUDIO_U16MSB;
	  else
	       desired.format = AUDIO_U16LSB;
     }
     desired.channels = format->channels;
     desired.samples = 512;
     desired.callback = sdl_output_callback;
     if (SDL_OpenAudio(&desired,NULL) < 0) {
	  c = g_strdup_printf(_("SDL: Couldn't open audio: %s"),
			      SDL_GetError());
	  console_message(c);
	  g_free(c);
	  return -1;
     }
     return 0;
}

static gboolean sdl_output_stop(gboolean must_flush)
{
     if (must_flush)
	  while (ringbuf_available(sdl_data.output_buffer) > 0) do_yield(TRUE);
     if (SDL_GetAudioStatus() != SDL_AUDIO_STOPPED) 
	  SDL_CloseAudio();
     ringbuf_drain(sdl_data.output_buffer);
     return must_flush;
}

static void sdl_output_clear_buffers(void)
{
     SDL_PauseAudio(1);
     ringbuf_drain(sdl_data.output_buffer);
}

static guint sdl_output_play(gchar *buffer, guint bufsize)
{
     guint i;
     if (!bufsize) return ringbuf_available(sdl_data.output_buffer);
     SDL_LockAudio();
     /* printf("output_play: before: %d,%d\n",ringbuf_available(
	sdl_data.output_buffer),ringbuf_freespace(sdl_data.output_buffer)); */
     i = (guint)ringbuf_enqueue( sdl_data.output_buffer, buffer, bufsize );
     /* printf("output_play: after: %d,%d\n",
	ringbuf_available(sdl_data.output_buffer),
	ringbuf_freespace(sdl_data.output_buffer)); */
     SDL_UnlockAudio();
     if (SDL_GetAudioStatus() == SDL_AUDIO_PAUSED)
	  SDL_PauseAudio(0);
     return i;
}

static gboolean sdl_output_want_data(void)
{
     return !ringbuf_isfull(sdl_data.output_buffer);
}

