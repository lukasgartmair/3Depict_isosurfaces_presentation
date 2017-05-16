/*
 *	stringKeyFrameDialog.h -String value keyframe selection dialog
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
// -*- C++ -*- generated by wxGlade 0.6.5 on Sat Sep 22 21:13:08 2012

#include <wx/wx.h>
// begin wxGlade: ::dependencies
#include <wx/grid.h>
// end wxGlade

#include <vector>


#ifndef STRINGKEYFRAMEDIALOG_H
#define STRINGKEYFRAMEDIALOG_H


class StringKeyFrameDialog: public wxDialog {
public:
	// begin wxGlade: StringKeyFrameDialog::ids
	// end wxGlade

	StringKeyFrameDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_DIALOG_STYLE);

	//!Data 
	size_t getStartFrame() const ;

	bool getStrings(std::vector<std::string> &res) const;
	
private:
	// begin wxGlade: StringKeyFrameDialog::methods
	void set_properties();
	void do_layout();
	// end wxGlade

	//!Should we be using a file as the string data source?
	bool dataSourceFromFile;

	//!File to use as the data source
	std::string dataFilename;

	//!Initial frame to use for value strings
	size_t startFrame;

	//strings that are to be used as the data for each frame 
	std::vector<std::string> valueStrings;

	//!Check to see if the various data elements are OK
	bool startFrameOK, filenameOK,gridOK;
	
	//Enable/disable OK button, checking dialog content validity
	void updateOKButton();

	//Build the grid UI from the internal string value data 
	void buildGrid();

protected:
	// begin wxGlade: StringKeyFrameDialog::attributes
	wxStaticText* labelStartFrame;
	wxTextCtrl* textStartFrame;
	wxRadioButton* radioFromFile;
	wxTextCtrl* textFilename;
	wxButton* btnChooseFile;
	wxRadioButton* radioFromTable;
	wxGrid* gridStrings;
	wxButton* btnStringAdd;
	wxButton* btnRemove;
	wxButton* btnCancel;
	wxButton* btnOK;
	// end wxGlade

	DECLARE_EVENT_TABLE();

public:
	virtual void OnTextStart(wxCommandEvent &event); // wxGlade: <event_handler>
	virtual void OnFileRadio(wxCommandEvent &event); // wxGlade: <event_handler>
	virtual void OnTextFilename(wxCommandEvent &event); // wxGlade: <event_handler>
	virtual void OnBtnChooseFile(wxCommandEvent &event); // wxGlade: <event_handler>
	virtual void OnTableRadio(wxCommandEvent &event); // wxGlade: <event_handler>
	virtual void OnGridCellChange(wxGridEvent &event); // wxGlade: <event_handler>
	virtual void OnGridEditorShown(wxGridEvent &event); // wxGlade: <event_handler>
	virtual void OnBtnAdd(wxCommandEvent &event); // wxGlade: <event_handler>
	virtual void OnBtnRemove(wxCommandEvent &event); // wxGlade: <event_handler>
}; // wxGlade: end class


#endif // STRINGKEYFRAMEDIALOG_H