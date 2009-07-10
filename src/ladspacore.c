/*
 * Copyright (C) 2004 2005 2007, Magnus Hjorth
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

#ifdef HAVE_LADSPA

#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <locale.h>
#include "um.h"
#include "ladspacore.h"
#include "gettext.h"

static GHashTable *effect_list;

static int descriptor_to_index(LADSPA_PortDescriptor pdesc)
{
     int j;
     if ((pdesc & LADSPA_PORT_INPUT) != 0) j=0;
     else if ((pdesc & LADSPA_PORT_OUTPUT) != 0) j=1;
     else return -1;
     if ((pdesc & LADSPA_PORT_CONTROL) != 0) return j;
     else if ((pdesc & LADSPA_PORT_AUDIO) != 0) return j+2;
     else return -1;
}

static void scan_file(gchar *filename, gchar *basename, gboolean do_check)
{
     void *p;
     LADSPA_Descriptor_Function func;
     gchar *msg,*c;
     gint i,j;
     int k[4];
     unsigned long l;
     const LADSPA_Descriptor *desc;
     LadspaEffect *eff;

     p = dlopen(filename,RTLD_NOW);
     if (p == NULL) {
	  console_message(dlerror());
	  return;
     }
     func = dlsym(p,"ladspa_descriptor");
     if (func == NULL) {	       
	  msg = g_strdup_printf("%s: ladspa_descriptor: %s",
				basename,dlerror());
	  console_message(msg);
	  g_free(msg);
	  dlclose(p);
	  return;
     }
     i=0;
     while (1) {
     outer:
	  desc = func(i);
	  if (desc == NULL) break;	       
	  c = g_strdup_printf("%ld_%s",(long int)desc->UniqueID,basename);
	  if (do_check && 
	      g_hash_table_lookup(effect_list,c) != NULL) {
	       g_free(c);
	       i++;
	       continue;
	  }
	  eff = g_malloc0(sizeof(*eff));
	  eff->id = c;
	  eff->filename = g_strdup(filename);
	  eff->effect_number = i++;
	  eff->name = g_strdup(desc->Name);
	  eff->maker = g_strdup(desc->Maker);
	  eff->copyright = g_strdup(desc->Copyright);
	  for (l=0; l<desc->PortCount; l++) {
	       j = descriptor_to_index(desc->PortDescriptors[l]);
	       if (j < 0) {
		    msg = g_strdup_printf(_("Effect %s contains "
					    "invalid port %s"),eff->name,
					  desc->PortNames[l]);
		    console_message(msg);
		    g_free(msg);
		    g_free(eff->id);
		    g_free(eff->filename);
		    g_free(eff->name);
		    g_free(eff->maker);
		    g_free(eff->copyright);
		    g_free(eff);
		    goto outer;
	       }
	       eff->numports[j]++;
	  }
	  for (j=0; j<4; j++)
	       if (eff->numports[j] > 0)
		    eff->ports[j] = g_malloc(eff->numports[j] * 
					     sizeof(LadspaPort));
	  k[0] = k[1] = k[2] = k[3] = 0;
	  for (l=0; l<desc->PortCount; l++) {
	       j = descriptor_to_index(desc->PortDescriptors[l]);
	       if (j < 0) continue;
	       eff->ports[j][k[j]].number = l;
	       eff->ports[j][k[j]].name = g_strdup(desc->PortNames[l]);
	       memcpy(&(eff->ports[j][k[j]].prh),
		      &(desc->PortRangeHints[l]),
		      sizeof(LADSPA_PortRangeHint));
	       k[j] ++;
	  }
	  g_hash_table_insert(effect_list,eff->id,eff);
     }
     dlclose(p);
}

static void scan_directory(gchar *dir, gpointer dummy)
{
     DIR *d;
     struct dirent *de;
     int i;
     gchar *fn,*msg;
     char *cur_lc_numeric;

     d = opendir(dir);
     if (d == NULL) {
	  if (errno != ENOENT) {
	       msg = g_strdup_printf(_("Ladspa: Error scanning %s"),dir);
	       console_perror(msg);
	       g_free(msg);
	  }
	  return;
     }

     /* Some LADSPA plugins resets LC_NUMERIC setting,
      * so we need to save it. */
     cur_lc_numeric = setlocale(LC_NUMERIC, NULL);
     
     while (1) {
	  de = readdir(d);
	  if (de == NULL) break;
	  i = strlen(de->d_name);
	  if (i<4 || strcmp(de->d_name + i-3, ".so")) continue;
	  fn = g_strdup_printf("%s/%s",dir,de->d_name);

	  scan_file(fn,de->d_name,FALSE);

	  g_free(fn);
     }
     closedir(d);

     /* Restore LC_NUMERIC setting. */
     setlocale(LC_NUMERIC, cur_lc_numeric);
}

