/*
 * Copyright (C) 2006 2009, Magnus Hjorth
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

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include "session.h"
#include "inifile.h"
#include "um.h"
#include "tempfile.h"
#include "gettext.h"
#include "document.h"
#include "mainloop.h"

/* Session states */
#define SESSION_RUNNING 0   /* Currently running in another process */
#define SESSION_SUSPENDED 1 /* Suspended by user */
#define SESSION_CRASHED 2   /* Crash - session file there but process no 
			     * longer existing */
#define SESSION_LEFTOVER 3  /* Temporary files found, but no session file */
#define SESSION_OLD 4       /* Temporary files from version <1.4.8 found */ 
#define SESSION_UNKNOWN 5   /* Session either still running or crashed */

struct session {
     int id;
     int pid;
     int state;
     off_t size;     
     time_t date;
     gchar *logfile;
     GList *datafiles;
};

static int current_id = 0;
static gchar *current_filename;
static gchar *session_dir;
static EFILE *current_file;
static GList *session_list = NULL;
/* static EFILE *current_session = NULL; */

static gint session_compare_func(gconstpointer a, gconstpointer b)
{
     struct session *as = (struct session *)a, *bs = (struct session *)b;
     return (as->date > bs->date)?1:0;
}

void session_init(int *argc, char **argv)
{
     int highest_id = 0;
     gchar *c,*p,*q;
     DIR *d;     
     struct dirent *de;
     int i,j,k,l,m,n;
     char r;
     struct session *s;
     struct stat st;
     GList *list,*list2;
     gboolean b;
     /* Check for session files on the system */
     session_dir = g_strjoin(NULL,get_home_directory(),"/.mhwaveedit",NULL);
     d = opendir(session_dir);
     if (d == NULL) {
	  user_perror(_("Error opening session directory"));
	  return;
     }
     while (1) {
	  de = readdir(d);
	  if (de == NULL) break;
	  i = sscanf(de->d_name,"mhwaveedit-session-%d-%d-%c",&j,&k,&r);
	  if (i < 3) continue;
	  /* Stat the session file once to check mod. date and size */
	  p = g_strdup_printf("%s/%s",session_dir,de->d_name);

	  i = stat(p,&st);
	  if (i < 0) {
	       console_perror(p);
	       g_free(p);
	       continue;
	  } else if (!S_ISREG(st.st_mode)) {
	       q = g_strdup_printf(_("%s: Wrong file type"),p);
	       console_message(q);
	       g_free(q);
	       g_free(p);
	       continue;
	  }

	  s = g_malloc(sizeof(*s));
	  s->id = k;
	  if (k > highest_id) highest_id = k;
	  s->pid = j;
	  s->datafiles = NULL;
	  s->size = st.st_size;
	  s->date = st.st_mtime;
	  s->logfile = p;


	  /* Choose state */
	  if (r == 's') {
	       s->state = SESSION_SUSPENDED;
	  } else {
	       /* Is there a process with the PID running? */
	       i = kill(s->pid, 0);
	       if (i != 0 && errno == ESRCH) {
		    /* Must be a crash */
		    s->state = SESSION_CRASHED;
	       } else {
		    /* Could be running */
		    /* Could check date here to see if reasonable */
		    s->state = SESSION_UNKNOWN;
	       }
	  }

	  session_list = g_list_append(session_list, s);
     }
     closedir(d);

     /* Search for leftover tempfiles */
     for (i=0; ; i++) {
	  c = get_temp_directory(i);
	  if (c == NULL) break;
	  d = opendir(c);
	  if (d == NULL) {
	       console_perror(c);
	       continue;
	  }
	  while (1) {
	       de = readdir(d);
	       if (de == NULL) break;
	       j = sscanf(de->d_name,"mhwaveedit-temp-%d-%d-%d",&k,&l,&m);
	       if (j < 2)
		    continue;
	       /* Try to stat the file */
	       p = g_strdup_printf("%s/%s",c,de->d_name);
	       n = stat(p,&st);
	       if (n < 0) {
		    console_perror(p);
		    g_free(p);
		    continue;
	       } else if (!S_ISREG(st.st_mode)) {
		    q = g_strdup_printf(_("%s: Wrong file type"),p);
		    console_message(q);
		    g_free(p);
		    g_free(q);
		    continue;
	       }
	       /* Try to add to known session */
	       for (list=session_list; list!=NULL; list=list->next) {
		    s = (struct session *)list->data;
		    if ((j>2 && m==s->id) || 
			(j<3 && s->pid == k && s->state==SESSION_OLD))
			 break;
	       }
	       /* Create new session */
	       if (list == NULL) {
		    s = g_malloc0(sizeof(*s));
		    s->pid = k;
		    if (j>2) {
			 s->state = SESSION_LEFTOVER;
			 s->id = m;
		    } else {
			 s->state = SESSION_OLD;
			 s->id = ++highest_id;			 
		    }
		    session_list = g_list_append(session_list, s);
	       }
	       /* Add file to data file list */
	       s->datafiles = g_list_append(s->datafiles, p);
	       /* Update session info */
	       s->size += st.st_size;
	       if (s->date < st.st_mtime) s->date = st.st_mtime;
	  }
	  closedir(d);
     }

     current_id = highest_id+1;

     /* Remove empty sessions unless the session was suspended by user */
     for (list=session_list; list!=NULL; list=list2) {
	  s = (struct session *) list->data;
	  list2 = list->next;
	  if (s->state != SESSION_SUSPENDED && s->datafiles == NULL) {
	       if (s->logfile != NULL) {
		    xunlink(s->logfile);
		    g_free(s->logfile);
	       }
	       session_list = g_list_remove(session_list, s);
	       g_free(s);
	       if (session_list == NULL) break;
	  }
     }

     /* Sort list by date */
     session_list = g_list_sort(session_list, session_compare_func);

     /* Debug */
     /*
     puts("Sessions:");
     for (list=session_list; list!=NULL; list=list->next) {
	  s = (struct session *)list->data;
	  printf("#%d: state=%d, size=%d\n",s->id,s->state,(int)s->size);
	  for (list2=s->datafiles; list2!=NULL; list2=list2->next) {
	       printf("    %s\n",(char *)list2->data);
	  }
	  } */

     current_filename = g_strdup_printf("%s/mhwaveedit-session-%d-%d-r",
					session_dir,(int)getpid(),current_id);

     b = report_write_errors;
     report_write_errors = FALSE;

     current_file = e_fopen(current_filename,EFILE_WRITE);
     if (current_file == NULL)
	  console_perror(_("Could not create session file"));

     report_write_errors = b;

}

