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


#include <config.h>

#include "rateest.h"

#define BIN_SEC 0.25
#define BIN_USEC 250000
#define BIN_SEC_DIV 4

#define LOW_RATIO_THRESHOLD 0.7
#define HIGH_RATIO_THRESHOLD 1.3

/* The best previous estimate we got so far */
static off_t best_written, best_played;
static gfloat best_seconds;
static gfloat best_discarded_seconds;

/* Total number of frames logged */
static off_t total_written_frames;

/* The value of total_written_frames when the current_* variables were reset */
static off_t current_checkpoint;
/* Time current checkpoint was taken. Also time of last underrun */
static GTimeVal current_checkpoint_time;

/* The current estimation we're building up */
/* Number of frames written for all time intervals that had "normal" usage */
static off_t current_played_frames;
static gfloat current_played_seconds;
/* Number of seconds that were discarded because of too high usage */
static gfloat current_discarded_seconds;

/* Last time period # */
static gboolean is_first_period = TRUE;
static long last_log_period;
/* Amount of data played during this time period */
static off_t current_period_frames;

/* Last value returned by rateest_frames_played */
static off_t last_frames_played_value;


static long calc_current_period(void)
{
     GTimeVal tv;
     g_get_current_time(&tv);
     return tv.tv_sec*BIN_SEC_DIV + tv.tv_usec/BIN_USEC;
}

void rateest_init(guint expected_samplerate)
{
     best_written = best_played = (gfloat)expected_samplerate;
     best_seconds = 1.0;
     total_written_frames = 0;
     current_checkpoint = 0;
     g_get_current_time(&current_checkpoint_time);
     current_played_frames = 0;
     current_played_seconds = 0;
     last_log_period = calc_current_period();
     is_first_period = TRUE;
     current_period_frames = 0;
     last_frames_played_value = 0;     
}

void rateest_log_data(guint frames)
{
     long l;
     gfloat ratio;

     l = calc_current_period();
     if (l != last_log_period) {

	  ratio = ((gfloat)(current_period_frames * BIN_SEC_DIV)) /
	       (best_played / best_seconds);
	  /* printf("Period finished: frames=%d, ratio=%f\n",
	     (int)current_period_frames,ratio); */

	  /* Ignore the first period, since it has unknown length */
	  if (is_first_period) {
	       current_discarded_seconds += current_period_frames / 
		    rateest_real_samplerate();
	  } else if (l < last_log_period || (l-last_log_period > 1) || 
		     ratio < LOW_RATIO_THRESHOLD) {
	       /* Store the current estimate as the best if it is */
	       if (current_played_seconds > best_seconds) {
		    best_written = (gfloat)(total_written_frames-
					    current_checkpoint);
		    best_played = current_played_frames;
		    best_seconds = current_played_seconds;
		    best_discarded_seconds = current_discarded_seconds;
	       }
	       current_checkpoint = total_written_frames;
	       g_get_current_time(&current_checkpoint_time);
	       current_played_frames = 0;
	       current_played_seconds = 0.0;
	       current_discarded_seconds = 0.0;
	  } else if (ratio < HIGH_RATIO_THRESHOLD) {
	       /* Add this period to the current estimation */
	       current_played_frames += current_period_frames;
	       current_played_seconds += BIN_SEC;
	  } else
	       current_discarded_seconds += BIN_SEC;
	  current_period_frames = 0;
	  last_log_period = l;
     }

     current_period_frames += frames;
     total_written_frames += frames;
}

void rateest_prelog_data(guint frames)
{
     total_written_frames += frames;
}

off_t rateest_frames_written(void)
{
     return total_written_frames;
}

gfloat rateest_real_samplerate(void)
{
     if (best_seconds > current_played_seconds)
	  return ((gfloat)best_played)/best_seconds;
     else
	  return ((gfloat)current_played_frames)/current_played_seconds;
}

off_t rateest_frames_played(void)
{
     GTimeVal tv,tv2;
     int i;
     gfloat f,sr;
     off_t o,lat;
     /* First, use clock time, checkpoint time and estimated sample
      * rate to calculate position. 
      * If the clock seems broken, use written frames and estimated latency. */
     g_get_current_time(&tv);
     i = timeval_subtract(&tv2,&tv,&current_checkpoint_time);
     sr = rateest_real_samplerate();
     if (i >= 0) {
	  /* printf("tv2: %ld.%6ld\n",tv2.tv_sec,tv2.tv_usec); */
	  f = (gfloat)tv2.tv_usec * 0.000001 + (gfloat)tv2.tv_sec;
	  f *= sr;
	  /* printf("f: %f, sr: %f\n",f,sr); */
	  o = current_checkpoint + (off_t)f;
	  /* printf("First guess: %d, total_written: %d\n",(int)o,
	     (int)total_written_frames); */
	  if (o>total_written_frames) o=total_written_frames;
     }
     if (i<0) {	  
	  if (best_seconds > current_played_seconds)
	       lat = best_written - (best_seconds + best_discarded_seconds)*sr;
	  else
	       lat = (total_written_frames - current_checkpoint) - 
		    (current_played_seconds + current_discarded_seconds)*sr;
	  o = total_written_frames - lat;	  
     }
     if (o > last_frames_played_value) last_frames_played_value = o;
     return last_frames_played_value;
}
