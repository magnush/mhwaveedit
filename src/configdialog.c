/*
 * Copyright (C) 2002 2003 2004 2005 2008 2009 2010 2011 2012, Magnus Hjorth
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
#include <gdk/gdkkeysyms.h>

#include <math.h>

#include "sound.h"
#include "inifile.h"
#include "main.h"
#include "configdialog.h"
#include "um.h"
#include "mainwindow.h"
#include "tempfile.h"
#include "player.h"
#include "rateconv.h"
#include "gettext.h"
#include "filetypes.h"

static GtkObjectClass *parent_class;

static void config_dialog_destroy(GtkObject *obj)
{
     ConfigDialog *cd = CONFIG_DIALOG(obj);
     cd->selected_tempdir = NULL;
     parent_class->destroy(obj);
}

static void config_dialog_class_init(GtkObjectClass *klass)
{
     parent_class = gtk_type_class(gtk_window_get_type());
     klass->destroy = config_dialog_destroy;
}

static void config_dialog_ok(GtkButton *button, gpointer user_data)
{
    gchar *c;
    gboolean b = FALSE;
    GList *la,*lb;
    GtkWidget *w;
    ConfigDialog *cd = CONFIG_DIALOG(user_data);
    if (intbox_check(cd->sound_buffer_size) || 
        intbox_check(cd->disk_threshold) ||
	intbox_check(cd->view_quality) ||
	intbox_check_limit(cd->recent_files,0,MAINWINDOW_RECENT_MAX,
			   _("number of recent files")) ||
        intbox_check_limit(cd->vzoom_max,1,9999, _("maximum vertical zoom")) ||
	format_selector_check(cd->fallback_format))
        return;
    /* c = (gchar *)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(cd->sound_driver)->entry)); */
    c = sound_driver_id_from_index(combo_selected_index(cd->sound_driver));
    g_assert(c != NULL);

    if (gtk_toggle_button_get_active(cd->driver_autodetect))
	 inifile_set(INI_SETTING_SOUNDDRIVER,"auto");
    else
	 b = inifile_set(INI_SETTING_SOUNDDRIVER,c) || b;

    b = inifile_set_guint32(INI_SETTING_SOUNDBUFSIZE,
			    cd->sound_buffer_size->val) || b;
    if (b) user_info(_("Some of the settings you have changed will not be "
		     "activated until you restart the program"));

    inifile_set_guint32(INI_SETTING_REALMAX,cd->disk_threshold->val*1024);
    inifile_set_guint32(INI_SETTING_VIEW_QUALITY,
			cd->view_quality->val);
    inifile_set_gboolean(INI_SETTING_TIMESCALE,
			 gtk_toggle_button_get_active(cd->time_scale_default));
    inifile_set_gboolean(INI_SETTING_VZOOM,
			 gtk_toggle_button_get_active(cd->vzoom_default));
    inifile_set_gboolean(INI_SETTING_HZOOM,
			 gtk_toggle_button_get_active(cd->hzoom_default));
    inifile_set_gboolean(INI_SETTING_SPEED,
			 gtk_toggle_button_get_active(cd->speed_default));
    inifile_set_gboolean(INI_SETTING_SLABELS,
			 gtk_toggle_button_get_active(cd->labels_default));
    inifile_set_gboolean(INI_SETTING_BUFPOS,
			 gtk_toggle_button_get_active(cd->bufpos_default));
    inifile_set(INI_SETTING_MIXER,
		(char *)gtk_entry_get_text(cd->mixer_utility));
    inifile_set_gboolean("useGeometry",
			 gtk_toggle_button_get_active(cd->remember_geometry));
    inifile_set_gboolean("drawImprove",
			 gtk_toggle_button_get_active(cd->improve));
    inifile_set_gboolean("mainwinFront",
			 gtk_toggle_button_get_active(cd->mainwin_front));

    default_time_mode = combo_selected_index(cd->time_display);
    inifile_set_guint32(INI_SETTING_TIME_DISPLAY,default_time_mode);

    default_time_mode = combo_selected_index(cd->time_display);
    inifile_set_guint32(INI_SETTING_TIME_DISPLAY,default_time_mode);

    default_timescale_mode = combo_selected_index(cd->time_display_timescale);
    inifile_set_guint32(INI_SETTING_TIME_DISPLAY_SCALE,default_timescale_mode);

    sound_lock_driver = gtk_toggle_button_get_active(cd->sound_lock);
    inifile_set_gboolean("soundLock",sound_lock_driver);

    status_bar_roll_cursor = gtk_toggle_button_get_active(cd->roll_cursor);
    inifile_set_gboolean("rollCursor",status_bar_roll_cursor);

    output_byteswap_flag = gtk_toggle_button_get_active(cd->output_bswap);
    inifile_set_gboolean("outputByteswap",output_byteswap_flag);

    output_stereo_flag = gtk_toggle_button_get_active(cd->output_stereo);
    inifile_set_gboolean("outputStereo",output_stereo_flag);

    view_follow_strict_flag = 
	 gtk_toggle_button_get_active(cd->center_cursor);
    inifile_set_gboolean("centerCursor",view_follow_strict_flag);

    autoplay_mark_flag = gtk_toggle_button_get_active(cd->mark_autoplay);
    inifile_set_gboolean("autoPlayMark",autoplay_mark_flag);

    la = gtk_container_children(GTK_CONTAINER(cd->tempdirs));
    lb = NULL;
    for (; la != NULL; la = la->next) {
	 w = GTK_WIDGET(la->data);
	 w = GTK_BIN(w)->child;
	 gtk_label_get(GTK_LABEL(w),&c);
	 lb = g_list_append(lb,c);
    }
    set_temp_directories(lb);
    g_list_free(lb);

    inifile_set_guint32("recentFiles",cd->recent_files->val);

    inifile_set_guint32("vzoomMax",cd->vzoom_max->val);

    format_selector_get(cd->fallback_format,&player_fallback_format);
    format_selector_save_to_inifile(cd->fallback_format,"playerFallback");

    chunk_filter_use_floating_tempfiles = 
	 gtk_toggle_button_get_active(cd->floating_tempfiles);
    inifile_set_gboolean("tempfilesFP",chunk_filter_use_floating_tempfiles);   

    varispeed_reset_flag = 
	 gtk_toggle_button_get_active(cd->varispeed_autoreset);
    inifile_set_gboolean("speedReset",varispeed_reset_flag);

    varispeed_smooth_flag = 
	 !gtk_toggle_button_get_active(cd->varispeed_fast);
    inifile_set_gboolean("speedSmooth",varispeed_smooth_flag);

    b = gtk_toggle_button_get_active(cd->varispeed_enable);
    inifile_set_gboolean("varispeed",b);
    mainwindow_set_speed_sensitive(b);
    inifile_set_guint32("varispeedConv",
			combo_selected_index(cd->varispeed_method));
    inifile_set_guint32("speedConv",combo_selected_index(cd->speed_method));

    dither_editing = gtk_toggle_button_get_active(cd->dither_editing);
    inifile_set_guint32("ditherEditing",
			dither_editing?DITHER_MINIMAL:DITHER_NONE);
    dither_playback = gtk_toggle_button_get_active(cd->dither_playback);
    inifile_set_guint32("ditherPlayback",
			dither_playback?DITHER_MINIMAL:DITHER_NONE);

    if (sndfile_ogg_supported())
	 inifile_set_guint32("sndfileOggMode",
			     combo_selected_index(cd->oggmode));

    sample_convert_mode = combo_selected_index(cd->convmode);
    inifile_set_guint32("sampleMode",sample_convert_mode);

    gtk_widget_destroy(GTK_WIDGET(cd));

    mainwindow_update_texts();
}

