/*
 * Copyright (C) 2005 2006 2007 2010 2011 2012, Magnus Hjorth
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


#include "um.h"
#include "document.h"
#include "filetypes.h"
#include "player.h"
#include "gettext.h"

ListObject *document_objects = NULL;

Document *playing_document = NULL;
static guint untitled_count = 0;

gboolean view_follow_strict_flag;

enum { VIEW_CHANGED_SIGNAL, SELECTION_CHANGED_SIGNAL, CURSOR_CHANGED_SIGNAL,
       STATE_CHANGED_SIGNAL, LAST_SIGNAL };
static guint document_signals[LAST_SIGNAL] = { 0 };

static GtkObjectClass *parent_class;

static void document_set_cursor_main(Document *d, off_t cursorpos, 
				     gboolean playslave, gboolean running,
				     off_t bufpos);

static void clear_marklist(struct MarkList *m)
{
     int i;
     for (i=0; i<m->length; i++) g_free(m->names[i]);
     g_free(m->names);
     g_free(m->places);
     m->names = NULL;
     m->places = NULL;
     m->length = m->alloced = 0;     
}

static void copy_marklist(struct MarkList *dest, struct MarkList *src)
{
     int i;

     for (i=0; i<dest->length; i++) g_free(dest->names[i]);
     dest->length = 0;

     if ( dest->alloced < src->length ) {
	  dest->names = g_realloc(dest->names, 
				  src->length * sizeof(dest->names[0]));
	  dest->places = g_realloc(dest->places, 
				   src->length * sizeof(dest->places[0]));
	  dest->alloced = src->length;
     }

     for (i=0; i<src->length; i++) {
	  dest->names[i] = g_strdup(src->names[i]);
	  dest->places[i] = src->places[i];
     }
     dest->length = src->length;
}

static gboolean marklist_equal(struct MarkList *l1, struct MarkList *l2)
{
     int i,j;
     if (l1->length != l2->length) return FALSE;
     for (i=0; i<l1->length; i++) {
	  for (j=0; j<l2->length; j++)
	       if (!strcmp(l1->names[i],l2->names[j])) break;	  	  
	  if (j == l2->length) return FALSE;
	  if (l1->places[i] != l2->places[i]) return FALSE;
     }
     return TRUE;
}

static void document_init(GtkObject *obj)
{
     Document *d = DOCUMENT(obj);
     d->filename = NULL;
     d->lossy = FALSE;
     d->titlename = NULL;
     d->title_serial = 0;
     d->history_pos = NULL;
     d->chunk = NULL;
     d->viewstart = d->viewend = 0;
     d->selstart = d->selend = 0;
     d->cursorpos = 0;
     d->marks.length = d->marks.alloced = 0;
     /* Most initialization is delayed until we have a chunk */
}

static void clear_history(struct HistoryEntry *h)
{ 
     struct HistoryEntry *ph;
     if (h != NULL) {
	  while (h->next != NULL) h=h->next;
	  while (h != NULL) {
	       if (h->chunk != NULL) {
		    gtk_object_unref(GTK_OBJECT(h->chunk));
		    h->chunk = NULL;
	       }
	       clear_marklist(&(h->marks));

	       ph = h->prev;
	       g_free(h);
	       h = ph;
	  }
     }     
}

static void document_destroy(GtkObject *object)
{
     Document *d = DOCUMENT(object);     
     if (d->filename != NULL) {
	  g_free(d->filename);
	  d->filename = NULL;
     }
     if (d->titlename != NULL) {
	  g_free(d->titlename);
	  d->titlename = NULL;
     }
     if (d->chunk != NULL) {
	  gtk_object_unref(GTK_OBJECT(d->chunk));
	  d->chunk = NULL;
     }
     clear_history(d->history_pos);
     d->history_pos = NULL;
     list_object_remove(document_objects,object);
     parent_class->destroy(object);
}

static void document_state_changed(Document *d)
{
     list_object_notify(document_objects,d);
}

