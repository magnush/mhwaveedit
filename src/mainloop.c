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


#include <config.h>


#include <gtk/gtk.h>
#include "mainloop.h"
#include "sound.h"
#include "statusbar.h"
#include "document.h"
#include "mainwindow.h"

gboolean idle_work_flag;

void mainloop(void)
{
     static guint player_count=0;
     gint i;
     i = sound_poll();
     if (i > 0) { player_count=0; return; }
     player_count++;
     if (gtk_events_pending()) {
	  gtk_main_iteration();
	  return;
     }
     if (!idle_work_flag) {
	  idle_work_flag = TRUE;
	  return;
     }
     if (status_bar_progress_count()>0) return;
     document_update_cursors();
     if (chunk_view_autoscroll()) return;
     if (mainwindow_update_caches()) return;
     if (i < 0) {
	  gtk_main_iteration();
     } else if (i >= 0 && player_count > 10)
	  do_yield(TRUE);
}
