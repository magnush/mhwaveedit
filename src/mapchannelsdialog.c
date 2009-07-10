/*
 * Copyright (C) 2002 2003 2004 2005 2006, Magnus Hjorth
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

#include "gettext.h"

#include "mainwindow.h"
#include "mapchannelsdialog.h"
#include "effectbrowser.h"
#include "main.h"

static GtkObjectClass *parent_class;
static gboolean *map_matrix = NULL;

static Chunk *map_channels_dialog_apply_proc(Chunk *chunk, StatusBar *bar,
					     gpointer user_data)
{
     MapChannelsDialog *mcd = MAP_CHANNELS_DIALOG(user_data);
     g_assert(mcd->channels_in == chunk->format.channels);
     return chunk_remap_channels(chunk,mcd->channels_out,map_matrix,
				 dither_editing,bar);
}

static gboolean map_channels_dialog_apply(EffectDialog *ed)
{
     guint i,j;
     gboolean b;

     Document *d = EFFECT_BROWSER(ed->eb)->dl->selected;
     MapChannelsDialog *mcd = MAP_CHANNELS_DIALOG(ed);

     g_assert(mcd->channels_in == d->chunk->format.channels);

     if (intbox_check(mcd->outchannels)) return TRUE;

     map_matrix = g_malloc(mcd->channels_in*mcd->channels_out*
				   sizeof(gboolean));
     for (i=0; i<mcd->channels_in; i++)
	  for (j=0; j<mcd->channels_out; j++)
	       map_matrix[i*mcd->channels_out + j] = 
		    gtk_toggle_button_get_active(mcd->channel_map[i*8+j]);
     
     b = document_apply_cb(d,map_channels_dialog_apply_proc,FALSE,mcd);

     g_free(map_matrix);
     map_matrix = NULL;

     return b;
}

static void map_channels_dialog_target_changed(EffectDialog *ed) 
{
     MapChannelsDialog *ccd = MAP_CHANNELS_DIALOG(ed);
     if (ccd->channels_in != EFFECT_BROWSER(ed->eb)->dl->format.channels)
	  effect_browser_invalidate_effect(EFFECT_BROWSER(ed->eb),
					   "mapchannels",'B');
}

static void numchannels_changed(Intbox *box, long int val, gpointer user_data)
{
     MapChannelsDialog *mcd = MAP_CHANNELS_DIALOG(user_data);
     int i,j;

     mcd->channels_out = val;

     for (i=0; i<8; i++) {

	  if (i < val)
	       gtk_widget_show(mcd->outnames[i]);
	  else
	       gtk_widget_hide(mcd->outnames[i]);

	  for (j=0; j<mcd->channels_in; j++) {
	       if (i < val)
		    gtk_widget_show(GTK_WIDGET(mcd->channel_map[j*8+i]));
	       else
		    gtk_widget_hide(GTK_WIDGET(mcd->channel_map[j*8+i]));
	  }
     }      
}

static void map_channels_setup(EffectDialog *ed)
{
    int i,j;
    GtkWidget *z,*a,*b,*c,*d;
    GtkLabel *l;
    guint channels_in = EFFECT_BROWSER(ed->eb)->dl->format.channels;
    MapChannelsDialog *mcd = MAP_CHANNELS_DIALOG(ed);

    mcd->channels_in = channels_in;
    mcd->channels_out = channels_in;
    mcd->outchannels = INTBOX(intbox_new(8));

    z = gtk_vbox_new(FALSE,5);
    gtk_box_pack_start(GTK_BOX(ed->input_area),z,FALSE,FALSE,0);
    
    a = gtk_label_new(_("This effect applies to the whole file, "
			"not just the selection"));
    gtk_container_add(GTK_CONTAINER(z),a);

    a = gtk_hbox_new(FALSE,3);
    gtk_container_add(GTK_CONTAINER(z),a);

    b = gtk_label_new(_("Output channels: "));
    gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
    b = intbox_create_scale(mcd->outchannels,1,8);
    gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
    b = GTK_WIDGET(mcd->outchannels);
    gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);

    a = gtk_frame_new(_("Map"));
    gtk_container_add(GTK_CONTAINER(z),a);


    b = gtk_table_new(2+channels_in,3,FALSE);
    gtk_container_add(GTK_CONTAINER(a),b);
    attach_label("x",b,1,1);
    c = gtk_label_new(_("Source"));
    gtk_table_attach(GTK_TABLE(b),c,0,1,2,2+channels_in,0,0,4,0);
    attach_label(_("Destination"),b,0,2);
    /*
    c = gtk_label_new(_("Destination"));
    gtk_misc_set_alignment(GTK_MISC(c),0.0,0.5);
    gtk_table_attach(GTK_TABLE(b),c,2,3,0,1,GTK_FILL,0,10,0);
    */
    
    c = gtk_table_new(1+channels_in,8,TRUE);
    gtk_table_attach(GTK_TABLE(b),c,2,3,1,2+channels_in,GTK_FILL,GTK_FILL,0,0);
    
    for (i=0; i<8; i++) {
	 l = attach_label(channel_name(i,8),c,0,i);
	 mcd->outnames[i] = GTK_WIDGET(l);
    }
    for (i=0; i<channels_in; i++) {
	 attach_label(channel_name(i,channels_in),b,2+i,1);
	 for (j=0; j<8; j++) {
	      d = gtk_check_button_new();
	      if (i == j)
		   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d),TRUE);
	      mcd->channel_map[i*8+j] = GTK_TOGGLE_BUTTON(d);
	      gtk_table_attach(GTK_TABLE(c),d,j,j+1,1+i,1+i+1,GTK_FILL,0,0,
			       0);
	 }
    }

    gtk_table_set_col_spacings(GTK_TABLE(c),4);

    gtk_widget_show_all(z);

    gtk_signal_connect(GTK_OBJECT(mcd->outchannels),"numchanged",
		       GTK_SIGNAL_FUNC(numchannels_changed),mcd);
    intbox_set(mcd->outchannels,mcd->channels_out);
    
}

static void map_channels_dialog_class_init(EffectDialogClass *klass)
{
    parent_class = gtk_type_class(effect_dialog_get_type());
    klass->apply = map_channels_dialog_apply;
    klass->setup = map_channels_setup;
    klass->target_changed = map_channels_dialog_target_changed;
}

static void map_channels_dialog_init(MapChannelsDialog *obj)
{
}

GtkType map_channels_dialog_get_type(void)
{
    static GtkType id = 0;
    if (!id) {
        GtkTypeInfo info = {
            "MapChannelsDialog",
            sizeof(MapChannelsDialog),
            sizeof(MapChannelsDialogClass),
            (GtkClassInitFunc)map_channels_dialog_class_init,
            (GtkObjectInitFunc)map_channels_dialog_init
        };
        id = gtk_type_unique(effect_dialog_get_type(),&info);
    }
    return id;
}
