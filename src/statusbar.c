/*
 * Copyright (C) 2004 2005 2008 2010, Magnus Hjorth
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

#include <config.h>

#include <string.h>
#include <stdio.h>

#include "statusbar.h"
#include "mainloop.h"
#include "gettext.h"

#define LEFT_MARGIN 4

gboolean status_bar_roll_cursor;

static GtkWidgetClass *parent_class;
static int progress_count = 0;

enum { PROGRESS_BEGIN_SIGNAL, PROGRESS_END_SIGNAL, LAST_SIGNAL };
static guint status_bar_signals[LAST_SIGNAL] = { 0 };

static void status_bar_expose(GtkWidget *widget, GdkEventExpose *event, 
			    gpointer user_data)
{
     StatusBar *bar = STATUSBAR(user_data);
     GdkGC *gc;
     
     if (bar->mode == 1) {
	  gc = get_gc(PROGRESS,widget);
	  gdk_gc_set_clip_rectangle(gc,&(event->area));
	  gdk_draw_rectangle(widget->window, gc, TRUE, 
			     widget->allocation.x,event->area.y,
			     bar->bar_width,event->area.height);

	  gdk_gc_set_clip_mask(gc,NULL);
     }
}

static void status_bar_init(GtkObject *obj)
{
     StatusBar *sb = STATUSBAR(obj);
     GtkContainer *cont = GTK_CONTAINER(obj);
     GtkFixed *fix = GTK_FIXED(obj);
     GtkWidget *da;

     da = gtk_label_new("");
     sb->da = da;
     gtk_signal_connect(GTK_OBJECT(da),"expose_event",
			GTK_SIGNAL_FUNC(status_bar_expose),obj);
     gtk_fixed_put(fix,da,0,0);

     sb->mode = 2;
     sb->rate = 0;
     sb->fresh_label = GTK_LABEL(gtk_label_new(_("(no file loaded)")));
     sb->progress_label = GTK_LABEL(gtk_label_new(""));
     sb->cursor = GTK_LABEL(gtk_label_new(""));
     sb->view = GTK_LABEL(gtk_label_new(""));
     sb->sel = GTK_LABEL(gtk_label_new(""));
     gtk_fixed_put(fix,GTK_WIDGET(sb->fresh_label),LEFT_MARGIN,2);
     gtk_fixed_put(fix,GTK_WIDGET(sb->progress_label),LEFT_MARGIN,2);
     gtk_fixed_put(fix,GTK_WIDGET(sb->cursor),LEFT_MARGIN,2);
     gtk_container_add(cont,GTK_WIDGET(sb->view));
     gtk_container_add(cont,GTK_WIDGET(sb->sel));
     gtk_widget_show(GTK_WIDGET(sb->fresh_label));     
}

static void status_bar_size_allocate(GtkWidget *widget, 
				     GtkAllocation *allocation)
{
     StatusBar *sb = STATUSBAR(widget);
     GtkWidget *daw = GTK_WIDGET(sb->da);
     if (daw->allocation.height != allocation->height || 
	 daw->allocation.width != allocation->width) {
	  gtk_widget_set_usize(daw,allocation->width,allocation->height);
     }
     parent_class->size_allocate(widget,allocation);
}

static void status_bar_size_request(GtkWidget *widget, 
				    GtkRequisition *requisition)
{
     parent_class->size_request(widget,requisition);
     requisition->width = 10;
}

static void status_bar_class_init(GtkObjectClass *klass)
{
     parent_class = GTK_WIDGET_CLASS(gtk_type_class(gtk_fixed_get_type()));
     GTK_WIDGET_CLASS(klass)->size_allocate = status_bar_size_allocate;
     GTK_WIDGET_CLASS(klass)->size_request = status_bar_size_request;
     STATUSBAR_CLASS(klass)->progress_begin = NULL;
     STATUSBAR_CLASS(klass)->progress_end = NULL;
     status_bar_signals[PROGRESS_BEGIN_SIGNAL] = 
	  gtk_signal_new("progress-begin", GTK_RUN_FIRST,GTK_CLASS_TYPE(klass),
			 GTK_SIGNAL_OFFSET(StatusBarClass,progress_begin),
			 gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 0);
     status_bar_signals[PROGRESS_END_SIGNAL] = 
	  gtk_signal_new("progress-end", GTK_RUN_FIRST, GTK_CLASS_TYPE(klass),
			 GTK_SIGNAL_OFFSET(StatusBarClass,progress_end),
			 gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 0);
     gtk_object_class_add_signals(klass,status_bar_signals,LAST_SIGNAL);
}

GtkType status_bar_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "StatusBar",
	       sizeof(StatusBar),
	       sizeof(StatusBarClass),
	       (GtkClassInitFunc)status_bar_class_init,
	       (GtkObjectInitFunc)status_bar_init
	  };
	  id = gtk_type_unique(gtk_fixed_get_type(),&info);
     }
     return id;
}

GtkWidget *status_bar_new(void)
{
     return GTK_WIDGET(gtk_type_new(status_bar_get_type()));
}

static void status_bar_set_mode(StatusBar *sb, gint mode)
{
     gint old_mode;
     if (mode == sb->mode) return;
     old_mode = sb->mode;
     switch (sb->mode) {
     case 0:
	  gtk_widget_hide(GTK_WIDGET(sb->cursor));
	  gtk_widget_hide(GTK_WIDGET(sb->view));
	  gtk_widget_hide(GTK_WIDGET(sb->sel));
	  break;
     case 1:
	  gtk_widget_hide(GTK_WIDGET(sb->progress_label));
	  progress_count --;
	  break;
     case 2:
	  gtk_widget_hide(GTK_WIDGET(sb->fresh_label));
	  break;
     }
     switch (mode) {
     case 0:
	  /* The labels aren't shown here since they may need to be
	   * laid out one by one. Instead, that is handled in
	   * status_bar_set_info. */
	  break;
     case 1:
	  gtk_widget_show(GTK_WIDGET(sb->progress_label));
	  sb->progress_cur = 0;
	  sb->progress_max = 1;
	  sb->bar_width = 0;
	  progress_count ++;
	  sb->progress_break = FALSE;
	  break;
     case 2:
	  gtk_widget_show(GTK_WIDGET(sb->fresh_label));
	  break;
     }
     sb->mode = mode;
     if (old_mode == 1) 
	  gtk_signal_emit(GTK_OBJECT(sb),
			  status_bar_signals[PROGRESS_END_SIGNAL]);
     if (mode == 1) 
	  gtk_signal_emit(GTK_OBJECT(sb),
			  status_bar_signals[PROGRESS_BEGIN_SIGNAL]);
     gtk_widget_queue_draw(GTK_WIDGET(sb));
}