static void document_class_init(GtkObjectClass *klass)
{
     DocumentClass *dc = DOCUMENT_CLASS(klass);
     parent_class = gtk_type_class(gtk_object_get_type());
     
     klass->destroy = document_destroy;
     dc->view_changed = NULL;
     dc->selection_changed = NULL;
     dc->cursor_changed = NULL;
     dc->state_changed = document_state_changed;

     document_signals[VIEW_CHANGED_SIGNAL] = 
	  gtk_signal_new("view-changed", GTK_RUN_FIRST, GTK_CLASS_TYPE(klass),
			 GTK_SIGNAL_OFFSET(DocumentClass,view_changed),
			 gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 0);
     document_signals[SELECTION_CHANGED_SIGNAL] = 
	  gtk_signal_new("selection-changed", GTK_RUN_FIRST, 
			 GTK_CLASS_TYPE(klass), 
			 GTK_SIGNAL_OFFSET(DocumentClass,selection_changed),
			 gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 0);
     document_signals[CURSOR_CHANGED_SIGNAL] = 
	  gtk_signal_new("cursor-changed", GTK_RUN_FIRST, GTK_CLASS_TYPE(klass),
			 GTK_SIGNAL_OFFSET(DocumentClass,cursor_changed),
			 gtk_marshal_NONE__BOOL, GTK_TYPE_NONE, 1, 
			 GTK_TYPE_BOOL);
     document_signals[STATE_CHANGED_SIGNAL] = 
	  gtk_signal_new("state-changed", GTK_RUN_FIRST, GTK_CLASS_TYPE(klass),
			 GTK_SIGNAL_OFFSET(DocumentClass,state_changed),
			 gtk_marshal_NONE__NONE, GTK_TYPE_NONE, 0);

     gtk_object_class_add_signals(klass,document_signals,LAST_SIGNAL);

}

GtkType document_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "Document",
	       sizeof(Document),
	       sizeof(DocumentClass),
	       (GtkClassInitFunc) document_class_init,
	       (GtkObjectInitFunc) document_init
	  };
	  id = gtk_type_unique(gtk_object_get_type(),&info);
     }
     return id;
}

Document *document_new_with_file(gchar *filename, StatusBar *bar)
{
     Chunk *c;
     gboolean b;
     Document *d;
     c = chunk_load_x(filename,dither_editing,bar,&b);
     if (c == NULL) return NULL;
     d = document_new_with_chunk(c,filename,bar);
     d->lossy = b;
     return d;
}

static void document_set_filename(Document *d, gchar *filename, 
				  gboolean do_notify)
{
     guint i=0;
     GList *l;
     Document *x;
     if ((filename == d->filename && filename != NULL) || 
	 (d->filename != NULL && filename != NULL && 
	  !strcmp(d->filename,filename)))
	  return;
     g_free(d->titlename);
     g_free(d->filename);
     d->filename = g_strdup(filename);
     if (filename == NULL) {
	  d->titlename = g_strdup_printf(_("untitled #%d"),++untitled_count);
     } else {	  
	  for (l=document_objects->list;l!=NULL;l=l->next) {
	       x = DOCUMENT(l->data);
	       if (x == d) continue;
	       if (d->filename == NULL || x->filename == NULL ||
		   strcmp(namepart(d->filename),namepart(x->filename)))
		    continue;
	       if (x->title_serial > i) i=x->title_serial;
	  }
	  d->title_serial = i+1;
	  if (d->title_serial > 1)
	       d->titlename = g_strdup_printf("%s #%d",namepart(d->filename),
					      d->title_serial);
	  else
	       d->titlename = g_strdup(namepart(d->filename));
     }
     if (do_notify)
	  list_object_notify(document_objects,d);     
}

void document_forget_filename(Document *d)
{
     document_set_filename(d,NULL,FALSE);
     gtk_signal_emit(GTK_OBJECT(d),document_signals[STATE_CHANGED_SIGNAL]);
}

Document *document_new_with_chunk(Chunk *chunk, gchar *sourcename,
				  StatusBar *bar)
{
     Document *d = (Document *)gtk_type_new(document_get_type());
     /* Don't send the notify signal from document_objects here, since we 
      * haven't added the document yet. */
     document_set_filename(d,sourcename,FALSE);
     d->chunk = chunk;
     gtk_object_ref(GTK_OBJECT(chunk));
     gtk_object_sink(GTK_OBJECT(chunk));
     d->viewstart = 0;
     d->viewend = chunk->length;
     d->bar = bar;
     /* The add signal will be sent here */
     list_object_add(document_objects,d);
     return d;
}

