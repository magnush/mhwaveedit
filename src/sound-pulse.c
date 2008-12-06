/*
 * Copyright (C) 2008, Magnus Hjorth
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


#include <pulse/pulseaudio.h>

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

static pa_mainloop_api *pulse_api(void);

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

     /* printf("Adding I/O event, fd=%d, events=%d\n",fd,events); */
     e = g_malloc(sizeof(*e));
     e->userdata = userdata;
     e->fd = fd;
     e->events = events;
     e->cb = cb;
     e->destroy_cb = NULL;
     e->iosource = mainloop_io_source_add(fd, pa_to_poll(events),
					  pulse_api_io_cb, e);
     return (pa_io_event *)e;     
}

static void pulse_api_io_enable(pa_io_event *e, pa_io_event_flags_t events)
{
     struct pulse_api_io_event *es = (struct pulse_api_io_event *)e;

     /* printf("IO event set enable, fd=%d ,events=%d\n",es->fd,events); */
          
     if (events == 0) {
	  mainloop_io_source_enable(es->iosource,FALSE);
     } else if (events == es->events) {
	  mainloop_io_source_enable(es->iosource,TRUE);
     } else {
	  mainloop_io_source_free(es->iosource);
	  es->events = events;
	  es->iosource = mainloop_io_source_add(es->fd,pa_to_poll(events),
						pulse_api_io_cb, es);
     }
}

static void pulse_api_io_free(pa_io_event *e)
{
     struct pulse_api_io_event *es = (struct pulse_api_io_event *)e;
     /* printf("Removing IO event (fd %d)\n",es->fd); */
     mainloop_io_source_free(es->iosource);
     if (es->destroy_cb) es->destroy_cb(pulse_api(),e,es->userdata);
     g_free(es);
}

static void pulse_api_io_set_destroy(pa_io_event *e, 
				     pa_io_event_destroy_cb_t cb)
{
     struct pulse_api_io_event *es = (struct pulse_api_io_event *)e;
     es->destroy_cb = cb;
}

static void pa_api_time_new_cb(gpointer timesource, GTimeVal *current_time,
			       gpointer user_data)
{
     struct pulse_api_time_event *es = 
	  (struct pulse_api_time_event *)user_data;
     struct timeval tv;
     /* puts("Time event triggered"); */
     tv.tv_sec = current_time->tv_sec;
     tv.tv_usec = current_time->tv_usec;
     es->cb(pulse_api(),(pa_time_event *)es,&tv,es->userdata);
}

static pa_time_event *pulse_api_time_new(pa_mainloop_api *a, 
					 const struct timeval *tv,
					 pa_time_event_cb_t cb,
					 void *userdata)
{
     struct pulse_api_time_event *es;
     GTimeVal gtv;

     /*
     GTimeVal temp_tv;

     g_get_current_time(&temp_tv);     
     printf("Adding time event, triggers in %d s %d us\n",
	    tv->tv_sec-temp_tv.tv_sec, tv->tv_usec-temp_tv.tv_usec); */

     es = g_malloc(sizeof(*es));
     es->cb = cb;
     es->destroy_cb = NULL;
     es->userdata = userdata;
     gtv.tv_sec = tv->tv_sec;
     gtv.tv_usec = tv->tv_usec;
     es->timesource = mainloop_time_source_add(&gtv,pa_api_time_new_cb,es);
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
     /* puts("Removing time event"); */
     mainloop_time_source_free(es->timesource);
     if (es->destroy_cb) es->destroy_cb(pulse_api(),e,es->userdata);
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
     /* puts("Defer event triggered"); */
     es->cb(pulse_api(),(pa_defer_event *)es,es->userdata);
     return 0;
}

static pa_defer_event *pulse_api_defer_new(pa_mainloop_api *a, 
					   pa_defer_event_cb_t cb,
					   void *userdata)
{
     struct pulse_api_defer_event *es;
     
     /* puts("Adding defer event"); */
     es = g_malloc(sizeof(*es));
     es->cb = cb;
     es->destroy_cb = NULL;
     es->userdata = userdata;
     es->constsource = mainloop_constant_source_add(pulse_api_defer_new_cb,
						    es, FALSE);
     return (pa_defer_event *)es;
}

static void pulse_api_defer_enable(pa_defer_event *e, int b)
{
     struct pulse_api_defer_event *es = (struct pulse_api_defer_event *)e;
     /* printf("Defer event set enabled=%d\n",b); */
     mainloop_constant_source_enable(es->constsource, b);
}

static void pulse_api_defer_free(pa_defer_event *e)
{
     struct pulse_api_defer_event *es = (struct pulse_api_defer_event *)e;
     /* puts("Removing defer event"); */
     mainloop_constant_source_free(es->constsource);
     if (es->destroy_cb) es->destroy_cb(pulse_api(),e,es->userdata);
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

	  api_setup = TRUE;
     }

     return &api;
}


/* --------------------------------
 *  Driver core
 */

struct {
     pa_context *ctx;
     pa_context_state_t ctx_state;
     int ctx_errno;
     pa_stream *stream;
     pa_stream_state_t stream_state;
     gboolean clear_flag;

     gboolean recursing_mainloop;

} pulse_data = { 0 };

