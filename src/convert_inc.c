/*
 * Copyright (C) 2002 2003 2004 2005 2008 2009 2012, Magnus Hjorth
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


/* FIXME: Endian-ness stuff! */
/* This piece of code is imported several times into dataformat.c. */

/* Macros that must be defined and are undefined at the end of this file:
 * FTYPE - Floating point sample type
 * OTYPE - Other floating-point sample type
 * C_PCMxy_FLOAT, C_FLOAT_PCMxy - Conversion routine names
 * PZMODE - Preserve-zero level mode
 */

#define CASTU32(x) ((guint32)x)
#define CASTS32(x) ((gint32)x)

#define MUL(x,m) (((FTYPE)(x))*((FTYPE)(m)))
#define DIV(x,d) (((FTYPE)(x))/((FTYPE)(d)))
#define MULADD(x,m,a) (((FTYPE)(x))*((FTYPE)(m)) + ((FTYPE)(a)))
#define MULSUB(x,m,a) (((FTYPE)(x))*((FTYPE)(m)) - ((FTYPE)(a)))
#define SUBDIV(x,s,d) ((((FTYPE)(x)) - ((FTYPE)(s))) / ((FTYPE)(d)))
#define ADDDIV(x,s,d) ((((FTYPE)(x)) + ((FTYPE)(s))) / ((FTYPE)(d)))

#ifdef PZMODE

#define NORM8U(x) SUBDIV(x,128.0,127.0)
#define NORM8S(x) DIV(x,127.0)
#define UNNORM8U(x) MULADD(x,127.0,128.0)
#define UNNORM8S(x) MUL(x,127.0)

#define NORM16U(x) SUBDIV(x,32768.0,32767.0)
#define NORM16S(x) DIV(x,32767.0)
#define UNNORM16U(x) MULADD(x,32767.0,32768.0)
#define UNNORM16S(x) MUL(x,32767.0)

#define NORM24U(x) SUBDIV(x,8388608.0,8388607.0)
#define UNNORM24U(x) MULADD(x,8388607.0,8388608.0)
#define UNNORM24S(x) MUL(x,8388607.0)

#define NORM32U(x) SUBDIV(x,2147483648.0,2147483647.0)
#define NORM32S(x) DIV(x,2147483647.0)
#define UNNORM32U(x) MULADD(x,2147483647.0,2147483648.0)
#define UNNORM32S(x) MUL(x,2147483647.0)

#undef PZMODE

#else

#define NORM8U(x) SUBDIV(x,127.5,127.5)
#define NORM8S(x) ADDDIV(x,0.5,127.5)
#define UNNORM8U(x) MULADD(x,127.5,127.5)
#define UNNORM8S(x) MULSUB(x,127.5,0.5)

#define NORM16U(x) SUBDIV(x,32767.5,32767.5)
#define NORM16S(x) ADDDIV(x,0.5,32767.5)
#define UNNORM16U(x) MULADD(x,32767.5,32767.5)
#define UNNORM16S(x) MULSUB(x,32767.5,0.5)

#define NORM24U(x) SUBDIV(x,8388607.5,8388607.5)
#define UNNORM24U(x) MULADD(x,8388607.5,8388607.5)
#define UNNORM24S(x) MULSUB(x,8388607.5,0.5)

#define NORM32U(x) SUBDIV(x,2147483647.5,2147483647.5)
#define NORM32S(x) ADDDIV(x,0.5,2147483647.5)
#define UNNORM32S(x) MULSUB(x,2147483647.5,0.5)

#endif /* ifdef PZMODE */

#if IS_BIGENDIAN == TRUE