gboolean document_save(Document *d, gchar *filename, gint type_id, 
		       gboolean use_defs)
{
     gboolean b;
     int i;
     if (d->filename != NULL && !strcmp(filename,d->filename) &&
	 d->lossy) {
	  i = user_message(_("Loading and then saving files with 'lossy' "
			     "formats (like mp3 and ogg) leads to a quality "
			     "loss, also for the unmodified parts of the "
			     "file."), UM_OKCANCEL);
	  if (i == MR_CANCEL) return TRUE;
     }
     b = chunk_save(d->chunk,filename,type_id,use_defs,dither_editing,d->bar);
     if (!b) {
	  clear_history(d->history_pos);
	  d->history_pos = NULL;
	  document_set_filename(d,filename,TRUE);
	  gtk_signal_emit(GTK_OBJECT(d),
			  document_signals[STATE_CHANGED_SIGNAL]);
     }
     return b;
}

static void cursor_cb(off_t pos, off_t bufpos, gboolean is_running)
{     
     Document *d = playing_document;
     if (playing_document != NULL) {
	  if (!is_running) playing_document=NULL; 
	  document_set_cursor_main(d,pos,TRUE,is_running,bufpos);
	  if (!is_running) gtk_object_unref(GTK_OBJECT(d));
     }     
}

void document_play(Document *d, off_t start, off_t end, gboolean loop, 
		   gfloat speed)
{     
     off_t ps,pe,pc;
     
     g_assert(d != NULL);
     /* This special if case was introduced to avoid a skipping sound
      * caused by the cursor not matching the actual playing positon when
      * pressing the "play" button and you're already playing. This
      * becomes a "wobbling" sound when holding down the ',' key due
      * to the keyboard auto-repeating. */
     if (player_playing() && playing_document == d && 
	 BOOLEQ(player_looping(),loop) && start == d->cursorpos 
	 && d->cursorpos < player_get_buffer_pos()) {
	  /* Keep everything intact except end */
	  player_get_range(&ps,&pe);
	  pc = player_get_buffer_pos();
	  player_change_range(pc,end);	  
     } else {

	  /* General case */
	  /* Stop the player if running, this should lead to cursor_cb
	   * being called and clearing playing_document */
	  player_stop();
	  g_assert(playing_document == NULL);
	  /* playing_document must be setup before calling player_play,
	   * since for very short files the cursor_cb might be called
	   * immediately with is_running==FALSE */
	  playing_document = d;
	  gtk_object_ref(GTK_OBJECT(d));
	  if (player_play(d->chunk,start,end,loop,cursor_cb)) {
	       playing_document = NULL;
	       gtk_object_unref(GTK_OBJECT(d));
	  }
     }

     /* This sets the speed to the proper value */
     player_set_speed(speed);
}

void document_play_from_cursor(Document *d, gboolean loopmode, gfloat speed)
{
     g_assert(d != NULL);
     document_play(d,d->cursorpos,d->chunk->length,loopmode,speed);
}

void document_play_selection(Document *d, gboolean loopmode, gfloat speed)
{
     g_assert(d != NULL);
     if (d->selstart != d->selend)
	  document_play(d,d->selstart,d->selend,loopmode,speed);
     else
	  document_play(d,0,d->chunk->length,loopmode,speed);
}

void document_stop(Document *d, gboolean do_return)
{
     off_t s,e;
     g_assert(d != NULL);
     if (d != playing_document) return;
     player_get_range(&s,&e);
     player_stop();
     g_assert(playing_document == NULL);
     if (do_return)
	  document_set_cursor(d,s);     
}

/* Make sure the current state is stored properly in history */
static void fix_history(Document *d)
{
     struct HistoryEntry *h;

     if (d->history_pos == NULL || d->chunk != d->history_pos->chunk || 
	 d->selstart != d->history_pos->selstart ||
	 d->selend != d->history_pos->selend || 
	 !marklist_equal(&(d->marks),&(d->history_pos->marks))) {

	  /* Create a new history entry and add it to the history */ 
	  h = g_malloc0(sizeof(*h));
	  h->chunk = d->chunk;
	  gtk_object_ref(GTK_OBJECT(d->chunk));
	  h->selstart = d->selstart;
	  h->selend = d->selend;
	  h->viewstart = d->viewstart;
	  h->viewend = d->viewend;
	  h->cursor = d->cursorpos;
	  copy_marklist(&(h->marks), &(d->marks));	  

	  if (d->history_pos == NULL) {
	       h->prev = h->next = NULL;
	       d->history_pos = h;
	  } else {
	       h->prev = d->history_pos;
	       h->next = d->history_pos->next;
	       if (h->next != NULL) h->next->prev = h;
	       d->history_pos->next = h;
	       d->history_pos = h;
	  }
     }
     
}

