/*
 *	mainFrame.cpp - Main 3Depict window
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

#ifdef __APPLE__
//FIXME: workaround for UI layout under apple platform 
// wxMac appears to have problems with nested panels.
#define APPLE_EFFECTS_WORKAROUND  1
#endif

enum
{ 
	WINDOW_LOCK_REFRESH,
	WINDOW_LOCK_PROPEDIT,
	WINDOW_LOCK_NONE
};


//OS specific stuff
#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif

#include "mainFrame.h"

//wxWidgets stuff
#include <wx/colordlg.h>
#include <wx/aboutdlg.h> 
#include <wx/progdlg.h>
#include <wx/display.h>
#include <wx/process.h>
#include <wx/dir.h>
#include <wx/imaglist.h>
#include <wx/stdpaths.h>
#include <wx/tipdlg.h>

#include <wx/utils.h>  // Needed for wxLaunchDefaultApplication
//extra imports
#undef I // Not sure who is defining this, but it is causing problems - mathgl?
#include "common/voxels.h"

//Custom program dialog windows
#include "gui/dialogs/StashDialog.h" //Stash editor
#include "gui/dialogs/resolutionDialog.h" // resolution selection dialog
#include "gui/dialogs/ExportRngDialog.h" // Range export dialog
#include "gui/dialogs/ExportPos.h" // Ion export dialog
#include "gui/dialogs/prefDialog.h" // Preferences dialog
#include "gui/dialogs/autosaveDialog.h" // startup autosave dialog for multiple load options
#include "gui/dialogs/filterErrorDialog.h" // Dialog for displaying details for filter analysis error messages
#include "gui/dialogs/animateFilterDialog.h" // Dialog for performing property sweeps on filters
#include "gui/dialogs/rangeEditDialog.h" // Dialog for performing editing rangefiles

#include "common/stringFuncs.h"

//Program Icon
#include "art.h"

//Filter imports
#include "backend/filters/rangeFile.h"
#include "backend/filters/dataLoad.h"
#include "wx/propertyGridUpdater.h"

#include <vector>
#include <string>
#include <utility>
#include <map>
#include <list>
#include <stack>

using std::vector;
using std::string;
using std::pair;
using std::map;
using std::make_pair;
using std::list;
using std::max;
using std::stack;

//milliseconds before clearing status bar (by invoking a status timer event)
const unsigned int STATUS_TIMER_DELAY=10000; 
//Milliseconds between querying viscontrol for needing update
const unsigned int UPDATE_TIMER_DELAY=50; 
//Milliseconds between progress bar updates 
const unsigned int PROGRESS_TIMER_DELAY=40; 
//Seconds between autosaves
const unsigned int AUTOSAVE_DELAY=180; 

//Default window size
const unsigned int DEFAULT_WIN_WIDTH=1024;
const unsigned int DEFAULT_WIN_HEIGHT=800;

//minimum startup window size
const unsigned int MIN_WIN_WIDTH=100;
const unsigned int MIN_WIN_HEIGHT=100;



//!Number of pages in the panel at the bottom
const unsigned int NOTE_CONSOLE_PAGE_OFFSET= 2;

//The conversion factor from the baseline shift slider to camera units
const float BASELINE_SHIFT_FACTOR=0.0002f;


const char *cameraIntroString=NTRANS("New camera name...");
const char *stashIntroString=NTRANS("New stash name...");
#if (defined(__WIN32) || defined(__WIN64))
//Being non-empty string causes segfault under wine. Don't know why
const char *ADD_FILTER_TEXT="";
#else
const char *ADD_FILTER_TEXT=NTRANS("New Filter...");
#endif

//Name of autosave state file. MUST end in .xml middle
const char *AUTOSAVE_PREFIX= "autosave.";
const char *AUTOSAVE_SUFFIX=".xml";


//This is the dropdown matching list. This must match
//the order for comboFilters_choices, as declared in 
//MainFrame's constructor

//--- These settings must be modified concomitantly.
const unsigned int FILTER_DROP_COUNT=15;

const char * comboFilters_choices[FILTER_DROP_COUNT] =
{
	NTRANS("Annotation"),
	NTRANS("Bounding Box"),
	NTRANS("Clipping"),
	NTRANS("Cluster Analysis"),
	NTRANS("Compos. Profiles"),
	NTRANS("Downsampling"),
	NTRANS("Extern. Prog."),
	NTRANS("Ion Colour"),
	NTRANS("Ion Info"),
	NTRANS("Ion Transform"),
	NTRANS("Spectrum"),
	NTRANS("Range File"),
	NTRANS("Spat. Analysis"),
	NTRANS("Voxelisation"),
	NTRANS("Proxigram"),
};

//Mapping between filter ID and combo position
const unsigned int comboFiltersTypeMapping[FILTER_DROP_COUNT] = {
	FILTER_TYPE_ANNOTATION,
	FILTER_TYPE_BOUNDBOX,
	FILTER_TYPE_IONCLIP,
	FILTER_TYPE_CLUSTER_ANALYSIS,
	FILTER_TYPE_PROFILE,
	FILTER_TYPE_IONDOWNSAMPLE,
	FILTER_TYPE_EXTERNALPROC,
	FILTER_TYPE_IONCOLOURFILTER,
	FILTER_TYPE_IONINFO,
	FILTER_TYPE_TRANSFORM,
	FILTER_TYPE_SPECTRUMPLOT,
	FILTER_TYPE_RANGEFILE,
	FILTER_TYPE_SPATIAL_ANALYSIS,
	FILTER_TYPE_VOXELS,
	FILTER_TYPE_PROXIGRAM,
 };
//----


//Constant identifiers for binding events in wxwidgets "event table"
enum {

	//File menu
	ID_MAIN_WINDOW= wxID_ANY+1000, // There is a bug under MSW where wxID_ANY+1 causes collisions with some controls with implicit IDs...

	ID_FILE_EXIT,
	ID_FILE_OPEN,
	ID_FILE_MERGE,
	ID_FILE_SAVE,
	ID_FILE_SAVEAS,
	ID_FILE_EXPORT_PLOT,
	ID_FILE_EXPORT_IMAGE,
	ID_FILE_EXPORT_IONS,
	ID_FILE_EXPORT_RANGE,
	ID_FILE_EXPORT_ANIMATION,
	ID_FILE_EXPORT_FILTER_ANIMATION,
	ID_FILE_EXPORT_PACKAGE,

	//Edit menu
	ID_EDIT_UNDO,
	ID_EDIT_REDO,
	ID_EDIT_RANGE,
	ID_EDIT_PREFERENCES,

	//Help menu
	ID_HELP_ABOUT,
	ID_HELP_HELP,
	ID_HELP_CONTACT,

	//View menu
	ID_VIEW_BACKGROUND,
	ID_VIEW_CONTROL_PANE,
	ID_VIEW_RAW_DATA_PANE,
	ID_VIEW_SPECTRA,
	ID_VIEW_PLOT_LEGEND,
	ID_VIEW_WORLDAXIS,
	ID_VIEW_FULLSCREEN,
	//Left hand note pane
	ID_NOTEBOOK_CONTROL,
	ID_NOTE_CAMERA,
	ID_NOTE_DATA,
	ID_NOTE_PERFORMANCE,
	ID_NOTE_TOOLS,
	ID_NOTE_VISUALISATION,
	//Lower pane
	ID_PANEL_DATA,
	ID_PANEL_VIEW,
	ID_NOTE_SPECTRA,
	ID_NOTE_RAW,
	ID_GRID_RAW_DATA,
	ID_BUTTON_GRIDCOPY,
	ID_LIST_PLOTS,

	//Splitter IDs
	ID_SPLIT_LEFTRIGHT,
	ID_SPLIT_FILTERPROP,
	ID_SPLIT_TOP_BOTTOM,
	ID_SPLIT_SPECTRA,
	ID_RAWDATAPANE_SPLIT,
	ID_CONTROLPANE_SPLIT,

	//Camera panel 
	ID_COMBO_CAMERA,
	ID_GRID_CAMERA_PROPERTY,
	ID_BUTTON_ALIGNCAM_XMINUS,
	ID_BUTTON_ALIGNCAM_XPLUS,
	ID_BUTTON_ALIGNCAM_YMINUS,
	ID_BUTTON_ALIGNCAM_YPLUS,
	ID_BUTTON_ALIGNCAM_ZMINUS,
	ID_BUTTON_ALIGNCAM_ZPLUS,



	//Filter panel 
	ID_COMBO_FILTER,
	ID_COMBO_STASH,
	ID_BTN_STASH_MANAGE,
	ID_CHECK_AUTOUPDATE,
	ID_FILTER_NAMES,
	ID_GRID_FILTER_PROPERTY,
	ID_LIST_FILTER,
	ID_TREE_FILTERS,
	ID_BUTTON_REFRESH,
	ID_BTN_EXPAND,
	ID_BTN_COLLAPSE,
	ID_BTN_FILTERTREE_ERRS,

	//Effects panel
	ID_EFFECT_ENABLE,
	ID_EFFECT_CROP_ENABLE,
	ID_EFFECT_CROP_AXISONE_COMBO,
	ID_EFFECT_CROP_PANELONE,
	ID_EFFECT_CROP_PANELTWO,
	ID_EFFECT_CROP_AXISTWO_COMBO,
	ID_EFFECT_CROP_CHECK_COORDS,
	ID_EFFECT_CROP_TEXT_DX,
	ID_EFFECT_CROP_TEXT_DY,
	ID_EFFECT_CROP_TEXT_DZ,
	ID_EFFECT_STEREO_ENABLE,
	ID_EFFECT_STEREO_COMBO,
	ID_EFFECT_STEREO_BASELINE_SLIDER,
	ID_EFFECT_STEREO_LENSFLIP,

	//Options panel
	ID_CHECK_ALPHA,
	ID_CHECK_LIGHTING,
	ID_CHECK_LIMIT_POINT_OUT,
	ID_TEXT_LIMIT_POINT_OUT,
	ID_CHECK_CACHING,
	ID_CHECK_WEAKRANDOM,
	ID_SPIN_CACHEPERCENT,

	//Misc
	ID_PROGRESS_ABORT,
	ID_STATUS_TIMER,
	ID_PROGRESS_TIMER,
	ID_UPDATE_TIMER,
	ID_AUTOSAVE_TIMER,


};

enum
{
	FILE_OPEN_TYPE_UNKNOWN=1,
	FILE_OPEN_TYPE_XML=2,
	FILE_OPEN_TYPE_POS=4,
	FILE_OPEN_TYPE_TEXT=8,
	FILE_OPEN_TYPE_LAWATAP_ATO=16,
};


void setWxTreeImages(wxTreeCtrl *t, const map<size_t, wxArtID> &artFilters)
{
#if defined(__WIN32) || defined(__WIN64) || defined(__APPLE__)
	const int winTreeIconSize=9;
	wxImageList *imList = new wxImageList(winTreeIconSize,winTreeIconSize);
#else
	wxImageList *imList = new wxImageList;
#endif
	//map to map wxArtIDs to position in the image list
	map<wxArtID,size_t> artMap;

	size_t offset=0;
	//Construct an image list for the tree
	for(map<size_t,wxArtID>::const_iterator it=artFilters.begin();it!=artFilters.end();++it)
	{
		#if defined(__WIN32) || defined(__WIN64) || defined(__APPLE__)

			imList->Add(wxBitmap(wxBitmap(wxArtProvider::GetBitmap(it->second)).
						ConvertToImage().Rescale(winTreeIconSize, winTreeIconSize)));
		#else
			imList->Add(wxArtProvider::GetBitmap(it->second));
		#endif

		artMap[it->second] = offset;
		offset++;
	}

	//Assign the image list - note wx will delete the image list here - we don't need to.
	t->AssignImageList(imList);

	//Now do a DFS run through the tree, checking to see if any of the elements need
	// a particular image
	std::stack<wxTreeItemId> items;
	if (t->GetRootItem().IsOk())
		items.push(t->GetRootItem());

	while (!items.empty())
	{
		wxTreeItemId next = items.top();
		items.pop();

		//Get the filter pointer
		//--
		size_t tItemVal;
		wxTreeItemData *tData;
		
		tData=t->GetItemData(next);
		tItemVal=((wxTreeUint *)tData)->value;
		//--


		map<size_t,wxArtID>::const_iterator it;
		it = artFilters.find(tItemVal);

		if (next != t->GetRootItem() && it!=artFilters.end())
			t->SetItemImage(next,artMap[it->second]);
		else
		{
			t->SetItemImage(next,-1);
		}

		wxTreeItemIdValue cookie;
		wxTreeItemId nextChild = t->GetFirstChild(next, cookie);
		while (nextChild.IsOk())
		{
			items.push(nextChild);
			nextChild = t->GetNextSibling(nextChild);
		}
	}

	return;
}

void clearWxTreeImages(wxTreeCtrl *t)
{
	t->AssignImageList(NULL);
}

MainWindowFrame::MainWindowFrame(wxWindow* parent, int id, const wxString& title, const wxPoint& pos, const wxSize& size, long style):
    wxFrame(parent, id, title, pos, size, style)
{
	COMPILE_ASSERT( (THREEDEP_ARRAYSIZE(comboFilters_choices) + 1 ) == FILTER_TYPE_ENUM_END);

	initedOK=false;
	plotUpdates=false;
	programmaticEvent=false;
	fullscreenState=0;
	verCheckThread=0;
	refreshThread=0;
	refreshControl=0;
	ensureResultVisible=false;
	lastProgressData.reset();

	//Set up the program icon handler
	wxArtProvider::Push(new MyArtProvider);
	SetIcon(wxArtProvider::GetIcon(wxT("MY_ART_ID_ICON")));

	//Set up the drag and drop handler
	dropTarget = new FileDropTarget(this);
	SetDropTarget(dropTarget);


	//Set up the recently used files menu
	// Note that this cannot exceed 9. Items show up, but do not trigger events.
	// This is bug 12141 in wxWidgets : http://trac.wxwidgets.org/ticket/12141
	ASSERT(configFile.getMaxHistory() <=9);
	recentHistory= new wxFileHistory(configFile.getMaxHistory());


	programmaticEvent=false;
	currentlyUpdatingScene=false;
	statusTimer = new wxTimer(this,ID_STATUS_TIMER);
	updateTimer= new wxTimer(this,ID_UPDATE_TIMER);
	progressTimer= new wxTimer(this,ID_PROGRESS_TIMER);
	autoSaveTimer= new wxTimer(this,ID_AUTOSAVE_TIMER);
	requireFirstUpdate=true;


	//Set up keyboard shortcuts "accelerators"
	wxAcceleratorEntry entries[1];
	entries[0].Set(0,WXK_ESCAPE,ID_PROGRESS_ABORT);
	wxAcceleratorTable accel(1, entries);
        SetAcceleratorTable(accel);

	// begin wxGlade: MainWindowFrame::MainWindowFrame
    splitLeftRight = new wxSplitterWindow(this, ID_SPLIT_LEFTRIGHT, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    panelRight = new wxPanel(splitLeftRight, wxID_ANY);
    splitTopBottom = new wxSplitterWindow(panelRight, ID_SPLIT_TOP_BOTTOM, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    noteDataView = new wxNotebook(splitTopBottom, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_LEFT);
    noteDataViewConsole = new wxPanel(noteDataView, wxID_ANY);
    noteRaw = new wxPanel(noteDataView, ID_NOTE_RAW);
    splitterSpectra = new wxSplitterWindow(noteDataView, ID_SPLIT_SPECTRA, wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    window_2_pane_2 = new wxPanel(splitterSpectra, wxID_ANY);
    panelTop = new BasicGLPane(splitTopBottom);


	glPanelOK = panelTop->displaySupported();

	if(!glPanelOK)
	{
		wxErrMsg(this,TRANS("OpenGL Failed"),
TRANS("Unable to initialise the openGL (3D) panel. Program cannot start. Please check your video drivers.")  );
		
		std::cerr << TRANS("Unable to initialise the openGL (3D) panel. Program cannot start. Please check your video drivers.") << std::endl;
		return;
	}
   panelTop->setScene(&visControl.scene);

    panelLeft = new wxPanel(splitLeftRight, wxID_ANY);
    notebookControl = new wxNotebook(panelLeft, ID_NOTEBOOK_CONTROL, wxDefaultPosition, wxDefaultSize, wxNB_RIGHT);
    noteTools = new wxPanel(notebookControl, ID_NOTE_PERFORMANCE, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    notePost = new wxPanel(notebookControl, wxID_ANY);
    noteEffects = new wxNotebook(notePost, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_LEFT);
    noteFxPanelStereo = new wxPanel(noteEffects, wxID_ANY);
    noteFxPanelCrop = new wxPanel(noteEffects, wxID_ANY);
    noteCamera = new wxScrolledWindow(notebookControl, ID_NOTE_CAMERA, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    noteData = new wxPanel(notebookControl, ID_NOTE_DATA, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    filterSplitter = new wxSplitterWindow(noteData,ID_SPLIT_FILTERPROP , wxDefaultPosition, wxDefaultSize, wxSP_3D|wxSP_BORDER);
    filterPropertyPane = new wxPanel(filterSplitter, wxID_ANY);
    //topPanelSizer_staticbox = new wxStaticBox(panelTop, -1, wxT(""));
    filterTreePane = new wxPanel(filterSplitter, wxID_ANY);
    MainFrame_Menu = new wxMenuBar();
   
    fileMenu = new wxMenu();
    fileMenu->Append(ID_FILE_OPEN, TRANS("&Open...\tCtrl+O"), TRANS("Open state file"), wxITEM_NORMAL);
    fileMenu->Append(ID_FILE_MERGE, TRANS("&Merge...\tCtrl+Shift+O"), TRANS("Merge other file"), wxITEM_NORMAL);
    
    recentFilesMenu=new wxMenu();
    recentHistory->UseMenu(recentFilesMenu);
    fileMenu->AppendSubMenu(recentFilesMenu,TRANS("&Recent"));
    fileSave = fileMenu->Append(ID_FILE_SAVE, TRANS("&Save\tCtrl+S"), TRANS("Save state to file"), wxITEM_NORMAL);
    fileSave->Enable(false);
    fileMenu->Append(ID_FILE_SAVEAS, TRANS("Save &As...\tCtrl+Shift+S"), TRANS("Save current state to new file"), wxITEM_NORMAL);
    fileMenu->AppendSeparator();
    fileExport = new wxMenu();
    fileExport->Append(ID_FILE_EXPORT_PLOT, TRANS("&Plot...\tCtrl+P"), TRANS("Export Current Plot"), wxITEM_NORMAL);
    fileExport->Append(ID_FILE_EXPORT_IMAGE, TRANS("&Image...\tCtrl+I"), TRANS("Export Current 3D View"), wxITEM_NORMAL);
    fileExport->Append(ID_FILE_EXPORT_IONS, TRANS("Ion&s...\tCtrl+N"), TRANS("Export Ion Data"), wxITEM_NORMAL);
    fileExport->Append(ID_FILE_EXPORT_RANGE, TRANS("Ran&ges...\tCtrl+G"), TRANS("Export Range Data"), wxITEM_NORMAL);
    fileExport->Append(ID_FILE_EXPORT_FILTER_ANIMATION, TRANS("&Animate Filters...\tCtrl+T"), TRANS("Export Animated Filter"), wxITEM_NORMAL);
    fileExport->Append(ID_FILE_EXPORT_ANIMATION, TRANS("Ani&mate Camera...\tCtrl+M"), TRANS("Export Animated Camera"), wxITEM_NORMAL);
    fileExport->Append(ID_FILE_EXPORT_PACKAGE, TRANS("Pac&kage...\tCtrl+K"), TRANS("Export analysis package"), wxITEM_NORMAL);

    fileMenu->AppendSubMenu(fileExport,TRANS("&Export"));
    fileMenu->AppendSeparator();
#ifdef __APPLE__
    fileMenu->Append(ID_FILE_EXIT, TRANS("&Quit\tCtrl+Q"), TRANS("Exit Program"), wxITEM_NORMAL);
#else
    fileMenu->Append(ID_FILE_EXIT, TRANS("E&xit"), TRANS("Exit Program"), wxITEM_NORMAL);
#endif
    MainFrame_Menu->Append(fileMenu, TRANS("&File"));

    wxMenu* wxglade_tmp_menu_1 = new wxMenu();
    wxglade_tmp_menu_1->Append(ID_VIEW_BACKGROUND, 
		    TRANS("&Background Colour...\tCtrl+B"),TRANS("Change background colour"));
    wxglade_tmp_menu_1->AppendSeparator(); //Separator
#ifndef __APPLE__
    checkMenuControlPane= wxglade_tmp_menu_1->Append(ID_VIEW_CONTROL_PANE, 
		    TRANS("&Control Pane\tF2"), TRANS("Toggle left control pane"), wxITEM_CHECK);
#else
    checkMenuControlPane= wxglade_tmp_menu_1->Append(ID_VIEW_CONTROL_PANE, 
		    TRANS("&Control Pane\tAlt+C"), TRANS("Toggle left control pane"), wxITEM_CHECK);

#endif
    checkMenuControlPane->Check();
#ifndef __APPLE__
    checkMenuRawDataPane= wxglade_tmp_menu_1->Append(ID_VIEW_RAW_DATA_PANE, 
		    TRANS("&Raw Data Pane\tF3"), TRANS("Toggle raw data  pane (bottom)"), wxITEM_CHECK);
#else
    checkMenuRawDataPane= wxglade_tmp_menu_1->Append(ID_VIEW_RAW_DATA_PANE, 
		    TRANS("&Raw Data Pane\tAlt+R"), TRANS("Toggle raw data  pane (bottom)"), wxITEM_CHECK);
#endif
    checkMenuRawDataPane->Check();
#ifndef __APPLE__
    checkMenuSpectraList=wxglade_tmp_menu_1->Append(ID_VIEW_SPECTRA, TRANS("&Plot List\tF4"),TRANS("Toggle plot list"), wxITEM_CHECK);
#else
    checkMenuSpectraList=wxglade_tmp_menu_1->Append(ID_VIEW_SPECTRA, TRANS("&Plot List\tAlt+P"),TRANS("Toggle plot list"), wxITEM_CHECK);
#endif
    checkMenuSpectraList->Check();

    wxglade_tmp_menu_1->AppendSeparator(); //Separator
    wxMenu* viewPlot= new wxMenu();
    checkViewLegend=viewPlot->Append(ID_VIEW_PLOT_LEGEND,TRANS("&Legend\tCtrl+L"),TRANS("Toggle Legend display"),wxITEM_CHECK);
    checkViewLegend->Check();
    wxglade_tmp_menu_1->AppendSubMenu(viewPlot,TRANS("P&lot..."));
    checkViewWorldAxis=wxglade_tmp_menu_1->Append(ID_VIEW_WORLDAXIS,TRANS("&Axis\tCtrl+Shift+I"),TRANS("Toggle World Axis display"),wxITEM_CHECK);
    checkViewWorldAxis->Check();
    
    wxglade_tmp_menu_1->AppendSeparator(); //Separator
#ifndef __APPLE__
    menuViewFullscreen=wxglade_tmp_menu_1->Append(ID_VIEW_FULLSCREEN, TRANS("&Fullscreen mode\tF11"),TRANS("Next fullscreen mode: with toolbars"));
#else
    menuViewFullscreen=wxglade_tmp_menu_1->Append(ID_VIEW_FULLSCREEN, TRANS("&Fullscreen mode\tCtrl+Shift+F"),TRANS("Next fullscreen mode: with toolbars"));
#endif


    wxMenu *Edit = new wxMenu();
    editUndoMenuItem = Edit->Append(ID_EDIT_UNDO,TRANS("&Undo\tCtrl+Z"));
    editUndoMenuItem->Enable(false);
    editRedoMenuItem = Edit->Append(ID_EDIT_REDO,TRANS("&Redo\tCtrl+Y"));
   editRedoMenuItem->Enable(false);
    Edit->AppendSeparator();
    editRangeMenuItem=Edit->Append(ID_EDIT_RANGE,TRANS("&Range"));
    editRangeMenuItem->Enable(false);
    Edit->AppendSeparator();
    Edit->Append(ID_EDIT_PREFERENCES,TRANS("&Preferences"));

    MainFrame_Menu->Append(Edit, TRANS("&Edit"));


    MainFrame_Menu->Append(wxglade_tmp_menu_1, TRANS("&View"));
    wxMenu* Help = new wxMenu();
    Help->Append(ID_HELP_HELP, TRANS("&Help...\tCtrl+H"), TRANS("Show help files and documentation"), wxITEM_NORMAL);
    Help->Append(ID_HELP_CONTACT, TRANS("&Contact..."), TRANS("Open contact page"), wxITEM_NORMAL);
    Help->AppendSeparator();
    Help->Append(ID_HELP_ABOUT, TRANS("&About..."), TRANS("Information about this program"), wxITEM_NORMAL);
    MainFrame_Menu->Append(Help, TRANS("&Help"));
    SetMenuBar(MainFrame_Menu);
    lblSettings = new wxStaticText(noteData, wxID_ANY, TRANS("Stashed Filters"));


    comboStash = new wxComboBox(noteData, ID_COMBO_STASH, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN|wxTE_PROCESS_ENTER|wxCB_SORT);
    btnStashManage = new wxButton(noteData, ID_BTN_STASH_MANAGE, wxT("..."),wxDefaultPosition,wxSize(28,28));
    filteringLabel = new wxStaticText(noteData, wxID_ANY, TRANS("New Filters"));


    //Workaround for wx bug http://trac.wxwidgets.org/ticket/4398
    //Combo box wont sort even when asked under wxGTK<3.0
    // use sortedArrayString, rather than normal arraystring
    wxSortedArrayString filterNames;
    for(unsigned int ui=0;ui<FILTER_DROP_COUNT; ui++)
    {
	const char * str = comboFilters_choices[ui];

	//construct translation->comboFilters_choices offset.
	filterMap[TRANS(str)] = ui;
	//Add to filter name wxArray
	wxString wxStrTrans = TRANS(str);
	filterNames.Add(wxStrTrans);
    }
    



    comboFilters = new wxComboBox(filterTreePane, ID_COMBO_FILTER, TRANS(ADD_FILTER_TEXT), 
    			wxDefaultPosition, wxDefaultSize, filterNames, wxCB_DROPDOWN|wxCB_SORT);
    comboFilters->Enable(false);

    treeFilters = new TextTreeCtrl(filterTreePane, ID_TREE_FILTERS, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS|wxTR_NO_LINES|wxTR_HIDE_ROOT|wxTR_DEFAULT_STYLE|wxSUNKEN_BORDER|wxTR_EDIT_LABELS);
    vector<string> msgs;
    msgs.push_back("No data loaded:");
    msgs.push_back("open file, then add filters");
    treeFilters->setMessages(msgs); 
    checkAutoUpdate = new wxCheckBox(filterTreePane, ID_CHECK_AUTOUPDATE, TRANS("Auto Refresh"));
    refreshButton = new wxButton(filterTreePane, wxID_REFRESH, wxEmptyString);
    btnFilterTreeExpand= new wxButton(filterTreePane, ID_BTN_EXPAND, wxT("▼"),wxDefaultPosition,wxSize(30,30));
    btnFilterTreeCollapse = new wxButton(filterTreePane, ID_BTN_COLLAPSE, wxT("▲"),wxDefaultPosition,wxSize(30,30));
    btnFilterTreeErrs = new wxBitmapButton(filterTreePane,ID_BTN_FILTERTREE_ERRS,wxArtProvider::GetBitmap(wxART_INFORMATION),wxDefaultPosition,wxSize(40,40));

    propGridLabel = new wxStaticText(filterPropertyPane, wxID_ANY, TRANS("Filter settings"));
    gridFilterPropGroup = new wxPropertyGrid(filterPropertyPane, ID_GRID_FILTER_PROPERTY,wxDefaultPosition,wxDefaultSize,PROPERTY_GRID_STYLE);
    gridFilterPropGroup->SetExtraStyle(PROPERTY_GRID_EXTRA_STYLE);
    labelCameraName = new wxStaticText(noteCamera, wxID_ANY, TRANS("Camera Name"));
    comboCamera = new wxComboBox(noteCamera, ID_COMBO_CAMERA, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN|wxTE_PROCESS_ENTER );
    buttonRemoveCam = new wxButton(noteCamera, wxID_REMOVE, wxEmptyString);
    cameraNamePropertySepStaticLine = new wxStaticLine(noteCamera, wxID_ANY);
    gridCameraProperties = new wxPropertyGrid(noteCamera,ID_GRID_CAMERA_PROPERTY,
					wxDefaultPosition,wxDefaultSize,PROPERTY_GRID_STYLE);
    buttonAlignCamXPlus = new wxButton(noteCamera, ID_BUTTON_ALIGNCAM_XPLUS, "X+");
    buttonAlignCamYPlus = new wxButton(noteCamera, ID_BUTTON_ALIGNCAM_YPLUS, "Y+");
    buttonAlignCamZPlus = new wxButton(noteCamera, ID_BUTTON_ALIGNCAM_ZPLUS, "Z+");
    buttonAlignCamXMinus = new wxButton(noteCamera, ID_BUTTON_ALIGNCAM_XMINUS, "X-");
    buttonAlignCamYMinus = new wxButton(noteCamera, ID_BUTTON_ALIGNCAM_YMINUS, "Y-");
    buttonAlignCamZMinus = new wxButton(noteCamera, ID_BUTTON_ALIGNCAM_ZMINUS, "Z-");
    checkAlignCamResize = new wxCheckBox(noteCamera, wxID_ANY, _("Resize to Fit"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
#ifndef APPLE_EFFECTS_WORKAROUND
    checkPostProcessing = new wxCheckBox(notePost, ID_EFFECT_ENABLE, TRANS("3D Post-processing"));
#endif
    checkFxCrop = new wxCheckBox(noteFxPanelCrop, ID_EFFECT_CROP_ENABLE, TRANS("Enable Cropping"));
    const wxString comboFxCropAxisOne_choices[] = {
        TRANS("x-y"),
        TRANS("x-z"),
        TRANS("y-x"),
        TRANS("y-z"),
        TRANS("z-x"),
        TRANS("z-y")
    };
    comboFxCropAxisOne = new wxComboBox(noteFxPanelCrop, ID_EFFECT_CROP_AXISONE_COMBO, wxT(""), wxDefaultPosition, wxDefaultSize, 6, comboFxCropAxisOne_choices, wxCB_SIMPLE|wxCB_DROPDOWN|wxCB_READONLY);
    panelFxCropOne = new CropPanel(noteFxPanelCrop, ID_EFFECT_CROP_AXISONE_COMBO,
		   		 wxDefaultPosition,wxDefaultSize,wxEXPAND);
    const wxString comboFxCropAxisTwo_choices[] = {
        TRANS("x-y"),
        TRANS("x-z"),
        TRANS("y-x"),
        TRANS("y-z"),
        TRANS("z-x"),
        TRANS("z-y")
    };
    comboFxCropAxisTwo = new wxComboBox(noteFxPanelCrop, ID_EFFECT_CROP_AXISTWO_COMBO, wxT(""), wxDefaultPosition, wxDefaultSize, 6, comboFxCropAxisTwo_choices, wxCB_SIMPLE|wxCB_DROPDOWN|wxCB_READONLY);
    panelFxCropTwo = new CropPanel(noteFxPanelCrop, ID_EFFECT_CROP_AXISTWO_COMBO,wxDefaultPosition,wxDefaultSize,wxEXPAND);
    checkFxCropCameraFrame = new wxCheckBox(noteFxPanelCrop,ID_EFFECT_CROP_CHECK_COORDS,TRANS("Use camera coordinates"));
    labelFxCropDx = new wxStaticText(noteFxPanelCrop, wxID_ANY, TRANS("dX"));
    textFxCropDx = new wxTextCtrl(noteFxPanelCrop, ID_EFFECT_CROP_TEXT_DX, wxEmptyString);
    labelFxCropDy = new wxStaticText(noteFxPanelCrop, wxID_ANY, TRANS("dY"));
    textFxCropDy = new wxTextCtrl(noteFxPanelCrop, ID_EFFECT_CROP_TEXT_DY, wxEmptyString);
    labelFxCropDz = new wxStaticText(noteFxPanelCrop, wxID_ANY, TRANS("dZ"));
    textFxCropDz = new wxTextCtrl(noteFxPanelCrop, ID_EFFECT_CROP_TEXT_DZ, wxEmptyString);
    checkFxEnableStereo = new wxCheckBox(noteFxPanelStereo, ID_EFFECT_STEREO_ENABLE, TRANS("Enable Anaglyphic Stereo"));
    checkFxStereoLensFlip= new wxCheckBox(noteFxPanelStereo, ID_EFFECT_STEREO_LENSFLIP, TRANS("Flip Channels"));
    lblFxStereoMode = new wxStaticText(noteFxPanelStereo, wxID_ANY, TRANS("Anaglyph Mode"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    const wxString comboFxStereoMode_choices[] = {
        TRANS("Red-Blue"),
        TRANS("Red-Green"),
        TRANS("Red-Cyan"),
        TRANS("Green-Magenta"),
    };
    comboFxStereoMode = new wxComboBox(noteFxPanelStereo, ID_EFFECT_STEREO_COMBO, wxT(""), wxDefaultPosition, wxDefaultSize, 4, comboFxStereoMode_choices, wxCB_DROPDOWN|wxCB_SIMPLE|wxCB_READONLY);
    bitmapFxStereoGlasses = new wxStaticBitmap(noteFxPanelStereo, wxID_ANY, wxNullBitmap);
    labelFxStereoBaseline = new wxStaticText(noteFxPanelStereo, wxID_ANY, TRANS("Baseline Separation"));
    sliderFxStereoBaseline = new wxSlider(noteFxPanelStereo,ID_EFFECT_STEREO_BASELINE_SLIDER, 20, 0, 100);
    labelAppearance = new wxStaticText(noteTools, wxID_ANY, TRANS("Appearance"));
    checkAlphaBlend = new wxCheckBox(noteTools,ID_CHECK_ALPHA , TRANS("Smooth && translucent objects"));
    checkAlphaBlend->SetValue(true);
    checkLighting = new wxCheckBox(noteTools, ID_CHECK_LIGHTING, TRANS("3D lighting"));
    checkLighting->SetValue(true);
    static_line_1 = new wxStaticLine(noteTools, wxID_ANY);
    labelPerformance = new wxStaticText(noteTools, wxID_ANY, TRANS("Performance"));
    checkWeakRandom = new wxCheckBox(noteTools, ID_CHECK_WEAKRANDOM, TRANS("Fast and weak randomisation."));
    checkWeakRandom->SetValue(true);
    checkLimitOutput = new wxCheckBox(noteTools, ID_CHECK_LIMIT_POINT_OUT, TRANS("Limit Output Pts"));
    std::string tmpStr;
//    stream_cast(tmpStr,visControl.getIonDisplayLimit());
    textLimitOutput = new wxTextCtrl(noteTools, ID_TEXT_LIMIT_POINT_OUT, (tmpStr),
		    	wxDefaultPosition,wxDefaultSize,wxTE_PROCESS_ENTER );
    checkCaching = new wxCheckBox(noteTools, ID_CHECK_CACHING, TRANS("Filter caching"));
    checkCaching->SetValue(true);
    labelMaxRamUsage = new wxStaticText(noteTools, wxID_ANY, TRANS("Max. Ram usage (%)"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    spinCachePercent = new wxSpinCtrl(noteTools, ID_SPIN_CACHEPERCENT, wxT("50"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 100);
    panelView = new wxPanel(panelTop, ID_PANEL_VIEW);
    panelSpectra = new MathGLPane(splitterSpectra, wxID_ANY);
    plotListLabel = new wxStaticText(window_2_pane_2, wxID_ANY, TRANS("Plot List"));
    plotList = new wxListBox(window_2_pane_2, ID_LIST_PLOTS, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_MULTIPLE|wxLB_NEEDED_SB);
    gridRawData = new CopyGrid(noteRaw, ID_GRID_RAW_DATA);
    btnRawDataSave = new wxButton(noteRaw, wxID_SAVE, wxEmptyString);
    btnRawDataClip = new wxButton(noteRaw, wxID_COPY, wxEmptyString);
    textConsoleOut = new wxTextCtrl(noteDataViewConsole, 
		    wxID_ANY, wxEmptyString,wxDefaultPosition,
		    wxDefaultSize,wxTE_MULTILINE|wxTE_READONLY);
    MainFrame_statusbar = CreateStatusBar(3, 0);

    set_properties();
    do_layout();
#ifdef FIX_WXPROPGRID_16222
    backCameraPropGrid=0;
    backFilterPropGrid=0;
#endif
    //Disable post-processing
#ifndef APPLE_EFFECTS_WORKAROUND
    checkPostProcessing->SetValue(false); 
    noteFxPanelCrop->Enable(false);
    noteFxPanelStereo->Enable(false);
#else
    //Disable effects panel stereo controls explicitly
    comboFxStereoMode->Enable(false);
    sliderFxStereoBaseline->Enable(false);
    checkFxStereoLensFlip->Enable(false);

    //Disable Crop controls explicitly
    checkFxCropCameraFrame->Enable(false);
    comboFxCropAxisOne->Enable(false);
    panelFxCropOne->Enable(false);
    comboFxCropAxisTwo->Enable(false);
    panelFxCropTwo->Enable(false);
    textFxCropDx->Enable(false);
    textFxCropDy->Enable(false);
    textFxCropDz->Enable(false);
    labelFxCropDx->Enable(false);
    labelFxCropDy->Enable(false);
    labelFxCropDz->Enable(false);

#endif

    //Link the crop panels in the post section appropriately
    panelFxCropOne->link(panelFxCropTwo,CROP_LINK_BOTH); 
    panelFxCropTwo->link(panelFxCropOne,CROP_LINK_BOTH); 



    //Manually tuned splitter parameters.
    filterSplitter->SetMinimumPaneSize(180);
    filterSplitter->SetSashGravity(0.8);
    splitLeftRight->SetSashGravity(0.15);
    splitTopBottom->SetSashGravity(0.85);
    splitterSpectra->SetSashGravity(0.82);

    //Set callback for mathgl plot
    panelSpectra->registerUpdateHandler(this,
		    (UpdateHandler)&MainWindowFrame::onPanelSpectraUpdate);

    //Inform top panel about timer and timeouts
    panelTop->setParentStatus(MainFrame_statusbar,statusTimer,STATUS_TIMER_DELAY);

    panelTop->clearCameraUpdates();

    // end wxGlade
   
    if(configFile.read() == CONFIG_ERR_BADFILE)
    {
	textConsoleOut->AppendText(TRANS("Warning: Your configuration file appears to be invalid:\n"));
	wxString wxS = TRANS("\tConfig Load: ");
	wxS+= ( configFile.getErrMessage());
	textConsoleOut->AppendText(wxS);
    }
    else
	    restoreConfigDefaults();
	
	
	//Try to set the window size to a nice size
	SetSize(getNiceWindowSize());
	initedOK=true;   


	// Set the limit value checkbox and text field with the
	// value obtained from the configuration file
	unsigned int ionLimit=visControl.getIonDisplayLimit(); 
	checkLimitOutput->SetValue((ionLimit!=0));
	if(ionLimit)
	{
		std::string sValue;
		stream_cast(sValue,visControl.getIonDisplayLimit());
		textLimitOutput->SetValue(sValue);
	}	



#ifndef DISABLE_ONLINE_UPDATE
        wxDateTime datetime = wxDateTime::Today();	
	
	//Annoy the user, on average, every (% blah) days. 
	const int CHECK_FREQUENCY=7;

	//Generate a pseudorandom number of fixed sequence	
	LinearFeedbackShiftReg lfsr;
	//Set the period to 2^9 (power of 2 > weeksinyear*daysinweek)
	lfsr.setMaskPeriod(9);
	lfsr.setState(109); //Use a fixed random seed, to ensure that users will be in-sync for checking

 	unsigned int offset = datetime.GetWeekOfYear()*7 + datetime.GetWeekDay();
	while(offset--)
		lfsr.clock(); //Discard a whole bunch of entires

	//Everyone will get the same pseudorandom number on the same day.
	size_t pseudorandomVal=lfsr.clock();
	ASSERT(pseudorandomVal); //shouldn't be zero, or LFSR is in bad state

	if( configFile.getAllowOnlineVersionCheck() &&  
		!(pseudorandomVal %CHECK_FREQUENCY))
	{
		
		verCheckThread=new VersionCheckThread(this);
		verCheckThread->Create();
		verCheckThread->Run();
	}
#endif
}

MainWindowFrame::~MainWindowFrame()
{

	//Delete and stop all the timers.
	delete statusTimer;
	delete updateTimer;
	delete autoSaveTimer;
	delete progressTimer;

	//delete the file history  pointer
	delete recentHistory;
   
   	//Bindings did not get initialised, if glpane is not OK,
	// so abort, rather than disconnecting
   	if(!glPanelOK)
		return; 

	//wxwidgets can crash if objects are ->Connect-ed  in 
	// wxWindowBase::DestroyChildren(), so Disconnect before destructing
	comboCamera->Unbind(wxEVT_SET_FOCUS, &MainWindowFrame::OnComboCameraSetFocus, this);
	comboStash->Unbind(wxEVT_SET_FOCUS, &MainWindowFrame::OnComboStashSetFocus, this);
	noteDataView->Unbind(wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, &MainWindowFrame::OnNoteDataView, this);
	treeFilters->Unbind(wxEVT_KEY_DOWN,&MainWindowFrame::OnTreeKeyDown,this);
	
}


void MainWindowFrame::finaliseStartup()
{
	updateTimer->Start(UPDATE_TIMER_DELAY,wxTIMER_CONTINUOUS);
	autoSaveTimer->Start(AUTOSAVE_DELAY*1000,wxTIMER_CONTINUOUS);
}

BEGIN_EVENT_TABLE(MainWindowFrame, wxFrame)
    EVT_TIMER(ID_STATUS_TIMER,MainWindowFrame::OnStatusBarTimer)
    EVT_TIMER(ID_PROGRESS_TIMER,MainWindowFrame::OnProgressTimer)
    EVT_TIMER(ID_UPDATE_TIMER,MainWindowFrame::OnUpdateTimer)
    EVT_TIMER(ID_AUTOSAVE_TIMER,MainWindowFrame::OnAutosaveTimer)
#ifdef FIX_WXPROPGRID_16222
    EVT_IDLE(MainWindowFrame::OnIdle)
#endif   
    EVT_SPLITTER_UNSPLIT(ID_SPLIT_TOP_BOTTOM, MainWindowFrame::OnRawDataUnsplit) 
    EVT_SPLITTER_UNSPLIT(ID_SPLIT_LEFTRIGHT, MainWindowFrame::OnControlUnsplit) 
    EVT_SPLITTER_UNSPLIT(ID_SPLIT_SPECTRA, MainWindowFrame::OnSpectraUnsplit) 
    EVT_SPLITTER_DCLICK(ID_SPLIT_FILTERPROP, MainWindowFrame::OnFilterPropDoubleClick) 
    EVT_SPLITTER_DCLICK(ID_SPLIT_LEFTRIGHT, MainWindowFrame::OnControlUnsplit) 
    EVT_SPLITTER_SASH_POS_CHANGED(ID_SPLIT_LEFTRIGHT, MainWindowFrame::OnControlSplitMove) 
    EVT_SPLITTER_SASH_POS_CHANGED(ID_SPLIT_TOP_BOTTOM, MainWindowFrame::OnTopBottomSplitMove) 
    EVT_SPLITTER_SASH_POS_CHANGED(ID_SPLIT_FILTERPROP, MainWindowFrame::OnFilterSplitMove) 
    // begin wxGlade: MainWindowFrame::event_table
    EVT_MENU(ID_FILE_OPEN, MainWindowFrame::OnFileOpen)
    EVT_MENU(ID_FILE_MERGE, MainWindowFrame::OnFileMerge)
    EVT_MENU(ID_FILE_SAVE, MainWindowFrame::OnFileSave)
    EVT_MENU(ID_FILE_SAVEAS, MainWindowFrame::OnFileSaveAs)
    EVT_MENU(ID_FILE_EXPORT_PLOT, MainWindowFrame::OnFileExportPlot)
    EVT_MENU(ID_FILE_EXPORT_IMAGE, MainWindowFrame::OnFileExportImage)
    EVT_MENU(ID_FILE_EXPORT_IONS, MainWindowFrame::OnFileExportIons)
    EVT_MENU(ID_FILE_EXPORT_RANGE, MainWindowFrame::OnFileExportRange)
    EVT_MENU(ID_FILE_EXPORT_ANIMATION, MainWindowFrame::OnFileExportVideo)
    EVT_MENU(ID_FILE_EXPORT_FILTER_ANIMATION, MainWindowFrame::OnFileExportFilterVideo)
    EVT_MENU(ID_FILE_EXPORT_PACKAGE, MainWindowFrame::OnFileExportPackage)
    EVT_MENU(ID_FILE_EXIT, MainWindowFrame::OnFileExit)

    EVT_MENU(ID_EDIT_UNDO, MainWindowFrame::OnEditUndo)
    EVT_MENU(ID_EDIT_REDO, MainWindowFrame::OnEditRedo)
    EVT_MENU(ID_EDIT_RANGE, MainWindowFrame::OnEditRange)
    EVT_MENU(ID_EDIT_PREFERENCES, MainWindowFrame::OnEditPreferences)
    
    EVT_MENU(ID_VIEW_BACKGROUND, MainWindowFrame::OnViewBackground)
    EVT_MENU(ID_VIEW_CONTROL_PANE, MainWindowFrame::OnViewControlPane)
    EVT_MENU(ID_VIEW_RAW_DATA_PANE, MainWindowFrame::OnViewRawDataPane)
    EVT_MENU(ID_VIEW_SPECTRA, MainWindowFrame::OnViewSpectraList)
    EVT_MENU(ID_VIEW_PLOT_LEGEND, MainWindowFrame::OnViewPlotLegend)
    EVT_MENU(ID_VIEW_WORLDAXIS, MainWindowFrame::OnViewWorldAxis)
    EVT_MENU(ID_VIEW_FULLSCREEN, MainWindowFrame::OnViewFullscreen)

    EVT_MENU(ID_HELP_HELP, MainWindowFrame::OnHelpHelp)
    EVT_MENU(ID_HELP_CONTACT, MainWindowFrame::OnHelpContact)
    EVT_MENU(ID_HELP_ABOUT, MainWindowFrame::OnHelpAbout)
    EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, MainWindowFrame::OnRecentFile)
    
    EVT_BUTTON(wxID_REFRESH,MainWindowFrame::OnButtonRefresh)
    EVT_BUTTON(wxID_COPY,MainWindowFrame::OnButtonGridCopy)
    EVT_BUTTON(wxID_SAVE,MainWindowFrame::OnButtonGridSave)
    EVT_TEXT(ID_COMBO_STASH, MainWindowFrame::OnComboStashText)
    EVT_TEXT_ENTER(ID_COMBO_STASH, MainWindowFrame::OnComboStashEnter)
    EVT_COMBOBOX(ID_COMBO_STASH, MainWindowFrame::OnComboStash)
    EVT_TREE_END_DRAG(ID_TREE_FILTERS, MainWindowFrame::OnTreeEndDrag)
    EVT_TREE_SEL_CHANGING(ID_TREE_FILTERS, MainWindowFrame::OnTreeSelectionPreChange)
    EVT_TREE_SEL_CHANGED(ID_TREE_FILTERS, MainWindowFrame::OnTreeSelectionChange)
    EVT_TREE_DELETE_ITEM(ID_TREE_FILTERS, MainWindowFrame::OnTreeDeleteItem)
    EVT_TREE_BEGIN_DRAG(ID_TREE_FILTERS, MainWindowFrame::OnTreeBeginDrag)
    EVT_BUTTON(ID_BTN_EXPAND, MainWindowFrame::OnBtnExpandTree)
    EVT_BUTTON(ID_BTN_COLLAPSE, MainWindowFrame::OnBtnCollapseTree)
    EVT_BUTTON(ID_BTN_FILTERTREE_ERRS, MainWindowFrame::OnBtnFilterTreeErrs)
    EVT_PG_CHANGING(ID_GRID_FILTER_PROPERTY, MainWindowFrame::OnGridFilterPropertyChange)
    EVT_PG_CHANGING(ID_GRID_CAMERA_PROPERTY, MainWindowFrame::OnGridCameraPropertyChange)
    EVT_PG_DOUBLE_CLICK(ID_GRID_FILTER_PROPERTY, MainWindowFrame::OnGridFilterDClick)
    EVT_TEXT(ID_COMBO_CAMERA, MainWindowFrame::OnComboCameraText)
    EVT_TEXT_ENTER(ID_COMBO_CAMERA, MainWindowFrame::OnComboCameraEnter)
    EVT_BUTTON(wxID_REMOVE, MainWindowFrame::OnButtonRemoveCam)
    EVT_BUTTON(ID_BUTTON_ALIGNCAM_XPLUS, MainWindowFrame::OnButtonAlignCameraXPlus)
    EVT_BUTTON(ID_BUTTON_ALIGNCAM_YPLUS, MainWindowFrame::OnButtonAlignCameraYPlus)
    EVT_BUTTON(ID_BUTTON_ALIGNCAM_ZPLUS, MainWindowFrame::OnButtonAlignCameraZPlus)
    EVT_BUTTON(ID_BUTTON_ALIGNCAM_XMINUS, MainWindowFrame::OnButtonAlignCameraXMinus)
    EVT_BUTTON(ID_BUTTON_ALIGNCAM_YMINUS, MainWindowFrame::OnButtonAlignCameraYMinus)
    EVT_BUTTON(ID_BUTTON_ALIGNCAM_ZMINUS, MainWindowFrame::OnButtonAlignCameraZMinus)
    EVT_CHECKBOX(ID_CHECK_ALPHA, MainWindowFrame::OnCheckAlpha)
    EVT_CHECKBOX(ID_CHECK_LIGHTING, MainWindowFrame::OnCheckLighting)
    EVT_CHECKBOX(ID_CHECK_CACHING, MainWindowFrame::OnCheckCacheEnable)
    EVT_CHECKBOX(ID_CHECK_WEAKRANDOM, MainWindowFrame::OnCheckWeakRandom)
    EVT_SPINCTRL(ID_SPIN_CACHEPERCENT, MainWindowFrame::OnCacheRamUsageSpin)
    EVT_COMBOBOX(ID_COMBO_CAMERA, MainWindowFrame::OnComboCamera)
    EVT_COMBOBOX(ID_COMBO_FILTER, MainWindowFrame::OnComboFilter)
    EVT_TEXT(ID_COMBO_FILTER, MainWindowFrame::OnComboFilterText)
    EVT_BUTTON(ID_BTN_STASH_MANAGE, MainWindowFrame::OnButtonStashDialog)
    EVT_LISTBOX(ID_LIST_PLOTS, MainWindowFrame::OnSpectraListbox)
    EVT_CLOSE(MainWindowFrame::OnClose)
    EVT_TREE_END_LABEL_EDIT(ID_TREE_FILTERS,MainWindowFrame::OnTreeEndLabelEdit)
    EVT_TREE_BEGIN_LABEL_EDIT(ID_TREE_FILTERS,MainWindowFrame::OnTreeBeginLabelEdit)
   
    //Post-processing stuff	
    EVT_CHECKBOX(ID_EFFECT_ENABLE, MainWindowFrame::OnCheckPostProcess)
    EVT_CHECKBOX(ID_EFFECT_CROP_ENABLE, MainWindowFrame::OnFxCropCheck)
    EVT_CHECKBOX(ID_EFFECT_CROP_CHECK_COORDS, MainWindowFrame::OnFxCropCamFrameCheck)
    EVT_COMBOBOX(ID_EFFECT_CROP_AXISONE_COMBO, MainWindowFrame::OnFxCropAxisOne)
    EVT_COMBOBOX(ID_EFFECT_CROP_AXISTWO_COMBO, MainWindowFrame::OnFxCropAxisTwo)
    EVT_CHECKBOX(ID_EFFECT_STEREO_ENABLE, MainWindowFrame::OnFxStereoEnable)
    EVT_CHECKBOX(ID_EFFECT_STEREO_LENSFLIP, MainWindowFrame::OnFxStereoLensFlip)
    EVT_COMBOBOX(ID_EFFECT_STEREO_COMBO, MainWindowFrame::OnFxStereoCombo)
    EVT_COMMAND_SCROLL(ID_EFFECT_STEREO_BASELINE_SLIDER, MainWindowFrame::OnFxStereoBaseline)
    EVT_TEXT(ID_TEXT_LIMIT_POINT_OUT, MainWindowFrame::OnTextLimitOutput)
    EVT_TEXT_ENTER(ID_TEXT_LIMIT_POINT_OUT, MainWindowFrame::OnTextLimitOutputEnter)
    EVT_CHECKBOX(ID_CHECK_LIMIT_POINT_OUT, MainWindowFrame::OnCheckLimitOutput)
  

    EVT_COMMAND(wxID_ANY, RemoteUpdateAvailEvent, MainWindowFrame::OnCheckUpdatesThread)
    EVT_COMMAND(wxID_ANY, RefreshCompleteEvent, MainWindowFrame::OnFinishRefreshThread)
    // end wxGlade
END_EVENT_TABLE();



RefreshThread::RefreshThread(wxWindow *target,RefreshController *rC)
		 : wxThread(wxTHREAD_JOINABLE)
{
	ASSERT(rC);
	refreshControl=rC;
	targetWindow=target;
}

RefreshThread::~RefreshThread()
{
}

void *RefreshThread::Entry()
{
  	wxCommandEvent event( RefreshCompleteEvent);
	event.SetInt(0);

	//pack the unsigned int into the event
	unsigned int i=refreshControl->refresh();
	event.SetInt((int)i);
	wxPostEvent(targetWindow,event);

	return 0;
}

#ifdef FIX_WXPROPGRID_16222
void MainWindowFrame::OnIdle(wxIdleEvent &evt)
{
	if(backFilterPropGrid)
	{
		delete backFilterPropGrid;
		backFilterPropGrid=0;
	}

	if(backCameraPropGrid)
	{
		delete backCameraPropGrid;
		backCameraPropGrid=0;
	}
}
#endif

unsigned int MainWindowFrame::guessFileType(const std::string &dataFile)
{
	
	//Split the filename into chunks: path, volume, name and extension
	//the format of this is OS dependant, but wxWidgets can deal with this.
	wxFileName fname;
	wxString volume,path,name,ext;
	bool hasExt;
	fname.SplitPath((dataFile),&volume,
			&path,&name,&ext, &hasExt);

	//Test the extension to determine what we will do
	//TODO: This is really lazy, and I should use something like libmagic.
	std::string extStr;
	extStr=lowercase(stlStr(ext));

	if( extStr == std::string("xml"))
		return FILE_OPEN_TYPE_XML;
	
	if( extStr == std::string("txt"))
		return FILE_OPEN_TYPE_TEXT;

	if( extStr == std::string("pos"))
		return FILE_OPEN_TYPE_POS;
	
	if( extStr == std::string("ato"))
		return FILE_OPEN_TYPE_LAWATAP_ATO;

	return FILE_OPEN_TYPE_UNKNOWN;
}


bool MainWindowFrame::getTreeFilterId(const wxTreeItemId &tId, size_t &filterId) const
{
	if(!tId.IsOk())
		return false;

	//Disallow obtaining the filterID for the root item
	if(tId == treeFilters->GetRootItem())
		return false;

	wxTreeItemData *tData=treeFilters->GetItemData(tId);
	filterId = ((wxTreeUint *)tData)->value;

	return true;
}

void MainWindowFrame::checkAskSaveState()
{
	if(visControl.stateIsModified())
	{
		wxMessageDialog wxD (this,
			TRANS("Current state has not been saved, would you like to save it now?")
			,TRANS("State changed"),wxYES_NO|wxICON_QUESTION|wxYES_DEFAULT );
		wxD.SetAffirmativeId(wxID_YES);
		wxD.SetEscapeId(wxID_NO);

		if ( wxD.ShowModal()== wxID_YES) 
		{
			wxCommandEvent event;
			OnFileSave(event);
		}
	}
}

void MainWindowFrame::OnFileOpen(wxCommandEvent &event)
{
	//Do not allow any action if a scene update is in progress
	ASSERT(!refreshThreadActive());

	vector<pair<std::string,std::string> > validTypes;
	validTypes.push_back(	make_pair(TRANS("Readable files (*.xml, *.pos, *.txt,*.csv, *.ato)"),
					"*.xml;*XML;*.pos;*,POS;*.txt;*.TXT;*.csv;*.CSV;*.ato;*.ATO") );
	validTypes.push_back( make_pair(TRANS("XML State File (*.xml)"),"*.xml;*.XML"));
	validTypes.push_back( make_pair(TRANS("POS File (*.pos)"),"*.pos;*.POS"));
	validTypes.push_back( make_pair(TRANS("LAWATAP ATO File (*.ato)"),"*.ato;*.ATO"));
	validTypes.push_back( make_pair(TRANS("Text File (*.txt, *.csv)"),"*.csv;*.txt;*.CSV;*.TXT"));
	validTypes.push_back( make_pair(TRANS("All Files (*)"),"*"));

	std::string totalStr;
	totalStr=validTypes[0].first + std::string("|") + validTypes[0].second;
	for(unsigned int ui=1;ui<validTypes.size();ui++)
	{
		totalStr+="|",
		totalStr+=validTypes[ui].first ;
		totalStr+="|",
		totalStr+=validTypes[ui].second;
	}

	//Load a file, either a state file, or a new pos file
	wxFileDialog wxF(this,TRANS("Select Data or State File..."), wxT(""),
		wxT(""),(totalStr),wxFD_OPEN|wxFD_FILE_MUST_EXIST);

	//Show the file dialog	
	if( (wxF.ShowModal() == wxID_CANCEL))
		return;

	//See if the user would like to save state, if we are opening a state file
	// which will overwrite our current state
	std::string filePath = stlStr(wxF.GetPath());
	if(guessFileType(filePath) == FILE_OPEN_TYPE_XML)
		checkAskSaveState();
	
	//Force an update to viscontrol
	visControl.clearScene();
	visControl.scene.draw();

	textConsoleOut->Clear();
	//Get vis controller to update tree control to match internal
	// structure. Retain tree selection & visibility if we currently
	// have a valid selection
	size_t filterId;
	if(getTreeFilterId(treeFilters->GetSelection(),filterId))
	{
		visControl.setWxTreeFilterViewPersistence(filterId);
	}
	
	//Load the file
	if(!loadFile(wxF.GetPath()))
	{
		//If the load failed, do not try to set the 
		// selection & visibility
		visControl.clearTreeFilterViewPersistence();
		return;
	}


	std::string tmp;
	tmp = stlStr(wxF.GetPath());
	configFile.addRecentFile(tmp);
	//Update the "recent files" menu
	recentHistory->AddFileToHistory(wxF.GetPath());
}

void MainWindowFrame::OnFileMerge(wxCommandEvent &event)
{
	ASSERT(!refreshThreadActive());

	//Load a file, either a state file, or a new pos file, or text file
	wxFileDialog wxF(this,TRANS("Select Data or State File..."), wxT(""),
		wxT(""),TRANS("3Depict file (*.xml, *.pos,*.txt)|*.xml;*.pos;*.txt|POS File (*.pos)|*.pos|XML State File (*.xml)|*.xml|All Files (*)|*"),wxFD_OPEN|wxFD_FILE_MUST_EXIST);

	//Show the file dialog	
	if( (wxF.ShowModal() == wxID_CANCEL))
		return;

	textConsoleOut->Clear();
	//Load the file
	if(!loadFile(wxF.GetPath(),true))
		return;

	statusMessage(TRANS("Merged file."),MESSAGE_INFO);


	setSaveStatus();
}

void MainWindowFrame::OnDropFiles(const wxArrayString &files, int x, int y)
{
	//We can't alter the filter state if we are refreshing
	if(refreshThreadActive())
		return;

	textConsoleOut->Clear();
	wxMouseState wxm = wxGetMouseState();

	//Try opening the files as range (if ext. agrees)
	// or as 
	bool loaded =false;
	bool rangeLoaded=false;
	for(unsigned int ui=0;ui<files.Count();ui++)
	{
		string ext;

		//Check to see if can be loaded as a range file,
		//but only if there is a node to hang it off in the tree
		//----
		
		bool rangeOK;
		rangeOK=false;

		if(treeFilters->GetCount())
		{
			//Check the extension to see if it should be a range file
			wxFileName fileName;
			fileName.Assign(files[ui]);
			ext=stlStr(fileName.GetExt());

			for(size_t uj=0;uj<ext.size();uj++)
				ext[uj]=tolower(ext[uj]); 
			
			if(RangeFile::extensionIsRange(ext.c_str()))
			{
				//Now we have opened the range file,
				//we are going to have to splice it into the tree
				//TODO:Better to use the XY coordinates,
				// rather than just dropping it on the selection
				// or the first item
				size_t filterId;
				if(!getTreeFilterId(treeFilters->GetSelection(),filterId))
					return;
				
				RangeFile rng;
				string s;
				s=stlStr(files[ui]);
				if(rng.openGuessFormat(s.c_str()))
				{
					rangeOK=true;
					rangeLoaded=true;
						
					//Load rangefile &  construct filter
					RangeFileFilter *f;
					f=(RangeFileFilter *)configFile.getDefaultFilter(FILTER_TYPE_RANGEFILE);
					//Copy across the range data
					f->setRangeData(rng);
					f->setRangeFilename(s.c_str());

					//Add the filter, using the seelcted
					// item as the parent
					visControl.state.treeState.addFilter(f,false,filterId);

					//update the tree control
					updateWxTreeCtrl(treeFilters);
				}
				else
				{
					//OK, we need to let the user know something went wrong.
					//Less annoying than a dialog, but the statusbar is going
					//to be useless, as it will be overwritten during the subsequent
					//refresh (when we treat this as a pos file).
					//FIXME: Something needs to go here... A queue for messages?
				}
			
			}
		}
		//---
	
		//If it is a pos file, just handle it by trying to load
		if(!rangeOK)
		{

			//If command down, load first file using this,
			//then merge the rest
			if(!loaded)
				loaded=loadFile(files[ui],!wxm.CmdDown());
			else
				loaded=loadFile(files[ui],true);
		}
	}


	if(!wxm.CmdDown() && files.Count())
	{
#ifdef __APPLE__    
		statusMessage(TRANS("Tip: You can use ⌘ (command) to merge"),MESSAGE_HINT);
#else
		statusMessage(TRANS("Tip: You can use ctrl to merge"),MESSAGE_HINT);
#endif
	}

	if(loaded || rangeLoaded)
		doSceneUpdate();

}

bool MainWindowFrame::loadFile(const wxString &fileStr, bool merge,bool noUpdate)
{
	ASSERT(!refreshThreadActive());

	//Don't try to alter viscontrol if we are refreshing. That would be bad.
	
	std::string dataFile = stlStr(fileStr);
	unsigned int fileType=guessFileType(dataFile);
	
	if(fileType == FILE_OPEN_TYPE_XML)
	{
		std::stringstream ss;
		
		//Load the file as if it were an XML file
		if(!visControl.state.load(dataFile.c_str(),merge,ss))
		{
			std::string str;
			str=ss.str();
			textConsoleOut->AppendText((str));
			//Note that the parent window must be NULL 
			// if the parent window is not visible (eg autosave startup)
			wxWindow *parentWin=NULL;
			if(this->IsShown())
				parentWin=this;

			wxErrMsg(parentWin,TRANS("Load error"),
				TRANS("Error loading state file.\nSee console for more info."));
			return false;
		}


		if(visControl.state.treeState.getTreeRef().hasHazardousContents())
		{
			wxMessageDialog wxD(this,
						TRANS("This state file contains filters that can be unsafe to run\nDo you wish to remove these before continuing?.") 
						,TRANS("Security warning"),wxYES_NO|wxICON_WARNING|wxYES_DEFAULT );

			wxD.SetAffirmativeId(wxID_YES);
			wxD.SetEscapeId(wxID_NO);

			if(wxD.ShowModal()!= wxID_NO)
				visControl.state.treeState.stripHazardousContents();

		}

		//Update the background colour
		if(panelTop->isInited())
			panelTop->updateClearColour();

		checkViewWorldAxis->Check(visControl.state.getWorldAxisMode());
		visControl.scene.setWorldAxisVisible(visControl.state.getWorldAxisMode());

		visControl.updateCameraComboBox(comboCamera);
		//Only update the camera grid if we have a valid uniqueID
		if(visControl.state.getNumCams() > 1)
		{
			//set the active camera
			visControl.setActiveCam(visControl.state.getActiveCam());	
			//Use the active cam to update the grid.
			visControl.updateCameraPropGrid(gridCameraProperties,
						visControl.state.getActiveCam());
		}
		else
		{
			//Reset the camera property fields & combo box
			gridCameraProperties->Clear();
			comboCamera->SetValue((TRANS(cameraIntroString)));
		}

		//reset the stash combo box
		comboStash->SetValue((TRANS(stashIntroString)));


		//Check to see if we have any effects that we need to enable
		vector<const Effect*> effs;
		visControl.scene.getEffects(effs);
		if(!effs.empty())
		{
			//OK, we have some effects; we will need to update the UI
			updateFxUI(effs);
		}

		fileSave->Enable(true);
		
		//Update the stash combo box
		visControl.updateStashComboBox(comboStash);

		gridFilterPropGroup->Clear();
	}
	else 
	{

		FilterTree fTree;

		Filter *posFilter;
		posFilter= configFile.getDefaultFilter(FILTER_TYPE_DATALOAD);

		//Bastardise the default settings such that it knows to use the correct
		// file type, based upon file extension
		unsigned int fileMode;
		if(fileType == FILE_OPEN_TYPE_TEXT)
			fileMode=DATALOAD_TEXT_FILE;
		else if(fileType == FILE_OPEN_TYPE_LAWATAP_ATO)
			fileMode=DATALOAD_LAWATAP_ATO_FILE;
		else
			fileMode=DATALOAD_FLOAT_FILE;
		
		((DataLoadFilter*)posFilter)->setFileMode(fileMode);
		((DataLoadFilter*)posFilter)->setFilename(dataFile);

		//Remember the filter that we wish to keep visible after altering 
		// tree control.
		// adding filters will invalidate IDs, so this needs to be set now
		size_t filterId;
		
		if(getTreeFilterId(treeFilters->GetSelection(),filterId))
			visControl.setWxTreeFilterViewPersistence(filterId);

		//Append a new filter to the filter tree
		fTree.addFilter(posFilter,0);
		visControl.state.treeState.addFilterTree(fTree,true,0);

	}	

	updateWxTreeCtrl(treeFilters);

	if(!noUpdate)
		return doSceneUpdate(true);

	return true;
}	

void MainWindowFrame::OnRecentFile(wxCommandEvent &event)
{

	if(refreshThreadActive())
		return;

	wxString f(recentHistory->GetHistoryFile(event.GetId() - wxID_FILE1));

	if (!f.empty())
	{
		textConsoleOut->Clear();
		
		//Remember the filter that we wish to keep visible after altering 
		// tree control.
		// adding filters will invalidate IDs, so this needs to be set now
		size_t filterId;

		if(getTreeFilterId(treeFilters->GetSelection(),filterId))
			visControl.setWxTreeFilterViewPersistence(filterId);

		bool loadOK=false;
		if(!wxFileExists(f))
			statusMessage("File does not exist",MESSAGE_ERROR);
		else 
		{
			//See if the user wants to save the current state
			if(guessFileType(stlStr(f)) == FILE_OPEN_TYPE_XML)
				checkAskSaveState();

			//start the loading sequence. Note that this is done
			// in a rear thread, so we cannot be totally sure it worked yet	
			loadOK=loadFile(f);	
		}
		
		if(!loadOK)
		{
			//Didn't load?  We don't want it.
			visControl.clearTreeFilterViewPersistence();
			recentHistory->RemoveFileFromHistory(event.GetId()-wxID_FILE1);
			configFile.removeRecentFile(stlStr(f));
		}
		
		setSaveStatus();
	}

}

void MainWindowFrame::OnFileSave(wxCommandEvent &event)
{
	std::string saveFilename=visControl.state.getFilename();

	//Save menu should not be selectable if there is no file to save to.
	ASSERT(!saveFilename.empty());
	//If the file does not exist, use saveas instead
	if( saveFilename.empty()  || !wxFileExists((saveFilename)))
	{
		OnFileSaveAs(event);
		return;
	}
	
	std::map<string,string> dummyMap;
	//Try to save the viscontrol state
	if(!visControl.state.save(saveFilename.c_str(),dummyMap,false))
	{
		wxErrMsg(this,TRANS("Save error"),TRANS("Unable to save. Check output destination can be written to."));
	}
	else
	{
		//Update the recent files, and the menu.
		configFile.addRecentFile(saveFilename);
		recentHistory->AddFileToHistory((saveFilename));
	
		std::string tmpStr;
		tmpStr=	std::string("Saved state: ") + saveFilename;
		statusMessage(tmpStr.c_str(),MESSAGE_INFO);

	}

	setSaveStatus();
}

void MainWindowFrame::OnFileExportPlot(wxCommandEvent &event)
{
	if(!panelSpectra->getNumVisible())
	{
		wxErrMsg(this,TRANS("Unable to save"),
			TRANS("No plot available. Please create a plot before exporting."));
		return;
	}

	wxFileDialog wxF(this,TRANS("Save plot..."), wxT(""),
		wxT(""),TRANS("By Extension (svg,png)|*.svg;*.png|Scalable Vector Graphics File (*.svg)|*.svg|PNG File (*.png)|*.png|All Files (*)|*"),wxFD_SAVE);

	if( wxF.ShowModal() == wxID_CANCEL)
		return;

	std::string dataFile = stlStr(wxF.GetPath());
	
	//Split the filename into chunks: path, volume, name and extension
	//the format of this is OS dependant, but wxWidgets can deal with this.
	std::string strExt;
	{
	wxFileName fname;
	wxString volume,path,name,ext;
	bool hasExt;
	fname.SplitPath(wxF.GetPath(),&volume,
			&path,&name,&ext, &hasExt);
	

	strExt=stlStr(ext);
	strExt = lowercase(strExt);
	
	}

	unsigned int errCode;

	enum
	{
		EXT_SVG,
		EXT_PNG,
		EXT_NONE
	};
	const char *extensions[] = {"png","svg",""};

	size_t extId = EXT_NONE;
	for(size_t ui=0;ui<EXT_NONE;ui++)
	{
		if(strExt == extensions[ui])
		{
			extId=ui;
			break;
		}
	}


	//If the user did not specify a known extension, 
	// give them a multi-choice dialog they can pick from
	if(extId == EXT_NONE)
	{
		const char *descriptions[] = {"PNG File", "Scalable Vector Graphic",""};
		wxArrayString wxStrs;
		for(size_t ui=0;ui<EXT_NONE;ui++)
			wxStrs.Add((descriptions[ui]));


		wxSingleChoiceDialog  wxD(this,TRANS("Select type for save"),
						TRANS("Choose file type"),wxStrs);

		if(wxD.ShowModal() == wxID_CANCEL)
			return;

		strExt=extensions[wxD.GetSelection()];

		//Update the filename extension to use
		dataFile+=".";
		dataFile+=strExt;

	}
	
	
	//Try to save the file (if we recognise the extension)
	if(strExt == "svg")
		errCode=panelSpectra->saveSVG(dataFile);
	else if (strExt == "png")
	{
		//Show a resolution chooser dialog
		ResolutionDialog d(this,wxID_ANY,TRANS("Choose resolution"));

		int plotW,plotH;
		panelSpectra->GetClientSize(&plotW,&plotH);
		d.setRes(plotW,plotH,true);
		if(d.ShowModal() == wxID_CANCEL)
			return;

		
		errCode=panelSpectra->savePNG(dataFile,d.getWidth(),d.getHeight());
	
	}	
	else
	{
		ASSERT(false);
		wxErrMsg(this,TRANS("Unable to save"),
			TRANS("Unknown file extension. Please use \"svg\" or \"png\""));
		return;
	}

	//Did we save OK? If not, let the user know
	if(errCode)
	{
		wxErrMsg(this,TRANS("Save error"),panelSpectra->getErrString(errCode));
	}
	else
	{
		dataFile=std::string(TRANS("Saved plot: ")) + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}
}

void MainWindowFrame::OnFileExportImage(wxCommandEvent &event)
{
	wxFileDialog wxF(this,TRANS("Save Image..."), wxT(""),
		wxT(""),TRANS("PNG File (*.png)|*.png|All Files (*)|*"),wxFD_SAVE);
	std::string dataFile; 
	do
	{

		if( (wxF.ShowModal() == wxID_CANCEL))
			return;

		dataFile=stlStr(wxF.GetPath());

		//ask user for confirm if file exists
		if(!wxFileExists(wxF.GetPath()))
			break;

		wxMessageDialog wxMd(this,TRANS("File already exists. Overwrite?"),
					TRANS("Overwrite?"),wxYES_NO|wxICON_WARNING);
		
		if( wxMd.ShowModal() == wxID_YES)
			break;
	} while(true); 
	
	//Show a resolution chooser dialog
	ResolutionDialog d(this,wxID_ANY,TRANS("Choose resolution"));

	//Use the current res as the dialog default
	int w,h;
	panelTop->GetClientSize(&w,&h);
	d.setRes(w,h,true);

	//Show dialog, skip save if user cancels dialog
	if(d.ShowModal() == wxID_CANCEL)
		return;

	bool saveOK=panelTop->saveImage(d.getWidth(),d.getHeight(),dataFile.c_str());

	if(!saveOK)
	{
		wxErrMsg(this,TRANS("Save error"),
			TRANS("Unable to save. Check output destination can be written to."));
	}
	else
	{
		dataFile=std::string(TRANS("Saved 3D View :")) + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}

}


void MainWindowFrame::OnFileExportVideo(wxCommandEvent &event)
{
	wxFileDialog wxF(this,TRANS("Save Image..."), wxT(""),
		wxT(""),TRANS("PNG File (*.png)|*.png|All Files (*)|*"),wxFD_SAVE);

	if( (wxF.ShowModal() == wxID_CANCEL))
		return;
	
	//Show a resolution chooser dialog
	ResolutionDialog d(this,wxID_ANY,TRANS("Choose resolution"));

	//Use the current res as the dialog default
	int w,h;
	panelTop->GetClientSize(&w,&h);
	d.setRes(w,h,true);

	//Show dialog, skip save if user cancels dialo
	if(d.ShowModal() == wxID_CANCEL)
		return;

	if((d.getWidth() < w && d.getHeight() > h) ||
		(d.getWidth() > w && d.getHeight() < h) )
	{
		wxErrMsg(this, TRANS("Program limitation"),
			TRANS("Limitation on the screenshot dimension; please ensure that both width and height exceed the initial values,\n or that they are smaller than the initial values.\n If this bothers, please submit a bug."))
		;
		return;
	}


	wxFileName fname;
	wxString volume,path,name,ext;
	bool hasExt;
	fname.SplitPath(wxF.GetPath(),&volume,
			&path,&name,&ext, &hasExt);

	if(!hasExt)
		ext=wxT("png");

	///TODO: This is nasty and hackish. We should present a nice,
	//well laid out dialog for frame count (show angular increment) 
	wxTextEntryDialog teD(this,TRANS("Number of frames"),TRANS("Frame count"),
						wxT("180"),(long int)wxOK|wxCANCEL);

	unsigned int numFrames=0;
	std::string strTmp;
	do
	{
		if(teD.ShowModal() == wxID_CANCEL)
			return;


		strTmp = stlStr(teD.GetValue());
	}while(stream_cast(numFrames,strTmp));

	


	bool saveOK=panelTop->saveImageSequence(d.getWidth(),d.getHeight(),
							numFrames,path,name,ext);
							

	if(!saveOK)
	{
		wxErrMsg(this,TRANS("Save error"),TRANS("Unable to save. Check output destination can be written to."));
	}
	else
	{
		std::string dataFile = stlStr(wxF.GetPath());
		dataFile=std::string(TRANS("Saved 3D View :")) + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}
	


	//Force a paint update for the scene, to  ensure aspect ratio information is preserved
	wxPaintEvent ptEvent;
	wxPostEvent(panelTop,ptEvent);
}


void MainWindowFrame::setLockUI(bool locking=true,
		unsigned int lockMode=WINDOW_LOCK_REFRESH)
{
	unsigned int nUndo,nRedo;
	nUndo=visControl.state.treeState.getUndoSize();
	nRedo=visControl.state.treeState.getRedoSize();
	switch(lockMode)
	{
		case WINDOW_LOCK_REFRESH:
		{
			unsigned int nFilters;
			nFilters = visControl.state.treeState.size();
			comboFilters->Enable(!locking && nFilters);
			if(locking)
				refreshButton->SetLabel(TRANS("Abo&rt"));
			else
				refreshButton->SetLabel(TRANS("&Refresh"));
			refreshButton->Enable(nFilters);
			
			btnFilterTreeErrs->Enable(!locking);
			treeFilters->Enable(!locking);	


			editUndoMenuItem->Enable(!locking && nUndo);
			editRedoMenuItem->Enable(!locking && nRedo);
		
			fileMenu->Enable(ID_FILE_OPEN,!locking);
			fileMenu->Enable(ID_FILE_MERGE,!locking);
		
			gridFilterPropGroup->Enable(!locking);
			comboStash->Enable(!locking);

			//Locking of the tools pane
			checkWeakRandom->Enable(!locking);
			checkCaching->Enable(!locking);
			spinCachePercent->Enable(!locking);
			textLimitOutput->Enable(!locking);
			checkLimitOutput->Enable(!locking);

			fileMenu->Enable(ID_FILE_OPEN,!locking);
			fileMenu->Enable(ID_FILE_MERGE,!locking);

			//Save menu needs to be handled specially in the case of an unlock
			// as determining if it can be enabled needs work
			if(!locking)
				fileMenu->Enable(ID_FILE_SAVE,false);
			else
				setSaveStatus();

			fileMenu->Enable(ID_FILE_SAVEAS,!locking);

			for(size_t ui=0;ui<recentFilesMenu->GetMenuItemCount();ui++)
			{
				wxMenuItem *m;
				m=recentFilesMenu->FindItemByPosition(ui);
				m->Enable(!locking);

			}

			fileExport->Enable(ID_FILE_EXPORT_ANIMATION,!locking);
			fileExport->Enable(ID_FILE_EXPORT_FILTER_ANIMATION,!locking);
			fileExport->Enable(ID_FILE_EXPORT_PACKAGE,!locking);

			panelSpectra->limitInteraction(locking);
			break;
		}
		case WINDOW_LOCK_PROPEDIT:
		{
			comboFilters->Enable(!locking);
			refreshButton->Enable(!locking);
			btnFilterTreeErrs->Enable(!locking);

			comboStash->Enable(!locking);
			treeFilters->Enable(!locking);

			editUndoMenuItem->Enable(!locking && nUndo);
			editRedoMenuItem->Enable(!locking && nRedo);

			fileMenu->Enable(ID_FILE_OPEN,!locking);
			fileMenu->Enable(ID_FILE_MERGE,!locking);
			fileMenu->Enable(ID_FILE_SAVEAS,!locking);
			
			//Save menu needs to be handled specially in the case of an unlock
			// as determining if it can be enabled needs work
			if(!locking)
				fileMenu->Enable(ID_FILE_SAVE,false);
			else
				setSaveStatus();

			//Lock/unlock all the recent files entries
			for(size_t ui=0;ui<recentFilesMenu->GetMenuItemCount();ui++)
			{
				wxMenuItem *m;
				m=recentFilesMenu->FindItemByPosition(ui);
				m->Enable(!locking);

			}

			fileExport->Enable(ID_FILE_EXPORT_ANIMATION,!locking);
			fileExport->Enable(ID_FILE_EXPORT_FILTER_ANIMATION,!locking);
			fileExport->Enable(ID_FILE_EXPORT_PACKAGE,!locking);

			//Locking of the tools pane
			checkWeakRandom->Enable(!locking);
			checkCaching->Enable(!locking);
			checkLimitOutput->Enable(!locking);
			textLimitOutput->Enable(!locking);
			spinCachePercent->Enable(!locking);


			//Lock panel spectra, so we cannot alter things like ranges
			panelSpectra->limitInteraction(locking);
			break;
		}
		default:
			ASSERT(false);
	}
}

void MainWindowFrame::OnFileExportFilterVideo(wxCommandEvent &event)
{
	//Don't let the user run the animation dialog if they have
	// no filters open
	if(!visControl.state.treeState.size())
	{
		statusMessage(TRANS("Cannot animate with no filters."));
		return;
	}

	//Cannot proceed until refresh is completed or aborted
	if(refreshThreadActive())
		return;


	int w, h;
	panelTop->GetClientSize(&w,&h);
	
	ExportAnimationDialog *exportDialog = 
		new ExportAnimationDialog(this, wxID_ANY, wxEmptyString);
	exportDialog->setDefImSize(w,h);


	//FIXME: Tree ownership is very complex, making code here brittle
	// - order of operations for initing the export dialog is important
	// Getting/Setting animation state requires the filtertree to
	// be under exportDialog's control
	FilterTree treeWithCache;
	//Steal the filter tree, and give the pointer to the export dialog
	// viscontrol now has an empty tree, so watch out.
	visControl.state.treeState.swapFilterTree(treeWithCache);
	//supply a copy of the filter tree (w/o cache) to export dialog
	exportDialog->setTree(treeWithCache);

	//Set the saved animation properties, as needed
	{
	PropertyAnimator p;
	vector<pair<string,size_t> > pathMap;
	visControl.state.getAnimationState(p,pathMap);
	if(p.getMaxFrame())
	{
		exportDialog->setAnimationState(p,pathMap);
	}
	}
	
	exportDialog->prepare();


	//Display Animate dialog
	bool dialogErr;
	dialogErr=(exportDialog->ShowModal() == wxID_CANCEL);

	//even if user aborts, record the state of the animation
	{
	PropertyAnimator propAnim;
	vector<pair<string,size_t> > pathMap;
	exportDialog->getAnimationState(propAnim,pathMap);
	
	//restore the cache to viscontrol
	visControl.state.treeState.swapFilterTree(treeWithCache);

	visControl.state.setAnimationState(propAnim,pathMap);
	}

	//Stop processing here if user aborted
	if(dialogErr)
	{
		exportDialog->Destroy();
		return;
	}


	//Stop timer based events, and lock UI
	//--
	updateTimer->Stop();
	autoSaveTimer->Stop();
	//--

	size_t numFrames;
	numFrames=exportDialog->getNumFrames();

	//Display modal progress dialog
	//--
	wxProgressDialog *prog;
	prog = new wxProgressDialog(TRANS("Animating"),
		TRANS("Performing refresh"),numFrames,this,wxPD_CAN_ABORT|wxPD_APP_MODAL );
	prog->Show();
	//--

	currentlyUpdatingScene=true;

	string errMessage;
	bool needAbortDlg=false;


	//Modify the tree.
	for(size_t ui=0;ui<numFrames;ui++)
	{
		//If user presses abort, abort procedure
		if(!prog->Update(ui))
			break;

		bool needsUp;
		//steal tree, including caches, from viscontrol
		visControl.state.treeState.swapFilterTree(treeWithCache);
		
		//Modify the tree, as needed, altering cached data
		if(!exportDialog->getModifiedTree(ui,treeWithCache,needsUp))
		{
			std::string s;
			stream_cast(ui,s);
			errMessage = TRANS("Filter property change failed") + s;
			needAbortDlg=true;
			break;
		}

		//restore tree to viscontrol
		visControl.state.treeState.swapFilterTree(treeWithCache);

		//Perform update
		if(needsUp || !exportDialog->wantsOnlyChanges())
		{
			typedef std::vector<const FilterStreamData * >  STREAMOUT;
			std::list<FILTER_OUTPUT_DATA> outData;
			std::list<STREAMOUT> outStreams;
			std::vector<std::pair<const Filter *, std::string> > cMessages;
			ProgressData progData;

			//First try to refresh the tree
			if(visControl.state.treeState.refresh(outData,cMessages,progData))
			{
				std::string tmpStr;
				stream_cast(tmpStr,ui);
				errMessage=TRANS("Refresh failed on frame :") + tmpStr;
				needAbortDlg=true;
				break;
			}
			
		
			//Now obtain the output streams as a flat list
			for(list<FILTER_OUTPUT_DATA>::iterator it=outData.begin();
					it!=outData.end();++it)
				outStreams.push_back(it->second);


			try
			{
				if(exportDialog->wantsImages())
				{

					// Update the  scene contents.
					visControl.updateScene(outStreams,false);
					panelTop->forceRedraw();
					//Attempt to save the image to disk
					if(!panelTop->saveImage(exportDialog->getImageWidth(),
						exportDialog->getImageHeight(),
						exportDialog->getFilename(ui,FILENAME_IMAGE).c_str(),false,false))
					{
						pair<string,string> errMsg;
						string tmpStr;
						stream_cast(tmpStr,ui);
						errMsg.first=TRANS("Unable to save");
						errMsg.second = TRANS("Image save failed for frame ");
						errMsg.second+=tmpStr;
						throw errMsg;
					}
					
				}

				if(exportDialog->wantsIons())
				{
					//merge all the output streams into one
					vector<const FilterStreamData *> mergedStreams;
					for(list<STREAMOUT>::iterator it=outStreams.begin();
							it!=outStreams.end();++it)
					{
						size_t origSize;
						origSize=mergedStreams.size();
						mergedStreams.resize( origSize+ it->size());
						std::copy(it->begin(),it->end(),mergedStreams.begin() +origSize);
					}

					if(IonStreamData::exportStreams(mergedStreams,exportDialog->getFilename(ui,FILENAME_IONS)))
					{
						pair<string,string> errMsg;
						string tmpStr;
						stream_cast(tmpStr,ui);
						errMsg.first=TRANS("Ion save failed");
						errMsg.second = TRANS("Unable to save ions for frame ");
						errMsg.second+=tmpStr;
						throw errMsg;
					}
				}

				if(exportDialog->wantsPlots())
				{

					size_t plotNumber=0;
					//Save each plot by name, where possible
					for(list<STREAMOUT>::iterator it=outStreams.begin(); it!=outStreams.end();++it)
					{
						for(size_t uj=0;uj<it->size();uj++)
						{
							//Skip non plot output
							if((*it)[uj]->getStreamType() != STREAM_TYPE_PLOT ) 
								continue;

							//Save the plot output
							std::string filename;
							const PlotStreamData* p = (const PlotStreamData*)(*it)[uj];
							filename = exportDialog->getFilename(ui,FILENAME_PLOT,plotNumber);
						
							plotNumber++;

							if(!p->save(filename.c_str()))
							{
								pair<string,string> errMsg;
								string tmpStr;
								stream_cast(tmpStr,ui);
								errMsg.first=TRANS("Plot save failed");
								errMsg.second = TRANS("Unable to save plot or frame ");
								errMsg.second+=tmpStr;
								throw errMsg;

							}

						}

					}
				}

				if(exportDialog->wantsRanges())
				{
					size_t rangeNum=0;

					//TODO: Integrate enums for rangefiles?
					map<unsigned int,unsigned int> rangeEnumMap;
					rangeEnumMap[RANGE_OAKRIDGE] = RANGE_FORMAT_ORNL;
					rangeEnumMap[RANGE_AMETEK_RRNG] = RANGE_FORMAT_RRNG;
					rangeEnumMap[RANGE_AMETEK_ENV] = RANGE_FORMAT_ENV;
					//Save each range
					for(list<STREAMOUT>::iterator it=outStreams.begin(); it!=outStreams.end();++it)
					{
						for(size_t uj=0;uj<it->size();uj++)
						{
							//Skip non plot output
							if((*it)[uj]->getStreamType() != STREAM_TYPE_RANGE) 
								continue;

							//Save the plot output
							std::string filename;
							const RangeStreamData* p = (const RangeStreamData*)(*it)[uj];
							filename = exportDialog->getFilename(ui,FILENAME_RANGE,rangeNum);

							size_t format;
							format=rangeEnumMap.at(exportDialog->getRangeFormat());

							if(!p->save(filename.c_str(),format))
							{	pair<string,string> errMsg;
								string tmpStr;
								stream_cast(tmpStr,ui);
								errMsg.first=TRANS("Range save failed");
								errMsg.second = TRANS("Unable to save range for frame ");
						
								throw errMsg;
							}

						}
					}
				}
			

				if(exportDialog->wantsVoxels())
				{
					size_t offset=0;
					for(list<STREAMOUT>::iterator it=outStreams.begin(); it!=outStreams.end();++it)
					{
						for(size_t uj=0;uj<it->size();uj++)
						{
							if( ((*it)[uj])->getStreamType() != STREAM_TYPE_VOXEL)
								continue;

							const VoxelStreamData *v;
							v=(const VoxelStreamData*)(*it)[uj];
							
							std::string filename = exportDialog->getFilename(ui,FILENAME_VOXEL,offset);
							if(v->data->writeFile(filename.c_str()))
							{
								pair<string,string> errMsg;
								string tmpStr;
								stream_cast(tmpStr,ui);
								errMsg.first=TRANS("Voxel save failed");
								errMsg.second = TRANS("Unable to save voxels for frame ");
								errMsg.second+=tmpStr;
								throw errMsg;
							}
				
							offset++;
						}
					}
				}
			}
			catch(std::pair<string,string> &errMsg)
			{
				errMessage=errMsg.first + "\n" + errMsg.second;
				//clean up data
				FilterTree::safeDeleteFilterList(outData);
				needAbortDlg=true;
				break;
			}

			//Clean up date from this run, releasing stream pointers.
			FilterTree::safeDeleteFilterList(outData);
			outStreams.clear();

		}

	}

	
	if(needAbortDlg)
		wxErrMsg(this,TRANS("Animate failed"),errMessage);

	currentlyUpdatingScene=false;

	//Re-run the scene update for the original case,
	// this allows for things like the selection bindings to be reinitialised.
	doSceneUpdate();
	
	//Restore UI and timers
	//--
	prog->Destroy();
	exportDialog->Destroy();
	
	panelTop->Enable(true);
	
	updateTimer->Start(UPDATE_TIMER_DELAY,wxTIMER_CONTINUOUS);
	autoSaveTimer->Start(AUTOSAVE_DELAY*1000,wxTIMER_CONTINUOUS);
	//--
}

void MainWindowFrame::OnFileExportPackage(wxCommandEvent &event)
{
	if(!treeFilters->GetCount())
	{
		statusMessage(TRANS("No filters means no data to export"),MESSAGE_ERROR);
		return;

	}

	//Determine if we want to export a debug package (hold CTRL+SHIFT during export menu select)
	bool wantDebugPack;
	{
	bool shiftState=wxGetKeyState(WXK_SHIFT);
	bool ctrlState= wxGetKeyState(WXK_CONTROL);
	wantDebugPack=(shiftState && ctrlState);
	}
		
	//This could be nicer, or reordered
	wxTextEntryDialog wxTD(this,TRANS("Package name"),
					TRANS("Package directory name"),wxT(""),wxOK|wxCANCEL);

	wxTD.SetValue(TRANS("AnalysisPackage"));

	if(wxTD.ShowModal() == wxID_CANCEL)
		return;


	//Pop up a directory dialog, to choose the base path for the new folder

	unsigned int res;
	
	wxDirDialog wxD(this);
	res = wxD.ShowModal();
	
	wxMessageDialog wxMesD(this,TRANS("Package folder already exists, won't overwrite.")
					,TRANS("Not available"),wxOK|wxICON_ERROR);

	while(res != wxID_CANCEL)
	{
		//Dir cannot exist yet, as we want to make it.
		if(wxDirExists(wxD.GetPath() +wxFileName::GetPathSeparator()
			+ wxTD.GetValue()))
		{
			wxMesD.ShowModal();
			res=wxD.ShowModal();
		}
		else
			break;
	}

	//User aborted directory choice. 
	if(res==wxID_CANCEL)
		return;

	wxString folder;
	folder=wxD.GetPath() + wxFileName::GetPathSeparator() + wxTD.GetValue() +
			wxFileName::GetPathSeparator();
	//Check to see that the folder actually exists
	if(!wxMkdir(folder))
	{
		wxMessageDialog wxMesD(this,TRANS("Package folder creation failed\ncheck writing to this location is possible.")
						,TRANS("Folder creation failed"),wxOK|wxICON_ERROR);
		wxMesD.ShowModal();
		return;
	}



	//OK, so the folder exists, lets make the XML state file
	std::string dataFile = string(stlStr(folder)) + "state.xml";

	std::map<string,string> fileMapping;
	//Try to save the viscontrol state
	if(!visControl.state.save(dataFile.c_str(),fileMapping,true))
	{
		wxErrMsg(this,TRANS("Save error"),
			TRANS("Unable to save. Check output destination can be written to."));
	}
	else
	{
		//Copy the files in the mapping
		wxProgressDialog wxP(TRANS("Copying"),
			TRANS("Copying referenced files"),fileMapping.size());

		wxP.Show();
		for(map<string,string>::iterator it=fileMapping.begin();
				it!=fileMapping.end();++it)
		{
			//Hack, if we are exporting a debugging package,
			// pos files should be 
			// only copied for the first CHUNKSIZE bytes 
			bool copyError; bool isPosFile;
			copyError=false;isPosFile=false;
			
			string strName=it->first;

			if(strName.size() > 4)
			{
				strName=strName.substr(strName.size()-4);
				if(strName == ".pos")
					isPosFile=true;	
			}

			size_t filesize;
			const size_t CHUNKSIZE=1024*1024*2;
			filesize=0;
			if(wantDebugPack && isPosFile)
				getFilesize(it->second.c_str(),filesize);
		
			//If we want a debugging package, then only copy the first part of the file	
			if(wantDebugPack && isPosFile && filesize > CHUNKSIZE)
			{
				std::ifstream inputF(it->second.c_str(),std::ios::binary);
				if(!inputF)
				{
					copyError=true;
					break;
				}
			
				//copy one chunk
				char *c = new char[CHUNKSIZE];
				std::string outfname;
				outfname=stlStr(folder) + it->first;
				std::ofstream of(outfname.c_str(),std::ios::binary);
				if(!of)
				{
					delete[] c;
					copyError=true;
					break;
				}
				inputF.read(c,CHUNKSIZE);
				of.write(c,CHUNKSIZE);

				delete[] c;
			}
			else
			{
				//if the file exists, then try to copy it to the local folder.
				// The file might be optional, and therefore blank, so it is not an error
				// to not have the file existing
				if(wxFileExists(it->second))
					copyError=!wxCopyFile((it->second),folder+(it->first));
				else
					copyError=false;
			}

			if(copyError)
			{
				wxErrMsg(this,TRANS("Save error"),TRANS("Error copying file"));
				return;
			}
			wxP.Update();
		}



		wxString s;
		s=wxString(TRANS("Saved package: ")) + folder;
		if(wantDebugPack)
		{
			s+=(" (debug mode)");
		}
		statusMessage(stlStr(s).c_str(),MESSAGE_INFO);
	}
}

void MainWindowFrame::OnFileExportIons(wxCommandEvent &event)
{
	if(!treeFilters->GetCount())
	{
		statusMessage(TRANS("No filters means no data to export"),MESSAGE_ERROR);
		return;

	}


	//Steal the filter tree (including caches) from viscontrol
	FilterTree f;
	visControl.state.treeState.switchoutFilterTree(f);
	
	//Load up the export dialog
	ExportPosDialog *exportDialog=new ExportPosDialog(this,wxID_ANY,TRANS("Export"));
	exportDialog->initialiseData(f);
	
	//create a file chooser for later. The format string is special as we use it to demux the 
	// format later
	wxFileDialog wxF(this,TRANS("Save pos..."), wxT(""),
		wxT(""),TRANS("POS Data (*.pos)|*.pos|Text File (*.txt)|*.txt|VTK Legacy (*.vtk)|*.vtk|All Files (*)|*"),wxFD_SAVE);
	
	//If the user cancels the file chooser, 
	//drop them back into the export dialog.
	do
	{
		cerr << "Show dialog" << __FILE__ << " :" << __LINE__ << endl;
		//Show, then check for user cancelling export dialog
		if(exportDialog->ShowModal() == wxID_CANCEL)
		{
			//Take control of the filter tree back from the export dialog,
			// and return it to visControl	
			exportDialog->swapFilterTree(f);
			visControl.state.treeState.swapFilterTree(f);
			exportDialog->Destroy();
			
			//Need this to reset the ID values
			updateWxTreeCtrl(treeFilters);
			return;	
		}
		
	}
	while( (wxF.ShowModal() == wxID_CANCEL)); //Check for user abort in file chooser

	

	//Check file already exists (no overwrite without asking)
	if(wxFileExists(wxF.GetPath()))
	{
		wxMessageDialog wxD(this,TRANS("File already exists, overwrite?")
				   ,TRANS("Overwrite?"),wxOK|wxCANCEL|wxICON_QUESTION);

		if(wxD.ShowModal() == wxID_CANCEL)
		{
			//Take control of the filter tree back from the export dialog,
			// and return it to visControl	
			exportDialog->swapFilterTree(f);
			visControl.state.treeState.swapFilterTree(f);
			
			//Need this to reset the ID values
			updateWxTreeCtrl(treeFilters);
			exportDialog->Destroy();
			return;
		}
	}
	
	std::string dataFile = stlStr(wxF.GetPath());
	
	//Retrieve the ion streams that we need to save
	vector<const FilterStreamData *> exportVec;
	exportDialog->getExportVec(exportVec);


	//Using the wildcard constant selected, set if we want text or pos
	unsigned int format;
	if(wxF.GetFilterIndex() == 0)
		format = IONFORMAT_POS;
	else if(wxF.GetFilterIndex() == 1)
		format = IONFORMAT_TEXT;
	else
		format = IONFORMAT_VTK; 

	//write the ion streams to disk
	if(IonStreamData::exportStreams(exportVec,dataFile,format))
	{
		wxErrMsg(this,TRANS("Save error"),
			TRANS("Unable to save. Check output destination can be written to."));
	}
	else
	{
		dataFile=std::string(TRANS("Saved ions: ")) + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}

	//Take control of the filter tree back from the export dialog,
	// and return it to visControl	
	exportDialog->swapFilterTree(f);
	visControl.state.treeState.swapFilterTree(f);

	//Call ->Destroy to invoke destructor, which will safely delete the
	//filterstream pointers it generated	
	exportDialog->Destroy();
	//Need this to reset the ID values
	updateWxTreeCtrl(treeFilters);
}

void MainWindowFrame::OnFileExportRange(wxCommandEvent &event)
{

	if(!treeFilters->GetCount())
	{
		statusMessage(TRANS("No filters means no data to export"),
				MESSAGE_ERROR);
		return;
	}
	ExportRngDialog *rngDialog = new ExportRngDialog(this,wxID_ANY,TRANS("Export Ranges"),
							wxDefaultPosition,wxSize(600,400));

	vector<const Filter *> rangeData;
	//Retrieve all the range filters in the viscontrol
	visControl.state.treeState.getFiltersByType(rangeData,FILTER_TYPE_RANGEFILE);
	//pass this to the range dialog
	rngDialog->addRangeData(rangeData);

	if(rngDialog->ShowModal() == wxID_CANCEL)
	{
		rngDialog->Destroy();
		return;
	}

	rngDialog->Destroy();
}


void MainWindowFrame::OnFileSaveAs(wxCommandEvent &event)
{
	//Show a file save dialog
	wxFileDialog wxF(this,TRANS("Save state..."), wxT(""),
		wxT(""),TRANS("XML state file (*.xml)|*.xml|All Files (*)|*"),wxFD_SAVE);

	//Show, then check for user cancelling dialog
	if( (wxF.ShowModal() == wxID_CANCEL))
		return;

	std::string dataFile = stlStr(wxF.GetPath());

	wxFileName fname;
	wxString volume,path,name,ext;
	bool hasExt;
	fname.SplitPath(wxF.GetPath(),&volume,
			&path,&name,&ext, &hasExt);

	//Check file already exists (no overwrite without asking)
	if(wxFileExists(wxF.GetPath()))
	{
		wxMessageDialog wxD(this,TRANS("File already exists, overwrite?")
						,TRANS("Overwrite?"),wxOK|wxCANCEL|wxICON_QUESTION);

		if(wxD.ShowModal() == wxID_CANCEL)
			return;
	}
	if(hasExt)
	{
		//Force the string to end in ".xml"	
		std::string strExt;
		strExt=stlStr(ext);
		strExt = lowercase(strExt);
		if(strExt != "xml")
			dataFile+=".xml";
	}
	else
		dataFile+=".xml";

	bool oldRelPath=visControl.state.getUseRelPaths();
	//Check to see if we have are using relative paths,
	//and if so, do any of our filters
	if(visControl.state.getUseRelPaths() && visControl.state.hasStateOverrides())
	{
		wxMessageDialog wxD(this,TRANS("Files have been referred to using relative paths. Keep relative paths?")
						,TRANS("Overwrite?"),wxYES|wxNO|wxICON_QUESTION);
	
		wxD.SetEscapeId(wxID_NO);
		wxD.SetAffirmativeId(wxID_YES);
		//Just for the moment, set relative paths to false, if the user asks.
		//we will restore this later
		if(wxD.ShowModal() == wxID_NO)
		{
			oldRelPath=true;
			visControl.state.setUseRelPaths(false);
		}

	}


	std::map<string,string> dummyMap;
	//Try to save the viscontrol state
	if(!visControl.state.save(dataFile.c_str(),dummyMap,false))
	{
		wxErrMsg(this,TRANS("Save error"),
			TRANS("Unable to save. Check output destination can be written to."));
	}
	else
	{
		std::string tmpStr;
		tmpStr=stlStr(wxF.GetPath());
		visControl.state.setFilename(tmpStr);

		//Update the recent files, and the menu.
		configFile.addRecentFile(dataFile);
		recentHistory->AddFileToHistory((dataFile));
	
		dataFile=std::string(TRANS("Saved state: ")) + dataFile;
		statusMessage(dataFile.c_str(),MESSAGE_INFO);
	}

	//Restore the relative path behaviour
	visControl.state.setUseRelPaths(oldRelPath);
	setSaveStatus();
}


void MainWindowFrame::OnFileExit(wxCommandEvent &event)
{
	//Close query is handled by OnClose()
	Close();
}

void MainWindowFrame::OnEditUndo(wxCommandEvent &event)
{
	
	visControl.state.treeState.popUndoStack();

	//Get vis controller to update tree control to match internal
	// structure. Retain tree selection & visibility if we currently
	// have a valid selection
	size_t filterId;
	if(getTreeFilterId(treeFilters->GetSelection(),filterId))
		visControl.setWxTreeFilterViewPersistence(filterId);

	//Update tree control
	updateWxTreeCtrl(treeFilters);

	if(getTreeFilterId(treeFilters->GetSelection(),filterId))
	{
		//Update property grid	
		visControl.updateFilterPropGrid(gridFilterPropGroup,filterId);
	}
	else
	{
		gridFilterPropGroup->Clear();
	}


	

	doSceneUpdate();
}

void MainWindowFrame::OnEditRedo(wxCommandEvent &event)
{
	visControl.state.treeState.popRedoStack();

	size_t filterId;
	if(getTreeFilterId(treeFilters->GetSelection(),filterId))
		visControl.setWxTreeFilterViewPersistence(filterId);

	//Update tree control
	updateWxTreeCtrl(treeFilters);

	//If we can still get the ID, lets use it
	if(getTreeFilterId(treeFilters->GetSelection(),filterId))
	{
		//Update property grid	
		visControl.updateFilterPropGrid(gridFilterPropGroup, filterId);
	}
	else
	{
		gridFilterPropGroup->Clear();
	}



	doSceneUpdate();
}

void MainWindowFrame::OnEditRange(wxCommandEvent &event)
{
	RangeEditorDialog *r = new RangeEditorDialog(this,wxID_ANY,TRANS("Range editor"));

	r->setPlotWrapper(*(visControl.getPlotWrapper()));

	if(r->ShowModal() == wxID_CANCEL)
	{
		r->Destroy();
		return;
	}

	//Obtain the modified rangefiles from the dialog
	map<const RangeFile *, const RangeFile *> modifiedRanges;
	r->getModifiedRanges(modifiedRanges);

	//Pass the modified rangefiles to viscontrol
	visControl.state.treeState.modifyRangeFiles(modifiedRanges);

	r->Destroy();

	
	doSceneUpdate();

}

void MainWindowFrame::OnEditPreferences(wxCommandEvent &event)
{
	//Create  a new preference dialog
	PrefDialog *p = new PrefDialog(this,wxID_ANY,wxT("Preferences"));

	//TODO: Refactor preference dialog to accept a config file object

	vector<Filter *> filterDefaults;

	//obtain direct copies of the cloned Filter pointers
	configFile.getFilterDefaults(filterDefaults);
	p->setFilterDefaults(filterDefaults);

	//Get the default mouse/camera parameters
	unsigned int mouseZoomRate,mouseMoveRate;
	bool preferOrthoCamera;
	mouseZoomRate=configFile.getMouseZoomRate();
	mouseMoveRate=configFile.getMouseMoveRate();
	preferOrthoCamera=configFile.getWantStartupOrthoCam();
	
	
	unsigned int panelMode;

	//Set Panel startup flags
	bool rawStartup,controlStartup,plotStartup;
	controlStartup=configFile.getPanelEnabled(CONFIG_STARTUPPANEL_CONTROL);
	rawStartup=configFile.getPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA);
	plotStartup=configFile.getPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST);

	panelMode=configFile.getStartupPanelMode();

	p->setPanelDefaults(panelMode,controlStartup,rawStartup,plotStartup);

#ifndef DISABLE_ONLINE_UPDATE
	p->setAllowOnlineUpdate(configFile.getAllowOnlineVersionCheck());
#endif


	p->setMouseZoomRate(mouseZoomRate);
	p->setMouseMoveRate(mouseMoveRate);
	p->setPreferOrthoCam(preferOrthoCamera);

	//Initialise panel
	p->initialise();
	//show panel
	if(p->ShowModal() !=wxID_OK)
	{
		p->cleanup();
		p->Destroy();
		return;
	}

	filterDefaults.clear();

	//obtain cloned copies of the pointers
	p->getFilterDefaults(filterDefaults);

	mouseZoomRate=p->getMouseZoomRate();
	mouseMoveRate=p->getMouseMoveRate();
	preferOrthoCamera=p->getPreferOrthoCam();

	panelTop->setMouseZoomFactor((float)mouseZoomRate/100.0f);
	panelTop->setMouseMoveFactor((float)mouseMoveRate/100.0f);

	configFile.setMouseZoomRate(mouseZoomRate);
	configFile.setMouseMoveRate(mouseMoveRate);
	configFile.setWantStartupOrthoCam(preferOrthoCamera);

	//Note that this transfers control of pointer to the config file 
	configFile.setFilterDefaults(filterDefaults);

	//Retrieve pane settings, and pass to config manager
	p->getPanelDefaults(panelMode,controlStartup,rawStartup,plotStartup);
	
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_CONTROL,controlStartup,true);
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA,rawStartup,true);
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST,plotStartup,true);

	configFile.setStartupPanelMode(panelMode);

#ifndef DISABLE_ONLINE_UPDATE
	configFile.setAllowOnline(p->getAllowOnlineUpdate());
	configFile.setAllowOnlineVersionCheck(p->getAllowOnlineUpdate());
#endif
	

	p->cleanup();
	p->Destroy();
}

void MainWindowFrame::OnViewBackground(wxCommandEvent &event)
{

	//retrieve the current colour from the openGL panel	
	float r,g,b;
	panelTop->getGlClearColour(r,g,b);	
	//Show a wxColour choose dialog. 
	wxColourData d;
	d.SetColour(wxColour((unsigned char)(r*255),(unsigned char)(g*255),
	(unsigned char)(b*255),(unsigned char)(255)));
	wxColourDialog *colDg=new wxColourDialog(this->GetParent(),&d);

	if( colDg->ShowModal() == wxID_OK)
	{
		wxColour c;
		//Change the colour
		c=colDg->GetColourData().GetColour();
	
		//Scale colour ranges to 0-> 1 and set in the gl pane	
		panelTop->setGlClearColour(c.Red()/255.0f,c.Green()/255.0f,c.Blue()/255.0f);
	}

	panelTop->forceRedraw();
}

void MainWindowFrame::OnViewControlPane(wxCommandEvent &event)
{
	if(event.IsChecked())
	{
		if(!splitLeftRight->IsSplit())
		{
			const float SPLIT_FACTOR=0.3;
			int x,y;
			GetClientSize(&x,&y);
			splitLeftRight->SplitVertically(panelLeft,
						panelRight,(int)(SPLIT_FACTOR*x));
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_CONTROL,true);
	
		}
	}
	else
	{
		if(splitLeftRight->IsSplit())
		{
			splitLeftRight->Unsplit(panelLeft);
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_CONTROL,false);
		}
	}
}


void MainWindowFrame::OnViewRawDataPane(wxCommandEvent &event)
{
	if(event.IsChecked())
	{
		if(!splitTopBottom->IsSplit())
		{
			const float SPLIT_FACTOR=0.3;

			int x,y;
			GetClientSize(&x,&y);
			splitTopBottom->SplitHorizontally(panelTop,
						noteDataView,(int)(SPLIT_FACTOR*x));
	
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA,true);
		}
	}
	else
	{
		if(splitTopBottom->IsSplit())
		{
			splitTopBottom->Unsplit();
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA,false);
		}
	}
}

void MainWindowFrame::OnViewSpectraList(wxCommandEvent &event)
{
	if(event.IsChecked())
	{
		if(!splitterSpectra->IsSplit())
		{
			const float SPLIT_FACTOR=0.6;

			int x,y;
			splitterSpectra->GetClientSize(&x,&y);
			splitterSpectra->SplitVertically(panelSpectra,
						window_2_pane_2,(int)(SPLIT_FACTOR*x));
	
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST,true);
		}
	}
	else
	{
		if(splitterSpectra->IsSplit())
		{
			splitterSpectra->Unsplit();
			configFile.setPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST,false);
		}
	}
}

void MainWindowFrame::OnViewPlotLegend(wxCommandEvent &event)
{
	panelSpectra->setLegendVisible(event.IsChecked());
	panelSpectra->Refresh();
}

void MainWindowFrame::OnViewWorldAxis(wxCommandEvent &event)
{
	visControl.scene.setWorldAxisVisible(event.IsChecked());
	panelTop->forceRedraw();
}

void MainWindowFrame::OnHelpHelp(wxCommandEvent &event)
{
	//First attempt to locate the local copy of the manual.
	string s;
	s=locateDataFile("3Depict-manual.pdf");

	//Also Debian makes us use the lowercase "D", so check there too.
	if(!s.size())
		s=locateDataFile("3depict-manual.pdf");

	//FIXME: under windows, currently we use "manual.pdf"
	if(!s.size())
		s=locateDataFile("manual.pdf");

	//If we found it, use the default program associated with that data file
	bool launchedOK=false;
	if( wxFileExists((s))  && s.size())
	{
		//we found the manual. Launch the default handler.
		launchedOK=wxLaunchDefaultApplication((s));
	}

	//Still no go? Give up and launch a browser.
	if(!launchedOK)
	{
		std::string helpFileLocation("http://threedepict.sourceforge.net/documentation.html");
		wxLaunchDefaultBrowser((helpFileLocation),wxBROWSER_NEW_WINDOW);

		statusMessage(TRANS("Manual not found locally. Launching web browser"),MESSAGE_INFO);
	}
}

void MainWindowFrame::OnHelpContact(wxCommandEvent &event)
{
	std::string contactFileLocation("http://threedepict.sourceforge.net/contact.html");
	wxLaunchDefaultBrowser((contactFileLocation),wxBROWSER_NEW_WINDOW);

	statusMessage(TRANS("Opening contact page in external web browser"),MESSAGE_INFO);
}

void MainWindowFrame::OnButtonStashDialog(wxCommandEvent &event)
{

	if(!visControl.state.getStashCount())
	{
		statusMessage(TRANS("No filter stashes to edit."),MESSAGE_ERROR);
		return;
	}

	StashDialog *s = new StashDialog(this,wxID_ANY,TRANS("Filter Stashes"));
	s->setVisController(&visControl);
	s->ready();
	s->ShowModal();

	s->Destroy();

	//Stash list may have changed. Force update
	visControl.updateStashComboBox(comboStash);
}


void MainWindowFrame::OnHelpAbout(wxCommandEvent &event)
{
	wxAboutDialogInfo info;
	info.SetName((PROGRAM_NAME));
	info.SetVersion((PROGRAM_VERSION));
	info.SetDescription(TRANS("Quick and dirty analysis for point data.")); 
	info.SetWebSite(wxT("https://threedepict.sourceforge.net/"));

	info.AddDeveloper(wxT("D. Haley"));	
	info.AddDeveloper(wxT("A. Ceguerra"));	
	//GNU GPL v3
	info.SetCopyright(_T("Copyright (C) 2015 3Depict team\n This software is licenced under the GPL Version 3.0 or later\n This program comes with ABSOLUTELY NO WARRANTY.\nThis is free software, and you are welcome to redistribute it\nunder certain conditions; Please see the file COPYING in the program directory for details"));	

	info.AddArtist(_T("Thanks go to all who have developed the libraries that I use, which make this program possible.\n This includes the wxWidgets team, Alexy Balakin (MathGL), the FTGL and freetype people, the GNU Scientific Library contributors, the tree.h guy (Kasper Peeters)  and more."));

	info.AddArtist(wxString(TRANS("Compiled with wx Version: " )) + 
			wxString(wxSTRINGIZE_T(wxVERSION_STRING)));

	wxArrayString s;
	s.Add(_T("Deutsch (German) : Erich (de)"));
	info.SetTranslators(s);

	wxAboutBox(info);
}


void MainWindowFrame::OnComboStashText(wxCommandEvent &event)
{
	std::string s;
	s=stlStr(comboStash->GetValue());
	if(!s.size())
		return;
	
	int n = comboStash->FindString(comboStash->GetValue());
	
	if ( n== wxNOT_FOUND ) 
		statusMessage(TRANS("Press enter to store new stash"),MESSAGE_HINT);
	else
	{
		//The combo generates an ontext event when a string
		//is selected (yeah, I know, weird..) Block this case.
		if(comboStash->GetSelection() != n)
			statusMessage(TRANS("Press enter to restore stash"),MESSAGE_HINT);
	}
}

void MainWindowFrame::OnComboStashEnter(wxCommandEvent &event)
{
	//The user has pressed enter, in the combo box. If there is an existing stash of this name,
	//use it. Otherwise store the current tree control as part of the new stash
	std::string userText;

	userText=stlStr(comboStash->GetValue());

	//Forbid names with no text content
	userText=stripWhite(userText);
	if(!userText.size())
		return;

	unsigned int stashPos = (unsigned int ) -1;
	unsigned int nStashes = visControl.state.getStashCount();
	for(unsigned int ui=0;ui<nStashes; ui++)
	{
		if(visControl.state.getStashName(ui)== userText)
		{
			stashPos=ui;
			break;
		}
	}

	if(stashPos == (unsigned int) -1)
	{
		size_t filterId;
		if(!getTreeFilterId(treeFilters->GetSelection(),filterId))
		{
			statusMessage(TRANS("Unable to create stash, selection invalid"),MESSAGE_ERROR);
			return;
		}

		visControl.state.stashFilters(filterId,userText.c_str());
		visControl.updateStashComboBox(comboStash);
			
		statusMessage(TRANS("Created new filter tree stash"),MESSAGE_INFO);

	}
	else
	{
		//Stash exists, process as if we selected it
		OnComboStash(event);
	}

	//clear the text in the combo box
	comboStash->SetValue(wxT(""));
}

void MainWindowFrame::OnComboFilterText(wxCommandEvent &event)
{
	//prevent user from modifying text
#ifndef __APPLE__
	comboFilters->ChangeValue(TRANS(ADD_FILTER_TEXT));
#endif
}

void MainWindowFrame::OnComboStash(wxCommandEvent &event)
{
	//Find the stash associated with this item
	wxListUint *l;
	l =(wxListUint*)comboStash->GetClientObject(comboStash->GetSelection());

	size_t filterId;
	//Get the parent filter from the tree selection
	if(getTreeFilterId(treeFilters->GetSelection(),filterId))
	{
		//Get the parent filter pointer	
		const Filter *parentFilter=
			visControl.state.treeState.getFilterById(filterId);
	
		visControl.state.addStashedToFilters(parentFilter,l->value);
		
		updateWxTreeCtrl(treeFilters,
						parentFilter);

		if(checkAutoUpdate->GetValue())
			doSceneUpdate();


	}
	
	//clear the text in the combo box
	comboStash->SetValue(wxT(""));
}



void MainWindowFrame::OnTreeEndDrag(wxTreeEvent &event)
{
	if(refreshThreadActive())
	{
		event.Veto();
		return;
	}

	//Should be enforced by ::Allow() in start drag.
	ASSERT(filterTreeDragSource && filterTreeDragSource->IsOk()); 
	//Allow tree to be manhandled, so you can move filters around
	wxTreeItemId newParent = event.GetItem();

	bool needRefresh=false;
	size_t sId;
	if(!getTreeFilterId(*filterTreeDragSource,sId))
		return;

	wxMouseState wxm = wxGetMouseState();


	//if we have a parent node to reparent this to	
	if(newParent.IsOk())
	{
		size_t pId;
		if(!getTreeFilterId(newParent,pId))
			return;

		//Copy elements from a to b, if a and b are not the same
		if(pId != sId)
		{
			visControl.setWxTreeFilterViewPersistence(sId);
			visControl.setWxTreeFilterViewPersistence(pId);
			//If command button down (ctrl or clover on mac),
			//then copy, otherwise move
			if(wxm.CmdDown())
				needRefresh=visControl.state.treeState.copyFilter(sId,pId);
			else
				needRefresh=visControl.state.treeState.reparentFilter(sId,pId);
		}	
	}
	else 
	{

		const Filter *fSource = visControl.state.treeState.getFilterById(sId);
		
		//Only filters that are a data source are allowed to be in the base.
		if( fSource->isPureDataSource())
		{
			if(wxm.CmdDown())
				needRefresh=visControl.state.treeState.copyFilter(sId,0);
			else
				needRefresh=visControl.state.treeState.reparentFilter(sId,0);
		}
		else
			statusMessage(TRANS("Filter type not a data source - can't be at tree base"),MESSAGE_ERROR);
	}

	if(needRefresh )
	{
		//Refresh the treecontrol
		updateWxTreeCtrl(treeFilters);

		//We have finished the drag	
		statusMessage("",MESSAGE_NONE);
		if(checkAutoUpdate->GetValue())
			doSceneUpdate();
	}
	delete filterTreeDragSource;
	filterTreeDragSource=0;	
}

void MainWindowFrame::OnTreeSelectionPreChange(wxTreeEvent &event)
{
	if(refreshThreadActive())
	{
		event.Veto();
		return;
	}
}


void MainWindowFrame::OnTreeSelectionChange(wxTreeEvent &event)
{
	if(programmaticEvent)
		return;

	ASSERT(!refreshThreadActive())

	size_t filterId;
	if(!getTreeFilterId(treeFilters->GetSelection(),filterId))
	{
		gridFilterPropGroup->Clear();
		return;
	}

	comboFilters->Enable();
	visControl.updateFilterPropGrid(gridFilterPropGroup, filterId);

	panelTop->forceRedraw();
}


void MainWindowFrame::updateEditRangeMenu()
{
	vector<const Filter *> filtersRange,filtersSpectra;
	visControl.state.treeState.getFiltersByType(filtersRange,FILTER_TYPE_RANGEFILE);
	visControl.state.treeState.getFiltersByType(filtersSpectra,FILTER_TYPE_SPECTRUMPLOT);

	//Only show the menu item if we have both ranges and plots in our
	// filter tree
	bool wantEnable = filtersRange.size() && filtersSpectra.size();
	editRangeMenuItem->Enable(wantEnable);
}

void MainWindowFrame::OnTreeDeleteItem(wxTreeEvent &event)
{
	if(refreshThreadActive())
	{
		ASSERT(false); //Shouldn't happen, but might have...
		event.Veto();
		return;
	}
	//This event is only generated programatically,
	// we do not have to handle the direct deletion.

}

void MainWindowFrame::OnTreeBeginLabelEdit(wxTreeEvent &event)
{
	if(refreshThreadActive() )
	{
		ASSERT(false);
		event.Veto();
		return;
	}
}

void MainWindowFrame::OnTreeEndLabelEdit(wxTreeEvent &event)
{
	if(event.IsEditCancelled())
		return;


	//There is a case where the tree doesn't quite clear
	//when there is an editor involved.
	if(visControl.state.treeState.size())
	{
		std::string s;
		s=stlStr(event.GetLabel());
		if(s.size())
		{
			size_t filterId;
			if(!getTreeFilterId(treeFilters->GetSelection(),filterId))
				return;
			
			//If the string has been changed, then we need to update	
			visControl.state.treeState.setFilterString(filterId,s);
			//We need to reupdate the scene, in order to re-fill the 
			//spectra list box
			doSceneUpdate();
		}
		else
		{
			event.Veto(); // Disallow blank strings.
		}
	}
}

void MainWindowFrame::OnTreeBeginDrag(wxTreeEvent &event)
{
	if(refreshThreadActive() )
	{
		ASSERT(false); //shouldn't happen (should lock), but might
		event.Veto();
		return;
	}

	//No dragging if editing, or if no filters
	if(treeFilters->GetEditControl() || event.GetItem() == treeFilters->GetRootItem())
	{
		event.Veto();
		return;
	}
	
	//Record the drag source
	wxTreeItemId t = event.GetItem();

	if(t.IsOk())
	{
		filterTreeDragSource = new wxTreeItemId;
		*filterTreeDragSource =t;
		event.Allow();

#ifdef __APPLE__    
		statusMessage(TRANS("Moving - Hold ⌘ (command) to copy"),MESSAGE_HINT);
#else
		statusMessage(TRANS("Moving - Hold control to copy"),MESSAGE_HINT);
#endif
	}

}

void MainWindowFrame::OnBtnExpandTree(wxCommandEvent &event)
{
	treeFilters->ExpandAll();
}


void MainWindowFrame::OnBtnCollapseTree(wxCommandEvent &event)
{
	treeFilters->CollapseAll();
}

void MainWindowFrame::OnBtnFilterTreeErrs(wxCommandEvent &event)
{

	//Grab the error strings
	vector<FILTERTREE_ERR> res;
	visControl.state.treeState.getAnalysisResults(res);

	ASSERT(res.size());

	vector<string> errStrings;

	for(unsigned int ui=0;ui<res.size();ui++)
	{
		std::string s;

		switch(res[ui].severity)
		{
			case ANALYSE_SEVERITY_WARNING:
				s += "Warning:\n";
				break;
			case ANALYSE_SEVERITY_ERROR:
				s+="Error:\n" ;
				break;
			default:
				ASSERT(false);
		}
		
		s=  res[ui].shortReportMessage + "\n";
		s+= "\t" +  res[ui].verboseReportMessage +"\n";
		if(res[ui].reportedFilters.size())
		{
			s+="\tImplicated Filters:\n"; 
			for(unsigned int uj=0;uj<res[ui].reportedFilters.size(); uj++)
				s+="\t\t->" + res[ui].reportedFilters[uj]->getUserString() + "\n";
		}

		errStrings.push_back(s);
		s.clear();

	}
	res.clear();

	FilterErrorDialog *f= new FilterErrorDialog(this);
	f->SetText(errStrings);

	f->ShowModal();

	delete f;

}

void MainWindowFrame::OnTreeKeyDown(wxKeyEvent &event)
{
 	if(currentlyUpdatingScene)
 	{
 		return;
 	}
	const wxKeyEvent k = event; 
 	switch(k.GetKeyCode())
	{
		case WXK_BACK:
		case WXK_DELETE:
		{
			wxTreeItemId id;

			if(!treeFilters->GetCount())
				return;

			id=treeFilters->GetSelection();

			if(!id.IsOk() || id == treeFilters->GetRootItem())
				return;


			//TODO: Refactor out wxTreeItem... code, into separate routine
			// that only spits out viscontrol Ids
			//Rebuild the tree control, ensuring that the parent is visible,
			//if it has a parent (recall root node  of wx control is hidden)
			
			//Get the parent & its data
			wxTreeItemId parent = treeFilters->GetItemParent(id);
			wxTreeItemData *parentData=treeFilters->GetItemData(parent);


			//Tree data contains unique identifier for vis control to do matching
			wxTreeItemData *tData=treeFilters->GetItemData(id);
			//Remove the item from the Tree 
			visControl.state.treeState.removeFilterSubtree(((wxTreeUint *)tData)->value);
			//Clear property grid
			gridFilterPropGroup->Clear();
			if(parent !=treeFilters->GetRootItem())
			{
				ASSERT(parent.IsOk()); // should be - base node should always exist.

				//Ensure that the parent stays visible 
				visControl.setWxTreeFilterViewPersistence(
						((wxTreeUint*)parentData)->value);
				updateWxTreeCtrl(treeFilters);

				
				//OK, so those old Id s are no longer valid,
				//as we just rebuilt the tree. We need new ones
				//Parent is now selected
				parent=treeFilters->GetSelection();
				parentData=treeFilters->GetItemData(parent);


				//Update the filter property grid with the parent's data
				visControl.updateFilterPropGrid(gridFilterPropGroup,
							((wxTreeUint *)parentData)->value);
			}
			else
			{
				if(parent.IsOk())
					updateWxTreeCtrl(treeFilters);
			}
	
			//Force a scene update, independent of if autoUpdate is enabled. 
			doSceneUpdate();	
			break;
		}
		default:
			event.Skip();
	}
}


void MainWindowFrame::OnGridFilterPropertyChange(wxPropertyGridEvent &event)
{
	//Silence error mesages
	// we will handle validation in the backend
	event.SetValidationFailureBehavior(0);
	
	if(programmaticEvent || currentlyUpdatingScene || refreshThreadActive())
	{
		event.Veto();
		return;
	}

	programmaticEvent=true;
	//Should only be in the second col
	
	size_t filterId;
	if(!getTreeFilterId(treeFilters->GetSelection(),filterId))
	{
		programmaticEvent=false;
		return;
	}



	//Obtain the key/value pairing that we are about to set
	std::string newValue,keyStr;
	newValue=getPropValueFromEvent(event);
	
	size_t key;
	keyStr=event.GetProperty()->GetName();
	stream_cast(key,keyStr);

	//Try to apply the new value
	bool needUpdate;
	if(!visControl.state.treeState.setFilterProperty(filterId,
				key,newValue,needUpdate))
	{
		event.Veto();
		programmaticEvent=false;
		return;
	}


	if(needUpdate && checkAutoUpdate->GetValue())
		doSceneUpdate();
	else 
		clearWxTreeImages(treeFilters);

#ifdef FIX_WXPROPGRID_16222
	//See wx bug #16222 - cannot modify a property grid's contents
	// from a change event. Must work in a side-object then swap
	//--
	backFilterPropGrid= new wxPropertyGrid(filterPropertyPane,ID_GRID_FILTER_PROPERTY,
					wxDefaultPosition,wxDefaultSize,PROPERTY_GRID_STYLE);
	backFilterPropGrid->SetExtraStyle(PROPERTY_GRID_EXTRA_STYLE);


	visControl.updateFilterPropGrid(backFilterPropGrid,filterId,
			stlStr(gridFilterPropGroup->SaveEditableState()));


	int columnPos = gridFilterPropGroup->GetSplitterPosition();
	

	std::swap(backFilterPropGrid,gridFilterPropGroup);
	do_filtergrid_prop_layout();
	//Restore the original splitter position
	gridFilterPropGroup->SetSplitterPosition(columnPos);
	//--
#else
	visControl.updateFilterPropGrid(gridFilterPropGroup,filterId,
		stlStr(gridFilterPropGroup->SaveEditableState()));
#endif

	programmaticEvent=false;
	
}

void MainWindowFrame::OnGridFilterDClick(wxPropertyGridEvent &event)
{
	Refresh();
}

void MainWindowFrame::OnGridCameraPropertyChange(wxPropertyGridEvent &event)
{

	//Check for inited OK. Seem to be getting called before 
	//do_layout is complete.
	if(programmaticEvent || !initedOK)
	{
		event.Veto();
		return;
	}

	programmaticEvent=true;
	
	std::string eventType,newValue;
	eventType=event.GetValue().GetType();
	if(eventType == "long")
	{
		//Either integer property or enum
		//integer property
		wxLongLong ll;
		ll=event.GetValue().GetLong();
		
		const wxPGChoices &choices = event.GetProperty()->GetChoices();
		if(!choices.IsOk())
		{
			stream_cast(newValue,ll);
		}
		else
		{
			//So wx makes life hard here. We need to do a dance to get the selection
			// as a string
			unsigned int ul;
			ul=ll.ToLong();

			wxArrayString arrStr;
			arrStr=choices.GetLabels();
			newValue=arrStr[ul];
		}
	}
	else
	{
		//We don't need colour props in camera
		// not implemented
		ASSERT(eventType != "wxColour");
		newValue =  event.GetValue().GetString();
	}


	std::string keyStr;
	size_t key;
	keyStr=event.GetProperty()->GetName();
	stream_cast(key,keyStr);

	//Get the camera ID value 
	wxListUint *l;
	int n = comboCamera->FindString(comboCamera->GetValue());
	if(n == wxNOT_FOUND)
	{
		programmaticEvent=false;
		return;
	}
	l =(wxListUint*)  comboCamera->GetClientObject(n);

	ASSERT(l);

	size_t cameraId;
	cameraId = l->value;

	//Set property
	visControl.setCamProperty(cameraId,key,newValue);

#ifdef FIX_WXPROPGRID_16222
	//FIXME :Need to send the new grid, not the old, due to wx bug
	//See wx bug #16222 - cannot modify a property grid's contents
	// from a change event. Must work in a side-objectm then swap
	//--
	backCameraPropGrid= new wxPropertyGrid(noteCamera,ID_GRID_CAMERA_PROPERTY,
					wxDefaultPosition,wxDefaultSize,PROPERTY_GRID_STYLE);
	backCameraPropGrid->SetExtraStyle(PROPERTY_GRID_EXTRA_STYLE);
	
	visControl.updateCameraPropGrid(backCameraPropGrid,cameraId);
	int columnPos =gridCameraProperties->GetSplitterPosition();
	
	std::swap(backCameraPropGrid,gridCameraProperties);
	do_cameragrid_prop_layout();
	gridCameraProperties->SetSplitterPosition(columnPos);
#else
	visControl.updateCameraPropGrid(gridCameraProperties,cameraId);
#endif


#ifdef __WIN32
	//Move the splitter panel
	splitLeftRight->SetSashPosition(splitLeftRight->GetSashPosition()+1);
	splitLeftRight->SetSashPosition(splitLeftRight->GetSashPosition()-1);
#endif
	//Ensure that the GL panel shows latest cam orientation 
	panelTop->forceRedraw();
	programmaticEvent=false;
}


void MainWindowFrame::OnComboCameraText(wxCommandEvent &event)
{
	std::string s;
	s=stlStr(comboCamera->GetValue());
	if(!s.size())
		return;
	
	int n = comboCamera->FindString(comboCamera->GetValue());
	
	if ( n== wxNOT_FOUND ) 
		statusMessage(TRANS("Press enter to store new camera"),MESSAGE_HINT);
	else
		statusMessage(TRANS("Press enter to restore camera"),MESSAGE_HINT);
}

void MainWindowFrame::OnComboCameraEnter(wxCommandEvent &event)
{
	std::string camName;
	camName=stlStr(comboCamera->GetValue());

	//Disallow cameras with no name
	if (camName.empty())
		return;

	//Search for the camera's position in the combo box
	int n = comboCamera->FindString(comboCamera->GetValue());

	//If we have found the camera...
	if ( n!= wxNOT_FOUND ) 
	{
		//Select the combo box item
		comboCamera->Select(n);
		//Set this camera as thew new camera
		wxListUint *l;
		l =(wxListUint*)  comboCamera->GetClientObject(comboCamera->GetSelection());
		visControl.setActiveCam(l->value);
		
		std::string s = std::string(TRANS("Restored camera: ") ) +stlStr(comboCamera->GetValue());	
		
		statusMessage(s.c_str(),MESSAGE_INFO);
		
		//refresh the camera property grid
		visControl.updateCameraPropGrid(gridCameraProperties ,l->value);

		setSaveStatus();

		//force redraw in 3D pane
		panelTop->forceRedraw();
	}
	else
	{
		ASSERT(camName.size());
		//Create a new camera for the scene.
		visControl.state.addCam(camName,true);
		
		std::string s = std::string(TRANS("Stored camera: " )) +
						stlStr(comboCamera->GetValue());	
		statusMessage(s.c_str(),MESSAGE_INFO);

		visControl.updateCameraComboBox(comboCamera);
		visControl.updateCameraPropGrid(gridCameraProperties,
			visControl.state.getActiveCam());
		panelTop->forceRedraw();

		setSaveStatus();
	}
}

void MainWindowFrame::OnComboCamera(wxCommandEvent &event)
{
	//Set the active camera
	wxListUint *l;
	l =(wxListUint*)  comboCamera->GetClientObject(comboCamera->GetSelection());
	visControl.setActiveCam(l->value);


	visControl.updateCameraPropGrid(gridCameraProperties,l->value);

	std::string s = std::string(TRANS("Restored camera: ") ) +stlStr(comboCamera->GetValue());	
	statusMessage(s.c_str(),MESSAGE_INFO);
	
	panelTop->forceRedraw();
	
	setSaveStatus();
}

void MainWindowFrame::OnComboCameraSetFocus(wxFocusEvent &event)
{
	
	if(!haveSetComboCamText)
	{
		//Even if we have
		int pos;
		pos = comboCamera->FindString(comboCamera->GetValue());

		//clear the text if it is the introduction string, or something 
		// we don't have in the camera 
		if(pos == wxNOT_FOUND)
			comboCamera->SetValue(wxT(""));

		haveSetComboCamText=true;
		event.Skip();
		return;
	}
	
	event.Skip();
}

void MainWindowFrame::OnComboStashSetFocus(wxFocusEvent &event)
{
	if(!haveSetComboStashText)
	{
		comboStash->SetValue(wxT(""));
		haveSetComboStashText=true;
		event.Skip();
		return;
	}
	event.Skip();
}

void MainWindowFrame::OnComboFilterEnter(wxCommandEvent &event)
{
	if(currentlyUpdatingScene || refreshThreadActive())
	{
		ASSERT(false); //this should not happen
		return;
	}

	OnComboFilter(event);
}


void MainWindowFrame::OnComboFilter(wxCommandEvent &event)
{
	if(currentlyUpdatingScene)
		return;
	
	size_t filterId;
	if(!getTreeFilterId(treeFilters->GetSelection(),filterId))
	{
		if(treeFilters->GetCount())
			statusMessage(TRANS("Select an item from the filter tree before choosing a new filter"));
		else
			statusMessage(TRANS("Load data source (file->open) before choosing a new filter"));

		comboFilters->SetSelection(wxNOT_FOUND);
		comboFilters->ChangeValue(TRANS(ADD_FILTER_TEXT));
		return;
	}

	//Perform the appropriate action for the particular filter,
	//or use the default action for every other filter
	bool haveErr=false;


	//Convert the string into a filter ID based upon our mapping
	wxString s;
	s=comboFilters->GetString(event.GetSelection());
	size_t filterType;
	filterType=filterMap[stlStr(s)];


	ASSERT(stlStr(s) == TRANS(comboFilters_choices[filterType]));
	Filter *f;
	switch(comboFiltersTypeMapping[filterType])
	{
		case FILTER_TYPE_RANGEFILE:
		{
			///Prompt user for file
			wxFileDialog wxF(this,TRANS("Select RNG File..."),wxT(""),wxT(""), 
					TRANS(RANGEFILE_WX_CONSTANT),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
			

			if( (wxF.ShowModal() == wxID_CANCEL))
			{
				haveErr=true;
				break;
			}

			//Load rangefile &  construct filter
			f=configFile.getDefaultFilter(FILTER_TYPE_RANGEFILE);
			std::string dataFile = stlStr(wxF.GetPath());
			RangeFileFilter *r = (RangeFileFilter*)f;
			r->setRangeFilename(dataFile);

			
				
			if(!r->updateRng())
			{
				std::string errString;
				errString = TRANS("Failed reading range file.");
				errString += "\n";
				errString+=r->getRange().getErrString();
				
				wxErrMsg(this,TRANS("Error loading file"),errString);
				
				delete f;
				haveErr=true;
				break;
			}


			break;
		}
		default:
		{
		
			ASSERT(filterType < FILTER_TYPE_ENUM_END);
			//Generate the appropriate filter
			f=configFile.getDefaultFilter(comboFiltersTypeMapping[filterType]);

		}
	
	}

	if(haveErr)
	{
		//Clear the combo box
		comboFilters->SetSelection(wxNOT_FOUND);
		comboFilters->ChangeValue(TRANS(ADD_FILTER_TEXT));
		return;
	}

	//Add the filter to viscontrol
	visControl.state.treeState.addFilter(f,false,filterId);
	//Rebuild tree control
	updateWxTreeCtrl(treeFilters,f);


	if(checkAutoUpdate->GetValue())
		doSceneUpdate();

	comboFilters->SetSelection(wxNOT_FOUND);
	comboFilters->ChangeValue(TRANS(ADD_FILTER_TEXT));

	//update prop grid
#ifdef FIX_WXPROPGRID_16222
	ASSERT(!backFilterPropGrid);
#endif
	updateFilterPropertyGrid(gridFilterPropGroup,f);
	
}

bool MainWindowFrame::doSceneUpdate(bool ensureVisible)
{
	//Update scene
	ASSERT(!currentlyUpdatingScene);

	//Suspend the update timer, and start the progress timer
	updateTimer->Stop();
	currentlyUpdatingScene=true;
	haveAborted=false;

		
	statusMessage("",MESSAGE_NONE);
	noteDataView->SetPageText(NOTE_CONSOLE_PAGE_OFFSET,TRANS("Cons."));

	//Disable tree filters,refresh button and undo
	setLockUI(true);


	if(!requireFirstUpdate)
		textConsoleOut->Clear();	

	//Set focus on the main frame itself, so that we can catch escape key presses
	SetFocus();
	wxBusyCursor busyCursor;
	//reset the progress timer animation
	visControl.scene.resetProgressAnim();

	ensureResultVisible=ensureVisible;

	ASSERT(!refreshControl);
	refreshControl = new RefreshController(visControl.state.treeState);
	refreshThread=new RefreshThread(this,refreshControl);
	progressTimer->Start(PROGRESS_TIMER_DELAY);

	refreshThread->Create();
	refreshThread->Run();

	return true;
}

void MainWindowFrame::updateWxTreeCtrl( wxTreeCtrl *t, const Filter *f)
{
	programmaticEvent=true;

	//This routines causes, (during the call..) wx to process the tree
	// selection code. we have to block the selection processing
	// with the programmaticEvent var
	visControl.updateWxTreeCtrl(t,f);
	programmaticEvent=false;
}


void MainWindowFrame::finishSceneUpdate(unsigned int errCode)
{
	ASSERT(refreshThread);

	//If there was an error, then
	//display it	
	if(errCode)
	{
		const ProgressData &p=refreshControl->curProg;

		statusTimer->Start(STATUS_TIMER_DELAY);
		if(errCode)
		{
			std::string errString;
			//FIXME: This is a hack where we use the numerical value to encode the error's source. 
			//We should not do this, but instead replace the errCode with an error object that contains both code, object and some way to extract the string 
			if(errCode == FILTER_ERR_ABORT)
			{
				errString = TRANS("Refresh Aborted.");
				MainFrame_statusbar->SetStatusText("",1);
			}
			else if(errCode <FILTERTREE_REFRESH_ERR_BEGIN)
			{
				if(p.curFilter)
					errString = p.curFilter->getErrString(errCode);
			}
			else 
			{
				errString=FilterTree::getRefreshErrString(errCode);
			}
			
			statusMessage(errString.c_str(),MESSAGE_ERROR);	
		}

	
	}
	else
	{
		visControl.updateScene(refreshControl);
		updateProgressStatus();
	}

	
	currentlyUpdatingScene=false;

	//Restore the UI elements to their interactive state
	setLockUI(false);

	panelSpectra->Refresh(false);	

	updateEditRangeMenu();


	//Add (or hide) a little "Star" to inform the user there is some info available
	if(textConsoleOut->IsEmpty() || noteDataView->GetSelection()==NOTE_CONSOLE_PAGE_OFFSET)
		noteDataView->SetPageText(NOTE_CONSOLE_PAGE_OFFSET,TRANS("Cons."));
	else
	{
#if defined(__WIN32) || defined(__WIN64)
		noteDataView->SetPageText(NOTE_CONSOLE_PAGE_OFFSET,TRANS("*Cons."));
#else
		noteDataView->SetPageText(NOTE_CONSOLE_PAGE_OFFSET,TRANS("§Cons."));
#endif
	}


	setFilterTreeAnalysisImages();

	visControl.updateRawGrid();

	setSaveStatus();

	
	//Force a paint update for the scene
	panelTop->forceRedraw();

}

void MainWindowFrame::OnFinishRefreshThread(wxCommandEvent &event)
{
	ASSERT(refreshControl);
	//The tree itself should not be refreshing once the thread has completed.
	ASSERT(!visControl.state.treeState.isRefreshing());
	progressTimer->Stop();

	vector<std::pair<const Filter*, std::string> > consoleMessages;
	consoleMessages=refreshControl->getConsoleMessages();

	const Filter *lastFilter =0;	
	for(size_t ui=0; ui<consoleMessages.size();ui++)
	{
		if(lastFilter!=consoleMessages[ui].first)
		{
			lastFilter=consoleMessages[ui].first;
			textConsoleOut->AppendText("-------------\n");
			textConsoleOut->AppendText(consoleMessages[ui].first->getUserString() + "\n");
			textConsoleOut->AppendText("-------------\n");
		}
		
		textConsoleOut->AppendText(consoleMessages[ui].second + "\n");	
	}
	textConsoleOut->AppendText("\n");	


	finishSceneUpdate((unsigned int)event.GetInt());

	//First wait for the refresh thread to terminate
	refreshThread->Wait();
	delete refreshThread;
	refreshThread=0;

	delete refreshControl;
	refreshControl=0;

	if(!event.GetInt())
	{
		//Set the progress string to complete, if no error
		MainFrame_statusbar->SetStatusText("",0);
		MainFrame_statusbar->SetStatusText(TRANS("Complete"),1);
		MainFrame_statusbar->SetStatusText("",2);
	}


	if(ensureResultVisible)
	{
		//If we are using the default camera,
		//move it to make sure that it is visible
		if(visControl.state.getNumCams() == 1)
			visControl.scene.ensureVisible(CAMERA_DIR_YPLUS);

		ensureResultVisible=false;
	}


	//restart the update timer, to check for updates from the backend
	updateTimer->Start(UPDATE_TIMER_DELAY);
}

void MainWindowFrame::setFilterTreeAnalysisImages()
{
	vector<FILTERTREE_ERR> lastErrs;
	visControl.state.treeState.getAnalysisResults(lastErrs);

	//Show the error button if required
	btnFilterTreeErrs->Show(!lastErrs.empty());

	if(lastErrs.empty())
	{
		treeFilters->AssignImageList(NULL);
		return;
	}
	
	//Maps filters to their maximal severity level
	map<const Filter*,unsigned int> severityMapping;

	for(size_t ui=0;ui<lastErrs.size();ui++)
	{
		for(size_t uj=0;uj<lastErrs[ui].reportedFilters.size();uj++)
		{
			const Filter *filt;
			filt=lastErrs[ui].reportedFilters[uj];

			//Find the last entry
			map<const Filter*,unsigned int>::iterator it;
			it = severityMapping.find(filt);

		
			//If doesn't exist, put one in. If it does exist, keep only max. severity msg
			if(it == severityMapping.end())
				severityMapping[filt] = lastErrs[ui].severity;
			else
				it->second = std::max(lastErrs[ui].severity,severityMapping[filt]);
		}
	}
	
	//Map filters into icons
	map<size_t, wxArtID> iconSettings;
	{
		//Maps particular severity values into icons
		map<unsigned int, wxArtID> severityIconMapping;
		severityIconMapping[ANALYSE_SEVERITY_ERROR] = wxART_ERROR;
		severityIconMapping[ANALYSE_SEVERITY_WARNING] =wxART_WARNING;

		for(map<const Filter*,unsigned int>::const_iterator it=severityMapping.begin();it!=severityMapping.end(); ++it)
		{
			size_t id;
			id=visControl.state.treeState.getIdByFilter(it->first);
			iconSettings[id] = severityIconMapping[it->second];
		}
	}

	//apply the filter->icon mapping
	setWxTreeImages(treeFilters,iconSettings);

}

void MainWindowFrame::OnStatusBarTimer(wxTimerEvent &event)
{
	if(statusQueue.empty())
	{
		//clear the status bar colour, then wipe the status text from each field
		MainFrame_statusbar->SetBackgroundColour(wxNullColour);
		for(unsigned int ui=0; ui<3; ui++)
			MainFrame_statusbar->SetStatusText(wxT(""),ui);
		
		//Stop the status timer, as we are done 
		statusTimer->Stop();
	}	
	else
	{
		//update the status bar with the next
		// message
		std::string msg,tmpStr;
		if(statusQueue.size() > 1)
		{
			stream_cast(tmpStr,statusQueue.size());
			msg = tmpStr + string(" ") + TRANS("msgs");
			msg+=" : ";
		}
		msg+= statusQueue.front().second,
		showStatusMessage(msg.c_str(),
				statusQueue.front().first);
		statusQueue.pop_front();
	}	
}

void MainWindowFrame::OnProgressTimer(wxTimerEvent &event)
{
	updateProgressStatus();
}

void MainWindowFrame::OnAutosaveTimer(wxTimerEvent &event)
{

	//Save a state file to the configuration dir
	//with the title "autosave.xml"
	//

	wxString filePath = (configFile.getConfigDir());

	unsigned int pid;
	pid = wxGetProcessId();

	std::string pidStr;
	stream_cast(pidStr,pid);

	filePath+=wxFileName::GetPathSeparator()+ string(AUTOSAVE_PREFIX) + (pidStr) +
				string(AUTOSAVE_SUFFIX);
	//Save to the autosave file
	std::string s;
	s=  stlStr(filePath);

	//Only save if we have autosave data
	if(visControl.state.hasStateData())
	{
		std::map<string,string> dummyMap;
		if(visControl.state.save(s.c_str(),dummyMap,false))
			statusMessage(TRANS("Autosave complete."),MESSAGE_INFO);
		else
		{
			//The save failed, but may have left an incomplete file lying around
			if(wxFileExists(filePath))
				wxRemoveFile(filePath);
		}
	}
}

void MainWindowFrame::OnUpdateTimer(wxTimerEvent &event)
{
	programmaticEvent=true;

	//TODO: HACK AROUND: force tree filter to relayout under wxGTK and Mac
	#ifndef __WXMSW__
	//Note: Calling this under windows causes the dropdown box that hovers over the top of this to
	//be closed, rendering the dropdown useless. That took ages to work out.
	treeFilters->GetParent()->Layout();
	#endif

	if(requireFirstUpdate && !refreshThreadActive())
	{
		doSceneUpdate();

		requireFirstUpdate=false;
	}


	//see if we need to update the post effects due to user interaction
	//with the crop panels
	if(panelFxCropOne->hasUpdate() || panelFxCropTwo->hasUpdate())
	{
		updatePostEffects();
		panelFxCropOne->clearUpdate();
		panelFxCropOne->clearUpdate();
	}

	//Check viscontrol to see if it needs an update, such as
	//when the user interacts with an object when it is not 
	//in the process of refreshing.
	//Don't attempt to update if already updating, or last
	//update aborted
	bool visUpdates=visControl.state.treeState.hasUpdates();
	bool monitorUpdates=visControl.state.treeState.hasMonitorUpdates();
	//I can has updates?
	if((visUpdates || plotUpdates|| monitorUpdates ) && !refreshThreadActive())
	{
		if(visUpdates)
			visControl.state.treeState.applyBindingsToTree();	

		if(plotUpdates)
		{
			//FIXME: Hack. Rather than simply clearing the
			//cache globally, consider actually working out
			//which filter had the update, and refreshing that
			//filter only. Here we assume that only Rangefiles
			//can trigger an update
			visControl.state.treeState.clearCacheByType(FILTER_TYPE_RANGEFILE);
		}

		doSceneUpdate();
	}
	plotUpdates=false;

	//Check the openGL pane to see if the camera property grid needs refreshing
	if(panelTop->hasCameraUpdates())
	{
		//Use the current combobox value to determine which camera is the 
		//current camera in the property grid
		visControl.transferSceneCameraToState();
			
		int n = comboCamera->FindString(comboCamera->GetValue());

		if(n != wxNOT_FOUND)
		{
			wxListUint *l;
			l =(wxListUint*)  comboCamera->GetClientObject(n);

			visControl.updateCameraPropGrid(gridCameraProperties,l->value);
		}

		panelTop->clearCameraUpdates();

		setSaveStatus();
	}

		

	if(visUpdates)
	{
		size_t filterId;
	
		if(!getTreeFilterId(treeFilters->GetSelection(),filterId))
		{
			programmaticEvent=false;
			return;
		}

		visControl.updateFilterPropGrid(gridFilterPropGroup,filterId);

	}


	programmaticEvent=false;	
}

void MainWindowFrame::statusMessage(const char *message, unsigned int type)
{

	if(type == MESSAGE_NONE)
	{
		statusTimer->Stop();
		statusQueue.clear();
		
		//clear the status bar colour, then wipe the status text from each field
		MainFrame_statusbar->SetBackgroundColour(wxNullColour);
		for(unsigned int ui=0; ui<3; ui++)
			MainFrame_statusbar->SetStatusText(wxT(""),ui);
		
	}
	else
	{
		if(statusTimer->IsRunning())
		{
			//go through and strip
			// other hints
			for(list<pair<unsigned int,string> >::iterator it=statusQueue.begin();
				it!=statusQueue.end(); )
			{
				if(it->first != MESSAGE_HINT)
				{
					++it;	
					continue;
				}
				
				it=statusQueue.erase(it);
			}
				
			//Emplace our message
			statusQueue.push_back(make_pair(type,message));	

			//keep only unique messages 
			list<pair<unsigned int,string> >::iterator tmpIt;
			tmpIt= std::unique(statusQueue.begin(),statusQueue.end());
			statusQueue.erase(tmpIt,statusQueue.end());	
			
			if(!statusQueue.empty())
			showStatusMessage(statusQueue.begin()->second.c_str(),statusQueue.begin()->first);	

			
		}
		else
		{
			showStatusMessage(message,type);	
			statusTimer->Start(STATUS_TIMER_DELAY);
		}
	}
}

void MainWindowFrame::showStatusMessage(const char *message, unsigned int type)
{
	//Wx does not support statusbar colouring under MSW
	// using this can result in visual oddness
	#if !(defined(__WIN32) || defined(__WIN64))
	switch(type)
	{
		case MESSAGE_ERROR:
			MainFrame_statusbar->SetBackgroundColour(*wxGREEN);
			break;
		case MESSAGE_INFO:
			MainFrame_statusbar->SetBackgroundColour(*wxCYAN);
			break;
		case MESSAGE_HINT:
			MainFrame_statusbar->SetBackgroundColour(wxNullColour);
			break;
		default:
			ASSERT(false);
	}
#endif

	MainFrame_statusbar->SetStatusText((message),0);
}

void MainWindowFrame::updateProgressStatus()
{
	//we can get some "left over" events that are queued but not processed
	// from the main thread
	if(!refreshThreadActive())
		return;

	std::string progressString,filterProg;

	//If we have no tree, don't update the progress
	if(!visControl.state.treeState.size())
		return;


	//Request a panel refresh, so we update the opengl spinner
	panelTop->Refresh();

	//The refresh should still be present if we are using this function
	if(haveAborted)
	{
		progressString=TRANS("Aborting....");
		progressTimer->Stop(); //Supress any future events
		visControl.scene.progressCircle.setMaxStep(0);
	}
	else
	{
		//Check for new progress data
		const ProgressData &p=refreshControl->curProg;
		ASSERT(p.filterProgress <=100 || p.filterProgress==(unsigned int) -1);

		if(p == lastProgressData

				|| !p.maxStep)
			return;

		//This shouldn't happen, but prevent >100% progress from being reported
		unsigned int cappedProgress;
		if (p.filterProgress!=(unsigned int)-1)
			cappedProgress = std::min(p.filterProgress,(unsigned int)100);
		else
			cappedProgress = 0;
			
		//Inform progress circle in scene about current progress
		visControl.scene.progressCircle.setCurFilter(p.totalProgress);
		visControl.scene.progressCircle.setMaxStep(p.maxStep);
		visControl.scene.progressCircle.setNumFilters(p.totalNumFilters);
		visControl.scene.progressCircle.setProgress(cappedProgress);
		visControl.scene.progressCircle.setStep(p.step);


		lastProgressData=p;

		//Update the text progress
		{
			ASSERT(p.totalProgress <= visControl.state.treeState.size());

			//Create a string from the total and percentile progresses
			std::string totalProg,totalCount,step,maxStep;
			stream_cast(filterProg,cappedProgress);
			stream_cast(totalProg,p.totalProgress);
			stream_cast(totalCount,p.totalNumFilters);


			stream_cast(step,p.step);
			stream_cast(maxStep,p.maxStep);

			ASSERT(p.step <=p.maxStep);

			if(p.curFilter)
			{
				if(!p.maxStep)
					progressString = totalProg+TRANS(" of ") + totalCount +
						" (" + p.curFilter->typeString() +")";
				else
				{
					progressString = totalProg+TRANS(" of ") + totalCount +
						" (" + p.curFilter->typeString() + ", "
						+ step + "/" + maxStep + ": " + 
						p.stepName+")";
				}
			}
			else
			{
				//If we have no filter, then we must be done if the totalProgress is
				//equal to the total count.
				if(totalProg == totalCount)
					progressString = TRANS("Updated.");
				else
					progressString = totalProg + TRANS(" of ") + totalCount;
			}


			//Show the abort notice if we have hit 100%
			if( p.filterProgress == (unsigned int) -1)
			{
				filterProg=TRANS("Calculating...");
			}
			else if(p.filterProgress != 100 && p.filterProgress < p.totalNumFilters)
				filterProg+=TRANS("\% Done (Esc aborts)");
			else
				filterProg+=TRANS("\% Done");
		}

	}

	MainFrame_statusbar->SetBackgroundColour(wxNullColour);
	MainFrame_statusbar->SetStatusText(wxT(""),0);
	MainFrame_statusbar->SetStatusText((progressString),1);
	MainFrame_statusbar->SetStatusText((filterProg),2);

}

void MainWindowFrame::updatePostEffects()
{
	visControl.scene.clearEffects();

	//Do we need post-processing?
#ifndef APPLE_EFFECTS_WORKAROUND
	if(!checkPostProcessing->IsChecked())
		return;
#endif
	if( checkFxCrop->IsChecked())
	{

		wxString ws;
		string s;
		ws=comboFxCropAxisOne->GetValue();
		s =stlStr(ws);

		//String encodes permutation (eg "x-y").
		unsigned int axisPerm[4];
		axisPerm[0] =(unsigned int)(s[0] -'x')*2;
		axisPerm[1] = (unsigned int)(s[0] -'x')*2+1;
		axisPerm[2] =(unsigned int)(s[2] -'x')*2;
		axisPerm[3] = (unsigned int)(s[2] -'x')*2+1;
		
		//Get the crop data, and generate an effect
		BoxCropEffect *b = new BoxCropEffect;

		//Assume, that unless otherwise specified
		//the default crop value is zero
		float array[6];
		float tmpArray[4];
		for(unsigned int ui=0;ui<6;ui++)
			array[ui]=0;
		
		//Permute the indices for the crop fractions, then assign
		panelFxCropOne->getCropValues(tmpArray);
		for(unsigned int ui=0;ui<4;ui++)
			array[axisPerm[ui]] = tmpArray[ui];
		
		
		ws=comboFxCropAxisTwo->GetValue();
		s =stlStr(ws);
		
		axisPerm[0] =(unsigned int)(s[0] -'x')*2;
		axisPerm[1] = (unsigned int)(s[0] -'x')*2+1;
		axisPerm[2] =(unsigned int)(s[2] -'x')*2;
		axisPerm[3] = (unsigned int)(s[2] -'x')*2+1;
		panelFxCropTwo->getCropValues(tmpArray);
		
		for(unsigned int ui=0;ui<4;ui++)
			array[axisPerm[ui]] = tmpArray[ui];

		b->setFractions(array);

		//Should we be using the camera frame?
		b->useCamCoords(checkFxCropCameraFrame->IsChecked());

		//Send the effect to the scene
		if(b->willDoSomething())
		{
			visControl.scene.addEffect(b);
			visControl.scene.setEffects(true);


			//Update the dx,dy and dz boxes
			BoundCube bcTmp;
			bcTmp=visControl.scene.getBound();

			b->getCroppedBounds(bcTmp);	

			if(!checkFxCropCameraFrame->IsChecked())
			{
				float delta;
				delta=bcTmp.getBound(0,1)-bcTmp.getBound(0,0);
				stream_cast(s,delta);
				textFxCropDx->SetValue((s));
				
				delta=bcTmp.getBound(1,1)-bcTmp.getBound(1,0);
				stream_cast(s,delta);
				textFxCropDy->SetValue((s));
				
				delta=bcTmp.getBound(2,1)-bcTmp.getBound(2,0);
				stream_cast(s,delta);
				textFxCropDz->SetValue((s));
			}
			else
			{
				textFxCropDx->SetValue(wxT(""));
				textFxCropDy->SetValue(wxT(""));
				textFxCropDz->SetValue(wxT(""));
			}
			
			//well, we dealt with this update.
			panelFxCropOne->clearUpdate();
			panelFxCropTwo->clearUpdate();
		}
		else
		{
			textFxCropDx->SetValue(wxT(""));
			textFxCropDy->SetValue(wxT(""));
			textFxCropDz->SetValue(wxT(""));
			delete b;

			//we should let this return true, 
			//so that an update takes hold
		}

	}
	

	if(checkFxEnableStereo->IsChecked())
	{
		AnaglyphEffect *anaglyph = new AnaglyphEffect;

		unsigned int sel;
		sel=comboFxStereoMode->GetSelection();
		anaglyph->setMode(sel);
		int v=sliderFxStereoBaseline->GetValue();

		float shift=((float)v)*BASELINE_SHIFT_FACTOR;

		anaglyph->setBaseShift(shift);
		anaglyph->setFlip(checkFxStereoLensFlip->IsChecked());
		visControl.scene.addEffect(anaglyph);
	}

	panelTop->forceRedraw();
}

void MainWindowFrame::updateFxUI(const vector<const Effect*> &effs)
{
	//Here we pull information out from the effects and then
	//update the  ui controls accordingly

	Freeze();

	for(unsigned int ui=0;ui<effs.size();ui++)
	{
		switch(effs[ui]->getType())
		{
			case EFFECT_BOX_CROP:
			{
				const BoxCropEffect *e=(const BoxCropEffect*)effs[ui];

				//Enable the checkbox
				checkFxCrop->SetValue(true);
				//set the combos back to x-y y-z
				comboFxCropAxisOne->SetSelection(0);
				comboFxCropAxisTwo->SetSelection(1);
			
				//Temporarily de-link the panels
				panelFxCropOne->link(0,CROP_LINK_NONE);
				panelFxCropTwo->link(0,CROP_LINK_NONE);

				//Set the crop values 
				for(unsigned int ui=0;ui<6;ui++)
				{
					if(ui<4)
						panelFxCropOne->setCropValue(ui,
							e->getCropValue(ui));
					else if(ui > 2)
						panelFxCropTwo->setCropValue(ui-2,
							e->getCropValue(ui));
				}

				//Ensure that the values that went in were valid
				panelFxCropOne->makeCropValuesValid();
				panelFxCropTwo->makeCropValuesValid();


				//Restore the panel linkage
				panelFxCropOne->link(panelFxCropTwo,CROP_LINK_BOTH); 
				panelFxCropTwo->link(panelFxCropOne,CROP_LINK_BOTH); 


				break;
			}
			case EFFECT_ANAGLYPH:
			{
				const AnaglyphEffect *e=(const AnaglyphEffect*)effs[ui];
				//Set the slider from the base-shift value
				float shift;
				shift=e->getBaseShift();
				sliderFxStereoBaseline->SetValue(
					(unsigned int)(shift/BASELINE_SHIFT_FACTOR));


				//Set the stereo drop down colour
				unsigned int mode;
				mode = e->getMode();
				ASSERT(mode < comboFxStereoMode->GetCount());

				comboFxStereoMode->SetSelection(mode);
				//Enable the stereo mode
				checkFxEnableStereo->SetValue(true);
				break;
			}
			default:
				ASSERT(false);
		}
	
	}

	//Re-enable the effects UI as needed
	if(!effs.empty())
	{
#ifndef APPLE_EFFECTS_WORKAROUND
		checkPostProcessing->SetValue(true);
		noteFxPanelCrop->Enable();
		noteFxPanelStereo->Enable();
#endif
	
		visControl.scene.setEffects(true);
	}
	

	Thaw();
}

//This routine is used by other UI processes to trigger an abort
void MainWindowFrame::OnProgressAbort(wxCommandEvent &event)
{
	if(!haveAborted)
		visControl.state.treeState.setAbort();
	haveAborted=true;
}

void MainWindowFrame::OnViewFullscreen(wxCommandEvent &event)
{
	if(programmaticEvent)
		return;

	programmaticEvent=true;

	ShowFullScreen(!fullscreenState);
	fullscreenState=(fullscreenState+1)%2;
	
	programmaticEvent=false;
}

void MainWindowFrame::OnButtonRefresh(wxCommandEvent &event)
{
	//TODO: Remove this line when wx bug 16222 is fixed
	if(!gridCameraProperties || !gridFilterPropGroup)
		return;

	//Run abort code as needed
	if(currentlyUpdatingScene || refreshThreadActive())
	{
		OnProgressAbort(event);
		return;
	}

	//dirty hack to get keyboard state.
	wxMouseState wxm = wxGetMouseState();
	if(wxm.ShiftDown())
	{
		visControl.state.treeState.purgeFilterCache();
	}
	else
	{
		if(checkCaching->IsChecked())
			statusMessage(TRANS("Tip: You can shift-click to force full refresh, if required"),MESSAGE_HINT);
	}
	doSceneUpdate();	
}

void MainWindowFrame::OnRawDataUnsplit(wxSplitterEvent &event)
{
	checkMenuRawDataPane->Check(false);
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA,false);
}

void MainWindowFrame::OnFilterPropDoubleClick(wxSplitterEvent &event)
{
	//Disallow unsplitting of filter property panel
	event.Veto();
}

void MainWindowFrame::OnControlSplitMove(wxSplitterEvent &event)
{
	//For some reason, the damage rectangle is not updated
	// for the tree ctrl
	treeFilters->Refresh();
}

void MainWindowFrame::OnTopBottomSplitMove(wxSplitterEvent &event)
{
	Refresh();
	panelTop->forceRedraw();
}

void MainWindowFrame::OnFilterSplitMove(wxSplitterEvent &event)
{
	//For some reason, the damage rectangle is not updated
	// for the tree ctrl
	treeFilters->Refresh();
}

void MainWindowFrame::OnControlUnsplit(wxSplitterEvent &event)
{
	//Make sure that the LHS panel is removed, rather than the default (right)
	splitLeftRight->Unsplit(panelLeft);  

	checkMenuControlPane->Check(false);
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_CONTROL,false);
}

void MainWindowFrame::OnSpectraUnsplit(wxSplitterEvent &event)
{
	checkMenuSpectraList->Check(false);
	configFile.setPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST,false);
}


void MainWindowFrame::OnButtonGridCopy(wxCommandEvent &event)
{
	gridRawData->copyData();	
}

void MainWindowFrame::OnButtonGridSave(wxCommandEvent &event)
{
	if(!gridRawData->GetNumberRows()||!gridRawData->GetNumberCols())
	{
		statusMessage(TRANS("No data to save"),MESSAGE_ERROR);
		return;
	}
	gridRawData->saveData();	
}

void MainWindowFrame::OnCheckAlpha(wxCommandEvent &event)
{
	visControl.scene.setAlpha(event.IsChecked());

	panelTop->forceRedraw();
}

void MainWindowFrame::OnCheckLighting(wxCommandEvent &event)
{
	visControl.scene.setLighting(event.IsChecked());
	
	panelTop->forceRedraw();
}

void MainWindowFrame::OnCheckCacheEnable(wxCommandEvent &event)
{
	if(event.IsChecked())
	{
		visControl.state.treeState.setCachePercent((unsigned int)spinCachePercent->GetValue());
	}
	else
	{
		visControl.state.treeState.setCachePercent(0);
		visControl.state.treeState.purgeFilterCache();

		doSceneUpdate();
	}
}

void MainWindowFrame::OnCheckWeakRandom(wxCommandEvent &event)
{
	Filter::setStrongRandom(!event.IsChecked());

	doSceneUpdate();
}

void MainWindowFrame::OnCheckLimitOutput(wxCommandEvent &event)
{
	size_t limitVal;
	if(event.IsChecked())
	{
		bool isOK=validateTextAsStream(textLimitOutput,limitVal);

		if(!isOK)
			return;
	}
	else
		limitVal=0;

	visControl.setIonDisplayLimit(limitVal);
	doSceneUpdate();
	
	configFile.setMaxPoints(limitVal);
}

void MainWindowFrame::OnTextLimitOutput(wxCommandEvent &event)
{
	//Under GTK wx3.0, this fires during object construction
	if(!initedOK)
		return;
	size_t limitVal;
	bool isOK=validateTextAsStream(textLimitOutput,limitVal);

	if(!isOK)
		return;

	if(checkLimitOutput->IsChecked())
	{
		visControl.setIonDisplayLimit(limitVal);
		configFile.setMaxPoints(limitVal);
	}
}

void MainWindowFrame::OnTextLimitOutputEnter(wxCommandEvent &event)
{
	size_t limitVal;
	bool isOK=validateTextAsStream(textLimitOutput,limitVal);

	if(!isOK)
		return;

	if(checkLimitOutput->IsChecked())
	{
		visControl.setIonDisplayLimit(limitVal);
		doSceneUpdate();
	}

	//If we set the limit to zero this is a special case
	// that disables the limit, so untick the checkbox to make it clear to the
	// user that we are not using this any more
	if(limitVal == 0)
		checkLimitOutput->SetValue(false);
}

void MainWindowFrame::OnCacheRamUsageSpin(wxSpinEvent &event)
{
	ASSERT(event.GetPosition() >= 0 &&event.GetPosition()<=100);

	visControl.state.treeState.setCachePercent(event.GetPosition());
	
}
void MainWindowFrame::OnButtonRemoveCam(wxCommandEvent &event)
{

	std::string camName;


	camName=stlStr(comboCamera->GetValue());

	if (!camName.size())
		return;

	int n = comboCamera->FindString(comboCamera->GetValue());

	if ( n!= wxNOT_FOUND ) 
	{
		wxListUint *l;
		l =(wxListUint*)  comboCamera->GetClientObject(n);

		visControl.state.removeCam(l->value);
		comboCamera->Delete(n);
		
		programmaticEvent=true;
		comboCamera->SetValue(wxT(""));
		gridCameraProperties->Clear();
		programmaticEvent=false;

		setSaveStatus();

		// There is one camera that we cannot access
		// TODO: This logic should not be here, but in the widget update
		if(visControl.state.getNumCams() > 1)
		{
			visControl.updateCameraComboBox(comboCamera);
			visControl.updateCameraPropGrid(gridCameraProperties,visControl.state.getActiveCam());
		}
		else
			gridCameraProperties->Clear();
	}
	
}

void MainWindowFrame::OnSpectraListbox(wxCommandEvent &event)
{
	//This function gets called programatically by 
	//doSceneUpdate. Prevent interaction.
	if(refreshThreadActive())
		return;


	//Get the currently selected item
	//Spin through the selected items
	for(unsigned int ui=0;ui<plotList->GetCount(); ui++)
	{
		unsigned int plotID;

		//Retrieve the uniqueID
		plotID = visControl.getPlotID(ui);

		panelSpectra->setPlotVisible(plotID,plotList->IsSelected(ui));

	}
	
	panelSpectra->Refresh();
	//The raw grid contents may change due to the list selection
	//change. Update the grid
	visControl.updateRawGrid();
}

void MainWindowFrame::OnClose(wxCloseEvent &event)
{
	if(refreshThreadActive())
	{
		if(!haveAborted)
		{
			refreshThread->abort();
			haveAborted=true;

			statusMessage(TRANS("Aborting..."),MESSAGE_INFO);
			return;
		}
		else
		{
			wxMessageDialog wxD(this,
					TRANS("Waiting for refresh to abort. Exiting could lead to the program backgrounding. Exit anyway? "),
					TRANS("Confirmation request"),wxOK|wxCANCEL|wxICON_ERROR);

			if(wxD.ShowModal() != wxID_OK)
			{
				event.Veto();
				return;
			}
		}
	}
	else
	{
		//If the program is being forced by the OS to shut down, don't ask the user for abort,
		// as we can't abort it anyway.
		if(event.CanVeto())
		{
			if(visControl.stateIsModified()) 
			{
				//Prompt for close
				wxMessageDialog wxD(this,
						TRANS("Are you sure you wish to exit 3Depict?"),\
						TRANS("Confirmation request"),wxOK|wxCANCEL|wxICON_ERROR);
				if(wxD.ShowModal() != wxID_OK)
				{
					event.Veto();
					return;
				}
			
			}
		}
	}
	
	//Remove the autosave file if it exists, as we are shutting down neatly.

	//Get self PID
	std::string pidStr;
	unsigned int pid;
	pid=wxGetProcessId();
	stream_cast(pidStr,pid);
	
	wxString filePath =(configFile.getConfigDir());
	filePath+=string("/") + string(AUTOSAVE_PREFIX) + pidStr+ string(AUTOSAVE_SUFFIX);

	if(wxFileExists(filePath))
		wxRemoveFile(filePath);

	//Remember current window size for next time
	wxSize winSize;
	winSize=GetSize();
	configFile.setInitialAppSize(winSize.GetWidth(),winSize.GetHeight());

	//Remember the sash positions for next time, as fractional values fo
	// the window size, but only if split (as otherwise frac could exceed 1)
	float frac;
	if(splitLeftRight->IsSplit())
	{
		frac =(float) splitLeftRight->GetSashPosition()/winSize.GetWidth();
		configFile.setLeftRightSashPos(frac);
	}
	if(splitTopBottom->IsSplit())
	{
		frac = (float) splitTopBottom->GetSashPosition()/winSize.GetHeight();
		configFile.setTopBottomSashPos(frac);
	}
	if(filterSplitter->IsSplit())
	{
		frac= (float)filterSplitter->GetSashPosition()/winSize.GetHeight();
		configFile.setFilterSashPos(frac);
	}
	if(splitterSpectra->IsSplit())
	{
		frac = (float)splitterSpectra->GetSashPosition()/winSize.GetWidth();
		configFile.setPlotListSashPos(frac);
	}

	winSize=noteDataView->GetSize();

	//Try to save the configuration
	configFile.write();

	 if(verCheckThread)
	 {
		 if(!verCheckThread->isComplete())
		 {
			 //Kill it.
			 verCheckThread->Kill();
		 }
		 else
			 verCheckThread->Wait();
	}
	 

	 //Terminate the program
 	 Destroy();
}

void MainWindowFrame::realignCameraButton(unsigned int direction)
{
	if(checkAlignCamResize->IsChecked())
		visControl.scene.ensureVisible(direction);
	else
	{
		//move the camera from its current position to the target direction
		Camera *cam=visControl.scene.getActiveCam();
		if(cam->type() == CAM_LOOKAT)
		{
			BoundCube bc = visControl.scene.getBound();
			CameraLookAt *cLook=(CameraLookAt*)cam;
			cLook->setTarget(bc.getCentroid());
			cLook->repositionAroundTarget(direction);

			//set the "up" direction that we use by default
			Point3D p;
			switch(direction)
			{
				case CAMERA_DIR_XPLUS:
					p=Point3D(0,0,1);
					break;
				case CAMERA_DIR_YPLUS:
					p=Point3D(0,0,1);
					break;
				case CAMERA_DIR_ZPLUS:
					p=Point3D(0,1,0);
					break;
				case CAMERA_DIR_XMINUS:
					p=Point3D(0,0,-1);
					break;
				case CAMERA_DIR_YMINUS:
					p=Point3D(0,0,-1);
					break;
				case CAMERA_DIR_ZMINUS:
					p=Point3D(0,-1,0);
					break;
			}
			cLook->setUpDirection(p);
		}
	}	

	panelTop->forceRedraw();
}

void MainWindowFrame::OnButtonAlignCameraXPlus(wxCommandEvent &event)
{
	realignCameraButton(CAMERA_DIR_XPLUS);
}	

void MainWindowFrame::OnButtonAlignCameraYPlus(wxCommandEvent &event)
{
	realignCameraButton(CAMERA_DIR_YPLUS);
}

void MainWindowFrame::OnButtonAlignCameraZPlus(wxCommandEvent &event)
{
	realignCameraButton(CAMERA_DIR_ZPLUS);
}

void MainWindowFrame::OnButtonAlignCameraXMinus(wxCommandEvent &event)
{
	realignCameraButton(CAMERA_DIR_XMINUS);
}

void MainWindowFrame::OnButtonAlignCameraYMinus(wxCommandEvent &event)
{
	realignCameraButton(CAMERA_DIR_YMINUS);
}

void MainWindowFrame::OnButtonAlignCameraZMinus(wxCommandEvent &event)
{
	realignCameraButton(CAMERA_DIR_ZMINUS);
}


void MainWindowFrame::OnCheckPostProcess(wxCommandEvent &event)
{

#ifdef APPLE_EFFECTS_WORKAROUND
	//FIXME: I have disabled this under apple
	ASSERT(false);
#endif
	//Disable the entire UI panel
	noteFxPanelCrop->Enable(event.IsChecked());
	noteFxPanelStereo->Enable(event.IsChecked());
	visControl.scene.setEffects(event.IsChecked());
	updatePostEffects();
		
	setSaveStatus();
	
	panelTop->forceRedraw();
}


void MainWindowFrame::OnFxCropCheck(wxCommandEvent &event)
{
	//Disable/enable the other UI controls on the crop effects page
	//Include the text labels to give them that "greyed-out" look
	checkFxCropCameraFrame->Enable(event.IsChecked());
	comboFxCropAxisOne->Enable(event.IsChecked());
	panelFxCropOne->Enable(event.IsChecked());
	comboFxCropAxisTwo->Enable(event.IsChecked());
	panelFxCropTwo->Enable(event.IsChecked());
	textFxCropDx->Enable(event.IsChecked());
	textFxCropDy->Enable(event.IsChecked());
	textFxCropDz->Enable(event.IsChecked());
	labelFxCropDx->Enable(event.IsChecked());
	labelFxCropDy->Enable(event.IsChecked());
	labelFxCropDz->Enable(event.IsChecked());

	setSaveStatus();

	updatePostEffects();
}


void MainWindowFrame::OnFxCropCamFrameCheck(wxCommandEvent &event)
{
	updatePostEffects();
}



void MainWindowFrame::OnFxCropAxisOne(wxCommandEvent &event)
{
	linkCropWidgets();
	updatePostEffects();
}

void MainWindowFrame::OnFxCropAxisTwo(wxCommandEvent &event)
{
	linkCropWidgets();
	updatePostEffects();
}

void MainWindowFrame::linkCropWidgets()
{
	//Adjust the link mode for the 
	//two crop panels as needed
	
	unsigned int linkMode=0;

	string first[2],second[2];

	wxString s;
	string tmp;

	//TODO: Don't parse output, but actually 
	// wire in axis selection
	s=comboFxCropAxisOne->GetValue();
	tmp=stlStr(s);
	first[0]=tmp[0];
	second[0]=tmp[2];

	s=comboFxCropAxisTwo->GetValue();
	tmp=stlStr(s);
	first[1]=tmp[0];
	second[1]=tmp[2];

	if(first[0] == first[1] && second[0] == second[1])
	{
		//First and second axis match? then link both axes
		linkMode=CROP_LINK_BOTH;
	}
	else if(first[0] == second[1] && second[0] == first[1])
		linkMode=CROP_LINK_BOTH_FLIP; //Fipped axis linkage
	else if(first[0] == first[1])
		linkMode=CROP_LINK_LR; //left right libnkage
	else if(second[0] == second[1])
		linkMode=CROP_LINK_TB; //top pottom linkage
	else if(second[0] == first[1])
	{
		//tb-lr flip
		panelFxCropOne->link(panelFxCropTwo,CROP_LINK_TB_FLIP);
		panelFxCropTwo->link(panelFxCropOne,CROP_LINK_LR_FLIP);
	}
	else if(second[1]== first[0])
	{
		//lr-tb flip
		panelFxCropOne->link(panelFxCropTwo,CROP_LINK_LR_FLIP);
		panelFxCropTwo->link(panelFxCropOne,CROP_LINK_TB_FLIP);
	}
	else
	{
		//Pigeonhole principle says we can't get here.
		ASSERT(false);
	}
		

	if(linkMode)
	{
		panelFxCropOne->link(panelFxCropTwo,linkMode);
		panelFxCropTwo->link(panelFxCropOne,linkMode);
	}

}




void MainWindowFrame::OnFxStereoEnable(wxCommandEvent &event)
{
	comboFxStereoMode->Enable(event.IsChecked());
	sliderFxStereoBaseline->Enable(event.IsChecked());
	checkFxStereoLensFlip->Enable(event.IsChecked());

	updatePostEffects();
}

void MainWindowFrame::OnFxStereoLensFlip(wxCommandEvent &event)
{
	updatePostEffects();
}


void MainWindowFrame::OnFxStereoCombo(wxCommandEvent &event)
{
	updatePostEffects();
}


void MainWindowFrame::OnFxStereoBaseline(wxScrollEvent &event)
{
	updatePostEffects();
}

void MainWindowFrame::restoreConfigDefaults()
{
	std::vector<std::string> strVec;

	//Set the files that are listed in the recent files
	//menu
	configFile.getRecentFiles(strVec);

	for(unsigned int ui=0; ui<strVec.size(); ui++)
		recentHistory->AddFileToHistory((strVec[ui]));
	
	//Set the mouse zoom speeds
	float zoomRate,moveRate;
	zoomRate=configFile.getMouseZoomRate();
	moveRate=configFile.getMouseMoveRate();

	panelTop->setMouseZoomFactor((float)zoomRate/100.0f);
	panelTop->setMouseMoveFactor((float)moveRate/100.0f);


	//If the config file has a max points value stored, use it,
	// but don't force a refresh, as we will do that later
	if(configFile.getHaveMaxPoints())
	{
		std::string s;
		stream_cast(s,configFile.getMaxPoints());

		textLimitOutput->SetValue((s));
		visControl.setIonDisplayLimit(configFile.getMaxPoints());
	}

	
	if(configFile.getWantStartupOrthoCam())
	{
		visControl.state.setCamProperty(visControl.state.getActiveCam(), 
					CAMERA_KEY_LOOKAT_PROJECTIONMODE,TRANS("Orthogonal"));
		visControl.setActiveCam(visControl.state.getActiveCam());	
	}
}

void MainWindowFrame::checkShowTips()
{
	//Show startup tip dialog as needed
	if(configFile.wantStartupTips())
	{
		std::string tipFile;
		tipFile=locateDataFile("startup-tips.txt");
		if(!tipFile.empty())
		{
			const unsigned int ROUGH_NUMBER_TIPS=22;
			wxTipProvider *tipProvider = wxCreateFileTipProvider((tipFile), 
					(size_t) ((float)rand()/(float)RAND_MAX*(float)ROUGH_NUMBER_TIPS));

			if(tipProvider)
			{
				bool wantTipsAgain=wxShowTip(this, tipProvider);
				delete tipProvider;
				configFile.setWantStartupTips(wantTipsAgain);
			}


		}
		else
		{
			WARN(false,"Tip file not found at startup, but user wanted it...");
		}
	}
}

void MainWindowFrame::restoreConfigPanelDefaults()
{

	//Set the panel defaults (hidden/shown)
	// and their sizes
	wxSize winSize;
	winSize=getNiceWindowSize();
	float val,oldGravity;
	if(!configFile.getPanelEnabled(CONFIG_STARTUPPANEL_CONTROL))
	{
		splitLeftRight->Unsplit(panelLeft);
		checkMenuControlPane->Check(false);
	}
	else
	{
		val=configFile.getLeftRightSashPos();
		if(val > std::numeric_limits<float>::epsilon())
		{
			oldGravity=splitLeftRight->GetSashGravity();
			splitLeftRight->SetSashGravity(1.0);
			splitLeftRight->SetSashPosition((int)(val*(float)winSize.GetWidth()));
			splitLeftRight->SetSashGravity(oldGravity);

		}
	}

	if(!configFile.getPanelEnabled(CONFIG_STARTUPPANEL_RAWDATA))
	{
		splitTopBottom->Unsplit();
		checkMenuRawDataPane->Check(false);
	}
	else
	{
		val=configFile.getTopBottomSashPos();
		if(val > std::numeric_limits<float>::epsilon())
		{
			oldGravity=splitTopBottom->GetSashGravity();
			splitTopBottom->SetSashGravity(1.0);
			splitTopBottom->SetSashPosition((int)(val*(float)winSize.GetHeight()));
			splitTopBottom->SetSashGravity(oldGravity);
		}
	}

	//Set default or nice position for plotlist panel
	if(!configFile.getPanelEnabled(CONFIG_STARTUPPANEL_PLOTLIST))
	{
		splitterSpectra->Unsplit();
		checkMenuSpectraList->Check(false);
	}
	else
	{
		
		winSize=noteDataView->GetSize();
		val=configFile.getPlotListSashPos();
		if(val > std::numeric_limits<float>::epsilon())
		{
			oldGravity=splitterSpectra->GetSashGravity();
			splitterSpectra->SetSashGravity(1.0);
			splitterSpectra->SetSashPosition((int)(val*(float)winSize.GetWidth()));
			splitterSpectra->SetSashGravity(oldGravity);
		}
		
	}

	//set nice position for filter splitter (in left side of main window)
	if(configFile.configLoadedOK())
	{
		val=configFile.getFilterSashPos();
		winSize=noteData->GetSize();
		if(val > std::numeric_limits<float>::epsilon())
		{
			oldGravity=filterSplitter->GetSashGravity();
			filterSplitter->SetSashGravity(1.0);
			filterSplitter->SetSashPosition((int)(val*(float)winSize.GetHeight()));
			filterSplitter->SetSashGravity(oldGravity);
		}
	}

}

// wxGlade: add MainWindowFrame event handlers

void MainWindowFrame::SetCommandLineFiles(wxArrayString &files)
{
	
	textConsoleOut->Clear();
	bool loadedOK=false;
	//Load them up as data.
	for(unsigned int ui=0;ui<files.size();ui++)
	{
		loadedOK|=loadFile(files[ui],true,true);
	}

	requireFirstUpdate=loadedOK;

}

void MainWindowFrame::OnNoteDataView(wxNotebookEvent &evt)
{
	//Get rid of the console page
	if(evt.GetSelection() == NOTE_CONSOLE_PAGE_OFFSET)
		noteDataView->SetPageText(NOTE_CONSOLE_PAGE_OFFSET,TRANS("Cons."));

	//Keep processing
	evt.Skip();
}

void MainWindowFrame::OnCheckUpdatesThread(wxCommandEvent &evt)
{
	//Check to see if we have a new version or not, and
	//what that version number is
	
	ASSERT(verCheckThread->isComplete());

	//Check to see if we got the version number OK. 
	// this might have failed, e.g. if the user has no net connection,
	// or the remote RSS is not parseable
	if(verCheckThread->isRetrieveOK())
	{
		string remoteMax=verCheckThread->getVerStr().c_str();

		vector<string> maxVers;
		maxVers.push_back(remoteMax);
		maxVers.push_back(PROGRAM_VERSION);
		
		string s;
		if(getMaxVerStr(maxVers) !=PROGRAM_VERSION)
		{
			//Use status bar message to notify user about update
			s = string(TRANS("Update Notice: New version ")) + remoteMax + TRANS(" found online.");
		}
		else
		{
			s=string(TRANS("Online Check: " ))+string(PROGRAM_NAME) + TRANS(" is up-to-date."); 
		}
		statusMessage(s.c_str(),MESSAGE_INFO);
	}

	//Wait for, then delete the other thread, as we are done with it
	verCheckThread->Wait();
	verCheckThread=0;

}

void MainWindowFrame::checkReloadAutosave()
{
	wxString configDirPath =(configFile.getConfigDir());
	configDirPath+=("/") ;

	if(!wxDirExists(configDirPath))
		return;

	//obtain a list of autosave xml files
	//--
	wxArrayString *dirListing= new wxArrayString;
	std::string s;
	s=std::string(AUTOSAVE_PREFIX) +
			std::string("*") + std::string(AUTOSAVE_SUFFIX);
	wxString fileMask = (s);

	wxDir::GetAllFiles(configDirPath,dirListing,fileMask,wxDIR_FILES);

	if(!dirListing->GetCount())
	{
		delete dirListing;
		return;
	}
	//--


	unsigned int prefixLen;
	prefixLen = stlStr(configDirPath).size() + strlen(AUTOSAVE_PREFIX) + 1;

	//For convenience, Construct a mapping to the PIDs from the string
	//--
	map<string,unsigned int> autosaveNamePIDMap;
	for(unsigned int ui=0;ui<dirListing->GetCount(); ui++)
	{
		std::string tmp;
		tmp = stlStr(dirListing->Item(ui));
		//File name should match specified glob.
		ASSERT(tmp.size() >=(strlen(AUTOSAVE_PREFIX) + strlen(AUTOSAVE_SUFFIX)));

		//Strip the non-glob bit out of the string
		tmp = tmp.substr(prefixLen-1,tmp.size()-(strlen(AUTOSAVE_SUFFIX) + prefixLen-1));

		unsigned int pid;
		if(stream_cast(pid,tmp))
			continue;
		autosaveNamePIDMap[stlStr(dirListing->Item(ui))] = pid;
	}
	delete dirListing;
	//--


	//Filter on process existence and name match.
	//---
	for(map<string,unsigned int>::iterator it=autosaveNamePIDMap.begin();
		it!=autosaveNamePIDMap.end();)
	{
		//Note that map does not have a return value for erase in C++<C++11.
		// so use the unusual postfix incrementor method
		if(wxProcess::Exists(it->second) && processMatchesName(it->second,PROGRAM_NAME) )
			autosaveNamePIDMap.erase(it++); //Note postfix!
		else
			++it;
	}
	//--
	

	//A little messy, but handles two cases of dialog
	// one, where one file is either loaded, or deleted
	// two, where one of multiple files are either loaded, all deleted or none deleted
	vector<string> removeFiles;

	//Do we want to full erase the files in removeFiles (true)
	// or move (false)
	bool doErase=false;
	if(autosaveNamePIDMap.size() == 1)
	{
		//If we have exactly one autosave, ask the user about loading it
		wxString filePath=(autosaveNamePIDMap.begin()->first);
		wxMessageDialog wxD(this,
			TRANS("An auto-save state was found, would you like to restore it?.") 
			,TRANS("Autosave"),wxCANCEL|wxOK|wxICON_QUESTION|wxYES_DEFAULT );

		if(wxD.ShowModal()!= wxID_CANCEL)
		{
			if(!loadFile(filePath,false,true))
			{
				doErase=true;
				statusMessage(TRANS("Unable to load autosave file.."),MESSAGE_ERROR);
			}
			else
			{
				doErase=false;
				requireFirstUpdate=true;
				//Prevent the program from allowing save menu usage
				//into autosave file
				std::string tmpStr;
				visControl.state.setFilename(tmpStr);

				setSaveStatus();
			}

		
			removeFiles.push_back(stlStr(filePath));
		}
	}
	else if(autosaveNamePIDMap.size() > 1)
	{	
		//OK, so we have more than one autosave, from dead 3depict processes. 
		//ask the user which one they would like to load
		vector<std::pair<time_t,string> > filenamesAndTimes;


		for(map<string,unsigned int>::iterator it=autosaveNamePIDMap.begin();
			it!=autosaveNamePIDMap.end();++it)
		{
			time_t timeStamp=wxFileModificationTime((it->first));
			filenamesAndTimes.push_back(make_pair(timeStamp,it->first));
		}

		//Sort filenamesAndTimes by decreasing age, so that newest appears at
		// top of dialog
		ComparePairFirstReverse cmp;
		std::sort(filenamesAndTimes.begin(),filenamesAndTimes.end(),cmp);

		vector<string> autoSaveChoices;
		time_t now = wxDateTime::Now().GetTicks();
		for(size_t ui=0;ui<filenamesAndTimes.size();ui++)
		{
			string s;
			//Get the timestamp for the file
			s=veryFuzzyTimeSince(filenamesAndTimes[ui].first,now);
			s=filenamesAndTimes[ui].second + string(", ") + s;  //format like "filename.xml, a few seconds ago"
			autoSaveChoices.push_back(s);
		}

		//OK, looks like we have multiple selection options.
		//populate a list to ask the user to choose from.
		//User may only pick a single thing to restore.
		AutosaveDialog *dlg= new AutosaveDialog(this);
		dlg->setItems(autoSaveChoices);
	

		int dlgResult;
		dlgResult=dlg->ShowModal();

		//Show the dialog to get a choice from the user
		//We need to load a file if, and only if,
		// autosaves were not purged
		if(dlgResult == wxID_OK)
		{
			if(!dlg->removedItems())
			{
				requireFirstUpdate=true;

				std::string tmpStr;
				tmpStr =filenamesAndTimes[dlg->getSelectedItem()].second;

				if(loadFile((tmpStr),false,true))
				{
					//Prevent the program from allowing save menu usage
					//into autosave file
					doErase=true;
				}
				else 
					doErase=false;

				//If it either does, or doesn't work,
				//there is little point in keeping it
				removeFiles.push_back(tmpStr);

			}
			else 
			{
				for(unsigned int ui=0;ui<filenamesAndTimes.size();ui++)
					removeFiles.push_back(filenamesAndTimes[ui].second);
				doErase=true;
			}
	
		}
		else if(dlgResult == wxID_CANCEL)
		{
			if(dlg->removedItems())
			{
				for(unsigned int ui=0;ui<filenamesAndTimes.size();ui++)
					removeFiles.push_back(filenamesAndTimes[ui].second);
				doErase=true;
			}
		}
	}

	std::string tmpDir;
	tmpDir = configFile.getConfigDir()  + stlStr(wxFileName::GetPathSeparator())
		+string("oldAutosave");
	wxString wxtmpDir=(tmpDir);
	
	//Build the old autosave dir if needed
	if(removeFiles.size() && !doErase)
	{
		if(!wxDirExists(wxtmpDir))
		{
			if(!wxMkdir(wxtmpDir))
			{
				//well, the folder cannot be created,
				//so there is no neat way to move the 
				//autosave somewhere safe. Instead, lets
				// just delete it
				doErase=true;
			}
	
		}
	}

	for(unsigned int ui=0; ui<removeFiles.size(); ui++)
	{
		//move the autosave file elsewhere after loading it
		std::string baseDir = tmpDir+ stlStr(wxFileName::GetPathSeparator()); 
		wxtmpDir=(baseDir);

		//make a backup if needed 
		if(!doErase)
		{
			wxFileName fileNaming((removeFiles[ui]));
			wxCopyFile((removeFiles[ui]),wxtmpDir+fileNaming.GetFullName()); 
		}
		//if the copy works or not, just delete the autsave anyway
		wxRemoveFile((removeFiles[ui]));	
	}
}

void MainWindowFrame::setSaveStatus()
{
	fileSave->Enable( visControl.stateIsModified() && 
			visControl.state.getFilename().size());
}

wxSize MainWindowFrame::getNiceWindowSize() const
{
	wxDisplay *disp=new wxDisplay;
	wxRect r = disp->GetClientArea();

	bool haveDisplaySizePref;
	unsigned int xPref,yPref;

	haveDisplaySizePref=configFile.getInitialAppSize(xPref,yPref);

	//So Min size trumps all
	// - then client area 
	// - then saved setting
	// - then default size 
	wxSize winSize;
	if(haveDisplaySizePref)
		winSize.Set(xPref,yPref);
	else
	{
		winSize.Set(DEFAULT_WIN_WIDTH,DEFAULT_WIN_HEIGHT);
	}

	//Override using minimal window sizes
	winSize.Set(std::max(winSize.GetWidth(),(int)MIN_WIN_WIDTH),
			std::max(winSize.GetHeight(),(int)MIN_WIN_HEIGHT));

	//Shrink to display size, as needed	
	winSize.Set(std::min(winSize.GetWidth(),r.GetWidth()),
			std::min(winSize.GetHeight(),r.GetHeight()));


	delete disp;

	return winSize;

}

void MainWindowFrame::set_properties()
{
    // begin wxGlade: MainWindowFrame::set_properties
    SetTitle((PROGRAM_NAME));
    comboFilters->SetSelection(-1);
    
    comboFilters->SetToolTip(TRANS("List of available filters"));
#ifdef __APPLE__
    treeFilters->SetToolTip(TRANS("Tree - drag to move items, hold ⌘ for copy. Tap delete to remove items"));
#else
    treeFilters->SetToolTip(TRANS("Tree - drag to move items, hold Ctrl for copy. Tap delete to remove items."));
#endif
    checkAutoUpdate->SetToolTip(TRANS("Enable/Disable automatic updates of data when filter change takes effect"));
    checkAutoUpdate->SetValue(true);

    checkAlphaBlend->SetToolTip(TRANS("Enable/Disable \"Alpha blending\" (transparency) in rendering system. Blending is used to smooth objects (avoids artefacts known as \"jaggies\") and to make transparent surfaces. Disabling will provide faster rendering but look more blocky")); 
    checkLighting->SetToolTip(TRANS("Enable/Disable lighting calculations in rendering, for objects that request this. Lighting provides important depth cues for objects comprised of 3D surfaces. Disabling may allow faster rendering in complex scenes"));
    checkWeakRandom->SetToolTip(TRANS("Enable/Disable weak randomisation (Galois linear feedback shift register). Strong randomisation uses a much slower random selection method, but provides better protection against inadvertent correlations, and is recommended for final analyses"));

    checkLimitOutput->SetToolTip(TRANS("Limit the number of points that can be displayed in the 3D  scene. Does not affect filter tree calculations. Disabling this can severely reduce performance, due to large numbers of points being visible at once."));
    checkCaching->SetToolTip(TRANS("Enable/Disable caching of intermediate results during filter updates. Disabling caching will use less system RAM, though changes to any filter property will cause the entire filter tree to be recomputed, greatly slowing computations"));

    gridCameraProperties->SetToolTip(TRANS("Camera data information"));
    noteCamera->SetScrollRate(10, 10);

#ifndef APPLE_EFFECTS_WORKAROUND
    checkPostProcessing->SetToolTip(TRANS("Enable/disable visual effects on final 3D output"));
#endif
    checkFxCrop->SetToolTip(TRANS("Enable cropping post-process effect"));
    comboFxCropAxisOne->SetSelection(0);
    comboFxCropAxisTwo->SetSelection(0);
    checkFxEnableStereo->SetToolTip(TRANS("Colour based 3D effect enable/disable - requires appropriate colour filter 3D glasses."));
    comboFxStereoMode->SetToolTip(TRANS("Glasses colour mode"));
    comboFxStereoMode->SetSelection(0);
    sliderFxStereoBaseline->SetToolTip(TRANS("Level of separation between left and right images, which sets 3D depth to visual distortion tradeoff"));
    gridRawData->CreateGrid(10, 2);
    gridRawData->EnableEditing(false);
    gridRawData->EnableDragRowSize(false);
    gridRawData->SetColLabelValue(0, TRANS("X"));
    gridRawData->SetColLabelValue(1, TRANS("Y"));
    btnRawDataSave->SetToolTip(TRANS("Save raw data to file"));
    btnRawDataClip->SetToolTip(TRANS("Copy raw data to clipboard"));
    btnStashManage->SetToolTip(TRANS("Manage \"stashed\" data."));
    textConsoleOut->SetToolTip(TRANS("Program text output"));
    comboCamera->SetToolTip(TRANS("Select active camera, or type to create new named camera"));
    buttonRemoveCam->SetToolTip(TRANS("Remove the selected camera"));
    checkFxCropCameraFrame->SetToolTip(TRANS("Perform cropping from coordinate frame of camera"));
    spinCachePercent->SetToolTip(TRANS("Set the maximum amount of RAM to use in order to speed repeat computations"));
    btnFilterTreeCollapse->SetToolTip(TRANS("Collapse the filter tree"));
    btnFilterTreeExpand->SetToolTip(TRANS("Expand the filter tree"));
    refreshButton->SetToolTip (TRANS("Process the filter tree, hold shift to purge cached filter data"));

    // end wxGlade
    //

    panelSpectra->setPlotWrapper(visControl.getPlotWrapper(),false);
    
    //Set the controls that the viscontrol needs to interact with
   //TODO: Require these via the constructor ?
    visControl.setRawGrid(gridRawData); //Raw data grid
    visControl.setPlotList(plotList);
    visControl.setConsole(textConsoleOut);


    refreshButton->Enable(false);
    comboCamera->Bind(wxEVT_SET_FOCUS, &MainWindowFrame::OnComboCameraSetFocus, this);
    comboStash->Bind(wxEVT_SET_FOCUS, &MainWindowFrame::OnComboStashSetFocus, this);
    noteDataView->Bind(wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, &MainWindowFrame::OnNoteDataView, this);
    treeFilters->Bind(wxEVT_KEY_DOWN,&MainWindowFrame::OnTreeKeyDown, this); //Only required for 2.9
    gridCameraProperties->Clear();
    int widths[] = {-4,-2,-1};
    MainFrame_statusbar->SetStatusWidths(3,widths);

}

void MainWindowFrame::do_layout()
{
    // begin wxGlade: MainWindowFrame::do_layout
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerLeft = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerTools = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerToolsRamUsage = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_1 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* postProcessSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerFxStereo = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerSetereoBaseline = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerStereoCombo = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* cropFxSizer = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* sizerFxCropGridLow = new wxFlexGridSizer(3, 2, 2, 2);
    wxBoxSizer* cropFxBodyCentreSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* rightPanelSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* textConsoleSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* rawDataGridSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* rawDataSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* plotListSizery = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* topPanelSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerFxCropRHS = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sizerFxCropLHS = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* filterPaneSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* filterTreeLeftRightSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* filterRightOfTreeSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* filterMainCtrlSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* stashRowSizer = new wxBoxSizer(wxHORIZONTAL);
    filterPaneSizer->Add(lblSettings, 0, 0, 0);
    stashRowSizer->Add(comboStash, 1, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND, 3);
    stashRowSizer->Add(btnStashManage, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 0);
    filterPaneSizer->Add(stashRowSizer, 0, wxEXPAND, 0);
    filterPaneSizer->Add(filteringLabel, 0, 0, 0);
    filterMainCtrlSizer->Add(comboFilters, 0, wxLEFT|wxRIGHT|wxEXPAND, 4);
    filterMainCtrlSizer->Add(treeFilters, 3, wxLEFT|wxBOTTOM|wxEXPAND, 3);
    filterTreeLeftRightSizer->Add(filterMainCtrlSizer, 3, wxEXPAND, 0);
    filterRightOfTreeSizer->Add(checkAutoUpdate, 0, 0, 0);
    filterRightOfTreeSizer->Add(10, 10, 0, 0, 0);
    filterRightOfTreeSizer->Add(refreshButton, 0, wxALL, 2);
    filterRightOfTreeSizer->Add(20, 20, 0, 0, 0);
    filterRightOfTreeSizer->Add(btnFilterTreeCollapse, 0, wxLEFT, 6);
    filterRightOfTreeSizer->Add(btnFilterTreeExpand, 0, wxLEFT, 6);
    filterRightOfTreeSizer->Add(10, 10, 0, 0, 0);
    filterRightOfTreeSizer->Add(btnFilterTreeErrs,0,wxLEFT,6);
    btnFilterTreeErrs->Show(false);
    filterTreeLeftRightSizer->Add(filterRightOfTreeSizer, 2, wxEXPAND, 0);
    filterTreePane->SetSizer(filterTreeLeftRightSizer);
    do_filtergrid_prop_layout();
//    filterSplitter->SplitHorizontally(filterTreePane, filterPropertyPane);//DISABLED This has to be done later to get the window to work.
    filterPaneSizer->Add(filterSplitter, 1, wxEXPAND, 0);
    noteData->SetSizer(filterPaneSizer);
    do_cameragrid_prop_layout();

#ifndef APPLE_EFFECTS_WORKAROUND
    postProcessSizer->Add(checkPostProcessing, 0, wxALL, 5);
#endif
    cropFxSizer->Add(checkFxCrop, 0, wxALL, 6);
    cropFxSizer->Add(checkFxCropCameraFrame, 0, wxLEFT, 15);
    sizerFxCropLHS->Add(comboFxCropAxisOne, 0, wxRIGHT|wxBOTTOM|wxEXPAND, 5);
    sizerFxCropLHS->Add(panelFxCropOne, 1, wxRIGHT|wxEXPAND| 5);
    cropFxBodyCentreSizer->Add(sizerFxCropLHS, 1, wxEXPAND, 0);
    sizerFxCropRHS->Add(comboFxCropAxisTwo, 0, wxLEFT|wxBOTTOM|wxEXPAND, 5);
    sizerFxCropRHS->Add(panelFxCropTwo, 1, wxLEFT|wxEXPAND, 5);
    cropFxBodyCentreSizer->Add(sizerFxCropRHS, 1, wxEXPAND, 0);
    cropFxSizer->Add(cropFxBodyCentreSizer, 1, wxLEFT|wxRIGHT|wxTOP|wxEXPAND, 5);
    sizerFxCropGridLow->Add(labelFxCropDx, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 0);
    sizerFxCropGridLow->Add(textFxCropDx, 0, 0, 0);
    sizerFxCropGridLow->Add(labelFxCropDy, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 0);
    sizerFxCropGridLow->Add(textFxCropDy, 0, 0, 0);
    sizerFxCropGridLow->Add(labelFxCropDz, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 0);
    sizerFxCropGridLow->Add(textFxCropDz, 0, 0, 0);
    sizerFxCropGridLow->AddGrowableRow(0);
    sizerFxCropGridLow->AddGrowableRow(1);
    sizerFxCropGridLow->AddGrowableRow(2);
    sizerFxCropGridLow->AddGrowableCol(0);
    sizerFxCropGridLow->AddGrowableCol(1);
    cropFxSizer->Add(sizerFxCropGridLow, 0, wxBOTTOM|wxEXPAND, 5);
    noteFxPanelCrop->SetSizer(cropFxSizer);
    sizerFxStereo->Add(checkFxEnableStereo, 0, wxLEFT|wxTOP, 6);
    sizerFxStereo->Add(20, 20, 0, 0, 0);
    sizerStereoCombo->Add(lblFxStereoMode, 0, wxLEFT|wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
    sizerStereoCombo->Add(comboFxStereoMode, 0, wxLEFT, 5);
    sizerStereoCombo->Add(bitmapFxStereoGlasses, 0, 0, 0);
    sizerFxStereo->Add(sizerStereoCombo, 0, wxBOTTOM|wxEXPAND, 15);
    sizerSetereoBaseline->Add(labelFxStereoBaseline, 0, wxLEFT|wxTOP, 5);
    sizerSetereoBaseline->Add(sliderFxStereoBaseline, 1, wxLEFT|wxRIGHT|wxTOP|wxEXPAND, 5);
    sizerFxStereo->Add(sizerSetereoBaseline, 0, wxEXPAND, 0);
    sizerFxStereo->Add(checkFxStereoLensFlip, 0, wxLEFT, 5);
    noteFxPanelStereo->SetSizer(sizerFxStereo);
    noteEffects->AddPage(noteFxPanelCrop, TRANS("Crop"));
    noteEffects->AddPage(noteFxPanelStereo, TRANS("Stereo"));
    postProcessSizer->Add(noteEffects, 1, wxEXPAND, 0);
    notePost->SetSizer(postProcessSizer);
    sizerTools->Add(labelAppearance, 0, wxTOP, 3);
    sizerTools->Add(checkAlphaBlend, 0, wxLEFT|wxTOP|wxBOTTOM, 5);
    sizerTools->Add(checkLighting, 0, wxLEFT|wxTOP|wxBOTTOM, 6);
    sizerTools->Add(static_line_1, 0, wxEXPAND, 0);
    sizerTools->Add(labelPerformance, 0, wxTOP, 3);
    sizerTools->Add(checkWeakRandom, 0, wxLEFT|wxTOP|wxBOTTOM, 5);
    sizer_1->Add(checkLimitOutput, 0, wxRIGHT, 3);
    sizer_1->Add(textLimitOutput, 0, wxLEFT, 4);
    sizerTools->Add(sizer_1, 0, wxLEFT|wxEXPAND, 5);
    sizerTools->Add(checkCaching, 0, wxLEFT|wxTOP|wxBOTTOM, 5);
    sizerToolsRamUsage->Add(labelMaxRamUsage, 0, wxRIGHT, 5);
    sizerToolsRamUsage->Add(spinCachePercent, 0, 0, 5);
    sizerTools->Add(sizerToolsRamUsage, 1, wxTOP|wxEXPAND, 5);
    noteTools->SetSizer(sizerTools);
    notebookControl->AddPage(noteData, TRANS("Data"));
    notebookControl->AddPage(noteCamera, TRANS("Cam"));
    notebookControl->AddPage(notePost, TRANS("Post"));
    notebookControl->AddPage(noteTools, TRANS("Tools"));
    sizerLeft->Add(notebookControl, 1, wxLEFT|wxBOTTOM|wxEXPAND, 2);
    panelLeft->SetSizer(sizerLeft);
    topPanelSizer->Add(panelView, 1, wxEXPAND, 0);
    panelTop->SetSizer(topPanelSizer);
    plotListSizery->Add(plotListLabel, 0, 0, 0);
    plotListSizery->Add(plotList, 1, wxEXPAND, 0);
    window_2_pane_2->SetSizer(plotListSizery);
    splitterSpectra->SplitVertically(panelSpectra, window_2_pane_2);
    rawDataGridSizer->Add(gridRawData, 3, wxEXPAND, 0);
    rawDataSizer->Add(20, 20, 1, 0, 0);
    rawDataSizer->Add(btnRawDataSave, 0, wxLEFT, 2);
    rawDataSizer->Add(btnRawDataClip, 0, wxLEFT, 2);
    rawDataGridSizer->Add(rawDataSizer, 0, wxTOP|wxEXPAND, 5);
    noteRaw->SetSizer(rawDataGridSizer);
    textConsoleSizer->Add(textConsoleOut, 1, wxEXPAND, 0);
    noteDataViewConsole->SetSizer(textConsoleSizer);
    noteDataView->AddPage(splitterSpectra, TRANS("Plot"));
    noteDataView->AddPage(noteRaw, TRANS("Raw"));
    noteDataView->AddPage(noteDataViewConsole, TRANS("Cons."));
    splitTopBottom->SplitHorizontally(panelTop, noteDataView);
    rightPanelSizer->Add(splitTopBottom, 1, wxEXPAND, 0);
    panelRight->SetSizer(rightPanelSizer);
    splitLeftRight->SplitVertically(panelLeft, panelRight);
    topSizer->Add(splitLeftRight, 1, wxEXPAND, 0);
    SetSizer(topSizer);
    topSizer->Fit(this);
    Layout();
    // end wxGlade
    //
    // GTK fix hack thing. reparent window

	

	panelTop->Reparent(splitTopBottom);

	//Set the combo text
	haveSetComboCamText=false;
	comboCamera->SetValue((TRANS(cameraIntroString)));
	haveSetComboStashText=false;
	comboStash->SetValue((TRANS(stashIntroString)));

}

void MainWindowFrame::do_filtergrid_prop_layout()
{
	wxBoxSizer* filterPropGridSizer = new wxBoxSizer(wxVERTICAL);

	filterPropGridSizer->Add(propGridLabel, 0, 0, 0);
	filterPropGridSizer->Add(gridFilterPropGroup, 1, wxLEFT|wxEXPAND, 4);
	filterPropertyPane->SetSizer(filterPropGridSizer);
	filterPropertyPane->Fit();
	filterPropGridSizer->Fit(filterPropertyPane);

	Layout();
	filterSplitter->UpdateSize();

}

void MainWindowFrame::do_cameragrid_prop_layout()
{
    sizerAlignCam_staticbox = new wxStaticBox(noteCamera, wxID_ANY, _("Align Camera"));

    wxBoxSizer* camPaneSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* camTopRowSizer = new wxBoxSizer(wxHORIZONTAL);
    sizerAlignCam_staticbox->Lower();
    wxStaticBoxSizer* sizerAlignCam = new wxStaticBoxSizer(sizerAlignCam_staticbox, wxVERTICAL);
    wxBoxSizer* sizerCamAlignMinus = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizerCamAlignPlus = new wxBoxSizer(wxHORIZONTAL);
    
    camPaneSizer->Add(labelCameraName, 0, 0, 0);
    camTopRowSizer->Add(comboCamera, 3, 0, 0);
    camTopRowSizer->Add(buttonRemoveCam, 0, wxLEFT|wxRIGHT, 2);
    camPaneSizer->Add(camTopRowSizer, 0, wxTOP|wxBOTTOM|wxEXPAND, 4);
    camPaneSizer->Add(cameraNamePropertySepStaticLine, 0, wxEXPAND, 0);
    camPaneSizer->Add(gridCameraProperties, 1, wxEXPAND, 0);
    sizerCamAlignPlus->Add(buttonAlignCamXPlus, 0, wxALIGN_CENTER|wxALL, 5);
    sizerCamAlignPlus->Add(buttonAlignCamYPlus, 0, wxALIGN_CENTER|wxALL, 5);
    sizerCamAlignPlus->Add(buttonAlignCamZPlus, 0, wxALIGN_CENTER|wxALL, 5);
    sizerAlignCam->Add(sizerCamAlignPlus, 0, 0, 0);
    sizerCamAlignMinus->Add(buttonAlignCamXMinus, 0, wxALIGN_CENTER|wxALL, 5);
    sizerCamAlignMinus->Add(buttonAlignCamYMinus, 0, wxALIGN_CENTER|wxALL, 5);
    sizerCamAlignMinus->Add(buttonAlignCamZMinus, 0, wxALIGN_CENTER|wxALL, 5);
    sizerAlignCam->Add(sizerCamAlignMinus, 0, wxALIGN_CENTER, 0);
    sizerAlignCam->Add(checkAlignCamResize, 0, wxALIGN_CENTER|wxALL, 4);
    camPaneSizer->Add(sizerAlignCam, 1, 0, 0);

    noteCamera->SetSizer(camPaneSizer);
    noteCamera->Fit();
    //camPaneSizer->Fit();

    noteCamera->Layout();

    //noteCamera->UpdateSize();
}
