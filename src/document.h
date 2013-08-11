/*
 * Copyright (C) 2005 2006, Magnus Hjorth
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


#ifndef DOCUMENT_H_INCLUDED
#define DOCUMENT_H_INCLUDED

#include "chunk.h"
#include "listobject.h"

#define DOCUMENT(obj) GTK_CHECK_CAST(obj,document_get_type(),Document)
#define DOCUMENT_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,document_get_type(),DocumentClass)
#define IS_DOCUMENT(obj) GTK_CHECK_TYPE(obj,document_get_type())

struct MarkList {
     int length, alloced;
     char **names;
     off_t *places;
};

struct HistoryEntry;
struct HistoryEntry {
     Chunk *chunk;
     off_t selstart,selend,viewstart,viewend,cursor;
     struct MarkList marks;
     struct HistoryEntry *next,*prev;
};


typedef struct {

     GtkObject object;


     /* The filename associated with the document (or NULL if none). */ 
     gchar *filename;
     /* TRUE if the file referred to by filename has a lossy format. */
     gboolean lossy;


     /* A unique name for each document, either:
      * the file's name part without the path
      * the file's name part + a number (if >1 document with the same filename)
      * "untitled #n" (if filename == NULL)
      */
     gchar *titlename; /* Unique for each window, 
			* set whenever view->chunk!=NULL */
     guint title_serial;


     /* This points to where we are in the history or NULL if the 
      * history is empty */
     struct HistoryEntry *history_pos;

     /* Pointer to Mark data structure */
     struct MarkList marks;

     Chunk *chunk;              /* The chunk being viewed. */
     off_t viewstart,viewend;   /* The view start and end+1  */
     off_t selstart,selend;     /* Selection start and end+1 */
     off_t cursorpos;           /* Cursor position */

     /* Used when sending the selection_changed signal */
     off_t old_selstart, old_selend;
     /* Used when sending the cursor_changed signal */
     off_t old_cursorpos;
     off_t playbufpos, old_playbufpos;

     gboolean followmode;       /* Follow mode flag */     

     StatusBar *bar;

} Document;


typedef struct {

     GtkObjectClass parent_class;

     /* Emitted when the view start/end is changed. */
     void (*view_changed)(Document *doc); 

     /* Emitted when the selection start/end is changed. */
     void (*selection_changed)(Document *doc);

     /* Emitted when the cursor position is changed. 
      * Rolling is TRUE if the cursor was set because of playback */
     void (*cursor_changed)(Document *doc, gboolean rolling);

     /* Emitted when an action has been performed. */
     void (*state_changed)(Document *doc);

} DocumentClass;


extern ListObject *document_objects;

extern Document *playing_document;

/* If TRUE and follow mode is enabled, the cursor will always be centered in
 * view. */
extern gboolean view_follow_strict_flag;



typedef Chunk *(*document_apply_proc)(Chunk *chunk, StatusBar *bar, 
					 gpointer user_data);



GtkType document_get_type(void);

Document *document_new_with_file(gchar *filename, StatusBar *bar);
Document *document_new_with_chunk(Chunk *chunk, gchar *sourcename, 
				  StatusBar *bar);

void document_set_status_bar(Document *d, StatusBar *bar);

gboolean document_save(Document *d, gchar *filename, gint type_id, 
		       gboolean use_defs);



void document_play(Document *d, off_t start, off_t end, gboolean loop,
		   gfloat speed);
void document_play_from_cursor(Document *d, gboolean loopmode, gfloat speed);
void document_play_selection(Document *d, gboolean loopmode, gfloat speed);

/* Stop and keep cursor at current position */
void document_stop(Document *d, gboolean do_return);


/* Changes the document's chunk to new_chunk. The old state is pushed onto
 * the history stack. All marks, selection endpoints and the cursor will be 
 * moved movedist (can be negative) samples if they're to the right of 
 * movestart. If that would move them to the left of movestart, they will be 
 * either removed (marks) or set to movestart (selection endpoints & cursor) 
 *
 * To simplify other code, if new_chunk==NULL this function does nothing.
 */

void document_update(Document *d, Chunk *new_chunk, off_t movestart, 
		     off_t movedist);


/* Apply a certain filter function on the selection (or on the whole file
 * if nothing selected). */
gboolean document_apply(Document *d, chunk_filter_proc proc, 
			chunk_filter_proc eof_proc, gint amount,
			gboolean convert, gchar *title);

/* Same as document_apply but just parses the data, doesn't change it */
void document_parse(Document *d, chunk_parse_proc proc, 
		    gboolean allchannels, gboolean convert, gchar *title);

/* Call a procedure with a chunk representing the selection (or the whole
 * file if nothing selected). The returned Chunk from the procedure will
 * replace the selection. 
 */
gboolean document_apply_cb(Document *d, document_apply_proc proc, 
			   gboolean selection_only, gpointer user_data);




void document_set_followmode(Document *d, gboolean followmode);


void document_set_cursor(Document *d, off_t cursorpos);

off_t document_nudge_cursor(Document *d, off_t delta);

void document_set_view(Document *d, off_t viewstart, off_t viewend);

void document_scroll(Document *d, off_t distance);


/* Sets the selection.
 * d - A Document.
 * selstart - Starting point of the selection
 * selend - End of selection + 1. If selend==selstart the selection is removed.
 *          If selend<selstart the arguments are switched.
 */

void document_set_selection(Document *d, off_t selstart, off_t selend);



/* Zoom in or out 
 * d - A Document
 * zoom - The zooming factor. >1.0 means zooming in, <1.0 means zooming out. 
 *        Must be non-zero.
 */

void document_zoom(Document *d, gfloat zoom, gboolean followcursor);


/* Places a mark. 
 *
 * d - A Document.
 * label - A text label. If a mark with the same label exists, it will be 
 * removed.
 * position - At which sample to place the mark. Specify DOCUMENT_BAD_MARK
 *            to remove the mark.
 */

void document_set_mark(Document *d, gchar *label, off_t position);


/* Gets the position of a mark with a certain label.
 * d - A Document
 * label - The text label of the mark.
 * Returns: The position of the mark, or DOCUMENT_BAD_MARK
 * if it doesn't exist.
 */

off_t document_get_mark(Document *d, const gchar *label);
#define DOCUMENT_BAD_MARK ((off_t)-1)


/* Clears the filename component */
void document_forget_filename(Document *d);


/* Removes all marks. */
void document_clear_marks(Document *d);


void document_foreach_mark(Document *d, 
			   void (*func)(gchar *label, off_t position, 
					gpointer user_data), 
			   gpointer user_data);

gboolean document_can_undo(Document *d);
void document_undo(Document *d);
gboolean document_can_redo(Document *d);
void document_redo(Document *d);

#endif