static gboolean ladspa_cleanup_func(gpointer key, gpointer value, 
				    gpointer user_data)
{
     int i,j;
     LadspaEffect *eff;
     eff = (LadspaEffect *)value;
     g_free(eff->id);
     g_free(eff->name);
     g_free(eff->filename);
     g_free(eff->maker);
     g_free(eff->copyright);

     for (i=0; i<4; i++) {
	  for (j=0; j<eff->numports[i]; j++)
	       g_free(eff->ports[i][j].name);	  
	  g_free(eff->ports[i]);
     }

     g_free(eff);
     /* eff->id == value, so no need to free value again */
     return TRUE;
}

static void ladspa_cleanup(void)
{
     if (effect_list == NULL) return;
     g_hash_table_foreach_remove(effect_list, ladspa_cleanup_func, NULL);
     g_hash_table_destroy(effect_list);
     effect_list = NULL;
}

static void ladspa_path_foreach(void (*function)(gchar *dirname, 
						 gpointer user_data),
				gpointer user_data)
{
     gchar *p,*c;    

     p = getenv("LADSPA_PATH");
     if (p == NULL) {
	  p = DEFAULT_LADSPA_PATH;
	  /* console_message(_("Environment variable LADSPA_PATH not set.\n"
	     "LADSPA support is disabled."));
	     return; */
     }
     p = g_strdup(p);
     c = strtok(p,":");
     while (c != NULL) {
	  function(c,user_data);
	  c = strtok(NULL,":");
     }
     free(p);

}

void ladspa_rescan(void)
{
     ladspa_cleanup();
     effect_list = g_hash_table_new(g_str_hash,g_str_equal);

     ladspa_path_foreach(scan_directory,NULL);
}

static void foreach_func(gpointer key, gpointer value, gpointer user_data)
{
     void (*func)(void *) = (void (*)(void *))user_data;
     func(value);
}

void ladspa_foreach_effect(void (*func)(LadspaEffect *eff))
{
     g_hash_table_foreach(effect_list,foreach_func,func);
}

void find_effect_func(gchar *dirname, gpointer user_data)
{
     gchar *fn = (gchar *)user_data;
     gchar *c;
     c = g_strdup_printf("%s/%s",dirname,fn);
     if (file_exists(c)) 
	  scan_file(c,fn,TRUE);
     g_free(c);
}

LadspaEffect *ladspa_find_effect(gchar *id)
{
     LadspaEffect *eff;
     gchar *c,*d,*e;
     unsigned long unid;

     if (effect_list == NULL) 
	  effect_list = g_hash_table_new(g_str_hash,g_str_equal);
     eff = (LadspaEffect *)g_hash_table_lookup(effect_list,id);
     if (eff != NULL) return eff;
     
     /* Parse the ID to get uniqueID and filename */
     c = g_strdup(id);
     d = strchr(c,'_');
     if (d == NULL) {
	  g_free(c);
	  return NULL;
     }
     *d = 0;
     d++;
     /* c should now be UniqueID, d filename. g_free(c) frees both c & d */
     unid = strtoul(c,&e,10);
     if (*e != 0) {
	  g_free(c);
	  return NULL;
     }
     
     /* Search for the file and add it's effects if found */
     ladspa_path_foreach(find_effect_func,d);
     g_free(c);

     /* Try again */
     eff = (LadspaEffect *)g_hash_table_lookup(effect_list,id);
     
     return eff;
     
}

static LadspaEffect *processing_effect;
static const LADSPA_Descriptor *processing_desc;
static LADSPA_Handle processing_handle;
static sample_t *processing_outbuf;

