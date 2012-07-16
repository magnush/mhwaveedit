#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if 0
static double stepsize(int samplesize)
{
     static const double tbl_biased[4] = {
	  (1.0/127.5), (1.0/32767.5),
	  (1.0/8388607.5), (1.0/2147483647.5) };
     static const double tbl_scaled[4] = {
	  (1.0/127.0), (1.0/32767.0),
	  (1.0/8388607.0), (1.0/2147483647.0) };

     if (1 == 0) 
	  return tbl_biased[samplesize-1];
     else
	  return tbl_scaled[samplesize-1];
}
#endif

static int test_dither_ampl_f(float amp, int ssize, int mode, int verbose)
{
     float divtbl[8] = { 127.5, 127.0, 32767.5, 32767.0, 
			 8388607.5, 8388607.0, 2147483648.5, 2147483648.0 };
     int x,xv,r,ymax,ymin;
     int imax,imin;
     volatile float fmax,fmin;
     volatile float f,fact,xmax,xmin;
     imax = RAND_MAX/2;
     imin = -RAND_MAX/2;
     fmax = ((float)imax)/((float)RAND_MAX) * amp;
     fmin = ((float)imin)/((float)RAND_MAX) * amp;
     r = (1 << (ssize*8-1));
     if (ssize == 4) r = (1 << 23)+1;
     fact = divtbl[(ssize-1)*2+mode];
     for (x=-r; x<r; x++) {
	  if (ssize==4 && x < (-1 << 23)) {
	       xv = 0x7FFFFFFF;
	       // printf("x: %d\n",r);
	  } else if (ssize == 4 && x > (1 << 23)) {
	       xv = 0x80000000;
	       // printf("x: %d\n",r);
	  } else
	       xv = x;
	  f = (float)xv;
	  if (mode == 0) f += 0.5;
	  f /= fact;
	  xmax = f + fmax;
	  xmin = f + fmin;
	  xmax *= fact;
	  xmin *= fact;
	  if (mode == 0) { xmax -= 0.5; xmin -= 0.5; }
	  ymax = lrintf(xmax);
	  ymin = lrintf(xmin);
	  if (ymax != xv || ymin != xv) {
	       if (verbose)
		    printf("Failed amp=%.10f ssize=%d mode=%d, "
			   "x=%d ymax=%d ymin=%d\n",amp,ssize,mode,xv,
			   ymax,ymin);
	       return -1;
	  }
     }
     if (verbose)
	  printf("Worked amp=%.10f ssize=%d mode=%d, "
		 "x=%d ymax=%d ymin=%d\n",amp,ssize,mode,x,ymax,ymin);
     return 0;
}

static int test_dither_ampl_d(double amp, int ssize, int mode, int verbose)
{
     double divtbl[8] = { 127.5, 127.0, 32767.5, 32767.0, 
			  8388607.5, 8388607.0, 2147483648.5, 2147483648.0 };
     int x,xv,r,ymax,ymin;
     int imax,imin;
     double fmax,fmin;
     double f,fact,xmax,xmin;
     imax = RAND_MAX/2;
     imin = -RAND_MAX/2;
     fmax = ((double)imax)/((double)RAND_MAX) * amp;
     fmin = ((double)imin)/((double)RAND_MAX) * amp;
     r = (1 << (ssize*8-1));
     if (ssize == 4) r = (1 << 23)+1;
     fact = divtbl[(ssize-1)*2+mode];
     // printf("Fact: %.16g\n",fact);
     for (x=-r; x<r; x++) {
	  if (ssize==4 && x < (-1 << 23)) {
	       xv = 0x7FFFFFFF;
	       // printf("x: %d\n",r);
	  } else if (ssize == 4 && x > (1 << 23)) {
	       xv = 0x80000000;
	       // printf("x: %d\n",r);
	  } else 
	       xv = x;
	  f = (double)xv;
	  if (mode == 0) f += 0.5;
	  f /= fact;
	  xmax = f + fmax;
	  xmin = f + fmin;
	  if (xmax > 1.0) xmax = 1.0;
	  xmax *= fact;
	  xmin *= fact;
	  if (mode == 0) { xmax -= 0.5; xmin -= 0.5; }
	  ymax = lrint(xmax);
	  ymin = lrint(xmin);
	  if (ymax != xv || ymin != xv) {
	       if (verbose)
		    printf("Failed amp=%.16g ssize=%d mode=%d, "
			   "x=%d xmax=%.16g xmin=%.16g ymax=%d ymin=%d\n",amp,ssize,mode,xv,
			   xmax,xmin,ymax,ymin);
	       return -1;
	  }
     }     
     if (verbose)
	  printf("Worked amp=%.16g ssize=%d mode=%d, "
		 "x=%d ymax=%d ymin=%d\n",amp,ssize,mode,x,ymax,ymin);
     return 0;
}

