/*
 * Copyright (C) 2002 2003 2004 2005 2006 2008 2011 2012, Magnus Hjorth
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <gtk/gtk.h>
#include "um.h"
#include "main.h"
#include "mainloop.h"
#include "gettext.h"

gboolean um_use_gtk = FALSE;

int user_message_flag=0;
static int modal_result;
static const int mr_yes=MR_YES,mr_no=MR_NO,mr_cancel=MR_CANCEL,mr_ok=MR_OK;

static void modal_callback(GtkWidget *widget, gpointer data)
{
     modal_result=*((int *)data);
}

/* Output each line of msg with "mhWaveEdit: " in front of it. */
void console_message(const char *msg)
{
     char *b,*c,*d;
     b = c = g_strdup(msg);
     while (1) {
	  fputs(_("mhWaveEdit: "),stderr);
	  d = strchr(c,'\n');
	  if (d != NULL) *d = 0;
	  fputs(c,stderr);
	  fputs("\n",stderr);
	  if (d != NULL) c = d+1;
	  else break;
     }
     g_free(b);
}

void console_perror(const char *msg)
{
     fprintf(stderr,_("mhWaveEdit: %s: %s\n"),msg,strerror(errno));
}

void user_perror(const char *msg){
     char *d;
     d = g_strdup_printf("%s: %s",msg,strerror(errno));
     user_error(d);
     g_free(d);
}

#if GTK_MAJOR_VERSION == 1

static GtkWidget *wnd = NULL;

static void window_destroy(GtkWidget *widget, gpointer user_data)
{
     wnd = NULL;
     // puts("window_destroy!");
}

int do_user_message(char *msg, int type, gboolean block)
{
     GtkWidget *l,*b;
     /* If we're called recursively with UM_OK it's probably some kind of error
      * message so spit it out to stderr... */
     if (!um_use_gtk || (user_message_flag && type==UM_OK)) {
	  g_assert(type == UM_OK);
	  console_message(msg);
	  return MR_OK;
     }
     g_assert(!user_message_flag);
     g_assert(block || type==UM_OK);
     wnd=gtk_dialog_new();
     gtk_window_set_title(GTK_WINDOW(wnd),_("Message"));
     gtk_window_set_modal(GTK_WINDOW(wnd),TRUE);
     gtk_window_set_position(GTK_WINDOW(wnd),GTK_WIN_POS_CENTER);
     /*gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(wnd)->vbox),10);*/
     gtk_container_set_border_width(GTK_CONTAINER(wnd),10);
     if (block) gtk_signal_connect(GTK_OBJECT(wnd),"destroy",GTK_SIGNAL_FUNC(window_destroy),NULL);
     l = gtk_label_new(msg);
     gtk_label_set_line_wrap(GTK_LABEL(l),TRUE);
     gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wnd)->vbox),l,TRUE,FALSE,8);
     gtk_widget_show(l);
     switch (type) {
     case UM_OK:
	  b=gtk_button_new_with_label(_("OK"));
	  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wnd)->action_area),b,FALSE,FALSE,0);
	  if (block) gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(modal_callback),(gpointer)&mr_ok);
	  gtk_signal_connect_object(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(wnd));
	  gtk_widget_show(b);
	  modal_result=MR_OK;
	  break;
     case UM_YESNOCANCEL:
	  b=gtk_button_new_with_label(_("Yes"));
	  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wnd)->action_area),b,
			     FALSE,FALSE,0);
	  gtk_signal_connect(GTK_OBJECT(b),"clicked",
			     GTK_SIGNAL_FUNC(modal_callback),
			     (gpointer)&mr_yes);
	  gtk_signal_connect_object(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(wnd));
	  gtk_widget_show(b);
	  b=gtk_button_new_with_label(_("No"));
	  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wnd)->action_area),b,
			     FALSE,FALSE,0);
	  gtk_signal_connect(GTK_OBJECT(b),"clicked",
			     GTK_SIGNAL_FUNC(modal_callback),
			     (gpointer)&mr_no);
	  gtk_signal_connect_object(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(wnd));
	  gtk_widget_show(b);
	  b=gtk_button_new_with_label(_("Cancel"));
	  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wnd)->action_area),b,
			     FALSE,FALSE,0);
	  gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(modal_callback),(gpointer)&mr_cancel);
	  gtk_signal_connect_object(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(wnd));
	  gtk_widget_show(b);
	  modal_result=MR_CANCEL;
	  break;
     case UM_OKCANCEL:
	  b=gtk_button_new_with_label(_("OK"));
	  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wnd)->action_area),b,
			     FALSE,FALSE,0);
	  gtk_signal_connect(GTK_OBJECT(b),"clicked",
			     GTK_SIGNAL_FUNC(modal_callback),
			     (gpointer)&mr_ok);
	  gtk_signal_connect_object(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(wnd));
	  gtk_widget_show(b);
	  b=gtk_button_new_with_label(_("Cancel"));
	  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wnd)->action_area),b,
			     FALSE,FALSE,0);
	  gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(modal_callback),(gpointer)&mr_cancel);
	  gtk_signal_connect_object(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(wnd));
	  gtk_widget_show(b);
	  modal_result=MR_CANCEL;
	  break;	  
     }
     gtk_widget_show(wnd);
     if (block) {
	  user_message_flag++;
	  while (wnd)
	       mainloop();
	  user_message_flag--;
	  // puts("Out!");
	  return modal_result;
     } else return MR_OK;
}

