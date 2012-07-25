/*
 * Copyright (C) 2002 2003 2009 2010, Magnus Hjorth
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



/* This is the "dummy" sound driver. It pretends to output and input data but 
 * does nothing. It it used if no supported sound libraries were found. It also 
 * has some debugging code built into it to check that the program does not call
 * the sound driver in a wrong way. (e.g. not calling sound_init first).
 */

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "sound.h"

static struct {
     gint state;
     int lasttime;
     Dataformat format;
     gint input_bytes_available;
     gint output_bytes_buffered;
} dummy_data = {1};


static gboolean dummy_init(gboolean silent)
{
     g_assert(dummy_data.state == 1);
     dummy_data.state = 2;
     return TRUE;
}

static void dummy_quit(void)
{
     g_assert(dummy_data.state == 2);
     dummy_data.state = 5;
}

static gint dummy_output_select_format(Dataformat *format, gboolean silent,
				       GVoidFunc ready_func)
{
     g_assert(dummy_data.state == 2);
     dummy_data.state = 3;
     memcpy(&dummy_data.format,format,sizeof(Dataformat));
     dummy_data.output_bytes_buffered = 0;
     return 0;
}

static gboolean dummy_output_stop(gboolean must_flush)
{
     g_assert(dummy_data.state == 2 || dummy_data.state == 3);
     dummy_data.state = 2;
     return TRUE;
}

static void dummy_output_checkbuf(void)
{
     time_t t;
     t = time(0);
     if (t != dummy_data.lasttime) {
	  dummy_data.output_bytes_buffered -= 
	       dummy_data.format.samplerate*dummy_data.format.samplebytes;	  
	  if (dummy_data.output_bytes_buffered < 0 || t > dummy_data.lasttime+1)
	       dummy_data.output_bytes_buffered = 0;
	  dummy_data.lasttime = t;
     }
}

static gboolean dummy_output_want_data(void)
{
     g_assert(dummy_data.state == 3);
     dummy_output_checkbuf();
     return (dummy_data.output_bytes_buffered < 
	     2 * dummy_data.format.samplerate * dummy_data.format.samplebytes);
}

static guint dummy_output_play(gchar *buffer, guint bufsize)
{
     gint i;
     g_assert(dummy_data.state == 3);
     dummy_output_checkbuf();
     i = 2*dummy_data.format.samplerate*dummy_data.format.samplebytes;
     i -= dummy_data.output_bytes_buffered;
     if (i > bufsize) i = bufsize;
     dummy_data.output_bytes_buffered += i;
     return i;
}

static void dummy_output_clear_buffers(void)
{
     g_assert(dummy_data.state == 3);
     dummy_data.output_bytes_buffered = 0;
}

static GList *dummy_input_supported_formats(gboolean *complete)
{
     g_assert(dummy_data.state == 2);
     *complete = FALSE;
     return NULL;
}

static gint dummy_input_select_format(Dataformat *format, gboolean silent,
				      GVoidFunc ready_func)
{
     g_assert(dummy_data.state == 2);
     /* if (format->samplerate < 100000 && format->samplerate > 0) return -1; */
     memcpy(&dummy_data.format,format,sizeof(Dataformat));
     dummy_data.input_bytes_available = 0;
     dummy_data.state = 4;
     return 0;
}

static void dummy_input_stop(void)
{
     g_assert(dummy_data.state == 4 || dummy_data.state == 2);
     dummy_data.state = 2;
}

static void dummy_input_store(Ringbuf *buffer)
{
     guint32 i;
     time_t t;
     g_assert(dummy_data.state == 4);
     t = time(0);
     if (t != dummy_data.lasttime) {
	  dummy_data.input_bytes_available += 
	       dummy_data.format.samplerate * dummy_data.format.samplebytes;
	  dummy_data.lasttime = t;
     }
     i = MIN(ringbuf_freespace(buffer),dummy_data.input_bytes_available);
     dummy_data.input_bytes_available -= ringbuf_enqueue_zeroes(buffer,i);
}

