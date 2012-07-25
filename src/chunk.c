/*
 * Copyright (C) 2002 2003 2004 2005 2006 2007 2008 2009 2011, Magnus Hjorth
 * Copyright (C) 2012, Magnus Hjorth
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
     object->format.packing=0;
     object->format.channels=2;
     object->format.sign=1;
     object->format.bigendian = IS_BIGENDIAN;
     object->format.samplebytes=4;
     object->length=0;
     object->size=0;
     object->parts = 0;
     chunks = g_list_append( chunks, object );
}

GtkType chunk_get_type(void)
{
     static GtkType id=0;
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
			      conv_outrate,conv_dither_mode,FALSE);
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
     if (conv != NULL) {
	  rateconv_destroy(conv);
	  conv = NULL;
     }
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

static Chunk *chunk_ds_remap(Chunk *chunk, int new_channels, int *map)
{
     GList *l;
     DataPart *dp;
     Datasource *new_ds;
     Chunk *new_chunk,*new_chunk_part;
     Chunk *res = NULL, *c;
     for (l=chunk->parts; l!=NULL; l=l->next) {
	  dp = (DataPart *)l->data;
	  new_ds = datasource_channel_map(dp->ds,new_channels,map);
	  new_chunk = chunk_new_from_datasource(new_ds);
	  new_chunk_part = chunk_get_part(new_chunk,dp->position,dp->length);
	  gtk_object_sink(GTK_OBJECT(new_chunk));
	  if (res == NULL) {
	       res = new_chunk_part;
	  } else {
	       c = chunk_append(res,new_chunk_part);
	       gtk_object_sink(GTK_OBJECT(res));
	       gtk_object_sink(GTK_OBJECT(new_chunk_part));
	       res = c;
	  }
     }
     g_free(map);
     return res;
}

Chunk *chunk_remove_channel(Chunk *chunk, gint channel, StatusBar *bar)
{
     int *map,i,new_chans;

     new_chans = chunk->format.channels - 1;
     map = g_malloc(new_chans * sizeof(int));
     for (i=0; i<new_chans; i++) {
	  if (i >= channel) map[i]=i+1;
	  else map[i]=i;
     }
     return chunk_ds_remap(chunk, new_chans, map);
}

gboolean chunk_dump(Chunk *chunk, EFILE *file, gboolean bigendian, 
		    int dither_mode, StatusBar *bar)
{
     GList *l;
     DataPart *dp;
     off_t clipcount = 0;
     l = chunk->parts;
     while (l) {
	  dp = (DataPart *)l->data;
	  if (datasource_dump(dp->ds,dp->position,dp->length,file,
			      dither_mode,bar,&clipcount))
	       return TRUE;
	  l = l->next;
     }
     return clipwarn(clipcount,TRUE);
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
     off_t *clipcount;
};

static gboolean convert_back_write(WriteoutID id, gpointer data, guint length)
{
     struct convert_back *cbp = (struct convert_back *)id;
     guint i;
     i = length / sizeof(sample_t);
     convert_array(data,&dataformat_sample_t,cbp->buf,cbp->tofmt,i,
		   cbp->dither_mode, cbp->clipcount);
     return tempfile_write(cbp->tmp,cbp->buf,i*cbp->tofmt->samplesize);
}

Chunk *chunk_filter_tofmt(Chunk *chunk, chunk_filter_tofmt_proc proc, 
			  chunk_filter_tofmt_proc eof_proc, 
			  gint amount, gboolean must_convert, 
			  Dataformat *tofmt, int dither_mode,
			  StatusBar *bar, gchar *title)
{
     /* This code has to keep track of five different sample formats:
      *   1) The Chunk's native format (chunk->format)
      *   2) The processing function's input format (informat)
      *   3) The processing function's output format (outformat)
      *   4) The output temporary file's format (outformat or tofmt)
      *   5) The output Chunk's native format (tofmt)
      *
      * These modes are possible:
      *
      *   1) Native format all the way
      *      if must_convert == FALSE and no FP data in chunk
      *      convert = FALSE, convert_back = FALSE,
      *      formats (1)=(2)=(3), (4)=(5)
      *
      *   2) FP processing with FP tempfiles
      *      if must_convert == TRUE || FP data in chunk
      *      convert = TRUE, convert_back = FALSE
      *      formats (2)=(3)=floating point version of (1)
      *      format (4)=floating point version of (5)
      *
      *   3) FP processing with native tempfiles
      *      (this mode is only used if the "use FP tempfiles option" is 
      *      disabled, could cause quality loss)
      *      if (must_convert == TRUE || FP data in chunk) && 
      *          !chunk_filter_use_floating_tempfiles,
      *      convert = TRUE, convert_back = TRUE
      *      formats (2)=(3)=floating point version of (1),  (4)=(5)
      */


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
     off_t clipcount = 0;
     
     /* Force processing in floating-point if any of the Datasources has 
      * faked pcm data */
     convert = must_convert || has_fake_pcm(chunk);
     convert_back = convert && !chunk_filter_use_floating_tempfiles;

     if (convert) {
	  memcpy(&informat,&dataformat_sample_t,sizeof(Dataformat));
	  memcpy(&outformat,&dataformat_sample_t,sizeof(Dataformat));
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
	  cbp->clipcount = &clipcount;
     }
     status_bar_begin_progress(bar,samplesleft,title);

     while (samplesleft > 0) {
	  if (convert)
	       u = chunk_read_array_fp(ch,samplepos,
				       MIN(sizeof(buf)/full_size,samplesleft),
				       (sample_t *)buf,dither_mode,
				       &clipcount) *
		    full_size;
	  else
	       u = chunk_read_array(ch,samplepos,
				    MIN(sizeof(buf),samplesleft*full_size),buf,
				    dither_mode,&clipcount);
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
     if (cbp != NULL) g_free(cbp);

     if (clipwarn(clipcount,TRUE)) {
	  gtk_object_sink(GTK_OBJECT(ds));
	  return NULL;
     }

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
		     StatusBar *bar, gchar *title, off_t samplepos, 
		     gboolean reverse)
{

     guint single_size,full_size,proc_size;
     gchar buf[4096],*c=NULL,*d;
     off_t samplesleft;
     off_t readpos;
     off_t readsamples;
     guint u;
     gint x;
     ChunkHandle *ch;
     off_t clipcount = 0;

     if(reverse) {
          samplesleft = samplepos;
     } else {
          samplesleft = chunk->length - samplepos;
     }

     if (convert) single_size = sizeof(sample_t);
     else single_size = chunk->format.samplesize;
     
     full_size = single_size * chunk->format.channels;

     if (allchannels) proc_size = full_size;
     else proc_size = single_size;

     readsamples = sizeof(buf) / full_size;
     if(samplesleft < readsamples) {
          readsamples = samplesleft;
     }

     ch = chunk_open(chunk);
     if (ch == NULL) { g_free(c); return TRUE; }
     status_bar_begin_progress( bar, chunk->length, title);

     while (samplesleft > 0) {
	  if(reverse) {
	       readpos = samplepos - readsamples;
	  } else {
	       readpos = samplepos;
	  }

	  if (convert) {
	       u = chunk_read_array_fp(ch,readpos,
				       readsamples,
				       (sample_t *)buf,dither_mode,&clipcount);
	       u = u * full_size;
	  } else {
	       u = chunk_read_array(ch,readpos,readsamples * full_size,
				    buf,dither_mode,&clipcount);
	  }

	  g_assert((u / full_size) == readsamples);

	  if (!u) {
	       chunk_close(ch);
	       g_free(c);
	       status_bar_end_progress(bar);
	       return TRUE;
	  }

	  d = buf;

	  if(reverse) {
	       x = u - proc_size;
	  } else {
	       x = 0;
	  }

	  while((x >= 0) && (x < u)) {
	       if (proc(d+x,proc_size,chunk)) {
		    chunk_close(ch);
		    g_free(c);
		    status_bar_end_progress(bar);
		    return TRUE;
	       }
	       if(reverse) {
		    x -= proc_size;
	       } else {
		    x += proc_size;
	       }
	  }

	  samplesleft -= readsamples;
	  if(reverse) {
	       samplepos -= readsamples;
	       if(samplepos < 0) {
		    readsamples = readsamples + samplepos;
		    samplepos = 0;
	       }
	  } else {
	       samplepos += readsamples;
	  }
	  if(samplesleft < readsamples) {
	       readsamples = samplesleft;
	  }
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
     clipwarn(clipcount,FALSE);
     return FALSE;
}

Chunk *chunk_onechannel(Chunk *chunk, int dither_mode, StatusBar *bar)
{
     gboolean *map;
     int i;
     Chunk *r;
     map = g_malloc(chunk->format.channels * 1 * sizeof(gboolean));
     for (i=0; i<chunk->format.channels; i++)
	  map[i] = TRUE;
     r = chunk_remap_channels(chunk,1,map,dither_mode,bar);
     g_free(map);
     return r;
}

Chunk *chunk_copy_channel(Chunk *chunk, gint channel, int dither_mode, 
			  StatusBar *bar)
{
     int i,new_chans;
     int *map;

     new_chans = chunk->format.channels + 1;
     map = g_malloc(new_chans * sizeof(int));
     for (i=0; i<new_chans; i++) {
	  if (i <= channel) map[i] = i;
	  else map[i] = i-1;
     }
     return chunk_ds_remap(chunk,new_chans,map);
}


Chunk *chunk_mix(Chunk *c1, Chunk *c2, int dither_mode, StatusBar *bar)
{
     #define BUFLEN 256
     off_t u,mixlen;
     guint i,chn,x1,x2,x;
     off_t clipcount = 0;
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
     format.bigendian = dataformat_sample_t.bigendian;
     format.samplebytes = format.samplesize * format.channels;

     /* Prepare for processing */
     ch1 = chunk_open(c1);
     if ( ch1 == NULL ) return NULL;
     ch2 = chunk_open(c2);
     if ( ch2 == NULL ) { chunk_close(ch1); return NULL; }
     tmp = tempfile_init(&format,FALSE);
     status_bar_begin_progress(bar, mixlen, _("Mixing"));

     for (u=0; u<mixlen; u+=x) {

	  x1 = chunk_read_array_fp(ch1,u,BUFLEN/chn,buf1,dither_mode,
				   &clipcount);
	  x2 = chunk_read_array_fp(ch2,u,BUFLEN/chn,buf2,dither_mode,
				   &clipcount);
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
	  if (clipcount > 1000000000) clipcount = 1000000000;
	  str = g_strdup_printf(_("The mixed result was clipped %d times."),
				(int)clipcount);
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
#undef BUFLEN
}

static void sandwich_copy(char *dest_buf, int dest_offset, int dest_item_size,
			  char *source_buf, int source_item_size, int items)
{
     int i;
     for (i=0; i<items; i++)
	  memcpy(dest_buf+dest_offset+i*dest_item_size, 
		 source_buf+i*source_item_size,
		 source_item_size);
}


static Chunk *chunk_sandwich_main(Chunk *c1, Chunk *c2, int dither_mode, StatusBar *bar)
{
#define BUFLEN 512
     off_t u,mixlen;
     guint chn1,chn2,x,x1,x2;
     off_t clipcount = 0;
     char *buf1,*buf2,*outbuf;
     Dataformat format;
     Chunk *m;
     ChunkHandle *ch1, *ch2;
     TempFile tmp;

     g_assert(dataformat_samples_equal(&(c1->format),&(c2->format)) && 
	      c1->format.samplerate == c2->format.samplerate);

     /* Number of channels */
     chn1 = c1->format.channels;
     chn2 = c2->format.channels;

     /* Number of multi-channel samples to mix */
     g_assert(c1->length == c2->length);
     mixlen = c1->length;

     memcpy(&format,&(c1->format),sizeof(Dataformat));
     format.channels = chn1+chn2;
     format.samplebytes = format.samplesize * format.channels;

     /* Prepare for processing */
     ch1 = chunk_open(c1);
     if ( ch1 == NULL ) return NULL;
     ch2 = chunk_open(c2);
     if ( ch2 == NULL ) { chunk_close(ch1); return NULL; }
     tmp = tempfile_init(&format,FALSE);
     status_bar_begin_progress(bar, mixlen, _("Combining channels"));

     buf1 = g_malloc(BUFLEN*c1->format.samplebytes);
     buf2 = g_malloc(BUFLEN*c2->format.samplebytes);
     outbuf = g_malloc(BUFLEN*format.samplebytes);

     for (u=0; u<mixlen; u+=x) {

	  x = BUFLEN;
	  if ((off_t)x > (mixlen-u))
	       x = ((guint)(mixlen-u));

	  x1 = chunk_read_array(ch1,u,x*c1->format.samplebytes,buf1,
				dither_mode,&clipcount);
	  x2 = chunk_read_array(ch2,u,x*c2->format.samplebytes,
				buf2,dither_mode,&clipcount);
	  
	  if (x1 == 0 || x2 == 0) {
	       tempfile_abort(tmp);
	       chunk_close(ch1);
	       chunk_close(ch2);
	       status_bar_end_progress(bar);
	       g_free(buf1);
	       g_free(buf2);
	       g_free(outbuf);
	       return NULL;
	  }

	  g_assert(x1 == x*c1->format.samplebytes &&
		   x2 == x*c2->format.samplebytes);
	  
	  /* Combine the data from buf1,buf2 into outbuf */
	  sandwich_copy(outbuf,0,format.samplebytes,
			buf1,c1->format.samplebytes, x);
	  sandwich_copy(outbuf,c1->format.samplebytes,format.samplebytes,
			buf2,c2->format.samplebytes, x);

	  /* Write outbuf */
	  if (tempfile_write(tmp,outbuf,x*format.samplebytes)
	      || status_bar_progress(bar,x)) {
	       tempfile_abort(tmp);
	       chunk_close(ch1);
	       chunk_close(ch2);
	       status_bar_end_progress(bar);
	       g_free(buf1);
	       g_free(buf2);
	       g_free(outbuf);
	       return NULL;
	  }
     }    

     /* Finish processing */
     chunk_close(ch1);
     chunk_close(ch2);
     m = tempfile_finished(tmp);
     status_bar_end_progress(bar);
     g_free(buf1);
     g_free(buf2);
     g_free(outbuf);
     if (!m) return NULL;

     /* Warn if clipping occured */
     clipwarn(clipcount,FALSE);

     return m;
#undef BUFLEN
}

Chunk *chunk_sandwich(Chunk *c1, Chunk *c2, 
		      off_t c1_align_point, off_t c2_align_point,
		      off_t *align_point_out, int dither_mode, StatusBar *bar)
{
     Chunk *before, *mainpart_c1, *mainpart_c2, *mainpart_sw, *after;
     Chunk *c,*d,*c1_remap,*c2_remap;
     int *map1,*map2,outchans,i;
     int mp_c1_maxlen,mp_c2_maxlen,mp_c1_offset,mp_c2_offset,mp_len;
     off_t offset;

     offset = c2_align_point-c1_align_point;     
     outchans = c1->format.channels + c2->format.channels;

     map1 = g_malloc(outchans * sizeof(map1[0]));
     map2 = g_malloc(outchans * sizeof(map2[0]));
     for (i=0; i<c1->format.channels; i++) { map1[i]=i; map2[i]=-1; }
     for (; i<outchans; i++) { map1[i]=-1; map2[i]=i-(c1->format.channels); }

     c1_remap = chunk_ds_remap(c1,outchans,map1);
     c2_remap = chunk_ds_remap(c2,outchans,map2);

     /* g_free(map1); g_free(map2); Done already by chunk_ds_remap */

     if (offset == 0) {
	  before = NULL;
	  mp_c1_maxlen = c1->length;
	  mp_c1_offset = 0;
	  mp_c2_maxlen = c2->length;
	  mp_c2_offset = 0;
	  *align_point_out = c1_align_point;
     } else if (offset > 0) {
	  before = chunk_get_part(c2_remap, 0, offset);
	  mp_c1_maxlen = c1->length;
	  mp_c1_offset = 0;
	  mp_c2_maxlen = c2->length - offset;
	  mp_c2_offset = offset;
	  *align_point_out = c2_align_point;
     } else {
	  before = chunk_get_part(c1_remap, 0, -offset);
	  mp_c1_maxlen = c1->length+offset;
	  mp_c1_offset = -offset;
	  mp_c2_maxlen = c2->length;
	  mp_c2_offset = 0;
	  *align_point_out = c1_align_point;
     }

     mp_len = MIN(mp_c1_maxlen, mp_c2_maxlen);
     if (mp_len > 0) {
	  mainpart_c1 = chunk_get_part(c1, mp_c1_offset, mp_len);
	  mainpart_c2 = chunk_get_part(c2, mp_c2_offset, mp_len);
     } else 
	  mainpart_c1 = mainpart_c2 = NULL;

     if (mp_len < mp_c1_maxlen) {	  
	  after = chunk_get_part(c1_remap, mp_c1_offset+mp_len, 
				 mp_c1_maxlen-mp_len);
     } else if (mp_len < mp_c2_maxlen) {
	  after = chunk_get_part(c2_remap, mp_c2_offset+mp_len,
				 mp_c2_maxlen-mp_len);
     } else {
	  after = NULL;
     }

     /* Floating refs: c1_remap,c2_remap,before,after,mainpart_c1,mainpart_c2 */
     gtk_object_sink(GTK_OBJECT(c1_remap));
     gtk_object_sink(GTK_OBJECT(c2_remap));

     if (mp_len > 0) {
	  mainpart_sw = chunk_sandwich_main(mainpart_c1, mainpart_c2, 
					    dither_mode, bar);
	  gtk_object_sink(GTK_OBJECT(mainpart_c1));
	  gtk_object_sink(GTK_OBJECT(mainpart_c2));
	  if (mainpart_sw == NULL) {
	       if (before != NULL) gtk_object_sink(GTK_OBJECT(before));
	       if (after != NULL) gtk_object_sink(GTK_OBJECT(after));
	       return NULL;
	  }
     } else 
	  mainpart_sw = NULL;
     
     /* Floating refs: before,after,mainpart_sw */

     c = before;
     if (mainpart_sw != NULL) {
	  if (c != NULL) {
	       d = chunk_append(c,mainpart_sw);
	       gtk_object_sink(GTK_OBJECT(c));
	       gtk_object_sink(GTK_OBJECT(mainpart_sw));
	       c = d;
	  } else
	       c = mainpart_sw;	  
     }
     if (after != NULL) {
	  if (c != NULL) {
	       d = chunk_append(c,after);
	       gtk_object_sink(GTK_OBJECT(c));
	       gtk_object_sink(GTK_OBJECT(after));
	       c = d;
	  } else
	       c = after;
     }
     return c;
}


static struct {
     sample_t *buf;
     int bufsize;
     /* (outchannels x (inchannels+1))-matrix */
     /* Each row has a list of input channel numbers, ending with -1 */
     int *map; 
} chunk_remap_channels_data;

static gboolean chunk_remap_channels_mixmode_proc(void *in, 
						  guint sample_size,
						  chunk_writeout_func out_func,
						  WriteoutID id,
						  Dataformat *informat,
						  Dataformat *outformat)
{
     int samps,i,j,k,l;
     sample_t *iptr, *optr, s;
     samps = sample_size / informat->samplebytes;

     /* Make sure we have room in the buffer */
     if (samps * outformat->samplebytes > chunk_remap_channels_data.bufsize) {
	  g_free(chunk_remap_channels_data.buf);
	  chunk_remap_channels_data.buf = 
	       g_malloc(samps*outformat->samplebytes);
	  chunk_remap_channels_data.bufsize = samps*outformat->samplebytes;
     }

     /* Process data */
     iptr = (sample_t *)in;
     optr = chunk_remap_channels_data.buf;

     for (i=0; i<samps; i++) {
	  for (j=0; j<outformat->channels; j++) {
	       s = 0.0;
	       for (k=(j*(informat->channels+1)); 1; k++) {
		    l = chunk_remap_channels_data.map[k];
		    if (l < 0) break;
		    s += iptr[l];
	       }
	       optr[j] = s;
	  }
	  iptr += informat->channels;
	  optr += outformat->channels;
     }

     return out_func(id, chunk_remap_channels_data.buf, 
		     samps*outformat->samplebytes);     
}
						  
Chunk *chunk_remap_channels(Chunk *chunk, int channels_out, gboolean *map, 
			    int dither_mode, StatusBar *bar)
{
     gboolean mixmode;
     int i,j,k;
     int channels_in = chunk->format.channels;
     Dataformat tofmt;
     Chunk *r;
     int *bigmap,*smallmap;
     
     memcpy(&tofmt,&(chunk->format),sizeof(Dataformat));
     tofmt.channels = channels_out;
     tofmt.samplebytes = tofmt.channels * tofmt.samplesize;

     chunk_remap_channels_data.buf = NULL;
     chunk_remap_channels_data.bufsize = 0;

     /* Generate the maps */
     mixmode = FALSE;
     bigmap = g_malloc((channels_in+1)*channels_out*sizeof(int));
     smallmap = g_malloc(channels_out*sizeof(int));

     for (i=0; i<channels_out; i++) {
	  smallmap[i] = -1;
	  k = 0;
	  for (j=0; j<channels_in; j++) 
	       if (map[j*channels_out + i]) {
		    bigmap[i*(channels_in+1)+(k++)] = j;
		    smallmap[i] = j;
	       }
	  bigmap[i*(channels_in+1) + k] = -1;
	  if (k > 1) mixmode = TRUE;
     }

     if (mixmode) {
	  chunk_remap_channels_data.map = bigmap;
	  g_free(smallmap);
	  r = chunk_filter_tofmt(chunk,chunk_remap_channels_mixmode_proc,
				 NULL,CHUNK_FILTER_MANY,TRUE,&tofmt,
				 dither_mode,bar,_("Mixing channels"));
	  g_free(chunk_remap_channels_data.buf);
	  g_free(chunk_remap_channels_data.map);
     } else {
	  g_free(bigmap);
	  r = chunk_ds_remap(chunk,channels_out,smallmap);
     }
     
     return r;
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
		       guint size, void *buffer, int dither_mode, 
		       off_t *clipcount)
{
     GList *l;     
     DataPart *dp;
     guint r=0,i,j;
     guint sb = handle->format.samplebytes;
     off_t o;

     /* Loop until we have collected enough data or reached EOF
      * The condition size >= sb handles the case size is not an even
      * multiple of the number of sample bytes (24-bit data usually) */
     
     for (l=handle->parts; l!=NULL && size>=sb; l=l->next) {
	  dp = (DataPart *)l->data;
	  if (dp->length > sampleno) {
	       /* Calculate required size if it's larger than we need. */
	       o = dp->length - sampleno;	       
	       o *= (off_t)sb;
	       if (o < (off_t)size) j = (guint)o;
	       else j = size;
	       /* Read data. */
	       i = datasource_read_array(dp->ds,
					 dp->position + sampleno,
					 j,buffer,dither_mode,clipcount);
	       if (i == 0) return 0;
	       r+=i;
	       sampleno = 0;
	       size -= i;
	       buffer = ((gchar *)buffer) + i;
	  } else
	       sampleno -= dp->length;
     }
     return r;
}

guint chunk_read_array_fp(ChunkHandle *handle, off_t sampleno, 
			  guint samples, sample_t *buffer, int dither_mode,
			  off_t *clipcount)
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
					    j,buffer,dither_mode,clipcount);
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
     g_assert(dataformat_equal(&(chunk->format),&(new->format)));
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
	  if (s == 1.0 && chunk->format.type == DATAFORMAT_PCM) return TRUE;
     }
     return FALSE;
}

