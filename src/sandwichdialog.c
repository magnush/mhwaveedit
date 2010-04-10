/*
 * Copyright (C) 2009, Magnus Hjorth
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

#include "sandwichdialog.h"
#include "gettext.h"
#include "effectbrowser.h"
#include "um.h"

static gboolean sandwich_dialog_apply(EffectDialog *ed)
{
     SandwichDialog *sd = SANDWICH_DIALOG(ed);
     Document *d1, *d2;
     Chunk *c1, *c2, *t, *r;
     off_t c1ap, c2ap, apo;
     Dataformat fmt;
     const gchar *c;

     d1 = EFFECT_BROWSER(ed->eb)->dl->selected;
     d2 = sd->docsel->selected;

     c1 = d1->chunk;
     c2 = d2->chunk;

     if (gtk_toggle_button_get_active(sd->align_begin)) {
	  c1ap = c2ap = 0;
     } else if (gtk_toggle_button_get_active(sd->align_end)) {
	  c1ap = c1->length-1;
	  c2ap = c2->length-1;
     } else if (gtk_toggle_button_get_active(sd->align_marker)) {
	  c = gtk_entry_get_text(sd->marker_entry);
	  c1ap = document_get_mark(d1,c);
	  c2ap = document_get_mark(d2,c);
	  if (c1ap == DOCUMENT_BAD_MARK || c2ap == DOCUMENT_BAD_MARK) {
	       user_error("Mark does not exist in both documents.");
	       return TRUE;
	  }
     } else {
	  c1ap = c2ap = 0; /* Silence compiler */
	  g_assert_not_reached();
     }

     t = NULL;
     if (!dataformat_samples_equal(&(c1->format),&(c2->format)) || 
	 c1->format.samplerate != c2->format.samplerate) {
	  memcpy(&fmt,&(c1->format),sizeof(Dataformat));
	  fmt.channels = c2->format.channels;
	  t = chunk_convert(c2, &fmt, dither_editing, d1->bar);
	  if (t == NULL) return TRUE;

	  if (t->format.samplerate != c2->format.samplerate) {
	       if (c2ap >= c2->length-1) c2ap = t->length-1;
	       else c2ap = (c2ap*t->format.samplerate)/(c2->format.samplerate);
	  }

	  c2 = t;
     }

     g_assert(dataformat_samples_equal(&(c1->format),&(c2->format)) &&
	      c1->format.samplerate == c2->format.samplerate);
     
     r = chunk_sandwich(c1,c2,c1ap,c2ap,&apo,dither_editing,d1->bar);
     
     if (t != NULL) gtk_object_sink(GTK_OBJECT(t));
     if (r == NULL) return TRUE;
     
     document_update(d1, r, 0, apo-c1ap);
     return FALSE;
}

static void sandwich_dialog_class_init(SandwichDialogClass *klass)
{     
     EFFECT_DIALOG_CLASS(klass)->apply = sandwich_dialog_apply;
}

static void marker_entry_changed(GtkEditable *editable, gpointer user_data)
{
     SandwichDialog *obj = SANDWICH_DIALOG(user_data);
     gtk_toggle_button_set_active(obj->align_marker,TRUE);
}

static void sandwich_dialog_init(SandwichDialog *obj)
{
     GtkWidget *a,*b,*c,*d;

     a = document_list_new(NULL);
     obj->docsel = DOCUMENT_LIST(a);
     a = gtk_radio_button_new_with_label(NULL,_("Align beginning of files"));
     obj->align_begin = GTK_TOGGLE_BUTTON(a);
     a = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(a),
						     _("Align end of files"));
     obj->align_end = GTK_TOGGLE_BUTTON(a);
     a = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(a),
						     _("Align at marker: "));  
     obj->align_marker = GTK_TOGGLE_BUTTON(a);
     gtk_toggle_button_set_active(obj->align_begin,TRUE);
     a = gtk_entry_new();
     obj->marker_entry = GTK_ENTRY(a);
     gtk_entry_set_width_chars(obj->marker_entry, 7);
     gtk_entry_set_text(obj->marker_entry, "0");
     gtk_signal_connect(GTK_OBJECT(obj->marker_entry),"changed",
			GTK_SIGNAL_FUNC(marker_entry_changed),obj);
     
     a = gtk_vbox_new(FALSE,3);
     b = gtk_hbox_new(FALSE,0);
     gtk_container_add(GTK_CONTAINER(a),b);
     c = gtk_label_new(_("Add channels from: "));
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     c = GTK_WIDGET(obj->docsel);
     gtk_box_pack_start(GTK_BOX(b),c,TRUE,TRUE,0);
     b = gtk_frame_new(_("Alignment"));
     gtk_container_add(GTK_CONTAINER(a),b);
     c = gtk_vbox_new(TRUE,0);          
     gtk_container_add(GTK_CONTAINER(b),c);
     gtk_container_set_border_width(GTK_CONTAINER(c),3);
     gtk_box_pack_start(GTK_BOX(c),GTK_WIDGET(obj->align_begin),FALSE,FALSE,0);
     gtk_box_pack_start(GTK_BOX(c),GTK_WIDGET(obj->align_end),FALSE,FALSE,0);
     d = gtk_hbox_new(FALSE,0);
     gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
     gtk_box_pack_start(GTK_BOX(d),GTK_WIDGET(obj->align_marker),FALSE,FALSE,0);
     gtk_box_pack_start(GTK_BOX(d),GTK_WIDGET(obj->marker_entry),FALSE,FALSE,0);

     gtk_box_pack_start(GTK_BOX(EFFECT_DIALOG(obj)->input_area),
			a,FALSE,FALSE,0);
     gtk_widget_show_all(a);
}

GtkType sandwich_dialog_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "SandwichDialog",
	       sizeof(SandwichDialog),
	       sizeof(SandwichDialogClass),
	       (GtkClassInitFunc) sandwich_dialog_class_init,
	       (GtkObjectInitFunc) sandwich_dialog_init
	  };
	  id = gtk_type_unique(effect_dialog_get_type(),&info);
     }
     return id;
}
