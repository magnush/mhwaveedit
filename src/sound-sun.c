/*
 * Copyright (C) 2004 2005, Magnus Hjorth
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

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/audio.h>
#include <sys/time.h>
#include <sys/types.h>
#include "gettext.h"

/* Constant for output endian-ness */
/* IS_BIGENDIAN = same as CPU, TRUE = Always big endian, 
 * FALSE = Always little endian */
#define SUNAUD_OUTPUT_ENDIAN IS_BIGENDIAN

static gchar *sunaud_dev;
static int sunaud_fd = -1;
static Ringbuf *sunaud_output_buffer;
static gboolean sunaud_output_drained;
static Dataformat sunaud_format;
static int sunaud_input_overruns;

static void sunaud_output_flush(void);

static gboolean sunaud_init(gboolean silent)
{
     sunaud_dev = getenv("AUDIODEV");
     if (!sunaud_dev) sunaud_dev = "/dev/audio";     
     sunaud_output_buffer = ringbuf_new(
	  inifile_get_guint32(INI_SETTING_SOUNDBUFSIZE,
			      INI_SETTING_SOUNDBUFSIZE_DEFAULT));
     return TRUE;
}

static void sunaud_quit(void)
{
     ringbuf_free(sunaud_output_buffer);
}

static gboolean sunaud_select_format(gboolean rec, Dataformat *format, 
				     gboolean silent)
{
     audio_info_t info;
     audio_prinfo_t *prinfo;
     gchar *c;
     g_assert(sunaud_fd == -1);
     if (format->type == DATAFORMAT_FLOAT || !format->sign ||
	 XOR(format->bigendian,SUNAUD_OUTPUT_ENDIAN)) return TRUE;
     sunaud_fd = open(sunaud_dev,rec?O_RDONLY:O_WRONLY,O_NONBLOCK);
     if (sunaud_fd == -1) {
	  c = g_strdup_printf(_("SunAudio: Couldn't open %s"),sunaud_dev);
	  user_perror(c);
	  g_free(c);
	  return TRUE;
     }
     AUDIO_INITINFO(&info);
     if (rec) prinfo = &(info.record); else prinfo = &(info.play);
     prinfo->sample_rate = format->samplerate;
     prinfo->channels = format->channels;
     prinfo->precision = format->samplesize * 8;
     prinfo->encoding = AUDIO_ENCODING_LINEAR;
     if (ioctl(sunaud_fd, AUDIO_SETINFO, &info) < 0) {
	  close(sunaud_fd);
	  sunaud_fd = -1;
	  return TRUE;
     }
     memcpy(&sunaud_format,format,sizeof(Dataformat));
     if (!rec) {
	  ringbuf_drain(sunaud_output_buffer);
	  sunaud_output_drained = TRUE;
     }
     else sunaud_input_overruns = 0;
     return FALSE;
}

static gint sunaud_output_select_format(Dataformat *format, gboolean silent)
{
     return sunaud_select_format(FALSE,format,silent) ? -1 : 0;
}

static gboolean sunaud_output_suggest_format(Dataformat *format, 
					     Dataformat *result)
{
     if (format->type == DATAFORMAT_FLOAT) return FALSE;
     
     memcpy(result,format,sizeof(Dataformat));
     result->sign = TRUE;
     result->bigendian = SUNAUD_OUTPUT_ENDIAN;
     return TRUE;
}

static gint sunaud_input_select_format(Dataformat *format, gboolean silent)
{
     return sunaud_select_format(TRUE,format,silent) ? -1 : 0;
}

static gboolean sunaud_output_stop(gboolean must_flush)
{
     if (must_flush)
	  while (sunaud_fd >= 0 && !ringbuf_isempty(sunaud_output_buffer)) {
	       sunaud_output_flush();
	       do_yield(TRUE);
	  }
     if (sunaud_fd > 0) close(sunaud_fd);
     sunaud_fd = -1;
     return must_flush;
}

