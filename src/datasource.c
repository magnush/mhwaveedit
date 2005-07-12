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

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "gtkfiles.h"
#include "ringbuf.h"
#include "um.h"
#include "main.h"
#include "inifile.h"

#include "datasource.h"
#include "tempfile.h"
#include "gettext.h"

static GtkObjectClass *parent_class;
static GList *datasource_list = NULL;


guint datasource_count(void)
{
     return g_list_length(datasource_list);
}

static void datasource_init(Datasource *obj)
{
     /* printf("datasource_init(%p)\n",obj); */
     datasource_list = g_list_append(datasource_list, obj);
     obj->type = DATASOURCE_SILENCE;
     obj->format.type = DATAFORMAT_PCM;
     obj->format.samplerate=44100;
     obj->format.samplesize=2;
     obj->format.channels=2;
     obj->format.sign=1;
     obj->format.bigendian = IS_BIGENDIAN;
     obj->format.samplebytes=4;
     obj->length = 0;
     obj->bytes = 0;
     obj->opencount = 0;
     obj->tag = 0;
}

static void datasource_clear(Datasource *ds)
{
     g_assert(ds->opencount == 0);

     switch (ds->type) {

     case DATASOURCE_TEMPFILE:
	  if (ds->data.virtual.filename)
	       xunlink(ds->data.virtual.filename);	  
	  /* Fall through */

     case DATASOURCE_VIRTUAL:
	  if (ds->data.virtual.filename) {
	       g_free(ds->data.virtual.filename);
	       ds->data.virtual.filename = NULL;
	  }
	  break;
	  
     case DATASOURCE_REAL:
	  if (ds->data.real) {
	       g_free(ds->data.real);
	       ds->data.real = NULL;
	  }
	  break;

     case DATASOURCE_SNDFILE_TEMPORARY:
	  if (ds->data.sndfile.filename)
	       xunlink(ds->data.sndfile.filename);
	  /* Fall through */

     case DATASOURCE_SNDFILE:
	  if (ds->data.sndfile.filename) {
	       g_free(ds->data.sndfile.filename);
	       ds->data.sndfile.filename = NULL;
	  }
	  break;
	  
     case DATASOURCE_REF:
     case DATASOURCE_CLONE:
     case DATASOURCE_BYTESWAP:
     case DATASOURCE_CONVERT:
	  if (ds->data.clone) 
	       gtk_object_unref(GTK_OBJECT(ds->data.clone));
	  ds->data.clone = NULL;
     }
     
     ds->type = DATASOURCE_SILENCE;
}

static void datasource_destroy(GtkObject *obj)
{     
     Datasource *ds = DATASOURCE(obj);
     /* printf("datasource_destroy(%p)\n",obj); */
     datasource_clear(ds);
     datasource_list = g_list_remove(datasource_list, obj);
     parent_class->destroy(obj);
}

static void datasource_class_init(GtkObjectClass *klass)
{
     parent_class = gtk_type_class(gtk_object_get_type());
     klass->destroy = datasource_destroy;
}

GtkType datasource_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
		GtkTypeInfo info = {
		     "Datasource",
		     sizeof(Datasource),
		     sizeof(DatasourceClass),
		     (GtkClassInitFunc) datasource_class_init,
		     (GtkObjectInitFunc) datasource_init 
		};
		id=gtk_type_unique(gtk_object_get_type(),&info);	  
     }
     return id;
}

/* Creates a copy of the original datasource
 * If it refers to a file, create a DATASOURCE_REF instead.  
 */

static Datasource *datasource_copy(Datasource *orig)
{
     Datasource *ds;

     ds = gtk_type_new(datasource_get_type());
     ds->type = orig->type;
     memcpy(&(ds->format),&(orig->format),sizeof(Dataformat));
     ds->length = orig->length;
     ds->bytes = orig->bytes;

     switch (orig->type) {
     case DATASOURCE_VIRTUAL:
     case DATASOURCE_TEMPFILE:
     case DATASOURCE_SNDFILE:
     case DATASOURCE_SNDFILE_TEMPORARY:
	  ds->type = DATASOURCE_REF;	  
	  ds->data.clone = orig;
	  gtk_object_ref(GTK_OBJECT(orig));
	  gtk_object_sink(GTK_OBJECT(orig));
	  break;
     case DATASOURCE_REAL:
	  ds->data.real = g_malloc(orig->bytes);
	  memcpy(ds->data.real, orig->data.real, orig->bytes);
	  break;
     case DATASOURCE_CLONE:
     case DATASOURCE_BYTESWAP:
     case DATASOURCE_REF:
     case DATASOURCE_CONVERT:
	  ds->data.clone = orig->data.clone;
	  gtk_object_ref(GTK_OBJECT(ds->data.clone));
	  gtk_object_sink(GTK_OBJECT(ds->data.clone));
     }
     return ds;
}

