/*
 *	dataLoad.cpp - filter to  load datasets from various source
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
#include "dataLoad.h"

//Needed for modification time
#include <wx/file.h>
#include <wx/filename.h>
#include "../../wx/wxcommon.h"

#include "filterCommon.h"


#include "backend/APT/APTFileIO.h"

using std::string;
using std::pair;
using std::make_pair;
using std::endl;

//Default number of ions to load
const size_t MAX_IONS_LOAD_DEFAULT=5*1024*1024/(4*sizeof(float)); //5 MB worth.

// Tp prevent the dropdown lists from getting too unwieldy, set an artificial maximum
const unsigned int MAX_NUM_FILE_COLS=5000; 

//Allowable text file deliminators
const char *TEXT_DELIMINATORS = "\t ,";

//Supported data types
enum
{
	FILEDATA_TYPE_POS,
	FILEDATA_TYPE_TEXT,
	FILEDATA_TYPE_ATO,
	FILEDATA_TYPE_ENUM_END, // Not a data type, just end of enum
};

enum
{
	ENDIAN_MODE_AUTO,
	ENDIAN_MODE_LITTLE,
	ENDIAN_MODE_BIG,
	ENDIAN_MODE_ENUM_END,
};

const char *ENDIAN_MODE_STR[] = { NTRANS("Auto"),
					NTRANS("Little"),
					NTRANS("Big")
				};

const char *AVAILABLE_FILEDATA_TYPES[] = { 	NTRANS("POS Data"),
					NTRANS("Text Data"),
					NTRANS("ATO Data"),
					};
const char *DEFAULT_LABEL="Mass-to-Charge (Da/e)";



// == Pos load filter ==
DataLoadFilter::DataLoadFilter() : fileType(FILEDATA_TYPE_POS), doSample(true), maxIons(MAX_IONS_LOAD_DEFAULT),
	rgbaf(1.0f,0.0f,0.0f,1.0f),ionSize(2.0f), numColumns(4), enabled(true),
	volumeRestrict(false), monitorTimestamp(-1),monitorSize((size_t)-1),wantMonitor(false),
	valueLabel(TRANS(DEFAULT_LABEL)), endianMode(0)
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(AVAILABLE_FILEDATA_TYPES) == FILEDATA_TYPE_ENUM_END);
	cache=true;

	bound.setInverseLimits();
	
	for (unsigned int i  = 0; i < numColumns; i++) {
		index[i] = i;
	}

}

Filter *DataLoadFilter::cloneUncached() const
{
	DataLoadFilter *p=new DataLoadFilter;
	p->ionFilename=ionFilename;
	p->doSample=doSample;
	p->maxIons=maxIons;
	p->ionSize=ionSize;
	p->fileType=fileType;
	//Colours
	p->rgbaf=rgbaf;	
	//Bounding volume
	p->bound.setBounds(bound);
	p->volumeRestrict=volumeRestrict;
	p->numColumns=numColumns;
	p->enabled=enabled;

	for(size_t ui=0;ui<INDEX_LENGTH;ui++)
		p->index[ui]=index[ui];

	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->enabled=enabled;
	p->userString=userString;

	p->wantMonitor=wantMonitor;
	p->numColumns=numColumns;

	return p;
}


//TODO: Simplify me - enum not required
void DataLoadFilter::setFileMode(unsigned int fileMode)
{
	switch(fileMode)
	{
		case DATALOAD_TEXT_FILE:
			fileType=FILEDATA_TYPE_TEXT;
			break;
		case DATALOAD_FLOAT_FILE:
			fileType=FILEDATA_TYPE_POS;
			break;
		case DATALOAD_LAWATAP_ATO_FILE:
			fileType=FILEDATA_TYPE_ATO;
			break;
		default:
			ASSERT(false);
	}
}


void DataLoadFilter::setFilename(const char *name)
{
	ionFilename = name;
	guessNumColumns();
}

void DataLoadFilter::setFilename(const std::string &name)
{
	ionFilename = name;
	guessNumColumns();
}

void DataLoadFilter::guessNumColumns()
{
	//Test the extension to determine what we will do
	string extension;
	if(ionFilename.size() > 4)
		extension = ionFilename.substr ( ionFilename.size() - 4, 4 );

	//By default, return 4. If you want to have other file types,
	// uncomment the below
	numColumns=4;
	/*
	//Set extension to lowercase version
	for(size_t ui=0;ui<extension.size();ui++)
		extension[ui] = tolower(extension[ui]);

	if( extension == std::string(".pos")) {
		numColumns = 4;
		return;
	}
	numColumns = 4;*/
}

//!Get (approx) number of bytes required for cache
size_t DataLoadFilter::numBytesForCache(size_t nObjects) const 
{
	//Otherwise we have to work it out, based upon the file
	size_t result;
	getFilesize(ionFilename.c_str(),result);

	if(doSample)
		return std::min(maxIons*sizeof(float)*4,result);


	return result;	
}

