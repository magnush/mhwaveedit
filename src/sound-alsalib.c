/*
 * Copyright (C) 2004 2005 2006 2008 2010 2011 2012, Magnus Hjorth
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

#include <alsa/asoundlib.h>
#include "gettext.h"

/* #define ALSADEBUG */

static struct {
     snd_pcm_t *whand,*rhand;
     Dataformat wfmt,rfmt;
     gboolean draining,draining_done;     
     int overrun_count;
     gpointer iogroup;
     gpointer csource;
     gboolean eventdriv;
     gboolean inside_ready_func;
     int rw_call_count;
     GVoidFunc ready_func;
} alsa_data = { 0 };

static gboolean alsa_output_want_data(void);

static gboolean alsa_init(gboolean silent)
{     
     alsa_data.eventdriv = inifile_get_gboolean("ALSAEventDriven",TRUE);
     /* We assume that if you have alsa-lib, you're using the ALSA kernel 
      * drivers. */
     return TRUE;
}

static void alsa_quit(void)
{
}

static void alsa_prefs_ok(GtkButton *button, gpointer user_data)
{
     GtkWidget *w = GTK_WIDGET(user_data);
     GtkWidget *ep,*er;
     GtkToggleButton *tb;
     ep = gtk_object_get_data(GTK_OBJECT(w),"playentry");
     er = gtk_object_get_data(GTK_OBJECT(w),"recentry");
     tb = gtk_object_get_data(GTK_OBJECT(w),"eventtb");
     alsa_data.eventdriv = gtk_toggle_button_get_active(tb);
     inifile_set("ALSAPlayDevice",(gchar *)gtk_entry_get_text(GTK_ENTRY(ep)));
     inifile_set("ALSARecDevice",(gchar *)gtk_entry_get_text(GTK_ENTRY(er)));
     inifile_set_gboolean("ALSAEventDriven",alsa_data.eventdriv);
     gtk_widget_destroy(w);
}

static void alsa_show_preferences(void)
{
     GtkWidget *a,*b,*c,*d;
     alsa_init(TRUE); /* Read config */
     a = gtk_window_new(GTK_WINDOW_DIALOG);
     gtk_window_set_title(GTK_WINDOW(a),_("ALSA Preferences"));
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_window_set_position(GTK_WINDOW(a),GTK_WIN_POS_MOUSE);
     gtk_container_set_border_width(GTK_CONTAINER(a),5);
     gtk_signal_connect(GTK_OBJECT(a),"delete_event",
			GTK_SIGNAL_FUNC(gtk_false),NULL);
     b = gtk_table_new(4,2,FALSE);
     gtk_container_add(GTK_CONTAINER(a),b);
     attach_label(_("Playback device: "),b,0,0);
     c = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(c),inifile_get("ALSAPlayDevice","default"));
     gtk_table_attach(GTK_TABLE(b),c,1,2,0,1,GTK_EXPAND|GTK_FILL,0,5,5);
     attach_label(_("Recording device: "),b,1,0);
     gtk_object_set_data(GTK_OBJECT(a),"playentry",c);
     c = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(c),inifile_get("ALSARecDevice","hw"));
     gtk_table_attach(GTK_TABLE(b),c,1,2,1,2,GTK_EXPAND|GTK_FILL,0,5,5);
     gtk_object_set_data(GTK_OBJECT(a),"recentry",c);
     c = gtk_check_button_new_with_label(_("Event driven I/O"));
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c),alsa_data.eventdriv);
     gtk_table_attach(GTK_TABLE(b),c,0,2,2,3,GTK_EXPAND|GTK_FILL,0,5,5);
     gtk_object_set_data(GTK_OBJECT(a),"eventtb",c);     
     c = gtk_hbutton_box_new();
     gtk_table_attach(GTK_TABLE(b),c,0,2,3,4,GTK_EXPAND|GTK_FILL,
		      GTK_EXPAND|GTK_FILL,0,5);
     d = gtk_button_new_with_label(_("OK"));
     gtk_signal_connect(GTK_OBJECT(d),"clicked",GTK_SIGNAL_FUNC(alsa_prefs_ok),
			a);
     gtk_container_add(GTK_CONTAINER(c),d);
     d = gtk_button_new_with_label(_("Cancel"));
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(a));
     gtk_container_add(GTK_CONTAINER(c),d);     
     gtk_widget_show_all(a);
}

