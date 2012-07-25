/*
 * Copyright (C) 2002 2003 2004 2005 2011, Magnus Hjorth
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

#include <stdlib.h>
#include <gtk/gtk.h>
#include "um.h"
#include "float_box.h"
#include "main.h"
#include "gettext.h"

#if GTK_MAJOR_VERSION==2
static GtkEntryClass *parent_class;
#else
static GtkEditableClass *parent_class;
#endif

enum {
     NUMCHANGED_SIGNAL,
     LAST_SIGNAL
};

#if GTK_MAJOR_VERSION == 2

#include "float_box_marsh.c"
#define gtk_marshal_NONE__FLOAT gtk_marshal_VOID__FLOAT

#else
typedef void (*GtkSignal_NONE__FLOAT) (GtkObject *object, gfloat arg, 
				       gpointer user_data);

static void gtk_marshal_NONE__FLOAT(GtkObject *object, GtkSignalFunc func,
				    gpointer func_data, GtkArg *args)
{
     GtkSignal_NONE__FLOAT rfunc;
     rfunc=(GtkSignal_NONE__FLOAT)func;
     rfunc(object,GTK_VALUE_FLOAT(args[0]),func_data);
}
#endif

static guint floatbox_signals[LAST_SIGNAL] = { 0 };

static void floatbox_update_text(Floatbox *box)
{
     char e[30];
     format_float(box->val, e, sizeof(e));
     gtk_entry_set_text(GTK_ENTRY(box),e);
}

#if GTK_MAJOR_VERSION==2
static void floatbox_activate(GtkEntry *editable)
#else
static void floatbox_activate(GtkEditable *editable)
#endif
{
     float f;
     char *c,*d;
     c=(char *)gtk_entry_get_text(GTK_ENTRY(editable));
     f=strtod(c,&d);
     if (*d==0)
	  floatbox_set(FLOATBOX(editable),f);
     else
	  floatbox_update_text(FLOATBOX(editable));
     if (parent_class->activate) parent_class->activate(editable);
}

static gint floatbox_focus_out(GtkWidget *widget, GdkEventFocus *event)
{
     char *c,*d;
     float f;
     Floatbox *b = FLOATBOX(widget);
     c=(char *)gtk_entry_get_text(GTK_ENTRY(widget));
     f=strtod(c,&d);
     if (*d==0 && b->adj!=NULL && f>=gtk_adjustment_get_lower(b->adj) &&
	 f<=gtk_adjustment_get_upper(b->adj)) {
	  gtk_adjustment_set_value(b->adj,f);
     }
     return GTK_WIDGET_CLASS(parent_class)->focus_out_event(widget,event);
}

static void floatbox_class_init(FloatboxClass *klass)
{
     parent_class = gtk_type_class(gtk_entry_get_type());

#if GTK_MAJOR_VERSION==2
     GTK_ENTRY_CLASS(klass)->activate = floatbox_activate;
#else
     GTK_EDITABLE_CLASS(klass)->activate = floatbox_activate;
#endif
     GTK_WIDGET_CLASS(klass)->focus_out_event = floatbox_focus_out;
     klass->numchange=NULL;

     floatbox_signals[NUMCHANGED_SIGNAL] = 
	  gtk_signal_new("numchanged",GTK_RUN_FIRST,
			 GTK_CLASS_TYPE(klass),
			 GTK_SIGNAL_OFFSET(FloatboxClass,numchange),
			 gtk_marshal_NONE__FLOAT,GTK_TYPE_NONE,1,
			 GTK_TYPE_FLOAT);

     gtk_object_class_add_signals(GTK_OBJECT_CLASS(klass),floatbox_signals,
				  LAST_SIGNAL);
}

static void floatbox_init(Floatbox *fbox)
{
#if GTK_MAJOR_VERSION == 2
     gtk_entry_set_width_chars(GTK_ENTRY(fbox),10);
#else
     GtkRequisition req;
     gtk_widget_size_request(GTK_WIDGET(fbox),&req);
     gtk_widget_set_usize(GTK_WIDGET(fbox),req.width/3,req.height);
#endif
     fbox->adj = NULL;
}

GtkType floatbox_get_type(void)
{
     static GtkType id=0;
     if (!id) {
	  GtkTypeInfo info = {
	       "Floatbox",
	       sizeof(Floatbox),
	       sizeof(FloatboxClass),
	       (GtkClassInitFunc) floatbox_class_init,
	       (GtkObjectInitFunc) floatbox_init };
	  id=gtk_type_unique(gtk_entry_get_type(),&info);
     }
     return id;
}

void floatbox_set(Floatbox *box, float val)
{
     if (box->val == val) return;
     if (box->adj != NULL && 
	 val >= gtk_adjustment_get_lower(box->adj) && 
	 val <= gtk_adjustment_get_upper(box->adj)) {

	  gtk_adjustment_set_value(box->adj, val);
	  return;
     }
     box->val=val;
     floatbox_update_text(box);
     gtk_signal_emit(GTK_OBJECT(box),floatbox_signals[NUMCHANGED_SIGNAL],box->val);
}

GtkWidget *floatbox_new(float val)
{
     Floatbox *box;
     box=gtk_type_new(floatbox_get_type());
     box->val = val-1.0; /* To force update */
     floatbox_set(box,val);
     return GTK_WIDGET(box);
}

