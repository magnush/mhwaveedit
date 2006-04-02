/*
 * Copyright (C) 2002 2003 2004 2005, Magnus Hjorth
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include "gtkfiles.h"
#include "um.h"
#include "chunk.h"
#include "inifile.h"
#include "main.h"
#include "ringbuf.h"
#include "rawdialog.h"
#include "tempfile.h"
#include "rateconv.h"
#include "gettext.h"


gboolean chunk_filter_use_floating_tempfiles;

static GtkObjectClass *parent_class;

static GList *chunks = NULL;

guint chunk_alive_count(void)
{
     return g_list_length ( chunks );
}

static void chunk_destroy(GtkObject *object)
{
     Chunk *c;
     GList *l;
     DataPart *dp;
     /* printf("chunk_destroy: %p\n",object); */
     c=CHUNK(object);
     g_assert(c->opencount == 0);
     if (c->parts) {
	  for (l=c->parts; l!=NULL; l=l->next) {
	       dp = (DataPart *)l->data;
	       gtk_object_unref(GTK_OBJECT(dp->ds));
	       g_free(dp);
	  }
	  g_list_free(c->parts);
     }
     c->parts = NULL;
     parent_class->destroy(object);
     chunks = g_list_remove ( chunks, object );
}

static void chunk_class_init(GtkObjectClass *klass)
{
     parent_class = gtk_type_class(gtk_object_get_type());
     klass->destroy = chunk_destroy;
}

static void chunk_init(Chunk *object)
{
     /* printf("chunk_init: %p\n",object); */
     object->format.type = DATAFORMAT_PCM;
     object->format.samplerate=44100;
     object->format.samplesize=2;
     object->format.channels=2;
     object->format.sign=1;
     object->format.bigendian = IS_BIGENDIAN;
     object->format.samplebytes=4;
     object->length=0;
     object->size=0;
     object->parts = 0;
     chunks = g_list_append( chunks, object );
}

guint chunk_get_type(void)
{
     static guint id=0;
     if (!id) {
	  GtkTypeInfo info = {
	       "Chunk",
	       sizeof(Chunk),
	       sizeof(ChunkClass),
	       (GtkClassInitFunc) chunk_class_init,
	       (GtkObjectInitFunc) chunk_init 
	  };
	  id=gtk_type_unique(gtk_object_get_type(),&info);
     }
     return id;
}

static Chunk *chunk_new(void)
{
     return gtk_type_new(chunk_get_type());
}

Chunk *chunk_new_silent(Dataformat *format, gfloat seconds)
{
     return chunk_new_from_datasource(
	  datasource_new_silent(format, seconds * format->samplerate));
}

static void chunk_calc_length(Chunk *c)
{
     GList *l;
     DataPart *dp;
     off_t len=0;
     for(l=c->parts; l!=NULL; l=l->next) {
	  dp = (DataPart *)l->data;
	  len += dp->length;
     }
     c->length = len;
     c->size = len*c->format.samplebytes;
}

static rateconv *conv;
static const gchar *conv_driver;
static guint32 conv_outrate;
static int conv_dither_mode;

static gboolean chunk_convert_samplerate_proc(void *in, guint sample_size,
					      chunk_writeout_func out_func,
					      WriteoutID id, 
					      Dataformat *informat,
					      Dataformat *outformat)
{
     guint i=0;
     gchar *p = (gchar *)in;
     gint j,k;
     gchar buf[4096];
     if (conv == NULL) 
	  conv = rateconv_new(FALSE,conv_driver,informat,
			      conv_outrate,conv_dither_mode);
     if (conv == NULL) return TRUE;
     if (sample_size == 0) rateconv_write(conv,NULL,0);
     while (1) {
	  if (sample_size > 0) {
	       /*puts("chunk_convert_samplerate_proc: Calling rateconv_write.."); */
	       j = rateconv_write(conv,p+i,sample_size-i);
	       /* printf("rateconv_write: tried=%d, got=%d\n",(int)sample_size-i,
		  j); */
	       if (j == -1) return TRUE;
	       i += j;
	       if (i == sample_size) return FALSE;
	  }
	  /* puts("chunk_convert_samplerate_proc: calling rateconv_read..."); */
	  k = rateconv_read(conv,buf,sizeof(buf));
	  /* printf("rateconv_read: tried=%d, got=%d\n",(int)sizeof(buf),k); */
	  if (k == -1) return TRUE;
	  if (k == 0 && sample_size == 0) return FALSE;
	  if (k>0 && out_func(id,buf,k)) return TRUE;
     }
}


Chunk *chunk_convert_samplerate(Chunk *chunk, gint samplerate, 
				const gchar *rateconv_driver, 
				int dither_mode, StatusBar *bar)
{
     Chunk *c;
     Dataformat fmt;
     /*puts("chunk_convert_samplerate: calling rateconv_new");*/
     conv = NULL;
     conv_driver = rateconv_driver;
     conv_outrate = samplerate;
     conv_dither_mode = dither_mode;
     memcpy(&fmt,&(chunk->format),sizeof(Dataformat));
     fmt.samplerate = samplerate;

     /* puts("chunk_convert_samplerate: calling chunk_filter"); */
     c = chunk_filter_tofmt(chunk,chunk_convert_samplerate_proc,
			    chunk_convert_samplerate_proc,
			    CHUNK_FILTER_MANY,
			    rateconv_prefers_float(rateconv_driver),&fmt,
			    dither_mode,
			    bar, _("Converting samplerate"));
     /* puts("chunk_convert_samplerate: calling rateconv_destroy"); */
     if (conv != NULL) rateconv_destroy(conv);
     return c;
}

