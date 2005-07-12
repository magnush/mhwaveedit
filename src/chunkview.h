/*
 * Copyright (C) 2002 2003 2004, Magnus Hjorth
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 */


/* ChunkView is a graphical component that gives a view of the data inside a 
 * chunk object. 
 */

#ifndef CHUNKVIEW_H_INCLUDED
#define CHUNKVIEW_H_INCLUDED

#include <gtk/gtk.h>
#include "chunk.h"
#include "viewcache.h"

#define CHUNKVIEW(obj) GTK_CHECK_CAST (obj, chunk_view_get_type(), ChunkView)
#define CHUNKVIEW_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,chunk_view_get_type(),ChunkViewClass)
#define IS_CHUNKVIEW(obj) GTK_CHECK_TYPE (obj,chunk_view_get_type())


typedef struct _ChunkView {

     /* PRIVATE MEMBERS - Keep out! */

     GtkDrawingArea parent;

     /* Image used for double-buffering the sample view. Only used under GTK2,
      * however the image_width/image_height variables are used at various
      * places so they are kept updated (ugly solution) */
     GdkPixmap *image;
     guint image_width,image_height;     


     ViewCache *cache;
     int last_redraw_time,need_redraw_left,need_redraw_right;
     GHashTable *marks;        

     gfloat scale_factor; /* Vertical scaling factor (default: 1.0) */

     /* READ-ONLY PUBLIC MEMBERS */
     /* To change these, use the public functions below. */

     Chunk *chunk;              /* The chunk being viewed. */
     off_t viewstart,viewend;   /* The view start and end+1  */
     off_t selstart,selend;     /* Selection start and end+1 */
     off_t cursorpos;           /* Cursor position */
     gboolean timescale;        /* Time scale visible flag */
     gboolean followmode;       /* Follow mode flag */

} ChunkView;

typedef struct _ChunkViewClass {
     GtkDrawingAreaClass parent_class;

     /* SIGNALS */

     /* Emitted when the view start/end is changed. */
     void (*view_changed)(ChunkView *view); 

     /* Emitted when the selection start/end is changed. */
     void (*selection_changed)(ChunkView *view);

     /* Emitted when the user double-clicked somewhere in the view */
     /* The off_t pointer is because off_t values aren't directly supported by
      * the GTK (1.2) type system */
     void (*double_click)(ChunkView *view, off_t *sample);

     /* Emitted when the cursor position is changed. The mode parameter 
      * is 1 if the cursor was changed by the user right clicking on the 
      * view, 2 if the cursor was changed after selecting using MB2 pressed,
      * and 0 if the cursor was changed by a call to chunk_view_set_cursor_pos.
      */
     void (*cursor_changed)(ChunkView *view, gint mode);

     /* Emitted when the viewed chunk is switched. */
     void (*chunk_changed)(ChunkView *view);

} ChunkViewClass;

extern gboolean chunk_view_follow_strict_flag;

GtkType chunk_view_get_type(void);


/* Creates a new ChunkView */
GtkWidget *chunk_view_new(void);


/* Selects which chunk to display. This will set the view to the whole file
 * and remove any selection.
 *   cv - A ChunkView object.
 *   c - The Chunk that <cv> should display.
 */

void chunk_view_set_chunk(ChunkView *cv, Chunk *c);


/* Same as chunk_view_set_chunk except it does not change the view. */

void chunk_view_set_chunk_keep_view(ChunkView *cv, Chunk *c);


/* Sets the selection.
 * cv - A ChunkView.
 * selstart - Starting point of the selection
 * selend - End of selection + 1. If selend==selstart the selection is removed.
 *          If selend<selstart the arguments are switched.
 */

void chunk_view_set_selection(ChunkView *cv, off_t selstart, off_t selend);


/* Selects which part of the chunk to view
 * cv - A ChunkView.
 * viewstart - Starting point of the view.
 * viewend - End of view + 1.
 */

void chunk_view_set_view(ChunkView *cv, off_t viewstart, off_t viewend);


/* Selects where to position the cursor (a gray vertical bar).
 * cv - A ChunkView.
 * cursor - Sample position of the cursor.
 */

void chunk_view_set_cursor(ChunkView *cv, off_t cursor);


/* Selects whether a time scale should be drawn at the bottom of the view. */
void chunk_view_set_timescale(ChunkView *cv, gboolean scale_visible);


/* Zoom in or out on the current view.
 * cv - A ChunkView
 * zoom - The zooming factor. >1.0 means zooming in, <1.0 means zooming out. 
 *        Must be non-zero.
 */

void chunk_view_zoom(ChunkView *cv, gfloat zoom, gboolean followcursor);


/* Places a mark. The mark will be displayed as a horizontal line with a 
 * text label.
 *
 * cv - A ChunkView.
 * label - A text label. If a mark with the same label exists, it will be 
 * removed.
 * position - At which sample to place the mark. Specify CHUNK_VIEW_BAD_MARK
 *            to remove the mark.
 */

void chunk_view_set_mark(ChunkView *cv, gchar *label, off_t position);


/* Gets the position of a mark with a certain label.
 * cv - A ChunkView
 * label - The text label of the mark.
 * Returns: The position of the mark, or CHUNK_VIEW_BAD_MARK
 * if it doesn't exist.
 */

off_t chunk_view_get_mark(ChunkView *cv, gchar *label);
#define CHUNK_VIEW_BAD_MARK ((off_t)-1)



/* Removes all marks. */
void chunk_view_clear_marks(ChunkView *cv);


void chunk_view_foreach_mark(ChunkView *cv, 
			     void (*func)(gchar *label, off_t position, 
					  gpointer user_data), 
			     gpointer user_data);


/* This is used when data has been inserted/removed to move the marks 
 * properly
 *
 *   cv - A ChunkView
 *   offset - The lowest position where data has been inserted / removed
 *   motion - How much data has been inserted, or if negative how much has been
 *            removed
 */

void chunk_view_update_marks(ChunkView *cv, off_t offset, off_t motion);

/* This is called when the caches should progress. */

gboolean chunk_view_update_cache(ChunkView *cv);

/* Enables/Disables follow mode. In follow mode, the view will move if the
 * cursor is placed outside the view. 
 */

void chunk_view_set_followmode(ChunkView *cv, gboolean fm);

gboolean chunk_view_autoscroll(void);

void chunk_view_force_repaint(ChunkView *cv);

void chunk_view_use_backing_pixmap(ChunkView *cv, gboolean use_pixmap);

void chunk_view_scroll(ChunkView *cv, off_t amount);

void chunk_view_set_scale(ChunkView *cv, gfloat scale);

#endif
