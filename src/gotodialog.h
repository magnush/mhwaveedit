/*
 * Copyright (C) 2003, Magnus Hjorth
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


#ifndef GOTODIALOG_H_INCLUDED
#define GOTODIALOG_H_INCLUDED

#include <gtk/gtk.h>
#include "float_box.h"
#include "mainwindow.h"

#define GOTO_DIALOG(obj) GTK_CHECK_CAST(obj,goto_dialog_get_type(),GotoDialog)
#define GOTO_DIALOG_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,goto_dialog_get_type(),GotoDialogClass)
#define IS_GOTO_DIALOG(obj) GTK_CHECK_TYPE(obj,goto_dialog_get_type())

#define GOTO_DIALOG_POS_AFTER_BEG_FILE 0
#define GOTO_DIALOG_POS_AFTER_END_FILE 1
#define GOTO_DIALOG_POS_AFTER_CURSOR 2
#define GOTO_DIALOG_POS_AFTER_BEG_SEL 3
#define GOTO_DIALOG_POS_AFTER_END_SEL 4

typedef struct _GotoDialog {
     GtkWindow parent;
     Mainwindow *mw;
     Floatbox *offset;
     GtkToggleButton *relbuttons[5];
} GotoDialog;

typedef struct _GotoDialogClass {
     GtkWindowClass edc;
} GotoDialogClass;

GtkType goto_dialog_get_type(void);
GtkWidget *goto_dialog_new(Mainwindow *mw);

#endif
