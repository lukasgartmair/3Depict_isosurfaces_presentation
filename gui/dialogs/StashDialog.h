/*
 * 	stashdialog.h - "Stash" filter storage edit dialog header
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


#ifndef STASHDIALOG_H
#define STASHDIALOG_H

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/grid.h>
#include <wx/propgrid/propgrid.h>

// end wxGlade

#include "./backend/filtertree.h"
class VisController;
// begin wxGlade: ::extracode
#include <wx/listctrl.h>
// end wxGlade


class StashDialog: public wxDialog {
public:
    // begin wxGlade: StashDialog::ids
    // end wxGlade

	
    StashDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_DIALOG_STYLE);

    void setVisController(VisController *s);

private:

    FilterTree curTree;
    std::vector<std::pair<unsigned int,const Filter *> > filterTreeMapping;
    UniqueIDHandler uniqueIds;
    // begin wxGlade: StashDialog::methods
    void set_properties();
    void do_layout();
    void updateList();
    void updateGrid();
    void updateTree();

    bool getStashIdFromList(unsigned int &stashId);
    // end wxGlade

    VisController *visControl;
protected:
    // begin wxGlade: StashDialog::attributes
    wxStaticText* label_5;
    wxListCtrl* listStashes;
    wxButton* btnRemove;
    wxStaticText* label_6;
    wxTreeCtrl* treeFilters;
    wxStaticText* label_7;
    wxPropertyGrid* gridProperties;
    wxButton* btnOK;
    // end wxGlade

    DECLARE_EVENT_TABLE();

public:
    virtual void OnListKeyDown(wxListEvent &event); // wxGlade: <event_handler>
    virtual void OnListSelected(wxListEvent &event); // wxGlade: <event_handler>
    virtual void OnTreeSelChange(wxTreeEvent &event); // wxGlade: <event_handler>
    virtual void OnGridEditor(wxPropertyGridEvent &event);

    virtual void OnBtnRemove(wxCommandEvent &event);
    void ready();
}; // wxGlade: end class



#endif // STASHDIALOG_H
