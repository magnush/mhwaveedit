/*
 * Copyright (C) 2004 2005 2006 2008 2009 2011 2012, Magnus Hjorth
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


/* JACK sound driver */

#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include "int_box.h"
#include "gettext.h"

/* All variables/functions prefixed with mhjack_ since the jack library
 * already uses the jack_ prefix */

#define MHJACK_PLAY    0x80000000
#define MHJACK_STOP    0x40000000
#define MHJACK_REC     0x20000000
#define MHJACK_SKIPREQ(x) (((x) >> 24) & 15)
#define MHJACK_SKIPPOS(x) ((x) & 0xFFFFFF)
#define MHJACK_NEWSKIP(x,p) (((x) & 0xF0000000) | (((x) + 0x01000000) & 0x0F000000) | (p))

static struct {
     gchar *client_name;
     jack_client_t *myself;
     gboolean is_activated,autoconnect_input,autoconnect_output;
     guint maxoutports, maxinports;
     gchar *inportnames[8],*outportnames[8];
     jack_port_t *inports[8],*outports[8];
     volatile guint32 procctrl;
     volatile unsigned int xrun_count;
     volatile off_t played_bytes;
     int cb_skipcount;
     Dataformat current_format;
     size_t buffer_size;     
     jack_ringbuffer_t *buffers[8];
     gboolean refill;
     guint refillframes, refillpos;
} mhjack = { 0 };

struct mhjack_prefdlg {
     GtkWindow *wnd;
     GtkEntry *client_name;
     struct { 
	  Intbox *maxports; 
	  GtkEntry *portnames[8]; 
	  GtkToggleButton *autoconnect;
     } ports[2];
};

/* static int dumpfile; */

static void mhjack_register_ports(void);

static void mhjack_read_config(void)
{
     guint i;
     gchar *c,*d;

     if (mhjack.client_name != NULL) return;

     /* Read config from inifile */
     mhjack.client_name = g_strdup(inifile_get("jackClientName","mhwe"));
     mhjack.maxoutports = (guint)inifile_get_guint32("jackMaxportsOut",2);
     if (mhjack.maxoutports < 0 || mhjack.maxoutports > 8) 
	  mhjack.maxoutports = 2;
     mhjack.maxinports = (guint)inifile_get_guint32("jackMaxportsIn",2);
     if (mhjack.maxinports < 0 || mhjack.maxinports > 8)
	  mhjack.maxinports = 2;
     for (i=0; i<8; i++) {
	  c = g_strdup_printf("jackOutport%d",i+1);
	  d = inifile_get(c,NULL);
	  if (d != NULL) d = g_strdup(d);
	  else d = g_strdup_printf("out%c",channel_char(i));
	  mhjack.outportnames[i] = d;
	  g_free(c);
	  c = g_strdup_printf("jackInport%d",i+1);
	  d = inifile_get(c,NULL);
	  if (d != NULL) d = g_strdup(d);
	  else d = g_strdup_printf("in%c",channel_char(i));
	  mhjack.inportnames[i] = d;
	  g_free(c);
     }
     mhjack.autoconnect_input = inifile_get_gboolean("jackAutoconnectInput",
						     TRUE);
     mhjack.autoconnect_output = inifile_get_gboolean("jackAutoconnectOutput",
						      TRUE);
}

