/*
 * Copyright (C) 2003 2004, Magnus Hjorth
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


#ifndef VIEWCACHE_H_INCLUDED
#define VIEWCACHE_H_INCLUDED

#include "chunk.h"

typedef struct {
     Chunk *chunk;
     ChunkHandle *handle;
     gboolean chunk_error;
     off_t start,end;
     gint xres;
     sample_t *values;
     off_t *offsets;
     gchar *calced;
} ViewCache;

ViewCache *view_cache_new(void);
void view_cache_free(ViewCache *vc);

/* Updates the cache. 
 *   chunk : The chunk we're viewing
 *   start_samp,end_samp : The part of the chunk we're viewing
 *   xres : Width of view in pixels
 *   low_updated,high_updated : Set to left and right endpoint of
 *   updated region. If nothing was updated, both are set to 0. The
 *   pointers may be NULL. Not modified if function returns FALSE.
 *   Returns TRUE if any work was done.
 */
gboolean view_cache_update(ViewCache *cache, Chunk *chunk, off_t start_samp, 
			   off_t end_samp, gint xres, 
			   gint *low_updated, gint *high_updated);

gboolean view_cache_uptodate(ViewCache *cache);

void view_cache_draw(ViewCache *cache, GdkDrawable *d, gint yres, 
		     GtkWidget *wid, gfloat scale);
void view_cache_draw_part(ViewCache *cache, GdkDrawable *d, gint xs, gint xe,
			  gint yres, GtkWidget *wid, gfloat scale);

#endif
