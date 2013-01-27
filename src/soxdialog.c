/*
 * Copyright (C) 2003 2004 2005 2006 2007 2009 2012, Magnus Hjorth
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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <gtk/gtk.h>

#include "main.h"
#include "soxdialog.h"
#include "pipedialog.h"
#include "effectbrowser.h"
#include "inifile.h"
#include "um.h"
#include "gettext.h"

static GtkObjectClass *parent_class;

static gchar *supported_effects[] = { 
     "echo","echos","reverb","chorus","flanger","phaser",
     "compand","pitch","stretch",
     "dcshift","mask","reverse","earwax","vibro",
     "lowp","highp","band",
     "lowpass","highpass","bandpass","bandreject","filter",NULL 
};

static gboolean sox_support_map[22] = { FALSE };
static gboolean v13_mode = FALSE;
static gboolean v14_mode = FALSE;

static gchar *supported_effect_names[] = { 
     N_("Echo"),
     N_("Echo sequence"),
     N_("Reverb"),
     N_("Chorus"),
     N_("Flanger"),
     N_("Phaser"),
     N_("Compress/Expand"),
     N_("Pitch adjust"),
     N_("Time stretch"),
     N_("DC Shift"),
     N_("Masking noise"),
     N_("Reverse"),
     N_("Earwax"),
     N_("Vibro"),
     N_("Lowpass filter (single-pole)"),
     N_("Highpass filter (single-pole)"),
     N_("Bandpass filter"),
     N_("Butterworth lowpass filter"),
     N_("Butterworth highpass filter"),
     N_("Butterworth bandpass filter"),
     N_("Butterworth bandreject filter"),
     N_("Sinc-windowed filter"),
     NULL 
};

static gchar *samplesize_switch[] = { NULL,"-b","-w",NULL,"-l" };
static gchar *samplesize_switch_v13[] = { NULL,"-1","-2","-3","-4" };

#define COMPAND_LINES 6

void sox_dialog_format_string(gchar *buf, guint bufsize, Dataformat *fmt)
{
     g_assert(fmt->type == DATAFORMAT_PCM && fmt->samplesize != 3 && fmt->packing == 0);

     if (v14_mode)
	  g_snprintf(buf,bufsize,"-t raw -r %d -e %s -b %d -c %d",fmt->samplerate,
		     fmt->sign?"signed":"unsigned",(8 * fmt->samplesize),
		     fmt->channels);
     else if (v13_mode)
	  g_snprintf(buf,bufsize,"-t raw -r %d %s %s -c %d",fmt->samplerate,
		     fmt->sign?"-s":"-u",samplesize_switch_v13[fmt->samplesize],
		     fmt->channels);
     else
	  g_snprintf(buf,bufsize,"-t raw -r %d %s %s -c %d",fmt->samplerate,
		     fmt->sign?"-s":"-u",samplesize_switch[fmt->samplesize],
		     fmt->channels);
}

static Chunk *sox_dialog_apply_proc_main(Chunk *chunk, StatusBar *bar, 
					 gpointer user_data)
{
     SoxDialog *sd = SOX_DIALOG(user_data);
     EffectDialog *ed = &(sd->ed);
     gchar fmt_buf[45];
     gchar cmd_buf[512],*c;
     gchar *t1,*t2;
     gfloat f;
     guint i,j;
     gint idx;
     gboolean b;
     off_t clipcount = 0;
     Chunk *r;

     sox_dialog_format_string(fmt_buf,sizeof(fmt_buf),&(chunk->format));
     g_snprintf(cmd_buf,sizeof(cmd_buf),"sox %s - %s - ",fmt_buf,fmt_buf);
     
     c=strchr(cmd_buf,0);     
     if (!strcmp(ed->effect_name,"echo") || 
	 !strcmp(ed->effect_name,"echos") ||
	 !strcmp(ed->effect_name,"reverb") || 
	 !strcmp(ed->effect_name,"chorus") || 
	 !strcmp(ed->effect_name,"flanger") || 
	 !strcmp(ed->effect_name,"phaser")) {
	  g_snprintf(fmt_buf,sizeof(fmt_buf),"sox_%s_gain",ed->effect_name);
	  inifile_set_gfloat(fmt_buf,sd->fb1->val);
	  if (!strcmp(ed->effect_name,"reverb")) {
	       f = sd->fb2->val;
	       inifile_set_gfloat("sox_reverb_rtime",f);
	  } else
	       f = 1.0;
	  g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf),"%s %f %f",ed->effect_name,
		     sd->fb1->val,f);
	  for (i=0; i<sd->i1; i++) {
	       if (sd->fba[0][i]->val > 0.0) {
		    for (j=0; j<4; j++) 
			 if (sd->fba[j] != NULL) {
			      c = strchr(c,0);
			      g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf)," %f",
					 sd->fba[j][i]->val);
			 }
		    if (sd->ca != NULL) {
			 c = strchr(c,0);
			 idx = combo_selected_index(sd->ca[i]);
			 if (idx == 0) strcpy(c," -s");
			 else strcpy(c," -t");
		    }
	       }
	  }
     } else if (!strcmp(ed->effect_name,"lowp") || 
	      !strcmp(ed->effect_name,"highp") ||
	      !strcmp(ed->effect_name,"highp") || 
	      !strcmp(ed->effect_name,"band") ||
	      !strcmp(ed->effect_name,"lowpass") || 
	      !strcmp(ed->effect_name,"highpass") || 
	      !strcmp(ed->effect_name,"bandpass") ||
	      !strcmp(ed->effect_name,"bandreject")) {
	  g_snprintf(fmt_buf,sizeof(fmt_buf),"sox_%s_freq",ed->effect_name);
	  inifile_set_gfloat(fmt_buf,sd->fb1->val);
	  if (sd->fb2 != NULL) {
	       g_snprintf(fmt_buf,sizeof(fmt_buf),"sox_%s_width",
			  ed->effect_name);
	       inifile_set_gfloat(fmt_buf,sd->fb2->val);
	  }
	  if (sd->tb1 != NULL)
	       inifile_set_gboolean("sox_band_noisemode",
				    gtk_toggle_button_get_active(sd->tb1));
	  g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf),"%s %s %f",
		     ed->effect_name,
		     (sd->tb1!=NULL && gtk_toggle_button_get_active(sd->tb1))?
		     "-n":"",sd->fb1->val);
	  if (sd->fb2 != NULL) {
	       c = strchr(c,0);
	       g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf)," %f",sd->fb2->val);
	  }
     } else if (!strcmp(ed->effect_name,"filter")) {
	  inifile_set_guint32("sox_filter_type",sd->i1);
	  inifile_set_guint32("sox_filter_low",sd->ib1->val);
	  inifile_set_guint32("sox_filter_high",sd->ib2->val);
	  inifile_set_guint32("sox_filter_length",sd->ib3->val);
	  inifile_set_gfloat("sox_filter_beta",sd->fb1->val);
	  if (sd->i1 == 0) 
	       g_snprintf(fmt_buf,sizeof(fmt_buf),"-%d",(int)sd->ib2->val);
	  else if (sd->i1 == 1)
	       g_snprintf(fmt_buf,sizeof(fmt_buf),"%d-",(int)sd->ib1->val);
	  else
	       g_snprintf(fmt_buf,sizeof(fmt_buf),"%d-%d",(int)sd->ib1->val,
			  (int)sd->ib2->val);
	  g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf),"filter %s %d %f",fmt_buf,
		     (int)sd->ib3->val,sd->fb1->val);
     } else if (!strcmp(ed->effect_name,"compand")) {
	  inifile_set_gfloat("sox_compand_attack",sd->fb1->val);
	  inifile_set_gfloat("sox_compand_decay",sd->fb2->val);
	  inifile_set_gfloat("sox_compand_gain",sd->fb3->val);
	  inifile_set_gfloat("sox_compand_startvol",sd->fb4->val);
	  inifile_set_gfloat("sox_compand_delay",sd->fb5->val);
	  g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf),"compand %f,%f ",
		     sd->fb1->val,sd->fb2->val);
	  for (i=0; i<COMPAND_LINES; i++) {
	       c = strchr(c,0);
	       g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf),(i>0)?",%f,%f":"%f,%f",
			  sd->fba[0][i]->val,sd->fba[1][i]->val);
	       if (sd->fba[0][i]->val >= 0.0) break;
	  }
	  c = strchr(c,0);
	  g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf)," %f %f %f",sd->fb3->val,
		     sd->fb4->val,sd->fb5->val);
     } else if (!strcmp(ed->effect_name,"dcshift")) {
	  b = gtk_toggle_button_get_active(sd->tb1);
	  inifile_set_gfloat("sox_dcshift_amount",sd->fb1->val);
	  inifile_set_gboolean("sox_dcshift_limiter",b);
	  if (b) inifile_set_gfloat("sox_dcshift_gain",sd->fb2->val);
	  if (b) 
	       g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf),"dcshift %f %f",
			  sd->fb1->val,sd->fb2->val);
	  else 
	       g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf),"dcshift %f",
			  sd->fb1->val);
     } else if (!strcmp(ed->effect_name,"pitch")) {
	  inifile_set_gfloat("sox_pitch_amount",sd->fb1->val);
	  inifile_set_gfloat("sox_pitch_width",sd->fb2->val);
	  t1 = combo_selected_string(sd->c1);
	  t2 = combo_selected_string(sd->c2);
	  g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf),"pitch %f %f %s %s",
		     sd->fb1->val,sd->fb2->val,t1,t2);
	  g_free(t1);
	  g_free(t2);
	  g_strdown(c);
     } else if (!strcmp(ed->effect_name,"stretch")) {
	  inifile_set_gfloat("sox_stretch_factor",sd->fb1->val);
	  inifile_set_gfloat("sox_stretch_window",sd->fb2->val);
	  g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf),"stretch %f %f",
		     sd->fb1->val,sd->fb2->val);	  
     } else if (!strcmp(ed->effect_name,"vibro")) {
	  inifile_set_gfloat("sox_vibro_speed",sd->fb1->val);
	  inifile_set_gfloat("sox_vibro_depth",sd->fb2->val);
	  g_snprintf(c,sizeof(cmd_buf)-(c-cmd_buf),"vibro %f %f",
		     sd->fb1->val,sd->fb2->val);
     } else if (!strcmp(ed->effect_name,"mask") ||
		!strcmp(ed->effect_name,"earwax") ||
		!strcmp(ed->effect_name,"reverse")) {
	  strcpy(c,ed->effect_name);
     } else {
	  g_assert_not_reached();
     }
     r = pipe_dialog_pipe_chunk(chunk,cmd_buf,FALSE,dither_editing,bar,
				&clipcount);
     if (r != NULL && clipwarn(clipcount,TRUE)) {
	  gtk_object_sink(GTK_OBJECT(r));
	  return NULL;
     }
     return r;
}

static Chunk *sox_dialog_apply_proc(Chunk *chunk, StatusBar *bar, 
				    gpointer user_data)
{
     Chunk *c,*d,*r;
     Dataformat stype = { DATAFORMAT_PCM, 44100, 4, 1, 4, TRUE, IS_BIGENDIAN };
     if ((chunk->format.type == DATAFORMAT_FLOAT) || 
	 (chunk->format.type == DATAFORMAT_PCM && 
	  (chunk->format.samplesize == 3 || chunk->format.packing!=0))) {
	  c = chunk_convert_sampletype(chunk,&stype);
	  d = sox_dialog_apply_proc_main(c,bar,user_data);
	  gtk_object_sink(GTK_OBJECT(c));
	  r = chunk_convert_sampletype(d,&(chunk->format));
	  gtk_object_sink(GTK_OBJECT(d));
	  return r;
     } else
	  return sox_dialog_apply_proc_main(chunk,bar,user_data);
}

static gboolean sox_dialog_apply(EffectDialog *ed)
{
     SoxDialog *sd = SOX_DIALOG(ed);
     Document *d = EFFECT_BROWSER(ed->eb)->dl->selected;
     /* Chunk *chunk = d->chunk; */
     guint i,j;
     /*
     if (chunk->format.samplesize != 1 &&
	 chunk->format.samplesize != 2 &&
	 chunk->format.samplesize != 4) {
	  user_info(_("SoX only supports 8, 16 and 32-bit sample sizes"));
	  return TRUE;
     }
     */
     if ((sd->fb1!=NULL && floatbox_check(sd->fb1)) ||
	 (sd->fb2!=NULL && floatbox_check(sd->fb2)) || 
	 (sd->fb3!=NULL && floatbox_check(sd->fb3)) || 
	 (sd->fb4!=NULL && floatbox_check(sd->fb4)) || 
	 (sd->fb5!=NULL && floatbox_check(sd->fb5)) || 
	 (sd->ib1!=NULL && intbox_check(sd->ib1)) ||
	 (sd->ib2!=NULL && intbox_check(sd->ib2)) ||
	 (sd->ib3!=NULL && intbox_check(sd->ib3)))
	  return TRUE;
     for (i=0; i<ARRAY_LENGTH(sd->fba); i++)
	  if (sd->fba[i] != NULL)	       
	       for (j=0; j<sd->i1; j++)
		    if (floatbox_check(sd->fba[i][j])) return TRUE;
     return document_apply_cb(d,sox_dialog_apply_proc,TRUE,ed);
}

