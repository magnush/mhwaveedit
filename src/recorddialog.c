/*
 * Copyright (C) 2004 2005, Magnus Hjorth
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

#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "recorddialog.h"
#include "main.h"
#include "inifile.h"
#include "sound.h"
#include "um.h"
#include "int_box.h"
#include "formatselector.h"
#include "gettext.h"

struct {
     GtkWindow *wnd;
     FormatSelector *fs;
     GtkEntry *name_entry;
     int old_choice;
} other_dialog;

static gboolean record_dialog_set_format(RecordDialog *rd, RecordFormat *rf);

static char *choose_format_string = N_("Choose a sample format");

static GtkObjectClass *parent_class;
static gboolean record_dialog_stopflag = FALSE;

static void reset_peaks(GtkButton *button, gpointer user_data)
{
	int i;
	RecordDialog *rd = RECORD_DIALOG(user_data);
	for (i=0; i<8; i++) rd->maxpeak_values[i] = 0.0;
}

static gchar *rf_string(RecordFormat *rf)
{
     gchar *c,*d;
     if (rf->fmt.type == DATAFORMAT_PCM)
	  c = g_strdup_printf(_("%d-bit"),rf->fmt.samplesize*8);
     else if (rf->fmt.samplesize == sizeof(float))
	  c = g_strdup(_("float"));
     else
	  c = g_strdup(_("double"));
     if (rf->name)
	  d = g_strdup_printf(_("%s (%s %s %d Hz)"),rf->name,
			      channel_format_name(rf->fmt.channels),
			      c,rf->fmt.samplerate);
     else 
	  d = g_strdup_printf(_("%s %s %d Hz"),
			      channel_format_name(rf->fmt.channels),
			      c,rf->fmt.samplerate);
     g_free(c);
     return d;
}

static void build_combo_strings(RecordDialog *obj)
{
     GList *l = NULL, *l2;
     l = g_list_append(l,g_strdup(_(choose_format_string)));
     for (l2 = obj->formats; l2 != NULL; l2 = l2->next) 
	  l = g_list_append(l,rf_string((RecordFormat *)(l2->data)));
     l = g_list_append(l,g_strdup(_("Other format...")));
     if (obj->format_strings) {
	  g_list_foreach(obj->format_strings,(GFunc)g_free,NULL);
	  g_list_free(obj->format_strings);	  
     }
     obj->format_strings = l;
}

static void set_limit_label(RecordDialog *rd)
{
     gchar buf[64];
     if (rd->limit_record) {
	  get_time_s(rd->current_format->fmt.samplerate,
		     (rd->limit_bytes-rd->written_bytes) /
		     rd->current_format->fmt.samplebytes, 0, buf);
	  gtk_label_set_text(rd->limit_label,buf);	 
     } else
	  gtk_label_set_text(rd->limit_label,_("(no limit)"));
}

static gboolean process_input(RecordDialog *rd, gboolean finish_mode)
{
     guint32 x,y;
     gchar buf[4096]; 
     guint c,d,e;
     sample_t peak,rms,avg,*sp,*sq,s,pmax;
     guint32 clip_amount,clip_size;
     int i;

     if (rd->current_format == NULL || record_dialog_stopflag) return FALSE;
     /* Read input */
     input_store(rd->databuf);

     x = ringbuf_available(rd->databuf);
     /* Round to even samples */
     x -= x % rd->current_format->fmt.samplebytes;
     /* If we have <0.1 s of data, wait for more. */
     if ((x < rd->analysis_bytes && !finish_mode) || x==0) return FALSE;
     /* Send data older than 0.1s straight into the tempfile if we have one */
     if (!finish_mode)
	  x -= rd->analysis_bytes;
     while (x > 0) {
	  y = ringbuf_dequeue(rd->databuf,buf,MIN(x,sizeof(buf)));
	  if (rd->tf != NULL && !rd->paused) {
	       if (tempfile_write(rd->tf,buf,y)) {
		    record_dialog_stopflag = TRUE;
		    return FALSE;
	       }
	       rd->written_bytes += y;
	       if ( (rd->limit_record) && 
		    (rd->written_bytes >= rd->limit_bytes) ) {
		    record_dialog_stopflag = TRUE;
		    return TRUE;
	       }
	  }
	  x -= y;
     }
     if (finish_mode) return TRUE;
     /* Analyse data */
     y = ringbuf_dequeue(rd->databuf,rd->analysis_buf,rd->analysis_bytes);
     g_assert(y == rd->analysis_bytes);
     /* First write out the raw data to disk */
     if (rd->tf != NULL && !rd->paused) {
	  if (tempfile_write(rd->tf,rd->analysis_buf,y)) {
	       record_dialog_stopflag = TRUE;
	       return FALSE;
	  }
	  rd->written_bytes += y;
	  if ( (rd->limit_record) && (rd->written_bytes >= rd->limit_bytes) )
	       record_dialog_stopflag = TRUE;
     }
     convert_array(rd->analysis_buf,&(rd->current_format->fmt),
		   rd->analysis_sbuf,&dataformat_sample_t,
		   rd->analysis_samples, DITHER_NONE);
     /* Calculate statistics */
     sq = &(rd->analysis_sbuf[rd->analysis_samples]);
     d = rd->current_format->fmt.channels;
     for (c=0; c<d; c++) {
	  peak = rms = avg = 0.0;
	  clip_size = clip_amount = 0;
	  sp = &(rd->analysis_sbuf[c]);
	  for (; sp<sq; sp+=d) {
	       s = *sp;
	       avg += s;
	       s = fabs(s);
	       if (s > peak) peak = s;
	       rms += s*s;
	       *sp = s; /* Save the abs value for clipping calculation */
	  }
	  avg /= ((sample_t)rd->analysis_samples);
	  rms = sqrt((rms)/ ((sample_t)rd->analysis_samples) - avg*avg);
	  pmax = maximum_float_value(&(rd->current_format->fmt));
	  /* Since the conversion routines were changed for 1.2.9, this
	   * algo can give false alarms, but only when you're _very_ near 
	   * clipping. */
	  if (peak >= pmax) {
	       /* Calculate clipping amount and size */
	       sp = &(rd->analysis_sbuf[c]);
	       while (1) {
		    for (; sp<sq; sp+=d)
			 if (*sp >= pmax) break;
		    if (sp >= sq) break;
		    for (e=0; sp<sq && *sp>=pmax; e++,sp+=d) { }
		    clip_amount ++;
		    clip_size += e*e;
	       }	       
	  }
	  /* Set the labels and VU meters */
	  vu_meter_set_value(rd->meters[c],(float)peak);
	  g_snprintf(buf,sizeof(buf),"%.5f",(float)peak);
	  gtk_label_set_text(rd->peak_labels[c],buf);
	  if (((float)peak) > rd->maxpeak_values[c] )
	  {
	  	rd->maxpeak_values[c] = (float)peak;
		gtk_label_set_text(rd->maxpeak_labels[c],buf);
	  }
	  g_snprintf(buf,sizeof(buf),"%.5f",(float)rms);
	  gtk_label_set_text(rd->rms_labels[c],buf);
	  /* This probably needs some tweaking but has to do for now. */
	  if (clip_amount == 0) 
	       gtk_label_set_text(rd->clip_labels[c],_("None"));
	  else if (((float)clip_size)/((float)clip_amount) < 2.0
		   || (clip_size < rd->analysis_samples / (d * 100)))
	       gtk_label_set_text(rd->clip_labels[c],_("Mild"));
	  else 
	       gtk_label_set_text(rd->clip_labels[c],_("Heavy"));
     }
     if (rd->tf) {
	  get_time(rd->current_format->fmt.samplerate,
		   rd->written_bytes/rd->current_format->fmt.samplebytes,
		   0,buf,default_time_mode);
	  gtk_label_set_text(rd->time_label,buf);
	  g_snprintf(buf,200,"%" OFF_T_FORMAT "", 
		     rd->written_bytes);
	  gtk_label_set_text(rd->bytes_label,buf);
	  get_time_s(rd->current_format->fmt.samplerate,
		     rd->written_bytes/rd->current_format->fmt.samplebytes,
		     0, buf);
	  if ( strcmp(buf, GTK_WINDOW(rd)->title) ) {
	       gtk_window_set_title(GTK_WINDOW(rd),buf);
	       /* Also update the remaining time here */
	       set_limit_label(rd);
	  }
	  i = input_overrun_count();
	  if (i>=0) {
	       g_snprintf(buf,sizeof(buf),"%d",i-rd->overruns_before_start);
	       gtk_label_set_text(rd->overruns_label,buf);
	  }

     }
     return TRUE;
}

