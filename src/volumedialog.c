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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


#include <config.h>

#include <math.h>
#include <gtk/gtk.h>
#include "mainwindow.h"
#include "effectdialog.h"
#include "effectbrowser.h"
#include "volumedialog.h"
#include "float_box.h"
#include "gettext.h"


static Chunk *volume_dialog_apply_proc(Chunk *chunk, StatusBar *bar, 
				       gpointer user_data)
{
     VolumeDialog *v = VOLUME_DIALOG(user_data);
     return chunk_volume_ramp(chunk,v->start_percent->val / 100.0,
			      v->end_percent->val / 100.0,dither_editing,bar);
}

static gboolean volume_dialog_apply(EffectDialog *ed)
{
     sample_t sv,ev;
     VolumeDialog *v = VOLUME_DIALOG(ed);
     if (floatbox_check(v->start_percent) || floatbox_check(v->end_percent))
	  return TRUE;
     sv = v->start_percent->val / 100.0;
     ev = v->end_percent->val / 100.0;
     if (sv == ev && sv == 1.0) return FALSE;
     return document_apply_cb(EFFECT_BROWSER(ed->eb)->dl->selected,
			      volume_dialog_apply_proc,TRUE,v);
}

static sample_t peak;

static gboolean findtop_proc(void *sample, gint sample_size, Chunk *chunk)
{
     sample_t d;
     d = fabs(* ((sample_t *)sample));
     if (d>peak) peak = d;
     return (d>=1.0);
}

static void findtop(VolumeDialog *v)
{
     gfloat f;
     Document *d = EFFECT_BROWSER(EFFECT_DIALOG(v)->eb)->dl->selected;
     peak = 0.0;
     if (d == NULL) return;
     document_parse(d,findtop_proc,FALSE,TRUE,_("Calculating peak volume..."));
     gdk_window_raise(GTK_WIDGET(EFFECT_DIALOG(v)->eb)->window);
     f = 100.0 * maximum_float_value(&(d->chunk->format)) / peak;
     if (f < 100.0) f = 100.0;
     floatbox_set(v->start_percent,f);
     floatbox_set(v->end_percent,f);
}

static void volume_dialog_init(VolumeDialog *v)
{
     GtkWidget *a,*b,*c;

     a = gtk_hbox_new(FALSE,3);
     gtk_box_pack_start(GTK_BOX(EFFECT_DIALOG(v)->input_area),a,FALSE,FALSE,0);
     b = gtk_table_new(2,3,FALSE);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     c = gtk_label_new (_("Start:"));     
     gtk_label_set_justify(GTK_LABEL(c),GTK_JUSTIFY_RIGHT);
     gtk_table_attach(GTK_TABLE(b),c,0,1,0,1,0,0,0,0);
     c = gtk_label_new (_("End:"));
     gtk_label_set_justify(GTK_LABEL(c),GTK_JUSTIFY_RIGHT);
     gtk_table_attach(GTK_TABLE(b),c,0,1,1,2,0,0,0,0);
     c = floatbox_new(100.0);
     v->start_percent = FLOATBOX(c);
     gtk_table_attach(GTK_TABLE(b),c,1,2,0,1,
		      GTK_EXPAND|GTK_SHRINK|GTK_FILL,0,3,0);
     gtk_widget_show(c);
     c = floatbox_new(100.0);
     v->end_percent = FLOATBOX(c);
     gtk_table_attach(GTK_TABLE(b), c, 1, 2, 1, 2, 
		      GTK_EXPAND|GTK_SHRINK|GTK_FILL,0,3,0);
     gtk_widget_show(c);
     c = gtk_label_new("%");
     gtk_table_attach(GTK_TABLE(b),c,2,3,0,1,0,0,0,0);
     c = gtk_label_new("%");
     gtk_table_attach(GTK_TABLE(b),c,2,3,1,2,0,0,0,0);
     b = gtk_alignment_new(0,1,0,0);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     c = gtk_button_new_with_label(_("Find top volume"));
     gtk_signal_connect_object(GTK_OBJECT(c),"clicked",
			       GTK_SIGNAL_FUNC(findtop),GTK_OBJECT(v));
     gtk_container_add(GTK_CONTAINER(b),c);
     gtk_widget_show_all(a);
}

static void volume_dialog_class_init(EffectDialogClass *klass)
{
     klass->apply = volume_dialog_apply;
}

GtkType volume_dialog_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "VolumeDialog",
	       sizeof(VolumeDialog),
	       sizeof(VolumeDialogClass),
	       (GtkClassInitFunc) volume_dialog_class_init,
	       (GtkObjectInitFunc) volume_dialog_init
	  };
	  id = gtk_type_unique(effect_dialog_get_type(),&info);
     }
     return id;
}
