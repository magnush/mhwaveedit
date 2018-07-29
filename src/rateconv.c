/*
 * Copyright (C) 2004 2005 2006 2007 2009 2010, Magnus Hjorth
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
#include <errno.h>

#include <gtk/gtk.h>

#ifdef HAVE_LIBSAMPLERATE
#include <samplerate.h>
#endif

#include "rateconv.h"
#include "soxdialog.h"
#include "pipedialog.h"
#include "ringbuf.h"
#include "um.h"
#include "gettext.h"

struct driver_data {
     const gchar *name;
     const gchar *id;
     gboolean is_realtime;
     gboolean prefers_float;
     gpointer (*new_func)(struct driver_data *driver, gboolean realtime,
			  Dataformat *format, guint32 outrate, 
			  int dither_mode);
     void (*destroy_func)(gpointer convdata);
     void (*set_outrate_func)(gpointer convdata, guint32 outrate);
     gint (*write_func)(gpointer convdata, gpointer buf, guint buflen);
     gint (*read_func)(gpointer convdata, gpointer buf, guint buflen);
     gboolean (*hasdata_func)(gpointer convdata);
};

struct driver_data *repeat_driver;

struct convdata_base {
     struct driver_data *driver;
     Ringbuf *passthru_buffer;
     gboolean emptying;     
     guint32 writecount,readcount;
     guint32 empty_point;
     guint32 inrate,outrate;
};

static GList *drivers = NULL;
static GList *realtime_drivers = NULL;

static gint sox_read(gpointer convdata, gpointer buf, guint bufsize);

#ifdef HAVE_LIBSAMPLERATE

struct convdata_src {
     struct convdata_base b;
     Dataformat format;
     SRC_STATE *state;
     long maxframes;
     SRC_DATA data;
     int dither_mode;
};

static gpointer rateconv_src_new(struct driver_data *driver, gboolean realtime,
				 Dataformat *format, guint32 outrate, 
				 int dither_mode)
{
     struct convdata_src *cd;
     long frames;
     SRC_STATE *state;
     int i;
     int src_converter_type;
     gchar *c;
     
     /* puts("rateconv_src_new starts"); */
     src_converter_type = driver->id[3] - '1';
     state = src_new(src_converter_type, format->channels, &i);
     if (state == NULL) {
	  c = g_strdup_printf(_("Error initialising sample rate conversion: %s"),
			      src_strerror(i));
	  user_error(c);
	  g_free(c);
	  return NULL;
     }

     cd = g_malloc(sizeof(*cd));
     cd->state = state;
     memcpy(&(cd->format),format,sizeof(Dataformat));     
     frames = 16384;
     cd->maxframes = frames;
     cd->data.data_in = g_malloc(frames * sizeof(float) * format->channels);
     cd->data.input_frames = 0;
     cd->data.data_out = g_malloc(frames * sizeof(float) * format->channels);
     cd->data.src_ratio = ((double)outrate)/((double)format->samplerate);
     cd->data.end_of_input = 0;
     cd->dither_mode = dither_mode;
     /* puts("rateconv_src_new ends"); */
     return cd;
}

static void rateconv_src_destroy(gpointer convdata)
{
     struct convdata_src *cd = (struct convdata_src *)convdata;
     src_delete(cd->state);
     g_free(cd->data.data_in);
     g_free(cd->data.data_out);
     g_free(cd);
}

static int rateconv_src_write(gpointer convdata, gpointer buf, guint bufsize)
{
     struct convdata_src *cd = (struct convdata_src *)convdata;
     /* puts("rateconv_src_write starts"); */
     long wf = bufsize / cd->format.samplebytes;
     long l = cd->maxframes - cd->data.input_frames;
     if (buf == NULL) {
	  cd->data.end_of_input = 1;
	  return 0;
     }
     if (wf > l) wf = l;
     if (wf == 0) return 0;
     convert_array(buf, &(cd->format), 
		   &(cd->data.data_in[cd->data.input_frames * 
				      cd->format.channels]),
		   &dataformat_single, wf * cd->format.channels,
		   cd->dither_mode, NULL);
     cd->data.input_frames += wf;
     /* puts("rateconv_src_write ends"); */
     return wf * cd->format.samplebytes;
}

