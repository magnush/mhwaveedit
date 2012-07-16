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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "dataformat.h"
#include "inifile.h"
#include "main.h"
#include "gettext.h"

Dataformat dataformat_sample_t,dataformat_single;
gboolean ieee_le_compatible,ieee_be_compatible;
gint sample_convert_mode;

void floating_point_check(void)
{
     char c[4] = { 0, 0, 0xD0, 0xC0 };
     char d[4] = { 0xC0, 0xD0, 0, 0 };
     float *f;
     f = (float *)c;
     ieee_le_compatible = (*f == -6.5);
     f = (float *)d;
     ieee_be_compatible = (*f == -6.5);

     dataformat_sample_t.type = DATAFORMAT_FLOAT;
     dataformat_sample_t.samplesize = sizeof(sample_t);
     dataformat_sample_t.channels = 0;
     dataformat_sample_t.samplebytes = 0;
     dataformat_sample_t.samplerate = 0;
     
     dataformat_single.type = DATAFORMAT_FLOAT;
     dataformat_single.samplesize = sizeof(float);
     dataformat_single.channels = 0;
     dataformat_single.samplebytes = 0;
     dataformat_single.samplerate = 0;
}

gboolean dataformat_equal(Dataformat *f1, Dataformat *f2)
{
	return (f1->type == f2->type && f1->samplerate==f2->samplerate && 
		f1->samplesize==f2->samplesize && 
		f1->channels==f2->channels && 
		(f1->type != DATAFORMAT_PCM || 
		 (f1->sign==f2->sign && 
		  (f1->samplesize == 1 || f1->bigendian == f2->bigendian))));
}

gboolean dataformat_samples_equal(Dataformat *f1, Dataformat *f2)
{
     return (f1->type == f2->type && f1->samplesize==f2->samplesize &&
	     (f1->type != DATAFORMAT_PCM || 
	      (f1->sign==f2->sign && 
	       (f1->samplesize == 1 || f1->bigendian == f2->bigendian))));
}

const gchar *sampletype_name(int sampletype, guint samplesize)
{
     if (sampletype == DATAFORMAT_FLOAT) {
	  if (samplesize == sizeof(double)) return "double";
	  else return "float";
     }
     switch (samplesize) {
     case 1: return "8 bit";
     case 2: return "16 bit";
     case 3: return "24 bit";
     default:
     case 4: return "32 bit";
     }
}

gboolean dataformat_get_from_inifile(gchar *ini_prefix, gboolean full,
				     Dataformat *result)
{
     guint t,ss,chn;
     guint32 sr;
     gboolean sign=FALSE,end=FALSE;
     gchar *c,*d;
     c = g_strdup_printf("%s_SampleSize",ini_prefix);
     d = inifile_get(c,NULL);
     g_free(c);
     if (d == NULL) return FALSE;
     switch (d[0]) {
     case '1': t=DATAFORMAT_PCM; ss=1; break;
     case '2': t=DATAFORMAT_PCM; ss=2; break;
     case '3': t=DATAFORMAT_PCM; ss=3; break;
     case '4': t=DATAFORMAT_PCM; ss=4; break;
     case 's': t=DATAFORMAT_FLOAT; ss=sizeof(float); break;
     case 'd': t=DATAFORMAT_FLOAT; ss=sizeof(double); break;
     default: return FALSE;
     }     
     if (t == DATAFORMAT_PCM) {
	  c = g_strdup_printf("%s_Signed",ini_prefix);
	  sign=inifile_get_gboolean(c,FALSE);
	  g_free(c);
	  c = g_strdup_printf("%s_BigEndian",ini_prefix);
	  end=inifile_get_gboolean(c,FALSE);
	  g_free(c);
     }
     result->type = t;
     result->samplesize = ss;
     result->sign = sign;
     result->bigendian = end;
     if (full) {
	  c = g_strdup_printf("%s_SampleRate",ini_prefix);
	  sr = inifile_get_guint32(c,44100);
	  g_free(c);
	  c = g_strdup_printf("%s_Channels",ini_prefix);
	  chn = inifile_get_guint32(c,2);
	  g_free(c);
	  result->samplerate = sr;
	  result->channels = chn;
     }
     result->samplebytes = result->channels * result->samplesize;
     return TRUE;
}

void dataformat_save_to_inifile(gchar *ini_prefix, Dataformat *format, 
				gboolean full)
{
     char *s[4] = { "1","2","3","4" };
     gchar *c;
     c = g_strdup_printf("%s_SampleSize",ini_prefix);
     if (format->type == DATAFORMAT_PCM) {
	  inifile_set(c,s[format->samplesize-1]);
	  g_free(c);
	  c = g_strdup_printf("%s_Signed",ini_prefix);	  
	  inifile_set_gboolean(c,format->sign);
	  g_free(c);
	  c = g_strdup_printf("%s_BigEndian",ini_prefix);
	  inifile_set_gboolean(c,format->bigendian);	  
     } else if (format->samplesize == sizeof(float)) {
	  inifile_set(c,"s");
     } else {
	  inifile_set(c,"d");
     }
     g_free(c);
     if (full) {
	  c = g_strdup_printf("%s_SampleRate",ini_prefix);
	  inifile_set_guint32(c,format->samplerate);
	  g_free(c);
	  c = g_strdup_printf("%s_Channels",ini_prefix);
	  inifile_set_guint32(c,format->channels);
	  g_free(c);
     }
}



