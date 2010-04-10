/*
 * Copyright (C) 2010, Magnus Hjorth
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

#include "bitmap.h"
#include "main.h"

#include <string.h>

static GtkWidgetClass *parent_class;

static void bitmap_style_set(GtkWidget *widget, GtkStyle *previous_style)
{
     BITMAP(widget)->update_gc = TRUE;
     if (parent_class->style_set != NULL)
	  parent_class->style_set(widget,previous_style);
}

static gboolean bitmap_expose(GtkWidget *widget, GdkEventExpose *event)
{
     Bitmap *b = BITMAP(widget);
     GdkColor cb,cf,c;
     gint af,ab;
     gint allx,ally,allw,allh,ox,oy;
     g_assert(b->bmp != NULL);
     if (b->gc == NULL) {
	  b->gc = gdk_gc_new(widget->window);
	  b->update_gc = TRUE;
     }
     if (b->update_gc) {
	  if (b->color_mode == 0) {
	       gdk_gc_set_rgb_fg_color(b->gc, &b->clr);
	  } else {
	       cb = widget->style->bg[0];
	       cf = widget->style->fg[0];
	       af = b->alpha;
	       ab = 256-af;
	       c.pixel = 0;
	       c.red = (cf.red*af + cb.red*ab) >> 8;
	       c.green = (cf.green*af + cb.red*ab) >> 8;
	       c.blue = (cf.blue*af + cb.blue*ab) >> 8;
	       gdk_gc_set_rgb_fg_color(b->gc, &c);
	  }	  
	  gdk_gc_set_fill(b->gc, GDK_STIPPLED);
	  gdk_gc_set_stipple(b->gc, b->bmp);
	  b->update_gc = FALSE;
     }
     ox = allx = widget->allocation.x;
     oy = ally = widget->allocation.y;
     allw = widget->allocation.width;
     allh = widget->allocation.height;
     if (allw != b->bmpw) 
	  ox += (allw-b->bmpw) / 2;
     if (allh != b->bmph)
	  oy += (allh-b->bmph) / 2;

     gdk_gc_set_ts_origin(b->gc, ox, oy);
     
     gdk_draw_rectangle(widget->window, b->gc, TRUE, ox, oy, b->bmpw, b->bmph);

     if (parent_class->expose_event != NULL)
	  return parent_class->expose_event(widget,event);
     return FALSE;
}

static void bitmap_destroy(GtkObject *object)
{
     Bitmap *b = BITMAP(object);
     if (b->gc != NULL) {
	  gdk_gc_unref(b->gc);
	  b->gc = NULL;
     }
     if (b->bmp != NULL) {
	  gdk_bitmap_unref(b->bmp);
	  b->bmp = NULL;
     }
     if (GTK_OBJECT_CLASS(parent_class)->destroy != NULL)
	  GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

static void bitmap_class_init(GtkObjectClass *klass)
{
     GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
     GtkObjectClass *oc = GTK_OBJECT_CLASS(klass);
     parent_class = gtk_type_class(gtk_widget_get_type());
     wc->style_set = bitmap_style_set;
     wc->expose_event = bitmap_expose;
     oc->destroy = bitmap_destroy;
}

static void bitmap_init(GtkObject *obj)
{
     Bitmap *b = BITMAP(obj);

     gtk_widget_set_has_window(GTK_WIDGET(obj),FALSE);

     b->gc = NULL;
     b->bmp = NULL;
     b->color_mode = 0;
     memset(&(b->clr), 0, sizeof(b->clr));
     b->alpha = 1.0;
}

GtkType bitmap_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "Bitmap",
	       sizeof(Bitmap),
	       sizeof(BitmapClass),
	       (GtkClassInitFunc)bitmap_class_init,
	       (GtkObjectInitFunc)bitmap_init
	  };
	  id = gtk_type_unique(gtk_widget_get_type(),&info);
     }
     return id;
}

GtkWidget *bitmap_new_from_data(unsigned char *data, int width, int height)
{
     Bitmap *b;
     GtkWidget *w;
     gpointer p;
     p = gtk_type_new(bitmap_get_type());
     w = p;
     b = p;
     b->bmp = gdk_bitmap_create_from_data(NULL,(const gchar *)data,width,height);    
     w->requisition.width = width;
     w->requisition.height = height;
     b->bmpw = width;
     b->bmph = height;
     return GTK_WIDGET(b);
}

void bitmap_set_color(Bitmap *bmp, GdkColor *clr)
{
     memcpy(&bmp->clr, clr, sizeof(bmp->clr));
     bmp->color_mode = 0;
     gtk_widget_queue_draw(GTK_WIDGET(bmp));
}

void bitmap_set_fg(Bitmap *bmp, gfloat alpha)
{
     bmp->alpha = (gint)(alpha * 256.0);
     bmp->color_mode = 1;
     gtk_widget_queue_draw(GTK_WIDGET(bmp));
}