void document_update(Document *d, Chunk *new_chunk, 
		     off_t movestart, off_t movedist)
{
     int i,j;

     if (new_chunk == NULL) return;

     /* Make sure the current state is stored in history */
     fix_history(d);

     /* Remove current redo data, if any. */
     if (d->history_pos->next != NULL) {
	  d->history_pos->next->prev = NULL;
	  clear_history(d->history_pos->next);
	  d->history_pos->next = NULL;	  
     }

     /* If we've converted sample rate or channel, we must stop playback */
     if ((d->chunk->format.channels != new_chunk->format.channels || 
	  d->chunk->format.samplerate != new_chunk->format.samplerate) &&
	 playing_document == d)
	  player_stop();

     /* Set the new chunk */
     gtk_object_unref(GTK_OBJECT(d->chunk));
     d->chunk = new_chunk;
     gtk_object_ref(GTK_OBJECT(new_chunk));
     gtk_object_sink(GTK_OBJECT(new_chunk));

     /* Update the view. We have three cases: */
     if ((d->viewend - d->viewstart) >= new_chunk->length) {
	  /* 1. The resulting chunk is smaller than the current view */
	  d->viewstart = 0;
	  d->viewend = new_chunk->length;
     } else if (d->viewend > new_chunk->length) {
	  /* 2. The resulting chunk is large enough for the current view
	   *    but the view must be panned to the left */
	  d->viewstart -= (d->viewend - new_chunk->length);
	  d->viewend = new_chunk->length;
     } else {
	  /* 3. The view still works - do nothing */
     }

     /* Update selection endpoints */
     if (d->selstart >= movestart) {
	  d->selstart += movedist;
	  if (d->selstart < movestart) d->selstart = movestart;	  	      
	  d->selend += movedist;	  
	  if (d->selend < movestart) d->selend = movestart;	  
     }
     if (d->selstart >= new_chunk->length)
	  d->selstart = d->selend = 0;
     else if (d->selend >= new_chunk->length) 
	  d->selend = new_chunk->length;

     /* Update marks */
     for (i=d->marks.length-1; i>=0; i--) {
	  if (d->marks.places[i] >= movestart) {

	       d->marks.places[i] += movedist;

	       if (d->marks.places[i] < movestart || 
		   d->marks.places[i] > new_chunk->length) {
		    g_free(d->marks.names[i]);
		    for (j=i; j<d->marks.length-1; j++) {
			 d->marks.places[j] = d->marks.places[j+1];
			 d->marks.names[j] = d->marks.names[j+1];
		    }
		    d->marks.length --;
	       }		    
	  }
     }

     /* If we're playing this document, update the playback range. If not, 
      * update the cursor */
     if (playing_document == d && player_playing()) 
	  player_switch(new_chunk, movestart, movedist);
     else {
	  if (d->cursorpos >= movestart) {
	       d->cursorpos += movedist;
	       if (d->cursorpos < movestart) d->cursorpos = movestart;
	  }
	  if (d->cursorpos > new_chunk->length) 
	       d->cursorpos = new_chunk->length;
     }

     /* Make sure the current state is stored in history */
     fix_history(d);

     /* Emit signal */
     gtk_signal_emit(GTK_OBJECT(d),document_signals[STATE_CHANGED_SIGNAL]);
}