int user_message(char *msg, int type)
{
     return do_user_message(msg,type,TRUE);
}

void user_info(char *msg)
{
     user_message(msg,UM_OK);
}

void user_error(char *msg)
{
     user_message(msg,UM_OK);
}

void popup_error(char *msg)
{
     do_user_message(msg,UM_OK,FALSE);
}

void user_warning(char *msg)
{
     user_message(msg,UM_OK);
}

#else

static gboolean responded = FALSE;
static gint r;

static void response(GtkDialog *dialog, gint arg1, gpointer user_data)
{
     responded = TRUE;
     r = arg1;
}

static void nonblock_response(GtkDialog *dialog, gint arg1, gpointer user_data)
{
     if (arg1 != GTK_RESPONSE_DELETE_EVENT)
	  gtk_widget_destroy(GTK_WIDGET(dialog));
}

static int showdlg(GtkMessageType mt, GtkButtonsType bt, char *msg, 
		   gboolean block)
{
     GtkWidget *w;
     g_assert(block || bt == GTK_BUTTONS_OK);
     if (!um_use_gtk) {
	  g_assert(bt == GTK_BUTTONS_OK);
	  console_message(msg);
	  return MR_OK;
     }
     w = gtk_message_dialog_new(NULL,GTK_DIALOG_MODAL,mt,bt,"%s",msg);
     if (bt == GTK_BUTTONS_NONE)
	  gtk_dialog_add_buttons(GTK_DIALOG(w),GTK_STOCK_YES,GTK_RESPONSE_YES,
				 GTK_STOCK_NO,GTK_RESPONSE_NO,GTK_STOCK_CANCEL,
				 GTK_RESPONSE_CANCEL,NULL);
     if (block) {
	  gtk_signal_connect(GTK_OBJECT(w),"response",
			     GTK_SIGNAL_FUNC(response),NULL);
	  responded = FALSE;
     } else
	  gtk_signal_connect(GTK_OBJECT(w),"response",
			     GTK_SIGNAL_FUNC(nonblock_response),NULL);
     gtk_window_set_position(GTK_WINDOW(w),GTK_WIN_POS_CENTER);
     gtk_widget_show(w);
     if (block)
	  while (!responded)
	       mainloop();
     else
	  return MR_OK;
     if (r != GTK_RESPONSE_DELETE_EVENT) gtk_widget_destroy(w);
     if (r == GTK_RESPONSE_OK) return MR_OK;
     if (r == GTK_RESPONSE_CANCEL) return MR_CANCEL;
     if (r == GTK_RESPONSE_YES) return MR_YES;
     if (r == GTK_RESPONSE_NO) return MR_NO;
     if (bt == GTK_BUTTONS_OK) return MR_OK;
     return MR_CANCEL;
}

int user_message(char *msg, int type)
{
     if (type == UM_YESNOCANCEL)
	  return showdlg(GTK_MESSAGE_QUESTION,GTK_BUTTONS_NONE,msg,TRUE);
     if (type == UM_OKCANCEL)
	  return showdlg(GTK_MESSAGE_WARNING,GTK_BUTTONS_OK_CANCEL,msg,TRUE);
     return showdlg(GTK_MESSAGE_INFO,GTK_BUTTONS_OK,msg,TRUE);     
}