static void mhjack_preferences_ok(GtkButton *button, gpointer user_data)
{
     struct mhjack_prefdlg *dlg = (struct mhjack_prefdlg *)user_data;
     const gchar *c;
     guint i;
     gchar buf[32];
     /* Validate input */
     if (intbox_check(dlg->ports[0].maxports) || 
	 intbox_check(dlg->ports[1].maxports)) return;
     if (dlg->ports[0].maxports->val > 8 || dlg->ports[0].maxports->val < 0) {
	  user_error(_("Invalid number of input ports."));
	  return;
     }
     if (dlg->ports[1].maxports->val > 8 || dlg->ports[1].maxports->val < 0) {
	  user_error(_("Invalid number of output ports."));
	  return;
     }
     /* Apply settings */
     c = gtk_entry_get_text(dlg->client_name);
     if (mhjack.myself != NULL && strcmp(c, mhjack.client_name)) {
	  g_free(mhjack.client_name);
	  mhjack.client_name = g_strdup(c);
	  inifile_set("jackClientName",(gchar *)c);
	  user_info(_("The client name change won't take effect until you "
		    "restart the program."));
     }
     mhjack.maxinports = (guint)dlg->ports[0].maxports->val;
     inifile_set_guint32("jackMaxportsIn",mhjack.maxinports);
     mhjack.maxoutports = (guint)dlg->ports[1].maxports->val;
     inifile_set_guint32("jackMaxportsOut",mhjack.maxoutports);
     for (i=0; i<8; i++) {
	  g_free(mhjack.inportnames[i]);
	  mhjack.inportnames[i] = 
	       g_strdup(gtk_entry_get_text(dlg->ports[0].portnames[i]));
	  g_snprintf(buf,sizeof(buf),"jackInport%d",i+1);
	  inifile_set(buf,mhjack.inportnames[i]);
	  if (mhjack.myself != NULL && mhjack.inports[i] != NULL) {
	       if (i<mhjack.maxinports) {
		    /* printf("jack_port_set_name #%d %s\n",i, 
		       mhjack.inportnames[i]); */
		    jack_port_set_name(mhjack.inports[i],
				       mhjack.inportnames[i]);
	       } else {
		    /* printf("unregister inport #%d\n",i); */
		    jack_port_unregister(mhjack.myself,mhjack.inports[i]);
		    mhjack.inports[i] = NULL;
	       }		    
	  }
	  g_free(mhjack.outportnames[i]);
	  mhjack.outportnames[i] = 
	       g_strdup(gtk_entry_get_text(dlg->ports[1].portnames[i]));
	  g_snprintf(buf,sizeof(buf),"jackOutport%d",i+1);
	  inifile_set(buf,mhjack.outportnames[i]);
	  if (mhjack.myself != NULL && mhjack.outports[i] != NULL) {
	       if (i<mhjack.maxoutports)
		    jack_port_set_name(mhjack.outports[i],
				       mhjack.outportnames[i]);
	       else {
		    jack_port_unregister(mhjack.myself,mhjack.outports[i]);
		    mhjack.outports[i] = NULL;
	       }
	  }
     }
     mhjack.autoconnect_input = 
	  gtk_toggle_button_get_active(dlg->ports[0].autoconnect);
     mhjack.autoconnect_output = 
	  gtk_toggle_button_get_active(dlg->ports[1].autoconnect);
     inifile_set_gboolean("jackAutoconnectInput",mhjack.autoconnect_input);
     inifile_set_gboolean("jackAutoconnectOutput",mhjack.autoconnect_output);

     if (mhjack.myself != NULL) mhjack_register_ports();

     gtk_widget_destroy(GTK_WIDGET(dlg->wnd));
}