static void pulse_context_state_cb(pa_context *c, void *userdata)
{
     g_assert(pulse_data.ctx == c);

     pulse_data.ctx_state = pa_context_get_state(c);
     printf("Context state change to: %d\n",pulse_data.ctx_state);

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
     if (pulse_data.ctx != NULL) return TRUE;
     pulse_data.ctx = pa_context_new(pulse_api(),"mhwaveedit");
     pulse_data.ctx_state = PA_CONTEXT_UNCONNECTED;
     pa_context_set_state_callback(pulse_data.ctx,pulse_context_state_cb,
				   NULL);
     pa_context_connect(pulse_data.ctx, NULL,
			autospawn?0:PA_CONTEXT_NOAUTOSPAWN, NULL);

     while (pulse_data.ctx_state != PA_CONTEXT_READY &&
	    pulse_data.ctx_state != PA_CONTEXT_FAILED &&
	    pulse_data.ctx_state != PA_CONTEXT_TERMINATED)
	  mainloop();

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

static gboolean format_to_pulse(Dataformat *format, pa_sample_spec *ss_out)
{
     pa_sample_format_t sf;

     if (format->type == DATAFORMAT_PCM) {
	  if (format->samplesize == 1 && format->sign == FALSE)
	       sf = PA_SAMPLE_U8;
	  else if (format->samplesize == 2 && format->sign == TRUE)
	       sf = (format->bigendian)?PA_SAMPLE_S16BE:PA_SAMPLE_S16LE;
	  else if (format->samplesize == 4 && format->sign == TRUE)
	       sf = (format->bigendian)?PA_SAMPLE_S32BE:PA_SAMPLE_S32LE;
	  else
	       return TRUE;
     } else if (format->type == DATAFORMAT_FLOAT && format->samplesize == 4) {
	  if (ieee_le_compatible)
	       sf = PA_SAMPLE_FLOAT32LE;
	  else if (ieee_be_compatible)
	       sf = PA_SAMPLE_FLOAT32BE;
	  else
	       return TRUE;
     } else 
	  return TRUE;

     ss_out->format = sf;
     ss_out->rate = format->samplerate;
     ss_out->channels = format->channels;
     return FALSE;
}

static void pulse_stream_state_cb(pa_stream *p, void *userdata)
{
     g_assert(pulse_data.stream == p);
     pulse_data.stream_state = pa_stream_get_state(p);
     printf("Stream state change to: %d\n",pulse_data.stream_state);
     if (pulse_data.stream_state == PA_STREAM_FAILED ||
	 pulse_data.stream_state == PA_STREAM_TERMINATED) {
	  pa_stream_unref(pulse_data.stream);
	  pulse_data.stream = NULL;
     }
}

static gint pulse_output_select_format(Dataformat *format, gboolean silent,
				       GVoidFunc ready_func)
{
     pa_sample_spec ss;
     gchar *c;

     printf("pulse_output_select_format, silent==%d\n",silent);
     if (format_to_pulse(format,&ss)) return -1;

     if (!pulse_connect(TRUE,silent)) return silent?-1:+1;
     
     g_assert(pulse_data.stream == NULL);
     pulse_data.stream_state = PA_STREAM_UNCONNECTED;
     pulse_data.stream = pa_stream_new(pulse_data.ctx, "p", &ss, NULL);
     pa_stream_set_state_callback(pulse_data.stream, pulse_stream_state_cb,
				  NULL);

     pa_stream_set_write_callback(pulse_data.stream,
				  (pa_stream_request_cb_t)ready_func,NULL);


     pa_stream_connect_playback(pulse_data.stream,NULL,NULL,0,NULL,NULL);

     pulse_data.recursing_mainloop = TRUE;
     while (pulse_data.stream_state != PA_STREAM_READY &&
	    pulse_data.stream_state != PA_STREAM_FAILED &&
	    pulse_data.stream_state != PA_STREAM_TERMINATED)
	  mainloop();
     pulse_data.recursing_mainloop = FALSE;

     if (!silent && pulse_data.stream_state != PA_STREAM_READY) {
	  c = g_strdup_printf(_("Connection to PulseAudio server failed: %s"),
			      pa_strerror(pa_context_errno(pulse_data.ctx)));
	  user_error(c);
	  g_free(c);
	  return 1;
     }

     if (pulse_data.stream_state != PA_STREAM_READY)
	  return -1;
          
     return 0;
}

static gboolean pulse_output_want_data(void)
{
     size_t s;
     if (pulse_data.stream == NULL) return FALSE;
     s = pa_stream_writable_size(pulse_data.stream);
     return (s > 0);
}

static guint pulse_output_play(gchar *buffer, guint bufsize)
{
     if (pulse_data.stream == NULL) return 0;
     pa_stream_write(pulse_data.stream, buffer, bufsize, NULL, 0, 
		     pulse_data.clear_flag?PA_SEEK_ABSOLUTE:PA_SEEK_RELATIVE);
     pulse_data.clear_flag = FALSE;
     return bufsize;
}

static void pulse_output_clear_buffers(void)
{
     pulse_data.clear_flag = TRUE;
}

static gboolean pulse_output_stop(gboolean must_flush)
{
     if (pulse_data.stream != NULL)
	  pa_stream_disconnect(pulse_data.stream);
     return FALSE;
}

static gboolean pulse_needs_polling(void)
{     
     return !pulse_data.recursing_mainloop && pulse_output_want_data();
}
