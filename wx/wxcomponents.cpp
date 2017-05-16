/*
 *	wxcomponents.h - Custom wxWidgets components header
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

#include "wxcomponents.h"
#include "wxcommon.h"
#include "common/stringFuncs.h"

#include "common/constants.h"
#include "common/translation.h"


#include <wx/clipbrd.h>

#include <stack>


using std::ofstream;
using std::vector;
using std::stack;

const float FONT_HEADING_SCALEFACTOR=1.25f;

void upWxTreeCtrl(const FilterTree &filterTree, wxTreeCtrl *t,
		std::map<size_t,Filter *> &filterMap,vector<const Filter *> &persistentFilters,
		const Filter *visibleFilt)
{
	TreePersist tPersist;
	tPersist.saveTreeExpandState(t);
	//Remove any filters that don't exist any more
	for(unsigned int ui=persistentFilters.size();ui--;)
	{
		if(!filterTree.contains(persistentFilters[ui]))
		{
			std::swap(persistentFilters[ui],persistentFilters.back());
			persistentFilters.pop_back();
		}
	}

	stack<wxTreeItemId> treeIDs;
	t->Freeze();
	//Warning: this generates an event, 
	//most of the time (some windows versions do not according to documentation)
	t->DeleteAllItems();

	//Clear the mapping
	filterMap.clear();
	size_t nextID=0;
	
	size_t lastDepth=0;
	//Add dummy root node. This will be invisible to wxTR_HIDE_ROOT controls
	wxTreeItemId tid;
	tid=t->AddRoot(wxT("TreeBase"));
	t->SetItemData(tid,new wxTreeUint(nextID));

	// Push on stack to prevent underflow, but don't keep a copy, 
	// as we will never insert or delete this from the UI	
	treeIDs.push(tid);

	nextID++;
	std::map<const Filter*,wxTreeItemId> reverseFilterMap;
	//Depth first  add
	for(tree<Filter * >::pre_order_iterator filtIt=filterTree.depthBegin();
					filtIt!=filterTree.depthEnd(); ++filtIt)
	{	
		//Push or pop the stack to make it match the iterator position
		if( lastDepth > filterTree.depth(filtIt))
		{
			while(filterTree.depth(filtIt) +1 < treeIDs.size())
				treeIDs.pop();
		}
		else if( lastDepth < filterTree.depth(filtIt))
		{
			treeIDs.push(tid);
		}
		

		lastDepth=filterTree.depth(filtIt);
	
		//This will use the user label or the type string.	
		tid=t->AppendItem(treeIDs.top(),
			((*filtIt)->getUserString()));
		t->SetItemData(tid,new wxTreeUint(nextID));
	

		//Record mapping to filter for later reference
		filterMap[nextID]=*filtIt;
		//Remember the reverse mapping for later in
		// this function when we reset visibility
		reverseFilterMap[*filtIt] = tid;

		nextID++;
	}

	//Try to restore the  selection in a user friendly manner
	// - Try restoring all requested filter's visibility
	// - then restore either the first requested filter as the selection
	//   or the specified parameter filter as the selection.
	if(persistentFilters.size())
	{
		for(unsigned int ui=0;ui<persistentFilters.size();ui++)
			t->EnsureVisible(reverseFilterMap[persistentFilters[ui]]);

		if(!visibleFilt)
			t->SelectItem(reverseFilterMap[persistentFilters[0]]);
		else
			t->SelectItem(reverseFilterMap[visibleFilt]);

		persistentFilters.clear();
	}
	else if(visibleFilt)
	{
		ASSERT(reverseFilterMap.find(visibleFilt)!=reverseFilterMap.end())
		t->SelectItem(reverseFilterMap[visibleFilt]);
	}
	
	t->GetParent()->Layout();

	tPersist.restoreTreeExpandState(t);

	t->Thaw();
}

//Convert my internal choice string format to comma delimited 
std::string choiceStringToCommaDelim(std::string choiceString)
{
	std::string retStr;

	bool haveColon=false;
	bool haveBar=false;
	for(unsigned int ui=0;ui<choiceString.size();ui++)
	{
		if(haveColon && haveBar )
		{
			if(choiceString[ui] != ',')
				retStr+=choiceString[ui];
			else
			{
				haveBar=false;
				retStr+=",";
			}
		}
		else
		{
			if(choiceString[ui]==':')
				haveColon=true;
			else
			{
				if(choiceString[ui]=='|')
					haveBar=true;
			}
		}
	}

	return retStr;
}


BEGIN_EVENT_TABLE(CopyGrid, wxGrid)
	EVT_KEY_DOWN(CopyGrid::OnKey) 
END_EVENT_TABLE()
CopyGrid::CopyGrid(wxWindow* parent, wxWindowID id, 
		const wxPoint& pos , 
		const wxSize& size , 
		long style, const wxString& name ) : wxGrid(parent,id,pos,size,style,name)
{

}


void CopyGrid::selectData()
{
}

void CopyGrid::saveData()
{
	wxFileDialog *wxF = new wxFileDialog(this,TRANS("Save Data..."), wxT(""),
		wxT(""),TRANS("Text File (*.txt)|*.txt|All Files (*)|*"),wxFD_SAVE);

	if( (wxF->ShowModal() == wxID_CANCEL))
		return;
	

	std::string dataFile = stlStr(wxF->GetPath());
	ofstream f(dataFile.c_str());

	if(!f)
	{
		wxMessageDialog *wxD  =new wxMessageDialog(this,
			TRANS("Error saving file. Check output dir is writable."),TRANS("Save error"),wxOK|wxICON_ERROR);

		wxD->ShowModal();
		wxD->Destroy();

		return;
	}	 

        // Number of rows and cols
        int rows,cols;
	rows=GetNumberRows();
	cols=GetNumberCols();
        // data variable contain text that must be set in the clipboard
        // For each cell in selected range append the cell value in the data
	//variable
        // Tabs '\\t' for cols and '\\r' for rows
	
	//print headers
	for(int  c=0; c<cols; c++)
	{
		f << stlStr(GetColLabelValue(c)) << "\t";
	}
	f<< std::endl;
	
	
        for(int  r=0; r<rows; r++)
	{
            for(int  c=0; c<cols; c++)
	    {
                f << stlStr(GetCellValue(r,c));
                if(c < cols - 1)
                    f <<  "\t";

	    }
	    f << std::endl; 
	}
	
}

void CopyGrid::OnKey(wxKeyEvent &event)
{



        if (event.CmdDown() && event.GetKeyCode() == 67)
	{
	
            copyData();
	    return;
	}
    event.Skip();
    return;

}



void CopyGrid::copyData() 
{
       
	
	//This is an undocumented class AFAIK. :(
	wxGridCellCoordsArray arrayTL(GetSelectionBlockTopLeft());
	wxGridCellCoordsArray arrayBR(GetSelectionBlockBottomRight());
	

	// data variable contain text that must be set in the clipboard
	std::string data;
	std::string endline;
#ifdef __WXMSW__
	endline="\r\n";
#else
	endline="\n";
#endif

        // Number of rows and cols
        int rows,cols;
	if(arrayTL.Count() && arrayBR.Count())
	{

		wxGridCellCoords coordTL = arrayTL.Item(0);
		wxGridCellCoords coordBR = arrayBR.Item(0);


		rows = coordBR.GetRow() - coordTL.GetRow() +1;
		cols = coordBR.GetCol() - coordTL.GetCol() +1;	

		//copy title from header
		if(cols)
		{
			for(int ui=0;ui<cols;ui++)
			{
				data+=stlStr(GetColLabelValue(ui));

				if(ui<cols-1)
					data+="\t";
			}
			data+=endline;
		}

		// For each cell in selected range append the cell value in the data
		//variable
		// Tabs '\\t' for cols and '\\n' for rows
		for(int  r=0; r<rows; r++)
		{
		    for(int  c=0; c<cols; c++)
		    {
			data+= stlStr(GetCellValue(coordTL.GetRow() + r,
							coordTL.GetCol()+ c));
			if(c < cols - 1)
			    data+= "\t";

		    }
		    data+=endline;
		}
	}
	else
	{
		const wxArrayInt& rows(GetSelectedRows());
		const wxArrayInt& cols(GetSelectedCols());
//		const wxGridCellCoordsArray& cells(GetSelectedCells());

		//Copy title from header
		if(cols.size())
		{
			for(int ui=0;ui<(int)cols.size();ui++)
			{
				data+=stlStr(GetColLabelValue(ui));

				if(ui<(int)cols.size()-1)
					data+="\t";
			}
			data+=endline;

			for(int ui=0;ui<GetNumberRows(); ui++)
			{
			    for(int  c=0; c<cols.size(); c++)
			    {
				data+= stlStr(GetCellValue(ui,cols[c]));
				if(c < cols.size()-1)
				    data+= "\t";

			    }
			    data +=endline;
			}


		}
		else if (rows.size())
		{
			for(int ui=0;ui<GetNumberCols();ui++)
			{
				data+=stlStr(GetColLabelValue(ui));

				if(ui<cols.size()-1)
					data+="\t";
			}
			data+=endline;

			for(int r=0;r<(int)rows.size(); r++)
			{
			    for(int  c=0; c<GetNumberCols(); c++)
			    {
				data+= stlStr(GetCellValue(rows[r],c));
				if(c < GetNumberCols()-1)
				    data+= "\t";

			    }
			    data +=endline;
			}
		}
/*		else if(cells.size())
		{
			//FIXME: Needs more thought than I have time for right now. 
			// the problem is that cells[] doesn't necessarily sort tl->br,
			// so you have to do this first
			int lastRow=cells[0].GetRow();
			int lastCol=cells[0].GetCol();
			for(int cell=0; cell<cells.size();cell++)
			{

				if(cells[cell].GetRow() > lastRow)
				{
					lastRow=cells[cell].GetRow();
					data+=endline;
				}
				data+=stlStr(GetCellValue(cells[cell].GetRow(),
							cells[cell].GetCol()));

				if(lastCol < cells[cell].GetCol())
				{
					lastCol=cells[cell].GetCol();
					data+="\t";
				}
			}
		} */
		else 
			return;


	}

	// Put the data in the clipboard
	if (wxTheClipboard->Open())
	{
		wxTextDataObject* clipData= new wxTextDataObject;
		// Set data object value
		clipData->SetText((data));
		wxTheClipboard->UsePrimarySelection(false);
		wxTheClipboard->SetData(clipData);
		wxTheClipboard->Close();
	}

}


