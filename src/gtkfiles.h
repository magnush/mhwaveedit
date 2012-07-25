/*
 * Copyright (C) 2002 2003 2004 2005 2006 2011, Magnus Hjorth
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

/* GTK wrappers around the standard I/O functions, and some other file-related 
 * stuff. */

#ifndef GTKFILES_H_INCLUDED
#define GTKFILES_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <gtk/gtk.h>

typedef struct {
     int fd;
     char *filename;
} EFILE;

#define EFILE_READ 0
#define EFILE_WRITE 1
#define EFILE_APPEND 2

gboolean xunlink(char *filename);

/* Renames oldname into newname. If oldname and newname are on
 * different file systems, and allow_copy is TRUE, the file will be copied.
 *
 * Return value: 
 *  0. Success.
 *  1. Failure. (an error message has already been displayed)
 *  2. allow_copy is FALSE and the filenames were not on the same filesystem.
 */

gint xrename(char *oldname, char *newname, gboolean allow_copy);

/* This flag is TRUE by default. If it is FALSE, e_fopen with mode!=EFILE_READ
 * and e_fwrite will not popup error dialog boxes when an error occurs. If an 
 * error other than disk full occurs, a warning message is issued to stderr. */

extern gboolean report_write_errors;

int xopen(char *filename, int flags, int mode);

/* Constants for openmode are defined above (EFILE_READ/WRITE/APPEND) */
EFILE *e_fopen(char *filename, int openmode);

EFILE *e_fopen_fd(int fd, char *virtual_filename);
gboolean e_fclose(EFILE *stream);
gboolean e_fclose_remove(EFILE *stream);
void close_all_files(void); /* Used after forking */
void close_all_files_except(int *fds, int count);
gboolean e_fseek(EFILE *stream, off_t offset, int whence);
gboolean e_fread(void *data, size_t size, EFILE *stream);
gboolean e_fwrite(void *data, size_t size, EFILE *stream);
gboolean e_fwrite0(size_t size, EFILE *stream);
off_t e_ftell(EFILE *stream);
long int e_readline(gchar **line, size_t *size, EFILE *stream);
gboolean e_copydata(EFILE *from, EFILE *to, off_t bytes);
gboolean errdlg_copyfile(gchar *from, gchar *to);

gboolean e_fread_bswap(void *data, size_t size, EFILE *stream);
gboolean e_fwrite_bswap(void *data, size_t size, EFILE *stream);

gboolean is_same_file(char *filename1, char *filename2);
off_t errdlg_filesize(gchar *filename);

gboolean file_exists(char *filename);
gboolean file_is_normal(char *filename);
gboolean file_is_directory(char *filename);

gboolean program_exists(char *progname);

/* Meant to be called from signal handlers when inside get_filename */
void get_filename_modify_extension(gchar *new_extension);
gchar *get_filename(gchar *current_name, gchar *filemask, gchar *title_text,
		    gboolean savemode, GtkWidget *custom_widget);
gchar *get_directory(gchar *current_name, gchar *title_text);

gchar *make_filename_rooted(gchar *name);

gboolean fd_canread(int fd);
gboolean fd_canwrite(int fd);

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
#define e_fwrite_le(a,b,c) e_fwrite_bswap(a,b,c)
#define e_fread_le(a,b,c) e_fread_bswap(a,b,c)
#define e_fwrite_be(a,b,c) e_fwrite(a,b,c)
#define e_fread_be(a,b,c) e_fread(a,b,c)
#else
#define e_fwrite_le(a,b,c) e_fwrite(a,b,c)
#define e_fread_le(a,b,c) e_fread(a,b,c)
#define e_fwrite_be(a,b,c) e_fwrite_bswap(a,b,c)
#define e_fread_be(a,b,c) e_fread_bswap(a,b,c)
#endif

#define e_fwrite_xe(a,b,c,be) ((be)?e_fwrite_be(a,b,c):e_fwrite_le(a,b,c))
#define e_fread_xe(a,b,c,be) ((be)?e_fread_be(a,b,c):e_fread_le(a,b,c))

gboolean e_fread_u16_xe(guint16 *data, EFILE *stream, gboolean be);
gboolean e_fread_u32_xe(guint32 *data, EFILE *stream, gboolean be);

#endif
