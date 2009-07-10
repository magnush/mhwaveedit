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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


#ifndef HISTORYBOX_H_INCLUDED
#define HISTORYBOX_H_INCLUDED

#include <gtk/gtk.h>

#define HISTORY_BOX(obj) GTK_CHECK_CAST(obj,history_box_get_type(),HistoryBox)
#define HISTORY_BOX_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,history_box_get_type(),HistoryBoxClass)
#define IS_HISTORY_BOX(obj) GTK_CHECK_TYPE(obj,history_box_get_type())

typedef struct {
     GtkCombo combo;
     gpointer history;
} HistoryBox;

typedef struct {
     GtkComboClass comboclass;
} HistoryBoxClass;

GtkType history_box_get_type(void);
GtkWidget *history_box_new(gchar *historyname, gchar *value);
gchar *history_box_get_value(HistoryBox *box);
void history_box_set_history(HistoryBox *box, gchar *historyname);
void history_box_set_value(HistoryBox *box, gchar *value);
void history_box_rotate_history(HistoryBox *box);

#endif