void user_info(char *msg)
{
     showdlg(GTK_MESSAGE_INFO,GTK_BUTTONS_OK,msg,TRUE);
}

void user_error(char *msg)
{
     showdlg(GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,msg,TRUE);
}

void popup_error(char *msg)
{
     showdlg(GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,msg,FALSE);
}

void user_warning(char *msg)
{
     showdlg(GTK_MESSAGE_WARNING,GTK_BUTTONS_OK,msg,TRUE);
}


#endif

struct user_input_data {
     gboolean (*validator)(gchar *c);
     GtkWidget *entry,*window;
     gchar *result;
};

static gboolean user_input_quitflag;

static void user_input_destroy(void)
{
     user_input_quitflag=TRUE;
}

static void user_input_ok(GtkButton *button, gpointer user_data)
{
     gchar *c;
     struct user_input_data *uid = user_data;
     c = (gchar *)gtk_entry_get_text(GTK_ENTRY(uid->entry));
     if (uid->validator(c)) {
	  uid->result = g_strdup(c);
	  modal_result = MR_OK;
	  gtk_widget_destroy(uid->window);
     }
}

gchar *user_input(gchar *label, gchar *title, gchar *defvalue, 
		  gboolean (*validator)(gchar *c), GtkWindow *below)
{
     GtkWidget *a,*b,*c,*d,*ent;
     struct user_input_data uid;
     a = gtk_window_new(GTK_WINDOW_DIALOG);
     if (below != NULL) {
	  gtk_window_set_transient_for(GTK_WINDOW(a),below);
	  gtk_window_set_position(GTK_WINDOW(a),GTK_WIN_POS_CENTER_ON_PARENT);
     } else
	  gtk_window_set_position(GTK_WINDOW(a),GTK_WIN_POS_CENTER);
     gtk_window_set_title(GTK_WINDOW(a),title?title:_("Input"));
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_container_set_border_width(GTK_CONTAINER(a),5);
     gtk_signal_connect(GTK_OBJECT(a),"destroy",
			GTK_SIGNAL_FUNC(user_input_destroy),NULL);
     b = gtk_vbox_new(FALSE,3);
     gtk_container_add(GTK_CONTAINER(a),b);
     c = gtk_hbox_new(FALSE,2);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     if (label) {
	  d = gtk_label_new(label);
	  gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
     }
     ent = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(ent),defvalue);
#if GTK_MAJOR_VERSION > 1
     gtk_entry_set_activates_default(GTK_ENTRY(ent),TRUE);
#endif
     gtk_box_pack_start(GTK_BOX(b),ent,FALSE,FALSE,0);
     c = gtk_hseparator_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,3);
     c = gtk_hbutton_box_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     d = gtk_button_new_with_label(_("OK"));
     gtk_signal_connect(GTK_OBJECT(d),"clicked",
			GTK_SIGNAL_FUNC(user_input_ok),&uid);
     gtk_container_add(GTK_CONTAINER(c),d);
     GTK_WIDGET_SET_FLAGS(d,GTK_CAN_DEFAULT);
     gtk_widget_grab_default(d);
     d = gtk_button_new_with_label(_("Cancel"));
     gtk_signal_connect(GTK_OBJECT(d),"clicked",GTK_SIGNAL_FUNC(modal_callback),
			(gpointer)&mr_cancel);
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(a));
     gtk_container_add(GTK_CONTAINER(c),d);
     gtk_widget_show_all(a);
     uid.window = a;
     uid.validator = validator;
     uid.entry = ent;
     modal_result = MR_CANCEL;
     user_input_quitflag = FALSE;
     while (!user_input_quitflag)
	  mainloop();
     if (modal_result == MR_CANCEL) return NULL;
     else return uid.result;
}

static gboolean user_input_float_validator(gchar *c)
{
     gchar *d;
     errno = 0;
     strtod(c,&d);
     return (errno == 0 && *d == 0);
}

