/*
 *	wxcommon.cpp - Comon wxwidgets functionality 
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
#include "wxcommon.h"

#include "common/stringFuncs.h"
#include "common/constants.h"
#include <stack>

#include <wx/wx.h>
#include <wx/xml/xml.h>
#include <wx/filename.h>

#if defined(WIN32) || defined(WIN64)
	#include <wx/msw/registry.h>  
#endif

#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif

using std::stack;
using std::pair;
using std::map;
using std::make_pair;
using std::string;

//Auto update checking RSS URL
const char *RSS_FEED_LOCATION="http://threedepict.sourceforge.net/rss.xml";

//Auto update event for posting back to main thread upon completion
wxEventType RemoteUpdateAvailEvent = wxNewEventType(); // You get to choose the name yourself

//Signalling event for main frame that it is completed
wxEventType RefreshCompleteEvent= wxNewEventType(); // You get to choose the name yourself
		
//Maximum amount of content in RSS header is 1MB.
const unsigned int MAX_RSS_CONTENT_SIZE=1024*1024;

//Unlikely text string that can be appended to treepersist 
const char *PATH_NONCE="%$-";

std::string inputString;

std::string locateDataFile(const char *name)
{
	//Current strategies:
	//Linux:
	//	- Look in cwd & some common hard-coded install locations.
	//Mac:
	// 	- look in bundle
	//Windows
	//	- Locate a registry key that has the install path, which is preset by
	//	  application installer

#if defined(WIN32) || defined(WIN64)

	//This must match the key used in the installer
	wxRegKey *pRegKey = new wxRegKey(wxT("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\3Depict.exe"));

	if( pRegKey->Exists() )
	{
		//Now we are talkin. Regkey exists
		//OK, lets see if this dir actually exists or if we are being lied to (old dead regkey, for example)
		wxString keyVal;
		//Get the default key
		pRegKey->QueryValue(wxT(""),keyVal);
		//Strip the "3Depict.exe" from the key string
		std::string s;
		s = (const char *)keyVal.mb_str();
		
		if(s.size() > 11)
		{
			s=s.substr(0,s.size()-11);			

			s+=name;

			if(wxFileExists((s)))
				return s;
		}
	}
	
	std::string s;
	s =name; 
	
	if(s.size() && wxFileExists((s)))
	{
		return string(name);
	}
	else
		return std::string("");
#elif defined( __linux__)

	//Possible search paths. Must have trailing slash. will
	//be searched in sequence.
	const char *possibleDirs[] = { "./",
					"/usr/local/share/3Depict/",
					"/usr/share/3Depict/",
					"/usr/share/3depict/", //Under debian, we have to use lowercase according to the debian guidelines, so handle this case.
					"../data/",
					"./data/"
					};

	const unsigned int NUM_SEARCH_DIRS=THREEDEP_ARRAYSIZE(possibleDirs);

	std::string s;
	for(unsigned int ui=0; ui<NUM_SEARCH_DIRS; ui++)
	{
		s=std::string(possibleDirs[ui]) + name;

		if(wxFileExists((s)))
			return s;
	}

	//Return empty string, as we can't find it
	return std::string("");
#elif defined (__APPLE__)
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    char path[PATH_MAX];
    if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
    {
        // error!
	return std::string("");
    }
    CFRelease(resourcesURL);
	std::string s=std::string(path) + "/" + name;
		if(wxFileExists((s)))
			return s;
		else
			return std::string("");
#else

	//	- Look in cwd
	if(wxFileExists(wxCStr(name)))
		return  std::string(name);
	else
		return std::string("");
#endif
}

VersionCheckThread::VersionCheckThread(wxWindow *target) : wxThread(wxTHREAD_JOINABLE)
{
	complete=false;
	retrieveOK=false;
	targetWindow=target; 
	url.GetProtocol().Initialize();
}


void *VersionCheckThread::Entry()
{
  	wxCommandEvent event( RemoteUpdateAvailEvent);
	ASSERT(targetWindow);

	wxInputStream* inputStream;
	versionStr.clear();

	//Try to download RSS feed
	std::string strUrl;
	wxString rssUrl;

	//Build the rss query string, encoding 3depict version and OS description 
	strUrl = std::string(RSS_FEED_LOCATION) + std::string("?progver=") + std::string(PROGRAM_VERSION) + 
				std::string("&os=") + stlStr(::wxGetOsDescription());

	wxURI uri((strUrl));
	rssUrl = uri.BuildURI();

	url.SetURL(rssUrl); 

	//If the URL could not be downloaded, tough.
	if (url.GetError() != wxURL_NOERR)
	{
		retrieveOK=false;
		complete=true;
		wxPostEvent(targetWindow,event);
		return 0;
	}	

	inputStream = url.GetInputStream();

	if(!inputStream || !inputStream->CanRead())
	{
		retrieveOK=false;
		complete=true;
		wxPostEvent(targetWindow,event);
		return 0;
	}
	
	wxXmlDocument *doc= new wxXmlDocument;
	if(!doc->Load(*inputStream))
	{
		delete doc;
		retrieveOK=false;
		complete=true;
		wxPostEvent(targetWindow,event);
		return 0;
	}

	//FIXME : leaking inptustream?	

	//Check we grabbed an RSS feed
	if(doc->GetRoot()->GetName() != wxT("rss"))
	{
		delete doc;
		retrieveOK=false;
		complete=true;
		wxPostEvent(targetWindow,event);
		return 0;
	}

	//Find first channel
	wxXmlNode *child = doc->GetRoot()->GetChildren();

	while(child)
	{
		if(child->GetName() == wxT("channel"))
			break;
	    child = child->GetNext();
	}

	if(!child)
	{
		delete doc;
		retrieveOK=false;
		complete=true;
		wxPostEvent(targetWindow,event);
		return 0;
	}
	
	std::vector<std::string> itemStrs;

	//Spin through all the <item> nodes in the first <channel></channel>
	wxXmlNode *itemNode=child->GetChildren();
	while(itemNode)
	{
		//OK, we have an item node,lets check its children
		if(itemNode->GetName() == wxT("item"))
		{
			child=itemNode->GetChildren();

			while(child)
			{
				//OK, we found a child node; 
				if(child->GetName() == wxT("title"))
				{
					std::string stlContent;
					wxString content = child->GetNodeContent();

					stlContent=stlStr(content);
					if(stlContent.size() < MAX_RSS_CONTENT_SIZE &&
						isVersionNumberString(stlContent))
						itemStrs.push_back(stlContent);
					break;
				}
	    
				child = child->GetNext();
			}

		}
	    
		itemNode = itemNode->GetNext();
	}
	delete doc;

	if(itemStrs.empty())
	{
		//hmm. thats odd. no items. guess we failed :(
		retrieveOK=false;
		complete=true;
		wxPostEvent(targetWindow,event);
		return 0;
	}

	//Find the greatest version number
	versionStr=getMaxVerStr(itemStrs);
	retrieveOK=true;
	complete=true;
	wxPostEvent(targetWindow,event);

	return 0;
}

void wxErrMsg(wxWindow *win, const std::string &title, const std::string &mesg)
{
	wxMessageDialog *wxMesD  =new wxMessageDialog(win,(mesg)
					,(title),wxOK|wxICON_ERROR);
	wxMesD->ShowModal();
	wxMesD->Destroy();
}


//Does a process with a given ID both (1) exist, and (2) match the process name?

#if defined(__LINUX__) || defined(__BSD__)
bool processMatchesName(size_t processID, const std::string &procName)
{
	//Execute the ps process, then filter the output by processID
	
	wxArrayString stdOut;
	long res;
	res=wxExecute(wxT("ps ax"),stdOut, wxEXEC_BLOCK);

	if(res !=0 )
		return false;

	std::string pidStr;
	stream_cast(pidStr,processID);
	//Parse stdout..
	for(size_t ui=0;ui<stdOut.GetCount();ui++)
	{
		std::string s;
		s=stlStr(stdOut[ui]);

		//FIXME: This is a little lax. finding the proc name should
		//check the position of the found string more heavily
		// however, we run the risk of differing ps
		//implementations, causing false negatives.
		// - probably should use the kernel?
		std::vector<std::string> strVec;
		s=stripWhite(s);
		splitStrsRef(s.c_str()," \t",strVec);



		//Return true if *any* field returns true
		bool pidFound,procNameFound;
		procNameFound=pidFound=false;
		for(unsigned int ui=0;ui<strVec.size(); ui++)
		{
			wxFileName fName((strVec[ui]));
			std::string maybeProcName;
			maybeProcName = stlStr(fName.GetFullName());
			
			
			if( pidStr == strVec[ui])
				pidFound=true;
			else if(procName == maybeProcName)
				procNameFound=true;

			if(procNameFound && pidFound)
				return true;
		}

	}
	return false;
}
#else
	#include <windows.h>
	typedef long NTSTATUS; 

	#define STATUS_SUCCESS               ((NTSTATUS)0x00000000L)
	#define STATUS_INFO_LENGTH_MISMATCH  ((NTSTATUS)0xC0000004L)

	typedef enum _SYSTEM_INFORMATION_CLASS {
		SystemProcessInformation = 5
	} SYSTEM_INFORMATION_CLASS;

	typedef struct _UNICODE_STRING {
		USHORT Length;
		USHORT MaximumLength;
		PWSTR  Buffer;
	} UNICODE_STRING;

	typedef LONG KPRIORITY; // Thread priority

	typedef struct _SYSTEM_PROCESS_INFORMATION_DETAILD {
		ULONG NextEntryOffset;
		ULONG NumberOfThreads;
		LARGE_INTEGER SpareLi1;
		LARGE_INTEGER SpareLi2;
		LARGE_INTEGER SpareLi3;
		LARGE_INTEGER CreateTime;
		LARGE_INTEGER UserTime;
		LARGE_INTEGER KernelTime;
		UNICODE_STRING ImageName;
		KPRIORITY BasePriority;
		HANDLE UniqueProcessId;
		ULONG InheritedFromUniqueProcessId;
		ULONG HandleCount;
		BYTE Reserved4[4];
		PVOID Reserved5[11];
		SIZE_T PeakPagefileUsage;
		SIZE_T PrivatePageCount;
		LARGE_INTEGER Reserved6[6];
	} SYSTEM_PROCESS_INFORMATION_DETAILD, *PSYSTEM_PROCESS_INFORMATION_DETAILD;

	//Function ptr
	typedef  NTSTATUS (WINAPI *PFN_NT_QUERY_SYSTEM_INFORMATION)(
									  IN       SYSTEM_INFORMATION_CLASS SystemInformationClass,
									  IN OUT   PVOID SystemInformation,
									  IN       ULONG SystemInformationLength,
									  OUT OPTIONAL  PULONG ReturnLength
									);

	bool processMatchesName(size_t processID, const std::string &procName)
	{
		//Construct the memory structures, and load the DLLs needed to grab the win32 api constructs required
		size_t bufferSize = 102400;
		PSYSTEM_PROCESS_INFORMATION_DETAILD pspid=
			(PSYSTEM_PROCESS_INFORMATION_DETAILD) malloc (bufferSize);
		ULONG ReturnLength;
		PFN_NT_QUERY_SYSTEM_INFORMATION pfnNtQuerySystemInformation = (PFN_NT_QUERY_SYSTEM_INFORMATION)
			GetProcAddress (GetModuleHandle(TEXT("ntdll.dll")), "NtQuerySystemInformation");

				
		//Grab the process ID stuff, expanding the buffer until we can do the job we need.
		while (true) {
			NTSTATUS status;
			status = pfnNtQuerySystemInformation (SystemProcessInformation, (PVOID)pspid,
												  bufferSize, &ReturnLength);
			if (status == STATUS_SUCCESS)
				break;
			else if (status != STATUS_INFO_LENGTH_MISMATCH) { // 0xC0000004L
				return false;   // error
			}

			bufferSize *= 2;
			pspid = (PSYSTEM_PROCESS_INFORMATION_DETAILD) realloc ((PVOID)pspid, bufferSize);
		}

		PSYSTEM_PROCESS_INFORMATION_DETAILD pspidBase;
		pspidBase=pspid;
		
		
		//FIXME: Hack. Program name under windows is PROGRAM_NAME + ".exe"
		const char *EXENAME="3Depict.exe";
		
		//Loop through the linked list of process data structures
		while( (pspid=(PSYSTEM_PROCESS_INFORMATION_DETAILD)(pspid->NextEntryOffset + (PBYTE)pspid)) && pspid->NextEntryOffset)
		{
			
			
			//If the name exists, is not null, and its the  PID we are looking for
			if(pspid->ImageName.Length && pspid->ImageName.Buffer && (size_t)pspid->UniqueProcessId  == processID )
			{
				//FIXME: I am unsure about the multibyte handling here. I think this *only* works if the program name is within the ASCII region of the codepage
				wchar_t *name = new wchar_t[pspid->ImageName.Length+1];
				sprintf((char*)name,"%ls",pspid->ImageName.Buffer);
				if(!strcmp(EXENAME,(char*)name))
				{
			
					delete[] name;
					
					free(pspidBase);
					return true;
				}
				delete[] name;

			}
			
		}

		free(pspidBase);

	}
#endif



void copyRGBAtoWXImage(unsigned int width, unsigned int height,
		const unsigned char *rgbaBuf, wxImage &image, 
		const unsigned char *mask)

{
	//wx image must have an alpha channel
	ASSERT(image.HasAlpha());
	ASSERT(image.GetWidth() == width && 
		image.GetHeight() == height);

	//FIXME: This will likely be very slow
	const unsigned char *p=rgbaBuf;
	for(unsigned int uj=0;uj<height;uj++)
	{
		for(unsigned int ui=0;ui<width;ui++)
		{	
			image.SetRGB(ui,uj,*p, *(p+1), *(p+2)); 

			if( (*p == *mask) && 
			    ( *(mask+1) == *(p+1)) &&
			    ( *(mask+2) == *(p+2)) )
				image.SetAlpha(ui,uj,0);
			else
				image.SetAlpha(ui,uj,255);
			p+=4;
		}
	}

}

void combineWxImage(wxImage &base, const wxImage &overlay)
{
	ASSERT(base.GetWidth() == overlay.GetWidth());
	ASSERT(base.GetHeight() == overlay.GetHeight());
	ASSERT(overlay.HasAlpha());
	ASSERT(base.IsOk() && overlay.IsOk());


	unsigned int width=base.GetWidth();
	unsigned int height=base.GetHeight();


	//Now loop through each pixel and perform
	// combine operation
	#pragma omp parallel for
	for(unsigned int uj=0;uj<height;uj++)
	{
		unsigned char rgbIm[3],rgbaOv[4];
		for(unsigned int ui=0;ui<width;ui++)
		{
			rgbaOv[3] = overlay.GetAlpha(ui,uj);

			if(rgbaOv[3])
			{
				//obtain src rgb
				rgbIm[0]=base.GetRed(ui,uj);
				rgbIm[1]=base.GetGreen(ui,uj);
				rgbIm[2]=base.GetBlue(ui,uj);
				//obtain overlay rgb	
				rgbaOv[0]=overlay.GetRed(ui,uj);
				rgbaOv[1]=overlay.GetGreen(ui,uj);
				rgbaOv[2]=overlay.GetBlue(ui,uj);
		
		
				for(unsigned int chan=0;chan<3;chan++)
					rgbIm[chan] = (unsigned char) (float(255-rgbaOv[3])/255.0f*rgbIm[chan] + float(rgbaOv[3]/255.0f)*rgbaOv[chan]);
				
				base.SetRGB(ui,uj,rgbIm[0],rgbIm[1],rgbIm[2]);	
			}
		}
	}	

}

	
TreePersistNode::TreePersistNode(const wxTreeCtrl *treeCtrl,wxTreeItemId t)
{
	ASSERT(t.IsOk());

	expanded = treeCtrl->IsExpanded(t);
	selected = treeCtrl->IsSelected(t);
}

TreePersistNode::TreePersistNode()
{
}

void TreePersist::saveTreeExpandState(wxTreeCtrl *treeCtrl)
{
	treeState.clear();
	buildPathMapping(treeCtrl, treeState);
}

//TODO: Limit copy/paste code shared between overloads
void TreePersist::buildPathMapping(wxTreeCtrl *treeCtrl,std::map<std::string, TreePersistNode> &retMap)
{
	//DFS walker stack for wxwidgets' tree item IDs
	stack<pair<string,wxTreeItemId> > treeIDs;
	
	{
	wxTreeItemId baseItem;
	baseItem=treeCtrl->GetRootItem();

	//If tree is empty, then baseItem may not be OK	
	if(!baseItem.IsOk())
		return;
	//Start with wx root node
	treeIDs.push(make_pair("",baseItem));
	}

	//Build the map<> containing the flattened path in the tree
	// and 
	while(!treeIDs.empty())
	{
		std::string pathStr;
		wxTreeItemId curItem;
		pathStr= treeIDs.top().first;
		curItem = treeIDs.top().second;
		ASSERT(curItem.IsOk());
		treeIDs.pop();
		if(treeCtrl->ItemHasChildren(curItem))
		{
			wxTreeItemId tmp;
			wxTreeItemIdValue token;
			std::string baseStr;
		
			tmp = treeCtrl->GetFirstChild(curItem,token);
			baseStr = pathStr;

			//Push all children onto processing stack
			//similarly, push down the tree<> content
			while(tmp.IsOk())
			{
				pathStr=baseStr+ 
					string("/") + stlStr(treeCtrl->GetItemText(tmp));

				while(retMap.find(pathStr) != retMap.end())
					pathStr+=PATH_NONCE;
				
				retMap[pathStr]=TreePersistNode(treeCtrl,tmp);
				treeIDs.push(make_pair(pathStr,tmp));
				
				tmp=treeCtrl->GetNextChild(curItem,token);
			}
		} 
	}
}

void TreePersist::buildPathMapping(wxTreeCtrl *treeCtrl,std::map<std::string, wxTreeItemId> &retMap)
{
	//DFS walker stack for wxwidgets' tree item IDs
	stack<pair<string,wxTreeItemId> > treeIDs;
		
	//Start with wx root node
	treeIDs.push(make_pair("",treeCtrl->GetRootItem()));

	//Build the map<> containing the flattened path in the tree
	// and 
	while(!treeIDs.empty())
	{
		std::string pathStr;
		wxTreeItemId curItem;
		pathStr= treeIDs.top().first;
		curItem = treeIDs.top().second;
		treeIDs.pop();
	
		if(treeCtrl->ItemHasChildren(curItem))
		{
			wxTreeItemId tmp;
			wxTreeItemIdValue token;
			std::string baseStr;
		
			tmp = treeCtrl->GetFirstChild(curItem,token);
			baseStr = pathStr;

			//Push all children onto processing stack
			while(tmp.IsOk())
			{
				wxString s;
				s=treeCtrl->GetItemText(tmp);
				pathStr=baseStr+ string("/");
				pathStr+=stlStr(s);
				while(retMap.find(pathStr) != retMap.end())
					pathStr+=PATH_NONCE;
				
				treeIDs.push(make_pair(pathStr,tmp));
				retMap[pathStr] = tmp;
				tmp=treeCtrl->GetNextChild(curItem,token);
			}
		}
	}
}


void TreePersist::restoreTreeExpandState(wxTreeCtrl *treeCtrl) const
{
	map<string,wxTreeItemId> treeMap;
	buildPathMapping(treeCtrl,treeMap);

	for(map<string,wxTreeItemId>::const_iterator it=treeMap.begin(); it!=treeMap.end(); ++it)
	{
		map<string,TreePersistNode>::const_iterator stateIt;
		stateIt= treeState.find(it->first);

		//If the path is found, then restore the item state
		if(stateIt!=treeState.end())
		{
			if(stateIt->second.expanded)
				treeCtrl->Expand(it->second); 
		}

	}
}
