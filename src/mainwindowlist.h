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


/* Main window selection widget for the effect browser */

#ifndef MAINWINDOWLIST_H_INCLUDED
#define MAINWINDOWLIST_H_INCLUDED

#include "combo.h"
#include "mainwindow.h"
#include "listobject.h"

#define MAINWINDOW_LIST(obj) GTK_CHECK_CAST(obj,mainwindow_list_get_type(),MainwindowList)
#define MAINWINDOW_LIST_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,mainwindow_list_get_type(),MainwindowListClass)
#define IS_MAINWINDOW_LIST(obj) GTK_CHECK_TYPE(obj,mainwindow_list_get_type())

typedef struct {
     Combo combo;
     Mainwindow *selected;
     /* Format of the selected window. The reason to keep track of this 
      * separately is that it makes it easier to handle the special case
      * where there are no windows (selected==NULL) */
     Dataformat format; 
} MainwindowList;

typedef struct {
     ComboClass comboclass;
     /* The current mainwindow has changed, either by the user or the 
      * selected mainwindow was closed, or the chunk in the current 
      * mainwindow has changed. . */
     void (*window_changed)(MainwindowList *mwl);     
} MainwindowListClass;

GtkType mainwindow_list_get_type(void);
GtkWidget *mainwindow_list_new(Mainwindow *chosen);
void mainwindow_list_setup(MainwindowList *mwl, Mainwindow *chosen);

#endif
