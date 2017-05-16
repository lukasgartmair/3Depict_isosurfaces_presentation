/*
 *	filtertesting.cpp - unit testing implementation for filter code
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
#include <string>
#include <list>
#include <set>
#include <fstream>
#include <iostream>


using namespace std;

//!Run each filter through its own unit test function
bool filterTests();
//!Try cloning the filter from itself, and checking the filter
// clone is identical
bool filterCloneTests();

//!basic filter tree topology tests
bool filterTreeTests();

//Test for bug present in 7199bb83f0ac (internal),
// whereby Pos -> External -> Box would produce output, even if
// external did nothing
// Bug was due to incorrect handling of refresh input data stack
bool filterRefreshNoOut();

//!Test a given filter tree that the refresh works
bool testFilterTree(const FilterTree &f);

//!Test a given filter tree that the refresh is successful,
// then return the output. Must delete output with safeDeleteFilterList
bool testFilterTree(const FilterTree &f,
	std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > &outData ) ;

bool testFilterTree(const FilterTree &f)
{
	std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > outData;

	testFilterTree(f,outData);	
	
	f.safeDeleteFilterList(outData);

	return true;
}

bool testFilterTree(const FilterTree &f,
	std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > &outData )
{
	std::vector<SelectionDevice *> devices;
	std::vector<std::pair<const Filter *, string > > consoleMessages;

	ProgressData prog;
#ifdef  HAVE_CPP_1X
	ATOMIC_BOOL wantAbort(false);
#else
	ATOMIC_BOOL wantAbort=false;
#endif
	if(f.refreshFilterTree(outData,devices,consoleMessages,prog,wantAbort))
	{
		f.safeDeleteFilterList(outData);
		return false;
	}


	typedef std::pair<Filter *, std::vector<const FilterStreamData * > > FILTER_PAIR;

	for(list<FILTER_PAIR>::iterator it=outData.begin();
			it!=outData.end();++it)
	{
		cerr << it->first->getUserString() << ":" << endl;
		for(size_t ui=0;ui<it->second.size();ui++)
		{
			size_t streamType;
			streamType=ilog2(it->second[ui]->getStreamType());
			ASSERT(streamType<NUM_STREAM_TYPES);



			//Print out the stream name, and the number of objects it contains
			cerr << "\t" << STREAM_NAMES[streamType] << " " <<
				"\t" << it->second[ui]->getNumBasicObjects() << endl;
		}
	}


	//TODO: report on Xml contents
	
	
	f.safeDeleteFilterList(outData);

	return true;
}

bool filterTests()
{
	//Instantiate various filters, then run their unit tests
	for(unsigned int ui=0; ui<FILTER_TYPE_ENUM_END; ui++)
	{
		Filter *f;
	       	f= makeFilter(ui);
		if(!f->runUnitTests())
			return false;

		delete f;
	}

	if(!Filter::boolToggleTests())
		return false;

	if(!Filter::helpStringTests())
		return false;

	if(!filterRefreshNoOut())
		return false;

	if(!filterCloneTests())
		return false;
	
	if(!filterTreeTests())
		return false;

	return true;
}

bool filterCloneTests()
{
	//Run the clone/uncloned versions of filter write functions
	//against each other and ensure
	//that their XML output is the same. Then check against
	//the read function.
	//
	// Without a user config file (with altered defaults), this is not a
	// "strong" test, as nothing is being altered   inside the filter after 
	// instantiation in the default case -- stuff can still be missed 
	// in the cloneUncached, and won't be detected, but it does prevent cross-wiring. 
	//
	ConfigFile configFile;
    	configFile.read();
	
	bool fileWarn=false;
	for(unsigned int ui=0; ui<FILTER_TYPE_ENUM_END; ui++)
	{
		//Get the user's preferred, or the program
		//default filter.
		Filter *f = configFile.getDefaultFilter(ui);
		
		//now attempt to clone the filter, and write both XML outputs.
		Filter *g;
		g=f->cloneUncached();

		//create the files
		string sOrig,sClone;
		{

			wxString wxs,tmpStr;

			tmpStr=wxT("3Depict-unit-test-a");
			tmpStr=tmpStr+(f->getUserString());
			
			wxs= wxFileName::CreateTempFileName(tmpStr);
			sOrig=stlStr(wxs);
		
			//write out one file from original object	
			ofstream fileOut(sOrig.c_str());
			if(!fileOut)
			{
				//Run a warning, but only once.
				WARN(fileWarn,"unable to open output xml file for xml test");
				fileWarn=true;
			}

			f->writeState(fileOut,STATE_FORMAT_XML);
			fileOut.close();

			//write out file from cloned object
			tmpStr=wxT("3Depict-unit-test-b");
			tmpStr=tmpStr+(f->getUserString());
			wxs= wxFileName::CreateTempFileName(tmpStr);
			sClone=stlStr(wxs);
			fileOut.open(sClone.c_str());
			if(!fileOut)
			{
				//Run a warning, but only once.
				WARN(fileWarn,"unable to open output xml file for xml test");
				fileWarn=true;
			}

			g->writeState(fileOut,STATE_FORMAT_XML);
			fileOut.close();



		}

		//Now run diff
		//------------
		string command;
		command = string("diff \'") +  sOrig + "\' \'" + sClone + "\'";

		wxArrayString stdOut;
		long res;
		res=wxExecute((command),stdOut, wxEXEC_BLOCK);


		string comment = f->getUserString() + string(" Orig: ")+ sOrig + string (" Clone:") +sClone+
			string("Cloned filter output was different... (or diff not around?)");
		TEST(res==0,comment.c_str());
		//-----------
	
		//Check both files are valid XML
		TEST(isValidXML(sOrig.c_str()) ==true,"XML output of filter not valid...");	

		//Now, try to re-read the XML, and get back the filter,
		//then write it out again.
		//---
		{
			xmlDocPtr doc;
			xmlParserCtxtPtr context;
			context =xmlNewParserCtxt();
			if(!context)
			{
				WARN(false,"Failed allocating XML context");
				return false;
			}

			//Open the XML file
			doc = xmlCtxtReadFile(context, sClone.c_str(), NULL,XML_PARSE_NONET|XML_PARSE_NOENT);

			//release the context
			xmlFreeParserCtxt(context);

			//retrieve root node	
			xmlNodePtr nodePtr = xmlDocGetRootElement(doc);

			//Read the state file, then re-write it!
			g->readState(nodePtr);

			xmlFreeDoc(doc);

			ofstream fileOut(sClone.c_str());
			g->writeState(fileOut,STATE_FORMAT_XML);

			//Re-run diff
			res=wxExecute((command),stdOut, wxEXEC_BLOCK);
			
			comment = f->getUserString() + string("Orig: ")+ sOrig + string (" Clone:") +sClone+
				string("Read-back filter output was different... (or diff not around?)");
			TEST(res==0,comment.c_str());
		}
		//----
		//clean up
		wxRemoveFile((sOrig));
		wxRemoveFile((sClone));

		delete f;
		delete g;
	}
	return true;
}

bool filterTreeTests()
{
	FilterTree fTree;


	Filter *fA = new IonDownsampleFilter;
	Filter *fB = new IonDownsampleFilter;
	Filter *fC = new IonDownsampleFilter;
	Filter *fD = new IonDownsampleFilter;
	//Tree layout:
	//A
	//-> B
	//  -> D
	//-> C
	fTree.addFilter(fA,0);
	fTree.addFilter(fB,fA);
	fTree.addFilter(fC,fA);
	fTree.addFilter(fD,fB);
	TEST(fTree.size() == 4,"Tree construction");
	TEST(fTree.maxDepth() == 2, "Tree construction");

	
	//Copy B's child to B.
	//A
	//-> B
	//  -> D
	//  -> E
	//-> C
	size_t oldSize;
	oldSize=fTree.size();
	TEST(fTree.copyFilter(fD,fB),"copy test");
	TEST(oldSize+1 == fTree.size(), "copy test");
	TEST(fTree.maxDepth() == 2, " copy test");

	//Remove B from tree
	fTree.removeSubtree(fB);
	TEST(fTree.size() == 2, "subtree remove test");
	TEST(fTree.maxDepth() == 1, "subtree remove test");

	fTree.clear();

	Filter *f[4];
	f[0] = new IonDownsampleFilter;
	f[1] = new IonDownsampleFilter;
	f[2] = new IonDownsampleFilter;
	f[3] = new IonDownsampleFilter;

	for(unsigned int ui=0;ui<4;ui++)
	{
		std::string s;
		stream_cast(s,ui);
		f[ui]->setUserString(s);
	}

	//build a new tree arrangement
	//0
	// ->3
	//1
	// ->2
	fTree.addFilter(f[0],0);
	fTree.addFilter(f[1],0);
	fTree.addFilter(f[2],f[1]);
	fTree.addFilter(f[3],f[0]);

	fTree.reparentFilter(f[1],f[3]);

	//Now :
	// 0
	//   ->3
	//       ->1
	//          ->2
	TEST(fTree.size() == 4,"reparent test")
	TEST(fTree.maxDepth() == 3, "reparent test");
	for(unsigned int ui=0;ui<4;ui++)
	{
		TEST(fTree.contains(f[ui]),"reparent test");
	}

	FilterTree fSpareTree=fTree;
	fTree.addFilterTree(fSpareTree,f[2]);


	TEST(fTree.maxDepth() == 7,"reparent test");

	//Test the swap function
	FilterTree fTmp;
	fTmp=fTree;
	//swap spare with working
	fTree.swap(fSpareTree);
	//spare should now be the same size as the original working
	TEST(fSpareTree.maxDepth() == fTmp.maxDepth(),"filtertree swap");
	//swap working back
	fTree.swap(fSpareTree);
	TEST(fTree.maxDepth() == fTmp.maxDepth(),"filtertree swap");


	bool needUp;
	ASSERT(fTree.setFilterProperty(f[0],KEY_IONDOWNSAMPLE_FRACTION,"0.5",needUp)
	 || fTree.setFilterProperty(f[0],KEY_IONDOWNSAMPLE_COUNT,"10",needUp));

	std::string tmpName;
	if(!genRandomFilename(tmpName) )
	{
		//Build an XML file containing the filter tree
		// then try to load it
		try
		{
			//Create an XML file
			std::ofstream tmpFile(tmpName.c_str());

			if(!tmpFile)
				throw false;
			tmpFile << "<testXML>" << endl;
			//Dump tree contents into XML file
			std::map<string,string> dummyMap;
			if(fTree.saveXML(tmpFile,dummyMap,false,true))
			{
				tmpFile<< "</testXML>" << endl;
			}
			else
			{
			WARN(true,"Unable to write to random file in current folder. skipping test");
			}
		

			//Reparse tree
			//---
			xmlDocPtr doc;
			xmlParserCtxtPtr context;

			context =xmlNewParserCtxt();


			if(!context)
			{
				cout << "Failed to allocate parser" << std::endl;
				throw false;
			}

			//Open the XML file
			doc = xmlCtxtReadFile(context, tmpName.c_str(), NULL,XML_PARSE_NONET|XML_PARSE_NOENT);

			if(!doc)
				throw false;
			
			//release the context
			xmlFreeParserCtxt(context);
			//retrieve root node	
			xmlNodePtr nodePtr = xmlDocGetRootElement(doc);
			if(!nodePtr)
				throw false;
			
			nodePtr=nodePtr->xmlChildrenNode;
			if(!nodePtr)
				throw false;

			//find filtertree data
			if(XMLHelpFwdToElem(nodePtr,"filtertree"))
				throw false;
			
			TEST(!fTree.loadXML(nodePtr,cerr,""),"Tree load test");

		

			xmlFreeDoc(doc);	
			//-----
		}
		catch(bool)
		{
			WARN(false,"Couldn't run XML reparse of output file - write permission?" );
			wxRemoveFile((tmpName));
			return true;
		}
		wxRemoveFile((tmpName));
	}
	else
	{
		WARN(true,"Unable to open random file in current folder. skipping a test");
	}

	return true;
}


bool filterRefreshNoOut()
{


	//Create a file with some data in it
	string strData;

	wxString wxs,tmpStr;
	tmpStr=wxT("3Depict-unit-test-");
	wxs= wxFileName::CreateTempFileName(tmpStr);
	tmpStr = tmpStr + wxs;

	strData=stlStr(wxs) + string(".txt");

	//Create a text file with some dummy data
	{
	ofstream f(strData.c_str());

	if(!f)
	{
		WARN(false,"Unable to write to dir, skipped unit test");
		return true;
	}

	//Write out some usable data
	f << "1 2 3 4" << std::endl;
	f << "2 1 3 5" << std::endl;
	f << "3 2 1 6" << std::endl;
	f.close();
	}

	DataLoadFilter *fData = new DataLoadFilter;
	Filter *fB = new ExternalProgramFilter;
	Filter *fC = new IonDownsampleFilter;

	//Set the data load filter
	fData->setFilename(strData);
	fData->setFileMode(DATALOAD_TEXT_FILE);

	bool needUp;

	TEST(fB->setProperty(EXTERNALPROGRAM_KEY_COMMAND,"",needUp),"set prop");
	
	FilterTree fTree;
	fTree.addFilter(fData,0);
	fTree.addFilter(fB,fData);
	fTree.addFilter(fC,fB);


	std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > outData;
	TEST(testFilterTree(fTree,outData),"ext program tree test");

	TEST(outData.empty(),"External program refresh test");
	fTree.safeDeleteFilterList(outData);

	wxRemoveFile((strData));

	return true;
}
