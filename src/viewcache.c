/*
 * Copyright (C) 2002 2003 2004 2005 2007 2009, Magnus Hjorth
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

#include <string.h>
#include <math.h>
#include "viewcache.h"
#include "inifile.h"
#include "main.h"

/* How many columns to update (max) for each call to view_cache_update */
#define PIXELS_PER_UPDATE 10
#define GUINT32(x) ((guint32)x)
#define FLOAT(x) ((float)x)
#define DOUBLE(x) ((double)x)

/* These constants define the different possible values in the calced array */
#define CALC_UNKNOWN 0 /* No value */
#define CALC_DIRTY   1 /* Approximate value after zooming */
#define CALC_LOWQ    2 /* Low quality */
#define CALC_DONE    3 /* High quality  */

ViewCache *view_cache_new(void)
{
     ViewCache *vc;
     vc = g_malloc(sizeof(*vc));
     vc->chunk = NULL;
     vc->handle = NULL;
     vc->values = NULL;
     vc->offsets = NULL;
     vc->calced = NULL;
     return vc;
}

static void view_cache_clear(ViewCache *vc)
{
     if (vc->chunk) {
	  if (vc->handle) chunk_close(vc->handle);
	  vc->handle = NULL;
	  gtk_object_unref(GTK_OBJECT(vc->chunk));
     }
     g_free(vc->values);
     vc->values = NULL;
     g_free(vc->offsets);
     vc->offsets = NULL;
     g_free(vc->calced);
     vc->calced = NULL;
}

void view_cache_free(ViewCache *vc)
{
     view_cache_clear(vc);
     g_free(vc);
}