static void setup_filter(EffectDialog *ed, gchar *bw_name, gboolean nm)
{
     SoxDialog *sd = SOX_DIALOG(ed);
     GtkWidget *a,*b;
     gchar c[64];
     a = gtk_table_new(3,3,FALSE);
     gtk_container_add(GTK_CONTAINER(ed->input_area),a);
     attach_label(_("Frequency: "),a,0,0);
     g_snprintf(c,sizeof(c),"sox_%s_freq",ed->effect_name);
     b = floatbox_new(inifile_get_gfloat(c,440.0));
     gtk_table_attach(GTK_TABLE(a),b,1,2,0,1,0,0,0,0);	  
     sd->fb1 = FLOATBOX(b);
     attach_label(_("Hz"),a,0,2);
     if (bw_name != NULL) {
	  attach_label(bw_name,a,1,0);
	  g_snprintf(c,sizeof(c),"sox_%s_width",ed->effect_name);
	  b = floatbox_new(inifile_get_gfloat(c,50.0));
	  gtk_table_attach(GTK_TABLE(a),b,1,2,1,2,0,0,0,0);	  
	  sd->fb2 = FLOATBOX(b);
	  attach_label(_("Hz"),a,1,2);
     }
     if (nm) {
	  b = gtk_check_button_new_with_label(_("Noise mode"));
	  gtk_toggle_button_set_active
	       (GTK_TOGGLE_BUTTON(b),
		inifile_get_gboolean("sox_band_noisemode",FALSE));
	  gtk_table_attach(GTK_TABLE(a),b,0,3,2,3,0,0,0,0);
	  sd->tb1 = GTK_TOGGLE_BUTTON(b);
     }
     gtk_widget_show_all(a);
}

