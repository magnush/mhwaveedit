/*
 * Copyright (C) 2002 2003 2004 2005 2006, Magnus Hjorth
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
#include <sys/stat.h>
#include <sys/types.h>

#include "tempfile.h"
#include "ringbuf.h"
#include "inifile.h"
#include "session.h"

#define MAX_REAL_SIZE inifile_get_guint32(INI_SETTING_REALMAX,INI_SETTING_REALMAX_DEFAULT)

static int tempfile_count = 0;


static guint8 *copyle16(guint8 *buf, guint16 n)
{
     buf[0] = n&0xFF;
     n >>= 8;
     buf[1] = n&0xFF;
     return buf+2;
}

static guint8 *copybe16(guint8 *buf, guint16 n)
{
     buf[1] = n&0xFF;
     n >>= 8;
     buf[0] = n&0xFF;
     return buf+2;
}

static guint8 *copyle32(guint8 *buf, guint32 n)
{
     buf[0] = n&0xFF;
     n >>= 8;
     buf[1] = n&0xFF;
     n >>= 8;
     buf[2] = n&0xFF;
     n >>= 8;
     buf[3] = n&0xFF;
     return buf+4;
}

static guint8 *copybe32(guint8 *buf, guint32 n)
{
     buf[3] = n&0xFF;
     n >>= 8;
     buf[2] = n&0xFF;
     n >>= 8;
     buf[1] = n&0xFF;
     n >>= 8;
     buf[0] = n&0xFF;
     return buf+4;
}

/*
PCM wav:
'RIFF'         4
#size+36       4
'wav '         4
'fmt '         4
#16            4
#1             2
#channels      2
#srate         4
#bytespersec   4
#bytesperframe 2
#bitspersmp    2
'data'         4
#size          4
           ----- total 44 bytes

IEEE wav:
'RIFF'         4
#size+50       4
'wav '         4
'fmt '         4
#18            4
#3             2
#channels      2
#srate         4
#bytespersec   4
#bytesperframe 2
#bitspersmp    2
#0             2
'fact'         4
#4             4
#samples       4
'data'         4
#size          4
           ------ total 58 bytes
 */
guint get_wav_header(Dataformat *format, off_t datasize, void *buffer)
{
     gboolean bigendian=FALSE;
     guint32 l, Bps=format->samplerate*format->channels*format->samplesize;
     guint16 Bpsmp=format->channels*format->samplesize;
     guint16 samplebits=(format->samplesize)<<3;
     guint8 *c = buffer;

     if (format->type == DATAFORMAT_PCM) {
	  bigendian = format->bigendian;
	  l = datasize + 36;
     } else {
	  bigendian = FALSE;
	  l = datasize + 50;
     }
     if (((off_t)l) < datasize) l = 0xFFFFFFFF;
     
     if (bigendian)
	  memcpy(c,"RIFX",4);
     else
	  memcpy(c,"RIFF",4);
     c += 4;     
     c = bigendian?copybe32(c,l):copyle32(c,l);

     if (format->type == DATAFORMAT_PCM) {
	  if (bigendian)
	       memcpy(c,"WAVEfmt \0\0\0\20\0\1",14);
	  else
	       memcpy(c,"WAVEfmt \20\0\0\0\1\0",14);
     } else
	  memcpy(c,"WAVEfmt \22\0\0\0\3\0",14);
     c += 14;
     if (bigendian) {
	  c = copybe16(c,format->channels);
	  c = copybe32(c,format->samplerate);
	  c = copybe32(c,Bps);
	  c = copybe16(c,Bpsmp);
	  c = copybe16(c,samplebits);
     } else {
	  c = copyle16(c,format->channels);
	  c = copyle32(c,format->samplerate);
	  c = copyle32(c,Bps);
	  c = copyle16(c,Bpsmp);
	  c = copyle16(c,samplebits);
     }

     if (format->type == DATAFORMAT_FLOAT) {
	  memcpy(c,"\0\0fact\4\0\0\0",10);
	  c += 10;
	  l = datasize / format->samplebytes;
	  if (((off_t)l) < datasize/format->samplebytes) l = 0xFFFFFFFF;
	  c = copyle32(c,l);
     } 
     memcpy(c,"data",4);
     c += 4;
     l = (guint32) datasize;
     if (((off_t)l) < datasize) l = 0xFFFFFFFF;
     c = bigendian?copybe32(c,l):copyle32(c,l);
     
     return (guint)((c-(guint8 *)buffer));
}


gboolean write_wav_header(EFILE *f, Dataformat *format,
			  off_t datasize)
{
     gchar c[58];
     guint i;
     i = get_wav_header(format,datasize,c);
     return e_fwrite(c,i,f);
}

static gchar *try_tempdir(gchar *dir)
{
     gchar *c;
     FILE *f;
     if (!dir || dir[0]==0) return 0;
     c = g_strjoin("/",dir,".mhwaveedit_temp_test",NULL);
     f = fopen(c,"w");
     if (f) {
	  fclose(f);
	  unlink(c);
     }
     g_free(c);
     return f?dir:NULL;
}