Chunk *chunk_convert_speed(Chunk *chunk, gfloat speed_factor, 
			   int dither_mode, StatusBar *bar)
{
     guint32 target_samplerate;
     Chunk *c;
     int i;
     target_samplerate = (guint32)(((float)chunk->format.samplerate) / 
				   speed_factor);
     conv_outrate = target_samplerate;

     i = inifile_get_guint32("speedConv",0);
     if (i >= rateconv_driver_count(FALSE)) i=0;
     conv_driver = rateconv_driver_id(FALSE,i);
     
     conv = NULL;
     c = chunk_filter_tofmt(chunk,chunk_convert_samplerate_proc,
			    chunk_convert_samplerate_proc,CHUNK_FILTER_MANY,
			    FALSE,&(chunk->format),dither_mode,
			    bar,_("Adjusting speed"));
     if (conv != NULL) rateconv_destroy(conv);
     return c;
}

Chunk *chunk_convert_sampletype(Chunk *chunk, Dataformat *newtype)
{
     GList *l;
     Datasource *ds;
     Chunk *r=NULL,*c,*c2;
     DataPart *dp;
     Dataformat newformat;

     g_assert(!dataformat_samples_equal(&(chunk->format),newtype));
     memcpy(&newformat,newtype,sizeof(Dataformat));
     newformat.samplerate = chunk->format.samplerate;
     newformat.channels = chunk->format.channels;

     for (l=chunk->parts; l!=NULL; l=l->next) {
	  dp = (DataPart *)(l->data);
	  ds = datasource_convert(dp->ds,&newformat);
	  g_assert(ds != NULL);
	  c2 = chunk_new_from_datasource(ds);
	  c = chunk_get_part(c2,dp->position,dp->length);
	  gtk_object_sink(GTK_OBJECT(c2));
	  if (r != NULL) {
	       c2 = r;
	       r = chunk_append(c2,c);
	       gtk_object_sink(GTK_OBJECT(c2));
	       gtk_object_sink(GTK_OBJECT(c));
	  } else
	       r = c;
     }
     
     return r;
}

static gint channel_to_remove, current_channel;

static gboolean chunk_remove_channel_proc(void *in, guint samplesize, 
					  chunk_writeout_func out_func, 
					  WriteoutID id,
					  Dataformat *format, 
					  Dataformat *outformat)
{
     guint c = current_channel;
     current_channel++;
     if (current_channel == format->channels) current_channel = 0;
     if (c != channel_to_remove) return out_func(id,in,samplesize);
     else return FALSE;
}

Chunk *chunk_remove_channel(Chunk *chunk, gint channel, StatusBar *bar)
{
     Dataformat fmt;
     channel_to_remove = channel;
     current_channel = 0;
     memcpy(&fmt,&(chunk->format),sizeof(Dataformat));
     fmt.channels --;
     return chunk_filter_tofmt(chunk,chunk_remove_channel_proc,NULL,
			       CHUNK_FILTER_ONE,FALSE,&fmt,DITHER_NONE,bar,
			       _("Removing channel"));
}

gboolean chunk_dump(Chunk *chunk, EFILE *file, gboolean bigendian, 
		    int dither_mode, StatusBar *bar)
{
     GList *l;
     DataPart *dp;
     l = chunk->parts;
     while (l) {
	  dp = (DataPart *)l->data;
	  if (datasource_dump(dp->ds,dp->position,dp->length,file,
			      dither_mode,bar))
	       return TRUE;
	  l = l->next;
     }
     return FALSE;
}

static gboolean has_fake_pcm(Chunk *c)
{
     GList *l;
     DataPart *part;
     for (l=c->parts; l!=NULL; l=l->next) {
	  part = (DataPart *)(l->data);
	  if (part->ds->type == DATASOURCE_CONVERT) return TRUE;
     }
     return FALSE;
}

/* Allmän funktion för stegvis bearbetning av en Chunk.
 * Funktionen läser steg för steg av chunken och anropar proc.  
 * Efter allt data skickats in kommer eof_proc (om != NULL) att anropas med 
 * samplesize=0.
 */

Chunk *chunk_filter(Chunk *chunk, chunk_filter_proc proc, 
		    chunk_filter_proc eof_proc, gint amount, 
		    gboolean must_convert, int dither_mode, 
		    StatusBar *bar, gchar *title)
{
     return chunk_filter_tofmt(chunk,(chunk_filter_tofmt_proc)proc,
			       (chunk_filter_tofmt_proc)eof_proc,amount,
			       must_convert,&(chunk->format),dither_mode,
			       bar,title);
}

struct convert_back {
     TempFile tmp;
     Dataformat *tofmt;
     gchar buf[16384];
     int dither_mode;
};

static gboolean convert_back_write(WriteoutID id, gpointer data, guint length)
{
     struct convert_back *cbp = (struct convert_back *)id;
     guint i;
     i = length / sizeof(sample_t);
     convert_array(data,&dataformat_sample_t,cbp->buf,cbp->tofmt,i,
		   cbp->dither_mode);
     return tempfile_write(cbp->tmp,cbp->buf,i*cbp->tofmt->samplesize);
}