/* PCM<->FLOAT SAMPLE CONVERSION ROUTINES */
/* These routines could really use some optimizing if anyone feels like it. */

/* 1) single-float, max-range */

#define FTYPE float

#ifdef HAVE_LRINTF
#define RINT(x) lrintf(x)
#else 
#define RINT(x) ((long int)((x<0)?(x-0.5000001):(x+0.5000001)))
#endif

#define C_PCM8S_FLOAT convert_pcm8s_float
#define C_PCM8U_FLOAT convert_pcm8u_float
#define C_PCM16SLE_FLOAT convert_pcm16sle_float
#define C_PCM16SBE_FLOAT convert_pcm16sbe_float
#define C_PCM16ULE_FLOAT convert_pcm16ule_float
#define C_PCM16UBE_FLOAT convert_pcm16ube_float
#define C_PCM24SLE_FLOAT convert_pcm24sle_float
#define C_PCM24SBE_FLOAT convert_pcm24sbe_float
#define C_PCM24ULE_FLOAT convert_pcm24ule_float
#define C_PCM24UBE_FLOAT convert_pcm24ube_float
#define C_PCM32SLE_FLOAT convert_pcm32sle_float
#define C_PCM32SBE_FLOAT convert_pcm32sbe_float
#define C_PCM32ULE_FLOAT convert_pcm32ule_float
#define C_PCM32UBE_FLOAT convert_pcm32ube_float
#define C_FLOAT_PCM8S convert_float_pcm8s
#define C_FLOAT_PCM8U convert_float_pcm8u
#define C_FLOAT_PCM16SLE convert_float_pcm16sle
#define C_FLOAT_PCM16SBE convert_float_pcm16sbe
#define C_FLOAT_PCM16ULE convert_float_pcm16ule
#define C_FLOAT_PCM16UBE convert_float_pcm16ube
#define C_FLOAT_PCM24SLE convert_float_pcm24sle
#define C_FLOAT_PCM24SBE convert_float_pcm24sbe
#define C_FLOAT_PCM24ULE convert_float_pcm24ule
#define C_FLOAT_PCM24UBE convert_float_pcm24ube
#define C_FLOAT_PCM32SLE convert_float_pcm32sle
#define C_FLOAT_PCM32SBE convert_float_pcm32sbe
#define C_FLOAT_PCM32ULE convert_float_pcm32ule
#define C_FLOAT_PCM32UBE convert_float_pcm32ube

#include "convert_inc.c"

/* 2) double-float, max-range */

#define FTYPE double

#ifdef HAVE_LRINT
#define RINT(x) lrint(x)
#else 
#define RINT(x) ((long int)((x<0)?(x-0.5000001):(x+0.5000001)))
#endif

#define C_PCM8S_FLOAT convert_pcm8s_double
#define C_PCM8U_FLOAT convert_pcm8u_double
#define C_PCM16SLE_FLOAT convert_pcm16sle_double
#define C_PCM16SBE_FLOAT convert_pcm16sbe_double
#define C_PCM16ULE_FLOAT convert_pcm16ule_double
#define C_PCM16UBE_FLOAT convert_pcm16ube_double
#define C_PCM24SLE_FLOAT convert_pcm24sle_double
#define C_PCM24SBE_FLOAT convert_pcm24sbe_double
#define C_PCM24ULE_FLOAT convert_pcm24ule_double
#define C_PCM24UBE_FLOAT convert_pcm24ube_double
#define C_PCM32SLE_FLOAT convert_pcm32sle_double
#define C_PCM32SBE_FLOAT convert_pcm32sbe_double
#define C_PCM32ULE_FLOAT convert_pcm32ule_double
#define C_PCM32UBE_FLOAT convert_pcm32ube_double
#define C_FLOAT_PCM8S convert_double_pcm8s
#define C_FLOAT_PCM8U convert_double_pcm8u
#define C_FLOAT_PCM16SLE convert_double_pcm16sle
#define C_FLOAT_PCM16SBE convert_double_pcm16sbe
#define C_FLOAT_PCM16ULE convert_double_pcm16ule
#define C_FLOAT_PCM16UBE convert_double_pcm16ube
#define C_FLOAT_PCM24SLE convert_double_pcm24sle
#define C_FLOAT_PCM24SBE convert_double_pcm24sbe
#define C_FLOAT_PCM24ULE convert_double_pcm24ule
#define C_FLOAT_PCM24UBE convert_double_pcm24ube
#define C_FLOAT_PCM32SLE convert_double_pcm32sle
#define C_FLOAT_PCM32SBE convert_double_pcm32sbe
#define C_FLOAT_PCM32ULE convert_double_pcm32ule
#define C_FLOAT_PCM32UBE convert_double_pcm32ube

