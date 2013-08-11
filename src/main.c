/*
 * Copyright (C) 2002 2003 2004 2005 2007 2008 2010 2011 2012, Magnus Hjorth
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

#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include "mainloop.h"
#include "mainwindow.h"
#include "datasource.h"
#include "player.h"
#include "chunk.h"
#include "sound.h"
#include "main.h"
#include "inifile.h"
#include "um.h"
#include "effectbrowser.h"
#include "soxdialog.h"
#include "statusbar.h"
#include "ladspadialog.h"
#include "gettext.h"
#include "session.h"

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

GdkPixmap *icon = NULL;
gboolean quitflag;
gboolean quality_mode = TRUE;
gchar *driver_option = NULL;

int dither_editing;
int dither_playback;

const char *strip_context(const char *s)
{
     const char *c;
     if (s[0] == '|') {
	  c = strchr(s+1,'|');
	  if (c != NULL) return c+1;
     } 
     return s;
}

static int idle_work(gpointer csource, gpointer user_data)
{
     if (!idle_work_flag) {
	  idle_work_flag = TRUE;
	  return 1;
     }     
     if (status_bar_progress_count() > 0) return 1;
     if (mainwindow_update_caches()) return 1;
     return -1;
}

#if GTK_MAJOR_VERSION == 1

void gdk_gc_set_rgb_fg_color(GdkGC *gc, GdkColor *clr)
{
     static GdkColor cached = {0,-1,-1,-1};
     if (clr->red == cached.red && clr->green == cached.green && 
	 clr->blue == cached.blue) 
	  clr->pixel = cached.pixel;
     else {
	  gdk_colormap_alloc_color(gdk_colormap_get_system(), 
				   clr,FALSE,TRUE);
	  memcpy(&cached,clr,sizeof(cached));
     }
     gdk_gc_set_foreground(gc, clr);
}

#endif

#if GTK_MAJOR_VERSION == 1 || (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 18)

void gtk_widget_set_has_window(GtkWidget *w, gboolean has_window)
{
     if (!has_window)
	  GTK_WIDGET_SET_FLAGS(w,GTK_NO_WINDOW);
     else
	  GTK_WIDGET_UNSET_FLAGS(w,GTK_NO_WINDOW);
}

void gtk_widget_set_can_focus(GtkWidget *w, gboolean can_focus)
{
     if (!can_focus)
	  GTK_WIDGET_SET_FLAGS(w,GTK_CAN_FOCUS);
     else
	  GTK_WIDGET_UNSET_FLAGS(w,GTK_CAN_FOCUS);
}

#endif

int main(int argc, char **argv)
{
     int i;
     int wavefile_count=0;
     gboolean ladspa = TRUE;
     gboolean b;
     gchar *c;

     /* Some versions of the aRts C library call g_thread_init internally,
      * and that confuses GLib2's memory allocation stuff. */
#ifdef HAVE_GTHREAD
     g_thread_init(NULL);
#endif

     /* Set default locale */
     setlocale(LC_ALL,"");

     /* This must be the same for all locales */
     setlocale(LC_NUMERIC, "POSIX");

    /* Disable gtk's ability to set the locale. */
    /* If gtk is allowed to set the locale, then it will override the above */
    /* setlocale for LC_NUMERIC. */
#if GTK_MAJOR_VERSION == 2
     gtk_disable_setlocale();
#endif

#ifdef ENABLE_NLS
     /* Setup message domain */
     bindtextdomain("mhwaveedit", LOCALEDIR);
     textdomain("mhwaveedit");
#if GTK_MAJOR_VERSION == 2
     bind_textdomain_codeset("mhwaveedit", "UTF-8");