Chunk *chunk_filter_tofmt(Chunk *chunk, chunk_filter_tofmt_proc proc, 
			  chunk_filter_tofmt_proc eof_proc, 
			  gint amount, gboolean must_convert, 
			  Dataformat *tofmt, int dither_mode,
			  StatusBar *bar, gchar *title)
{
     guint proc_size,single_size,full_size;
     gchar buf[8192];
     off_t samplepos=0, samplesleft=chunk->length;
     guint u, x;
     gboolean convert, convert_back;
     Dataformat informat,outformat;
     ChunkHandle *ch;
     TempFile tmp;
     Chunk *ds,*r;
     struct convert_back *cbp = NULL;
     
     /* Force processing in floating-point if any of the Datasources has 
      * faked pcm data */
     convert = must_convert || has_fake_pcm(chunk);
     convert_back = convert && !chunk_filter_use_floating_tempfiles;

     if (convert) {
	  informat.type = outformat.type = DATAFORMAT_FLOAT;
	  informat.samplesize = outformat.samplesize = sizeof(sample_t);
	  informat.channels = chunk->format.channels;
	  outformat.channels = tofmt->channels;
	  informat.samplebytes = informat.samplesize * informat.channels;
	  outformat.samplebytes = outformat.samplesize * outformat.channels;
	  informat.samplerate = chunk->format.samplerate;	  
	  outformat.samplerate = tofmt->samplerate;
     } else {
	  memcpy(&informat,&(chunk->format),sizeof(Dataformat));
	  memcpy(&outformat,tofmt,sizeof(Dataformat));
     }

     single_size = informat.samplesize;
     full_size = informat.samplebytes;

     if (amount == CHUNK_FILTER_FULL) proc_size = full_size;     
     else if (amount == CHUNK_FILTER_ONE) proc_size = single_size;
     else proc_size = 0; /* Set this later for CHUNK_FILTER_MANY */

     ch = chunk_open(chunk);
     if (ch == NULL) return NULL;
     tmp = tempfile_init(convert_back ? tofmt : &outformat,FALSE);
     if (convert_back) {
	  cbp = g_malloc(sizeof(*cbp));
	  cbp->tmp = tmp;
	  cbp->tofmt = tofmt;
	  cbp->dither_mode = dither_mode;
     }
     status_bar_begin_progress(bar,samplesleft,title);

     while (samplesleft > 0) {
	  if (convert)
	       u = chunk_read_array_fp(ch,samplepos,
				       MIN(sizeof(buf)/full_size,samplesleft),
				       (sample_t *)buf,dither_mode)*full_size;
	  else
	       u = chunk_read_array(ch,samplepos,
				    MIN(sizeof(buf),samplesleft*full_size),buf,
				    dither_mode);
	  if (!u) {
	       chunk_close(ch);
	       tempfile_abort(tmp);
	       if (cbp != NULL) g_free(cbp);
	       status_bar_end_progress(bar);
	       return NULL;
	  }
	  if (amount == CHUNK_FILTER_MANY) proc_size = u;
	  for (x=0; x<u; x+=proc_size) {
	       if (proc(buf+x,proc_size,
			convert_back?convert_back_write:tempfile_write,
			convert_back?cbp:tmp, &informat,&outformat)) {
		    chunk_close(ch);
		    tempfile_abort(tmp);
		    if (cbp != NULL) g_free(cbp);
		    status_bar_end_progress(bar);
		    return NULL;
	       }
	  }
	  samplesleft -= u / full_size;
	  samplepos += u / full_size;
	  if (status_bar_progress(bar, u / full_size)) {
	       chunk_close(ch);
	       tempfile_abort(tmp);
	       if (cbp != NULL) g_free(cbp);
	       status_bar_end_progress(bar);
	       return NULL;
	  }
     }
     if (eof_proc!=NULL && 
	 eof_proc(NULL,0, convert_back?convert_back_write:tempfile_write,
		  convert_back?cbp:tmp, &informat,&outformat)) {
	  chunk_close(ch);
	  tempfile_abort(tmp);
	  if (cbp != NULL) g_free(cbp);
	  status_bar_end_progress(bar);
	  return NULL;
     }
     chunk_close(ch);
     ds = tempfile_finished(tmp);
     status_bar_end_progress(bar);

     /* Check if the datasources format is the same as the user expects. If 
      * not, convert. */
     if (ds != NULL && convert && !convert_back && 
	 !dataformat_samples_equal(&(ds->format),tofmt)) {

	  r = chunk_convert_sampletype(ds,tofmt);
	  gtk_object_sink(GTK_OBJECT(ds));
	  return r;
     } else 
	  return ds;
}

/* Allmän funktion för stegvis avläsning av en Chunk. proc kommer att anropas 
 * en gång för varje sampling i chunken */

gboolean chunk_parse(Chunk *chunk, chunk_parse_proc proc, 
		     gboolean allchannels, gboolean convert, int dither_mode, 
		     StatusBar *bar, gchar *title)
{

     guint single_size,full_size,proc_size;
     gchar buf[4096],*c=NULL,*d;
     off_t samplepos=0, samplesleft=chunk->length;
     guint u, x;
     ChunkHandle *ch;

     if (convert) single_size = sizeof(sample_t);
     else single_size = chunk->format.samplesize;
     
     full_size = single_size * chunk->format.channels;

     if (allchannels) proc_size = full_size;
     else proc_size = single_size;


     ch = chunk_open(chunk);
     if (ch == NULL) { g_free(c); return TRUE; }
     status_bar_begin_progress( bar, chunk->length, title);

     while (samplesleft > 0) {
	  if (convert)	       
	       u = chunk_read_array_fp(ch,samplepos,sizeof(buf)/full_size,
				       (sample_t *)buf,dither_mode)*full_size;
	  else
	       u = chunk_read_array(ch,samplepos,sizeof(buf),buf,dither_mode);
	  if (!u) {
	       chunk_close(ch);
	       g_free(c);
	       status_bar_end_progress(bar);
	       return TRUE;
	  }
	  d = buf;
	  for (x=0; x<u; x+=proc_size) {
	       if (proc(d+x,proc_size,chunk)) {
		    chunk_close(ch);
		    g_free(c);
		    status_bar_end_progress(bar);
		    return TRUE;
	       }
	  }
	  samplesleft -= u / full_size;
	  samplepos += u / full_size;
	  if (status_bar_progress(bar, u / full_size)) {
	       chunk_close(ch);
	       g_free(c);
	       status_bar_end_progress(bar);
	       return TRUE;
	  }
     }
     chunk_close(ch);
     g_free(c);
     status_bar_end_progress(bar);
     return FALSE;
}

