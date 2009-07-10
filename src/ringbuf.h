/*
 * Copyright (C) 2002 2003 2005, Magnus Hjorth
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


/* Ring-buffer functions */

#ifndef RINGBUF_H_INCLUDED
#define RINGBUF_H_INCLUDED

#include <glib.h>

typedef struct {
     
     volatile guint32 start;  /* First position in the buffer. */
     volatile guint32 end;    /* First unused position. Equal to start if 
			       * buffer is empty. */

     guint32 size;   /* Number of bytes that the buffer can hold. */
     gchar data[1];  /* The data buffer. */
} Ringbuf;

/* Creates a new ringbuffer with desired size */
Ringbuf *ringbuf_new(guint32 size);

/* Destroys a ringbuffer */
void ringbuf_free(Ringbuf *bufptr);

/* Empties a ringbuffer */
void ringbuf_drain(Ringbuf *buf);

/* Returns the amount of data currently in the queue. */
guint32 ringbuf_available(Ringbuf *buf);

/* Returns the amount of data that can be enqueued onto the queue
   (buf->size - ringbuf_available(buf)). */
guint32 ringbuf_freespace(Ringbuf *buf);

/* Tells whether the ringbuffer is empty or not. */
gboolean ringbuf_isempty(Ringbuf *buf);

/* Tells whether the ringbuffer is full or not. */
gboolean ringbuf_isfull(Ringbuf *buf);

/* Adds data to the end of the buffer. Returns the number of bytes added. */
guint32 ringbuf_enqueue(Ringbuf *buf, gpointer data, guint32 datasize);

/* Adds null bytes to the end of the buffer. Returns the number of bytes added*/
guint32 ringbuf_enqueue_zeroes(Ringbuf *buf, guint32 count);

/* Copies and removes data from the end of the buffer. Returns the number of
   bytes removed. */
guint32 ringbuf_dequeue(Ringbuf *buf, gpointer data, guint32 datasize);

/* Transfers data from one ringbuffer to another. Returns when source is empty
   or dest is full. Returns number of bytes transferred. */
guint32 ringbuf_transfer(Ringbuf *source, Ringbuf *dest);

#endif