static void mhjack_preferences(void)
{
     GtkWidget *a,*b,*c,*d,*e,*f,*g;
     struct mhjack_prefdlg *dlg;
     gchar *titles[2] = {
	 N_(" Input ports "),
	 N_(" Output ports "),
     };    
     guint i,j;
     gchar buf[16];
     mhjack_read_config();
     dlg = g_malloc(sizeof(*dlg));     
     a = gtk_window_new(GTK_WINDOW_DIALOG);
     dlg->wnd = GTK_WINDOW(a);
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_window_set_title(GTK_WINDOW(a),_("Jack Preferences"));
     gtk_window_set_position(GTK_WINDOW(a),GTK_WIN_POS_CENTER);
     gtk_container_set_border_width(GTK_CONTAINER(a),8);
     gtk_signal_connect_object(GTK_OBJECT(a),"destroy",GTK_SIGNAL_FUNC(g_free),
			       (GtkObject *)dlg);
     b = gtk_vbox_new(FALSE,5);
     gtk_container_add(GTK_CONTAINER(a),b);
     c = gtk_hbox_new(FALSE,3);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     d = gtk_label_new(_("Client name: "));
     gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
     d = gtk_entry_new();
     gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
     dlg->client_name = GTK_ENTRY(d);
     gtk_entry_set_max_length(dlg->client_name, jack_client_name_size());
     gtk_entry_set_text(dlg->client_name, mhjack.client_name);
     c = gtk_hbox_new(TRUE,12);
     gtk_box_pack_start(GTK_BOX(b),c,TRUE,TRUE,0);
     for (i=0; i<2; i++) {
	  d = gtk_frame_new(_(titles[i]));
	  gtk_box_pack_start(GTK_BOX(c),d,TRUE,TRUE,0);
	  e = gtk_vbox_new(FALSE,4);
	  gtk_container_add(GTK_CONTAINER(d),e);
	  gtk_container_set_border_width(GTK_CONTAINER(e),6);
	  f = gtk_hbox_new(FALSE,0);
	  gtk_box_pack_start(GTK_BOX(e),f,TRUE,TRUE,0);
	  g = gtk_label_new(_("Number of ports (0-8): "));
	  gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
	  g = intbox_new(i>0 ? mhjack.maxoutports : mhjack.maxinports);	  
	  gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
	  dlg->ports[i].maxports = INTBOX(g);
	  f = gtk_table_new(8,2,FALSE);
	  gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
	  for (j=0; j<8; j++) {
	       g_snprintf(buf,sizeof(buf),_("Port #%d"),j+1);
	       attach_label(buf,f,j,0);
	       g = gtk_entry_new();
	       gtk_table_attach(GTK_TABLE(f),g,1,2,j,j+1,GTK_EXPAND|GTK_FILL,0,
				0,0);
	       dlg->ports[i].portnames[j] = GTK_ENTRY(g);
	       gtk_entry_set_max_length(GTK_ENTRY(g),jack_port_name_size());
	       gtk_entry_set_text(GTK_ENTRY(g),i>0 ? mhjack.outportnames[j] :
				  mhjack.inportnames[j]);
	  }
	  gtk_table_set_row_spacings(GTK_TABLE(f),2);
	  gtk_table_set_col_spacings(GTK_TABLE(f),2);
     }
     c = gtk_check_button_new_with_label(_("Automatically connect input "
					   "ports on startup"));
     dlg->ports[0].autoconnect = GTK_TOGGLE_BUTTON(c);
     gtk_toggle_button_set_active(dlg->ports[0].autoconnect,
				  mhjack.autoconnect_input);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     c = gtk_check_button_new_with_label(_("Automatically connect output "
					   "ports on startup"));
     dlg->ports[1].autoconnect = GTK_TOGGLE_BUTTON(c);
     gtk_toggle_button_set_active(dlg->ports[1].autoconnect,
				  mhjack.autoconnect_output);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
				  
     
     c = gtk_hbutton_box_new();
     gtk_box_pack_end(GTK_BOX(b),c,FALSE,FALSE,0);
     d = gtk_button_new_with_label(_("OK"));
     gtk_signal_connect(GTK_OBJECT(d),"clicked",
			GTK_SIGNAL_FUNC(mhjack_preferences_ok),dlg);
     gtk_container_add(GTK_CONTAINER(c),d);
     d = gtk_button_new_with_label(_("Close"));
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(a));
     gtk_container_add(GTK_CONTAINER(c),d);     
     gtk_widget_show_all(a);
}

static void mhjack_register_ports(void)
{
     guint i;

     /* If jack_port_register fails, the NULL value will be stored in 
      * mhjack.in/outports, so we can check for this or re-register later. */
     for (i=0; i < mhjack.maxinports; i++) 
	  if (mhjack.inports[i] == NULL)
	       mhjack.inports[i] = 
		    jack_port_register(mhjack.myself, mhjack.inportnames[i],
				       JACK_DEFAULT_AUDIO_TYPE, 
				       JackPortIsInput|JackPortIsTerminal, 0);
     for (i=0; i< mhjack.maxoutports; i++)
	  if (mhjack.outports[i] == NULL)
	       mhjack.outports[i] = 
		    jack_port_register(mhjack.myself, mhjack.outportnames[i],
				       JACK_DEFAULT_AUDIO_TYPE,
				       JackPortIsOutput|JackPortIsTerminal, 0);
     
}


