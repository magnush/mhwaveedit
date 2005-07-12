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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 */


#include <config.h>

#include "main.h"
#include "formatselector.h"
#include "inifile.h"
#include "gettext.h"

static void format_selector_class_init(GtkObjectClass *klass)
{
}

static void samplesize_changed(Combo *obj, gpointer user_data)
{
     int i;
     FormatSelector *fs = FORMAT_SELECTOR(user_data);
     i = combo_selected_index(obj);
     if (i<4) {
	  fs->format.type = DATAFORMAT_PCM;
	  fs->format.samplesize = i+1;
     } else {
	  fs->format.type = DATAFORMAT_FLOAT;
	  fs->format.samplesize = (i>4)?sizeof(double):sizeof(float);
     }
     gtk_widget_set_sensitive(GTK_WIDGET(fs->sign_combo),(i<4));
     gtk_widget_set_sensitive(GTK_WIDGET(fs->endian_combo),(i<4));
     if (i == 0)
	  combo_set_selection(fs->sign_combo,0);	  
     else if (i < 4)
	  combo_set_selection(fs->sign_combo,1);
}

static void signedness_changed(Combo *obj, gpointer user_data)
{
     FormatSelector *fs = FORMAT_SELECTOR(user_data);
     fs->format.sign = combo_selected_index(obj);
}

static void endianness_changed(Combo *obj, gpointer user_data)
{
     FormatSelector *fs = FORMAT_SELECTOR(user_data);
     fs->format.bigendian = combo_selected_index(obj);
}


static void format_selector_init(GtkWidget *widget)
{
     GtkWidget *a=widget,*b;
     FormatSelector *fs = FORMAT_SELECTOR(widget);
     GList *l;

     gtk_table_set_row_spacings(GTK_TABLE(widget),3);
     gtk_table_set_col_spacings(GTK_TABLE(widget),4);
     attach_label(_("Sample type: "),widget,1,0);
     b = combo_new();
     l = g_list_append(NULL,_("8 bit PCM"));
     l = g_list_append(l, _("16 bit PCM"));
     l = g_list_append(l, _("24 bit PCM"));
     l = g_list_append(l, _("32 bit PCM"));
     l = g_list_append(l, _("Floating-point, single"));
     l = g_list_append(l, _("Floating-point, double"));
     combo_set_items(COMBO(b),l,0);
     g_list_free(l);
     fs->samplesize_combo = COMBO(b);
     gtk_signal_connect(GTK_OBJECT(b),"selection_changed",
			GTK_SIGNAL_FUNC(samplesize_changed),fs);
     gtk_table_attach(GTK_TABLE(a),b,1,2,1,2,GTK_FILL,0,0,0);
     attach_label(_("Signedness: "),a,2,0);
     b = combo_new();     
     l = g_list_append(NULL, _("Unsigned"));
     l = g_list_append(l, _("Signed"));
     combo_set_items(COMBO(b),l,0);
     g_list_free(l);
     fs->sign_combo = COMBO(b);
     gtk_signal_connect(GTK_OBJECT(b),"selection_changed",
			GTK_SIGNAL_FUNC(signedness_changed),fs);
     gtk_table_attach(GTK_TABLE(a),b,1,2,2,3,GTK_FILL,0,0,0);
     attach_label(_("Endianness: "),a,3,0);
     b = combo_new();
     l = g_list_append(NULL,_("Little endian"));
     l = g_list_append(l,_("Big endian"));
     combo_set_items(COMBO(b),l,0);
     g_list_free(l);
     fs->endian_combo = COMBO(b);
     gtk_signal_connect(GTK_OBJECT(b),"selection_changed",
			GTK_SIGNAL_FUNC(endianness_changed),fs);
     gtk_table_attach(GTK_TABLE(a),b,1,2,3,4,GTK_FILL,0,0,0);
     
     fs->format.samplerate = 0;
     fs->format.channels = 0;
     fs->format.samplebytes = 0;
     fs->format.type = DATAFORMAT_PCM;
     fs->format.samplesize = 1;
     fs->format.sign = FALSE;
     fs->format.bigendian = FALSE;

     fs->channel_combo = NULL;
     fs->rate_box = NULL;

     gtk_widget_show_all(widget);
     gtk_widget_hide(widget);
}

