/*
 * Copyright (C) 2002 2003 2004 2005 2006 2008 2010, Magnus Hjorth
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


#if defined(HAVE_SYS_SOUNDCARD_H)
#include <sys/soundcard.h>
#elif defined(HAVE_SOUNDCARD_H)
#include <soundcard.h>
#else
#warning "Header file not found for OSS driver!"
#endif

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/types.h>

#include <gtk/gtk.h>

#include "ringbuf.h"
#include "um.h"
#include "main.h"
#include "gettext.h"
#include "mainloop.h"

#define OSS_PCMFILE_PLAYBACK "OSSdevice"
#define OSS_PCMFILE_RECORD "OSSRecDevice"

#ifdef __OpenBSD__
#define OSS_PCMFILE_DEFAULT "/dev/audio"
#else
#define OSS_PCMFILE_DEFAULT "/dev/dsp"
#endif

#define OSS_NOSELECT "OSSAvoidSelect"
#define OSS_NOSELECT_DEFAULT FALSE

static Ringbuf *oss_output_buffer;
static int oss_fd;
static int oss_format;
static int oss_samplesize;
static int oss_samplerate;
static int oss_channels;
static int oss_samplebytes_in,oss_samplebytes_out;
static gboolean oss_noselect = FALSE;
static gpointer play_source;

static int oss_output_flush(gpointer tsource, GTimeVal *tv, gpointer user_data);

static gboolean oss_init(gboolean silent)
{
     oss_output_buffer = ringbuf_new(
	  inifile_get_guint32(INI_SETTING_SOUNDBUFSIZE,
			      INI_SETTING_SOUNDBUFSIZE_DEFAULT));
     oss_noselect = inifile_get_gboolean(OSS_NOSELECT,OSS_NOSELECT_DEFAULT);
     play_source = mainloop_time_source_add(NULL, oss_output_flush, NULL);
     /* When autodetecting, this is the last tested driver before the dummy,
      * so we might as well return TRUE */
     return TRUE;
}

static void oss_quit(void)
{
     mainloop_time_source_free(play_source);
     ringbuf_free(oss_output_buffer);
}

static int oss_errdlg_open(char *filename, int flags, gboolean silent)
{
     int fd;
     gchar *c;
     fd = open(filename,flags);
     if (fd == -1 && !silent) {
	  c = g_strdup_printf(_("Could not open '%s': %s"),filename,strerror(errno));
	  user_error(c);
	  g_free(c);
     }
     return fd;
}

static int oss_errdlg_ioctl(int filedes, int cmd, void *arg, gboolean silent)
{
     int i;
     gchar *c;
     i = ioctl(filedes,cmd,arg);
     if (i == -1 && errno != EPIPE && !silent) {
	  c = g_strdup_printf(_("Error in sound driver: ioctl failed: %s"),strerror(errno));
	  user_error(c);
	  g_free(c);
     }
     return i;
}

