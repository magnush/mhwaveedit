/*
 * Copyright (C) 2008 2009 2010, Magnus Hjorth
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

#ifndef MAINLOOP_H_INCLUDED
#define MAINLOOP_H_INCLUDED

#include <gtk/gtk.h>

void mainloop(void);
void mainloop_recurse_on(gpointer *sources, int n_sources);

typedef void (*iosource_cb)(gpointer iosource, int fd, gushort revents, 
			    gpointer user_data);

/* fd is -1 if timeout occurred 
 * Return value: <0=Disable, 0=Wait for additional events, >0=Re-enable all events 
 * For timeout,  <0=Disable watchdog, 0=Restart watchdog and wait for same events, >0=Re-enable all events */
typedef int (*iogroup_cb)(gpointer iogroup, int fd, gushort revents, gpointer user_data);

/* Return value: 0=disable, >0=call again in X millis from current_time
 * <0=call again in X millis from nominal time */ 
typedef gint (*timesource_cb)(gpointer timesource, GTimeVal *current_time, 
			      gpointer user_data);
/* Return value:
 * <0: May sleep
 * 0:  Execute other event sources but do not sleep
 * >0: Do not execute any other event sources 
 */
typedef int (*constsource_cb)(gpointer csource, gpointer user_data);

gpointer mainloop_io_source_add(int fd, gushort events, iosource_cb cb, 
				gpointer user_data);
void mainloop_io_source_set_events(gpointer iosource, gushort new_events);
void mainloop_io_source_enable(gpointer iosource, gboolean enable);
void mainloop_io_source_free(gpointer iosource);

gpointer mainloop_io_group_add(int nfds, GPollFD *pfds, int wdtime_ms, 
			       iogroup_cb cb, gpointer user_data);
void mainloop_io_group_enable(gpointer iogroup, gboolean enable);
void mainloop_io_group_free(gpointer iogroup);

gpointer mainloop_time_source_add(GTimeVal *tv, timesource_cb cb,
				  gpointer user_data);
void mainloop_time_source_restart(gpointer timesource, GTimeVal *new_tv);
void mainloop_time_source_free(gpointer timesource);
gboolean mainloop_time_source_enabled(gpointer timesource);

gpointer mainloop_constant_source_add(constsource_cb cb, gpointer user_data, 
				      gboolean lowprio);
void mainloop_constant_source_enable(gpointer constsource, gboolean enable);
void mainloop_constant_source_free(gpointer constsource);

/* Convenience wrappers */

typedef void (*defer_once_cb)(gpointer user_data);
void mainloop_defer_once(defer_once_cb cb, gint reltime, gpointer user_data);

#endif
