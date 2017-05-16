/*
 *	wxCropPanel.cpp - cropping window  for user interaction
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

#ifndef WXCROPPANEL_H
#define WXCROPPANEL_H

#include <wx/wx.h>

//Selection method
enum
{
	SELECT_MODE_NONE,
	SELECT_MODE_SIDE,
	SELECT_MODE_CENTRE,
	SELECT_MODE_CORNER,
	SELECT_MODE_END_ENUM
};


enum
{
	CROP_LINK_NONE,
	CROP_LINK_LR,
	CROP_LINK_LR_FLIP,
	CROP_LINK_TB,
	CROP_LINK_TB_FLIP,
	CROP_LINK_BOTH,
	CROP_LINK_BOTH_FLIP,
};

class CropPanel : public wxPanel
{
	private:
		//!A panel to force the linkage to (link crop border positions)
		CropPanel *linkedPanel;

		//!The link mode for the other panel
		unsigned int linkMode;

		//!True if event generated programmatically (blocker bool)
		bool programmaticEvent;
		//!Cropping %ages for window 
		float crop[4];

		//Mouse coords and crop coords at drag start (0->1)
		float mouseAtDragStart[2];
		float cropAtDragStart[4];

		//!Selection mode and index for different crop edges/corners/centre
		unsigned int selMode,selIndex;

		//!Is the control currently being dragged by the user with the mouse?
		bool dragging;

		//!True if the crop array has been modified.
		bool hasUpdates;
		
		bool validCoords() const;
		
	
		void draw() ;

		//!Get the "best" crop widget as defined by coordinates (in 0->1 space)
		//returns the index of the selected item as parameter
		unsigned int getBestCropWidget(float xMouse,float yMouse, unsigned int &idx) const;


		DECLARE_EVENT_TABLE();
	public:
		CropPanel(wxWindow * parent, wxWindowID id = wxID_ANY,
			const wxPoint & pos = wxDefaultPosition,
			const wxSize & size = wxDefaultSize,
			long style = wxTAB_TRAVERSAL) ;


		bool hasUpdate(){return hasUpdates;};
		void clearUpdate(){hasUpdates=false;};

		void getCropValues(float *array) const;
		//!Directly set the crop value, (0->1), index can be 0->3
		void setCropValue(unsigned int index, float v);

		void makeCropValuesValid(); 

		//!Link this panel's updates to another. Use CROP_LINK_NONE to disable
		void link(CropPanel *otherPanel,unsigned int mode);

		void OnEraseBackground(wxEraseEvent& event);

		void mouseMove(wxMouseEvent& event);
		void mouseDown(wxMouseEvent& event);
		void mouseReleased(wxMouseEvent& event);
		void mouseLeftWindow(wxMouseEvent& event);
		void mouseDoubleLeftClick(wxMouseEvent& event);
		void onPaint(wxPaintEvent& evt);
		void onResize(wxSizeEvent& evt);
		
		void updateLinked();
		~CropPanel() {};
};
#endif
