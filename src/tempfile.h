/*
 * Copyright (C) 2002 2003 2004 2005, Magnus Hjorth
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


#ifndef TEMPFILE_H_INCLUDED
#define TEMPFILE_H_INCLUDED

#include <gtk/gtk.h>
#include "chunk.h"
#include "datasource.h"


/* This type identifies the temporary file during creation */
typedef gpointer TempFile;

/* Start creating a temporary file. Returns a handle to the temporary file or
 * NULL on failure. The handle must later be destroyed using either 
 * tempfile_finished or tempfile_abort.
 *
 * The realtime argument should be used when recording or performing
 * other timing-sensitive tasks. Files in each temporary directory will
 * then be opened right away to avoid delays later. Also, memory
 * buffering is disabled and data goes directly to file.
 */

TempFile tempfile_init(Dataformat *format, gboolean realtime);



/* Write data into the temporary file. 
 *
 * temp - Handle to the temporary file.
 * data - The data to output
 * length - Length of the data
 *
 * return value - TRUE on failure, FALSE on success
 */

gboolean tempfile_write(TempFile temp, gpointer data, guint length);



/* Abort creating the temporary file. 
 *   temp - Handle to the temporary file.
 */

void tempfile_abort(TempFile temp);



/* End the temporary file.
 *   temp - Handle to the temporary file.
 *   format - The PCM format the written data was in. 
 *   fake_pcm - If the written data was really raw sample_t values
 *   return value - A Chunk containing the temporary data or NULL on
 *                  failure. 
 */

Chunk *tempfile_finished(TempFile temp);

void set_temp_directories(GList *dirs);

gchar *get_temp_directory(guint num);

gchar *get_temp_filename(guint dirnum);
gchar *get_temp_filename_d(gchar *dir);


/* Creates a wav file header for the specified sample format and size and 
 * puts it in the supplied buffer. 
 * Returns size of the header, always 44 or 58 bytes. */

guint get_wav_header(Dataformat *format, off_t datasize, void *buffer);

gboolean write_wav_header(EFILE *f, Dataformat *format, off_t datasize);

#endif
