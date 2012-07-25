/*
 * Copyright (C) 2002 2003 2004 2005 2006 2007 2008 2009 2011, Magnus Hjorth
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



/* Chunk objects represent a segment of (PCM) audio data. The Chunks gets its
 * data from one or many Datasource objects. 
 *
 * The Datasources that constitute the Chunk's data must all have the same
 * format as the Chunk. 
 *
 * The Chunks are never modified after they are created, instead all functions 
 * that modify a chunk return a new chunk with the changes, while the original
 * chunk and its data remains intact. 
 */



#ifndef CHUNK_H_INCLUDED
#define CHUNK_H_INCLUDED

#include <stdio.h>
#include <gtk/gtk.h>

#ifdef HAVE_LIBSNDFILE
#include <sndfile.h>
#endif

#include "datasource.h"
#include "statusbar.h"
#include "gtkfiles.h"

#define CHUNK(obj) GTK_CHECK_CAST(obj,chunk_get_type(),Chunk)
#define CHUNK_CLASS(class) GTK_CHECK_CLASS_CAST(class,chunk_get_type(),ChunkClass)
#define IS_CHUNK(obj) GTK_CHECK_TYPE(obj,chunk_get_type())


typedef struct _Chunk {
     GtkObject object;

     Dataformat format;

     GList *parts;               /* All items should be of type DataPart */

     /* These must match the sum of the parts */
     off_t length;             /* Number of samples */
     off_t size;               /* Number of bytes (should always be 
				* length*format.samplesize*format.channels) */

     guint opencount;            /* For detecting missing chunk_close calls */

} Chunk;

typedef struct _ChunkClass {
	GtkObjectClass parent;
} ChunkClass;


typedef struct {
     Datasource *ds;
     off_t position;
     off_t length;
} DataPart;


/* Data structure used for chunk_open,chunk_read etc. */
typedef Chunk ChunkHandle; 





typedef gpointer WriteoutID;

/* Function for outputting processed data 
 *
 * data - The data to output
 * length - Length of the data (must be multiples of sample size)
 *
 * return value - TRUE on failure, FALSE on success
 */

typedef gboolean (*chunk_writeout_func)(WriteoutID id, gpointer data, 
					guint length);


/* Type for use as callback function for chunk_filter.
 *
 * NOTE: These callback functions should not call any other chunk functions 
 * except the chunk_sample_get/put-functions...
 *
 *  in - Buffer of input data
 *  sample_size - The size of the in buffer (0 means EOF) 
 *  out - Function to call for outputting data. If a out function call fails,
 *    this function should fail as well. 
 *  id - Parameter to send with every call to out.
 *  format - Format of input and output data.
 *
 *  return value - TRUE on failure, FALSE on success
 */

typedef gboolean (*chunk_filter_proc)(void *in, guint sample_size, 
				      chunk_writeout_func out_func, 
				      WriteoutID id, Dataformat *format);


typedef gboolean (*chunk_filter_tofmt_proc)(void *in, guint sample_size,
					    chunk_writeout_func out_func,
					    WriteoutID id, 
					    Dataformat *informat,
					    Dataformat *outformat);


/* Type for use as callback function for chunk_parse.

    sample - Buffer of input data
    sample_size - The size of the in buffer (0 means EOF) 
    chunk - The chunk being processed.

    return value - Return FALSE to continue parsing, otherwise parsing stops.
 */

typedef gboolean (*chunk_parse_proc)(void *sample,gint sample_size,
				     Chunk *chunk);





/* Flag that decides whether tempfiles created using chunk_filter
 * should be floating-point if the processing function outputs
 * floating-point data. If not, the data will be converted back and
 * written in the original Chunk's format.
 */

extern gboolean chunk_filter_use_floating_tempfiles;


/* ------------------------------ FUNCTIONS --------------------------------- */


/* Note: All functions that return gboolean values return FALSE on success. */

/* If any function fails, it will display an error message box before it 
 * returns.*/



/*  ------ Creation functions ------  */


GtkType chunk_get_type(void);
Chunk *chunk_new_from_datasource(Datasource *ds);