#include "convert_inc.c"

/* 3) single-float, preserve-zero */

#define PZMODE
#define FTYPE float

#ifdef HAVE_LRINTF
#define RINT(x) lrintf(x)
#else
#define RINT(x) ((long int)((x<0)?(x-0.5000001):(x+0.5000001)))
#endif

#define C_PCM8S_FLOAT convert_pcm8s_float_pz
#define C_PCM8U_FLOAT convert_pcm8u_float_pz
#define C_PCM16SLE_FLOAT convert_pcm16sle_float_pz
#define C_PCM16SBE_FLOAT convert_pcm16sbe_float_pz
#define C_PCM16ULE_FLOAT convert_pcm16ule_float_pz
#define C_PCM16UBE_FLOAT convert_pcm16ube_float_pz
#define C_PCM24SLE_FLOAT convert_pcm24sle_float_pz
#define C_PCM24SBE_FLOAT convert_pcm24sbe_float_pz
#define C_PCM24ULE_FLOAT convert_pcm24ule_float_pz
#define C_PCM24UBE_FLOAT convert_pcm24ube_float_pz
#define C_PCM32SLE_FLOAT convert_pcm32sle_float_pz
#define C_PCM32SBE_FLOAT convert_pcm32sbe_float_pz
#define C_PCM32ULE_FLOAT convert_pcm32ule_float_pz
#define C_PCM32UBE_FLOAT convert_pcm32ube_float_pz
#define C_FLOAT_PCM8S convert_float_pcm8s_pz
#define C_FLOAT_PCM8U convert_float_pcm8u_pz
#define C_FLOAT_PCM16SLE convert_float_pcm16sle_pz
#define C_FLOAT_PCM16SBE convert_float_pcm16sbe_pz
#define C_FLOAT_PCM16ULE convert_float_pcm16ule_pz
#define C_FLOAT_PCM16UBE convert_float_pcm16ube_pz
#define C_FLOAT_PCM24SLE convert_float_pcm24sle_pz
#define C_FLOAT_PCM24SBE convert_float_pcm24sbe_pz
#define C_FLOAT_PCM24ULE convert_float_pcm24ule_pz
#define C_FLOAT_PCM24UBE convert_float_pcm24ube_pz
#define C_FLOAT_PCM32SLE convert_float_pcm32sle_pz
#define C_FLOAT_PCM32SBE convert_float_pcm32sbe_pz
#define C_FLOAT_PCM32ULE convert_float_pcm32ule_pz
#define C_FLOAT_PCM32UBE convert_float_pcm32ube_pz

#include "convert_inc.c"

/* 4) double-float, preserve-zero */

#define PZMODE
#define FTYPE double

#ifdef HAVE_LRINT
#define RINT(x) lrint(x)
#else
#define RINT(x) ((long int)((x<0)?(x-0.5000001):(x+0.5000001)))
#endif

#define C_PCM8S_FLOAT convert_pcm8s_double_pz
#define C_PCM8U_FLOAT convert_pcm8u_double_pz
#define C_PCM16SLE_FLOAT convert_pcm16sle_double_pz
#define C_PCM16SBE_FLOAT convert_pcm16sbe_double_pz
#define C_PCM16ULE_FLOAT convert_pcm16ule_double_pz
#define C_PCM16UBE_FLOAT convert_pcm16ube_double_pz
#define C_PCM24SLE_FLOAT convert_pcm24sle_double_pz
#define C_PCM24SBE_FLOAT convert_pcm24sbe_double_pz
#define C_PCM24ULE_FLOAT convert_pcm24ule_double_pz
#define C_PCM24UBE_FLOAT convert_pcm24ube_double_pz
#define C_PCM32SLE_FLOAT convert_pcm32sle_double_pz
#define C_PCM32SBE_FLOAT convert_pcm32sbe_double_pz
#define C_PCM32ULE_FLOAT convert_pcm32ule_double_pz
#define C_PCM32UBE_FLOAT convert_pcm32ube_double_pz
#define C_FLOAT_PCM8S convert_double_pcm8s_pz
#define C_FLOAT_PCM8U convert_double_pcm8u_pz
#define C_FLOAT_PCM16SLE convert_double_pcm16sle_pz
#define C_FLOAT_PCM16SBE convert_double_pcm16sbe_pz
#define C_FLOAT_PCM16ULE convert_double_pcm16ule_pz
#define C_FLOAT_PCM16UBE convert_double_pcm16ube_pz
#define C_FLOAT_PCM24SLE convert_double_pcm24sle_pz
#define C_FLOAT_PCM24SBE convert_double_pcm24sbe_pz
#define C_FLOAT_PCM24ULE convert_double_pcm24ule_pz
#define C_FLOAT_PCM24UBE convert_double_pcm24ube_pz
#define C_FLOAT_PCM32SLE convert_double_pcm32sle_pz
#define C_FLOAT_PCM32SBE convert_double_pcm32sbe_pz
#define C_FLOAT_PCM32ULE convert_double_pcm32ule_pz
#define C_FLOAT_PCM32UBE convert_double_pcm32ube_pz