static gboolean ladspa_filter_proc(void *in, guint sample_size, 
				   chunk_writeout_func out_func, WriteoutID id,
				   Dataformat *format)
{
     sample_t *s = (sample_t *)in;
     guint chans = format->channels;
     guint frames = sample_size / (sizeof(sample_t)*chans);
     guint i,j,k,m;
     int l;
     float *b;
     for (i=0; i<frames; i+=j) {
	  j = MIN(frames-i, 1024);
	  /* Set input */
	  for (k=0; k < processing_effect->numports[2]; k++) {
	       l = processing_effect->ports[2][k].map;
	       if (l < 0) continue;
	       b = processing_effect->ports[2][k].buffer;
#ifndef USE_DOUBLE_SAMPLES	       
	       if (chans == 1) {
		    memcpy(b,&s[i],j*sizeof(float));
		    continue;
	       }		    
#endif
	       for (m=0; m<j; m++)
		    b[m] = (float) s[chans*(i+m) + l];
	  }
	  /* Run plugin */
	  processing_desc->run(processing_handle,j);
	  /* Get output */
	  /* Copy the input to use as default (a little inefficient, 
	     I know..) */
	  if (processing_effect->keep)
	       memcpy(processing_outbuf,&s[i],j*chans*sizeof(sample_t));
	  else
	       memset(processing_outbuf,0,j*chans*sizeof(sample_t));
	  for (k=0; k < processing_effect->numports[3]; k++) {
	       l = processing_effect->ports[3][k].map;
	       if (l < 0) continue;
	       b = processing_effect->ports[3][k].buffer;
#ifndef USE_DOUBLE_SAMPLES	       
	       if (chans == 1) {
		    memcpy(processing_outbuf,b,j*sizeof(float));
		    continue;
	       }		    
#endif
	       for (m=0; m<j; m++)
		    processing_outbuf[chans*m + l] = b[m];
	  }
	  /* Write output */
	  if (out_func(id,processing_outbuf,j*chans*sizeof(sample_t)))
	       return TRUE;
     }
     return FALSE;
}

Chunk *ladspa_run_effect(Chunk *chunk, StatusBar *bar, LadspaEffect *eff, 
			 int dither_mode)
{
     void *p;
     gchar *c;
     int i,j,k;
     Chunk *r;
     float f;
     LADSPA_Descriptor_Function func;
     const LADSPA_Descriptor *desc;
     LADSPA_Handle hand;
     char *cur_lc_numeric;

     /* Save LC_NUMERIC setting. */
     cur_lc_numeric = setlocale(LC_NUMERIC, NULL);

     /* Open plugin */
     p = dlopen(eff->filename,RTLD_LAZY);
     if (p == NULL) {
	  user_error(dlerror());
	  return NULL;
     }
     func = dlsym(p,"ladspa_descriptor");
     if (func == NULL) {
	  c = g_strdup_printf("%s: ladspa_descriptor: %s",eff->filename,
			      dlerror());
	  user_error(c);
	  g_free(c);
	  dlclose(p);
	  setlocale(LC_NUMERIC, cur_lc_numeric);
	  return NULL;
     }
     desc = func(eff->effect_number);
     g_assert(desc != NULL);
     hand = desc->instantiate(desc,chunk->format.samplerate);
     /* Connect ports */
     for (i=0; i<2; i++) 
	  for (j=0; j<eff->numports[i]; j++) {
	       f = eff->ports[i][j].value;
	       desc->connect_port(hand,eff->ports[i][j].number,
				  &(eff->ports[i][j].value));
	       eff->ports[i][j].value = f;
	  }
     for (i=2; i<4; i++)
	  for (j=0; j<eff->numports[i]; j++) {
	       eff->ports[i][j].buffer = g_malloc(1024*sizeof(float));
	       desc->connect_port(hand,eff->ports[i][j].number,
				  eff->ports[i][j].buffer);
	       if (i == 2 && eff->ports[i][j].map == -1)
		    for (k=0; k<1024; k++)
			 eff->ports[i][j].buffer[k] = 
			      eff->ports[i][j].value;
	  }
     /* Apply effect */
     processing_effect = eff;
     processing_desc = desc;
     processing_handle = hand;
     processing_outbuf = g_malloc(1024*chunk->format.channels*
				  sizeof(sample_t));
     if (desc->activate != NULL) desc->activate(hand);
     c = g_strdup_printf(_("Applying effect '%s'"),eff->name);
     r = chunk_filter(chunk, ladspa_filter_proc, NULL, CHUNK_FILTER_MANY,
		      TRUE, dither_mode, bar, c);
     g_free(c);
     if (desc->deactivate != NULL) desc->deactivate(hand);
     if (desc->cleanup != NULL) desc->cleanup(hand);
     dlclose(p);
     g_free(processing_outbuf);
     for (i=2; i<4; i++)
	  for (j=0; j<eff->numports[i]; j++)
	       g_free(eff->ports[i][j].buffer);

     /* Restore LC_NUMERIC setting. */
     setlocale(LC_NUMERIC, cur_lc_numeric);
     return r;
}

#endif /* HAVE_LADSPA */
