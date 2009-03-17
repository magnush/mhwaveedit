/*
 * Copyright (C) 2002 2003 2004 2005, Magnus Hjorth
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


/* FIXME: Endian-ness stuff! */
/* This piece of code is imported several times into dataformat.c. */

/* Macros that must be defined and are undefined at the end of this file:
 * FTYPE - Floating point sample type
 * OTYPE - Other floating-point sample type
 * C_PCMxy_FLOAT, C_FLOAT_PCMxy - Conversion routine names
 */

#define CASTU32(x) ((guint32)x)
#define CASTS32(x) ((gint32)x)

#if IS_BIGENDIAN == TRUE

#define C_PCM16SNE_FLOAT C_PCM16SBE_FLOAT
#define C_PCM16UNE_FLOAT C_PCM16UBE_FLOAT
#define C_PCM32SNE_FLOAT C_PCM32SBE_FLOAT
#define C_PCM32UNE_FLOAT C_PCM32UBE_FLOAT

#define C_PCM16SOE_FLOAT C_PCM16SLE_FLOAT
#define C_PCM16UOE_FLOAT C_PCM16ULE_FLOAT
#define C_PCM32SOE_FLOAT C_PCM32SLE_FLOAT
#define C_PCM32UOE_FLOAT C_PCM32ULE_FLOAT

#define C_FLOAT_PCM16SNE C_FLOAT_PCM16SBE
#define C_FLOAT_PCM16UNE C_FLOAT_PCM16UBE
#define C_FLOAT_PCM32SNE C_FLOAT_PCM32SBE
#define C_FLOAT_PCM32UNE C_FLOAT_PCM32UBE

#define C_FLOAT_PCM16SOE C_FLOAT_PCM16SLE
#define C_FLOAT_PCM16UOE C_FLOAT_PCM16ULE
#define C_FLOAT_PCM32SOE C_FLOAT_PCM32SLE
#define C_FLOAT_PCM32UOE C_FLOAT_PCM32ULE

static void C_PCM24SLE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l0,l1,l2,m;
     unsigned char *c;
     for (; count>4; count-=4,in+=3,out+=4) {
	  l0 = in[0]; /* d00 d01 d02 d10 */
	  l1 = in[1]; /* d11 d12 d20 d21 */
	  l2 = in[2]; /* d22 d30 d31 d32 */
	  m = ((l0<<8) & 0xFF0000) + ((l0>>8) & 0xFF00) + (l0>>24);
	  m = (m+0x800000) & 0xFFFFFF; /* signed -> unsigned */
	  out[0] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l0 & 0xFF) + ((l1 >> 16) & 0xFF00) + (l1 & 0xFF0000);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[1] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = ((l1<<8) & 0xFF00) + ((l1>>8) & 0xFF) + ((l2>>8) & 0xFF0000);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[2] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l2 << 16) + (l2 & 0xFF00) + ((l2>>16) & 0xFF);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[3] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = l0 + (l1<<8) + (l2<<16);
	  m = (m+0x800000) & 0xFFFFFF;
	  *out = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;	  
     }
}

static void C_PCM24SBE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l0,l1,l2,m;
     unsigned char *c;
     for (; count>4; count-=4,in+=3,out+=4) {
	  l0 = in[0]; /* d02 d01 d00 d12 */
	  l1 = in[1]; /* d11 d10 d22 d21 */
	  l2 = in[2]; /* d20 d32 d31 d30 */
	  m = l0 >> 8;
	  m = (m+0x800000) & 0xFFFFFF; /* signed -> unsigned */
	  out[0] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l0 << 16) + (l1 >> 16);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[1] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l1<<8) + (l2>>24);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[2] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = ((l2 << 16) & 0xFF0000) + (l2 & 0xFF00) + ((l2>>16) & 0xFF);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[3] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = (l0<<16) + (l1<<8) + l2;
	  m = (m+0x800000) & 0xFFFFFF;
	  *out = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;	  
     }
}