static gboolean chunk_onechannel_proc(void *in, guint sample_size, 
				      chunk_writeout_func out_func, 
				      WriteoutID id, Dataformat *format,
				      Dataformat *outformat)
{
     static int channel_count = 0;
     static sample_t mixup = 0.0;
     sample_t s = *((sample_t *)in);
     mixup += s;
     channel_count++;
     if (channel_count == format->channels) {
	  s = mixup / (sample_t)channel_count;
	  channel_count = 0;
	  mixup = 0.0;
	  return out_func(id,&s,sizeof(s));
     } else 
	  return FALSE;
}

Chunk *chunk_onechannel(Chunk *chunk, int dither_mode, StatusBar *bar)
{
     Dataformat fmt;
     memcpy(&fmt,&(chunk->format),sizeof(Dataformat));
     fmt.channels = 1;
     fmt.samplebytes = fmt.samplesize;
     return chunk_filter_tofmt(chunk,chunk_onechannel_proc,NULL,
			       CHUNK_FILTER_ONE,TRUE,&fmt,dither_mode,bar,
			       NULL);
}

static gint channel_to_copy;

static gboolean chunk_copy_channel_proc(void *in, guint sample_size, 
					chunk_writeout_func out_func, 
					WriteoutID id,
					Dataformat *informat, 
					Dataformat *outformat)
{
     static gint channel_count=0;
     gint i;
     i = channel_count;
     channel_count ++;
     if (channel_count == informat->channels) channel_count = 0;
     if (out_func(id,in,sample_size)) return TRUE;
     if (channel_to_copy==i) return out_func(id,in,sample_size);
     else return FALSE;
}

static gboolean chunk_copy_channel_mono_proc(void *in, guint sample_size,
					     chunk_writeout_func out_func, 
					     WriteoutID id,
					     Dataformat *informat, 
					     Dataformat *outformat)
{
     return (out_func(id,in,sample_size) || out_func(id,in,sample_size));
}

Chunk *chunk_copy_channel(Chunk *chunk, gint channel, int dither_mode, 
			  StatusBar *bar)
{
     Chunk *c;
     Dataformat fmt;
     memcpy(&fmt,&(chunk->format),sizeof(Dataformat));
     fmt.channels ++;
     fmt.samplebytes = fmt.channels * fmt.samplesize;
     channel_to_copy = channel;
     if (chunk->format.channels == 1) 
	  c = chunk_filter_tofmt(chunk,chunk_copy_channel_mono_proc,NULL,
				 CHUNK_FILTER_ONE,FALSE,&fmt,dither_mode,bar,
				 NULL);
     else 
	  c = chunk_filter_tofmt(chunk,chunk_copy_channel_proc,NULL,
				 CHUNK_FILTER_ONE,FALSE,&fmt,dither_mode, 
				 bar,NULL);
     return c;
}


Chunk *chunk_mix(Chunk *c1, Chunk *c2, int dither_mode, StatusBar *bar)
{
     #define BUFLEN 256
     off_t u,mixlen;
     guint i,chn,x1,x2,x;
     guint clipcount = 0;
     sample_t buf1[BUFLEN],buf2[BUFLEN],buf3[BUFLEN];
     sample_t s,pmax;
     gchar *str;
     Dataformat format;
     Chunk *m,*c,*d;
     ChunkHandle *ch1, *ch2;
     TempFile tmp;

     g_assert(chunk_format_equal(c1,c2));

     pmax = maximum_float_value(&(c1->format));

     /* Number of channels */
     chn = c1->format.channels;
     /* Number of multi-channel samples to mix */
     mixlen = MIN(c1->length, c2->length);

     memcpy(&format,&(c1->format),sizeof(Dataformat));
     format.type = DATAFORMAT_FLOAT;
     format.samplesize = sizeof(sample_t);
     format.samplebytes = format.samplesize * format.channels;

     /* Prepare for processing */
     ch1 = chunk_open(c1);
     if ( ch1 == NULL ) return NULL;
     ch2 = chunk_open(c2);
     if ( ch2 == NULL ) { chunk_close(ch1); return NULL; }
     tmp = tempfile_init(&format,FALSE);
     status_bar_begin_progress(bar, mixlen, _("Mixing"));

     for (u=0; u<mixlen; u+=x) {

	  x1 = chunk_read_array_fp(ch1,u,BUFLEN/chn,buf1,dither_mode);
	  x2 = chunk_read_array_fp(ch2,u,BUFLEN/chn,buf2,dither_mode);
	  if (x1 == 0 || x2 == 0) {
	       tempfile_abort(tmp);
	       chunk_close(ch1);
	       chunk_close(ch2);
	       status_bar_end_progress(bar);
	       return NULL;
	  }

	  /* Number of mixable multi-channel samples for this run */
	  x = MIN(x1,x2);
	  /* Mix the data from buf1,buf2 into buf3 */
	  for (i=0; i<x*chn; i++) {
	       s = buf1[i] + buf2[i];
	       if (s > pmax) { clipcount ++; s = pmax; }
	       else if (s < -1.0) { clipcount ++; s = -1.0; }
	       buf3[i] = s;
	  }	  

	  /* Write buf3 directly */
	  if (tempfile_write(tmp,buf3,x*chn*sizeof(sample_t))
	      || status_bar_progress(bar,x)) {
	       tempfile_abort(tmp);
	       chunk_close(ch1);
	       chunk_close(ch2);
	       status_bar_end_progress(bar);
	       return NULL;
	  }
     }

     /* Finish processing */
     chunk_close(ch1);
     chunk_close(ch2);
     m = tempfile_finished(tmp);
     status_bar_end_progress(bar);
     if (!m) return NULL;

     /* Fake the original pcm format */
     if (!dataformat_equal(&(c1->format),&(m->format))) {
	  c = chunk_convert_sampletype(m,&(c1->format));
	  g_assert(c != NULL);
	  gtk_object_sink(GTK_OBJECT(m));
	  m = c;
     }

     /* Warn if clipping occured */
     if (clipcount > 0) {
	  str = g_strdup_printf(_("The mixed result was clipped %d times."),
				clipcount);
	  user_warning(str);
	  g_free(str);
     }

     /* We may need to append some unmixed data at the end */
     if (c1->length == c2->length) return m;
     if (c1->length > mixlen)
	  c = chunk_get_part(c1,mixlen,c1->length-mixlen);
     else 
	  c = chunk_get_part(c2,mixlen,c2->length-mixlen);
     d = chunk_append(m,c);
     gtk_object_sink(GTK_OBJECT(m));
     gtk_object_sink(GTK_OBJECT(c));

     return d;
}