#endif
#endif

     floating_point_check();


     /* Check for "terminating" options that don't require starting GTK. */
     for (i=1; i<argc; i++) {
	  if (!strcmp(argv[i],"--version")) { 
	       puts(PROGRAM_VERSION_STRING); 
	       return 0; 
	  } else if (!strcmp(argv[i],"--help")) {	       
               printf(_("Syntax: %s [files]\n"),argv[0]);
               return 0;
	  } else if (!strcmp(argv[i],"--test")) {
		puts(_("Testing conversion functions:"));
		conversion_selftest();
		puts(_("Testing conversion functions finished."));
		return 0;
	  } else if (!strcmp(argv[i],"--perftest")) {
	       conversion_performance_test();
	       return 0;
	  } else if (!strcmp(argv[i],"--")) break;
     }

     /* Setup GTK */
     gtk_init(&argc,&argv);

     /* gdk_window_set_debug_updates(TRUE); */
     /* Check for options. */
     for (i=1; i<argc; i++) {
	  if (!strcmp(argv[i],"--no-ladspa")) {
	       ladspa = FALSE;	       
	  } else if (!strcmp(argv[i],"--driver")) {
	       i++;
	       if (i == argc)
		    console_message(_("Expected driver name after "
				      "--driver option"));
	       else
		    driver_option = argv[i];
	  } else if (!strcmp(argv[i],"--")) break;
	  else if (argv[i][0] == '-') {
	       c = g_strdup_printf(_("Unknown option '%s'"),argv[i]);
	       console_message(c);
	       g_free(c);
	       return 1;
	  }
     }

     /* Call init functions. */
     inifile_init();
     sound_init();
     session_init(&argc,argv);

     /* Setup some global flags from inifile */
     status_bar_roll_cursor=inifile_get_gboolean("rollCursor",FALSE);
     view_follow_strict_flag = inifile_get_gboolean("centerCursor",TRUE);
     autoplay_mark_flag = inifile_get_gboolean("autoPlayMark",FALSE);
     varispeed_reset_flag = inifile_get_gboolean("speedReset",FALSE);
     varispeed_smooth_flag = inifile_get_gboolean("speedSmooth",TRUE);
     dataformat_get_from_inifile("playerFallback",TRUE,
				 &player_fallback_format);
     chunk_filter_use_floating_tempfiles = 
	  inifile_get_gboolean("tempfilesFP",TRUE);
     dither_editing = inifile_get_guint32("ditherEditing",DITHER_MINIMAL);
     if (dither_editing < 0 || dither_editing > DITHER_MAX)
	  dither_editing = DITHER_MINIMAL;
     dither_playback = inifile_get_gboolean("ditherPlayback",DITHER_NONE);
     if (dither_playback < 0 || dither_playback > DITHER_MAX)
	  dither_playback = DITHER_NONE;
     sample_convert_mode = inifile_get_guint32("sampleMode",CONVERT_MODE_NOOFFS);
     if (sample_convert_mode < 0 || sample_convert_mode > CONVERT_MODE_MAX)
	  sample_convert_mode = CONVERT_MODE_NOOFFS;

     gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_END);

     mainwindow_objects = list_object_new(FALSE);
     document_objects = list_object_new(FALSE);

     default_time_mode = inifile_get_guint32(INI_SETTING_TIME_DISPLAY,0);
     if (default_time_mode > 6) default_time_mode = 0;
     default_timescale_mode = 
	  inifile_get_guint32(INI_SETTING_TIME_DISPLAY_SCALE,1);
     if (default_timescale_mode > 6) default_time_mode = 0;

     /* Some color related stuff */
     set_custom_colors(NULL);

     /* Register effects */
     effect_register_init();
     if (ladspa) ladspa_dialog_register();
     sox_dialog_register();

     um_use_gtk = TRUE;

     /* Search command line for filenames and create windows */
     b = TRUE;
     for (i=1;i<argc;i++) {	  
	  if (b && !strcmp(argv[i],"--")) b = FALSE;
	  else if (b && !strcmp(argv[i],"--driver")) { i++; continue; }
	  else if (b && argv[i][0] == '-') continue;
	  gtk_widget_show(mainwindow_new_with_file(argv[i],TRUE));
	  wavefile_count++;
     }
     if (wavefile_count==0 && !session_dialog()) 
	  gtk_widget_show(mainwindow_new());

     /* gtk_idle_add(idle_work,NULL); */

     /* Add low priority idle function */
     mainloop_constant_source_add(idle_work,NULL,TRUE);

     /* Run it! */
     while (!quitflag)
	  mainloop();



     /* Cleanup */
     player_stop();
     if (playing_document != NULL) {
	  gtk_object_unref(GTK_OBJECT(playing_document));
	  playing_document = NULL;
     }
     sound_quit();
     effect_browser_shutdown();
     inifile_quit();
     session_quit();
     g_assert ( chunk_alive_count() == 0 && datasource_count() == 0);     
     return 0;
}



gchar *namepart(gchar *fullname)
{
     gchar *c;
     c = strrchr ( fullname, '/' );
     return c ? c+1 : fullname;
}



/* timeval_subtract - Stolen from glibc info docs... */

     /* Subtract the `struct timeval' values X and Y,
        storing the result in RESULT.
        Return 1 if the difference is negative, otherwise 0.  */
     
int
     timeval_subtract (result, x, y)
          GTimeVal *result, *x, *y;
     {
       /* Perform the carry for the later subtraction by updating Y. */
       if (x->tv_usec < y->tv_usec) {
         int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
         y->tv_usec -= 1000000 * nsec;
         y->tv_sec += nsec;
       }
       if (x->tv_usec - y->tv_usec > 1000000) {
         int nsec = (x->tv_usec - y->tv_usec) / 1000000;
         y->tv_usec += 1000000 * nsec;
         y->tv_sec -= nsec;
       }
     
       /* Compute the time remaining to wait.
          `tv_usec' is certainly positive. */
       result->tv_sec = x->tv_sec - y->tv_sec;
       result->tv_usec = x->tv_usec - y->tv_usec;
     
       /* Return 1 if result is negative. */
       return x->tv_sec < y->tv_sec;
     }


