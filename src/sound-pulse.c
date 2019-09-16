/*
 * Copyright (C) 2008 2009 2011 2012, Magnus Hjorth
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


#include <poll.h>
#include <pulse/pulseaudio.h>

#include "int_box.h"

#ifndef PA_CHECK_VERSION
#define PA_CHECK_VERSION(a,b,c) (0)
#endif

#undef MLDEBUG

/* ----------------------- */
/* Mainloop API */

struct pulse_api_io_event {
     gpointer iosource;
     int fd;
     pa_io_event_flags_t events;
     gpointer userdata;
     pa_io_event_cb_t cb;
     pa_io_event_destroy_cb_t destroy_cb;
};

struct pulse_api_time_event {
     gpointer timesource;
     pa_time_event_cb_t cb;
     pa_time_event_destroy_cb_t destroy_cb;
     void *userdata;
};

struct pulse_api_defer_event {
     gpointer constsource;
     pa_defer_event_cb_t cb;
     pa_defer_event_destroy_cb_t destroy_cb;
     void *userdata;
};

struct pulse_api_userdata {
     gpointer *sources;
     int sources_cap;
     int sources_len;
};

static pa_mainloop_api *pulse_api(void);

#ifdef MLDEBUG
static void pulse_api_list_sources(struct pulse_api_userdata *ud)
{
     int i;
     printf("sources: ");
     for (i=0; i<ud->sources_len; i++)
	  printf("%p ",ud->sources[i]);
     puts("");
}
#endif

static void pulse_api_source_added(gpointer src, struct pa_mainloop_api *api)
{     
     struct pulse_api_userdata *ud = (struct pulse_api_userdata *)api->userdata;

#ifdef MLDEBUG
     printf("pulse_api_source_added: %p\n",src);
#endif
     if (ud->sources_cap == ud->sources_len) {
	  if (ud->sources_cap == 0)
	       ud->sources_cap = 1;
	  else
	       ud->sources_cap *= 2;	  
	  ud->sources = g_realloc(ud->sources,
				  ud->sources_cap*sizeof(ud->sources[0]));
     }
     ud->sources[ud->sources_len++] = src;
#ifdef MLDEBUG
     pulse_api_list_sources(ud);
#endif
}

static void pulse_api_source_removed(gpointer src, struct pa_mainloop_api *api)
{
     struct pulse_api_userdata *ud = (struct pulse_api_userdata *)api->userdata;
     int i;
#ifdef MLDEBUG
     printf("pulse_api_source_removed: %p\n",src);
#endif
     for (i=0; i<ud->sources_len; i++)
	  if (ud->sources[i] == src) 
	       break;
     for (; i+1<ud->sources_len; i++)
	  ud->sources[i] = ud->sources[i+1];
     ud->sources_len--;

#ifdef MLDEBUG
     pulse_api_list_sources(ud);
#endif
}

static int pa_to_poll(pa_io_event_flags_t events)
{
     int i = 0;
     if (events & PA_IO_EVENT_INPUT) i |= POLLIN;
     if (events & PA_IO_EVENT_OUTPUT) i |= POLLOUT;
     if (events & PA_IO_EVENT_HANGUP) i |= POLLHUP;
     if (events & PA_IO_EVENT_ERROR) i |= POLLERR;
     return i;
}

static pa_io_event_flags_t poll_to_pa(int events)
{
     pa_io_event_flags_t e = PA_IO_EVENT_NULL;
     if (events & POLLIN) e |= PA_IO_EVENT_INPUT;
     if (events & POLLOUT) e |= PA_IO_EVENT_OUTPUT;
     if (events & POLLHUP) e |= PA_IO_EVENT_HANGUP;
     if (events & PA_IO_EVENT_ERROR) e |= POLLERR;
     return e;
}

static void pulse_api_io_cb(gpointer iosource, int fd, gushort revents,
			    gpointer user_data)
{
     struct pulse_api_io_event *e = user_data;
     /* puts("I/O event triggered"); */
     e->cb(pulse_api(),(pa_io_event *)e,fd,poll_to_pa(revents),e->userdata);
}

static pa_io_event *pulse_api_io_new(pa_mainloop_api *a, int fd, 
				     pa_io_event_flags_t events, 
				     pa_io_event_cb_t cb,
				     void *userdata)
{
     struct pulse_api_io_event *e;

#ifdef MLDEBUG
     printf("Adding I/O event, fd=%d, events=%d\n",fd,events);
#endif
     e = g_malloc(sizeof(*e));
     e->userdata = userdata;
     e->fd = fd;
     e->events = events;
     e->cb = cb;
     e->destroy_cb = NULL;
     e->iosource = mainloop_io_source_add(fd, pa_to_poll(events),
					  pulse_api_io_cb, e);
     pulse_api_source_added(e->iosource,a);
     return (pa_io_event *)e;     
}