#define C_PCM16SNE_FLOAT C_PCM16SBE_FLOAT
#define C_PCM16UNE_FLOAT C_PCM16UBE_FLOAT
#define C_PCM32SNE_FLOAT C_PCM32SBE_FLOAT
#define C_PCM32UNE_FLOAT C_PCM32UBE_FLOAT
#define C_PCM24SNEPM_FLOAT C_PCM24SBEPM_FLOAT
#define C_PCM24UNEPM_FLOAT C_PCM24UBEPM_FLOAT
#define C_PCM24SNEPL_FLOAT C_PCM24SBEPL_FLOAT
#define C_PCM24UNEPL_FLOAT C_PCM24UBEPL_FLOAT

#define C_PCM16SOE_FLOAT C_PCM16SLE_FLOAT
#define C_PCM16UOE_FLOAT C_PCM16ULE_FLOAT
#define C_PCM32SOE_FLOAT C_PCM32SLE_FLOAT
#define C_PCM32UOE_FLOAT C_PCM32ULE_FLOAT
#define C_PCM24SOEPM_FLOAT C_PCM24SLEPM_FLOAT
#define C_PCM24UOEPM_FLOAT C_PCM24ULEPM_FLOAT
#define C_PCM24SOEPL_FLOAT C_PCM24SLEPL_FLOAT
#define C_PCM24UOEPL_FLOAT C_PCM24ULEPL_FLOAT

#define C_FLOAT_PCM16SNE C_FLOAT_PCM16SBE
#define C_FLOAT_PCM16UNE C_FLOAT_PCM16UBE
#define C_FLOAT_PCM32SNE C_FLOAT_PCM32SBE
#define C_FLOAT_PCM32UNE C_FLOAT_PCM32UBE
#define C_FLOAT_PCM24SNEPM C_FLOAT_PCM24SBEPM
#define C_FLOAT_PCM24UNEPM C_FLOAT_PCM24UBEPM
#define C_FLOAT_PCM24SNEPL C_FLOAT_PCM24SBEPL
#define C_FLOAT_PCM24UNEPL C_FLOAT_PCM24UBEPL

#define C_FLOAT_PCM16SOE C_FLOAT_PCM16SLE
#define C_FLOAT_PCM16UOE C_FLOAT_PCM16ULE
#define C_FLOAT_PCM32SOE C_FLOAT_PCM32SLE
#define C_FLOAT_PCM32UOE C_FLOAT_PCM32ULE
#define C_FLOAT_PCM24SOEPM C_FLOAT_PCM24SLEPM
#define C_FLOAT_PCM24UOEPM C_FLOAT_PCM24ULEPM
#define C_FLOAT_PCM24SOEPL C_FLOAT_PCM24SLEPL
#define C_FLOAT_PCM24UOEPL C_FLOAT_PCM24ULEPL

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
	  out[0] = NORM24U(m);
	  m = (l0 & 0xFF) + ((l1 >> 16) & 0xFF00) + (l1 & 0xFF0000);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[1] = NORM24U(m);
	  m = ((l1<<8) & 0xFF00) + ((l1>>8) & 0xFF) + ((l2>>8) & 0xFF0000);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[2] = NORM24U(m);
	  m = (l2 << 16) + (l2 & 0xFF00) + ((l2>>16) & 0xFF);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[3] = NORM24U(m);
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = l0 + (l1<<8) + (l2<<16);
	  m = (m+0x800000) & 0xFFFFFF;
	  *out = NORM24U(m);
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
	  out[0] = NORM24U(m);
	  m = (l0 << 16) + (l1 >> 16);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[1] = NORM24U(m);
	  m = (l1<<8) + (l2>>24);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[2] = NORM24U(m);
	  m = ((l2 << 16) & 0xFF0000) + (l2 & 0xFF00) + ((l2>>16) & 0xFF);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[3] = NORM24U(m);
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = (l0<<16) + (l1<<8) + l2;
	  m = (m+0x800000) & 0xFFFFFF;
	  *out = NORM24U(m);
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
	  out[0] = NORM24U(m);
	  m = (l0 & 0xFF) + ((l1 >> 16) & 0xFF00) + (l1 & 0xFF0000);
	  out[1] = NORM24U(m);
	  m = ((l1<<8) & 0xFF00) + ((l1>>8) & 0xFF) + ((l2>>8) & 0xFF0000);
	  out[2] = NORM24U(m);
	  m = ((l2 << 16) & 0xFF0000) + (l2 & 0xFF00) + ((l2>>16) & 0xFF);
	  out[3] = NORM24U(m);
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = l0 + (l1<<8) + (l2<<16);
	  *out = NORM24U(m);
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
	  out[0] = NORM24U(m);
	  m = ((l0 << 16) & 0xFF0000) + (l1 >> 16);
	  out[1] = NORM24U(m);
	  m = ((l1<<8) & 0xFFFF00) + (l2>>24);
	  out[2] = NORM24U(m);
	  m = ((l2 << 16) & 0xFF0000) + (l2 & 0xFF00) + ((l2>>16) & 0xFF);
	  out[3] = NORM24U(m);
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = (l0<<16) + (l1<<8) + l2;
	  *out = NORM24U(m);
     }
}