static float search_dither_amp_f(int ssize, int mode, float start, int verbose)
{
     float min,max,med;
     int i;
     i = test_dither_ampl_f(start,ssize,mode,verbose);
     if (i == 0) { min=start; max=1.0; } else { min=0.0; max=start; }
     while (1) {
	  med = (min+max)/2.0;
	  if (med <= min || med >= max)
	       med = nextafterf(min,max);	  
	  if (med >= max) return min;
	  i = test_dither_ampl_f(med,ssize,mode,verbose);
	  if (i == 0) min=med; else max=med;
     }     
}

static double search_dither_amp_d(int ssize, int mode, double start, int verbose)
{
     double min,max,med;
     int i;
     i = test_dither_ampl_d(start,ssize,mode,verbose);
     if (i == 0) { min=start; max=1.0; } else { min=0.0; max=start; }
     while (1) {
	  med = (min+max)/2.0;
	  if (med <= min || med >= max)
	       med = nextafter(min,max);	  
	  if (med >= max) return min;
	  i = test_dither_ampl_d(med,ssize,mode,verbose);
	  if (i == 0) min=med; else max=med;
     }     
}

int main(int argc, char **argv)
{
     float f;
     double d;
     
     // test_dither_ampl_f(0.0078738, 1, 1);
     f = search_dither_amp_f(1,0,0.007,0);
     printf("Float, 8-bit biased: %.7g\n",f);
     f = search_dither_amp_f(1,1,0.007,0);
     printf("Float, 8-bit pure-scaled: %.7g\n",f);
     f = search_dither_amp_f(2,0,0.00003,0);
     printf("Float, 16-bit biased: %.7g\n",f);
     f = search_dither_amp_f(2,1,0.00003,0);
     printf("Float, 16-bit pure-scaled: %.7g\n",f);
     f = search_dither_amp_f(3,0,5.9604e-8,0);
     printf("Float, 24-bit biased: %.7g\n",f);
     f = search_dither_amp_f(3,1,5.9604e-8,0);
     printf("Float, 24-bit pure-scaled: %.7g\n",f);
     f = search_dither_amp_f(4,0,2.3283e-10,0);
     printf("Float, 32-bit biased: %.7g\n",f);
     f = search_dither_amp_f(4,1,2.3283e-10,0);
     printf("Float, 32-bit pure-scaled: %.7g\n",f);
     
     d = search_dither_amp_d(1,0,0.007,0);
     printf("Double, 8-bit biased: %.16g\n",d);
     d = search_dither_amp_d(1,1,0.007,0);
     printf("Double, 8-bit pure-scaled: %.16g\n",d);
     d = search_dither_amp_d(2,0,0.00003,0);
     printf("Double, 16-bit biased: %.16g\n",d);
     d = search_dither_amp_d(2,1,0.00003,0);
     printf("Double, 16-bit pure-scaled: %.16g\n",d);
     d = search_dither_amp_d(3,0,2.0e-7,0);
     printf("Double, 24-bit biased: %.16g\n",d);
     d = search_dither_amp_d(3,1,5.9604e-8,0);
     printf("Double, 24-bit pure-scaled: %.16g\n",d);
     d = search_dither_amp_d(4,0,2.3283e-10,0);
     printf("Double, 32-bit biased: %.16g\n",d);
     d = search_dither_amp_d(4,1,2.3283e-10,0);
     printf("Double, 32-bit pure-scaled: %.16g\n",d);
     


#if 0
     int i;
     float f;
     /* i = (rand()/2 - rand()/2); */
     i = RAND_MAX/2+100;
     printf("random: %d (RAND_MAX=%d)\n",i,RAND_MAX);
     f = ((float)i)/((float)RAND_MAX);
     printf("%f\n",f);
     f *= (float)stepsize(2);
     printf("%f\n",f);
     f *= 32767.0;
     printf("%f\n",f);
     i = lrintf(f);
     printf("%d\n",i);
#endif
     return 0;     
}
