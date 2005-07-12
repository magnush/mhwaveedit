/*
 * Copyright (C) 2003 2004, Magnus Hjorth
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
#include <gdk/gdkkeysyms.h>

#include <string.h>
#include "effectbrowser.h"
#include "volumedialog.h"
#include "speeddialog.h"
#include "sampleratedialog.h"
#include "samplesizedialog.h"
#include "combinechannelsdialog.h"
#include "pipedialog.h"
#include "inifile.h"
#include "mainwindowlist.h"
#include "um.h"
#include "gettext.h"

struct effect {
     gchar *name;
     gchar *title;
     GtkType dialog_type;
};

static GSList *effects = NULL;
static guint num_effects = 0;

static GSList *geometry_stack = NULL;
static gboolean geometry_stack_inited = FALSE;

static GtkObjectClass *parent_class;

static void effect_browser_remove_effect(EffectBrowser *eb);

void effect_browser_register_effect(gchar *name, gchar *title, 
				    GtkType dialog_type)
{
     struct effect *e;
     e = g_malloc(sizeof(*e));
     e->name = g_strdup(name);
     e->title = g_strdup(title);
     e->dialog_type = dialog_type;
     effects = g_slist_append(effects,e);
     num_effects ++;
}

void effect_browser_register_default_effects(void)
{     
     effect_browser_register_effect("volume",_("[B] Volume adjust/fade"),
				    volume_dialog_get_type());
     effect_browser_register_effect("srate",_("[B] Convert samplerate"),
				    samplerate_dialog_get_type());
     effect_browser_register_effect("ssize",_("[B] Convert sample format"),
				    samplesize_dialog_get_type());
     effect_browser_register_effect("combine",_("[B] Combine channels"),
				    combine_channels_dialog_get_type());
     effect_browser_register_effect("speed",_("[B] Speed"),
				    speed_dialog_get_type());
     effect_browser_register_effect("pipe",_("[B] Pipe through program"),
				    pipe_dialog_get_type());
}

static void effect_browser_destroy(GtkObject *obj)
{
     EffectBrowser *eb = EFFECT_BROWSER(obj);
     guint i;
     effect_browser_remove_effect(eb);
     eb->current_dialog = -1;
     if (eb->dialogs != NULL)
	  for (i=0; i<num_effects; i++) 
	       if (eb->dialogs[i] != NULL) {
		    gtk_widget_unref(GTK_WIDGET(eb->dialogs[i]));
	       }
     g_free(eb->dialogs);
     eb->dialogs = NULL;
     if (parent_class->destroy) parent_class->destroy(obj);
}

static void geom_push(EffectBrowser *eb)
{
     gchar *c;
     guint pos;
     /* This seems to be the only way to find out handle position */
     pos = GTK_WIDGET(eb->effect_list_container)->allocation.width;
     c = g_strdup_printf("%d",pos);
     geometry_stack_push(GTK_WINDOW(eb),c,&geometry_stack);
     g_free(c);
}

static gint effect_browser_delete_event(GtkWidget *widget, GdkEventAny *event)
{
     geom_push(EFFECT_BROWSER(widget));
     if (GTK_WIDGET_CLASS(parent_class)->delete_event)
	  return GTK_WIDGET_CLASS(parent_class)->delete_event(widget,event);
     else
	  return FALSE;
}

static void effect_browser_class_init(GtkObjectClass *klass)
{
     parent_class = gtk_type_class(gtk_window_get_type());
     klass->destroy = effect_browser_destroy;
     GTK_WIDGET_CLASS(klass)->delete_event = effect_browser_delete_event;
}

static void apply_click(GtkWidget *widget, EffectBrowser *eb)
{
     if (eb->mwl->selected == NULL) {
	  user_error(_("You have no open file to apply the effect to!"));
	  return;
     }
     effect_dialog_apply(eb->dialogs[eb->current_dialog]);     
     if (!inifile_get_gboolean("mainwinFront",TRUE))
	  gdk_window_raise(GTK_WIDGET(eb)->window);
}

static void effect_browser_close(EffectBrowser *eb)
{
     geom_push(eb);
     gtk_widget_destroy(GTK_WIDGET(eb));
}

static void ok_click(GtkWidget *widget, EffectBrowser *eb)
{
     if (eb->mwl->selected == NULL) {
	  user_error(_("You have no open file to apply the effect to!"));
	  return;
     }
     gtk_widget_hide(GTK_WIDGET(eb));
     if (effect_dialog_apply(eb->dialogs[eb->current_dialog]))
	  gtk_widget_show(GTK_WIDGET(eb));
     else
	  effect_browser_close(eb);
}

static void effect_browser_remove_effect(EffectBrowser *eb)
{
     if (eb->current_dialog >= 0) 
	  gtk_container_remove
	       (GTK_CONTAINER(eb->dialog_container),
		GTK_WIDGET(eb->dialogs[eb->current_dialog]));
}

