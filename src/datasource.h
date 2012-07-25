/*
 * Copyright (C) 2002 2003 2004 2005 2006 2009, Magnus Hjorth
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

/* Datasource object represent a source of PCM data. */

/* This is (and should be) mainly used by chunk.c */

#ifndef DATASOURCE_H_INCLUDED
#define DATASOURCE_H_INCLUDED

#include <gtk/gtk.h>
#include "gtkfiles.h"
#include "statusbar.h"
#include "dataformat.h"

#define DATASOURCE(obj) GTK_CHECK_CAST(obj,datasource_get_type(),Datasource)
#define DATASOURCE_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,datasource_get_type(),DatasourceClass)
#define IS_DATASOURCE(obj) GTK_CHECK_TYPE(obj,datasource_get_type())

#ifndef HAVE_LIBSNDFILE
#define SNDFILE void
#else
#include <sndfile.h>
#endif



/* Values for the type member of the Datasource struct. Determines how to 
 * actually get the data. */

/* Data stored directly in memory */
#define DATASOURCE_REAL              0 
/* The data is stored in a (read-only) file. */ 
#define DATASOURCE_VIRTUAL           1 
/* Data stored in a file that will be removed when the Datasource is
 * destroyed. */
#define DATASOURCE_TEMPFILE          2 
/* Silence (duh) */
#define DATASOURCE_SILENCE           3 
/* The data is read using libSndfile */
#define DATASOURCE_SNDFILE           4 
/* Same as DATASOURCE_SNDFILE but will be removed when the chunk is 
 * destroyed. */ 
#define DATASOURCE_SNDFILE_TEMPORARY 5 
/* Reference to another Datasource. Has the same format. 
 * All requests will simply be passed on to the referred datasource. */
#define DATASOURCE_REF               6 
/* Clone of another PCM Datasource. The Clone can have different
 * format than the original. This is used for mending files with
 * broken headers..*/
#define DATASOURCE_CLONE             7 
/* Byteswapped clone of PCM datsource.*/
#define DATASOURCE_BYTESWAP          8 
/* Autoconverting datasource. This Datasource must have the
 * same number of channels as referred Datasource.
 * Floating point requests will be passed on to the referred
 * Datasource, PCM requests will be sent as FP requests and then 
 * automatically converted from the FP data. */
#define DATASOURCE_CONVERT           9
/* Clone of other datasource with channels rearranged. 
 * Has the sample sample format and rate, but can have different number of 
 * channels */
#define DATASOURCE_CHANMAP          10

struct temparea {
     gpointer ptr;
     int size;
};

struct _Datasource;

struct _Datasource {

     GtkObject object;

     gint type;

     Dataformat format;

     off_t length;             /* Number of samples */
     off_t bytes;               /* Number of bytes in file. */

     guint opencount;            /* To keep track of nested open/close calls */

     /* Temporary areas for internal use */
     struct temparea read_temparea,readfp_temparea;

     gint tag;                   /* Extra field... */

     /* Type-specific data */
     union {

	  /* --- DATASOURCE_TEMPFILE and DATASOURCE_VIRTUAL --- */
	  struct {
	       char *filename; 
	       off_t offset;  /* Offset of data in file (bytes) */
	       EFILE *handle;
	       off_t pos;     /* Current position in file (bytes) */
	  } virtual;

	  /* --- DATASOURCE_REAL ---*/
	  char *real;  /* Pointer into the memory buffer with the sound data. */
	  

	  /* --- DATASOURCE_SNDFILE and DATASOURCE_SNDFILE_TEMPORARY --- */
	  struct {
	       char *filename;
	       gboolean raw_readable;
	       SNDFILE *handle;
	       off_t pos; /* Current position in file (samples) */
	  } sndfile;

	  /* DATASOURCE_CLONE, DATASOURCE_REF, DATASOURCE_CONVERT and 
	   * DATASOURCE_BYTESWAP */
	  struct _Datasource *clone;

	  /* DATASOURCE_CHANMAP */
	  struct {
	       struct _Datasource *clone;
	       int *map;
	  } chanmap;

     } data;
};

typedef struct _Datasource Datasource;

