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


#ifndef SAMPLESIZE_DIALOG_H_INCLUDED
#define SAMPLESIZE_DIALOG_H_INCLUDED

#include <gtk/gtk.h>
#include "effectdialog.h"
#include "formatselector.h"

#define SAMPLESIZE_DIALOG(obj) GTK_CHECK_CAST(obj,samplesize_dialog_get_type(),SamplesizeDialog)
#define SAMPLESIZE_DIALOG_CLASS(klass) GTK_CHECK_CLASS_CAST(obj,samplesize_dialog_get_type(),SamplesizeDialogClass)
#define IS_SAMPLESIZE_DIALOG(obj) GTK_CHECK_TYPE(obj,samplesize_dialog_get_type())

typedef struct _SamplesizeDialog {

     EffectDialog ed;

     gboolean gurumode;

     FormatSelector *fs;

} SamplesizeDialog;

typedef struct _SamplesizeDialogClass {
     EffectDialogClass ed_class;
} SamplesizeDialogClass;

GtkType samplesize_dialog_get_type(void);

#endif
