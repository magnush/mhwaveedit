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


#ifndef PIPEDIALOG_H_INCLUDED
#define PIPEDIALOG_H_INCLUDED

#include <gtk/gtk.h>
#include "effectdialog.h"
#include "historybox.h"


#define PIPE_DIALOG(obj) GTK_CHECK_CAST(obj,pipe_dialog_get_type(),PipeDialog)
#define PIPE_DIALOG_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,pipe_dialog_get_type(),PipeDialogClass)
#define IS_PIPE_DIALOG(obj) GTK_CHECK_TYPE(obj,pipe_dialog_get_type())

typedef struct {
     
     EffectDialog ed;
     HistoryBox *cmd;
     GtkToggleButton *sendwav;

} PipeDialog;


typedef struct {
     EffectDialogClass edc;
} PipeDialogClass;

GtkType pipe_dialog_get_type(void);

gpointer pipe_dialog_open_pipe(gchar *command, int *fds, gboolean open_out);
gboolean pipe_dialog_error_check(gpointer handle);
void pipe_dialog_close(gpointer handle);
void pipe_dialog_close_input(gpointer handle);

Chunk *pipe_dialog_pipe_chunk(Chunk *chunk, gchar *command, gboolean sendwav, 
			      int dither_mode, StatusBar *bar, 
			      off_t *clipcount);
gboolean pipe_dialog_send_chunk(Chunk *chunk, gchar *command, gboolean sendwav,
				int dither_mode, StatusBar *bar, 
				off_t *clipcount);

#endif