gint session_get_id(void)
{
     return current_id;
}

static void session_resume(struct session *s)
{
     GList *l;
     gchar *fn;
     Mainwindow *w;

     g_assert(s->state == SESSION_CRASHED || s->state == SESSION_LEFTOVER ||
	      s->state == SESSION_OLD);
     for (l=s->datafiles; l!=NULL; l=l->next) {
	  fn = (gchar *) l->data;
	  w = MAINWINDOW(mainwindow_new_with_file(fn,FALSE));
	  gtk_widget_show(GTK_WIDGET(w));
	  if (w->doc == NULL) continue;
	  /* This will rename the file to a regular tempfile name for this 
	   * session and change the reference from a regular to a tempfile. 
	   * If the file was converted into a new tempfile by the loader so 
	   * that the original file is no longer being used, it will just be 
	   * removed */
	  datasource_backup_unlink(fn);
	  /* Sets the title name to "untitled X" and makes the document appear 
	   * "changed" so the "save?" dialog pops up when closing etc. */
	  document_forget_filename(w->doc);	  
     }
     /* Remove the session. */
     g_list_foreach(s->datafiles, (GFunc)g_free, NULL);
     g_list_free(s->datafiles);
     g_free(s->logfile);
     session_list = g_list_remove(session_list, s);

     /* Popup notification */
     if (!inifile_get_gboolean("crashMsgShown",FALSE)) {
	  user_message(_("The files that belonged to the crashed session have been "
			 "recovered. Any files that are not saved will be "
			 "removed permanently.\n\n(This notice is only shown once)"), 
		       UM_OK);
	  inifile_set_gboolean("crashMsgShown",TRUE);
     }
}