static void effect_browser_set_effect_main(EffectBrowser *eb, struct effect *e,
					   gint i)
{
     GtkWidget *w;
     effect_browser_remove_effect(eb);
     eb->current_dialog = i;
     if (eb->dialogs[i] == NULL) {
	  w = GTK_WIDGET(gtk_type_new(e->dialog_type));
	  effect_dialog_setup(EFFECT_DIALOG(w),e->name,(gpointer)eb);
	  eb->dialogs[i] = EFFECT_DIALOG(w);
	  gtk_widget_ref(w);
	  gtk_object_sink(GTK_OBJECT(w));
	  inifile_set("lastEffect",e->name);
     }
     gtk_container_add(eb->dialog_container,
		       GTK_WIDGET(eb->dialogs[i]));
     gtk_widget_show(GTK_WIDGET(eb->dialogs[i]));
}

void effect_browser_invalidate_effect(EffectBrowser *eb, gchar *effect_name)
{
     GSList *sl;
     struct effect *e;
     gint i=0;
     sl = effects;
     while (1) {
	  g_assert(sl != NULL);
	  e = (struct effect *)sl->data;
	  if (!strcmp(e->name,effect_name)) break;
	  sl=sl->next;
	  i++;
     }
     if (i == eb->current_dialog) {
	  effect_browser_remove_effect(eb);
	  eb->current_dialog = -1;
	  gtk_widget_unref(GTK_WIDGET(eb->dialogs[i]));
	  eb->dialogs[i] = NULL;
	  effect_browser_set_effect_main(eb,e,i);
     } else if (eb->dialogs[i] != NULL) {
	  gtk_widget_unref(GTK_WIDGET(eb->dialogs[i]));
	  eb->dialogs[i] = NULL;
     }
}

static void effect_browser_select_child(GtkList *list, GtkWidget *widget,
					gpointer user_data)
{
     EffectBrowser *eb = EFFECT_BROWSER(user_data);
     GSList *sl;
     gint i,j;
     i = gtk_list_child_position(eb->effect_list,widget);
     g_assert(i>=0);
     for (sl=effects,j=i; j>0; j--) {
	  sl=sl->next;
	  g_assert(sl!=NULL);
     }
     effect_browser_set_effect_main(eb,(struct effect *)(sl->data),i);
}

static void effect_browser_init(EffectBrowser *eb)
{
     GtkWidget *b,*b1,*b1w,*b2,*b21,*b21w,*b22,*b23,*b24,*b241,*b242;
     GtkWidget *b243,*w;
     GSList *sl;
     GtkAccelGroup* ag;
     struct effect *e;
     gchar *c,*d;
     gint x;

     ag = gtk_accel_group_new();

     eb->dialogs = g_malloc0(num_effects * sizeof(eb->dialogs[0]));
     eb->current_dialog = -1;
     
     b1w = gtk_list_new();
     eb->effect_list = GTK_LIST(b1w);
     gtk_list_set_selection_mode(GTK_LIST(b1w),GTK_SELECTION_SINGLE);

     for (sl=effects; sl!=NULL; sl=sl->next) {
	  e = (struct effect *)sl->data;
	  w = gtk_list_item_new_with_label(e->title);
	  gtk_container_add(GTK_CONTAINER(b1w),w);
	  gtk_widget_show(w);
     }

     b1 = gtk_scrolled_window_new(NULL,NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(b1),
				    GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(b1),b1w);
     gtk_widget_set_usize(GTK_WIDGET(b1),150,150);
     eb->effect_list_container = GTK_CONTAINER(b1);

     b21w = gtk_alignment_new(0.5,0.5,1.0,1.0);
     eb->dialog_container = GTK_CONTAINER(b21w);
     
     b21 = gtk_scrolled_window_new(NULL,NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(b21),
				    GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(b21),b21w);
  
     b22 = gtk_hseparator_new();

     b23 = gtk_hbox_new(FALSE,3);
     eb->mw_list_box = GTK_BOX(b23);

     b241 = gtk_button_new_with_label(_("OK"));
     gtk_widget_add_accelerator (b241, "clicked", ag, GDK_KP_Enter, 0, (GtkAccelFlags) 0);
     gtk_widget_add_accelerator (b241, "clicked", ag, GDK_Return, 0, (GtkAccelFlags) 0);
     gtk_signal_connect(GTK_OBJECT(b241),"clicked",(GtkSignalFunc)ok_click,eb);

     b242 = gtk_button_new_with_label(_("Apply"));
     gtk_signal_connect(GTK_OBJECT(b242),"clicked",(GtkSignalFunc)apply_click,
			eb);
     
     b243 = gtk_button_new_with_label(_("Close"));
     gtk_widget_add_accelerator (b243, "clicked", ag, GDK_Escape, 0, (GtkAccelFlags) 0);
     gtk_signal_connect_object(GTK_OBJECT(b243),"clicked",
			       (GtkSignalFunc)effect_browser_close,
			       GTK_OBJECT(eb));

     b24 = gtk_hbutton_box_new(); 
     gtk_box_pack_start(GTK_BOX(b24),b241,FALSE,TRUE,3);
     gtk_box_pack_start(GTK_BOX(b24),b242,FALSE,TRUE,3);
     gtk_box_pack_start(GTK_BOX(b24),b243,FALSE,TRUE,3);

     b2 = gtk_vbox_new(FALSE,5);     
     gtk_box_pack_start(GTK_BOX(b2),b21,TRUE,TRUE,0);
     gtk_box_pack_end(GTK_BOX(b2),b24,FALSE,FALSE,0);
     gtk_box_pack_end(GTK_BOX(b2),b23,FALSE,TRUE,0);
     gtk_box_pack_end(GTK_BOX(b2),b22,FALSE,TRUE,0);

     b = gtk_hpaned_new();
     gtk_paned_pack1(GTK_PANED(b),b1,FALSE,TRUE);
     gtk_paned_pack2(GTK_PANED(b),b2,TRUE,TRUE);

     gtk_window_set_title(GTK_WINDOW(eb),_("Effects"));
     gtk_window_add_accel_group(GTK_WINDOW (eb), ag);
     gtk_window_set_policy(GTK_WINDOW(eb),FALSE,TRUE,FALSE);

     if (!geometry_stack_inited) {
	  if (inifile_get_gboolean("useGeometry",FALSE))
	       geometry_stack = geometry_stack_from_inifile("effectGeometry");
	  geometry_stack_inited = TRUE;
     }
     if (!geometry_stack_pop(&geometry_stack,&c,GTK_WINDOW(eb))) {
	 gtk_window_set_position (GTK_WINDOW (eb), GTK_WIN_POS_CENTER);
	 gtk_window_set_default_size(GTK_WINDOW(eb),600,300);	 
     } else {
	  if (c != NULL) {
	       x = strtoul(c,&d,10);
	       if (*d == 0 && *c != 0)
		    gtk_paned_set_position(GTK_PANED(b),x);	       
	       g_free(c);
	  }
     }
     gtk_container_set_border_width(GTK_CONTAINER(eb),5);
     gtk_container_add(GTK_CONTAINER(eb),b);
     gtk_widget_show_all(b);
}