static int rateconv_src_read(gpointer convdata, gpointer buf, guint bufsize)
{
     struct convdata_src *cd = (struct convdata_src *)convdata;
     /* puts("rateconv_src_read starts"); */
     long rf = bufsize / cd->format.samplebytes;
     long l = cd->maxframes;
     int i;
     gchar *c;
     if (rf > l) rf = l;
     cd->data.output_frames = rf;
     i = src_process(cd->state, &(cd->data));
     if (i) {
	  c = g_strdup_printf(_("Error converting samplerate: %s\n"),
			      src_strerror(i));
	  console_message(c);
	  g_free(c);
	  return -1;
     }
     rf = cd->data.output_frames_gen;
     if (rf > 0)
	  convert_array(cd->data.data_out, &dataformat_single, buf,
			&(cd->format), rf * cd->format.channels, 
			cd->dither_mode, NULL);
     if (cd->data.input_frames_used < cd->data.input_frames) {
	  l = cd->data.input_frames - cd->data.input_frames_used;
	  memmove(cd->data.data_in, 
		  &(cd->data.data_in[cd->data.input_frames_used * 
				     cd->format.channels]),
		  l*cd->format.channels*sizeof(float));
	  cd->data.input_frames = l;
     } else
	  cd->data.input_frames = 0;
     /* puts("rateconv_src_read ends"); */
     return rf * cd->format.samplebytes;
}

static gboolean rateconv_src_hasdata(gpointer convdata)
{
     struct convdata_src *cd = (struct convdata_src *)convdata;
     return (cd->data.input_frames > 0);
}

static void rateconv_src_set_outrate(gpointer convdata, guint32 outrate)
{
     struct convdata_src *cd = (struct convdata_src *)convdata;
     double new_ratio;
     int i;
     gchar *c;
     new_ratio = ((double)outrate) / ((double)cd->format.samplerate);
     i = src_set_ratio(cd->state,new_ratio);
     if (i) {
	  c = g_strdup_printf(_("Error changing samplerate conversion "
				"ratio: %s\n"),src_strerror(i));
	  console_message(c);
	  g_free(c);
     } else
	  cd->data.src_ratio = new_ratio;
}


#endif

struct convdata_sox {
     struct convdata_base b;
     Dataformat format;
     gboolean converting;
     Dataformat convert_from_format;
     int fds[3];     
     gpointer pipehandle;
     gchar tmpbuf[32],tmpbuf_in[32];
     /* 0 = opened, 1 = purge tmpbuf then close, 2 = write closed, 
	3 = read closed */
     int close_status; 
     int tmpbuf_size,tmpbuf_in_size;
     int dither_mode;
};

static gpointer sox_new(struct driver_data *driver, gboolean realtime,
			Dataformat *format, guint32 outrate, int dither_mode)
{
     struct convdata_sox cd,*cdp;
     Dataformat fmt;
     gchar c[512],d[64],e[64];
     if (format->type == DATAFORMAT_FLOAT || format->samplesize == 3 ||
	 XOR(format->bigendian,IS_BIGENDIAN)) {
	  cd.converting = TRUE;
	  memcpy(&(cd.convert_from_format),format,sizeof(Dataformat));
	  cd.format.type = DATAFORMAT_PCM;
	  cd.format.samplerate = format->samplerate;
	  cd.format.samplesize = 4;
	  cd.format.channels = format->channels;
	  cd.format.sign = FALSE;
	  cd.format.bigendian = IS_BIGENDIAN;
	  cd.format.samplebytes = cd.format.samplesize * cd.format.channels;
     } else {
	  cd.converting = FALSE;
	  memcpy(&(cd.format),format,sizeof(Dataformat));
     }
     sox_dialog_format_string(d,sizeof(d),&(cd.format));
     memcpy(&fmt,&(cd.format),sizeof(fmt));
     fmt.samplerate = outrate;
     sox_dialog_format_string(e,sizeof(e),&fmt);
     /* driver->id+4 converts driver name to effect name, for example 
      * "sox_resample" to "resample" */
     g_snprintf(c,sizeof(c),"sox %s - %s - %s",d,e,driver->id+4);
     /* puts(c); */
     cd.pipehandle = pipe_dialog_open_pipe(c,cd.fds,TRUE);
     if (cd.pipehandle == NULL) return NULL;
     cd.tmpbuf_size = 0;
     cd.tmpbuf_in_size = 0;
     cd.close_status = 0;
     cd.dither_mode = dither_mode;
     cdp = g_malloc(sizeof(*cdp));
     memcpy(cdp,&cd,sizeof(struct convdata_sox));
     return (gpointer)cdp;
}