void status_bar_reset(StatusBar *sb)
{
     status_bar_set_mode(sb,2);
}

void status_bar_set_info(StatusBar *sb, off_t cursorpos, gboolean is_rolling,
			 off_t viewstart, off_t viewend, off_t selstart,
			 off_t selend, off_t samplerate, off_t maxvalue)
{
     gchar buf[256];
     gboolean cdif=FALSE,vdif=FALSE,sdif=FALSE,mdif;
     guint p;
     GtkRequisition req;

     sb->rate = samplerate;

     /* What needs to be updated? */
     if (sb->mode != 0) {	  
	  mdif = TRUE;
     } else {	  
	  if (XOR(is_rolling,sb->rolling))
	       cdif = TRUE;
	  else if (is_rolling && status_bar_roll_cursor)
	       cdif = (cursorpos < sb->cur) || (cursorpos > sb->cur+samplerate/20);
	  else 
	       cdif = (!is_rolling && sb->cur != cursorpos);	  

	  vdif = (sb->vs != viewstart) || (sb->ve != viewend);
	  if (selstart == selend)
	       sdif = (sb->ss != sb->se);
	  else
	       sdif = (sb->ss != selstart) || (sb->se != selend);
	  if (maxvalue > sb->max) {
	       mdif = TRUE;
	  } else 
	       mdif = FALSE;
     }     

     /* Hide other labels */
     status_bar_set_mode(sb,0);

     p = LEFT_MARGIN;
     /* Update cursor info */
     if (cdif || mdif) {
	  if (is_rolling && !status_bar_roll_cursor)
	       strcpy(buf,_("Cursor: running"));
	  else {
	       strcpy(buf,_("Cursor: "));
	       get_time(samplerate,cursorpos,maxvalue,buf+strlen(buf),
			default_time_mode);
	  }
	  gtk_label_set_text(sb->cursor,buf);
	  sb->cur = cursorpos;
	  sb->rolling = is_rolling;
     }
     if (mdif) {
	  /* gtk_fixed_move(sb->fixed,GTK_WIDGET(sb->cursor),p,2); */
	  gtk_widget_show(GTK_WIDGET(sb->cursor));
	  gtk_widget_size_request(GTK_WIDGET(sb->cursor),&req);
	  /* add some extra margin to be able to handle
	   * larger values and a LOT of extra margin if using
	   * 'Samples' display mode (slightly hackish I admit)
	   */
	  if (default_time_mode != 2) 
	       p = p + req.width + req.width/4;
	  else
	       p = p + req.width + req.width/2;
     }
     /* Update view info */
     if (vdif || mdif) {
	  g_snprintf(buf,150,_("View: [ %s - %s ]"),
		     get_time(samplerate,viewstart,maxvalue,buf+150,
			      default_time_mode),
		     get_time(samplerate,viewend,maxvalue,buf+200,
			      default_time_mode));
	  gtk_label_set_text(sb->view,buf);
	  sb->vs = viewstart;
	  sb->ve = viewend;
     }
     if (mdif) {
	  gtk_fixed_move(GTK_FIXED(sb),GTK_WIDGET(sb->view),p,2);
	  gtk_widget_show(GTK_WIDGET(sb->view));
	  gtk_widget_size_request(GTK_WIDGET(sb->view),&req);
	  if (default_time_mode != 2)
	       p = p + req.width + 10;
	  else
	       p = p + req.width + req.width/4;
     }
     /* Update selection info */
     if (sdif || mdif) {
	  if (selstart != selend) {
	       g_snprintf(buf,150,_("Selection: %s+%s"),
			  get_time(samplerate,selstart,maxvalue,buf+150,
				   default_time_mode),
			  get_time(samplerate,selend-selstart,maxvalue,
				   buf+200,default_time_mode));
	       gtk_label_set_text(sb->sel,buf);
	  } else
	       gtk_label_set_text(sb->sel,"");
	  sb->ss = selstart;
	  sb->se = selend;
     }
     if (mdif) {
	  gtk_fixed_move(GTK_FIXED(sb),GTK_WIDGET(sb->sel),p,2);
	  gtk_widget_show(GTK_WIDGET(sb->sel));
     }
     /* Update the max value */
     sb->max = maxvalue;
}