void chunk_foreach(GFunc func, gpointer user_data)
{
     g_list_foreach(chunks,func,user_data);
}

Chunk *chunk_new_from_datasource(Datasource *ds)
{
     Chunk *c;
     DataPart *dp;     
     if (ds==NULL) return NULL;
     dp = g_malloc(sizeof(*dp));
     dp->ds = ds;
     dp->position = 0;
     dp->length = ds->length;
     gtk_object_ref(GTK_OBJECT(ds));
     gtk_object_sink(GTK_OBJECT(ds));
     c = chunk_new();
     memcpy(&(c->format),&(ds->format),sizeof(Dataformat));
     c->length = ds->length;
     c->size = c->length * c->format.samplebytes;
     c->parts = g_list_append(NULL,dp);
     /* printf("chunk_new_from_datasource(%p) -> %p\n",ds,c); */
     return c;
}

ChunkHandle *chunk_open(Chunk *chunk)
{
     GList *l;
     DataPart *dp;
     for (l=chunk->parts;l!=NULL;l=l->next) {
	  dp = (DataPart *)l->data;
	  if (datasource_open(dp->ds)) {
	       for (l=l->prev;l!=NULL;l=l->prev) {
		    dp = (DataPart *)l->data;
		    datasource_close(dp->ds);
	       }
	       return NULL;
	  }
     }
     chunk->opencount ++;
     return chunk;     
}

void chunk_close(ChunkHandle *handle)
{
     GList *l;
     DataPart *dp;
     g_assert(handle->opencount > 0);
     for (l=handle->parts;l!=NULL;l=l->next) {
	  dp = (DataPart *)l->data;
	  datasource_close(dp->ds);
     }
     handle->opencount --;     	  
}

gboolean chunk_read(ChunkHandle *handle, off_t sampleno, void *buffer,
		    int dither_mode)
{
     GList *l;     
     DataPart *dp;
     for (l=handle->parts;l!=NULL;l=l->next) {
	  dp = (DataPart *)l->data;
	  if (dp->length > sampleno) 
	       return datasource_read(dp->ds,sampleno,buffer,dither_mode);
	  else
	       sampleno -= dp->length;
     }
     g_assert_not_reached();
     return TRUE;
}

gboolean chunk_read_fp(ChunkHandle *handle, off_t sampleno, sample_t *buffer,
		       int dither_mode)
{
     GList *l;     
     DataPart *dp;
     for (l=handle->parts;l!=NULL;l=l->next) {
	  dp = (DataPart *)l->data;
	  if (dp->length > sampleno) 
	       return datasource_read_fp(dp->ds,dp->position+sampleno,buffer,
					 dither_mode);
	  else
	       sampleno -= dp->length;
     }
     g_assert_not_reached();
     return TRUE;
}

guint chunk_read_array(ChunkHandle *handle, off_t sampleno, 
		       guint size, void *buffer, int dither_mode)
{
     GList *l;     
     DataPart *dp;
     guint r=0,i,j;
     off_t o;
     for (l=handle->parts;l!=NULL;l=l->next) {
	  dp = (DataPart *)l->data;
	  if (dp->length > sampleno) {
	       /* Calculate required size if it's larger than we need. */
	       o = dp->length - sampleno;	       
	       o *= (off_t)(handle->format.samplebytes);
	       if (o < (off_t)size) j = (guint)o;
	       else j = size;
	       /* Read data. */
	       i = datasource_read_array(dp->ds,
					 dp->position + sampleno,
					 j,buffer,dither_mode);
	       if (i == 0) return 0;
	       r+=i;
	       sampleno = 0;
	       size -= i;
	       buffer = ((gchar *)buffer) + i;
	       if (size==0 || l->next==NULL) return r;
	  } else
	       sampleno -= dp->length;
     }
     return r;
}