static void sox_destroy(gpointer convdata)
{
     struct convdata_sox *cd = (struct convdata_sox *)convdata;
     if (cd->pipehandle != NULL) pipe_dialog_close(cd->pipehandle);
     g_free(convdata);
}

static gboolean sox_purge_tmpbuf(struct convdata_sox *cd)
{
     int i,j;
     if (!fd_canwrite(cd->fds[0])) return FALSE;
     i = write(cd->fds[0],cd->tmpbuf,cd->tmpbuf_size);
     if (i <= 0) return TRUE;
     cd->tmpbuf_size -= i;
     for (j=0; j<cd->tmpbuf_size; j++)
	  cd->tmpbuf[j] = cd->tmpbuf[j+i];
     if (cd->close_status == 1 && cd->tmpbuf_size == 0) {
	  pipe_dialog_close_input(cd->pipehandle);
	  cd->close_status = 2;
     }
     return FALSE;
}

static gint sox_write_main(struct convdata_sox *cd, gpointer buf, 
			   guint bufsize)
{
     int i,j;
     gchar *c,*p = buf;
     gboolean purged=FALSE;

     pipe_dialog_error_check(cd->pipehandle);
     
     bufsize -= bufsize % cd->format.samplebytes;

     if (cd->tmpbuf_size > 0) {
	  if (sox_purge_tmpbuf(cd)) return -1;
	  if (cd->tmpbuf_size > 0) return 0;
	  purged = TRUE;
     }
     if (buf == NULL && cd->close_status == 0) {
	  cd->close_status = 1;     
	  if (cd->tmpbuf_size == 0) {
	       pipe_dialog_close_input(cd->pipehandle);
	       cd->close_status = 2;	       
	  }	  
     }
     if (bufsize == 0 || cd->close_status>0 || !fd_canwrite(cd->fds[0])) 
	  return purged?cd->format.samplebytes:0;     
     /* puts("sox_write: calling write..."); */
#ifdef PIPE_BUF
     i = write(cd->fds[0],buf,MIN(bufsize,PIPE_BUF));
#else
     i = write(cd->fds[0],buf,MIN(bufsize,fpathconf(cd->fds[0], _PC_PIPE_BUF)));
#endif
     /* printf("tried = %d, got = %d\n",(int)bufsize,i); */
     if (i == 0) {
	  user_error(_("Unexpected EOF in connection to subprocess"));
	  return -1;
     }
     if (i == -1) {
	  c = g_strdup_printf(_("Error writing to subprocess: %s"),
			      strerror(errno));
	  user_error(c);
	  g_free(c);
	  return -1;
     }
     j = i % cd->format.samplebytes;
     if (j > 0) {
	  cd->tmpbuf_size = cd->format.samplebytes - j;
	  memcpy(cd->tmpbuf,p+i,cd->tmpbuf_size);
     }
     
     return i-j+(purged?cd->format.samplebytes:0);
}

