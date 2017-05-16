/*
 *	ExportPos.h - Point data export dialog
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

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>

#include <map>

#ifndef EXPORTPOS_H
#define EXPORTPOS_H

// begin wxGlade: ::dependencies
// end wxGlade

// begin wxGlade: ::extracode

// end wxGlade

#include "backend/filtertree.h"


class ExportPosDialog: public wxDialog {
public:
    // begin wxGlade: ExportPosDialog::ids
    // end wxGlade

    ExportPosDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_DIALOG_STYLE);
    ~ExportPosDialog();
private:


    FilterTree filterTree;

    std::map<size_t, Filter*> filterMap;

    //!Have we refreshed the filterstream data list?
    bool haveRefreshed;
    //!Should we be exporting selected ions (false) or visible ions (true)
    bool exportVisible;
    //!List containing filter and  ion streams to export 
   std::list<std::pair<Filter *, std::vector<const FilterStreamData * > > > outputData; 
    //!vector containing currently available filter streams
   std::vector<const FilterStreamData *> availableFilterData;

   //List containing currently selected filter streams
   std::list<const FilterStreamData *> selectedFilterData;


   //!Use selectedFilterData to draw wx widget
   void  updateSelectedList();
    // begin wxGlade: ExportPosDialog::methods
    void set_properties();
    void do_layout();
    // end wxGlade

protected:
    // begin wxGlade: ExportPosDialog::attributes
    wxStaticText* lblExport;
    wxRadioButton* radioVisible;
    wxRadioButton* radioSelection;
    wxTreeCtrl* treeData;
    wxStaticText* lblAvailableData;
    wxListCtrl* listAvailable;
    wxButton* btnAddData;
    wxButton* btnAddNode;
    wxButton* btnAddAll;
    wxPanel* panel_2;
    wxStaticText* label_4;
    wxListCtrl* listSelected;
    wxButton* btnSave;
    wxButton* btnCancel;
    // end wxGlade

    DECLARE_EVENT_TABLE();

public:
    virtual void OnVisibleRadio(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnSelectedRadio(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnTreeFiltersSelChanged(wxTreeEvent &event); // wxGlade: <event_handler>
    virtual void OnBtnAddAll(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnBtnAddData(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnBtnAddNode(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnSave(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCancel(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnListAvailableItemActivate(wxListEvent &event); // wxGlade: <event_handler>
    virtual void OnListSelectedItemActivate(wxListEvent &event); // wxGlade: <event_handler>
    virtual void OnListSelectedItemKeyDown(wxListEvent &event); // wxGlade: <event_handler>

    void initialiseData(FilterTree &f);
    void enableSelectionControls(bool enabled);
    void getExportVec(std::vector<const FilterStreamData *> &v) const; 
    void swapFilterTree(FilterTree &f) { f.swap(filterTree);haveRefreshed=false;}

}; // wxGlade: end class


#endif // EXPORTPOS_H
