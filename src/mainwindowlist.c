/*
 * Copyright (C) 2002 2003 2004, Magnus Hjorth
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


#include "mainwindowlist.h"

static GtkObjectClass *parent_class;
enum { CHANGED_SIGNAL, LAST_SIGNAL };
static gint mainwindow_list_signals[LAST_SIGNAL] = { 0 };
static gboolean updating = FALSE;

static void mainwindow_list_changed(Combo *combo)
{
     gint s;
     Mainwindow *w=NULL;
     MainwindowList *mwl = MAINWINDOW_LIST(combo);

     if (updating) return;
     s = list_object_get_size(mainwindow_objects);
     w = MAINWINDOW(list_object_get(mainwindow_objects,
				    combo_selected_index(combo)));     
     if (w != mwl->selected) {
	  mwl->selected = w;
	  /* printf("Changed to window %s\n",w->titlename); */
	  memcpy(&(mwl->format),&(w->view->chunk->format),sizeof(Dataformat));
	  gtk_signal_emit(GTK_OBJECT(mwl),
			  mainwindow_list_signals[CHANGED_SIGNAL]);
     }

     if (COMBO_CLASS(parent_class)->selection_changed)
	  COMBO_CLASS(parent_class)->selection_changed(combo);
}

static void mainwindow_list_addnotify(ListObject *lo, gpointer item, 
				      gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(item);
     MainwindowList *mwl = MAINWINDOW_LIST(user_data);
     if (mwl->selected == NULL) {
	  mwl->selected=w;
	  gtk_widget_set_sensitive(GTK_WIDGET(mwl),TRUE);
     }
     mainwindow_list_setup(mwl,mwl->selected);     
     if (w == mwl->selected) {	  
	  gtk_signal_emit(GTK_OBJECT(mwl),
			  mainwindow_list_signals[CHANGED_SIGNAL]);
     }
}

static void mainwindow_list_remove(ListObject *lo, gpointer item,
				   gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(item);
     MainwindowList *mwl = MAINWINDOW_LIST(user_data);
     if (w == mwl->selected) {
	  if (list_object_get_size(mainwindow_objects) == 0) {
	       /* We set the selected item to NULL and dim the widget, but 
		* no signal is sent out. */
	       mwl->selected = NULL;
	       gtk_widget_set_sensitive(GTK_WIDGET(mwl),FALSE);
	       return;
	  }
	  mwl->selected = MAINWINDOW(list_object_get(mainwindow_objects,0));
	  mainwindow_list_setup(mwl,mwl->selected);
	  gtk_signal_emit(GTK_OBJECT(mwl),
			  mainwindow_list_signals[CHANGED_SIGNAL]);
     } else
	  mainwindow_list_setup(mwl,mwl->selected);

}

static void mainwindow_list_init(GtkObject *obj)
{
     MainwindowList *mwl = MAINWINDOW_LIST(obj);
     /* Most initialization is done in mainwindow_list_setup */
     mwl->selected = NULL;
     gtk_signal_connect_while_alive(GTK_OBJECT(mainwindow_objects), 
				    "item_added",
				    GTK_SIGNAL_FUNC(mainwindow_list_addnotify),
				    obj,obj);
     gtk_signal_connect_while_alive(GTK_OBJECT(mainwindow_objects), 
				    "item_removed",
				    GTK_SIGNAL_FUNC(mainwindow_list_remove),
				    obj,obj);
     gtk_signal_connect_while_alive(GTK_OBJECT(mainwindow_objects), 
				    "item_notify",
				    GTK_SIGNAL_FUNC(mainwindow_list_addnotify),
				    obj,obj);
}

static void mainwindow_list_class_init(GtkObjectClass *klass)
{
     MainwindowListClass *mwlc = MAINWINDOW_LIST_CLASS(klass);
     parent_class = gtk_type_class(combo_get_type());
     mwlc->window_changed = NULL;
     COMBO_CLASS(klass)->selection_changed = mainwindow_list_changed;

     mainwindow_list_signals[CHANGED_SIGNAL] = 
	  gtk_signal_new( "window_changed", GTK_RUN_FIRST, 
			  GTK_CLASS_TYPE(klass),
			  GTK_SIGNAL_OFFSET(MainwindowListClass,
					    window_changed),
			  gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 0);
     gtk_object_class_add_signals(klass,mainwindow_list_signals,LAST_SIGNAL);
}

GtkType mainwindow_list_get_type(void)
{
     static GtkType id=0;
     if (!id) {
	  GtkTypeInfo info = {
	       "MainwindowList",
	       sizeof(MainwindowList),
	       sizeof(MainwindowListClass),
	       (GtkClassInitFunc)mainwindow_list_class_init,
	       (GtkObjectInitFunc)mainwindow_list_init
	  };
	  id = gtk_type_unique( combo_get_type(), &info );	       
     }	
     return id;
}

GtkWidget *mainwindow_list_new(Mainwindow *chosen)
{
     GtkWidget *widget;
     widget = GTK_WIDGET(gtk_type_new(mainwindow_list_get_type()));
     mainwindow_list_setup(MAINWINDOW_LIST(widget),chosen);
     return widget;     
}

static void mainwindow_list_setup_func(gpointer item, gpointer user_data)
{
     GList **lp = (GList **)user_data;
     Mainwindow *w = MAINWINDOW(item);
     if (w->titlename != NULL)
	  *lp = g_list_append(*lp, w->titlename);
}

void mainwindow_list_setup(MainwindowList *mwl, Mainwindow *chosen)
{   
     GList *l = NULL;
     gint i;
     updating = TRUE;     
     list_object_foreach(mainwindow_objects,mainwindow_list_setup_func,&l);
     combo_set_items(COMBO(mwl),l,0);
     if (chosen) {
	  i = g_list_index(l,chosen->titlename);
	  g_assert(i >= 0);
	  combo_set_selection(COMBO(mwl),i);
	  memcpy(&(mwl->format),&(chosen->view->chunk->format),
		 sizeof(Dataformat));
	  mwl->selected = chosen;
     }
     g_list_free(l);     
     updating = FALSE;
}
