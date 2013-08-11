/*
 * Copyright (C) 2002 2003 2004 2005 2006 2007 2008 2009 2010, Magnus Hjorth
 * Copyright (C) 2011 2012, Magnus Hjorth
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

/* This contains the main procedure and some other useful procedures and macros. */

#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#include <stdio.h>
#include <gtk/gtk.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <string.h>

/* GTK compatibility stuff */
#if GTK_MAJOR_VERSION == 1
#define GTK_CLASS_TYPE(klass) (((GtkObjectClass *)klass)->type)
#define gtk_style_get_font(style) ((style)->font)
#define GTK_WIN_POS_CENTER_ON_PARENT GTK_WIN_POS_CENTER
#define gtk_container_get_children gtk_container_children
#define gtk_bin_get_child(bin) ((bin)->child)
#define gtk_widget_set_size_request gtk_widget_set_usize
#define gtk_entry_set_width_chars(e,w) gtk_widget_set_usize(GTK_WIDGET(e),w*10,-1)
void gdk_gc_set_rgb_fg_color(GdkGC *gc, GdkColor *clr);
#define gtk_adjustment_get_lower(adj) ((adj)->lower)
#define gtk_adjustment_get_upper(adj) ((adj)->upper)
#else
#define GTK_WINDOW_DIALOG GTK_WINDOW_TOPLEVEL
#define gtk_object_class_add_signals(x,y,z)
#endif

#if GTK_MAJOR_VERSION == 1 || (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 18)
void gtk_widget_set_has_window(GtkWidget *w, gboolean has_window);
void gtk_widget_set_can_focus(GtkWidget *w, gboolean can_focus);
#endif

/* Global stuff */
#define PROGRAM_VERSION_STRING PACKAGE " " VERSION

/* Permissions for creating ~/.mhwaveedit directory */
#define CONFDIR_PERMISSION 0755

/* Various portability stuff */
#ifndef HAVE_CEILL
#define ceill(x) ((long)ceil(x))
#endif
#ifndef HAVE_FSEEKO
#define fseeko(s,o,w) fseek(s,(long)(o),w)
#endif
#ifndef HAVE_FTELLO
#define ftello(s) ((off_t)ftell(s))
#endif


/* Useful macros and functions */

#define ARRAY_LENGTH(arr) (sizeof(arr)/sizeof((arr)[0]))
#define ARRAY_END(arr) (arr + ARRAY_LENGTH(arr))

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define IS_BIGENDIAN FALSE
#else
#define IS_BIGENDIAN TRUE
#endif

#if SIZEOF_OFF_T > 4
#define OFF_T_FORMAT "lld"
#define OFF_T_FTYPE long long
#else
#define OFF_T_FORMAT "ld"
#define OFF_T_FTYPE long
#endif

#define BOOLEQ(x,y) (((x) && (y)) || (!(x) && !(y)))
#define XOR(x,y) (((x) && !(y)) || (!(x) && (y)))

extern gboolean quitflag;
extern GdkPixmap *icon;

extern gboolean quality_mode;

extern gchar *driver_option;

extern gboolean idle_work_flag;


/* Various functions */

void byteswap(void *buffer, int element_size, int buffer_size);

int timeval_subtract(GTimeVal *result, GTimeVal *x, GTimeVal *y);
float timeval_divide(GTimeVal *x, GTimeVal *y);
void timeval_divide_float(GTimeVal *result, GTimeVal *x, gfloat y);

gboolean free2(gpointer key, gpointer value, gpointer user_data);

gchar *get_home_directory(void);

void do_yield(gboolean may_sleep);

int hexval(gchar chr);

void launch_mixer(void);

GtkLabel *attach_label(gchar *text, GtkWidget *table, guint y, guint x);

enum Color { 
     BLACK=0, WHITE, BACKGROUND, WAVE1, WAVE2, CURSOR, MARK, SELECTION, 
     PROGRESS, BARS, BUFPOS, LAST_COLOR
};

#define FIRST_CUSTOM_COLOR BACKGROUND
#define CUSTOM_COLOR_COUNT (LAST_COLOR-FIRST_CUSTOM_COLOR)

extern GdkColor factory_default_colors[];
extern gchar *color_names[];
extern gchar *color_inifile_entry[];