GtkType format_selector_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "FormatSelector",
	       sizeof(FormatSelector),
	       sizeof(FormatSelectorClass),
	       (GtkClassInitFunc) format_selector_class_init,
	       (GtkObjectInitFunc) format_selector_init
	  };
	  id = gtk_type_unique(gtk_table_get_type(),&info);
     }
     return id;
}

/* Show channel and sample rate items */
static void format_selector_show_full(FormatSelector *fs)
{
     GList *l=NULL;
     GtkWidget *a=GTK_WIDGET(fs),*b,*c;
     guint i;
     attach_label(_("Channels: "),a,0,0);
     b = combo_new();
     for (i=1; i<9; i++) l=g_list_append(l,g_strdup(channel_format_name(i)));
     combo_set_items(COMBO(b),l,0);
     g_list_foreach(l,(GFunc)g_free,NULL);
     g_list_free(l);
     gtk_table_attach(GTK_TABLE(fs),b,1,2,0,1,GTK_FILL,0,0,5);
     fs->channel_combo = COMBO(b);
     gtk_widget_show(b);
     attach_label(_("Sample rate: "),a,4,0);
     b = gtk_alignment_new(0.0,0.5,0.0,1.0);
     gtk_table_attach(GTK_TABLE(fs),b,1,2,4,5,GTK_FILL,0,0,5);
     gtk_widget_show(b);
     c = intbox_new(22050);     
     fs->rate_box = INTBOX(c);
     gtk_container_add(GTK_CONTAINER(b),c);
     gtk_widget_show(c);
}

GtkWidget *format_selector_new(gboolean show_full)
{
     GtkWidget *fs = gtk_type_new(format_selector_get_type());
     if (show_full) format_selector_show_full(FORMAT_SELECTOR(fs));
     return fs;
}

void format_selector_set(FormatSelector *fs, Dataformat *fmt)
{
     if (fmt->type == DATAFORMAT_PCM) {
	  combo_set_selection(fs->samplesize_combo, fmt->samplesize-1);
	  combo_set_selection(fs->sign_combo, fmt->sign?1:0);
	  combo_set_selection(fs->endian_combo, fmt->bigendian?1:0);
     } else {
	  if (fmt->samplesize == sizeof(float))
	       combo_set_selection(fs->samplesize_combo, 4);
	  else
	       combo_set_selection(fs->samplesize_combo, 5);
     }     
     if (fs->channel_combo != NULL)
	  combo_set_selection(fs->channel_combo, fmt->channels-1);
     if (fs->rate_box != NULL)
	  intbox_set(fs->rate_box, fmt->samplerate);
}

void format_selector_get(FormatSelector *fs, Dataformat *result)
{
     result->type = fs->format.type  ;
     result->samplesize = fs->format.samplesize;
     result->sign = fs->format.sign;
     result->bigendian = fs->format.bigendian;
     if (fs->channel_combo != NULL) 
	  result->channels = combo_selected_index(fs->channel_combo)+1;
     if (fs->rate_box != NULL)
	  result->samplerate = fs->rate_box->val;
     result->samplebytes = result->samplesize * result->channels;
}

void format_selector_set_from_inifile(FormatSelector *fs, gchar *ini_prefix)
{
     Dataformat fmt;
     if (dataformat_get_from_inifile(ini_prefix,fs->channel_combo!=NULL,&fmt))
	  format_selector_set(fs,&fmt);
}

void format_selector_save_to_inifile(FormatSelector *fs, gchar *ini_prefix)
{
     dataformat_save_to_inifile(ini_prefix,&(fs->format),
				fs->channel_combo!=NULL);
}

gboolean format_selector_check(FormatSelector *fs)
{
     if (fs->rate_box != NULL)
	  return intbox_check_limit(fs->rate_box,1000,500000,_("sample rate"));
     else
	  return FALSE;
}