gboolean view_cache_update(ViewCache *cache, Chunk *chunk, off_t start_samp, 
			   off_t end_samp, gint xres, gint *low_updated,
			   gint *high_updated)
{
     static sample_t *sbuf=NULL;
     static guint sbufsize=0;
     static gboolean readflag = FALSE;
          
     sample_t *new_values;
     off_t *new_offsets, o;
     gchar *new_calced;

     gboolean chunk_change;
     gboolean range_change;

     gdouble d;
     gdouble real_spp;
     gint lowq_spp,highq_spp;
     gint dirty_code = CALC_DIRTY;
     sample_t f,g,h;
     guint i,j,k,l,m,n,new_spp;
     gchar *c;
     guint channels = chunk?chunk->format.channels:0;

     sample_t x;
     guint impr = 0;

     /* if (xres > end_samp-start_samp) xres=end_samp-start_samp; */
     if (readflag) return FALSE;

     /* Special case - empty file */
     if (end_samp == start_samp) {
	  if (cache->xres != xres) {
	       g_free(cache->calced);
	       cache->calced = g_malloc0(xres);
	       cache->xres = xres;
	  }
	  if (low_updated) {
	       *low_updated = 0;
	       *high_updated = xres-1;
	  }
	  return FALSE;
     }

     chunk_change = (cache->chunk != chunk);
     range_change = (chunk != NULL) && (chunk_change ||
					cache->start != start_samp || 
					cache->end != end_samp || 
					cache->xres != xres);

     /* Reading from the inifile takes a bit of time so we check if we can
      * exit here. */
     if (!chunk_change && !range_change && cache->handle==NULL) return FALSE;
     
     real_spp = ((gdouble)(end_samp - start_samp)) / ((gdouble)xres);     
     lowq_spp = inifile_get_guint32(INI_SETTING_VIEW_QUALITY,
				    INI_SETTING_VIEW_QUALITY_DEFAULT);
     if (lowq_spp >= real_spp) lowq_spp = 0;
     highq_spp = inifile_get_guint32(INI_SETTING_VIEW_QUALITY_HIGH,
				     INI_SETTING_VIEW_QUALITY_HIGH_DEFAULT);
     if (highq_spp >= real_spp) highq_spp = 0;
     if (!inifile_get_gboolean("drawImprove",TRUE)) {
	  highq_spp=lowq_spp;
	  dirty_code = CALC_DONE;
     }
     

     /* Handle changed chunk */
     if (chunk_change) {
	  view_cache_clear(cache);
	  cache->chunk = chunk;
	  if (chunk != NULL) 
	       gtk_object_ref(GTK_OBJECT(chunk));
	  cache->chunk_error = FALSE;
     }

     if (cache->chunk_error) return FALSE;

     /* Handle changed range */
     if (range_change) {     	  

	  if (cache->handle == NULL) 
	       /* Failure here is handled a bit later */
	       cache->handle = chunk_open(chunk);

	  /* Create new buffers */
	  new_values = g_malloc(xres * channels * 2 * sizeof(*new_values));
	  /* Make one extra entry in offsets with end point. (used below) */
	  new_offsets = g_malloc((xres+1) * sizeof(*new_offsets));
	  new_calced = g_malloc0(xres);

	  /* Calculate new_offsets */
	  new_offsets[0] = start_samp;
	  new_offsets[xres] = end_samp;
	  d = (double)start_samp + real_spp;
	  for (i=1; i<xres; i++,d+=real_spp) new_offsets[i] = (off_t)d;

	  /* Check again to see if lowq_spp or highq_spp needs changing */
	  lowq_spp = inifile_get_guint32(INI_SETTING_VIEW_QUALITY,
					 INI_SETTING_VIEW_QUALITY_DEFAULT);
	  if (lowq_spp >= real_spp) lowq_spp = 0;
	  highq_spp=inifile_get_guint32(INI_SETTING_VIEW_QUALITY_HIGH,
					INI_SETTING_VIEW_QUALITY_HIGH_DEFAULT);
	  if (highq_spp >= real_spp) highq_spp = 0;
	  if (!inifile_get_gboolean("drawImprove",TRUE)) highq_spp=lowq_spp;

	  /* Three cases - scrolling, zooming and no overlap at all */	  
	  if (chunk_change ||
	      end_samp <= cache->start || start_samp >= cache->end) {

	       /* No overlap - don't do anything more */

	  } else if (xres == cache->xres && 
		     end_samp-start_samp == cache->end-cache->start) {

	       /* Scrolling */

	       /* Set j=old location, i=new location, k=# of values to copy */
	       j=0;
	       i=0;
	       if (cache->offsets[0] < new_offsets[0]) 
		    j = GUINT32(DOUBLE(new_offsets[0] - cache->offsets[0]) / 
				real_spp);
	       else
		    i = GUINT32(DOUBLE(cache->offsets[0] - new_offsets[0]) / 
				real_spp);

	       k = xres - MAX(i,j);
	       /* printf("%d -> %d, (%d)\n",j,i,k); */
	       memcpy(new_values+(2*i*channels), 
		      cache->values+(2*j*channels),
		      k*channels*2*sizeof(sample_t));
	       o = cache->offsets[j] - new_offsets[i];
	       for (l=0; l<=xres; l++) new_offsets[l] += o;
	       /* When real_spp < 1.0, we may get errors due to offset rounding
		* therefore, set the cache status to DIRTY */

	       if (real_spp >= 1.0)
		    memcpy(new_calced+i, cache->calced+j, k);
	       else
		    memset(new_calced+i, CALC_DIRTY, k);
	       
	  } else if (end_samp-start_samp > cache->end-cache->start) {

	       /* Zoom out */

	       /* i: index to copy from in old cache. 
		* j: index to copy to in new cache. */

	       i=0;
	       j=0;	       
	       
	       while (1) {
		    
		    while (i<cache->xres &&
			   (!cache->calced[i] || 
			    cache->offsets[i+1]<new_offsets[j]))
			 i++;
		    if (i == cache->xres) break;

		    while (cache->offsets[i]>new_offsets[j] && j<xres) j++;
		    if (j == xres) break;

		    /* printf("Copying %d -> %d\n",(int)i,(int)j); */
		    new_calced[j] = dirty_code;
		    memcpy(new_values + j*2*channels, 
			   cache->values + 2*i*channels,
			   2*channels*sizeof(new_values[0]));
		    
		    j++;
	       }


	  } else {

	       /* Zoom in */

	       /* i: index to copy from in old cache.
		* j: first index to copy to in new cache.
		* k: last index to copy to in new cache. */
	       i=0;
	       j=0;

	       while (1) {

		    /* Set i to the next calculated value large enough 
		       in old cache */
		    while (i<cache->xres &&
			   (!cache->calced[i] || 
			    cache->offsets[i+1]<new_offsets[j]))
			 i++;
		    if (i == cache->xres) break;
		    
		    /* Setup j */
		    while (new_offsets[j] < cache->offsets[i] && j<xres) j++;
		    if (j == xres) break;

		    /* Setup k */
		    k = j;
		    while (new_offsets[k+1] < cache->offsets[i+1] && k+1<xres) 
			 k++;

		    /* Perform copy */
		    /* Set the new cache entries as "dirty" */
		    /* printf("Copying %d -> %d-%d\n",(int)i,(int)j,(int)k); */
		    new_calced[j] = dirty_code;
		    /* memset(new_calced+j, CALC_DIRTY, 1+k-j); */
		    memcpy(new_values + j*2*channels, 
			   cache->values + i*2*channels, 
			   2*channels*sizeof(new_values[0]));
		    
		    j = k+1;
		    i++; 

	       }

	  }

	  if (cache->handle == NULL) {
	       memset(new_values,0,xres*channels*2*sizeof(*new_values));
	       memset(new_calced,CALC_DONE,xres);
	       cache->chunk_error = TRUE;
	  }

	  /* Update cache vars */
	  cache->start = start_samp;
	  cache->end = end_samp;
	  cache->xres = xres;
	  g_free(cache->values);
	  cache->values = new_values;
	  g_free(cache->offsets);
	  cache->offsets = new_offsets;
	  g_free(cache->calced);
	  cache->calced = new_calced;
	  if (low_updated != NULL) {
	       *low_updated = 0;
	       *high_updated = xres-1;
	  }
	  return TRUE;
     }



     /* Regular update */

     if (chunk==NULL || cache->handle==NULL || cache->chunk_error) 
	  return FALSE;

     /* Special case - zoomed in very far */
     if (real_spp < 1.0) {
	  /* Due to rounding, the offsets array can differ from
	   * start_samp/end_samp by 1 sample. */
	  k = cache->offsets[xres-1] - cache->offsets[0] + 1;
	  m = k * channels * sizeof(sample_t);
	  if (m > sbufsize) {
	       g_free(sbuf);
	       sbufsize = m;
	       sbuf = g_malloc(m);
	  }
	  readflag = TRUE;
	  m = chunk_read_array_fp(cache->handle, cache->offsets[0], k, sbuf, 
				  DITHER_NONE, NULL);
	  readflag = FALSE;

	  for (n=0; n<xres; n++) {
	       for (l=0; l<channels; l++) {
		    cache->values[(n*channels+l)*2] = cache->values[(n*channels+l)*2+1] = 
			 sbuf[(cache->offsets[n] - cache->offsets[0])*channels + l];
	       }
	  }

	  memset(cache->calced, CALC_DONE, xres);

	  chunk_close(cache->handle);
	  cache->handle = NULL;

	  if (low_updated) {
	       *low_updated = 0;
	       *high_updated = xres-1;
	  }

	  return TRUE;
     }
     
     /* Scan for uncalculated data */
     c = memchr(cache->calced, CALC_UNKNOWN, xres);
     if (c == NULL) c = memchr(cache->calced, CALC_DIRTY, xres);
     if (c == NULL) { 
	  impr=lowq_spp; 
	  c = memchr(cache->calced, CALC_LOWQ, xres); 
     }     	 
     if (c == NULL) {
	  /* All cache entries are perfect, close handle. */
	  chunk_close(cache->handle);
	  cache->handle = NULL;
	  if (low_updated) *low_updated = *high_updated = 0;
	  return TRUE;
     }
 
     i = c - cache->calced;     

     /* Scan for end of uncalculated data */
     j = i+1;
     if (impr == 0) 
	  while (j<i+PIXELS_PER_UPDATE && j<xres && cache->calced[j]<CALC_LOWQ)
	       j++;
    
     /* Fill in as calculated */
     if (impr == 0 && lowq_spp > 0) {
	  new_spp = lowq_spp;
	  memset(cache->calced+i, CALC_LOWQ, j-i);
     } else {
	  new_spp = highq_spp;
	  memset(cache->calced+i, CALC_DONE, j-i);
     }

     if (low_updated) { 
	  *low_updated = i; 
	  *high_updated = j;
     }

     if (new_spp == 0 && impr == 0) {
	  /* CONSECUTIVE MODE */	  
	  k = cache->offsets[j]-cache->offsets[i]; /* # of samples to read */
	  if (k < 1) k = 1;
	  /* Read data */
	  m = k * channels * sizeof(sample_t); /* # of converted bytes */
	  if (m>sbufsize) {
	       g_free(sbuf);
	       sbufsize = m;
	       sbuf = g_malloc(sbufsize);
	  }
	  readflag = TRUE;
	  m=chunk_read_array_fp(cache->handle,cache->offsets[i],k,sbuf,
				DITHER_NONE, NULL);
	  readflag = FALSE;
	  g_assert(m==0 || m==k);

	  if (m == 0) {
	       /* Fill in all data as read so we don't get flooded with error
		* messages */
	       memset(cache->calced, CALC_DONE, xres);
	       return TRUE;
	  }
	  
	  /* Calculate max/min */
	  m = 0; /* Current full sample in sbuf */
	  n = i; /* Current pos in calced */

	  while (n<j) {	       
	       /* Set "impossible" values */
	       for (l=0; l<channels; l++) {
		    cache->values[(n*channels+l)*2] = 1.0; /* Min */
		    cache->values[(n*channels+l)*2+1] = -1.0; /* Max */
	       }
	       /* Go through all necessary samples */
	       while (m+cache->offsets[i] < cache->offsets[n+1]) {
		    for (l=0; l<channels; l++) {
			 x = sbuf[m*channels+l];
			 if (x < cache->values[(n*channels+l)*2])
			      cache->values[(n*channels+l)*2] = x;
			 if (x > cache->values[(n*channels+l)*2+1])
			      cache->values[(n*channels+l)*2+1] = x;
		    }
		    m++;
	       }
	       n++;
	  }

     } else if (new_spp == 0) {

	  /* Improvement to perfect */
	  for (; i<j; i++) {

	       k = cache->offsets[i+1]-cache->offsets[i]-impr;
	       g_assert ( ((gint)k) >= 0 );

	       l = k * channels * sizeof(sample_t);
	       if (sbufsize < l) {
		    g_free(sbuf);
		    sbufsize = l;
		    sbuf = g_malloc(sbufsize);
	       }
	       readflag = TRUE;
	       m = chunk_read_array_fp(cache->handle, cache->offsets[i]+impr,
				       k, sbuf,DITHER_NONE,NULL);
	       readflag = FALSE;
	       if (m == 0) {
		    memset(cache->calced, CALC_DONE, xres);
		    return TRUE;
	       }
	       
	       for (m=0; m<channels; m++) {
		    g = cache->values[(i*channels + m)*2];
		    h = cache->values[(i*channels + m)*2 + 1];
		    for (n=0; n<k; n++) {
			 f = sbuf[n*channels+m];
			 if (f<g) g=f;
			 if (f>h) h=f;
		    }
		    cache->values[(i*channels + m)*2] = g;
		    cache->values[(i*channels + m)*2 + 1] = h;		    
	       }

	       
	  }

     } else {
	  
	  /* REGULAR MODE */
	  while (i<j) {
	       k = (new_spp-impr) * channels * sizeof(sample_t);
	       if (sbufsize < k) {
		    g_free(sbuf);
		    sbufsize = k;
		    sbuf = g_malloc(sbufsize);
	       }
	       readflag = TRUE;
	       l=chunk_read_array_fp(cache->handle, cache->offsets[i]+impr, 
				     new_spp-impr, sbuf, DITHER_NONE,NULL);
	       readflag = FALSE;
	       g_assert(l==new_spp-impr || l==0);
	       if (l == 0) {
		    memset(cache->calced, CALC_DONE, xres);
		    return TRUE;
	       }
	       
	       
	       for (k=0; k<channels; k++) {
		    if (impr == 0) {
			 g = 1.0;
			 h = -1.0;
		    } else {
			 g = cache->values[(i*channels+k)*2];
			 h = cache->values[(i*channels+k)*2+1];
		    }
		    for (l=0; l<new_spp-impr; l++) {
			 f = sbuf[l*channels+k];
			 if (f<g) g=f;
			 if (f>h) h=f;
		    }
		    cache->values[(i*channels+k)*2] = g;
		    cache->values[(i*channels+k)*2+1] = h;
	       }

	       i++;
	  }

     }
     
     return TRUE;
}