static void sox_filter_type_changed(Combo *combo, gpointer user_data)
{
     SoxDialog *sd = SOX_DIALOG(user_data);
     guint i;
     i = combo_selected_index(combo);
     sd->i1 = i;
     gtk_widget_set_sensitive(GTK_WIDGET(sd->ib1),(i==1 || i==2));
     gtk_widget_set_sensitive(GTK_WIDGET(sd->ib2),(i==0 || i==2));
}

#define ECHOS_MAX 7

static void setup_echo_column(SoxDialog *sd, GtkTable *t, guint col, 
			      gint lines)
{
     guint i;
     GtkWidget *w;
     sd->fba[col] = g_malloc(lines*sizeof(EffectDialog *));
     for (i=0; i<lines; i++) {
	  w = floatbox_new(0.0);
	  sd->fba[col][i] = FLOATBOX(w);
	  gtk_table_attach(t,w,col,col+1,i+1,i+2,GTK_FILL,0,0,0);	  
     }
}

static void setup_echos(EffectDialog *ed, gchar *gain_name, gint lines,
			gboolean show_decay, gboolean show_speed, 
			gboolean show_depth, gboolean show_mtype)
{
     SoxDialog *sd = SOX_DIALOG(ed);
     GtkWidget *a,*b,*c;
     gchar buf[64];
     guint i;
     GList *l;
     GtkRequisition req;

     sd->i1 = lines;
     a = gtk_vbox_new(FALSE,8);
     gtk_container_add(GTK_CONTAINER(ed->input_area),a);
     b = gtk_hbox_new(FALSE,2);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     c = gtk_label_new(gain_name);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     g_snprintf(buf,sizeof(buf),"sox_%s_gain",ed->effect_name);
     c = floatbox_new(inifile_get_gfloat(buf,1.0));
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     sd->fb1 = FLOATBOX(c);
     if (!strcmp(ed->effect_name,"reverb")) {
	  b = gtk_hbox_new(FALSE,2);
	  gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
	  c = gtk_label_new(_("Reverb time: "));
	  gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);	  
	  c = floatbox_new(inifile_get_gfloat("sox_reverb_rtime",500.0));
	  gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
	  sd->fb2 = FLOATBOX(c);
	  c = gtk_label_new(_("ms"));
	  gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);	  
     }
     b = gtk_table_new(lines+1,5,FALSE);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     attach_label(_("Delay (ms)  "),b,0,0);
     setup_echo_column(sd,GTK_TABLE(b),0,lines);
     if (show_decay) {
	  attach_label(_("Decay  "),b,0,1);
	  setup_echo_column(sd,GTK_TABLE(b),1,lines);
     }
     if (show_speed) {
	  attach_label(_("Speed (Hz)  "),b,0,2);
	  setup_echo_column(sd,GTK_TABLE(b),2,lines);
     }
     if (show_depth) {
	  attach_label(_("Depth (ms)  "),b,0,3);
	  setup_echo_column(sd,GTK_TABLE(b),3,lines);
     }
     if (show_mtype) {
	  attach_label(_("Modulation  "),b,0,4);
	  sd->ca = g_malloc(lines*sizeof(GtkCombo *));
	  l = NULL;
	  l = g_list_append(l,translate_strip(N_("Modulation|Sinusoidal")));
	  l = g_list_append(l,translate_strip(N_("Modulation|Triangular")));
	  for (i=0; i<lines; i++) {
	       c = combo_new();
	       gtk_widget_size_request(c,&req);
#ifdef COMBO_OLDSCHOOL
	       gtk_widget_set_usize(c,req.width/2,req.height);
#endif
	       sd->ca[i] = COMBO(c);
	       combo_set_items(COMBO(c),l,0);
	       gtk_table_attach(GTK_TABLE(b),c,4,5,i+1,i+2,GTK_FILL,0,0,0);
	  }
	  g_list_free(l);
     }
     b = gtk_label_new(_("(Lines with delay=0 will be ignored.)"));
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     gtk_widget_show_all(a);
}

