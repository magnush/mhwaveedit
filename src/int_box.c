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
#include "int_box.h"
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

typedef void (*GtkSignal_NONE__LONG) (GtkObject *object, long int arg1, 
				       gpointer user_data);

#if GTK_MAJOR_VERSION == 2

#    include "int_box_marsh.c"
#    define gtk_marshal_NONE__LONG gtk_marshal_VOID__LONG

#else

static void gtk_marshal_NONE__LONG(GtkObject *object, GtkSignalFunc func,
				    gpointer func_data, GtkArg *args)
{
     GtkSignal_NONE__LONG rfunc;
     rfunc=(GtkSignal_NONE__LONG)func;
     rfunc(object,GTK_VALUE_LONG(args[0]),func_data);
}

#endif /* GTK 2 */

static guint intbox_signals[LAST_SIGNAL] = { 0 };

static void intbox_update_text(Intbox *box)
{
     gchar e[30];
     g_snprintf(e,sizeof(e),"%ld",box->val);
     gtk_entry_set_text(GTK_ENTRY(box),e);
}

#if GTK_MAJOR_VERSION==2
static void intbox_activate(GtkEntry *editable)
#else
static void intbox_activate(GtkEditable *editable)
#endif
{
     long l;
     char *c,*d;
     c=(char *)gtk_entry_get_text(GTK_ENTRY(editable));
     l=strtol(c,&d,10);
     if (*d==0)
	  intbox_set(INTBOX(editable),l);
     else
	  intbox_update_text(INTBOX(editable));
     if (parent_class->activate) parent_class->activate(editable);
}

static gint intbox_focus_out(GtkWidget *widget, GdkEventFocus *event)
{
     long l;
     char *c,*d;
     Intbox *b = INTBOX(widget);
     c=(char *)gtk_entry_get_text(GTK_ENTRY(widget));
     l=strtol(c,&d,10);
     if (*d==0 && b->adj!=NULL && l>=(long)gtk_adjustment_get_lower(b->adj) &&
	 l<=(long)gtk_adjustment_get_upper(b->adj))
	  gtk_adjustment_set_value(b->adj,l);
     return GTK_WIDGET_CLASS(parent_class)->focus_out_event(widget,event);
}

static void intbox_class_init(IntboxClass *klass)
{
     parent_class = gtk_type_class(gtk_entry_get_type());
#if GTK_MAJOR_VERSION==2
     GTK_ENTRY_CLASS(klass)->activate = intbox_activate;
#else
     GTK_EDITABLE_CLASS(klass)->activate = intbox_activate;
#endif
     GTK_WIDGET_CLASS(klass)->focus_out_event = intbox_focus_out;
     klass->numchange=NULL;
     intbox_signals[NUMCHANGED_SIGNAL] = 
	  gtk_signal_new("numchanged",GTK_RUN_FIRST,
			 GTK_CLASS_TYPE(klass),
			 GTK_SIGNAL_OFFSET(IntboxClass,numchange),
			 gtk_marshal_NONE__LONG,GTK_TYPE_NONE,1,
			 GTK_TYPE_LONG);

     gtk_object_class_add_signals(GTK_OBJECT_CLASS(klass),intbox_signals,
				  LAST_SIGNAL);
}

static void intbox_init(Intbox *fbox)
{
#if GTK_MAJOR_VERSION==2
     gtk_entry_set_width_chars(GTK_ENTRY(fbox),10);
#else
     GtkRequisition req;
     gtk_widget_size_request(GTK_WIDGET(fbox),&req);
     gtk_widget_set_usize(GTK_WIDGET(fbox),req.width/3,req.height);
#endif
     fbox->adj = NULL;
}

GtkType intbox_get_type(void)
{
static GtkType id=0;
if (!id) {
	GtkTypeInfo info = {
		"Intbox",
		sizeof(Intbox),
		sizeof(IntboxClass),
		(GtkClassInitFunc) intbox_class_init,
		(GtkObjectInitFunc) intbox_init,
		NULL,
		NULL};
	id=gtk_type_unique(gtk_entry_get_type(),&info);
	}
return id;
}

void intbox_set(Intbox *box, long val)
{
if (box->val == val) return;
 if (box->adj != NULL && 
     val >= (long)gtk_adjustment_get_lower(box->adj) && 
     val <= (long)gtk_adjustment_get_upper(box->adj)) {
      gtk_adjustment_set_value(box->adj,(gfloat)val);
      return;
 } 
box->val=val;
 intbox_update_text(box); 
gtk_signal_emit(GTK_OBJECT(box),intbox_signals[NUMCHANGED_SIGNAL],box->val);
}

GtkWidget *intbox_new(long val)
{
Intbox *box;
box=gtk_type_new(intbox_get_type());
box->val=val-1;
intbox_set(box,val);
return GTK_WIDGET(box);
}

gboolean intbox_check(Intbox *box)
{
     long l;
     char *c,*d;
     c=(char *)gtk_entry_get_text(GTK_ENTRY(box));
     l=strtol(c,&d,10);
     if (*d==0) {
	  intbox_set(box,l);
	  return FALSE;
     } else {
	  d = g_strdup_printf(_("'%s' is not a number!"),c);
	  user_error(d);
	  g_free(d);
	  return TRUE;
     }
}

gboolean intbox_check_limit(Intbox *box, long int lowest, long int highest,
			    gchar *valuename)
{
     long l;
     char *c,*d;
     c = (char *)gtk_entry_get_text(GTK_ENTRY(box));
     l = strtol(c,&d,10);
     if (*d == 0 && l >= lowest && l <= highest) {
	  intbox_set(box,l);
	  return FALSE;
     } else {
	  d = g_strdup_printf(_("Value for %s must be a number between %ld and "
			      "%ld"),valuename,lowest,highest);
	  user_error(d);
	  g_free(d);
	  return TRUE;
     }
}

static void intbox_adj_changed(GtkAdjustment *adjustment, gpointer user_data)
{
     Intbox *box = INTBOX(user_data);
     box->val = box->adj->value;
     intbox_update_text(box);
     gtk_signal_emit(GTK_OBJECT(box),intbox_signals[NUMCHANGED_SIGNAL],
		     box->val);
}


GtkWidget *intbox_create_scale(Intbox *box, long minval, long maxval)
{
     GtkWidget *w;
#if GTK_MAJOR_VERSION > 1
     GtkRequisition req;
#endif

     if (box->adj == NULL) {	  
	  box->adj = GTK_ADJUSTMENT(gtk_adjustment_new(minval,minval,
						       maxval+1.0,
						       1.0,
						       10.0,
						       1.0));
	  gtk_signal_connect(GTK_OBJECT(box->adj),"value_changed",
			     GTK_SIGNAL_FUNC(intbox_adj_changed),box);
	  gtk_adjustment_set_value(box->adj,box->val);
     }
     w = gtk_hscale_new(box->adj);
     gtk_scale_set_digits(GTK_SCALE(w),0);
#if GTK_MAJOR_VERSION > 1
     gtk_widget_size_request(w,&req);
     gtk_widget_set_usize(w,req.width*5,req.height);
#endif
     return w;
}
