/*
 * Copyright (C) 2002 2003 2004 2005 2009 2012, Magnus Hjorth
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
#include <sys/time.h>

#include <gtk/gtk.h>

#include <portaudio.h>

#include "ringbuf.h"
#include "sound.h"
#include "main.h"
#include "gettext.h"

static struct {
     gboolean delay_time_set;
     gfloat delay_time;
     off_t played_bytes;
     guint32 samplerate;
     gint samplesize;
     guint framesize;
     
     GTimeVal start_time;
     
     Ringbuf *output_buffer, *input_buffer;
     int input_overrun_count;
     
     PaDeviceID out, in;
     PortAudioStream *outstream, *instream;
} portaudio_data = { 0 };

// Anropas innan några andra output_-anrop och före gtk_init. 
static gboolean portaudio_init(gboolean silent)
{
     Pa_Initialize();
     portaudio_data.output_buffer = ringbuf_new( 
	  inifile_get_guint32(INI_SETTING_SOUNDBUFSIZE,
                              INI_SETTING_SOUNDBUFSIZE_DEFAULT) );
     portaudio_data.input_buffer = ringbuf_new(65536);
     portaudio_data.out = Pa_GetDefaultOutputDeviceID();
     portaudio_data.in = Pa_GetDefaultInputDeviceID();
     if (portaudio_data.out == paNoDevice)
	  g_warning ( _("sound-portaudio: No output devices available!") );
     if (portaudio_data.in == paNoDevice)
	  g_warning ( _("sound-portaudio: No input devices available!") );
     return TRUE;
}

// Anropas till sist
static void portaudio_quit(void)
{
     ringbuf_free(portaudio_data.output_buffer);
     Pa_Terminate();
}

static gboolean portaudio_samplerate_supported(const PaDeviceInfo *i, 
					       guint32 samplerate)
{
     int x;
     if (i->numSampleRates == -1) {
	  return (samplerate >= i->sampleRates[0] &&
		  samplerate <= i->sampleRates[1]);
     } else {
	  for (x=0; x<i->numSampleRates; x++)
	       if (i->sampleRates[x] == samplerate) return TRUE;
	  return FALSE;
     }
}

static PaSampleFormat portaudio_fmtparse(Dataformat *format)
{     
     if (format->type == DATAFORMAT_FLOAT) {
	  if (format->samplesize == sizeof(float))
	       return paFloat32;
	  else
	       return 0;
     }
     if (XOR(format->bigendian,IS_BIGENDIAN)) return 0;
     switch (format->samplesize) {
     case 1:
	  return format->sign ? paInt8 : paUInt8;
     case 2:
	  return format->sign ? paInt16 : 0;
     case 3:
	  return format->sign ? paPackedInt24 : 0;
     case 4:
	  if (format->packing == 0)
	       return format->sign ? paInt32 : 0;
	  else if (format->packing == 1)
	       return format->sign ? paInt24 : 0;
	  else
	       return 0;
     default:
	  g_assert_not_reached();
	  return 0;
     }
}

static gboolean portaudio_samplesize_supported(const PaDeviceInfo *i, 
					       Dataformat *format)
{
     return ((i->nativeSampleFormats & portaudio_fmtparse(format))!=0);
}

static gboolean portaudio_output_supports_format(Dataformat *format)
{
     const PaDeviceInfo *i = NULL;
     if (portaudio_data.out != paNoDevice)
	  i = Pa_GetDeviceInfo(portaudio_data.out);
     return (i && format->channels <= i->maxOutputChannels && 
	     portaudio_samplerate_supported(i,format->samplerate) &&
	     portaudio_samplesize_supported(i,format));
}


static gboolean portaudio_output_stop(gboolean must_flush)
{
     if (!portaudio_data.outstream) return TRUE;
     if (must_flush) {
	  /* Note: We must check the ringbuffer after checking if the
	   * stream is inactive to avoid races */
	  if (Pa_StreamActive(portaudio_data.outstream)==0 &&
	      !ringbuf_isempty(portaudio_data.output_buffer))
	       Pa_StartStream(portaudio_data.outstream);
	  while (!ringbuf_isempty(portaudio_data.output_buffer)) 
	       do_yield(TRUE);
     }
     Pa_StopStream(portaudio_data.outstream);
     Pa_CloseStream(portaudio_data.outstream);
     portaudio_data.outstream = NULL;
     ringbuf_drain(portaudio_data.output_buffer);
     return must_flush;
}

static void portaudio_output_clear_buffers(void)
{
     Pa_AbortStream(portaudio_data.outstream);
     ringbuf_drain(portaudio_data.output_buffer);
}

static gboolean portaudio_output_want_data(void)
{
     return !ringbuf_isfull(portaudio_data.output_buffer);
}

static int portaudio_output_callback(void *inputBuffer, void *outputBuffer, 
				     unsigned long framesPerBuffer, 
				     PaTimestamp outTime, void *userData)
{
     guint32 i,j;
     i = ringbuf_dequeue(portaudio_data.output_buffer,outputBuffer,
			 framesPerBuffer*portaudio_data.framesize);
     j = framesPerBuffer * portaudio_data.framesize - i;
     if (j>0) memset(((gchar *)outputBuffer)+i, 0, j);
     portaudio_data.played_bytes += i;
     return 0;
}

