/*
 * Copyright (C) 2002 2003 2004 2005 2006, Magnus Hjorth
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


#ifndef UM_H_INCLUDED
#define UM_H_INCLUDED

#include <gtk/gtk.h>

#define UM_OK 0
#define UM_YESNOCANCEL 1
#define UM_OKCANCEL 2

#define MR_OK 0
#define MR_YES 1
#define MR_NO 2
#define MR_CANCEL 3

extern gboolean um_use_gtk;

void console_message(const char *msg);
void console_perror(const char *msg);
void user_perror(const char *msg);

int user_message(char *msg, int type);
void user_info(char *msg);
void user_error(char *msg);
/* Same as user_error but returns immediately */
void popup_error(char *msg);
void user_warning(char *msg);

extern int user_message_flag;

gchar *user_input(gchar *label, gchar *title, gchar *defvalue, 
		  gboolean (*validator)(gchar *c), GtkWindow *below);
gboolean user_input_float(gchar *label, gchar *title, gfloat defvalue, 
			  GtkWindow *below, gfloat *result);

gint user_choice(gchar **choices, guint def, gchar *windowtitle, 
		 gchar *windowtext, gboolean allow_cancel);

#endif