Datasource *datasource_clone_df(Datasource *source, Dataformat *format)
{
     Datasource *df;

     g_assert(source->format.type == DATAFORMAT_PCM);

     if (source->type == DATASOURCE_REAL ||
	 source->type == DATASOURCE_SILENCE ||
	 source->type == DATASOURCE_CLONE) {
	  df = datasource_copy(source);
	  g_assert(df->type == source->type);
	  memcpy(&(df->format),format,sizeof(Dataformat));
	  df->length = df->bytes / format->samplebytes;
	  return df;
     }
 
     df = gtk_type_new(datasource_get_type());
     df->type = DATASOURCE_CLONE;
     memcpy(&(df->format),format,sizeof(Dataformat));
     df->bytes = source->bytes;
     df->length = source->bytes / format->samplebytes;
     df->data.clone = source;
     gtk_object_ref(GTK_OBJECT(source));
     return df;
}

Datasource *datasource_byteswap(Datasource *source)
{
     Datasource *ds;
     if (source == NULL) return NULL;
     ds = gtk_type_new(datasource_get_type());
     ds->type = DATASOURCE_BYTESWAP;
     memcpy(&(ds->format),&(source->format),sizeof(Dataformat));
     ds->format.bigendian = !source->format.bigendian;
     ds->length = source->length;
     ds->bytes = source->bytes;
     ds->data.clone = source;
     gtk_object_ref(GTK_OBJECT(source));
     gtk_object_sink(GTK_OBJECT(source));
     return ds;
}

Datasource *datasource_new_silent(Dataformat *format, off_t samples)
{
     Datasource *ds;
     ds = gtk_type_new(datasource_get_type());
     memcpy(&(ds->format), format, sizeof(Dataformat));
     ds->type = DATASOURCE_SILENCE;
     ds->length = samples;
     ds->bytes = samples * ds->format.samplebytes;
     return ds;
}

Datasource *datasource_new_from_data(void *data, Dataformat *format, 
				     guint32 size)
{
     Datasource *ds;
     ds = (Datasource *)gtk_type_new(datasource_get_type());
     ds->type = DATASOURCE_REAL;
     memcpy(&(ds->format),format,sizeof(Dataformat));
     ds->length = size / format->samplebytes;
     ds->bytes = size;
     ds->data.real = data;
     return ds;
}

#define DUMP_BUFSIZE 65536

gboolean datasource_dump(Datasource *ds, off_t position, 
			 off_t length, EFILE *file, int dither_mode, 
			 StatusBar *bar)
{
     gchar *buf;
     off_t i;
     guint u;
     if (datasource_open(ds)) return TRUE;
     buf = g_malloc(DUMP_BUFSIZE);
     while (length > 0) {
	  i = MIN(length*ds->format.samplebytes,DUMP_BUFSIZE);
	  u = datasource_read_array(ds,position,i,buf,dither_mode);
	  if (u==0 || e_fwrite(buf,u,file) || status_bar_progress(bar,u)) { 
	       datasource_close(ds); 
	       g_free(buf);
	       return TRUE; 
	  }
	  length -= u/ds->format.samplebytes;
	  position += u/ds->format.samplebytes;
     }
     datasource_close(ds);
     g_free(buf);
     return FALSE;
}

gboolean datasource_realize(Datasource *ds, int dither_mode)
{
     gchar *c;
     guint32 sz=ds->bytes;

     if (datasource_open(ds)) return TRUE;

     c = g_malloc(sz);
     if (datasource_read_array(ds,0,sz,c, dither_mode)) {
	  g_free(c);
	  datasource_close(ds);
	  return TRUE;
     }
     datasource_close(ds);
     
     datasource_clear(ds);
     ds->type = DATASOURCE_REAL;
     ds->data.real = c;     

     return FALSE;
}