static gboolean session_delete(struct session *s)
{
     GList *l;
     gchar *fn;
     gboolean b;

     for (l=s->datafiles; l!=NULL; l=l->next) {
	  fn = (gchar *)l->data;
	  b = xunlink(fn);
	  if (b) return TRUE;
     }
     /* Remove the session. */
     g_list_foreach(s->datafiles, (GFunc)g_free, NULL);
     g_list_free(s->datafiles);
     g_free(s->logfile);
     session_list = g_list_remove(session_list, s);
     
     return FALSE;
}

void session_quit(void)
{
     if (current_file != NULL)
	  e_fclose_remove(current_file);
	  
}

struct session_dialog_data {
     GtkList *listwid;
     struct session **listmap;
     GtkWidget *resume_button,*delete_button;
     gboolean destroy_flag, resume_click_flag;
     gint resume_index;
};

static void session_dialog_destroy(GtkObject *obj, gpointer user_data)
{
     struct session_dialog_data *ddata = 
	  (struct session_dialog_data *)user_data;
     ddata->destroy_flag = TRUE;
}

static void session_dialog_select_child(GtkList *list, GtkWidget *widget, 
					gpointer user_data)
{
     struct session_dialog_data *ddata = 
	  (struct session_dialog_data *)user_data;
     ddata->resume_index = gtk_list_child_position(list,widget);
     gtk_widget_set_sensitive(ddata->resume_button,TRUE);
     gtk_widget_set_sensitive(ddata->delete_button,TRUE);     
}


static void session_dialog_exit(GtkWidget *widget, gpointer user_data)
{
     quitflag = TRUE;
}

static void session_dialog_resume_click(GtkWidget *widget, gpointer user_data)
{
     struct session_dialog_data *ddata = 
	  (struct session_dialog_data *)user_data;
     ddata->resume_click_flag = TRUE;
}

static void session_dialog_delete_click(GtkWidget *widget, gpointer user_data)
{
     struct session_dialog_data *ddata = 
	  (struct session_dialog_data *)user_data;
     int i;
     gboolean b;
     i = user_message("Delete session?",UM_OKCANCEL);
     if (i != MR_OK) return;
     b = session_delete(ddata->listmap[ddata->resume_index]);
     if (b) return;
     gtk_list_clear_items(ddata->listwid,ddata->resume_index,
			  ddata->resume_index+1);
     for (i=ddata->resume_index; ddata->listmap[i]!=NULL; i++)
	  ddata->listmap[i] = ddata->listmap[i+1]; 
     gtk_widget_set_sensitive(ddata->resume_button,FALSE);
     gtk_widget_set_sensitive(ddata->delete_button,FALSE);     
}