sample_t chunk_peak_level(Chunk *c, StatusBar *bar)
{
     chunk_peak_level_max = 0.0;
     if (chunk_parse(c,chunk_peak_level_proc,FALSE,TRUE,DITHER_NONE,bar,
		     _("Calculating peak level"),0,FALSE)) return -1.0;
     return chunk_peak_level_max;
}

/* BEGIN zero-crossing search added by Forest Bond */

static off_t chunk_zero_crossing_seen_samples;
static sample_t *chunk_zero_crossing_saved_sample = NULL;

static gboolean chunk_signal_crossed_zero(sample_t prev, sample_t current)
{
     if((current >= 0.0) && (prev <= 0.0)) {
          return TRUE;
     } else if((current <= 0.0) && (prev >= 0.0)) {
          return TRUE;
     }
     return FALSE;
}

static void chunk_zero_crossing_init(gint sample_size) {
     if(chunk_zero_crossing_saved_sample != NULL) {
          g_free(chunk_zero_crossing_saved_sample);
          chunk_zero_crossing_saved_sample = NULL;
     }
     chunk_zero_crossing_saved_sample = g_malloc(sample_size);
}

static void chunk_zero_crossing_cleanup() {
     if(chunk_zero_crossing_saved_sample != NULL) {
          g_free(chunk_zero_crossing_saved_sample);
          chunk_zero_crossing_saved_sample = NULL;
     }
}