static gint oss_try_format(Dataformat *format, gboolean input, gboolean silent)
{
     gchar *fname;
     int i,j;
     /* For now, we just refuse floating-point format. We should probably 
      * use a fallback format instead */
     if (format->type == DATAFORMAT_FLOAT) return -1;
     /* Determine the right format */
     oss_samplesize = format->samplesize;
     switch (oss_samplesize) {
     case 1:
	  oss_format = (format->sign) ? AFMT_S8 : AFMT_U8;
	  break;
     case 2:
	  if (!format->bigendian)
	       oss_format = (format->sign) ? AFMT_S16_LE : AFMT_U16_LE;
	  else
	       oss_format = (format->sign) ? AFMT_S16_BE : AFMT_U16_BE;
	  break;
     case 3:
     case 4:
	  if (!format->sign) return -1;
	  if (format->packing == 2) return -1;
	  /* This is really hairy, but the AFMT_S32-constants don't seem to be 
	   * defined in all soundcard.h files */
	  if (format->bigendian) {
#ifdef AFMT_S32_BE
	       oss_format = AFMT_S32_BE;
#else
	       return -1;
#endif
	  } else {
#ifdef AFMT_S32_LE
	  oss_format = AFMT_S32_LE;
#else
	  return -1;
#endif
	  }
	  break;
     default:
	  g_assert_not_reached();
     }
     /* Open the file */
     fname = inifile_get(OSS_PCMFILE_PLAYBACK,OSS_PCMFILE_DEFAULT);
     if (input)
	  fname = inifile_get(OSS_PCMFILE_RECORD, fname);
     oss_fd = oss_errdlg_open(fname, input ? O_RDONLY : O_WRONLY, silent);
     if (oss_fd == -1) return silent?-1:+1;
     /* Try to set the format */
     j = oss_format;
     i = oss_errdlg_ioctl(oss_fd, SNDCTL_DSP_SETFMT, &j, silent);
     if (i == -1 || j != oss_format) { 
	  close(oss_fd); 
	  oss_fd=-1; 
	  return (i != -1 || silent)?-1:+1; 
     }
     /* Try to set the number of channels */
     j = oss_channels = format->channels;
     i = oss_errdlg_ioctl(oss_fd, SNDCTL_DSP_CHANNELS, &j, silent);
     if (i==-1 || j != oss_channels) { 
	  close(oss_fd); 
	  oss_fd=-1; 
	  return (i != -1 || silent)?-1:+1; 
     }
     /* Try to set the sample rate */
     j = oss_samplerate = format->samplerate;
     i = oss_errdlg_ioctl(oss_fd, SNDCTL_DSP_SPEED, &j, silent);
     /* FIXME: Variable tolerance (esp. for input) */
     /* Currently tolerates 5% difference between requested/received samplerate
      */
     if (i==-1 || abs(j-oss_samplerate) > oss_samplerate/20) {
	  close(oss_fd); 
	  oss_fd=-1; 
	  return (i != -1 || silent)?-1:+1;
     }
     /* Everything went well! */
     oss_samplebytes_in = oss_samplebytes_out = oss_channels * oss_samplesize;     
     if (oss_samplesize == 3)
	  oss_samplebytes_out += oss_channels;
     return 0;
}

static gint oss_output_select_format(Dataformat *format, gboolean silent,
				     GVoidFunc ready_func)
{
     return oss_try_format(format,FALSE,silent);
}

static gint oss_input_select_format(Dataformat *format, gboolean silent,
				    GVoidFunc ready_func)
{     
     return oss_try_format(format,TRUE,silent);
}

static int oss_errdlg_write(int fd, gchar *buffer, size_t size)
{
     ssize_t s;
     gchar *c;
     s = write(fd,buffer,size);
     if (s == -1) {
	  c = g_strdup_printf(_("Error in sound driver: write failed: %s"),strerror(errno));
	  user_error(c);
	  g_free(c);
     } 
     return s;
}

static int oss_errdlg_read(int fd, gchar *buffer, size_t size)
{
     ssize_t s;
     gchar *c;
     s = read(fd,buffer,size);
     if (s == -1) {
	  c = g_strdup_printf(_("Error in sound driver: read failed: %s"),strerror(errno));
	  user_error(c);
	  g_free(c);
     } 
     return s;
}

static int oss_output_flush(gpointer ts, GTimeVal *tv, gpointer ud)
{
     fd_set a;
     struct timeval t = { 0 };
     int i;
     guint u;
     gchar *c;
     audio_buf_info info;
     /* printf("oss_output_flush: ringbuf_available: %d\n",ringbuf_available(oss_output_buffer)); */
     /* Do we have anything to write at all? */
     if (ringbuf_available(oss_output_buffer) == 0) return 0;
     /* Do a select call and see if it's ready for writing */
     if (!oss_noselect) {
	  FD_ZERO(&a);
	  FD_SET(oss_fd, &a);
	  i = select(FD_SETSIZE, NULL, &a, NULL, &t);
	  if (i==-1) {
	       c = g_strdup_printf(_("Error in sound driver: select failed: %s"),strerror(errno));
	       user_error(c);
	       return 2000;
	  }
	  if (i==0) return 50;
     }
#ifdef SNDCTL_DSP_GETOSPACE
     /* Now do an ioctl call to check the number of writable bytes */
     i = oss_errdlg_ioctl(oss_fd, SNDCTL_DSP_GETOSPACE, &info, TRUE);
     if (i == -1 && errno == EPIPE) {
	  /* This is a workaround for a bug in ALSA 0.9.6 OSS emulation, where
	   * this call can return "broken pipe", especially after a 
	   * SNDCTL_DSP_RESET call. */	  
	  info.fragments = 1024;
	  info.fragsize = 1*2*3*4;
     } else if (i == -1) return 0;
#else
     /* Fill in dummy values */
     info.fragments = 1024;
     info.fragsize = 1*2*3*4;
#endif
     /* Calculate the number of bytes to write (into u) */
     u = ringbuf_available(oss_output_buffer);
     if (u > info.fragsize)
	  u -= u % info.fragsize;
     u = MIN(u, info.fragments*info.fragsize);
     if (u == 0) return 50;
     /* Write it out! */
     c = g_malloc(u);
     ringbuf_dequeue(oss_output_buffer, c, u);
     i = oss_errdlg_write(oss_fd, c, u);
     g_free(c);
     return 10;
}

