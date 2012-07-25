/*
 * Copyright (C) 2004 2005 2007 2008 2011, Magnus Hjorth
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
#include "inifile.h"
#include "ladspadialog.h"
#include "effectbrowser.h"
#include "int_box.h"
#include "float_box.h"
#include "um.h"
#include "combo.h"
#include "gettext.h"

static gchar *first_effect = NULL;

gchar *ladspa_dialog_first_effect(void)
{
     return first_effect;
}

#if defined(HAVE_LADSPA)

GtkObjectClass *parent_class;

gboolean ladspa_dialog_apply(EffectDialog *ed)
{
     LadspaDialog *ld = LADSPA_DIALOG(ed);
     guint i,j,k;
     Intbox *ib;
     Floatbox *fb;
     GtkToggleButton *tb;
     gchar *c,*d;
     gboolean output_mapped[8] = { 0 };
     gboolean res,is_tb;
     float f,l,u;
     Document *doc = EFFECT_BROWSER(ed->eb)->dl->selected;
     Chunk *chunk = doc->chunk;
     LADSPA_PortRangeHintDescriptor prhd;

     for (i=0; i<ld->effect->numports[0]; i++) {
	  is_tb = FALSE;
	  if (IS_INTBOX(ld->settings[0][i])) {
	       ib = INTBOX(ld->settings[0][i]);
	       if (intbox_check(ib)) return TRUE;	
	       f = (float)ib->val;
	  } else if (IS_FLOATBOX(ld->settings[0][i])) {
	       fb = FLOATBOX(ld->settings[0][i]);
	       if (floatbox_check(fb)) return TRUE;
	       f = fb->val;
	  } else {
	       /* Must be a togglebutton */
	       tb = GTK_TOGGLE_BUTTON(ld->settings[0][i]);
	       if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tb)))
		    f = 1.0;
	       else
		    f = 0.0;
	       is_tb = TRUE;
	  }
	  prhd = ld->effect->ports[0][i].prh.HintDescriptor;
	  l = ld->effect->ports[0][i].prh.LowerBound;
	  u = ld->effect->ports[0][i].prh.UpperBound;
	  if (LADSPA_IS_HINT_SAMPLE_RATE(prhd)) {
	       l *= (gfloat)chunk->format.samplerate;
	       u *= (gfloat)chunk->format.samplerate;
	  }
	  if (LADSPA_IS_HINT_BOUNDED_BELOW(prhd) && f < l) {
	       c = g_strdup_printf(_("Value for '%s' must not be below %f"),
				   ld->effect->ports[0][i].name,l);
	       user_error(c);
	       g_free(c);
	       return TRUE;
	  }
	  if (LADSPA_IS_HINT_BOUNDED_ABOVE(prhd) && f > u) {
	       c = g_strdup_printf(_("Value for '%s' must not be above %f"),
				   ld->effect->ports[0][i].name,
				   u);
	       user_error(c);
	       g_free(c);
	       return TRUE;
	  }
	  c = g_strdup_printf("ladspa_%s_defaultControl%d",ld->effect->id,i);
	  if (is_tb) inifile_set_gboolean(c, f != 0.0);
	  else inifile_set_gfloat(c, f);
	  ld->effect->ports[0][i].value = f;
     }

     for (j=2; j<4; j++)
	  for (i=0; i<ld->effect->numports[j]; i++) {

	       k = combo_selected_index(COMBO(ld->settings[j][i]));

	       d = g_strdup_printf("ladspa_%s_default%s%d",ld->effect->id,
				   (j==2)?"Input":"Output",i);
	       if (k >= ld->channels) {
		    ld->effect->ports[j][i].map = -1;
		    ld->effect->ports[j][i].value = 0.0;
		    inifile_set_gint32(d,-1);
		    g_free(d);
		    continue;
	       }
	       g_assert(ld->channels == chunk->format.channels);
	       g_assert(k < ld->channels);
	       if (j==3){
		    if (output_mapped[k]) {
			 g_free(d);
			 d = g_strdup_printf
			      (_("You have mapped more than one "
			       "output port to channel '%s'. You "
			       "can only map one output port to "
			       "each channel."),channel_name(k,ld->channels));
			 user_error(d);
			 g_free(d);
			 return TRUE;
		    } else output_mapped[k] = TRUE;
	       }
	       ld->effect->ports[j][i].map = k;
	       inifile_set_guint32(d,k);
	       g_free(d);
	  }

     ld->effect->keep = gtk_toggle_button_get_active(ld->keep);
     c = g_strdup_printf("ladspa_%s_defaultKeep",ld->effect->id);
     inifile_set_gboolean(c,ld->effect->keep);
     g_free(c);
     
     res = document_apply_cb(doc,
			     (document_apply_proc)ladspa_run_effect,
			     TRUE,ld->effect);
     if (res) return TRUE;

     for (i=0; i<ld->effect->numports[1]; i++) {
	  c = g_strdup_printf("%f",ld->effect->ports[1][i].value);
	  gtk_label_set_text(GTK_LABEL(ld->settings[1][i]), c);
	  g_free(c);
     }

     return FALSE;
}