unsigned int DataLoadFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{

	errStr="";
	//use the cached copy if we have it.
	if(cacheOK)
	{
		bool doUseCache=true;
		//If we are monitoring the file,
		//the cache is only valid if we have 
		//the same timestamp as on the file.
		if(wantMonitor)
		{
			if(!wxFile::Exists((ionFilename)))
			{
				monitorTimestamp=-1;
				monitorSize=-1;
				doUseCache=false;
				clearCache();
			}
			else
			{
				//How can we have a valid cache if we don't
				//have a valid load time?
				ASSERT(monitorTimestamp!=-1 && monitorSize!=(size_t)-1);

				size_t fileSizeVal;
				getFilesize(ionFilename.c_str(),fileSizeVal);

				if(wxFileModificationTime((ionFilename)) ==monitorTimestamp
					||  fileSizeVal!= monitorSize)
				{
					doUseCache=false;
					clearCache();
				}
			}
		}

		//Use the cache if it is still OK, otherwise use
		//the full function
		if(doUseCache)
		{
			propagateCache(getOut);

			propagateStreams(dataIn,getOut);
			progress.filterProgress=100;
			return 0;
		}
	}

	//If theres no file, then there is not a lot we can do..
	if(!wxFile::Exists((ionFilename)))
	{
		wxFileName f((ionFilename));
		
		errStr= stlStr(f.GetFullName())  + TRANS(" does not exist");
		return ERR_FILE_OPEN;
	}
	
	//If we have disable the filter, or we are are monitoring and 
	//there is no file
	if(!enabled ||(wantMonitor && !wxFile::Exists((ionFilename))) )
	{
		monitorTimestamp=-1;
		monitorSize=-1;
			
		propagateStreams(dataIn,getOut);

		return 0;
	}

	//Update the monitoring timestamp such that we know
	//when the file was last loaded
	monitorTimestamp = wxFileModificationTime((ionFilename));
	size_t tmp;
	if(getFilesize(ionFilename.c_str(),tmp))
		monitorSize=tmp;

	IonStreamData *ionData = new IonStreamData;
	ionData->parent=this;	

	progress.step=1;
	progress.stepName=TRANS("Reading File");
	progress.maxStep=1;

	unsigned int uiErr;	
	switch(fileType)
	{
		case FILEDATA_TYPE_POS:
		{
			if(doSample)
			{
				
				//Load the pos file, limiting how much you pull from it
				if((uiErr = LimitLoadPosFile(numColumns, INDEX_LENGTH, index, ionData->data, ionFilename.c_str(),
									maxIons,progress.filterProgress,(*Filter::wantAbort),strongRandom)))
				{
					consoleOutput.push_back(string(TRANS("Error loading file: ")) + ionFilename);
					delete ionData;
					errStr=TRANS(POS_ERR_STRINGS[uiErr]);
					return uiErr;
				}
		
			}	
			else
			{
				//Load the entirety of the file
				if((uiErr = GenericLoadFloatFile(numColumns, INDEX_LENGTH, index, ionData->data, ionFilename.c_str(),
									progress.filterProgress,(*Filter::wantAbort))))
				{
					consoleOutput.push_back(string(TRANS("Error loading file: ")) + ionFilename);
					delete ionData;
					errStr=TRANS(POS_ERR_STRINGS[uiErr]);
					return uiErr;
				}
			}	
			
			//warn the user if we have not loaded all the data. Users keep missing this 
			//--
			size_t fileSizeVal;
			getFilesize(ionFilename.c_str(),fileSizeVal);
			size_t numAvailable=fileSizeVal/(numColumns*sizeof(float));
			if(ionData->data.size() < numAvailable)
			{
				string strNumLoaded,strNumAvailable;
				stream_cast(strNumLoaded,ionData->data.size());
				stream_cast(strNumAvailable,numAvailable);
				consoleOutput.push_back(string(TRANS("Sampling is active, loaded ")) + strNumLoaded + 
					string( TRANS(" of " ) ) + strNumAvailable + string(TRANS(" available.")));

			}
			else
			{
				string strNumAvailable;
				stream_cast(strNumAvailable,numAvailable);

				consoleOutput.push_back(string(TRANS("Loaded entire dataset, " )) + strNumAvailable + string(TRANS(" points.")));
			}
			//--
			break;
		}
		case FILEDATA_TYPE_TEXT:
		{

			vector<vector<float> > outDat;
			vector<std::string> headerData;
		
			if(doSample)
			{
				//Load the output data using a random sampling technique. Load up to 4 data columns
				if((uiErr=limitLoadTextFile(4,outDat,ionFilename.c_str(),
						TEXT_DELIMINATORS,maxIons,progress.filterProgress,(*Filter::wantAbort),strongRandom)))

				{
					consoleOutput.push_back(string(TRANS("Error loading file: ")) + ionFilename);
					delete ionData;
					errStr=TEXT_LOAD_ERR_STRINGS[uiErr];
					return uiErr;
				}
			}
			else
			{
				//Load the entire text data using 
				if((uiErr=loadTextData(ionFilename.c_str(),outDat,headerData,TEXT_DELIMINATORS)))
				{
					consoleOutput.push_back(string(TRANS("Error loading file: ")) + ionFilename);
					delete ionData;
					errStr=TEXT_LOAD_ERR_STRINGS[uiErr];
					return uiErr;
				}
		
			}	

			//Data output must be 3 or 4 entries
			if(outDat.size() !=4 && outDat.size() != 3)
			{
				std::string sizeStr;
				stream_cast(sizeStr,outDat.size());

				consoleOutput.push_back(
					string(TRANS("Data file contained incorrect number of columns -- should be 3 or 4, was ")) + sizeStr );
				delete ionData;

				errStr=TEXT_LOAD_ERR_STRINGS[ERR_FILE_FORMAT];
				return ERR_FILE_FORMAT;
			}
		
	
			//All columns must have the same number of entries
			ASSERT(outDat[0].size() == outDat[1].size() && 
				outDat[1].size() == outDat[2].size());

			ionData->data.resize(outDat[0].size());
			if(outDat.size() == 4)
			{
				ASSERT(outDat[2].size() == outDat[3].size());
				#pragma omp parallel for
				for(unsigned int ui=0;ui<outDat[0].size(); ui++)
				{
					//Convert vector to ionhits.	
					ionData->data[ui].setPos(outDat[0][ui],outDat[1][ui],outDat[2][ui]);
					ionData->data[ui].setMassToCharge(outDat[3][ui]);
				}
			}
			else
			{
				#pragma omp parallel for
				for(unsigned int ui=0;ui<outDat[0].size(); ui++)
				{
					//Convert vector to ionhits.	
					ionData->data[ui].setPos(outDat[0][ui],outDat[1][ui],outDat[2][ui]);
					ionData->data[ui].setMassToCharge(ui);
				}
			}

			
			break;
		}
		case FILEDATA_TYPE_ATO:
		{
			//TODO: Load Ato file with sampling
			//Load the file
			if((uiErr = LoadATOFile(ionFilename.c_str(), ionData->data,
						progress.filterProgress,(*Filter::wantAbort))))
			{
				consoleOutput.push_back(string(TRANS("Error loading file: ")) + ionFilename);
				delete ionData;
				errStr=TRANS(LAWATAP_ATO_ERR_STRINGS[uiErr]);
				return uiErr;
			}
				
		
			std::string tmpSize;
			stream_cast(tmpSize,ionData->data.size());
			consoleOutput.push_back(string(TRANS("Loaded dataset, " )) + tmpSize
								 + string(TRANS(" points.")));

			break;
		}
		default:
			ASSERT(false);
	}

	ionData->r = rgbaf.r();
	ionData->g = rgbaf.g();
	ionData->b = rgbaf.b();
	ionData->a = rgbaf.a();
	ionData->ionSize=ionSize;
	ionData->valueType=valueLabel;


	if(ionData->data.empty())
	{
		//Shouldn't get here...
		ASSERT(false);
		delete ionData;
		return	0;
	}
	progress.filterProgress=100;


	BoundCube dataCube;
	IonHit::getBoundCube(ionData->data,dataCube);

	if(dataCube.isNumericallyBig())
	{
		consoleOutput.push_back(
			TRANS("Warning:One or more bounds of the loaded data approaches "
			       "the limits of numerical stability for the internal data type"
			       "(magnitude too large). Consider rescaling data before loading"));
	}

	cacheAsNeeded(ionData);

	//Append the ion data 
	getOut.push_back(ionData);

	
	propagateStreams(dataIn,getOut);

	return 0;
}

