/*
 *	StashDialog.cpp - filter "Stash" tree editing and viewing dialog
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

#include "StashDialog.h"

#include "wx/wxcommon.h"
#include "wx/wxcomponents.h"
#include "wx/propertyGridUpdater.h"
#include "common/translation.h"

#include "./backend/viscontrol.h"

#include <stack>

using std::pair;
using std::string;
using std::stack;
using std::vector;
// begin wxGlade: ::extracode

// end wxGlade

enum
{
    ID_TREE_FILTERS=wxID_ANY+1,
    ID_GRID_FILTER,
    ID_LIST_STASH,
};

StashDialog::StashDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxDialog(parent, id, title, pos, size, style)
{
    // begin wxGlade: StashDialog::StashDialog
    label_5 = new wxStaticText(this, wxID_ANY, TRANS("Stashes"));
    listStashes = new wxListCtrl(this, ID_LIST_STASH, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxSUNKEN_BORDER);
    btnRemove = new wxButton(this, wxID_REMOVE, wxEmptyString);
    label_6 = new wxStaticText(this, wxID_ANY, TRANS("Stashed Tree"));
    treeFilters = new wxTreeCtrl(this, ID_TREE_FILTERS, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS|wxTR_NO_LINES|wxTR_DEFAULT_STYLE|wxSUNKEN_BORDER|wxTR_HIDE_ROOT);
    label_7 = new wxStaticText(this, wxID_ANY, TRANS("Properties"));
    gridProperties = new wxPropertyGrid(this, ID_GRID_FILTER);
    btnOK = new wxButton(this, wxID_OK, wxEmptyString);
    
    //Due to a bug in wx, empty reports throw an assertion
    // 'unknown list item format", when there are no columns
    listStashes->InsertColumn(0,TRANS("Stash Name"));
    listStashes->InsertColumn(1,TRANS("Filter Count"));

    set_properties();
    do_layout();
    // end wxGlade

}

void StashDialog::ready()
{
	updateList(); 
	updateGrid();
	updateTree();
}

BEGIN_EVENT_TABLE(StashDialog, wxDialog)
    // begin wxGlade: StashDialog::event_table
    EVT_LIST_KEY_DOWN(ID_LIST_STASH, StashDialog::OnListKeyDown)
    EVT_BUTTON(wxID_REMOVE, StashDialog::OnBtnRemove)
    EVT_LIST_ITEM_SELECTED(ID_LIST_STASH, StashDialog::OnListSelected)
    EVT_TREE_SEL_CHANGED(ID_TREE_FILTERS, StashDialog::OnTreeSelChange)
    EVT_PG_CHANGING(ID_GRID_FILTER,StashDialog::OnGridEditor)
    // end wxGlade
END_EVENT_TABLE();

void StashDialog::setVisController(VisController *s)
{
	visControl=s;
}

void StashDialog::set_properties()
{
    // begin wxGlade: StashDialog::set_properties
    SetTitle(TRANS("Stashed Trees"));
    SetSize(wxSize(600, 430));

    btnRemove->SetToolTip(TRANS("Erase stashed item"));
    treeFilters->SetToolTip(TRANS("Filter view for current stash"));
    gridProperties->SetToolTip(TRANS("Settings for selected filter in current stash"));
    listStashes->SetToolTip(TRANS("Available stashes"));
    // end wxGlade
}

void StashDialog::OnGridEditor(wxPropertyGridEvent &evt)
{
	//Silence error mesages
	evt.SetValidationFailureBehavior(0);
	
	evt.Veto();
}

void StashDialog::OnListKeyDown(wxListEvent &event)
{
	switch(event.GetKeyCode())
	{
		case WXK_DELETE:
		{
			//Spin through the selected items
			int item=-1;
			for ( ;; )
			{
				item = listStashes->GetNextItem(item,
							     wxLIST_NEXT_ALL,
							     wxLIST_STATE_SELECTED);
				if ( item == -1 )
					break;

				visControl->state.eraseStash(listStashes->GetItemData(item));
			}
			
			//Update the filter list
			updateList();
			updateTree();
			updateGrid();
		}

	}	
}

void StashDialog::OnListSelected(wxListEvent &event)
{
	updateTree();
	updateGrid();
}

void StashDialog::OnTreeSelChange(wxTreeEvent &event)
{
	updateGrid();
}

void StashDialog::updateList()
{
	//Generate the stash selection list

	//Clear the existing list
	listStashes->Freeze();
	listStashes->DeleteAllItems();

	unsigned int nStashes=visControl->state.getStashCount();	
	//Fill it with "stash" entries
	//Add columns to report listviews
	for (unsigned int ui=0; ui<nStashes; ui++)
	{
		string strTmp;
		pair<std::string,FilterTree> stash;
		long itemIdx;
		visControl->state.copyStashedTree(ui,stash);
		//First item is the stash name
		itemIdx = listStashes->InsertItem(ui,stash.first);

		//Second column is num filters
		stream_cast(strTmp,stash.second.size());
		listStashes->SetItem(itemIdx,1,(strTmp));

		//Set the stash ID as the list data item
		//this is the key to the stash val
		listStashes->SetItemData(itemIdx,ui);	
	}
	listStashes->Thaw();
}

void StashDialog::updateGrid()
{
	gridProperties->Clear();
	if(!treeFilters->GetCount())
		return;
	//Get the selection from the current tree
	wxTreeItemId id;
	id=treeFilters->GetSelection();

	if(!id.IsOk() || id == treeFilters->GetRootItem())
		return;

	unsigned int stashId;

	//Retrieve the stash ID
	if(!getStashIdFromList(stashId))
		return;
	
	//Tree data contains unique identifier for vis control to do matching
	wxTreeItemData *tData=treeFilters->GetItemData(id);
	
	unsigned int filterIdx = ((wxTreeUint *)tData)->value;

	FilterTree t;
	visControl->state.copyStashedTree(stashId,t);

	Filter *targetFilter=0;
	unsigned int pos=0;
	//Spin through the tree iterators until we hit the target index
	for(tree<Filter *>::iterator it=t.depthBegin();it!=t.depthEnd(); ++it)
	{
		if(pos == filterIdx)
		{
			targetFilter=*it;
			break;
		}
		pos++;
	}
	
	ASSERT(targetFilter);	

	updateFilterPropertyGrid(gridProperties,targetFilter,"");	

}

bool StashDialog::getStashIdFromList(unsigned int &stashId)
{
	//Get the currently selected item
	//Spin through the selected items
	int item=-1;
	unsigned int numItems=0;
	for ( ;; )
	{
		item = listStashes->GetNextItem(item,
					     wxLIST_NEXT_ALL,
					     wxLIST_STATE_SELECTED);
		if ( item == -1 )
			break;
		numItems++;
	}
	
	//error if not only a single selected
	if(numItems !=1)
		return false;	

	item=-1;
	item = listStashes->GetNextItem(item,
				     wxLIST_NEXT_ALL,
				     wxLIST_STATE_SELECTED);

	stashId = listStashes->GetItemData(item);


	return true;
}

void StashDialog::updateTree()
{
	treeFilters->DeleteAllItems();

	unsigned int stashId;

	//Get the selected stash and build the tree control
	if(!getStashIdFromList(stashId))
		return;

	visControl->state.copyStashedTree(stashId,curTree);


	uniqueIds.clear();
	//REBUILD TREE
	//=====	
	stack<wxTreeItemId> treeIDs;
	wxTreeItemId visibleTreeId;
	//Warning: this generates an event, 
	//most of the time (some windows versions do not according to documentation)
	treeFilters->DeleteAllItems();
	filterTreeMapping.clear(); 

	int lastDepth=0;
	//Add dummy root node. This will be invisible to wxTR_HIDE_ROOT controls
	wxTreeItemId tid;
	tid=treeFilters->AddRoot(wxT("TreeBase"));
	unsigned int curTreePos=0;	

	// Push on stack to prevent underflow, but don't keep a copy, 
	// as we will never insert or delete this from the UI	
	treeIDs.push(tid);

	//Depth first  add
	unsigned int pos=0;
	for(tree<Filter * >::pre_order_iterator filtIt=curTree.depthBegin();
					filtIt!=curTree.depthEnd(); ++filtIt)
	{
		//Push or pop the stack to make it match the iterator position
		if( lastDepth > curTree.depth(filtIt))
		{
			while(curTree.depth(filtIt) +1 < (int)treeIDs.size())
				treeIDs.pop();
		}
		else if( lastDepth < curTree.depth(filtIt))
		{
			treeIDs.push(tid);
		}
		

		lastDepth=curTree.depth(filtIt);
	
		//This will use the user label or the type string.	
		tid=treeFilters->AppendItem(treeIDs.top(),
			((*filtIt)->getUserString()));
		
		treeFilters->SetItemData(tid,new wxTreeUint(pos));
		pos++;

		//Record mapping to filter for later reference
		filterTreeMapping.push_back(std::make_pair(curTreePos,*filtIt));

		curTreePos++;
	}
	//=====	


		
}

void StashDialog::OnBtnRemove(wxCommandEvent &event)
{
	//Spin through the list, to find the selected items
	vector<size_t> itemsToRemove;
	int item=-1;
	for ( ;; )
	{
		item = listStashes->GetNextItem(item,
					     wxLIST_NEXT_ALL,
					     wxLIST_STATE_SELECTED);
		if ( item == -1 )
			break;
		itemsToRemove.push_back(listStashes->GetItemData(item));
	}
	
	visControl->state.eraseStashes(itemsToRemove);

	ready();
}

void StashDialog::do_layout()
{
    // begin wxGlade: StashDialog::do_layout
    wxBoxSizer* sizer_17 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_19 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_18 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_21 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_20 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizer_22 = new wxBoxSizer(wxHORIZONTAL);
    sizer_17->Add(9, 8, 0, 0, 0);
    sizer_20->Add(label_5, 0, wxLEFT, 5);
    sizer_20->Add(listStashes, 1, wxLEFT|wxRIGHT|wxEXPAND, 5);
    sizer_22->Add(btnRemove, 0, wxLEFT|wxALIGN_RIGHT, 6);
    sizer_20->Add(sizer_22, 0, wxTOP|wxEXPAND, 8);
    sizer_18->Add(sizer_20, 1, wxLEFT|wxEXPAND, 5);
    sizer_18->Add(15, 20, 0, 0, 0);
    sizer_21->Add(label_6, 0, 0, 4);
    sizer_21->Add(treeFilters, 1, wxRIGHT|wxEXPAND, 5);
    sizer_21->Add(label_7, 0, wxTOP, 10);
    sizer_21->Add(gridProperties, 1, wxRIGHT|wxBOTTOM|wxEXPAND, 5);
    sizer_18->Add(sizer_21, 1, wxRIGHT|wxEXPAND, 5);
    sizer_17->Add(sizer_18, 1, wxEXPAND, 0);
    sizer_17->Add(20, 20, 0, 0, 0);
    sizer_19->Add(20, 20, 1, 0, 0);
    sizer_19->Add(btnOK, 0, wxALL, 5);
    sizer_17->Add(sizer_19, 0, wxEXPAND, 0);
    SetSizer(sizer_17);
    Layout();
    // end wxGlade
}

