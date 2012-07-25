/*
 * Copyright (C) 2003 2004 2010, Magnus Hjorth
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

#include <string.h>
#include "historybox.h"
#include "inifile.h"

#define HISTORY_BOX_MAXENTRIES 20

typedef struct {
     gchar *name;
     GList *entries;
} HistoryBoxHistory;

static GHashTable *history_hash = NULL; /* key=gchar* (malloc'd), value=GList* */

static void history_box_init(HistoryBox *box)
{
}

static void history_box_class_init(GtkObjectClass *klass)
{
}

static void load_history(HistoryBoxHistory *hist)
{
     gint i;
     gchar c[128],*d;
     for (i=1; i<=HISTORY_BOX_MAXENTRIES; i++) {
	  g_snprintf(c,sizeof(c),"history_%s_%d",hist->name,i);
	  d = inifile_get(c,NULL);
	  if (!d) break;
	  hist->entries = g_list_append(hist->entries,g_strdup(d));
     }
}

static void save_history(HistoryBoxHistory *hist)
{
     gint i;
     gchar c[128];
     GList *l;
     for (i=1,l=hist->entries; l!=NULL; i++,l=l->next) {
	  g_snprintf(c,sizeof(c),"history_%s_%d",hist->name,i);
	  inifile_set(c,l->data);
     }
     g_snprintf(c,sizeof(c),"history_%s_%d",hist->name,i);
     inifile_set(c,NULL);
     g_assert(i<=HISTORY_BOX_MAXENTRIES+1);
}

GtkType history_box_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "HistoryBox",
	       sizeof(HistoryBox),
	       sizeof(HistoryBoxClass),
	       (GtkClassInitFunc) history_box_class_init,
	       (GtkObjectInitFunc) history_box_init
	  };
	  id = gtk_type_unique(gtk_combo_get_type(),&info);
     }
     return id;
}

GtkWidget *history_box_new(gchar *historyname, gchar *value)
{
     HistoryBox *hb = gtk_type_new(history_box_get_type());
     history_box_set_history(hb, historyname);
     history_box_set_value(hb, value);
     return GTK_WIDGET(hb);
}

gchar *history_box_get_value(HistoryBox *box)
{
     return (gchar *)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(box)->entry));
}

void history_box_set_history(HistoryBox *box, gchar *historyname)
{
     HistoryBoxHistory *hist;
     if (!history_hash) history_hash=g_hash_table_new(g_str_hash,g_str_equal);
     hist = g_hash_table_lookup(history_hash, historyname);
     if (!hist) {
	  hist = g_malloc(sizeof(*hist));
	  hist->name = g_strdup(historyname);
	  hist->entries = NULL;
	  load_history(hist);
	  g_hash_table_insert(history_hash,hist->name,hist);
     }
     if (hist->entries != NULL)
	  gtk_combo_set_popdown_strings(GTK_COMBO(box),hist->entries);
     else 
	  gtk_list_clear_items(GTK_LIST(GTK_COMBO(box)->list),0,-1);     
     box->history = hist;
}

void history_box_set_value(HistoryBox *box, gchar *value)
{
     gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(box)->entry),value);
}

void history_box_rotate_history(HistoryBox *box)
{
     gchar *v;
     HistoryBoxHistory *hist=box->history;
     GList *l,*n;
     v = g_strdup(history_box_get_value(box));
     for (l=hist->entries; l!=NULL; l=n) {
	  n = l->next;
	  if (!strcmp(l->data,v)) {
	       g_free(l->data);
	       hist->entries = g_list_remove_link(hist->entries,l);
	       g_list_free(l);
	  }
     }
     hist->entries = g_list_prepend(hist->entries, v);
     l = g_list_nth(hist->entries, HISTORY_BOX_MAXENTRIES-1);
     if (l && l->next) {
	  g_assert(l->next->next == NULL);
	  g_free(l->next->data);
	  g_list_free_1(l->next);
	  l->next = NULL;
     }
     gtk_combo_set_popdown_strings(GTK_COMBO(box),hist->entries);
     save_history(hist);
}