#else

#define C_PCM16SNE_FLOAT C_PCM16SLE_FLOAT
#define C_PCM16UNE_FLOAT C_PCM16ULE_FLOAT
#define C_PCM32SNE_FLOAT C_PCM32SLE_FLOAT
#define C_PCM32UNE_FLOAT C_PCM32ULE_FLOAT
#define C_PCM24SNEPM_FLOAT C_PCM24SLEPM_FLOAT
#define C_PCM24UNEPM_FLOAT C_PCM24ULEPM_FLOAT
#define C_PCM24SNEPL_FLOAT C_PCM24SLEPL_FLOAT
#define C_PCM24UNEPL_FLOAT C_PCM24ULEPL_FLOAT

#define C_PCM16SOE_FLOAT C_PCM16SBE_FLOAT
#define C_PCM16UOE_FLOAT C_PCM16UBE_FLOAT
#define C_PCM32SOE_FLOAT C_PCM32SBE_FLOAT
#define C_PCM32UOE_FLOAT C_PCM32UBE_FLOAT
#define C_PCM24SOEPM_FLOAT C_PCM24SBEPM_FLOAT
#define C_PCM24UOEPM_FLOAT C_PCM24UBEPM_FLOAT
#define C_PCM24SOEPL_FLOAT C_PCM24SBEPL_FLOAT
#define C_PCM24UOEPL_FLOAT C_PCM24UBEPL_FLOAT

#define C_FLOAT_PCM16SNE C_FLOAT_PCM16SLE
#define C_FLOAT_PCM16UNE C_FLOAT_PCM16ULE
#define C_FLOAT_PCM32SNE C_FLOAT_PCM32SLE
#define C_FLOAT_PCM32UNE C_FLOAT_PCM32ULE
#define C_FLOAT_PCM24SNEPM C_FLOAT_PCM24SLEPM
#define C_FLOAT_PCM24UNEPM C_FLOAT_PCM24ULEPM
#define C_FLOAT_PCM24SNEPL C_FLOAT_PCM24SLEPL
#define C_FLOAT_PCM24UNEPL C_FLOAT_PCM24ULEPL

