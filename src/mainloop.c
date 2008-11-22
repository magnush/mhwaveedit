/*
 * Copyright (C) 2008, Magnus Hjorth
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 */


#include <config.h>


#include <gtk/gtk.h>
#include "mainloop.h"
#include "sound.h"
#include "statusbar.h"
#include "document.h"
#include "mainwindow.h"

gboolean idle_work_flag;

struct constant_source {
     constsource_cb cb;
     gpointer user_data;
     gboolean enabled;
     gboolean lowprio;
};

struct time_source {
     timesource_cb cb;
     gpointer user_data;
     gboolean enabled;
     GTimeVal call_time;
};

struct io_source  {
     GPollFD pfd;
     iosource_cb cb;
     gpointer user_data;
     gboolean enabled;
};

static GList *constsources = NULL, *timesources = NULL, *iosources = NULL;



static int timesource_scan(GTimeVal *current_time, gboolean do_dispatch)
{
     GList *l;     
     struct time_source *src;
     GTimeVal tv;
     int i,j;
     int timeout = -1;
     for (l=timesources; l!=NULL; l=l->next) {
	  src = (struct time_source *)l->data;
	  if (!src->enabled) continue;
	  i = timeval_subtract(&tv, &src->call_time, current_time);
	  if (i || (tv.tv_sec==0 && tv.tv_usec==0)) {
	       timeout = 0;

	       if (do_dispatch) {
		    src->enabled = FALSE;
		    src->cb(src, current_time, src->user_data);
	       }

	  } else {
	       j = tv.tv_usec / 1000 + tv.tv_sec * 1000;
	       if (j <= 0) j=1;

	       if (timeout < 0 || j < timeout)
		    timeout = j;
	  }
     }
     return timeout;
}

static gboolean iosource_check_main(void)
{
     GList *l;     
     struct io_source *src;

     for (l=iosources; l!=NULL; l=l->next) {
	  src = (struct io_source *)l->data;
	  if (src->enabled && src->pfd.revents != 0) return TRUE;
     }
     return FALSE;
}

static gboolean iosource_dispatch_main(void)
{
     GList *l;     
     struct io_source *src;

     for (l=iosources; l!=NULL; l=l->next) {
	  src = (struct io_source *)l->data;
	  if (src->enabled && src->pfd.revents != 0) {
	       src->cb(src,src->pfd.fd,src->pfd.revents,src->user_data);
	       src->pfd.revents = 0;
	  }
     }
     return TRUE;
}





#if GLIB_MAJOR_VERSION < 2

static gboolean timesource_prepare(gpointer source_data, 
				   GTimeVal *current_time, gint *timeout,
				   gpointer user_data)
{
     int i;
     i = timesource_scan(current_time,FALSE);
     if (i < 0) 
	  return FALSE;
     else if (i == 0)
	  return TRUE;
     else {
	  if (*timeout < 0 || i < *timeout)
	       *timeout = i;
	  return FALSE;
     }
}

static gboolean timesource_check(gpointer source_data, GTimeVal *current_time,
				 gpointer user_data)
{
     int i;
     i = timesource_scan(current_time,FALSE);
     return (i == 0);
}

static gboolean timesource_dispatch(gpointer source_data, 
				    GTimeVal *dispatch_time, gpointer user_data)
{
     timesource_scan(dispatch_time,TRUE);
     return TRUE;
}

static GSourceFuncs tsf = { 
     timesource_prepare, timesource_check, timesource_dispatch, NULL
};
				    
static void mainloop_time_source_added(struct time_source *src)
{
     static gboolean inited = FALSE;
     if (!inited) {
	  g_source_add(G_PRIORITY_HIGH, FALSE, &tsf, NULL, NULL, NULL);
	  inited = TRUE;
     }
}

static gboolean iosource_prepare(gpointer source_data, 
				 GTimeVal *current_time, gint *timeout,
				 gpointer user_data)
{
     return FALSE;
}

static gboolean iosource_check(gpointer source_data, GTimeVal *current_time,
			       gpointer user_data)
{
     return iosource_check_main();
}

static gboolean iosource_dispatch(gpointer source_data, 
				    GTimeVal *dispatch_time, gpointer user_data)
{
     iosource_dispatch_main();
     return TRUE;
}

static GSourceFuncs isf = { 
     iosource_prepare, iosource_check, iosource_dispatch, NULL
};
				    

static void mainloop_io_source_enable_main(struct io_source *src)
{
     static gboolean inited = FALSE;
     if (!inited) {
	  g_source_add(G_PRIORITY_HIGH, FALSE, &isf, NULL, NULL, NULL);
	  inited = TRUE;
     }
     g_main_add_poll(&src->pfd, G_PRIORITY_HIGH);     
}

static void mainloop_io_source_disable_main(struct io_source *src)
{
     g_main_remove_poll(&src->pfd);
}




#else






static gboolean timesource_prepare(GSource *source, gint *timeout)
{
     GTimeVal tv;
     int i;
     g_source_get_current_time(source, &tv);
     i = timesource_scan(&tv,FALSE);
     if (i <= 0)
	  return (i == 0);
     else {
	  if (*timeout < 0 || i < *timeout) *timeout = i;
	  return FALSE;
     }
}

static gboolean timesource_check(GSource *source)
{
     GTimeVal tv;
     int i;
     g_source_get_current_time(source, &tv);
     i = timesource_scan(&tv,FALSE);
     return (i == 0);
}