/* Makes sure the ring buffers are allocated and at least 4x the JACK buffer
 * size and at least one second long. */
static int mhjack_setup_buffers(void)
{
     jack_nframes_t nframes;
     size_t sz;
     guint i;

     nframes = jack_get_buffer_size(mhjack.myself);
     sz = 4 * nframes * sizeof(float);

     if (sz < jack_get_sample_rate(mhjack.myself)*sizeof(float)) 
	  sz=jack_get_sample_rate(mhjack.myself)*sizeof(float);
     for (i=0; i<MAX(mhjack.maxinports,mhjack.maxoutports); i++) {

	  /* Remove too small buffers */
	  if (mhjack.buffers[i] != NULL && sz > mhjack.buffer_size) {
	       jack_ringbuffer_free(mhjack.buffers[i]);
	       mhjack.buffers[i] = NULL;
	  }

	  /* Create new buffer if no-one exists */
	  if (mhjack.buffers[i] == NULL)
	       mhjack.buffers[i] = jack_ringbuffer_create(sz);

	  /* Reset the buffer */
	  if (mhjack.buffers[i] != NULL)
	       jack_ringbuffer_reset(mhjack.buffers[i]);
     }

     mhjack.buffer_size = sz;
     return 0;
}

static int mhjack_xrun_callback(void *arg)
{
     mhjack.xrun_count ++;
     return 0;
}

/* Move read pointer ahead to position pos, but only if pos is within the
 * readable part of the buffer */
static void mhjack_jrb_skipto(jack_ringbuffer_t *rb, size_t pos)
{
     size_t w,r;
     w = rb->write_ptr;
     r = rb->read_ptr;
     if (w >= r) {
	  if (pos > r && pos <= w)
	       rb->read_ptr = pos;
     } else {
	  if (pos > r || pos <= w)
	       rb->read_ptr = pos;
     }
}

static int mhjack_process_callback(jack_nframes_t nframes, void *arg)
{     
     guint i,first_silent_port=0,skippos;
     gboolean xrun = FALSE, first = TRUE, do_skip=FALSE;
     size_t sz = nframes * sizeof(float), sz2;
     gchar *p;
     int pctrl;
     pctrl = mhjack.procctrl;
     if ((pctrl & MHJACK_PLAY) != 0) {
	  i = MHJACK_SKIPREQ(pctrl);
	  if (i != mhjack.cb_skipcount) {
	       mhjack.cb_skipcount = i;
	       do_skip = TRUE;
	       skippos = MHJACK_SKIPPOS(pctrl);
	  }
	  for (i=0; i<mhjack.current_format.channels; i++) {
	       if (mhjack.outports[i] == NULL) continue;
	       if (do_skip) {
		    mhjack_jrb_skipto(mhjack.buffers[i],skippos);
	       }
	       p = jack_port_get_buffer(mhjack.outports[i],nframes);
	       sz2 = jack_ringbuffer_read(mhjack.buffers[i],p,sz);
	       if (sz2 < sz) {
		    xrun = TRUE;
		    memset(p+sz2,0,sz-sz2);		    
	       }
	       /* Make sure bytes_played always follow the same port. For 
		* simplicity, we use the first one. */
	       if (first) {
		    mhjack.played_bytes += sz2;
		    first = FALSE;
	       }
	  }
	  first_silent_port = mhjack.current_format.channels;
     } 

     for (i=first_silent_port; i<mhjack.maxoutports; i++) {
	  if (mhjack.outports[i] == NULL) continue;
	  p = jack_port_get_buffer(mhjack.outports[i],nframes);
	  memset(p,0,sz);
     }

     if ((pctrl & MHJACK_REC) != 0) {
	  for (i=0; i<mhjack.current_format.channels; i++) {
	       if (mhjack.inports[i] == NULL) continue;
	       p = jack_port_get_buffer(mhjack.inports[i],nframes);
	       sz2 = jack_ringbuffer_write(mhjack.buffers[i],p,sz);
	       if (sz2 < sz) 
		    xrun = TRUE;
	  }
     }
     if (xrun && ((pctrl&MHJACK_REC) != 0 || (pctrl&MHJACK_STOP) == 0)) {
	  console_message(_("Over/underrun in JACK driver"));
	  mhjack.xrun_count ++;
     }
     return 0;
}