gboolean session_dialog(void)
{
     struct session_dialog_data ddata;
     GtkWidget *a,*b,*c,*d;
     GList *l = NULL,*m;
     int i;
     struct session *s;
     gchar *ch,*p;
     /* Running and unknown are never shown. Old is also called crash */
     gchar *state_names[] = { NULL, _("Suspended"), _("Crash"),
			      _("Left files"), _("Crash"), NULL };

     if (session_list == NULL) return FALSE;

     ddata.listmap = g_malloc((g_list_length(session_list)+1) * 
			      sizeof(struct session *));
     for (m=session_list,i=0; m!=NULL; m=m->next) {
	  s = (struct session *) m->data;
	  if (s->state == SESSION_RUNNING || s->state == SESSION_UNKNOWN)
	       continue;
	  ch = g_strdup_printf(_("%s on %s(%d files, %ld bytes)"),
			       state_names[s->state],ctime(&(s->date)),
			       g_list_length(s->datafiles),(long)s->size);
	  /* Replace the newline returned by ctime */
	  p = strchr(ch,'\n');
	  if (p) *p=' ';
	  l = g_list_append(l,gtk_list_item_new_with_label(ch));
	  g_free(ch);
	  ddata.listmap[i++] = s;
     }
     ddata.listmap[i] = NULL;

     if (l == NULL) {
	  g_free(ddata.listmap);
	  return FALSE;
     }

     a = gtk_window_new(GTK_WINDOW_DIALOG);
     gtk_window_set_title(GTK_WINDOW(a),_("Sessions"));
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_window_set_default_size(GTK_WINDOW(a),400,200);
     gtk_container_set_border_width(GTK_CONTAINER(a),5);
     gtk_signal_connect(GTK_OBJECT(a),"destroy",
			GTK_SIGNAL_FUNC(session_dialog_destroy),&ddata);
     
     b = gtk_vbox_new(FALSE,5);
     gtk_container_add(GTK_CONTAINER(a),b);
     
     c = gtk_label_new(_("Earlier sessions were found. Choose one to resume or"
			 " start a new session."));
     gtk_label_set_line_wrap(GTK_LABEL(c),TRUE);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);     
   
     c = gtk_scrolled_window_new(NULL,NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(c),GTK_POLICY_NEVER,
				    GTK_POLICY_ALWAYS);
     gtk_box_pack_start(GTK_BOX(b),c,TRUE,TRUE,0);

     d = gtk_list_new();
     ddata.listwid = GTK_LIST(d);
     gtk_list_insert_items(GTK_LIST(d),l,0);
     gtk_signal_connect(GTK_OBJECT(d),"select_child",
			GTK_SIGNAL_FUNC(session_dialog_select_child),&ddata);
     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(c),d);

     c = gtk_hbutton_box_new();
     gtk_box_pack_end(GTK_BOX(b),c,FALSE,TRUE,0);

     d = gtk_button_new_with_label(_("Resume selected"));
     ddata.resume_button = d;
     gtk_widget_set_sensitive(d,FALSE);
     gtk_signal_connect(GTK_OBJECT(d),"clicked",
			GTK_SIGNAL_FUNC(session_dialog_resume_click),&ddata);
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       (GtkObject *)a);
     gtk_container_add(GTK_CONTAINER(c),d);

     d = gtk_button_new_with_label(_("Delete selected"));
     ddata.delete_button = d;
     gtk_widget_set_sensitive(d,FALSE);
     gtk_signal_connect(GTK_OBJECT(d),"clicked",
			GTK_SIGNAL_FUNC(session_dialog_delete_click),&ddata);
     gtk_container_add(GTK_CONTAINER(c),d);

     d = gtk_button_new_with_label(_("Start new session"));
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       (GtkObject *)a);
     gtk_container_add(GTK_CONTAINER(c),d);

     d = gtk_button_new_with_label(_("Exit"));
     gtk_signal_connect(GTK_OBJECT(d),"clicked",
			GTK_SIGNAL_FUNC(session_dialog_exit),&ddata);
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       (GtkObject *)a);
     gtk_container_add(GTK_CONTAINER(c),d);

     c = gtk_hseparator_new();
     gtk_box_pack_end(GTK_BOX(b),c,FALSE,TRUE,0);

     ddata.destroy_flag = ddata.resume_click_flag = FALSE;
     gtk_widget_show_all(a);
     while (!ddata.destroy_flag) mainloop();

     if (ddata.resume_click_flag)
	  session_resume(ddata.listmap[ddata.resume_index]);

     g_free(ddata.listmap);

     return ddata.resume_click_flag;
}