GtkType effect_browser_get_type(void)
{
     static GtkType id=0;
     if (!id) {
	  GtkTypeInfo info = {
	       "EffectBrowser",
	       sizeof(EffectBrowser),
	       sizeof(EffectBrowserClass),
	       (GtkClassInitFunc)effect_browser_class_init,
	       (GtkObjectInitFunc)effect_browser_init
	  };
	  id = gtk_type_unique(gtk_window_get_type(),&info);
     }
     return id;
}

GtkWidget *effect_browser_new(Mainwindow *mw)
{
     return effect_browser_new_with_effect(mw,"volume");
}

GtkWidget *effect_browser_new_with_effect(Mainwindow *mw, gchar *effect)
{
     GtkWidget *w;
     EffectBrowser *eb = 
	  EFFECT_BROWSER(gtk_type_new(effect_browser_get_type()));
     gtk_signal_connect(GTK_OBJECT(eb->effect_list),"select_child",
			(GtkSignalFunc)effect_browser_select_child,eb);

     w = mainwindow_list_new(mw);
     gtk_box_pack_end(GTK_BOX(eb->mw_list_box),w,TRUE,TRUE,0);
     gtk_widget_show(w);
     eb->mwl = MAINWINDOW_LIST(w);
     w = gtk_label_new(_("Apply to: "));
     gtk_box_pack_end(GTK_BOX(eb->mw_list_box),w,FALSE,FALSE,0);
     gtk_widget_show(w);     

     if (effect == NULL) effect = inifile_get("lastEffect","volume");
     effect_browser_set_effect(eb,effect);
     if (eb->current_dialog == -1) effect_browser_set_effect(eb,"volume");
     g_assert(eb->current_dialog >= 0);
     return GTK_WIDGET(eb);
}

void effect_browser_set_effect(EffectBrowser *eb, gchar *effect)
{
     struct effect *e;
     GSList *s;
     gint i=0;

     s=effects;
     while (s != NULL) {
	  e=(struct effect *)s->data;
	  if (!strcmp(e->name,effect)) break;
	  s=s->next;
	  i++;
     }
     if (s != NULL) 
	  gtk_list_select_item(eb->effect_list,i);
}

void effect_browser_shutdown(void)
{     
     if (inifile_get_gboolean("useGeometry",FALSE))
	  geometry_stack_save_to_inifile("effectGeometry",geometry_stack);  
}