static void pulse_api_io_enable(pa_io_event *e, pa_io_event_flags_t events)
{
     struct pulse_api_io_event *es = (struct pulse_api_io_event *)e;

#ifdef MLDEBUG
     printf("IO event set enable, fd=%d ,events=%d, es->events=%d\n",
	    es->fd,events,es->events);
#endif
     mainloop_io_source_set_events(es->iosource,pa_to_poll(events));
}

static void pulse_api_io_free(pa_io_event *e)
{
     struct pulse_api_io_event *es = (struct pulse_api_io_event *)e;
#ifdef MLDEBUG
     printf("Removing IO event (fd %d)\n",es->fd);
#endif
     mainloop_io_source_free(es->iosource);
     pulse_api_source_removed(es->iosource, pulse_api());
     if (es->destroy_cb) es->destroy_cb(pulse_api(),e,es->userdata);
     
     g_free(es);
}

static void pulse_api_io_set_destroy(pa_io_event *e, 
				     pa_io_event_destroy_cb_t cb)
{
     struct pulse_api_io_event *es = (struct pulse_api_io_event *)e;
     es->destroy_cb = cb;
}

static gint pa_api_time_new_cb(gpointer timesource, GTimeVal *current_time,
			       gpointer user_data)
{
     struct pulse_api_time_event *es = 
	  (struct pulse_api_time_event *)user_data;
     struct timeval tv;
     /* puts("Time event triggered"); */
     tv.tv_sec = current_time->tv_sec;
     tv.tv_usec = current_time->tv_usec;
     es->cb(pulse_api(),(pa_time_event *)es,&tv,es->userdata);
     return 0;
}

static pa_time_event *pulse_api_time_new(pa_mainloop_api *a, 
					 const struct timeval *tv,
					 pa_time_event_cb_t cb,
					 void *userdata)
{
     struct pulse_api_time_event *es;
     GTimeVal gtv;

#ifdef MLDEBUG
     GTimeVal temp_tv;

     g_get_current_time(&temp_tv);     
     printf("Adding time event, triggers in %d s %d us\n",
	    tv->tv_sec-temp_tv.tv_sec, tv->tv_usec-temp_tv.tv_usec);
#endif

     es = g_malloc(sizeof(*es));
     es->cb = cb;
     es->destroy_cb = NULL;
     es->userdata = userdata;
     gtv.tv_sec = tv->tv_sec;
     gtv.tv_usec = tv->tv_usec;
     es->timesource = mainloop_time_source_add(&gtv,pa_api_time_new_cb,es);
     pulse_api_source_added(es->timesource, a);
     return (pa_time_event *)es;
}

static void pulse_api_time_restart(pa_time_event *e, const struct timeval *tv)
{
     struct pulse_api_time_event *es = (struct pulse_api_time_event *)e;
     GTimeVal gtv;

     /*
     GTimeVal temp_tv;
     g_get_current_time(&temp_tv);
     printf("Restarting time event, triggers in %d s %d us\n",
	tv->tv_sec-temp_tv.tv_sec, tv->tv_usec-temp_tv.tv_usec); */

     gtv.tv_sec = tv->tv_sec;
     gtv.tv_usec = tv->tv_usec;
     mainloop_time_source_restart(es->timesource,&gtv);
}

static void pulse_api_time_free(pa_time_event *e)
{
     struct pulse_api_time_event *es = (struct pulse_api_time_event *)e;
#ifdef MLDEBUG
     puts("Removing time event");
#endif
     mainloop_time_source_free(es->timesource);
     if (es->destroy_cb) es->destroy_cb(pulse_api(),e,es->userdata);
     pulse_api_source_removed(es->timesource, pulse_api());
     g_free(es);
}

static void pulse_api_time_set_destroy(pa_time_event *e, 
				       pa_time_event_destroy_cb_t cb)
{
     struct pulse_api_time_event *es = (struct pulse_api_time_event *)e;
     es->destroy_cb = cb;
}

static int pulse_api_defer_new_cb(gpointer csource, gpointer user_data)
{
     struct pulse_api_defer_event *es = user_data;
#ifdef MLDEBUG
     puts("Defer event triggered");
#endif
     es->cb(pulse_api(),(pa_defer_event *)es,es->userdata);
     return 0;
}

static pa_defer_event *pulse_api_defer_new(pa_mainloop_api *a, 
					   pa_defer_event_cb_t cb,
					   void *userdata)
{
     struct pulse_api_defer_event *es;

#ifdef MLDEBUG     
     puts("Adding defer event");
#endif
     es = g_malloc(sizeof(*es));
     es->cb = cb;
     es->destroy_cb = NULL;
     es->userdata = userdata;
     es->constsource = mainloop_constant_source_add(pulse_api_defer_new_cb,
						    es, FALSE);
     pulse_api_source_added(es->constsource,a);
     return (pa_defer_event *)es;
}

static void pulse_api_defer_enable(pa_defer_event *e, int b)
{
     struct pulse_api_defer_event *es = (struct pulse_api_defer_event *)e;
#ifdef MLDEBUG
     printf("Defer event set enabled=%d\n",b);
#endif
     mainloop_constant_source_enable(es->constsource, b);
}