gchar *get_home_directory(void)
{
     static gchar *homedir = NULL;
     struct passwd *p;
     if (!homedir)
	  homedir = getenv("HOME");
     if (!homedir) {
	  p = getpwuid(getuid());
	  if (p) homedir = p->pw_dir;
     }
     if (!homedir) {
	  g_warning(_("Could not find home directory. Using current directory as "
		    "home directory."));
	  homedir = ".";
     }
     return homedir;
}

static gboolean color_alloced[LAST_COLOR] = { 0 };
GdkColor color_table[LAST_COLOR];

/* old defaults
GdkColor factory_default_colors[] = {
     { 0, 0, 0, 0 }, { 0, 0, 0, 0xFFFF }, { 0, 0xFFFF, 0, 0 },
     { 0, 0x8000, 0x8000, 0x8000 }, { 0, 0x0f00, 0x8400, 0x6900 },
     { 0, 0xE800, 0xA500, 0x4000 }, { 0, 0xff00, 0xff00, 0xff00 },
     { 0, 0x8000, 0x8000, 0x8000 }
};
*/

GdkColor factory_default_colors[] = {
     { 0, 0, 0, 0 }, { 0, 29952, 50176, 27392 }, { 0, 41216, 39936, 52992 },
     { 0, 62720, 62720, 62720 }, { 0, 60160, 55552, 0 },
     { 0, 43776, 11776, 0 }, { 0, 0xff00, 0xff00, 0xff00 },
     { 0, 0x8000, 0x8000, 0x8000 }, { 0, 0x6200, 0x7c00, 0x5e00 }
};
static GdkGC *gc_table[LAST_COLOR] = { };

gchar *color_names[] = {
    N_("Black"),
    N_("White"),
    N_("Background"),
    N_("L Waveform"),
    N_("R Waveform"),
    N_("Cursor"),
    N_("Marks"),
    N_("Selection"),
    N_("Progress bar"),
    N_("Zero-level"),
    N_("Buffer position")
};

gchar *color_inifile_entry[] = { NULL, NULL, "colorBG", 
				 "colorWave1", "colorWave2",
				 "colorCursor","colorMark",
				 "colorSelection","colorProgress",
				 "colorBars","colorBufpos"
};

GdkColor *get_color(enum Color c)
{
     if (!color_alloced[c]) {
	  if (c == BLACK) 
	       gdk_color_black(gdk_colormap_get_system(),&color_table[c]);
	  else if (c == WHITE)
	       gdk_color_white(gdk_colormap_get_system(),&color_table[c]);
	  else {
	       gdk_colormap_alloc_color(gdk_colormap_get_system(),
					&color_table[c],FALSE,TRUE);	       
	  }
     }
     color_alloced[c] = TRUE;
     return &color_table[c];
}

GdkGC *get_gc(enum Color c, GtkWidget *w)
{
     if (gc_table[c] == NULL) {
	  gc_table[c] = gdk_gc_new(w->window);
	  gdk_gc_set_foreground(gc_table[c],get_color(c));
     }
     return gc_table[c];
}

int hexval(gchar chr)
{
     if (chr >= '0' && chr <= '9') return chr-'0';
     else if (chr >= 'a' && chr <= 'f') return chr-'a'+10;
     else if (chr >= 'A' && chr <= 'F') return chr-'A'+10;
     else return -1;
}

static void parse_color(gchar *str, GdkColor *color)
{
     unsigned int i,j,k,rgb[3];
     for (i=0; i<3; i++,str+=2) {
	  j = hexval(str[0]);
	  if (j == -1) return;
	  k = hexval(str[1]);
	  if (k == -1) return;
	  rgb[i] = j*16+k;
     }
     color->red = rgb[0] * 256;
     color->green = rgb[1] * 256;
     color->blue = rgb[2] * 256;
}

void set_custom_colors(GdkColor *c)
{
     gint i;
     gchar *d;
     if (!c) {
	  set_custom_colors(factory_default_colors);
	  for (i=FIRST_CUSTOM_COLOR; i<LAST_COLOR; i++) {
	       d = inifile_get(color_inifile_entry[i],NULL);
	       /* printf("%s -> %s\n",color_inifile_entry[i],d); */
	       if (d != NULL) {
		    /* printf("Before: %x,%x,%x\n",color_table[i].red,
		       color_table[i].green,color_table[i].blue); */
		    parse_color(d,&color_table[i]);	       
		    /* printf("After: %x,%x,%x\n",color_table[i].red,
		       color_table[i].green,color_table[i].blue);  */
	       }
	  }
     } else 
	  memcpy(&color_table[FIRST_CUSTOM_COLOR],c,
		 CUSTOM_COLOR_COUNT*sizeof(GdkColor));
     memset(color_alloced,0,sizeof(color_alloced));
     for (i=FIRST_CUSTOM_COLOR; i<LAST_COLOR; i++) {
	  if (gc_table[i] != NULL) {
	       gdk_gc_unref(gc_table[i]);
	       gc_table[i] = NULL;
	  }
     }
     mainwindow_repaint_views();
}

