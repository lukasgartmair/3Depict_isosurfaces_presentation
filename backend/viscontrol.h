/*

 * 	viscontrol.h - Visualisation control header; "glue" between user interface and scene
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

#ifndef VISCONTROL_H
#define VISCONTROL_H

#include <wx/listbox.h>
#include <wx/textctrl.h>
#include <wx/grid.h>

class VisController;
class wxGrid;
class wxTreeCtrl;

#include "state.h"
#include "filtertreeAnalyse.h"
#include "filter.h"
#include "gl/scene.h"
#include "backend/plot.h"

#include <list>

//!Tree controller

class RefreshController
{
	private:
		TreeState *treeState;
		//!Results of last refresh
		std::list<FILTER_OUTPUT_DATA> refreshData;
		std::vector<std::pair<const Filter*, std::string> > consoleMessages;
	public:

		//Initialisation requires treecontroller
		RefreshController(TreeState &treeState);
		~RefreshController();
		//!Current progress
		ProgressData curProg;

		//!Refresh the tree-control's tree, and return error code
		// returns 0 on success, nonzero on failure (see TreeState::refreshFilterTree)
		unsigned int refresh();


		std::list<FILTER_OUTPUT_DATA> &getRefreshData() { return refreshData;};
		std::vector<std::pair<const Filter*, std::string> > &getConsoleMessages() { return consoleMessages;};
	

};

//!Visualisation controller
/*!
 * Keeps track of what visualisation controls the user has available
 * such as cameras, filters and data groups. 
 * This is essentially responsible for interfacing between program
 * data structures and the user interface.
 *
 * Only one of these should be instantiated at any time .
 */
class VisController
{
	private:
		static bool isInstantiated;
		//!Target Plot wrapper system
		PlotWrapper targetPlots;
		//!Target raw grid
		wxGrid *targetRawGrid;

		//!UI element for console output
		wxTextCtrl *textConsole;

		//!UI element for selecting plots from a list (for enable/disable)
		wxListBox *plotSelList;


		//!Maximum number of ions to pass to scene
		size_t limitIonOutput;

		//Filters that should be able to be seen next time we show
		// the wxTree control
		std::vector<const Filter *> persistentFilters;

		//Map plot position to ID. TODO: Remove me
		std::map<size_t, size_t> plotMap;




		//!Update the console strings
		void updateConsole(const std::vector<std::string> &v, const Filter *f) const;
		//!Limit the number of objects we are sending to the scene
		void throttleSceneInput(std::list<std::vector<const FilterStreamData *> > &outputData, 
			std::map<const IonStreamData *, const IonStreamData *> &throttleMap) const;
	public:
		AnalysisState state;
		Scene scene;

		VisController() {ASSERT(!isInstantiated); isInstantiated=true; scene.setVisControl(this);}; 
	
		void setActiveCam(unsigned int cam);

		//Returns true if current state has been modified since last save 
		bool stateIsModified(unsigned int minLevel = STATE_MODIFIED_ANCILLARY) const;
	
		//Set the maximum number of ions to allow the scene to display
		void setIonDisplayLimit(size_t newLimit) { limitIonOutput=newLimit;}

		size_t getIonDisplayLimit() const { return limitIonOutput;}

		RefreshController &getRefreshControl() const;

		void clearScene() {scene.clearAll();};

		
		//Return the selection devices obtained from the last refresh
		std::vector<SelectionDevice *> &getSelectionDevices() { return state.treeState.getSelectionDevices();};

		//Apply bindings from any selection devices (3D object modifiers) to the tree
		void applyBindingsToTree()  { state.treeState.applyBindingsToTree();}

		//Obtain updated camera from the scene and then commit it to the state
		void transferSceneCameraToState();
		//set the camera property for the state, then transfer to scene
		void setCamProperty(size_t offset, unsigned int key, const std::string &value);

		//!Ask that next time we build the tree, this filter is kept visible/selected.
		//	may be used repeatedly to make more items visible, 
		//	prior to calling updateWxTreeCtrl. 
		//	filterId must exist during call.
		void setWxTreeFilterViewPersistence(size_t filterId);

		//!Erase the filters that will persist in the view
		void clearTreeFilterViewPersistence() { persistentFilters.clear();}

		//!Write out the filters into a wxtreecontrol.
		// optional argument is the fitler to keep visible in the control
		void updateWxTreeCtrl(wxTreeCtrl *t,const Filter *f=0);
		//!Update a wxPropertyGrid with the properties for a given filter
		void updateFilterPropGrid(wxPropertyGrid *g,size_t filterId, const std::string &stateString="") const; 
		//!Update a wxPropertyGrid with the properties for a given filter
		void updateCameraPropGrid(wxPropertyGrid *g,size_t cameraId) const; 
		
		void updateCameraComboBox(wxComboBox *comboCamera) const;

		//Update the raw numerical data grid
		void updateRawGrid() const;
		
		void updateStashComboBox(wxComboBox *comboStash) const;
		//Update the 3D scene
		void updateScene(RefreshController *r); 

		//update a scene, simply using some streams and whether we should release the data
		void updateScene(std::list<std::vector<const FilterStreamData *> > &sceneData, 
				bool releaseData);
		//!Set the backend grid control for raw data
		void setRawGrid(wxGrid *theRawGrid){targetRawGrid=theRawGrid;};
		//!get the plot wrapper : TODO: Deprecate me
		PlotWrapper *getPlotWrapper(){return &targetPlots;};
		
		//Get a plot ID from the listbox position
		size_t getPlotID(size_t position) const ;

		//!Set the listbox for plot selection
		void setPlotList(wxListBox *box){plotSelList=box;};
		//!Set the text console
		void setConsole(wxTextCtrl *t) { textConsole = t;}
	
};

#endif