static void C_PCM24ULE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l0,l1,l2,m;
     unsigned char *c;
     for (; count>4; count-=4,in+=3,out+=4) {
	  l0 = in[0]; /* d00 d01 d02 d10 */
	  l1 = in[1]; /* d11 d12 d20 d21 */
	  l2 = in[2]; /* d22 d30 d31 d32 */
	  m = ((l0<<8) & 0xFF0000) + ((l0>>8) & 0xFF00) + (l0>>24);
	  out[0] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l0 & 0xFF) + ((l1 >> 16) & 0xFF00) + (l1 & 0xFF0000);
	  out[1] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = ((l1<<8) & 0xFF00) + ((l1>>8) & 0xFF) + ((l2>>8) & 0xFF0000);
	  out[2] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = ((l2 << 16) & 0xFF0000) + (l2 & 0xFF00) + ((l2>>16) & 0xFF);
	  out[3] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = l0 + (l1<<8) + (l2<<16);
	  *out = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;	  
     }
}

static void C_PCM24UBE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l0,l1,l2,m;
     unsigned char *c;
     for (; count>4; count-=4,in+=3,out+=4) {
	  l0 = in[0]; /* d02 d01 d00 d12 */
	  l1 = in[1]; /* d11 d10 d22 d21 */
	  l2 = in[2]; /* d20 d32 d31 d30 */
	  m = l0 >> 8;
	  out[0] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = ((l0 << 16) & 0xFF0000) + (l1 >> 16);
	  out[1] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = ((l1<<8) & 0xFFFF00) + (l2>>24);
	  out[2] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = ((l2 << 16) & 0xFF0000) + (l2 & 0xFF00) + ((l2>>16) & 0xFF);
	  out[3] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = (l0<<16) + (l1<<8) + l2;
	  *out = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;	  
     }
}

#else

#define C_PCM16SNE_FLOAT C_PCM16SLE_FLOAT
#define C_PCM16UNE_FLOAT C_PCM16ULE_FLOAT
#define C_PCM32SNE_FLOAT C_PCM32SLE_FLOAT
#define C_PCM32UNE_FLOAT C_PCM32ULE_FLOAT

#define C_PCM16SOE_FLOAT C_PCM16SBE_FLOAT
#define C_PCM16UOE_FLOAT C_PCM16UBE_FLOAT
#define C_PCM32SOE_FLOAT C_PCM32SBE_FLOAT
#define C_PCM32UOE_FLOAT C_PCM32UBE_FLOAT

#define C_FLOAT_PCM16SNE C_FLOAT_PCM16SLE
#define C_FLOAT_PCM16UNE C_FLOAT_PCM16ULE
#define C_FLOAT_PCM32SNE C_FLOAT_PCM32SLE
#define C_FLOAT_PCM32UNE C_FLOAT_PCM32ULE

#define C_FLOAT_PCM16SOE C_FLOAT_PCM16SBE
#define C_FLOAT_PCM16UOE C_FLOAT_PCM16UBE
#define C_FLOAT_PCM32SOE C_FLOAT_PCM32SBE
#define C_FLOAT_PCM32UOE C_FLOAT_PCM32UBE

static void C_PCM24SLE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l0,l1,l2,m;
     unsigned char *c;
     for (; count>4; count-=4,in+=3,out+=4) {
	  l0 = in[0]; /* d10 d02 d01 d00 */
	  l1 = in[1]; /* d21 d20 d12 d11 */
	  l2 = in[2]; /* d32 d31 d30 d22 */
	  m = l0;
	  m = (m+0x800000) & 0xFFFFFF; /* signed -> unsigned */
	  out[0] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l0 >> 24) + (l1 << 8);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[1] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l1 >> 16) + (l2 << 16);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[2] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l2 >> 8);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[3] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = l0 + (l1<<8) + (l2<<16);
	  m = (m+0x800000) & 0xFFFFFF;
	  *out = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;	  
     }
}

static void C_PCM24SBE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l0,l1,l2,m;
     unsigned char *c;
     for (; count>4; count-=4,in+=3,out+=4) {
	  l0 = in[0]; /* d12 d00 d01 d02 */
	  l1 = in[1]; /* d21 d22 d10 d11 */
	  l2 = in[2]; /* d30 d31 d32 d20 */
	  m = (l0 << 16) + (l0 & 0xFF00) + ((l0 >> 16) & 0xFF);
	  m = (m+0x800000) & 0xFFFFFF; /* signed -> unsigned */
	  out[0] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = ((l0>>8) & 0xFF0000) + ((l1<<8) & 0xFF00) + ((l1>>8) & 0xFF);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[1] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l1 & 0xFF0000) + ((l1>>16) & 0xFF00) + (l2 & 0xFF);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[2] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = ((l2 << 8) & 0xFF0000) + ((l2>>8) & 0xFF00) + ((l2>>24) & 0xFF);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[3] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = (l0<<16) + (l1<<8) + l2;
	  m = (m+0x800000) & 0xFFFFFF;
	  *out = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;	  
     }
}