void save_colors(void)
{
     int i;
     gchar triplet[7];
     for (i=FIRST_CUSTOM_COLOR; i<LAST_COLOR; i++) {
	  g_snprintf(triplet,sizeof(triplet),"%.2x%.2x%.2x",
		     color_table[i].red/256, color_table[i].green/256,
		     color_table[i].blue/256);
	  inifile_set(color_inifile_entry[i],triplet);
     }
}

void do_yield(gboolean may_sleep)
{
#ifdef HAVE_SCHED_YIELD
     static int lasttime=0, yieldcount=0;
     int i;
     if (!may_sleep) { sched_yield(); return; }
     i = time(0);
     if (i != lasttime) { yieldcount=0; lasttime=i; }
     sched_yield();
     yieldcount++;
     if (yieldcount == 20) { usleep(1000); yieldcount=0; }
#else
     if (may_sleep) usleep(1000);
#endif
}

void launch_mixer(void)
{
     gchar *m,*c;
     pid_t p;
     m = inifile_get(INI_SETTING_MIXER,INI_SETTING_MIXER_DEFAULT);
     p = fork();
     if (p == -1) {
	  c = g_strdup_printf(_("Error launching mixer: fork: %s"),strerror(errno));
	  user_error(c);
	  g_free(c);
	  return;
     }
     if (p != 0) return;
     close_all_files();
     if (execl("/bin/sh","sh","-c",m,NULL) == -1) {
	  fprintf(stderr,"mhwaveedit: execl: %s: %s\n",m,strerror(errno));
	  _exit(1);
     }
}

gboolean free2(gpointer key, gpointer value, gpointer user_data)
{
     g_free(key);
     g_free(value);
     return TRUE;
}

#define BYTESWAP_BSIZE 1*2*3*4*5*6

void byteswap(void *buffer, int element_size, int buffer_size)
{
     int i,j;
     char *c,*d,*bufend=NULL;
     char tempbuf[BYTESWAP_BSIZE];
     g_assert(element_size < 7 || element_size==8);
     if (element_size == 1) return;
     while (buffer_size != 0) {
	  i = BYTESWAP_BSIZE;
	  if (i > buffer_size) i=buffer_size;
	  bufend = ((char *)buffer) + i;
	  for (j=0; j<element_size; j++) {
	       c = ((char *)buffer)+j;
	       d = tempbuf+element_size-1-j;
	       while (c<bufend) {
		    *d = *c;
		    c += element_size;
		    d += element_size;
	       } 
	  }
	  memcpy(buffer,tempbuf,i);
	  buffer = bufend;
	  buffer_size -= i;	  
     }
}

gchar *channel_name(guint chan, guint total)
{
     static gchar buf[15];
     if (total == 1) return _("Mono");
     else if (chan == 0) return _("Left");
     else if (chan == 1) return _("Right");
     else {
	  g_snprintf(buf,sizeof(buf),_("Ch%d"),chan+1);
	  return buf;
     }
}

gchar channel_char(guint chan)
{
     if (chan == 0) return 'L';
     else if (chan == 1) return 'R';
     else return '1'+chan;
}

gchar *channel_format_name(guint chans)
{
     static gchar buf[15];
     if (chans == 1) return _("Mono");
     else if (chans == 2) return _("Stereo");
     else {
	  g_snprintf(buf,sizeof(buf),_("%d channels"),chans);
	  return buf;
     }
}

GtkLabel *attach_label(gchar *text, GtkWidget *table, guint y, guint x)
{
     GtkWidget *l;
     l = gtk_label_new(text);
     gtk_misc_set_alignment(GTK_MISC(l),0.0,0.5);
     gtk_table_attach(GTK_TABLE(table),l,x,x+1,y,y+1,GTK_FILL,0,0,0);
     return GTK_LABEL(l);
}