gboolean document_apply(Document *d, chunk_filter_proc proc, 
			chunk_filter_proc eof_proc, gint amount,
			gboolean convert, gchar *title)
{
     Chunk *c,*p,*r;
     off_t u,plen,rlen;
     /* Decide if we should filter selection or the whole file */
     if ((d->selstart == d->selend) ||
	 (d->selstart==0 && d->selend >= d->chunk->length)) {
	  /* procstart(w); */
	  r = chunk_filter( d->chunk, proc, eof_proc, amount, 
			    convert, dither_editing, d->bar, title);
	  /* procend(w); */
	  if (r) { 
	       document_update(d, r, 0, 0);
	       return FALSE;
	  } else
	       return TRUE;
     } else {
	  u = d->selstart;
	  p = chunk_get_part(d->chunk, u, d->selend - u);
	  plen = p->length;
	  /* procstart(w); */
	  r = chunk_filter( p, proc, eof_proc, amount,
			    convert, dither_editing, d->bar, title);
	  /* procend(w); */
	  gtk_object_sink(GTK_OBJECT(p));
	  if (r) {
	       rlen = r->length;
	       c = chunk_replace_part(d->chunk,u,plen,r);
	       gtk_object_sink(GTK_OBJECT(r));
	       /* If the result is longer, report the difference as an 
		* insertion at the old selection's right endpoint
		* If the result is shorter, report the difference as a 
		* removal at the new selection's right endpoint */
	       if (rlen > plen)
		    document_update(d, c, d->selstart+plen, rlen-plen);
	       else
		    document_update(d, c, d->selstart+rlen, rlen-plen);
	       return FALSE;
	  } else
	       return TRUE;
     }
}



void document_parse(Document *d, chunk_parse_proc proc, 
		    gboolean allchannels, gboolean convert, gchar *title)
{
     Chunk *c;
     if ((d->selstart == d->selend) ||
	 (d->selstart==0 && d->selend >= d->chunk->length)) {
	  /* procstart(w); */
	  chunk_parse(d->chunk,proc,allchannels,convert,dither_editing,
		      d->bar,title,0,FALSE);
	  /* procend(w); */
     } else {
	  c = chunk_get_part(d->chunk,d->selstart,
			     d->selend - d->selstart);
	  /* procstart(w); */
	  chunk_parse(c,proc,allchannels,convert,dither_editing,
		      d->bar,title,0,FALSE);
	  /* procend(w); */
	  gtk_object_sink(GTK_OBJECT(c));
     }
}


gboolean document_apply_cb(Document *d, document_apply_proc proc, 
			   gboolean selection_only, gpointer user_data)
{
     Chunk *c,*p,*r;
     off_t u,plen,rlen;
     /* Decide if we should filter selection or the whole file */
     if ((!selection_only) || 
	 (d->selstart == d->selend) ||
	 (d->selstart==0 && d->selend >= d->chunk->length)) {
	  r = proc( d->chunk, d->bar, user_data );
	  if (r) { 
	       document_update(d, r, 0, 0);
	       return FALSE;
	  } else
	       return TRUE;
     } else {
	  u = d->selstart;
	  p = chunk_get_part(d->chunk, u, d->selend - u);
	  plen = p->length;
	  r = proc( p, d->bar, user_data );
	  gtk_object_sink(GTK_OBJECT(p));
	  if (r) {
	       rlen = r->length;
	       c = chunk_replace_part(d->chunk,u,plen,r);
	       gtk_object_sink(GTK_OBJECT(r));
	       /* If the result is longer, report the difference as an 
		* insertion at the old selection's right endpoint
		* If the result is shorter, report the difference as a 
		* removal at the new selection's right endpoint */
	       if (rlen > plen)
		    document_update(d, c, d->selstart+plen, rlen-plen);
	       else
		    document_update(d, c, d->selstart+rlen, rlen-plen);
	       return FALSE;
	  } else
	       return TRUE;
     }     
}


