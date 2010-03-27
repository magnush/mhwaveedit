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

/* This module was re-written completely from scratch in Sep 2004. 
 * Thanks to Erica Andrews for contributing the original esound
 * driver. */

#include <esd.h>
#include <errno.h>
#include <unistd.h>
#include "float_box.h"
#include "gettext.h"

#define ESOUND_CLEARDELAY "EsoundClearDelay"
#define ESOUND_CLEARDELAY_DEFAULT 0.4

/* Playback/Recording connections (<= 0 if not connected) */
static int esound_play_fd = -1;
static int esound_record_fd = -1;
static Dataformat esound_play_format;
static float esound_clear_delay;
static GTimeVal esound_noflush_stop_time = {};

static void esound_prefs_init(void)
{
     esound_clear_delay = inifile_get_gfloat(ESOUND_CLEARDELAY,
					     ESOUND_CLEARDELAY_DEFAULT);
}

static gboolean esound_init(gboolean silent)
{
     /* Just try once here so the user gets a clear error message if ESD
      * isn't available */
     int fd;
     gchar *c;
     esound_prefs_init();
     if (getenv("ESD_NO_SPAWN") == NULL) 
	  putenv("ESD_NO_SPAWN=y");
     fd = esd_open_sound(NULL);
     if (fd < 0) {
	  if (!silent) {
	       c = g_strdup_printf(_("Couldn't connect to ESD daemon: %s"),
				   strerror(errno));
	       user_error(c);
	       g_free(c);
	  }
	  return FALSE;
     }
     esd_close(fd);
     return TRUE;
}

static void esound_quit(void)
{
}

static void esound_select_format(Dataformat *format, int *fdp, 
				 gboolean recording)
{
     int fmt = ESD_STREAM;
     g_assert(*fdp < 0);     
     if (recording) fmt |= ESD_RECORD; else fmt |= ESD_PLAY;
     if (format->samplesize == 1) fmt |= ESD_BITS8;
     else if (format->samplesize == 2) fmt |= ESD_BITS16;
     else return;
     if (format->channels == 1) fmt |= ESD_MONO;
     else if (format->channels == 2) fmt |= ESD_STEREO;
     else return;
     if (recording)
	  *fdp = esd_record_stream(fmt,format->samplerate,
				   NULL,"mhWaveEdit");
     else
	  *fdp = esd_play_stream(fmt,format->samplerate,NULL,
				 "mhWaveEdit");     
}

static gint esound_output_select_format(Dataformat *format, gboolean silent, 
					GVoidFunc ready_func)
{
     GTimeVal tv,r;
     int i;
     unsigned long j,k;

     if (format->type == DATAFORMAT_FLOAT) return -1;
     g_get_current_time(&tv);
     i = timeval_subtract(&r,&tv,&esound_noflush_stop_time);
     if (i == 0) {
	  j = r.tv_sec * 1000000 + r.tv_usec;
	  /* It seems we need a much shorter delay here than when jumping.
	   * Fixing it to 0.1 seconds for now */
	  k = 100000;
	  if (r.tv_sec < 4 && j<k) usleep(k-j);
     }
     esound_select_format(format,&esound_play_fd,FALSE);
     if (esound_play_fd < 0) { 
	  if (!silent) {
	       user_perror(_("EsounD: Unable to open playback stream"));
	       return 1;
	  } 
	  return -1;
     }
     memcpy(&esound_play_format,format,sizeof(Dataformat));
     return 0;
}

static gint esound_input_select_format(Dataformat *format, gboolean silent,
				       GVoidFunc ready_func)
{
     if (format->type == DATAFORMAT_FLOAT) return -1;
     esound_select_format(format,&esound_record_fd,TRUE);
     if (esound_record_fd < 0) {
	  if (!silent)
	       user_perror(_("EsounD: Unable to open recording stream"));
	  return -1;
     }
     return 0;
}

static gboolean esound_output_want_data(void)
{
     fd_set wfds;
     int i;
     struct timeval t = { 0.0, 0.0 };    
     FD_ZERO(&wfds);
     FD_SET(esound_play_fd,&wfds);
     i = select(esound_play_fd+1, NULL, &wfds, NULL, &t);
     if (i == -1) {
	  console_perror(_("EsounD driver: select"));
	  return TRUE;
     }
     return (i>0);
}

static guint esound_output_play(gchar *buf, guint len)
{
     ssize_t sst;
     if (buf == NULL || len == 0) return 0;
     sst = write(esound_play_fd, buf, len);
     if (sst < 0) {
	  console_perror(_("EsounD driver: write"));
	  return 0;
     }
     /* FIXME: It's theoretically possible that just part of a sample was 
      * written if the socket buffer size isn't a multiple of 4. */
     return (guint) sst;
     
}

/* Both esound_output_stop and esound_output_clear_buffers
 * should wait for all data in the pipeline to be played, but I don't know
 * how to find out how long time that should be. Use a configurable setting 
 * for now. */

