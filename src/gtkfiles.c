/*
 * Copyright (C) 2002 2003 2004 2005 2006 2011, Magnus Hjorth
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
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "um.h"
#include "gtkfiles.h"
#include "main.h"
#include "mainloop.h"
#include "inifile.h"
#include "gettext.h"

gboolean report_write_errors = TRUE;

static int highest_fd = 2;

int xopen(char *filename, int flags, int mode)
{
     int fd;
     gchar *c;
     fd = open(filename,flags,mode);
     if (fd == -1) {
	  if (report_write_errors || mode==EFILE_READ) {
	       c = g_strdup_printf(_("Could not open %s: %s"),filename,
				   strerror(errno));
	       user_error(c);
	       g_free(c);
	  } else if (errno != ENOSPC) {
	       c = g_strdup_printf(_("Warning: Unexpected error: %s"),
				   filename);
	       user_perror(c);
	       g_free(c);
	  }
     }     
     if (fd > highest_fd) highest_fd = fd;
     return fd;
}


EFILE *e_fopen(char *filename, int mode)
{
     int fd,flag;
     switch (mode) {
     case EFILE_READ:   flag = O_RDONLY; break;
     case EFILE_WRITE:  flag = O_WRONLY | O_CREAT | O_TRUNC; break;
     case EFILE_APPEND: flag = O_WRONLY | O_CREAT | O_APPEND; break;	
     default: g_assert_not_reached(); return NULL;
     }
     fd = xopen(filename,flag,0666);
     if (fd == -1) return NULL;
     return e_fopen_fd(fd,filename);
}

EFILE *e_fopen_fd(int fd, gchar *virtual_filename)
{
     EFILE *e;
     e = g_malloc(sizeof(*e));
     e->fd = fd;
     e->filename = g_strdup(virtual_filename);
     return e;
}

void close_all_files(void)
{
     close_all_files_except(NULL,0);
}

void close_all_files_except(int *fds, int count)
{
     /* We close one file higher than highest_fd because of some sound
	drivers keep a file handle that wasn't opened through us. */
     int i,j;
     for (i=3; i<=highest_fd+1; i++) {
	  for (j=0; j<count; j++) 
	       if (i == fds[j]) break;	  
	  if (j==count && close(i) == -1 && errno != EBADF) 
	       console_perror("close_all_files");
     }
}

gboolean e_fclose(EFILE *f)
{
     gchar *c;
     gboolean b=FALSE;
     if (close(f->fd) != 0) {
	  c = g_strdup_printf(_("Error closing %s: %s"), f->filename, strerror(errno));
	  user_error(c);
	  g_free(c);
	  b = TRUE;
     }
     g_free(f->filename);
     g_free(f);
     return b;
}

gboolean e_fclose_remove(EFILE *f)
{
     gchar *c;
     gboolean b=FALSE;
     if (close(f->fd) != 0) {
	  c = g_strdup_printf(_("Error closing %s: %s"), f->filename, strerror(errno));
	  user_error(c);	  
	  g_free(c);
	  b=TRUE;
     }
     if (xunlink(f->filename)) b=TRUE;
     g_free(f->filename);
     g_free(f);
     return b;
}

gboolean e_fseek(EFILE *stream, off_t offset, int whence)
{
     char *c;
     if (lseek(stream->fd,offset,whence) == (off_t)-1) {
	  c = g_strdup_printf(_("Could not seek in %s: %s"),stream->filename,
			      strerror(errno));
	  user_error(c);
	  g_free(c);
	  return TRUE;
     }
     return FALSE;
}

gint e_fread_upto(void *data, size_t size, EFILE *stream)
{
     char *c;
     gint r = 0;
     ssize_t i;
     while (size > 0) {
	  i = read(stream->fd, (void *)(((char *)data)+r), size);
	  if (i == 0) return r;
	  if (i < 0) {
	       if (errno == EINTR) continue;
	       c = g_strdup_printf(_("Could not read from %s: %s"),
				   stream->filename,strerror(errno));
	       user_error(c);
	       g_free(c);
	       return -1;
	  }
	  r += i;
	  size -= i;	  
     }
     return r;     
}

gboolean e_fread(void *data, size_t size, EFILE *stream)
{
     gint i;
     gchar *c;
     i = e_fread_upto(data,size,stream);
     if (i < size) {
	  if (i >= 0) {
	       c = g_strdup_printf(_("Unexpected end of file reading from %s"),
				   stream->filename);
	       user_error(c);
	       g_free(c);
	  }
	  return TRUE;
     }
     return FALSE;
}

