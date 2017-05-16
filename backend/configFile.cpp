/*
 *	configFile.cpp  - User configuration loading/saving
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

#include "configFile.h"
#include "wx/wxcommon.h"


#include "backend/filters/allFilter.h"

#include "common/stringFuncs.h"
#include "common/xmlHelper.h"



const char *CONFIG_FILENAME="config.xml";

const unsigned int MAX_RECENT=9;

const unsigned int MAX_MOUSE_PERCENT= 400;



#include <wx/stdpaths.h>


using std::endl;
using std::string;
using std::deque;


ConfigFile::ConfigFile() : configLoadOK(false), panelMode(CONFIG_PANELMODE_REMEMBER),
	haveInitialAppSize(false), mouseZoomRatePercent(100),mouseMoveRatePercent(100),
	wantStartupOrthoCam(false),allowOnline(true), allowOnlineVerCheck(true), leftRightSashPos(0),
	topBottomSashPos(0),filterSashPos(0),plotListSashPos(0), haveMaxPoints(false),
	maxPointsScene(0), doWantStartupTips(true)
{ 
}


ConfigFile::~ConfigFile()
{
	for(unsigned int ui=0;ui<filterDefaults.size();ui++)
		delete filterDefaults[ui];
}

unsigned int ConfigFile::getMaxHistory()
{
	return MAX_RECENT;
}

void ConfigFile::addRecentFile(const  std::string &str)
{
	recentFiles.push_back(str);
	if(recentFiles.size() > MAX_RECENT)
		recentFiles.pop_front();
}

void ConfigFile::getRecentFiles(std::vector<std::string> &files) const
{
	files.resize(recentFiles.size());
	std::copy(recentFiles.begin(),recentFiles.end(),files.begin());
}

void ConfigFile::removeRecentFile(const std::string &str)
{
	std::deque<string>::iterator it;
	it=std::find(recentFiles.begin(),recentFiles.end(),str);

	if(it!=recentFiles.end())
		recentFiles.erase(it);
}

void ConfigFile::getFilterDefaults(vector<Filter *>  &defs)
{
	defs.resize(filterDefaults.size());
	std::copy(filterDefaults.begin(),filterDefaults.end(),defs.begin());
}
void ConfigFile::setFilterDefaults(const vector<Filter *>  &defs)
{
	for(unsigned int ui=0;ui<filterDefaults.size();ui++)
		delete filterDefaults[ui];
	filterDefaults.resize(defs.size());

	for(unsigned int ui=0;ui<filterDefaults.size();ui++)
	{
		//Disallow storing of potentially hazardous filters
		filterDefaults[ui]=defs[ui];
		ASSERT(!(filterDefaults[ui]->canBeHazardous()));
	}
}



bool ConfigFile::getInitialAppSize(unsigned int &x, unsigned int &y) const
{
	if(haveInitialAppSize)
	{
		x=initialSizeX;
		y=initialSizeY;
	}
	
	return haveInitialAppSize;
}

void ConfigFile::setInitialAppSize(unsigned int x, unsigned int y)
{
	haveInitialAppSize=true;
	initialSizeX=x;
	initialSizeY=y;
}

Filter *ConfigFile::getDefaultFilter(unsigned int type) const
{
	for(unsigned int ui=0;ui<filterDefaults.size();ui++)
	{
		if(type == filterDefaults[ui]->getType())
		{
			ASSERT(!filterDefaults[ui]->canBeHazardous());
			return filterDefaults[ui]->cloneUncached();
		}
	}

	return makeFilter(type);	
}

unsigned int ConfigFile::read()
{
	string filename;
	filename = getConfigDir() + std::string("/") + std::string(CONFIG_FILENAME);

	//Load the state from an XML file
	//here we use libxml2's loading routines
	//http://xmlsoft.org/
	//Tutorial: http://xmlsoft.org/tutorial/xmltutorial.pdf
	xmlDocPtr doc;
	xmlParserCtxtPtr context;

	context =xmlNewParserCtxt();

	if(!context)
	{
		return CONFIG_ERR_NOPARSER;
	}

	//Open the XML file again, but without DTD validation
	doc = xmlCtxtReadFile(context, filename.c_str(), NULL, XML_PARSE_NONET|XML_PARSE_NOENT);

	if(!doc)
		return CONFIG_ERR_NOFILE;

	//release the context
	xmlFreeParserCtxt(context);

	//retrieve root node	
	xmlNodePtr nodePtr = xmlDocGetRootElement(doc);


	try
	{
		std::stack<xmlNodePtr>  nodeStack;

		//Umm where is our root node guys?
		if(!nodePtr)
			throw 1;
		
		//push root tag	
		nodeStack.push(nodePtr);
		
		//This *should* be an threeDepict state file
		if(xmlStrcmp(nodePtr->name, (const xmlChar *)"threeDepictconfig"))
		{
			errMessage=TRANS("Config file present, but is not valid (root node test)");
			throw 1;
		}

		//push root node
		nodeStack.push(nodePtr);
		nodePtr=nodePtr->xmlChildrenNode;	
		
		//push not quite root tag	
		nodeStack.push(nodePtr);
		if(!XMLHelpFwdToElem(nodePtr,"initialwinsize"))
		{
			if(XMLGetAttrib(nodePtr, initialSizeX, "width") &&
				XMLGetAttrib(nodePtr,initialSizeY,"height"))
			{
				if( initialSizeX >0 && initialSizeY > 0)
					haveInitialAppSize=true;
			}
		}
		nodePtr=nodeStack.top();
		nodeStack.pop();


		//Clean up current configuration
		recentFiles.clear();
		
		if(!XMLHelpFwdToElem(nodePtr,"recent"))
		{
			nodeStack.push(nodePtr);
			nodePtr=nodePtr->xmlChildrenNode;


			std::string thisName;
			while(!XMLHelpFwdToElem(nodePtr,"file") && recentFiles.size() < MAX_RECENT)
			{
				xmlChar *xmlString;
				
				//read name of file
				xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
				if(!xmlString)
				{
					errMessage=TRANS("Unable to interpret recent file entry");
					throw 1;
				}
				thisName=(char *)xmlString;

				recentFiles.push_back(thisName);

				xmlFree(xmlString);
			}
		}

		//restore old node	
		nodePtr=nodeStack.top();
		nodeStack.pop();
		
		//Advance and push
		if(!nodePtr->next)
			goto nodeptrEndJump;

		nodePtr=nodePtr->next;
		nodeStack.push(nodePtr);
	   
		if(!XMLHelpFwdToElem(nodePtr,"filterdefaults"))
		{
			nodePtr=nodePtr->xmlChildrenNode;

			if(nodePtr)
			{

				
				while(!XMLHelpNextType(nodePtr,XML_ELEMENT_NODE))
				{
					string s;
					
					s=(char *)(nodePtr->name);
					Filter *f;
					f=makeFilter(s);

					if(!f)
					{
						errMessage=TRANS("Unable to determine filter type in defaults listing.");
						throw 1;
					}
			

					//potentially hazardous filters cannot have their
					//default properties altered. Quietly drop them
					if(!f->canBeHazardous())
					{
						nodeStack.push(nodePtr);
						nodePtr=nodePtr->xmlChildrenNode;
						if(f->readState(nodePtr))
							filterDefaults.push_back(f);

						nodePtr=nodeStack.top();
						nodeStack.pop();
					}	
				}

			}
		}
		
		//restore old node	
		nodePtr=nodeStack.top();
		nodeStack.pop();
		
		//Advance and push
		if(!nodePtr->next)
			goto nodeptrEndJump;

		nodePtr=nodePtr->next;
		nodeStack.push(nodePtr);
		if(!XMLHelpFwdToElem(nodePtr,"startuppanels"))
		{
			startupPanelView.resize(CONFIG_STARTUPPANEL_END_ENUM);

			std::string tmpStr;
			xmlChar *xmlString;
			xmlString=xmlGetProp(nodePtr,(xmlChar*)"mode");

			if(xmlString)
			{
				tmpStr=(char*)xmlString;
				
				panelMode=CONFIG_PANELMODE_NONE;
				stream_cast(panelMode,tmpStr);
			
				if(panelMode >=CONFIG_PANELMODE_END_ENUM)	
					panelMode=CONFIG_PANELMODE_NONE;

				xmlFree(xmlString);
			}

			if(panelMode)
			{
				xmlString=xmlGetProp(nodePtr,(xmlChar*)"rawdata");
				if(xmlString)
				{
				
					tmpStr=(char *)xmlString;
					if(tmpStr == "1")
						startupPanelView[CONFIG_STARTUPPANEL_RAWDATA]=true;
					else
						startupPanelView[CONFIG_STARTUPPANEL_RAWDATA]=false;
				
					xmlFree(xmlString);
				}
				
				xmlString=xmlGetProp(nodePtr,(xmlChar*)"control");
				if(xmlString)
				{
				
					tmpStr=(char *)xmlString;
					if(tmpStr == "1")
						startupPanelView[CONFIG_STARTUPPANEL_CONTROL]=true;
					else
						startupPanelView[CONFIG_STARTUPPANEL_CONTROL]=false;
					xmlFree(xmlString);
				}

				xmlString=xmlGetProp(nodePtr,(xmlChar*)"plotlist");
				if(xmlString)
				{
				
					tmpStr=(char *)xmlString;
					if(tmpStr == "1")
						startupPanelView[CONFIG_STARTUPPANEL_PLOTLIST]=true;
					else
						startupPanelView[CONFIG_STARTUPPANEL_PLOTLIST]=false;
					xmlFree(xmlString);
				}
		
			}
		}

		//restore old node	
		nodePtr=nodeStack.top();
		nodeStack.pop();
		
		//Advance and push, as needed
		if(!nodePtr->next)
			goto nodeptrEndJump;

		nodePtr=nodePtr->next;
		nodeStack.push(nodePtr);
		if(!XMLHelpFwdToElem(nodePtr,"mousedefaults"))
		{
			xmlNodePtr mouseDataNodePtr=nodePtr->xmlChildrenNode;
			if(mouseDataNodePtr)
			{
				nodeStack.push(mouseDataNodePtr);
				if(!XMLHelpFwdToElem(mouseDataNodePtr,"speed"))
				{
					unsigned int percentage;
					if(XMLGetAttrib(mouseDataNodePtr,percentage,"zoom") && percentage <MAX_MOUSE_PERCENT)
						mouseZoomRatePercent=percentage;
					
					if(XMLGetAttrib(mouseDataNodePtr,percentage,"move") && percentage < MAX_MOUSE_PERCENT)
						mouseMoveRatePercent=percentage;
				}
				mouseDataNodePtr=nodeStack.top();
				nodeStack.pop();
			}
		}

		nodePtr=nodePtr->next;
		nodeStack.push(nodePtr);
		if(!XMLHelpFwdToElem(nodePtr,"netaccess"))
		{
			std::string tmpStr;
			xmlChar *xmlString;
			
			xmlString=xmlGetProp(nodePtr,(xmlChar*)"enabled");
			if(xmlString)
			{
				tmpStr=(char *)xmlString;

				if(!(tmpStr == "1" || tmpStr == "0"))
					throw 1;

				allowOnline = (tmpStr == "1");
				xmlFree(xmlString);
			}
			
			if(nodePtr->xmlChildrenNode)
			{
				nodePtr=nodePtr->xmlChildrenNode;

				if(!XMLHelpFwdToElem(nodePtr,"versioncheck"))
				{
					xmlChar *xmlString;
					
					xmlString=xmlGetProp(nodePtr,(xmlChar*)"enabled");
					if(xmlString)
					{
						tmpStr=(char *)xmlString;
						if(!(tmpStr == "1" || tmpStr == "0"))
							throw 1;

						allowOnlineVerCheck = (tmpStr == "1");
						xmlFree(xmlString);
					}

				}
			}
		}
		nodePtr=nodeStack.top();
		nodeStack.pop();


		nodeStack.push(nodePtr);
		if(!XMLHelpFwdToElem(nodePtr,"sashposition"))
		{
			if(nodePtr->xmlChildrenNode)
			{
				nodePtr=nodePtr->xmlChildrenNode;

				while(!XMLHelpFwdToElem(nodePtr,"pos"))
				{

					string name;
					if(XMLGetAttrib(nodePtr, name,"name"))
					{
						if(name == "topbottom")
							XMLGetAttrib(nodePtr,topBottomSashPos,"value");
						if(name == "leftright")
							XMLGetAttrib(nodePtr,leftRightSashPos,"value");
						if(name == "filter")
							XMLGetAttrib(nodePtr,filterSashPos,"value");
						if(name == "plotlist")
							XMLGetAttrib(nodePtr,plotListSashPos,"value");
					}

					nodePtr=nodePtr->next;
				}
			}
		}
		nodePtr=nodeStack.top();
		nodeStack.pop();


		nodeStack.push(nodePtr);
		haveMaxPoints=XMLGetNextElemAttrib(nodePtr,maxPointsScene,"maxdisplaypoints","value");

		nodePtr=nodeStack.top();
		nodeStack.pop();

		nodeStack.push(nodePtr);
		//have we seen a startup tip entry?
		if(!XMLHelpFwdToElem(nodePtr,"startuptips"))
		{
			std::string str;
			XMLGetAttrib(nodePtr,str,"value");

			//Check if the user wants startup tips. If we cant understand this, then say no.
			if(!boolStrDec(str,doWantStartupTips))
				doWantStartupTips=false;
		}
		nodePtr=nodeStack.top();
		nodeStack.pop();
		
		nodeStack.push(nodePtr);
		//Does the user want, by default, an orthographic camera
		if(!XMLHelpFwdToElem(nodePtr,"wantorthocam"))
		{
			std::string str;
			XMLGetAttrib(nodePtr,str,"value");

			//Check if the user wants ortho camera, if no idea, revert to not 
			if(!boolStrDec(str,wantStartupOrthoCam))
				wantStartupOrthoCam=false;
		}
		nodePtr=nodeStack.top();
		nodeStack.pop();


nodeptrEndJump:
		;

	}
	catch (int)
	{
		//Code threw an error, just say "bad parse" and be done with it
		xmlFreeDoc(doc);
		return CONFIG_ERR_BADFILE;
	}


	xmlFreeDoc(doc);

	configLoadOK=true;
	return 0;

}

bool ConfigFile::createConfigDir()
{
	wxString filePath = (getConfigDir());

	//Create the folder if it does not exist
	if(!wxDirExists(filePath))
	{
		if(!wxMkdir(filePath))
			return false;

		//Make it a hidden folder
#if defined(__WIN32) || defined(__WIN64)
		SetFileAttributes(filePath.wc_str(),FILE_ATTRIBUTE_HIDDEN);
#endif
	}
	
	return true;
}

std::string ConfigFile::getConfigDir() 
{
 	wxStandardPaths &paths = wxStandardPaths::Get();
	wxString filePath = paths.GetDocumentsDir()+("/.")+(PROGRAM_NAME);
	return stlStr(filePath);
}


bool ConfigFile::write()
{
	string filename;
	
	if(!createConfigDir())
		return false;

	filename = getConfigDir() + std::string("/") + std::string(CONFIG_FILENAME);

	//Open file for output
	std::ofstream f(filename.c_str());

	if(!f)
		return false;

	//Write state open tag 
	f<< "<threeDepictconfig>" << endl;
	f<<tabs(1)<< "<writer version=\"" << PROGRAM_VERSION << "\"/>" << endl;

	if(haveInitialAppSize)	
	{
		f<<tabs(1)<< "<initialwinsize width=\"" << initialSizeX << "\" height=\"" <<
				initialSizeY << "\"/>" << endl;
	}

	f<<tabs(1) << "<recent>" << endl;

	for(unsigned int ui=0;ui<recentFiles.size();ui++)
		f<<tabs(2) << "<file name=\"" << recentFiles[ui] << "\"/>" << endl;

	f<< tabs(1) << "</recent>" << endl;
	
	f<< tabs(1) << "<filterdefaults>" << endl;

	for(unsigned int ui=0;ui<filterDefaults.size();ui++)
		filterDefaults[ui]->writeState(f,STATE_FORMAT_XML,2);
	f<< tabs(1) << "</filterdefaults>" << endl;

	if(startupPanelView.size())
	{
		ASSERT(startupPanelView.size() == CONFIG_STARTUPPANEL_END_ENUM);

		f << tabs(1) << "<startuppanels mode=\"" << panelMode << "\" rawdata=\"" << 
			boolStrEnc(startupPanelView[CONFIG_STARTUPPANEL_RAWDATA]) << 
			"\" control=\"" << boolStrEnc(startupPanelView[CONFIG_STARTUPPANEL_CONTROL])  << 
			"\" plotlist=\"" << boolStrEnc(startupPanelView[CONFIG_STARTUPPANEL_PLOTLIST]) << "\"/>" << endl;
	}

	f << tabs(1) <<  "<mousedefaults> " << endl;
	f << tabs(2) <<  "<speed zoom=\"" << mouseZoomRatePercent << "\" move=\"" << 
	       		mouseMoveRatePercent << "\"/>" << endl;
	f << tabs(1) <<  "</mousedefaults> " << endl;

	//Online access settings
#if (!defined(__APPLE__) && !defined(WIN32))
	f << tabs(1) <<"<!--" << TRANS("Online access for non win32/apple platforms is intentionally disabled, ") <<
		TRANS("regardless of the settings you use here. Use your package manager to keep up-to-date") << "-->" << endl;
#endif
	f << tabs(1) <<  "<netaccess enabled=\"" << allowOnline <<  "\"> " << endl;

	f << tabs(2) <<  "<versioncheck enabled=\"" << boolStrEnc(allowOnlineVerCheck) << "\"/> " << endl;
	
	f << tabs(1) <<  "</netaccess>" << endl;


	//Online access settings
	f << tabs(1) << "<sashposition>" << endl;
		if(topBottomSashPos)
			f << tabs(2) << "<pos name=\"topbottom\" value=\"" << topBottomSashPos << "\"/>" << endl;
		if(leftRightSashPos)
			f << tabs(2) << "<pos name=\"leftright\" value=\"" << leftRightSashPos<< "\"/>" << endl;
		if(filterSashPos)
			f << tabs(2) << "<pos name=\"filter\" value=\"" <<  filterSashPos<< "\"/>" << endl;
		if(plotListSashPos)
			f << tabs(2) << "<pos name=\"plotlist\" value=\"" << plotListSashPos<< "\"/>" << endl;
	f << tabs(1) << "</sashposition>" << endl;


	if(haveMaxPoints)
		f << tabs(1) << "<maxdisplaypoints value=\"" << maxPointsScene << "\"/>" << endl;

	f << tabs(1) << "<startuptips value=\"" << boolStrEnc(doWantStartupTips) << "\"/>" <<endl;
	f << tabs(1) << "<wantorthocam value=\"" << boolStrEnc(wantStartupOrthoCam) << "\"/>" <<endl;

	f << "</threeDepictconfig>" << endl;

	ASSERT(isValidXML(filename.c_str()));

	return true;
}

bool ConfigFile::getPanelEnabled(unsigned int panelID) const
{
	ASSERT(panelID < CONFIG_STARTUPPANEL_END_ENUM);

	switch(panelMode)
	{
		case CONFIG_PANELMODE_NONE:
			return true;	
		case CONFIG_PANELMODE_REMEMBER:
		case CONFIG_PANELMODE_SPECIFY:
			if(startupPanelView.size())
			{
				ASSERT(startupPanelView.size() == CONFIG_STARTUPPANEL_END_ENUM);
				return startupPanelView[panelID];
			}
			else
				return true;
		default:
			ASSERT(false);
	}
}

void ConfigFile::setPanelEnabled(unsigned int panelID, bool enabled, bool permanent) 
{
	ASSERT(panelID < CONFIG_STARTUPPANEL_END_ENUM);
	
	//Create the vector as needed, filling with default of "enabled"
	if(startupPanelView.empty())
		startupPanelView.resize(CONFIG_STARTUPPANEL_END_ENUM,true);

	ASSERT(startupPanelView.size() == CONFIG_STARTUPPANEL_END_ENUM);

	if(panelMode != CONFIG_PANELMODE_SPECIFY || permanent)
		startupPanelView[panelID] = enabled;
}

void ConfigFile::setStartupPanelMode(unsigned int panelM)
{
	ASSERT(panelM < CONFIG_PANELMODE_END_ENUM);
	panelMode=panelM;
}
		
unsigned int ConfigFile::getStartupPanelMode() const
{
	return panelMode;
}


bool ConfigFile::getAllowOnlineVersionCheck() const
{
	#if defined(WIN32) || defined(__APPLE__)
		//windows don't have good package
		//management systems as yet, so we check,
		//iff the user opts in
		return allowOnlineVerCheck;
	#else
		//Linux and friends should NEVER look online.
		//as they have package management systems to do this.
		return false;
	#endif
}

void ConfigFile::setAllowOnline(bool v)
{
	//Do not allow this setting to
	//be modified from the default for non-apple-non windows 
	//platforms
	#if defined( __APPLE__) || defined(WIN32)
		allowOnline=v;
	#endif
}
void ConfigFile::setAllowOnlineVersionCheck(bool v)
{
	//Do not allow this setting to
	//be modified from the default for non windows
	//platforms
	#if defined(WIN32) || defined(__APPLE__)
		allowOnlineVerCheck=v;
	#endif
}


void ConfigFile::setLeftRightSashPos(float fraction) 
{
	ASSERT(fraction <= 1.0f && fraction >=0.0f);
	leftRightSashPos=fraction;
}

void ConfigFile::setTopBottomSashPos(float fraction)
{
	ASSERT(fraction <= 1.0f && fraction >=0.0f);
	topBottomSashPos=fraction;
}


void ConfigFile::setFilterSashPos(float fraction)
{
	ASSERT(fraction <= 1.0f && fraction >=0.0f);
	filterSashPos=fraction;
}


void ConfigFile::setPlotListSashPos(float fraction)
{
	ASSERT(fraction <= 1.0f && fraction >=0.0f);
	plotListSashPos=fraction;
}