static void record_dialog_format_changed(Combo *combo, gpointer user_data)
{
     gchar *c;
     c = combo_selected_string(combo);
     if (strcmp(c,_(choose_format_string)))
	  RECORD_DIALOG(user_data)->format_changed = TRUE;
     g_free(c);
}

static void other_dialog_ok(GtkButton *button, gpointer user_data)
{    
     RecordDialog *rd = RECORD_DIALOG(user_data);
     RecordFormat *rf,*rf2;
     static RecordFormat srf;
     gchar *c,*d;
     guint i;
     GList *l;

     if (format_selector_check(other_dialog.fs)) return;

     format_selector_get(other_dialog.fs,&(srf.fmt));
     c = (gchar *)gtk_entry_get_text(other_dialog.name_entry);
     while (*c == ' ') c++; /* Skip beginning spaces */
     if (*c != 0) {	  
	  srf.name = g_strdup(c);
	  /* Trim ending spaces */
	  c = strchr(c,0);
	  c--;
	  while (*c == ' ') { *c = 0; c--; }
     } else
	  srf.name = NULL;
     srf.num = 0;

     rf = g_malloc(sizeof(*rf));
     memcpy(rf,&srf,sizeof(*rf));

     if (srf.name != NULL) {
	  /* If there is an old format with the same name, remove its name and
	   * give the new format the same number */
	  /* Otherwise, set the number to one higher than the
	   * currently highest used number. */
	  i = 0;
	  l = rd->formats;
	  while (l != NULL) {
	       rf2 = (RecordFormat *)(l->data);
	       if (!strcmp(rf2->name,srf.name)) {
		    g_free(rf2->name);
		    rf2->name = NULL;
		    rf->num = rf2->num;
		    break;
	       }
	       if (rf2->num > i) i = rf2->num;
	       l = l->next;
	  } 	  
	  if (l == NULL) rf->num = i+1;
	  c = g_strdup_printf("recordFormat%d",rf->num);
	  d = g_strdup_printf("%s_Name",c);
	  inifile_set(d,rf->name);
	  dataformat_save_to_inifile(c,&(srf.fmt),TRUE);
	  g_free(d);
	  g_free(c);
     }

     rd->formats = g_list_append(rd->formats,rf);
     build_combo_strings(rd);
     combo_set_items(rd->format_combo,rd->format_strings,
		     g_list_length(rd->format_strings)-2);     
     combo_remove_item(rd->format_combo,0);
     rd->format_combo_fresh = FALSE;

     gtk_widget_destroy(GTK_WIDGET(other_dialog.wnd));
}

