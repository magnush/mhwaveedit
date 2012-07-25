/*
 * Copyright (C) 2002 2003 2004 2005 2008 2009 2010, Magnus Hjorth
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


#ifndef OUTPUT_H_INCLUDED
#define OUTPUT_H_INCLUDED

#include <gtk/gtk.h>
#include "main.h"
#include "dataformat.h"
#include "ringbuf.h"



/* Possible states for sound driver:
   1. Uninitialized (starting state)
   2. Idle
   3. Playing (or waiting for output)
   4. Recording
   5. Quitted (ending state)
*/

/* Global variable for selecting whether the sound driver should "lock" itself
 * onto the device. 
 */

extern gboolean sound_lock_driver;

/* Flag that decides whether output data should be byte-swapped. */

extern gboolean output_byteswap_flag;

/* Flag that will refuse to play mono files and suggest stereo instead */

extern gboolean output_stereo_flag;

/* ---------------------------------
   Functions available in all states 
   --------------------------------- */


/* Returns the name of the current output driver. The string should not be 
 * freed or modified.
 */

gchar *sound_driver_name(void);


/* Returns the ID of the current output driver. The string should not be
 * freed or modified.
 */

gchar *sound_driver_id(void);


/* Returns the index of the current output driver. */

int sound_driver_index(void);


/* Returns a list of valid soundDriver names */

GList *sound_driver_valid_names(void);


/* Translates a soundDriver name into an id. Returns NULL if name is not a 
 * valid name. 
 */

gchar *sound_driver_id_from_name(gchar *name);


gchar *sound_driver_id_from_index(int index);

/* Translates a soundDriver id into a name. Returns NULL if id is not a valid 
 * driver ID.
 */

gchar *sound_driver_name_from_id(gchar *id);


/* Returns whether a sound driver has a preferences dialog. If id==NULL the 
 * current driver is used.
 */

gboolean sound_driver_has_preferences(gchar *id);


/* Show the preferences dialog for a sound driver (if it exists). If id==NULL
 * the current driver is used.
 */

void sound_driver_show_preferences(gchar *id);



/* ----------------------------------------------
 * Functions available in state 1 (Uninitialized) 
 * ---------------------------------------------- */


/* Sound module initialization.
 * Called before gtk_init.
 * Changes state to state 2 (Idle).
 */

void sound_init(void);




/* -------------------------------------
 * Functions available in state 2 (Idle)
 * ------------------------------------- */


/* For sound drivers that need periodic polling, this function checks for 
 * readiness and calls the appropriate callbacks 
 * Returns -1 if polling again periodically is not needed,
 * 0 if no work was done
 * +1 if work was done.  */

gint sound_poll(void);

/* Sound module cleanup.
 * Changes state to state 5 (Quitted)
 */

void sound_quit(void);


/* Select which format to play and setup playing.
 * If the format isn't supported by the driver, the function does nothing and 
 * returns <0 if no message displayed or >0 if message was displayed. 
 * Changes state to state 3 (Playing) and returns FALSE if successful
 *
 * ready_func will be called whenever new data can be written.
 * ready_func is "edge-triggered", that is it is only called once and will not
 * be called again until you have called output_play with bufsize>0
 */

gint output_select_format(Dataformat *format, gboolean silent, 
			  GVoidFunc ready_func);


/* Suggest a format to use for playing back data of the input format.
 * Returns FALSE if no suggestion is available */
gboolean output_suggest_format(Dataformat *format, Dataformat *result);


/* output_stop() - does nothing in this state */


/* Returns TRUE if input is supported by the driver. 
 * The other input_*-routines will not be called if this function returns FALSE
 */

gboolean input_supported(void);


/* Returns a list of supported input formats. *complete is set to true if
 * these are the only formats supported. */

GList *input_supported_formats(gboolean *complete);

/* Select which format to record and setup recording.
 * If the format isn't supported by the driver, the function does nothing and 
 * returns <0 if no message displayed or >0 if message was displayed. 
 * Changes state to state 4 (Recording) and returns FALSE if successful. 
 * ready_func will be called when there is new data available. 
 *
 * On some drivers, ready_func may not be called until one call to input_store
 * has been made. Therefore, you should make sure to call input_store
 * once after this call succeeded.
 */

gint input_select_format(Dataformat *format, gboolean silent, 
			 GVoidFunc ready_func);



/* input_stop() - does nothing in this state */





/* ----------------------------------------
 * Functions available in state 3 (Playing)
 * ---------------------------------------- */


/* Stops playback.
 * Changes state to state 2 (Idle).
 * Can also be called in state 2, but should do nothing in that case.
 *
 * If must_flush is true the call will output all currently buffered
 * data before stopping. If must_flush is false it can still do so if
 * the driver's buffers are small.
 *
 * Returns true if all buffered output was sent. 
 */

gboolean output_stop(gboolean must_flush);


/* Returns TRUE iff it's possible to output more data (with output_play) */

gboolean output_want_data(void); 

/* Wait for output_want_data to return TRUE or a timeout expires. This call is
 * not supported on all drivers. It returns FALSE immediately if the call is not
 * supported
 */

gboolean output_wait(guint timeout);

/* Send as much data as possible to the output device.
 * Return the amount of data sent. 
 *
 * If bufsize=0 and buffer=NULL, the driver will go into "drain
 * mode". Each time it is called again with bufsize=0 and buffer=NULL,
 * some drivers will return the amount of data left in the buffers, 
 * some return non-zero if drain is incomplete, and zero for complete
 * some return always zero
 * Calling output_play with data cancels the drain process.
 */

guint output_play(gchar *buffer, guint bufsize);

/* Skips currently buffered data so we can start playing new data as quickly as
 * possible.
 */

void output_clear_buffers(void);



/* ------------------------------------------
 * Functions available in state 4 (Recording)
 * ------------------------------------------ */


/* Stops recording.
 * Changes state to state 2 (Idle).
 * Can also be called in state 2, but should do nothing in that case.
 */

void input_stop(void);


/* Hints to the sound driver that we're about to stop and we really don't want 
 * to gather any new data. */

void input_stop_hint(void);


/* Stores recorded data into the buffer parameter. */

void input_store(Ringbuf *buffer);

/* Returns number of input overruns since recording started or -1 if unknown. 
 */

int input_overrun_count(void);



#endif