static void mhjack_autoconnect(jack_port_t **ports, int typeflag, 
			       gboolean silent)
{
     const char **c,*p1,*p2;
     char *d;
     int i,j;
     c = jack_get_ports(mhjack.myself,NULL,NULL,typeflag|JackPortIsPhysical);
     if (c == NULL) {
	  /* This should not happen */
	  if (!silent) user_error(_("jack_get_ports returned NULL"));
	  return;
     }
     for (i=0; i<8; i++) {
	  if (c[i] == NULL) break;
	  if (typeflag == JackPortIsInput) { 
	       if (i >= mhjack.maxoutports) break;
	       p1=jack_port_name(ports[i]); 
	       p2=c[i]; 
	  } else { 
	       if (i >= mhjack.maxinports) break;
	       p1=c[i]; 
	       p2=jack_port_name(ports[i]); 
	  }
	  j = jack_connect(mhjack.myself,p1,p2);
	  if (j<0 && !silent) {
	       d = g_strdup_printf(_("Connection from %s to %s failed: %s"),
				   p1,p2,strerror(j));
	       user_error(d);
	       g_free(d);
	  }
     }
     g_free(c);
}

static void silent_error_callback(const char *msg)
{
}

static void normal_error_callback(const char *msg)
{
     fprintf(stderr,"%s\n",msg);
}

static void mhjack_connect(gboolean silent)
{
     gchar *c;
     jack_status_t s;
     if (mhjack.myself == NULL) {

	  if (silent)
	       jack_set_error_function(silent_error_callback);

	  /* Connect to the JACK server */
	  mhjack.myself = jack_client_open(mhjack.client_name,
					   JackNoStartServer|JackUseExactName,
					   &s);
	  /* All versions of Jack don't seem to set JackNameNotUnique properly.
	   * Testing with Jack 1.9.4 gave JackServerError|JackFailure even
	   * though JackUseExactName was the cause of the error */
	  if (mhjack.myself == NULL /* && (s & JackNameNotUnique) != 0 */ ) {
	       c = g_strdup_printf("%s%d",mhjack.client_name,getpid());
	       if (!silent)
		    fprintf(stderr,"Retrying with Jack client name %s\n",c);
	       mhjack.myself = jack_client_open(c,JackNoStartServer,&s);
	       g_free(c);
	  }

	  if (silent)
	       jack_set_error_function(normal_error_callback);

	  if (mhjack.myself == NULL) {
	       if (!silent)
		    user_error(_("Could not connect to the JACK server."));
	       return;
	  }

	  jack_set_xrun_callback(mhjack.myself,mhjack_xrun_callback,NULL);
	  jack_set_process_callback(mhjack.myself,mhjack_process_callback,
				    NULL);

	  mhjack_register_ports();
     }

     if (!mhjack.is_activated) {
	  if (jack_activate(mhjack.myself)) {
	       if (!silent) 
		    user_error(_("Activation failed!"));
	       return;
	  }
	  
	  mhjack.is_activated = TRUE;

	  if (mhjack.autoconnect_input)
	       mhjack_autoconnect(mhjack.inports,JackPortIsOutput,silent);
	  if (mhjack.autoconnect_output)
	       mhjack_autoconnect(mhjack.outports,JackPortIsInput,silent);
     }
}

