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


#ifndef DATAFORMAT_H_INCLUDED
#define DATAFORMAT_H_INCLUDED

#include <gtk/gtk.h>

/* struct describing the sample format. */

#define DATAFORMAT_PCM   0 /* Linear PCM */
#define DATAFORMAT_FLOAT 1 /* Floating-point (float or double array) */

typedef struct {

     int type;                   /* DATAFORMAT_PCM or DATAFORMAT_FLOAT */

     guint32 samplerate;         /* Samples/sec */
     unsigned samplesize;           /* bytes (PCM: 1,2,3 or 4, 
				  * float: sizeof(gfloat) or sizeof(gdouble))*/
     int channels;              /* Number of channels (1-8) */
     int samplebytes;           /* should always be samplesize*channels */

     /* For DATAFORMAT_PCM only: */
     gboolean sign;              /* TRUE=signed, FALSE=unsigned */
     gboolean bigendian;         /* TRUE=big endian, FALSE=little endian */

} Dataformat;

extern Dataformat dataformat_sample_t;
extern Dataformat dataformat_single;

extern gboolean ieee_le_compatible;

#define FORMAT_IS_SAMPLET(fmtp) ((fmtp)->type == DATAFORMAT_FLOAT && (fmtp)->samplesize == sizeof(sample_t))

void floating_point_check(void);

/* Returns TRUE if the two formats are equal */

gboolean dataformat_equal(Dataformat *f1, Dataformat *f2);

/* Returns TRUE if the two formats are equal but doesn't care for sample rate
 * or channels */
gboolean dataformat_samples_equal(Dataformat *f1, Dataformat *f2);

/* Returns TRUE if there was a format stored. */
gboolean dataformat_get_from_inifile(gchar *ini_prefix, gboolean full, 
				     Dataformat *result);
void dataformat_save_to_inifile(gchar *ini_prefix, Dataformat *format, 
				gboolean full);


/* -------------------
 * CONVERSION ROUTINES 
 * ------------------- */

#ifdef USE_DOUBLE_SAMPLES

typedef gdouble sample_t;
#define sf_readf_sample_t sf_readf_double
#define sf_writef_sample_t sf_writef_double

#else

typedef gfloat sample_t;
#define sf_readf_sample_t sf_readf_float
#define sf_writef_sample_t sf_writef_float

#endif

#define DITHER_NONE       0
#define DITHER_TRIANGULAR 1
#define DITHER_MAX        1

/* These convert one or many samples between pcm formats and sample_t 
 * NOTE: These convert SINGLE-CHANNEL samples so you have to multiply the
 * count with the number of channels if you want to read full samples. 
 * NOTE 2: These routines assumes the data to be in host endian order.
 */

#define maximum_float_value(x) (1.0)

void convert_array(void *indata, Dataformat *indata_format,
		   void *outdata, Dataformat *outdata_format,
		   guint count, int dither_mode);

void conversion_selftest(void);
void conversion_performance_test(void);


#endif