static gboolean timesource_dispatch(GSource *source, GSourceFunc callback,
				    gpointer user_data)
{
     GTimeVal tv;
     g_source_get_current_time(source,&tv);
     timesource_scan(&tv,TRUE);
     return TRUE;
}

static GSourceFuncs tsf2 = {
     timesource_prepare, 
     timesource_check, 
     timesource_dispatch, 
     NULL
};

static void mainloop_time_source_added(struct time_source *src)
{
     static gboolean inited = FALSE;
     GSource *s;
     if (!inited) {
	  s = g_source_new(&tsf2, sizeof(*s));
	  g_source_attach(s,NULL);
	  inited = TRUE;
     }
}


static gboolean iosource_prepare(GSource *source, int *timeout)
{
     return FALSE;
}

static gboolean iosource_check(GSource *source)
{
     return iosource_check_main();
}


static gboolean iosource_dispatch(GSource *source, GSourceFunc callback, 
				  gpointer user_data)
{
     iosource_dispatch_main();
     return TRUE;
}

static GSource *iosource_handle = NULL;

static GSourceFuncs isf2 = { iosource_prepare, iosource_check, 
			     iosource_dispatch };

static void mainloop_io_source_enable_main(struct io_source *src)
{
     if (iosource_handle == NULL) {
	  iosource_handle = g_source_new(&isf2, sizeof(GSource));
	  g_source_attach(iosource_handle,NULL);
     }
     g_source_add_poll(iosource_handle, &src->pfd);
}

static void mainloop_io_source_disable_main(struct io_source *src)
{
     g_source_remove_poll(iosource_handle, &src->pfd);
}

#endif



/* ---------------------------------------- */

void mainloop(void)
{
     static guint player_count=0;
     struct constant_source *csrc;
     gint i,j;
     GList *l;

     i = -1;
     for (l=constsources; l!=NULL; l=l->next) {
	  csrc = (struct constant_source *)l->data;
	  if (csrc->enabled && !csrc->lowprio) {
	       j = csrc->cb(csrc,csrc->user_data);
	       if (j > 0) return;
	       if (j > i) i = j;
	  }
     }
     j = sound_poll();
     if (j > 0) { player_count=0; return; }
     if (j > i) i = j;
     player_count++;
     if (gtk_events_pending()) {
	  gtk_main_iteration();
	  return;
     }
     if (!idle_work_flag) {
	  idle_work_flag = TRUE;
	  return;
     }

     for (l=constsources; l!=NULL; l=l->next) {
	  csrc = (struct constant_source *)l->data;
	  if (csrc->enabled && csrc->lowprio) {
	       j = csrc->cb(csrc,csrc->user_data);
	       if (j > 0) return;
	       if (j > i) i = j;
	  }
     }

     if (status_bar_progress_count()>0) return;
     document_update_cursors();
     if (chunk_view_autoscroll()) return;
     if (mainwindow_update_caches()) return;
     if (i < 0) {
	  gtk_main_iteration();
     } else if (i >= 0 && player_count > 10)
	  do_yield(TRUE);
}



gpointer mainloop_constant_source_add(constsource_cb cb, gpointer user_data, 
				      gboolean low_prio)
{
     struct constant_source *src;
     src = g_malloc(sizeof(*src));
     src->cb = cb;
     src->user_data = user_data;
     src->enabled = TRUE;
     src->lowprio = low_prio;
     constsources = g_list_prepend(constsources, src);

     return src;
}

void mainloop_constant_source_enable(gpointer constsource, gboolean enable)
{
     struct constant_source *src = (struct constant_source *)constsource;
     src->enabled = enable;
}

void mainloop_constant_source_free(gpointer constsource)
{
     constsources = g_list_remove(constsources, constsource);
     g_free(constsource);
}

gpointer mainloop_time_source_add(GTimeVal *tv, timesource_cb cb, 
				  gpointer user_data)
{
     struct time_source *src;

     src = g_malloc(sizeof(*src));
     memcpy(&src->call_time,tv,sizeof(src->call_time));
     src->cb = cb;
     src->user_data = user_data;
     src->enabled = TRUE;

     timesources = g_list_append(timesources, src);

     mainloop_time_source_added(src);

     return src;
}

void mainloop_time_source_restart(gpointer timesource, GTimeVal *new_tv)
{
     struct time_source *src = (struct time_source *)timesource;
     memcpy(&(src->call_time),new_tv,sizeof(src->call_time));
     src->enabled = TRUE;
}

void mainloop_time_source_free(gpointer timesource)
{
     timesources = g_list_remove(timesources, timesource);
     g_free(timesource);     
}

gpointer mainloop_io_source_add(int fd, gushort events, iosource_cb cb, 
				gpointer user_data)
{
     struct io_source *src;
     src = g_malloc(sizeof(*src));
     src->pfd.fd = fd;
     src->pfd.events = events;
     src->pfd.revents = 0;
     src->cb = cb;
     src->user_data = user_data;
     src->enabled = TRUE;
     iosources = g_list_append(iosources, src);
     
     mainloop_io_source_enable_main(src);

     return src;
}

void mainloop_io_source_enable(gpointer iosource, gboolean enable)
{
     struct io_source *src = (struct io_source *)iosource;
     if (XOR(src->enabled,enable)) {
	  src->enabled = enable;
	  if (enable)
	       mainloop_io_source_enable_main(src);
	  else
	       mainloop_io_source_disable_main(src);
     }
}

void mainloop_io_source_free(gpointer iosource)
{
     mainloop_io_source_enable(iosource,FALSE);
     iosources = g_list_remove(iosources, iosource);
}

