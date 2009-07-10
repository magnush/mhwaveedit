/*
 * Copyright (C) 2004 2005, Magnus Hjorth
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


#ifndef LADSPADIALOG_H_INCLUDED
#define LADSPADIALOG_H_INCLUDED

#ifdef HAVE_LADSPA

#include "effectdialog.h"
#include "ladspacore.h"

#define LADSPA_DIALOG(obj) GTK_CHECK_CAST(obj,ladspa_dialog_get_type(),LadspaDialog)
#define LADSPA_DIALOG_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,ladspa_dialog_get_type(),LadspaDialogClass)
#define IS_LADSPA_DIALOG(obj) GTK_CHECK_TYPE(obj,ladspa_dialog_get_type())

typedef struct {
     EffectDialog ed;
     LadspaEffect *effect;
     guint channels;
     GtkWidget **settings[4];
     GtkToggleButton *keep;
} LadspaDialog;

typedef struct {
     EffectDialogClass edc;
} LadspaDialogClass;

GtkType ladspa_dialog_get_type(void);

#endif /* HAVE_LADSPA */

void ladspa_dialog_register(void);
gchar *ladspa_dialog_first_effect(void);

#endif /* LADSPADIALOG_H_INCLUDED */
