/*
 * Copyright (C) 2003 2004, Magnus Hjorth
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


#ifndef EFFECTBROWSER_H_INCLUDED
#define EFFECTBROWSER_H_INCLUDED


#include "documentlist.h"
#include "effectdialog.h"


#define EFFECT_BROWSER(obj) GTK_CHECK_CAST(obj,effect_browser_get_type(),EffectBrowser)
#define EFFECT_BROWSER_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,effect_browser_get_type(),EffectBrowserClass)
#define IS_EFFECT_BROWSER(obj) GTK_CHECK_TYPE(obj,effect_browser_get_type())




typedef struct {    
     GtkWindow window;

     DocumentList *dl;

     GtkBox *mw_list_box;
     GtkList *effect_list;

     EffectDialog **dialogs;
     gint current_dialog;

     GtkContainer *effect_list_container;
     GtkContainer *dialog_container;
} EffectBrowser;


typedef struct {
     GtkWindowClass window_class;
} EffectBrowserClass;

void effect_browser_register_default_effects(void);

GtkType effect_browser_get_type(void);
GtkWidget *effect_browser_new(Document *doc);
GtkWidget *effect_browser_new_with_effect(Document *doc, gchar *effect_name);

void effect_browser_set_effect(EffectBrowser *eb, gchar *effect_name);

void effect_browser_register_effect(gchar *name, gchar *title, 
				    GtkType dialog_type);
void effect_browser_invalidate_effect(EffectBrowser *eb, gchar *effect_name);

void effect_browser_shutdown(void);
				    
#endif
