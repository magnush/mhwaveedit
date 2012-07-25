/*
 * Copyright (C) 2002 2003 2005 2012, Magnus Hjorth
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


#ifndef FORMATSELECTOR_H_INCLUDED
#define FORMATSELECTOR_H_INCLUDED

#include <gtk/gtk.h>
#include "combo.h"
#include "dataformat.h"
#include "int_box.h"

#define FORMAT_SELECTOR(obj) GTK_CHECK_CAST(obj,format_selector_get_type(),FormatSelector)
#define FORMAT_SELECTOR_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,format_selector_get_type(),FormatSelectorClass)
#define IS_FORMAT_SELECTOR(obj) GTK_CHECK_TYPE(obj,format_selector_get_type())

typedef struct {

     GtkTable table;

     Combo *samplesize_combo,*sign_combo,*endian_combo,*packing_combo;
     Combo *channel_combo;
     Intbox *rate_box;

} FormatSelector;

typedef struct {
     GtkTableClass table_class;
} FormatSelectorClass;

GtkType format_selector_get_type(void);
GtkWidget *format_selector_new(gboolean show_full);
void format_selector_set(FormatSelector *fs, Dataformat *format);
void format_selector_get(FormatSelector *fs, Dataformat *result);
void format_selector_set_from_inifile(FormatSelector *fs, gchar *ini_prefix);
void format_selector_save_to_inifile(FormatSelector *fs, gchar *ini_prefix);
gboolean format_selector_check(FormatSelector *fs);

#endif
