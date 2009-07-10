/*
 * Copyright (C) 2002 2003 2004 2006, Magnus Hjorth
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


#ifndef MAPCHANNELSDIALOG_H_INCLUDED
#define MAPCHANNELSDIALOG_H_INCLUDED

#include <gtk/gtk.h>
#include "effectdialog.h"
#include "int_box.h"

#define MAP_CHANNELS_DIALOG(obj) GTK_CHECK_CAST(obj,map_channels_dialog_get_type(),MapChannelsDialog)
#define MAP_CHANNELS_DIALOG_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,map_channels_dialog_get_type(),MapChannelsDialogClass)
#define IS_MAP_CHANNELS_DIALOG(obj) GTK_CHECK_TYPE(obj,map_channels_dialog_get_type())

typedef struct {
     EffectDialog ed;
     guint channels_in,channels_out;
     GtkToggleButton *channel_map[8*8];
     Intbox *outchannels;
     GtkWidget *outnames[8];
} MapChannelsDialog;

typedef struct {
    EffectDialogClass edc;
} MapChannelsDialogClass;

GtkType map_channels_dialog_get_type();

#endif
