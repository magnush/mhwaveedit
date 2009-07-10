/*
 * Copyright (C) 2005, Magnus Hjorth
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


/* Code for "guessing" the real sample rate and total buffer size
 * without using any special sound driver calls, by logging time
 * and size of succesful writes. */

#ifndef RATEEST_H_INCLUDED
#define RATEEST_H_INCLUDED

#include "main.h"

/* Reset and setup estimator */
void rateest_init(guint expected_samplerate);
/* Call this whenever data has been sent to sound driver */
void rateest_log_data(guint frames);
/* Call this to account for data sent earlier */
void rateest_prelog_data(guint frames); 
/* Number of logged written frames. */
off_t rateest_frames_written(void);
/* Estimate of number of actually played frames. */
off_t rateest_frames_played(void);
/* Estimate of real sample rate */
gfloat rateest_real_samplerate(void);


#endif
