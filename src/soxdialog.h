/*
 * Copyright (C) 2003 2004 2006, Magnus Hjorth
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


#ifndef SOXDIALOG_H_INCLUDED
#define SOXDIALOG_H_INCLUDED

#include "effectdialog.h"
#include "float_box.h"
#include "int_box.h"
#include "combo.h"

#define SOX_DIALOG(obj) GTK_CHECK_CAST(obj,sox_dialog_get_type(),SoxDialog)
#define SOX_DIALOG_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,sox_dialog_get_type(),SoxDialogClass)
#define IS_SOX_DIALOG(obj) GTK_CHECK_TYPE(obj,sox_dialog_get_type())

typedef struct {
     EffectDialog ed;
     
     gint i1;
     Floatbox *fb1,*fb2,*fb3,*fb4,*fb5;
     Floatbox **fba[4];
     Combo **ca,*c1,*c2;
     Intbox *ib1,*ib2,*ib3;
     GtkToggleButton *tb1;
} SoxDialog;

typedef struct {
     EffectDialogClass ed_class;
} SoxDialogClass;

GtkType sox_dialog_get_type(void);
void sox_dialog_register(void);
gchar *sox_dialog_first_effect(void);
void sox_dialog_format_string(gchar *buf, guint bufsize, Dataformat *fmt);

#endif