static void pulse_api_defer_free(pa_defer_event *e)
{
     struct pulse_api_defer_event *es = (struct pulse_api_defer_event *)e;
#ifdef MLDEBUG
     puts("Removing defer event");
#endif
     mainloop_constant_source_free(es->constsource);
     if (es->destroy_cb) es->destroy_cb(pulse_api(),e,es->userdata);
     pulse_api_source_removed(es->constsource,pulse_api());
     g_free(es);
}

static void pulse_api_defer_set_destroy(pa_defer_event *e, 
					pa_defer_event_destroy_cb_t cb)
{
     struct pulse_api_defer_event *es = (struct pulse_api_defer_event *)e;
     es->destroy_cb = cb;
}

static void pulse_api_quit(pa_mainloop_api *a, int retval)
{
}

static pa_mainloop_api *pulse_api(void)
{
     static gboolean api_setup = FALSE;
     static struct pa_mainloop_api api;
     struct pulse_api_userdata *ud;

     if (!api_setup) {
	  api.io_new = pulse_api_io_new;
	  api.io_enable = pulse_api_io_enable;
	  api.io_free = pulse_api_io_free;
	  api.io_set_destroy = pulse_api_io_set_destroy;
	  api.time_new = pulse_api_time_new;
	  api.time_restart = pulse_api_time_restart;
	  api.time_free = pulse_api_time_free;
	  api.time_set_destroy = pulse_api_time_set_destroy;
	  api.defer_new = pulse_api_defer_new;
	  api.defer_enable = pulse_api_defer_enable;
	  api.defer_free = pulse_api_defer_free;
	  api.defer_set_destroy = pulse_api_defer_set_destroy;
	  api.quit = pulse_api_quit;

	  ud = g_malloc0(sizeof(*ud));
	  api.userdata = ud;

	  api_setup = TRUE;
     }

     return &api;
}

static void pulse_api_block(void)
{
     gpointer *srcp;
     struct pulse_api_userdata *ud = 
	  (struct pulse_api_userdata *)(pulse_api()->userdata);
#ifdef MLDEBUG
     puts("pulse_api_block");
     pulse_api_list_sources(ud);
#endif

     srcp = g_malloc(ud->sources_len * sizeof(ud->sources[0]));
     memcpy(srcp,ud->sources,ud->sources_len*sizeof(ud->sources[0]));
     mainloop_recurse_on(srcp, ud->sources_len);
     g_free(srcp);
}


/* --------------------------------
 *  Driver core
 */

static struct {
     pa_context *ctx;
     pa_context_state_t ctx_state;
     int ctx_errno;
     pa_stream *stream;
     pa_stream_state_t stream_state;
     pa_sample_spec stream_sspec;
     pa_buffer_attr stream_attr;
     pa_stream_flags_t stream_flags;
     gboolean record_flag;
     const char *record_data;
     size_t record_bytes,record_pos;
     gint overflow_count,overflow_report_count;
     gboolean flush_state; /* 0 = no flush requested yet, 1 = waiting, 2 = done */
     gboolean recursing_mainloop;
     gpointer ready_constsource;
     GVoidFunc ready_func;
     gboolean stopping;
} pulse_data = { 0 };

static void pulse_context_state_cb(pa_context *c, void *userdata)
{
     g_assert(pulse_data.ctx == c);

     pulse_data.ctx_state = pa_context_get_state(c);
     /* printf("Context state change to: %d\n",pulse_data.ctx_state); */

     if (pulse_data.ctx_state == PA_CONTEXT_FAILED ||
	 pulse_data.ctx_state == PA_CONTEXT_TERMINATED) {
	  pulse_data.ctx_errno = pa_context_errno(pulse_data.ctx);
	  pa_context_unref(pulse_data.ctx);
	  pulse_data.ctx = NULL;
     }

}

static gboolean pulse_connect(gboolean autospawn, gboolean silent)
{
     gchar *c;
     int i;
     if (pulse_data.ctx != NULL) return TRUE;
     pulse_data.ctx = pa_context_new(pulse_api(),"mhwaveedit");
     g_assert(pulse_data.ctx != NULL);
     pulse_data.ctx_state = PA_CONTEXT_UNCONNECTED;
     pa_context_set_state_callback(pulse_data.ctx,pulse_context_state_cb,
				   NULL);
     i = pa_context_connect(pulse_data.ctx, NULL,
			    autospawn?0:PA_CONTEXT_NOAUTOSPAWN, NULL);
     g_assert(i == 0 || pulse_data.ctx == NULL);

     while (pulse_data.ctx_state != PA_CONTEXT_READY &&
	    pulse_data.ctx_state != PA_CONTEXT_FAILED &&
	    pulse_data.ctx_state != PA_CONTEXT_TERMINATED)
	  pulse_api_block();

     if (!silent && pulse_data.ctx_state != PA_CONTEXT_READY) {
	  c = g_strdup_printf(_("Connection to PulseAudio server failed: %s"),
			      pa_strerror(pulse_data.ctx_errno));
	  user_error(c);
	  g_free(c);
     }

     return (pulse_data.ctx != NULL);
}