static gint sox_write(gpointer convdata, gpointer buf, guint bufsize)
{
     struct convdata_sox *cd = (struct convdata_sox *)convdata;
     if (cd->converting) {
	  gpointer p;
	  guint ns,frames;
	  gint i;
	  frames = bufsize / cd->convert_from_format.samplebytes;
	  ns = frames * cd->format.samplebytes;
	  p = g_malloc(ns);
	  convert_array(buf,&(cd->convert_from_format),
			p,&(cd->format),frames*cd->format.channels,
			cd->dither_mode,NULL);
	  i = sox_write_main(cd,p,ns);
	  g_free(p);
	  if (i<=0) return i;
	  return (i / cd->format.samplebytes) * 
	       cd->convert_from_format.samplebytes;
     }
     return sox_write_main(cd,buf,bufsize);
}

static gint sox_read_main(struct convdata_sox *cd, gpointer buf, guint bufsize)
{
     int i,j;
     gchar *c, *p = buf;

     if (cd->close_status == 3) return 0;

     pipe_dialog_error_check(cd->pipehandle);

     bufsize -= bufsize % cd->format.samplebytes;     
     if (bufsize == 0) return 0;
     if (cd->tmpbuf_size > 0) sox_purge_tmpbuf(cd);
    

     if (cd->tmpbuf_in_size > 0) {
	  if (!fd_canread(cd->fds[1]) && cd->close_status < 2) return 0;
	  i = read(cd->fds[1],cd->tmpbuf_in+cd->tmpbuf_in_size,
		   cd->format.samplebytes - cd->tmpbuf_in_size);
	  if (i == -1) {
	       c = g_strdup_printf(_("Error reading from sub process: %s"),
				   strerror(errno));
	       user_error(c);
	       g_free(c);
	       return -1;
	  }
	  if (i == 0) {
	       cd->tmpbuf_in_size = 0;
	       return 0;
	  }
	  cd->tmpbuf_in_size += i;
	  if (cd->tmpbuf_in_size < cd->format.samplebytes) return 0;
	  memcpy(buf,cd->tmpbuf_in,cd->format.samplebytes);
	  cd->tmpbuf_in_size = 0;
	  i = sox_read_main(cd, p+cd->format.samplebytes,
			    bufsize-cd->format.samplebytes);
	  if (i == -1) return -1;
	  return cd->format.samplebytes + i;
     }
     if (!fd_canread(cd->fds[1]) && cd->close_status < 2) return 0;
     i = read(cd->fds[1],buf,bufsize);
     if (i == -1) {
	  c = g_strdup_printf(_("Error reading from sub process: %s"),
			      strerror(errno));
	  user_error(c);
	  g_free(c);
	  return -1;
     }
     if (i == 0) {
	  if (cd->close_status < 2) {
	       user_error(_("SoX closed connection too early!"));
	       return -1;
	  } else {
	       cd->close_status = 3;
	       pipe_dialog_close(cd->pipehandle);
	       cd->pipehandle = NULL;
	       return 0;
	  }
     }
     j = i % cd->format.samplebytes;
     if (j > 0) {
	  cd->tmpbuf_in_size = j;
	  i -= j;
	  memcpy(cd->tmpbuf_in, p+i, j);
     }
     
     return i;
}

static gint sox_read(gpointer convdata, gpointer buf, guint bufsize)
{
     struct convdata_sox *cd = (struct convdata_sox *)convdata;
     if (cd->converting) {
	  gpointer p;
	  guint ns,frames;
	  gint i;
	  frames = bufsize / cd->convert_from_format.samplebytes;
	  ns = frames * cd->format.samplebytes;
	  p = g_malloc(ns);
	  i = sox_read_main(cd,p,ns);
	  if (i > 0) {
	       convert_array(p,&(cd->format),buf,&(cd->convert_from_format),
			     i/cd->format.samplesize, cd->dither_mode, NULL);
	  }
	  g_free(p);
	  if (i<=0) return i;
	  return (i / cd->convert_from_format.samplebytes) * 
	       cd->format.samplebytes;
     }
     return sox_read_main(cd,buf,bufsize);     
}

