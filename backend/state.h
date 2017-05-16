/*
 *	state.h - user session state handler
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

#ifndef STATE_H
#define STATE_H

#include <string>
#include <vector>

#include "gl/cameras.h"
#include "gl/effect.h"

#include <wx/thread.h>

#include "tree.hh"
#include "filtertree.h"
#include "filtertreeAnalyse.h"

#include "animator.h"

//Unit tests
#ifdef DEBUG
bool runStateTests();
#endif

void setStateModifyLevel(int newLevel);
int getStateModifyLevel();

enum
{
	STATE_MODIFIED_NONE=0,
	STATE_MODIFIED_VIEW, // the 3D view has chaged
	STATE_MODIFIED_ANCILLARY, //Eg stashes, inactive cameras, and other things that might get saved
	STATE_MODIFIED_DATA // actual data output is latered
};


class TreeState
{
	private:
		//!currently active tree
		FilterTree filterTree;
		
		//!ID handler that assigns each filter its own ID that
		// is guaranteed to be unique for the life of the filter
		// in the filterTree. These are not guaranteed to be dense
		//TODO: Can we const the filter pointer?
		std::map<size_t, Filter * > filterMap;	

		//!Undo/redo stack for current state
		std::deque<FilterTree> undoFilterStack,redoFilterStack;
	
		FilterTreeAnalyse fta;

		std::vector<SelectionDevice *> selectionDevices;

		void applyBindings(const std::vector<std::pair<const Filter *,SelectionBinding> > &bindings);

		//!True if there are pending updates from the user
		bool pendingUpdates;

		//re-entry catcher
		wxMutex amRefreshing;	

		//Do we want to abort the refresh? This is passed to the tree
		// to signal if the primary thread would like to abort
		ATOMIC_BOOL wantAbort;
	public:
		TreeState() {pendingUpdates=false; wantAbort=false;}
		
		void operator=(const TreeState &otherState);  

		unsigned int refresh(std::list<FILTER_OUTPUT_DATA> &outData,
				std::vector<std::pair<const Filter*, std::string> > &consoleMessages, ProgressData &prog);

		//set the abort flag 
		void setAbort() { wantAbort=true;}
		//are we refreshing?
		bool isRefreshing() const { return filterTree.isRefreshing();}
		//!Inform that it has new updates to filters from external sources (eg bindings)
		void setUpdates() { pendingUpdates=true;};

		//!Returns true if the filter tree has updates that need to be processed
		bool hasUpdates() const; 
		
		//!Returns true if the filter tree has updates via a filter monitor 
		bool hasMonitorUpdates() const; 

		//Obtain a clone of the active filter tree
		void cloneFilterTree(FilterTree &f) const {f=filterTree;};

		const FilterTree &getTreeRef() const { return filterTree ;};

		//!Add a new filter to the tree. Set isbase=false and parentID for not
		//setting a parent (ie making filter base)
		void addFilter(Filter *f, bool isBase, size_t parentId);
		
		//!Add a new subtree to the tree. Note that the tree will be cleared
		// as a result of this operation. Control of all pointers will be handled internally.
		// Currently, If you wish to use ::getFilterById you *must* rebuild the tree control with
		// ::updateWxTreeCtrl. This should be fixed.
		void addFilterTree(FilterTree &f,bool isBase=true, 
						size_t parentId=(unsigned int)-1); 

		//!Grab the filter tree from the internal one, and swap the 
		// internal with a cloned copy of the internal.
		// Can be used eg, to steal the cache
		// Note that the contents of the incoming filter tree will be destroyed.
		//  -> This implies the tree comes *OUT* of viscontrol,
		//     and a tree  cannot be inserted in via this function
		void switchoutFilterTree(FilterTree &f);

		//Perform a swap operation on the filter tree. 
		// - *must* have same topology, or you must call updateWxTreeCtrl
		// - can be used to *insert* a tree into this function
		void swapFilterTree(FilterTree &f) { f.swap(filterTree);}

		void swapFilterMap(std::map<size_t,Filter*> &m) { filterMap.swap(m);}

		//!Duplicate a branch of the tree to a new position. Do not copy cache,
		bool copyFilter(size_t toCopy, size_t newParent,bool copyToRoot=false) ;

		//TODO: Deprecate me - filter information should not be leaking like this!
		//Get the ID of the filter from its actual pointer
		size_t getIdByFilter(const Filter* f) const;

		const Filter* getFilterById(size_t filterId) const; 

		//!Return all of a given type of filter from the filter tree. Type must be the exact type of filter - it is not a mask
		void getFiltersByType(std::vector<const Filter *> &filters, unsigned int type)  const;

		//!Return the number of filters currently in the main tree
		size_t numFilters() const { return filterTree.size();};

		//!Clear the cache for the filters
		void purgeFilterCache() { filterTree.purgeCache();};

		//!Delete a filter and all its children
		void removeFilterSubtree(size_t filterId);

		//Move a filter from one part of the tree to another
		bool reparentFilter(size_t filterID, size_t newParentID);

		//!Set the properties using a key-value result 
		/*
		 * The return code tells whether to reject or accept the change. 
		 * need update tells us if the change to the filter resulted in a change to the scene
		 */
		bool setFilterProperty(size_t filterId,unsigned int key,
				const std::string &value, bool &needUpdate);
	
		//!Set the filter's string	
		void setFilterString(size_t id, const std::string &s);
		//Modify rangefiles pointed to by given map to new Rangefile (second pointer)
		void modifyRangeFiles(const std::map<const RangeFile *, const RangeFile *> &toModify) { filterTree.modifyRangeFiles(toModify);};
		
		//!Clear all caches
		void clearCache();
		
		//!Clear all caches
		void clearCacheByType(unsigned int type) { filterTree.clearCacheByType(type);};

		void clear() { filterTree.clear();filterMap.clear() ;fta.clear(); } 

		size_t size() const { return filterTree.size(); }

		//Push the filter tree undo stack
		void pushUndoStack();

		//Pop the filter tree undo stack. If restorePopped is true,
		// then the internal filter tree is updated with the stack tree
		void popUndoStack(bool restorePopped=true);

		//Pop the redo stack, this unconditionally enforces an update of the
		// active internal tree
		void popRedoStack();

		//Obtain the size of the undo stack
		size_t getUndoSize() const { return undoFilterStack.size();};
		//obtain the size of the redo stack
		size_t getRedoSize() const { return redoFilterStack.size();};

		//Clear undo/redo filter tree stacks
		void clearUndoRedoStacks() { undoFilterStack.clear(); redoFilterStack.clear();}

		void stripHazardousContents() { filterTree.stripHazardousContents();}

		//!Apply external filter modifications that have been changed due to bindings
		void applyBindingsToTree();
		
		//!Get the analysis results for the last refresh
		void getAnalysisResults(std::vector<FILTERTREE_ERR> &res) const { fta.getAnalysisResults(res);}
	
		//!Set the cache maximum ram usage (0->100) 
		void setCachePercent(unsigned int newCache);
			
		bool hasStateOverrides() const { return filterTree.hasStateOverrides();}
	
		//Return the selection devices obtained from the last refresh
		std::vector<SelectionDevice *> &getSelectionDevices() { return selectionDevices;};
	
};