static gboolean pulse_init(gboolean silent)
{     
     return pulse_connect(FALSE,silent);
}

static void pulse_quit(void)
{
     if (pulse_data.ctx != NULL) {
	  pa_context_disconnect(pulse_data.ctx);
	  /* Should be unref:d and set to NULL by the state callback */
	  g_assert(pulse_data.ctx == NULL);
     }
}

#if PA_CHECK_VERSION(0,9,15)
#define HAS24
#endif

static gboolean format_to_pulse(Dataformat *format, pa_sample_spec *ss_out)
{
     pa_sample_format_t sf;

     if (format->type == DATAFORMAT_PCM) {
	  if (format->samplesize == 1 && !format->sign)
	       sf = PA_SAMPLE_U8;
	  else if (format->samplesize == 2 && format->sign)
	       sf = (format->bigendian)?PA_SAMPLE_S16BE:PA_SAMPLE_S16LE;
#ifdef HAS24
	  else if (format->samplesize == 3 && format->sign)
	       sf = (format->bigendian)?PA_SAMPLE_S24BE:PA_SAMPLE_S24LE;
#endif
	  else if (format->samplesize == 4 && format->sign) {
	       if (format->packing == 0 || format->packing == 1)
		    sf = (format->bigendian)?PA_SAMPLE_S32BE:PA_SAMPLE_S32LE;
#ifdef HAS24
	       else
		    sf = (format->bigendian)?PA_SAMPLE_S24_32LE:PA_SAMPLE_S24_32BE;
#endif
	  } else
	       return TRUE;
     } else if (format->type == DATAFORMAT_FLOAT && format->samplesize == 4) {
	  if (!ieee_le_compatible && !ieee_be_compatible)
	       return TRUE;
	  else if (format->bigendian)
	       sf = PA_SAMPLE_FLOAT32BE;
	  else
	       sf = PA_SAMPLE_FLOAT32LE;
     } else 
	  return TRUE;

     ss_out->format = sf;
     ss_out->rate = format->samplerate;
     ss_out->channels = format->channels;
     return FALSE;
}

static gboolean pa_format_from_pulse(pa_sample_spec *ss, Dataformat *format_out)
{
     Dataformat f;
     int i = ss->format;

     switch (i) {
     case PA_SAMPLE_U8:
     case PA_SAMPLE_S16LE:
     case PA_SAMPLE_S16BE:
#ifdef HAS24
     case PA_SAMPLE_S24LE:
     case PA_SAMPLE_S24BE:
     case PA_SAMPLE_S24_32LE:
     case PA_SAMPLE_S24_32BE:
#endif
     case PA_SAMPLE_S32LE:
     case PA_SAMPLE_S32BE:
	  f.type = DATAFORMAT_PCM;
	  f.packing = 0;
	  if (i == PA_SAMPLE_U8) 
	       f.samplesize = 1;
	  else if (i == PA_SAMPLE_S16LE || i == PA_SAMPLE_S16BE) {
	       f.samplesize = 2;
#ifdef HAS24
	  } else if (i == PA_SAMPLE_S24LE || i == PA_SAMPLE_S24BE)
	       f.samplesize = 3;
	  else if (i == PA_SAMPLE_S24_32LE || i == PA_SAMPLE_S24_32LE) {
	       f.samplesize = 4;
	       f.packing = 2;
#endif
	  } else
	       f.samplesize = 4;
	  f.sign = !(i == PA_SAMPLE_U8);
	  if (i == PA_SAMPLE_S16BE || i == PA_SAMPLE_S32BE
#ifdef HAS24
	      || i == PA_SAMPLE_S24BE || i == PA_SAMPLE_S24_32BE
#endif
	      )
	       f.bigendian = TRUE;
	  else
	       f.bigendian = FALSE;	  
	  break;
     case PA_SAMPLE_FLOAT32LE:
	  if (!ieee_le_compatible && !ieee_be_compatible) return TRUE;
	  f.type = DATAFORMAT_FLOAT;
	  f.samplesize = 4;
	  f.bigendian = FALSE;
	  break;
     case PA_SAMPLE_FLOAT32BE:
	  if (!ieee_le_compatible && !ieee_be_compatible) return TRUE;
	  f.type = DATAFORMAT_FLOAT;
	  f.samplesize = 4;
	  f.bigendian = TRUE;
	  break;
     default:
	  return TRUE;
     }
     f.channels = ss->channels;
     f.samplebytes = f.samplesize * f.channels;
     f.samplerate = ss->rate;

     memcpy(format_out,&f,sizeof(Dataformat));
     return FALSE;
}