static void C_PCM24ULE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l0,l1,l2,m;
     unsigned char *c;
     for (; count>4; count-=4,in+=3,out+=4) {
	  l0 = in[0]; /* d10 d02 d01 d00 */
	  l1 = in[1]; /* d21 d20 d12 d11 */
	  l2 = in[2]; /* d32 d31 d30 d22 */
	  m = l0 & 0xFFFFFF;
	  out[0] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l0 >> 24) + ((l1 << 8) & 0xFFFF00);
	  out[1] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l1 >> 16) + ((l2 << 16) & 0xFF0000);
	  out[2] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l2 >> 8);
	  out[3] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = l0 + (l1<<8) + (l2<<16);
	  *out = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;	  
     }
}

static void C_PCM24UBE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l0,l1,l2,m;
     unsigned char *c;
     for (; count>4; count-=4,in+=3,out+=4) {
	  l0 = in[0]; /* d12 d00 d01 d02 */
	  l1 = in[1]; /* d21 d22 d10 d11 */
	  l2 = in[2]; /* d30 d31 d32 d20 */
	  m = ((l0 << 16) & 0xFF0000) + (l0 & 0xFF00) + ((l0 >> 16) & 0xFF);
	  out[0] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = ((l0>>8) & 0xFF0000) + ((l1<<8) & 0xFF00) + ((l1>>8) & 0xFF);
	  out[1] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = (l1 & 0xFF0000) + ((l1>>16) & 0xFF00) + (l2 & 0xFF);
	  out[2] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
	  m = ((l2 << 8) & 0xFF0000) + ((l2>>8) & 0xFF00) + ((l2>>24) & 0xFF);
	  out[3] = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = (l0<<16) + (l1<<8) + l2;
	  *out = ((FTYPE)m - (FTYPE)8388607.5) / (FTYPE)8388607.5;	  
     }
}

#endif



static void C_PCM8S_FLOAT(signed char *in, FTYPE *out, int count)
{
     for (; count>0; count--,in++,out++)
	  *out = (((FTYPE)(*in)) + (FTYPE)0.5) / (FTYPE)127.5;
}

static void C_PCM8U_FLOAT(unsigned char *in, FTYPE *out, int count)
{
     for (; count>0; count--,in++,out++)
	  *out = (((FTYPE)(*in)) - (FTYPE)127.5) / (FTYPE)127.5;
}

static void C_PCM16SNE_FLOAT(signed short *in, FTYPE *out, int count)
{
     /* puts("HELLO!"); */
     for (; count>0; count--,in++,out++)
	  *out = (((FTYPE)(*in)) + (FTYPE)0.5) / (FTYPE)32767.5;     
}

static void C_PCM16SOE_FLOAT(unsigned short *in, FTYPE *out, int count)
{
     unsigned short u;
     signed short i;
     for (; count>0; count--,in++,out++) {
	  u = *in;
	  i = (u >> 8) + (u << 8);
	  *out = (((FTYPE)(i)) + (FTYPE)0.5) / (FTYPE)32767.5;
     }
}

static void C_PCM16UNE_FLOAT(unsigned short *in, FTYPE *out, int count)
{
     for (; count>0; count--,in++,out++)
	  *out = (((FTYPE)(*in)) - (FTYPE)32767.5) / (FTYPE)32767.5;
}

static void C_PCM16UOE_FLOAT(unsigned short *in, FTYPE *out, int count)
{
     unsigned short i;
     for (; count>0; count--,in++,out++) {
	  i = *in;
	  i = (i >> 8) + (i << 8);
	  *out = (((FTYPE)(i)) - (FTYPE)32767.5) / (FTYPE)32767.5;
     }
}

static void C_PCM32SNE_FLOAT(gint32 *in, FTYPE *out, int count)
{
     for (; count>0; count--,in++,out++)
	  *out = (((FTYPE)(*in)) + (FTYPE)0.5) / (FTYPE)2147483647.5;
}

