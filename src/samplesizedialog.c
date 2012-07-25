/*
 * Copyright (C) 2002 2003 2004 2005 2010, Magnus Hjorth
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

#include "samplesizedialog.h"
#include "main.h"
#include "effectbrowser.h"
#include "gettext.h"
#include "um.h"

static Chunk *samplesize_apply_proc(Chunk *chunk, StatusBar *bar,
				    gpointer user_data)
{
     Dataformat df;
     SamplesizeDialog *ssd = SAMPLESIZE_DIALOG(user_data);
     memcpy(&(df),&(chunk->format),sizeof(Dataformat));
     format_selector_get(ssd->fs,&df);
     if (dataformat_samples_equal(&df,&(chunk->format))) {
	  user_info(_("File already has the selected sample format"));
	  return NULL;
     }
     if (ssd->gurumode) 
	  return chunk_clone_df(chunk,&df);
     else
	  return chunk_convert_sampletype(chunk,&df);
}


static gboolean samplesize_apply(EffectDialog *ed)
{
     gboolean b;
     b = document_apply_cb(EFFECT_BROWSER(ed->eb)->dl->selected,
			   samplesize_apply_proc,FALSE,ed);
     mainwindow_update_texts();
     return b;
}

static void samplesize_setup(EffectDialog *ed)
{
     Dataformat *f = &(EFFECT_BROWSER(ed->eb)->dl->format);
     format_selector_set(SAMPLESIZE_DIALOG(ed)->fs, f);
}

static void samplesize_dialog_class_init(EffectDialogClass *klass)
{
     klass->apply = samplesize_apply;
     klass->setup = samplesize_setup;
}

static void gurumode_toggle(GtkToggleButton *togglebutton, gpointer user_data)
{
     SamplesizeDialog *ssd = SAMPLESIZE_DIALOG(user_data);
     ssd->gurumode = gtk_toggle_button_get_active(togglebutton);
}

static void samplesize_dialog_init(SamplesizeDialog *ssd)
{
     GtkWidget *b,*c;
     GtkBox *ia = GTK_BOX(EFFECT_DIALOG(ssd)->input_area);

     ssd->gurumode = FALSE;

     ssd->fs = FORMAT_SELECTOR(format_selector_new(FALSE));

     b = gtk_vbox_new(FALSE,3);
     gtk_box_pack_start(ia,b,FALSE,FALSE,0);
     gtk_widget_show(b);
     c = gtk_label_new ( _("(This changes the sample type of "
			 "the entire file, not just the selection)") );
     gtk_label_set_justify(GTK_LABEL(c),GTK_JUSTIFY_LEFT);
     gtk_misc_set_alignment(GTK_MISC(c),0,0.5);
     gtk_label_set_line_wrap(GTK_LABEL(c),TRUE);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);

     c = GTK_WIDGET(ssd->fs);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);

     c=gtk_check_button_new_with_label(_("Don't actually change the data "
				       "(for repairing bad files etc)"));
     gtk_signal_connect(GTK_OBJECT(c),"toggled",GTK_SIGNAL_FUNC(gurumode_toggle),
			ssd);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);
}

GtkType samplesize_dialog_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "SamplesizeDialog",
	       sizeof(SamplesizeDialog),
	       sizeof(SamplesizeDialogClass),
	       (GtkClassInitFunc) samplesize_dialog_class_init,
	       (GtkObjectInitFunc) samplesize_dialog_init
	  };
	  id = gtk_type_unique(effect_dialog_get_type(),&info);
     }
     return id;
}