static void pulse_stream_state_cb(pa_stream *p, void *userdata)
{
     g_assert(pulse_data.stream == p);
     pulse_data.stream_state = pa_stream_get_state(p);
     /* printf("Stream state change to: %d\n",pulse_data.stream_state); */
     if (pulse_data.stream_state == PA_STREAM_FAILED ||
	 pulse_data.stream_state == PA_STREAM_TERMINATED) {
	  pa_stream_unref(pulse_data.stream);
	  pulse_data.stream = NULL;
     }
}

static void pulse_overflow_func(pa_stream *p, void *userdata)
{
     pulse_data.overflow_count ++;
}

static int pulse_wait_connect(int silent)
{
     gchar *c;

     pulse_data.recursing_mainloop = TRUE;
     while (pulse_data.stream_state != PA_STREAM_READY &&
	    pulse_data.stream_state != PA_STREAM_FAILED &&
	    pulse_data.stream_state != PA_STREAM_TERMINATED)
	  pulse_api_block();
     pulse_data.recursing_mainloop = FALSE;

     if (!silent && pulse_data.stream_state != PA_STREAM_READY) {
	  c = g_strdup_printf(_("Connection to PulseAudio server failed: %s"),
			      pa_strerror(pa_context_errno(pulse_data.ctx)));
	  user_error(c);
	  g_free(c);
	  return 1;
     }

     return 0;
}

static gint pulse_select_format_main(Dataformat *format, gboolean record,
				     gboolean silent, 
				     pa_stream_request_cb_t ready_func, 
				     gpointer rf_userdata)
{
     pa_sample_spec ss;
     int i;
     guint32 u;

     /* printf("pulse_output_select_format, silent==%d\n",silent); */
     if (format_to_pulse(format,&ss)) return -1;

     if (!pulse_connect(TRUE,silent)) return silent?-1:+1;
     
     g_assert(pulse_data.stream == NULL);
     pulse_data.record_flag = record;
     pulse_data.record_data = NULL;
     pulse_data.stream_state = PA_STREAM_UNCONNECTED;
     pulse_data.stream = pa_stream_new(pulse_data.ctx, "p", &ss, NULL);
     g_assert(pulse_data.stream != NULL);
     memcpy(&pulse_data.stream_sspec, &ss, sizeof(pa_sample_spec));
     pa_stream_set_state_callback(pulse_data.stream, pulse_stream_state_cb,
				  NULL);
     
     if (record) {
	  pa_stream_set_read_callback(pulse_data.stream,ready_func,rf_userdata);
	  pulse_data.overflow_count = pulse_data.overflow_report_count = 0;
	  pa_stream_set_overflow_callback(pulse_data.stream,
					  pulse_overflow_func,NULL);
     } else
	  pa_stream_set_write_callback(pulse_data.stream,ready_func,
				       rf_userdata);

     memset(&pulse_data.stream_attr, 0xFF, sizeof(pulse_data.stream_attr));
     pulse_data.stream_flags = 0;

     if (record) {
	  pulse_data.stream_flags |= PA_STREAM_ADJUST_LATENCY;
	  pulse_data.stream_attr.fragsize = (format->samplerate *
					     format->samplebytes) / 10;
     } else {
	  u = inifile_get_guint32("pulseLatency", 0);
	  if (u != 0) {
	       pulse_data.stream_flags |= PA_STREAM_ADJUST_LATENCY;
	       pulse_data.stream_attr.tlength =
		    (u * format->samplerate * format->samplebytes) / 1000;
	  }
     }

     if (record)
	  i = pa_stream_connect_record(pulse_data.stream,NULL,
				       &pulse_data.stream_attr,
				       pulse_data.stream_flags);
     else
	  i = pa_stream_connect_playback(pulse_data.stream,NULL,
					 &pulse_data.stream_attr,
					 pulse_data.stream_flags,NULL,
					 NULL);     
     
     g_assert(i == 0 || pulse_data.stream == NULL);

     if (pulse_wait_connect(silent)) return 1;

     if (pulse_data.stream_state != PA_STREAM_READY)
	  return -1;
          
     return 0;     
}

static int ready_constsource_cb(gpointer csource, gpointer user_data)
{
     g_assert(pulse_data.stream != NULL);
     mainloop_constant_source_enable(csource, FALSE);
     pulse_data.ready_func();
     return 0;
}

static void pulse_ready_func(pa_stream *p, size_t bytes, void *userdata)
{
     if (!pulse_data.stopping)
          mainloop_constant_source_enable(pulse_data.ready_constsource, TRUE);
}

static void ready_constsource_setup(void)
{
     if (pulse_data.ready_constsource == NULL)
	  pulse_data.ready_constsource = 
	       mainloop_constant_source_add(ready_constsource_cb,NULL,FALSE);
     mainloop_constant_source_enable(pulse_data.ready_constsource, FALSE);
}

static gint pulse_output_select_format(Dataformat *format, gboolean silent,
				       GVoidFunc ready_func)
{
     ready_constsource_setup();
     pulse_data.ready_func = ready_func;
     return pulse_select_format_main(format,FALSE,silent,
				     pulse_ready_func,NULL);
}