static gboolean other_dialog_delete(GtkWidget *widget, GdkEvent *event,
				    gpointer user_data)
{
     return FALSE;
}

static void restore_choice(RecordDialog *rd) 
{
     combo_set_selection(rd->format_combo, other_dialog.old_choice);
}

static void other_format_dialog(RecordDialog *rd)
{
     GtkWidget *a,*b,*c,*d;
     GtkAccelGroup* ag;
     
     ag = gtk_accel_group_new();
     a = gtk_window_new(GTK_WINDOW_DIALOG);
     gtk_window_set_title(GTK_WINDOW(a),_("Custom format"));
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_window_set_transient_for(GTK_WINDOW(a),GTK_WINDOW(rd));
     gtk_window_set_policy(GTK_WINDOW(a),FALSE,FALSE,TRUE);
     gtk_container_set_border_width(GTK_CONTAINER(a),10);
     gtk_signal_connect_object(GTK_OBJECT(a),"delete_event",
			       GTK_SIGNAL_FUNC(restore_choice),GTK_OBJECT(rd));
     gtk_signal_connect(GTK_OBJECT(a),"delete_event",
			GTK_SIGNAL_FUNC(other_dialog_delete),NULL);
     other_dialog.wnd = GTK_WINDOW(a);
     b = gtk_vbox_new(FALSE,6);
     gtk_container_add(GTK_CONTAINER(a),b);
     c = format_selector_new(TRUE);
     gtk_container_add(GTK_CONTAINER(b),c);
     other_dialog.fs = FORMAT_SELECTOR(c);
     c = gtk_hseparator_new();
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_label_new(_("The sign and endian-ness can usually be left at their "
		       "defaults, but should be changed if you're unable to "
		       "record or get bad sound."));
     gtk_label_set_line_wrap(GTK_LABEL(c),TRUE);
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_hseparator_new();
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_label_new(_("To add this format to the presets, enter a name "
		       "below. Otherwise, leave it blank."));
     gtk_label_set_line_wrap(GTK_LABEL(c),TRUE);     
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_hbox_new(FALSE,4);
     gtk_container_add(GTK_CONTAINER(b),c);
     d = gtk_label_new(_("Name :"));
     gtk_container_add(GTK_CONTAINER(c),d);
     d = gtk_entry_new();
     gtk_container_add(GTK_CONTAINER(c),d);
     other_dialog.name_entry = GTK_ENTRY(d);
     c = gtk_hseparator_new();
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_hbutton_box_new();
     gtk_container_add(GTK_CONTAINER(b),c);
     d = gtk_button_new_with_label(_("OK"));
     gtk_widget_add_accelerator (d, "clicked", ag, GDK_KP_Enter, 0, 
				 (GtkAccelFlags) 0);
     gtk_widget_add_accelerator (d, "clicked", ag, GDK_Return, 0, 
				 (GtkAccelFlags) 0);
     gtk_container_add(GTK_CONTAINER(c),d);
     gtk_signal_connect(GTK_OBJECT(d),"clicked",
			GTK_SIGNAL_FUNC(other_dialog_ok),rd);
     d = gtk_button_new_with_label(_("Cancel"));
     gtk_widget_add_accelerator (d, "clicked", ag, GDK_Escape, 0, 
				 (GtkAccelFlags) 0);
     gtk_container_add(GTK_CONTAINER(c),d );
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(restore_choice),
			       GTK_OBJECT(rd));
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(a));
     gtk_widget_show_all(a);
     gtk_window_add_accel_group(GTK_WINDOW (a), ag);
}

