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


#include <config.h>
#include "ringbuf.h"
#include <string.h>

Ringbuf *ringbuf_new(guint32 size)
{
     Ringbuf *r;

     r = g_malloc(sizeof(Ringbuf)+size);

     r->size = size;
     r->start = r->end = 0;

     return r;
}

void ringbuf_free(Ringbuf *bufptr)
{
     g_free(bufptr);
}

guint32 ringbuf_available(Ringbuf *buf)
{
     guint32 start,end;

     start = buf->start;
     end = buf->end;

     if (end >= start) 
	  return end-start;
     else 
	  return buf->size+1 + end - start;
}

guint32 ringbuf_freespace(Ringbuf *buf)
{
     guint32 start,end;

     start = buf->start;
     end = buf->end;

     if (start > end)
	  return start - end - 1;
     else
	  return buf->size + start - end;
}

void ringbuf_drain(Ringbuf *buf)
{
     buf->end = buf->start;
}

gboolean ringbuf_isempty(Ringbuf *buf)
{
     return (buf->end == buf->start);
}

gboolean ringbuf_isfull(Ringbuf *buf)
{
     guint32 start,end;

     start = buf->start;
     end = buf->end;

     if (start == 0)
	  return (end == buf->size);
     else
	  return (end == start-1);
}

guint32 ringbuf_enqueue(Ringbuf *buf, gpointer data, guint32 datasize)
{
     guint32 blksize;
     guint32 start,end;
     guint32 bytes_done = 0;

     start = buf->start;
     end = buf->end;

     if (end >= start) {

	  if (start == 0)
	       blksize = MIN(datasize, buf->size - end);
	  else
	       blksize = MIN(datasize, 1+buf->size - end);

	  if (blksize == 0) return 0;
	  memcpy( buf->data + end, data, blksize );     
	  end += blksize;
	  if (end == 1+buf->size) {
	       end = 0;
	       bytes_done = blksize;
	       /* Fall through */
	  } else {
	       buf->end = end;	       
	       return blksize;
	  }
     } 
     
     blksize = MIN ( datasize-bytes_done, start - end - 1);

     if (blksize == 0) { buf->end = end; return bytes_done; }
     
     memcpy( buf->data + end,
	     G_STRUCT_MEMBER_P( data, bytes_done ), blksize);

     end += blksize;
     bytes_done += blksize;

     buf->end = end;
     
     return bytes_done;
}

guint32 ringbuf_enqueue_zeroes(Ringbuf *buf, guint32 count)
{
     guint32 blksize;
     guint32 start,end;
     guint32 bytes_done = 0;

     start = buf->start;
     end = buf->end;

     if (end >= start) {

	  if (start == 0)
	       blksize = MIN(count, buf->size - end);
	  else
	       blksize = MIN(count, 1+buf->size - end);

	  if (blksize == 0) return 0;
	  memset( buf->data + end, 0, blksize );
	  end += blksize;
	  if (end == 1+buf->size) {
	       end = 0;
	       bytes_done = blksize;
	       /* Fall through */
	  } else {
	       buf->end = end;	       
	       return blksize;
	  }
     } 
     
     blksize = MIN ( count-bytes_done, start - end - 1);

     if (blksize == 0) { buf->end = end; return bytes_done; }
     
     memset( buf->data + end, 0, blksize);

     end += blksize;
     bytes_done += blksize;

     buf->end = end;
     
     return bytes_done;
}

guint32 ringbuf_dequeue(Ringbuf *buf, gpointer data, guint32 datasize)
{
     guint32 blksize,start,end;
     guint32 bytes_done = 0;

     start = buf->start;
     end = buf->end;

     if (start > end) {
	  blksize = MIN ( datasize, buf->size+1 - start );
	  memcpy(data, buf->data+start, blksize);
	  start += blksize;
	  if (start == buf->size+1) {
	       start = 0;
	       bytes_done = blksize;
	       /* Fall through */
	  } else {
	       buf->start = start;
	       return blksize;
	  }	  
     } 

     blksize = MIN ( datasize - bytes_done, end - start );
     if (blksize == 0) { buf->start=start; return bytes_done; }
    
     memcpy ( G_STRUCT_MEMBER_P(data,bytes_done), buf->data + start, blksize );

     start += blksize;
     bytes_done += blksize;

     buf->start = start;

     return bytes_done;
}

guint32 ringbuf_transfer ( Ringbuf *source, Ringbuf *dest )
{
     guint32 blksize;
     guint32 dest_start,dest_end;
     guint32 bytes_done = 0;

     dest_start = dest->start;
     dest_end = dest->end;

     if (dest_end >= dest_start) {

	  if (dest_start == 0)
	       blksize = dest->size - dest_end;
	  else
	       blksize = 1+dest->size - dest_end;

	  if (blksize == 0) return 0;
	  blksize = ringbuf_dequeue(source, dest->data + dest_end, blksize);
	  dest_end += blksize;
	  if (dest_end == 1+dest->size) {
	       dest_end = 0;
	       bytes_done = blksize;
	       /* Fall through */
	  } else {
	       dest->end = dest_end;
	       return blksize;
	  }
     } 
     
     blksize = dest_start - dest_end - 1;

     if (blksize == 0) { dest->end = dest_end; return bytes_done; }
     
     blksize = ringbuf_dequeue(source, dest->data + dest_end, blksize);

     dest_end += blksize;
     bytes_done += blksize;

     dest->end = dest_end;
     
     return bytes_done;
}

