/*
 * Copyright (C) 2004, Magnus Hjorth
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


/* This is the status bar that is at the bottom of each
 * Mainwindow. During normal editing, it displays cursor position, view
 * start and endpoint and selection start/end. It can be switch into
 * progress mode, then it displays a progress bar with a string over
 * it. It's built as a GtkFixed with labels inside it. 
 */

#ifndef STATUSBAR_H_INCLUDED
#define STATUSBAR_H_INCLUDED

#include <gtk/gtk.h>
#include "main.h"

#define STATUSBAR(obj) GTK_CHECK_CAST(obj,status_bar_get_type(),StatusBar)
#define STATUSBAR_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,status_bar_get_type(),StatusBarClass)
#define IS_STATUSBAR(obj) GTK_CHECK_TYPE(obj,status_bar_get_type())

typedef struct {
     GtkFixed fixed;
     GtkWidget *da;

     /* This flag decides whether we're in regular mode (0),  progress
      * mode (1), or newly started (2). */
     gint mode;

     /* Used in newly started mode: */
     GtkLabel *fresh_label;

     /* These are used in progress mode: */
     GtkLabel *progress_label; /* Also used when newly started. */
     off_t progress_cur,progress_max; 
     guint bar_width; /* Progress bar width in pixels. */
     gboolean progress_break;

     /* These are used in regular mode: */
     GtkLabel *cursor,*view,*sel;
     off_t cur,vs,ve,ss,se,rate,max;
     gboolean rolling;
} StatusBar;

typedef struct {
     GtkFixedClass fixed_class;
     
     void (*progress_begin)(StatusBar *bar);
     void (*progress_end)(StatusBar *bar);

} StatusBarClass;

/* Global variable that decides whether the cursor position text
 * should update while playing. */
extern gboolean status_bar_roll_cursor;

GtkType status_bar_get_type(void);
GtkWidget *status_bar_new(void);

/* Reset the status bar to the original condition ("no file loaded") */
void status_bar_reset(StatusBar *sb);

/* Set the information shown during regular editing. This will switch
   the status bar into regular mode. */
void status_bar_set_info(StatusBar *sb, off_t cursorpos, gboolean is_rolling,
			 off_t viewstart, off_t viewend, off_t selstart, 
			 off_t selend, off_t samplerate, off_t maxvalue);

/* Go into progress mode. If called when already in progress mode,
   description may be NULL. In that case, the old description is kept. */
void status_bar_begin_progress(StatusBar *sb, off_t progress_length,
			       gchar *description);

/* When in progress mode, switches the status bar back into regular mode.
 * Otherwise, does nothing. */
void status_bar_end_progress(StatusBar *sb);

/* Update progress indicator. The status bar must be in progress mode
 * when this is called. Returns TRUE if someone called 
 * status_bar_break_progress */
gboolean status_bar_progress(StatusBar *sb, off_t progress);

/* Causes future calls to status_bar_progress to return TRUE */
void status_bar_break_progress(StatusBar *sb);

/* Returns the number of status bars in progress mode */
int status_bar_progress_count(void);

#endif
