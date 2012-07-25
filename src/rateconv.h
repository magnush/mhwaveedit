/*
 * Copyright (C) 2004 2005 2006, Magnus Hjorth
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


/* Routines for converting sample rate, both realtime and block-based */

#ifndef RATECONV_H_INCLUDED
#define RATECONV_H_INCLUDED

#include "datasource.h"

int rateconv_driver_count(gboolean realtime);
const char *rateconv_driver_id(gboolean realtime, int index);
const char *rateconv_driver_name(gboolean realtime, int index);
int rateconv_driver_index(gboolean realtime, const gchar *driver_id);

typedef void rateconv;
/* Returns TRUE if the converter prefers in/out data in floating-point format,
 * for quality reasons. */
gboolean rateconv_prefers_float(const char *driver_id);
rateconv *rateconv_new(gboolean realtime, const char *driver_id, 
		       Dataformat *format, guint32 outrate, int dither_mode,
		       gboolean passthru);
/* data == NULL means no more data will be sent */
gint rateconv_write(rateconv *conv, void *data, guint bufsize);
gint rateconv_read(rateconv *conv, void *buffer, guint bufsize);
gboolean rateconv_hasdata(rateconv *conv);
void rateconv_destroy(rateconv *conv);

/* Only for realtime converters */
void rateconv_set_outrate(rateconv *conv, guint32 outrate); 

#endif