guint chunk_read_array_fp(ChunkHandle *handle, off_t sampleno, 
			  guint samples, sample_t *buffer, int dither_mode)
{
     GList *l;     
     DataPart *dp;
     guint r=0,i,j;
     off_t o;
     g_assert(sampleno < handle->length);
     for (l=handle->parts;l!=NULL;l=l->next) {
	  dp = (DataPart *)l->data;
	  if (dp->length > sampleno) {
	       o = dp->length-sampleno;
	       if (o < (off_t)samples)
		    j = (guint)o;
	       else
		    j = samples;
	       i = datasource_read_array_fp(dp->ds,
					    dp->position+sampleno,
					    j,buffer,dither_mode);
	       g_assert(i <= samples);
	       g_assert(i <= dp->length - sampleno);
	       if (i==0) return 0;
	       r += i;
	       buffer += i*handle->format.channels;
	       samples -= i;
	       sampleno = 0;
	       if (samples == 0 || l->next==NULL) return r;
	  } else
	       sampleno -= dp->length;
     }
     return r;
}

static DataPart *datapart_list_copy(GList *src, GList **dest)
{
     DataPart *old,*new;
     old = src->data;
     new = g_malloc(sizeof(DataPart));
     new->ds = old->ds;
     new->position = old->position;
     new->length = old->length;
     gtk_object_ref(GTK_OBJECT(old->ds));
     (*dest) = g_list_append((*dest),new);
     return new;
}

Chunk *chunk_append(Chunk *first, Chunk *second)
{
     Chunk *c;
     GList *nl=NULL,*l;
     off_t len=0;
     DataPart *dp;
     g_assert(chunk_format_equal(first,second));
     for (l=first->parts; l!=NULL; l=l->next) {
	  dp = datapart_list_copy(l,&nl);
	  len += dp->length;
     }
     for (l=second->parts; l!=NULL; l=l->next) {
	  dp = datapart_list_copy(l,&nl);
	  len += dp->length;
     }
     c = chunk_new();
     memcpy(&(c->format),&(first->format),sizeof(Dataformat));
     c->parts = nl;
     c->length = len;
     c->size = len * c->format.samplebytes;
     return c;
}

Chunk *chunk_insert(Chunk *chunk, Chunk *part, off_t sampleno)
{
     GList *nl=NULL,*l,*m;     
     DataPart *dp;
     Chunk *c;

     g_assert(sampleno <= chunk->length);
     g_assert(chunk_format_equal(chunk,part));
     if (sampleno == chunk->length) return chunk_append(chunk,part);

     /* Get beginning */
     for (l=chunk->parts; sampleno>0; l=l->next) {
	  dp = datapart_list_copy(l,&nl);
	  if (sampleno < dp->length) {
	       dp->length = sampleno;
	       break;
	  }
	  sampleno -= dp->length;
     }

     /* Add the inserted part */
     for (m=part->parts; m!=NULL; m=m->next) 
	  datapart_list_copy(m,&nl);

     /* Add the rest */
     for (; l!=NULL; l=l->next) {
	  dp = datapart_list_copy(l,&nl);
	  dp->position += sampleno;
	  dp->length -= sampleno;
	  sampleno = 0;
     }

     /* Create the chunk */
     c = chunk_new();
     memcpy(&(c->format),&(chunk->format),sizeof(Dataformat));
     c->parts = nl;
     c->length = chunk->length + part->length;
     c->size = chunk->size + part->size;
     return c;
}

Chunk *chunk_get_part(Chunk *chunk, off_t start, off_t length)
{
     GList *l,*nl=NULL;
     DataPart *dp;
     Chunk *c;
     l=chunk->parts;
     /* Skip beginning parts */
     while (1) {
	  dp = (DataPart *)l->data;
	  if (start < dp->length) break;
	  start -= dp->length;
	  l=l->next;
     }
     while (length>0 && l!=NULL) {
	  dp = datapart_list_copy(l,&nl);
	  dp->position += start;
	  dp->length -= start;
	  start = 0;
	  if (dp->length > length) dp->length=length;
	  length -= dp->length;
	  l=l->next;
     }

     c = chunk_new();
     memcpy(&(c->format),&(chunk->format),sizeof(Dataformat));
     c->parts = nl;
     chunk_calc_length(c);
     return c;
}

Chunk *chunk_remove_part(Chunk *chunk, off_t start, off_t length)
{
     GList *nl=NULL,*l;
     DataPart *dp;
     Chunk *c;
     
     /* Copy beginning parts */
     l=chunk->parts;
     while (start>0) {
	  dp = datapart_list_copy(l,&nl);
	  if (start < dp->length) {
	       dp->length = start;
	       break;
	  }
	  start -= dp->length;
	  l=l->next;
     }
     
     /* Skip parts */
     length += start;
     while (length>0 && l!=NULL) {
	  dp = (DataPart *)l->data;
	  if (dp->length > length) break;
	  length -= dp->length;
	  l=l->next;
     }

     /* Copy remaining parts */
     for (;l!=NULL;l=l->next) {
	  dp = datapart_list_copy(l,&nl);
	  dp->position += length;
	  dp->length -= length;	  
	  length = 0;
     }

     c = chunk_new();
     memcpy(&(c->format),&(chunk->format),sizeof(Dataformat));
     c->parts = nl;
     chunk_calc_length(c);
     return c;
}

Chunk *chunk_replace_part(Chunk *chunk, off_t start, off_t length, Chunk *new)
{
     Chunk *c;
     GList *nl=NULL,*l;
     DataPart *dp;
     guint i;
     for (l=new->parts; l!=NULL; l=l->next)
	  datapart_list_copy(l,&nl);
     c = chunk_remove_part(chunk,start,length);
     for (l=c->parts,i=0; start>0; l=l->next,i++) {
	  dp = (DataPart *)l->data;
	  g_assert(dp->length <= start);
	  start -= dp->length;
     }
     for (l=nl; l!=NULL; l=l->next,i++)
	  c->parts = g_list_insert(c->parts, l->data, i);
     g_list_free(nl);
     c->length += new->length;
     c->size = c->length * c->format.samplebytes;
     return c;
}