static gboolean alsa_open_rw(snd_pcm_t **handp, Dataformat *fmtp, 
			     gchar *devini, snd_pcm_stream_t str,
			     gboolean silent)
{
     int i;
     gchar *c,*devname;
     if (*handp == NULL) {
	  devname = inifile_get(devini,"default");
	  i = snd_pcm_open(handp,devname,str,
			   SND_PCM_NONBLOCK);
	  if (i<0) {
	       if (!silent) {
		    c = g_strdup_printf((str == SND_PCM_STREAM_CAPTURE)?
					_("Error opening ALSA device '%s' "
					  "for recording: %s"):
					_("Error opening ALSA device '%s' "
					  "for playback: %s"), devname,
					snd_strerror(i));
		    user_error(c);
		    g_free(c);
	       }
	       return TRUE;
	  }
	  memset(fmtp,0,sizeof(*fmtp));
     }
     return FALSE;
}

static gboolean alsa_open_write(gboolean silent)
{
     return alsa_open_rw(&(alsa_data.whand),&(alsa_data.wfmt),
			 "ALSAPlayDevice",SND_PCM_STREAM_PLAYBACK,silent);
}

static gboolean alsa_open_read(gboolean silent)
{
     return alsa_open_rw(&(alsa_data.rhand),&(alsa_data.rfmt),
			 "ALSARecDevice",SND_PCM_STREAM_CAPTURE,silent);
}

static snd_pcm_format_t alsa_get_format(Dataformat *format)
{
     if (format->type == DATAFORMAT_FLOAT) {
	  if (format->samplesize == sizeof(float))
	       return format->bigendian ? SND_PCM_FORMAT_FLOAT_BE :
		    SND_PCM_FORMAT_FLOAT_LE;
	  else
	       return format->bigendian ? SND_PCM_FORMAT_FLOAT64_BE :
		    SND_PCM_FORMAT_FLOAT64_LE;
     }
     switch (format->samplesize) {
     case 1:
	  if (format->sign) 
	       return SND_PCM_FORMAT_S8;
	  else 
	       return SND_PCM_FORMAT_U8;
     case 2:
	  if (format->sign) 
	       return format->bigendian?SND_PCM_FORMAT_S16_BE:
		    SND_PCM_FORMAT_S16_LE;
	  else 
	       return format->bigendian ? SND_PCM_FORMAT_U16_BE :
		    SND_PCM_FORMAT_U16_LE;
     case 3:
	  if (format->sign) 
	       return format->bigendian ? SND_PCM_FORMAT_S24_3BE : 
		    SND_PCM_FORMAT_S24_3LE;
	  else
	       return format->bigendian ? SND_PCM_FORMAT_U24_3BE : 
		    SND_PCM_FORMAT_U24_3LE;	       
     case 4:
	  switch (format->packing) {
	  case 0:
	  case 1: /* Use 32-bit for 24-in-32 with data in MSB */
	       if (format->sign)
		    return format->bigendian?
			 SND_PCM_FORMAT_S32_BE:SND_PCM_FORMAT_S32_LE;
	       else
		    return format->bigendian?
			 SND_PCM_FORMAT_U32_BE:SND_PCM_FORMAT_U32_LE;
	  case 2:
	       if (format->sign)
		    return format->bigendian?
			 SND_PCM_FORMAT_S24_BE:SND_PCM_FORMAT_S24_LE;
	       else
		    return format->bigendian?
			 SND_PCM_FORMAT_U24_BE:SND_PCM_FORMAT_S24_LE;
	  }
     }
     g_assert_not_reached();
     return SND_PCM_FORMAT_UNKNOWN;
}