void DataLoadFilter::getProperties(FilterPropGroup &propertyList) const
{
	FilterProperty p;

	size_t curGroup=0;

	p.type=PROPERTY_TYPE_FILE;
	p.key=DATALOAD_KEY_FILE;
	p.name=TRANS("File");
	p.helpText=TRANS("File from which to load data");
	p.data=ionFilename;
	//Wx- acceptable string format
	p.dataSecondary = TRANS("Readable files (*.xml, *.pos, *.txt,*.csv, *.ato)|*.xml;*.pos;*.txt;*.csv;*.ato|All Files|*") ;

	propertyList.addProperty(p,curGroup);

	vector<pair<unsigned int,string> > choices;

	for(unsigned int ui=0;ui<FILEDATA_TYPE_ENUM_END; ui++)
		choices.push_back(make_pair(ui,AVAILABLE_FILEDATA_TYPES[ui]));	
					
	p.data=choiceString(choices,fileType);
	p.name=TRANS("File type");
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Type of file to be loaded");
	p.key=DATALOAD_KEY_FILETYPE;
	
	propertyList.addProperty(p,curGroup);

	propertyList.setGroupTitle(curGroup,TRANS("File"));
	//---------
	curGroup++;	
	
	string colStr;
	switch(fileType)
	{
		case FILEDATA_TYPE_POS:
		{
			stream_cast(colStr,numColumns);
			p.name=TRANS("Entries per point");
			p.helpText=TRANS("Number of decimal values in file per 3D point (normally 4)");
			p.data=colStr;
			p.key=DATALOAD_KEY_NUMBER_OF_COLUMNS;
			p.type=PROPERTY_TYPE_INTEGER;
			propertyList.addProperty(p,curGroup);
			break;
		}
		case FILEDATA_TYPE_TEXT:
		{
			break;
		}
		case FILEDATA_TYPE_ATO:
		{
			vector<pair<unsigned int,string> > endianChoices;
			endianChoices.resize(ENDIAN_MODE_ENUM_END);

			for(unsigned int ui=0;ui<ENDIAN_MODE_ENUM_END;ui++)
				endianChoices[ui]=make_pair(ui,ENDIAN_MODE_STR[ui]);

			p.name=TRANS("File \"Endianness\"");
			p.helpText=TRANS("On-disk data storage format. If file won\'t load, just try each");
			p.data=choiceString(endianChoices,endianMode);
			p.key=DATALOAD_KEY_ENDIANNESS;
			p.type=PROPERTY_TYPE_CHOICE;
			propertyList.addProperty(p,curGroup);
			break;

		}
		default:
			ASSERT(false);
			
	}

	choices.clear();
	for (unsigned int i = 0; i < numColumns; i++) {
		string tmp;
		stream_cast(tmp,i);
		choices.push_back(make_pair(i,tmp));
	}
	
	colStr= choiceString(choices,index[0]);
	p.name="X";
	p.data=colStr;
	p.key=DATALOAD_KEY_SELECTED_COLUMN0;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Relative offset of each entry in file for point's X position");
	propertyList.addProperty(p,curGroup);
	
	colStr= choiceString(choices,index[1]);
	p.name="Y";
	p.data=colStr;
	p.key=DATALOAD_KEY_SELECTED_COLUMN1;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Relative offset of each entry in file for point's Y position");
	propertyList.addProperty(p,curGroup);
	
	colStr= choiceString(choices,index[2]);
	p.name="Z";
	p.data=colStr;
	p.key=DATALOAD_KEY_SELECTED_COLUMN2;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Relative offset of each entry in file for point's Z position");
	propertyList.addProperty(p,curGroup);
	
	colStr= choiceString(choices,index[3]);
	p.name=TRANS("Value");
	p.data=colStr;
	p.key=DATALOAD_KEY_SELECTED_COLUMN3;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Relative offset of each entry in file to use for scalar value of 3D point");
	propertyList.addProperty(p,curGroup);
	
	p.name=TRANS("Value Label");
	p.data=valueLabel;
	p.key=DATALOAD_KEY_VALUELABEL;
	p.type=PROPERTY_TYPE_STRING;
	p.helpText=TRANS("Name for the scalar value associated with each point");
	propertyList.addProperty(p,curGroup);
	
	propertyList.setGroupTitle(curGroup,TRANS("Format params."));

	curGroup++;

	string tmpStr;
	stream_cast(tmpStr,enabled);
	p.name=TRANS("Enabled");
	p.data=tmpStr;
	p.key=DATALOAD_KEY_ENABLED;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Load this file?");
	propertyList.addProperty(p,curGroup);

	if(enabled)
	{
		std::string tmpStr;

		//FIXME: ATO Files need an implementation of sampling read
		if(fileType!=FILEDATA_TYPE_ATO)
		{
			stream_cast(tmpStr,doSample);
			p.name=TRANS("Sample data");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Perform random selection on file contents, instead of loading entire file");
			p.key=DATALOAD_KEY_SAMPLE;
			propertyList.addProperty(p,curGroup);

			if(doSample)
			{
				stream_cast(tmpStr,maxIons*sizeof(float)*4/(1024*1024));
				p.name=TRANS("Load Limit (MB)");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_INTEGER;
				p.helpText=TRANS("Limit for size of data to load");
				p.key=DATALOAD_KEY_SIZE;
				propertyList.addProperty(p,curGroup);
			}
		}

		stream_cast(tmpStr,wantMonitor);
		p.name=TRANS("Monitor");
		p.data=tmpStr;
		p.key=DATALOAD_KEY_MONITOR;
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Watch file timestamp to track changes to file contents from other programs");
		propertyList.addProperty(p,curGroup);
	}
	
	propertyList.setGroupTitle(curGroup,TRANS("Load params."));

	if(enabled)
	{
		
		curGroup++;

		p.name=TRANS("Default colour ");
		p.data=rgbaf.toColourRGBA().rgbaString(); 
		p.type=PROPERTY_TYPE_COLOUR;
		p.helpText=TRANS("Default colour for points, if not overridden by other filters");
		p.key=DATALOAD_KEY_COLOUR;
		propertyList.addProperty(p,curGroup);

		stream_cast(tmpStr,ionSize);
		p.name=TRANS("Draw Size");
		p.data=tmpStr; 
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Default size for points, if not overridden by other filters");
		p.key=DATALOAD_KEY_IONSIZE;
		
		propertyList.addProperty(p,curGroup);
		propertyList.setGroupTitle(curGroup,TRANS("Appearance"));
	}

}