static void chunk_zero_crossing_save_sample(sample_t *sample,
  gint sample_size) {
     memcpy(chunk_zero_crossing_saved_sample, sample, sample_size);
}

static gboolean chunk_zero_crossing_any_proc(void *sample, gint sample_size, 
				      Chunk *chunk)
{
     sample_t *start = (sample_t *)sample;
     sample_t *end = start + (sample_size/sizeof(sample_t));
     sample_t *current = start;
     sample_t *prev = chunk_zero_crossing_saved_sample;

     if(chunk_zero_crossing_seen_samples < 1) {
          chunk_zero_crossing_init(sample_size);
          chunk_zero_crossing_save_sample(start, sample_size);
          chunk_zero_crossing_seen_samples++;
          return FALSE;
     }

     while(current < end) {
          if(chunk_signal_crossed_zero(*prev, *current)) {
               return TRUE;
          }
          current++;
          prev++;
     }

     chunk_zero_crossing_save_sample(start, sample_size);
     chunk_zero_crossing_seen_samples++;
     return FALSE;
}

off_t chunk_zero_crossing_any_forward(Chunk *c, StatusBar *bar,off_t samplepos)
{
     chunk_zero_crossing_seen_samples = 0;
     chunk_parse(c,chunk_zero_crossing_any_proc,TRUE,TRUE,DITHER_NONE,bar,
		     _("Finding zero-crossing"), samplepos, FALSE);

     chunk_zero_crossing_cleanup();
     return samplepos + chunk_zero_crossing_seen_samples;
}