gchar *get_time(guint32 samplerate, off_t samples, off_t samplemax, 
		gchar *timebuf, gint mode)
{
     static gchar static_buf[64];
     gfloat secs, ffps;
     guint mins, msecs, hours, maxhours, frames, ifps, isecs;
     guint fptm;

     if (samplemax == 0) samplemax = samples;

     if (!timebuf) 
	  timebuf = static_buf;
     
     if (mode > 6) mode = 0;

     if (mode == 2) {
	  g_snprintf(timebuf,50,"%05" OFF_T_FORMAT,(OFF_T_FTYPE)samples);
     } else if (mode < 2) {
	  secs = (gfloat) samples / (gfloat) samplerate;
	  mins = (guint) (secs / 60.0);
	  hours = mins / 60;
	  mins = mins % 60;
	  msecs = ((guint) (secs  * 1000.0));
	  msecs %= 60000;
	  maxhours = samplemax / (samplerate * 3600);
	  if (mode == 0) {
	       if (maxhours > 0)
		    g_snprintf(timebuf,50,"%d'%02d:%02d.%d",hours,mins,
			       msecs/1000,(msecs%1000)/100);
	       else 
		    g_snprintf(timebuf,50,"%02d:%02d.%d",mins,
			       msecs/1000,(msecs%1000)/100);
	  } else if (mode == 1) {
	       if (maxhours > 0)
		    g_snprintf(timebuf,50,"%d'%02d:%02d.%03d",hours,mins,
			       msecs/1000,msecs%1000);
	       else 
		    g_snprintf(timebuf,50,"%02d:%02d.%03d",mins,
			       msecs/1000,msecs%1000);
	  } else {
	       if (maxhours > 0)
		    g_snprintf(timebuf,50,"%d'%02d:%02d",hours,mins,
			       msecs/1000);
	       else
		    g_snprintf(timebuf,50,"%02d:%02d",mins,msecs/1000);
	  }
     } else {
	  secs = (gfloat) samples / (gfloat) samplerate;
	  
	  if (mode == 3) { ffps=24.0; ifps=24; }
	  else if (mode == 4) { ffps=25.0; ifps=25; }
	  else if (mode == 5) { ffps=30.0*1.000/1.001; ifps=30; }
	  else { ffps=30.0; ifps=30; }

	  frames = (guint) (secs * ffps);

	  /* Ten-minute units never need to be dropped. */
	  if (mode == 5) {
	       fptm = 60*30*10 - 2*9;
	  } else {
	       fptm = ifps * 600;
	  }
	  mins = 10 * (frames / fptm);
	  frames = frames % fptm;

	  /* Translate the remaining frames to mins+isecs+frames */
	  if (mode != 5) {
	       isecs = frames / ifps;
	       frames %= ifps;
	       mins += isecs / 60;
	       isecs %= 60;
	  } else {
	       /* First minute is 60x30 frames */
	       if (frames >= 60*30) {
		    mins ++;
		    frames -= 60*30;
		    /* Remaining minutes are 60x30-2 frames */
		    mins += frames / (60*30-2);
		    frames %= (60*30-2);
		    /* Skip two frames on the last minute */
		    frames += 2;
	       }
	       isecs = frames / ifps;
	       frames %= ifps;
	       g_assert(isecs < 60);	       
	  }

	  hours = mins / 60;
	  mins = mins % 60;

	  g_snprintf(timebuf,50,"%02d:%02d:%02d[%02d]", hours, mins, isecs, 
		     frames);
     }
     return timebuf;
}

guint default_time_mode = 100, default_timescale_mode = 100;

gchar *get_time_s(guint32 samplerate, off_t samples, off_t samplemax,
		  gchar *timebuf)
{
     return get_time(samplerate,samples,samplemax,timebuf,-1);
}

gchar *get_time_l(guint32 samplerate, off_t samples, off_t samplemax,
		  gchar *timebuf)
{
     return get_time(samplerate,samples,samplemax,timebuf,1);
}

gchar *get_time_head(guint32 samplerate, off_t samples, off_t samplemax,
		     gchar *timebuf, gint timemode)
{
     switch (timemode) {
     case TIMEMODE_REAL:
     case TIMEMODE_REALLONG:
     case TIMEMODE_24FPS:
     case TIMEMODE_25FPS:
     case TIMEMODE_NTSC:
     case TIMEMODE_30FPS:	  
     default:
	  return get_time(samplerate,samples,samplemax,timebuf,-1);
     case TIMEMODE_SAMPLES:
	  return get_time(samplerate,samples,samplemax,timebuf,
			    TIMEMODE_SAMPLES);
     }
}

gchar *get_time_tail(guint32 samplerate, off_t samples, off_t samplemax,
		     gchar *timebuf, gint timemode)
{
     
     off_t frameno;
     guint i;
     switch (timemode) {
     case TIMEMODE_REAL:
	  return NULL;
     case TIMEMODE_REALLONG:
	  i = ((samples % samplerate) * 1000) / samplerate;
	  g_snprintf(timebuf,50,".%03d",i);
	  return timebuf;
     case TIMEMODE_24FPS:
	  i = ((samples % samplerate) * 24) / samplerate;
	  g_snprintf(timebuf,50,"[%02d]",i);
	  return timebuf;
     case TIMEMODE_25FPS:
	  i = ((samples % samplerate) * 25) / samplerate;
	  g_snprintf(timebuf,50,"[%02d]",i);
	  return timebuf;
     case TIMEMODE_NTSC:
	  frameno = (samples * 30 * 1000) / (samplerate * 1001);
	  /* Remove all 10 minute blocks of 60x30x10-2x9 frames */
	  i = (guint) (frameno % (60*30*10-2*9));
	  /* Remove the first minute */
	  if (i >= 60*30) {
	       i -= 60*30;
	       /* Remove subsequent minutes */
	       i %= 60*30-2;
	       /* Add drop frame in current minute to count */
	       i += 2;
	  }
	  /* Remove seconds */
	  i %= 30;
	  g_snprintf(timebuf,50,"[%02d]",i);
	  return timebuf;
     case TIMEMODE_30FPS:	  
	  i = ((samples % samplerate) * 30) / samplerate;
	  g_snprintf(timebuf,50,"[%02d]",i);
	  return timebuf;
     default:
     case TIMEMODE_SAMPLES:
	  return NULL;
     }     
}