#define C_FLOAT_PCM16SOE C_FLOAT_PCM16SBE
#define C_FLOAT_PCM16UOE C_FLOAT_PCM16UBE
#define C_FLOAT_PCM32SOE C_FLOAT_PCM32SBE
#define C_FLOAT_PCM32UOE C_FLOAT_PCM32UBE
#define C_FLOAT_PCM24SOEPM C_FLOAT_PCM24SBEPM
#define C_FLOAT_PCM24UOEPM C_FLOAT_PCM24UBEPM
#define C_FLOAT_PCM24SOEPL C_FLOAT_PCM24SBEPL
#define C_FLOAT_PCM24UOEPL C_FLOAT_PCM24UBEPL

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
	  out[0] = NORM24U(m);
	  m = (l0 >> 24) + (l1 << 8);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[1] = NORM24U(m);
	  m = (l1 >> 16) + (l2 << 16);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[2] = NORM24U(m);
	  m = (l2 >> 8);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[3] = NORM24U(m);
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = l0 + (l1<<8) + (l2<<16);
	  m = (m+0x800000) & 0xFFFFFF;
	  *out = NORM24U(m);
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
	  out[0] = NORM24U(m);
	  m = ((l0>>8) & 0xFF0000) + ((l1<<8) & 0xFF00) + ((l1>>8) & 0xFF);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[1] = NORM24U(m);
	  m = (l1 & 0xFF0000) + ((l1>>16) & 0xFF00) + (l2 & 0xFF);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[2] = NORM24U(m);
	  m = ((l2 << 8) & 0xFF0000) + ((l2>>8) & 0xFF00) + ((l2>>24) & 0xFF);
	  m = (m+0x800000) & 0xFFFFFF;
	  out[3] = NORM24U(m);
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = (l0<<16) + (l1<<8) + l2;
	  m = (m+0x800000) & 0xFFFFFF;
	  *out = NORM24U(m);
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
	  out[0] = NORM24U(m);
	  m = (l0 >> 24) + ((l1 << 8) & 0xFFFF00);
	  out[1] = NORM24U(m);
	  m = (l1 >> 16) + ((l2 << 16) & 0xFF0000);
	  out[2] = NORM24U(m);
	  m = (l2 >> 8);
	  out[3] = NORM24U(m);
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = l0 + (l1<<8) + (l2<<16);
	  *out = NORM24U(m);
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
	  out[0] = NORM24U(m);
	  m = ((l0>>8) & 0xFF0000) + ((l1<<8) & 0xFF00) + ((l1>>8) & 0xFF);
	  out[1] = NORM24U(m);
	  m = (l1 & 0xFF0000) + ((l1>>16) & 0xFF00) + (l2 & 0xFF);
	  out[2] = NORM24U(m);
	  m = ((l2 << 8) & 0xFF0000) + ((l2>>8) & 0xFF00) + ((l2>>24) & 0xFF);
	  out[3] = NORM24U(m);
     }
     c = (unsigned char *)in;
     for (; count>0; count--,c+=3,out++) {
	  l0 = c[0];
	  l1 = c[1];
	  l2 = c[2];
	  m = (l0<<16) + (l1<<8) + l2;
	  *out = NORM24U(m);
     }
}

#endif



static void C_PCM8S_FLOAT(signed char *in, FTYPE *out, int count)
{
     for (; count>0; count--,in++,out++)
	  *out = NORM8S(*in);
}

static void C_PCM8U_FLOAT(unsigned char *in, FTYPE *out, int count)
{
     for (; count>0; count--,in++,out++)
	  *out = NORM8U(*in);
}

static void C_PCM16SNE_FLOAT(signed short *in, FTYPE *out, int count)
{
     /* puts("HELLO!"); */
     for (; count>0; count--,in++,out++)
	  *out = NORM16S(*in);
}

static void C_PCM16SOE_FLOAT(unsigned short *in, FTYPE *out, int count)
{
     unsigned short u;
     signed short i;
     for (; count>0; count--,in++,out++) {
	  u = *in;
	  i = (u >> 8) + (u << 8);
	  *out = NORM16S(i);
     }
}

static void C_PCM16UNE_FLOAT(unsigned short *in, FTYPE *out, int count)
{
     for (; count>0; count--,in++,out++)
	  *out = NORM16U(*in);
}

static void C_PCM16UOE_FLOAT(unsigned short *in, FTYPE *out, int count)
{
     unsigned short i;
     for (; count>0; count--,in++,out++) {
	  i = *in;
	  i = (i >> 8) + (i << 8);
	  *out = NORM16U(i);
     }
}

static void C_PCM32SNE_FLOAT(gint32 *in, FTYPE *out, int count)
{
     for (; count>0; count--,in++,out++)
	  *out = NORM32S(*in);
}

