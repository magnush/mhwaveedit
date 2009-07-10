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


#include <config.h>

#include <math.h>

#include "main.h"
#include "vu_meter.h"

static void vu_meter_size_request(GtkWidget *widget, GtkRequisition *req)
{
     req->width = 80;
     req->height = 60;
}

static void draw_meter_line(VuMeter *v, GdkGC *gc)
{
     guint w,h;
     gfloat ang = (0.75-0.5*v->value)*3.1415;
     gfloat len;
     w = GTK_WIDGET(v)->allocation.width;
     h = GTK_WIDGET(v)->allocation.height;
     len = ((gfloat)h)*0.8;
     gdk_draw_line(GTK_WIDGET(v)->window,gc,w/2,h,
		   w/2+(gint)(len*cos(ang)),h-(gint)(len*sin(ang)));     
}

static void draw_black_arc(VuMeter *v)
{
     GtkWidget *widget = GTK_WIDGET(v);
     gdk_draw_arc(widget->window,widget->style->black_gc,1,
		  widget->allocation.width/2-15,
		  widget->allocation.height-15,30,30,0,180*64);
}

static gint vu_meter_expose(GtkWidget *widget, GdkEventExpose *event)
{
     VuMeter *vu = VU_METER(widget);
     guint w,h;
     gfloat len;
     w = widget->allocation.width;
     h = widget->allocation.height;
     len = ((gfloat)h)*0.8;
     gdk_draw_rectangle(widget->window,widget->style->black_gc,0,0,0,w,h);
     gdk_draw_rectangle(widget->window,widget->style->black_gc,0,1,1,w-2,h-2);
     gdk_draw_rectangle(widget->window,widget->style->black_gc,0,2,2,w-4,h-4);
     /* gdk_draw_rectangle(widget->window,widget->style->bg_gc[0],1,3,3,w-6,h-6); */
     gdk_draw_arc(widget->window,widget->style->white_gc,1,w/2-len,h-len,
		  len*2,len*2,45*64,90*64);
     draw_black_arc(vu);
     draw_meter_line(vu,widget->style->black_gc);
     return TRUE;
}

static void vu_meter_class_init(GtkWidgetClass *klass)
{
     klass->size_request = vu_meter_size_request;
     klass->expose_event = vu_meter_expose;
}

static void vu_meter_init(VuMeter *v)
{
     v->value = 0.0;
     v->goal = 0.0;
}

GtkType vu_meter_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "VuMeter",
	       sizeof(VuMeter),
	       sizeof(VuMeterClass),
	       (GtkClassInitFunc) vu_meter_class_init,
	       (GtkObjectInitFunc) vu_meter_init
	  };
	  id = gtk_type_unique(gtk_drawing_area_get_type(),&info);
     }
     return id;
}

GtkWidget *vu_meter_new(gfloat value)
{
     VuMeter *v;
     v = gtk_type_new(vu_meter_get_type());
     v->value = value;
     v->goal = value;
     g_get_current_time(&(v->valuetime));
     return GTK_WIDGET(v);
}

static void vu_meter_update_value_real(VuMeter *v, GTimeVal *tv)
{
     GTimeVal diff;
     float f;
     timeval_subtract(&diff,tv,&(v->valuetime));
     f = ((float)diff.tv_usec) / 1000000.0 + ((float)diff.tv_sec);
     v->value = v->value + (v->goal - v->value)*(1-exp(-f*10.0));
     memcpy(&(v->valuetime),tv,sizeof(GTimeVal));
}

static void undraw_value(VuMeter *v)
{
     draw_meter_line(v,GTK_WIDGET(v)->style->white_gc);
     draw_black_arc(v);
}

static void draw_value(VuMeter *v)
{
     draw_meter_line(v,GTK_WIDGET(v)->style->black_gc);
}

void vu_meter_update(VuMeter *v)
{
     GTimeVal tv;
     undraw_value(v);
     g_get_current_time(&tv);
     vu_meter_update_value_real(v,&tv);
     draw_value(v);
}

void vu_meter_set_value(VuMeter *v, gfloat value)
{
     GTimeVal tv;
     undraw_value(v);
     g_get_current_time(&tv);
     vu_meter_update_value_real(v,&tv);
     v->goal = value;
     if (value > v->value) v->value = value;
     draw_value(v);
}