//The underlying data for any given state in the analysis toolchain
class AnalysisState
{
	private:

		//Items that should be written to file
		// on state save
		//===
		//!Viewing cameras for looking at results
		std::vector<Camera *> savedCameras;

		//!Filter trees that have been designated as inactive, but
		// user would like to have them around for use
		std::vector<std::pair<std::string,FilterTree> > stashedTrees;


		//Scene modification 3D Effects 
		std::vector<const Effect *> effects;

		//Background colours
		float rBack,gBack,bBack;
		
		//Viewing mode for the world indication axes
		int worldAxisMode;

		//Camera user has currently activated
		size_t activeCamera;

		//Should the plot legend be enabled
		bool plotLegendEnable;

		//Filter path and ID of plots that need to be enabled at startup 
		std::vector<std::pair<std::string,unsigned int> > enabledStartupPlots;

		//true if system should be using relative paths when
		// saving state
		bool useRelativePathsForSave;
		
		//!Working directory for saving
		std::string workingDir;
		//===


		//file to save to
		std::string fileName;
		
		
	
		//!User-set animation properties
		PropertyAnimator animationState;

		//TODO: Migrte into some state wrapper class with animationState
		//Additional state information for animation
		std::vector<std::pair<std::string,size_t>  > animationPaths;

		bool camNameExists(const std::string &s)  const ;

		//Clear the effect vector
		void clearEffects();
		
		//Clear the camera data vector
		void clearCams();

		//Actual load function for loading internal state
		// this is used by ::load to mitigate actual "state" 
		// class instance modifications on failure
		bool loadInternal(const char *cpFilename,  bool merge,
				std::ostream &errStream);
#ifdef DEBUG
		void checkSane() const;
#endif
	public:

		TreeState treeState;

		AnalysisState();

		~AnalysisState();

		//Wipe the state clean
		void clear();


		void operator=(const AnalysisState &oth);

		

		//Load an XML representation of the analysis state
		// - returns true on success, false on fail
		// - errStream will have human readable messages in 
		//	the case that there is a failure
		// - merge will attempt to join the 
		bool load(const char *cpFilename,  bool merge,
				std::ostream &errStream);

		//save an XML-ised representation of the analysis sate
		//	- Provides the on-disk to local name
		//      mapping to use when saving. This needs to be copied by
		//     the caller into the same dir as the XML file to be usable
		// 	- write package says if state should attempt to ensure that output
		// 		state is fully self-contained, and locally referenced
		bool save(const char *cpFilename, std::map<std::string,std::string> &fileMapping,
				bool writePackage,bool setModifyLevel=true) const ;

