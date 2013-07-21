/*
 * Copyright (C) 2002 2003 2004 2005 2008 2009 2010, Magnus Hjorth
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

#include "sound.h"
#include "inifile.h"
#include "main.h"
#include "um.h"
#include "gettext.h"
#include "mainloop.h"

gboolean output_byteswap_flag;
static gchar *output_byteswap_buffer=NULL;
static guint output_byteswap_bufsize=0;
gboolean sound_lock_driver;
static gboolean sound_delayed_quit=FALSE;
static Dataformat playing_format;
gboolean output_stereo_flag;
static GVoidFunc output_ready_func=NULL,input_ready_func=NULL;
static char zerobuf[1024];
static int output_play_count;
static gboolean output_want_data_cached;

#ifdef HAVE_ALSALIB
  #include "sound-alsalib.c"
#endif

#ifdef HAVE_OSS
  #include "sound-oss.c"
#endif

#ifdef HAVE_JACK
  #include "sound-jack.c"
#endif

#ifdef HAVE_SUN
  #include "sound-sun.c"
#endif

#if defined (HAVE_PORTAUDIO)
  #include "sound-portaudio.c"
#endif

#if defined (HAVE_SDL)
  #include "sound-sdl.c"
#endif

#ifdef HAVE_ESOUND
#include "sound-esound.c"
#endif

#ifdef HAVE_ARTSC
#include "sound-artsc.c"
#endif

#ifdef HAVE_PULSEAUDIO
#include "sound-pulse.c"
#endif

#include "sound-dummy.c"

static GList *input_supported_true(gboolean *complete)
{
     *complete = FALSE;
     return NULL;
}

struct sound_driver {
     gchar *name, *id;
     
     void (*preferences)(void);

     gboolean (*init)(gboolean silent);
     void (*quit)(void);

     gint (*output_select_format)(Dataformat *format, gboolean silent, 
				  GVoidFunc ready_func);
     gboolean (*output_want_data)(void);
     guint (*output_play)(gchar *buffer, guint bufsize);
     gboolean (*output_stop)(gboolean);
     void (*output_clear_buffers)(void);
     gboolean (*output_suggest_format)(Dataformat *format, Dataformat *result);
     gboolean (*driver_needs_polling)(void);

     GList *(*input_supported_formats)(gboolean *complete);
     gint (*input_select_format)(Dataformat *format, gboolean silent, 
				 GVoidFunc ready_func);
     void (*input_store)(Ringbuf *buffer);
     void (*input_stop)(void);   
     void (*input_stop_hint)(void);
     int (*input_overrun_count)(void);
};

static struct sound_driver drivers[] = {

#ifdef HAVE_OSS
     { "Open Sound System", "oss", oss_preferences, oss_init, oss_quit, 
       oss_output_select_format,
       oss_output_want_data, oss_output_play, oss_output_stop, 
       oss_output_clear_buffers,NULL,NULL,
       input_supported_true, oss_input_select_format,
       oss_input_store, oss_input_stop },
#endif

#ifdef HAVE_ALSALIB
     { "ALSA", "alsa", alsa_show_preferences, alsa_init, alsa_quit, 
       alsa_output_select_format,
       alsa_output_want_data, alsa_output_play, alsa_output_stop, 
       alsa_output_clear_buffers,NULL,alsa_needs_polling,
       input_supported_true,  
       alsa_input_select_format, alsa_input_store, alsa_input_stop,
       alsa_input_stop_hint,alsa_input_overrun_count },     
#endif

#ifdef HAVE_JACK
     { "JACK", "jack", mhjack_preferences, mhjack_init, mhjack_quit, 
       mhjack_output_select_format,
       mhjack_output_want_data, mhjack_output_play, mhjack_output_stop, 
       mhjack_clear_buffers, mhjack_output_suggest_format,NULL,
       mhjack_input_supported_formats,  
       mhjack_input_select_format, mhjack_input_store, mhjack_input_stop,
       mhjack_input_stop, mhjack_get_xrun_count },
#endif

#ifdef HAVE_SUN
     { "Sun audio", "sun", NULL, sunaud_init, sunaud_quit,
       sunaud_output_select_format, sunaud_output_want_data,
       sunaud_output_play, sunaud_output_stop,
       sunaud_output_clear_buffers,
       sunaud_output_suggest_format, NULL, 
       input_supported_true,sunaud_input_select_format,sunaud_input_store,
       sunaud_input_stop,NULL,sunaud_input_overrun_count },
#endif

#if defined (HAVE_PORTAUDIO)
     { "PortAudio", "pa", NULL, portaudio_init, portaudio_quit, 
       portaudio_output_select_format,
       portaudio_output_want_data, portaudio_output_play, 
       portaudio_output_stop,
       portaudio_output_clear_buffers, 
       NULL, NULL,
       portaudio_input_supported_formats,  
       portaudio_input_select_format, portaudio_input_store, 
       portaudio_input_stop, NULL, portaudio_input_overrun_count }, 
#endif

#if defined (HAVE_SDL)
     { N_("SDL (output only)"), "sdl", NULL, sdl_init, sdl_quit, 
       sdl_output_select_format, 
       sdl_output_want_data, sdl_output_play, sdl_output_stop, 
       sdl_output_clear_buffers, NULL, NULL,
       NULL, NULL, NULL, NULL, NULL },
#endif

#ifdef HAVE_ESOUND

     { "ESound", "esound", esound_preferences, esound_init, esound_quit,
       esound_output_select_format, 
       esound_output_want_data, esound_output_play, esound_output_stop,
       esound_output_clear_buffers, NULL, NULL,
       input_supported_true,  
       esound_input_select_format, esound_input_store, esound_input_stop },

#endif

#ifdef HAVE_ARTSC

     { "aRts", "arts", NULL, mharts_init, mharts_quit,
       mharts_output_select_format, 
       mharts_output_want_data, mharts_output_play, mharts_output_stop,
       mharts_output_clear_buffers, mharts_output_suggest_format, NULL,
       input_supported_true,  
       mharts_input_select_format, mharts_input_store, mharts_input_stop },

#endif

#ifdef HAVE_PULSEAUDIO

     { "PulseAudio", "pulse", pulse_preferences, pulse_init, pulse_quit,
       pulse_output_select_format, pulse_output_want_data, pulse_output_play,
       pulse_output_stop, pulse_output_clear_buffers, NULL, 
       pulse_needs_polling, 
       pulse_input_supported_formats,
       pulse_input_select_format,pulse_input_store,pulse_input_stop,NULL,
       pulse_input_overrun_count
     },
       
#endif

     { N_("Dummy (no sound)"), "dummy", NULL, dummy_init, dummy_quit,
       dummy_output_select_format, 
       dummy_output_want_data, dummy_output_play, dummy_output_stop,
       dummy_output_clear_buffers, NULL, NULL,
       dummy_input_supported_formats,
       dummy_input_select_format, dummy_input_store, dummy_input_stop }

};

static guint current_driver = 0;

/* Auto-detection order. */

