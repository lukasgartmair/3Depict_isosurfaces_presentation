/*
 *	externalProgram.cpp - Call out external programs as a datasource/sink
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
#include "externalProgram.h"

#include "filterCommon.h"

#include "../../wx/wxcommon.h"
#include "backend/APT/APTFileIO.h"
#include "backend/plot.h"

#include <wx/filename.h>
#include <wx/dir.h>

using std::vector;
using std::string;
using std::pair;
using std::make_pair;

//!Error codes
enum
{
	COMMANDLINE_FAIL=1,
	SYSTEM_EXEC_FAIL,
	SETWORKDIR_FAIL,
	WRITEPOS_FAIL,
	WRITEPLOT_FAIL,
	MAKEDIR_FAIL,
	PLOTCOLUMNS_FAIL,
	READPLOT_FAIL,
	READPOS_FAIL,
	SUBSTITUTE_FAIL,
	COMMAND_FAIL, 
	EXT_PROG_ERR_ENUM_END, 
};

//=== External program filter === 
ExternalProgramFilter::ExternalProgramFilter() : alwaysCache(false),
		cleanInput(true)
{
	cacheOK=false;
	cache=false; 
}

Filter *ExternalProgramFilter::cloneUncached() const
{
	ExternalProgramFilter *p=new ExternalProgramFilter();

	//Copy the values
	p->workingDir=workingDir;
	p->commandLine=commandLine;
	p->alwaysCache=alwaysCache;
	p->cleanInput=cleanInput;

	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t ExternalProgramFilter::numBytesForCache(size_t nObjects) const
{
	if(alwaysCache)
		return 0;
	else
		return (size_t)-1; //Say we don't know, we are not going to cache anyway.
}

size_t ExternalProgramFilter::substituteVariables(const std::string &commandStr,
			const vector<string> &ionOutputNames, const vector<string> &plotOutputNames,std::string &substitutedCommand) 
{
	vector<string> commandLineSplit;

	splitStrsRef(commandStr.c_str(),' ',commandLineSplit);
	//Nothing to do
	if(commandLineSplit.empty())
		return 0;

	//Construct the command, using substitution
	string command;
	unsigned int ionOutputPos,plotOutputPos;
	ionOutputPos=plotOutputPos=0;
	command = commandLineSplit[0];
	for(unsigned int ui=1;ui<commandLineSplit.size();ui++)
	{
		if(commandLineSplit[ui].find('%') == std::string::npos)
		{
			command+= string(" ") + commandLineSplit[ui];
		}
		else
		{
			size_t pos,lastPos;
			lastPos=pos=0;

			string thisCommandEntry;
			pos =commandLineSplit[ui].find("%",pos);
			while(pos != string::npos)
			{
				//OK, so we found a % symbol.
				//lets do some substitution
				
				//% must be followed by something otherwise this is an error
				if(pos == commandLineSplit[ui].size())
					return COMMANDLINE_FAIL;

				char code;
				code = commandLineSplit[ui][pos+1];

				thisCommandEntry+=commandLineSplit[ui].substr(lastPos,pos-lastPos);

				switch(code)
				{
					case '%':
						//Escape '%%' to '%' symbol
						thisCommandEntry+="%";
						pos+=2;
						break;
					case 'i':
					{
						//Substitute '%i' with ion file name
						if(ionOutputPos == ionOutputNames.size())
						{
							//User error; not enough pos files to fill.
							return SUBSTITUTE_FAIL;
						}

						thisCommandEntry+=ionOutputNames[ionOutputPos];
						ionOutputPos++;
						pos++;
						break;
					}
					case 'I':
					{
						//Substitute '%I' with all ion file names, space separated
						if(ionOutputPos == ionOutputNames.size())
						{
							//User error. not enough pos files to fill
							return SUBSTITUTE_FAIL;
						}
						for(unsigned int ui=ionOutputPos; ui<ionOutputNames.size();ui++)
							thisCommandEntry+=ionOutputNames[ui] + " ";

						ionOutputPos=ionOutputNames.size();
						pos++;
						break;
					}
					case 'p':
					{
						//Substitute '%p' with all plot file names, space separated
						if(plotOutputPos == plotOutputNames.size())
						{
							//User error. not enough pos files to fill
							return SUBSTITUTE_FAIL;
						}
						for(unsigned int ui=plotOutputPos; ui<plotOutputNames.size();ui++)
							thisCommandEntry+=plotOutputNames[ui];

						plotOutputPos=plotOutputNames.size();
						pos++;
						break;
					}
					case 'P': 
					{
						//Substitute '%I' with all plot file names, space separated
						if(plotOutputPos == plotOutputNames.size())
						{
							//User error. not enough pos files to fill
							return SUBSTITUTE_FAIL;
						}
						for(unsigned int ui=plotOutputPos; ui<plotOutputNames.size();ui++)
							thisCommandEntry+=plotOutputNames[ui]+ " ";

						plotOutputPos=plotOutputNames.size();
						pos++;
						break;
					}
					default:
						//Invalid user input string. % must be escaped or recognised.
						return SUBSTITUTE_FAIL;
				}


				lastPos=pos;
				pos =commandLineSplit[ui].find("%",pos);
			}
		
			thisCommandEntry+=commandLineSplit[ui].substr(lastPos+1);

			command+= string(" ") + thisCommandEntry;
		}

	}


	command.swap(substitutedCommand);

	return 0;
}

unsigned int ExternalProgramFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		propagateCache(getOut);
		progress.filterProgress=100;
		return 0;
	}

	if(commandLine.empty())
	{
		progress.filterProgress=100;
		return 0;
	}
	vector<string> ionOutputNames,plotOutputNames;

	//Compute the bounding box of the incoming streams
	string s;
	wxString tempDir;
	if(workingDir.size())
		tempDir=((workingDir) +wxT("/inputData"));
	else
		tempDir=(wxT("inputData"));


	//Create a temporary dir
	if(!wxDirExists(tempDir) )
	{
		//Audacity claims that this can return false even on
		//success (NoiseRemoval.cpp, line 148).
		//I was having problems with this function too;
		//so use their workaround
		wxMkdir(tempDir);

		if(!wxDirExists(tempDir) )	
			return MAKEDIR_FAIL;

	
	}
	progress.maxStep=3;
	progress.step=1;
	progress.stepName=TRANS("Collate Input");	
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				const IonStreamData *i;
				i = (const IonStreamData * )(dataIn[ui]);

				if(i->data.empty())
					break;
				//Save the data to a file
				wxString tmpStr;

				tmpStr=wxFileName::CreateTempFileName(tempDir+ wxT("/pointdata"));
				//wxwidgets has no suffix option... annoying.
				wxRemoveFile(tmpStr);

				s = stlStr(tmpStr);
				s+=".pos";
				if(IonHit::makePos(i->data,s.c_str()))
				{
					//Uh-oh problem. Clean up and exit
					return WRITEPOS_FAIL;
				}

				ionOutputNames.push_back(s);
				break;
			}
			case STREAM_TYPE_PLOT:
			{
				const PlotStreamData *i;
				i = (const PlotStreamData * )(dataIn[ui]);

				if(i->xyData.empty())
					break;
				//Save the data to a file
				wxString tmpStr;

				tmpStr=wxFileName::CreateTempFileName(tempDir + wxT("/plot"));
				//wxwidgets has no suffix option... annoying.
				wxRemoveFile(tmpStr);
				s = stlStr(tmpStr);
			        s+= ".xy";
				if(!writeTextFile(s.c_str(),i->xyData))
				{
					//Uh-oh problem. Clean up and exit
					return WRITEPLOT_FAIL;
				}

				plotOutputNames.push_back(s);
				break;
			}
			default:
				break;
		}	
	}

	//Nothing to do.
	if(plotOutputNames.empty() &&
		ionOutputNames.empty())
	{
		progress.filterProgress=100;
		return 0;
	}
	std::string substitutedCommand;
	size_t errCode;
	errCode=substituteVariables(commandLine,ionOutputNames,plotOutputNames,
					substitutedCommand);
	if(errCode)
		return errCode;

	//If we have a specific working dir; use it. Otherwise just use curdir
	wxString origDir = wxGetCwd();
	if(workingDir.size())
	{
		//Set the working directory before launching
		if(!wxSetWorkingDirectory((workingDir)))
			return SETWORKDIR_FAIL;
	}
	else
	{
		if(!wxSetWorkingDirectory(wxT(".")))
			return SETWORKDIR_FAIL;
	}

	int result;
	progress.step=2;
	progress.stepName=TRANS("Execute");	

	//Execute the program
	//TODO: IO redirection - especially under windows?
	result=std::system(substitutedCommand.c_str());

	if(result == -1)
		return SYSTEM_EXEC_FAIL; 

	if(cleanInput)
	{
		//If program returns error, this was a problem
		//delete the input files.
		for(unsigned int ui=0;ui<ionOutputNames.size();ui++)
		{
			//try to delete the file, if the command did not
			// remove it
			if(wxFileExists(ionOutputNames[ui]))
				wxRemoveFile(ionOutputNames[ui]);

		}
		for(unsigned int ui=0;ui<plotOutputNames.size();ui++)
		{
			//try to delete the file
			wxRemoveFile((plotOutputNames[ui]));
		}
	}
	wxSetWorkingDirectory(origDir);	
	if(result)
		return COMMAND_FAIL; 
	
	wxSetWorkingDirectory(origDir);	

	wxDir *dir = new wxDir;
	wxArrayString *a = new wxArrayString;
	if(workingDir.size())
		dir->GetAllFiles((workingDir),a,wxT("*.pos"),wxDIR_FILES);
	else
		dir->GetAllFiles(wxGetCwd(),a,wxT("*.pos"),wxDIR_FILES);

	progress.step=3;
	progress.stepName=TRANS("Collate output");	

	//read the output files, which is assumed to be any "pos" file
	//in the working dir
	for(unsigned int ui=0;ui<a->Count(); ui++)
	{
		wxULongLong size;
		size = wxFileName::GetSize((*a)[ui]);

		if( (size !=0) && size!=wxInvalidSize)
		{
			//Load up the pos file

			string sTmp;
			wxString wxTmpStr;
			wxTmpStr=(*a)[ui];
			sTmp = stlStr(wxTmpStr);
			unsigned int dummy;
			IonStreamData *d = new IonStreamData();
			d->parent=this;
			//TODO: some kind of secondary file for specification of
			//ion attribs?
			d->r = 1.0;
			d->g=0;
			d->b=0;
			d->a=1.0;
			d->ionSize = 2.0;

			unsigned int index2[] = {
					0, 1, 2, 3
					};
			if(GenericLoadFloatFile(4, 4, index2, d->data,sTmp.c_str(),dummy,*(Filter::wantAbort)))
			{
				delete d;
				delete dir;
				return READPOS_FAIL;
			}


			if(alwaysCache)
			{
				d->cached=1;
				filterOutputs.push_back(d);
			}
			else
				d->cached=0;
			getOut.push_back(d);
		}
	}

	a->Clear();
	if(workingDir.size())
		dir->GetAllFiles((workingDir),a,wxT("*.xy"),wxDIR_FILES);
	else
		dir->GetAllFiles(wxGetCwd(),a,wxT("*.xy"),wxDIR_FILES);

	//read the output files, which is assumed to be any "pos" file
	//in the working dir
	for(unsigned int ui=0;ui<a->Count(); ui++)
	{
		wxULongLong size;
		size = wxFileName::GetSize((*a)[ui]);

		if( (size !=0) && size!=wxInvalidSize)
		{
			string sTmp;
			wxString wxTmpStr;
			wxTmpStr=(*a)[ui];
			sTmp = stlStr(wxTmpStr);

			vector<vector<float> > dataVec;

			vector<string> header;	

			//Possible delimiters to try when loading file
			//try each in turn
			const char *delimString ="\t, ";
			if(loadTextData(sTmp.c_str(),dataVec,header,delimString))
			{
				delete dir;
				return READPLOT_FAIL;
			}

			//Check that the input has the correct size
			for(unsigned int uj=0;uj<dataVec.size()-1;uj+=2)
			{
				//well the columns don't match
				if(dataVec[uj].size() != dataVec[uj+1].size())
				{
					delete dir;
					return PLOTCOLUMNS_FAIL;
				}
			}

			//Check to see if the header might be able
			//to be matched to the data
			bool applyLabels=false;
			if(header.size() == dataVec.size())
				applyLabels=true;

			for(unsigned int uj=0;uj<dataVec.size()-1;uj+=2)
			{
				//TODO: some kind of secondary file for specification of
				//plot attribs?
				PlotStreamData *d = new PlotStreamData();
				d->parent=this;
				d->r = 0.0;
				d->g=1.0;
				d->b=0;
				d->a=1.0;
				d->index=uj;
				d->plotMode=PLOT_MODE_1D;
				d->plotStyle=PLOT_LINE_LINES;


				//set the title to the filename (trim the .xy extension
				//and the working directory name)
				string tmpFilename;
				tmpFilename=sTmp.substr(workingDir.size(),sTmp.size()-
								workingDir.size()-3);

				d->dataLabel=getUserString() + string(":") + onlyFilename(tmpFilename);

				if(applyLabels)
				{

					//set the xy-labels to the column headers
					d->xLabel=header[uj];
					d->yLabel=header[uj+1];
				}
				else
				{
					d->xLabel="x";
					d->yLabel="y";
				}

				d->xyData.resize(dataVec[uj].size());

				ASSERT(dataVec[uj].size() == dataVec[uj+1].size());
				for(unsigned int uk=0;uk<dataVec[uj].size(); uk++)
				{
					d->xyData[uk]=make_pair(dataVec[uj][uk], 
								dataVec[uj+1][uk]);
				}
				
				if(alwaysCache)
				{
					d->cached=1;
					filterOutputs.push_back(d);
				}
				else
					d->cached=0;
				
				getOut.push_back(d);
			}
		}
	}

	if(alwaysCache)
		cacheOK=true;

	delete dir;
	delete a;
	progress.filterProgress=100;

	return 0;
}


void ExternalProgramFilter::getProperties(FilterPropGroup &propertyList) const
{
	std::string tmpStr;
	size_t curGroup=0;
	FilterProperty p;

	p.name=TRANS("Command");
	p.data= commandLine;
	p.type=PROPERTY_TYPE_STRING;
	p.helpText=TRANS("Full command to send to operating system. See manual for escape sequence meanings");
	p.key=EXTERNALPROGRAM_KEY_COMMAND;
	propertyList.addProperty(p,curGroup);
	
	p.name=TRANS("Work Dir");
	p.data= workingDir;
	p.type=PROPERTY_TYPE_DIR;
	p.helpText=TRANS("Directory to run the command in");
	p.key=EXTERNALPROGRAM_KEY_WORKDIR;		
	propertyList.addProperty(p,curGroup);
	
	propertyList.setGroupTitle(curGroup,TRANS("Command"));
	curGroup++;
	tmpStr=boolStrEnc(cleanInput);
	p.name=TRANS("Cleanup input");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Erase input files when command completed");
	p.key=EXTERNALPROGRAM_KEY_CLEANUPINPUT;		
	propertyList.addProperty(p,curGroup);
	
	tmpStr=boolStrEnc(alwaysCache);
	p.name=TRANS("Cache");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Assume program does not alter its output, unless inputs from 3Depict are altered");
	p.key=EXTERNALPROGRAM_KEY_ALWAYSCACHE;		
	propertyList.addProperty(p,curGroup);

	propertyList.setGroupTitle(curGroup,TRANS("Data"));
}

bool ExternalProgramFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{
	needUpdate=false;
	switch(key)
	{
		case EXTERNALPROGRAM_KEY_COMMAND:
		{
			if(!applyPropertyNow(commandLine,value,needUpdate))
				return false;
			break;
		}
		case EXTERNALPROGRAM_KEY_WORKDIR:
		{
			if(workingDir!=value)
			{
				//Check the directory exists
				if(!wxDirExists((value)))
					return false;

				workingDir=value;
				needUpdate=true;
				clearCache();
			}
			break;
		}
		case EXTERNALPROGRAM_KEY_ALWAYSCACHE:
		{
			if(!applyPropertyNow(alwaysCache,value,needUpdate))
				return false;
			break;
		}
		case EXTERNALPROGRAM_KEY_CLEANUPINPUT:
		{
			if(!applyPropertyNow(cleanInput,value,needUpdate))
				return false;
			break;
		}
		default:
			ASSERT(false);

	}

	return true;
}


std::string  ExternalProgramFilter::getSpecificErrString(unsigned int code) const
{
	const char *errStrs[] = 	{ "",
			"Error processing command line",
			"Unable to launch external program",
			"Unable to set working directory",
			"Error saving posfile result for external program",
			"Error saving plot result for externalprogram",
			"Error creating temporary directory",
			"Detected unusable number of columns in plot",
			"Unable to parse plot result from external program",
			"Unable to load ions from external program", 
			"Unable to perform commandline substitution",
			"Error executing external program, returned nonzero" };
	
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(errStrs) == EXT_PROG_ERR_ENUM_END);
	ASSERT(code < EXT_PROG_ERR_ENUM_END);
	return errStrs[code];
}

void ExternalProgramFilter::setPropFromBinding(const SelectionBinding &b)
{
	ASSERT(false);
}

bool ExternalProgramFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{
			f << tabs(depth) << "<" << trueName() << ">" << endl;

			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;
			f << tabs(depth+1) << "<commandline name=\"" << escapeXML(commandLine )<< "\"/>" << endl;
			f << tabs(depth+1) << "<workingdir name=\"" << escapeXML(convertFileStringToCanonical(workingDir)) << "\"/>" << endl;
			f << tabs(depth+1) << "<alwayscache value=\"" << alwaysCache << "\"/>" << endl;
			f << tabs(depth+1) << "<cleaninput value=\"" << cleanInput << "\"/>" << endl;
			f << tabs(depth) << "</" << trueName() << ">" << endl;
			break;

		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool ExternalProgramFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	//Retrieve user string
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);

	//Retrieve command
	if(XMLHelpFwdToElem(nodePtr,"commandline"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
	if(!xmlString)
		return false;
	commandLine=(char *)xmlString;
	xmlFree(xmlString);
	
	//Retrieve working dir
	if(XMLHelpFwdToElem(nodePtr,"workingdir"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
	if(!xmlString)
		return false;
	workingDir=(char *)xmlString;
	xmlFree(xmlString);


	//get should cache
	string tmpStr;
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"alwayscache","value"))
		return false;

	if(!boolStrDec(tmpStr,alwaysCache))
		return false;

	//check readable 
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"cleaninput","value"))
		return false;

	if(!boolStrDec(tmpStr,cleanInput))
		return false;

	return true;
}

unsigned int ExternalProgramFilter::getRefreshBlockMask() const
{
	//Absolutely nothing can go through this filter.
	return 0;
}

unsigned int ExternalProgramFilter::getRefreshEmitMask() const
{
	//Can only generate ion streams and plot streams
	return STREAM_TYPE_IONS | STREAM_TYPE_PLOT;
}

unsigned int ExternalProgramFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_IONS | STREAM_TYPE_PLOT;
}

#ifdef DEBUG
#include <memory>

using std::auto_ptr;
using std::ifstream;

bool echoTest()
{
	int errCode;
#if !defined(__WIN32__) && !defined(__WIN64__)
	errCode=system("echo testing... > /dev/null");
#else
	errCode=system("echo testing... > NUL");
#endif
	if(errCode)
	{
		WARN(false,"Unable to perform echo test on this system -- echo missing?");
		return true;
	}
	
	ExternalProgramFilter* f = new ExternalProgramFilter;
	f->setCaching(false);

	bool needUp;
	string s;
				
	wxString tmpFilename;
	tmpFilename=wxFileName::CreateTempFileName(wxT(""));
	s = string(" echo test > ") + stlStr(tmpFilename);
	TEST(f->setProperty(EXTERNALPROGRAM_KEY_COMMAND,s,needUp),"Set prop");

	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	ProgressData p;
	f->refresh(streamIn,streamOut,p);


	s=stlStr(tmpFilename);
	ifstream file(s.c_str());
	
	TEST(file,"echo retrieval");


	wxRemoveFile(tmpFilename);

	delete f;

	return true;
}

IonStreamData* createTestPosData(unsigned int numPts)
{
	IonStreamData* d= new IonStreamData;

	d->data.resize(numPts);
	for(unsigned int ui=0;ui<numPts;ui++)
	{
		d->data[ui].setPos(ui,ui,ui);
		d->data[ui].setMassToCharge(ui);
	}

	return d;
}

bool posTest()
{
	const unsigned int NUM_PTS=100;
	auto_ptr<IonStreamData> someData;
	someData.reset(createTestPosData(NUM_PTS));

	ExternalProgramFilter* f = new ExternalProgramFilter;
	f->setCaching(false);

	bool needUp;
	string s;

	wxString tmpFilename,tmpDir;
	tmpDir=wxFileName::GetTempDir();


#if defined(__WIN32__) || defined(__WIN64__)
	tmpDir=tmpDir + wxT("\\3Depict\\");

#else
	tmpDir=tmpDir + wxT("/3Depict/");
#endif
	if(wxDirExists(tmpDir))
	{
		wxFileName dirFile(tmpDir);
		dirFile.Rmdir( wxPATH_RMDIR_RECURSIVE);
	}

		
	wxMkdir(tmpDir);

	std::string randName;
	genRandomFilename(randName);
	tmpFilename = tmpDir + "/" + randName + ".pos";
	s ="mv -f \%i " + tmpFilename;

	ASSERT(tmpFilename.size());
	
	TEST(f->setProperty(EXTERNALPROGRAM_KEY_COMMAND,s,needUp),"Set prop");
	TEST(f->setProperty(EXTERNALPROGRAM_KEY_WORKDIR,stlStr(tmpDir),needUp),"Set prop");
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(someData.get());
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"refresh error code");

	//Should have exactly one stream, which is an ion stream
	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");

	TEST(streamOut[0]->getNumBasicObjects() ==NUM_PTS,"Number of ions");
	const IonStreamData* out =(IonStreamData*) streamOut[0]; 

	for(unsigned int ui=0;ui<out->data.size();ui++)
	{
		TEST(out->data[ui].getPos() == someData->data[ui].getPos(),"position");
		TEST(out->data[ui].getMassToCharge() == 
			someData->data[ui].getMassToCharge(),"position");
	}



	wxRemoveFile(tmpFilename);
	wxRmdir(tmpDir+wxT("inputData"));
	wxRmdir(tmpDir);

	delete streamOut[0];

	delete f;

	return true;
}


bool ExternalProgramFilter::substituteTest()
{
	
	vector<string> ionNames,plotNames;
	plotNames.push_back("some Plot.xy");
	ionNames.push_back("my \"pos file.pos");

	string commandLine;
	commandLine="echo \"My ions are \'%i\'\"";


	string resultString;
	TEST(!substituteVariables(commandLine,ionNames,plotNames,resultString),"substitution fail");
	TEST(resultString == "echo \"My ions are \'my \"pos file.pos\'\"","substitution fail");

	commandLine=" echo (\"%i\")";
	TEST(!substituteVariables(commandLine,ionNames,plotNames,resultString),"substitution fail");
	TEST(resultString == " echo (\"my \"pos file.pos\")","substitution fail");


	return true;

}

bool ExternalProgramFilter::runUnitTests() 
{
	if(!echoTest())
		return false;

#ifndef __APPLE__
	if(!posTest())
		return false;

	if(!substituteTest())
		return false;
#endif
	return true;
}


#endif