static int csource_func(gpointer csource, gpointer user_data)
{
     int i;
#ifdef ALSADEBUG
     puts("csource_func");
#endif
     i = alsa_data.rw_call_count;
     alsa_data.inside_ready_func = TRUE;
     alsa_data.ready_func();
     alsa_data.inside_ready_func = FALSE;
     if (alsa_data.rw_call_count == i) {
	  mainloop_constant_source_enable(csource,FALSE);
	  return -1;
     }
     if (alsa_output_want_data()) {
	  return 0;
     }
     mainloop_constant_source_enable(csource,FALSE);
     mainloop_io_group_enable(alsa_data.iogroup,TRUE);
     return -1;
}    

static int iogroup_ready_func(gpointer iogroup, int fd, gushort revents,
			       gpointer user_data)
{
     int i;
#ifdef ALSADEBUG
     GTimeVal tv;
     g_get_current_time(&tv);
     printf("iogroup_ready_func: time=%3d.%06d, fd=%d, revents=%d\n",
	    (int)tv.tv_sec,(int)tv.tv_usec,(int)fd,(int)revents);
#endif     
     if (alsa_data.whand!=NULL && !alsa_output_want_data()) {
#ifdef ALSADEBUG
	  printf("Alsa woke us up but didn't want data\n");
#endif
	  return 0;
     }
     i = alsa_data.rw_call_count;
     alsa_data.inside_ready_func = TRUE;
     alsa_data.ready_func();
     alsa_data.inside_ready_func = FALSE;     
     if (alsa_data.rw_call_count == i) return -1;
     if (!alsa_data.eventdriv && alsa_data.whand!=NULL && alsa_output_want_data()) {
	  if (alsa_data.csource == NULL) 
	       alsa_data.csource = mainloop_constant_source_add(csource_func,NULL,FALSE);
	  mainloop_constant_source_enable(alsa_data.csource,TRUE);
	  return -1;
     }
     return 1;
}

static gboolean alsa_set_format(Dataformat *format,Dataformat *fmtp,
				snd_pcm_t **handp,gboolean playback, 
				GVoidFunc ready_func)
{     
     snd_pcm_hw_params_t *par;
     int i;
     snd_pcm_uframes_t bufsize;
     unsigned int pertime,wdtime;
     int perdir;
     int fd_count;
     struct pollfd *fds;
     GPollFD *pfds;
     if (dataformat_equal(fmtp,format)) return FALSE;
     snd_pcm_hw_params_malloc(&par);
     snd_pcm_hw_params_any(*handp,par);
     i = snd_pcm_hw_params_set_access(*handp,par,
				      SND_PCM_ACCESS_RW_INTERLEAVED);
     if (i) { 
	  console_message(_("snd_pcm_hw_params_set_access failed")); 
	  return TRUE; 
     }
     i = snd_pcm_hw_params_set_format(*handp,par,alsa_get_format(format));
     if (i) { 
	  console_message(_("snd_pcm_hw_params_set_format failed")); 
	  return TRUE; 
     }
     i = snd_pcm_hw_params_set_channels(*handp,par,format->channels);
     if (i) { 
	  console_message(_("snd_pcm_hw_params_set_channels failed")); 
	  return TRUE; 
     }
     i = snd_pcm_hw_params_set_rate(*handp,par,format->samplerate,0);
     if (i) { 
	  console_message(_("snd_pcm_hw_params_set_rate failed")); 
	  return TRUE; 
     }
     i = snd_pcm_hw_params_set_buffer_size_last(*handp,par,&bufsize);
     if (i) {
	  console_message(_("snd_pcm_hw_params_set_buffer_size_last failed"));
	  return TRUE;
     }
#ifdef ALSADEBUG 
     printf("Buffer size: %d\n",(int)bufsize);
#endif
     pertime = 100001; /* 0.1 sec */
     perdir = -1;
     if (playback) {
	  i = snd_pcm_hw_params_set_period_time_near(*handp,par,&pertime,&perdir);
	  if (i) {
	       console_message("snd_pcm_hw_params_set_period_time_near failed");
	       return TRUE;
	  }
#ifdef ALSADEBUG
	  printf("Period time: %d us\n",pertime);
#endif
     }
     i = snd_pcm_hw_params(*handp,par);
     snd_pcm_hw_params_free(par);
     if (i<0) {
	  printf("snd_pcm_hw_params: %s\n",snd_strerror(i));
	  memset(fmtp,0,sizeof(*fmtp));
	  return TRUE;
     }

     memcpy(fmtp,format,sizeof(*fmtp));
     alsa_data.ready_func = ready_func;

     fd_count = snd_pcm_poll_descriptors_count(*handp);
     fds = g_malloc(fd_count*sizeof(*fds));
     fd_count = snd_pcm_poll_descriptors(*handp,fds,fd_count*sizeof(*fds));
	  
     pfds = g_malloc(fd_count*sizeof(GPollFD));
     for (i=0; i<fd_count; i++) {
	  pfds[i].fd = fds[i].fd;
	  pfds[i].events = fds[i].events;
     }

     g_assert(alsa_data.iogroup == NULL);
     if (alsa_data.eventdriv)
	  wdtime = pertime + 50000;
     else {
	  wdtime = pertime;
	  fd_count = 0;
     }
     alsa_data.iogroup = mainloop_io_group_add(fd_count,pfds,wdtime/1000,
					       iogroup_ready_func,NULL);
     alsa_data.rw_call_count = 0;
     g_free(pfds);
     g_free(fds);

     return FALSE;
}

