/*
 * Copyright (C) 2002 2003 2004 2005 2008 2009 2011 2012, Magnus Hjorth
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

#ifndef CONFIGDIALOG_H_INCLUDED
#define CONFIGDIALOG_H_INCLUDED

#include <gtk/gtk.h>
#include "int_box.h"
#include "combo.h"
#include "formatselector.h"

#define CONFIG_DIALOG(obj) GTK_CHECK_CAST(obj,config_dialog_get_type(),ConfigDialog)
#define CONFIG_DIALOG_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,config_dialog_get_type(),ConfigDialogClass)
#define IS_CONFIG_DIALOG(obj) GTK_CHECK_TYPE(obj,config_dialog_get_type())

typedef struct {
     GtkWindow window;
     Intbox *disk_threshold;
     Combo *sound_driver;
     GtkButton *sound_driver_prefs;
     Intbox *sound_buffer_size;
     GtkEntry *mixer_utility;
     GtkToggleButton *time_scale_default,*sound_lock,*roll_cursor,*improve;
     GtkToggleButton *vzoom_default, *hzoom_default,*speed_default,*labels_default;
     GtkToggleButton *bufpos_default;
     GtkToggleButton *output_bswap,*center_cursor,*mark_autoplay;
     GtkToggleButton *mainwin_front,*varispeed_enable,*varispeed_autoreset;
     GtkToggleButton *varispeed_fast,*dither_editing,*dither_playback;
     GtkToggleButton *driver_autodetect,*output_stereo;
     Intbox *recent_files;
     Intbox *vzoom_max;
     Intbox *view_quality;
     Combo *time_display,*time_display_timescale,*varispeed_method,
	  *speed_method;
     GtkToggleButton *remember_geometry;
     GtkList *tempdirs;
     GtkWidget *selected_tempdir;
     GtkButton *tempdir_remove,*tempdir_up,*tempdir_down,*tempdir_add;
     GtkButton *tempdir_browse;
     GtkEntry *tempdir_add_entry;
     FormatSelector *fallback_format;
     GtkToggleButton *floating_tempfiles;
     Combo *oggmode;
     Combo *convmode;
} ConfigDialog;

typedef struct {
     GtkWindowClass wc;
} ConfigDialogClass;

GtkType config_dialog_get_type(void);
GtkWidget *config_dialog_new(void);

#endif