gboolean view_cache_uptodate(ViewCache *cache)
{
     return (cache->handle == NULL);
}

void view_cache_draw(ViewCache *cache, GdkDrawable *d, gint yres, 
		     GtkWidget *wid, gfloat scale)
{
     view_cache_draw_part(cache,d,0,cache->xres-1,yres,wid,scale);
}

void view_cache_draw_part(ViewCache *cache, GdkDrawable *d, gint xs, gint xe, 
			  gint yres, GtkWidget *wid, gfloat scale)
{
     gint i,j,min,max,lmin=0,lmax=0,segc1=0,segc2=0,segc;
     gint *segcp;
     gboolean haslast;
     float mid,size,f,g;
     sample_t q,w;
     /* GdkGC *gc; */
     gchar *c,*b;
     guint channels = (cache->chunk)?cache->chunk->format.channels:0;
     GdkSegment *seg1,*seg2,*seg;

     if (cache->calced == NULL) return;

     /* if (xs >= cache->chunk->length) return;
	if (xe >= cache->chunk->length) xe = cache->chunk->length-1; */

     size = yres/(2*channels) - 5;

     seg1 = g_malloc(((channels+1)/2) * (1+xe-xs) * sizeof(GdkSegment));
     if (channels > 1)
	  seg2 = g_malloc((channels/2) * (1+xe-xs) * sizeof(GdkSegment));
     else
	  seg2 = NULL;

     for (j=0; j<channels; j++) {

	  if ((j&1) == 0) { seg = seg1; segcp = &segc1;
	  } else { seg = seg2; segcp = &segc2; }
	  
	  segc = *segcp;

	  mid = ((yres*j)/channels) + (yres/(2*channels));
	  /* gc = (j&1)?get_gc(WAVE2,wid):get_gc(WAVE1,wid); */

	  haslast = FALSE;
	  
	  for (i=xs; i<=xe; i++) {
	       if (cache->calced[i] == 0) {
		    c = memchr(cache->calced+i, 1, 1+xe-i);
		    b = memchr(cache->calced+i, 2, 1+xe-i);
		    if (c == NULL || (b != NULL && b<c)) c=b;
		    b = memchr(cache->calced+i, 3, 1+xe-i);
		    if (c == NULL || (b != NULL && b<c)) c=b;
		    if (c == NULL) break;
		    i = c-cache->calced;
		    haslast = FALSE;
	       }
	       q = cache->values[(i*channels+j)*2] * scale;
	       w = cache->values[(i*channels+j)*2+1] * scale;
	       if (q < -1.0) q = -1.0; else if (q > 1.0) q = 1.0;
	       if (w < -1.0) w = -1.0; else if (w > 1.0) w = 1.0;
	       f = -q*size+mid;
	       g = -w*size+mid;
	       min = (gint) g;
	       max = (gint) f;
	       if (haslast) {
		    if (min > lmax) min = lmax+1;
		    else if (max < lmin) max = lmin-1;
	       }	       
	       /* gdk_draw_line(d,gc,i,max,i,min); */
	       seg[segc].x1 = seg[segc].x2 = i;
	       seg[segc].y1 = max;
	       seg[segc].y2 = min;
	       segc++;
	       lmin = min;
	       lmax = max;
	       haslast = TRUE;
	  }
	  *segcp = segc;
     }
     
     gdk_draw_segments(d,get_gc(WAVE1,wid),seg1,segc1);
     if (seg2 != NULL) gdk_draw_segments(d,get_gc(WAVE2,wid),seg2,segc2);
     
     g_free(seg1);
     g_free(seg2);
}
