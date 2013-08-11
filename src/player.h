/*
 * Copyright (C) 2003 2004 2005 2010, Magnus Hjorth
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


/* This module handles the sound playing. It reads its data from a
 * Chunk object, then plays it through the sound driver. The function
 * player_work must be called periodically to keep the playback
 * going. 
 * 
 * The code supports playing loops, and has special code to handle
 * very small loops without eating CPU. */

#ifndef PLAYER_H_INCLUDED
#define PLAYER_H_INCLUDED

#include <gtk/gtk.h>
#include "chunk.h"

extern gboolean varispeed_smooth_flag;

/* Fallback sample type to use if other fails */
extern Dataformat player_fallback_format;

/* Play a certain chunk. Returns TRUE if anything failed.  */
typedef void (*player_notify_func)(off_t realpos, off_t bufpos,
				   gboolean is_running);

gboolean player_play(Chunk *chunk, off_t startpos, off_t endpos, 
		     gboolean loop, player_notify_func nf);

/* TRUE if the play thread is currently playing. */

gboolean player_playing(void);

/* TRUE if the play thread is currently looping a sound */

gboolean player_looping(void);

/* Returns start- and endpoint of the currently playing part. */

void player_get_range(off_t *startpos, off_t *endpos);

/* Changes the playback range without moving the playback position */
void player_change_range(off_t start, off_t end);

/* Stops playing (or does nothing if not playing). */

void player_stop(void);

/* Gets the current position of the player thread in the data buffer. */

off_t player_get_buffer_pos(void);

/* Gets the current position which is actually playing right now. This function
 * compensates for the buffering done by the sound drivers. 
 */

off_t player_get_real_pos(void);


/* Use another chunk as sound source but keep all buffered data. 
 * movestart and movedist work the same way as in document_update */
void player_switch(Chunk *new_chunk, off_t movestart, off_t movedist);

/* Changes the buffer position. */

void player_set_buffer_pos(off_t pos);

/* Move playback position */

void player_nudge(gfloat seconds);

/* Set playback speed (1.0 = normal). Does nothing if varispeed is disabled  */

void player_set_speed(gfloat speed);

gfloat player_get_speed(void);

#endif