off_t chunk_zero_crossing_any_reverse(Chunk *c, StatusBar *bar,off_t samplepos)
{
     chunk_zero_crossing_seen_samples = 0;
     chunk_parse(c,chunk_zero_crossing_any_proc,TRUE,TRUE,DITHER_NONE,bar,
		     _("Finding zero-crossing"), samplepos, TRUE);

     chunk_zero_crossing_cleanup();
     return samplepos - chunk_zero_crossing_seen_samples;
}

static gboolean chunk_zero_crossing_all_proc(void *sample, gint sample_size, 
				      Chunk *chunk)
{
     sample_t *start = (sample_t *)sample;
     sample_t *end = start + (sample_size/sizeof(sample_t));
     sample_t *current = start;
     sample_t *prev = chunk_zero_crossing_saved_sample;

     if(chunk_zero_crossing_seen_samples < 1) {
          chunk_zero_crossing_init(sample_size);
          chunk_zero_crossing_save_sample(start, sample_size);
          chunk_zero_crossing_seen_samples++;
          return FALSE;
     }

     while(current < end) {

          if(! chunk_signal_crossed_zero(*prev, *current)) {
               chunk_zero_crossing_save_sample(start, sample_size);
               chunk_zero_crossing_seen_samples++;
               return FALSE;
          }
          current++;
          prev++;
     }
     return TRUE;
}