static void document_set_cursor_main(Document *d, off_t cursorpos, 
				     gboolean playslave, gboolean running,
				     off_t bufpos)
{
     off_t vs,ve,dist;

     g_assert(cursorpos >= 0 && cursorpos <= d->chunk->length);

     if (d->cursorpos == cursorpos) return;

     if (!playslave && playing_document == d && player_playing()) {
	  player_set_buffer_pos(cursorpos);
	  return;
     }

	  
     if (d->followmode && playslave) {
	  dist = d->viewend - d->viewstart;
	  if (view_follow_strict_flag) {
	       vs = cursorpos - dist/2;
	       ve = vs + dist;
	  } else {	       
	       if (d->cursorpos < d->viewend && 
		   cursorpos > d->viewend && cursorpos < d->viewend + dist) {
		    /* Scroll one page forward */
		    vs = d->viewend;
		    ve = d->viewend + dist;
	       }  else if (cursorpos >= d->viewstart && 
			   cursorpos < d->viewend) {
		    /* Do nothing */
		    vs = d->viewstart;
		    ve = d->viewend;
	       } else {
		    vs = cursorpos - dist/2;
		    ve = vs + dist;
	       }
	  }
	  if (vs < 0) {
	       ve -= vs;
	       vs = 0;
	  } else if (ve > d->chunk->length) {
	       ve = d->chunk->length;
	       vs = ve - dist;
	  }
	  document_set_view(d,vs,ve);
     }
     
     d->old_cursorpos = d->cursorpos;
     d->cursorpos = cursorpos;
     d->old_playbufpos = d->playbufpos;
     d->playbufpos = running ? bufpos : -1;

     gtk_signal_emit(GTK_OBJECT(d),
		     document_signals[CURSOR_CHANGED_SIGNAL],
		     running);	  
}

void document_set_cursor(Document *d, off_t cursorpos)
{
     document_set_cursor_main(d,cursorpos,FALSE,(playing_document == d && player_playing()),cursorpos);
}

off_t document_nudge_cursor(Document *d, off_t delta)
{
     off_t newpos;

     newpos = d->cursorpos + delta;
     if(newpos > d->chunk->length)
     {
          delta = (d->chunk->length) - (d->cursorpos);
     } else if(newpos < 0) {
          delta = - (d->cursorpos);
     }

     newpos = d->cursorpos + delta;
     document_set_cursor(d,newpos);

     return delta;
}

void document_set_followmode(Document *d, gboolean mode)
{
     d->followmode = mode;
}

void document_set_view(Document *d, off_t viewstart, off_t viewend)
{
     off_t o;
     
     /* puts("document_set_view"); */
     if (viewstart > viewend) {
	  o = viewstart;
	  viewstart = viewend;
	  viewend = o;
     }
     g_assert(viewstart >= 0 && 
	      (viewstart < d->chunk->length || d->chunk->length == 0) &&
	      viewend >= 0 && viewend <= d->chunk->length);
     if (d->viewstart != viewstart || d->viewend != viewend) {
	  d->viewstart = viewstart;
	  d->viewend = viewend;
	  gtk_signal_emit(GTK_OBJECT(d),document_signals[VIEW_CHANGED_SIGNAL]);
     }
}

void document_scroll(Document *d, off_t distance)
{
     if (d->viewstart + distance < 0) 
	  distance = -d->viewstart;
     else if (d->viewend + distance > d->chunk->length)
	  distance = d->chunk->length - d->viewend;
     document_set_view(d,d->viewstart+distance,d->viewend+distance);
}

void document_set_selection(Document *d, off_t selstart, off_t selend)
{
     off_t o;
     
     /* Adjust the input arguments so they're properly ordered etc. */
     if (selstart > selend) {
	  o = selstart;
	  selstart = selend;
	  selend = o;
     }
     if (selstart == selend) 
	  selstart = selend = 0;
     
     if (d->chunk->length == 0 && d->selstart == 0 && d->selend == 0) return;

     g_assert(selstart >= 0 && selstart < d->chunk->length &&
	      selend >= 0 && selend <= d->chunk->length);
     if (d->selstart != selstart || d->selend != selend) {
	  d->old_selstart = d->selstart;
	  d->old_selend = d->selend;
	  d->selstart = selstart;
	  d->selend = selend;
	  gtk_signal_emit(GTK_OBJECT(d),
			  document_signals[SELECTION_CHANGED_SIGNAL]);
     }
}