static gboolean datasource_open_sndfile(Datasource *ds)
{
#if defined(HAVE_LIBSNDFILE)
     SF_INFO i;
     SNDFILE *s;
     char *d;
     int fd;
     fd = xopen(ds->data.sndfile.filename,O_RDONLY,0);
     if (fd == -1) return TRUE;
     s = sf_open_fd(fd,SFM_READ,&i,TRUE);
     if (s == NULL) {	  
	  d = g_strdup_printf(_("Couldn't open %s"),
			      ds->data.sndfile.filename);
	  user_error(d);
	  g_free(d);
	  close(fd);
	  return TRUE;
     }

     ds->data.sndfile.handle = s;
     ds->data.sndfile.pos = 0;

     sf_command ( s, SFC_SET_NORM_FLOAT, NULL, SF_TRUE );
     sf_command ( s, SFC_SET_NORM_DOUBLE, NULL, SF_TRUE );
     return FALSE;
#else
     g_assert_not_reached();
     return TRUE;
#endif     
}

static void datasource_close_sndfile(Datasource *ds)
{
#if defined(HAVE_LIBSNDFILE)
     sf_close(ds->data.sndfile.handle);
#else
     g_assert_not_reached();
#endif
}

gboolean datasource_open(Datasource *ds)
{
     if (ds->opencount == 0) 
	  switch (ds->type) {
	  case DATASOURCE_VIRTUAL:
	  case DATASOURCE_TEMPFILE:
	       ds->data.virtual.handle = e_fopen(ds->data.virtual.filename,
						 EFILE_READ);
	       if (ds->data.virtual.handle == NULL) return TRUE;
	       ds->data.virtual.pos = 0;
	       break;
	  case DATASOURCE_SNDFILE:
	  case DATASOURCE_SNDFILE_TEMPORARY:
	       if (datasource_open_sndfile(ds)) return TRUE;
	       break;
	  case DATASOURCE_REF:
	  case DATASOURCE_CLONE:
	  case DATASOURCE_BYTESWAP:	       
	  case DATASOURCE_CONVERT:
	       if (datasource_open(ds->data.clone)) return TRUE;
	  }     
     ds->opencount ++;
     return FALSE;
}

void datasource_close(Datasource *ds)
{
     g_assert(ds->opencount != 0);
     ds->opencount --;
     if (ds->opencount > 0) return;
     switch (ds->type) {
     case DATASOURCE_VIRTUAL:
     case DATASOURCE_TEMPFILE:
	  e_fclose(ds->data.virtual.handle);
	  break;
     case DATASOURCE_SNDFILE:
     case DATASOURCE_SNDFILE_TEMPORARY:
	  datasource_close_sndfile(ds);
	  break;
     case DATASOURCE_REF:
     case DATASOURCE_CLONE:
     case DATASOURCE_BYTESWAP:
     case DATASOURCE_CONVERT:
	  datasource_close(ds->data.clone);
	  break;
     }     
}

static guint datasource_clone_read_array(Datasource *source, off_t sampleno,
					 guint size, gpointer buffer, 
					 int dither_mode)
{
     /* This is not optimized but it's only used in rare cases so the important
      * thing is that it works.*/
     off_t orig_offset;
     off_t orig_sampleno;
     off_t orig_size;
     off_t orig_adjust;
     guint x;
     gchar *p;
     /* Offset to be sent to the original chunk */
     orig_offset = sampleno * source->format.samplebytes;
     /* orig_offset converted to sample number */
     orig_sampleno = orig_offset / source->data.clone->format.samplebytes;
     /* How bany bytes too early will we read? */
     orig_adjust = orig_offset % source->data.clone->format.samplebytes;
     /* How much data should we read (should be able to fill buffer) */
     orig_size = size + orig_adjust + source->data.clone->format.samplebytes - 1;
     p = g_malloc(orig_size);
     x = datasource_read_array(source->data.clone, orig_sampleno, orig_size, p,
			       dither_mode);
     if (x != 0) {
	  g_assert(x-orig_adjust >= size);
	  memcpy(buffer, p+orig_adjust, size);
	  x = size;
     }
     g_free(p);
     return x;			       
}