static void sound_settings_click(GtkButton *button, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     sound_driver_show_preferences(
	  sound_driver_id_from_index(combo_selected_index(cd->sound_driver)));
}

static void sound_driver_changed(Combo *combo, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     gchar *c;
     c = combo_selected_string(combo);
     gtk_widget_set_sensitive(GTK_WIDGET(cd->sound_driver_prefs),
			      sound_driver_has_preferences(
				   sound_driver_id_from_name(c)));
     g_free(c);
}

static gboolean color_expose(GtkWidget *widget, GdkEventExpose *event,
			     gpointer user_data)
{
     GdkColor *c = (GdkColor *)user_data;
     GdkGC *gc;
     gc = gdk_gc_new(widget->window);     
     gdk_gc_set_foreground(gc,c);
     gdk_draw_rectangle(widget->window,gc,TRUE,
			event->area.x,event->area.y,event->area.width,
			event->area.height);
     gdk_gc_unref(gc);
     return TRUE;
}


static void color_select(GtkList *list, GtkWidget *widget,
			 gpointer user_data)
{
     GtkColorSelection *cs = GTK_COLOR_SELECTION(user_data);
     GdkColor *c = (GdkColor *)gtk_object_get_user_data(GTK_OBJECT(widget));
     gdouble color[4];
     
     gtk_object_set_user_data(GTK_OBJECT(cs),widget);
     color[0] = ((gdouble)(c->red))/65535.0;
     color[1] = ((gdouble)(c->green))/65535.0;
     color[2] = ((gdouble)(c->blue))/65535.0;
     color[3] = 1.0;
     gtk_color_selection_set_color(cs,color);
#if GTK_MAJOR_VERSION == 1
gtk_color_selection_set_color(cs,color);
#endif
#if GTK_MAJOR_VERSION == 2
gtk_color_selection_set_previous_color(cs, c);
#endif
}

static void color_set(GtkColorSelection *selection, gpointer user_data)
{
     gdouble color[4];
     GtkWidget *widget;
     GdkColor *c;
     gtk_color_selection_get_color(selection,color);
     widget = GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(selection)));
     c = gtk_object_get_user_data(GTK_OBJECT(widget));
     c->red = (guint)(color[0]*65535.0);
     c->green = (guint)(color[1]*65535.0);
     c->blue = (guint)(color[2]*65535.0);
     gdk_colormap_alloc_color(gdk_colormap_get_system(),c,FALSE,TRUE);     
     gtk_widget_queue_draw(widget);
}

static void color_apply(GtkButton *button, gpointer user_data)
{
     GdkColor *c = (GdkColor *)user_data;
     set_custom_colors(c);
}

