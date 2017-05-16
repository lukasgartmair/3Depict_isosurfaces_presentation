/* 
 * APTClasses.h - Generic APT components code
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

#include "APTFileIO.h"
#include "ionhit.h"

#include "../../common/stringFuncs.h"
#include "../../common/basics.h"
#include "../../common/translation.h"



#include <cstring>
#include <new>


using std::pair;
using std::string;
using std::vector;
using std::ifstream;
using std::make_pair;


const size_t PROGRESS_REDUCE=5000;


//---------
const char *TEXT_LOAD_ERR_STRINGS[] = { "",
					NTRANS("Error opening file"),
					NTRANS("Only found header, no data"),
					NTRANS("Unable to reopen file after first scan"),
					NTRANS("Error whilst reading file contents"),
					NTRANS("Unexpected file format"),
					NTRANS("Unexpected file format"),
					NTRANS("Insufficient memory to continue"),
					};

const char *POS_ERR_STRINGS[] = { "",
       				NTRANS("Memory allocation failure on POS load"),
				NTRANS("Error opening pos file"),
				NTRANS("Pos file empty"),
				NTRANS("Pos file size appears to have non-integer number of entries"),
				NTRANS("Error reading from pos file (after open)"),
				NTRANS("Error - Found NaN in pos file"),
				NTRANS("Error - Found Inf in pos file"),
				NTRANS("Pos load aborted by interrupt.")
};
//---------

//Text file error codes and strings
//---------
enum
{
	TEXT_ERR_OPEN=1,
	TEXT_ERR_ONLY_HEADER,
	TEXT_ERR_REOPEN,
	TEXT_ERR_READ_CONTENTS,
	TEXT_ERR_FORMAT,
	TEXT_ERR_ALLOC_FAIL,
	TEXT_ERR_ENUM_END //not an error, just end of enum
};

const char *ION_TEXT_ERR_STRINGS[] = { "",
       					NTRANS("Error opening file"),
       					NTRANS("No numerical data found"),
       					NTRANS("Error re-opening file, after first scan"),
       					NTRANS("Unable to read file contents after open"),
					NTRANS("Error interpreting field in file"),
					NTRANS("Incorrect number of fields in file"),
					NTRANS("Unable to allocate memory to store data"),
					};
//---------

//ATO formatted files error codes and associated strings
//---------
enum
{
	LAWATAP_ATO_OPEN_FAIL=1,
	LAWATAP_ATO_EMPTY_FAIL,
	LAWATAP_ATO_SIZE_ERR,
	LAWATAP_ATO_VERSIONCHECK_ERR,
	LAWATAP_ATO_MEM_ERR,
	LAWATAP_ATO_BAD_ENDIAN_DETECT,
	LAWATAP_ATO_ENUM_END
};

const char *LAWATAP_ATO_ERR_STRINGS[] = { "",
				NTRANS("Error opening file"),
				NTRANS("File is empty"),
				NTRANS("Filesize does not match expected format"),
				NTRANS("File version number not <4, as expected"),
				NTRANS("Unable to allocate memory to store data"),
				NTRANS("Unable to detect endian-ness in file")
				};
//---------

unsigned int LimitLoadPosFile(unsigned int inputnumcols, unsigned int outputnumcols, const unsigned int index[], vector<IonHit> &posIons,const char *posFile, size_t limitCount,
	       	unsigned int &progress, ATOMIC_BOOL &wantAbort,bool strongSampling)
{


	//Function is only defined for 4 columns here.
	ASSERT(outputnumcols == 4);
	//buffersize must be a power of two and at least outputnumcols*sizeof(float)
	const unsigned int NUMROWS=1;
	const unsigned int BUFFERSIZE=inputnumcols * sizeof(float) * NUMROWS;
	const unsigned int BUFFERSIZE2=outputnumcols * sizeof(float) * NUMROWS;
	char *buffer=new char[BUFFERSIZE];

	
	if(!buffer)
		return POS_ALLOC_FAIL;

	char *buffer2=new char[BUFFERSIZE2];
	if(!buffer2)
	{
		delete[] buffer;
		return POS_ALLOC_FAIL;
	}

	//open pos file
	std::ifstream CFile(posFile,std::ios::binary);

	if(!CFile)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_OPEN_FAIL;
	}
	
	CFile.seekg(0,std::ios::end);
	size_t fileSize=CFile.tellg();

	if(!fileSize)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_EMPTY_FAIL;
	}
	
	CFile.seekg(0,std::ios::beg);
	
	//calculate the number of points stored in the POS file
	size_t pointCount=0;
	size_t maxIons;
	size_t maxCols = inputnumcols * sizeof(float);
	//regular case
	
	if(fileSize % maxCols)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_SIZE_MODULUS_ERR;	
	}

	maxIons =fileSize/maxCols;
	limitCount=std::min(limitCount,maxIons);

	//If we are going to load the whole file, don't use a sampling method to do it.
	if(limitCount == maxIons)
	{
		//Close the file
		CFile.close();
		delete[] buffer;
		delete[] buffer2;
		//Try opening it using the normal functions
		return GenericLoadFloatFile(inputnumcols, outputnumcols, index, posIons,posFile,progress, wantAbort);
	}

	//Use a sampling method to load the pos file
	std::vector<size_t> ionsToLoad;
	try
	{
		posIons.resize(limitCount);

		RandNumGen rng;
		rng.initTimer();
		unsigned int dummy;
		randomDigitSelection(ionsToLoad,maxIons,rng,
				limitCount,dummy,strongSampling);
	}
	catch(std::bad_alloc)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_ALLOC_FAIL;
	}


	//sort again
	//NOTE: I tried to use a functor here to get progress
	// It was not stable with parallel sort	
	std::sort(ionsToLoad.begin(),ionsToLoad.end());

	unsigned int curProg = PROGRESS_REDUCE;	

	//TODO: probably not very nice to the disk drive. would be better to
	//scan ahead for contiguous data regions, and load that where possible.
	//Or switch between different algorithms based upon ionsToLoad.size()/	
	std::ios::pos_type  nextIonPos;
	for(size_t ui=0;ui<ionsToLoad.size(); ui++)
	{
		nextIonPos =  ionsToLoad[ui]*maxCols;
		
		if(CFile.tellg() !=nextIonPos )
			CFile.seekg(nextIonPos);

		CFile.read(buffer,BUFFERSIZE);

		for (size_t i = 0; i < outputnumcols; i++) // iterate through floats
			memcpy(&(buffer2[i * sizeof(float)]), &(buffer[index[i] * sizeof(float)]), sizeof(float));
		
		if(!CFile.good())
		{
			delete[] buffer;
			delete[] buffer2;
			return POS_READ_FAIL;
		}
		posIons[ui].setHit((float*)buffer2);
		//Data bytes stored in pos files are big
		//endian. flip as required
		#ifdef __LITTLE_ENDIAN__
			posIons[ui].switchEndian();	
		#endif
		
		if(posIons[ui].hasNaN())
		{
			delete[] buffer;
			delete[] buffer2;
			return POS_NAN_LOAD_ERROR;	
		}
	
		if(posIons[ui].hasInf())
		{
			delete[] buffer;
			delete[] buffer2;
			return POS_INF_LOAD_ERROR;	
		}
		
		pointCount++;
		if(!curProg--)
		{

			progress= (unsigned int)((float)(CFile.tellg())/((float)fileSize)*100.0f);
			curProg=PROGRESS_REDUCE;
			if(wantAbort)
			{
				delete[] buffer;
				delete[] buffer2;
				posIons.clear();
				return POS_ABORT_FAIL;
				
			}
		}
				
	}

	delete[] buffer;
	delete[] buffer2;
	return 0;
}

unsigned int GenericLoadFloatFile(unsigned int inputnumcols, unsigned int outputnumcols, 
		const unsigned int index[], vector<IonHit> &posIons,const char *posFile, 
			unsigned int &progress, ATOMIC_BOOL &wantAbort)
{
	ASSERT(outputnumcols==4); //Due to ionHit.setHit
	//buffersize must be a power of two and at least sizeof(float)*outputnumCols
	const unsigned int NUMROWS=512;
	const unsigned int BUFFERSIZE=inputnumcols * sizeof(float) * NUMROWS;
	const unsigned int BUFFERSIZE2=outputnumcols * sizeof(float) * NUMROWS;

	char *buffer=new char[BUFFERSIZE];
	
	if(!buffer)
		return POS_ALLOC_FAIL;
	
	char *buffer2=new char[BUFFERSIZE2];
	if(!buffer2)
	{
		delete[] buffer;
		return POS_ALLOC_FAIL;
	}
	//open pos file
	std::ifstream CFile(posFile,std::ios::binary);
	
	if(!CFile)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_OPEN_FAIL;
	}
	
	CFile.seekg(0,std::ios::end);
	size_t fileSize=CFile.tellg();
	
	if(!fileSize)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_EMPTY_FAIL;
	}
	
	CFile.seekg(0,std::ios::beg);
	
	//calculate the number of points stored in the POS file
	IonHit hit;
	size_t pointCount=0;
	//regular case
	size_t curBufferSize=BUFFERSIZE;
	size_t curBufferSize2=BUFFERSIZE2;
	
	if(fileSize % (inputnumcols * sizeof(float)))
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_SIZE_MODULUS_ERR;	
	}
	
	try
	{
		posIons.resize(fileSize/(inputnumcols*sizeof(float)));
	}
	catch(std::bad_alloc)
	{
		delete[] buffer;
		delete[] buffer2;
		return POS_ALLOC_FAIL;
	}
	
	
	while(fileSize < curBufferSize) {
		curBufferSize = curBufferSize >> 1;
		curBufferSize2 = curBufferSize2 >> 1;
	}		
	
	//Technically this is dependent upon the buffer size.
	unsigned int curProg = 10000;	
	size_t ionP=0;
	int maxCols = inputnumcols * sizeof(float);
	int maxPosCols = outputnumcols * sizeof(float);
	do
	{
		//Taking curBufferSize chunks at a time, read the input file
		while((size_t)CFile.tellg() <= fileSize-curBufferSize)
		{
			CFile.read(buffer,curBufferSize);
			if(!CFile.good())
			{
				delete[] buffer;
				delete[] buffer2;
				return POS_READ_FAIL;
			}
			
			for (unsigned int j = 0; j < NUMROWS; j++) // iterate through rows
			{
				for (unsigned int i = 0; i < outputnumcols; i++) // iterate through floats
				{
					memcpy(&(buffer2[j * maxPosCols + i * sizeof(float)]), 
						&(buffer[j * maxCols + index[i] * sizeof(float)]), sizeof(float));
				}
			}
			
			unsigned int ui;
			for(ui=0; ui<curBufferSize2; ui+=IonHit::DATA_SIZE)
			{
				hit.setHit((float*)(buffer2+ui));
				//Data bytes stored in pos files are big
				//endian. flip as required
				#ifdef __LITTLE_ENDIAN__
					hit.switchEndian();	
				#endif
				
				if(hit.hasNaN())
				{
					delete[] buffer;
					delete[] buffer2;
					return POS_NAN_LOAD_ERROR;	
				}

				if(hit.hasInf())
				{
					delete[] buffer;
					delete[] buffer2;
					return POS_INF_LOAD_ERROR;	
				}

				posIons[ionP] = hit;
				ionP++;
				
				pointCount++;
			}	
			
			if(!curProg--)
			{
				progress= (unsigned int)((float)(CFile.tellg())/((float)fileSize)*100.0f);
				curProg=PROGRESS_REDUCE;
				if(wantAbort)
				{
					delete[] buffer;
					delete[] buffer2;
					posIons.clear();
					return POS_ABORT_FAIL;
				
				}
			}
				
		}

		curBufferSize = curBufferSize >> 1 ;
		curBufferSize2 = curBufferSize2 >> 1 ;
	}while(curBufferSize2 >= IonHit::DATA_SIZE);
	
	ASSERT((unsigned int)CFile.tellg() == fileSize);
	delete[] buffer;
	delete[] buffer2;
	
	return 0;
}


//TODO: Add progress
unsigned int limitLoadTextFile(unsigned int maxCols, 
			vector<vector<float> > &data,const char *textFile, const char *delim, const size_t limitCount,
				unsigned int &progress, ATOMIC_BOOL &wantAbort,bool strongRandom)
{
	ASSERT(maxCols);
	ASSERT(textFile);

	vector<size_t> newLinePositions;
	std::vector<std::string> subStrs;

	//Do a brute force scan through the dataset
	//to locate newlines.
	char *buffer;
	const int BUFFER_SIZE=16384; //This is totally a guess. I don't know what is best.

	ifstream CFile(textFile,std::ios::binary);

	if(!CFile)
		return TEXT_ERR_OPEN;


	//seek to the end of the file
	//to get the filesize
	size_t maxPos,curPos;
	CFile.seekg(0,std::ios::end);
	maxPos=CFile.tellg();

	CFile.close();

	CFile.open(textFile);

	curPos=0;


	//Scan through file for end of header.
	//we define this as the split value being able to generate 
	//1) Enough data to make interpretable columns
	//2) Enough columns that can be interpreted.
	while(CFile.good() && !CFile.eof() && curPos < maxPos)
	{
		string s;
		curPos = CFile.tellg();
		getline(CFile,s);

		if(!CFile.good())
			return TEXT_ERR_READ_CONTENTS;

		splitStrsRef(s.c_str(),delim,subStrs);	
		stripZeroEntries(subStrs);

		//Skip unstreamable lines
		bool unStreamable;
		unStreamable=false;
		for(unsigned int ui=0; ui<subStrs.size(); ui++)
		{
			float f;
			if(stream_cast(f,subStrs[ui]))
			{
				unStreamable=true;
				break;
			}

		}

		//well, we can stream this, so it assume it is not the header.
		if(!unStreamable)
			break;	
	}	

	//could not find any data.. only header.
	if(!CFile.good() || CFile.eof() || curPos >=maxPos)
		return TEXT_ERR_ONLY_HEADER;


	CFile.close();

	//Re-open the file in binary mode to find the newlines
	CFile.open(textFile,std::ios::binary);

	if(!CFile)
		return TEXT_ERR_REOPEN;


	//Jump to beyond the header
	CFile.seekg(curPos);


	//keep a beginning of file marker
	newLinePositions.push_back(curPos);
	bool seenNumeric=false;
	buffer = new char[BUFFER_SIZE];
	while(CFile.good() && !CFile.eof() && curPos < maxPos)
	{
		size_t bytesToRead;

		if(!CFile.good())
		{
			delete[] buffer;
			return TEXT_ERR_READ_CONTENTS;
		}
		//read up to BUFFER_SIZE bytes from the file
		//but only if they are available
		bytesToRead = std::min(maxPos-curPos,(size_t)BUFFER_SIZE);

		CFile.read(buffer,bytesToRead);

		//check that this buffer contains numeric info	
		for(unsigned int ui=0;ui<bytesToRead; ui++)
		{
			//Check for a unix-style endline
			//or the latter part of a windows endline
			//either or, whatever.
			if( buffer[ui] == '\n')
			{
				//Check that we have not hit a run of non-numeric data
				if(seenNumeric)
					newLinePositions.push_back(ui+curPos);
			} 
			else if(buffer[ui] >= '0' && buffer[ui] <='9')
				seenNumeric=true;
		}
		
		curPos+=bytesToRead;	
	
	}
	
	//Don't keep any newline that hits the end of the file, but do keep a zero position
	if(newLinePositions.size())
		newLinePositions.pop_back();
	CFile.close();


	//OK, so now we know where those pesky endlines are. This gives us jump positions
	//to new lines in the file. Each component must have some form of numeric data 
	//preceding it. That numeric data may not be fully parseable, but we assume it is until we know better.
	//
	//If it is *not* parseable, just throw an error when we find that out.

	//If we are going to load the whole file, don't use a sampling method to do it.
	if(limitCount >=newLinePositions.size())
	{
		delete[] buffer;

		vector<string> header;
	
		//Just use the non-sampling method to load.	
		if(loadTextData(textFile,data,header,delim))
			return TEXT_ERR_FORMAT;

		return 0;	
	}

	//Generate some random positions to load
	std::vector<size_t> dataToLoad;
	try
	{
		data.resize(limitCount);

		RandNumGen rng;
		rng.initTimer();
		unsigned int dummy;
		randomDigitSelection(dataToLoad,newLinePositions.size(),rng,
				limitCount,dummy,strongRandom);
	}
	catch(std::bad_alloc)
	{
		delete[] buffer;
		return TEXT_ERR_ALLOC_FAIL;
	}

	//check for abort before/after sort, as this is a long process that we cannot
	// safely abort
	if(wantAbort)
	{
		delete[] buffer;
		return POS_ABORT_FAIL;
	}
	//Sort the data such that we are going to
	//always jump forwards in the file; better disk access and whatnot.
	std::sort(dataToLoad.begin(),dataToLoad.end());
	
	//check again for abort
	if(wantAbort)
	{
		delete[] buffer;
		return POS_ABORT_FAIL;
	}

	//OK, so we have  a list of newlines
	//that we can use as entry points for random seek.
	//We also have some random entry points (sorted).
	// Now re-open the file in text mode and try to load the
	// data specified at the offsets


	//Open file in text mode
	CFile.open(textFile);

	if(!CFile)
	{
		delete[] buffer;
		return TEXT_ERR_REOPEN;
	}


	//OK, now jump to each of the random positions,
	//as dictated by the endline position 
	//and attempt a parsing there.

	subStrs.clear();
	for(size_t ui=0;ui<dataToLoad.size();ui++)	
	{

		std::ios::pos_type  nextIonPos;
		//Jump to position immediately after the newline
		nextIonPos = (newLinePositions[dataToLoad[ui]]+1);
		
		if(CFile.tellg() !=nextIonPos )
			CFile.seekg(nextIonPos);

		string s;

		getline(CFile,s);

		//Now attempt to scan the line for the selected columns.
		//split around whatever delimiter we can find
		splitStrsRef(s.c_str(),delim,subStrs);	

		size_t maxStrs;
		maxStrs=std::min(subStrs.size(),(size_t)maxCols);
		if(data.size() < maxStrs)
			data.resize(maxStrs);
		
		for(size_t uj=0;uj<maxStrs;uj++)
		{
			float tmp;
			if(stream_cast(tmp,subStrs[uj]))
			{
				//FIXME: Allow skipping bad lines
				//Can't parse line.. Abort.
				delete[] buffer;
				return TEXT_ERR_FORMAT;
			}

			data[uj].push_back(tmp);
		}
	}

	delete[] buffer;
	return 0;

}



unsigned int LoadATOFile(const char *fileName, vector<IonHit> &ions, unsigned int &progress, ATOMIC_BOOL &wantAbort,unsigned int forceEndian)
{


	//open pos file
	std::ifstream CFile(fileName,std::ios::binary);

	if(!CFile)
		return LAWATAP_ATO_OPEN_FAIL;

	//Get the filesize
	CFile.seekg(0,std::ios::end);
	size_t fileSize=CFile.tellg();


	//There are differences in the format, unfortunately.
	// Gault et al, Atom Probe Microscopy says 
	// - there are 14 entries of 4 bytes, 
	//   totalling "44" bytes - which cannot be correct. They however,
	//   say that the XYZ is added later.
	// - Header is 2 32 binary
	// - File is serialised as little-endian
	// - Various incompatible versions exist. Unclear how to distinguish

	// Larson et al say that 
	// - there are 14 fields
	// - Header byte 0x05 (0-indexed) is version number, and only version 3 is outlined
	// - Pulsenumber can be subject to FP aliasing (bad storage, occurs for values > ~16.7M ), 
	//	- Aliasing errors must be handled, if reading this field
	// - File is in big-endian


	//In summary, we assume there are 14 entries, 4 bytes each, after an 8 byte header.
	// we assume that the endian-ness must be auto-detected somehow, as no sources
	// agree on file endian-ness. If we cannot detect it, we assume little endian

	//Header (8 bytes), record 14 entries, 4 bytes each

	const size_t LAWATAP_ATO_HEADER_SIZE=8;
	const size_t LAWATAP_ATO_RECORD_SIZE = 14*4;
	const size_t LAWATAP_ATO_MIN_FILESIZE = 8 + LAWATAP_ATO_RECORD_SIZE;

	if(fileSize < LAWATAP_ATO_MIN_FILESIZE)
		return LAWATAP_ATO_EMPTY_FAIL;
	
	
	//calculate the number of points stored in the POS file
	IonHit hit;
	size_t pointCount=0;
	if((fileSize - LAWATAP_ATO_HEADER_SIZE)  % (LAWATAP_ATO_RECORD_SIZE))
		return LAWATAP_ATO_SIZE_ERR;	


	//Check that the version number, stored at offxet 0x05 (1-indexed), is 3.
	CFile.seekg(4);
	unsigned int versionByte;
	CFile.read((char*)&versionByte,sizeof(unsigned int));

	//Assume that we can have a new version that doesn't affect the readout
	// assume that earlier versions are compatible. This means, for a random byte
	// in a random length (modulo) file, 
	// we have a 1-4/255 chance of rejection from this test, and a 1/56 chance of
	// rejection from filesize, giving a ~0.02% chance of incorrect acceptance.
	if(!versionByte || versionByte > 4)
		return LAWATAP_ATO_VERSIONCHECK_ERR;


	pointCount = (fileSize-LAWATAP_ATO_HEADER_SIZE)/LAWATAP_ATO_RECORD_SIZE;
	
	try
	{
		ions.resize(pointCount);
	}
	catch(std::bad_alloc)
	{
		return LAWATAP_ATO_MEM_ERR;
	}


	//Heuristic to detect endianness.
	// - Randomly sample 100 pts from file, and check to see if, when interpreted either way,
	// there are any NaN
	//   


	bool endianFlip;
	
	if(forceEndian)
	{
		ASSERT(forceEndian < 3);
#ifdef __LITTLE_ENDIAN__
		endianFlip=(forceEndian == 2);
#elif __BIG_ENDIAN
		endianFlip=(forceEndian == 1);
#endif
	}
	else
	{
		//Auto-detect endianness from file content
		size_t numToCheck=std::min(pointCount,(size_t)100);
	
		//Indicies of points to check
		vector<unsigned int> randomNumbers;
		unsigned int dummy;
		RandNumGen rng;
		rng.initTimer();
		randomDigitSelection(randomNumbers,pointCount,rng, 
					numToCheck,dummy,wantAbort);

		//Make the traverse in ascending order
		std::sort(randomNumbers.begin(),randomNumbers.end());

		//One for no endian-flip, one for flip
		bool badFloat[2]={ false,false };
		//Track the presence of unreasonably large numbers
		bool veryLargeNumber[2] = { false,false };
	
		//Skip through several records, looking for bad float data,
		float *buffer = new float[LAWATAP_ATO_RECORD_SIZE/4];
		for(size_t ui=0;ui<numToCheck;ui++)
		{
			size_t offset;
			offset=randomNumbers[ui];
			
			CFile.seekg(LAWATAP_ATO_HEADER_SIZE + LAWATAP_ATO_RECORD_SIZE*offset);
			CFile.read((char*)buffer,LAWATAP_ATO_RECORD_SIZE);

			const unsigned int BYTES_TO_CHECK[] = { 0,1,2,3,5,6,8,9,10 };
			const size_t CHECKBYTES = 9;

			//Check each field for inf/nan presence
			for(size_t uj=0;uj<CHECKBYTES;uj++)
			{
				if(std::isnan(buffer[BYTES_TO_CHECK[uj]]) ||
					std::isinf(buffer[BYTES_TO_CHECK[uj]]))
					badFloat[0]=true;

				//Flip the endian-ness
				floatSwapBytes(buffer+BYTES_TO_CHECK[uj]);

				if(std::isnan(buffer[BYTES_TO_CHECK[uj]]) ||
					std::isinf(buffer[BYTES_TO_CHECK[uj]]))
					badFloat[1]=true;
				
				//Swap it back
				floatSwapBytes(buffer+BYTES_TO_CHECK[uj]);


			}
		
			
			//Check for some very likely values.
			//Check for large negative masses
			if( buffer[3] < -1000.0f)
				veryLargeNumber[0] = true;

			// unlikely to exceed 1000 kV
			if( fabs(buffer[6]) > 1000.0f || fabs(buffer[10]) > 1000.0f)
				veryLargeNumber[0] = true;

			//Swap and try again
			floatSwapBytes(buffer+3);
			floatSwapBytes(buffer+6);
			floatSwapBytes(buffer+10);
			if( buffer[3] < -1000.0f) 
				veryLargeNumber[1] = true;

			if( fabs(buffer[6]) > 1000.0f || fabs(buffer[10]) > 1000.0f)
				veryLargeNumber[1] = true;
		}

		delete[] buffer;


		//Now summarise the results

		//If we have a disagreement about bad-float-ness,
		// or stupid-number ness.  then choose the good one.
		// Otherwise abandon detection
		if(badFloat[0] != badFloat[1])
		{
			endianFlip=(badFloat[0]);
		}
		else if(veryLargeNumber[0] != veryLargeNumber[1])
		{
			endianFlip=veryLargeNumber[0];
		}
		else
		{
			//Assume little endian
#ifdef __LITTLE_ENDIAN__
			endianFlip= false;
#else
			endianFlip=true;
#endif
		}
	}

	//File records consist of 14 fields, some of which may not be initialised.
	// each being 4-byte IEEE little-endian float
	// It is unknown how to detect initialised state.
	// Field 	Data	
	// 0-3		x,y,z,m/c in Angstrom (x,yz) or Da (m/c)
	// 4		clusterID, if set
	//			- Ignore this field, as this information is redundant
	// 5 		Approximate Pulse #, due to Float. Pt. limitation 
	// 6		Standing Voltage (kV)
	// 7		TOF (us) (maybe corrected? maybe not?)
	// 8-9		Detector position (cm)
	// 10		Pulse voltage (kV)
	// 11		"Virtual voltage" for reconstruction.
	//			- Ignore this field, as this information is redundant
	// 12,13	Fourier intensity
	//			- Ignore these fields, as this information is redundant
	//Attempt to detect
	CFile.seekg(8);

	float *buffer = new float[LAWATAP_ATO_RECORD_SIZE/4];
	size_t curPos=0;	

	if(endianFlip)
	{
		//Read and swap
		while((size_t)CFile.tellg() < fileSize)
		{
			CFile.read((char*)buffer,LAWATAP_ATO_RECORD_SIZE);

			for(size_t ui=0;ui<LAWATAP_ATO_RECORD_SIZE;ui++)
				floatSwapBytes(buffer+ui);

			ions[curPos] = IonHit(buffer);
			curPos++;
			
		}
	}
	else
	{
		//read without swapping
		while((size_t)CFile.tellg() < fileSize)
		{	
			CFile.read((char*)buffer,LAWATAP_ATO_RECORD_SIZE);
			ions[curPos] = IonHit(buffer);
			curPos++;
		}
	}

	delete[] buffer;


	return 0;
}



#ifdef DEBUG
bool testATOFormat();


bool testFileIO()
{
	if(!testATOFormat())
		return false;

	return true;
}


bool writeATO(const std::string &filename,bool flip, unsigned int nPoints)
{
	std::ofstream outF(filename.c_str(),std::ios::binary);

	if(!outF)
		return false;


	IonHit h;
	h.setMassToCharge(100);
	h.setPos(Point3D(1,1,0));

	const unsigned int LAWATAP_ATO_RECORD_COUNT=14;

	float *buffer = new float[LAWATAP_ATO_RECORD_COUNT];
	//zero buffer
	memset(buffer,0,LAWATAP_ATO_RECORD_COUNT*sizeof(float));

	//unpack ion data into buffer, in big-endian form
	h.makePosData(buffer);

	if(!flip)
	{
		//Fkip the endinanness
		for(size_t ui=0;ui<4;ui++)
			floatSwapBytes(buffer+ui);
	}
	unsigned int intData=0;
	
	outF.write((char*)&intData,4);

	intData=3;
	//Write out verion num  as "3"
	outF.write((char*)&intData,4);

	for(size_t ui=0;ui<nPoints;ui++)
	{

		outF.write((char*)buffer,LAWATAP_ATO_RECORD_COUNT*sizeof(float));
	}
	delete[] buffer;

	return true;
}

bool testATOFormat()
{
	std::string filename;
	genRandomFilename(filename);

	if(!writeATO(filename,0,100))
	{
		//Assume we couldn't write due to some non-terminal problem
		// like missing write permissions
		WARN(false,"Unable to create file for testing ATO format. skipping");
		return true;
	}
	unsigned int dummyProgress;


	ATOMIC_BOOL wantAbort;
	wantAbort=false;
	vector<IonHit> ions;
	//Load using auto-detection of endinanness
	TEST(!LoadATOFile(filename.c_str(),ions,dummyProgress,wantAbort),"ATO load test  (auto endianness)");

	TEST(ions.size() == 100,"ion size check");

	TEST((ions[0].getPos().sqrDist(Point3D(1,1,0)) < sqrtf(std::numeric_limits<float>::epsilon())),"Checking read/write OK");
	//Load using auto-detection of endinanness

	//Load, forcing assuming cont4ents are little endianness as requried
	TEST(!LoadATOFile(filename.c_str(),ions,dummyProgress,wantAbort,1),"ATO load test (forced endianness)");
	TEST(ions.size() == 100,"ion size check");
	TEST((ions[0].getPos().sqrDist(Point3D(1,1,0)) < sqrtf(std::numeric_limits<float>::epsilon())),"checking read/write OK");


	

	rmFile(filename);

	return true;

}

#endif