static gboolean sox_hasdata(gpointer convdata)
{
     struct convdata_sox *cd = (struct convdata_sox *)convdata;
     if (cd->tmpbuf_size > 0) sox_purge_tmpbuf(cd);
     if (cd->close_status == 3) return FALSE;
     if (cd->close_status == 2) return TRUE;
     return fd_canread(cd->fds[1]);
}

struct convdata_repeat {
     struct convdata_base b;
     Ringbuf *databuf;
     Dataformat format;
     gchar cursamp[32];
     gfloat fracpos, ratio;
};

static gpointer repeat_new(struct driver_data *driver, gboolean realtime,
			   Dataformat *format, guint32 outrate, 
			   int dither_mode)
{
     struct convdata_repeat *data;
     data = g_malloc(sizeof(*data));
     data->databuf = ringbuf_new(realtime ? 1024*format->samplebytes : 
				 format->samplerate * format->samplebytes);
     data->fracpos = 1.0;
     data->ratio = (format->samplerate) / ((gfloat)outrate);
     memcpy(&(data->format),format,sizeof(Dataformat));
     return (gpointer)data;
}

static void repeat_destroy(gpointer convdata)
{
     struct convdata_repeat *data = (struct convdata_repeat *)convdata;
     ringbuf_free(data->databuf);
     g_free(data);
}

static gint repeat_write(gpointer convdata, gpointer buf, guint bufsize)
{
     struct convdata_repeat *data = (struct convdata_repeat *)convdata;
     if (buf == NULL) return 0;
     return (gint)ringbuf_enqueue(data->databuf,buf,bufsize);
}

static gint repeat_read(gpointer convdata, gpointer buf, guint bufsize)
{
     struct convdata_repeat *data = (struct convdata_repeat *)convdata;
     gchar *p = (gchar *)buf;
     guint i;
     gfloat fracpos,ratio;
     guint samplebytes;     

     bufsize -= bufsize % data->format.samplebytes;

     if (data->format.samplerate == data->b.outrate) 
	  return ringbuf_dequeue(data->databuf,buf,bufsize);     

     i = bufsize;
     fracpos = data->fracpos;
     ratio = data->ratio;
     samplebytes = data->format.samplebytes;

     while (i > 0) {
	  /* Read new sample data */
	  while (fracpos >= 1.0) {
	       if (ringbuf_dequeue(data->databuf,data->cursamp,samplebytes)
		   < samplebytes)
		    goto breakout;
	       fracpos -= 1.0;	       
	  }
	  /* Write sample data */
	  memcpy(p,data->cursamp,samplebytes);
	  p += samplebytes;
	  i -= samplebytes;
	  fracpos += ratio;
     }
 breakout:     

     data->fracpos = fracpos;
     return bufsize - i;
}

static gboolean repeat_hasdata(gpointer convdata)
{
     struct convdata_repeat *data = (struct convdata_repeat *)convdata;
     return (data->fracpos < 1.0 || ringbuf_available(data->databuf));
}

static void repeat_set_outrate(gpointer convdata, guint32 outrate)
{
     struct convdata_repeat *data = (struct convdata_repeat *)convdata;
     data->ratio = (data->format.samplerate) / ((gfloat)outrate);
}