bool DataLoadFilter::setProperty(  unsigned int key, 
					const std::string &value, bool &needUpdate)
{
	
	needUpdate=false;
	switch(key)
	{
		case DATALOAD_KEY_FILETYPE:
		{
			unsigned int ltmp;
			ltmp=(unsigned int)-1;
			
			for(unsigned int ui=0;ui<FILEDATA_TYPE_ENUM_END; ui++)
			{
				if(AVAILABLE_FILEDATA_TYPES[ui] == value)
				{
					ltmp=ui;
					break;
				}
			}
			if(ltmp == (unsigned int) -1 || ltmp == fileType)
				return false;

			fileType=ltmp;
			clearCache();
			needUpdate=true;
			break;
		}
		case DATALOAD_KEY_FILE:
		{
			//Ensure is not a dir (posix),
			// as fstream will open dirs under linux
#if !defined(WIN32) && !defined(WIN64)
			if(isNotDirectory(value.c_str()) == false)
				return false;

#endif
			//ensure that the new file can be found
			//Try to open the file
			std::ifstream f(value.c_str());
			if(!f)
				return false;
			f.close();

			setFilename(value);
			//Invalidate the cache
			clearCache();

			needUpdate=true;
			break;
		}
		case DATALOAD_KEY_ENABLED:
		{
			if(!applyPropertyNow(enabled,value,needUpdate))
				return false;
			break;
		}
		case DATALOAD_KEY_MONITOR:
		{
			if(!applyPropertyNow(wantMonitor,value,needUpdate))
				return false;
			break;
		}
		
		case DATALOAD_KEY_SAMPLE:
		{
			if(!applyPropertyNow(doSample,value,needUpdate))
				return false;
			break;
		}
		case DATALOAD_KEY_SIZE:
		{
			size_t ltmp;
			if(stream_cast(ltmp,value))
				return false;

			if(!ltmp)
				return false;

			if(ltmp*(1024*1024/(sizeof(float)*4)) != maxIons)
			{
				//Convert from MB to ions.			
				maxIons = ltmp*(1024*1024/(sizeof(float)*4));
				needUpdate=true;
				//Invalidate cache
				clearCache();
			}
			break;
		}
		case DATALOAD_KEY_COLOUR:
		{
			ColourRGBA tmpRgba;
			tmpRgba.parse(value);

			if(tmpRgba != rgbaf.toColourRGBA())
			{
				rgbaf=tmpRgba.toRGBAf();


				//Check the cache, updating it if needed
				if(cacheOK)
				{
					for(unsigned int ui=0;ui<filterOutputs.size();ui++)
					{
						if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_IONS)
						{
							IonStreamData *i;
							i=(IonStreamData *)filterOutputs[ui];
							i->r=rgbaf.r();
							i->g=rgbaf.g();
							i->b=rgbaf.b();
							i->a=rgbaf.a();
						}
					}

				}
				needUpdate=true;
			}


			break;
		}
		case DATALOAD_KEY_IONSIZE:
		{
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;

			if(ltmp < 0)
				return false;

			ionSize=ltmp;

			//Check the cache, updating it if needed
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{
					if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_IONS)
					{
						IonStreamData *i;
						i=(IonStreamData *)filterOutputs[ui];
						i->ionSize=ionSize;
					}
				}
			}
			needUpdate=true;

			break;
		}
		case DATALOAD_KEY_VALUELABEL:
		{
			if(value !=valueLabel)
			{
				valueLabel=value;
				needUpdate=true;
			
				//Check the cache, updating it if needed
				if(cacheOK)
				{
					for(unsigned int ui=0;ui<filterOutputs.size();ui++)
					{
						if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_IONS)
						{
							IonStreamData *i;
							i=(IonStreamData *)filterOutputs[ui];
							i->valueType=valueLabel;
						}
					}
				}

			}

			break;
		}
		case DATALOAD_KEY_SELECTED_COLUMN0:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp >= numColumns)
				return false;
			
			index[0]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case DATALOAD_KEY_SELECTED_COLUMN1:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp >= numColumns)
				return false;
			
			index[1]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case DATALOAD_KEY_SELECTED_COLUMN2:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp >= numColumns)
				return false;
			
			index[2]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case DATALOAD_KEY_SELECTED_COLUMN3:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp >= numColumns)
				return false;
			
			index[3]=ltmp;
			needUpdate=true;
			clearCache();
			
			break;
		}
		case DATALOAD_KEY_NUMBER_OF_COLUMNS:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp >= MAX_NUM_FILE_COLS)
				return false;
			
			numColumns=ltmp;
			for (unsigned int i = 0; i < INDEX_LENGTH; i++) {
				index[i] = (index[i] < numColumns? index[i]: numColumns - 1);
			}
			needUpdate=true;
			clearCache();
			
			break;
		}
		
		case DATALOAD_KEY_ENDIANNESS:
		{
			unsigned int ltmp;
			ltmp=(unsigned int)-1;
			
			for(unsigned int ui=0;ui<ENDIAN_MODE_ENUM_END; ui++)
			{
				if(ENDIAN_MODE_STR[ui] == value)
				{
					ltmp=ui;
					break;
				}
			}
			if(ltmp == (unsigned int) -1 || ltmp == endianMode)
				return false;

			endianMode=ltmp;
			clearCache();
			needUpdate=true;
			break;
		}
		default:
			ASSERT(false);
			break;
	}
	return true;
}