off_t chunk_zero_crossing_all_forward(Chunk *c, StatusBar *bar,off_t samplepos)
{
     chunk_zero_crossing_seen_samples = 0;
     chunk_parse(c,chunk_zero_crossing_all_proc,TRUE,TRUE,DITHER_NONE,bar,
		     _("Finding zero-crossing"), samplepos, FALSE);

     chunk_zero_crossing_cleanup();
     return samplepos + chunk_zero_crossing_seen_samples;
}

off_t chunk_zero_crossing_all_reverse(Chunk *c, StatusBar *bar,off_t samplepos)
{
     chunk_zero_crossing_seen_samples = 0;
     chunk_parse(c,chunk_zero_crossing_all_proc,TRUE,TRUE,DITHER_NONE,bar,
		     _("Finding zero-crossing"), samplepos, TRUE);

     chunk_zero_crossing_cleanup();

     return samplepos - chunk_zero_crossing_seen_samples;
}

/* END zero-crossing search */

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
     gchar *s;
     Chunk *r;

     s = g_strdup_printf(_("Amplifying (by %3.1f%% / %+.1fdB)"),factor*100.0,
			 20*log10(factor));
     chunk_amplify_factor = factor;
     r = chunk_filter(c,chunk_amplify_proc,NULL,CHUNK_FILTER_MANY,TRUE,
		      dither_mode,bar,s);
     g_free(s);
     return r;
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

