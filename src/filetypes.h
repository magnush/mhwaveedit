/*
 * Copyright (C) 2002 2003 2004 2005 2006 2012, Magnus Hjorth
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


/* This module contains functions for loading and saving files. Most of it is
 * taken from the old chunk.c code.
 */

#ifndef FILETYPES_H_INCLUDED
#define FILETYPES_H_INCLUDED

#include "chunk.h"
#include "statusbar.h"


/* Returns the number of file formats supported. All numbers below this count 
 * can be used as the fileformat parameter for the other functions. */

guint fileformat_count(void);


/* Returns the name or default extension for a certain file format. */
gchar *fileformat_name(guint fileformat);
gchar *fileformat_extension(guint fileformat);
gboolean fileformat_has_options(guint fileformat);

/* Indicate if libsndfile has ogg support */
gboolean sndfile_ogg_supported();

/* Loads a document from a specified file. */

Chunk *chunk_load(gchar *filename, int dither_mode, StatusBar *bar);

Chunk *chunk_load_x(gchar *filename, int dither_mode, StatusBar *bar, 
		    gboolean *lossy);

/* Saves the contents of a chunk into a new file. The file format is determined
 * from the extension or asked at run time. 
 */

gboolean chunk_save(Chunk *chunk, gchar *filename, int filetype, 
		    gboolean user_default_settings, int dither_mode, 
		    StatusBar *bar);

#endif
