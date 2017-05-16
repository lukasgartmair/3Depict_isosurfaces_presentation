/*
 *	ExportRngDialog.h - Range data export dialog
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

#ifndef EXPORTRNGDIALOG_H
#define EXPORTRNGDIALOG_H

#include <wx/wx.h>
// begin wxGlade: ::dependencies
#include <wx/listctrl.h>
#include <wx/grid.h>
// end wxGlade

#include <map>
#include <vector>

class VisController;
class Filter;

class ExportRngDialog: public wxDialog {
public:
    // begin wxGlade: ExportRngDialog::ids
    // end wxGlade

    ExportRngDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_DIALOG_STYLE);

private:
    //!Vis controller pointer FIXME: Can we downgrade this to const?
    VisController *visControl;
    //!vector containing currently available filter streams
    std::vector<const Filter *> rngFilters;

    // begin wxGlade: ExportRngDialog::methods
    void set_properties();
    void do_layout();
    // end wxGlade
    
    //!Use filter data to draw wx widget
    void  updateRangeList();
    //!Draw details of selected range into grid
    void updateGrid(unsigned int index);

    unsigned int selectedRange;
protected:
    // begin wxGlade: ExportRngDialog::attributes
    wxStaticText* lblRanges;
    wxListCtrl* listRanges;
    wxStaticText* label_3;
    wxGrid* gridDetails;
    wxButton* btnOK;
    wxButton* btnCancel;
    // end wxGlade

    DECLARE_EVENT_TABLE();

public:
    virtual void OnListRangeItemActivate(wxListEvent &event); // wxGlade: <event_handler>
    virtual void OnSave(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCancel(wxCommandEvent &event); // wxGlade: <event_handler>

    void addRangeData(std::vector<const Filter *> rangeData);

}; // wxGlade: end class


#endif // EXPORTRNGDIALOG_H