static void toggle_sensitive(GtkToggleButton *togglebutton, gpointer user_data)
{
     gtk_widget_set_sensitive(GTK_WIDGET(user_data),
			      gtk_toggle_button_get_active(togglebutton));
}

static void sox_dialog_browser_setup(EffectDialog *ed)
{
     SoxDialog *sd = SOX_DIALOG(ed);
     GtkWidget *a,*b,*c,*w1;
     GList *l=NULL;
     guint i;
     
     if (!strcmp(ed->effect_name,"echo")) {
	  setup_echos(ed,_("Input gain: "),ECHOS_MAX,TRUE,FALSE,FALSE,FALSE);
     } else if (!strcmp(ed->effect_name,"echos")) {
	  setup_echos(ed,_("Input gain: "),ECHOS_MAX,TRUE,FALSE,FALSE,FALSE);
     } else if (!strcmp(ed->effect_name,"reverb")) {
	  setup_echos(ed,_("Output gain: "),ECHOS_MAX,FALSE,FALSE,FALSE,FALSE);
     } else if (!strcmp(ed->effect_name,"chorus")) {
	  setup_echos(ed,_("Input gain: "),ECHOS_MAX,TRUE,TRUE,TRUE,TRUE);
     } else if (!strcmp(ed->effect_name,"flanger")) {
	  setup_echos(ed,_("Input gain: "),1,TRUE,TRUE,FALSE,TRUE);
     } else if (!strcmp(ed->effect_name,"phaser")) {
	  setup_echos(ed,_("Input gain: "),1,TRUE,TRUE,FALSE,TRUE);
     } else if (!strcmp(ed->effect_name,"lowp")) {
	  setup_filter(ed,NULL,FALSE);
     } else if (!strcmp(ed->effect_name,"highp")) {
	  setup_filter(ed,NULL,FALSE);
     } else if (!strcmp(ed->effect_name,"band")) {
	  setup_filter(ed,_("Width: "),TRUE);
     } else if (!strcmp(ed->effect_name,"lowpass")) {
	  setup_filter(ed,NULL,FALSE);
     } else if (!strcmp(ed->effect_name,"highpass")) {
	  setup_filter(ed,NULL,FALSE);
     } else if (!strcmp(ed->effect_name,"bandpass")) {
	  setup_filter(ed,_("Bandwidth: "),FALSE);
     } else if (!strcmp(ed->effect_name,"bandreject")) {
	  setup_filter(ed,_("Bandwidth: "),FALSE);
     } else if (!strcmp(ed->effect_name,"filter")) {

	  sd->i1 = inifile_get_guint32("sox_filter_type",0);
	  if  (sd->i1 < 0 || sd->i1 > 2) sd->i1=0;	  
	  
	  a = gtk_table_new(6,3,FALSE);
	  gtk_container_add(GTK_CONTAINER(ed->input_area),a);

	  attach_label(_("Filter type: "),a,0,0);
	  attach_label(_("Low 6dB corner: "),a,1,0);
	  attach_label(_("Hz"),a,1,2);
	  attach_label(_("High 6db corner: "),a,2,0);
	  attach_label(_("Hz"),a,2,2);
	  attach_label(_("Window length: "),a,4,0);
	  attach_label(_("samples"),a,4,2);
	  attach_label(_("Beta"),a,5,0);

	  w1 = b = combo_new();
	  l = g_list_append(l,_("Lowpass"));
	  l = g_list_append(l,_("Highpass"));
	  l = g_list_append(l,_("Bandpass"));
	  combo_set_items(COMBO(b),l,2);
	  g_list_free(l);	  
	  gtk_table_attach(GTK_TABLE(a),b,1,3,0,1,0,0,0,7);
	  	  
	  b = intbox_new(inifile_get_guint32("sox_filter_low",100));
	  sd->ib1 = INTBOX(b);
	  gtk_table_attach(GTK_TABLE(a),b,1,2,1,2,GTK_FILL,0,0,0);
	  
	  b = intbox_new(inifile_get_gfloat("sox_filter_high",400));
	  sd->ib2 = INTBOX(b);
	  gtk_table_attach(GTK_TABLE(a),b,1,2,2,3,GTK_FILL,0,0,0);
	  
	  b = gtk_fixed_new();
	  gtk_table_attach(GTK_TABLE(a),b,0,3,3,4,GTK_FILL,0,0,4);

	  b = intbox_new(inifile_get_guint32("sox_filter_length",128));
	  sd->ib3 = INTBOX(b);
	  gtk_table_attach(GTK_TABLE(a),b,1,2,4,5,GTK_FILL,0,0,0);

	  b = floatbox_new(inifile_get_gfloat("sox_filter_beta",16.0));
	  sd->fb1 = FLOATBOX(b);
	  gtk_table_attach(GTK_TABLE(a),b,1,2,5,6,GTK_FILL,0,0,0);

	  gtk_signal_connect(GTK_OBJECT(w1),"selection_changed",
			     GTK_SIGNAL_FUNC(sox_filter_type_changed),sd);
	  combo_set_selection(COMBO(w1),sd->i1);
	  gtk_widget_show_all(a);
     } else if (!strcmp(ed->effect_name,"compand")) {
	  a = gtk_vbox_new(FALSE,8);
	  gtk_container_add(GTK_CONTAINER(ed->input_area),a);
	  b = gtk_table_new(2,3,FALSE);
	  gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
	  attach_label(_("Attack integration time: "),b,0,0);
	  c = floatbox_new(inifile_get_gfloat("sox_compand_attack",0.3));
	  gtk_table_attach(GTK_TABLE(b),c,1,2,0,1,0,0,0,0);
	  sd->fb1 = FLOATBOX(c);
	  attach_label(_(" seconds"),b,0,2);
	  attach_label(_("Decay integration time: "),b,1,0);
	  c = floatbox_new(inifile_get_gfloat("sox_compand_decay",1.0));
	  gtk_table_attach(GTK_TABLE(b),c,1,2,1,2,0,0,0,0);
	  sd->fb2 = FLOATBOX(c);
	  attach_label(_(" seconds"),b,1,2);
	  b = gtk_table_new(2,COMPAND_LINES+1,FALSE);
	  gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
	  attach_label(_("Input level (dB)"),b,0,0);
	  attach_label(_("Output level (dB)"),b,1,0);
	  sd->fba[0] = g_malloc(COMPAND_LINES*sizeof(Floatbox *));
	  sd->fba[1] = g_malloc(COMPAND_LINES*sizeof(Floatbox *));
	  for (i=0; i<COMPAND_LINES; i++) {
	       c = floatbox_new(0.0);
	       sd->fba[0][i] = FLOATBOX(c);
	       gtk_table_attach(GTK_TABLE(b),c,i+1,i+2,0,1,0,0,0,0);
	       c = floatbox_new(0.0);
	       sd->fba[1][i] = FLOATBOX(c);
	       gtk_table_attach(GTK_TABLE(b),c,i+1,i+2,1,2,0,0,0,0);
	  }
	  b = gtk_table_new(3,3,FALSE);
	  gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
	  attach_label(_("Post-processing gain: "),b,0,0);
	  c = floatbox_new(inifile_get_gfloat("sox_compand_gain",0.0));
	  gtk_table_attach(GTK_TABLE(b),c,1,2,0,1,0,0,0,0);
	  sd->fb3 = FLOATBOX(c);
	  attach_label(_(" dB"),b,0,2);
	  attach_label(_("Initial volume: "),b,1,0);
	  c = floatbox_new(inifile_get_gfloat("sox_compand_startvol",0.0));
	  gtk_table_attach(GTK_TABLE(b),c,1,2,1,2,0,0,0,0);
	  sd->fb4 = FLOATBOX(c);
	  attach_label(_("Delay time: "),b,2,0);
	  c = floatbox_new(inifile_get_gfloat("sox_compand_delay",0.0));
	  gtk_table_attach(GTK_TABLE(b),c,1,2,2,3,0,0,0,0);
	  sd->fb5 = FLOATBOX(c);
	  attach_label(_(" seconds"),b,2,2);
	  gtk_widget_show_all(a);
     } else if (!strcmp(ed->effect_name,"dcshift")) {
	  a = gtk_table_new(3,2,FALSE);
	  gtk_container_add(GTK_CONTAINER(ed->input_area),a);
	  attach_label(_("Shift amount: "),a,0,0);
	  b = floatbox_new(inifile_get_gfloat("sox_dcshift_amount",0.0));
	  gtk_table_attach(GTK_TABLE(a),b,1,2,0,1,0,0,0,0);
	  sd->fb1 = FLOATBOX(b);
	  b = w1 = gtk_check_button_new_with_label(_("Peak limiter"));
	  gtk_table_attach(GTK_TABLE(a),b,0,2,1,2,GTK_FILL,0,0,0);
	  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),TRUE);
	  sd->tb1 = GTK_TOGGLE_BUTTON(b);
	  attach_label(_("Limiter gain: "),a,2,0);
	  b = floatbox_new(inifile_get_gfloat("sox_dcshift_gain",0.05));
	  gtk_table_attach(GTK_TABLE(a),b,1,2,2,3,0,0,0,0);
	  sd->fb2 = FLOATBOX(b);
	  gtk_signal_connect(GTK_OBJECT(w1),"toggled",
			     GTK_SIGNAL_FUNC(toggle_sensitive),sd->fb2);
	  gtk_toggle_button_set_active
	       (GTK_TOGGLE_BUTTON(w1),
		inifile_get_gboolean("sox_dcshift_limiter",FALSE));
	  gtk_widget_show_all(a);
     } else if (!strcmp(ed->effect_name,"pitch")) {
	  a = gtk_table_new(4,3,FALSE);
	  gtk_container_add(GTK_CONTAINER(ed->input_area),a);
	  attach_label(_("Amount: "),a,0,0);
	  attach_label(_(" cents"),a,0,2);
	  attach_label(_("Window width: "),a,1,0);
	  attach_label(_(" ms"),a,1,2);
	  attach_label(_("Interpolation: "),a,2,0);
	  attach_label(_("Fade: "),a,3,0);
	  b = floatbox_new(inifile_get_gfloat("sox_pitch_amount",0.0));
	  gtk_table_attach(GTK_TABLE(a),b,1,2,0,1,GTK_FILL,0,0,0);
	  sd->fb1 = FLOATBOX(b);
	  b = floatbox_new(inifile_get_gfloat("sox_pitch_width",20.0));
	  gtk_table_attach(GTK_TABLE(a),b,1,2,1,2,GTK_FILL,0,0,0);
	  sd->fb2 = FLOATBOX(b);
	  b = combo_new();
	  gtk_table_attach(GTK_TABLE(a),b,1,3,2,3,0,0,0,0);
	  l = g_list_append(NULL,translate_strip(N_("Interpolation|Cubic")));
	  l = g_list_append(l,translate_strip(N_("Interpolation|Linear")));
	  combo_set_items(COMBO(b),l,0);
	  g_list_free(l);
	  sd->c1 = COMBO(b);
	  b = combo_new();
	  gtk_table_attach(GTK_TABLE(a),b,1,3,3,4,0,0,0,0);
	  l = g_list_append(NULL,translate_strip(N_("Fade|Cos")));
	  l = g_list_append(l,translate_strip(N_("Fade|Hamming")));
	  l = g_list_append(l,translate_strip(N_("Fade|Linear")));
	  l = g_list_append(l,translate_strip(N_("Fade|Trapezoid")));
	  combo_set_items(COMBO(b),l,0);
	  g_list_free(l);
	  sd->c2 = COMBO(b);
	  gtk_widget_show_all(a);
     } else if (!strcmp(ed->effect_name,"stretch")) {
	  a = gtk_table_new(2,3,FALSE);
	  gtk_container_add(GTK_CONTAINER(ed->input_area),a);
	  attach_label(_("Factor: "),a,0,0);
	  attach_label(_("Window size: "),a,1,0);
	  attach_label(_(" ms"),a,1,2);
	  b = floatbox_new(inifile_get_gfloat("sox_stretch_factor",1.0));
	  gtk_table_attach(GTK_TABLE(a),b,1,2,0,1,GTK_FILL,0,0,0);
	  sd->fb1 = FLOATBOX(b);
	  b = floatbox_new(inifile_get_gfloat("sox_stretch_window",20.0));
	  gtk_table_attach(GTK_TABLE(a),b,1,2,1,2,GTK_FILL,0,0,0);
	  sd->fb2 = FLOATBOX(b);
	  gtk_widget_show_all(a);
     } else if (!strcmp(ed->effect_name,"vibro")) {
	  a = gtk_table_new(2,3,FALSE);
	  gtk_container_add(GTK_CONTAINER(ed->input_area),a);
	  attach_label(_("Speed: "),a,0,0);
	  attach_label(_(" Hz"),a,0,2);
	  attach_label(_("Depth: "),a,1,0);
	  b = floatbox_new(inifile_get_gfloat("sox_vibro_speed",5.0));
	  gtk_table_attach(GTK_TABLE(a),b,1,2,0,1,GTK_FILL,0,0,0);
	  sd->fb1 = FLOATBOX(b);
	  b = floatbox_new(inifile_get_gfloat("sox_vibro_depth",0.5));
	  gtk_table_attach(GTK_TABLE(a),b,1,2,1,2,GTK_FILL,0,0,0);
	  sd->fb2 = FLOATBOX(b);
	  gtk_widget_show_all(a);
     } else if (!strcmp(ed->effect_name,"mask") ||
		!strcmp(ed->effect_name,"earwax") ||
		!strcmp(ed->effect_name,"reverse")) {
	  a = gtk_label_new(_("This effect has no options."));
	  gtk_container_add(GTK_CONTAINER(ed->input_area),a);
	  gtk_widget_show(a);
     } else {
	  /* The effect name doesn't match any of the known ones. This
	     shouldn't happen. */
	  g_assert_not_reached();
     }
}