void status_bar_begin_progress(StatusBar *sb, off_t progress_length, 
			       gchar *description)
{
     gchar *c;
     if (description == NULL && sb->mode != 1) description=_("Processing data");
     if (description != NULL) {
	  c = g_strdup_printf(_("%s... (Press ESC to cancel)"),description);
	  gtk_label_set_text(sb->progress_label,c);
	  g_free(c);
     }
     status_bar_set_mode(sb,1);
     sb->progress_max = progress_length;
     sb->progress_cur = 0;
}

void status_bar_end_progress(StatusBar *sb)
{
     if (sb != NULL) {
	  if (sb->rate != 0) 
	       status_bar_set_info(sb,sb->cur,sb->rolling,sb->vs,sb->ve,sb->ss,sb->se,
				   sb->rate,sb->max);
	  else
	       status_bar_reset(sb);
     }
}

gboolean status_bar_progress(StatusBar *sb, off_t progress)
{
     guint bw,obw;     
     gfloat f;
     sb->progress_cur += progress;    
     f = ((gfloat)sb->progress_cur)/((gfloat)sb->progress_max);
     if (f > 1.0) f = 1.0;
     f *= (gfloat)(GTK_WIDGET(sb)->allocation.width);
     bw = (guint)f;
     /* Usually, bw should be larger than sb->bar_width since we've done 
      * progress, but it can be smaller if we just shrunk the window. */     
     if (bw > sb->bar_width) {
	  obw = sb->bar_width;
	  gtk_widget_queue_draw_area(GTK_WIDGET(sb),sb->da->allocation.x+obw,
				     sb->da->allocation.y,bw-obw,
				     sb->da->allocation.height);
     } else if (bw < sb->bar_width) {
	  gtk_widget_queue_draw(GTK_WIDGET(sb));
     }
     sb->bar_width = bw;
     idle_work_flag = FALSE;
     while (!idle_work_flag) mainloop();
     return sb->progress_break;
}

void status_bar_break_progress(StatusBar *sb)
{
     sb->progress_break = TRUE;
}

int status_bar_progress_count(void)
{
     return progress_count;
}