static gint pulse_input_select_format(Dataformat *format, gboolean silent,
				      GVoidFunc ready_func)
{
     ready_constsource_setup();
     pulse_data.ready_func = ready_func;
     return pulse_select_format_main(format,TRUE,silent,
				     pulse_ready_func,NULL);
}

static gboolean pulse_output_want_data(void)
{
     size_t s;
     if (pulse_data.stream == NULL) return FALSE;
     s = pa_stream_writable_size(pulse_data.stream);
     return (s > 0);
}

static void pulse_flush_success_cb(pa_stream *s, int success, void *userdata)
{
     gchar *c;
     if (!success) {
	  c = g_strdup_printf(_("Failed to drain stream: %s"),
			      pa_strerror(pa_context_errno(pulse_data.ctx)));
	  user_error(c);
	  g_free(c);
     }
     /* puts("pulse_flush_success_cb"); */
     pulse_data.flush_state = 2;
}

static void pulse_flush_start(void)
{
     pa_operation *o;

     /* puts("pulse_flush_start"); */
     if (pulse_data.flush_state != 0) return;
     pulse_data.flush_state = 1;
     o = pa_stream_drain(pulse_data.stream, pulse_flush_success_cb, 
			 NULL);
     g_assert(o != NULL);
     pa_operation_unref(o);
}

static gboolean pulse_flush_done(void)
{
     return (pulse_data.flush_state == 2);
}

static gboolean pulse_flush_in_progress(void)
{
     return (pulse_data.flush_state == 1);
}

static guint pulse_output_play(gchar *buffer, guint bufsize)
{
     int i;
     size_t s;
     if (pulse_data.stream == NULL) return 0;
     if (buffer == NULL) {
	  g_assert(bufsize == 0);
	  pulse_flush_start();
	  return pulse_flush_done()?0:1;
     }
     s = pa_stream_writable_size(pulse_data.stream);
     if (bufsize > s) bufsize = s;
     i = pa_stream_write(pulse_data.stream, buffer, bufsize, NULL, 0, 
			 PA_SEEK_RELATIVE);
     g_assert(i == 0);
     if (pulse_data.flush_state == 2)
	  pulse_data.flush_state = 0;
     if (bufsize < s) 
	  mainloop_constant_source_enable(pulse_data.ready_constsource,TRUE);
     return bufsize;
}

static void pulse_output_clear_buffers(void)
{
     pa_operation *p;
     int i;
     if (pulse_data.stream == NULL) return;
     if (inifile_get_gboolean("pulseReconnect",TRUE)) {
	  pa_stream_set_write_callback(pulse_data.stream, NULL, NULL);
	  pa_stream_set_state_callback(pulse_data.stream, NULL, NULL);
	  pa_stream_disconnect(pulse_data.stream);
	  pa_stream_unref(pulse_data.stream);
	  pulse_data.stream_state = PA_STREAM_UNCONNECTED;
	  pulse_data.stream = pa_stream_new(pulse_data.ctx, "p", &pulse_data.stream_sspec, NULL);
	  pa_stream_set_state_callback(pulse_data.stream, pulse_stream_state_cb, NULL);
	  pa_stream_set_write_callback(pulse_data.stream,pulse_ready_func, NULL);
	  i = pa_stream_connect_playback(pulse_data.stream,NULL,
					 &pulse_data.stream_attr,
					 pulse_data.stream_flags,NULL,
					 NULL);
	  g_assert(i==0 && pulse_data.stream!=NULL);
	  i = pulse_wait_connect(0);
	  g_assert(pulse_data.stream_state == PA_STREAM_READY ||
		   (i != 0 && pulse_data.stream==NULL) );
     } else {
	  p = pa_stream_flush(pulse_data.stream, NULL, NULL);
	  pa_operation_unref(p);
     }
}

static gboolean pulse_output_stop(gboolean must_flush)
{
     /* puts("pulse_output_stop"); */
     if (pulse_data.ready_constsource != NULL)
	  mainloop_constant_source_enable(pulse_data.ready_constsource,FALSE);
     pulse_data.stopping = 1;

     if (pulse_data.stream != NULL) {
	  if (must_flush || pulse_flush_in_progress()) {

	       pulse_flush_start();

	       pulse_data.recursing_mainloop = TRUE;
	       while (!pulse_flush_done())
		    pulse_api_block();
	       pulse_data.recursing_mainloop = FALSE;
	  }
	  pulse_data.flush_state = 0;
	  
	  pa_stream_disconnect(pulse_data.stream);
	  pulse_data.recursing_mainloop = TRUE;
	  while (pulse_data.stream != NULL)
	       pulse_api_block();
	  pulse_data.recursing_mainloop = FALSE;

     }
     pulse_data.stopping = 0;
     return FALSE;
}

static gboolean pulse_needs_polling(void)
{     
     return FALSE;
}

struct pulse_scb_data {
     pa_sample_spec ss;
     gboolean is_set;
};