gboolean e_fwrite(void *data, size_t size, EFILE *stream)
{
     char *c;
     gint w = 0;
     ssize_t i;
     while (size > 0) {
	  i = write(stream->fd, (void *)(((char *)data)+w), size);
	  if (i == 0) {
	       c = g_strdup_printf(_("Unable to write data to %s"),
				   stream->filename);
	       user_error(c);
	       g_free(c);
	       return TRUE;
	  }
	  if (i < 0) {
	       if (errno == EINTR) continue;
	       c = g_strdup_printf(_("Could not read from %s: %s"),
				   stream->filename,strerror(errno));
	       user_error(c);
	       g_free(c);
	       return TRUE;
	  }
	  w += i;
	  size -= i;	  
     }
     return FALSE;
}

gboolean e_fwrite0(size_t size, EFILE *stream)
{
     char buf[4096];
     size_t s;
     memset(buf,0,sizeof(buf));
     while (size>0) {
	  s = MIN(size,sizeof(buf));
	  if (e_fwrite(buf,s,stream)) return TRUE;
	  size -= s;
     }
     return FALSE;
}

gboolean e_fread_bswap(void *data, size_t size, EFILE *stream)
{
     size_t i;
     gchar c;
     if (e_fread(data,size,stream)) return TRUE;
     i=0;
     size--;
     while (i<size) {
	  c = ((gchar *)data)[i];
	  ((gchar *)data)[i] = ((gchar *)data)[size];
	  ((gchar *)data)[size] = c;
	  i++;
	  size--;	  
     }
     return FALSE;
}

gboolean e_fwrite_bswap(void *data, size_t size, EFILE *stream)
{
     static gchar *buf = NULL;
     static size_t bufsize = 0;
     size_t i;
     if (size>bufsize) { buf=g_realloc(buf,size); bufsize=size; }
     for (i=0; i<size; i++) buf[i]=((gchar *)data)[size-(1+i)];
     return e_fwrite(buf,size,stream);
}

off_t e_ftell(EFILE *stream)
{
     off_t l;
     char *c;
     l = lseek(stream->fd, 0, SEEK_CUR);
     if (l == -1) {
	  c = g_strdup_printf(_("Could not get file position in %s: %s"),
			      stream->filename, strerror(errno));
	  user_error(c);
	  g_free(c);
     }
     return l;
}

gboolean is_same_file(char *filename1, char *filename2)
{
     struct stat s1, s2;

     if (!strcmp(filename1,filename2)) return TRUE;
     if (stat(filename1,&s1)) {
	  if (errno == ENOENT) return FALSE;
	  else return TRUE;
     }
     if (stat(filename2,&s2)) {
	  if (errno == ENOENT) return FALSE;
	  else return TRUE;
     }
     return (s1.st_dev==s2.st_dev && s1.st_ino==s2.st_ino);
}

/* Removes double slashes */
static void cleanup_filename(gchar *fn)
{
     gchar *c;
     while (1) {
	  c = strstr(fn,"//");
	  if (c == NULL) return;
	  while (*c != 0) { c[0]=c[1]; c++; }
     }     
}

#if (GTK_MAJOR_VERSION < 2) || (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 4) || defined(DISABLE_FILECHOOSER)

/* Use the GtkFileSelection */

static gchar *get_filename_result;
static gboolean get_filename_quitflag;
static gboolean get_filename_savemode;
static GtkFileSelection *get_filename_fs;

static void get_filename_callback(GtkButton *button, gpointer user_data)
{
     GtkFileSelection *fs = GTK_FILE_SELECTION(user_data);
     gchar *c;
     if (button==GTK_BUTTON(fs->ok_button)) {
	  if (get_filename_savemode && 
	      file_exists((gchar *)gtk_file_selection_get_filename(fs))) {
	       if (user_message(_("File already exists. Overwrite?"),
				UM_YESNOCANCEL) != MR_YES) {
		    gtk_signal_emit_stop_by_name(GTK_OBJECT(button),"clicked");
		    return;
	       }
	  } else if (!get_filename_savemode &&
		     !file_exists((gchar *)
				  gtk_file_selection_get_filename(fs))) {
	       user_error(_("No file with that name!"));
	       gtk_signal_emit_stop_by_name(GTK_OBJECT(button),"clicked");
	       return;	       
	  }
	  if (get_filename_result) free(get_filename_result);
	  get_filename_result = g_strdup(gtk_file_selection_get_filename(fs));
	  if (inifile_get_gboolean("useGeometry",FALSE)) {
	       c = get_geom(GTK_WINDOW(fs));
	       inifile_set("fileGeometry",c);
	       g_free(c);
	  }
     }
}