/* This table specify at which intervals big or small lines can be 
 * drawn. */
static const gint bigsizes[] = { 1, 2, 5, 10, 20, 30, 
				 60, 120, 180, 300, 600, 
				 900, 1800, 3600, 36000 };
/* This table is TRUE whenever the entry in the table above is
 * not an even divisor in the  entry that follows it. */ 
static const gboolean bigskip[] = { FALSE, TRUE, FALSE, FALSE, TRUE, 
				    FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, 
				    TRUE, FALSE, FALSE };

static const gint smallsizes_real[] = { 1000, 100, 10 };
static const gint smallsizes_24fps[] = { 24, 12, 4 };
static const gint smallsizes_25fps[] = { 25, 5 };
static const gint smallsizes_30fps[] = { 30, 10, 5 };

/* Returns:
 * 0 - Both major and minor points should have text from the get_time_head 
 *     function.
 * 1 - Major points should have text from the get_time_head function, and 
 *     minor points should have text from teh get_time_tail function.
 */ 

/* Midpoints are generated when there is a minor point scale that has
 * <= nmidpoints elements, is larger than the scale used for the minor points
 * and the major points are at the smallest scale. 
 * Sounds very messy, but it's for making sure that we always have points 
 * we can draw text on. 
 * */
guint find_timescale_points(guint32 samplerate, off_t start_samp, 
			    off_t end_samp,
			    off_t *points, int *npoints, 
			    off_t *midpoints, int *nmidpoints,
			    off_t *minor_points, int *nminorpoints,
			    int timemode)
{
     guint pctr=0,mpctr=0,midpctr=0;
     off_t p,q,r,s;
     int i;
     const int *ss;
     int ssl;
     int max_points = *npoints, max_minorpoints = *nminorpoints;
     int max_midpoints = *nmidpoints;

     g_assert(max_points > 2); 
     g_assert(max_minorpoints >= max_points);

     *nmidpoints = 0;

     /* Handle sample-based time */
     if (timemode == TIMEMODE_SAMPLES) {
	  
	  p = 1;
	  q = start_samp;
	  r = end_samp;
	  while (r-q >= (off_t)(max_points-1)) {
	       q /= 10;
	       r /= 10;
	       p *= 10;
	  }	  
	  for (s=q; s<=r+1; s++) {
	       points[pctr++] = s * p;
	       g_assert(pctr <= max_points);
	  }	  

	  p = 1;
	  q = start_samp;
	  r = end_samp;
	  while (r-q >= (off_t)(max_minorpoints-1)) {
	       q /= 10;
	       r /= 10;
	       p *= 10;
	  }	  
	  for (s=q; s<=r+1; s++) {
	       minor_points[mpctr++] = s * p;
	       g_assert(mpctr <= max_minorpoints);
	  }	  
	  *npoints = pctr;
	  *nminorpoints = mpctr;
	  return 0;
     }

     /* Handle major points (common between timecode and real time) */
     i = 0;
     while (i < (ARRAY_LENGTH(bigsizes)-1) &&
	    ( (end_samp-start_samp)/(bigsizes[i]*samplerate) > 
	      (off_t)(max_points-2) ) )
	  i++;
     q = start_samp / (bigsizes[i] * samplerate);
     r = end_samp / (bigsizes[i] * samplerate);
     /* printf("q = %d, r = %d\n",(int)q,(int)r); */
     while (1) {
	  points[pctr++] = (q++) * bigsizes[i] * samplerate;
	  /* printf("q = %d, pctr = %d, max_points = %d\n",(int)q,(int)pctr,(int)max_points); */
	  g_assert(pctr <= max_points);
	  if (q > r) break;
     }
     *npoints = pctr;

     /* Handle minor points of >=1s */
     if (i > 0) {
	  i--;
	  while (bigskip[i]) i--;
	  if ( (end_samp-start_samp) / (bigsizes[i]*samplerate) >= 
	       (off_t)(max_minorpoints-2)) {
	       *nminorpoints = 0;
	       return 0;
	  }
	  while (i > 0 &&
		 ( (end_samp-start_samp)/(bigsizes[i-1]*samplerate) < 
		   (off_t)(max_minorpoints-2) ) )
	       i--;
	  
	  q = start_samp / (bigsizes[i] * samplerate);
	  r = end_samp / (bigsizes[i] * samplerate);
	  for (s=q; s<=r+1; s++) {
	       minor_points[mpctr++] = s * bigsizes[i] * samplerate;
	       g_assert(mpctr <= max_minorpoints);
	  }	  
	  *nminorpoints = mpctr;
	  return 0;
     }

     /* Handle sub-second minor ticks for NTSC timecode */
     /* This is a special case since NTSC frames are not second-aligned */
     if (timemode == TIMEMODE_NTSC) {
	  /* Calculate start and end frame count (rounded down) */
	  q = (start_samp * 30 * 1000) / (samplerate * 1001);
	  r = (end_samp * 30 * 1000) / (samplerate * 1001);
	  
	  if (r-q >= max_minorpoints) {
	       *nminorpoints = 0;
	       return 0;
	  }
	  
	  for (s=q; s<=r; s++) {
	       /* Convert from frame to sample,  rounding upwards */
	       minor_points[mpctr++] = 
		    (s * samplerate * 1001 + 29999) / (30000);
	  }
	  *nminorpoints = mpctr;
	  return 1;
     }
     
     /* Handle sub-second minor ticks, real time or non-NTSC timecode */
     switch (timemode) {
     case TIMEMODE_REAL:
     case TIMEMODE_REALLONG:
	  ss = smallsizes_real;
	  ssl = ARRAY_LENGTH(smallsizes_real);
	  break;
     case TIMEMODE_24FPS:
	  ss = smallsizes_24fps;
	  ssl = ARRAY_LENGTH(smallsizes_24fps);
	  break;
     case TIMEMODE_25FPS:
	  ss = smallsizes_25fps;
	  ssl = ARRAY_LENGTH(smallsizes_25fps);
	  break;
     case TIMEMODE_30FPS:
	  ss = smallsizes_30fps;
	  ssl = ARRAY_LENGTH(smallsizes_30fps);
	  break;
     default:
	  g_assert_not_reached();
     }

     i = 0;
     while (i < ssl && 
	    ((end_samp-start_samp)*ss[i])/samplerate >= 
	    (off_t)(max_minorpoints-2))
	  i++;

     if (i >= ssl) {
	  *nminorpoints = 0;
	  return 0;
     }

     q = (start_samp*ss[i])/samplerate;
     r = (end_samp*ss[i])/samplerate;
     
     for (s=q; s<=r+1; s++) {
	  minor_points[mpctr++] = (s * samplerate + ss[i]-1) / ss[i];
     }
     *nminorpoints = mpctr;

     /* Generate mid points */
     do {
	  i++;
     } while (i < ssl && 
	      ((end_samp-start_samp)*ss[i])/samplerate >= 
	      (off_t)(max_midpoints-2));

     if (i >= ssl) return 1;
     
     q = (start_samp*ss[i])/samplerate;
     r = (end_samp*ss[i])/samplerate;
     
     for (s=q; s<=r+1; s++) {
	  midpoints[midpctr++] = (s * samplerate + ss[i]-1) / ss[i];
     }
     *nmidpoints = midpctr;     
     
     return 1;
}

