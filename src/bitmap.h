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


/* Simple widget to display an xbm bitmap.
 * Supports two color modes (default is black color):
 *   Constant color - set using bitmap_set_color
 *   Themed - interpolated between the current theme's fg and bg colors
 *     Set using bitmap_set_fg (alpha=0.0 gives bg color, 1.0 gives fg color). 
 * In both modes the bitmap has transparent background.
 */

#ifndef BITMAP_H_INCLUDED
#define BITMAP_H_INCLUDED

#include <gtk/gtk.h>

#define BITMAP(obj) GTK_CHECK_CAST(obj,bitmap_get_type(),Bitmap)
#define BITMAP_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,bitmap_get_type(),BitmapClass)
#define IS_BITMAP(obj) GTK_CHECK_TYPE(obj,bitmap_get_type())

typedef struct {
     GtkWidget wid;
     GdkGC *gc;
     gboolean update_gc;
     GdkBitmap *bmp;
     gint bmpw, bmph;
     gint color_mode; /* 0=fixed, 1=between fg/bg */
     GdkColor clr;
     gint alpha; /* 0..256 */
} Bitmap;

typedef struct {
     GtkWidgetClass klass;
} BitmapClass;

GtkType bitmap_get_type(void);
GtkWidget *bitmap_new_from_data(unsigned char *data, int width, int height);
void bitmap_set_color(Bitmap *bmp, GdkColor *clr);
void bitmap_set_fg(Bitmap *bmp, gfloat alpha);

#endif
