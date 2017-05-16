/*
 * common/assertion.h  - Program assertion header
 * Copyright (C) 2015  D Haley
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ASSERTION_H
#define ASSERTION_H

#ifdef DEBUG
	#include <iostream>
	#include <cstdlib>
	
	#include <sys/time.h>

	//Do we want to trap floating point exceptions 
	void trapfpe (bool doTrap=true);
	//Do we want to trap floating point exceptions 
	bool getTrapfpe ();
	//Ask the user about continuing (or not) in the case of an assertion
	void userAskAssert(const char * const filename, const unsigned int lineNumber); 
	//Warn the programmer about an error detected by a check
	void warnProgrammer(const char * const filename, const unsigned int lineNumber,
							const char *message);

	//Assertion macro. Used to trigger fatal errors in program (debug mode)
	#ifndef ASSERT
	#define ASSERT(f) {if(!(f)) { userAskAssert(__FILE__,__LINE__);}}
	#endif

	//warn programmer about unusual situation occurrence
	#ifndef WARN
	#define WARN(f,g) { if(!(f)) { warnProgrammer(__FILE__,__LINE__,g);}}
	#endif
	

	inline void warnProgrammer(const char * const filename, const unsigned int lineNumber,const char *message) 
	{
		std::cerr << "Warning to programmer." << std::endl;
		std::cerr << "Filename: " << filename << std::endl;
		std::cerr << "Line number: " << lineNumber << std::endl;
		std::cerr << message << std::endl;
	}

	//Debug timing routines
	#define DEBUG_TIME_START() timeval TIME_DEBUG_t; gettimeofday(&TIME_DEBUG_t,NULL);
	#define DEBUG_TIME_END()  { timeval TIME_DEBUG_tend; gettimeofday(&TIME_DEBUG_tend,NULL); \
	std::cerr << (TIME_DEBUG_tend.tv_sec - TIME_DEBUG_t.tv_sec) + ((float)TIME_DEBUG_tend.tv_usec-(float)TIME_DEBUG_t.tv_usec)/1.0e6 << std::endl; }

	#ifndef TEST
	#define EQ_TOL(f,g) (fabs( (f) - (g)) < 0.001)
	#define EQ_TOLV(f,g,h) (fabs( (f) - (g)) < (h))

	#define TEST(f,g) if(!(f)) { std::cerr << "Test fail :" << __FILE__ << ":" << __LINE__ << "\t"<< (g) << std::endl;return false;}
	#endif

	#define TRACE(f) { timespec timeval; clock_gettime(CLOCK_MONOTONIC, &timeval); std::cerr  << "<" << f <<">" __FILE__ << ":" << __LINE__ << " t: " << timeval.tv_sec << "." << timeval.tv_nsec/1000 << endl;} 

	//A hack to generate compile time asserts (thanks Internet).
	//This causes gcc to give "duplicate case value", if the predicate is false
	#ifndef  HAVE_CPP_1X
	#define COMPILE_ASSERT(pred)            \
		{ switch(0){case 0:case pred:;}}
	#else
	#define COMPILE_ASSERT(pred) {static_assert( pred , "Static assertion failed" );}
	#endif
#else
	#define ASSERT(f)
	#define COMPILE_ASSERT(f)
	#define WARN(f,g) 
	#define TEST(f,g)
	#define TRACE(f)
	//Do we want to trap floating point exceptions 
	void trapfpe (bool doTrap=true);
		

#endif

#endif
