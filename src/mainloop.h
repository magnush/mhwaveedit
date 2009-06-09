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

#ifndef MAINLOOP_H_INCLUDED
#define MAINLOOP_H_INCLUDED

#include <gtk/gtk.h>

void mainloop(void);
void mainloop_recurse_on(gpointer *sources, int n_sources);

typedef void (*iosource_cb)(gpointer iosource, int fd, gushort revents, 
			    gpointer user_data);
typedef void (*timesource_cb)(gpointer timesource, GTimeVal *current_time, 
			      gpointer user_data);
/* Return value:
 * <0: May sleep
 * 0:  Execute other event sources but do not sleep
 * >0: Do not execute any other event sources 
 */
typedef int (*constsource_cb)(gpointer csource, gpointer user_data);

gpointer mainloop_io_source_add(int fd, gushort events, iosource_cb cb, 
				gpointer user_data);
void mainloop_io_source_enable(gpointer iosource, gboolean enable);
void mainloop_io_source_free(gpointer iosource);

gpointer mainloop_time_source_add(GTimeVal *tv, timesource_cb cb,
				  gpointer user_data);
void mainloop_time_source_restart(gpointer timesource, GTimeVal *new_tv);
void mainloop_time_source_free(gpointer timesource);

gpointer mainloop_constant_source_add(constsource_cb cb, gpointer user_data, 
				      gboolean lowprio);
void mainloop_constant_source_enable(gpointer constsource, gboolean enable);
void mainloop_constant_source_free(gpointer constsource);

#endif
