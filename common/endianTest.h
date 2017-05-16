/*
 *	endianttest.h - Platform endian testing
 *	Copyright (C) 2015, D Haley 

 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.

 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.

 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _ENDIAN_TEST_H_
#define _ENDIAN_TEST_H_
#if defined (_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#define __LITTLE_ENDIAN__
#else
#ifdef __linux__
#include <endian.h>
#endif
#endif

#ifdef __BYTE_ORDER
//if both are not defined it is TRUE!
#if __BYTE_ORDER == __BIG_ENDIAN
#ifndef __BIG_ENDIAN__
#define __BIG_ENDIAN__
#endif
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#ifndef __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__
#endif
#elif __BYTE_ORDER == __PDP_ENDIAN
#ifndef __ARM_ENDIAN__
#define __ARM_ENDIAN__
#endif
#else
#error "Endian determination failed"
#endif
#endif

const int ENDIAN_TEST=1;
//Run-time detection
inline int is_bigendian() { return (*(char*)&ENDIAN_TEST) == 0 ;}

inline int is_littleendian() { return (*(char*)&ENDIAN_TEST) == 1 ;}


inline void floatSwapBytes(float *inFloat)
{
	//Use a union to avoid strict-aliasing error
	union FloatSwapUnion{
	   float f;
	   char c[4];
	} ;
	FloatSwapUnion fa,fb;
	fa.f = *inFloat;

	fb.c[0] = fa.c[3];
	fb.c[1] = fa.c[2];
	fb.c[2] = fa.c[1];
	fb.c[3] = fa.c[0];

	*inFloat=fb.f;
}

#endif
