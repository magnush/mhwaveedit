/*
 * Copyright (C) 2004 2007, Magnus Hjorth
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


#ifndef LADSPACORE_H_INCLUDED
#define LADSPACORE_H_INCLUDED

#if defined(HAVE_SYS_LADSPA_H)
#include <ladspa.h>
#else
#include "../ext/ladspa.h"
#endif
#include "chunk.h"

typedef struct {
     gchar *name;
     unsigned long number;
     LADSPA_PortRangeHint prh;

     /* This data should be set before running ladspa_run_effect */

     /* For audio ports only, which channel to map to.
      * Audio input - Which channel of input sound to send here or -1 for
      *               sending a constant value (specified in value). 
      * Audio output - Which channel of output sound to put this in or 
      *                -1 to ignore. */
     int map; 

     /* For output control ports - set after running.
      * For input control ports - value to set.
      * For output audio ports - ignored 
      * For input audio ports - if map == -1, constant to emit */
     float value; 

     /* Data used by ladspa_run_effect */

     float *buffer;

} LadspaPort;

typedef struct {
     gchar *id,*name,*filename;
     guint effect_number;
     gchar *maker,*copyright;
     /* 0 = control input, 1 = control output, 
      * 2 = audio input, 3 = audio output */
     int numports[4];
     LadspaPort *ports[4];

     gboolean keep;
} LadspaEffect;

void ladspa_rescan(void);
LadspaEffect *ladspa_find_effect(gchar *id);
void ladspa_foreach_effect(void (*func)(LadspaEffect *eff));
/* Note: The parameters to this effect are arranged so this function can be
 * used as a callback for mainwindow_effect_manual */
Chunk *ladspa_run_effect(Chunk *chunk, StatusBar *bar, LadspaEffect *f,
			 int dither_mode);

#endif