static void sox_dialog_destroy(GtkObject *obj)
{
     SoxDialog *sd = SOX_DIALOG(obj);
     g_free(sd->fba[0]);
     g_free(sd->fba[1]);
     g_free(sd->fba[2]);
     g_free(sd->fba[3]);
     memset(sd->fba,0,sizeof(sd->fba));
     g_free(sd->ca);
     parent_class->destroy(obj);
}

static void sox_dialog_class_init(GtkObjectClass *klass)
{
     parent_class = gtk_type_class(effect_dialog_get_type());
     EFFECT_DIALOG_CLASS(klass)->apply = sox_dialog_apply;
     EFFECT_DIALOG_CLASS(klass)->setup = sox_dialog_browser_setup;
     klass->destroy = sox_dialog_destroy;
}

static void sox_dialog_init(GtkObject *obj)
{
     SoxDialog *sd = SOX_DIALOG(obj);
     sd->fb1 = sd->fb2 = sd->fb3 = sd->fb4 = sd->fb5 = NULL;
     sd->tb1 = NULL;
     sd->ib1 = sd->ib2 = sd->ib3 = NULL;    
     memset(sd->fba,0,sizeof(sd->fba));
     sd->ca = NULL;
     /* We wait with proper initialization until we know
      * which effect we represent (in sox_dialog_mainwindow_set). */
}