BEGIN_EVENT_TABLE(TextTreeCtrl, wxTreeCtrl)
	EVT_PAINT(TextTreeCtrl::OnTreePaint)
END_EVENT_TABLE()

void TextTreeCtrl::OnTreePaint(wxPaintEvent &event)
{
	//Draws a message in the text control, if the
	// control is otherwise empty

	//Call standard handler on exit
	event.Skip(true);
	//If there are items in the control, just abort
	if(GetCount() || messageStrs.empty())
		return;

	//scan for the largest string
	size_t largestTextSize=0,idx=(size_t)-1;
	for(size_t ui=0;ui<messageStrs.size();ui++)
	{
		if(messageStrs[ui].size() > largestTextSize)
		{
			largestTextSize=messageStrs[ui].size();
			idx=ui;
		}
	}

	if(idx ==(size_t) -1)
		return;

	//Check that the string we want fits in the control 
	int w,h;
	GetClientSize(&w,&h);

	//Create drawing context
	wxPaintDC *dc = new wxPaintDC(this);
	//Set text font
	wxFont font;
	font.SetFamily(wxFONTFAMILY_SWISS);
	
	if(font.IsOk())
		dc->SetFont(font);
	
	wxSize textSize=dc->GetTextExtent((messageStrs[idx]));

	//Don't go ahead with the drawing if the text
	// won't fit in the control
	const float HEIGHT_SPACING=1.1;
	float blockHeight=textSize.GetHeight()*messageStrs.size()*HEIGHT_SPACING;

	if(textSize.GetWidth() >=w || blockHeight> h) 
	{
		delete dc;
		return;
	}

	//Draw each text in turn, advancing by spacing
	
	// start far enough back so that 
	float startY= 0.5*(h - blockHeight);

	for(size_t ui=0;ui<messageStrs.size();ui++)
	{
		textSize=dc->GetTextExtent((messageStrs[ui]));
		int startX;
		startX=w/2 - textSize.GetWidth()/2; 

#if !(defined(_WIN32) || defined(_WIN64) ) 
		dc->DrawText((messageStrs[ui]),
					startX,startY);	
#else
		dc->DrawTextW((messageStrs[ui]),
					startX,startY);
#endif
		startY+=HEIGHT_SPACING*textSize.GetHeight();
	}

	delete dc;
}