GdkColor *get_color(enum Color c);
GdkGC *get_gc(enum Color c, GtkWidget *wid);
void set_custom_colors(GdkColor *c);
void save_colors(void);

gchar *channel_name(guint chan, guint total);
gchar channel_char(guint chan);
gchar *channel_format_name(guint chans);


gchar *namepart(gchar *filename);

#define TIMEMODE_REAL    0
#define TIMEMODE_REALLONG 1
#define TIMEMODE_SAMPLES 2
#define TIMEMODE_24FPS 3
#define TIMEMODE_25FPS 4
#define TIMEMODE_NTSC 5
#define TIMEMODE_30FPS 6

/* Determines how long time a certain number of samples represents, and returns
   it as a text string.

     samplerate - The sample rate for which we should determine the time
       samplerate member is used.)
     samples - The number of samples to measure.
     samplemax - Maximum value that needs to be displayed in the same format. If                 unknown, use the same value as for samples.
     timebuf - The buffer to store the text string into (should be at least 50 
       bytes)

     return value - timebuf
*/

extern guint default_time_mode;
extern guint default_timescale_mode;

gchar *get_time(guint32 samplerate, off_t samples, off_t samplemax, 
		gchar *timebuf, gint timemode);
/* Returns time always in short format H'MM:SS or MM:SS */
gchar *get_time_s(guint32 samplerate, off_t samples, off_t samplemax, 
		  gchar *timebuf);
/* Returns time always in long time format H'MM:SS.mmmm */
gchar *get_time_l(guint32 samplerate, off_t samples, off_t samplemax,
		  gchar *timebuf);

/* Returns time in same format as get_time, but with sub-seconds removed */
gchar *get_time_head(guint32 samplerate, off_t samples, off_t samplemax,
		     gchar *timebuf, int timemode);
/* Returns time in same format as get_time, but with only sub-second part */
gchar *get_time_tail(guint32 samplerate, off_t samples, off_t samplemax,
		     gchar *timebuf, int timemode);

/* Find a number of even sample points around start_samp and end_samp 
 * suitable for marking out on a time scale with major and minor ticks.
 *
 * At least two major points are always returned, one before/on start_samp and 
 * one after/on end_samp.
 * *npoints and *nminorpoints should be set to the maximum allowed number of 
 * points, and will be set to the number of stored points on return.
 * 
 * Returns a hint on how the text should be drawn:
 * 0 - Both major and minor points should have text from the get_time_head 
 *     function.
 * 1 - Major points should have text from the get_time_head function, and 
 *     minor points should have text from thh get_time_tail function.
 */
guint find_timescale_points(guint32 samplerate, 
			    off_t start_samp, off_t end_samp, 
			    off_t *points, int *npoints,
			    off_t *midpoints, int *nmidpoints,
			    off_t *minorpoints, int *nminorpoints,
			    int timemode);


/* Converts a string in the form: [H'][MM:]SS[.mmmm] to a seconds value. 
 * Returns negative value if the input is invalid. */
gfloat parse_time(gchar *timestr);

/* Lookup all keys corresponding to a certain value in a hash table. */
GSList *hash_table_lookup_keys(GHashTable *hash_table, gconstpointer value);

/* Parse a geometry string in the form  x_y_w_h */
gboolean parse_geom(gchar *str, GtkAllocation *result);
/* Get a geometry string from a window (it must be realized) */
gchar *get_geom(GtkWindow *window);

/* Geometry stack functions */
GSList *geometry_stack_from_inifile(gchar *ininame);
void geometry_stack_save_to_inifile(gchar *ininame, GSList *stack);

/* Returns TRUE on success */
gboolean geometry_stack_pop(GSList **stackp, gchar **extra, GtkWindow *wnd);
void geometry_stack_push(GtkWindow *w, gchar *extra, GSList **stackp);

/*
 * Tries to translate the string s. 
 *
 * If the string gets translated, returns the translated string.
 * If the string doesn't get translated, returns a pointer to after the first
 * pipe char ('|') in the string.
 *
 * This is used when you want different translations for the same string, 
 * depending on context. "X|Z" and "Y|Z" both return "Z" if not translated,
 * but can be translated to different things. 
 */
char *translate_strip(const char *s);

void format_float(float f, char *r, int maxsz);


/* Variables that choose dither methods. Doesn't really belong in this 
 * file, but I'm lazy.. */
extern int dither_editing;
extern int dither_playback;



#endif