		//Combine a separate state file into this one, avoiding clashes
		void merge(const AnalysisState &srcState);

		//Return the current state's filename
		std::string getFilename() const { return fileName; }
		//Return the current state's filename
		void setFilename(std::string &s) {fileName=s; }
	
		//obtain the world axis display state
		int getWorldAxisMode() const;



		//obtain the scene background colour
		void getBackgroundColour(float &r, float &g, float &b) const;


		//Set the background colour for the 
		void setBackgroundColour(float r, float g, float b);

		//set the display mode for the world XYZ axes
		void setWorldAxisMode(unsigned int mode);
	
		// === Cameras ===
		//Set the camera vector, clearing any existing cams
		// note that control of pointers will be taken
		void setCamerasByCopy(std::vector<Camera *> &c, unsigned int active);


		void setCameraByClone(const Camera *c, unsigned int offset) ;

		//Obtain the ID of the active camera
		size_t getActiveCam() const  { ASSERT(activeCamera < savedCameras.size()) ; return activeCamera;};

		//Set
		void setActiveCam(size_t offset) {ASSERT(offset < savedCameras.size()); activeCamera=offset; };

		//Remove the  camera at the specified offset
		void removeCam(size_t offset);
		
		const Camera *getCam(size_t offset) const;
		//Obtain a copy of the internal camera vector.
		// - must delete the copy manually.
		void copyCams(std::vector<Camera *> &cams) const;

		//Obtain a copy of the internal camera vector.
		// note that this reference has limited validity, and may be
		// invalidated if the state is modified
		void copyCamsByRef(std::vector<const Camera *> &cams) const;

		size_t getNumCams() const { return savedCameras.size();}

		//Add a camera by cloning an existing camera
		void addCamByClone(const Camera *c);

		bool setCamProperty(size_t offset, unsigned int key, const std::string &value);

		std::string getCamName(size_t offset) const; 

		//!Add a new camera to the scene
		void addCam(const std::string &camName, bool makeActive=false);
		//=====

		//Effect functions
		//===

		//Set the effect vector
		void setEffectsByCopy(const std::vector<const Effect *> &e);

		//Copy the internal effect vector. 
		//	-Must manually delete each pointer
		void copyEffects(std::vector<Effect *> &effects) const;
		//===

		//Plotting functions
		//=======

		void setPlotLegend(bool enabled) {plotLegendEnable=enabled;}
		void setEnabledPlots(const std::vector<std::pair<std::string,unsigned int> > &enabledPlots) {enabledStartupPlots = enabledPlots;}

		void getEnabledPlots(std::vector<std::pair<std::string,unsigned int> > &enabledPlots) const { enabledPlots=enabledStartupPlots;}

		//Set whether to use relative paths in saved file
		void setUseRelPaths(bool useRel);
		//get whether to use relative paths in saved file
		bool getUseRelPaths() const;

		//Set the working directory to be specified when using relative paths
		void setWorkingDir(const std::string &work);
		//Set the working directory to be specified when using relative paths
		std::string getWorkingDir() const { return workingDir;};

		///Set the stashed filters to use internally
		void setStashedTreesByClone(const std::vector<std::pair<std::string,FilterTree> > &s);

		//Add an element to the stashed filters
		void addStashedTree( const std::pair<std::string,FilterTree> &);
	
		//!Transform the subtree at the given point into a stash, and save it
		void stashFilters(unsigned int filterId, const char *stashName);

		//Retrieve the specified stashed filter
		void copyStashedTree(size_t offset, std::pair<std::string,FilterTree > &) const;
		void copyStashedTree(size_t offset, FilterTree &) const;

		//retrieve all stashed filters
		void copyStashedTrees(std::vector<std::pair<std::string,FilterTree> > &stashList) const;

		//!Insert  the given stash into the tree as a child of the given parent filter
		void addStashedToFilters(const Filter *parentFilter, unsigned int stashOffset);
		//Remove the stash at the specified offset. Numbers will
		// be reset, so previous offsets will no longer be valid
		void eraseStash(size_t offset);
		//Remove the given stashew at the specified offsets
		void eraseStashes(std::vector<size_t> &offset);

		//Return the number of stash elements
		size_t getStashCount()  const { return stashedTrees.size();}

		//Get the stash name
		std::string getStashName(size_t offset) const;

		
		//Returns true if there is any data in the stash or the active tree
		bool hasStateData() const { return (stashedTrees.size() || treeState.size());}
		//!Returns true if any of the filters (incl. stash)
		//return a state override (i.e. refer to external entities, such as files)
		bool hasStateOverrides() const ;


		void setAnimationState(const PropertyAnimator &p,const std::vector<std::pair<std::string,size_t> > &animPth) {animationState=p;animationPaths=animPth;}
		
		void getAnimationState( PropertyAnimator &p, std::vector<std::pair<std::string,size_t> > &animPth) const; 


};
#endif
