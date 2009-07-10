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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


#ifndef VOLUME_DIALOG_H_INCLUDED
#define VOLUME_DIALOG_H_INCLUDED

#include <gtk/gtk.h>
#include "mainwindow.h"
#include "effectdialog.h"
#include "float_box.h"

#define VOLUME_DIALOG(obj) GTK_CHECK_CAST(obj,volume_dialog_get_type(),VolumeDialog)
#define VOLUME_DIALOG_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,volume_dialog_get_type(),VolumeDialogClass)
#define IS_VOLUME_DIALOG(obj) GTK_CHECK_TYPE(obj,volume_dialog_get_type())

typedef struct _VolumeDialog {
     EffectDialog ed;
     Floatbox *start_percent, *end_percent;
} VolumeDialog;

typedef struct _VolumeDialogClass {
     EffectDialogClass ed_class;
} VolumeDialogClass;

GtkType volume_dialog_get_type(void);

#endif