static void colors_click(GtkButton *button, gpointer user_data)
{
     GtkWidget *a,*b,*c,*d,*e,*f,*g;
     GtkWidget *cs;
     gint i,key;
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     GdkColor *ctable;
     GtkAccelGroup* ag;
     ag = gtk_accel_group_new();
          
     ctable = g_malloc((LAST_COLOR-FIRST_CUSTOM_COLOR)*sizeof(GdkColor));
     for (i=FIRST_CUSTOM_COLOR; i<LAST_COLOR; i++)
	  memcpy(&ctable[i-FIRST_CUSTOM_COLOR],get_color(i),sizeof(GdkColor));

     cs = gtk_color_selection_new();
#if GTK_MAJOR_VERSION == 2
     gtk_color_selection_set_has_opacity_control (GTK_COLOR_SELECTION(cs), 
						  FALSE);
     gtk_color_selection_set_has_palette (GTK_COLOR_SELECTION(cs), TRUE);
#endif
     gtk_signal_connect(GTK_OBJECT(cs),"color_changed",
			GTK_SIGNAL_FUNC(color_set),NULL);

     a = gtk_window_new(GTK_WINDOW_DIALOG);
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_window_set_transient_for(GTK_WINDOW(a),GTK_WINDOW(cd));
     gtk_window_set_title(GTK_WINDOW(a),_("Colors"));
     gtk_window_set_policy(GTK_WINDOW(a),FALSE,FALSE,TRUE);
     gtk_signal_connect_object(GTK_OBJECT(a),"delete_event",
			       GTK_SIGNAL_FUNC(g_free),(GtkObject *)ctable);
     b = gtk_vbox_new(FALSE,5);
     gtk_container_set_border_width(GTK_CONTAINER(b),5);
     gtk_container_add(GTK_CONTAINER(a),b);
     c = gtk_hbox_new(FALSE,10);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     d = gtk_list_new();
     gtk_signal_connect(GTK_OBJECT(d),"select_child",
			GTK_SIGNAL_FUNC(color_select),cs);
     gtk_list_set_selection_mode(GTK_LIST(d),GTK_SELECTION_BROWSE);
     gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
     for (i=FIRST_CUSTOM_COLOR; i<LAST_COLOR; i++) {
	  e = gtk_list_item_new();
	  gtk_object_set_user_data(GTK_OBJECT(e),
				   (gpointer)(&ctable[i-FIRST_CUSTOM_COLOR]));
	  gtk_container_add(GTK_CONTAINER(d),e);
	  f = gtk_hbox_new(FALSE,3);
	  gtk_container_set_border_width(GTK_CONTAINER(f),3);
	  gtk_container_add(GTK_CONTAINER(e),f);
	  g = gtk_drawing_area_new();
	  gtk_drawing_area_size(GTK_DRAWING_AREA(g),20,20);
	  gtk_signal_connect(GTK_OBJECT(g),"expose_event",
			     GTK_SIGNAL_FUNC(color_expose),
			     (gpointer)(&ctable[i-FIRST_CUSTOM_COLOR]));
	  gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
	  g = gtk_label_new(_(color_names[i]));
	  gtk_misc_set_alignment(GTK_MISC(g),0.0,1.0);
	  gtk_box_pack_start(GTK_BOX(f),g,TRUE,TRUE,0);
     }
     d = cs;
     gtk_box_pack_start(GTK_BOX(c),d,TRUE,TRUE,0);
     c = gtk_hbutton_box_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     d = gtk_button_new_with_label("");
     key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(d)->child),_("_Preview"));
     gtk_widget_add_accelerator (d, "clicked", ag, key, GDK_MOD1_MASK,
				 (GtkAccelFlags) 0);
     gtk_signal_connect(GTK_OBJECT(d),"clicked",GTK_SIGNAL_FUNC(color_apply),
			ctable);     
     gtk_container_add(GTK_CONTAINER(c),d);
     d = gtk_button_new_with_label("");
     key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(d)->child),_("_OK"));
     gtk_widget_add_accelerator (d, "clicked", ag, key, GDK_MOD1_MASK,
				 (GtkAccelFlags) 0);
     gtk_signal_connect(GTK_OBJECT(d),"clicked",GTK_SIGNAL_FUNC(color_apply),
			ctable);
     gtk_signal_connect(GTK_OBJECT(d),"clicked",GTK_SIGNAL_FUNC(save_colors),
			NULL);
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       (GtkObject *)a);
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(g_free),(GtkObject *)ctable);
     gtk_container_add(GTK_CONTAINER(c),d);
     GTK_WIDGET_SET_FLAGS(d,GTK_CAN_DEFAULT);
     gtk_widget_grab_default(d);

     d = gtk_button_new_with_label("");
     key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(d)->child),_("_Cancel"));
     gtk_widget_add_accelerator (d, "clicked", ag, key, GDK_MOD1_MASK,
				 (GtkAccelFlags) 0);
     gtk_widget_add_accelerator (d, "clicked", ag, GDK_Escape, 0,
				 (GtkAccelFlags) 0);
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(set_custom_colors),
			       (GtkObject *)NULL);
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(g_free),(GtkObject *)ctable);
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       (GtkObject *)a);
     gtk_container_add(GTK_CONTAINER(c),d);

     gtk_widget_show_all(a);
     gtk_window_add_accel_group(GTK_WINDOW (a), ag);
}

static void tempdir_add_click(GtkButton *button, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     GtkWidget *w;
     gchar *ch;
     ch = (gchar *)gtk_entry_get_text(cd->tempdir_add_entry);
     if (ch[0] == 0) return;
     w = gtk_list_item_new_with_label(ch);
     gtk_container_add(GTK_CONTAINER(cd->tempdirs),w);
     gtk_widget_show(w);
     gtk_entry_set_text(cd->tempdir_add_entry,"");
}

static void tempdir_remove_click(GtkButton *button, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     gtk_container_remove(GTK_CONTAINER(cd->tempdirs),cd->selected_tempdir);
}