gboolean floatbox_check(Floatbox *box)
{
     gfloat f;
     char *c,*d;
     c=(char *)gtk_entry_get_text(GTK_ENTRY(box));
     f=strtod(c,&d);
     if (*d==0) {
	  floatbox_set(box,f);
	  return FALSE;
     } else {
	  d = g_strdup_printf(_("'%s' is not a number!"),c);
	  user_error(d);
	  g_free(d);
	  return TRUE;
     }
}

gboolean floatbox_check_limit(Floatbox *box, float lowest, float highest,
			      gchar *valuename)
{
     gfloat f;
     char *c,*d;
     c=(char *)gtk_entry_get_text(GTK_ENTRY(box));
     f=strtod(c,&d);
     if (*d==0 && f >= lowest && f <= highest) {
	  floatbox_set(box,f);
	  return FALSE;
     } else {
	  d = g_strdup_printf(_("Value for '%s' must be a number between %f and "
			      "%f"),valuename,lowest,highest);
	  user_error(d);
	  g_free(d);
	  return TRUE;
     }     
}


static void floatbox_adj_changed(GtkAdjustment *adjustment, gpointer user_data)
{
     Floatbox *box = FLOATBOX(user_data);
     box->val = box->adj->value;
     floatbox_update_text(box);
     gtk_signal_emit(GTK_OBJECT(box),floatbox_signals[NUMCHANGED_SIGNAL],
		     box->val);     
}

GtkWidget *floatbox_create_scale(Floatbox *box, float minval, float maxval)
{
     GtkWidget *w;
#if GTK_MAJOR_VERSION > 1
     GtkRequisition req;
#endif
     if (box->adj == NULL) {	  
	  box->adj = GTK_ADJUSTMENT(gtk_adjustment_new(minval,minval,
						       maxval+
						       (maxval-minval)/10.0,
						       (maxval-minval)/100.0,
						       (maxval-minval)/4.0,
						       (maxval-minval)/10.0));
	  gtk_signal_connect(GTK_OBJECT(box->adj),"value_changed",
			     GTK_SIGNAL_FUNC(floatbox_adj_changed),box);
	  gtk_adjustment_set_value(box->adj,box->val);
     }
     w = gtk_hscale_new(box->adj);
#if GTK_MAJOR_VERSION > 1
     gtk_widget_size_request(w,&req);
     gtk_widget_set_usize(w,req.width*5,req.height);
#endif
     return w;
}