#if defined(HAVE_LIBSNDFILE)
static void sndfile_read_error(sf_count_t x, Datasource *source)
{
     gchar c[256],*d;
     if (x<0) sf_error_str(source->data.sndfile.handle,c,sizeof(c));
     else strcpy(c,_("Unexpected end of file"));
     d = g_strdup_printf(_("Error reading %s: %s"),
			 source->data.sndfile.filename, c);
     user_error(d);
     g_free(d);
}
#endif

static guint datasource_sndfile_read_array_pcm(Datasource *source, 
					       off_t sampleno, guint size,
					       gpointer buffer)
{
#if defined(HAVE_LIBSNDFILE)
     gchar c[256],*d;
     sf_count_t x;
     g_assert(source->format.type == DATAFORMAT_PCM);
     if (source->data.sndfile.pos != sampleno) {
	  if (sf_seek(source->data.sndfile.handle,sampleno,SEEK_SET) == -1) {
	       sf_error_str(source->data.sndfile.handle,c,sizeof(c));
	       d = g_strdup_printf(_("Error seeking in %s: %s"),
				   source->data.sndfile.filename, c);
	       user_error(d);
	       g_free(d);
	       return 0;
	  }
	  source->data.sndfile.pos = sampleno;
     }
     if (source->data.sndfile.raw_readable) {
	  /* Read raw */
	  x = sf_read_raw(source->data.sndfile.handle,buffer,size);	  
	  if (x>0) source->data.sndfile.pos += x/source->format.samplebytes;
	  if (x<size) {
	       sndfile_read_error(x,source);
	       return 0;
	  }
	  return x;
     } else if (source->format.samplesize==2 && source->format.sign) {
	  /* Read as short */
	  x = sf_readf_short(source->data.sndfile.handle,buffer,
			     size / source->format.samplebytes);
	  if (x>0) source->data.sndfile.pos += x;
	  if (x < size / source->format.samplebytes) {
	       sndfile_read_error(x,source);
	       return 0;
	  }
	  return x * source->format.samplebytes;
     } else {	  
	  g_assert_not_reached();
	  return 0;
     } 
#else
     g_assert_not_reached();
     return 0;
#endif
}

static guint datasource_sndfile_read_array_fp(Datasource *source, 
					      off_t sampleno, guint samples,
					      sample_t *buffer)
{
#if defined(HAVE_LIBSNDFILE)
     gchar c[256],*d;
     sf_count_t x;

     if (source->data.sndfile.pos != sampleno) {
	  if (sf_seek(source->data.sndfile.handle,sampleno,SEEK_SET) == -1) {
	       sf_error_str(source->data.sndfile.handle,c,sizeof(c));
	       d = g_strdup_printf(_("Error seeking in %s: %s"),
				   source->data.sndfile.filename, c);
	       user_error(d);
	       g_free(d);
	       return 0;
	  }
	  source->data.sndfile.pos = sampleno;
     }

     x = sf_readf_sample_t(source->data.sndfile.handle,buffer,samples);
     if (x>0) source->data.sndfile.pos += x;
     if (x<samples) {
	  sndfile_read_error(x,source);
	  return 0;		    
     }
     return x;

#else
     g_assert_not_reached();
     return 0;
#endif
}