static gboolean oss_output_want_data(void)
{
     /* oss_output_flush(); */
     return (ringbuf_freespace(oss_output_buffer) >= oss_samplebytes_out);
}

static guint oss_output_play(gchar *buffer, guint bufsize)
{
     guint u=0;
     GTimeVal t;
     guint bs0 = bufsize;
     /* 24-bit samples need padding */
     if (oss_samplesize == 3) {
	  while (bufsize >= 3 && ringbuf_freespace(oss_output_buffer)>=oss_samplebytes_out) {
	       for (u=0; u<oss_channels; u++) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
		    ringbuf_enqueue(oss_output_buffer,"",1);
		    ringbuf_enqueue(oss_output_buffer,buffer,3);
#else
		    ringbuf_enqueue(oss_output_buffer,buffer,3);
		    ringbuf_enqueue(oss_output_buffer,"",1);
#endif
		    buffer += 3;
		    bufsize -= 3;
		    u += 3;
	       }
	  }
     }
     /* Normal operation */
     else if (bufsize) {
	  u = ringbuf_freespace(oss_output_buffer);
	  if (u > bufsize) u = bufsize;
	  /* Make sure we store whole samples */
	  u -= u%(oss_samplebytes_out);
	  if (u>0)
	       u = ringbuf_enqueue(oss_output_buffer, buffer, u);	  
	  buffer += u;
	  bufsize -= u;
     }
     if (ringbuf_freespace(oss_output_buffer) < oss_samplebytes_out && 
	 !mainloop_time_source_enabled(play_source)) {
	  puts("Starting time source");
	  g_get_current_time(&t);
	  mainloop_time_source_restart(play_source,&t);
     }
     g_assert(bufsize == 0 || bufsize >= oss_samplebytes_in);
     printf("oss_output_play, bs0: %d, u: %d\n",bs0,u);
     /* oss_output_flush(); */
     return bs0 ? u : ringbuf_available(oss_output_buffer);
}

static void oss_output_clear_buffers(void)
{
#ifdef SNDCTL_DSP_RESET
     ioctl(oss_fd, SNDCTL_DSP_RESET);
#endif
     ringbuf_drain(oss_output_buffer);
}


static gboolean oss_output_stop(gboolean must_flush)
{
     if (oss_fd == -1) return TRUE;
     if (must_flush)
	  while (ringbuf_available(oss_output_buffer)>0) {
	       oss_output_flush(NULL,NULL,NULL);
	       do_yield(TRUE);
	  }
     else
	  oss_output_clear_buffers();
     close(oss_fd);
     oss_fd = -1;
     /* printf("oss_stop: oss_delay_time = %f\n",oss_delay_time); */
     ringbuf_drain(oss_output_buffer);
     return must_flush;
}

static void oss_input_stop(void)
{
     if (oss_fd >= 0) close(oss_fd);
     oss_fd = -1;
}

static void oss_input_store(Ringbuf *buffer)
{
     fd_set a;
     struct timeval t = { 0 };
     int i;
     guint u;
     gchar *c;
     audio_buf_info info;
     /* Do a select call to see if data is available */
     if (!oss_noselect) {
	  FD_ZERO(&a);
	  FD_SET(oss_fd, &a);
	  i = select(FD_SETSIZE, &a, NULL, NULL, &t);
	  if (i==-1) {
	       c = g_strdup_printf(_("Error in sound driver: select failed: %s"),strerror(errno));
	       user_error(c);
	       return;
	  }
	  if (i==0) return;
     }

     /* Calculate the number of bytes to read (into buffer) */
     /* If we don't know how much data is available, don't read more than 0.1
      * seconds to avoid GUI latency... */
     u = ringbuf_freespace(buffer);
#ifdef SNDCTL_DSP_GETISPACE
     /* Now do an ioctl call to check the number of readable bytes */
     /* Note: It seems as ALSA 0.9's OSS emulation returns 0 for this call... */
     i = oss_errdlg_ioctl(oss_fd, SNDCTL_DSP_GETISPACE, &info, TRUE);
     if (i > 0 && i < u) u=i;
     else u = MIN(u,oss_samplesize*oss_channels*oss_samplerate/50);
#else
     u = MIN(u,oss_samplesize*oss_channels*oss_samplerate/50);
#endif
     if (u == 0) return;
     /* Read it! */
     c = g_malloc(u);
     i = oss_errdlg_read(oss_fd, c, u);
     if (i != -1)
	  ringbuf_enqueue(buffer, c, i);
     g_free(c);
}