static void update_limit(RecordDialog *rd)
{
     gdouble d;
     if (rd->tf == NULL) return;
     d = (gdouble)rd->limit_seconds;
     d *= (gdouble)(rd->current_format->fmt.samplerate);
     d *= (gdouble)(rd->current_format->fmt.samplebytes);
     rd->limit_bytes = (off_t)d;
     set_limit_label(rd);
}

static void set_time_limit(GtkButton *button, gpointer user_data)
{
     gfloat f;
     gchar buf[64];
     RecordDialog *rd = RECORD_DIALOG(user_data);
     
     f = parse_time((gchar *)gtk_entry_get_text(rd->limit_entry));

     if (f < 0.0) {
	  popup_error(_("Invalid time value. Time must be specified in the form"
		      " (HH')MM:SS(.mmmm)"));
	  return;
     }
     
     rd->limit_record = TRUE;
     rd->limit_seconds = f;
     get_time_l(1000,(off_t)(f*1000.0),(off_t)(f*1000.0),buf);
     gtk_label_set_text(rd->limit_set_label, buf);
     gtk_widget_set_sensitive(GTK_WIDGET(rd->disable_limit_button),TRUE);
     update_limit(rd);
}

static void disable_time_limit(GtkButton *button, gpointer user_data)
{
     RecordDialog *rd = RECORD_DIALOG(user_data);
     rd->limit_record = FALSE;
     gtk_label_set_text(rd->limit_set_label,_("(no limit)"));
     gtk_widget_set_sensitive(GTK_WIDGET(rd->disable_limit_button),FALSE);     
}

static gboolean record_dialog_set_format(RecordDialog *rd, RecordFormat *rf)
{
     GtkWidget *a,*b;
     gint i;

     /* printf("record_dialog_set_format: %s fresh=%d\n",rf->name,
	rd->format_combo_fresh); */
     input_stop();
     rd->current_format = NULL;
     if (rd->vu_frame->child != NULL)
	  gtk_container_remove(GTK_CONTAINER(rd->vu_frame),
			       rd->vu_frame->child);
     g_free(rd->peak_labels);
     g_free(rd->maxpeak_labels);
     g_free(rd->rms_labels);
     g_free(rd->clip_labels);
     g_free(rd->meters);
     rd->peak_labels = NULL;
     rd->maxpeak_labels = NULL;
     rd->rms_labels = NULL;
     rd->clip_labels = NULL;
     rd->meters = NULL;
     gtk_label_set_text(rd->status_label,_("Format not selected"));
     gtk_widget_set_sensitive(rd->record_button,FALSE);
     gtk_widget_set_sensitive(rd->reset_button,FALSE);

     i = input_select_format(&(rf->fmt),FALSE);
     if (i < 0) {
	  user_error(_("This format is not supported by the input driver!"));
	  return TRUE;
     } else if (i > 0)
	  return TRUE;

     gtk_label_set_text(rd->status_label,_("Ready for recording"));
     rd->current_format = rf;
     if (rd->format_combo_fresh) {
	  combo_remove_item(rd->format_combo,0);
	  rd->format_combo_fresh = FALSE;
     }
     other_dialog.old_choice = combo_selected_index(rd->format_combo);
     rd->peak_labels = g_malloc(rf->fmt.channels*sizeof(GtkLabel *));
     rd->maxpeak_labels = g_malloc(rf->fmt.channels*sizeof(GtkLabel *));
     rd->clip_labels = g_malloc(rf->fmt.channels*sizeof(GtkLabel *));
     rd->rms_labels = g_malloc(rf->fmt.channels*sizeof(GtkLabel *));
     rd->meters = g_malloc(rf->fmt.channels*sizeof(VuMeter *));
     memset(rd->maxpeak_values,0,sizeof(rd->maxpeak_values));
     a = gtk_table_new(6*((rf->fmt.channels+1)/2),4,FALSE);
     gtk_container_set_border_width(GTK_CONTAINER(a),4);
     
     for (i=0; i*2<rf->fmt.channels; i++) {
	  attach_label(_("Peak: "),a,i*6+2,0);
	  attach_label(_("Peak max: "),a,i*6+3,0);
	  attach_label(_("RMS: "),a,i*6+4,0);
	  attach_label(_("Clipping: "),a,i*6+5,0);	  
     }
     for (i=0; i<rf->fmt.channels; i++) {
	  b = gtk_label_new(channel_name(i,rf->fmt.channels));
	  gtk_table_attach(GTK_TABLE(a),b,(i&1)+1,(i&1)+2,(i/2)*6+0,(i/2)*6+1,
			   0,0,0,0);
	  b = vu_meter_new(0.0);
	  rd->meters[i] = VU_METER(b);
	  gtk_table_attach(GTK_TABLE(a),b,(i&1)+1,(i&1)+2,(i/2)*6+1,(i/2)*6+2,
			   0,0,0,0);	  
	  rd->peak_labels[i] = attach_label("",a,(i/2)*6+2,(i&1)+1);
	  rd->maxpeak_labels[i] = attach_label("",a,(i/2)*6+3,(i&1)+1);
	  rd->rms_labels[i] = attach_label("",a,(i/2)*6+4,(i&1)+1);
	  rd->clip_labels[i] = attach_label(_("None"),a,(i/2)*6+5,(i&1)+1);
     }

     gtk_table_set_col_spacings(GTK_TABLE(a),5);
     gtk_table_set_row_spacings(GTK_TABLE(a),3);
     gtk_container_add(GTK_CONTAINER(rd->vu_frame),a);
     gtk_widget_show_all(a);

     gtk_widget_set_sensitive(rd->record_button,TRUE);
     gtk_widget_set_sensitive(rd->reset_button,TRUE);

     /* Create a 2 second ring buffer */
     if (rd->databuf != NULL) {
	  ringbuf_free(rd->databuf);
	  g_free(rd->analysis_buf);
	  g_free(rd->analysis_sbuf);
     }
     rd->databuf = ringbuf_new(2*rf->fmt.samplebytes*rf->fmt.samplerate);
     /* Do analysis on 0.1 s parts. */
     rd->analysis_bytes = rf->fmt.samplebytes * rf->fmt.samplerate / 10;
     rd->analysis_samples = rf->fmt.channels * rf->fmt.samplerate / 10;
     rd->analysis_buf = g_malloc(rd->analysis_bytes);
     rd->analysis_sbuf = g_malloc(sizeof(sample_t) * rd->analysis_samples); 
     return FALSE;
}