gfloat parse_time(gchar *timestr)
{
     gchar *c,*d;
     gfloat t = 0.0, f;
     long int l;
     int i;

     c = timestr;
     /* Hours */
     d = strchr(c,'\'');
     if (d != NULL) {
	  l = strtol(c,&d,10);
	  if (l < 0 || *d != '\'') return -1.0;
	  t += (gfloat)l * 3600.0;
	  c = d+1;
     }
     /* Minutes */
     d = strchr(c,':');
     if (d != NULL) {
	  l = strtol(c,&d,10);
	  if (l < 0 || *d != ':') return -1.0;
	  t += (gfloat)l * 60.0;
	  c = d+1;
     }
     /* Seconds */
     if (*c != '.') {
	  l = strtol(c,&d,10);
	  if (l < 0 || (*d != '.' && *d != 0)) return -1.0;
	  t += (gfloat)l;
	  if (*d == 0) return t;
	  c = d + 1;
     } else
	  c = c + 1;
     /* Millis */
     l = strtol(c,&d,10);
     if (l < 0 || *d != 0) return -1.0;
     f = (gfloat)l;
     for (i=0; i<(d-c); i++) f *= 0.1;
     t += f;
     return t;
}

struct lookup_keys_priv {
     gconstpointer value;
     GSList *keys;
};

static void lookup_keys_func(gpointer key, gpointer value,
			     gpointer user_data)
{
     struct lookup_keys_priv *p = (struct lookup_keys_priv *)user_data;
     if (!strcmp(value,p->value)) p->keys=g_slist_prepend(p->keys,key); 
}

GSList *hash_table_lookup_keys(GHashTable *hash_table, gconstpointer value)
{
     struct lookup_keys_priv p = { value, NULL };
     g_hash_table_foreach(hash_table,lookup_keys_func,&p);
     return p.keys;
}

struct geometry_stack_item {
     gint x,y;
     gint width, height;
     gchar *extra;
};

static gboolean parse_geom_x(gchar *str, struct geometry_stack_item *result)
{
     struct geometry_stack_item a;
     gchar *d,*e;
     a.x = strtoul(str,&d,10);
     if (*d != '_') return TRUE;
     a.y = strtoul(d+1,&d,10);
     if (*d != '_') return TRUE;
     a.width = strtoul(d+1,&d,10);
     if (*d != '_') return TRUE;
     a.height = strtoul(d+1,&e,10);
     if (e == d+1) return TRUE;
     
     if (*e == '_') a.extra = g_strdup(e+1);
     else a.extra = NULL;

     memcpy(result,&a,sizeof(*result));

     return FALSE;
}