bool DataLoadFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	//Retrieve file name	
	if(XMLHelpFwdToElem(nodePtr,"file"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
	if(!xmlString)
		return false;
	ionFilename=(char *)xmlString;
	xmlFree(xmlString);

	//retrieve file type (text,pos etc), if needed; default to pos.
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"type");
	if(xmlString)
	{
		int type;
		if(stream_cast(type,xmlString))
			return false;
		
		if(fileType >=FILEDATA_TYPE_ENUM_END)
			return false;
		fileType=type;
		xmlFree(xmlString);
	}
	else
		fileType=FILEDATA_TYPE_POS;


	//Override the string, as needed
	if( (stateFileDir.size()) &&
		(ionFilename.size() > 2 && ionFilename.substr(0,2) == "./") )
	{
		ionFilename=stateFileDir + ionFilename.substr(2);
	}

	//Filenames need to be converted from unix format (which I make canonical on disk) into native format 
	ionFilename=convertFileStringToNative(ionFilename);

	//Retrieve number of columns	
	if(!XMLGetNextElemAttrib(nodePtr,numColumns,"columns","value"))
		return false;
	if(numColumns >= MAX_NUM_FILE_COLS)
		return false;
	
	//Retrieve index	
	if(XMLHelpFwdToElem(nodePtr,"xyzm"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"values");
	if(!xmlString)
		return false;
	std::vector<string> v;
	splitStrsRef((char *)xmlString,',',v);
	for (unsigned int i = 0; i < INDEX_LENGTH && i < v.size(); i++)
	{
		if(stream_cast(index[i],v[i]))
			return false;

		if(index[i] >=numColumns)
			return false;
	}
	xmlFree(xmlString);
	
	//Retrieve enabled/disabled
	//--
	unsigned int tmpVal;
	if(!XMLGetNextElemAttrib(nodePtr,tmpVal,"enabled","value"))
		return false;
	enabled=tmpVal;
	//--
	
	//Retrieve monitor mode 
	//--
	xmlNodePtr nodeTmp;
	nodeTmp=nodePtr;
	if(XMLGetNextElemAttrib(nodePtr,tmpVal,"monitor","value"))
		wantMonitor=tmpVal;
	else
	{
		nodePtr=nodeTmp;
		wantMonitor=false;
	}
	//--
	
	//Retrieve value type string (eg mass-to-charge, 
	// or whatever the data type is)
	//--
	nodeTmp=nodePtr;
	if(XMLHelpFwdToElem(nodePtr,"valuetype"))
	{
		nodePtr=nodeTmp;
		valueLabel=TRANS(DEFAULT_LABEL);
	}
	else
	{
		xmlChar *xmlString;
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
		if(!xmlString)
			return false;
		valueLabel=(char *)xmlString;
		xmlFree(xmlString);

	}

	//--


	//Get sampling enabled/disabled
	//---
	//TODO: Remove me:
	// Note, in 3Depict-0.0.10 and lower, we did not have this option,
	// so some statefiles will exist without this. In the case it is not
	// found, we need to make it up
	{
	nodeTmp=nodePtr;
	bool needSampleState=false;
	if(!XMLGetNextElemAttrib(nodePtr,doSample,"dosample","value"))
	{
		nodePtr=nodeTmp;
		needSampleState=true;
	}
	//---

	//Get max Ions
	//--
	//TODO: Forbid zero values - don't do it now, as we previously used this
	// for disabling sampling, so some users' XML files will still have this
	if(!XMLGetNextElemAttrib(nodePtr,maxIons,"maxions","value"))
		return false;

	if(needSampleState) 
		doSample=maxIons;

	//FIXME: Compatibility hack.  User preferences from 3Depict <=0.0.10
	
	// could cause == 0 maxIons, causing an assertion error in
	// ::refresh() due to no ions. Override this.
	// In the future, we should reject this as invalid, however
	// it was valid for 3Depict <= 0.0.10
	if(!maxIons)
		maxIons=MAX_IONS_LOAD_DEFAULT;
	//--
	}
	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;

	if(!parseXMLColour(nodePtr,rgbaf))
		return false;	
	//====

	//Retrieve drawing size value
	//--
	if(!XMLGetNextElemAttrib(nodePtr,ionSize,"ionsize","value"))
		return false;
	//check positive or zero
	if(ionSize <=0)
		return false;
	//--


	return true;
}

unsigned int DataLoadFilter::getRefreshBlockMask() const
{
	return 0;
}

unsigned int DataLoadFilter::getRefreshEmitMask() const
{
	return STREAM_TYPE_IONS;
}

unsigned int DataLoadFilter::getRefreshUseMask() const
{
	return 0;
}

std::string  DataLoadFilter::getSpecificErrString(unsigned int code) const
{
	ASSERT(errStr.size());
	return errStr;
}

void DataLoadFilter::setPropFromBinding(const SelectionBinding &b) 
{
	ASSERT(false);
}

bool DataLoadFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;

			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;
			f << tabs(depth+1) << "<file name=\"" << escapeXML(convertFileStringToCanonical(ionFilename)) << "\" type=\"" << fileType << "\"/>" << endl;
			f << tabs(depth+1) << "<columns value=\"" << numColumns << "\"/>" << endl;
			f << tabs(depth+1) << "<xyzm values=\"" << index[0] << "," << index[1] << "," << index[2] << "," << index[3] << "\"/>" << endl;
			f << tabs(depth+1) << "<enabled value=\"" << enabled<< "\"/>" << endl;
			f << tabs(depth+1) << "<monitor value=\"" << wantMonitor<< "\"/>"<< endl; 
			f << tabs(depth+1) << "<valuetype value=\"" << escapeXML(valueLabel)<< "\"/>"<< endl; 
			f << tabs(depth+1) << "<dosample value=\"" << doSample << "\"/>" << endl;
			f << tabs(depth+1) << "<maxions value=\"" << maxIons << "\"/>" << endl;

			f << tabs(depth+1) << "<colour r=\"" <<  rgbaf.r() << "\" g=\"" << rgbaf.g() 
				<< "\" b=\"" << rgbaf.b() << "\" a=\"" << rgbaf.a() << "\"/>" <<endl;
			f << tabs(depth+1) << "<ionsize value=\"" << ionSize << "\"/>" << endl;
			f << tabs(depth) << "</" << trueName() << ">" << endl;
			break;
		}
		default:
			//Shouldn't get here, unhandled format string 
			ASSERT(false);
			return false;
	}

	return true;

}