static void get_filename_destroy(void)
{
     get_filename_quitflag = TRUE;
}

static gchar *get_filename_internal_get_name(void)
{
     return g_strdup(gtk_file_selection_get_filename(get_filename_fs));
}

static void get_filename_internal_set_name(gchar *new_name)
{
     gtk_file_selection_set_filename(get_filename_fs,new_name);
}

gchar *get_filename(gchar *current_name, gchar *filemask, gchar *title_text,
		    gboolean savemode, GtkWidget *custom_widget)
{
     GtkFileSelection *f;
     GtkAllocation all;
     gchar *c;

     get_filename_result=NULL;
     get_filename_savemode = savemode;
     f=GTK_FILE_SELECTION(gtk_file_selection_new(title_text));
     get_filename_fs = f;
     if (custom_widget != NULL)
	  gtk_box_pack_end(GTK_BOX(GTK_FILE_SELECTION(f)->main_vbox),
			   custom_widget, FALSE, FALSE, 0);
     c = inifile_get("fileGeometry",NULL);     
     if (c!=NULL && inifile_get_gboolean("useGeometry",FALSE) &&
	 !parse_geom(c,&all)) {
	  gtk_window_set_default_size(GTK_WINDOW(f),all.width,all.height);
	  gtk_widget_set_uposition(GTK_WIDGET(f),all.x,all.y);
     } else
	  gtk_window_set_position(GTK_WINDOW(f),GTK_WIN_POS_CENTER);
     if (current_name) gtk_file_selection_set_filename(f,current_name);
     if (filemask) gtk_file_selection_complete(f,filemask);
     gtk_signal_connect(GTK_OBJECT(f),"destroy",
			GTK_SIGNAL_FUNC(get_filename_destroy),NULL);
     gtk_signal_connect(GTK_OBJECT(f->ok_button),"clicked",
			GTK_SIGNAL_FUNC(get_filename_callback),f);
     gtk_signal_connect(GTK_OBJECT(f->cancel_button),"clicked",
			GTK_SIGNAL_FUNC(get_filename_callback),f);
     gtk_signal_connect_object(GTK_OBJECT(f->ok_button),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(f));
     gtk_signal_connect_object(GTK_OBJECT(f->cancel_button),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(f));
     gtk_window_set_modal(GTK_WINDOW(f),TRUE);
     gtk_widget_show(GTK_WIDGET(f));

     /* gtk_window_maximize(GTK_WINDOW(f)); */


     get_filename_quitflag=FALSE;
     while (!get_filename_quitflag) mainloop();
     if (get_filename_result != NULL)
	  cleanup_filename(get_filename_result);
     return get_filename_result;     
}

static void get_directory_callback(GtkButton *button, gpointer user_data)
{
     GtkFileSelection *fs = GTK_FILE_SELECTION(user_data);
     gchar *c;
     get_filename_result = g_strdup(gtk_file_selection_get_filename(fs));
     c = strchr(get_filename_result,0);
     c--;
     while (c>get_filename_result && *c=='/') {
	  *c = 0;
	  c--;
     }
     if (inifile_get_gboolean("useGeometry",FALSE)) {
	  c = get_geom(GTK_WINDOW(fs));
	  inifile_set("dirGeometry",c);
	  g_free(c);
     }
}