static void C_PCM32SOE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     guint32 u;
     guint32 x;
     gint32 i;     
     g_assert(sizeof(u) == 4 && sizeof(x) == 4 && sizeof(i) == 4);
     for (; count>0; count--,in++,out++) {
	  u = *in;
	  x = (u<<CASTU32(24)) + ((u<<CASTU32(8)) & CASTU32(0xFF0000)) + 
	       ((u>>CASTU32(8)) & CASTU32(0xFF00)) + (u>>CASTU32(24));
	  i = (gint32)x;
	  *out = (((FTYPE)(i)) + (FTYPE)0.5) / (FTYPE)2147483647.5;
     }
}

static void C_PCM32UNE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     for (; count>0; count--,in++,out++)
	  *out = (((FTYPE)(*in)) - (FTYPE)2147483647.5) / (FTYPE)2147483647.5;
}

static void C_PCM32UOE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     guint32 i,x;
     for (; count>0; count--,in++,out++) {
	  i = *in;
	  x = (i<<CASTU32(24)) + ((i<<CASTU32(8)) & CASTU32(0xFF0000)) + 
	       ((i>>CASTU32(8)) & CASTU32(0xFF00)) + (i>>CASTU32(24));
	       
	  *out = (((FTYPE)(x)) - (FTYPE)2147483647.5) / (FTYPE)2147483647.5;
     }
}

static void C_FLOAT_PCM8S(FTYPE *in, signed char *out, int count)
{
     long int l;
     for (; count>0; count--,in++,out++) {
	  l = RINT(((*in) * (FTYPE)127.5) - (FTYPE)0.5);
	  if (l > 127) l = 127; else if (l < -128) l = -128;
	  *out = l;
     }
}

static void C_FLOAT_PCM8U(FTYPE *in, unsigned char *out, int count)
{
     long int l;
     for (; count>0; count--,in++,out++) {
	  l = RINT(((*in) * (FTYPE)127.5) + (FTYPE)127.5);
	  if (l > 255) l = 255; else if (l < 0) l = 0;
	  *out = l;
     }     
}

static void C_FLOAT_PCM16SNE(FTYPE *in, signed short *out, int count)
{
     long int l;
     for (; count>0; count--,in++,out++) {
	  l = RINT(((*in) * (FTYPE)32767.5) - (FTYPE)0.5);
	  if (l > 32767 ) l = 32767; else if (l < -32768) l=-32768;
	  *out = l;
     }
}

static void C_FLOAT_PCM16SOE(FTYPE *in, signed short *out, int count)
{
     C_FLOAT_PCM16SNE(in,out,count);
     byteswap(out,2,count*2);     
}

static void C_FLOAT_PCM16UNE(FTYPE *in, unsigned short *out, int count)
{
     long int l;
     for (; count>0; count--,in++,out++) {
	  l = RINT(((*in) * (FTYPE)32767.5) + (FTYPE)32767.5);
	  if (l > 65535) l = 65535; else if (l < 0) l = 0;
	  *out = l;
     }
}

static void C_FLOAT_PCM16UOE(FTYPE *in, unsigned short *out, int count)
{
     C_FLOAT_PCM16UNE(in,out,count);
     byteswap(out,2,count*2);
}

static void C_FLOAT_PCM24SLE(FTYPE *in, unsigned char *out, int count)
{
     long int l;
     for (; count>0; count--,in++,out+=3) {
	  l = RINT(((*in) * (FTYPE)8388607.5) - (FTYPE)0.5);
	  if (l > 0x7FFFFF) l = 0x7FFFFF; 
	  else if (l < -0x800000) l = -0x800000;
	  out[0] = l & 0xFF;
	  out[1] = (l & 0xFF00) >> 8;
	  out[2] = (l & 0xFF0000) >> 16;
     }
}

static void C_FLOAT_PCM24SBE(FTYPE *in, unsigned char *out, int count)
{
     long int l;
     for (; count>0; count--,in++,out+=3) {
	  l = RINT(((*in) * (FTYPE)8388607.5) - (FTYPE)0.5);
	  if (l > 0x7FFFFF) l = 0x7FFFFF; 
	  else if (l < -0x800000) l = -0x800000;
	  out[0] = (l & 0xFF0000) >> 16;
	  out[1] = (l & 0xFF00) >> 8;
	  out[2] = l & 0xFF;
     }
}