gboolean user_input_float(gchar *label, gchar *title, gfloat defvalue, 
			  GtkWindow *below, gfloat *result)
{
     gchar *c,d[128],*e;
     format_float(defvalue, d, sizeof(d));
     c = user_input(label,title,d,user_input_float_validator,below);
     if (!c) return TRUE;
     *result = (gfloat)strtod(c,&e);
     g_assert(*e == 0);
     g_free(c);
     return FALSE;     
}

static gpointer user_choice_choice;

static gboolean echo_func(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
     return *((int *)user_data) == MR_OK;
}

static void user_choice_select_child(GtkList *list, GtkWidget *widget,
				     gpointer user_data)
{
     user_choice_choice = gtk_object_get_data(GTK_OBJECT(widget),"choice");
}

gint user_choice(gchar **choices, guint def, gchar *windowtitle, 
		 gchar *windowtext, gboolean allow_cancel)
{
     GtkWidget *a,*b,*c,*d,*list=NULL,*w;
     guint i;
     a = gtk_window_new(GTK_WINDOW_DIALOG);
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_window_set_title(GTK_WINDOW(a), windowtitle?windowtitle:_("Choice"));
     gtk_container_set_border_width(GTK_CONTAINER(a),5);
     if (allow_cancel) 
	  gtk_signal_connect(GTK_OBJECT(a),"delete_event",
			     GTK_SIGNAL_FUNC(echo_func),(gpointer)&mr_cancel);
     else
	  gtk_signal_connect(GTK_OBJECT(a),"delete_event",
			     GTK_SIGNAL_FUNC(echo_func),(gpointer)&mr_ok);
     gtk_signal_connect(GTK_OBJECT(a),"destroy",
			GTK_SIGNAL_FUNC(user_input_destroy),NULL);
     b = gtk_vbox_new(FALSE,5);
     gtk_container_add(GTK_CONTAINER(a),b);
     if (windowtext) {
	  c = gtk_label_new(windowtext);
	  gtk_label_set_line_wrap(GTK_LABEL(c),TRUE);
	  gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
	  c = gtk_hseparator_new();
	  gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     }
     c = gtk_scrolled_window_new(NULL,NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(c),
				    GTK_POLICY_NEVER,
				    GTK_POLICY_AUTOMATIC);
     gtk_widget_set_usize(GTK_WIDGET(c),-1,300);
     gtk_box_pack_start(GTK_BOX(b),c,TRUE,TRUE,0);
     d = list = gtk_list_new();
     gtk_list_set_selection_mode(GTK_LIST(list),GTK_SELECTION_SINGLE);
     for (i=0; choices[i]!=NULL; i++) {
	  w = gtk_list_item_new_with_label(choices[i]);
	  gtk_object_set_data(GTK_OBJECT(w),"choice",choices+i);
	  gtk_container_add(GTK_CONTAINER(list),w);
	  if (i == def) gtk_list_select_child(GTK_LIST(list),w);
     }
     gtk_signal_connect(GTK_OBJECT(list),"select_child",
			(GtkSignalFunc)user_choice_select_child,NULL);
     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(c),d);
     c = gtk_hseparator_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     
     c = gtk_hbutton_box_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     
     d = gtk_button_new_with_label(_("OK"));
     gtk_signal_connect(GTK_OBJECT(d),"clicked",GTK_SIGNAL_FUNC(modal_callback),
			(gpointer)&mr_ok);
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(a));
     gtk_container_add(GTK_CONTAINER(c),d);
     
     if (allow_cancel) {
	  d = gtk_button_new_with_label(_("Cancel"));
	  gtk_signal_connect(GTK_OBJECT(d),"clicked",
			     GTK_SIGNAL_FUNC(modal_callback),
			     (gpointer)&mr_cancel);
	  gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
				    GTK_SIGNAL_FUNC(gtk_widget_destroy),
				    GTK_OBJECT(a));
	  gtk_container_add(GTK_CONTAINER(c),d);
     }

     user_choice_choice = choices+def;
     modal_result = MR_CANCEL;
     gtk_widget_show_all(a);

     user_input_quitflag = FALSE;
     while (!user_input_quitflag)
	  mainloop();
     
     g_assert(modal_result==MR_OK || allow_cancel);

     if (modal_result==MR_CANCEL) return -1;
     else return (guint) (((gchar **)user_choice_choice)-choices);
}