GtkType sox_dialog_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "SoxDialog",
	       sizeof(SoxDialog),
	       sizeof(SoxDialogClass),
	       (GtkClassInitFunc)sox_dialog_class_init,
	       (GtkObjectInitFunc)sox_dialog_init
	  };
	  id = gtk_type_unique(effect_dialog_get_type(),&info);	  
     }
     return id;
}

gboolean sox_dialog_register_main(gchar source_tag)
{
     int fd[2],fd2[2],i,j,lb_pos=0;
     pid_t p;
     gchar *c,*d,**s,**sn;
     gchar linebuf[8192];
     gboolean *map;
     int sox_maj=0;

     if (!program_exists("sox")) return FALSE;

     /* Run the command 'sox -h' and try to see which effects it
      * supports. */
     i = pipe(fd);
     j = pipe(fd2);
     if (i == -1 || j == -1) {
	  console_perror(_("Error creating pipe"));
	  if (i == 0) { close(fd[0]); close(fd[1]); }
	  if (j == 0) { close(fd2[0]); close(fd2[1]); }
	  return TRUE;
     }
     p = fork();
     if (p == -1) {
	  console_perror(_("Couldn't fork"));
	  close(fd[0]);
	  close(fd[1]);
	  close(fd2[0]);
	  close(fd2[1]);
	  return TRUE;
     }
     if (p == 0) {
	  /* Child process - run 'sox -h' and catch stdout/stderr output */
	  /* Put stdout descriptor in fd[0] */
	  fd[0] = fd2[1];
	  close_all_files_except(fd,2);
	  if (dup2(fd[0],1)==-1 || dup2(fd[1],2)==-1 || 
	      execlp("sox","sox","-h",NULL)==-1)
	       
	       printf(_("Error running 'sox -h': %s\n"),strerror(errno));
	  else 
	       puts(_("Should not reach this point"));
	  _exit(1);
     } else {
	  /* Parent process - read data */
	  close(fd[1]);
	  close(fd2[1]);
	  /* Put stdout descriptor in fd array */
	  fd[1] = fd2[0];
	  /* Read input */
	  j = 0;
	  while (lb_pos < sizeof(linebuf)-1 && j<2) {
	       i = read(fd[j],linebuf+lb_pos,sizeof(linebuf)-lb_pos-1);
	       if (i == 0) j++; /* Read from other descriptor */
	       if (i < 0) {
		    if (errno == EINTR) continue;
		    console_perror(_("Error reading sox output"));
		    return TRUE;
	       }
	       lb_pos += i;
	  }
	  linebuf[lb_pos] = 0;
	  if (lb_pos == 0) return TRUE;
	  /* printf("Sox output: %s\n",linebuf); */
	  /* Look at first line to see if it's SoX version >= 13 */
	  c = strchr(linebuf,'\n');
	  *c = 0;
	  d = strstr(linebuf,"SoX Version ");
	  if (d != NULL) sox_maj = strtol(d+12,NULL,10);
	  d = strstr(linebuf,"SoX v");
	  if (d != NULL && sox_maj==0) sox_maj = strtol(d+5,NULL,10);
	  if (sox_maj > 13) {
	       /* printf("SoX version %d detected\n",sox_maj); */
	       v14_mode = TRUE;
	  } else if (sox_maj > 12) {
	       /* printf("SoX version %d detected\n",sox_maj); */
	       v13_mode = TRUE;
	  }
	  *c = '\n';
	  /* Scan for available effects */
	  c = strstr(linebuf,"effect: ");
	  if (c == NULL) {
	       c = strstr(linebuf,"effects: ");
	       if (c == NULL) {
		    c = strstr(linebuf,"SUPPORTED EFFECTS: ");
	            if (c == NULL) {
			 c = strstr(linebuf,"\nEFFECTS: ");
			 if (c == NULL) {
			      console_message(_("Unable to detect supported "
						"SoX effects"));
			      return TRUE;
			 }
			 c += 10;
	       	    } else
		    c += 19;
		} else
		c += 9; 
	  } else
	    c += 8;

	  d = strchr(c,'\n');
	  if (d) *d=0;
	  for (d=strtok(c," "); d!=NULL; d=strtok(NULL," ")) {
	       /* printf("SoX supports effect '%s'\n",d); */
	       for (s=supported_effects,
			 map=sox_support_map; *s!=NULL && strcmp(*s,d); 
		    s++,map++) { /*Empty for loop*/ }
	       if (*s != NULL) *map = TRUE;
	  }
	  /* Finished.. */
	  close(fd[0]);
	  wait(NULL);
	  /* Register the effects we found */
	       for (s=supported_effects,sn=supported_effect_names,
			 map=sox_support_map; *s!=NULL; s++,sn++,map++)
		    if (*map) 
			 effect_register_add_effect(source_tag,*s,_(*sn),
						    "Chris Bagwell","");
	  return FALSE;
     }
     return FALSE;
}

void sox_dialog_rebuild_func(gchar source_tag, gpointer user_data)
{
     if (sox_dialog_register_main(source_tag)) 
	  console_message(_("Sox support couldn't be initialized"));
}

EffectDialog *sox_dialog_get_func(gchar *name, gchar source_tag, 
				  gpointer user_data)
{
     char **c;
     gboolean *b;
     for (c=supported_effects,b=sox_support_map; *c!=NULL; c++,b++) {
	  if (*b && !strcmp(*c,name)) 
	       return gtk_type_new(sox_dialog_get_type());
     }
     return NULL;
}

void sox_dialog_register(void)
{
     effect_register_add_source("SoX",'S',sox_dialog_rebuild_func,NULL,
				sox_dialog_get_func,NULL);
}

gchar *sox_dialog_first_effect(void)
{
     gboolean *map;
     gchar **s;
     s = supported_effects;
     map = sox_support_map;
     while (*map == FALSE && *s != NULL) { map++; s++; }
     return *s;
}