static void C_PCM32SOE_FLOAT(guint32 *in, FTYPE *out, int count)
{
     guint32 u;
     guint32 x;
     gint32 i;     
     /* g_assert(sizeof(u) == 4 && sizeof(x) == 4 && sizeof(i) == 4); */
     for (; count>0; count--,in++,out++) {
	  u = *in;
	  x = (u<<CASTU32(24)) + ((u<<CASTU32(8)) & CASTU32(0xFF0000)) + 
	       ((u>>CASTU32(8)) & CASTU32(0xFF00)) + (u>>CASTU32(24));
	  i = (gint32)x;
	  *out = NORM32S(i);
     }
}

static void C_PCM32UNE_FLOAT(guint32 *in, FTYPE *out, int count)
{
#ifdef FTYPE_IS_FLOAT
     gint32 x;
     for (; count>0; count--,in++,out++) {
	  x = (gint32)(*in ^ 0x80000000);
	  *out = NORM32S(x);
     }
#else
     for (; count>0; count--,in++,out++)
	  *out = NORM32U(*in);
#endif
}

static void C_PCM32UOE_FLOAT(guint32 *in, FTYPE *out, int count)
{
#ifdef FTYPE_IS_FLOAT
     guint32 i,x;
     gint32 xi;
     for (; count>0; count--,in++,out++) {
	  i = *in;
	  x = (i<<CASTU32(24)) + ((i<<CASTU32(8)) & CASTU32(0xFF0000)) +
	       ((i>>CASTU32(8)) & CASTU32(0xFF00)) + (i>>CASTU32(24));
	  xi = (gint32)(x ^ 0x80000000);
	  *out = NORM32S(xi);
     }
#else
     guint32 i,x;
     for (; count>0; count--,in++,out++) {
	  i = *in;
	  x = (i<<CASTU32(24)) + ((i<<CASTU32(8)) & CASTU32(0xFF0000)) + 
	       ((i>>CASTU32(8)) & CASTU32(0xFF00)) + (i>>CASTU32(24));
	       
	  *out = NORM32U(x);
     }
#endif
}

static void C_PCM24SNEPM_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l;
     for (; count>0; count--,in++,out++) {
	  l = (*in >> 8) ^ 0x800000;
	  *out = NORM24U(l);
     }
}

static void C_PCM24SOEPM_FLOAT(guint32 *in, FTYPE *out, int count)
{
     guint32 i,x;
     for (; count>0; count--,in++,out++) {
	  i = *in;
	  x = ((i << 16) & 0xFF0000) | (i & 0xFF00) | ((i >> 16) & 0xFF);
	  x ^= 0x800000;
	  *out = NORM24U(x);
     }
}

static void C_PCM24UNEPM_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l;
     for (; count>0; count--,in++,out++) {
	  l = *in >> 8;
	  *out = NORM24U(l);
     }
}

static void C_PCM24UOEPM_FLOAT(guint32 *in, FTYPE *out, int count)
{
     guint32 i,x;
     for (; count>0; count--,in++,out++) {
	  i = *in;
	  x = ((i << 16) & 0xFF0000) | (i & 0xFF00) | ((i >> 16) & 0xFF);
	  *out = NORM24U(x);
     }
}

static void C_PCM24SNEPL_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l;
     for (; count>0; count--,in++,out++) {
	  l = (*in & 0xFFFFFF) ^ 0x800000;
	  *out = NORM24U(l);
     }
}

static void C_PCM24SOEPL_FLOAT(guint32 *in, FTYPE *out, int count)
{
     guint32 i,x;
     for (; count>0; count--,in++,out++) {
	  i = *in;
	  x = ((i << 8) & 0xFF0000) | ((i >> 8) & 0xFF00) | ((i >> 24) & 0xFF);
	  x ^= 0x800000;
	  *out = NORM24U(x);
     }
}

