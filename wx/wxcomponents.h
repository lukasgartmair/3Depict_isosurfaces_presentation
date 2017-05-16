/*
 * 	wxcomponents.h - custom wxwidgets components
 *	Copyright (C) 2015, D. Haley

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


#ifndef WXCOMPONENTS_H
#define WXCOMPONENTS_H

#include <wx/grid.h>
#include <wx/treectrl.h>
#include <wx/laywin.h>

#include <vector>
#include <string>


#include "backend/filtertree.h"



//!3D combo grid renderer, from
//http://nomadsync.cvs.sourceforge.net/nomadsync/nomadsync/src/EzGrid.cpp?view=markup (GPL)
class wxGridCellChoiceRenderer : public wxGridCellStringRenderer
{
public:
	wxGridCellChoiceRenderer(wxLayoutAlignment border = wxLAYOUT_NONE) :
			m_border(border) {}
	virtual void Draw(wxGrid& grid,
	                  wxGridCellAttr& attr,
	                  wxDC& dc,
	                  const wxRect& rect,
	                  int row, int col,
	                  bool isSelected);
	virtual wxGridCellRenderer *Clone() const
	{
		return new wxGridCellChoiceRenderer;
	}
private:
	wxLayoutAlignment m_border;

};

class wxFastComboEditor : public wxGridCellChoiceEditor
{
public:
	wxFastComboEditor(const wxArrayString choices,	bool allowOthers = FALSE) : 
		wxGridCellChoiceEditor(choices, allowOthers), 
			m_pointActivate(-1,-1)
		{
			SetClientData((void*)&m_pointActivate);
		}
	virtual void BeginEdit(int row, int col, wxGrid* grid);
protected:
	wxPoint m_pointActivate;
};

//!Update a wxTree control to layout according to the specified filter tree
void upWxTreeCtrl(const FilterTree &filterTree, wxTreeCtrl *t,
		std::map<size_t,Filter *> &filterMap,std::vector<const Filter *> &persistentFilters,
		const Filter *visibleFilt);

//Subclassed wx tree ctrl to draw text in tree when empty
class TextTreeCtrl : public wxTreeCtrl
{
	private:
		std::vector<std::string> messageStrs;
	public:
		 TextTreeCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxTR_HAS_BUTTONS, const wxValidator& validator = wxDefaultValidator, const wxString& name = _("treeCtrl")) : wxTreeCtrl(parent,id,pos,size,style,validator,name) {};


		virtual void OnTreePaint(wxPaintEvent &evt);

		void setMessages(const std::vector<std::string> &msgs) { messageStrs=msgs;}

		DECLARE_EVENT_TABLE()
};


//!Data container for tree object data
class wxTreeUint : public wxTreeItemData
{
	public:
		wxTreeUint(unsigned int v) : value(v) {};
		unsigned int value;
};

//!Data container for wxWidgets list object data
class wxListUint : public wxClientData
{
	public:
		wxListUint(unsigned int v) { value=v;};
		unsigned int value;
};



//A wx Grid with copy & paste support
class CopyGrid : public wxGrid
{

public:
	CopyGrid(wxWindow* parent, wxWindowID id, 
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize, 
		long style = wxWANTS_CHARS, const wxString& name = wxPanelNameStr);

	void currentCell();
	void selectData();
	//!Copy data to the clipboard 
	void copyData();
	//!Prompts user to save data to file, and then saves it. pops up error 
	// dialog box if there is a problem. Data is tab deliminated
	void saveData();

	virtual void OnKey(wxKeyEvent &evt);

	virtual ~CopyGrid(){};
		
	DECLARE_EVENT_TABLE()

};


//Type IDs for TTFFinder::suggestFontName
enum
{
	TTFFINDER_FONT_SANS,
	TTFFINDER_FONT_SERIF,
	TTFFINDER_FONT_MONO
};

//A class to determine ttf file locations, in a best effort fashion
class TTFFinder 
{
	private:
		//*n?x (FHS compliant) searching
		static std::string nxFindFont(const char *fontFile);
		//MS win. searching
		static std::string winFindFont(const char *fontFile);
		//Mac OS X searching
		static std::string macFindFont(const char *fontFile);
	public:
		//Given a ttf file name, search for it in several common paths
		static std::string findFont(const char *fontFile);

		//!Given an font type (Sans, serif etc) suggest a font name. 
		//As long as function does not return empty std::string,, then index+1 is a valid
		//query (which may return empty std::string). Font names returned are a suggestion
		//only. Pass to findFont to confirm that a font file exists.
		static std::string suggestFontName(unsigned int fontType, unsigned int index) ;

		//!Returns a valid file that points to an installed ttf file, or an empty string
		//NOTE: TTF file is not checked for content validity!
		static std::string getBestFontFile(unsigned int type);
};

#endif