static void ladspa_dialog_target_changed(EffectDialog *ed) 
{
     LadspaDialog *ld = LADSPA_DIALOG(ed);
     EffectBrowser *eb = EFFECT_BROWSER(ed->eb);
     /* puts("ladspa_dialog_target_changed");      */
     if (ld->channels != eb->dl->format.channels)
	  effect_browser_invalidate_effect(eb,ld->effect->id,'L');     
}

void ladspa_dialog_setup(EffectDialog *ed)
{
     LadspaDialog *ld = LADSPA_DIALOG(ed);
     LadspaEffect *eff;
     GtkWidget *a,*b,*c,*d,*e,*x;
     guint i,k,n,q;
     gint j;
     float f,u,l;
     GList *li;
     gchar *ch;
     gboolean bo,want_scale;
     Dataformat *format = &(EFFECT_BROWSER(ed->eb)->dl->format);
     LADSPA_PortRangeHintDescriptor prhd;

     /* puts("ladspa_dialog_setup"); */
     ld->effect = eff = ladspa_find_effect(ed->effect_name);
     g_assert(eff != NULL);
     a = gtk_table_new(8,2,FALSE);
     gtk_container_add(ed->input_area, a);
     b = gtk_label_new(eff->name);
     gtk_table_attach(GTK_TABLE(a),b,0,2,0,1,GTK_FILL|GTK_SHRINK,0,0,0);
     gtk_misc_set_alignment(GTK_MISC(b),0.0,0.5);
     ch = g_strdup_printf(_("Author: %s"),eff->maker);
     b = gtk_label_new(ch);
     g_free(ch);
     gtk_table_attach(GTK_TABLE(a),b,0,2,1,2,GTK_FILL|GTK_SHRINK,0,0,0);
     gtk_misc_set_alignment(GTK_MISC(b),0.0,0.5);
     ch = g_strdup_printf(_("Copyright: %s"),eff->copyright);
     b = gtk_label_new(ch);
     g_free(ch);
     gtk_table_attach(GTK_TABLE(a),b,0,2,2,3,GTK_FILL|GTK_SHRINK,0,0,0);
     gtk_misc_set_alignment(GTK_MISC(b),0.0,0.5);
     if (eff->numports[0] > 0) {
	  ld->settings[0] = g_malloc(eff->numports[0] * sizeof(GtkWidget *));
	  b = gtk_frame_new(_(" Input controls "));
	  gtk_table_attach(GTK_TABLE(a),b,0,1,3,4,GTK_FILL,0,0,8);
	  c = gtk_vbox_new(FALSE,3);
	  gtk_container_add(GTK_CONTAINER(b),c);
	  gtk_container_set_border_width(GTK_CONTAINER(c),4);
	  for (i=0; i<eff->numports[0]; i++) {
	       prhd = eff->ports[0][i].prh.HintDescriptor;	       
	       ch = g_strdup_printf("ladspa_%s_defaultControl%d",eff->id,i);
	       if (LADSPA_IS_HINT_TOGGLED(prhd)) {
		    d=gtk_check_button_new_with_label(eff->ports[0][i].name);
		    bo = LADSPA_IS_HINT_DEFAULT_1(prhd);
		    bo = inifile_get_gboolean(ch,bo);
		    gtk_toggle_button_set_active
			 (GTK_TOGGLE_BUTTON(d),
			  LADSPA_IS_HINT_DEFAULT_1(prhd));
		    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
		    ld->settings[0][i] = d;
	       } else {
		    d = gtk_hbox_new(FALSE,3);
		    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
		    e = gtk_label_new(eff->ports[0][i].name);
		    gtk_box_pack_start(GTK_BOX(d),e,FALSE,FALSE,0);

		    u = eff->ports[0][i].prh.UpperBound;
		    l = eff->ports[0][i].prh.LowerBound;
		    if (LADSPA_IS_HINT_SAMPLE_RATE(prhd)) {
			 u *= format->samplerate;
			 l *= format->samplerate;
		    }

		    if (LADSPA_IS_HINT_DEFAULT_MINIMUM(prhd)) f=l;
		    else if (LADSPA_IS_HINT_DEFAULT_LOW(prhd)) {
			 if (LADSPA_IS_HINT_LOGARITHMIC(prhd))
			      f = exp(log(l)*0.75+log(u)*0.25);
			 else
			      f = l*0.75 + u*0.25;
		    } else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(prhd)) {
			 if (LADSPA_IS_HINT_LOGARITHMIC(prhd))
			      f = exp(log(l)*0.5 + log(u)*0.5);
			 else
			      f = l*0.5 + u*0.5;
		    } else if (LADSPA_IS_HINT_DEFAULT_HIGH(prhd)) {
			 if (LADSPA_IS_HINT_LOGARITHMIC(prhd))
			      f = exp(log(l)*0.25+log(u)*0.75);
			 else
			      f = l*0.25 + u*0.75;
		    } else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(prhd)) f=u;
		    else if (LADSPA_IS_HINT_DEFAULT_0(prhd)) f=0.0;
		    else if (LADSPA_IS_HINT_DEFAULT_1(prhd)) f=1.0;
		    else if (LADSPA_IS_HINT_DEFAULT_100(prhd)) f=100.0;
		    else if (LADSPA_IS_HINT_DEFAULT_440(prhd)) f=440.0;
		    else if (LADSPA_IS_HINT_BOUNDED_BELOW(prhd)) f=l;
		    else if (LADSPA_IS_HINT_BOUNDED_ABOVE(prhd)) f=u;
		    else f=0.0;
		    f = inifile_get_gfloat(ch,f);

		    want_scale = (LADSPA_IS_HINT_BOUNDED_BELOW(prhd) &&
				  LADSPA_IS_HINT_BOUNDED_ABOVE(prhd));

		    if (LADSPA_IS_HINT_INTEGER(prhd)) {
			 e = intbox_new((long)f);
			 if (want_scale) {
			      x = intbox_create_scale(INTBOX(e),(long)l,
						      (long)u);
			      gtk_box_pack_end(GTK_BOX(d),x,FALSE,FALSE,0);
			      gtk_widget_set_can_focus(x,FALSE);
			 }
		    } else {
			 e = floatbox_new(f);
			 if (want_scale) {
			      x = floatbox_create_scale(FLOATBOX(e),l,u);
			      gtk_box_pack_end(GTK_BOX(d),x,FALSE,FALSE,0);
			      gtk_widget_set_can_focus(x,FALSE);
			 }
		    }
			 
		    ld->settings[0][i] = e;
		    		    
		    gtk_box_pack_end(GTK_BOX(d),e,FALSE,FALSE,0);
	       }
	       g_free(ch);
	  }
     } else ld->settings[0] = NULL;

     k = format->channels;
     ld->channels = k;

     if (eff->numports[1] > 0) {
	  ld->settings[1] = g_malloc(eff->numports[1] * sizeof(GtkWidget *));
	  b = gtk_frame_new(_(" Output controls "));
	  gtk_table_attach(GTK_TABLE(a),b,0,1,4,5,GTK_FILL,0,0,8);
	  c = gtk_vbox_new(FALSE,3);
	  gtk_container_add(GTK_CONTAINER(b),c);
	  gtk_container_set_border_width(GTK_CONTAINER(c),4);
	  for (i=0; i<eff->numports[1]; i++) {
	       d = gtk_hbox_new(FALSE,3);
	       gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
	       e = gtk_label_new(eff->ports[1][i].name);
	       gtk_box_pack_start(GTK_BOX(d),e,FALSE,FALSE,0);
	       e = gtk_label_new("--");
	       ld->settings[1][i] = e;
	       gtk_box_pack_end(GTK_BOX(d),e,FALSE,FALSE,0);
	  }
     } else ld->settings[1] = NULL;

     li = NULL;
     for (j=0; j<k; j++)
	  li = g_list_append(li,g_strdup(channel_name(j,k)));
     li = g_list_append(li,g_strdup(_("None")));

     for (n=0; n<2; n++) {
	  if (eff->numports[n+2] == 0) {
	       ld->settings[n+2] = NULL;
	       continue;
	  }
	  ld->settings[n+2] = g_malloc(eff->numports[n+2]*sizeof(GtkWidget *));
	  b = gtk_frame_new(n==0 ? _(" Input audio ") : _(" Output audio "));
	  gtk_table_attach(GTK_TABLE(a),b,0,1,5+n,6+n,GTK_FILL,0,0,8);
	  c = gtk_vbox_new(FALSE,3);
	  gtk_container_add(GTK_CONTAINER(b),c);
	  gtk_container_set_border_width(GTK_CONTAINER(c),4);
	  for (i=0; i<eff->numports[n+2]; i++) {
	       d = gtk_hbox_new(FALSE,3);
	       gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
	       e = gtk_label_new(eff->ports[n+2][i].name);
	       gtk_box_pack_start(GTK_BOX(d),e,FALSE,FALSE,0);
	       e = combo_new();
	       ld->settings[n+2][i] = e;
	       gtk_box_pack_end(GTK_BOX(d),e,FALSE,FALSE,0);
	       ch = g_strdup_printf("ladspa_%s_default%s%d",eff->id,
				    (n==0)?"Input":"Output",i);	       

	       q = (i < k) ? i : k;	       
	       j = inifile_get_gint32(ch,q); 
	       g_free(ch);
	       if (j <= -1) 
		    j = k;
	       else if (j >= k) {
		    j = q;
	       }
	       combo_set_items(COMBO(e),li,j);
	  }
     }

     g_list_foreach(li,(GFunc)g_free,NULL);
     g_list_free(li);

     b = gtk_check_button_new_with_label(_("Keep data in unmapped output "
					 "channels"));
     gtk_table_attach(GTK_TABLE(a),b,0,2,7,8,GTK_FILL,0,0,0);
     ch = g_strdup_printf("ladspa_%s_defaultKeep",ld->effect->id);
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),
				  inifile_get_gboolean(ch,TRUE));
     g_free(ch);
     ld->keep = GTK_TOGGLE_BUTTON(b);

     gtk_widget_show_all(a);
}