static gchar *autodetect_order[] = { 
     /* Sound servers. These must auto-detect properly */
     "jack", "pulse", "esound", "arts", 
     /* "Direct" API:s that don't auto-detect properly. 
      * If compiled in they probably work. */
     "alsa", "sun", 
     /* "Direct" API:s that may or may not work and doesn't autodetect 
      * properly */
     "oss", 
     /* Drivers that shouldn't be used unless everything else fails */
     "pa", "sdl", "dummy", 
};

gchar *sound_driver_name(void)
{
     return _(drivers[current_driver].name);
}

gchar *sound_driver_id(void)
{
     return drivers[current_driver].id;
}

int sound_driver_index(void)
{
     return current_driver;
}

GList *sound_driver_valid_names(void)
{
    GList *l = NULL;
    int i;
    for (i=0; i<ARRAY_LENGTH(drivers); i++)
        l = g_list_append(l, _(drivers[i].name));
    return l;
}

gchar *sound_driver_id_from_name(gchar *name)
{
    int i;
    for (i=0; i<ARRAY_LENGTH(drivers); i++)
        if (!strcmp(_(drivers[i].name),name))
            return drivers[i].id;
    return NULL;
}

gchar *sound_driver_id_from_index(int index)
{
     return drivers[index].id;
}

gchar *sound_driver_name_from_id(gchar *id)
{
    int i;
    for (i=0; i<ARRAY_LENGTH(drivers); i++)
        if (!strcmp(drivers[i].id,id))
            return _(drivers[i].name);
    return NULL;
}