// output_select_format anropas alltid innan output_play
static gint portaudio_output_select_format(Dataformat *format, gboolean silent)
{
     PaError err;
     if (!portaudio_output_supports_format(format)) return -1;
     portaudio_data.played_bytes = 0;
     err = Pa_OpenStream(&portaudio_data.outstream, 
			 paNoDevice, 0, 0, NULL, /* input */
			 portaudio_data.out, format->channels, 
			 portaudio_fmtparse(format),
			 NULL,/*out*/
			 (double)format->samplerate, 256, 0, 
			 paClipOff|paDitherOff, portaudio_output_callback,NULL);
     if (err != 0) {	  
	  printf(_("Pa_OpenStream failed: %s\n"),Pa_GetErrorText(err));	  
	  return -1;
     }
     portaudio_data.samplerate = format->samplerate;
     portaudio_data.samplesize = format->samplesize;
     portaudio_data.framesize = format->samplesize * format->channels;
     return 0;
}

static guint portaudio_output_play(gchar *buffer, guint bufsize)
{
     guint32 i;
     GTimeVal nowtime,r;
     PaTimestamp p;
     if (!bufsize) return ringbuf_available(portaudio_data.output_buffer);
     i = ringbuf_enqueue(portaudio_data.output_buffer,buffer,bufsize);
     /* To avoid buffer underruns, wait until there is some data in the output
      * buffer... */
     if (Pa_StreamActive(portaudio_data.outstream)==0 && 
	 ringbuf_available(portaudio_data.output_buffer)>4096) {
	  Pa_StartStream(portaudio_data.outstream);
	  g_get_current_time(&portaudio_data.start_time);
	  portaudio_data.delay_time_set = FALSE;
     }
     if (!portaudio_data.delay_time_set) {
	  p = Pa_StreamTime( portaudio_data.outstream );
	  if (p>0) {
	       g_get_current_time(&nowtime);
	       timeval_subtract(&r,&portaudio_data.start_time,&nowtime);
	       portaudio_data.delay_time = 
		    (gfloat)r.tv_sec + (gfloat)r.tv_usec/1000000.0 - 
		    p / (gfloat)portaudio_data.samplerate;
	       portaudio_data.delay_time_set = TRUE;
	  }
     }
     return i;
}

static GList *portaudio_input_supported_formats(gboolean *complete)
{
     *complete = (portaudio_data.in == paNoDevice);
     return NULL;
}

static gboolean portaudio_input_supports_format(Dataformat *format)
{
     const PaDeviceInfo *i = NULL;
     i = Pa_GetDeviceInfo(portaudio_data.in);
     return (i && format->channels <= i->maxOutputChannels && 
	     portaudio_samplerate_supported(i,format->samplerate) &&
	     portaudio_samplesize_supported(i,format));
}

static int portaudio_input_callback(void *inputBuffer, void *outputBuffer, 
				    unsigned long framesPerBuffer, 
				    PaTimestamp outTime, void *userData)
{
     // puts("input_callback");
     ringbuf_enqueue(portaudio_data.input_buffer,inputBuffer,
		     framesPerBuffer*portaudio_data.framesize);
     if (ringbuf_isfull(portaudio_data.input_buffer)) {
	  puts(_("Buffer overrun!"));
          portaudio_data.input_overrun_count ++;
     }
     memset(outputBuffer,0,framesPerBuffer*portaudio_data.framesize); /* hack */
     return 0;
}

static gint portaudio_input_select_format(Dataformat *format, gboolean silent)
{
     PaError err;
     /*printf("in = %d, numInputChannels = %d, inputSampleFormat = %d\n",in,
       format->channels,fmtparse(format->samplesize,format->sign)); */
     if (!portaudio_input_supports_format(format)) return -1;
     
     portaudio_data.input_overrun_count = 0;
     /* PortAudio seems to act strangely when no output device is specified */

     err = Pa_OpenStream(
	  &portaudio_data.instream, 
	  portaudio_data.in, format->channels, 
	  portaudio_fmtparse(format), NULL, /*in*/
	  portaudio_data.out, format->channels, 
	  portaudio_fmtparse(format), NULL, /*out*/
	  (double)format->samplerate, 64, 0, 
	  paClipOff, portaudio_input_callback, NULL);
     if (err != 0) {
	  printf(_("Pa_OpenStream failed: %s\n"),Pa_GetErrorText(err));
	  return -1;
     }
     portaudio_data.samplerate = format->samplerate;
     portaudio_data.framesize = format->samplesize * format->channels;
     Pa_StartStream(portaudio_data.instream);
     return 0;
}

static void portaudio_input_stop(void)
{
     if (!portaudio_data.instream) return;
     Pa_AbortStream(portaudio_data.instream);
     Pa_CloseStream(portaudio_data.instream);
     portaudio_data.instream = NULL;
     ringbuf_drain(portaudio_data.input_buffer);
}

static void portaudio_input_store(Ringbuf *buffer)
{
     ringbuf_transfer(portaudio_data.input_buffer,buffer);
}

static int portaudio_input_overrun_count(void)
{
     return portaudio_data.input_overrun_count;
}