static guint datasource_read_array_main(Datasource *source, 
					off_t sampleno, guint size, 
					gpointer buffer, int dither_mode)
{
     off_t x;
     guint u;
     gchar *c;
     switch (source->type) {
     case DATASOURCE_REAL:
	  memcpy(buffer,source->data.real+sampleno*source->format.samplebytes,
		 size);
	  return size;
     case DATASOURCE_VIRTUAL: 
     case DATASOURCE_TEMPFILE:
	  /* Calculate offset in file */
	  x = source->data.virtual.offset + 
	       sampleno*source->format.samplebytes;
	  if (x != source->data.virtual.pos && 
	      e_fseek(source->data.virtual.handle, x, SEEK_SET))
	       return 0;
	  source->data.virtual.pos = x;
	  if (e_fread(buffer,size,source->data.virtual.handle)) return 0;
	  source->data.virtual.pos += size;
	  return size;
     case DATASOURCE_SILENCE:
	  memset(buffer,0,size);
	  return size;
     case DATASOURCE_SNDFILE:
     case DATASOURCE_SNDFILE_TEMPORARY:
	  if (source->format.type == DATAFORMAT_PCM)
	       return datasource_sndfile_read_array_pcm(source,sampleno,size,
							buffer);
	  else
	       return datasource_sndfile_read_array_fp
		    (source,sampleno,size/source->format.samplebytes,buffer);
     case DATASOURCE_REF:
	  return datasource_read_array_main(source->data.clone,sampleno,size,
					    buffer,dither_mode);
     case DATASOURCE_CLONE:
	  return datasource_clone_read_array(source,sampleno,size,buffer,
					     dither_mode);
     case DATASOURCE_BYTESWAP:
	  u = datasource_read_array_main(source->data.clone,sampleno,size,
					 buffer,dither_mode);
	  if (u>0) byteswap(buffer,source->format.samplesize,u);
	  return u;
     case DATASOURCE_CONVERT:
	  u = size / source->format.samplebytes;
	  if (source->format.type == DATAFORMAT_FLOAT && 
	      source->format.samplesize == sizeof(sample_t)) 
	       return datasource_read_array_fp(source->data.clone,sampleno,u,
					       buffer,dither_mode) *
		    source->format.samplebytes ;
	  c = g_malloc(u*sizeof(sample_t)*source->format.channels);
	  u = datasource_read_array_fp(source->data.clone, sampleno, u, 
				       (gpointer)c,dither_mode);
	  if (u > 0) {
	       convert_array(c,&dataformat_sample_t,buffer,&(source->format),
			     u*source->format.channels,dither_mode);
	  }
	  g_free(c);
	  return u * source->format.samplebytes;
     default:
	  g_assert_not_reached();
	  return 0;
     }
}

guint datasource_read_array(Datasource *source, off_t sampleno, guint size,
			    gpointer buffer, int dither_mode)
{
     off_t o;
     g_assert(source->opencount > 0);
     /* Check sampleno */
     g_assert(sampleno <= source->length);     
     /* Check size */
     /* Round down to even samples */     
     size = size - size % source->format.samplebytes; 
     o = (source->length - sampleno) * (off_t)(source->format.samplebytes);
     if (size > o) size = (guint)o; /* Round down to available data */
     if (size == 0) return 0;
     /* Do it */
     return datasource_read_array_main(source,sampleno,size,buffer,
				       dither_mode);
}

gboolean datasource_read(Datasource *source, off_t sampleno, gpointer buffer, 
			 int dither_mode)
{
     return (datasource_read_array(source,sampleno,source->format.samplebytes,
				   buffer,dither_mode) 
	     != source->format.samplebytes);
}


guint datasource_read_array_fp(Datasource *source, off_t sampleno,
			       guint samples, sample_t *buffer, 
			       int dither_mode)
{
     gchar *p;
     guint x,s;
     
     g_assert(sampleno <= source->length);
     if (samples > source->length-sampleno) 
	  samples = (guint)(source->length-sampleno);
     if (samples == 0) return 0;


     switch (source->type) {
     case DATASOURCE_SILENCE:
	  memset(buffer,0,samples*source->format.channels*sizeof(sample_t));
	  return samples;
     case DATASOURCE_SNDFILE:
     case DATASOURCE_SNDFILE_TEMPORARY:
       return datasource_sndfile_read_array_fp(source,sampleno,samples,
					       buffer);
     case DATASOURCE_REF:
     case DATASOURCE_CONVERT:
	  return datasource_read_array_fp(source->data.clone,sampleno,
					  samples,buffer,dither_mode);
     default:
	  if (source->format.type == DATAFORMAT_FLOAT &&
	      source->format.samplesize == sizeof(sample_t))
	       
	       return datasource_read_array(source,sampleno,
					    samples*source->format.samplebytes,
					    buffer, dither_mode) 
		    / source->format.samplebytes;
	       
	  s = samples * source->format.samplebytes;
	  p = g_malloc(s);
	  x = datasource_read_array(source,sampleno,s,p,dither_mode);
	  g_assert(x==s || x==0);
	  if (x==s) {
	       convert_array(p,&(source->format),buffer,&dataformat_sample_t,
			     samples*source->format.channels,dither_mode);
	       g_free(p);
	       return samples;
	  }
	  g_free(p);
	  return 0;
     }
}