static void register_drivers(void)
{
     static int already_run = 0;
     struct driver_data *d,d2;
#ifdef HAVE_LIBSAMPLERATE
     const char *c;
     int i;
     c = NULL; 
     i = 0;
#endif
     if (already_run) return;
     already_run++;

#ifdef HAVE_LIBSAMPLERATE

     d2.is_realtime = TRUE;
     d2.prefers_float = TRUE;
     d2.new_func = rateconv_src_new;
     d2.destroy_func = rateconv_src_destroy;
     d2.write_func = rateconv_src_write;
     d2.read_func = rateconv_src_read;
     d2.hasdata_func = rateconv_src_hasdata;
     d2.set_outrate_func = rateconv_src_set_outrate;

     for (i=0; 1; i++) {
	  c = src_get_name(i);
	  if (c == NULL) break;
	  d = (struct driver_data *)g_malloc(sizeof(*d));
	  memcpy(d,&d2,sizeof(*d));
	  d->name = c;	  
	  d->id = g_strdup_printf("src%c",i+'1');
	  drivers = g_list_append(drivers,d);
	  realtime_drivers = g_list_append(realtime_drivers,d);
     }
     
#endif

     if (sox_dialog_first_effect() != NULL || program_exists("sox")) {

	  d2.is_realtime = FALSE;
	  d2.prefers_float = FALSE;
	  d2.new_func = sox_new;
	  d2.destroy_func = sox_destroy;
	  d2.write_func = sox_write;
	  d2.read_func = sox_read;
	  d2.hasdata_func = sox_hasdata;

	  d = (struct driver_data *)g_malloc(sizeof(*d));
	  memcpy(d,&d2,sizeof(*d));
	  d->name = _("(SoX) Simulated analog filtration");
	  d->id = "sox_resample";
	  drivers = g_list_append(drivers,d);

	  d = (struct driver_data *)g_malloc(sizeof(*d));
	  memcpy(d,&d2,sizeof(*d));
	  d->name = _("(SoX) Polyphase interpolation");
	  d->id = "sox_polyphase";
	  drivers = g_list_append(drivers,d);	  

     }

     d = g_malloc(sizeof(*d));
     d->name = _("Sample repeat/skip (low quality)");
     d->id = "repeat";
     d->is_realtime = TRUE;
     d->prefers_float = FALSE;
     d->new_func = repeat_new;
     d->destroy_func = repeat_destroy;
     d->write_func = repeat_write;
     d->read_func = repeat_read;
     d->hasdata_func = repeat_hasdata;
     d->set_outrate_func = repeat_set_outrate;
     drivers = g_list_append(drivers,d);
     realtime_drivers = g_list_append(realtime_drivers,d);

     repeat_driver = d;
}

int rateconv_driver_count(gboolean realtime)
{
     GList *l;
     register_drivers();
     l = realtime ? realtime_drivers : drivers;
     return g_list_length(l);
}

const gchar *rateconv_driver_name(gboolean realtime, int index)
{
     GList *l;
     struct driver_data *d;
     register_drivers();
     l = realtime ? realtime_drivers : drivers;
     d = (struct driver_data *)g_list_nth_data(l,index);
     return d->name;
}

const gchar *rateconv_driver_id(gboolean realtime, int index)
{
     GList *l;
     struct driver_data *d;
     register_drivers();
     l = realtime ? realtime_drivers : drivers;
     d = (struct driver_data *)g_list_nth_data(l,index);
     return d->id;
}

int rateconv_driver_index(gboolean realtime, const gchar *driver_id)
{
     GList *l;
     int i;
     struct driver_data *d;
     l = realtime ? realtime_drivers : drivers;
     for (i=0; l!=NULL; i++,l=l->next) {
	  d = (struct driver_data *)l->data;
	  if (!strcmp(d->id,driver_id)) return i;
     }
     return -1;
}

gboolean rateconv_prefers_float(const gchar *driver_id)
{
     GList *l;
     struct driver_data *d;
     for (l=drivers; l!=NULL; l=l->next) {
	  d = (struct driver_data *)l->data;
	  if (!strcmp(d->id,driver_id))
	       return d->prefers_float;	  
     }
     return FALSE;
}