#include "convert_inc.c"

typedef void (*convert_function)(void *in, void *out, guint count);

/* (PZ-mode) (PCM size) (PCM sign) (PCM endian) (FP isdouble) */
static convert_function pcm_fp_functions[] = {
     (convert_function)convert_pcm8u_float,
     (convert_function)convert_pcm8u_double,
     (convert_function)convert_pcm8u_float,
     (convert_function)convert_pcm8u_double,
     (convert_function)convert_pcm8s_float,
     (convert_function)convert_pcm8s_double,
     (convert_function)convert_pcm8s_float,
     (convert_function)convert_pcm8s_double,
     (convert_function)convert_pcm16ule_float,
     (convert_function)convert_pcm16ule_double,
     (convert_function)convert_pcm16ube_float,
     (convert_function)convert_pcm16ube_double,
     (convert_function)convert_pcm16sle_float,
     (convert_function)convert_pcm16sle_double,
     (convert_function)convert_pcm16sbe_float,
     (convert_function)convert_pcm16sbe_double,
     (convert_function)convert_pcm24ule_float,
     (convert_function)convert_pcm24ule_double,
     (convert_function)convert_pcm24ube_float,
     (convert_function)convert_pcm24ube_double,
     (convert_function)convert_pcm24sle_float,
     (convert_function)convert_pcm24sle_double,
     (convert_function)convert_pcm24sbe_float,
     (convert_function)convert_pcm24sbe_double,
     (convert_function)convert_pcm32ule_float,
     (convert_function)convert_pcm32ule_double,
     (convert_function)convert_pcm32ube_float,
     (convert_function)convert_pcm32ube_double,
     (convert_function)convert_pcm32sle_float,
     (convert_function)convert_pcm32sle_double,
     (convert_function)convert_pcm32sbe_float,
     (convert_function)convert_pcm32sbe_double,
     (convert_function)convert_pcm8u_float_pz,
     (convert_function)convert_pcm8u_double_pz,
     (convert_function)convert_pcm8u_float_pz,
     (convert_function)convert_pcm8u_double_pz,
     (convert_function)convert_pcm8s_float_pz,
     (convert_function)convert_pcm8s_double_pz,
     (convert_function)convert_pcm8s_float_pz,
     (convert_function)convert_pcm8s_double_pz,
     (convert_function)convert_pcm16ule_float_pz,
     (convert_function)convert_pcm16ule_double_pz,
     (convert_function)convert_pcm16ube_float_pz,
     (convert_function)convert_pcm16ube_double_pz,
     (convert_function)convert_pcm16sle_float_pz,
     (convert_function)convert_pcm16sle_double_pz,
     (convert_function)convert_pcm16sbe_float_pz,
     (convert_function)convert_pcm16sbe_double_pz,
     (convert_function)convert_pcm24ule_float_pz,
     (convert_function)convert_pcm24ule_double_pz,
     (convert_function)convert_pcm24ube_float_pz,
     (convert_function)convert_pcm24ube_double_pz,
     (convert_function)convert_pcm24sle_float_pz,
     (convert_function)convert_pcm24sle_double_pz,
     (convert_function)convert_pcm24sbe_float_pz,
     (convert_function)convert_pcm24sbe_double_pz,
     (convert_function)convert_pcm32ule_float_pz,
     (convert_function)convert_pcm32ule_double_pz,
     (convert_function)convert_pcm32ube_float_pz,
     (convert_function)convert_pcm32ube_double_pz,
     (convert_function)convert_pcm32sle_float_pz,
     (convert_function)convert_pcm32sle_double_pz,
     (convert_function)convert_pcm32sbe_float_pz,
     (convert_function)convert_pcm32sbe_double_pz
};