std::string TTFFinder::findFont(const char *fontFile)
{
	//Action is OS dependant
	
#ifdef __APPLE__
		return macFindFont(fontFile);
#elif defined __UNIX_LIKE__ || defined __linux__
		return nxFindFont(fontFile);
#elif defined  __WINDOWS__
		return winFindFont(fontFile);
#else
#error OS not detected in preprocessor series
#endif
}


#ifdef __APPLE__
std::string TTFFinder::macFindFont(const char *fontFile) 
{
	//This is a list of possible target dirs to search
	//(Oh look Ma, I'm autoconf!)
	const char *dirs[] = {	".",
				"/Library/Fonts",
				"" ,
				}; //MUST end with "".

	wxPathList *p = new wxPathList;

	unsigned int ui=0;
	//Try a few standard locations
	while(strlen(dirs[ui]))
	{
		p->Add((dirs[ui]));
		ui++;
	};

	wxString s;

	//execute the search for the file
	s= p->FindValidPath((fontFile));


	std::string res;
	if(s.size())
	{
		if(p->EnsureFileAccessible(s))
			res = stlStr(s);
	}

	delete p;
	return res;
}
#elif defined __UNIX_LIKE__ || defined __linux__
std::string TTFFinder::nxFindFont(const char *fontFile) 
{
	//This is a list of possible target dirs to search
	//(Oh look Ma, I'm autoconf!)

	const char *dirs[] = {	".",
				"/usr/share/fonts/truetype", //Old debian 
				"/usr/share/fonts/truetype/freefont", // New debian
				"/usr/share/fonts/truetype/ttf-dejavu", //New debian
				"/usr/local/share/fonts/truetype", // User fonts
				"/usr/X11R6/lib/X11/fonts/truetype",
				"/usr/X11R6/lib64/X11/fonts/truetype",
				"/usr/lib/X11/fonts/truetype",// Fedora 32
				"/usr/lib64/X11/fonts/truetype", //Fedora 64
				"/usr/local/lib/X11/fonts/truetype", // Fedora 32 new
				"/usr/local/lib64/X11/fonts/truetype",// Fedora 64 new
				"",
				}; //MUST end with "".

	wxPathList *p = new wxPathList;

	unsigned int ui=0;
	//Try a few standard locations
	while(strlen(dirs[ui]))
	{
		p->Add((dirs[ui]));
		ui++;
	};

	wxString s;

	//execute the search for the file
	s= p->FindValidPath((fontFile));


	std::string res;
	if(s.size())
	{
		if(p->EnsureFileAccessible(s))
			res = stlStr(s);
	}

	delete p;
	return res;
}
#elif defined  __WINDOWS__
std::string TTFFinder::winFindFont(const char *fontFile)
{
            //This is a list of possible target dirs to search
	//(Oh look Ma, I'm autoconf!)
	const char *dirs[] = {	".",
               "C:\\Windows\\Fonts",
				"",
				}; //MUST end with "".

	wxPathList *p = new wxPathList;

	unsigned int ui=0;
	//Try a few standard locations
	while(strlen(dirs[ui]))
	{
		p->Add((dirs[ui]));
		ui++;
	};

	wxString s;

  
 
	//execute the search for the file
	s= p->FindValidPath((fontFile));


	std::string res;
	if(s.size())
	{
		if(p->EnsureFileAccessible(s))
			res = stlStr(s);
	}

	delete p;
	return res;


}
#endif

