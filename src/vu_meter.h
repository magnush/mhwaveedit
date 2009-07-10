/*
 * Copyright (C) 2002 2003 2004, Magnus Hjorth
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


#ifndef VU_METER_H_INCLUDED
#define VU_METER_H_INCLUDED

#include <gtk/gtk.h>

#define VU_METER(obj) GTK_CHECK_CAST(obj,vu_meter_get_type(),VuMeter)
#define VU_METER_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,vu_meter_get_type(),VuMeterClass)
#define IS_VU_METER(obj) GTK_CHECK_TYPE(obj,vu_meter_get_type())

typedef struct {
     GtkDrawingArea da;
     gfloat value; /* Between 0 and 1 */
     gfloat goal;
     GTimeVal valuetime;
} VuMeter;

typedef struct {
     GtkDrawingAreaClass dac;
} VuMeterClass;

GtkType vu_meter_get_type(void);
GtkWidget *vu_meter_new(gfloat value);
void vu_meter_set_value(VuMeter *v, gfloat value);
void vu_meter_update(VuMeter *v);

#endif