static guint mhjack_ringbuffer_space(gboolean input, gboolean readspace)
{ 
     guint i,j;
     guint w = 1024*1024; /* Just a very high number */

     for (i=0; i<mhjack.current_format.channels; i++)
	  if ((!input && mhjack.outports[i] != NULL) || 
	      (input && mhjack.inports[i] != NULL)) {
	       if (readspace)
		    j = jack_ringbuffer_read_space(mhjack.buffers[i]);
	       else
		    j = jack_ringbuffer_write_space(mhjack.buffers[i]);
	       if (w > j) w=j;
	  }
     return w;
}

static gboolean mhjack_init(gboolean silent)
{
     mhjack_read_config();
     mhjack_connect(silent);
     /* dumpfile = open("jackdump.dat",O_WRONLY|O_CREAT); */
     return (mhjack.myself != NULL);
}

static void mhjack_quit(void)
{
     if (mhjack.myself != NULL)
	  jack_client_close(mhjack.myself);
}

/* Output */

static gint mhjack_output_select_format(Dataformat *format, gboolean silent,
					GVoidFunc ready_func)
{
     /* Make sure we're connected before we check the format. This is
      * to make sure that the error message is displayed. Note that
      * mhjack_connect call is cheap when we're already connected */
     mhjack_connect(silent);
     if (!mhjack.is_activated) return silent ? -1 : +1;

     if (format->type != DATAFORMAT_FLOAT ||
	 format->samplesize != sizeof(float) ||
	 XOR(format->bigendian,ieee_be_compatible))
	  return -1;

     if (format->samplerate != jack_get_sample_rate(mhjack.myself)) return -1;

     memcpy(&(mhjack.current_format),format,sizeof(Dataformat));
     mhjack.xrun_count = 0;
     mhjack.played_bytes = 0;

     mhjack_setup_buffers();     

     mhjack.refill = TRUE;
     mhjack.refillpos = 0;
     mhjack.refillframes = 0;

     return 0;
}

static gboolean mhjack_output_suggest_format(Dataformat *format, 
					     Dataformat *result)
{
     mhjack_connect(TRUE);
     if (!mhjack.is_activated) return FALSE; 

     memcpy(result,format,sizeof(Dataformat));
     result->type = DATAFORMAT_FLOAT;
     result->samplesize = sizeof(float);
     result->bigendian = ieee_be_compatible;
     result->samplebytes = result->samplesize * result->channels;
     result->samplerate = jack_get_sample_rate(mhjack.myself);
     result->packing = 0;
     return TRUE;
}

/* We have two different buffer modes during playback:
 *   Normal mode:
 *     Keep the buffer filled to capacity minus 1/2 second
 *   Refill mode:
 *     Write 1/2 second of data into the buffer and then switch into
 *     normal mode. When switching over, playback is started if it
 *     was stopped, and a skip request (for dropping data) can be sent.
 *
 * The mode only matters to the user side, the playback thread plays what's
 * in the ring buffer.
 *
 * Playback skipping without gaps or underruns is implemented as follows:
 * - When output_clear_buffers is called, we switch to refill mode but the
 *   callback thread keeps playing from the >0.5 s of old sound currently in
 *   the ring buffers.
 * - When enough new data has been written into the ring buffers, a skip
 *   request is sent to the callback thread with the new data's starting
 *   position in the ring buffer.
 * - The next time the player thread wakes up it sees the skip request and
 *   jumps the read position ahead to the new data. If it has already
 *   played past that point, it ignores the skip request.
 */

static guint mhjack_writable(void)
{
     int writable,i,s;
     writable = s = mhjack_ringbuffer_space(FALSE,FALSE) / sizeof(float);
     i = (mhjack.buffer_size / 2) / sizeof(float);
     if (mhjack.refill) {
	  /* Return remaining part of refill before switching to normal */
	  if (i > mhjack.refillframes) i-=mhjack.refillframes; else i=0;
	  if (writable > i) writable=i;
     } else {
	  /* Leave half the buffer for refilling later */
	  if (writable > i) writable-=i; else writable=0;
     }
     /* if (writable > 0)
	  printf("writable: s=%d refill=%d refillframes=%d -> %d\n",s,
	  mhjack.refill,mhjack.refillframes,writable); */
     return writable;
}