struct oss_prefdlg { 
     GtkWindow *wnd; 
     GtkEntry *pcmdev_playback;
     GtkEntry *pcmdev_record;
     GtkToggleButton *noselect;
};

static void oss_preferences_ok(GtkButton *button, struct oss_prefdlg *pd)
{
     inifile_set(OSS_PCMFILE_PLAYBACK,
		 (char *)gtk_entry_get_text(pd->pcmdev_playback));
     inifile_set(OSS_PCMFILE_RECORD,
		 (char *)gtk_entry_get_text(pd->pcmdev_record));
     oss_noselect = gtk_toggle_button_get_active(pd->noselect);
     inifile_set_gboolean(OSS_NOSELECT,oss_noselect);
     gtk_widget_destroy(GTK_WIDGET(pd->wnd));
}

static void oss_preferences(void)
{
     GtkWidget *a,*b,*c,*d;
     struct oss_prefdlg *pd;
     gchar *q;
     pd = g_malloc(sizeof(struct oss_prefdlg));
     a = gtk_window_new(GTK_WINDOW_DIALOG);     
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_window_set_title(GTK_WINDOW(a),_("OSS preferences"));
     gtk_window_set_position(GTK_WINDOW(a),GTK_WIN_POS_CENTER);
     gtk_container_set_border_width(GTK_CONTAINER(a),5);
     gtk_signal_connect_object(GTK_OBJECT(a),"destroy",GTK_SIGNAL_FUNC(g_free),
			       (GtkObject *)pd);
     pd->wnd = GTK_WINDOW(a);
     b = gtk_vbox_new(FALSE,5);
     gtk_container_add(GTK_CONTAINER(a),b);
     c = gtk_hbox_new(FALSE,3);
     gtk_container_add(GTK_CONTAINER(b),c);
     d = gtk_label_new(_("Playback device file:"));
     gtk_container_add(GTK_CONTAINER(c),d);
     d = gtk_entry_new();
     q = inifile_get(OSS_PCMFILE_PLAYBACK,OSS_PCMFILE_DEFAULT);
     gtk_entry_set_text(GTK_ENTRY(d),q);
     gtk_container_add(GTK_CONTAINER(c),d);
     pd->pcmdev_playback = GTK_ENTRY(d);
     c = gtk_hbox_new(FALSE,3);
     gtk_container_add(GTK_CONTAINER(b),c);
     d = gtk_label_new(_("Recording device file:"));
     gtk_container_add(GTK_CONTAINER(c),d);
     d = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(d),inifile_get(OSS_PCMFILE_RECORD,q));
     gtk_container_add(GTK_CONTAINER(c),d);
     pd->pcmdev_record = GTK_ENTRY(d);
     c = gtk_check_button_new_with_label(_("Avoid select calls (try this if "
					 "recording locks up)"));
     gtk_container_add(GTK_CONTAINER(b),c);
     oss_noselect = inifile_get_gboolean(OSS_NOSELECT,OSS_NOSELECT_DEFAULT);
     pd->noselect = GTK_TOGGLE_BUTTON(c);
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c),oss_noselect);
     c = gtk_hseparator_new();
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_hbutton_box_new();
     gtk_container_add(GTK_CONTAINER(b),c);
     d = gtk_button_new_with_label(_("OK"));
     gtk_signal_connect(GTK_OBJECT(d),"clicked",
			GTK_SIGNAL_FUNC(oss_preferences_ok),pd);
     gtk_container_add(GTK_CONTAINER(c),d);
     d = gtk_button_new_with_label(_("Close"));
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(a));
     gtk_container_add(GTK_CONTAINER(c),d);     
     gtk_widget_show_all(a);
}