static void pulse_source_info_cb(pa_context *c, const pa_source_info *i,
				 int eol, gpointer userdata)
{
     struct pulse_scb_data *dp = (struct pulse_scb_data *)userdata;
     if (eol) return;
     memcpy(&(dp->ss),&(i->sample_spec),sizeof(pa_sample_spec));
     dp->is_set = TRUE;
}


/* PA 0.9.12-? has a bug where passing @DEFAULT_SOURCE@ doesn't work
 * Perform a quite messy workaround, ifdef:d with the
 * DEFUALT_SOURCE_BROKEN macro */

/* Becuase PA didn't introduce PA_MAJOR/MINOR/MICRO macros until 0.9.15, 
 * this will have to be done for the older versions as well... */
#define DEFAULT_SOURCE_BROKEN


#ifdef DEFAULT_SOURCE_BROKEN

static void pulse_output_info_cb(pa_context *c, const pa_source_output_info *i,
				 int eol, void *userdata)
{
     int *idxp = (int *)userdata;
     if (eol || *idxp >= 0) return;
     *idxp = i->source;
}

static int pulse_get_default_source_index(pa_context *c)
{
     pa_stream *str;
     pa_sample_spec ss;
     int i;
     pa_stream_state_t state;
     pa_operation *oper;
     pa_operation_state_t oper_state;
     int idx;
     int srcidx;

     ss.format = PA_SAMPLE_S16LE;
     ss.rate = 44100;
     ss.channels = 2;
     str = pa_stream_new(c,"in_fmt_probe",&ss,NULL);
     g_assert(str != NULL);
     i = pa_stream_connect_record(str,NULL,NULL,0);
     g_assert(i == 0);
     while (1) {
	  state = pa_stream_get_state(str);	  
	  if (state != PA_STREAM_CREATING) break;
	  pulse_api_block();		  
     }
     if (state != PA_STREAM_READY) {
	  printf("pa_stream_connect_record: %s\n",
		 pa_strerror(pa_context_errno(c)));
	  pa_stream_unref(str);
	  return -1;
     }
     idx = pa_stream_get_index(str);
     /*printf("Calling get_source_output_info_by_index with index %d\n",idx);*/
     srcidx = -1;
     oper = pa_context_get_source_output_info(c,idx,
					      pulse_output_info_cb,
					      &srcidx);
     while (1) {
	  oper_state = pa_operation_get_state(oper);
	  if (oper_state != PA_OPERATION_RUNNING) break;
	  pulse_api_block();
     }
     if (srcidx < 0)
	  printf("pa_context_get_output_info_by_name: %s\n",
		 pa_strerror(pa_context_errno(c)));     
     pa_operation_unref(oper);
     pa_stream_disconnect(str);
     pa_stream_unref(str);
     return srcidx;
}

#endif

static pa_operation *pulse_get_default_source_info(pa_context *c, 
						   pa_source_info_cb_t cb, 
						   void *userdata)
{
#ifdef DEFAULT_SOURCE_BROKEN
     int i;
     i = pulse_get_default_source_index(c);     
     if (i < 0) return NULL;
     return pa_context_get_source_info_by_index(c,i,cb,userdata);
#else
     return pa_context_get_source_info_by_name(c,"@DEFAULT_SOURCE@",cb,
					       userdata);
#endif
}

static GList *pulse_input_supported_formats(gboolean *complete)
{
     pa_operation *p;
     struct pulse_scb_data d;
     Dataformat f,*pf;

     if (!pulse_connect(TRUE,TRUE)) {
	  *complete = TRUE;
	  return NULL;
     }
     d.is_set = FALSE;
     p = pulse_get_default_source_info(pulse_data.ctx,
				       pulse_source_info_cb,&d);
     while (p != NULL && pa_operation_get_state(p) == PA_OPERATION_RUNNING)
	  pulse_api_block();
     if (p != NULL) pa_operation_unref(p);

     if (!d.is_set) {
	  /* if (p != NULL) puts("Unexpected p!=NULL"); */
	  
	  printf("get_default_source_info: %s\n",
		 pa_strerror(pa_context_errno(pulse_data.ctx)));
	  *complete = TRUE;
	  return NULL;
     }

     if (pa_format_from_pulse(&d.ss,&f)) {
	  printf("Unsupported input format: %s\n",
		 pa_sample_format_to_string(d.ss.format));
	  *complete = TRUE;
	  return NULL;
     }
          
     pf = g_malloc(sizeof(*pf));
     memcpy(pf,&f,sizeof(Dataformat));
     *complete = TRUE;
     return g_list_append(NULL,pf);
}