static void check_format_change(RecordDialog *rd)
{
     gchar *c;
     GList *l,*l2;
     
     if (!rd->format_changed || combo_mouse_pressed(rd->format_combo)) return;

     rd->format_changed = FALSE;
     c = combo_selected_string(rd->format_combo);
     l=rd->formats;
     l2=rd->format_strings->next;
     for (; l!=NULL; l=l->next,l2=l2->next)
	  if (!strcmp(l2->data,c)) break;
     g_free(c);
     if (l == NULL) 
	  other_format_dialog(rd);
     else
	  record_dialog_set_format(rd,(RecordFormat *)(l->data));     
}

static void record_dialog_start(GtkButton *button, gpointer user_data)
{
     RecordDialog *rd = RECORD_DIALOG(user_data);
     GtkRequisition req;
     int i;

     if (rd->tf != NULL) {
	  /* This is a hack to prevent the window from resizing when we 
	   * change the button's caption */
	  gtk_widget_size_request(rd->record_button,&req);
	  gtk_widget_set_usize(rd->record_button,req.width,req.height);

	  /* Toggle pause mode */
	  rd->paused = !rd->paused;	  
	  gtk_label_set_text(GTK_LABEL(GTK_BIN(rd->record_button)->child),
			     rd->paused?_("Resume recording"):_("Pause recording"));
	  gtk_label_set_text(rd->status_label,rd->paused?
			     translate_strip(N_("RecordStatus|Paused")):
			     translate_strip(N_("RecordStatus|Recording")));
	  return;
     }

     inifile_set_gboolean("limitRecord",rd->limit_record);
     inifile_set_gfloat( "limitSecs",rd->limit_seconds);
     if (rd->current_format->name != NULL)
	  inifile_set("lastRecordFormat",rd->current_format->name);
     gtk_widget_set_sensitive(GTK_WIDGET(rd->format_combo),FALSE);
     gtk_label_set_text(GTK_LABEL(GTK_BIN(rd->record_button)->child),
			_("Pause recording"));
     rd->paused = FALSE;
     gtk_label_set_text(GTK_LABEL(GTK_BIN(rd->close_button)->child),
			_("Finish"));
     rd->tf = tempfile_init(&(rd->current_format->fmt),TRUE);
     rd->written_bytes = 0;
     i = input_overrun_count();
     gtk_label_set_text(rd->status_label,
			translate_strip(N_("RecordStatus|Recording")));
     if (i>-1) {
	  rd->overruns_before_start = i;
	  gtk_label_set_text(rd->overruns_title,_("Overruns: "));
     }
     gtk_label_set_text(rd->bytes_text_label,_("Bytes written: "));
     gtk_label_set_text(rd->limit_text_label,_("Auto stop in: "));
     update_limit(rd);
}

