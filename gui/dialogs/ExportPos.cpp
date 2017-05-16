/*
 *	ExportPos.cpp  - POS file export dialog implementation
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


#include "ExportPos.h"
#include "wx/wxcommon.h"

#include "wx/wxcomponents.h"
#include "common/translation.h"

#include <wx/treectrl.h>

// begin wxGlade: ::extracode

// end wxGlade

wxWindow *exportPosYieldWindow=0;
bool abortOp;
wxStopWatch *exportPosDelayTime=0;


using std::list;
using std::pair;
using std::string;
using std::vector;


enum
{
	ID_BTN_ADDDATA=wxID_ANY+1,
	ID_BTN_ADDNODE,
	ID_BTN_ADDALL,
	ID_TREE_FILTERS,
	ID_LIST_SELECTED,
	ID_LIST_AVAILABLE,
	ID_RADIO_VISIBLE,
	ID_RADIO_SELECTION,
};


ExportPosDialog::ExportPosDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)

{
	haveRefreshed=false;
	exportVisible=true;
	// begin wxGlade: ExportPosDialog::ExportPosDialog
	lblExport = new wxStaticText(this, wxID_ANY, TRANS("Export:"));
	radioVisible = new wxRadioButton(this,ID_RADIO_VISIBLE , TRANS("Visible"));
	radioSelection = new wxRadioButton(this,ID_RADIO_SELECTION , TRANS("Selected Data"));
	treeData = new wxTreeCtrl(this, ID_TREE_FILTERS, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS|wxTR_NO_LINES|wxTR_HIDE_ROOT|wxTR_DEFAULT_STYLE|wxSUNKEN_BORDER);
	lblAvailableData = new wxStaticText(this, wxID_ANY, TRANS("Available Data"));
	listAvailable = new wxListCtrl(this, ID_LIST_AVAILABLE, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxSUNKEN_BORDER|wxLC_VRULES);
	btnAddData = new wxButton(this, ID_BTN_ADDDATA, wxT(">"));
	btnAddNode = new wxButton(this,ID_BTN_ADDNODE, wxT(">>"));
	btnAddAll = new wxButton(this, ID_BTN_ADDALL, wxT(">>>"));
	panel_2 = new wxPanel(this, wxID_ANY);
	label_4 = new wxStaticText(this, wxID_ANY, TRANS("Selection"));
	listSelected = new wxListCtrl(this, ID_LIST_SELECTED, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxSUNKEN_BORDER);
	btnSave = new wxButton(this, wxID_SAVE, wxEmptyString);
	btnCancel = new wxButton(this, wxID_CANCEL, wxEmptyString);

	btnSave->SetFocus();

	set_properties();
	do_layout();
	// end wxGlade

	//Disable most everything.
	enableSelectionControls(false);

	//Assign global variables their init value
	//--
	ASSERT(!exportPosYieldWindow);
	exportPosYieldWindow=this;
	
	ASSERT(!exportPosDelayTime); //Should not have been inited yet.

	exportPosDelayTime = new wxStopWatch();
	//--

	//Add columns to report listviews
	listSelected->InsertColumn(0,TRANS("Index"));
	listSelected->InsertColumn(1,TRANS("Count"));
	
	listAvailable->InsertColumn(0,TRANS("Index"));
	listAvailable->InsertColumn(1,TRANS("Count"));
}

ExportPosDialog::~ExportPosDialog()
{
	delete exportPosDelayTime;
	exportPosDelayTime=0;
	exportPosYieldWindow=0;
	//Should have called cleanup before exiting.
	ASSERT(!haveRefreshed);
}

BEGIN_EVENT_TABLE(ExportPosDialog, wxDialog)
    // begin wxGlade: ExportPosDialog::event_table
    EVT_BUTTON(ID_BTN_ADDDATA, ExportPosDialog::OnBtnAddData)
    EVT_BUTTON(ID_BTN_ADDNODE, ExportPosDialog::OnBtnAddNode)
    EVT_BUTTON(ID_BTN_ADDALL, ExportPosDialog::OnBtnAddAll)
    EVT_RADIOBUTTON(ID_RADIO_VISIBLE, ExportPosDialog::OnVisibleRadio)
    EVT_RADIOBUTTON(ID_RADIO_SELECTION, ExportPosDialog::OnSelectedRadio)
    EVT_TREE_SEL_CHANGED(ID_TREE_FILTERS, ExportPosDialog::OnTreeFiltersSelChanged)
    EVT_LIST_ITEM_ACTIVATED(ID_LIST_AVAILABLE, ExportPosDialog::OnListAvailableItemActivate)
    EVT_LIST_ITEM_ACTIVATED(ID_LIST_SELECTED, ExportPosDialog::OnListSelectedItemActivate)
    EVT_LIST_KEY_DOWN(ID_LIST_SELECTED, ExportPosDialog::OnListSelectedItemKeyDown)
    EVT_BUTTON(wxID_SAVE, ExportPosDialog::OnSave)
    EVT_BUTTON(wxID_CANCEL, ExportPosDialog::OnCancel)
	
    // end wxGlade
END_EVENT_TABLE()

void ExportPosDialog::initialiseData(FilterTree &f)
{
	ASSERT(!haveRefreshed)
		
	//Steal the filter tree contents
	f.swap(filterTree);
	vector<const Filter*> dummyPersist;
	upWxTreeCtrl(filterTree,treeData,filterMap,dummyPersist,0);	

	vector<SelectionDevice *> devices;
	ProgressData p;
	std::vector<std::pair<const Filter *, string > > consoleStrings;

	ATOMIC_BOOL wantAbort;
	wantAbort=false;
	filterTree.refreshFilterTree(outputData,devices,consoleStrings,p,wantAbort);

	//Delete all filter items that came out of refresh, other than ion streams
	filterTree.safeDeleteFilterList(outputData,STREAM_TYPE_IONS,true);

	haveRefreshed=true;
}

void ExportPosDialog::OnVisibleRadio(wxCommandEvent &event)
{
	//This event can fire BEFORE the dialog is shown (after initing)
	// under MSW/wx3.1
	if(!haveRefreshed)
		return;

	exportVisible=true;
	listAvailable->DeleteAllItems();
	enableSelectionControls(false);
}


void ExportPosDialog::OnSelectedRadio(wxCommandEvent &event)
{
	ASSERT(haveRefreshed);
	exportVisible=false;
	enableSelectionControls(true);
}


void ExportPosDialog::OnTreeFiltersSelChanged(wxTreeEvent &event)
{
	wxTreeItemId id;
	id=treeData->GetSelection();

	if(!id.IsOk() || id== treeData->GetRootItem())
	{
		event.Skip();
		return;
	}
	//Tree data contains unique identifier for vis control to do matching
	wxTreeItemData *tData=treeData->GetItemData(id);

	//Clear the available list
	listAvailable->DeleteAllItems();
	availableFilterData.clear();

	const Filter *targetFilter=filterMap[((wxTreeUint *)tData)->value];

	typedef std::pair<Filter *,vector<const FilterStreamData * > > filterOutputData;
	//Spin through the output list, looking for this filter's contribution
	for(list<filterOutputData>::iterator it=outputData.begin();it!=outputData.end();++it)
	{
		//Is this the filter we are looking for?
		if(it->first == targetFilter)
		{
			//huzzah.
			std::string label;

			for(unsigned int ui=0;ui<it->second.size();ui++)
			{
				const IonStreamData *ionData;
				ionData=(const IonStreamData *)((it->second)[ui]);
				
				wxColour c;
				c.Set((unsigned char)(ionData->r*255),
					(unsigned char)(ionData->g*255),(unsigned char)(ionData->b*255));
				//Add the item using the index as a str
				stream_cast(label,ui);
				listAvailable->InsertItem(ui,(label));
				
				size_t basicCount;
				basicCount=ionData->getNumBasicObjects();
				stream_cast(label,basicCount);

				listAvailable->SetItem(ui,1,(label));
				
				listAvailable->SetItemBackgroundColour(ui,c);

				availableFilterData.push_back((it->second)[ui]);
			}
		}

	}
}


void ExportPosDialog::OnListAvailableItemActivate(wxListEvent &event)
{
	unsigned int item=event.GetIndex();

	//If the selected item is not already in the "selected filter" list, add it
	if(find(selectedFilterData.begin(),selectedFilterData.end(),
			availableFilterData[item]) == selectedFilterData.end())
		selectedFilterData.push_back(availableFilterData[item]);

	//Update the selection list
	updateSelectedList();
}

void ExportPosDialog::OnListSelectedItemActivate(wxListEvent &event)
{
	unsigned int item=event.GetIndex();

	unsigned int thisIdx=0;
	for(list<const FilterStreamData *>::iterator it=selectedFilterData.begin();
			it!=selectedFilterData.end(); ++it)
	{
		if(thisIdx == item)
		{
			selectedFilterData.erase(it);
			break;
		}
	
		thisIdx++;	
	}

	//Update the selection list
	updateSelectedList();

	
}

void ExportPosDialog::OnListSelectedItemKeyDown(wxListEvent &event)
{
	switch(event.GetKeyCode())
	{
		case WXK_DELETE:
		{
			//Spin through the selected items
			int item=-1;
			for ( ;; )
			{
				item = listSelected->GetNextItem(item,
							     wxLIST_NEXT_ALL,
							     wxLIST_STATE_SELECTED);
				if ( item == -1 )
					break;

				list<const FilterStreamData *>::iterator it;
	
				it=find(selectedFilterData.begin(),selectedFilterData.end(),
									availableFilterData[item]);
				//If the selected item is not already in the "selected filter" list, add it
				if(it != selectedFilterData.end())
					selectedFilterData.erase(it);
			}
			
			//Update the selection list
			updateSelectedList();
		}

	}	
}

void ExportPosDialog::OnBtnAddAll(wxCommandEvent &event)
{
	selectedFilterData.clear();
	typedef std::pair<Filter *,vector<const FilterStreamData * > > filterOutputData;

	for(list<filterOutputData>::iterator it=outputData.begin();it!=outputData.end();++it)
	{
		for(unsigned int uj=0;uj<it->second.size();uj++)
			selectedFilterData.push_back(it->second[uj]);
	}


	updateSelectedList();
}


void ExportPosDialog::OnBtnAddData(wxCommandEvent &event)
{

	int item=-1;
	for ( ;; )
	{
		item = listAvailable->GetNextItem(item,
					     wxLIST_NEXT_ALL,
					     wxLIST_STATE_SELECTED);
		if ( item == -1 )
			break;

		//Disallow addition of duplicate entries
		if(find(selectedFilterData.begin(),selectedFilterData.end(),
				availableFilterData[item]) == selectedFilterData.end())
			selectedFilterData.push_back(availableFilterData[item]);
	}
	updateSelectedList();

}

void ExportPosDialog::updateSelectedList()
{
	//Clear the available list
	listSelected->DeleteAllItems();

	unsigned int idx=0;
	std::string label;
	for(list<const FilterStreamData *>::iterator it=selectedFilterData.begin();
				it!=selectedFilterData.end(); ++it)
	{
		const IonStreamData *ionData;
		ionData=(const IonStreamData *)(*it);
		
		wxColour c;
		c.Set((unsigned char)(ionData->r*255),
				(unsigned char)(ionData->g*255),
				(unsigned char)(ionData->b*255));
		//Add the item using the index as a str
		stream_cast(label,idx);
		listSelected->InsertItem(idx,(label));
		
		size_t basicCount;
		basicCount=ionData->getNumBasicObjects();
		stream_cast(label,basicCount);

		listSelected->SetItem(idx,1,(label));
		
		listSelected->SetItemBackgroundColour(idx,c);

		idx++;	
	}



	if(listSelected->GetItemCount())
	{
		btnSave->Enable();
	}
	else
		btnSave->Disable();
}

void ExportPosDialog::OnBtnAddNode(wxCommandEvent &event)
{

	//Spin through the selected items
	for (int item=0;item<listAvailable->GetItemCount(); item++)
	{
		//If the selected item is not already in the "selected filter" list, add it
		if(find(selectedFilterData.begin(),selectedFilterData.end(),
				availableFilterData[item]) == selectedFilterData.end())
			selectedFilterData.push_back(availableFilterData[item]);
	}

	updateSelectedList();
}



void ExportPosDialog::OnSave(wxCommandEvent &event)
{
	exportVisible=(radioVisible->GetValue());
	EndModal(wxID_OK);
}

void ExportPosDialog::OnCancel(wxCommandEvent &event)
{
	EndModal(wxID_CANCEL);
}

void ExportPosDialog::getExportVec(std::vector<const FilterStreamData * > &v) const
{
	typedef std::pair<Filter *,vector<const FilterStreamData * > > filterOutputData;

	//Incoming vector should be empty
	ASSERT(v.empty());

	//If the user has selected "visible", then all outputs are to be exported
	if(exportVisible)
	{
		v.reserve(outputData.size());

		for(list<filterOutputData>::const_iterator it=outputData.begin();
							it!=outputData.end();++it)
		{
			for(unsigned int ui=0;ui<it->second.size();ui++)
			{
				v.push_back(it->second[ui]);
				//Ensure pointer is valid by forcing a dereference
				ASSERT(v.back()->getStreamType() == STREAM_TYPE_IONS);
			}

		}
	}
	else
	{
		//If the user wants to perform custom picking, then only "selected" to be
		//exported
		v.reserve(selectedFilterData.size());
		for(list<const FilterStreamData *>::const_iterator it=selectedFilterData.begin();
				it!=selectedFilterData.end(); ++it)
		{
			v.push_back(*it);
		}

	}
}

// wxGlade: add ExportPosDialog event handlers


void ExportPosDialog::set_properties()
{
    // begin wxGlade: ExportPosDialog::set_properties
    SetTitle(TRANS("Export Pos Data"));
    // end wxGlade

	treeData->SetToolTip(TRANS("Tree of filters, select leaves to show ion data."));
	
	btnAddAll->SetToolTip(TRANS("Add all data from all filters"));
	btnAddNode->SetToolTip(TRANS("Add all data from currently selected filter"));
	btnAddData->SetToolTip(TRANS("Add selected data from currently selected filter"));
    radioVisible->SetValue(TRUE);
}

void ExportPosDialog::enableSelectionControls(bool enabled)

{
	//Enable/disable controls that are used for selection
	treeData->Enable(enabled);
	listAvailable->Enable(enabled);
	btnAddData->Enable(enabled);
	btnAddNode->Enable(enabled);
	btnAddAll->Enable(enabled);
	listSelected->Enable(enabled);

	//If the selection control is enabled,
	//bring the tree back to life
	//otherwise, grey it out.
	if(enabled)
	{
		treeData->ExpandAll();
		treeData->SetForegroundColour(wxNullColour);
		btnSave->Enable(listSelected->GetItemCount());
	}
	else
	{
		treeData->CollapseAll();
		treeData->SetForegroundColour(*wxLIGHT_GREY);
		treeData->Unselect();
		btnSave->Enable();
	}
}

void ExportPosDialog::do_layout()
{
    // begin wxGlade: ExportPosDialog::do_layout
    wxBoxSizer* sizer_4 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_12 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_13 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_11 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_9 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_10 = new wxBoxSizer(wxVERTICAL);
    sizer_4->Add(10, 20, 0, 0, 0);
    sizer_9->Add(lblExport, 0, wxTOP|wxBOTTOM, 5);
    sizer_9->Add(radioVisible, 0, 0, 0);
    sizer_9->Add(radioSelection, 0, 0, 0);
    sizer_10->Add(treeData, 1, wxTOP|wxBOTTOM|wxEXPAND, 6);
    sizer_10->Add(lblAvailableData, 0, 0, 0);
    sizer_10->Add(listAvailable, 1, wxBOTTOM|wxEXPAND, 5);
    sizer_9->Add(sizer_10, 1, wxEXPAND, 0);
    sizer_4->Add(sizer_9, 1, wxALL|wxEXPAND, 5);
    sizer_11->Add(20, 200, 0, 0, 0);
    sizer_11->Add(btnAddData, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 10);
    sizer_11->Add(btnAddNode, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 10);
    sizer_11->Add(btnAddAll, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, 10);
    sizer_11->Add(panel_2, 1, wxEXPAND, 0);
    sizer_4->Add(sizer_11, 0, wxEXPAND, 0);
    sizer_12->Add(20, 40, 0, 0, 0);
    sizer_12->Add(label_4, 0, wxTOP|wxBOTTOM, 6);
    sizer_12->Add(listSelected, 1, wxEXPAND, 0);
    sizer_12->Add(20, 20, 0, 0, 0);
    sizer_13->Add(20, 20, 1, 0, 0);
    sizer_13->Add(btnSave, 0, wxLEFT|wxRIGHT|wxBOTTOM|wxALIGN_BOTTOM, 6);
    sizer_13->Add(btnCancel, 0, wxBOTTOM|wxALIGN_BOTTOM, 6);
    sizer_12->Add(sizer_13, 0, wxEXPAND, 0);
    sizer_4->Add(sizer_12, 1, wxALL|wxEXPAND, 5);
    SetSizer(sizer_4);
    sizer_4->Fit(this);
    Layout();
    // end wxGlade
}

