/*
 * APTClasses.h - Generic APT components header 
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

#ifndef APTCLASSES_H
#define APTCLASSES_H

#include "common/basics.h"

class IonHit;


using std::vector;

class IonHit;

extern const char *POS_ERR_STRINGS[];

extern const char *ION_TEXT_ERR_STRINGS[];

extern const char *LAWATAP_ATO_ERR_STRINGS[];

extern const char *TEXT_LOAD_ERR_STRINGS[];

//!Errors that can be encountered when openning pos files
enum posErrors
{
	POS_ALLOC_FAIL=1,
	POS_OPEN_FAIL,
	POS_EMPTY_FAIL,
	POS_SIZE_MODULUS_ERR,	
	POS_READ_FAIL,
	POS_NAN_LOAD_ERROR,
	POS_INF_LOAD_ERROR,
	POS_ABORT_FAIL,
	POS_ERR_FINAL // Not actually an error, but tells us where the end of the num is.
};




//!Load a pos file directly into a single ion list
/*! Pos files are fixed record size files, with data stored as 4byte
 * big endian floating point. (IEEE 574?). Data is stored as
 * x,y,z,mass/charge. 
 * */
//!Load a pos file into a T of IonHits
unsigned int GenericLoadFloatFile(unsigned int inputnumcols, unsigned int outputnumcols, 
		const unsigned int index[], vector<IonHit> &posIons,const char *posFile, 
				unsigned int &progress, ATOMIC_BOOL &wantAbort);


unsigned int LimitLoadPosFile(unsigned int inputnumcols, unsigned int outputnumcols, const unsigned int index[], 
			vector<IonHit> &posIons,const char *posFile, size_t limitCount,
					       	unsigned int &progress, ATOMIC_BOOL &wantAbort,bool strongRandom);



unsigned int limitLoadTextFile(unsigned int numColsTotal, 
			vector<vector<float> > &data,const char *posFile, const char *deliminator, const size_t limitCount,
					       	unsigned int &progress, ATOMIC_BOOL &wantAbort,bool strongRandom);


//Load a CAMECA LAWATAP "ATO" formatted file.
//	- This is a totally different format to the "FlexTAP" ato format
//Supported versions are "version 3"
//	Force endian : 0 - do not force, autodetect, 1 - force little, 2- force big
unsigned int LoadATOFile(const char *fileName, vector<IonHit> &ions, unsigned int &progressm, ATOMIC_BOOL &wantAbort, unsigned int forceEndian=0);


#ifdef DEBUG
bool testFileIO();
#endif

#endif