rateconv *rateconv_new(gboolean realtime, const char *driver_id, 
		       Dataformat *format, guint32 outrate, int dither_mode,
		       gboolean passthru)
{
     GList *l;
     struct driver_data *d;
     struct convdata_base *conv;
     register_drivers();

     /* If desired sample rate is same as input rate, use the repeat
      * driver, which has special case for this. */
     if (format->samplerate == outrate && !realtime)
	  driver_id = "repeat";

     l = realtime ? realtime_drivers : drivers;
     for (; l!=NULL; l=l->next) {
	  d = (struct driver_data *)l->data;
	  if (!strcmp(d->id,driver_id)) {	       
	       conv = d->new_func(d,realtime,format,(outrate>0)?outrate:44100,dither_mode);
	       if (conv == NULL) return NULL;
	       conv->driver = d;
	       conv->inrate = format->samplerate;
	       conv->outrate = outrate;
	       if (passthru)
		    conv->passthru_buffer = 
			 ringbuf_new(16384 - (16384%format->samplebytes));
	       else
		    conv->passthru_buffer = NULL;
	       conv->emptying = FALSE;
	       conv->readcount = conv->writecount = 0;
	       conv->empty_point = 0;
	       return conv;
	  }
     }     
     return NULL;
}

gint rateconv_write(rateconv *conv, void *data, guint bufsize)
{
     struct convdata_base *convdata = (struct convdata_base *)conv;
     guint i = 0;
     gint j;
     if (convdata->passthru_buffer != NULL && 
	 convdata->inrate == convdata->outrate) {
	  i = ringbuf_freespace(convdata->passthru_buffer);
	  if (i < bufsize) bufsize = i;	  
     }
     j = convdata->driver->write_func(conv,data,bufsize);
     if (j > 0 && i > 0) {
	  g_assert(j <= i);
	  i = ringbuf_enqueue(convdata->passthru_buffer,data,j);
	  g_assert(i == j);
     }
     if (j > 0) convdata->writecount += j;
     return j;
}

gint rateconv_read(rateconv *conv, void *data, guint bufsize)
{
     struct convdata_base *convdata = (struct convdata_base *)conv;
     gint i;

     if (convdata->outrate == 0) {
	  memset(data,0,bufsize);
	  return bufsize;
     }
	  
     i = convdata->driver->read_func(conv,data,bufsize);

     if (i > 0) {

	  convdata->readcount += i;
	  while (convdata->readcount > convdata->outrate) {
	       convdata->readcount -= convdata->outrate;

	       if (convdata->empty_point > convdata->outrate)
		    convdata->empty_point -= convdata->outrate;
	       else
		    convdata->empty_point = 0;

	       if (convdata->writecount > convdata->inrate)
		    convdata->writecount -= convdata->inrate;
	       else
		    convdata->writecount = 0;	       

	  }

	  if (convdata->emptying && 
	      convdata->readcount >= convdata->empty_point)
	       convdata->emptying = FALSE;

	  if (convdata->passthru_buffer != NULL && 
	      convdata->inrate == convdata->outrate && !convdata->emptying)
	       return ringbuf_dequeue(convdata->passthru_buffer,data,i);

     }
     return i;
}

gboolean rateconv_hasdata(rateconv *conv)
{
     struct convdata_base *convdata = (struct convdata_base *)conv;
     return convdata->driver->hasdata_func(conv);
}

void rateconv_destroy(rateconv *conv)
{
     struct convdata_base *convdata = (struct convdata_base *)conv;
     return convdata->driver->destroy_func(conv);
}

#define FLOAT(x) ((float)x)
#define GUINT32(x) ((guint32)x)

void rateconv_set_outrate(rateconv *conv, guint32 outrate)
{
     struct convdata_base *convdata = (struct convdata_base *)conv;
     g_assert(convdata->driver->is_realtime);
     if (outrate > 0) 
	  convdata->driver->set_outrate_func(conv,outrate);
     convdata->outrate = outrate;
     if (convdata->passthru_buffer != NULL && 
	 convdata->outrate == convdata->inrate) {
	  ringbuf_drain(convdata->passthru_buffer);
	  convdata->emptying = TRUE;
	  convdata->empty_point = GUINT32(FLOAT(convdata->writecount) * 
					  (FLOAT(convdata->outrate) / 
					   FLOAT(convdata->inrate)));	  
     }
}