gboolean sound_driver_has_preferences(gchar *id)
{
     int i;

     if (!id) return (drivers[current_driver].preferences != NULL);

     for (i=0; i<ARRAY_LENGTH(drivers); i++)
	  if (!strcmp(drivers[i].id,id))
	       return (drivers[i].preferences != NULL);
     g_assert_not_reached();
     return FALSE;
}

void sound_driver_show_preferences(gchar *id)
{
     int i;

     if ((!id) && sound_driver_has_preferences(id)) 
	  drivers[current_driver].preferences();

     for (i=0; i<ARRAY_LENGTH(drivers); i++)
	  if (!strcmp(drivers[i].id,id))
	       drivers[i].preferences();
}

static void sound_init_driver(char *name)
{
     gchar *d,**p;
     int i;

     /* Handle auto-detection */
     if (!strcmp(name,"auto")) {
	  for (p=autodetect_order; ; p++) {
	       for (i=0; i<ARRAY_LENGTH(drivers) && 
			 strcmp(drivers[i].id,*p); i++) { }
	       if (i == ARRAY_LENGTH(drivers)) continue;
	       if (drivers[i].init(TRUE)) {
		    current_driver = i;
		    return;
	       }
	  }
	  g_assert_not_reached();
     }

     /* Set current_driver */
     for (i=0; i<ARRAY_LENGTH(drivers); i++) {
	  if (!strcmp(drivers[i].id,name)) {
	       current_driver = i;
	       break;
	  }
     }
     if (i == ARRAY_LENGTH(drivers)) {
	  d = g_strdup_printf(_("Invalid driver name: %s\nUsing '%s' driver "
				"instead"),name,drivers[0].name);
	  user_error(d);
	  current_driver = 0;
     }

     drivers[current_driver].init(FALSE);
}

void sound_init(void)
{
     gchar *c;
     sound_lock_driver = inifile_get_gboolean("soundLock",FALSE);
     output_byteswap_flag = inifile_get_gboolean("outputByteswap",FALSE);
     output_stereo_flag = inifile_get_gboolean("outputStereo",FALSE);
     if (driver_option != NULL)
	  c = driver_option;
     else {
	  c = inifile_get(INI_SETTING_SOUNDDRIVER, DEFAULT_DRIVER);
	  if (!strcmp(c,"default")) c = drivers[0].id;
     }

     sound_init_driver(c);

     /* Add sound_poll to main loop */
     mainloop_constant_source_add((constsource_cb)sound_poll,NULL,FALSE);

}

static void delayed_output_stop(void)
{
     drivers[current_driver].output_stop(FALSE);
     sound_delayed_quit=FALSE;
}

void sound_quit(void)
{
     if (sound_delayed_quit) delayed_output_stop();
     drivers[current_driver].quit();
}

static void sound_output_ready_func(void);
static gboolean sound_select_in_progress = FALSE;

gboolean sound_poll(void)
{
     int i=0;
     static int last_playcount;

     if (sound_select_in_progress) return -1;

     if (drivers[current_driver].driver_needs_polling!=NULL &&
	 !drivers[current_driver].driver_needs_polling() )
	  return -1;

     if (output_ready_func==NULL && input_ready_func==NULL)
	  return -1;

     if ((output_ready_func!=NULL || sound_delayed_quit) && 
	 output_want_data() && last_playcount != output_play_count) {
	  last_playcount = output_play_count;
	  sound_output_ready_func();
	  i=1;
     }
     if (input_ready_func != NULL) {
	  input_ready_func();
	  i=0;
     }

     return i;
}

/* We need to filter the output ready event if delayed stop mode is enabled */
static void sound_output_ready_func(void)
{
     output_want_data_cached = TRUE;
     if (sound_delayed_quit) {
	  while (output_want_data()) {
	       output_play(zerobuf,
			   sizeof(zerobuf)-
			   (sizeof(zerobuf)%playing_format.samplebytes));
	  }
     } else if (output_ready_func != NULL && output_want_data()) {
	  output_ready_func();
     } else if (output_ready_func != NULL) {
	  puts("output_ready_func called but !output_want_data");
     }
}

