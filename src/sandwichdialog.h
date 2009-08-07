/*
 * Copyright (C) 2009, Magnus Hjorth
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


#ifndef SANDWICHDIALOG_H_INCLUDED
#define SANDWICHDIALOG_H_INCLUDED

#include <gtk/gtk.h>
#include "effectdialog.h"
#include "documentlist.h"

#define SANDWICH_DIALOG(obj) GTK_CHECK_CAST(obj,sandwich_dialog_get_type(),SandwichDialog)
#define SANDWICH_DIALOG_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,sandwich_dialog_get_type(),SandwichDialogClass)
#define IS_SANDWICH_DIALOG(obj) GTK_CHECK_TYPE(obj,sandwich_dialog_get_type())

typedef struct {
     EffectDialog ed;
     DocumentList *docsel;
     GtkToggleButton *align_begin,*align_end,*align_marker;
     GtkEntry *marker_entry;
} SandwichDialog;

typedef struct {
     EffectDialogClass edc;
} SandwichDialogClass;

GtkType sandwich_dialog_get_type(void);

#endif