gboolean parse_geom(gchar *str, GtkAllocation *result)
{
     struct geometry_stack_item item;
     if (parse_geom_x(str,&item)) return TRUE;
     if (item.extra) g_free(item.extra);
     result->x = item.x;
     result->y = item.y;
     result->width = item.width;
     result->height = item.height;
     return FALSE;
}


static gchar *get_geom_x(GtkWindow *window, gchar *extra)
{
     gint x,y,width,height;
     gdk_window_get_size(GTK_WIDGET(window)->window,&width,&height);
     gdk_window_get_root_origin(GTK_WIDGET(window)->window,&x,&y);
     if (extra == NULL)
	  return g_strdup_printf("%d_%d_%d_%d",x,y,width,height);
     else
	  return g_strdup_printf("%d_%d_%d_%d_%s",x,y,width,height,extra);
}

gchar *get_geom(GtkWindow *window)
{
     return get_geom_x(window,NULL);
}

GSList *geometry_stack_from_inifile(gchar *ininame)
{
     gchar *c,*d;
     struct geometry_stack_item a,*ap;
     GSList *l = NULL;
     gboolean boo;

     c = inifile_get(ininame,NULL);
     if (c != NULL) {	  
	  while (g_slist_length(l) < 64) {
	       d = strchr(c,'|');
	       if (d) *d=0;
	       boo = parse_geom_x(c,&a);
	       if (d) *d='|';
	       if (boo) break;
	       if (a.width > 0 && a.height > 0) {
		    ap = g_malloc(sizeof(*ap));
		    memcpy(ap,&a,sizeof(*ap));
		    l = g_slist_append(l,ap);
	       }
	       if (d == NULL) break;
	       c = d+1;
	  }
     }

     return l;
}

static void stack_proc(gpointer data, gpointer user_data)
{
     struct geometry_stack_item *all = (struct geometry_stack_item *)data;
     gchar **c = (gchar **)user_data;
     gchar *d;
     if (all->extra)
	  d = g_strdup_printf("%s%s%d_%d_%d_%d_%s",(*c)?(*c):"",(*c)?"|":"",
			      all->x,all->y,all->width,all->height,all->extra);
     else
	  d = g_strdup_printf("%s%s%d_%d_%d_%d",(*c)?(*c):"",(*c)?"|":"",
			      all->x,all->y,all->width,all->height);
     g_free(*c);
     *c = d;
}

void geometry_stack_save_to_inifile(gchar *ininame, GSList *stack)
{
     gchar *c=NULL;
     g_slist_foreach(stack,stack_proc,&c);
     if (c != NULL) 
	  inifile_set(ininame,c);     
     g_free(c);
}

gboolean geometry_stack_pop(GSList **stackp, gchar **extra, GtkWindow *wnd)
{
     struct geometry_stack_item *ap;

     if (*stackp == NULL) return FALSE;

     ap = (struct geometry_stack_item *)((*stackp)->data);
     *stackp = g_slist_remove(*stackp,ap);

     if (ap->x < -4000 || ap->x > 4000 || ap->y < -4000 || ap->y > 4000 ||
	 ap->width > 4000 || ap->height > 4000) {
	  fputs(_("Ignoring extreme old window size/position values\n"),stderr);
	  g_free(ap->extra);
	  g_free(ap);
	  return geometry_stack_pop(stackp,extra,wnd);
     }
     gtk_window_set_default_size(GTK_WINDOW(wnd),ap->width,ap->height);
     gtk_widget_set_uposition(GTK_WIDGET(wnd),ap->x,ap->y);
     if (extra) *extra = ap->extra;
     g_free(ap);

     return TRUE;
}

void geometry_stack_push(GtkWindow *w, gchar *extra, GSList **stackp)
{
     struct geometry_stack_item *all;
     gint x,y,width,height;
     all = g_malloc(sizeof(*all));
     gdk_window_get_size(GTK_WIDGET(w)->window,&width,&height);
     gdk_window_get_root_origin(GTK_WIDGET(w)->window,&x,&y);
     all->x = x;
     all->y = y;
     all->width = width;
     all->height = height;
     if (extra) all->extra = g_strdup(extra);
     else all->extra = NULL;
     *stackp = g_slist_prepend(*stackp,all);
}

char *translate_strip(const char *s)
{
     char *c;
     c = _(s);
     if (c == s) {
	  c = strchr(s,'|');
	  g_assert(c != NULL);
	  return c+1;
     } else
	  return c;
}

void format_float(float f, char *r, int maxsz)
{
     gchar *c;
     g_snprintf(r,maxsz,"%#.7g",f);
     c = strchr(r,0);
     c-=2;
     while (c>=r && isdigit(c[0]) && c[1]=='0') {
	  c[1] = 0;
	  c--;
     }     
}