static void pulse_input_store(Ringbuf *buf)
{
     int i;
     size_t b;

     if (pulse_data.overflow_report_count < pulse_data.overflow_count) {
	  console_message("Overrun occured!");
	  pulse_data.overflow_report_count = pulse_data.overflow_count;
     }

     if (pulse_data.record_data == NULL) {	  
	  i = pa_stream_peek(pulse_data.stream, 
			     (const void **)&pulse_data.record_data,
			     &pulse_data.record_bytes);
	  if (i != 0) {
	       fprintf(stderr,"mhWaveEdit: pa_stream_peek: %s\n",
		       pa_strerror(pa_context_errno(pulse_data.ctx)));
	       return;
	  }
	  /*
	  printf("pa_stream_peek: i=%d,data=%p,bytes=%d\n",i,
		 pulse_data.record_data,pulse_data.record_bytes);
	  */
	  pulse_data.record_pos = 0;
     }
     
     if (pulse_data.record_data == NULL) return;

     b = ringbuf_enqueue(buf,
			 (gpointer)pulse_data.record_data+pulse_data.record_pos,
			 pulse_data.record_bytes-pulse_data.record_pos);
     pulse_data.record_pos += b;

     if (pulse_data.record_pos >= pulse_data.record_bytes) {
	  g_assert(pulse_data.record_pos == pulse_data.record_bytes);
	  pa_stream_drop(pulse_data.stream);
	  pulse_data.record_data = NULL;
     } else {
	  mainloop_constant_source_enable(pulse_data.ready_constsource,TRUE);
     }

}

static void pulse_input_stop(void)
{
     pulse_data.stopping = TRUE;
     if (pulse_data.ready_constsource != NULL)
	  mainloop_constant_source_enable(pulse_data.ready_constsource,FALSE);

     if (pulse_data.stream != NULL) {

	  g_assert(pulse_data.record_flag);

	  pa_stream_disconnect(pulse_data.stream);
	  pulse_data.recursing_mainloop = TRUE;
	  while (pulse_data.stream != NULL)
	       pulse_api_block();
	  pulse_data.recursing_mainloop = FALSE;
     }
     pulse_data.stopping = FALSE;
}

static int pulse_input_overrun_count(void)
{
     /* Pulse does not report internal overruns in the source,
      * therefore just using the overflow_count could give a false
      * impression */
     return -1;
}



/* --------------------------------
 *  Preferences UI
 */

struct pulse_prefs_ui {
     GtkWidget *wnd;
     GtkToggleButton *reconn;
     Intbox *pblat;
};

static void pulse_prefs_ok(GtkButton *button, gpointer user_data)
{
     struct pulse_prefs_ui *up = (struct pulse_prefs_ui *)user_data;
     if (intbox_check_limit(up->pblat,0,10000,_("playback latency")))
	  return;
     inifile_set_guint32("pulseLatency",up->pblat->val);
     inifile_set_gboolean("pulseReconnect",
			  gtk_toggle_button_get_active(up->reconn));
     gtk_widget_destroy(up->wnd);
}

static void pulse_preferences(void)
{
     GtkWidget *a,*b,*c,*d;
     struct pulse_prefs_ui *up;
     up = g_malloc(sizeof(*up));
     a = gtk_window_new(GTK_WINDOW_DIALOG);
     up->wnd = a;
     gtk_window_set_title(GTK_WINDOW(a),_("PulseAudio Preferences"));
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_window_set_position(GTK_WINDOW(a),GTK_WIN_POS_MOUSE);
     gtk_container_set_border_width(GTK_CONTAINER(a),5);
     gtk_signal_connect_object(GTK_OBJECT(a),"destroy",
			       GTK_SIGNAL_FUNC(g_free),(GtkObject *)up);
     b = gtk_vbox_new(FALSE,5);
     gtk_container_add(GTK_CONTAINER(a),b);
     c = gtk_hbox_new(FALSE,3);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     d = gtk_label_new(_("Playback latency: "));
     gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
     d = intbox_new(inifile_get_guint32("pulseLatency",0));
     up->pblat = INTBOX(d);
     gtk_box_pack_start(GTK_BOX(c),d,TRUE,TRUE,0);
     d = gtk_label_new(_("ms (0 for maximum)"));
     gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
     c = gtk_check_button_new_with_label(_("Reconnect when moving playback"));
     up->reconn = GTK_TOGGLE_BUTTON(c);
     gtk_toggle_button_set_active(up->reconn,inifile_get_gboolean("pulseReconnect",TRUE));
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     c = gtk_hseparator_new();
     gtk_box_pack_end(GTK_BOX(b),c,FALSE,FALSE,0);
     c = gtk_hbutton_box_new();
     gtk_box_pack_end(GTK_BOX(b),c,FALSE,FALSE,0);
     d = gtk_button_new_with_label(_("OK"));
     gtk_signal_connect(GTK_OBJECT(d),"clicked",GTK_SIGNAL_FUNC(pulse_prefs_ok),
			up);
     gtk_container_add(GTK_CONTAINER(c),d);
     d = gtk_button_new_with_label(_("Cancel"));
     gtk_signal_connect_object(GTK_OBJECT(d),"clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT(a));
     gtk_container_add(GTK_CONTAINER(c),d);
     gtk_widget_show_all(a);
}