gint output_select_format(Dataformat *format, gboolean silent, 
			  GVoidFunc ready_func)
{
     gint i;

     if (output_stereo_flag && format->channels==1) return -1;

     if (sound_delayed_quit) {
	  if (dataformat_equal(format,&playing_format)) {
	       sound_delayed_quit = FALSE;
	       output_ready_func = ready_func;
	       return FALSE;
	  }
	  else delayed_output_stop();
     }     

     /* We "guard" using this flag to protect against recursive calls
      * to sound_poll (happens if driver calls user_error, for example) */
     sound_select_in_progress = TRUE;

     /* Set up the variables before calling output_select_format, in
      * case the ready callback is called immediately */
     memcpy(&playing_format,format,sizeof(Dataformat));
     output_ready_func = ready_func;
     i = drivers[current_driver].output_select_format(format,silent,
						      sound_output_ready_func);
     if (i != 0) output_ready_func = NULL;

     sound_select_in_progress = FALSE;
     output_play_count++;
     return i;
}

GList *input_supported_formats(gboolean *complete)
{
     if (drivers[current_driver].input_supported_formats != NULL)
	  return drivers[current_driver].input_supported_formats(complete);
     else {
	  *complete = TRUE;
	  return NULL;
     }
}

gboolean input_supported(void)
{
     gboolean b;
     GList *l;
     l = input_supported_formats(&b);
     if (l != NULL) {
	  g_list_foreach(l,(GFunc)g_free,NULL);
	  g_list_free(l);
	  return TRUE;
     } else
	  return !b;     
}

gint input_select_format(Dataformat *format, gboolean silent, 
			 GVoidFunc ready_func)
{
     gint i;
     if (sound_delayed_quit) delayed_output_stop();
     i = drivers[current_driver].input_select_format(format,silent,ready_func);
     if (i == 0)
	  input_ready_func = ready_func;     
     return i;
}

gboolean output_stop(gboolean must_flush)
{
     output_ready_func = NULL;
     if (sound_lock_driver) {
	  if (must_flush)
	       while (output_play(NULL,0) > 0) { }
	  else
	       output_clear_buffers();    
	  sound_delayed_quit=TRUE;
	  return must_flush;
     } else {
	  return drivers[current_driver].output_stop(must_flush);
     }
}

gboolean output_want_data(void)
{
     if (!output_want_data_cached) 
	  output_want_data_cached = drivers[current_driver].output_want_data();
     return output_want_data_cached;
}

guint output_play(gchar *buffer, guint bufsize)
{     
     if (bufsize > 0)
	  output_want_data_cached = FALSE;
     if (output_byteswap_flag) {
	  if (output_byteswap_bufsize < bufsize) {
	       g_free(output_byteswap_buffer);
	       output_byteswap_buffer = g_malloc(bufsize);
	       output_byteswap_bufsize = bufsize;
	  }
	  memcpy(output_byteswap_buffer,buffer,bufsize);
	  byteswap(output_byteswap_buffer,playing_format.samplesize,bufsize);
	  buffer = output_byteswap_buffer;
     }
     if (bufsize > 0) output_play_count++;
     return drivers[current_driver].output_play(buffer,bufsize);
}

gboolean output_suggest_format(Dataformat *format, Dataformat *result)
{
     if (output_stereo_flag && format->channels==1) {
	  memcpy(result,format,sizeof(Dataformat));
	  result->channels = 2;
	  return TRUE;
     }

     if (drivers[current_driver].output_suggest_format)
	  return drivers[current_driver].output_suggest_format(format,result);
     else
	  return FALSE;
}

void input_stop(void)
{
     if (!sound_delayed_quit) 
     	  drivers[current_driver].input_stop();
     input_ready_func = NULL;
}

void input_store(Ringbuf *buffer)
{
     drivers[current_driver].input_store(buffer);
}

void output_clear_buffers(void)
{
     drivers[current_driver].output_clear_buffers();
}

void input_stop_hint(void)
{
     if (drivers[current_driver].input_stop_hint)
	  drivers[current_driver].input_stop_hint();
}

int input_overrun_count(void)
{
     if (drivers[current_driver].input_overrun_count)
	  return drivers[current_driver].input_overrun_count();
     else
	  return -1;
}