static void esound_output_clear_buffers(void)
{
     if (esound_clear_delay > 0)
	  usleep((unsigned long)(esound_clear_delay * 1000000.0));
     return;
}

static gboolean esound_output_stop(gboolean must_flush)
{
     if (esound_play_fd >= 0) {
	  /* If we want to guarantee all data to be sent, we must wait a 
	   * while. Otherwise, set the first_play variable so we don't restart
	   * immediately. */
	  if (must_flush)
	       esound_output_clear_buffers(); /* Just waits... */	  
	  else
	       g_get_current_time(&esound_noflush_stop_time);
	       
	  esd_close(esound_play_fd);
     }
     esound_play_fd = -1;
     return must_flush;
}

static void esound_input_stop(void)
{
     if (esound_record_fd >= 0) esd_close(esound_record_fd);
     esound_record_fd = -1;
}

static void esound_input_store(Ringbuf *buffer)
{
     gchar buf[4096];
     int i;
     fd_set set;
     struct timeval tv = { 0, 0 };
     int sz;
     if (esound_record_fd < 0) return;
     while (1) {
	  sz = ringbuf_freespace(buffer);
	  if (sz > sizeof(buf)) sz = sizeof(buf);
	  if (sz == 0) return;
	  FD_ZERO(&set);
	  FD_SET(esound_record_fd, &set);
	  i = select(esound_record_fd+1, &set, NULL, NULL, &tv);
	  if (i<0)
	       console_perror(_("EsounD driver: select"));
	  if (i == 0) return;
	  i = read(esound_record_fd,buf,sz);
	  if (i == 0) {
	       user_error(_("Esound connection closed by server"));
	       esound_record_fd = -1;
	       return;
	  }
	  if (i<0) {
	       if (errno == EAGAIN || errno == EINTR) continue;
	       console_perror(_("EsounD driver: read"));
	       return;
	  }
	  ringbuf_enqueue(buffer,buf,i);
     }     
}

struct esound_prefdlg { 
     GtkWindow *wnd; 
     Floatbox *jd;
};

static void esound_preferences_ok(GtkButton *button, struct esound_prefdlg *pd)
{
     if (floatbox_check_limit(pd->jd,0.0,4.0,_("jump delay"))) return;
     esound_clear_delay = pd->jd->val;
     inifile_set_gfloat(ESOUND_CLEARDELAY,esound_clear_delay);
     gtk_widget_destroy(GTK_WIDGET(pd->wnd));     
}

static void esound_preferences(void)
{
     GtkWidget *a,*b,*c,*d;
     struct esound_prefdlg *pd;
     esound_prefs_init();
     pd = g_malloc(sizeof(*pd));
     a = gtk_window_new(GTK_WINDOW_DIALOG);     
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_window_set_title(GTK_WINDOW(a),_("ESD preferences"));
     gtk_window_set_position(GTK_WINDOW(a),GTK_WIN_POS_CENTER);
     gtk_container_set_border_width(GTK_CONTAINER(a),5);
     gtk_signal_connect_object(GTK_OBJECT(a),"destroy",GTK_SIGNAL_FUNC(g_free),
			       (GtkObject *)pd);
     pd->wnd = GTK_WINDOW(a);
     b = gtk_vbox_new(FALSE,5);
     gtk_container_add(GTK_CONTAINER(a),b);
     c = gtk_label_new(_("If the cursor starts running too early when jumping, "
		       "this delay should be increased."));
     gtk_label_set_line_wrap(GTK_LABEL(c),TRUE);
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_hbox_new(FALSE,3);
     gtk_container_add(GTK_CONTAINER(b),c);
     d = gtk_label_new(_("Jump delay:"));
     gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
     d = floatbox_new(esound_clear_delay);
     gtk_box_pack_start(GTK_BOX(c),d,TRUE,TRUE,0);
     pd->jd = FLOATBOX(d);
     d = gtk_label_new(_("seconds"));
     gtk_box_pack_end(GTK_BOX(c),d,FALSE,FALSE,0);
     c = gtk_hseparator_new();
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_label_new(_("Selecting the ESD host to connect to is done through "
		       "the ESPEAKER environment variable."));
     gtk_label_set_line_wrap(GTK_LABEL(c),TRUE);
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_hseparator_new();
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_hbutton_box_new();
     gtk_container_add(GTK_CONTAINER(b),c);
     d = gtk_button_new_with_label(_("OK"));
     gtk_signal_connect(GTK_OBJECT(d),"clicked",
			GTK_SIGNAL_FUNC(esound_preferences_ok),pd);
     gtk_container_add(GTK_CONTAINER(c),d);
     d = gtk_button_new_with_label(_("Close"));
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(a));
     gtk_container_add(GTK_CONTAINER(c),d);     
     gtk_widget_show_all(a);
}

