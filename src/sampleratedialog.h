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


#ifndef SAMPLERATE_DIALOG_H_INCLUDED
#define SAMPLERATE_DIALOG_H_INCLUDED

#include <gtk/gtk.h>
#include "mainwindow.h"
#include "effectdialog.h"
#include "int_box.h"

#define SAMPLERATE_DIALOG(obj) GTK_CHECK_CAST(obj,samplerate_dialog_get_type(),SamplerateDialog)
#define SAMPLERATE_DIALOG_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,samplerate_dialog_get_type(),SamplerateDialogClass)
#define IS_SAMPLERATE_DIALOG(obj) GTK_CHECK_TYPE(obj,samplerate_dialog_get_type())

typedef struct _SamplerateDialog {
     EffectDialog ed;
     Intbox *rate;
     guint method;
} SamplerateDialog;

typedef struct _SamplerateDialogClass {
     EffectDialogClass ed_class;
} SamplerateDialogClass;

GtkType samplerate_dialog_get_type(void);

#endif