/** Returns a new chunk with the same data as this chunk but assumed to be in a
 * different sample format. 
 */
Chunk *chunk_clone_df(Chunk *chunk, Dataformat *format);




/* Creates a silent chunk
     format - The data format of the chunk.
     num_samples - The length of the chunk.
*/

#define chunk_new_empty(format,num_samples) chunk_new_from_datasource(datasource_new_silent(format,num_samples))



/* Same as chunk_new_empty but you specify length as number of seconds instead 
 * of samples.
 */

Chunk *chunk_new_silent(Dataformat *format, gfloat seconds);



/* The number of chunk objects currently in existence */

guint chunk_alive_count(void);






/*  ------ Saving functions ------ */



/* Save a chunk's data into a file.

     chunk - The chunk to save.
     file - An opened file handle to save the data into.
     bigendian - TRUE if dumped PCM data should be big-endian.
     bar - Status bar to display progress. Must already be in progress mode.
*/

gboolean chunk_dump(Chunk *chunk, EFILE *file, gboolean bigendian, 
		    int dither_mode, StatusBar *bar);




/*  ------ Chunk reading functions. ------ */


/* Open a chunk for reading. Returns a ChunkHandle that can be used with
 * chunk_read and chunk_read_array. The ChunkHandle must be released using
 * chunk_close. 
 */

ChunkHandle *chunk_open(Chunk *chunk);



/* Read a raw multi-channel sample from the chunk. 
     handle - ChunkHandle returned by chunk_open.
     sampleno - The sample to read.
     buffer - The buffer to read into. Must be at least chunk->samplebytes 
       bytes long. */

gboolean chunk_read(ChunkHandle *handle, off_t sampleno, void *buffer,
		    int dither_mode);



/* Read a floating-point multi-channel sample from the chunk
     handle - ChunkHandle returned by chunk_open.
     sampleno - The sample to read.
     buffer - The buffer to read into. Must be at least channels * sizeof(sample_t) 
       bytes long. */

gboolean chunk_read_fp(ChunkHandle *handle, off_t sampleno, sample_t *buffer,
		       int dither_mode);



/* Read an entire array of samples from the chunk.

     handle - ChunkHandle returned by chunk_open
     sampleno - The first sample to read.
     size - The number of bytes to read. Will be rounded down to whole sample.
     buffer - The buffer to read into

     return value - The number of bytes read (0 for error)
*/

guint chunk_read_array(ChunkHandle *handle, off_t sampleno, guint size, 
		       void *buffer, int dither_mode, off_t *clipcount);



/* Read an entire array of floating-point samples from the chunk.

     handle - ChunkHandle returned by chunk_open
     sampleno - The first sample to read.
     samples - The number of multi-channel samples to read
     buffer - The buffer to read into

     return value - The number of multi-channel samples read (0 for error)
*/

guint chunk_read_array_fp(ChunkHandle *handle, off_t sampleno, guint samples,
			  sample_t *buffer, int dither_mode, off_t *clipcount);


/* Frees the resources used by a ChunkHandle. All handles should be freed when
 * they're no longer in use.
 */

void chunk_close(ChunkHandle *handle);




/* ------ Chunk effects and utility functions ------- */

/* All these functions return a new (temporary) chunk on success and NULL on 
 * failure */



/* Creates a new chunk with the original chunks data converted into a different
 * sample rate possibly using interpolation. */

Chunk *chunk_convert_samplerate(Chunk *chunk, gint samplerate, 
				const gchar *rateconv_driver, 
				int dither_mode, StatusBar *bar);

Chunk *chunk_convert_speed(Chunk *chunk, gfloat speed_factor, 
			   int dither_mode, StatusBar *bar);


/* Creates a new chunk with the original chunk's data converted into a 
 * different sample format (samplerate and channels are unchanged) */
Chunk *chunk_convert_sampletype(Chunk *chunk, Dataformat *newtype);

/* Converts the chunk to any format. */
Chunk *chunk_convert(Chunk *chunk, Dataformat *new_format, 
		     int dither_mode, StatusBar *bar);

