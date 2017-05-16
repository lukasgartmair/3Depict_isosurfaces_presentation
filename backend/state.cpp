/*
 *	state.cpp - user session state handler
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

#include "state.h"

#include "common/translation.h"
#include "common/xmlHelper.h"
#include "common/stringFuncs.h"

const unsigned int MAX_UNDO_SIZE=10;

#include <unistd.h>
#include <stack>

using std::vector;
using std::string;
using std::pair;
using std::make_pair;
using std::map;
using std::endl; //TODO: Remove me?

//FIXME: Global - need to make part of AnalysisState.
//	 then provide references as needed
//true if modification to state has occurred
int stateModifyLevel=STATE_MODIFIED_NONE;

void setStateModifyLevel(int newLevel)
{
	stateModifyLevel=std::max(newLevel,stateModifyLevel);
}

int getStateModifyLevel()
{
	return stateModifyLevel;
}


AnalysisState::AnalysisState()
{
	useRelativePathsForSave=false;
	activeCamera=0;
	
	savedCameras.push_back(new CameraLookAt);	

	plotLegendEnable=true;

	rBack=gBack=bBack=0;
}


void AnalysisState::operator=(const AnalysisState &oth)
{
	clear();

	treeState=oth.treeState;	

	stashedTrees=oth.stashedTrees;

	effects.resize(oth.effects.size());
	for(size_t ui=0;ui<effects.size();ui++)
		effects[ui]= oth.effects[ui]->clone();

	savedCameras.resize(oth.savedCameras.size());
	for(size_t ui=0;ui<savedCameras.size();ui++)
		savedCameras[ui]= oth.savedCameras[ui]->clone();

	enabledStartupPlots=oth.enabledStartupPlots;
	plotLegendEnable=oth.plotLegendEnable;
	

	fileName=oth.fileName;
	workingDir=oth.workingDir;
	useRelativePathsForSave=oth.useRelativePathsForSave;

	rBack=oth.rBack;
	gBack=oth.gBack;
	bBack=oth.bBack;

	worldAxisMode=oth.worldAxisMode;
	activeCamera=oth.activeCamera;


	animationState=oth.animationState;
	animationPaths=oth.animationPaths;
	
}

AnalysisState::~AnalysisState()
{
	clear();
}


void AnalysisState::clear()
{
	treeState.clear();
	
	stashedTrees.clear();

	clearCams();

	clearEffects();


	enabledStartupPlots.clear();
	
	fileName.clear();
	workingDir.clear();
}

void AnalysisState::clearCams()
{
	for(size_t ui=0;ui<savedCameras.size();ui++)
		delete savedCameras[ui];
	savedCameras.clear();
}

void AnalysisState::clearEffects()
{
	for(size_t ui=0;ui<effects.size();ui++)
		delete effects[ui];

	effects.clear();

}
bool AnalysisState::save(const char *cpFilename, std::map<string,string> &fileMapping,
		bool writePackage, bool setStateModifyLevel) const
{
	//Open file for output
	std::ofstream f(cpFilename);

	if(!f)
		return false;
	
	//Write header, also use local language if available
	const char *headerMessage = NTRANS("This file is a \"state\" file for the 3Depict program, and stores information about a particular analysis session. This file should be a valid \"XML\" file");

	f << "<!--" <<  headerMessage;
	if(TRANS(headerMessage) != headerMessage) 
		f << endl << TRANS(headerMessage); 
	
	f << "-->" <<endl;



	//Write state open tag 
	f<< "<threeDepictstate>" << endl;
	f<<tabs(1)<< "<writer version=\"" << PROGRAM_VERSION << "\"/>" << endl;
	//write general settings
	//---------------
	f << tabs(1) << "<backcolour r=\"" << rBack << "\" g=\"" << 
		 gBack  << "\" b=\"" << bBack << "\"/>" <<  endl;
	
	f << tabs(1) << "<showaxis value=\"" << worldAxisMode << "\"/>"  << endl;
	
	//write plot status
	f << tabs(1) << "<plotstatus legend=\"" << boolStrEnc(plotLegendEnable) << "\">"  << endl;
	
	for(size_t ui=0;ui<enabledStartupPlots.size(); ui++)
	{
		
		f<< tabs(2) << "<enableplot filter=\"" << escapeXML(enabledStartupPlots[ui].first) << "\" id=\"" << enabledStartupPlots[ui].second << "\"/>" << endl;
	}
	f<<tabs(1) << "</plotstatus>" << endl;

	if(useRelativePathsForSave)
	{
		//Save path information
		//Note: When writing packages, 
		//- we don't want to leak path names to remote systems 
		//and
		//- we cannot assume that directory structures are preserved between systems
		//so don't keep working directory in this case.
		if(writePackage || workingDir.empty() )
		{
			//Are we saving the sate as a package, if so
			//make sure we let other 3depict loaders know
			//that we want to use relative paths
			f << tabs(1) << "<userelativepaths/>"<< endl;
		}
		else
		{
			//Not saving a package, however we could be, 
			//for example, be autosaving a load-from-package. 
			//We want to keep relative paths, but
			//want to be able to find files if something goes askew
			f << tabs(1) << "<userelativepaths origworkdir=\"" << workingDir << "\"/>"<< endl;
		}
	}

	//---------------


	//Write filter tree
	//---------------
	if(!treeState.getTreeRef().saveXML(f,fileMapping,writePackage,useRelativePathsForSave))
		return  false;
	//---------------

	//Save all cameras.
	f <<tabs(1) <<  "<cameras>" << endl;

	//First camera is the "working" camera, which is unnamed
	f << tabs(2) << "<active value=\"" << activeCamera << "\"/>" << endl;
	
	for(unsigned int ui=0;ui<savedCameras.size();ui++)
	{
		//ask each camera to write its own state, tab indent 2
		savedCameras[ui]->writeState(f,STATE_FORMAT_XML,2);
	}
	f <<tabs(1) <<  "</cameras>" << endl;
	
	if(stashedTrees.size())
	{
		f << tabs(1) << "<stashedfilters>" << endl;

		for(unsigned int ui=0;ui<stashedTrees.size();ui++)
		{
			f << tabs(2) << "<stash name=\"" << stashedTrees[ui].first
				<< "\">" << endl;
			stashedTrees[ui].second.saveXML(f,fileMapping,
					writePackage,useRelativePathsForSave,3);
			f << tabs(2) << "</stash>" << endl;
		}




		f << tabs(1) << "</stashedfilters>" << endl;
	}

	//Save any effects
	if(effects.size())
	{
		f <<tabs(1) <<  "<effects>" << endl;
		for(unsigned int ui=0;ui<effects.size();ui++)
			effects[ui]->writeState(f,STATE_FORMAT_XML,1);
		f <<tabs(1) <<  "</effects>" << endl;

	}

	//Save any animation data
	if(animationState.getMaxFrame())
	{
		//Write the flattened tree - "path" -  data
		f << tabs(1) << "<animationstate>" << endl;
		f << tabs(2) << "<animationtree>" << endl;
		for(unsigned int ui=0;ui<animationPaths.size();ui++)
		{
			f << tabs(3) << "<entry key=\"" << animationPaths[ui].second << "\" path=\"" 
					<< animationPaths[ui].first << "\"/>" << endl;
		}
		f << tabs(2) << "</animationtree>" << endl;

		animationState.writeState(f,STATE_FORMAT_XML,2);	

		f << tabs(1) << "</animationstate>" << endl;
	}


	//Close XMl tag.	
	f<< "</threeDepictstate>" << endl;

	//Debug check to ensure we have written a valid xml file
	ASSERT(isValidXML(cpFilename));

	if(setStateModifyLevel)
		stateModifyLevel=STATE_MODIFIED_NONE;

	return true;
}


bool AnalysisState::loadInternal(const char *cpFilename, bool doMerge, std::ostream &errStream)
{
	if(doMerge)
	{
		//create another state, and then perform merge
		AnalysisState otherState;
		bool loadOK;
		loadOK=	otherState.load(cpFilename,false,errStream);
	
		if(!loadOK)	
			return loadOK;
		this->merge(otherState);
		return true;
	}

	clear();

	//Load the state from an XML file
	
	//here we use libxml2's loading routines
	//http://xmlsoft.org/
	//Tutorial: http://xmlsoft.org/tutorial/xmltutorial.pdf
	xmlDocPtr doc;
	xmlParserCtxtPtr context;

	context =xmlNewParserCtxt();


	if(!context)
	{
		errStream << TRANS("Failed to allocate parser") << std::endl;
		return false;
	}

	//Open the XML file
	doc = xmlCtxtReadFile(context, cpFilename, NULL,XML_PARSE_NOENT|XML_PARSE_NONET);

	if(!doc)
		return false;
	
	//release the context
	xmlFreeParserCtxt(context);
	

	//By default, lets not use relative paths
	useRelativePathsForSave=false;

	//Lets do some parsing goodness
	//ahh parsing - verbose and boring
	FilterTree newFilterTree;
	vector<Camera *> newCameraVec;
	vector<Effect *> newEffectVec;
	vector<pair<string,FilterTree > > newStashes;
	

	std::string stateDir=onlyDir(cpFilename);
	try
	{
		std::stack<xmlNodePtr>  nodeStack;
		//retrieve root node	
		xmlNodePtr nodePtr = xmlDocGetRootElement(doc);

		//Umm where is our root node guys?
		if(!nodePtr)
		{
			errStream << TRANS("Unable to retrieve root node in input state file... Is this really a non-empty XML file?") <<  endl;
			throw 1;
		}
		
		//This *should* be an threeDepict state file
		if(xmlStrcmp(nodePtr->name, (const xmlChar *)"threeDepictstate"))
		{
			errStream << TRANS("Base state node missing. Is this really a state XML file??") << endl;
			throw 1;
		}
		//push root tag	
		nodeStack.push(nodePtr);
		
		//Now in threeDepictstate tag
		nodePtr = nodePtr->xmlChildrenNode;
		xmlChar *xmlString;
		//check for version tag & number
		if(!XMLHelpFwdToElem(nodePtr,"writer"))
		{
			xmlString=xmlGetProp(nodePtr, (const xmlChar *)"version"); 

			if(xmlString)
			{
				string tmpVer;
				
				tmpVer =(char *)xmlString;
				//Check to see if only contains 0-9 period and "-" characters (valid version number)
				if(tmpVer.find_first_not_of("0123456789.-")== std::string::npos)
				{
					//Check between the writer reported version, and the current program version
					vector<string> vecStrs;
					vecStrs.push_back(tmpVer);
					vecStrs.push_back(PROGRAM_VERSION);

					if(getMaxVerStr(vecStrs)!=PROGRAM_VERSION)
					{
						errStream << TRANS("State was created by a newer version of this program.. ")
							<< TRANS("file reading will continue, but may fail.") << endl ;
					}
				}
				else
				{
					errStream<< TRANS("Warning, unparseable version number in state file. File reading will continue, but may fail") << endl;
				}
				xmlFree(xmlString);
			}
		}
		else
		{
			errStream<< TRANS("Unable to find the \"writer\" node") << endl;
			throw 1;
		}
	

		//Get the background colour
		//====
		float rTmp,gTmp,bTmp;
		if(XMLHelpFwdToElem(nodePtr,"backcolour"))
		{
			errStream<< TRANS("Unable to find the \"backcolour\" node.") << endl;
			throw 1;
		}

		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"r");
		if(!xmlString)
		{
			errStream<< TRANS("\"backcolour\" node missing \"r\" value.") << endl;
			throw 1;
		}
		if(stream_cast(rTmp,(char *)xmlString))
		{
			errStream<< TRANS("Unable to interpret \"backColour\" node's \"r\" value.") << endl;
			throw 1;
		}

		xmlFree(xmlString);
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"g");
		if(!xmlString)
		{
			errStream<< TRANS("\"backcolour\" node missing \"g\" value.") << endl;
			throw 1;
		}

		if(stream_cast(gTmp,(char *)xmlString))
		{
			errStream<< TRANS("Unable to interpret \"backColour\" node's \"g\" value.") << endl;
			throw 1;
		}

		xmlFree(xmlString);
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"b");
		if(!xmlString)
		{
			errStream<< TRANS("\"backcolour\" node missing \"b\" value.") << endl;
			throw 1;
		}

		if(stream_cast(bTmp,(char *)xmlString))
		{
			errStream<< TRANS("Unable to interpret \"backColour\" node's \"b\" value.") << endl;
			throw 1;
		}

		if(rTmp > 1.0 || gTmp>1.0 || bTmp > 1.0 || 
			rTmp < 0.0 || gTmp < 0.0 || bTmp < 0.0)
		{
			errStream<< TRANS("\"backcolour\"s rgb values must be in range [0,1]") << endl;
			throw 1;
		}
		rBack=rTmp;
		gBack=gTmp;
		bBack=bTmp;
		
		xmlFree(xmlString);

		nodeStack.push(nodePtr);


		if(!XMLHelpFwdToElem(nodePtr,"userelativepaths"))
		{
			useRelativePathsForSave=true;

			//Try to load the original working directory, if possible
			if(!XMLGetAttrib(nodePtr,workingDir,"origworkdir"))
				workingDir.clear();
		}
		
		nodePtr=nodeStack.top();

		//====
		
		//Get the axis visibility
		if(!XMLGetNextElemAttrib(nodePtr,worldAxisMode,"showaxis","value"))
		{
			errStream << TRANS("Unable to find or interpret \"showaxis\" node") << endl;
			throw 1;
		}

		//Get plot legend status.
		// TODO: Deprecate failure check: this is new as of 0.0.16 release (internal ) 
		{
		xmlNodePtr tmpStatPtr=nodePtr;
		//Find list of which plots are enabled
		if(!XMLHelpFwdToElem(tmpStatPtr,"plotstatus"))
		{

			//Is the legend enabled?
			bool enableLegend;
			if(!XMLHelpGetProp(enableLegend,tmpStatPtr,"legend"))
				plotLegendEnable=enableLegend;

			//find plot listing
			xmlNodePtr enablePlotPtr=tmpStatPtr->xmlChildrenNode;

			while(enablePlotPtr)
			{
				if(XMLHelpFwdToElem(enablePlotPtr,"enableplot"))
					break;
				
				string filterPath;
				unsigned int plotID;

				//just abort loading this section if we can't find the filter
				if(XMLHelpGetProp(plotID,enablePlotPtr,"id"))
					break;
			
				if(XMLHelpGetProp(filterPath,enablePlotPtr,"filter"))
					break;

				enabledStartupPlots.push_back(make_pair(unescapeXML(filterPath),plotID));
			}

		}
		}

		//find filtertree data
		if(XMLHelpFwdToElem(nodePtr,"filtertree"))
		{
			errStream << TRANS("Unable to locate \"filtertree\" node.") << endl;
			throw 1;
		}

		//Load the filter tree
		if(newFilterTree.loadXML(nodePtr,errStream,stateDir))
			throw 1;

		//Read camera states, if present
		nodeStack.push(nodePtr);
		if(!XMLHelpFwdToElem(nodePtr,"cameras"))
		{
			//Move to camera active tag 
			nodePtr=nodePtr->xmlChildrenNode;
			if(XMLHelpFwdToElem(nodePtr,"active"))
			{
				errStream << TRANS("Cameras section missing \"active\" node.") << endl;
				throw 1;
			}

			//read ID of active cam
			xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
			if(!xmlString)
			{
				errStream<< TRANS("Unable to find property \"value\"  for \"cameras->active\" node.") << endl;
				throw 1;
			}

			if(stream_cast(activeCamera,xmlString))
			{
				errStream<< TRANS("Unable to interpret property \"value\"  for \"cameras->active\" node.") << endl;
				throw 1;
			}
			xmlFree(xmlString);

		
			//Spin through the list of each camera	
			while(!XMLHelpNextType(nodePtr,XML_ELEMENT_NODE))
			{
				std::string tmpStr;
				tmpStr =(const char *)nodePtr->name;
				Camera *thisCam;
				thisCam=0;
				
				//work out the camera type
				if(tmpStr == "persplookat")
				{
					thisCam = new CameraLookAt;
					if(!thisCam->readState(nodePtr->xmlChildrenNode))
					{
						std::string s =TRANS("Failed to interpret camera state for camera : "); 

						errStream<< s <<  newCameraVec.size() << endl;
						throw 1;
					}
				}
				else
				{
					errStream << TRANS("Unable to interpret the camera type for camera : ") << newCameraVec.size() <<  endl;
					throw 1;
				}

				ASSERT(thisCam);

				//Discard any cameras that are non-unique There is a bug in some of the writer functions,
				// so we can receive invalid data. We have to enforce validity
				bool haveCameraAlready;
				haveCameraAlready=false;
				for(unsigned int ui=0; ui<newCameraVec.size() ; ui++)
				{
					if(thisCam->getUserString() == newCameraVec[ui]->getUserString())
					{
						haveCameraAlready=true;
#ifdef DEBUG
						cerr << "Found duplicate camera, ignoring" << endl;
	#endif
						break;
					} 
				}
				if(!haveCameraAlready)
					newCameraVec.push_back(thisCam);
				else
				{
					delete thisCam;
					thisCam=0;
				}	
			}


		}

		//Now the cameras are loaded into a temporary vector. We will 
		// copy them into the scene soon

		nodePtr=nodeStack.top();
		nodeStack.pop();
		
		nodeStack.push(nodePtr);
		//Read stashes if present
		if(!XMLHelpFwdToElem(nodePtr,"stashedfilters"))
		{
			nodeStack.push(nodePtr);

			//Move to stashes 
			nodePtr=nodePtr->xmlChildrenNode;

			while(!XMLHelpFwdToElem(nodePtr,"stash"))
			{
				string stashName;
				FilterTree newStashTree;
				newStashTree.clear();

				//read name of stash
				xmlString=xmlGetProp(nodePtr,(const xmlChar *)"name");
				if(!xmlString)
				{
					errStream << TRANS("Unable to locate stash name for stash ") << newStashTree.size()+1 << endl;
					throw 1;
				}
				stashName=(char *)xmlString;

				if(!stashName.size())
				{
					errStream << TRANS("Empty stash name for stash ") << newStashTree.size()+1 << endl;
					throw 1;
				}

				xmlNodePtr tmpNode;
				tmpNode=nodePtr->xmlChildrenNode;
				
				if(XMLHelpFwdToElem(tmpNode,"filtertree"))
				{
					errStream << TRANS("No filter tree for stash:") << stashName << endl;
					throw 1;
				}

				if(newStashTree.loadXML(tmpNode,errStream,stateDir))
				{
					errStream << TRANS("For stash ") << newStashTree.size()+1 << endl;
					throw 1;
				}
				
				//if there were any valid elements loaded (could be empty, for exmapl)
				if(newStashTree.size())
					newStashes.push_back(make_pair(stashName,newStashTree));
			}

			nodePtr=nodeStack.top();
			nodeStack.pop();
		}
		nodePtr=nodeStack.top();
		nodeStack.pop();
	
		//Read effects, if present
		nodeStack.push(nodePtr);

		//Read effects if present
		if(!XMLHelpFwdToElem(nodePtr,"effects"))
		{
			std::string tmpStr;
			nodePtr=nodePtr->xmlChildrenNode;

			while(!XMLHelpNextType(nodePtr,XML_ELEMENT_NODE))
			{
				tmpStr =(const char *)nodePtr->name;

				Effect *e;
				e = makeEffect(tmpStr);
				if(!e)
				{
					errStream << TRANS("Unrecognised effect :") << tmpStr << std::endl;
					throw 1;
				}

				//Check the effects are unique
				for(unsigned int ui=0;ui<newEffectVec.size();ui++)
				{
					if(newEffectVec[ui]->getType()== e->getType())
					{
						delete e;
						errStream << TRANS("Duplicate effect found") << tmpStr << TRANS(" cannot use.") << std::endl;
						throw 1;
					}

				}

				nodeStack.push(nodePtr);
				//Parse the effect
				if(!e->readState(nodePtr))
				{
					errStream << TRANS("Error reading effect : ") << e->getName() << std::endl;
				
					throw 1;
				}
				nodePtr=nodeStack.top();
				nodeStack.pop();


				newEffectVec.push_back(e);				
			}
		}
		nodeStack.pop();
		nodePtr=nodeStack.top();


		if(!XMLHelpFwdToElem(nodePtr,"animationstate"))
		{
			nodePtr=nodePtr->xmlChildrenNode;
			if(!nodePtr)
				throw 1;

			if(XMLHelpFwdToElem(nodePtr,"animationtree"))
				throw 1;
			
			//Save this location
			nodeStack.push(nodePtr);

			nodePtr=nodePtr->xmlChildrenNode;
			if(!nodePtr)
				throw 1;

			vector<pair<string, size_t> > animationPathTmp;
			//Read the "flattened animation tree
			while(!XMLHelpFwdToElem(nodePtr,"entry"))
			{
				std::string path; 
				size_t val;
				if(XMLHelpGetProp(val,nodePtr,"key"))
					throw 1;
		
				//read the flattened tree "path" data
				//--
				xmlChar *xmlString;
				//grab the xml property
				xmlString = xmlGetProp(nodePtr,(const xmlChar *)"path");
				if(!xmlString)
					throw 1;
				path=(char *)xmlString;
				xmlFree(xmlString);
				//--


				animationPathTmp.push_back(make_pair(path,val));
			}

			nodePtr=nodeStack.top();
			nodeStack.pop();

			if(XMLHelpFwdToElem(nodePtr,"propertyanimator"))
				throw 1;
			
			nodePtr=nodePtr->xmlChildrenNode;
			if(!nodePtr)
				throw 1;
			
			//Try to load animation state
			animationState.loadState(nodePtr);
			animationPaths.swap(animationPathTmp);

		}
		
		nodePtr=nodeStack.top();
		nodeStack.pop();

		nodeStack.push(nodePtr);
	}
	catch (int)
	{
		//Code threw an error, just say "bad parse" and be done with it
		xmlFreeDoc(doc);
		return false;
	}
	xmlFreeDoc(doc);	


	//Check that stashes are uniquely named
	// do brute force search, as there are unlikely to be many stashes	
	for(unsigned int ui=0;ui<newStashes.size();ui++)
	{
		for(unsigned int uj=0;uj<newStashes.size();uj++)
		{
			if(ui == uj)
				continue;

			//If these match, states not uniquely named,
			//and thus statefile is invalid.
			if(newStashes[ui].first == newStashes[uj].first)
				return false;

		}
	}

	//Now replace it with the new data
	treeState.swapFilterTree(newFilterTree);
	std::swap(stashedTrees,newStashes);

	//Wipe the existing cameras, and then put the new cameras in place
	savedCameras.clear();
	
	//Set a default camera as needed. 
	Camera *c=new CameraLookAt();
	savedCameras.push_back(c);
	
	bool defaultSet = false;
	//spin through
	for(unsigned int ui=0;ui<newCameraVec.size();ui++)
	{
		//If there is no userstring, then its a  "default"
		// camera (one that does not show up to the users,
		// and cannot be erased from the scene)
		// set it directly. Otherwise, its a user camera.

		//if there are multiple without a string, only use the first
		if(newCameraVec[ui]->getUserString().size())
		{
			savedCameras.push_back(newCameraVec[ui]);
		}
		else if (!defaultSet)
		{
			ASSERT(savedCameras.size());
			delete savedCameras[0];
			savedCameras[0]=newCameraVec[ui];
			defaultSet=true;
		}

	}

	fileName=cpFilename;


	if(workingDir.empty())
	{
		char *wd;
#if defined(__APPLE__)
		//Apple defines a special getcwd that just works
		wd = getcwd(NULL, 0);
#elif defined(WIN32) || defined(WIN64)
		//getcwd under POSIX is not clearly defined, it requires
		// an input number of bytes that are enough to hold the path,
		// however it does not define how one goes about obtaining the
		// number of bytes needed. 
		char *wdtemp = (char*)malloc(PATH_MAX*20);
		wd=getcwd(wdtemp,PATH_MAX*20);
		if(!wd)
		{
			free(wdtemp);
			return false;	
		}

#else
		//GNU extension, which just does it (tm).
		wd = get_current_dir_name();
#endif
		workingDir=wd;
		free(wd);
	}

	// state is overwritten
	setStateModifyLevel(STATE_MODIFIED_NONE);

#ifdef DEBUG
	checkSane();
#endif
	//Perform sanitisation on results
	return true;
}

bool AnalysisState::load(const char *cpFilename, bool doMerge,std::ostream &errStream ) 
{
	AnalysisState otherState;
	if(!otherState.loadInternal(cpFilename,doMerge,errStream))
		return false;

	*this=otherState;

	return true;
}

void AnalysisState::merge(const AnalysisState &otherState)
{

	setStateModifyLevel(STATE_MODIFIED_DATA);

	//If we are merging, then there is a chance
	//of a name-clash. We avoid this by trying to append -merge continuously
	vector<std::pair<string,FilterTree> > newStashes;
	newStashes.resize(otherState.stashedTrees.size());
	for(size_t ui=0;ui<otherState.stashedTrees.size();ui++)
		newStashes[ui]=otherState.stashedTrees[ui];
	for(unsigned int ui=0;ui<newStashes.size();ui++)
	{
		//protect against overload (very unlikely)
		unsigned int maxCount;
		maxCount=100;
		while(hasFirstInPairVec(stashedTrees,newStashes[ui]) && --maxCount)
			newStashes[ui].first+=TRANS("-merge");

		if(maxCount)
			stashedTrees.push_back(newStashes[ui]);
		else
		{
			WARN(false," Unable to merge stashes correctly. This is improbable, so please report this.");
		}
	}
	
	FilterTree f = otherState.treeState.getTreeRef();

	//wipe treestate's undo/redo trees, as we can no longer rely on them
	treeState.clearUndoRedoStacks();	

	if(f.size())
		treeState.addFilterTree(f,true);
	

	const vector<Camera *> &newCameraVec = otherState.savedCameras;	
	for(unsigned int ui=0;ui<newCameraVec.size();ui++)
	{
		//Don't merge the default camera (which has no name)
		if(newCameraVec[ui]->getUserString().empty())
			continue;

		//Keep trying new names appending "-merge" each time to obtain a new, and hopefully unique name
		// Abort after many times
		unsigned int maxCount;
		maxCount=100;
		while(camNameExists(newCameraVec[ui]->getUserString()) && --maxCount)
		{
			newCameraVec[ui]->setUserString(newCameraVec[ui]->getUserString()+"-merge");
		}

		//If we have any attempts left, then it worked
		if(maxCount)
			savedCameras.push_back(newCameraVec[ui]->clone());
	}
}


bool AnalysisState::camNameExists(const std::string &s) const
{
	for(size_t ui=0; ui<savedCameras.size(); ui++) 
	{
		if (savedCameras[ui]->getUserString() == s ) 
			return true;
	}
	return false;
}

int AnalysisState::getWorldAxisMode() const 
{
	return worldAxisMode;
}

void AnalysisState::copyCams(vector<Camera *> &cams) const 
{
	ASSERT(!cams.size());

	cams.resize(savedCameras.size());
	for(size_t ui=0;ui<savedCameras.size();ui++)
		cams[ui] = savedCameras[ui]->clone();
}

void AnalysisState::copyCamsByRef(vector<const Camera *> &camRef) const
{
	camRef.resize(savedCameras.size());
	for(size_t ui=0;ui<camRef.size();ui++)
		camRef[ui]=savedCameras[ui];
}

const Camera *AnalysisState::getCam(size_t offset) const
{
	return savedCameras[offset];
}

void AnalysisState::removeCam(size_t offset)
{
	setStateModifyLevel(STATE_MODIFIED_ANCILLARY);

	ASSERT(offset < savedCameras.size());
	delete savedCameras[offset];
	savedCameras.erase(savedCameras.begin()+offset);
	if(activeCamera >=savedCameras.size())
		activeCamera=0;
}

void AnalysisState::addCamByClone(const Camera *c)
{
	setStateModifyLevel(STATE_MODIFIED_ANCILLARY);
	savedCameras.push_back(c->clone());
}

void AnalysisState::addCam(const std::string &camName, bool makeActive)
{
	//Disallow unnamed cameras
	ASSERT(camName.size());
	//Duplicate the current camera, and give it a new name
	Camera *c=getCam(getActiveCam())->clone();
	c->setUserString(camName);
	setStateModifyLevel(STATE_MODIFIED_ANCILLARY);
	savedCameras.push_back(c);

	if(makeActive)
		activeCamera=savedCameras.size()-1;
}

bool AnalysisState::setCamProperty(size_t offset, unsigned int key, const std::string &str)
{
	if(offset == activeCamera)
		setStateModifyLevel(STATE_MODIFIED_VIEW);
	else
		setStateModifyLevel(STATE_MODIFIED_ANCILLARY);
	return savedCameras[offset]->setProperty(key,str);
}

std::string AnalysisState::getCamName(size_t offset) const
{
	return  savedCameras[offset]->getUserString();
}

bool AnalysisState::getUseRelPaths() const 
{
	return useRelativePathsForSave;
}
		
void AnalysisState::getBackgroundColour(float &r, float &g, float &b) const
{
	r=rBack;
	g=gBack;
	b=bBack;
}

void AnalysisState::getAnimationState( PropertyAnimator &p, vector<pair<string,size_t> > &animPth) const
{
	p=animationState;
	animPth=animationPaths;
}


void AnalysisState::copyEffects(vector<Effect *> &e) const
{
	e.clear();
	for(size_t ui=0;ui<effects.size();ui++)
		e[ui]=effects[ui]->clone();
}


void AnalysisState::setBackgroundColour(float r, float g, float b)
{
	if(rBack != r || gBack!=g || bBack!=b)
		setStateModifyLevel(STATE_MODIFIED_VIEW);
	rBack=r;
	gBack=g;
	bBack=b;

}

void AnalysisState::setWorldAxisMode(unsigned int mode)
{
	if(mode)
		setStateModifyLevel(STATE_MODIFIED_VIEW);
	worldAxisMode=mode;
}

void AnalysisState::setCamerasByCopy(vector<Camera *> &c, unsigned int active)
{
	setStateModifyLevel(STATE_MODIFIED_DATA);
	clearCams();

	savedCameras.swap(c);
	activeCamera=active;
}

void AnalysisState::setCameraByClone(const Camera *c, unsigned int offset)
{
	ASSERT(offset < savedCameras.size()); 
	delete savedCameras[offset];
	savedCameras[offset]=c->clone(); 

	if(offset == activeCamera)
		setStateModifyLevel(STATE_MODIFIED_VIEW);
	else
		setStateModifyLevel(STATE_MODIFIED_ANCILLARY);
}

void AnalysisState::setEffectsByCopy(const vector<const Effect *> &e)
{
	setStateModifyLevel(STATE_MODIFIED_VIEW);
	clearEffects();

	effects.resize(e.size());
	for(size_t ui=0;ui<e.size();ui++)
		effects[ui] = e[ui]->clone();

}


void AnalysisState::setUseRelPaths(bool useRel)
{
	useRelativePathsForSave=useRel;
}
void AnalysisState::setWorkingDir(const std::string &work)
{
	if(work!=workingDir)
		setStateModifyLevel(STATE_MODIFIED_DATA);

	workingDir=work;
}

void AnalysisState::setStashedTreesByClone(const vector<std::pair<string,FilterTree> > &s)
{
	setStateModifyLevel(STATE_MODIFIED_ANCILLARY);
	stashedTrees=s;
}

void AnalysisState::addStashedTree(const std::pair<string,FilterTree> &s)
{
	setStateModifyLevel(STATE_MODIFIED_ANCILLARY);
	stashedTrees.push_back(s);
}

void AnalysisState::addStashedToFilters(const Filter *parentFilter, unsigned int stashOffset)
{
	//Save current filter state to undo stack
	treeState.pushUndoStack();

	//Retrieve the specified stash
	pair<string,FilterTree> f;
	copyStashedTree(stashOffset,f);

	treeState.addFilterTree(f.second,parentFilter);
}

void AnalysisState::copyStashedTrees(std::vector<std::pair<string,FilterTree > > &s) const
{
	s=stashedTrees;
}

void AnalysisState::copyStashedTree(size_t offset,std::pair<string,FilterTree> &s) const
{
	s=stashedTrees[offset];
}

void AnalysisState::copyStashedTree(size_t offset,FilterTree &s) const
{
	s=stashedTrees[offset].second;
}

void AnalysisState::stashFilters(unsigned int filterId, const char *stashName)
{
	//Obtain the parent filter that we 
	const Filter *target = treeState.getFilterById(filterId);
	
	FilterTree newTree;
	const FilterTree &curTree = treeState.getTreeRef();
	curTree.cloneSubtree(newTree,target);

	addStashedTree(std::make_pair(string(stashName),newTree));
}

//Get the stash name
std::string AnalysisState::getStashName(size_t offset) const
{
	ASSERT(offset < stashedTrees.size());
	return  stashedTrees[offset].first;
}

#ifdef DEBUG
void AnalysisState::checkSane() const
{
	ASSERT(activeCamera < savedCameras.size());

	ASSERT(rBack >=0.0f && rBack <=1.0f 
		&& gBack <= 1.0f && gBack >=0.0f &&
		bBack >=0.0f && bBack <=1.0f);

	
}
#endif

void AnalysisState::eraseStash(size_t offset)
{
	ASSERT(offset < stashedTrees.size());
	setStateModifyLevel(STATE_MODIFIED_ANCILLARY);
	stashedTrees.erase(stashedTrees.begin() + offset);
}

void AnalysisState::eraseStashes(std::vector<size_t> &offsets)
{
	std::sort(offsets.begin(),offsets.end());
	ASSERT(std::unique(offsets.begin(),offsets.end()) == offsets.end());



	setStateModifyLevel(STATE_MODIFIED_ANCILLARY);
	for(unsigned int ui=offsets.size();ui>0;)
	{
		ui--;
		
		stashedTrees.erase(stashedTrees.begin() + offsets[ui]);
	}
}
		
bool AnalysisState::hasStateOverrides() const
{
	if(treeState.hasStateOverrides())
		return true;

	for(size_t ui=0;ui<stashedTrees.size();ui++)
	{
		if(stashedTrees[ui].second.hasStateOverrides())
			return true;
	}

	return false;
}

void TreeState::operator=(const TreeState &oth) 
{
#ifdef DEBUG
	//Should not be refreshing
	wxMutexLocker lock(amRefreshing);
	ASSERT(lock.IsOk());
#endif
	filterTree=oth.filterTree;	
	
	fta=oth.fta;
	filterMap=oth.filterMap;	
	redoFilterStack=oth.redoFilterStack;
	undoFilterStack=oth.undoFilterStack;
	
	selectionDevices=oth.selectionDevices;
	pendingUpdates=oth.pendingUpdates;
	
}

void TreeState::addFilter(Filter *f, bool isBase,size_t parentId)
{ 
	pushUndoStack();
	if(!isBase)
		filterTree.addFilter(f,filterMap[parentId]);
	else
		filterTree.addFilter(f,0);


	vector<bool> idInUse(filterMap.size()+1,false);
	for(map<size_t,Filter*>::const_iterator it=filterMap.begin();it!=filterMap.end();++it)
	{
		idInUse[it->first] = true; 
	}

	//Find which ID we can use for inserting stuff into the filtermap
	size_t idToUse;
	for(size_t ui=0;ui<filterMap.size();ui++)
	{
		if(!idInUse[ui])
		{
			idToUse=ui;
			continue;
		}
	}	
	

	//Add new entry to filer map
	filterMap[idToUse] = f;
}

void TreeState::addFilterTree(FilterTree &f, bool isBase,size_t parentId)
{ 
	ASSERT(!(isBase && parentId==(size_t)-1));

	if(isBase)
		filterTree.addFilterTree(f,0);
	else
	{
		ASSERT(filterMap.size());
		filterTree.addFilterTree(f,filterMap[parentId]);
	}

	//FIXME: This technically does not need to be cleared. We can
	// tweak the filter map to remain valid. It is the caller's problem
	// to rebuild as needed

	//The filter map is now invalid, as we added an element to the tree,
	//and don't have a unique value for it. we need to relayout.
	filterMap.clear();
}

void TreeState::switchoutFilterTree(FilterTree &f)
{
	//Create a clone of the internal tree
	f=filterTree;

	//Fix up the internal filterMap to reflect the contents of the new tree
	//---
	//
	//Build a map from old filter*->new filter *
	tree<Filter*>::pre_order_iterator itB;
	itB=filterTree.depthBegin();
	std::map<Filter*,Filter*> filterRemap;
	for(tree<Filter*>::pre_order_iterator itA=f.depthBegin(); itA!=f.depthEnd(); ++itA)
	{
		ASSERT(itB != filterTree.depthEnd());
		filterRemap[*itA]=*itB;	
		++itB;
	}

	//Overwrite the internal map
	for(map<size_t,Filter*>::iterator it=filterMap.begin();it!=filterMap.end();++it)
		it->second=filterRemap[it->second];


	//Swap the internal tree with our clone
	f.swap(filterTree);
	
}

//!Duplicate a branch of the tree to a new position. Do not copy cache,
bool TreeState::copyFilter(size_t toCopy, size_t newParent,bool copyToRoot) 
{
	pushUndoStack();
	
	bool ret;
	if(copyToRoot)
		ret=filterTree.copyFilter(filterMap[toCopy],0);
	else
	
		ret=filterTree.copyFilter(filterMap[toCopy],filterMap[newParent]);

	if(ret)
	{
		//Delete the filtermap, as the current data is not valid anymore
		filterMap.clear();
	}

	return ret;
}


const Filter* TreeState::getFilterById(size_t filterId) const 
{
	//If triggering this assertion, check that
	//::updateWxTreeCtrl called after calling addFilterTree.
	ASSERT(filterMap.size());

	//Check that the mapping exists
	ASSERT(filterMap.find(filterId)!=filterMap.end());
	return filterMap.at(filterId);
}


size_t TreeState::getIdByFilter(const Filter* f) const
{
	for(map<size_t,Filter*>::const_iterator it=filterMap.begin(); it!=filterMap.end();++it)
	{
		if(it->second == f)
			return it->first;
	}	

	ASSERT(false);
	return (size_t)-1;
}
void TreeState::getFiltersByType(std::vector<const Filter *> &filters, unsigned int type)  const
{
	filterTree.getFiltersByType(filters,type);
}


void TreeState::setCachePercent(unsigned int newPct)
{
	filterTree.setCachePercent(newPct);
}


void TreeState::removeFilterSubtree(size_t filterId)
{
	//Save current filter state to undo stack
	pushUndoStack();
       	filterTree.removeSubtree(filterMap[filterId]);

	//FIXME: Faster implementation involving removal from map
	//--
	map<size_t,Filter*> newMap;
	for(map<size_t,Filter*>::iterator it=filterMap.begin(); it!=filterMap.end();++it)
	{
		if(filterTree.contains(it->second))
			newMap[it->first] = it->second;
	}
	newMap.swap(filterMap);
	//--

}

bool TreeState::reparentFilter(size_t filter, size_t newParent)
{
	//Save current filter state to undo stack
	pushUndoStack();

	//Try to reparent this filter. It might not work, if, for example
	// the new parent is actually a child of the filter we are trying to
	// assign the parent to. 
	if(!filterTree.reparentFilter(filterMap[filter],filterMap[newParent]))
	{
		//Didn't work. Pop the undo stack, to reverse our 
		//push, but don't restore it,
		// as this would cost us our filter caches
		popUndoStack(false);
		return false;
	}
	
	return true;
}

bool TreeState::setFilterProperty(size_t filterId, 
				unsigned int key, const std::string &value, bool &needUpdate)
{
	//Save current filter state to undo stack
	//for the case where the property change is good
	pushUndoStack();
	bool setOK;
	setOK=filterTree.setFilterProperty(filterMap[filterId],key,value,needUpdate);

	if(!setOK)
	{
		//Didn't work, so we need to discard the undo
		//Pop the undo stack, but don't restore it -
		// restoring would destroy the cache
		popUndoStack(false);
	}

	return setOK;
}

void TreeState::setFilterString(size_t filterId, const std::string &s)
{
	Filter *f;
	f = filterMap[filterId];
	
	f->setUserString(s);
}

unsigned int TreeState::refresh(std::list<FILTER_OUTPUT_DATA> &refreshData,
				std::vector<std::pair<const Filter*, std::string> > &consoleMessages, ProgressData &curProg)
{
	//Attempt to acquire a lock. Return -1 if locking fails
	wxMutexLocker lock(amRefreshing);
	if(!lock.IsOk())
	{
		ASSERT(false); // should not get here. Caller should not
				// try to double-refresh
		return -1;
	}
	ASSERT(refreshData.empty())

	//Analyse the filter tree structure for any errors
	//--	
	fta.analyse(filterTree);
	//--

	//Reset the progress back to zero	
	curProg.reset();
	//clear old devices
	selectionDevices.clear();
	//Remove any updates
	pendingUpdates=false;
	wantAbort=false;

	//Run the tree refresh system.
	unsigned int errCode;
	errCode=filterTree.refreshFilterTree(refreshData,selectionDevices,
			consoleMessages,curProg,wantAbort);

	//return error code, if any
	return errCode;
}

void TreeState::pushUndoStack()
{
	if(undoFilterStack.size() > MAX_UNDO_SIZE)
		undoFilterStack.pop_front();

	undoFilterStack.push_back(filterTree);
	redoFilterStack.clear();
}

void TreeState::popUndoStack(bool restorePopped)
{
	ASSERT(undoFilterStack.size());

	//Save the current filters to the redo stack.
	// note that the copy constructor will generate a clone for us.
	redoFilterStack.push_back(filterTree);

	if(redoFilterStack.size() > MAX_UNDO_SIZE)
		redoFilterStack.pop_front();

	if(restorePopped)
	{
		//Swap the current filter cache out with the undo stack result
		filterTree.swap(undoFilterStack.back());
		
	}

	//Pop the undo stack
	undoFilterStack.pop_back();

	setStateModifyLevel(STATE_MODIFIED_DATA);
}

void TreeState::popRedoStack()
{
	ASSERT(undoFilterStack.size() <=MAX_UNDO_SIZE);
	undoFilterStack.push_back(filterTree);

	//Swap the current filter cache out with the redo stack result
	filterTree.swap(redoFilterStack.back());
	
	//Pop the redo stack
	redoFilterStack.pop_back();

	setStateModifyLevel(STATE_MODIFIED_DATA);
}

void TreeState::applyBindings(const std::vector<std::pair<const Filter *,SelectionBinding> > &bindings)
{
	if(!bindings.size())
		return;
	pushUndoStack();

	for(unsigned int ui=0;ui<bindings.size();ui++)
	{
#ifdef DEBUG
		bool haveBind;
		haveBind=false;
#endif
		for(tree<Filter *>::iterator it=filterTree.depthBegin(); 
				it!=filterTree.depthEnd();++it)
		{
			if(*it  == bindings[ui].first)
			{
				//We are modifying the contents of
				//the filter, this could make a change that
				//modifies output so we need to clear 
				//all subtree caches to force reprocessing

				filterTree.clearCache(*it,false);

				(*it)->setPropFromBinding(bindings[ui].second);
#ifdef DEBUG
				haveBind=true;
#endif
				break;
			}
		}

		ASSERT(haveBind);

	}

}

void TreeState::applyBindingsToTree()
{
	//Clear any updates
	pendingUpdates=false;
		
	//Retrieve all the modified bindings
	vector<pair<const Filter *,SelectionBinding> > bindings;
	for(unsigned int ui=0;ui<selectionDevices.size();ui++)
		selectionDevices[ui]->getModifiedBindings(bindings);

	applyBindings(bindings);	

	//Clear the modifications to the selection devices
	for(unsigned int ui=0;ui<selectionDevices.size();ui++)
		selectionDevices[ui]->resetModifiedBindings();


}

bool TreeState::hasUpdates() const
{
	return pendingUpdates;
}
bool TreeState::hasMonitorUpdates() const
{
	for(tree<Filter *>::iterator it=filterTree.depthBegin(); 
			it!=filterTree.depthEnd();++it)
	{
		if((*it)->monitorNeedsRefresh())
			return true;
	}

	return false;
}



#ifdef DEBUG

#include "./filters/ionDownsample.h"
bool testStateReload();

bool runStateTests()
{
	return testStateReload();
}

bool testStateReload()
{

	AnalysisState someState;
	someState.setWorldAxisMode(0);
	someState.setBackgroundColour(0,0,0);

	FilterTree tree;
	IonDownsampleFilter *f = new IonDownsampleFilter;
	tree.addFilter(f,NULL);
	ASSERT(tree.size());

	someState.addStashedTree(make_pair("someStash",tree));
	ASSERT(tree.size());
	someState.treeState.swapFilterTree(tree);

	std::string saveString;
	genRandomFilename(saveString);

	map<string,string> dummyMapping;
	if(!someState.save(saveString.c_str(),dummyMapping,false))
	{
		WARN(false, "Unable to save file.. write permissions? Skipping test");
		return true;
	}
	someState.clear();

	std::ofstream strm;
	TEST(someState.load(saveString.c_str(),false,strm),"State load");

	TEST(someState.getStashCount() == 1,"Stash save+load");
	std::pair<string,FilterTree> stashOut;
	someState.copyStashedTree(0,stashOut);
	TEST(stashOut.first == "someStash","Stash name conservation");

	rmFile(saveString);

	return true;
}

#endif