static void C_PCM24UNEPL_FLOAT(guint32 *in, FTYPE *out, int count)
{
     unsigned long l;
     for (; count>0; count--,in++,out++) {
	  l = *in & 0xFFFFFF;
	  *out = NORM24U(l);
     }
}

static void C_PCM24UOEPL_FLOAT(guint32 *in, FTYPE *out, int count)
{
     guint32 i,x;
     for (; count>0; count--,in++,out++) {
	  i = *in;
	  x = ((i << 8) & 0xFF0000) | ((i >> 8) & 0xFF00) | ((i >> 24) & 0xFF);
	  *out = NORM24U(x);
     }
}

static int C_FLOAT_PCM8S(FTYPE *in, signed char *out, int count)
{
     long int l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  l = RINT(UNNORM8S(f));
	  if (l > 127 || f > 2.0) { l = 127; cc++; } else if (l < -128 || f < -2.0) { l = -128; cc++; }
	  *out = l;
     }
     return cc;
}

static int C_FLOAT_PCM8U(FTYPE *in, unsigned char *out, int count)
{
     long int l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  l = RINT(UNNORM8U(f));
	  if (l > 255 || f > 2.0) { l = 255; cc++; } else if (l < 0 || f < -2.0) { l = 0; cc++; }
	  *out = l;
     }
     return cc;
}

static int C_FLOAT_PCM16SNE(FTYPE *in, signed short *out, int count)
{
     long int l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  l = RINT(UNNORM16S(f));
	  if (l > 32767 || f > 2.0) { l = 32767; cc++; } else if (l < -32768 || f < -2.0) { l=-32768; cc++; }
	  *out = l;
     }
     return cc;
}

static int C_FLOAT_PCM16SOE(FTYPE *in, signed short *out, int count)
{
     int i;
     i = C_FLOAT_PCM16SNE(in,out,count);
     byteswap(out,2,count*2);
     return i;
}

static int C_FLOAT_PCM16UNE(FTYPE *in, unsigned short *out, int count)
{
     long int l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  l = RINT(UNNORM16U(f));
	  if (l > 65535 || f > 2.0) { l = 65535; cc++; } else if (l < 0 || f < -2.0) { l = 0; cc++; }
	  *out = l;
     }
     return cc;
}

static int C_FLOAT_PCM16UOE(FTYPE *in, unsigned short *out, int count)
{
     int i;
     i = C_FLOAT_PCM16UNE(in,out,count);
     byteswap(out,2,count*2);
     return i;
}

static int C_FLOAT_PCM24SLE(FTYPE *in, unsigned char *out, int count)
{
     long int l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out+=3) {
	  f = *in;
	  l = RINT(UNNORM24S(f));
	  if (l > 0x7FFFFF || f > 2.0) { l = 0x7FFFFF; cc++; }
	  else if (l < -0x800000 || f < -2.0) { l = -0x800000; cc++; }
	  out[0] = l & 0xFF;
	  out[1] = (l & 0xFF00) >> 8;
	  out[2] = (l & 0xFF0000) >> 16;
     }
     return cc;
}

static int C_FLOAT_PCM24SBE(FTYPE *in, unsigned char *out, int count)
{
     long int l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out+=3) {
	  f = *in;
	  l = RINT(UNNORM24S(f));
	  if (l > 0x7FFFFF || f > 2.0) { l = 0x7FFFFF; cc++; }
	  else if (l < -0x800000 || f < -2.0) { l = -0x800000; cc++; }
	  out[0] = (l & 0xFF0000) >> 16;
	  out[1] = (l & 0xFF00) >> 8;
	  out[2] = l & 0xFF;
     }
     return cc;
}

