/*
 * Copyright (C) 2003 2004 2007, Magnus Hjorth
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


#ifndef EFFECTBROWSER_H_INCLUDED
#define EFFECTBROWSER_H_INCLUDED


#include "documentlist.h"
#include "effectdialog.h"


#define EFFECT_BROWSER(obj) GTK_CHECK_CAST(obj,effect_browser_get_type(),EffectBrowser)
#define EFFECT_BROWSER_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,effect_browser_get_type(),EffectBrowserClass)
#define IS_EFFECT_BROWSER(obj) GTK_CHECK_TYPE(obj,effect_browser_get_type())


#define EFFECT_BROWSER_CACHE_SIZE 8

typedef struct {    
     GtkWindow window;

     DocumentList *dl;

     GtkBox *mw_list_box;
     GtkList *list_widget;
     GtkListItem *list_widget_sel,*list_widget_clicked;

     GtkToggleButton *close_after;

     gint current_dialog;
     EffectDialog *dialogs[EFFECT_BROWSER_CACHE_SIZE];
     gpointer dialog_effects[EFFECT_BROWSER_CACHE_SIZE];

     GtkContainer *effect_list_container;
     GtkContainer *dialog_container;
} EffectBrowser;


typedef struct {
     GtkWindowClass window_class;
} EffectBrowserClass;

typedef void (*effect_register_rebuild_func)(gchar source_tag,
					     gpointer user_data);
typedef EffectDialog *(*effect_register_get_func)(gchar *name, 
						  gchar source_tag, 
						  gpointer user_data);

#define EFFECT_PARAM_TAG 0
#define EFFECT_PARAM_TITLE 1
#define EFFECT_PARAM_AUTHOR 2
#define EFFECT_PARAM_LOCATION 3
#define EFFECT_PARAM_MAX 3

void effect_register_init(void);
void effect_register_add_source(gchar *name, gchar tag,
				effect_register_rebuild_func rebuild_func,
				gpointer rebuild_func_data,
				effect_register_get_func get_func,
				gpointer get_func_data);
void effect_register_add_effect(gchar source_tag, const gchar *name, 
				const gchar *title, const gchar *author, 
				const gchar *location);
void effect_register_rebuild(void);


GtkType effect_browser_get_type(void);
GtkWidget *effect_browser_new(Document *doc);
GtkWidget *effect_browser_new_with_effect(Document *doc, gchar *effect_name, 
					  gchar source_tag, 
					  gboolean close_after);

void effect_browser_set_effect(EffectBrowser *eb, gchar *effect_name, 
			       gchar source_tag);

void effect_browser_invalidate_effect(EffectBrowser *eb, gchar *effect_name,
				      gchar source_tag);

void effect_browser_shutdown(void);
				    
#endif