static void ladspa_dialog_destroy(GtkObject *obj)
{
     LadspaDialog *ld = LADSPA_DIALOG(obj);
     /* puts("ladspa_dialog_destroy"); */
     guint i;
     for (i=0; i<4; i++) {
	  g_free(ld->settings[i]);
	  ld->settings[i] = NULL;
     }
     parent_class->destroy(obj);
}

void ladspa_dialog_class_init(GtkObjectClass *klass)
{
     parent_class = gtk_type_class(effect_dialog_get_type());
     EFFECT_DIALOG_CLASS(klass)->apply = ladspa_dialog_apply;
     EFFECT_DIALOG_CLASS(klass)->setup = ladspa_dialog_setup;
     EFFECT_DIALOG_CLASS(klass)->target_changed = ladspa_dialog_target_changed;
     klass->destroy = ladspa_dialog_destroy;
}

void ladspa_dialog_init(GtkObject *obj)
{
     /* Wait with initialisation until the setup signal */
}

GtkType ladspa_dialog_get_type(void)
{
     static GtkType id=0;
     if (!id) {
	  GtkTypeInfo info = {
	       "LadspaDialog",
	       sizeof(LadspaDialog),
	       sizeof(LadspaDialogClass),
	       (GtkClassInitFunc)ladspa_dialog_class_init,
	       (GtkObjectInitFunc)ladspa_dialog_init
	  };
	  id = gtk_type_unique(effect_dialog_get_type(),&info);
     }
     return id;
}

static void register_func(LadspaEffect *eff)
{
     effect_register_add_effect('L', eff->id, eff->name, eff->maker,
				eff->filename);
     if (first_effect == NULL) first_effect = eff->id;
}

static void ladspa_dialog_rebuild(gchar source_tag, gpointer user_data)
{
     ladspa_rescan();
     ladspa_foreach_effect(register_func);
}

static EffectDialog *ladspa_dialog_get(gchar *name, gchar source_tag,
				       gpointer user_data)
{
     if (ladspa_find_effect(name) == NULL) return NULL;
     return EFFECT_DIALOG(gtk_type_new(ladspa_dialog_get_type()));
}

void ladspa_dialog_register(void)
{
     effect_register_add_source("LADSPA Plugin",'L',ladspa_dialog_rebuild,
				NULL,ladspa_dialog_get,NULL);
}

#else /* matches #if defined(HAVE_LADSPA) */

void ladspa_dialog_register(void)
{
}

#endif /* defined(HAVE_LADSPA) */
