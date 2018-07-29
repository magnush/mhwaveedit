/*
 * Copyright (C) 2002 2003 2004 2005 2006 2008 2010, Magnus Hjorth
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

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <gtk/gtk.h>

#include "main.h"
#include "pipedialog.h"
#include "um.h"
#include "inifile.h"
#include "tempfile.h"
#include "effectbrowser.h"
#include "gettext.h"

struct pipe_data {
     int fds[3];
     gchar *command;
     GtkWidget *error_log;
};

extern char **environ;

static Chunk *pipe_dialog_apply_proc(Chunk *chunk, StatusBar *bar,
				     gpointer user_data)
{
     gchar *cmd;
     gboolean sendwav;
     PipeDialog *pd = PIPE_DIALOG(user_data);
     Chunk *r;
     off_t clipcount = 0;
     cmd = history_box_get_value(pd->cmd);
     sendwav = gtk_toggle_button_get_active(pd->sendwav);
     inifile_set_gboolean("PipeDialog_sendWav",sendwav);
     r = pipe_dialog_pipe_chunk(chunk,cmd,sendwav,dither_editing,bar,
				&clipcount);
     if (r != NULL && clipwarn(clipcount,TRUE)) {
	  gtk_object_sink(GTK_OBJECT(r));
	  r = NULL;
     }

     return r;
}

static gboolean pipe_dialog_apply(EffectDialog *ed)
{
     gboolean b;
     PipeDialog *pd = PIPE_DIALOG(ed);
     b = document_apply_cb(EFFECT_BROWSER(ed->eb)->dl->selected,
			   pipe_dialog_apply_proc,TRUE,pd);
     history_box_rotate_history(pd->cmd);
     return b;
}

static void pipe_dialog_init(PipeDialog *pd)
{
     GtkWidget *a,*b,*c;
     c = gtk_vbox_new(FALSE,3);
     gtk_container_add(EFFECT_DIALOG(pd)->input_area,c);
     gtk_widget_show(c);
     a = gtk_hbox_new(FALSE,0);
     gtk_box_pack_start(GTK_BOX(c),a,FALSE,FALSE,0);
     gtk_widget_show(a);
     b = gtk_label_new(_("Command line: "));
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     gtk_widget_show(b);
     b = history_box_new("PipeDialog","");
     gtk_box_pack_start(GTK_BOX(a),b,TRUE,TRUE,0);
     gtk_widget_show(b);
     pd->cmd = HISTORY_BOX(b);     
     a = gtk_check_button_new_with_label(_("Send wav header"));
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a),
				  inifile_get_gboolean("PipeDialog_sendWav",
						       FALSE));
     gtk_box_pack_start(GTK_BOX(c),a,FALSE,FALSE,0);
     gtk_widget_show(a);
     pd->sendwav = GTK_TOGGLE_BUTTON(a);

}

static void pipe_dialog_class_init(EffectDialogClass *edc)
{
     edc->apply = pipe_dialog_apply;
}

GtkType pipe_dialog_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "PipeDialog",
	       sizeof(PipeDialog),
	       sizeof(PipeDialogClass),
	       (GtkClassInitFunc) pipe_dialog_class_init,
	       (GtkObjectInitFunc) pipe_dialog_init
	  };
	  id = gtk_type_unique(effect_dialog_get_type(),&info);
     }
     return id;
}

static void add_error_text(GtkWidget *t,gchar *text)
{
#if GTK_MAJOR_VERSION < 2
     gtk_text_insert(GTK_TEXT(t),gtk_style_get_font(t->style),t->style->fg,
		     &(t->style->white),text,-1);
#else
     GtkTextBuffer *tb;
     GtkTextIter iter;
     tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t));
     gtk_text_buffer_get_end_iter(tb,&iter);
     gtk_text_buffer_insert(tb, &iter, text, -1);
#endif
}

static GtkWidget *create_error_window(gchar *command)
{
     GtkWidget *a,*b,*c;
     GtkWidget *t;
     gchar *q;
     GtkAccelGroup* ag;

     ag = gtk_accel_group_new();
     a = gtk_window_new(GTK_WINDOW_DIALOG);
     gtk_window_set_title(GTK_WINDOW(a),_("Process output"));
     gtk_window_set_position (GTK_WINDOW (a), GTK_WIN_POS_CENTER);
     gtk_window_set_default_size(GTK_WINDOW(a),470,200);
     b = gtk_vbox_new(FALSE,0);
     gtk_container_add(GTK_CONTAINER(a),b);
#if GTK_MAJOR_VERSION < 2
     c = gtk_text_new(NULL,NULL);
#else
     c = gtk_text_view_new();
#endif
     t = c;
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_button_new_with_label(_("Close"));
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_KP_Enter, 0, 
				 (GtkAccelFlags) 0);
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_Return, 0, 
				 (GtkAccelFlags) 0);
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_Escape, 0, 
				 (GtkAccelFlags) 0);

     gtk_signal_connect_object(GTK_OBJECT(c),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(a));
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);

     q = g_strdup_printf(_("Output from command '%s':\n\n"),command);
     add_error_text(t,q);
     g_free(q);
     
     gtk_widget_show_all(a);
     gtk_window_add_accel_group(GTK_WINDOW (a), ag);
     
     return t;
}

gpointer pipe_dialog_open_pipe(gchar *command, int *fds, gboolean open_out)
{
     int cmd_in[2]={},cmd_out[2]={},cmd_err[2]={};
     unsigned char syncbyte;
     struct pipe_data *pd;
     pid_t p;
     gchar *c;
     char *argv[4];
     int i;
     
     /* Create pipes */
     if (pipe(cmd_in)==-1 || (open_out && pipe(cmd_out)==-1) || 
	 pipe(cmd_err)==-1) {
	  c = g_strdup_printf(_("Could not create pipe: %s"),strerror(errno));
	  user_error(c);
	  g_free(c);
	  if (!cmd_in[0]) close(cmd_in[0]);
	  if (!cmd_in[1]) close(cmd_in[1]);
	  if (!cmd_out[0]) close(cmd_out[0]);
	  if (!cmd_out[1]) close(cmd_out[1]);
	  if (!cmd_err[0]) close(cmd_err[0]);
	  return NULL;
     }
     /* Fork */
     p = fork();

     if (p == -1) {
	  /* Error (parent process) */
	  c = g_strdup_printf(_("Error: fork: %s"),strerror(errno));
	  user_error(c);
	  g_free(c);
	  close(cmd_in[0]);
	  close(cmd_in[1]);
	  if (open_out) {
	       close(cmd_out[0]);
	       close(cmd_out[1]);
	  }
	  close(cmd_err[0]);
	  close(cmd_err[1]);
	  return NULL;
     } 

     if (p == 0) {
	  /* Child process */
	  /* Redirect streams */
	  /* puts("Child: redirecting streams"); */
	  /* i = dup(1); */
	  dup2(cmd_in[0],0); 
	  if (open_out)
	       dup2(cmd_out[1],1);
	  dup2(cmd_err[1],2);
	  /* Close the streams we don't use */
	  /* write(i,"Child: closing streams\n",23); */
	  close(cmd_in[0]);
	  close(cmd_in[1]);
	  if (open_out) {
	       close(cmd_out[0]);
	       close(cmd_out[1]);
	  }
	  close(cmd_err[0]);
	  close(cmd_err[1]);
	  /* Synchronize */
	  if (open_out) {
	       i = read(0,&syncbyte,1);
	       g_assert(i == 1 && syncbyte == 0xAB);
	       syncbyte = 0xCD;
	       i = write(1,&syncbyte,1);
	       g_assert(i == 1);
	  }
	  /* Execute subprocess */
	  argv[0] = "sh";
	  argv[1] = "-c";
	  argv[2] = command;
	  argv[3] = NULL;
	  /* write(i,"Child: execve!\n",15); */
	  fflush(stdout);
	  close_all_files();
	  if (execve("/bin/sh",argv,environ) == -1) {
	       console_perror("/bin/sh");
	  } else
	       console_message(_("Should not reach this point!"));
	  _exit(1);
     }	  

     /* Parent process */
     /* Close the streams we don't use. */
     close(cmd_in[0]);
     if (open_out)
	  close(cmd_out[1]);
     close(cmd_err[1]);
     
     /* synchronize to child */
     if (open_out) {
	  syncbyte = 0xAB;
	  i = write(cmd_in[1],&syncbyte,1);
	  g_assert(i == 1);
	  i = read(cmd_out[0],&syncbyte,1);
	  g_assert(i == 1 && syncbyte == 0xCD);
     }

     pd = g_malloc(sizeof(*pd));
     pd->fds[0] = fds[0] = cmd_in[1];
     pd->fds[1] = fds[1] = cmd_out[0];
     pd->fds[2] = fds[2] = cmd_err[0];
     pd->error_log = NULL;
     pd->command = g_strdup(command);
     return (gpointer)pd;
}