static GList *tempdirs = NULL;

void set_temp_directories(GList *l)
{
     guint i;
     gchar *c,*d;
     g_list_foreach(tempdirs,(GFunc)g_free,NULL);
     g_list_free(tempdirs);
     tempdirs = NULL;
     i = 1;
     while (l != NULL) {
	  c = (gchar *)l->data;
	  d = g_strdup_printf("tempDir%d",i);
	  inifile_set(d,c);
	  g_free(d);
	  tempdirs = g_list_append(tempdirs,g_strdup(l->data));
	  l = l->next;
	  i++;
     }
     while (1) {
	  d = g_strdup_printf("tempDir%d",i);
	  if (inifile_get(d,NULL) == NULL) break;
	  inifile_set(d,NULL);
	  g_free(d);
	  i++;
     }
}

gchar *get_temp_directory(guint num)
{
     gchar *c,*d;
     gint i=1;

     if (!tempdirs) {
	  c = inifile_get("tempDir1",NULL);
	  if (!c) {
	       c = getenv("TEMP");
	       if (c==NULL) c=getenv("TMP");
	       if (try_tempdir(c)) 
		    tempdirs = g_list_append(tempdirs,g_strdup(c));
	       c = g_strjoin("/",get_home_directory(),".mhwaveedit",NULL);
	       mkdir(c,CONFDIR_PERMISSION);
	       if (try_tempdir(c))
		    tempdirs = g_list_append(tempdirs,g_strdup(c));
	  } else {
	       do {
		    if (try_tempdir(c))
			 tempdirs = g_list_append(tempdirs,g_strdup(c));
		    i++;
		    d = g_strdup_printf("tempDir%d",i);
		    c = inifile_get(d,NULL);
		    g_free(d);
	       } while (c!=NULL);
	  }
     }
     return (gchar *)g_list_nth_data(tempdirs,num);
}

G_LOCK_DEFINE_STATIC(tempfile);

gchar *get_temp_filename_d(gchar *dir)
{
     gchar *c;
     /* printf("%s\n",d); */
     G_LOCK(tempfile);
     if (dir != NULL)
	  c =  g_strdup_printf("%s/mhwaveedit-temp-%d-%04d-%d",dir,
			       (int)getpid(),++tempfile_count,
			       session_get_id());
     else c = NULL;
     G_UNLOCK(tempfile);
     return c;
}

gchar *get_temp_filename(guint dirnum)
{
     return get_temp_filename_d(get_temp_directory(dirnum));
}

struct temp {
     guint dirs;
     EFILE **handles;
     off_t *written_bytes;
     guint current;
     gboolean realtime;
     Ringbuf *start_buf;
     Dataformat format;
     /* These two are only used when we're writing out partial frames. */
     gchar sample_buf[64];
     guint sample_buf_pos;
};

static void tempfile_open_dir(struct temp *t, guint dirnum, gboolean df)
{
     gboolean b;
     gchar *c;
     b = report_write_errors;
     report_write_errors = df;
     c = get_temp_filename(dirnum);
     t->handles[dirnum] = e_fopen(c,EFILE_WRITE);
     g_free(c);
     if (t->handles[dirnum] != NULL &&
	 write_wav_header(t->handles[dirnum],&(t->format),0x7FFFFFFF)) {
	  e_fclose_remove(t->handles[dirnum]);
	  t->handles[dirnum] = NULL;
     }
     report_write_errors = b;
}

TempFile tempfile_init(Dataformat *format, gboolean realtime)
{
     guint i;
     struct temp *t;     

     t = g_malloc(sizeof(*t));
     t->current = 0;
     t->realtime = realtime;
     memcpy(&(t->format),format,sizeof(Dataformat));

     if (!tempdirs) get_temp_directory(0);
     g_assert(tempdirs != NULL);
     t->dirs = g_list_length(tempdirs);
     t->handles = g_malloc0(t->dirs*sizeof(EFILE *));
     if (realtime)
	  for (i=0; i<t->dirs; i++) tempfile_open_dir(t,i,FALSE);
     t->written_bytes = g_malloc0(t->dirs*sizeof(off_t));
          

     if (!realtime) {
	  t->start_buf = ringbuf_new(MAX_REAL_SIZE);
	  g_assert(ringbuf_available(t->start_buf) == 0);
     } else
	  t->start_buf = NULL;

     t->sample_buf_pos = 0;

     return t;
}