typedef struct {
     GtkObjectClass klass;
} DatasourceClass;


GtkType datasource_get_type(void);

/* ------------ 
 * CONSTRUCTORS 
 * ------------ */


/* Create data source from another Datasource object. The format of the data 
 * will be that of the format argument, regardless of what the other source
 * specifies. This makes it possible to override the format when you know it is
 * wrong. 
 * source must be a PCM datasource
 */
Datasource *datasource_clone_df(Datasource *source, Dataformat *format);


/* Returns a byte-swapped clone of another Datasource. 
 * source must be a PCM datasource */
Datasource *datasource_byteswap(Datasource *source);


/* Returns a Datasource containing the same data as source, but converted to 
 * new_format. If source is floating-point, the original data will still
 * be returned when the read_array_fp function is used. source and new must
 * have the same number of channels and samplerate.  */
Datasource *datasource_convert(Datasource *source, Dataformat *new_format);

/* Returns a Datasource containing the same data as source, but with its 
 * channels remapped.
 * n_channels is the number of channels in the new source. 
 * map should point to an array length n_channels mapping to source channels */
Datasource *datasource_channel_map(Datasource *source, int n_channels,
				   int *map);

/* Create a data source from a memory buffer of data. The buffer will be 
 * g_free:d when the data source is destroyed and the data should not be 
 * modified by the caller any more after this call.
 */
Datasource *datasource_new_from_data(void *data, Dataformat *format, 
				     guint32 size);


/* Create a new datasource containing silence with a certain format and the 
 * specified number of samples  
 */
Datasource *datasource_new_silent(Dataformat *format, off_t samples);



/* ---------------------
 * DATA ACCESS FUNCTIONS
 * --------------------- */



/* Open the data source. Must be called before any of the other data access 
 * functions. May be called more than one time for the same Datasource.
 */

gboolean datasource_open(Datasource *source);



/* Close the data source. Must be called once for each call to 
 * datasource_open. 
 */

void datasource_close(Datasource *source);



/* Read one/many PCM/float sample from the Data source into the buffer. The
 * sample data will always be in host endian order.  
 */
gboolean datasource_read(Datasource *source, off_t sampleno, gpointer buffer,
			 int dither_mode);
gboolean datasource_read_fp(Datasource *source, off_t sampleno, 
			    sample_t *buffer, int dither_mode);
/* Returns number of bytes read (0 on error). If clipping != NULL and clipping
 * conversion was performed, sets *clipping to TRUE.
 */
guint datasource_read_array(Datasource *source, off_t sampleno, 
			    guint size, gpointer buffer, int dither_mode,
			    off_t *clipcount);
/* Returns number of (multi-channel) samples read (0 on error). */
guint datasource_read_array_fp(Datasource *source, off_t sampleno,
			       guint samples, sample_t *buffer, 
			       int dither_mode, off_t *clipcount);






/* -------
 * VARIOUS 
 * ------- */


/* Returns the number of active Datasource objects */
guint datasource_count(void);



/* This dumps the contents of the datasource into a file. You don't need
 * to open the datasource when using this function. The StatusBar must
 * already be in progress mode. 
 */

gboolean datasource_dump(Datasource *ds, off_t position, off_t length,
			 EFILE *file, int dither_mode, StatusBar *bar,
			 off_t *clipcount);


/* This reads all the data of the datasource into a memory buffer and converts
 * it into a memory datasource.
 */

gboolean datasource_realize(Datasource *ds, int dither_mode);

/* This function will unlink a file. If any of the Datasources depend on this
 * file, the file will be moved or copied into a temporary file before the
 * original name is unlinked.
 */

gboolean datasource_backup_unlink(gchar *filename);

/* Returns zero if reading the datasource does not cause clipping conversion.
 * and the datasource contains normalized data.
 * Returns non-zero if reading the datasource causes clipping conversion
 * Returns 1 if the error can be solved by normalizing the Datasource
 * Returns 2 if the error can not be solved in that way (very special cases)
 * Returns <0 if an error or break occurred while checking
 * The StatusBar must already be in progress mode.
 */
gint datasource_clip_check(Datasource *ds, StatusBar *bar);

#endif