/* (PZ-mode) (PCM size) (PCM sign) (PCM endian) (FP isdouble) */
static convert_function fp_pcm_functions[] = {
     (convert_function)convert_float_pcm8u,
     (convert_function)convert_double_pcm8u,
     (convert_function)convert_float_pcm8u,
     (convert_function)convert_double_pcm8u,
     (convert_function)convert_float_pcm8s,
     (convert_function)convert_double_pcm8s,
     (convert_function)convert_float_pcm8s,
     (convert_function)convert_double_pcm8s,
     (convert_function)convert_float_pcm16ule,
     (convert_function)convert_double_pcm16ule,
     (convert_function)convert_float_pcm16ube,
     (convert_function)convert_double_pcm16ube,
     (convert_function)convert_float_pcm16sle,
     (convert_function)convert_double_pcm16sle,
     (convert_function)convert_float_pcm16sbe,
     (convert_function)convert_double_pcm16sbe,
     (convert_function)convert_float_pcm24ule,
     (convert_function)convert_double_pcm24ule,
     (convert_function)convert_float_pcm24ube,
     (convert_function)convert_double_pcm24ube,
     (convert_function)convert_float_pcm24sle,
     (convert_function)convert_double_pcm24sle,
     (convert_function)convert_float_pcm24sbe,
     (convert_function)convert_double_pcm24sbe,
     (convert_function)convert_float_pcm32ule,
     (convert_function)convert_double_pcm32ule,
     (convert_function)convert_float_pcm32ube,
     (convert_function)convert_double_pcm32ube,
     (convert_function)convert_float_pcm32sle,
     (convert_function)convert_double_pcm32sle,
     (convert_function)convert_float_pcm32sbe,
     (convert_function)convert_double_pcm32sbe,
     (convert_function)convert_float_pcm8u_pz,
     (convert_function)convert_double_pcm8u_pz,
     (convert_function)convert_float_pcm8u_pz,
     (convert_function)convert_double_pcm8u_pz,
     (convert_function)convert_float_pcm8s_pz,
     (convert_function)convert_double_pcm8s_pz,
     (convert_function)convert_float_pcm8s_pz,
     (convert_function)convert_double_pcm8s_pz,
     (convert_function)convert_float_pcm16ule_pz,
     (convert_function)convert_double_pcm16ule_pz,
     (convert_function)convert_float_pcm16ube_pz,
     (convert_function)convert_double_pcm16ube_pz,
     (convert_function)convert_float_pcm16sle_pz,
     (convert_function)convert_double_pcm16sle_pz,
     (convert_function)convert_float_pcm16sbe_pz,
     (convert_function)convert_double_pcm16sbe_pz,
     (convert_function)convert_float_pcm24ule_pz,
     (convert_function)convert_double_pcm24ule_pz,
     (convert_function)convert_float_pcm24ube_pz,
     (convert_function)convert_double_pcm24ube_pz,
     (convert_function)convert_float_pcm24sle_pz,
     (convert_function)convert_double_pcm24sle_pz,
     (convert_function)convert_float_pcm24sbe_pz,
     (convert_function)convert_double_pcm24sbe_pz,
     (convert_function)convert_float_pcm32ule_pz,
     (convert_function)convert_double_pcm32ule_pz,
     (convert_function)convert_float_pcm32ube_pz,
     (convert_function)convert_double_pcm32ube_pz,
     (convert_function)convert_float_pcm32sle_pz,
     (convert_function)convert_double_pcm32sle_pz,
     (convert_function)convert_float_pcm32sbe_pz,
     (convert_function)convert_double_pcm32sbe_pz
};

static void dither_convert_float(float *indata, char *outdata, int count,
				 convert_function fn, int outdata_ssize)
{
     float amp_factor;
     float databuf[4096];
     int i,j;
     amp_factor = powf(2.0f,(float)(1-outdata_ssize*8));
     while (count > 0) {
	  i = MIN(count,ARRAY_LENGTH(databuf));
	  memcpy(databuf,indata,i*sizeof(float));
	  for (j=0; j<i; j++)
	       databuf[j] += (((float)(rand()/2 + rand()/2))/
			      ((float)RAND_MAX)) * amp_factor;
	  fn(databuf,outdata,i);
	  indata += i;
	  outdata += outdata_ssize * i;
	  count -= i;
     }
}

static void dither_convert_double(double *indata, char *outdata, int count,
				  convert_function fn, int outdata_ssize)
{
     double amp_factor;
     double databuf[4096];
     int i,j;
     amp_factor = pow(2.0,(double)(1-outdata_ssize*8));
     while (count > 0) {
	  i = MIN(count,ARRAY_LENGTH(databuf));
	  memcpy(databuf,indata,i*sizeof(double));
	  for (j=0; j<i; j++)
	       databuf[j] += (((double)(rand()/2 + rand()/2))/
			      ((double)RAND_MAX)) * amp_factor;
	  fn(databuf,outdata,i);
	  indata += i;
	  outdata += outdata_ssize * i;
	  count -= i;
     }
}

sample_t minimum_float_value(Dataformat *x)
{
     static const sample_t tbl[4] = {
	  (-128.0/127.0), (-32768.0/32767.0),
	  (-8388608.0/8388607.0), (-2147483648.0/2147483647.0) };

     if (sample_convert_mode==0 || x->type!=DATAFORMAT_PCM)
	  return -1.0;
     else
	  return tbl[x->samplesize - 1];
}

sample_t convert_factor(Dataformat *infmt, Dataformat *outfmt)
{
     sample_t fpos, fneg;
     fneg = minimum_float_value(outfmt) / minimum_float_value(infmt);
     return fneg;
}