static int C_FLOAT_PCM24ULE(FTYPE *in, unsigned char *out, int count)
{
     long int l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out+=3) {
	  f = *in;
	  l = RINT(UNNORM24U(f));
	  if (l > 0xFFFFFF || f > 2.0) { l = 0xFFFFFF; cc++; } else if (l < 0 || f < -2.0) { l = 0; cc++; }
	  out[0] = l & 0xFF;
	  out[1] = (l & 0xFF00) >> 8;
	  out[2] = (l & 0xFF0000) >> 16;
     }
     return cc;
}

static int C_FLOAT_PCM24UBE(FTYPE *in, unsigned char *out, int count)
{
     long int l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out+=3) {
	  f = *in;
	  l = RINT(UNNORM24U(f));
	  if (l > 0xFFFFFF || f > 2.0) { l = 0xFFFFFF; cc++; } else if (l < 0 || f < -2.0) { l = 0; cc++; }
	  out[0] = (l & 0xFF0000) >> 16;
	  out[1] = (l & 0xFF00) >> 8;
	  out[2] = l & 0xFF;
     }
     return cc;
}

static int C_FLOAT_PCM32SNE(FTYPE *in, gint32 *out, int count)
{
     FTYPE f;
     gint32 i;
     int cc=0;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  i = RINT(UNNORM32S(f));
	  if (f > 1.25 || (f >= 1.0 && i<0x7FFFFFFF)) { i = 0x7FFFFFFF; cc++; }
	  else if (f < -1.25 || (f <= -1.0 && i>-0x7FFFFFFF)) { i = -0x80000000; cc++; }
	  *out = i;
     }
     return cc;
}

static int C_FLOAT_PCM32SOE(FTYPE *in, gint32 *out, int count)
{
     int i;
     i = C_FLOAT_PCM32SNE(in,out,count);
     byteswap(out,4,count*4);
     return i;
}

static int C_FLOAT_PCM32UNE(FTYPE *in, guint32 *out, int count)
{
     FTYPE f;
     gint32 i;
     int cc=0;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  i = RINT(UNNORM32S(f));
	  if (f > 1.25 || (f >= 1.0 && i<0x7FFFFFFF)) { i = 0x7FFFFFFF; cc++; }
	  else if (f < -1.25 || (f <= -1.0 && i>-0x7FFFFFFF)) { i = -0x80000000; cc++; }
	  *out = (guint32)i ^ 0x80000000;
     }
     return cc;
}

static int C_FLOAT_PCM32UOE(FTYPE *in, guint32 *out, int count)
{
     int i;
     i = C_FLOAT_PCM32UNE(in,out,count);
     byteswap(out,4,count*4);
     return i;
}

static int C_FLOAT_PCM24SNEPM(FTYPE *in, guint32 *out, int count)
{
     long l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  l = RINT(UNNORM24S(f));
	  if (l > 0x7FFFFF || f > 2.0) { l = 0x7FFFFF; cc++; }
	  else if (l < -0x800000 || f < -2.0) { l = -0x800000; cc++; }
	  *out = l << 8;
     }
     return cc;
}

static int C_FLOAT_PCM24SOEPM(FTYPE *in, guint32 *out, int count)
{
     int i;
     i = C_FLOAT_PCM24SNEPM(in,out,count);
     byteswap(out,4,count*4);
     return i;
}

static int C_FLOAT_PCM24UNEPM(FTYPE *in, guint32 *out, int count)
{
     unsigned long l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  l = RINT(UNNORM24U(f));
	  if (l > 0xFFFFFF || f > 2.0) { l = 0xFFFFFF; cc++; } else if (l < 0 || f < -2.0) { l = 0; cc++; }
	  *out = l << 8;
     }
     return cc;
}

static int C_FLOAT_PCM24UOEPM(FTYPE *in, guint32 *out, int count)
{
     int i;
     i = C_FLOAT_PCM24UNEPM(in,out,count);
     byteswap(out,4,count*4);
     return i;
}

static int C_FLOAT_PCM24SNEPL(FTYPE *in, guint32 *out, int count)
{
     long l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  l = RINT(UNNORM24S(f));
	  if (l > 0x7FFFFF || f > 2.0) { l = 0x7FFFFF; cc++; }
	  else if (l < -0x800000 || f < -2.0) { l = -0x800000; cc++; }
	  *out = l;
     }
     return cc;
}