static void record_dialog_close(GtkButton *button, gpointer user_data)
{
     record_dialog_stopflag = TRUE;
}

static gboolean record_dialog_delete_event(GtkWidget *widget, GdkEvent *event,
					   gpointer user_data)
{
     record_dialog_stopflag = TRUE;
     return TRUE;
}

void record_dialog_init(RecordDialog *obj)
{
     GtkWidget *a,*b,*c,*d,*e;
     GtkAccelGroup* ag;
     GtkRequisition req;  
     GList *l2 = NULL;
     RecordFormat *rf;
     guint i;
     gchar *s1,*s2,*s3,*s4,*s5,*s6;
     gchar limitbuf[64];

     ag = gtk_accel_group_new();

     obj->format_changed = FALSE;
     obj->current_format = NULL;
     obj->databuf = NULL;
     obj->meters = NULL;
     obj->peak_labels = obj->maxpeak_labels = obj->clip_labels = NULL;
     obj->rms_labels = NULL;
     obj->tf = NULL;
     obj->written_bytes = 0;
     obj->analysis_buf = NULL;
     obj->analysis_sbuf = NULL;
     obj->limit_record = inifile_get_gboolean("limitRecord",FALSE);
     obj->limit_seconds = inifile_get_gfloat("limitSecs",3600.0);
     if (obj->limit_seconds < 0.0) obj->limit_seconds = 3600.0;
     get_time_l(1000,(off_t)(obj->limit_seconds*1000.0),0,limitbuf);

     gtk_window_set_title(GTK_WINDOW(obj),_("Record"));
     gtk_window_set_modal(GTK_WINDOW(obj),TRUE);
     gtk_window_set_default_size(GTK_WINDOW(obj),320,400);
     gtk_window_set_position(GTK_WINDOW(obj),GTK_WIN_POS_CENTER);
     gtk_container_set_border_width(GTK_CONTAINER(obj),10);

     gtk_signal_connect(GTK_OBJECT(obj),"delete_event",
			GTK_SIGNAL_FUNC(record_dialog_delete_event),NULL);

     /* Build format list */
     /* Support reading both old and new style formats */
     if (inifile_get("recordFormat1",NULL) == NULL &&
	 inifile_get("recordFormat1_Name",NULL) == NULL) {
	  inifile_set("recordFormat1","16/2/44100/S/CD Quality");
	  inifile_set("recordFormat2","8/1/8000//Low quality");
     }
     for (i=1; ; i++) {
	  s1 = g_strdup_printf("recordFormat%d",i);
	  s2 = inifile_get(s1,NULL);
	  g_free(s1);
	  if (s2 == NULL) {
	       s1 = g_strdup_printf("recordFormat%d_Name",i);
	       s2 = inifile_get(s1,NULL);
	       g_free(s1);
	       if (s2 == NULL) break;
	       rf = g_malloc(sizeof(*rf));
	       s3 = g_strdup_printf("recordFormat%d",i);
	       if (!dataformat_get_from_inifile(s3,TRUE,&(rf->fmt))) {
		    g_free(s3);
		    g_free(rf);
		    continue;
	       }
	       g_free(s3);
	       rf->num = i;
	       rf->name = g_strdup(s2);
	       l2 = g_list_append(l2,rf);
	  } else {
	       s3 = strchr(s2,'/');
	       if (s3 == NULL) continue;
	       s3 += 1;
	       s4 = strchr(s3,'/');
	       if (s4 == NULL) continue;
	       s4 += 1;
	       s5 = strchr(s4,'/');
	       if (s5 == NULL) continue;
	       s5 += 1;
	       s6 = strchr(s5,'/');
	       if (s6 == NULL) continue;
	       s6 += 1;
	       rf = g_malloc(sizeof(*rf));
	       rf->num = i;
	       rf->fmt.type = DATAFORMAT_PCM;
	       rf->fmt.samplesize = ((guint)strtod(s2,NULL)) / 8;
	       rf->fmt.channels = ((guint)strtod(s3,NULL));
	       rf->fmt.samplebytes = rf->fmt.samplesize * rf->fmt.channels;
	       rf->fmt.samplerate = ((guint)strtod(s4,NULL));
	       rf->fmt.sign = FALSE;
	       rf->fmt.bigendian = IS_BIGENDIAN;
	       for (; *s5 != 0; s5++) {
		    if (*s5 == 'B') rf->fmt.bigendian = !IS_BIGENDIAN; 
		    else if (*s5 == 'S') rf->fmt.sign = TRUE;
	       }
	       if (rf->fmt.samplesize < 1 || rf->fmt.samplesize > 4 ||
		   rf->fmt.channels < 1 || rf->fmt.channels > 8 ||
		   rf->fmt.samplerate < 1) {
		    g_free(rf);
		    continue;
	       }    
	       rf->name = g_strdup(s6);
	       l2 = g_list_append(l2,rf);
	  }
     }
     obj->formats = l2;


     /* Add components */
     a = gtk_vbox_new(FALSE,10);
     gtk_container_add(GTK_CONTAINER(obj),a);
     b = gtk_frame_new(_("Recording settings"));
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     c = gtk_vbox_new(FALSE,0);
     gtk_container_set_border_width(GTK_CONTAINER(c),5);
     gtk_container_add(GTK_CONTAINER(b),c);
     d = gtk_hbox_new(FALSE,0);
     gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
     e = gtk_label_new(_("Sample format: "));
     gtk_box_pack_start(GTK_BOX(d),e,FALSE,FALSE,0);

     e = combo_new();
     gtk_box_pack_start(GTK_BOX(d),e,TRUE,TRUE,0);
     obj->format_combo = COMBO(e);
     obj->format_combo_fresh = TRUE;
     obj->format_strings = NULL;
     other_dialog.old_choice = 0;
     build_combo_strings(obj);
     combo_set_items(obj->format_combo,obj->format_strings,0);
     gtk_signal_connect(GTK_OBJECT(e),"selection_changed",
			GTK_SIGNAL_FUNC(record_dialog_format_changed),obj);

     d = gtk_hbox_new(FALSE,3);
     gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,6);

     e = gtk_label_new(_("Time limit: "));
     gtk_box_pack_start(GTK_BOX(d),e,FALSE,FALSE,0);
     e = gtk_label_new(_("(no limit)"));
     obj->limit_set_label = GTK_LABEL(e);
     if (obj->limit_record)
	  gtk_label_set_text(obj->limit_set_label, limitbuf);
     gtk_box_pack_start(GTK_BOX(d),e,FALSE,FALSE,0);

     e = gtk_button_new_with_label(_("Disable"));
     obj->disable_limit_button = GTK_BUTTON(e);
     gtk_widget_set_sensitive(GTK_WIDGET(e),obj->limit_record);
     gtk_signal_connect(GTK_OBJECT(e),"clicked",
			GTK_SIGNAL_FUNC(disable_time_limit),obj);
     gtk_box_pack_end(GTK_BOX(d),e,FALSE,FALSE,0);
     e = gtk_button_new_with_label(_("Set"));
     gtk_signal_connect(GTK_OBJECT(e),"clicked",
			GTK_SIGNAL_FUNC(set_time_limit),obj);
     gtk_box_pack_end(GTK_BOX(d),e,FALSE,FALSE,0);
     e = gtk_entry_new();
     obj->limit_entry = GTK_ENTRY(e);
     gtk_entry_set_text(obj->limit_entry, limitbuf );
     /* Max length = "hhh'mm:ss.mmm" */
     gtk_entry_set_max_length( obj->limit_entry, 14); 
     gtk_box_pack_end(GTK_BOX(d),e,FALSE,FALSE,0);     


     b = gtk_frame_new(_("Input levels"));
     obj->vu_frame = GTK_BIN(b);
     gtk_box_pack_start(GTK_BOX(a),b,TRUE,TRUE,0);
     b = gtk_table_new(3,4,FALSE);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     attach_label(_("Recording status: "),b,0,0);
     obj->status_label = GTK_LABEL(gtk_label_new(_("Format not selected")));
     gtk_table_attach(GTK_TABLE(b),GTK_WIDGET(obj->status_label),1,4,0,1,
		      GTK_FILL,0,0,0);
     gtk_misc_set_alignment(GTK_MISC(obj->status_label),0.0,0.5);
     attach_label(_("Time recorded: "),b,1,0);
     obj->time_label = attach_label(_("N/A"),b,1,1);
     gtk_widget_size_request(GTK_WIDGET(obj->time_label),&req);
     /* Stops wobble during recording */
     gtk_widget_set_usize(GTK_WIDGET(obj->time_label),150,req.height);	

     obj->limit_text_label = attach_label("",b,2,0);
     obj->limit_label = attach_label("",b,2,1);
     obj->bytes_text_label = attach_label("",b,1,2);
     obj->bytes_label = attach_label("",b,1,3);     
     obj->overruns_title = attach_label("",b,2,2);
     obj->overruns_label = attach_label("",b,2,3);
     b = gtk_hbutton_box_new();
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     c = gtk_button_new_with_label(_("Start recording"));
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_KP_Enter, 0, 
				 (GtkAccelFlags) 0);
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_Return, 0, 
				 (GtkAccelFlags) 0);
     gtk_widget_set_sensitive(c,FALSE);
     gtk_signal_connect(GTK_OBJECT(c),"clicked",
			GTK_SIGNAL_FUNC(record_dialog_start),obj);
     obj->record_button = GTK_WIDGET(c);
     gtk_container_add(GTK_CONTAINER(b),c);

     c = gtk_button_new_with_label(_("Reset max peaks"));
     gtk_widget_set_sensitive(c,FALSE);
     gtk_signal_connect(GTK_OBJECT(c),"clicked", GTK_SIGNAL_FUNC(reset_peaks),obj);
     obj->reset_button = GTK_WIDGET(c);
     gtk_container_add(GTK_CONTAINER(b),c);     

     c = gtk_button_new_with_label(_("Launch mixer"));
     gtk_signal_connect(GTK_OBJECT(c),"clicked",GTK_SIGNAL_FUNC(launch_mixer),
			NULL);
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_button_new_with_label(_("Close"));
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_Escape, 0, 
				 (GtkAccelFlags) 0);
     gtk_signal_connect(GTK_OBJECT(c),"clicked",
			GTK_SIGNAL_FUNC(record_dialog_close),obj);
     gtk_container_add(GTK_CONTAINER(b),c);
     obj->close_button = c;

     gtk_widget_show_all(a);
     gtk_window_add_accel_group(GTK_WINDOW (obj), ag);

     /* Set the last used format */
     s1 = inifile_get("lastRecordFormat",NULL);
     if (s1 != NULL) {
	  for (l2=obj->formats,i=1; l2!=NULL; l2=l2->next,i++) {
	       rf = (RecordFormat *)(l2->data);
	       if (rf->name!=NULL && !strcmp(rf->name,s1)) break;
	  }     
	  if (l2!=NULL) combo_set_selection(obj->format_combo,i);
     }
}