static guint mhjack_output_play(gchar *buffer, guint bufsize)
{
     guint writable,frames,i,j;
     float fbuf[1024],fbuf2[512];
     int pctrl = mhjack.procctrl;

     if (bufsize == 0) {
	  if ((pctrl&MHJACK_PLAY) == 0 && mhjack_ringbuffer_space(FALSE,TRUE)==0)
	       return 0;
	  /* The caller wants to drain the buffers, therefore set the 
	   * is_stopping flag so underrun messages aren't displayed. */
	  pctrl |= (MHJACK_PLAY | MHJACK_STOP);
	  mhjack.procctrl = pctrl;
	  return mhjack_ringbuffer_space(FALSE,TRUE);
     }

     /* write(dumpfile, buffer, bufsize); */

     /* Calculate room in the ring buffers */
     writable = mhjack_writable();

     /* Calculate how many frames we can write out */
     frames = bufsize / mhjack.current_format.samplebytes;
     if (frames > ARRAY_LENGTH(fbuf2)) frames=ARRAY_LENGTH(fbuf2);
     if (frames > writable) frames = writable;

     if (frames == 0) return 0;
     
     /* Convert data to floats */
     convert_array(buffer,&(mhjack.current_format),fbuf,&dataformat_single,
		   frames*mhjack.current_format.channels,DITHER_NONE,NULL);

     /* De-interleave and write data to the different ring buffers. */

     if (mhjack.current_format.channels == 1) {
	  /* Special case - mono output */
	  if (mhjack.outports[0] != NULL)
	       jack_ringbuffer_write(mhjack.buffers[0],(char *)fbuf,
				     frames*sizeof(float));
     } else {
	  for (i=0; i<mhjack.current_format.channels; i++) {
	       if (mhjack.outports[i] == NULL) continue;
	       /* De-interleave */
	       for (j=0; j<frames; j++)
		    fbuf2[j] = fbuf[j*mhjack.current_format.channels+i];
	       /* Write */
	       /* We already checked free space, so we assume we can
		* write all data here. */
	       jack_ringbuffer_write(mhjack.buffers[i],(char *)fbuf2,
				     frames*sizeof(float));
	  }
     }

     mhjack.refillframes += frames;
     writable -= frames;

     if (writable < 64) {
	  if (mhjack.refill) {
	       pctrl = MHJACK_NEWSKIP(pctrl,mhjack.refillpos);
	       mhjack.refill = FALSE;
	  }
	  if ((pctrl&MHJACK_PLAY)==0) {
	       /* Let the processing function work next time it's called */
	       pctrl |= MHJACK_PLAY;
	       pctrl &= ~MHJACK_STOP;
	  }
	  mhjack.procctrl = pctrl;
     }

     return frames * mhjack.current_format.samplebytes;
}

static void mhjack_clear_buffers(void)
{
     mhjack.refill = TRUE;
     mhjack.refillframes = 0;
     mhjack.refillpos = mhjack.buffers[0]->write_ptr;
}

static gboolean mhjack_output_stop(gboolean must_flush)
{
     int pctrl = mhjack.procctrl,w;
     if ((pctrl&MHJACK_PLAY)==0) {
	  if (mhjack_ringbuffer_space(FALSE,TRUE) == 0)
	       return TRUE;
     }
     if (!must_flush) {
	  w = mhjack.buffers[0]->write_ptr;
	  pctrl = MHJACK_NEWSKIP(pctrl, w);
     }
     pctrl |= (MHJACK_STOP | MHJACK_PLAY);
     mhjack.procctrl = pctrl;
     while (mhjack_ringbuffer_space(FALSE,TRUE) > 0) do_yield(FALSE);
     pctrl &= ~MHJACK_PLAY;
     mhjack.procctrl = pctrl;
     return must_flush;
}

static gboolean mhjack_output_want_data(void)
{
     return (mhjack_writable() > 0);
}

/* Input */

