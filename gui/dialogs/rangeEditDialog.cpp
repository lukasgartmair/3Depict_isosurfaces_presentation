/*
 *	rangeEditDialog.h - Point data export dialog
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

#include "rangeEditDialog.h"

#include "wx/wxcommon.h"
#include "wx/wxcomponents.h"
#include "common/translation.h"

#include "backend/filters/rangeFile.h"

#include <wx/colordlg.h>

#include <set>

using std::pair;
using std::vector;
using std::map;
using std::set;
using std::string;

// begin wxGlade: ::extracode
enum
{
	ID_CHECK_SHOW_OVERLAY=wxID_ANY+1,
	ID_SPLIT_LEFTRIGHT,
	ID_GRID_IONS,
	ID_GRID_RANGES,
	ID_BTN_RANGE_ADD,
	ID_BTN_RANGE_REMOVE,
	ID_LIST_OVERLAY,
	ID_LIST_PLOTS,
	ID_MANAGE_SET_MORE,
	ID_TEXT_FILTER_CMPNT,
	ID_PLOT_AREA,
};
// end wxGlade


enum
{
	ION_COL_PLOT=0,
	ION_COL_SHORTNAME,
	ION_COL_LONGNAME,
	ION_COL_COLOUR,
	ION_COL_ENUM_END
};

enum
{
	RNG_COL_PLOT=0,
	RNG_COL_PARENT_ION,
	RNG_COL_START,
	RNG_COL_END,
	RNG_COL_ENUM_END
};

enum
{
	GRID_FOCUS_NONE,
	GRID_FOCUS_IONS,
	GRID_FOCUS_RANGES
};

//Rangefile filter -> range file mapping typedef
typedef map<const RangeFileFilter *, RangeFile> RFMAP ;


PendingRange::PendingRange(RangeFile *rng)
{
	validStart=false;
	validEnd=false;
	validParent=false;
	rngPtr=rng;
}

void PendingRange::commit()
{
	ASSERT(isFinished());
	rngPtr->addRange(start,end,parentId);
}

float PendingRange::getStart() const
{
	if(validStart)
		return start;
	else
		return 0.0f;
}

float PendingRange::getEnd() const
{
	if(validEnd)
		return end;
	else
		return 1.0f;
}

std::string PendingRange::getIonName() const
{
	if(!validParent)
		return "";

	ASSERT(parentId < rngPtr->getNumIons());

	return rngPtr->getName(parentId);

}

bool PendingRange::isFinished() const
{
	if(!validEnd || !validStart)
		return false;

	if(!validParent)
		return false;

	if( end <=start)
		return false;

	return true;
}

PendingIon::PendingIon(RangeFile *rng)
{
	rngPtr=rng;
	validColour=validShortName=validLongName=false;

	colour.red=colour.green=colour.blue=0.5f;
}

void PendingIon::setShortName(const std::string &n)
{
	shortName=n;
	validShortName = (rngPtr->getIonID(shortName.c_str()) == (unsigned int)-1);
}

void PendingIon::setLongName(const std::string &n)
{
	longName=n;
	validLongName = (rngPtr->getIonID(shortName.c_str(),false) == (unsigned int)-1);
}

void PendingIon::setColour(const RGBf &c)
{
	colour=c;
	validColour=true;
}

RGBf PendingIon::getColour() const
{
	return colour;
}

std::string PendingIon::getShortName() const
{
	if(validShortName)
		return shortName; 
	else
		return string("");
}

std::string PendingIon::getLongName() const
{
	if(validLongName)
		return longName; 
	else
		return string("");
}

bool PendingIon::isFinished() const
{
	//Check to see if we have valid user data
	if (! (validShortName && validLongName && validColour))
		return false;

	//Disallow existing ion names
	if(rngPtr->getIonID(longName.c_str(),false) != (unsigned int)-1 ||
		rngPtr->getIonID(shortName.c_str()) != (unsigned int) -1)
		return false;

	return true;
}

void PendingIon::commit() const
{
	rngPtr->addIon(shortName,longName,colour);
}

RangeEditorDialog::RangeEditorDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxMAXIMIZE_BOX|wxMINIMIZE_BOX)
{
    // begin wxGlade: RangeEditorDialog::RangeEditorDialog
    splitVertical = new wxSplitterWindow(this, ID_SPLIT_LEFTRIGHT, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    panelSplitRight = new wxPanel(splitVertical, wxID_ANY);
    panelSplitLeft = new wxPanel(splitVertical, wxID_ANY);
    notebookLeft = new wxNotebook(panelSplitLeft, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_LEFT);
    noteLeftOverlay = new wxPanel(notebookLeft, wxID_ANY);
    noteLeftRanges = new wxPanel(notebookLeft, wxID_ANY);
    noteLeftPlots = new wxPanel(notebookLeft, wxID_ANY);
    listPlots = new wxListBox(noteLeftPlots, ID_LIST_PLOTS, wxDefaultPosition, wxDefaultSize, 0,(const wxString*) NULL);
    gridIons= new wxGrid(noteLeftRanges, ID_GRID_IONS);
    gridRanges = new wxGrid(noteLeftRanges, ID_GRID_RANGES);
    btnRangeIonAdd = new wxButton(noteLeftRanges,wxID_ADD, wxEmptyString);
    btnRangeIonRemove = new wxButton(noteLeftRanges, wxID_REMOVE, wxEmptyString);
    checkShowOverlay = new wxCheckBox(noteLeftOverlay, ID_CHECK_SHOW_OVERLAY, TRANS("Show Overlays"));
    textOverlayCmpnt = new wxTextCtrl(noteLeftOverlay, ID_TEXT_FILTER_CMPNT, wxEmptyString,
    						wxDefaultPosition,wxDefaultSize,wxTE_PROCESS_ENTER);
    listOverlay = new wxCheckListBox(noteLeftOverlay, ID_LIST_OVERLAY, wxDefaultPosition, wxDefaultSize, 0);
    plotPanel = new MathGLPane(panelSplitRight, ID_PLOT_AREA);
    btnOK = new wxButton(panelSplitRight, wxID_OK, wxEmptyString);
    btnCancel = new wxButton(panelSplitRight, wxID_CANCEL, wxEmptyString);

    set_properties();
    do_layout();
    // end wxGlade

    std::string fileLoc = locateDataFile("naturalAbundance.xml");
    if(!fileLoc.empty())
	    abundanceData.open(fileLoc.c_str());

    UpdateHandler h = (UpdateHandler)&RangeEditorDialog::onPlotUpdate;
    plotPanel->registerUpdateHandler(this,h);
    plotPanel->enableRegionSelfUpdate(true);

    lastFocused=GRID_FOCUS_NONE;
    currentRange=0;
    checkShowOverlay->SetValue(true);

    haveSetTextFocus=false;
    textOverlayCmpnt->SetValue(TRANS("e.g. H2O"));

    textOverlayCmpnt->Bind(wxEVT_SET_FOCUS, &RangeEditorDialog::OnTextOverlaySetFocus, this);
}



RangeEditorDialog::~RangeEditorDialog()
{
    textOverlayCmpnt->Unbind(wxEVT_SET_FOCUS, &RangeEditorDialog::OnTextOverlaySetFocus, this);
}


BEGIN_EVENT_TABLE(RangeEditorDialog, wxDialog)
    // begin wxGlade: RangeEditorDialog::event_table
    EVT_LISTBOX(ID_LIST_PLOTS, RangeEditorDialog::OnListPlots)
//    EVT_LIST_ITEM_SELECTED(ID_LIST_OVERLAY, RangeEditorDialog::OnListOverlaySelected)
    EVT_LIST_KEY_DOWN(ID_LIST_OVERLAY, RangeEditorDialog::OnListOverlayKeyDown) 
    EVT_TEXT(ID_TEXT_FILTER_CMPNT,RangeEditorDialog::OnTextOverlay)
    EVT_TEXT(ID_TEXT_FILTER_CMPNT,RangeEditorDialog::OnTextOverlay)
    EVT_TEXT_ENTER(ID_TEXT_FILTER_CMPNT,RangeEditorDialog::OnTextOverlayEnter)
    EVT_CHECKBOX(ID_CHECK_SHOW_OVERLAY, RangeEditorDialog::OnCheckShowOverlay)
    EVT_GRID_CMD_CELL_CHANGED(ID_GRID_RANGES, RangeEditorDialog::OnGridRangesCellChange)
    EVT_GRID_CMD_CELL_CHANGED(ID_GRID_IONS, RangeEditorDialog::OnGridIonsCellChange)
    EVT_GRID_CMD_CELL_LEFT_CLICK(ID_GRID_RANGES,RangeEditorDialog::OnGridRangeClick) 
    EVT_GRID_CMD_CELL_LEFT_CLICK(ID_GRID_IONS,RangeEditorDialog::OnGridIonClick) 
    EVT_GRID_CMD_EDITOR_SHOWN(ID_GRID_RANGES,RangeEditorDialog::OnGridRangesEditorShown)
    EVT_GRID_CMD_EDITOR_SHOWN(ID_GRID_IONS,RangeEditorDialog::OnGridIonsEditorShown)
    EVT_BUTTON(wxID_ADD, RangeEditorDialog::OnBtnRangeIonAdd)
    EVT_BUTTON(wxID_REMOVE, RangeEditorDialog::OnBtnRangeIonRemove)
    EVT_CHECKLISTBOX(ID_LIST_OVERLAY, RangeEditorDialog::OnListOverlayCheck)
    EVT_BUTTON(wxID_OK, RangeEditorDialog::OnBtnOK)
    EVT_BUTTON(wxID_CANCEL, RangeEditorDialog::OnBtnCancel)
    EVT_SPLITTER_DCLICK(ID_SPLIT_LEFTRIGHT, RangeEditorDialog::OnSashVerticalDClick)
    // end wxGlade
END_EVENT_TABLE();

void RangeEditorDialog::getModifiedRanges(map<const RangeFile *, const RangeFile *> &modRanges) const
{
	//modRanges.reserve(modifiedRanges.size());

	for(RFMAP::const_iterator it = modifiedRanges.begin() ;
			it!= modifiedRanges.end(); ++it)
	{
		const RangeFile *r = &(it->first->getRange());
		modRanges[r]=&(it->second);
	}
}


void RangeEditorDialog::onPlotUpdate()
{
#ifdef DEBUG
	size_t lastEditedRegion,lastEditedPlot;
	plotPanel->getLastEdited(lastEditedPlot,lastEditedRegion);

	ASSERT(lastEditedRegion!= -1);
	ASSERT(lastEditedPlot != -1);
#endif

	generateRangeEntries();
	generateIonEntries();

	setRangeReady();	
}



void RangeEditorDialog::setPlotWrapper(const PlotWrapper  &p)
{
	plotWrap = p;


	plotWrap.setEnableHighlightOverlap();
   

	//Find all unique ranges
	//--
	vector<pair<size_t,vector<PlotRegion> > > regions;
	plotWrap.getRegions(regions,false);


	std::set<const RangeFileFilter *> ranges;
	for(size_t ui=0;ui<regions.size();ui++)
	{
		//Region data is actually empty.
		if(regions[ui].second.empty())
		{
			//ignore this plot
			ignoreList.insert(regions[ui].first);
			continue;
		}
		
		const Filter *parentFilt;
		parentFilt = regions[ui].second[0].getParentAsFilter();

		if(parentFilt->getType() != FILTER_TYPE_RANGEFILE)
		{
			//ignore this plot
			ignoreList.insert(regions[ui].first);
			continue;
		}
		
		//Remember that we need to (shortly) create a new rangefile for this
		ranges.insert((const RangeFileFilter *)parentFilt);
		//Create a mapping between the plot and its owned rangefile
		plotToRangeFileMap[regions[ui].first]=(const RangeFileFilter*)parentFilt;
	}
	//--

	//create a copy of the range file that are the to-be-modified ranges
	for(set<const RangeFileFilter *>::const_iterator it=ranges.begin();
		it != ranges.end(); ++it)
	{
		modifiedRanges[*it]=((*it)->getRange());
		modifiedRanges[*it].setEnforceConsistent(false);
	}

	//Now, change the behaviour of region updating for the plot wrapper
	// to update our new rangefile objects


	plotWrap.switchOutRegionParent(modifiedRanges);

	//Set the plot panel to use the appropriate plot wrapper
	plotPanel->setPlotWrapper(&plotWrap,false);

	//Generate the list entries
	generateListEntries();
	
	setCurrentRange();
	
	//Find all the plots in the wrapper, and add them to the list
	generateIonEntries();
	generateRangeEntries();

	//Hack to ensure we select something at startup
	// only if there is nothing selected, and the plot list has items
	if(currentRange && !plotPanel->getNumVisible() && 
				listPlots->GetCount() > 0)
	{
		{
			ASSERT(listPlots->GetSelection() !=wxNOT_FOUND);
	
			unsigned int plotID;
			plotID=listToPlotIDs[listPlots->GetSelection()];
			plotWrap.setVisible(plotID);

	
			plotPanel->Refresh();
		}
	}
}


void RangeEditorDialog::setCurrentRange(size_t forceSelected)
{
	//IF we have no plots,
	// we cannot have any current range
	if(!listPlots->GetCount())
	{
		currentRange=0;
		return;
	}

	//Get the currently selected plot
	unsigned int curPlotID;
	if(forceSelected == (size_t) -1)
	{
		ASSERT(listPlots->GetCount());
		

		int selectedItem=listPlots->GetSelection();

		if(selectedItem== wxNOT_FOUND )
		{
			currentRange=0;
			return;
		}

		curPlotID=listToPlotIDs[selectedItem];
	}
	else
	{
		curPlotID=listToPlotIDs[forceSelected];
	}
	


	//If we don't have a new range for this plot, alter
	// the parent of any regions in the plot
	if(plotNewRanges.find(curPlotID) == plotNewRanges.end())
	{
		ASSERT(plotToRangeFileMap.find(curPlotID) != plotToRangeFileMap.end());

		currentRange=&(modifiedRanges[plotToRangeFileMap[curPlotID]]);
	}
	else
	{
		//Either create a new, or set the old Rangefile to assign to this plot
		currentRange=&(plotNewRanges[curPlotID]);
	}
}

void RangeEditorDialog::generatePlotRegions()
{
	RegionGroup r;

	if(!currentRange)
		return;

	//Go through each entry in the current range, and create a
	// region in the plot that  corresponds to it
	for(size_t ui=0;ui<currentRange->getNumRanges();ui++)
	{
		PlotRegion p(PlotRegion::ACCESS_MODE_RANGEFILE,currentRange);
		RGBf col;

		//set region colour
		col = currentRange->getColour(currentRange->getIonID((unsigned int)ui));
		p.r = col.red;
		p.g = col.green;
		p.b = col.blue;

		p.id = ui;

		p.bounds.clear();
		p.bounds.push_back(currentRange->getRange(ui));

		r.regions.push_back(p);

	}

	ASSERT(listPlots->GetSelection() != -1);

	//Send the current range data to the current plot
	unsigned int plotID;
	plotID = listToPlotIDs[listPlots->GetSelection()]; 

	//reassign new region group
	plotWrap.setRegionGroup(plotID,r);

	//Update plot
	plotPanel->Refresh();	
}

void RangeEditorDialog::generateListEntries() 
{
	programmaticEvent=true;
	vector<unsigned int> plotIDs;
	plotWrap.getPlotIDs(plotIDs);

	listPlots->Freeze();

	listPlots->Clear();
	//Add the plots that the user can get
	for(size_t ui=0;ui<plotIDs.size();ui++)
	{
		unsigned int plotID;
		plotID = plotIDs[ui];

		std::string title;
		title=plotWrap.getTitle(plotID);

		//Only use plots from spectra
		if(plotWrap.getParentType(plotID) != FILTER_TYPE_SPECTRUMPLOT || ignoreList.find(plotID) != ignoreList.end())
			continue;	

		//Append the plot to the list in the user interface,
		// with the plot Id embedded in the element
		int idx;
		idx=listPlots->Append((title));
		listToPlotIDs[idx]=plotIDs[ui];
	}

	//If there is only one spectrum, select it
	if(listPlots->GetCount() >= 1 )
		listPlots->SetSelection(0);

	listPlots->Thaw();
	programmaticEvent=false;
}

void RangeEditorDialog::generateOverlayList(const vector<OVERLAY_DATA> &overlays) 
{

	//Build the list of enabled overlays
	listOverlay->Clear();


	for(size_t ui=0;ui<overlays.size();ui++)
	{
		listOverlay->Insert((overlays[ui].title),ui);
		listOverlay->Check(ui,overlays[ui].enabled);
	}

}

void RangeEditorDialog::generateIonEntries(size_t rowVisibleHint)
{
	programmaticEvent=true;
	//Withhold drawing updates until we are done.
	gridIons->Freeze();

	int viewStartX,viewStartY;
	gridIons->GetViewStart(&viewStartX,&viewStartY);

	//Reset the ion grid
	//--
	if(gridIons->GetNumberCols())
		gridIons->DeleteCols(0,gridIons->GetNumberCols());
	if(gridIons->GetNumberRows())
		gridIons->DeleteRows(0,gridIons->GetNumberRows());
	
	gridIons->AppendCols(4);
	gridIons->SetColLabelValue(ION_COL_PLOT,TRANS("Plot"));
	gridIons->SetColLabelValue(ION_COL_SHORTNAME,TRANS("Short Name"));
	gridIons->SetColLabelValue(ION_COL_LONGNAME,TRANS("Long Name"));
	gridIons->SetColLabelValue(ION_COL_COLOUR,TRANS("Colour"));
	//--
	
	
	gridIonIds.clear();
	
	
	//Get the currently selected plot
	int curPlot=listPlots->GetSelection();

	//If no plot slected, abort update
	if(curPlot == (unsigned int) wxNOT_FOUND || !currentRange)
	{
		gridIons->Thaw();
		programmaticEvent=false;
		return;
	}
	
	
	std::string title;
	title=plotWrap.getTitle(curPlot);

	//Colour to  use for incomplete ions/ranges
	wxColour incomplColour;
	//A light blue colour
	incomplColour.Set(162,162,255);
	
	
	size_t curIonRow=0;
	
	//Fill in the ion grid
	gridIons->AppendRows(currentRange->getNumIons());
	for(size_t uj=0;uj<currentRange->getNumIons();uj++)
	{
		//Set the ionID from the row
		gridIonIds[uj]=curIonRow;

		gridIons->SetCellValue(curIonRow,
				ION_COL_PLOT,(title));

		gridIons->SetCellValue(curIonRow,
				ION_COL_SHORTNAME,(currentRange->getName(uj)));
		gridIons->SetCellValue(curIonRow,
				ION_COL_LONGNAME,(currentRange->getName(uj,false)));
		
		//set the colour
		wxGridCellAttr *attr = gridIons->GetOrCreateCellAttr(curIonRow,ION_COL_COLOUR);
		RGBf col=currentRange->getColour(uj);

		unsigned char r,g,b,a;
		r=col.red*255.0f;
		g=col.green*255.0f;
		b=col.blue*255.0f;
		a=255;
		attr->SetBackgroundColour(wxColour(r,g,b,a));
		attr->DecRef();

		curIonRow++;
	}
	
	//Add the incomplete ions
	incompleteIonOffset=currentRange->getNumIons();
	gridIons->AppendRows(incompleteIons.size());
	for(size_t ui=0;ui<incompleteIons.size();ui++)
	{
		gridIons->SetCellValue(curIonRow,
				ION_COL_PLOT,(title));

		gridIons->SetCellValue(curIonRow,
				ION_COL_SHORTNAME,(incompleteIons[ui].getShortName()));
		gridIons->SetCellValue(curIonRow,
				ION_COL_LONGNAME,(incompleteIons[ui].getLongName()));
		
		//set the colour
		wxGridCellAttr *attr = gridIons->GetOrCreateCellAttr(curIonRow,ION_COL_COLOUR);
		RGBf col=incompleteIons[ui].getColour();

		unsigned char r,g,b,a;
		r=col.red*255.0f;
		g=col.green*255.0f;
		b=col.blue*255.0f;
		a=255;
		attr->SetBackgroundColour(wxColour(r,g,b,a));
		attr->DecRef();


		for(size_t uj=0;uj<RNG_COL_ENUM_END; uj++)
		{
			if(uj == ION_COL_COLOUR)
				continue;
			gridIons->SetCellBackgroundColour(curIonRow,uj,incomplColour);
		}
		curIonRow++;
	}
	gridIons->Scroll(viewStartX,viewStartY);

	if(rowVisibleHint!=(size_t)-1)
	{
		ASSERT(rowVisibleHint < gridIons->GetNumberRows());
		gridIons->MakeCellVisible(rowVisibleHint,0);
	}

	gridIons->Thaw();
	programmaticEvent=false;
}

void RangeEditorDialog::generateRangeEntries(size_t rowVisibleHint)
{
	programmaticEvent=true;
	gridRanges->Freeze();


	int viewStartX,viewStartY;
	gridRanges->GetViewStart(&viewStartX,&viewStartY);

	vector<pair<size_t,vector<PlotRegion> > > regions;
	plotWrap.getRegions(regions);

	//OK, so we have each plot and the regions it contains
	//lets filter that down to the regions we can actually see

	
	
	//Reset the range grid
	//---
	if(gridRanges->GetNumberCols())
		gridRanges->DeleteCols(0,gridRanges->GetNumberCols());
	if(gridRanges->GetNumberRows())
		gridRanges->DeleteRows(0,gridRanges->GetNumberRows());
	
	gridRanges->AppendCols(4);
	gridRanges->SetColLabelValue(0,TRANS("Plot"));
	gridRanges->SetColLabelValue(1,TRANS("Ion"));
	gridRanges->SetColLabelValue(2,TRANS("Start"));
	gridRanges->SetColLabelValue(3,TRANS("End"));
	//---

	gridRangeIds.clear();
	
	
	//Get the currently selected plot
	int curPlot=listPlots->GetSelection();

	//If no plot slected, abort update
	if(curPlot == (unsigned int) wxNOT_FOUND || !currentRange)
	{
		gridRanges->Thaw();
		return;
	}


	//Colour to  use for incomplete ions/ranges
	wxColour incomplColour;
	//A light blue colour
	incomplColour.Set(162,162,255);
	std::string title;
	title=plotWrap.getTitle(curPlot);
	
	//Fill in the range grid
	size_t curRangeRow=0;
	gridRanges->AppendRows(currentRange->getNumRanges());
	for(size_t ui=0;ui<currentRange->getNumRanges();ui++)
	{
		gridRangeIds[ui]=curRangeRow;
		pair<float,float> rangeBound;
		rangeBound=currentRange->getRange(ui);

		std::string ionName;
		ionName=currentRange->getName(currentRange->getIonID((unsigned int)ui));

		gridRanges->SetCellValue(curRangeRow,RNG_COL_PLOT,(title));
		gridRanges->SetCellValue(curRangeRow,RNG_COL_PARENT_ION,(ionName));

		std::string tmpStr;
		stream_cast(tmpStr,rangeBound.first);
		gridRanges->SetCellValue(curRangeRow,RNG_COL_START,(tmpStr));
		stream_cast(tmpStr,rangeBound.second);
		gridRanges->SetCellValue(curRangeRow,RNG_COL_END,(tmpStr));

		curRangeRow++;
	}
	
	//Add the pending rows
	gridRanges->AppendRows(incompleteRanges.size());
	incompleteRangeOffset=currentRange->getNumRanges();


	for(size_t ui=0;ui<incompleteRanges.size();ui++)
	{
		gridRangeIds[ui]=curRangeRow;
	
		std::string ionName;
		ionName=incompleteRanges[ui].getIonName();

		gridRanges->SetCellValue(curRangeRow,RNG_COL_PLOT,(title));
		gridRanges->SetCellValue(curRangeRow,RNG_COL_PARENT_ION,(ionName));

		std::string tmpStr;
		stream_cast(tmpStr,incompleteRanges[ui].getStart());
		gridRanges->SetCellValue(curRangeRow,RNG_COL_START,(tmpStr));
		stream_cast(tmpStr,incompleteRanges[ui].getEnd());
		gridRanges->SetCellValue(curRangeRow,RNG_COL_END,(tmpStr));

		for(size_t uj=0;uj<RNG_COL_ENUM_END; uj++)
			gridRanges->SetCellBackgroundColour(curRangeRow,uj,incomplColour);

		curRangeRow++;
	}
	

	gridRanges->Scroll(viewStartX,viewStartY);
	
	if(rowVisibleHint!=(size_t)-1)
	{
		ASSERT(rowVisibleHint < gridRanges->GetNumberRows());
		gridRanges->MakeCellVisible(rowVisibleHint,0);
	}

	gridRanges->Thaw();
	programmaticEvent=false;
}

void RangeEditorDialog::OnListPlots(wxCommandEvent &event)
{
	if(programmaticEvent)
		return;


	setCurrentRange(event.GetSelection());
	
	plotWrap.hideAll();

	//Set the plot visibility for each plot
	for(unsigned int ui=0;ui<listPlots->GetCount(); ui++)
	{
		unsigned int plotID;
		plotID = listToPlotIDs[ui];

		//Set the plot visibility to match selection
		plotWrap.setVisible(plotID,listPlots->IsSelected(ui));
	}

	plotPanel->Refresh();
}

void RangeEditorDialog::setRangeReady()
{
	bool isReady=true;
	for(RFMAP::const_iterator it=modifiedRanges.begin(); 
				it!=modifiedRanges.end(); ++it)
	{
		if(!it->second.isSelfConsistent() )
		{
			isReady=false;
			break;
		}

	}

	
	btnOK->Enable(isReady);
}

void RangeEditorDialog::OnGridRangesEditorShown(wxGridEvent &event)
{
    event.Skip();
    wxLogDebug(wxT("Event handler (RangeEditorDialog::OnGridRangesEditorShown) not implemented yet")); //notify the user that he hasn't implemented the event handler yet
}

void RangeEditorDialog::OnGridIonsEditorShown(wxGridEvent &event)
{
	if(event.GetRow() < incompleteIonOffset)
	{
		//We are editing a regular ion, not an incomplete ion
		
		size_t ionId=gridIonIds[event.GetRow()];

		ASSERT(ionId < currentRange->getNumIons());
		switch(event.GetCol())
		{
			case ION_COL_PLOT:
				//Can't edit this column
				event.Veto();
				break;
			case ION_COL_COLOUR:
			{
				//Pop up a dialog asking for colour input
				RGBf rgbf=currentRange->getColour(ionId);

				wxColourData d;
				d.SetColour(wxColour((unsigned char)(rgbf.red*255),
							(unsigned char)(rgbf.green*255),
							(unsigned char)(rgbf.blue*255),
							(unsigned char)(255)));
				wxColourDialog *colDg=new wxColourDialog(this,&d);

				//Check to see if user actually put in a colour
				if( colDg->ShowModal() != wxID_OK)
				{
					event.Veto();
					delete colDg;
					return;
				}

				//Update the colour data in the range
				wxColour c;
				c=colDg->GetColourData().GetColour();

				rgbf.red=c.Red()/255.0f;
				rgbf.green=c.Green()/255.0f;
				rgbf.blue=c.Blue()/255.0f;

				currentRange->setColour(ionId,rgbf);
				
				//Change the colour in the grid
				wxGridCellAttr *attr = gridIons->GetOrCreateCellAttr(
								event.GetRow(),ION_COL_COLOUR);
				attr->SetBackgroundColour(c);
				
				delete colDg;

				//We have to veto the edit event so that  the user
				// doesn't get a text box to type into
				event.Veto();
		
				
				//We need to update the plot regions, as they
				// will have changed colour
				generatePlotRegions();
				break;
			}
			case ION_COL_SHORTNAME:
			case ION_COL_LONGNAME:
			{
				//Nothing to do until edit is complete
				break;	
			}
			default:
				ASSERT(false);
		}
	}
	else
	{
		size_t delta=event.GetRow() - incompleteIonOffset;
		switch(event.GetCol())
		{
			case ION_COL_LONGNAME:
			case ION_COL_SHORTNAME:
				event.Skip();
				break;
			case ION_COL_PLOT:
				event.Veto();
				break;
			case ION_COL_COLOUR:
			{
				RGBf rgbf=incompleteIons[delta].getColour();

				wxColourData d;
				d.SetColour(wxColour((unsigned char)(rgbf.red*255),
							(unsigned char)(rgbf.green*255),
							(unsigned char)(rgbf.blue*255),
							(unsigned char)(255)));
				wxColourDialog *colDg=new wxColourDialog(this,&d);

				if( colDg->ShowModal() != wxID_OK)
				{
					delete colDg;
					return;
				}

				wxColour c;
				//Change the colour
				c=colDg->GetColourData().GetColour();

				rgbf.red=c.Red()/255.0f;
				rgbf.green=c.Green()/255.0f;
				rgbf.blue=c.Blue()/255.0f;

				incompleteIons[delta].setColour(rgbf);
					
				delete colDg;
			
				//Check to see if the incomplete ion is now done
				// if so, commit it to the rangefile
				if(incompleteIons[delta].isFinished())
				{
					incompleteIons[delta].commit();
			
					std::swap(incompleteIons[delta],incompleteIons.back());
					incompleteIons.pop_back();

					generateIonEntries();
										
				}
				else
				{
					unsigned char r,g,b,a;
					r=rgbf.red*255.0f;
					g=rgbf.green*255.0f;
					b=rgbf.blue*255.0f;
					a=255;

					gridIons->SetCellBackgroundColour(event.GetRow(),
						ION_COL_COLOUR,wxColour(r,g,b,a));

				}


				event.Veto();
				
				break;
			}
			default:
				ASSERT(false);
		}
		
	}

	setRangeReady();
}

void RangeEditorDialog::OnGridRangesCellChange(wxGridEvent &event)
{

	if(programmaticEvent)
		return;

	programmaticEvent=true;
	

	std::string newContent;
	newContent=stlStr(gridRanges->GetCellValue(event.GetRow(),event.GetCol()));

	
	if(event.GetRow() >= incompleteRangeOffset)
	{
		//Process an incomplete range in the grid 
		size_t delta=event.GetRow() - incompleteRangeOffset;
		
		RangeFile &r = *(incompleteRanges[delta].getRangePtr());

		switch(event.GetCol())
		{
			case RNG_COL_PLOT:
				break;
			case RNG_COL_PARENT_ION:
			{
				//Check to see if we have the ion name already
				unsigned int newID = r.getIonID(newContent);
				if( newID== (unsigned int)-1)
				{
					event.Veto();
					programmaticEvent=false;
					return;
				}

				incompleteRanges[delta].setParentId(newID);
				break;
			}
			case RNG_COL_START:
			{
				float tmp;
				if(stream_cast(tmp,newContent))
				{
					event.Veto();
					programmaticEvent=false;
					return;
				}
				incompleteRanges[delta].setStart(tmp);
				break;
			}
			case RNG_COL_END:
			{
				float tmp;
				if(stream_cast(tmp,newContent))
				{
					event.Veto();
					programmaticEvent=false;
					return;
				}
				incompleteRanges[delta].setEnd(tmp);
				break;
			}
			default:
			{
				ASSERT(false);
			}
		}
		
		//if the  range is complete, add it
		if(incompleteRanges[delta].isFinished())
		{
			incompleteRanges[delta].commit();
	
			std::swap(incompleteRanges[delta],incompleteRanges.back());
			incompleteRanges.pop_back();

								
		}

		generateRangeEntries();

		//Re-generate the plot regions, as they have been changed now
		generatePlotRegions();


		programmaticEvent=false;
		return;
	}

	//We have a pre-existing range that is being modified

	//Find out which range was latered
	size_t rangeId;
	rangeId=gridRangeIds[event.GetRow()];
	
	switch(event.GetCol())
	{
		case RNG_COL_PLOT:
			break;
		case RNG_COL_PARENT_ION:
		{
			unsigned int newID = currentRange->getIonID(newContent);
			if( newID== (unsigned int)-1)
			{
				event.Veto();
				programmaticEvent=false;
				return;
			}

			currentRange->setIonID(rangeId,newID);
			break;
		}
		case RNG_COL_START:
		{
			float f;
			if(stream_cast(f,newContent))
			{
				event.Veto();
				programmaticEvent=false;
				return;
			}
			
			//Disallow inversion of range start/end
			if(f >=currentRange->getRange(rangeId).second)
			{
				event.Veto();
				programmaticEvent=false;
				return;
			}
			currentRange->setRangeStart(rangeId,f);

			break;
		}
		case RNG_COL_END:
		{
			float f;
			if(stream_cast(f,newContent))
			{
				event.Veto();
				programmaticEvent=false;
				return;
			}
			
			//Disallow inversion of range start/end
			if(f <=currentRange->getRange(rangeId).first)
			{
				event.Veto();
				programmaticEvent=false;
				return;
			}
			currentRange->setRangeEnd(rangeId,f);
			break;
		}

		default:
			ASSERT(false);
	}

	//Re-generate the altered plot regions
	generatePlotRegions();

	programmaticEvent=false;
	
	setRangeReady();
}

void RangeEditorDialog::OnGridIonsCellChange(wxGridEvent &event)
{
	if(programmaticEvent)
		return;

	std::string newContent;
	newContent=stlStr(gridIons->GetCellValue(event.GetRow(),event.GetCol()));
	
	if(event.GetRow()>= incompleteIonOffset)
	{
		size_t delta=event.GetRow() - incompleteIonOffset;
		
		PendingIon &p = incompleteIons[delta];
		switch(event.GetCol())
		{
			case ION_COL_PLOT:
				//Can't edit this column
				event.Veto();
				break;
			case ION_COL_COLOUR:
				//Already handled
				break;
			case ION_COL_SHORTNAME:
			{
				//Check to see if the name already exists, if it does, then veto the event
				if(p.getRangePtr()->getIonID(newContent.c_str()) == (unsigned int) - 1)
					p.setShortName(newContent);				
				else
					event.Veto();
				break;	
			}
			case ION_COL_LONGNAME:
			{
				//Check to see if the name already exists, if it does, then veto the event
				if(p.getRangePtr()->getIonID(newContent.c_str(),false) == (unsigned int) - 1)
					p.setLongName(newContent);				
				else
					event.Veto();
				break;	
			}
			default:
				ASSERT(false);
		}
		
		//if the  range is complete, add it
		if(incompleteIons[delta].isFinished())
		{
			incompleteIons[delta].commit();

			std::swap(incompleteIons[delta],incompleteIons.back());
			incompleteIons.pop_back();

			generateIonEntries();

		}

		return;
	}

	//Find out which ion was latered
	size_t ionId;
	ionId=gridIonIds[event.GetRow()];
	

	switch(event.GetCol())
	{
		case ION_COL_COLOUR:
			//already handled on cell editor shown- do nothing
			break;
		case ION_COL_SHORTNAME:
			currentRange->setIonShortName(ionId,newContent);
			break;
		case ION_COL_LONGNAME:
			currentRange->setIonLongName(ionId,newContent);
			break;
		case ION_COL_PLOT:
			//veto
			event.Veto();
			break;
		default:
			ASSERT(false);
	}
	
	setRangeReady();
}

void RangeEditorDialog::OnGridRangeClick(wxGridEvent &cmd)
{
	lastFocused=GRID_FOCUS_RANGES;
	cmd.Skip();
}

void RangeEditorDialog::OnGridIonClick(wxGridEvent &cmd)
{
	lastFocused=GRID_FOCUS_IONS;
	cmd.Skip();
}

void RangeEditorDialog::OnBtnRangeIonAdd(wxCommandEvent &event)
{
	//If there are no grids then the user cannot focus them,
	// so in this case, give the user the option of selecting what action to take
	if(lastFocused == GRID_FOCUS_NONE || 
		!gridRanges->GetNumberRows() || !gridIons->GetNumberRows())
	{
		wxArrayString wxStrs;
		wxStrs.Add(("Ion"));
		wxStrs.Add(("Range"));

		wxSingleChoiceDialog *wxD = new wxSingleChoiceDialog(this, TRANS("Range or ion?"),
				TRANS("Select type to add"),wxStrs,(void **)NULL,wxDEFAULT_DIALOG_STYLE|wxOK|wxCENTRE);

		wxD->ShowModal();

		if(wxD->GetSelection() == 0)
			lastFocused=GRID_FOCUS_IONS;
		else
			lastFocused=GRID_FOCUS_RANGES;

	}

	//Update either the range or ion grid with a new pending item.
	ASSERT(currentRange);
	switch(lastFocused)
	{
		case GRID_FOCUS_RANGES:
		{
			incompleteRanges.push_back(PendingRange(currentRange));

			size_t visibleRowHint=gridRanges->GetNumberRows();
			generateRangeEntries(visibleRowHint);
			break;
		}
		case GRID_FOCUS_IONS:
		{
			incompleteIons.push_back(PendingIon(currentRange));
			
			size_t visibleRowHint=gridIons->GetNumberRows();
			generateIonEntries(visibleRowHint);
			break;
		}
		default:
			ASSERT(false);
	}
	

	setRangeReady();
}


void RangeEditorDialog::OnBtnRangeIonRemove(wxCommandEvent &event)
{
	switch(lastFocused)
	{
		case GRID_FOCUS_RANGES:
		{
			size_t row=gridRanges->GetGridCursorRow();
			//TODO: Better selection handling
			// - grids are notoriously bad at selection in wx
			if(!gridRanges->GetNumberRows() || row == wxNOT_FOUND)
				break;
			
			if(row  < incompleteRangeOffset)
			{
				//Find the mapping between the range, then kill it
				size_t rangeId = gridRangeIds[row];
				//Kill the range
				currentRange->eraseRange(rangeId);
			}
			else
			{
				size_t delta=row - incompleteRangeOffset;
				//Remove the pending range
				std::swap(incompleteRanges[delta],
						incompleteRanges.back());
				incompleteRanges.pop_back();
			}
			//relayout the grid
			generateRangeEntries();	
	
			//Update plot
			generatePlotRegions();
			break;
		}
		case GRID_FOCUS_IONS:
		{
			size_t row=gridIons->GetGridCursorRow();
			//TODO: Better selection handling
			// - grids are notoriously bad at selection in wx
			if(!gridIons->GetNumberRows() || row == wxNOT_FOUND)
				break;
				
			if(row  < incompleteIonOffset)
			{
				//Find the mapping between the range, then kill it
				size_t ionId = gridIonIds[row];

				//Kill the ion, and any of its ranges
				currentRange->eraseIon(ionId);
			}
			else
			{
				size_t delta=row - incompleteIonOffset;
				//Remove the pending range
				std::swap(incompleteIons[delta],
						incompleteIons.back());
				incompleteIons.pop_back();
			}
			
			//relayout the grids, then update plot
			generateIonEntries();	
			generateRangeEntries();
			generatePlotRegions();

			break;
		}
		case GRID_FOCUS_NONE:
		{
			break;
		}
		default:
			ASSERT(false);
	}

	setRangeReady();
}


void RangeEditorDialog::OnCheckShowOverlay(wxCommandEvent &event)
{
	plotWrap.overlays.setEnabled(event.IsChecked());
	plotPanel->Refresh();
}



void RangeEditorDialog::OnBtnOK(wxCommandEvent &event)
{
	EndModal(wxID_OK);
}


void RangeEditorDialog::OnBtnCancel(wxCommandEvent &event)
{
	EndModal(wxID_CANCEL);
}

void RangeEditorDialog::OnListOverlayCheck(wxCommandEvent &event)
{
	long index=event.GetInt();

	bool isChecked;
	isChecked=listOverlay->IsChecked(index);

	plotWrap.overlays.setEnabled(index,isChecked);
	plotPanel->Refresh();
}

void RangeEditorDialog::OnListOverlayKeyDown(wxListEvent &event)
{
	//Check for delete key
	if(event.GetKeyCode() != WXK_DELETE)
		return;

	long index=event.GetIndex();
	plotWrap.overlays.erase(index);

	generateOverlayList(plotWrap.overlays.getOverlays());
	plotPanel->Refresh();
}

void RangeEditorDialog::OnSashVerticalDClick(wxSplitterEvent &event)
{
	event.Veto();
}

void RangeEditorDialog::OnTextOverlay(wxCommandEvent &event)
{

	std::string compoundString;
	compoundString=stlStr(textOverlayCmpnt->GetValue());
	vector<pair<string,size_t> > ionFragments;
	if(RangeFile::decomposeIonNames(compoundString,ionFragments))
	{
	    textOverlayCmpnt->SetDefaultStyle(wxTextAttr(*wxBLUE));
	}
	else
	{
	    textOverlayCmpnt->SetDefaultStyle(wxTextAttr(wxNullColour));
	}
}

void RangeEditorDialog::OnTextOverlaySetFocus(wxFocusEvent &event)
{
	if(!haveSetTextFocus)
	{
		haveSetTextFocus=true;
		textOverlayCmpnt->SetValue(wxT(""));
	}
	else 
		event.Skip();
}

void RangeEditorDialog::OnTextOverlayEnter(wxCommandEvent &event)
{
	//Obtain the user input for the text control
	std::string compoundString;
	compoundString = stlStr(textOverlayCmpnt->GetValue());

	//break the users' input into 
	vector<pair<string,size_t> > ionFragments;
	if(!RangeFile::decomposeIonNames(compoundString,ionFragments))
		return;

	//Check to see if each component has a matching symbol for all elements
	//----
	bool symbolsFound;
	vector<size_t> indicies;
	vector<string> symbols;

	//Get the indices for each gragment
	symbols.resize(ionFragments.size());
	for(size_t ui=0;ui<symbols.size();ui++)
		symbols[ui]=ionFragments[ui].first;

	abundanceData.getSymbolIndices(symbols,indicies);

	//Ensure there are no bad indices
	symbolsFound=(std::find(indicies.begin(),indicies.end(),(size_t)-1) == indicies.end());

	if(!symbolsFound)
	{
		textOverlayCmpnt->SetBackgroundColour(wxColour(*wxCYAN));
		return;
	}
	textOverlayCmpnt->SetBackgroundColour(wxNullColour);
	//----

	//Get the intensity distribution
	//----
	vector<size_t> fragmentCount;
	size_t totalFragments=0;
	fragmentCount.resize(ionFragments.size());
	for(size_t ui=0;ui<ionFragments.size();ui++)
	{
		fragmentCount[ui]=(float)ionFragments[ui].second;
		totalFragments+=fragmentCount[ui];
	}

	//Limit the number of fragments allowable, as the total number
	// of combinations is (species)^fragmentCount. Eg C2 has two
	// species, 12^C and 13^C, and with 10 items, you get 2^10 combinations
	size_t MAX_FRAGMENT_COUNT=10;
	if(totalFragments > MAX_FRAGMENT_COUNT)
	{
		textOverlayCmpnt->SetBackgroundColour(wxColour(*wxCYAN));
		return;
	}

	//Number of times to "fold" the intensity distribution
	const size_t MAX_FOLD_VALUE=3;

	
	OVERLAY_DATA overlay;
	overlay.title=compoundString;
	overlay.enabled=true;
	for(size_t ui=0;ui<MAX_FOLD_VALUE;ui++)
	{
		vector<pair<float,float> > massDist;
		abundanceData.generateIsotopeDist(indicies,fragmentCount,massDist,ui+1);

		for(size_t uj=0;uj<massDist.size();uj++)
			overlay.coordData.push_back(massDist[uj]);
	}
	//-----


	//Add to the list of components that can be disabled/enabled
	plotWrap.overlays.add(overlay);

	generateOverlayList(plotWrap.overlays.getOverlays());

	plotPanel->Refresh();
}

// wxGlade: add RangeEditorDialog event handlers


void RangeEditorDialog::set_properties()
{
    // begin wxGlade: RangeEditorDialog::set_properties
    SetTitle(TRANS("Range Editor"));
    gridRanges->CreateGrid(0, 3);
    gridIons->CreateGrid(0, 3);

    checkShowOverlay->SetToolTip(TRANS("Enable or disable all overlays"));
    listOverlay->SetToolTip(TRANS("Entered overlays, use delete to remove"));
    listPlots->SetToolTip(TRANS("Available plots for ranging"));
    textOverlayCmpnt->SetToolTip(TRANS("Enter species to display as overlay, e.g. SiO2"));
    gridRanges->SetToolTip(TRANS("Editable ranges"));
    gridIons->SetToolTip(TRANS("Editable ions"));
    // end wxGlade
}


void RangeEditorDialog::do_layout()
{
    // begin wxGlade: RangeEditorDialog::do_layout
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerRight = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerBottom = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerNote = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerOverlayPane = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerOverlay = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerOverlayContainer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerOverlayLeft = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerRanges = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerRangeBottom = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerPlotList = new wxBoxSizer(wxVERTICAL);
    sizerPlotList->Add(listPlots, 1, wxEXPAND, 0);
    noteLeftPlots->SetSizer(sizerPlotList);
    sizerRanges->Add(gridIons, 1, wxALL|wxEXPAND, 4);
    sizerRanges->Add(gridRanges, 1, wxALL|wxEXPAND, 4);
    sizerRangeBottom->Add(20, 20, 1, 0, 0);
    sizerRangeBottom->Add(btnRangeIonAdd, 0, wxALL, 4);
    sizerRangeBottom->Add(btnRangeIonRemove, 0, wxALL, 4);
    sizerRanges->Add(sizerRangeBottom, 0, wxALL|wxEXPAND, 4);
    noteLeftRanges->SetSizer(sizerRanges);
    sizerOverlay->Add(checkShowOverlay, 0, wxALL, 5);
    sizerOverlayLeft->Add(textOverlayCmpnt, 0, wxEXPAND, 0);
    sizerOverlayContainer->Add(sizerOverlayLeft, 0, wxALL|wxEXPAND, 2);
    sizerOverlayContainer->Add(listOverlay, 1, wxEXPAND, 0);
    sizerOverlay->Add(sizerOverlayContainer, 1, wxEXPAND, 0);
    sizerOverlayPane->Add(sizerOverlay, 1, wxEXPAND, 0);
    noteLeftOverlay->SetSizer(sizerOverlayPane);
    notebookLeft->AddPage(noteLeftPlots, TRANS("Plots"));
    notebookLeft->AddPage(noteLeftRanges, TRANS("Ranges"));
    notebookLeft->AddPage(noteLeftOverlay, TRANS("Overlay"));
    sizerNote->Add(notebookLeft, 1, wxEXPAND, 0);
    panelSplitLeft->SetSizer(sizerNote);
    sizerRight->Add(plotPanel, 1, wxEXPAND, 0);
    sizerBottom->Add(20, 20, 1, 0, 0);
    sizerBottom->Add(btnOK, 0, wxALL, 4);
    sizerBottom->Add(btnCancel, 0, wxALL, 4);
    sizerRight->Add(sizerBottom, 0, wxRIGHT|wxEXPAND|wxALIGN_CENTER_HORIZONTAL, 4);
    panelSplitRight->SetSizer(sizerRight);
    splitVertical->SplitVertically(panelSplitLeft, panelSplitRight);
    topSizer->Add(splitVertical, 1, wxEXPAND, 0);
    SetSizer(topSizer);
    topSizer->Fit(this);
    Layout();
    // end wxGlade
}