bool DataLoadFilter::writePackageState(std::ostream &f, unsigned int format,
			const std::vector<std::string> &valueOverrides, unsigned int depth) const
{
	ASSERT(valueOverrides.size() == 1);

	//Temporarily modify the state of the filter, then call writestate
	string tmpIonFilename=ionFilename;


	//override const -- naughty, but we know what we are doing...
	const_cast<DataLoadFilter*>(this)->ionFilename=valueOverrides[0];
	bool result;
	result=writeState(f,format,depth);

	const_cast<DataLoadFilter*>(this)->ionFilename=tmpIonFilename;

	return result;
}

void DataLoadFilter::getStateOverrides(std::vector<string> &externalAttribs) const 
{
	externalAttribs.push_back(ionFilename);

}

bool DataLoadFilter::monitorNeedsRefresh() const
{
	//We can only actually effect an update if the
	// filter is enabled (and we actually want to monitor).
	// otherwise, we don't need to refresh
	if(enabled && wantMonitor)
	{
		//Check to see that the file exists, if
		// not fall back to the cache.
		if(!wxFile::Exists((ionFilename)))
			return cacheOK;


		size_t sizeVal;
		getFilesize(ionFilename.c_str(),sizeVal);
		if(sizeVal != monitorSize)
			return true;

		return( wxFileModificationTime((ionFilename))
						!=monitorTimestamp);
		
		
	}


	return false;
}