static gboolean sunaud_output_want_data(void)
{
     static int call_count = 0;
     call_count++;
     if ((call_count & 4) == 0) sunaud_output_flush();
     return !ringbuf_isfull(sunaud_output_buffer);
}

static void sunaud_output_flush(void)
{
     static gchar buf[512];
     static int bufsize=0,bufpos=0;
     int loopcount = 0;
     ssize_t sst;
     if (sunaud_output_drained) {
	  bufsize = bufpos = 0;
	  sunaud_output_drained = FALSE;
     }
     for (; loopcount < 5; loopcount++) {
	  if (bufpos == bufsize) {
	       bufsize = ringbuf_dequeue(sunaud_output_buffer,buf,sizeof(buf));
	       bufpos = 0;
	  }
	  if (bufsize == 0 || sunaud_fd < 0) return; 
	  sst = write(sunaud_fd,buf+bufpos,bufsize-bufpos);
	  if (sst == 0) return;
	  if (sst < 0) {
	       if (errno == EAGAIN || errno == EBUSY || errno == EWOULDBLOCK) 
		    return;
	       console_perror(_("SunAudio: Error writing to sound device"));
	       close(sunaud_fd);
	       sunaud_fd = -1;
	       return;
	  }
	  bufpos += sst;
     }
}

static guint sunaud_output_play(gchar *buffer, guint bufsize)
{
     guint32 ui,r;
     if (ringbuf_freespace(sunaud_output_buffer) < bufsize) {
	  sunaud_output_flush();
	  ui = ringbuf_freespace(sunaud_output_buffer);
	  if (ui > bufsize) ui = bufsize;
	  ui -= ui % sunaud_format.samplebytes;	  
     } else
	  ui = bufsize;
     r = ringbuf_enqueue(sunaud_output_buffer,buffer,ui);
     g_assert(r == ui);
     sunaud_output_flush();
     return r;
}

static gfloat sunaud_output_play_min_delay_time(void)
{
     return 0.0;
}

static guint32 sunaud_output_passive_buffer(void)
{
     return ringbuf_available(sunaud_output_buffer);
}

static void sunaud_output_clear_buffers(void)
{
     ringbuf_drain(sunaud_output_buffer);
     sunaud_output_drained = TRUE;
     ioctl(sunaud_fd,AUDIO_DRAIN,NULL);
}

static void sunaud_input_stop(void)
{
     if (sunaud_fd >= 0) {
	  close(sunaud_fd);
	  sunaud_fd = -1;
     }
}

static void sunaud_input_overrun_check(void)
{
     static gboolean last_state = FALSE;
     audio_info_t info;
     int i;
     gboolean b;
     if (ioctl(sunaud_fd,AUDIO_GETINFO,&info) < 0) {
	  console_perror("ioctl");
	  return;
     }
     b = info.record.error != 0;
     if (!last_state && b) sunaud_input_overruns ++;
     last_state = b;
}

static void sunaud_input_store(Ringbuf *buffer)
{
     gchar b[4096];
     guint ui;
     ssize_t sst;     
     gchar *c;
     guint32 r;
     sunaud_input_overrun_check();
     ui = ringbuf_freespace(buffer);
     if (ui > sizeof(b)) ui = sizeof(b);
     ui -= ui % sunaud_format.samplebytes;
     if (ui == 0) return;
     if (sunaud_fd >= 0) {
	  sst = read(sunaud_fd,b,ui);
	  if (sst == -1 && (errno == EBUSY || errno == EAGAIN || 
			    errno == EWOULDBLOCK)) return;
	  if (sst == -1) {
	       user_perror(_("SunAudio: Error reading from sound device"));
	       user_error(c);
	       g_free(c);
	       close(sunaud_fd);
	       sunaud_fd = -1;
	       return;
	  }
	  r = ringbuf_enqueue(buffer,b,sst);
	  g_assert(r == sst);
     }
}

static int sunaud_input_overrun_count(void)
{
     return sunaud_input_overruns;
}