static void C_FLOAT_PCM24ULE(FTYPE *in, unsigned char *out, int count)
{
     long int l;
     for (; count>0; count--,in++,out+=3) {
	  l = RINT(((*in) * (FTYPE)8388607.5) + (FTYPE)8388607.5);
	  if (l > 0xFFFFFF) l = 0xFFFFFF; else if (l < 0) l = 0;
	  out[0] = l & 0xFF;
	  out[1] = (l & 0xFF00) >> 8;
	  out[2] = (l & 0xFF0000) >> 16;
     }
}

static void C_FLOAT_PCM24UBE(FTYPE *in, unsigned char *out, int count)
{
     long int l;
     for (; count>0; count--,in++,out+=3) {
	  l = RINT(((*in) * (FTYPE)8388607.5) + (FTYPE)8388607.5);
	  if (l > 0xFFFFFF) l = 0xFFFFFF; else if (l < 0) l = 0;
	  out[0] = (l & 0xFF0000) >> 16;
	  out[1] = (l & 0xFF00) >> 8;
	  out[2] = l & 0xFF;
     }
}

static void C_FLOAT_PCM32SNE(FTYPE *in, gint32 *out, int count)
{
     FTYPE f;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  if (f >= 1.0)  *out = 0x7FFFFFFF; 
	  else if (f <= -1.0) *out = -0x80000000;
	  else *out = RINT((f * (FTYPE)2147483647.5) - (FTYPE)0.5);
     }
}

static void C_FLOAT_PCM32SOE(FTYPE *in, gint32 *out, int count)
{
     C_FLOAT_PCM32SNE(in,out,count);
     byteswap(out,4,count*4);
}

static void C_FLOAT_PCM32UNE(FTYPE *in, guint32 *out, int count)
{
     FTYPE f;
     long int l;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  if (f >= 1.0)  *out = 0xFFFFFFFF; 
	  else if (f <= -1.0) *out = -0;
	  else {
	       l = RINT((f * (FTYPE)2147483647.5) - (FTYPE)0.5);
	       *out = l + 0x80000000;
	  }
     }
}

static void C_FLOAT_PCM32UOE(FTYPE *in, guint32 *out, int count)
{
     C_FLOAT_PCM32UNE(in,out,count);
     byteswap(out,4,count*4);
}

/* Defined in this file. */
#undef C_PCM16SNE_FLOAT
#undef C_PCM16SOE_FLOAT
#undef C_PCM16UNE_FLOAT
#undef C_PCM16UOE_FLOAT
#undef C_PCM32SNE_FLOAT
#undef C_PCM32SOE_FLOAT
#undef C_PCM32UNE_FLOAT
#undef C_PCM32UOE_FLOAT
#undef C_FLOAT_PCM16SNE
#undef C_FLOAT_PCM16SOE
#undef C_FLOAT_PCM16UNE
#undef C_FLOAT_PCM16UOE
#undef C_FLOAT_PCM32SNE
#undef C_FLOAT_PCM32SOE
#undef C_FLOAT_PCM32UNE
#undef C_FLOAT_PCM32UOE

/* Defined on the outside */
#undef FTYPE
#undef RINT
#undef C_PCM8S_FLOAT
#undef C_PCM8U_FLOAT
#undef C_PCM16SLE_FLOAT
#undef C_PCM16SBE_FLOAT
#undef C_PCM16ULE_FLOAT
#undef C_PCM16UBE_FLOAT
#undef C_PCM24SLE_FLOAT
#undef C_PCM24SBE_FLOAT
#undef C_PCM24ULE_FLOAT
#undef C_PCM24UBE_FLOAT
#undef C_PCM32SLE_FLOAT
#undef C_PCM32SBE_FLOAT
#undef C_PCM32ULE_FLOAT
#undef C_PCM32UBE_FLOAT
#undef C_FLOAT_PCM8S
#undef C_FLOAT_PCM8U
#undef C_FLOAT_PCM16SLE
#undef C_FLOAT_PCM16SBE
#undef C_FLOAT_PCM16ULE
#undef C_FLOAT_PCM16UBE
#undef C_FLOAT_PCM24SLE
#undef C_FLOAT_PCM24SBE
#undef C_FLOAT_PCM24ULE
#undef C_FLOAT_PCM24UBE
#undef C_FLOAT_PCM32SLE
#undef C_FLOAT_PCM32SBE
#undef C_FLOAT_PCM32ULE
#undef C_FLOAT_PCM32UBE