sample_t apply_convert_factor(Dataformat *infmt, Dataformat *outfmt,
			      sample_t *buffer, guint count)
{
     sample_t f;
     guint i;
     if (sample_convert_mode == 0 || infmt->type!=DATAFORMAT_PCM ||
	 outfmt->type!=DATAFORMAT_PCM)
	  return;
     f = convert_factor(infmt,outfmt);
     for (i=0; i<count; i++)
	  buffer[i] *= f;
}

void convert_array(void *indata, Dataformat *indata_format,
		   void *outdata, Dataformat *outdata_format,
		   guint count, int dither_mode)
{     
     int i;
     char *c;
     if (dataformat_samples_equal(indata_format,outdata_format)) {
	  memcpy(outdata,indata,count*indata_format->samplesize);
     } else if (indata_format->type == DATAFORMAT_PCM) {
	  if (outdata_format->type == DATAFORMAT_PCM) {
	       /* PCM -> PCM conversion */
	       if (outdata_format->samplesize > indata_format->samplesize)
		    dither_mode = DITHER_NONE;
	       c = g_malloc(count * sizeof(sample_t));	       
	       convert_array(indata,indata_format,c,&dataformat_sample_t,
			     count,dither_mode);
	       apply_convert_factor(indata_format,outdata_format,
				    (sample_t *)c, count);
	       convert_array(c,&dataformat_sample_t,outdata,outdata_format,
			     count,dither_mode);
	       g_free(c);
	  } else {
	       /* PCM -> FP conversion */
	       i = (sample_convert_mode ? 32:0) +
		    (indata_format->samplesize-1)*8 +
		    (indata_format->sign?4:0) +
		    (indata_format->bigendian?2:0) +
		    (outdata_format->samplesize/sizeof(double));
	       /* printf("convert_array: i=%d\n",i); */
	       g_assert(i<ARRAY_LENGTH(pcm_fp_functions));
	       pcm_fp_functions[i](indata,outdata,count);
	  }
     } else if (outdata_format->type == DATAFORMAT_PCM) {
	  /* FP -> PCM conversion */
	  i = (sample_convert_mode ? 32:0) +
	       (outdata_format->samplesize-1)*8 +
	       (outdata_format->sign?4:0) +
	       (outdata_format->bigendian?2:0) +
	       (indata_format->samplesize/sizeof(double));
	  g_assert(i < ARRAY_LENGTH(fp_pcm_functions));
	  g_assert(dither_mode != DITHER_UNSPEC);
	  if (dither_mode != DITHER_NONE) {
	       if (indata_format->samplesize == sizeof(float))
		    dither_convert_float(indata,outdata,count,
					 fp_pcm_functions[i],
					 outdata_format->samplesize);
	       else
		    dither_convert_double(indata,outdata,count,
					  fp_pcm_functions[i],
					  outdata_format->samplesize);
	  } else
	       fp_pcm_functions[i](indata,outdata,count);
     } else {
	  /* FP -> FP conversion */
	  if (indata_format->samplesize == sizeof(float)) {
	       float *f = indata;
	       double *d = outdata;
	       for (; count>0; count--,f++,d++)
		    *d = (double)(*f);
	  } else {
	       double *d = indata;
	       float *f = outdata;
	       for (; count>0; count--,f++,d++)
		    *f = (float)(*d);
	  }
     }
}

gint unnormalized_count(sample_t *buf, gint count, Dataformat *target)
{
     gint i,c=0;
     sample_t maxval,minval;
     maxval = maximum_float_value(target);
     minval = minimum_float_value(target);
     for (i=0; i<count; i++)
	  if (buf[i] > maxval || buf[i] < minval)
	       c++;
     return c;
}

static void print_format(Dataformat *fmt)
{
     if (fmt->type == DATAFORMAT_FLOAT) {
	  if (fmt->samplesize == sizeof(float))
	       puts(_("Floating-point (single)"));
	  else
	       puts(_("Floating-point (double)"));
     } else {
	  printf(_("PCM, %d bit, %s %s\n"),fmt->samplesize*8,
		 fmt->sign?_("Signed"):_("Unsigned"),
		 fmt->bigendian?_("Big-endian"):_("Little-endian"));
     }
}