gchar *get_directory(gchar *current_name, gchar *title_text)
{
     GtkFileSelection *f;
     gchar *c;
     GtkAllocation all;

     f = GTK_FILE_SELECTION(gtk_file_selection_new(title_text));
     c = inifile_get("dirGeometry",NULL);
     if (c!=NULL && inifile_get_gboolean("useGeometry",FALSE) &&
	 !parse_geom(c,&all)) {
	  gtk_window_set_default_size(GTK_WINDOW(f),all.width,all.height);
	  gtk_widget_set_uposition(GTK_WIDGET(f),all.x,all.y);
     } else
	  gtk_window_set_position(GTK_WINDOW(f),GTK_WIN_POS_CENTER);
     if (current_name) gtk_file_selection_set_filename(f,current_name);
     gtk_signal_connect(GTK_OBJECT(f),"destroy",
			GTK_SIGNAL_FUNC(get_filename_destroy),NULL);
     gtk_signal_connect(GTK_OBJECT(f->ok_button),"clicked",
			GTK_SIGNAL_FUNC(get_directory_callback),f);
     gtk_signal_connect(GTK_OBJECT(f->cancel_button),"clicked",
			GTK_SIGNAL_FUNC(get_directory_callback),f);
     gtk_signal_connect_object(GTK_OBJECT(f->ok_button),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(f));
     gtk_signal_connect_object(GTK_OBJECT(f->cancel_button),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(f));
     gtk_window_set_modal(GTK_WINDOW(f),TRUE);
     gtk_widget_set_sensitive(GTK_WIDGET(f->file_list),FALSE);
     gtk_widget_show(GTK_WIDGET(f));

     get_filename_quitflag=FALSE;
     while (!get_filename_quitflag) mainloop();
     if (get_filename_result != NULL) cleanup_filename(get_filename_result);
     return get_filename_result;     
     
}

#else

/* Use the GtkFileChooserDialog */

static GtkFileChooser *get_filename_fc;

struct response {
     gboolean savemode;
     gboolean responded;
     gint r;
};

static void response(GtkDialog *dialog, gint arg1, gpointer user_data)
{
     struct response *sr = (struct response *)user_data;
     gchar *c;
     if (sr->savemode && arg1 == GTK_RESPONSE_ACCEPT) {
	  c = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	  if (c != NULL && file_exists(c)) {
	       if (user_message(_("File already exists. Overwrite?"),
				UM_YESNOCANCEL) != MR_YES)
		    return;
	  }
     }
     sr->responded = TRUE;
     sr->r = arg1;
}

static gchar *get_filename_internal_get_name(void)
{
     gchar *c;
     c = gtk_file_chooser_get_filename(get_filename_fc);
     if (c == NULL) return g_strdup("");
     return c;
}

static void get_filename_internal_set_name(gchar *new_name)
{
     gchar *c,*d;
     c = g_filename_to_utf8(new_name,-1,NULL,NULL,NULL);
     gtk_file_chooser_set_filename(get_filename_fc,c);
     d = strrchr(c,'/');
     if (d != NULL && d[1] != 0)
	  gtk_file_chooser_set_current_name(get_filename_fc,d+1);
     g_free(c);
}

static gchar *get_filename_main(gchar *current_name, gchar *title_text, 
				gboolean savemode, 
				GtkFileChooserAction action, 
				gchar *geom_setting, GtkWidget *custom_widget)
{
     GtkWidget *w;
     GtkFileChooser *fc;
     gchar *c,*d;
     GtkAllocation all;
     struct response sr = { savemode, FALSE, 0 };
     if (current_name != NULL)
         c = g_filename_to_utf8(current_name,-1,NULL,NULL,NULL);
     else
         c = NULL;
     w = gtk_file_chooser_dialog_new(title_text,NULL,
				     action,
				     savemode?GTK_STOCK_SAVE_AS:GTK_STOCK_OPEN,
				     GTK_RESPONSE_ACCEPT,GTK_STOCK_CANCEL,
				     GTK_RESPONSE_CANCEL,NULL);
     fc = GTK_FILE_CHOOSER(w);
     get_filename_fc = fc;
     if (custom_widget != NULL)
	  gtk_file_chooser_set_extra_widget(fc, custom_widget);
     
     d = inifile_get(geom_setting,NULL);
     if (d!=NULL && !savemode && inifile_get_gboolean("useGeometry",FALSE) &&
	 !parse_geom(d,&all)) {
	  gtk_window_set_default_size(GTK_WINDOW(w),all.width,all.height);
	  gtk_widget_set_uposition(GTK_WIDGET(w),all.x,all.y);
     } else
	  gtk_window_set_position(GTK_WINDOW(w),GTK_WIN_POS_CENTER);
     gtk_window_set_modal(GTK_WINDOW(w),TRUE);

     if (c != NULL) {
	  if (c[0] == '/' &&
	      (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER || 
	       file_is_directory(c))) {
	       gtk_file_chooser_set_current_folder(fc,c); 
	  } else {
	       gtk_file_chooser_set_filename(fc,c);
	       if (savemode) {
		    d = strrchr(c,'/');
		    if (d != NULL && d[1] != 0)
			 gtk_file_chooser_set_current_name(fc,d+1);
	       }
	       g_free(c);
	  }
     }
     gtk_signal_connect(GTK_OBJECT(fc),"response",GTK_SIGNAL_FUNC(response),
			&sr);
     gtk_widget_show(w);
     while (!sr.responded) mainloop(); 
     c = NULL;
     if (sr.r == GTK_RESPONSE_ACCEPT) 
	  c = gtk_file_chooser_get_filename(fc);
     if (sr.r != GTK_RESPONSE_DELETE_EVENT) {
	  if (!savemode && inifile_get_gboolean("useGeometry",FALSE)) {
	       d = get_geom(GTK_WINDOW(w));
	       inifile_set(geom_setting,d);
	       g_free(d);
	  }
	  gtk_widget_destroy(w);
     }
     if (c != NULL) cleanup_filename(c);
     return c;
}

