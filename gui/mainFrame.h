/*
 * 	3Depict.h - main program header
 * 	Copyright (C) 2015 D Haley
 *
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
#include <wx/version.h> 

#if !(wxCHECK_VERSION(3,1,0))
	#if ( defined(__WIN32__) || defined(__WIN64__) )
		#error "Your wx version is too low for windows. You need wx 3.1 or greater, due to wx bug 16222."
	#else
		#define FIX_WXPROPGRID_16222
	#endif
#endif
// begin wxGlade: ::dependencies
#include <wx/splitter.h>
#include <wx/filename.h>
#include <wx/statline.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/listctrl.h>
#include <wx/docview.h>
#include <wx/dnd.h>
#include <wx/grid.h>
#include <wx/treectrl.h>
#include <wx/propgrid/propgrid.h>
#include <wx/filehistory.h>
// end wxGlade

//Local stuff
#include "wx/wxcommon.h"
#include "wx/wxcomponents.h"
#include "glPane.h"
#include "mathglPane.h"
#include "cropPanel.h" // cropping tools

#include "backend/viscontrol.h"
#include "backend/configFile.h"



#ifndef THREEDEPICT_H 
#define THREEDEPICT_H


class FileDropTarget;
class RefreshThread;

enum
{
	MESSAGE_ERROR=1,
	MESSAGE_INFO,
	MESSAGE_HINT, //lowest priority message in the queue. Only one HINT can be in queue at a time
	MESSAGE_NONE // pseudo-message to wipe all messages
};


//This is used to create and run a worker thread that will perform a refresh calculation
class RefreshThread: public wxThread
{
	private:
		RefreshController *refreshControl;
		wxWindow *targetWindow;
	public:
		RefreshThread(wxWindow *target,RefreshController *rc); 
		~RefreshThread();
		//!Used internally by wxwidgets to launch thread
		void *Entry();

		void abort() {ASSERT(false);}
};

class MainWindowFrame: public wxFrame {
public:
    // begin wxGlade: MainWindowFrame::ids
    // end wxGlade

    MainWindowFrame(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_FRAME_STYLE);
    virtual ~MainWindowFrame();

    //Drop the following files onto the given window XY coordinates.
    void OnDropFiles(const wxArrayString &files, int x, int y);

    bool isCurrentlyUpdatingScene() const { return currentlyUpdatingScene;};

    void linkCropWidgets();

    wxSize getNiceWindowSize() const ;

    //Obtain the filterId that is associated with the given tree node. 
    // returns false if it is not able to do so (eg invalid TreeItemId)
    bool getTreeFilterId(const wxTreeItemId &tId, size_t &filterId) const;

private:
    // begin wxGlade: MainWindowFrame::methods
    void set_properties();
    void do_layout();
    // end wxGlade
    //Force a re-layout of the filter property grid
    void do_filtergrid_prop_layout();
    //Force a re-layout of the camera property grid
    void do_cameragrid_prop_layout();
  
	bool refreshThreadActive() { return refreshThread && refreshThread->IsRunning();};
 
   	//!Queue up a status message for display
    	void showStatusMessage(const char *message, unsigned int messageType=MESSAGE_ERROR); 
   	
	//!Queue up a status message for display
    	void statusMessage(const char *message, unsigned int messageType=MESSAGE_ERROR); 

	//!Update the progress information in the status bar
	void updateProgressStatus();
	//!Perform an update to the 3D Scene. Returns false if refresh failed
	bool doSceneUpdate(bool ensureResultVisible=false);
	
	//!Complete the scene update. Returns false if failed
	void finishSceneUpdate(unsigned int errCode);

	//!Wrapper for viscontrol's update function, as we need to
	// prevent wx from firing events during tree update
	void updateWxTreeCtrl( wxTreeCtrl *t, const Filter *f=0);

	//!Update the post-processing effects in the 3D scene. 
	void updatePostEffects(); 
	//!Load a file into the panel given the full path to the file
	bool loadFile(const wxString &dataFile,bool merge=false,bool noUpdate=false);

	//!Load any errors that were detected in the last refresh into the filter tree
	void setFilterTreeAnalysisImages(); 

	//!Update the effects UI from some effects vector
	void updateFxUI(const std::vector<const Effect *> &fx);

	void setLockUI(bool amlocking,unsigned int lockMode);

	//Did the opengl panel initialise correctly?
	bool glPanelOK;
	//!Scene - user interaction interface "visualisation control"
	VisController visControl;

	//!Refresh control thread
	RefreshThread *refreshThread;
	//!Refresh control object
	RefreshController *refreshControl;

	//!Program on-disk configuration class
	ConfigFile configFile;

	//!Blocking bool to prevent functions from responding to programatically generated wx events
	bool programmaticEvent;
	//!A flag stating if the first update needs a refresh after GL window OK
	bool requireFirstUpdate;
	//!Have we set the combo cam/stash text in this session?
	bool haveSetComboCamText,haveSetComboStashText;
	//!Are we in the middle of updating the scene?
	bool currentlyUpdatingScene;
	//!Have we aborted an update
	bool haveAborted;
	//!Should the gui ensure that the refresh result is visible at the next update?
	bool ensureResultVisible;

	//!source item when dragging a filter in the tree control
	wxTreeItemId *filterTreeDragSource;

	//!Drag and drop functionality
	FileDropTarget *dropTarget;
	
	//!Current fullscreen status
	unsigned int fullscreenState;

	//!Did the main frame's constructor complete OK?
	bool initedOK;

	//Pointer to version check thread, occasionally initialised at startup to
	// check online for new program updates
	VersionCheckThread *verCheckThread;

	//Map to convert filter drop down choices to IDs
	std::map<std::string,size_t> filterMap;
   
	//TODO: Refactor -  remove me.
	// True if there are pending updates for the mathgl window
	bool plotUpdates;

	//List of pending messages to show in status bar
	// first int is priority (eg MESSAGE_ERROR), string is message
	std::list<std::pair<unsigned int, std::string > > statusQueue;
protected:

    wxTimer *statusTimer; //One-shot timer that is used to clear the status bar
    wxTimer *updateTimer; //Periodically calls itself to check for updates from user interaction
    wxTimer *progressTimer; //Periodically calls itself to refresh progress status
    wxTimer *autoSaveTimer; //Periodically calls itself to create an autosave state file
    wxMenuItem *checkMenuControlPane;
    wxMenuItem *checkMenuRawDataPane;
    wxMenuItem *checkMenuSpectraList;
    wxMenuItem *menuViewFullscreen;
    wxMenuItem *checkViewLegend;
    wxMenuItem *checkViewWorldAxis;

    wxMenuItem *editUndoMenuItem,*editRedoMenuItem;
    wxMenuItem *editRangeMenuItem;
    wxMenuItem *fileSave;
    wxMenu *recentFilesMenu;
    wxMenu *fileMenu;
    wxMenu *fileExport;
    wxFileHistory *recentHistory;
    ProgressData lastProgressData;


    // begin wxGlade: MainWindowFrame::attributes
    wxStaticBox* sizerAlignCam_staticbox;
    wxMenuBar* MainFrame_Menu;
    wxStatusBar* MainFrame_statusbar;
    wxStaticText* lblSettings;
    wxComboBox* comboStash;
    wxButton* btnStashManage;
    wxStaticLine* stashFilterStaticSep;
    wxStaticText* filteringLabel;
    wxComboBox* comboFilters;
    TextTreeCtrl* treeFilters;
    wxCheckBox* checkAutoUpdate;
    wxButton* refreshButton;
    wxButton* btnFilterTreeExpand;
    wxButton* btnFilterTreeCollapse;
    wxBitmapButton* btnFilterTreeErrs;
    wxPanel* filterTreePane;
    wxStaticText* propGridLabel;
    wxPropertyGrid* gridFilterPropGroup;
#ifdef FIX_WXPROPGRID_16222
    wxPropertyGrid *backFilterPropGrid;
#endif
    wxPanel* filterPropertyPane;
    wxSplitterWindow* filterSplitter;
    wxPanel* noteData;
    wxStaticText* labelCameraName;
    wxComboBox* comboCamera;
    wxButton* buttonRemoveCam;
    wxStaticLine* cameraNamePropertySepStaticLine;
    wxPropertyGrid* gridCameraProperties;
#ifdef FIX_WXPROPGRID_16222
    wxPropertyGrid* backCameraPropGrid;
#endif
    wxButton* buttonAlignCamXPlus;
    wxButton* buttonAlignCamYPlus;
    wxButton* buttonAlignCamZPlus;
    wxButton* buttonAlignCamXMinus;
    wxButton* buttonAlignCamYMinus;
    wxButton* buttonAlignCamZMinus;
    wxCheckBox* checkAlignCamResize;
    wxScrolledWindow* noteCamera;
    wxCheckBox* checkPostProcessing;
    wxCheckBox* checkFxCrop;
    wxCheckBox* checkFxCropCameraFrame;
    wxComboBox* comboFxCropAxisOne;
    CropPanel* panelFxCropOne;
    wxComboBox* comboFxCropAxisTwo;
    CropPanel* panelFxCropTwo;
    wxStaticText* labelFxCropDx;
    wxTextCtrl* textFxCropDx;
    wxStaticText* labelFxCropDy;
    wxTextCtrl* textFxCropDy;
    wxStaticText* labelFxCropDz;
    wxTextCtrl* textFxCropDz;
    wxPanel* noteFxPanelCrop;
    wxCheckBox* checkFxEnableStereo;
    wxStaticText* lblFxStereoMode;
    wxComboBox* comboFxStereoMode;
    wxStaticBitmap* bitmapFxStereoGlasses;
    wxStaticText* labelFxStereoBaseline;
    wxSlider* sliderFxStereoBaseline;
    wxCheckBox* checkFxStereoLensFlip;
    wxPanel* noteFxPanelStereo;
    wxNotebook* noteEffects;
    wxPanel* notePost;
    wxStaticText* labelAppearance;
    wxCheckBox* checkAlphaBlend;
    wxCheckBox* checkLighting;
    wxStaticLine* static_line_1;
    wxStaticText* labelPerformance;
    wxCheckBox* checkWeakRandom;
    wxCheckBox* checkLimitOutput;
    wxTextCtrl* textLimitOutput;
    wxCheckBox* checkCaching;
    wxStaticText* labelMaxRamUsage;
    wxSpinCtrl* spinCachePercent;
    wxPanel* noteTools;
    wxNotebook* notebookControl;
    wxPanel* panelLeft;
    wxPanel* panelView;
    BasicGLPane* panelTop;
    MathGLPane* panelSpectra;
    wxStaticText* plotListLabel;
    wxListBox* plotList;
    wxPanel* window_2_pane_2;
    wxSplitterWindow* splitterSpectra;
    CopyGrid* gridRawData;
    wxButton* btnRawDataSave;
    wxButton* btnRawDataClip;
    wxPanel* noteRaw;
    wxTextCtrl* textConsoleOut;
    wxPanel* noteDataViewConsole;
    wxNotebook* noteDataView;
    wxPanel* panelBottom;
    wxSplitterWindow* splitTopBottom;
    wxPanel* panelRight;
    wxSplitterWindow* splitLeftRight;
    // end wxGlade

    //Set the state for the state menu
    void setSaveStatus();
    //Perform camera realignment
    void realignCameraButton(unsigned int direction);


    DECLARE_EVENT_TABLE();

public:
    void OnFileOpen(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFileMerge(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFileSave(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFileSaveAs(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFileExportPlot(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFileExportImage(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFileExportIons(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFileExportRange(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnViewBackground(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFileExit(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnEditUndo(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnEditRedo(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnEditRange(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnEditPreferences(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnViewControlPane(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnViewRawDataPane(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnHelpHelp(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnHelpContact(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnHelpAbout(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnComboStashText(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnComboStashEnter(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnComboStash(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnButtonStashDialog(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnTreeEndDrag(wxTreeEvent &event); // wxGlade: <event_handler>
    void OnTreeKeyDown(wxKeyEvent &event); // wxGlade: <event_handler>
    void OnTreeDeleteItem(wxTreeEvent &event); // wxGlade: <event_handler>
    void OnTreeSelectionChange(wxTreeEvent &event); // wxGlade: <event_handler>
    void OnTreeBeginDrag(wxTreeEvent &event); // wxGlade: <event_handler>
    void OnTreeSelectionPreChange(wxTreeEvent &event); // wxGlade: <event_handler>
    void OnBtnExpandTree(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnBtnCollapseTree(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnBtnFilterTreeErrs(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnComboCameraText(wxCommandEvent &event); // wxGlade: <event_handler>

    void OnGridFilterPropertyChange(wxPropertyGridEvent &event); // wxGlade: <event_handler>
    void OnGridFilterDClick(wxPropertyGridEvent &event); // wxGlade: <event_handler>
    void OnComboCameraEnter(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnComboCamera(wxCommandEvent &event); // wxGlade: <event_handler>
    

   
    void OnButtonRemoveCam(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnButtonAlignCameraXPlus(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnButtonAlignCameraYPlus(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnButtonAlignCameraZPlus(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnButtonAlignCameraXMinus(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnButtonAlignCameraYMinus(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnButtonAlignCameraZMinus(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnCheckPostProcess(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFxCropCheck(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFxCropCamFrameCheck(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFxCropAxisOne(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFxCropAxisTwo(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFxStereoEnable(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFxStereoCombo(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnFxStereoBaseline(wxScrollEvent &event); // wxGlade: <event_handler>
    void OnFxStereoLensFlip(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnCheckAlpha(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnCheckLighting(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnCheckWeakRandom(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnSpectraListbox(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnCheckLimitOutput(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnTextLimitOutput(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnTextLimitOutputEnter(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnCheckCacheEnable(wxCommandEvent &event); // wxGlade: <event_handler>
    void OnCacheRamUsageSpin(wxSpinEvent &event); // wxGlade: <event_handler>

    void OnComboFilterEnter(wxCommandEvent &event); // 
    void OnComboFilter(wxCommandEvent &event); // 
    void OnComboFilterText(wxCommandEvent &event); // wxGlade: <event_handler>
    
    void OnStatusBarTimer(wxTimerEvent &event); // 
    void OnProgressTimer(wxTimerEvent &event);
    void OnProgressAbort(wxCommandEvent &event);
    void OnViewFullscreen(wxCommandEvent &event);
    void OnButtonRefresh(wxCommandEvent &event);
    void OnButtonGridCopy(wxCommandEvent &event);
    void OnButtonGridSave(wxCommandEvent &event);
    void OnRawDataUnsplit(wxSplitterEvent &event);
    void OnFilterPropDoubleClick(wxSplitterEvent &event);
    void OnControlUnsplit(wxSplitterEvent &event);
    void OnControlSplitMove(wxSplitterEvent &event);
    void OnFilterSplitMove(wxSplitterEvent &event);
    void OnTopBottomSplitMove(wxSplitterEvent &event);
    void OnSpectraUnsplit(wxSplitterEvent &event);
    void OnViewSpectraList(wxCommandEvent &event); 
    void OnViewPlotLegend(wxCommandEvent &event); 
    void OnViewWorldAxis(wxCommandEvent &event); 
    void OnClose(wxCloseEvent &evt);
    void OnComboCameraSetFocus(wxFocusEvent &evt);
    void OnComboStashSetFocus(wxFocusEvent &evt);
    void OnNoteDataView(wxNotebookEvent &evt);
    void OnGridCameraPropertyChange(wxPropertyGridEvent &event); // wxGlade: <event_handler>

    void OnFileExportVideo(wxCommandEvent &event);
    void OnFileExportFilterVideo(wxCommandEvent &event);
    void OnFileExportPackage(wxCommandEvent &event);
    void OnRecentFile(wxCommandEvent &event); // wxGlade: <event_handler>

    void OnTreeBeginLabelEdit(wxTreeEvent &evt);
    void OnTreeEndLabelEdit(wxTreeEvent &evt);
    
    void OnUpdateTimer(wxTimerEvent &evt);
    void OnAutosaveTimer(wxTimerEvent &evt);

    void OnCheckUpdatesThread(wxCommandEvent &evt);
    void OnFinishRefreshThread(wxCommandEvent &evt);

#ifdef FIX_WXPROPGRID_16222
    void OnIdle(wxIdleEvent &evt);
#endif
    void SetCommandLineFiles(wxArrayString &files);

    //return type of file, based upon heuristic check
    static unsigned int guessFileType(const std::string &file);


    //Check to see if the user wants a tip file
    void checkShowTips();

    //See if the user wants to save the current state
    void checkAskSaveState();
    
    //Check to see if we need to reload an autosave file (and reload it, as needed)
    void checkReloadAutosave();

    //Restore user UI defaults from config file (except panel defaults, which
    // due to wx behviour need to be done after window show)
    void restoreConfigDefaults();
    //Restore panel layout defaults
    void restoreConfigPanelDefaults();

    void onPanelSpectraUpdate() {plotUpdates=true;} ;

    bool initOK() const {return initedOK;}
    
    void finaliseStartup();

    //This is isolated from the layout code, due to "bug" 4815 in wx. The splitter window
    //does not know how to choose a good size until the window is shown
    void fixSplitterWindow() { 
	    	filterSplitter->SplitHorizontally(filterTreePane,filterPropertyPane);
	    	restoreConfigPanelDefaults();
	   	};


    //Update the enabled status for the range entry in the edit menu
    void updateEditRangeMenu();

}; // wxGlade: end class


class FileDropTarget : public wxFileDropTarget
{
private:
	MainWindowFrame *frame;
public:
	FileDropTarget(MainWindowFrame *f) {
		frame = f;
	}

	virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& files)
	{
		frame->OnDropFiles(files, x, y);

		return true;
	};

};

#endif 
