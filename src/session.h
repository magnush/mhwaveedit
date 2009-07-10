/*
 * Copyright (C) 2006, Magnus Hjorth
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


#ifndef SESSION_H_INCLUDED
#define SESSION_H_INCLUDED

#include <gtk/gtk.h>
#include "datasource.h"
#include "chunk.h"
#include "document.h"
#include "mainwindow.h"

/* Set up session handling system. */
void session_init(int *argc, char **argv);

/* If there are old sessions, a dialog will pop up and the user can choose
 * a session to restore/join. Returns TRUE if session was resumed. */
gboolean session_dialog(void);

gint session_get_id(void);

/* Called periodically from main loop */
void session_work(void);

/* Dumps session data to disk. Program should exit afterwards. */
gboolean session_suspend(void);

/* Remove all data for this session */
void session_quit(void);

/* Logging functions */
gboolean session_log_datasource(Datasource *ds);
gboolean session_log_chunk(Chunk *c);
gboolean session_log_document(Document *d);
gboolean session_log_mainwindow(Mainwindow *w);
gboolean session_log_clipboard(Chunk *clipboard);

#endif