Chunk *chunk_byteswap(Chunk *chunk);

/* Creates a new chunk with the original chunk's data but with one channel 
 * removed. */

Chunk *chunk_remove_channel(Chunk *chunk, gint channel, StatusBar *bar);



/* Creates a new chunk with the original chunk's data but with an extra channel
 * inserted containing a copy of another channel's data. */

Chunk *chunk_copy_channel(Chunk *chunk, gint channel, int dither_mode, 
			  StatusBar *bar);



/* Creates a new chunk with the original chunk's data mixed into one channel. */

Chunk *chunk_onechannel(Chunk *chunk, int dither_mode, StatusBar *bar);

/* Creates a new Chunk with the original chunk's data, but with the
 * last channels removed or silent channels added so the channel count
 * becomes new_channels */
Chunk *chunk_convert_channels(Chunk *chunk, guint new_channels);




/* Creates a new Chunk with a mix of the original chunk's channels.
 * channels_out determines the number of output channels
 * map is a (chunk->channels x channels_out)-matrix; 
 *   If map[src*channels_out + dst], then channel <src> from chunk will be
 *   copied into channel <dst> of the result. Output channels with no assigned
 *   inputs will be silent, output channels with more than one assigned input
 *   will be mixed.
 */
Chunk *chunk_remap_channels(Chunk *chunk, int channels_out, gboolean *map, 
			    int dither_mode, StatusBar *bar);

/* Creates a new chunk containing a mix of two chunk's data. The chunks must 
 * have the same format but need not be of the same length. */

Chunk *chunk_mix(Chunk *c1, Chunk *c2, int dither_mode, StatusBar *bar);

/* Creates a new Chunk with a channel count of the sum of c1 and c2:s channel
 * counts, having the data from Chunk c1 in the first channels, and
 * the data from Chunk c2 in the following channels.
 *
 * The data is aligned in time, so that the sample in c1 at
 * c1_align_point coincides with the sample in c2 at
 * c2_align_point. The point in the resulting chunk where this sample
 * exists is stored in *align_point_out. 
 */

Chunk *chunk_sandwich(Chunk *c1, Chunk *c2, 
		      off_t c1_align_point, off_t c2_align_point,
		      off_t *align_point_out, 
		      int dither_mode, StatusBar *bar);


off_t chunk_zero_crossing_any_forward(
  Chunk *c, StatusBar *bar, off_t cursorpos);
off_t chunk_zero_crossing_any_reverse(
  Chunk *c, StatusBar *bar, off_t cursorpos);
off_t chunk_zero_crossing_all_forward(
  Chunk *c, StatusBar *bar, off_t cursorpos);
off_t chunk_zero_crossing_all_reverse(
  Chunk *c, StatusBar *bar, off_t cursorpos);


/* Returns the maximum absolute value of all the samples in the chunk. 
 * Returns negative value if anything failed. */

sample_t chunk_peak_level(Chunk *c, StatusBar *bar);




/* Returns a new chunk with all samples of the chunk c multiplied by a 
 * factor. */

Chunk *chunk_amplify(Chunk *c, sample_t factor, int dither_mode, 
		     StatusBar *bar);



/* Creates a new chunk with all samples of the chunk c multiplied by a factor
 * varying from start_factor to end_factor 
 */
Chunk *chunk_volume_ramp(Chunk *c, sample_t start_factor, sample_t end_factor,
			 int dither_mode, StatusBar *bar);

/* Create a new chunk consisting of one chunk's data followed by another
 * chunk's data. Always succeeds.
 */

Chunk *chunk_append(Chunk *first, Chunk *second);



/* Create a new chunk of the same length and format as the input chunk, 
 * consisting of a linear interpolation between the input chunk's two 
 * endpoints. Returns NULL on failure. 
 *
 * If falldown_mode is TRUE and the chunk is long, the endpoints will
 *be ramped down to zero and the middle part will be constant zero.
 */