void document_zoom(Document *d, gfloat zoom, gboolean followcursor)
{     
     off_t dist,newdist;
     off_t newstart,newend;
     /* puts("document_zoom"); */
     dist = d->viewend - d->viewstart;
     newdist = (off_t) ((gfloat) dist / zoom);
     if (newdist >= d->chunk->length) {
	  document_set_view(d,0,d->chunk->length);
	  return;
     }
     if (newdist < 1) newdist = 1;
     /* printf("dist: %d, newdist: %d\n",(int)dist,(int)newdist);
	printf("d->cursorpos: %d, d->viewstart: %d, d->viewend: %d\n",
	(int)d->cursorpos,(int)d->viewstart,(int)d->viewend); */
     if (followcursor && d->cursorpos >= d->viewstart && 
	 d->cursorpos <= d->viewend)
	  newstart = d->cursorpos - newdist/2;
     else
	  newstart = d->viewstart + (dist - newdist)/2;
     if (newstart < 0) newstart = 0;
     newend = newstart + newdist;
     if (newend > d->chunk->length) {
	  newend = d->chunk->length;
	  newstart = newend - newdist;
     }
     document_set_view(d, newstart, newend);
}

void document_set_mark(Document *d, gchar *label, off_t position)
{
     int i,j;
     for (i=0; i<d->marks.length; i++)
	  if (!strcmp(d->marks.names[i],label)) {
	       if (position == DOCUMENT_BAD_MARK) {
		    g_free(d->marks.names[i]);
		    for (j=i; j<d->marks.length-1; j++) {
			 d->marks.names[j] = d->marks.names[j+1];
			 d->marks.places[j] = d->marks.places[j+1];
		    }
		    d->marks.length --;
	       } else {
		    d->marks.places[i] = position;
	       }
	       gtk_signal_emit(GTK_OBJECT(d),
			       document_signals[STATE_CHANGED_SIGNAL]);
	       return;
	  }
     if (d->marks.length == d->marks.alloced) {
	  d->marks.alloced += 16;
	  d->marks.names = g_realloc(d->marks.names,d->marks.alloced*
				     sizeof(d->marks.names[0]));
	  d->marks.places = g_realloc(d->marks.places,d->marks.alloced*
				      sizeof(d->marks.places[0]));
     }
     d->marks.names[d->marks.length] = g_strdup(label);
     d->marks.places[d->marks.length] = position;
     d->marks.length ++;
     gtk_signal_emit(GTK_OBJECT(d),
		     document_signals[STATE_CHANGED_SIGNAL]);
}

off_t document_get_mark(Document *d, const gchar *label)
{
     int i;
     for (i=0; i<d->marks.length; i++) {
	  if (!strcmp(d->marks.names[i],label))
	       return d->marks.places[i];
     }
     return DOCUMENT_BAD_MARK;
}

void document_clear_marks(Document *d)
{
     if (d->marks.length > 0) {
	  clear_marklist(&(d->marks));
	  gtk_signal_emit(GTK_OBJECT(d),
			  document_signals[STATE_CHANGED_SIGNAL]);
     }
}

void document_foreach_mark(Document *d, 
			   void (*func)(gchar *label, off_t position, 
					gpointer user_data), 
			   gpointer user_data)
{
     int i;
     for (i=0; i<d->marks.length; i++)
	  func(d->marks.names[i],d->marks.places[i],user_data);
}

gboolean document_can_undo(Document *d)
{
     return (d->history_pos != NULL && d->history_pos->prev != NULL);
}

gboolean document_can_redo(Document *d)
{
     return (d->history_pos != NULL && d->history_pos->next != NULL);
}

static void get_state_from_history(Document *d)
{
     d->viewstart = d->history_pos->viewstart;
     d->viewend = d->history_pos->viewend;
     d->selstart = d->history_pos->selstart;
     d->selend = d->history_pos->selend;
     gtk_object_unref(GTK_OBJECT(d->chunk));
     d->chunk = d->history_pos->chunk;
     gtk_object_ref(GTK_OBJECT(d->chunk));
     copy_marklist(&(d->marks),&(d->history_pos->marks));
     gtk_signal_emit(GTK_OBJECT(d),document_signals[STATE_CHANGED_SIGNAL]);
}

void document_undo(Document *d)
{
     g_assert(document_can_undo(d));
     player_stop();
     fix_history(d);
     d->history_pos = d->history_pos->prev;
     get_state_from_history(d);			    
}

void document_redo(Document *d)
{
     g_assert(document_can_redo(d));
     player_stop();
     fix_history(d);
     d->history_pos = d->history_pos->next;
     get_state_from_history(d);
}

void document_set_status_bar(Document *d, StatusBar *bar)
{
     d->bar = bar;
}
