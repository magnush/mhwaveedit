/*
 * Copyright (C) 2005, Magnus Hjorth
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


/* aRts driver using the simpler plain C API */

#include <unistd.h>
#include <artsc.h>

static int mharts_init_status;
static arts_stream_t mharts_play_stream=NULL,mharts_record_stream=NULL;
static Dataformat mharts_format;

static gboolean mharts_check_init_status(void)
{
     gchar *c;
     if (mharts_init_status != 0) {
	  c = g_strdup_printf(_("Unable to connect to aRts server: %s"),
			      arts_error_text(mharts_init_status));
	  user_error(c);
	  g_free(c);
	  return TRUE;
     }
     return FALSE;
}

static gboolean mharts_init(gboolean silent)
{
     mharts_init_status = arts_init();
     if (!silent)
	  mharts_check_init_status();
     return (mharts_init_status == 0);
}

static void mharts_quit(void)
{
     if (mharts_init_status == 0) arts_free();
}

static void mharts_perror(gchar *msg, int errcode)
{
     gchar *c;
     c = g_strdup_printf("aRts: %s: %s",msg,arts_error_text(errcode));
     console_message(c);
     g_free(c);
}

static void mharts_set_nonblock(arts_stream_t stream)
{
     int i;
     i = arts_stream_set(stream,ARTS_P_BLOCKING,0);
     if (i < 0)
	  mharts_perror(_("Failed to set non-blocking mode"),i);
     else if (i != 0)
	  console_message(_("Warning: aRts refuses to set non-blocking mode"));
}

static gint mharts_output_select_format(Dataformat *format, gboolean silent)
{
     if (format->type != DATAFORMAT_PCM || format->samplesize > 2 ||
	 format->channels > 2)
	  return -1;
     mharts_play_stream = arts_play_stream(format->samplerate, 
					   format->samplesize*8, 
					   format->channels, "mhwaveedit");
     if (mharts_play_stream == NULL) {
	  if (!silent) {
	       user_error(_("Failed to start playback"));
	       return 1;
	  }
	  else 
	       return -1;
     }
     mharts_set_nonblock(mharts_play_stream);
     memcpy(&mharts_format,format,sizeof(Dataformat));
     return 0;
}

static gint mharts_input_select_format(Dataformat *format, gboolean silent)
{
     if (format->type != DATAFORMAT_PCM || format->samplesize > 2 ||
	 format->channels > 2)
	  return -1;
     mharts_record_stream = arts_record_stream(format->samplerate,
					       format->samplesize*8,
					       format->channels, 
					       "mhwaveedit_rec");
     if (mharts_record_stream == NULL) {
	  if (!silent) {
	       user_error(_("Failed to start recording"));
	       return 1;
	  } else
	       return -1;
     }
     mharts_set_nonblock(mharts_record_stream);
     memcpy(&mharts_format,format,sizeof(Dataformat));
     return 0;
}

static gboolean mharts_output_suggest_format(Dataformat *format, 
					     Dataformat *result)
{
     if (format->channels > 2)
	  return FALSE;

     memcpy(result,format,sizeof(Dataformat));

     if (format->type != DATAFORMAT_PCM || format->samplesize > 2) {
	  result->type = DATAFORMAT_PCM;
	  result->samplesize = 2;
	  return TRUE;
     } else if (format->samplerate != 44100) {
	  result->samplerate = 44100;
	  return TRUE;
     } else 
	  return FALSE;
}

static gint mharts_buffer_size(void)
{
     int i;
     i = arts_stream_get(mharts_play_stream,ARTS_P_BUFFER_SIZE);
     if (i<0) {
	  mharts_perror(_("Failed to get buffer size"),i);
	  return 65536;
     }
     return i;
}

static gint mharts_buffer_space(void)
{
     int i;
     i = arts_stream_get(mharts_play_stream,ARTS_P_BUFFER_SPACE);
     if (i<0) {
	  mharts_perror(_("Failed to get buffer space"),i);
	  return mharts_buffer_size();
     }
     return i;
}

static gboolean mharts_output_stop(gboolean must_flush)
{
     int i,j,k;
     float f;
     if (mharts_play_stream == NULL) return TRUE;    
     if (must_flush) {	  
	  /* Flush buffers */
	  k = mharts_buffer_size();
	  i = mharts_buffer_space();	  
	  while (i < k) {	       
	       f = (float)(k-i) / (float)mharts_format.samplerate;
	       usleep((unsigned int)(f*1000000.0));
	       j = i;
	       i = mharts_buffer_space();
	       /* To avoid eternal loops in case of buggy behaviour */
	       if (j == i) break; 
	  }	  
     }
     arts_close_stream(mharts_play_stream);
     return must_flush;
}

static gboolean mharts_output_want_data(void)
{
     return (mharts_buffer_size()>0);
}

static guint mharts_output_play(gchar *buffer, guint bufsize)
{
     gint i,j;
     i = mharts_buffer_space();
     if (bufsize == 0) {
	  j = mharts_buffer_size() - i;
	  if (j<0) return 0;
	  else return j;
     }
     if (i == 0) return 0;
     if (bufsize > i) bufsize = i;    
     j = arts_write(mharts_play_stream, buffer, bufsize);
     if (j<0) {
	  mharts_perror(_("write failed"),j);
	  return 0;
     } else 
	  return j;
}

static void mharts_output_clear_buffers(void)
{
     arts_stream_t s;
     Dataformat f;
     int i;
     s = mharts_play_stream;
     memcpy(&f,&mharts_format,sizeof(f));
     i = mharts_output_select_format(&f,TRUE);
     if (i == 0) {
	  arts_close_stream(s);
     } else {
	  mharts_play_stream = s;
     }
}

static void mharts_input_stop(void)
{
     arts_close_stream(mharts_record_stream);
     mharts_record_stream = NULL;
}

static void mharts_input_store(Ringbuf *buffer)
{
     gchar buf[4096];
     int i,j;
     j = ringbuf_freespace(buffer);
     if (j > sizeof(buf)) j = sizeof(buf);
     j -= j % mharts_format.samplebytes;
     i = arts_read(mharts_record_stream, buf, j);
     if (i < 0) 
	  mharts_perror(_("read failed"),i);
}
