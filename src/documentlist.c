/*
 * Copyright (C) 2002 2003 2004 2005 2009, Magnus Hjorth
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


#include "documentlist.h"

static GtkObjectClass *parent_class;
enum { CHANGED_SIGNAL, LAST_SIGNAL };
static guint document_list_signals[LAST_SIGNAL] = { 0 };
static gboolean updating = FALSE;

static void document_list_changed(Combo *combo)
{
     Document *d=NULL;
     DocumentList *dl = DOCUMENT_LIST(combo);

     if (updating) return;
     d = DOCUMENT(list_object_get(document_objects,
				  combo_selected_index(combo)));     
     if (d != dl->selected) {
	  dl->selected = d;
	  memcpy(&(dl->format),&(d->chunk->format),sizeof(Dataformat));
	  gtk_signal_emit(GTK_OBJECT(dl),
			  document_list_signals[CHANGED_SIGNAL]);
     }

     if (COMBO_CLASS(parent_class)->selection_changed)
	  COMBO_CLASS(parent_class)->selection_changed(combo);
}

static void document_list_addnotify(ListObject *lo, gpointer item, 
				      gpointer user_data)
{
     Document *d = DOCUMENT(item);
     DocumentList *dl = DOCUMENT_LIST(user_data);
     if (dl->selected == NULL) {
	  dl->selected=d;
	  gtk_widget_set_sensitive(GTK_WIDGET(dl),TRUE);
     }
     document_list_setup(dl,dl->selected);     
     if (d == dl->selected) {	  
	  gtk_signal_emit(GTK_OBJECT(dl),
			  document_list_signals[CHANGED_SIGNAL]);
     }
}

static void document_list_remove(ListObject *lo, gpointer item,
				   gpointer user_data)
{
     Document *w = DOCUMENT(item);
     DocumentList *mwl = DOCUMENT_LIST(user_data);
     if (w == mwl->selected) {
	  if (list_object_get_size(document_objects) == 0) {
	       /* We set the selected item to NULL and dim the widget, but 
		* no signal is sent out. */
	       mwl->selected = NULL;
	       gtk_widget_set_sensitive(GTK_WIDGET(mwl),FALSE);
	       return;
	  }
	  mwl->selected = DOCUMENT(list_object_get(document_objects,0));
	  document_list_setup(mwl,mwl->selected);
	  gtk_signal_emit(GTK_OBJECT(mwl),
			  document_list_signals[CHANGED_SIGNAL]);
     } else
	  document_list_setup(mwl,mwl->selected);

}

static void document_list_init(GtkObject *obj)
{
     DocumentList *mwl = DOCUMENT_LIST(obj);
     /* Most initialization is done in document_list_setup */
     combo_set_max_request_width(COMBO(obj),350);
     mwl->selected = NULL;
     gtk_signal_connect_while_alive(GTK_OBJECT(document_objects), 
				    "item_added",
				    GTK_SIGNAL_FUNC(document_list_addnotify),
				    obj,obj);
     gtk_signal_connect_while_alive(GTK_OBJECT(document_objects), 
				    "item_removed",
				    GTK_SIGNAL_FUNC(document_list_remove),
				    obj,obj);
     gtk_signal_connect_while_alive(GTK_OBJECT(document_objects), 
				    "item_notify",
				    GTK_SIGNAL_FUNC(document_list_addnotify),
				    obj,obj);
}

static void document_list_class_init(GtkObjectClass *klass)
{
     DocumentListClass *mwlc = DOCUMENT_LIST_CLASS(klass);
     parent_class = gtk_type_class(combo_get_type());
     mwlc->document_changed = NULL;
     COMBO_CLASS(klass)->selection_changed = document_list_changed;

     document_list_signals[CHANGED_SIGNAL] = 
	  gtk_signal_new( "document_changed", GTK_RUN_FIRST, 
			  GTK_CLASS_TYPE(klass),
			  GTK_SIGNAL_OFFSET(DocumentListClass,
					    document_changed),
			  gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 0);
     gtk_object_class_add_signals(klass,document_list_signals,LAST_SIGNAL);
}

GtkType document_list_get_type(void)
{
     static GtkType id=0;
     if (!id) {
	  GtkTypeInfo info = {
	       "DocumentList",
	       sizeof(DocumentList),
	       sizeof(DocumentListClass),
	       (GtkClassInitFunc)document_list_class_init,
	       (GtkObjectInitFunc)document_list_init
	  };
	  id = gtk_type_unique( combo_get_type(), &info );	       
     }	
     return id;
}

GtkWidget *document_list_new(Document *chosen)
{
     GtkWidget *widget;
     widget = GTK_WIDGET(gtk_type_new(document_list_get_type()));
     document_list_setup(DOCUMENT_LIST(widget),chosen);
     return widget;     
}

struct setup_func_data {
     GList *lp;
     Document *first;
};

static void document_list_setup_func(gpointer item, gpointer user_data)
{
     struct setup_func_data *sfdp = (struct setup_func_data *)user_data;
     Document *w = DOCUMENT(item);
     if (w->titlename != NULL) {
	  sfdp->lp = g_list_append(sfdp->lp, w->titlename);
	  if (sfdp->first == NULL) sfdp->first = w;
     }
}

void document_list_setup(DocumentList *mwl, Document *chosen)
{   
     gint i;
     struct setup_func_data sfd;
     updating = TRUE;     

     sfd.lp = NULL;
     sfd.first = NULL;
     list_object_foreach(document_objects,document_list_setup_func,&sfd);
     combo_set_items(COMBO(mwl),sfd.lp,0);

     if (chosen == NULL) 
	  chosen = sfd.first;

     if (chosen) {
	  i = g_list_index(sfd.lp,chosen->titlename);
	  g_assert(i >= 0);
	  combo_set_selection(COMBO(mwl),i);
	  memcpy(&(mwl->format),&(chosen->chunk->format),
		 sizeof(Dataformat));
	  mwl->selected = chosen;
     }
     g_list_free(sfd.lp);     
     updating = FALSE;
}
