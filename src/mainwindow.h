/*
 * Copyright (C) 2002 2003 2004 2005 2006 2011 2012, Magnus Hjorth
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


#ifndef MAINWINDOW_H_INCLUDED
#define MAINWINDOW_H_INCLUDED

#include <gtk/gtk.h>
#include "chunkview.h"
#include "statusbar.h"
#include "listobject.h"

#define MAINWINDOW(obj) GTK_CHECK_CAST(obj,mainwindow_get_type(),Mainwindow)
#define MAINWINDOW_CLASS(class) GTK_CHECK_CLASS_CAST(class,mainwindow_get_type(),MainwindowClass)
#define IS_MAINWINDOW(obj) GTK_CHECK_TYPE(obj,mainwindow_get_type())

#define MAINWINDOW_RECENT_MAX 20
#define MAINWINDOW_SCROLL_DELTA_RATIO 4
#define MAINWINDOW_NUDGE_DELTA_RATIO 8

typedef struct _Mainwindow {
     GtkWindow window;

     ChunkView *view;
     GtkAdjustment *view_adj,*zoom_adj,*vertical_zoom_adj,*speed_adj;
     StatusBar *statusbar; 

     gboolean sensitive;
     GtkWidget *menubar,*toolbar;          

     GtkWidget *vzoom_icon,*vzoom_slider,*hzoom_icon,*hzoom_slider;
     GtkWidget *speed_icon,*speed_slider;
     GtkLabel *vzoom_label, *speed_label;
     gboolean show_labels;
     gboolean esc_pressed_flag;

     GList *need_chunk_items;
     GList *need_selection_items;
     GList *need_clipboard_items;
     GList *need_undo_items, *need_redo_items;
     GList *zoom_items;

     Document *doc;

     gboolean loopmode,bouncemode;     
     gboolean followmode;
     GtkWidget *recent_sep; /* Separator that should be hidden if there 
			     * aren't any recent files */
     GList *recent; /* List of menu items with recent files */
} Mainwindow;

typedef struct _MainwindowClass {
     GtkWindowClass parent;
} MainwindowClass;

extern ListObject *mainwindow_objects;

extern gboolean autoplay_mark_flag, varispeed_reset_flag;

GtkType mainwindow_get_type(void);
GtkWidget *mainwindow_new();
GtkWidget *mainwindow_new_with_file(char *filename, gboolean log);

gboolean mainwindow_update_caches(void);

void mainwindow_update_texts(void);
void mainwindow_repaint_views(void);
void mainwindow_set_sensitive(Mainwindow *mw, gboolean sensitive);
void mainwindow_set_all_sensitive(gboolean active);
void mainwindow_set_speed_sensitive(gboolean s);

Mainwindow *mainwindow_playing_window(void);

#endif