#define SBUFLEN 32
void conversion_selftest(void)
{    
     guint samplesizes[] = { 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 
			     sizeof(float), sizeof(double) };
     gboolean signs[] = { FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE,
			  FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE,
			  FALSE, FALSE };
     gboolean endians[] = { FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE,
			    FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE,
			    FALSE, FALSE };
     gint types[] = { DATAFORMAT_PCM, DATAFORMAT_PCM, DATAFORMAT_PCM, 
		      DATAFORMAT_PCM, DATAFORMAT_PCM, DATAFORMAT_PCM,
		      DATAFORMAT_PCM, DATAFORMAT_PCM, DATAFORMAT_PCM,
		      DATAFORMAT_PCM, DATAFORMAT_PCM, DATAFORMAT_PCM,
		      DATAFORMAT_PCM, DATAFORMAT_PCM, DATAFORMAT_PCM,
		      DATAFORMAT_PCM, DATAFORMAT_FLOAT, DATAFORMAT_FLOAT }; 
     guchar pcm_buf[SBUFLEN*8],pcm_buf2[SBUFLEN*8],pcm_buf3[SBUFLEN*8];
     sample_t sbuf[SBUFLEN],sbuf2[SBUFLEN];
     guint i,j;
     Dataformat fmt[2];
     gboolean expect_fail, err=FALSE;

#if 0
     fmt[0].type = DATAFORMAT_PCM;
     fmt[0].samplesize = 1;
     fmt[0].sign = FALSE;
     fmt[0].bigendian = TRUE;
     for (i=0; i<256; i++) pcm_buf[i] = i;	  
     convert_array(pcm_buf,fmt,sbuf,&dataformat_sample_t,256);
     convert_array(sbuf,&dataformat_sample_t,pcm_buf2,fmt,256);
     for (i=0; i<256; i++)
	  printf("%d -> %f -> %d\n",pcm_buf[i],sbuf[i],pcm_buf2[i]);
     return;
#endif

     /* Generate full range in sample_t buffer */
     for (j=0; j<SBUFLEN; j++)
	  sbuf[j] = 2.0*(sample_t)j/(sample_t)(SBUFLEN-1) - 1.0;

     /* puts(""); */
     
     /* Perform tests */
     puts(_("Testing ranges..."));
     for (i=0; i<ARRAY_LENGTH(samplesizes); i++) {
	  fmt[0].type = types[i];
	  fmt[0].samplesize = samplesizes[i];
	  fmt[0].sign = signs[i];
	  fmt[0].bigendian = endians[i];
	  convert_array(sbuf,&dataformat_sample_t,pcm_buf2,fmt,
			ARRAY_LENGTH(sbuf),DITHER_NONE);
	  convert_array(pcm_buf2,fmt,sbuf2,&dataformat_sample_t,
			ARRAY_LENGTH(sbuf),DITHER_NONE);
	  convert_array(sbuf2,&dataformat_sample_t,pcm_buf3,fmt,
			ARRAY_LENGTH(sbuf),DITHER_NONE);

	  if (fabs(sbuf2[0] - -1.0) > 0.000001 || 
	      fabs(sbuf2[SBUFLEN-1] - 1.0) > 0.000001 ||
	      memcmp(pcm_buf2,pcm_buf3,SBUFLEN*fmt[0].samplesize)) {
	       fputs(_("Range test failed for format: "),stdout);
	       print_format(fmt);
	       printf("   %f -> %f, %f -> %f\n",sbuf[0],sbuf2[0],
		      sbuf[SBUFLEN-1],sbuf2[SBUFLEN-1]);
	       err = TRUE;
	  }
     }

     puts(_("Testing all conversions.."));

     /* Generate random numbers in sbuf vector */
     for (i=0; i<ARRAY_LENGTH(sbuf); i++)
	  sbuf[i] = 2.0 * (float)rand() / (float)RAND_MAX - 1.0;
     

     for (i=0; i<ARRAY_LENGTH(samplesizes); i++) {
	  fmt[0].type = types[i];
	  fmt[0].samplesize = samplesizes[i];
	  fmt[0].sign = signs[i];
	  fmt[0].bigendian = endians[i];
	  convert_array(sbuf,&dataformat_sample_t,pcm_buf,fmt,
		       ARRAY_LENGTH(sbuf),DITHER_NONE);
	  for (j=0; j<ARRAY_LENGTH(samplesizes); j++) {
	       fmt[1].type = types[j];
	       fmt[1].samplesize = samplesizes[j];
	       fmt[1].sign = signs[j];
	       fmt[1].bigendian = endians[j];
	       if ((fmt[0].type == DATAFORMAT_PCM && 
		    fmt[1].type == DATAFORMAT_FLOAT && 
		    fmt[0].samplesize == 4 && fmt[1].samplesize == 4) || 
		   fmt[0].samplesize > fmt[1].samplesize)
		    expect_fail = TRUE;
	       else
		    expect_fail = FALSE;
	       if (expect_fail) continue;
	       convert_array(pcm_buf,fmt,pcm_buf2,fmt+1,ARRAY_LENGTH(sbuf),
			     DITHER_NONE);
	       convert_array(pcm_buf2,fmt+1,pcm_buf3,fmt,ARRAY_LENGTH(sbuf),
			     DITHER_NONE);
	       if (memcmp(pcm_buf,pcm_buf3,
			  ARRAY_LENGTH(sbuf)*fmt[0].samplesize)) {
		    if (expect_fail) fputs(_("(expected) "),stdout);
		    fputs(_("Conversion test failed, between: "),stdout);
		    print_format(fmt);
		    fputs(_("  and: "),stdout);
		    print_format(fmt+1);
		    err = TRUE;
	       }
	  }
	  
     }

     if (!err) puts(_("No errors detected!"));
#undef SBUFLEN     
}