Chunk *chunk_interpolate_endpoints(Chunk *chunk, gboolean falldown_mode,
				   int dither_mode, StatusBar *bar);



/* Creates a new chunk with the original chunk's data and another chunk's data 
 * inserted at a certain offset (offset must be at an even sample) */

Chunk *chunk_insert(Chunk *chunk, Chunk *part, off_t position);

/* Returns a chunk containing a part of the original chunk's data */

Chunk *chunk_get_part(Chunk *chunk, off_t start, off_t length);

/* These functions return a new chunk where a part of the original chunk's 
 * data has been removed or replaced by the data of another chunk.
 */

Chunk *chunk_remove_part(Chunk *chunk, off_t start, off_t length);
Chunk *chunk_replace_part(Chunk *chunk, off_t start, off_t length, Chunk *new);



/* ------ Processing functions ------ */


/* Pipe a chunk's contents through a callback function and create a new chunk 
 * with the output.
 *
 *   chunk - The chunk to process
 *   proc - The callback function (see declaration of chunk_filter_proc above)
 *   eof_proc - If this argument is non-NULL, when all data has been filtered
 *     through proc, this function will be called with sample_size==0 to produce
 *     the last data. This argument is usually either NULL or the same as proc.
 *   amount - How much should be sent each time. One of:
 *     CHUNK_FILTER_ONE - One sample value at a time
 *     CHUNK_FILTER_FULL - One multichannel sample at a time
 *     CHUNK_FILTER_MANY - Many multichannel samples at a time. The callback
 *           function can determine the exact amount by looking at the
 *            sample_size parameter.
 * 
 *   must_convert - If TRUE the in and out buffer will always be in
 *   sample_t format instead of the chunk's native sample format.
 *   bar - Status bar to display progress in
 *   title - Title to display in status bar.
 *   returns - The output or NULL on failure.
 */

#define CHUNK_FILTER_ONE 0
#define CHUNK_FILTER_FULL 1
#define CHUNK_FILTER_MANY 2

Chunk *chunk_filter(Chunk *chunk, chunk_filter_proc proc, 
		    chunk_filter_proc eof_proc, gint amount, 
		    gboolean must_convert, int dither_mode, 
		    StatusBar *bar, gchar *title);

Chunk *chunk_filter_tofmt(Chunk *chunk, chunk_filter_tofmt_proc proc, 
			  chunk_filter_tofmt_proc eof_proc, gint amount, 
			  gboolean must_convert, Dataformat *tofmt, 
			  int dither_mode,
			  StatusBar *bar, gchar *title);

/* Send a chunk's contents one sample at a time to a callback function.
 *
 *   chunk - The chunk to process.
 *   proc - The callback function (see definition of chunk_parse_proc above).
 *   allchannels - If TRUE the samples will be sent with all channels' data at
 *     the same time, if FALSE one channel at a time will be sent.
 *   convert - If TRUE the samples will be sent as sample_t (floating point) 
 *     values, otherwise they will be sent in raw format.
 *   samplepos - The location within the chunk that parsing should start at.
 *   reverse - If TRUE, parsing will move backward through the chunk; if FALSE
 *     parsing will move forward through the chunk.
 *
 *   returns - FALSE on success, TRUE if proc returned TRUE or a read error 
 *     occured.
 */

gboolean chunk_parse(Chunk *chunk, chunk_parse_proc proc, gboolean allchannels,
		     gboolean convert, int dither_mode,
		     StatusBar *bar, gchar *title, off_t samplepos, gboolean reverse);



/* ------ Miscellaneous chunk functions ------ */



/* Determines whether the format parameters of two chunks are the same.
 * To return TRUE, the chunks must have the same samplerate, samplesize, 
 * channels and sign. */

#define chunk_format_equal(c1,c2) dataformat_equal(&(c1->format),&(c2->format))



#define chunk_get_time(c,s,t) get_time((c)->format.samplerate,s,s,t,default_time_mode)



/* Calls func for each existing chunk. */
void chunk_foreach(GFunc func, gpointer user_data);

gboolean clipwarn(off_t clipcount, gboolean maycancel);


#endif