void pipe_dialog_close(gpointer handle)
{
     struct pipe_data *pd = (struct pipe_data *)handle;
     if (pd->fds[0] != 0) close(pd->fds[0]);
     if (pd->fds[1] != 0) close(pd->fds[1]);
     close(pd->fds[2]);
     g_free(pd->command);
     g_free(pd);
     wait(NULL);
}

gboolean pipe_dialog_error_check(gpointer handle)
{
     struct pipe_data *pd = (struct pipe_data *)handle;
     fd_set set;
     int i;
     struct timeval tv = {};
     gchar c[512];
     FD_ZERO(&set);
     FD_SET(pd->fds[2],&set);
     i = select(pd->fds[2]+1,&set,NULL,NULL,&tv);
     if (i < 1) return FALSE;
     i = read(pd->fds[2],c,sizeof(c)-1);
     if (i == 0) return FALSE;
     c[i] = 0;
     if (!pd->error_log) pd->error_log = create_error_window(pd->command);
     add_error_text(pd->error_log,c);
     return TRUE;
}

void pipe_dialog_close_input(gpointer handle)
{
     struct pipe_data *pd = (struct pipe_data *)handle;
     close(pd->fds[0]);
     pd->fds[0] = 0;
}

#define BZ 65536

static Chunk *pipe_dialog_pipe_chunk_main(Chunk *chunk, gchar *command, 
					  gboolean sendwav, gboolean do_read, 
					  StatusBar *bar, gboolean *sent_all, 
					  int dither_mode, off_t *clipcount)
{
     int fds[3],bs,bp,i;
     gboolean writing=TRUE;
     gboolean send_header = sendwav;
     off_t ui,read_bytes=0;
     gchar *inbuf=NULL,*outbuf,*c;
     ChunkHandle *ch=NULL;
     TempFile ct=0;
     fd_set rset,wset;
     gpointer pipehandle;

     /*
     puts("---");
     puts(command);
     puts("///");
     */
     
     if (sent_all) *sent_all = FALSE;

     pipehandle = pipe_dialog_open_pipe(command, fds, do_read);
     if (pipehandle == NULL) return NULL;

     /* Open chunk and create buffer */
     ch = chunk_open(chunk);
     if (!ch) {
	  pipe_dialog_close(pipehandle);
	  return NULL;
     }
     if (do_read) {
	  ct = tempfile_init(&(chunk->format),FALSE);
	  if (!ct) {
	       chunk_close(ch);
	       pipe_dialog_close(pipehandle);
	       return NULL;
	  }
     }
     status_bar_begin_progress(bar,ch->size,NULL);     
     if (do_read) inbuf = g_malloc(BZ);
     outbuf = g_malloc(BZ);
     ui = 0;
     bs = bp = 0;
     /* We want to handle broken pipes ourselves. */
     signal(SIGPIPE,SIG_IGN);
     /* Read/write data... */
     /* If do_read is TRUE, this loop will keep running until the
      * child process closes it's standard output (=our data
      * input). Otherwise, the loop will keep running until all data has
      * been sent to the program */ 
     while (do_read || writing) {
	  /* Wait for IO */
	  FD_ZERO(&rset);
	  if (do_read) FD_SET(fds[1],&rset);
	  FD_SET(fds[2],&rset);
	  FD_ZERO(&wset);
	  if (writing) FD_SET(fds[0],&wset);
	  /* printf("Waiting (writing=%d)\n",writing); */
	  i = select(FD_SETSIZE,&rset,&wset,NULL,NULL);
	  /* printf("Wait finished.\n"); */
	  if (i == -1) {
	       if (errno == EINTR) continue;
	       goto errno_error_exit;
	  } 
	  
	  /* Read data from standard output */
	  if (FD_ISSET(fds[1],&rset)) {
	       /* printf("Reading from stdin..\n"); */
	       i = read(fds[1],inbuf,BZ);
	       /* printf("Read finished\n"); */
	       if (i == -1) { if (errno != EINTR) goto errno_error_exit; }
	       else if (i == 0) { break; }
	       else { if (tempfile_write(ct,inbuf,i)) goto error_exit; }
	       read_bytes += i;
	       continue;
	  }

	  /* Read data from standard error... */
	  if (FD_ISSET(fds[2],&rset))
	       pipe_dialog_error_check(pipehandle);
	  

	  /* Send data to process's stdin */
	  if (writing && FD_ISSET(fds[0],&wset)) {
	       /* Make sure the buffer contains data */
	       if (bs == bp) {
		    if (send_header) {
			 /* FIXME: Broken for float on bigendian machines */
			 bs = get_wav_header(&(chunk->format),chunk->size,
					     outbuf);
			 bp = 0;
			 send_header = FALSE;
		    } else {
			 if (ui == chunk->length) { 
			      writing = FALSE; 
			      pipe_dialog_close_input(pipehandle);
			      continue; 
			 }
			 i = chunk_read_array(ch,ui,BZ,outbuf, dither_mode,
					      clipcount);
			 if (!i) goto error_exit;
			 bs = i;
			 bp = 0;
			 /* if (IS_BIGENDIAN && sendwav) 
			    byteswap(c,chunk->format.samplesize,bs); */
			 ui += i / chunk->format.samplebytes;
		    }
	       }
	       /* printf("Writing data...\n"); */
	       /* Write data */
#ifdef PIPE_BUF
	       i = write(fds[0],outbuf+bp,MIN(bs-bp,PIPE_BUF));
#else
	       i = write(fds[0],outbuf+bp,MIN(bs-bp,fpathconf(fds[0], _PC_PIPE_BUF)));
#endif
	       /* i = write(fds[0],outbuf+bp,1); */
	       /* printf("Finished writing.\n"); */
	       if (i == -1) {
		    /* Broken pipe - stop writing */
		    if (errno == EPIPE) { 
			 /* printf("Writing: Broken pipe.\n"); */
			 writing = FALSE; 
			 continue; 
		    }
		    if (errno == EINTR) continue;
		    goto errno_error_exit;
	       } 
	       bp += i;
	       if (status_bar_progress(bar, i)) 
		    goto error_exit;
	  }
     }
     
     /* Check for any remaining data sent to standard error */
     while (pipe_dialog_error_check(pipehandle)) { }
     
     /* Finish */
     g_free(inbuf);
     g_free(outbuf);
     chunk_close(ch);
     pipe_dialog_close(pipehandle);
     status_bar_end_progress(bar);

     if (do_read && !read_bytes) {
	  user_error(_("Command failed without returning any data"));
	  tempfile_abort(ct);	       
	  return NULL;
     }

     if (sent_all) *sent_all = !writing;
     
     return do_read ? tempfile_finished(ct) : NULL;

errno_error_exit:
     c = g_strdup_printf(_("Error: %s"),strerror(errno));
     user_error(c);
     g_free(c);
error_exit:
     g_free(inbuf);
     g_free(outbuf);
     chunk_close(ch);
     if (do_read) tempfile_abort(ct);
     pipe_dialog_close(pipehandle);
     status_bar_end_progress(bar);
     return NULL;
}

Chunk *pipe_dialog_pipe_chunk(Chunk *chunk, gchar *command, gboolean sendwav,
			      int dither_mode, StatusBar *bar, 
			      off_t *clipcount)
{
     return pipe_dialog_pipe_chunk_main(chunk,command,sendwav,TRUE,
					bar,NULL,dither_mode,clipcount);
}

gboolean pipe_dialog_send_chunk(Chunk *chunk, gchar *command, gboolean sendwav,
				int dither_mode, StatusBar *bar,
				off_t *clipcount)
{
     gboolean sent_all;
     pipe_dialog_pipe_chunk_main(chunk,command,sendwav,FALSE,
				 bar,&sent_all,dither_mode,clipcount);
     return !sent_all;
}
