/*
 * Copyright (C) 2002 2003 2008 2010 2012, Magnus Hjorth
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


/* This module handles the runtime configuration of the program. */

#ifndef INIFILE_H_INCLUDED
#define INIFILE_H_INCLUDED

#include <gtk/gtk.h>

/* Some keys and default values that are used in different places of the 
 * program */

#define INI_SETTING_SOUNDDRIVER "soundDriver"
#define INI_SETTING_REALMAX "diskEditingThreshold"
#define INI_SETTING_REALMAX_DEFAULT (1024*128)
#define INI_SETTING_SOUNDBUFSIZE "soundBufferSize"
#define INI_SETTING_SOUNDBUFSIZE_DEFAULT 65536
#define INI_SETTING_TIMESCALE "showTimeScale"
#define INI_SETTING_TIMESCALE_DEFAULT TRUE
#define INI_SETTING_VZOOM "showVerticalZoom"
#define INI_SETTING_VZOOM_DEFAULT TRUE
#define INI_SETTING_HZOOM "showHorizontalZoom"
#define INI_SETTING_HZOOM_DEFAULT TRUE
#define INI_SETTING_SPEED "showSpeed"
#define INI_SETTING_SPEED_DEFAULT inifile_get_gboolean("varispeed",TRUE)
#define INI_SETTING_SLABELS "showSliderLabels"
#define INI_SETTING_SLABELS_DEFAULT FALSE
#define INI_SETTING_BUFPOS "showBufpos"
#define INI_SETTING_BUFPOS_DEFAULT FALSE
#define INI_SETTING_MIXER "mixerUtility"
#define INI_SETTING_MIXER_DEFAULT DEFAULT_MIXERAPP
#define INI_SETTING_VIEW_QUALITY "viewQuality"
#define INI_SETTING_VIEW_QUALITY_DEFAULT 128
#define INI_SETTING_VIEW_QUALITY_HIGH "viewQualityHigh"
#define INI_SETTING_VIEW_QUALITY_HIGH_DEFAULT 5000
#define INI_SETTING_TIME_DISPLAY "timeDisplay"
#define INI_SETTING_TIME_DISPLAY_SCALE "timeDisplayScale"

/* This function must be called before any other of these functions. It will
 * initialize the inifile system and load the settings file ~/.mhwaveedit/config
 * if available. 
 */

void inifile_init(void);


/* Reads a setting.
 * setting - Name of the setting.
 * defaultValue - Default value returned if no such setting exists. 
 * Returns: A read-only string of the setting's value or defaultValue if the
 *          setting doesn't exist.
 */

gchar *inifile_get(gchar *setting, gchar *defaultValue);


/* Same as inifile_get excepts requires that the value is a guint32 value.
 * Returns: The value converted into a guint32 or defaultValue if setting 
 * doesn't exist or is not a valid number.
 */

guint32 inifile_get_guint32(gchar *setting, guint32 defaultValue);
gint32 inifile_get_gint32(gchar *setting, gint32 defaultValue);

/* Same as inifile_get except requires that the value is a boolean value.
 * 'y','yes','1','true','enabled','on' are interpreted as TRUE.
 * 'n','no','0','false','disabled','off' are interpreted as FALSE.
 * Returns: The value converted into a boolean or defaultValue if setting 
 * doesn't exist or is not a valid boolean.
 */

gboolean inifile_get_gboolean(gchar *setting, gboolean defaultValue);

/* Same as inifile_get except requires that the value is a floating-point
 * number. 
 */

gfloat inifile_get_gfloat(gchar *setting, gfloat defaultValue);

/* Changes a setting.
 * setting - Name of the setting.
 * value - The new value.
 * Returns: TRUE if the setting was modified, FALSE if it already had that
 *          value.
 */

gboolean inifile_set(gchar *setting, gchar *value);
gboolean inifile_set_guint32(gchar *setting, guint32 value);
gboolean inifile_set_gint32(gchar *setting, gint32 value);
gboolean inifile_set_gboolean(gchar *setting, gboolean value);
gboolean inifile_set_gfloat(gchar *setting, gfloat value);


/* Save the settings into the config file. */
void inifile_save(void);

/* Free all resources and save the settings if they were changed. */
void inifile_quit(void);

#endif