static gboolean alsa_needs_polling(void)
{
     return FALSE;
}

static gboolean alsa_set_write_format(Dataformat *format, GVoidFunc ready_func)
{
     return alsa_set_format(format,&(alsa_data.wfmt),&(alsa_data.whand),TRUE,
			    ready_func);
}

static gboolean alsa_set_read_format(Dataformat *format, GVoidFunc ready_func)
{
     return alsa_set_format(format,&(alsa_data.rfmt),&(alsa_data.rhand),FALSE,
			    ready_func);
}

static gint alsa_output_select_format(Dataformat *format, gboolean silent,
				      GVoidFunc ready_func)
{
     /* signal(SIGPIPE,SIG_IGN); */
     if (alsa_open_write(silent)) return silent?-1:+1;
     if (alsa_set_write_format(format,ready_func)) { 
	  snd_pcm_close(alsa_data.whand);
	  alsa_data.whand = NULL;
	  return -1;
     }
     return 0;
}

static gint alsa_input_select_format(Dataformat *format, gboolean silent, 
				     GVoidFunc ready_func)
{
     if (alsa_open_read(silent)) return silent?-1:+1;
     if (alsa_set_read_format(format,ready_func)) {
	  snd_pcm_close(alsa_data.rhand);
	  alsa_data.rhand = NULL;
	  return -1;
     }
     alsa_data.draining = alsa_data.draining_done = FALSE;
     return 0;
}

static void alsa_stop_iosource(void)
{
     if (alsa_data.iogroup != NULL) {
	  mainloop_io_group_free(alsa_data.iogroup);
	  alsa_data.iogroup = NULL;
     }
}

static gboolean alsa_output_stop(gboolean must_flush)
{
     alsa_stop_iosource();
     if (alsa_data.whand != NULL) {
	  if (must_flush)
	       snd_pcm_drain(alsa_data.whand);
	  else
	       snd_pcm_drop(alsa_data.whand);
	  snd_pcm_close(alsa_data.whand);
	  alsa_data.whand = NULL;
     }
     return must_flush;
}

static void alsa_input_stop(void)
{
     alsa_stop_iosource();
     if (alsa_data.rhand != NULL) {
	  snd_pcm_close(alsa_data.rhand);
	  alsa_data.rhand = NULL;
     }
}

static void alsa_input_stop_hint(void)
{
     if (!alsa_data.draining) {
	  snd_pcm_drain(alsa_data.rhand);
	  alsa_data.draining = TRUE;
     }
}

static gboolean alsa_output_want_data(void)
{
     snd_pcm_sframes_t s;
     /* signal(SIGPIPE,SIG_IGN); */
     s = snd_pcm_avail_update(alsa_data.whand);
     if (s == -EPIPE) {
	  puts(_("<ALSA playback buffer underrun>"));
	  snd_pcm_prepare(alsa_data.whand);
	  return TRUE;
     } else if (s<0) {
	  printf("snd_pcm_avail_update: %s\n",snd_strerror(s));
	  return FALSE;
     }
     /* return (s > alsa_data.wfmt.samplerate / 250); */
     return (s > 0);
}