Chunk *chunk_clone_df(Chunk *chunk, Dataformat *format)
{
     Chunk *c;
     GList *l,*nl=NULL;
     DataPart *dp,*ndp;

     for (l=chunk->parts; l!=NULL; l=l->next) {
	  dp = (DataPart *)l->data;
	  ndp = g_malloc(sizeof(*ndp));
	  ndp->ds = dp->ds;
	  if (!dataformat_equal(format,&(chunk->format))) 
	       ndp->ds = datasource_clone_df(ndp->ds, format);
	  gtk_object_ref(GTK_OBJECT(ndp->ds));
	  gtk_object_sink(GTK_OBJECT(ndp->ds));
	  ndp->position = dp->position;
	  ndp->length = (dp->length * dp->ds->format.samplebytes) /
	       ndp->ds->format.samplebytes;
	  nl = g_list_append(nl,ndp);
     }

     c = chunk_new();
     memcpy(&(c->format),format,sizeof(Dataformat));
     c->parts = nl;
     chunk_calc_length(c);
     return c;
}


static sample_t chunk_peak_level_max;

static gboolean chunk_peak_level_proc(void *sample, gint sample_size, 
				      Chunk *chunk)
{
     sample_t s;
     s = (sample_t)fabs((double)(*((sample_t *)sample)));
     if (s > chunk_peak_level_max) {
	  chunk_peak_level_max = s;
	  if (s == 1.0) return TRUE;
     }
     return FALSE;
}

sample_t chunk_peak_level(Chunk *c, StatusBar *bar)
{
     chunk_peak_level_max = 0.0;
     if (chunk_parse(c,chunk_peak_level_proc,FALSE,TRUE,DITHER_NONE,bar,
		     _("Calculating peak level"))) return -1.0;
     return chunk_peak_level_max;
}

static sample_t chunk_amplify_factor;

static gboolean chunk_amplify_proc(void *in, guint sample_size,
				   chunk_writeout_func out_func,
				   WriteoutID id, Dataformat *format)
{
     sample_t *sp,*ep;
     sp = ((sample_t *)in);
     ep = (sample_t *)(((gchar *)in)+sample_size);
     for (; sp<ep; sp++) *sp *= chunk_amplify_factor;
     return out_func(id,in,sample_size);
}

Chunk *chunk_amplify(Chunk *c, sample_t factor, int dither_mode, 
		     StatusBar *bar)
{
     chunk_amplify_factor = factor;
     return chunk_filter(c,chunk_amplify_proc,NULL,CHUNK_FILTER_MANY,TRUE,
			 dither_mode,bar,_("Amplifying"));
}

static sample_t ramp_start,ramp_diff;
static off_t samples_done,samples_total;

static gboolean volume_ramp_proc(void *in, guint sample_size,
				 chunk_writeout_func out_func,
				 WriteoutID id, Dataformat *format)
{
     sample_t *sp = (sample_t *)in;
     sample_t factor;
     int i,j;
     for (j=0; j<sample_size; j+=format->channels*sizeof(sample_t)) {
	  factor = ramp_start + (ramp_diff * ((sample_t)samples_done / 
					      (sample_t)samples_total));
	  for (i=0; i<format->channels; i++) {
	       *sp *= factor;
	       sp++;
	  }
	  samples_done ++;
     }
     return out_func(id,in,sample_size);
}

Chunk *chunk_volume_ramp(Chunk *c, sample_t start_factor, sample_t end_factor, 
			 int dither_mode, StatusBar *bar)
{
     if (start_factor == end_factor) 
	  return chunk_amplify(c,start_factor,dither_mode,bar);
     ramp_start = start_factor;
     ramp_diff = end_factor - start_factor;
     samples_done = 0;
     samples_total = c->length;
     return chunk_filter(c,volume_ramp_proc,NULL,CHUNK_FILTER_MANY,TRUE,
			 dither_mode, bar, _("Amplifying"));
}

Chunk *chunk_interpolate_endpoints(Chunk *chunk, int dither_mode, 
				   StatusBar *bar)
{
#define BUF_LENGTH 512
     sample_t startval[8],diffval[8];
     sample_t *sbuf;
     off_t ctr=0,length=chunk->length;
     guint bufctr;
     guint i;
     sample_t slength = (sample_t)length;
     guint k,channels=chunk->format.channels;
     Dataformat fmt;
     ChunkHandle *ch;
     TempFile tf;
     Chunk *r,*q;
     /* get the endpoint values */
     ch = chunk_open(chunk);
     if (ch == NULL) return NULL;
     if (chunk_read_fp(ch,0,startval,dither_mode) || 
	 chunk_read_fp(ch,length-1,diffval,dither_mode)) { 
	  chunk_close(ch);
	  return NULL;
     }
     chunk_close(ch);
     for (k=0;k<channels;k++) {
	  diffval[k]-=startval[k];
	  /* printf("%f %f\n",startval[k],diffval[k]); */
     }
     /* Prepare processing */
     memcpy(&fmt,&dataformat_sample_t,sizeof(Dataformat));
     fmt.channels = channels;
     fmt.samplerate = chunk->format.samplerate;
     fmt.samplebytes = fmt.channels * fmt.samplesize;
     tf = tempfile_init(&fmt,FALSE);
     if (tf == NULL) return NULL;
     status_bar_begin_progress(bar,chunk->length,NULL);
     sbuf = g_malloc(BUF_LENGTH*sizeof(sample_t));
     /* Output data */
     while (ctr < length) {
	  bufctr = BUF_LENGTH / channels;
	  if ((length-ctr) < (off_t)bufctr) bufctr = (guint)(length-ctr);
	  for (i=0; i<bufctr*channels; i+=channels,ctr++)
	       for (k=0; k<channels; k++)
		    sbuf[i+k] = startval[k] + 
			 diffval[k]*(((sample_t)ctr)/slength);
	  if (tempfile_write(tf,sbuf,bufctr*channels*sizeof(sample_t)) ||
	      status_bar_progress(bar,bufctr)) {
	       g_free(sbuf);
	       tempfile_abort(tf);	       
	       status_bar_end_progress(bar);
	       return NULL;
	  }
     }
     /* Finish */
     g_free(sbuf);
     r = tempfile_finished(tf);
     status_bar_end_progress(bar);
     if (r!=NULL && !dataformat_equal(&(r->format),&(chunk->format))) {
       q = chunk_convert_sampletype(r,&(chunk->format));
       gtk_object_sink(GTK_OBJECT(r));
       return q;
     } else 
       return r;
	  
#undef BUF_LENGTH
}