static void tempdir_up_click(GtkButton *button, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     GtkWidget *w;
     GList *l;
     gint pos;
     w = cd->selected_tempdir;
     pos = gtk_list_child_position(cd->tempdirs,w);
     if (pos == 0) return;
     gtk_object_ref(GTK_OBJECT(w));
     gtk_container_remove(GTK_CONTAINER(cd->tempdirs),w);
     l = g_list_append(NULL,w);
     gtk_list_insert_items(cd->tempdirs,l,pos-1);
     gtk_list_toggle_row(cd->tempdirs,w);
     gtk_object_unref(GTK_OBJECT(w));
}

static void tempdir_down_click(GtkButton *button, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     GtkWidget *w;
     GList *l;
     gint pos;
     w = cd->selected_tempdir;
     pos = gtk_list_child_position(cd->tempdirs,w);
     /* if (pos == 0) return; */
     gtk_object_ref(GTK_OBJECT(w));
     gtk_container_remove(GTK_CONTAINER(cd->tempdirs),w);
     l = g_list_append(NULL,w);
     gtk_list_insert_items(cd->tempdirs,l,pos+1);
     gtk_list_toggle_row(cd->tempdirs,w);
     gtk_object_unref(GTK_OBJECT(w));
}

static void tempdir_browse_click(GtkButton *button, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     gchar *c,*d;
     c = (gchar *)gtk_entry_get_text(cd->tempdir_add_entry);
     d = get_directory(c,_("Browse directory"));
     if (d != NULL) {
	  gtk_entry_set_text(cd->tempdir_add_entry,d);
	  g_free(d);
     }
}


static void tempdir_select(GtkList *list, GtkWidget *widget, 
			   gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     cd->selected_tempdir = widget;
     gtk_widget_set_sensitive(GTK_WIDGET(cd->tempdir_remove),TRUE);
     gtk_widget_set_sensitive(GTK_WIDGET(cd->tempdir_up),TRUE);
     gtk_widget_set_sensitive(GTK_WIDGET(cd->tempdir_down),TRUE);
}

static void tempdir_unselect(GtkList *list, GtkWidget *widget, 
			     gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     if (cd->selected_tempdir == widget) {
	  cd->selected_tempdir = NULL;
	  gtk_widget_set_sensitive(GTK_WIDGET(cd->tempdir_remove),FALSE);
	  gtk_widget_set_sensitive(GTK_WIDGET(cd->tempdir_up),FALSE);
	  gtk_widget_set_sensitive(GTK_WIDGET(cd->tempdir_down),FALSE);
     }
}

static void driver_autodetect_toggled(GtkToggleButton *button, 
				      gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     gboolean b;
     b = gtk_toggle_button_get_active(button);
     if (b) combo_set_selection(cd->sound_driver,sound_driver_index());
     gtk_widget_set_sensitive(GTK_WIDGET(cd->sound_driver), !b);
}