gchar *get_filename(gchar *current_name, gchar *filemask, gchar *title_text,
		    gboolean savemode, GtkWidget *custom_widget)
{
     return get_filename_main(current_name,title_text,savemode,
			      savemode?GTK_FILE_CHOOSER_ACTION_SAVE:
			      GTK_FILE_CHOOSER_ACTION_OPEN,"fileGeometry",
			      custom_widget);
}

gchar *get_directory(gchar *current_name, gchar *title_text)
{
     return get_filename_main(current_name,title_text,FALSE,
			      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
			      "dirGeometry",NULL);
}

#endif

void get_filename_modify_extension(gchar *new_extension)
{
     gchar *c,*d,*e,*f;
     c = get_filename_internal_get_name();
     d = strrchr(c,'.');
     e = strrchr(c,'/');
     if (d != NULL && (e == NULL || e < d))
	  *d = 0;
     if (c[0] != 0 && (e == NULL || e[1] != 0)) {
	  /* printf("%s\n",c); */
	  f = g_strdup_printf("%s%s",c,new_extension);
	  get_filename_internal_set_name(f);
	  g_free(f);
     }
     g_free(c);
}

gboolean e_copydata(EFILE *from, EFILE *to, off_t bytes)
{
     off_t count;
     size_t rest;
     guint8 buf[4096];
     count = bytes / sizeof(buf);
     rest = bytes % sizeof(buf);
     for (; count>0; count--)
	  if (e_fread(buf,sizeof(buf),from) ||
	      e_fwrite(buf,sizeof(buf),to)) return TRUE;
     if (rest>0) 
	  return (e_fread(buf,rest,from) || 
		  e_fwrite(buf,rest,to));
     else
	  return FALSE;
}

off_t errdlg_filesize(gchar *filename)
{
     EFILE *f;
     off_t r;
     
     f = e_fopen(filename,EFILE_READ);
     if (f == NULL) return -1;
     if (e_fseek(f,0,SEEK_END)) { e_fclose(f); return -1; }
     r = e_ftell(f);
     e_fclose(f);
     return r;
}

gboolean errdlg_copyfile(gchar *from, gchar *to)
{
     off_t l;
     EFILE *in, *out;
     gboolean b;
     l = errdlg_filesize ( from );
     if ( l == -1 ) return TRUE;
     in = e_fopen(from,EFILE_READ);
     if (!in) return TRUE;
     out = e_fopen(to,EFILE_WRITE);
     if (!out) { e_fclose(in); return TRUE; }
     b = e_copydata(in,out,l);
     e_fclose(in);
     if (b) e_fclose_remove(out);
     else e_fclose(out);
     return b;
}

gboolean file_exists(char *filename)
{
     int i;
     struct stat ss;
     i = stat(filename,&ss);
     if (i && errno==ENOENT) return FALSE;
     return TRUE;
}

gboolean program_exists(char *progname)
{
     gchar *c,*d,*e;
     c = getenv("PATH");
     if (c == NULL) c="/bin:/usr/bin";
     c = g_strdup(c);
     for (d=strtok(c,":"); d!=NULL; d=strtok(NULL,":")) {
	  e = g_strdup_printf("%s/%s",d,progname);
	  if (file_exists(e)) {
	       g_free(e);
	       g_free(c);
	       return TRUE;
	  }
	  g_free(e);
     }
     g_free(c);
     return FALSE;
}