std::string TTFFinder::suggestFontName(unsigned int fontType, unsigned int index) 
{
	//Possible font names
	const char *sansFontNames[] = {
		//First fonts are fonts I have a preference for in my app
		//in my preference order
		"FreeSans.ttf",
		"DejaVuSans.ttf",
		"Arial.ttf",
		"ArialUnicodeMS.ttf",
		"NimbusSansL.ttf",
		"LiberationSans.ttf",
		"Courier.ttf",
		
		//These are simply in semi-alphabetical order
		//may not even be font names (font families) :)
		"AkzidenzGrotesk.ttf",
		"Avenir.ttf",
		"BankGothic.ttf",
		"Barmeno.ttf",
		"Bauhaus.ttf",
		"BellCentennial.ttf",
		"BellGothic.ttf",
		"BenguiatGothic.ttf",
		"Beteckna.ttf",
		"Calibri.ttf",
		"CenturyGothic.ttf",
		"Charcoal.ttf",
		"Chicago.ttf",
		"ClearfaceGothic.ttf",
		"Clearview.ttf",
		"Corbel.ttf",
		"Denmark.ttf",
		"Droid.ttf",
		"Eras.ttf",
		"EspySans.ttf",
		"Eurocrat.ttf",
		"Eurostile.ttf",
		"FFDax.ttf",
		"FFMeta.ttf",
		"FranklinGothic.ttf",
		"Frutiger.ttf",
		"Futura.ttf",
		"GillSans.ttf",
		"Gotham.ttf",
		"Haettenschweiler.ttf",
		"HandelGothic.ttf",
		"Helvetica.ttf",
		"HelveticaNeue.ttf",
		"HighwayGothic.ttf",
		"Hobo.ttf",
		"Impact.ttf",
		"Johnston.ttf",
		"NewJohnston.ttf",
		"Kabel.ttf",
		"LucidaGrande.ttf",
		"Macintosh.ttf",
		"Microgramma.ttf",
		"Motorway.ttf",
		"Myriad.ttf",
		"NewsGothic.ttf",
		"Optima.ttf",
		"Pricedown.ttf",
		"RailAlphabet.ttf",
		"ScalaSans.ttf",
		"SegoeUI.ttf",
		"Skia.ttf",
		"Syntax.ttf",
		"",
	};

	//FIXME: Suggest some font names
	const char *serifFontNames[] = {""};	
					
	//FIXME: Suggest some font names
	const char *monoFontNames[] = {""};	



	std::string s;
	switch(fontType)
	{
		case TTFFINDER_FONT_SANS:
			s = sansFontNames[index];
			break;
		case TTFFINDER_FONT_SERIF:
			s = serifFontNames[index];
			break;
		case TTFFINDER_FONT_MONO:
			s = monoFontNames[index];
			break;
	}

	return s;
}

std::string TTFFinder::getBestFontFile(unsigned int type) 
{
	unsigned int index=0;

	std::string s;

	do
	{
		s=suggestFontName(type,index);

		if(s.size())
		{
			index++;
			s=findFont(s.c_str());
			if(s.size())
			{
				return s;	
			}
		}
		else
			return s;
	}
	while(true);

	ASSERT(false);
	return s;
}