static gboolean tempfile_write_main(struct temp *t, gchar *data, guint length)
{
     gboolean b,r;
     guint i;

     /* Handle partial frame writes */
     if (t->sample_buf_pos > 0) {
	  i = MIN(length, t->format.samplebytes - t->sample_buf_pos);
	  memcpy(t->sample_buf + t->sample_buf_pos, data, i);
	  t->sample_buf_pos += i;
	  if (t->sample_buf_pos < t->format.samplebytes)
	       return FALSE;
	  t->sample_buf_pos = 0;
	  if (tempfile_write_main(t,t->sample_buf,t->format.samplebytes))
	       return TRUE;
	  data += i;
	  length -= i;
     }

     if (length % t->format.samplebytes != 0) {
	  i = length % t->format.samplebytes;
	  memcpy(t->sample_buf,data+length-i,i);
	  t->sample_buf_pos = i;
	  length -= i;
     }

     if (length == 0) return FALSE;



     while (1) {
	  /* Make sure we have an opened file in the currently used directory*/
	  while (t->handles[t->current] == NULL && t->current < t->dirs) {
	       if (!t->realtime) tempfile_open_dir(t,t->current,FALSE);
	       if (t->handles[t->current] == NULL) t->current ++;
	  }
	  if (t->current >= t->dirs) {
	       /* Try again to make sure the user sees at least one error 
		* message. */
	       t->current = t->dirs-1;	       
	       tempfile_open_dir(t,t->current,TRUE);
	       if (t->handles[t->current] == NULL) return TRUE;
	  }
	  /* Write data */
	  b = report_write_errors;
	  report_write_errors = (t->current == t->dirs-1);
	  r = e_fwrite(data,length,t->handles[t->current]);
	  report_write_errors = b;
	  if (!r || (t->current == t->dirs-1)) {
	       t->written_bytes[t->current] += length;
	       return r;
	  }	  
	  t->current ++;
	  /* Loop around */
     }     
}

gboolean tempfile_write(TempFile handle, gpointer data, guint length)
{
     struct temp *t = (struct temp *)handle;
     gchar *buf,*c=data;
     guint32 u;
     /* Just add to start buffer */
     if (t->start_buf != NULL) {
	  u = ringbuf_enqueue(t->start_buf, c, length);
	  if (u == length) return FALSE;

	  /* Send buffer to file*/
	  c = c+u;
	  length = length - u;
	  buf = g_malloc(4096);
	  while (1) {
	       u = ringbuf_dequeue(t->start_buf, buf, 4096);
	       if (u == 0) break;
	       if (tempfile_write_main(t,buf,u)) return TRUE;	       
	  }
	  ringbuf_free(t->start_buf);
	  t->start_buf = NULL;
     } 
     
     /* Send data to file */
     return tempfile_write_main(t,c,length);
}

void tempfile_abort(TempFile handle)
{
     struct temp *t = (struct temp *)handle;
     guint i;
     for (i=0; i<t->dirs; i++) {
	  if (t->handles[i] != NULL)
	       e_fclose_remove(t->handles[i]);
     }
     g_free(t->handles);
     g_free(t->written_bytes);
     if (t->start_buf != NULL)
	  ringbuf_free(t->start_buf);
     g_free(t);
}

/* The error reporting in this function could be improved, but the user gets 
 * warned and it shouldn't leak so it's not that important. */
Chunk *tempfile_finished(TempFile handle)
{
     struct temp *t = (struct temp *)handle;
     guint32 u;
     Datasource *ds;
     Chunk *r=NULL,*c,*d;
     off_t l,o;
     guint i;
     
     if (t->start_buf != NULL) {
	  ds = gtk_type_new(datasource_get_type());
	  ds->type = DATASOURCE_REAL;
	  memcpy(&(ds->format),&(t->format),sizeof(Dataformat));
	  u = ringbuf_available(t->start_buf);
	  ds->data.real = g_malloc(u);
	  ringbuf_dequeue(t->start_buf,ds->data.real,u);

	  ds->bytes = (off_t)u;
	  ds->length = (off_t)(u / ds->format.samplebytes);

	  tempfile_abort(t);
	  return chunk_new_from_datasource(ds);
     }

     for (i=0; i<t->dirs; i++) {

	  if (t->handles[i] == NULL) continue;

	  l = t->written_bytes[i];
	  if (l == 0) {
	       e_fclose_remove(t->handles[i]);
	       t->handles[i] = NULL;
	       continue;
	  }

	  if (e_fseek(t->handles[i],0,SEEK_SET) ||
	      write_wav_header(t->handles[i], &(t->format),l)) {
	       e_fclose_remove(t->handles[i]);
	       t->handles[i] = NULL;
	       continue;
	  }
	  
	  o = e_ftell(t->handles[i]);
	  if (o < 0) {
	       e_fclose_remove(t->handles[i]);
	       t->handles[i] = NULL;
	       continue;
	  }

	  ds = gtk_type_new(datasource_get_type());
	  ds->type = DATASOURCE_TEMPFILE;

	  memcpy(&(ds->format),&(t->format),sizeof(Dataformat));
	  ds->length = l/(t->format.samplebytes);
	  ds->bytes = ds->length * ds->format.samplebytes;
	  ds->data.virtual.filename = g_strdup(t->handles[i]->filename);
	  ds->data.virtual.offset = o;

	  e_fclose(t->handles[i]);
	  t->handles[i] = NULL;

	  c = chunk_new_from_datasource(ds);
	  if (r == NULL)
	       r = c;
	  else {
	       d = r;
	       r = chunk_append(d,c);
	       gtk_object_sink(GTK_OBJECT(d));
	       gtk_object_sink(GTK_OBJECT(c));
	  }	  
     }

     tempfile_abort((TempFile)t);
     return r;
}