static void config_dialog_init(ConfigDialog *cd)
{
    GtkWidget *w,*a,*b,*c,*d,*e,*f,*g,*h;
    GList *l;
    GtkAccelGroup *ag;
    guint key,i,j;
    gchar *ch;

    ag = gtk_accel_group_new();

    gtk_window_set_title(GTK_WINDOW(cd),_("Preferences"));
    gtk_window_set_modal(GTK_WINDOW(cd),TRUE);
    gtk_window_set_position(GTK_WINDOW (cd), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(cd),10,400);
    gtk_container_set_border_width(GTK_CONTAINER(cd),5);

    /* Create the main widgets */

    w = intbox_new( inifile_get_guint32( INI_SETTING_REALMAX, 
					 INI_SETTING_REALMAX_DEFAULT )/1024);
    cd->disk_threshold = INTBOX(w);

    w = intbox_new(inifile_get_guint32(INI_SETTING_SOUNDBUFSIZE,
                                       INI_SETTING_SOUNDBUFSIZE_DEFAULT));
    cd->sound_buffer_size = INTBOX(w);

    w = combo_new();
    cd->sound_driver = COMBO(w);
    l = sound_driver_valid_names();
    combo_set_items(cd->sound_driver,l,sound_driver_index());
    gtk_signal_connect(GTK_OBJECT(cd->sound_driver),"selection_changed",
		       GTK_SIGNAL_FUNC(sound_driver_changed),cd);    

    i = rateconv_driver_count(TRUE);
    for (l=NULL,j=0; j<i; j++)
	 l = g_list_append(l,(gpointer)rateconv_driver_name(TRUE,j));

    w = combo_new();
    cd->varispeed_method = COMBO(w);
    j = inifile_get_guint32("varispeedConv",i-1);
    if (j >= i) j = i-1;
    combo_set_items(cd->varispeed_method, l, j);

    g_list_free(l);

    i = rateconv_driver_count(FALSE);
    for (l=NULL,j=0; j<i; j++)
	 l = g_list_append(l,(gpointer)rateconv_driver_name(FALSE,j));

    w = combo_new();
    cd->speed_method = COMBO(w);
    j = inifile_get_guint32("speedConv",0);
    if (j >= i) j = 0;
    combo_set_items(cd->speed_method, l, j);

    g_list_free(l);

    w = gtk_entry_new();
    cd->mixer_utility = GTK_ENTRY(w);
    gtk_entry_set_text(cd->mixer_utility,inifile_get(INI_SETTING_MIXER,
						     INI_SETTING_MIXER_DEFAULT));


    w = intbox_new(inifile_get_guint32(INI_SETTING_VIEW_QUALITY,
				       INI_SETTING_VIEW_QUALITY_DEFAULT));
    cd->view_quality = INTBOX(w);

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child), 
				_("Show _time scale by default"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    cd->time_scale_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->time_scale_default,inifile_get_gboolean(
				      INI_SETTING_TIMESCALE, 
				      INI_SETTING_TIMESCALE_DEFAULT));

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child), 
				_("Show _horizontal zoom slider by default"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    cd->hzoom_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->hzoom_default,inifile_get_gboolean(
				      INI_SETTING_HZOOM, 
				      INI_SETTING_HZOOM_DEFAULT));

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child), 
				_("Show _vertical zoom slider by default"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    cd->vzoom_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->vzoom_default,inifile_get_gboolean(
				      INI_SETTING_VZOOM, 
				      INI_SETTING_VZOOM_DEFAULT));

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child), 
				_("Show _speed slider by default"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    cd->speed_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->speed_default,inifile_get_gboolean(
				      INI_SETTING_SPEED, 
				      INI_SETTING_SPEED_DEFAULT));

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child),
				_("Show slider l_abels by default"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK,
				(GtkAccelFlags) 0);
    cd->labels_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->labels_default,
				 inifile_get_gboolean(INI_SETTING_SLABELS,
						      INI_SETTING_SLABELS_DEFAULT));

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child),
				_("Show playback buffer positio_n by default"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK,
				(GtkAccelFlags) 0);
    cd->bufpos_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->bufpos_default,
				 inifile_get_gboolean(INI_SETTING_BUFPOS,
						      INI_SETTING_BUFPOS_DEFAULT));

    w = gtk_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child), _("_Settings"));
    gtk_widget_add_accelerator(w, "clicked", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_signal_connect(GTK_OBJECT(w),"clicked",
		       GTK_SIGNAL_FUNC(sound_settings_click),cd);
    gtk_widget_set_sensitive(w,sound_driver_has_preferences(NULL));
    cd->sound_driver_prefs = GTK_BUTTON(w);

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child), 
				_("_Keep sound driver opened (to avoid start/stop"
				" clicks)"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    cd->sound_lock = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->sound_lock,sound_lock_driver);

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child),
				_("_Byte-swap output (try this if playback "
				"sounds horrible)"));
    gtk_widget_add_accelerator(w,"clicked",ag,key,GDK_MOD1_MASK,
			       (GtkAccelFlags)0);
    cd->output_bswap = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->output_bswap,output_byteswap_flag);

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child),
				_("Play _mono files as stereo"));
    gtk_widget_add_accelerator(w,"clicked",ag,key,GDK_MOD1_MASK,
			       (GtkAccelFlags)0);
    cd->output_stereo = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->output_stereo,output_stereo_flag);

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child), 
				_("_Update cursor information while playing"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    cd->roll_cursor = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->roll_cursor,status_bar_roll_cursor);

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child), 
				_("_Keep cursor in center of view when "
				"following playback"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    cd->center_cursor = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->center_cursor,
				 view_follow_strict_flag);

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child),
				_("_Auto-start playback when jumping to mark"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK,
				(GtkAccelFlags) 0);
    cd->mark_autoplay = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->mark_autoplay,autoplay_mark_flag);

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child),
				_("Enable _variable speed playback"));
    gtk_widget_add_accelerator (w,"clicked", ag, key, GDK_MOD1_MASK,
				(GtkAccelFlags) 0);
    cd->varispeed_enable = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->varispeed_enable,
				 inifile_get_gboolean("varispeed",TRUE));

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child),
				_("Auto-_reset speed"));
    gtk_widget_add_accelerator(w,"clicked",ag,key,GDK_MOD1_MASK,0);
    cd->varispeed_autoreset = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->varispeed_autoreset,varispeed_reset_flag);
    
    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child),
				_("Use fast and noisy method"));
    gtk_widget_add_accelerator(w,"clicked",ag,key,GDK_MOD1_MASK,0);
    cd->varispeed_fast = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->varispeed_fast,!varispeed_smooth_flag);
    
    w = combo_new();
    cd->time_display = COMBO(w);
    l = NULL;
    l = g_list_append(l,_("(H')MM:SS.t"));
    l = g_list_append(l,_("(H')MM:SS.mmmm"));
    l = g_list_append(l,translate_strip(N_("TimeDisplay|Samples")));
    l = g_list_append(l,_("Time Code 24fps"));
    l = g_list_append(l,_("Time Code 25fps (PAL)"));
    l = g_list_append(l,_("Time Code 29.97fps (NTSC)"));
    l = g_list_append(l,_("Time Code 30fps"));
    i = inifile_get_guint32(INI_SETTING_TIME_DISPLAY,0);
    combo_set_items(cd->time_display,l,i);

    w = combo_new();
    cd->time_display_timescale = COMBO(w);
    i = inifile_get_guint32(INI_SETTING_TIME_DISPLAY_SCALE,i);
    combo_set_items(cd->time_display_timescale,l,i);

    g_list_free(l);

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child), 
				_("_Remember window sizes/positions"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    cd->remember_geometry = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->remember_geometry,
				 inifile_get_gboolean("useGeometry",FALSE));

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child), 
				_("_Draw waveform a second time with improved "
				"quality"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    cd->improve = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->improve,
				 inifile_get_gboolean("drawImprove",TRUE));


    w = gtk_list_new();
    cd->tempdirs = GTK_LIST(w);
    gtk_list_set_selection_mode(cd->tempdirs,GTK_SELECTION_SINGLE);
    for (i=0; 1; i++) {
	 ch = get_temp_directory(i);
	 if (ch == NULL) break;
	 a = gtk_list_item_new_with_label(ch);
	 gtk_container_add(GTK_CONTAINER(w),a);
    }
    gtk_signal_connect(GTK_OBJECT(w),"select_child",
		       GTK_SIGNAL_FUNC(tempdir_select),cd);
    gtk_signal_connect(GTK_OBJECT(w),"unselect_child",
		       GTK_SIGNAL_FUNC(tempdir_unselect),cd);
    cd->selected_tempdir = NULL;
    

    w = gtk_entry_new();
    cd->tempdir_add_entry = GTK_ENTRY(w);


    w = gtk_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child), _("_Remove"));
    gtk_widget_add_accelerator(w, "clicked", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_signal_connect(GTK_OBJECT(w),"clicked",
		       GTK_SIGNAL_FUNC(tempdir_remove_click),cd);
    gtk_widget_set_sensitive(w,FALSE);
    cd->tempdir_remove = GTK_BUTTON(w);
    w = gtk_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child), _("_Add"));
    gtk_widget_add_accelerator(w, "clicked", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_signal_connect(GTK_OBJECT(w),"clicked",
		       GTK_SIGNAL_FUNC(tempdir_add_click),cd);
    cd->tempdir_add = GTK_BUTTON(w);
    w = gtk_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child), _("_Browse..."));
    gtk_widget_add_accelerator(w, "clicked", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_signal_connect(GTK_OBJECT(w),"clicked",
		       GTK_SIGNAL_FUNC(tempdir_browse_click),cd);
    cd->tempdir_browse = GTK_BUTTON(w);
    w = gtk_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child), _("_Up"));
    gtk_widget_add_accelerator(w, "clicked", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_signal_connect(GTK_OBJECT(w),"clicked",
		       GTK_SIGNAL_FUNC(tempdir_up_click),cd);
    gtk_widget_set_sensitive(w,FALSE);
    cd->tempdir_up = GTK_BUTTON(w);
    w = gtk_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child), _("_Down"));
    gtk_widget_add_accelerator(w, "clicked", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_signal_connect(GTK_OBJECT(w),"clicked",
		       GTK_SIGNAL_FUNC(tempdir_down_click),cd);
    gtk_widget_set_sensitive(w,FALSE);
    cd->tempdir_down = GTK_BUTTON(w);

    w = intbox_new(inifile_get_guint32("recentFiles",4));
    cd->recent_files = INTBOX(w);

    w = intbox_new(inifile_get_guint32("vzoomMax",100));
    cd->vzoom_max = INTBOX(w);
    
    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child), 
				_("Keep main _window in front after "
				"applying effect"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    cd->mainwin_front = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->mainwin_front,
				 inifile_get_gboolean("mainwinFront",TRUE));


    w = format_selector_new(TRUE);
    format_selector_set(FORMAT_SELECTOR(w),&player_fallback_format);
    cd->fallback_format = FORMAT_SELECTOR(w);


    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (w)->child), 
				_("_Use floating-point temporary files"));
    gtk_widget_add_accelerator (w, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    cd->floating_tempfiles = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->floating_tempfiles,
				 chunk_filter_use_floating_tempfiles);

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child),
				_("Enable _dithering for editing"));
    gtk_widget_add_accelerator(w,"clicked",ag,key,GDK_MOD1_MASK,
			       (GtkAccelFlags)0);
    cd->dither_editing = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->dither_editing,dither_editing);
    
    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child),
				_("Enable dithering for _playback"));
    gtk_widget_add_accelerator(w,"clicked",ag,key,GDK_MOD1_MASK,
			       (GtkAccelFlags)0);
    cd->dither_playback = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->dither_playback,dither_playback);

    w = gtk_check_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(w)->child),
				_("Auto dete_ct driver on each start-up"));
    gtk_widget_add_accelerator(w,"clicked",ag,key,GDK_MOD1_MASK,
			       (GtkAccelFlags)0);
    cd->driver_autodetect = GTK_TOGGLE_BUTTON(w);
    if (!strcmp(inifile_get(INI_SETTING_SOUNDDRIVER,DEFAULT_DRIVER),"auto")) {
	 gtk_widget_set_sensitive(GTK_WIDGET(cd->sound_driver),FALSE);
	 gtk_toggle_button_set_active(cd->driver_autodetect,TRUE);
    }
    gtk_signal_connect(GTK_OBJECT(w),"toggled",
		       GTK_SIGNAL_FUNC(driver_autodetect_toggled),cd);

    w = combo_new();
    l = NULL;
    l = g_list_append(l,_("Direct access"));
    l = g_list_append(l,_("Decompress"));
    l = g_list_append(l,_("Bypass"));
    combo_set_items(COMBO(w),l,inifile_get_guint32("sndfileOggMode",1));
    g_list_free(l);
    if (!sndfile_ogg_supported()) {
	 combo_set_selection(COMBO(w),2);
	 gtk_widget_set_sensitive(w,FALSE);
    }
    cd->oggmode = COMBO(w);

    w = combo_new();
    l = NULL;
    l = g_list_append(l,_("Biased"));
    l = g_list_append(l,_("Pure-Scaled"));
    combo_set_items(COMBO(w),l,sample_convert_mode);
    g_list_free(l);
    cd->convmode = COMBO(w);

    /* Layout the window */
    
    a = gtk_vbox_new(FALSE,5);
    gtk_container_add(GTK_CONTAINER(cd),a);
    b = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(b),GTK_POS_TOP);
    gtk_box_pack_start(GTK_BOX(a),b,TRUE,TRUE,0);



    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),8);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Interface")));
    d = gtk_frame_new(_(" Main window "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = GTK_WIDGET(cd->remember_geometry);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->mainwin_front);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new(_("Number of recent files in File menu: "));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->recent_files);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,8);

    d = gtk_frame_new(_(" View "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = GTK_WIDGET(cd->improve);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (g)->child), 
				_("Customize co_lors..."));
    gtk_widget_add_accelerator (g,"clicked",ag,key,GDK_MOD1_MASK,
				(GtkAccelFlags) 0);
    gtk_signal_connect(GTK_OBJECT(g),"clicked",GTK_SIGNAL_FUNC(colors_click),
		       cd);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);

    d = gtk_frame_new(_(" Window contents "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = GTK_WIDGET(cd->time_scale_default);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->vzoom_default);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->speed_default);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->labels_default);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->bufpos_default);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new(_("Vertical zoom limit: "));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->vzoom_max);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,8);
    g = gtk_label_new(_("x"));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);


    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),8);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Sound")));
    d = gtk_frame_new(_(" Driver options "));    
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new("");
    key = gtk_label_parse_uline(GTK_LABEL (g), _("_Driver:"));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->sound_driver);
    gtk_widget_add_accelerator(g, "grab_focus", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,8);
    g = GTK_WIDGET(cd->sound_driver_prefs);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,3);
    f = GTK_WIDGET(cd->driver_autodetect);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->output_bswap);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->sound_lock);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->output_stereo);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);

    d = gtk_frame_new(_(" Fallback format "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_label_new(_("Sample format to try when the sound file's format isn't"
		      " supported."));
    gtk_label_set_line_wrap(GTK_LABEL(f),TRUE);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->fallback_format);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    
    


    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),8);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Playback")));

    d = gtk_frame_new(_(" Playback settings "));    
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = GTK_WIDGET(cd->mark_autoplay);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->roll_cursor);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->center_cursor);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    
    d = gtk_frame_new(_(" Variable speed "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = GTK_WIDGET(cd->varispeed_enable);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->varispeed_fast);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->varispeed_autoreset);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);    
    

    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),8);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Files")));

    d = gtk_frame_new(_(" Temporary file directories "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_table_new(5,3,FALSE);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_scrolled_window_new(NULL,NULL);
    gtk_table_attach(GTK_TABLE(f),g,0,2,0,4,GTK_EXPAND|GTK_FILL,GTK_FILL,0,0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(g),GTK_POLICY_NEVER,
				   GTK_POLICY_AUTOMATIC);
    h = GTK_WIDGET(cd->tempdirs);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(g),h);
    g = GTK_WIDGET(cd->tempdir_up);
    gtk_table_attach(GTK_TABLE(f),g,2,3,0,1,GTK_FILL,0,0,0);
    g = GTK_WIDGET(cd->tempdir_down);
    gtk_table_attach(GTK_TABLE(f),g,2,3,1,2,GTK_FILL,0,0,0);
    g = GTK_WIDGET(cd->tempdir_remove);
    gtk_table_attach(GTK_TABLE(f),g,2,3,2,3,GTK_FILL,0,0,0);
    g = GTK_WIDGET(cd->tempdir_add_entry);
    gtk_table_attach(GTK_TABLE(f),g,0,1,4,5,GTK_EXPAND|GTK_FILL,0,0,4);
    g = GTK_WIDGET(cd->tempdir_browse);
    gtk_table_attach(GTK_TABLE(f),g,1,2,4,5,GTK_FILL,0,0,4);
    g = GTK_WIDGET(cd->tempdir_add);
    gtk_table_attach(GTK_TABLE(f),g,2,3,4,5,GTK_FILL,0,0,4);

    d = gtk_frame_new(_(" Temporary file settings "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_label_new(_("To avoid rounding errors when applying more than one "
		      "effect on the same data, floating-point temporary "
		      "files can be used. However, this will increase disk "
		      "and CPU usage."));
    gtk_label_set_line_wrap(GTK_LABEL(f),TRUE);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->floating_tempfiles);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = gtk_label_new(_("Some versions of libsndfile supports accessing "
			"Ogg files without decompressing to a temporary "
			"file."));
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    gtk_label_set_line_wrap(GTK_LABEL(f),TRUE);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new("Libsndfile ogg handling: ");
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->oggmode);
    gtk_box_pack_start(GTK_BOX(f),g,TRUE,TRUE,0);

    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),8);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Quality")));
    
    d = gtk_frame_new(_(" Rate conversions "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_add(GTK_CONTAINER(d),e);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    f = gtk_table_new(2,2,FALSE);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    attach_label(_("Varispeed: "),f,0,0);
    attach_label(_("Speed effect: "),f,1,0);
    g = GTK_WIDGET(cd->varispeed_method);
    gtk_table_attach(GTK_TABLE(f),g,1,2,0,1,GTK_EXPAND|GTK_FILL,0,0,0);
    g = GTK_WIDGET(cd->speed_method);
    gtk_table_attach(GTK_TABLE(f),g,1,2,1,2,GTK_EXPAND|GTK_FILL,0,0,0);
    gtk_table_set_row_spacings(GTK_TABLE(f),4);

    d = gtk_frame_new(_(" Dithering "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_add(GTK_CONTAINER(d),e);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);

    f = GTK_WIDGET(cd->dither_editing);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->dither_playback);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);

    d = gtk_frame_new(_(" Sample conversion "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_add(GTK_CONTAINER(d),e);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    f = gtk_hbox_new(FALSE,0);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new(_("Normalization mode: "));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->convmode);
    gtk_box_pack_start(GTK_BOX(f),g,TRUE,TRUE,0);

    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),8);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Other")));

    d = gtk_frame_new(_(" Time format "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);    

    e = gtk_table_new(2,2,FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    gtk_table_set_row_spacings(GTK_TABLE(e),4);

    g = gtk_label_new("");
    key = gtk_label_parse_uline(GTK_LABEL(g), _("Display t_imes as: "));
    gtk_misc_set_alignment(GTK_MISC(g),0.0,0.5);
    gtk_table_attach(GTK_TABLE(e),g,0,1,0,1,GTK_FILL,0,0,0);

    g = GTK_WIDGET(cd->time_display);
    gtk_widget_add_accelerator(g, "grab_focus", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_table_attach(GTK_TABLE(e),g,1,2,0,1,GTK_FILL|GTK_EXPAND,0,0,0);


    g = gtk_label_new("");
    key = gtk_label_parse_uline(GTK_LABEL(g), _("Times_cale format: "));
    gtk_misc_set_alignment(GTK_MISC(g),0.0,0.5);
    gtk_table_attach(GTK_TABLE(e),g,0,1,1,2,GTK_FILL,0,0,0);

    g = GTK_WIDGET(cd->time_display_timescale);
    gtk_widget_add_accelerator(g, "grab_focus", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_table_attach(GTK_TABLE(e),g,1,2,1,2,GTK_FILL|GTK_EXPAND,0,0,0);



    d = gtk_frame_new(_(" External applications "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,3);
    g = gtk_label_new("");
    key = gtk_label_parse_uline(GTK_LABEL (g), _("Mi_xer utility: "));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->mixer_utility);
    gtk_widget_add_accelerator(g, "grab_focus", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,8);

    
    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),8);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Advanced")));
    d = gtk_frame_new(_(" Advanced settings "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new("");
    key = gtk_label_parse_uline(GTK_LABEL (g), _("Disk editing _threshold: "));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);    
    g = GTK_WIDGET(cd->disk_threshold);
    gtk_widget_add_accelerator(g, "grab_focus", ag, key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
    gtk_box_pack_start(GTK_BOX(f),g,TRUE,TRUE,8);
    g = gtk_label_new("K ");
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new("");
    key = gtk_label_parse_uline(GTK_LABEL (g), _("View _quality:"));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->view_quality);
    gtk_widget_add_accelerator(g, "grab_focus", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_box_pack_start(GTK_BOX(f),g,TRUE,TRUE,8);
    g = gtk_label_new(_("samples/pixel"));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);    

    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);    
    g = gtk_label_new("");
    key = gtk_label_parse_uline(GTK_LABEL (g), _("Output _buffer size:"));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->sound_buffer_size);
    gtk_widget_add_accelerator(g, "grab_focus", ag, key, GDK_MOD1_MASK, 
			       (GtkAccelFlags) 0);
    gtk_box_pack_start(GTK_BOX(f),g,TRUE,TRUE,8);
    g = gtk_label_new(_("bytes"));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    
    
    

    b = gtk_hbutton_box_new();
    gtk_box_pack_start(GTK_BOX(a),b,FALSE,TRUE,0);


    c = gtk_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (c)->child), _("_OK"));
    gtk_widget_add_accelerator (c, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    gtk_widget_add_accelerator (c, "clicked", ag, GDK_KP_Enter, 0, 
				(GtkAccelFlags) 0);
    gtk_widget_add_accelerator (c, "clicked", ag, GDK_Return, 0, 
				(GtkAccelFlags) 0);
    gtk_signal_connect(GTK_OBJECT(c),"clicked",
		       GTK_SIGNAL_FUNC(config_dialog_ok),cd);

    gtk_container_add (GTK_CONTAINER (b), c);
    GTK_WIDGET_SET_FLAGS (c, GTK_CAN_DEFAULT);

    c = gtk_button_new_with_label("");
    key = gtk_label_parse_uline(GTK_LABEL (GTK_BIN (c)->child), _("_Close"));
    gtk_widget_add_accelerator (c, "clicked", ag, key, GDK_MOD1_MASK, 
				(GtkAccelFlags) 0);
    gtk_widget_add_accelerator (c, "clicked", ag, GDK_Escape, 0, 
				(GtkAccelFlags) 0);
    gtk_signal_connect_object(GTK_OBJECT(c),"clicked",
                              GTK_SIGNAL_FUNC(gtk_widget_destroy),
                              GTK_OBJECT(cd));
    gtk_container_add (GTK_CONTAINER (b), c);
    GTK_WIDGET_SET_FLAGS (c, GTK_CAN_DEFAULT);
    gtk_widget_show_all(a);

    gtk_window_add_accel_group(GTK_WINDOW (cd), ag);


}

GtkType config_dialog_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "ConfigDialog",
	       sizeof(ConfigDialog),
	       sizeof(ConfigDialogClass),
	       (GtkClassInitFunc) config_dialog_class_init,
	       (GtkObjectInitFunc) config_dialog_init
	  };
	  id = gtk_type_unique(gtk_window_get_type(),&info);
     }
     return id;
}

GtkWidget *config_dialog_new(void)
{
     return gtk_type_new(config_dialog_get_type());
}