gboolean file_is_normal(char *filename)
{
     struct stat s;
     int i;
     i = stat(filename,&s);
     return (i==0 && S_ISREG(s.st_mode));
}

gboolean file_is_directory(char *filename)
{
     struct stat s;
     int i;
     i = stat(filename,&s);
     return (i==0 && S_ISDIR(s.st_mode));
}

static int e_fgetc(EFILE *stream)
{
     unsigned char c;
     ssize_t i;
     gchar *m;
     while (1) {
	  i = read(stream->fd,&c,1);
	  if (i == 0) return -1;
	  if (i == -1) {
	       if (errno == EINTR) continue;
	       m = g_strdup_printf(_("Error reading from %s: %s"),
				   stream->filename,strerror(errno));
	       user_error(m);
	       g_free(m);
	       return -1;
	  }
	  return (int)c;
     }
}

long int e_readline(gchar **line, size_t *size, EFILE *stream)
{
     size_t s = 0;
     int c;
     while (1) {
	  if (s == *size) {
	       *size = *size ? *size * 2 : 32;
	       *line = g_realloc(*line, *size);
	  }
	  c = e_fgetc(stream);
	  if (c==EOF || (c=='\n' && s>0)) { (*line)[s]=0; return s; }
	  if (c=='\n') return e_readline(line,size,stream);
	  (*line)[s] = (gchar)c;
	  s++;
     }
}

gboolean xunlink(gchar *filename)
{
     gchar *c;

     if (unlink(filename) == -1 && errno != ENOENT) {
	  c = g_strdup_printf(_("Could not remove '%s': %s"),filename,
			      strerror(errno));
	  user_error(c);
	  g_free(c);
	  return TRUE;
     }
     return FALSE;
}

gint xrename(gchar *oldname, gchar *newname, gboolean allow_copy)
{
     gchar *c;

     g_assert(strcmp(oldname,newname) != 0);

     if (rename(oldname,newname) == 0) return 0;
     if (errno == EXDEV) {
	  if (allow_copy) return errdlg_copyfile(oldname,newname)?1:0;
	  else return 2;
     } else {
	  c = g_strdup_printf(_("Error creating link to '%s': %s"),oldname,
			      strerror(errno));
	  user_error(c);
	  g_free(c);
	  return 1;
     }
}

gchar *make_filename_rooted(gchar *name)
{
     gchar *c,*d;
     if (name[0] == '/' || name[0] == 0) return g_strdup(name);
     c = g_get_current_dir();
     d = g_strdup_printf("%s/%s",c,name);
     g_free(c);
     cleanup_filename(d);
     return d;
}

gboolean fd_canwrite(int fd)
{
     fd_set set;
     struct timeval tv = {};
     int i;
     FD_ZERO(&set);
     FD_SET(fd,&set);
     i = select(fd+1,NULL,&set,NULL,&tv);
     if (i == 0) return FALSE;
     else return TRUE;
}

gboolean fd_canread(int fd)
{
     fd_set set;
     struct timeval tv = {};
     int i;
     FD_ZERO(&set);
     FD_SET(fd,&set);
     i = select(fd+1,&set,NULL,NULL,&tv);
     if (i == 0) return FALSE;
     else return TRUE;
}

gboolean e_fread_u16_xe(guint16 *data, EFILE *stream, gboolean be)
{
     unsigned char buf[2];
     guint16 i;
     if (e_fread(buf,2,stream)) return TRUE;
     if (be) { 
	  i=buf[0];
	  i<<=8; i|=buf[1];
     } else {
	  i=buf[1];
	  i<<=8; i|=buf[0];
     }
     *data = i;
     return FALSE;
}

gboolean e_fread_u32_xe(guint32 *data, EFILE *stream, gboolean be)
{
     unsigned char buf[4];
     guint32 i;
     if (e_fread(buf,4,stream)) return TRUE;
     if (be) {
	  i=buf[0];
	  i<<=8; i|=buf[1];
	  i<<=8; i|=buf[2];
	  i<<=8; i|=buf[3];
     } else {
	  i=buf[3];
	  i<<=8; i|=buf[2];
	  i<<=8; i|=buf[1];
	  i<<=8; i|=buf[0];
     }
     *data = i;
     return FALSE;
}