#ifdef DEBUG


bool posFileTest();
bool textFileTest();


bool DataLoadFilter::runUnitTests()
{
	if(!posFileTest())
		return false;

	if(!textFileTest())
		return false;

	return true;
}

bool posFileTest()
{
	//Synthesise data, then *save* it.

	const unsigned int NUM_PTS=133;
	vector<IonHit> hits;
	hits.resize(NUM_PTS);
	for(unsigned int ui=0; ui<NUM_PTS; ui++)
	{
		hits[ui].setPos(Point3D(ui,ui,ui));
		hits[ui].setMassToCharge(ui);
	}

	//TODO: Make more robust
	const char *posName="testAFNEUEA1754.pos";
	//see if we can open the file for input. If so, it must exist
	std::ifstream file(posName,std::ios::binary);

	if(file)
	{
		std::string s;
		s="Unwilling to execute file test, will not overwrite file :";
		s+=posName;
		s+=". Test is indeterminate";
		WARN(false,s.c_str());

		return true;
	}

	if(IonHit::makePos(hits,posName))
	{
		WARN(false,"Unable to create test output file. Unit test was indeterminate. Requires write access to excution path");
		return true;
	}

	//-----------

	//Create the data load filter, load it, then check we loaded the same data
	//---------
	DataLoadFilter* d= new DataLoadFilter;
	d->setCaching(false);

	bool needUp;
	TEST(d->setProperty(DATALOAD_KEY_FILE,posName,needUp),"Set prop");
	TEST(d->setProperty(DATALOAD_KEY_SAMPLE,"0",needUp),"Set prop");
	//---------

	vector<const FilterStreamData*> streamIn,streamOut;
	ProgressData prog;
	TEST(!d->refresh(streamIn,streamOut,prog),"Refresh error code");
	delete d;


	TEST(streamOut.size() == 1, "Stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS, "Stream type");

	TEST(streamOut[0]->getNumBasicObjects() == hits.size(), "Stream count");
	
	
#if defined(__LINUX__) || defined(__APPLE__)
	//Hackish method to delete file
	std::string s;
	s=string("rm -f ") + string(posName);
	system(s.c_str());
#endif

	delete streamOut[0];
	return true;
}

bool textFileTest()
{
	//write some random data
	// with a fixed seed value
	RandNumGen r;
	r.initialise(232635); 
	const unsigned int NUM_PTS=1000;

	//TODO: do better than this
	const char *FILENAME="test-3mdfuneaascn.txt";
	//see if we can open the file for input. If so, it must exist,
	//and thus we don't want to overwrite it, as it may contain useful data.
	std::ifstream inFile(FILENAME);
	if(inFile)
	{
		std::string s;
		s="Unwilling to execute file test, will not overwrite file :";
		s+=FILENAME;
		s+=". Test is indeterminate";
		WARN(false,s.c_str());

		return true;
	}
		
	std::ofstream outFile(FILENAME);

	if(!outFile)
	{
		WARN(false,"Unable to create test output file. Unit test was indeterminate. Requires write access to excution path");
		return true;
	}

	vector<IonHit> hitVec;
	hitVec.resize(NUM_PTS);
	
	//Write out the file
	outFile << "x y\tz\tValues" << endl;
	for(unsigned int ui=0;ui<NUM_PTS;ui++)
	{
		Point3D p;
		p.setValue(r.genUniformDev(),r.genUniformDev(),r.genUniformDev());
		hitVec[ui].setPos(p);
		hitVec[ui].setMassToCharge(r.genUniformDev());
		outFile << p[0] << " " << p[1] << "\t" << p[2] << "\t" << hitVec[ui].getMassToCharge() << endl;
	}

	outFile.close();

	//Create the data load filter, load it, then check we loaded the same data
	//---------
	DataLoadFilter* d=new DataLoadFilter;
	d->setCaching(false);

	bool needUp;
	TEST(d->setProperty(DATALOAD_KEY_FILE,FILENAME,needUp),"Set prop");
	TEST(d->setProperty(DATALOAD_KEY_SAMPLE,"0",needUp),"Set prop"); //load all data
	//Load data as text file
	TEST(d->setProperty(DATALOAD_KEY_FILETYPE,
			AVAILABLE_FILEDATA_TYPES[FILEDATA_TYPE_TEXT],needUp),"Set prop"); 
	//---------


	vector<const FilterStreamData*> streamIn,streamOut;
	ProgressData prog;
	TEST(!d->refresh(streamIn,streamOut,prog),"Refresh error code");
	delete d;


	TEST(streamOut.size() == 1, "Stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS, "Stream type");

	TEST(streamOut[0]->getNumBasicObjects() == NUM_PTS,"Stream count");

#if defined(__LINUX__) || defined(__APPLE__)
	//Hackish method to delete file
	std::string s;
	s=string("rm -f ") + string(FILENAME);
	system(s.c_str());
#endif

	delete streamOut[0];
	return true;
}

#endif
