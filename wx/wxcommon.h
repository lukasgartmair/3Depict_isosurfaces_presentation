/*
 * wxcommon.h  - Common wxwidgets header stuff
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
#ifndef WXCOMMON_H
#define WXCOMMON_H
#include <wx/wx.h>
#include <wx/url.h>
#include <wx/treectrl.h>

#include <map>

#include "common/basics.h"
#include "backend/tree.hh"


//This function is adapted from
//http://www.creatis.insa-lyon.fr/software/public/creatools/bbtk/v0_9_2/doc/doxygen/bbtk/bbtkWx_8h-source.html
//Authors : Eduardo Davila, Laurent Guigues, Jean-Pierre Roux
//Used under CeCILLB licence, limited liability and no warranty by authors. 
//http://www.cecill.info/licences/Licence_CeCILL-B_V1-en.html 
//This licence is compatible with GPL licence used in 3Depict.
inline std::string stlStr(const wxString& s){
	std::string s2;
	if(s.wxString::IsAscii()) {
		s2=s.wxString::ToAscii();
	} else {
		const wxWX2MBbuf tmp_buf = wxConvCurrent->cWX2MB(s);
		const char *tmp_str = (const char*) tmp_buf;
		s2=std::string(tmp_str, strlen(tmp_str));
	}
	return s2;
}

//--------
//Perform validation of a wx text control, adjusting appearance as needed
// can pass an additional constraint function that needs to be satisfied (return true)
// in order for validation to succeed
template<class T> 
bool validateTextAsStream(wxTextCtrl *t, T &i, bool (*conditionFunc)(const T&))
{
	bool isOK;
	std::string s; 
	s= stlStr(t->GetValue());

	//string cannot be empty
	bool condition;
	condition = s.empty() || stream_cast(i,s);

	if(condition && conditionFunc)
		condition&=(*conditionFunc)(i);

	if(condition)
	{
		//OK, so bad things happened. Prevent the user from doing this
		isOK=false;

		//if it is bad and non-empty, highlight it as such
		// if it is empty, then just set it to normal colour
		if(s.empty())
			t->SetBackgroundColour(wxNullColour);
		else
			t->SetBackgroundColour(*wxCYAN);
	}
	else
	{
		t->SetBackgroundColour(wxNullColour);
		isOK=true;
	}


	return isOK;
}

template<class T> 
bool validateTextAsStream(wxTextCtrl *t, T &i)
{
	bool isOK;
	std::string s; 
	s= stlStr(t->GetValue());

	//string cannot be empty
	bool condition;
	condition = s.empty() || stream_cast(i,s);

	if(condition)
	{
		//OK, so bad things happened. Prevent the user from doing this
		isOK=false;

		//if it is bad and non-empty, highlight it as such
		// if it is empty, then just set it to normal colour
		if(s.empty())
			t->SetBackgroundColour(wxNullColour);
		else
			t->SetBackgroundColour(*wxCYAN);
	}
	else
	{
		t->SetBackgroundColour(wxNullColour);
		isOK=true;
	}


	return isOK;
}

//--------

void wxErrMsg(wxWindow *, const std::string &title,
		const std::string &mesg);

//locate the file we are looking for in OS specific paths. Returns empty string if file cannot be found
std::string locateDataFile(const char *name);

//Custom event for remote update thread posting
extern wxEventType RemoteUpdateAvailEvent; 

//Custom event for signalling that a refresh is completed
extern wxEventType RefreshCompleteEvent; 

//Return true IFF process ID and process name match running process
bool processMatchesName(size_t processID, const std::string &procName);

//Copy data into a wx image from an unpadded RGBA block of given width/eight
// image sould already have been initialised, with RGBA and size
void copyRGBAtoWXImage(unsigned int width, unsigned int height,
		const unsigned char *rgbaBuf, wxImage &image, const unsigned char *mask);

//Combine an overlay using the alpha channel
void combineWxImage(wxImage &base, const wxImage &overlay);

//!Remote version thread checker, downloads RSS file from remote system and then
// parses the file for the latest remote version number
class VersionCheckThread : public wxThread
{
	private:
		bool complete;
		bool retrieveOK;
		std::string versionStr;
		//Window to post event to upon completion
		wxWindow *targetWindow;
		wxURL url;
	public:
		VersionCheckThread(wxWindow *target); 

		//!Used internally by wxwidgets to launch thread
		void *Entry();

		//Returns true upon completion of thread.
		bool isComplete()  const { return complete; }

		//Returns true if version string was retrieved successfully
		bool isRetrieveOK() const { return retrieveOK; }

		//Return the maximal version string obtained from the remote RSS feed
		std::string getVerStr() {  return versionStr; }
};


//!storage node of tree persistence 
// - tree persistence means the state for 
//   a tree in terms of expanded and not-expanded items
class TreePersistNode
{
	public:
		bool expanded,selected;

		//Contructor for node persistence
		TreePersistNode();
		TreePersistNode(const wxTreeCtrl *ctrl,wxTreeItemId t);
};

//!storage helper class for tree persistence 
//   persistence - expanded and not-expanded items
class TreePersist
{
	private:
		std::map<std::string,TreePersistNode> treeState;

		//Build path mapping for persistance node
		static void buildPathMapping(wxTreeCtrl *t, std::map<std::string,TreePersistNode>  &retMap);
		//Build path mapping for wx tree items
		static void buildPathMapping(wxTreeCtrl *t, std::map<std::string,wxTreeItemId>  &retMap);
	public:
		//TODO: Const correctness
		void saveTreeExpandState(wxTreeCtrl *treeCtrl);
		void restoreTreeExpandState(wxTreeCtrl *treeCtrl) const;
};

#endif