static void mhjack_input_store(Ringbuf *buffer)
{
     guint readable,frames,i,j;
     float fbuf[512],fbuf2[1024];
     gchar sbuf[4096];
     size_t s;
     
     readable = mhjack_ringbuffer_space(TRUE,TRUE) / sizeof(float);     
     if (readable == 0) return;

     frames = ringbuf_freespace(buffer) / mhjack.current_format.samplebytes;
     if (frames > readable) frames = readable;
     if (frames > ARRAY_LENGTH(fbuf2)/mhjack.current_format.channels) 
	  frames = ARRAY_LENGTH(fbuf2)/mhjack.current_format.channels;
     
     /* Read data, then interleave it. */
     if (mhjack.current_format.channels == 1) {
	  /* Special case - one channel */
	  if (mhjack.inports[0] != NULL) {
	       s = jack_ringbuffer_read(mhjack.buffers[0],(char *)fbuf2,
					frames*sizeof(float));
	       g_assert(s == frames*sizeof(float));
	  } else
	       memset(fbuf2,0,frames*sizeof(float));
     } else {
	  for (i=0; i<mhjack.current_format.channels; i++) {
	       if (mhjack.inports[i] == NULL)
		    memset(fbuf,0,frames*sizeof(float));
	       else {
		    /* Read data */
		    s = jack_ringbuffer_read(mhjack.buffers[i],(char *)fbuf,
					     frames*sizeof(float));
		    g_assert(s == frames*sizeof(float));
		    /* printf("Read %ld bytes\n",(long int)s); */
		    /* Interleave */
		    for (j=0; j<frames; j++)
			 fbuf2[j*mhjack.current_format.channels+i] = fbuf[j];
	       }
	  }
     }     
     /* Convert data (in case sample_t == double, otherwise, convert_array
      * just does a memcpy). Dithering never matters in this conversion. */
     convert_array(fbuf2,&dataformat_single,sbuf,&(mhjack.current_format),
		   frames*mhjack.current_format.channels,DITHER_NONE,NULL);
     /* Add to ring buffer */
     s = ringbuf_enqueue(buffer,sbuf,frames*mhjack.current_format.samplebytes);
     g_assert(s == frames*mhjack.current_format.samplebytes);
}

static int mhjack_get_xrun_count(void)
{
     return mhjack.xrun_count;
}

static GList *mhjack_input_supported_formats(gboolean *complete)
{
     Dataformat *fmt;
     GList *l = NULL;
     int i,r;

     if (mhjack.myself == NULL)
	  mhjack_connect(TRUE);
     if (mhjack.myself == NULL) {
	  *complete = TRUE;
	  return NULL;
     }
     
     r = jack_get_sample_rate(mhjack.myself);
     for (i=1; i<=mhjack.maxinports; i++) {
	  fmt = g_malloc(sizeof(*fmt));
	  fmt->type = DATAFORMAT_FLOAT;
	  fmt->samplesize = sizeof(float);
	  fmt->packing = 0;
	  fmt->channels = i;
	  fmt->samplebytes = fmt->samplesize * fmt->channels;
	  fmt->samplerate = r;
	  fmt->bigendian = ieee_be_compatible;
	  l = g_list_append(l, fmt);
     }
     *complete = TRUE;
     return l;
}

static gint mhjack_input_select_format(Dataformat *format, gboolean silent,
				       GVoidFunc ready_func)
{
     gchar *c;
     mhjack_connect(silent);
     if (!mhjack.is_activated) return silent ? -1 : +1;
     
     if (format->samplerate != jack_get_sample_rate(mhjack.myself) ||
	 (mhjack_output_select_format(format,TRUE,ready_func))) {
	  if (!silent) {
	       c = g_strdup_printf(_("With JACK, the only supported recording "
				   "format is "
				   "Floating-point, single precision, %d Hz"),
				   jack_get_sample_rate(mhjack.myself));
	       user_error(c);
	       g_free(c);
	       return 1;
	  } else
	       return -1;	  
     }
     mhjack.procctrl |= MHJACK_REC;
     return 0;
}

/* Also used as input_stop_hint */
static void mhjack_input_stop(void)
{
     mhjack.procctrl &= ~MHJACK_REC;
}