Chunk *chunk_new_with_ramp(Dataformat *format, off_t length, 
			   sample_t *startvals, sample_t *endvals, 
			   int dither_mode, StatusBar *bar)
{
#define BUF_LENGTH 512
     int i,k;
     sample_t diffval[8], *sbuf;
     sample_t slength = (sample_t)length;
     TempFile tf;
     off_t ctr = 0;
     int channels = format->channels;
     int bufctr;
     Dataformat fmt;
     Chunk *r,*q;

     g_assert(channels <= 8);

     for (i=0; i<channels; i++)
	  diffval[i] = endvals[i] - startvals[i];
     
     memcpy(&fmt,&dataformat_sample_t,sizeof(Dataformat));
     fmt.channels = channels;
     fmt.samplerate = format->samplerate;
     fmt.samplebytes = fmt.channels * fmt.samplesize;
     tf = tempfile_init(&fmt,FALSE);
     if (tf == NULL) return NULL;

     status_bar_begin_progress(bar,length,NULL);
     sbuf = g_malloc(BUF_LENGTH*sizeof(sample_t));

     /* Output data */
     while (ctr < length) {
	  bufctr = BUF_LENGTH / channels;
	  if ((length-ctr) < (off_t)bufctr) bufctr = (guint)(length-ctr);
	  for (i=0; i<bufctr*channels; i+=channels,ctr++)
	       for (k=0; k<channels; k++)
		    sbuf[i+k] = startvals[k] + 
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
     if (r!=NULL && !dataformat_equal(&(r->format),format)) {
       q = chunk_convert_sampletype(r,format);
       gtk_object_sink(GTK_OBJECT(r));
       return q;
     } else 
       return r;
     
#undef BUF_LENGTH
}

Chunk *chunk_interpolate_endpoints(Chunk *chunk, gboolean falldown_mode, 
				   int dither_mode, StatusBar *bar)
{
     int i;
     Chunk *start,*mid,*end,*c,*d;
     ChunkHandle *ch;     

     sample_t startval[8],endval[8],zeroval[8];
     off_t length = chunk->length;

     /* get the endpoint values */
     ch = chunk_open(chunk);
     if (ch == NULL) return NULL;
     if (chunk_read_fp(ch,0,startval,dither_mode) || 
	 chunk_read_fp(ch,length-1,endval,dither_mode)) { 
	  chunk_close(ch);
	  return NULL;
     }
     chunk_close(ch);
     memset(zeroval,0,sizeof(zeroval));

     /* Length of 0.1 s in samples */
     i = chunk->format.samplerate / 10;
     if (i < 1) i=1;

     if (!falldown_mode || chunk->length <= 2*i) {
	  return chunk_new_with_ramp(&(chunk->format),chunk->length,startval,
				     endval,dither_mode,bar);
     } else {
	  start = chunk_new_with_ramp(&(chunk->format),i,startval,zeroval,
				      dither_mode,bar);
	  if (start == NULL) return NULL;
	  end = chunk_new_with_ramp(&(chunk->format),i,zeroval,endval,
				    dither_mode,bar);
	  if (end == NULL) {
	       gtk_object_sink(GTK_OBJECT(start));
	       return NULL;
	  }
	  mid = chunk_new_empty(&(chunk->format),chunk->length-2*i);
	  g_assert(mid != NULL);
	  
	  c = chunk_append(start,mid);
	  d = chunk_append(c,end);

	  gtk_object_sink(GTK_OBJECT(start));
	  gtk_object_sink(GTK_OBJECT(mid));
	  gtk_object_sink(GTK_OBJECT(end));
	  gtk_object_sink(GTK_OBJECT(c));
	  return d;
	  
     }
}

Chunk *chunk_byteswap(Chunk *chunk)
{
     Chunk *c,*d;
     Dataformat fmt;
     memcpy(&fmt,&(chunk->format),sizeof(Dataformat));
     fmt.bigendian = !fmt.bigendian;
     c = chunk_clone_df(chunk,&fmt);
     d = chunk_convert(c,&(chunk->format),DITHER_UNSPEC,NULL);
     gtk_object_sink(GTK_OBJECT(c)); 
     return d;
}

Chunk *chunk_convert_channels(Chunk *chunk, guint new_channels)
{
     int *map;
     int i;
     g_assert(chunk->format.channels != new_channels);
     map = g_malloc(new_channels * sizeof(int));
     for (i=0; i<new_channels; i++) map[i] = i;
     /* When converting mono files, put mono channel in both left and right. 
      * This is in most cases what we want. */
     if (chunk->format.channels==1 && new_channels > 1)
	  map[1] = 0;
     return chunk_ds_remap(chunk,new_channels,map);
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
	       d = chunk_convert_channels(chunk,new_format->channels);
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
	       c = chunk_convert_channels(d,new_format->channels);
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
	  c = chunk_convert_channels(chunk,new_format->channels);
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

gboolean clipwarn(off_t clipcount, gboolean maycancel)
{
     gboolean b = FALSE;
     gchar *c;
     if (clipcount > 0) {
	  if (clipcount > 1000000000)
	       clipcount = 1000000000;
	  c = g_strdup_printf(_("The input was clipped %d times during "
				"processing."),(int)clipcount);
	  if (maycancel)
	       b = (user_message(c,UM_OKCANCEL) != MR_OK);
	  else
	       user_warning(c);
	  
	  g_free(c);
     }

     return b;
}