static int C_FLOAT_PCM24SOEPL(FTYPE *in, guint32 *out, int count)
{
     int i;
     i = C_FLOAT_PCM24SNEPL(in,out,count);
     byteswap(out,4,count*4);
     return i;
}

static int C_FLOAT_PCM24UNEPL(FTYPE *in, guint32 *out, int count)
{
     unsigned long l;
     int cc=0;
     FTYPE f;
     for (; count>0; count--,in++,out++) {
	  f = *in;
	  l = RINT(UNNORM24U(f));
	  if (l > 0xFFFFFF || f > 2.0) { l = 0xFFFFFF; cc++; } else if (l < 0 || f < -2.0) { l = 0; cc++; }
	  *out = l;
     }
     return cc;
}

static int C_FLOAT_PCM24UOEPL(FTYPE *in, guint32 *out, int count)
{
     int i;
     i = C_FLOAT_PCM24UNEPL(in,out,count);
     byteswap(out,4,count*4);
     return i;
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
#undef C_PCM24SNEPM_FLOAT
#undef C_PCM24SOEPM_FLOAT
#undef C_PCM24UNEPM_FLOAT
#undef C_PCM24UOEPM_FLOAT
#undef C_PCM24SNEPL_FLOAT
#undef C_PCM24SOEPL_FLOAT
#undef C_PCM24UNEPL_FLOAT
#undef C_PCM24UOEPL_FLOAT
#undef C_FLOAT_PCM16SNE
#undef C_FLOAT_PCM16SOE
#undef C_FLOAT_PCM16UNE
#undef C_FLOAT_PCM16UOE
#undef C_FLOAT_PCM32SNE
#undef C_FLOAT_PCM32SOE
#undef C_FLOAT_PCM32UNE
#undef C_FLOAT_PCM32UOE
#undef C_FLOAT_PCM24SNEPM
#undef C_FLOAT_PCM24SOEPM
#undef C_FLOAT_PCM24UNEPM
#undef C_FLOAT_PCM24UOEPM
#undef C_FLOAT_PCM24SNEPL
#undef C_FLOAT_PCM24SOEPL
#undef C_FLOAT_PCM24UNEPL
#undef C_FLOAT_PCM24UOEPL
#undef MUL
#undef DIV
#undef MULADD
#undef SUBDIV
#undef NORM8U
#undef NORM8S
#undef UNNORM8U
#undef UNNORM8S
#undef NORM16U
#undef NORM16S
#undef UNNORM16U
#undef UNNORM16S
#undef NORM24U
#undef UNNORM24U
#undef UNNORM24S
#undef NORM32U
#undef NORM32S
#undef UNNORM32U
#undef UNNORM32S

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
#undef C_PCM24SLEPM_FLOAT
#undef C_PCM24SBEPM_FLOAT
#undef C_PCM24ULEPM_FLOAT
#undef C_PCM24UBEPM_FLOAT
#undef C_PCM24SLEPL_FLOAT
#undef C_PCM24SBEPL_FLOAT
#undef C_PCM24ULEPL_FLOAT
#undef C_PCM24UBEPL_FLOAT
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
#undef C_FLOAT_PCM24SLEPM
#undef C_FLOAT_PCM24SBEPM
#undef C_FLOAT_PCM24ULEPM
#undef C_FLOAT_PCM24UBEPM
#undef C_FLOAT_PCM24SLEPL
#undef C_FLOAT_PCM24SBEPL
#undef C_FLOAT_PCM24ULEPL
#undef C_FLOAT_PCM24UBEPL
#undef C_FLOAT_PCM32SLE
#undef C_FLOAT_PCM32SBE
#undef C_FLOAT_PCM32ULE
#undef C_FLOAT_PCM32UBE