void conversion_performance_test(void)
{
#define SBUFLEN 10000
#define FORMATS 16
     guint samplesizes[] = { 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 
			     sizeof(float), sizeof(double) };
     gboolean signs[] = { FALSE, TRUE, FALSE, TRUE, FALSE, TRUE,
			  FALSE, TRUE, FALSE, TRUE, FALSE, TRUE, FALSE, TRUE,
			  FALSE, FALSE };
     gboolean endians[] = { FALSE, FALSE, FALSE, FALSE, TRUE, TRUE,
			    FALSE, FALSE, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE,
			    FALSE, FALSE };
     gint types[] = { DATAFORMAT_PCM, 
		      DATAFORMAT_PCM, DATAFORMAT_PCM, DATAFORMAT_PCM,
		      DATAFORMAT_PCM, DATAFORMAT_PCM, DATAFORMAT_PCM,
		      DATAFORMAT_PCM, DATAFORMAT_PCM, DATAFORMAT_PCM,
		      DATAFORMAT_PCM, DATAFORMAT_PCM, DATAFORMAT_PCM,
		      DATAFORMAT_PCM, DATAFORMAT_FLOAT, DATAFORMAT_FLOAT }; 
     gchar *strings[] = { "8U", "8S",  
			  "16U_LE", "16S_LE", "16U_BE", "16S_BE",
			  "24U_LE", "24S_LE", "24U_BE", "24S_BE",
			  "32U_LE", "32S_LE", "32U_BE", "32S_BE",
			  "FP_s", "FP_d" };
     GTimeVal start_time, end_time, test_times[FORMATS*FORMATS];
     sample_t *sbuf;
     gpointer buf,buf2;
     guint i,j;
     gfloat f,g;
     Dataformat fmt[2];

     puts(_("Preparing tests.."));
     sbuf = g_malloc(SBUFLEN * sizeof(*sbuf));
     buf = g_malloc(SBUFLEN * 8);
     buf2 = g_malloc(SBUFLEN * 8);

     for (i=0; i<SBUFLEN; i++)
	  sbuf[i] = 2.0 * (float)rand() / (float)RAND_MAX - 1.0;
     
     fputs(_("Running tests.."),stdout);
     fflush(stdout);
     for (i=0; i<FORMATS; i++) {
	  fmt[0].type = types[i];
	  fmt[0].samplesize = samplesizes[i];
	  fmt[0].sign = signs[i];
	  fmt[0].bigendian = endians[i];
	  convert_array(sbuf,&dataformat_sample_t,buf,fmt,SBUFLEN,DITHER_NONE);
	  for (j=0; j<FORMATS; j++) {
	       fmt[1].type = types[j];
	       fmt[1].samplesize = samplesizes[j];
	       fmt[1].sign = signs[j];
	       fmt[1].bigendian = endians[j];
	       fputs(".",stdout);
	       fflush(stdout);
	       convert_array(buf,fmt,buf2,fmt+1,SBUFLEN,DITHER_NONE);
	       g_get_current_time(&start_time);
	       convert_array(buf,fmt,buf2,fmt+1,SBUFLEN,DITHER_NONE);
	       g_get_current_time(&end_time);
	       timeval_subtract(&test_times[i*FORMATS+j],&end_time,
				&start_time);
	  }
     }

     /* Find out which one took the longest and take that time / 100, then
      * "round down" to get the index */
     i = 0;
     for (j=0; j<FORMATS*FORMATS; j++) {
	  if (test_times[j].tv_sec > test_times[i].tv_sec ||
	      (test_times[j].tv_sec == test_times[i].tv_sec &&
	       test_times[j].tv_usec > test_times[i].tv_usec))
	       i = j;
     }    
     f = (float)test_times[i].tv_sec + 
	  ((float)test_times[i].tv_usec) * 0.000001;
     f /= 100;
     i = 0;
     while (f < 1.0) { f*=10.0; i++; }
     f = 1.0;
     while (i > 0) { f/=10.0; i--; }
     printf(_("\n\nTest results (1 time unit = %f usec/sample)\n"),
	    f*1000000/SBUFLEN);
     printf("       ");
     for (i=0; i<FORMATS; i++)
	  printf("%6s ",strings[i]);
     for (i=0; i<FORMATS; i++) {
	  printf("\n%6s ",strings[i]);
	  for (j=0; j<FORMATS; j++) {
	       g = (float)test_times[i*FORMATS+j].tv_sec + 
		    ((float)test_times[i*FORMATS+j].tv_usec) * 0.000001;
	       g /= f;
	       printf("%6.2f ",g);
	       
	  }
     }
     puts("\n");
}