static guint alsa_output_play(gchar *buffer, guint bufsize)
{
     snd_pcm_sframes_t r,s;
     guint u;
#ifdef ALSADEBUG
     GTimeVal tv;
#endif
     /* signal(SIGPIPE,SIG_IGN); */
     if (bufsize == 0) return 0;
     if (!alsa_data.inside_ready_func)
	  mainloop_io_group_enable(alsa_data.iogroup,TRUE);
     alsa_data.rw_call_count ++;
#ifdef ALSADEBUG
     g_get_current_time(&tv);
     if (!alsa_data.inside_ready_func)
	  printf("called output_play, %d bytes, re-enabled events, "
		 "time=%3d.%06d\n",
		 (int)bufsize,(int)tv.tv_sec,(int)tv.tv_usec);
     else
	  printf("called output_play from ready-func, %d bytes, "
		 "time=%3d.%06d\n",
		 (int)bufsize,(int)tv.tv_sec,(int)tv.tv_usec);	  
#endif
     s = bufsize/alsa_data.wfmt.samplebytes;
     while (1) {
	  r = snd_pcm_writei(alsa_data.whand,buffer,s);
	  if (r == -EAGAIN || r == 0) {
#ifdef ALSADEBUG
	       puts("snd_pcm_writei: EAGAIN!");
#endif
	       r = snd_pcm_avail_update(alsa_data.whand);
	       if (r > 0 && r<s) {
		    s = r;
		    continue;
	       }

	       if (snd_pcm_state(alsa_data.whand) == SND_PCM_STATE_PREPARED) {
#ifdef ALSADEBUG
		    puts("Starting stream");		    
#endif
		    snd_pcm_start(alsa_data.whand);
	       }
	       return 0;
	  }
	  if (r == -EPIPE) {
	       puts(_("<ALSA playback buffer underrun>"));
	       snd_pcm_prepare(alsa_data.whand);
	       continue;
	  }
	  if (r < 0) {
	       printf("snd_pcm_writei: %s\n",snd_strerror(r));
	       return 0;
	  }
	  break;
     }
     u = r*alsa_data.wfmt.samplebytes;
#ifdef ALSADEBUG
     g_get_current_time(&tv);
     printf("played %d samples time=%3d.%06d\n",(int)r,(int)tv.tv_sec,
	    (int)tv.tv_usec);
#endif
     return u;
}

static void alsa_output_clear_buffers(void)
{
     /* signal(SIGPIPE,SIG_IGN); */
     snd_pcm_drop(alsa_data.whand);
     snd_pcm_prepare(alsa_data.whand);
}

static void alsa_input_store(Ringbuf *buffer)
{
     gchar buf[4096];
     snd_pcm_sframes_t x;
     if (alsa_data.draining_done) return;
     alsa_data.rw_call_count ++;
     while (1) {
	  x = snd_pcm_readi(alsa_data.rhand,buf,
			    MIN(sizeof(buf),ringbuf_freespace(buffer))/
			    alsa_data.rfmt.samplebytes);
	  if (x > 0)
	       ringbuf_enqueue(buffer,buf,x*alsa_data.rfmt.samplebytes);
	  else if (alsa_data.draining) {
	       alsa_data.draining_done = TRUE;
	       return;
	  } else if (x == 0 || x == -EAGAIN)
	       return;
	  else if (x == -EPIPE) {
	       puts(_("<ALSA recording buffer overrun>"));
	       alsa_data.overrun_count ++;
	       snd_pcm_prepare(alsa_data.rhand);
	  } else {
	       printf("snd_pcm_readi: %s\n",snd_strerror(x));
	       snd_pcm_prepare(alsa_data.rhand);
	       return;
	  }
     }
     mainloop_io_group_enable(alsa_data.iogroup,TRUE);
}

int alsa_input_overrun_count(void)
{
     return alsa_data.overrun_count;
}