Chunk *chunk_byteswap(Chunk *chunk)
{
     Dataformat fmt;
     memcpy(&fmt,&(chunk->format),sizeof(Dataformat));
     fmt.bigendian = !fmt.bigendian;
     return chunk_clone_df(chunk,&fmt);
}

static gboolean chunk_convert_channels_down_proc(void *in, guint sample_size,
						 chunk_writeout_func out_func,
						 WriteoutID id, 
						 Dataformat *informat, 
						 Dataformat *outformat)
{
     return out_func(id,in,outformat->samplebytes);
}

static guint zero_level_buf_len;
static gchar zero_level_buf[32];

static gboolean chunk_convert_channels_up_proc(void *in, guint sample_size,
					       chunk_writeout_func out_func,
					       WriteoutID id,
					       Dataformat *informat,
					       Dataformat *outformat)
{
     if (zero_level_buf_len == 0) {
	  sample_t s[8] = { 0 };
	  zero_level_buf_len = outformat->samplebytes - informat->samplebytes;
	  convert_array(s,&dataformat_sample_t,zero_level_buf,outformat,
			outformat->channels-informat->channels, DITHER_NONE);
	  /* printf("zero_level_buf_len = %d\n",zero_level_buf_len); */
     }
     return (out_func(id,in,informat->samplebytes) || 
	     out_func(id,zero_level_buf,zero_level_buf_len));
}

Chunk *chunk_convert_channels(Chunk *chunk, guint new_channels, 
			      int dither_mode, StatusBar *bar)
{
     Dataformat outformat;
     g_assert(chunk->format.channels != new_channels);
     memcpy(&outformat,&(chunk->format),sizeof(Dataformat));
     outformat.channels = new_channels;
     outformat.samplebytes = outformat.channels * outformat.samplesize;
     if (new_channels < chunk->format.channels)
	  return chunk_filter_tofmt(chunk,chunk_convert_channels_down_proc,
				    NULL,CHUNK_FILTER_FULL,FALSE,&outformat,
				    dither_mode, 
				    bar,_("Removing channels"));
     else {
	  zero_level_buf_len = 0;
	  return chunk_filter_tofmt(chunk,chunk_convert_channels_up_proc,
				    NULL,CHUNK_FILTER_FULL,FALSE,&outformat,
				    dither_mode, 
				    bar,_("Adding channels"));
     }
}

Chunk *chunk_convert(Chunk *chunk, Dataformat *new_format, 
		     int dither_mode, StatusBar *bar)
{
     Chunk *c=NULL,*d;
     const gchar *conv_driver;
     if (new_format->samplerate != chunk->format.samplerate) {
	  conv_driver = inifile_get("RateconvDefault",NULL);
	  if (conv_driver == NULL || 
	      rateconv_driver_index(FALSE,conv_driver)==-1)
	       conv_driver = rateconv_driver_id(FALSE,0);
	  if (new_format->channels < chunk->format.channels) {
	       /* First decrease channels, then convert rate */
	       d = chunk_convert_channels(chunk,new_format->channels,
					  dither_mode,bar);
	       if (d == NULL) return NULL;
	       c = chunk_convert_samplerate(d,new_format->samplerate,
					    conv_driver,dither_mode,bar);
	       gtk_object_sink(GTK_OBJECT(d));
	       if (c == NULL) return NULL;

	  } else if (new_format->channels > chunk->format.channels) {
	       /* First convert rate, then increase channels */
	       d = chunk_convert_samplerate(chunk,new_format->samplerate,
					    conv_driver,dither_mode,bar);
	       if (d == NULL) return NULL;
	       c = chunk_convert_channels(d,new_format->channels,dither_mode,
					  bar);
	       gtk_object_sink(GTK_OBJECT(d));
	       if (c == NULL) return NULL;

	  } else {
	       /* Just convert rate */
	       c = chunk_convert_samplerate(chunk,new_format->samplerate,
					    conv_driver,dither_mode,bar);
	       if (c == NULL) return NULL;
	  }
     } else if (new_format->channels != chunk->format.channels) {
	  /* Just convert channels */
	  c = chunk_convert_channels(chunk,new_format->channels,
				     dither_mode,bar);
	  if (c == NULL) return NULL;
     } 

     /* Sample format conversion */
     if (c == NULL) d = chunk;
     else d = c;
     if (dataformat_equal(&(d->format),new_format)) return d;
     d = chunk_convert_sampletype(d,new_format);
     if (c != NULL) gtk_object_sink(GTK_OBJECT(c));
     return d;
}