gboolean datasource_read_fp(Datasource *ds, off_t sampleno, sample_t *buffer,
			    int dither_mode)
{
     return (datasource_read_array_fp(ds,sampleno,1,buffer,dither_mode)!=1);
}

static gboolean datasource_uses_file(Datasource *ds, gchar *filename)
{
     return ((ds->type==DATASOURCE_VIRTUAL && 
	      is_same_file(ds->data.virtual.filename,filename)) ||
	     ((ds->type==DATASOURCE_SNDFILE &&
	       is_same_file(ds->data.sndfile.filename,filename))));
}

gboolean datasource_backup_unlink(gchar *filename)
{
     GList *l,*q=NULL;
     Datasource *ds;
     Datasource *backup=NULL;
     gchar *lastname=filename,*t;
     guint dirnum=0;
     gint i;

     /* Find out which Datasources actually use the file. */
     for (l=datasource_list; l!=NULL; l=l->next) {
	  ds = (Datasource *)l->data;
	  if (datasource_uses_file(ds,filename))
	       q = g_list_append(q,ds);
     }
     
     /* Now iterate through those files. */
     for (l=q; l!=NULL; l=l->next) {
	  ds = (Datasource *)l->data;
	  if (!datasource_uses_file(ds,filename)) continue;

	  /* FIXME: When multiple tempdir support has been added, try moving to
	   * all different partitions before copying */
	  if (backup == NULL) {
	       dirnum = 0;
	       while (1) {
		    t = get_temp_filename(dirnum);
		    if (t == NULL) {
			 dirnum = 0;
			 t = get_temp_filename(0);
			 i = xrename(lastname,t,TRUE);
			 break;
		    }
		    i = xrename(lastname,t,FALSE);
		    if (i!=2) break;
		    dirnum ++;
		    g_free(t);
	       } 
	       if (i) {
		    g_free(t);
		    return TRUE;
	       }		     
	  } else {
	       /* Special case: the same file has been opened many times. */
	       t = get_temp_filename(0);
	       if (errdlg_copyfile(lastname,t)) {
		    g_free(t);
		    return TRUE;
	       }
	  }
		    
	  switch (ds->type) {
	  case DATASOURCE_VIRTUAL:
	       g_free(ds->data.virtual.filename);
	       ds->data.virtual.filename = t;
	       ds->type = DATASOURCE_TEMPFILE;
	       break;
	  case DATASOURCE_SNDFILE:
	       g_free(ds->data.sndfile.filename);
	       ds->data.sndfile.filename = t;
	       ds->type = DATASOURCE_SNDFILE_TEMPORARY;
	       break;
	  default: 
	       g_assert_not_reached(); 
	       break;
	  }	  
	  backup = ds;
	  lastname = t;
     }

     g_list_free(q);
     return xunlink(filename);
}

Datasource *datasource_convert(Datasource *source, Dataformat *new_format)
{    
     Datasource *ds;
     if (source == NULL) return NULL;
     g_assert(!dataformat_equal(new_format,&(source->format)));
     g_assert(new_format->channels == source->format.channels &&
	      new_format->samplerate == source->format.samplerate);

     if (source->format.type == DATAFORMAT_PCM && 
	 new_format->type == DATAFORMAT_PCM && 
	 source->format.samplesize == new_format->samplesize &&
	 source->format.sign == new_format->sign) {	  
	  return datasource_byteswap(source);
     }

     ds = gtk_type_new(datasource_get_type());
     ds->type = DATASOURCE_CONVERT;
     memcpy(&(ds->format),new_format,sizeof(Dataformat));
     ds->length = source->length;
     ds->bytes = ds->length * new_format->samplebytes;
     ds->data.clone = source;
     gtk_object_ref(GTK_OBJECT(source));
     gtk_object_sink(GTK_OBJECT(source));
     return ds;     
}
