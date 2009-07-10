/*
 * Copyright (C) 2002 2003 2004 2005, Magnus Hjorth
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

#include "mainwindow.h"
#include "combinechannelsdialog.h"
#include "effectbrowser.h"
#include "main.h"

static GtkObjectClass *parent_class;
static sample_t *combination_matrix;
static sample_t *samples = NULL;

static gboolean combine_channels_dialog_apply_proc(void *in, guint sample_size,
						   chunk_writeout_func out_func,
						   WriteoutID id, 
						   Dataformat *format)
{
     sample_t *s = (sample_t *)in, d;
     guint i,j,chn=format->channels;
     if (!samples)
	  samples = g_malloc(sample_size);
     for (i=0; i<chn; i++) {
	  d = 0.0;
	  for (j=0; j<chn; j++)
	       d += s[j] * combination_matrix[i*chn+j];
	  samples[i] = d;
     }
     return out_func(id,samples,sample_size);
}

static gboolean combine_channels_dialog_apply(EffectDialog *ed)
{
     guint i;
     gboolean b;
     Document *d = EFFECT_BROWSER(ed->eb)->dl->selected;
     CombineChannelsDialog *ccd = COMBINE_CHANNELS_DIALOG(ed);
     g_assert(ccd->channels == d->chunk->format.channels);
     for (i=0; i<ccd->channels*ccd->channels; i++)
	  if (floatbox_check(ccd->combination_matrix[i])) return TRUE;
     combination_matrix =g_malloc(ccd->channels*ccd->channels*sizeof(sample_t));
     for (i=0; i<ccd->channels*ccd->channels; i++)
	  combination_matrix[i] = ccd->combination_matrix[i]->val / 100.0;
     b = document_apply(d,combine_channels_dialog_apply_proc,NULL,
			CHUNK_FILTER_FULL,TRUE,NULL);
     g_free(samples);
     samples = NULL;
     g_free(combination_matrix);
     return b;
}

static void combine_channels_dialog_target_changed(EffectDialog *ed) 
{
     CombineChannelsDialog *ccd = COMBINE_CHANNELS_DIALOG(ed);
     if (ccd->channels != EFFECT_BROWSER(ed->eb)->dl->format.channels)
	  effect_browser_invalidate_effect(EFFECT_BROWSER(ed->eb),"combine",
					   'B');
}

static void combine_channels_setup(EffectDialog *ed)
{
    int i,j;
    GtkWidget *a,*b,*c;
    guint channels = EFFECT_BROWSER(ed->eb)->dl->format.channels;
    CombineChannelsDialog *ccd = COMBINE_CHANNELS_DIALOG(ed);
    ccd->channels = channels;
    ccd->combination_matrix = g_malloc(channels*channels*sizeof(Floatbox *));
    for (i=0; i<channels*channels; i++)
        ccd->combination_matrix[i] = 
        FLOATBOX(floatbox_new( (i%(channels+1)==0) ? 100.0 : 0.0 ));
    for (i=0; i<ccd->channels; i++) {
        a=gtk_frame_new(channel_name(i,channels));
	gtk_container_set_border_width(GTK_CONTAINER(a),3);
        gtk_box_pack_start(GTK_BOX(ed->input_area),a,FALSE,FALSE,0);
        gtk_widget_show(a);
        b = gtk_table_new(channels,3,FALSE);
        gtk_container_add(GTK_CONTAINER(a),b);
        gtk_widget_show(b);
        for (j=0; j<channels; j++) {
            c=gtk_label_new(channel_name(j,channels));
            gtk_table_attach(GTK_TABLE(b),c,0,1,j,j+1,GTK_FILL,0,3,3);
            gtk_widget_show(c);
            c = GTK_WIDGET(ccd->combination_matrix[i*channels+j]);
            gtk_table_attach(GTK_TABLE(b),c,1,2,j,j+1,GTK_FILL,0,3,3);
            gtk_widget_show(c);
            c = gtk_label_new("%");
            gtk_table_attach(GTK_TABLE(b),c,2,3,j,j+1,GTK_FILL,0,0,3);
            gtk_widget_show(c);
        }
    }
}

static void combine_channels_destroy(GtkObject *obj)
{
    CombineChannelsDialog *ccd = COMBINE_CHANNELS_DIALOG(obj);
    g_free(ccd->combination_matrix);
    ccd->combination_matrix = NULL;
    parent_class->destroy(obj);
}

static void combine_channels_dialog_class_init(EffectDialogClass *klass)
{
    parent_class = gtk_type_class(effect_dialog_get_type());
    klass->apply = combine_channels_dialog_apply;
    klass->setup = combine_channels_setup;
    klass->target_changed = combine_channels_dialog_target_changed;
    GTK_OBJECT_CLASS(klass)->destroy = combine_channels_destroy;
}

static void combine_channels_dialog_init(CombineChannelsDialog *obj)
{
}

GtkType combine_channels_dialog_get_type(void)
{
    static GtkType id = 0;
    if (!id) {
        GtkTypeInfo info = {
            "CombineChannelsDialog",
            sizeof(CombineChannelsDialog),
            sizeof(CombineChannelsDialogClass),
            (GtkClassInitFunc)combine_channels_dialog_class_init,
            (GtkObjectInitFunc)combine_channels_dialog_init
        };
        id = gtk_type_unique(effect_dialog_get_type(),&info);
    }
    return id;
}