static void free_format(RecordFormat *rf) {
     g_free(rf->name);
     g_free(rf);
}

static void record_dialog_destroy(GtkObject *obj)
{
     RecordDialog *rd = RECORD_DIALOG(obj);
     if (rd->formats) {
	  g_list_foreach(rd->formats,(GFunc)free_format,NULL);
	  g_list_free(rd->formats);
	  rd->formats = NULL;
     }
     if (rd->format_strings) {
	  g_list_foreach(rd->format_strings,(GFunc)g_free,NULL);
	  g_list_free(rd->format_strings);
	  rd->format_strings = NULL;
     }
     if (rd->databuf) ringbuf_free(rd->databuf);
     rd->databuf = NULL;
     g_free(rd->analysis_buf);
     rd->analysis_buf = NULL;
     g_free(rd->analysis_sbuf);
     rd->analysis_sbuf = NULL;
     parent_class->destroy(obj);
}

static void record_dialog_class_init(GtkObjectClass *klass)
{
     parent_class = gtk_type_class(gtk_window_get_type());
     klass->destroy = record_dialog_destroy;
}

GtkType record_dialog_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "RecordDialog",
	       sizeof(RecordDialog),
	       sizeof(RecordDialogClass),
	       (GtkClassInitFunc) record_dialog_class_init,
	       (GtkObjectInitFunc) record_dialog_init
	  };
	  id = gtk_type_unique(gtk_window_get_type(),&info);
     }
     return id;
}

Chunk *record_dialog_execute(void)
{
     RecordDialog *rd;
     Chunk *ds;
     int i;

     rd = RECORD_DIALOG(gtk_type_new(record_dialog_get_type()));
     record_dialog_stopflag = FALSE;
     gtk_widget_show(GTK_WIDGET(rd));
     while (!record_dialog_stopflag) {
	  process_input(rd,FALSE);
	  while (gtk_events_pending()) 
	       gtk_main_iteration();
	  check_format_change(rd);	  
	  if (!record_dialog_stopflag && !process_input(rd,FALSE)) 
	       do_yield(TRUE);
     }
     if (rd->tf != NULL) {
	  input_stop_hint();
	  i = 0; /* Just to avoid infinite loops */
	  while (process_input(rd,TRUE) && i<128) { i++; }
	  input_stop();
	  ds = tempfile_finished(rd->tf);
	  gtk_widget_destroy(GTK_WIDGET(rd));
	  return ds;
     } else {
	  input_stop();
	  gtk_widget_destroy(GTK_WIDGET(rd));
	  return NULL;
     }
}
