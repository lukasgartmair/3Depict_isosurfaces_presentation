/*
 *	animateFilterDialog - GUI for animate filter
 *	Copyright (C) 2015, D. Haley, A Ceguerra 
 
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

#ifndef ANIMATEFILTERDIALOG_H
#define ANIMATEFILTERDIALOG_H

#include <wx/wx.h>
// begin wxGlade: ::dependencies
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/notebook.h>
#include <wx/treectrl.h>
#include <wx/grid.h>
// end wxGlade


#include "backend/animator.h"
#include "backend/viscontrol.h" //for upWxTreeCtrl


// begin wxGlade: ::extracode
// end wxGlade

enum
{
	FILENAME_IMAGE,
	FILENAME_IONS,
	FILENAME_RANGE,
	FILENAME_PLOT,
	FILENAME_VOXEL
};

enum
{
	RANGE_OAKRIDGE,
	RANGE_AMETEK_RRNG,
	RANGE_AMETEK_ENV,
	RANGE_FORMATNAME_END
};

class ExportAnimationDialog: public wxDialog {
public:
    // begin wxGlade: ExportAnimationDialog::ids
    // end wxGlade

    ExportAnimationDialog(wxWindow* parent, int id, const wxString& title, const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize, long style=wxDEFAULT_DIALOG_STYLE);
    ~ExportAnimationDialog();
   
    //!Must be called before displaying dialog, and after setting tree
    void prepare();
    
    //obtain the desired filename for a particular type of output
    std::string getFilename(unsigned int frame, unsigned int nameType, unsigned int number=0) const ;
    //Obtain the desired width of the output image
    unsigned int getImageWidth() const { return imageWidth;}
    //Obtain the desired height of the output image
    unsigned int getImageHeight() const { return imageHeight;};

    //Get the number of frames that are in the animation sequence
    size_t getNumFrames() const { return propertyAnimator.getMaxFrame();}
   
    //Return a modified version f the filter tree, applying the changes requested
    // by the user
    bool getModifiedTree(size_t frame, FilterTree &t,bool &needUp) const;

    //Set the tree that we are to work with
    void setTree(const FilterTree &origTree) { filterTree=&origTree;}; 

    //Does the user want to obtain image output?
    bool wantsImages() const { return wantImageOutput;}
    //Does the user want to obtain plot output?
    bool wantsPlots() const { return wantPlotOutput;}
    //Does the user want to obtain ion output?
    bool wantsIons() const { return wantIonOutput;}
    //Does the user want to obtain range output?
    bool wantsRanges() const { return wantRangeOutput;}
    //Does the user want to obtain range output?
    bool wantsVoxels() const { return wantVoxelOutput;}
    
    //Does the user want all data output, or only when the data
    // changes (needs a refresh)
    bool wantsOnlyChanges() const { return wantOnlyChanges;}

    //!Obtain the format the user wants to save ranges in
    size_t getRangeFormat()  const;

    //! Obtain the current state from the animation (keyframes)
    // the second element provides the mappings for the property animator to 
    // filter tree path locations
    void getAnimationState(PropertyAnimator &prop, std::vector<std::pair<std::string,size_t> > &pathMapping) const ;
    //!Obtain the current state from the animation
    void setAnimationState(const PropertyAnimator &prop,
		    const std::vector<std::pair<std::string,size_t> > &pathMapping);

    //!Obtain the filter tree path string->animation ID mapping
    void getPathMapping(std::vector<std::pair<std::string,size_t> > &mapping, bool allowMissing=false) const;

    void setDefImSize(unsigned int w, unsigned int h) ; 
private:
    //!Tree of filters that can be manipulated
    const FilterTree *filterTree;
    //!Mapping from ID of filter to the pointer in the filter tree
    std::map<size_t,Filter *> filterMap;

    //!Mapping to allow for converting entry of RNG selection combo into
    // the correct range enum value
    std::map<std::string,size_t> rangeMap;
    //!Default height/width desired for output images
    size_t imageWidth,imageHeight;
    bool imageSizeOK;

    PropertyAnimator propertyAnimator;

    //!Working directory for outputting data
    std::string workDir;

    std::string imagePrefix;
    //!True if any con
    bool existsConflicts;
    //!true if the user has selected image output functionality
    bool wantImageOutput;
    //!True if the user has requested ion data output
    bool wantIonOutput;
    //!True if the user wants plots output
    bool wantPlotOutput;
    //!True if the user wants to save voxel data
    bool wantVoxelOutput;
    //!True if the user wants to save range data
    bool wantRangeOutput;
    //!True if the user only wants to save data if it changes
    bool wantOnlyChanges;

    //!Current frame that the user wants to see in the frame view
    size_t currentFrame;

    //!Type of rangefile to export
    size_t rangeExportMode;


    //viewport aspect ratio for image output
    float imageAspectRatio;

    //Used to jump out of wx events that are generated by 
    // the code, rather than the user, eg text events
    bool programmaticEvent;
    //UI update function
    void update(); 


    //Enable/disable the OK button depending upon dialog state
    void updateOKButton();
    //update function for frame view page, grid control
    void updateFilterViewGrid();
    //update function for frame view page, grid contorl
    void updateFrameViewGrid();
    //Updates the slider on the frame view page
    void updateFrameViewSlider();
    // begin wxGlade: ExportAnimationDialog::methods
    void set_properties();
    void do_layout();
    // end wxGlade

protected:
    // begin wxGlade: ExportAnimationDialog::attributes
    wxStaticBox* outputDataSizer_staticbox;
    wxStaticBox* keyFramesSizer_staticbox;
    wxStaticBox* filterPropertySizer_staticbox;
    wxTreeCtrl* filterTreeCtrl;
    wxPropertyGrid* propertyGrid;
    wxPanel* filterLeftPane;
    wxGrid* animationGrid;
    wxButton* keyFrameRemoveButton;
    wxPanel* filterRightPane;
    wxSplitterWindow* splitPaneFilter;
    wxPanel* filterViewPane;
    wxStaticText* labelWorkDir;
    wxTextCtrl* textWorkDir;
    wxButton* buttonWorkDir;
    wxCheckBox* checkOutOnlyChanged;
    wxStaticLine* outputDataSepLine;
    wxStaticText* labelDataType;
    wxCheckBox* checkImageOutput;
    wxStaticText* lblImageName;
    wxTextCtrl* textImageName;
    wxStaticText* labelImageSize;
    wxTextCtrl* textImageSize;
    wxButton* buttonImageSize;
    wxCheckBox* checkPoints;
    wxCheckBox* checkPlotData;
    wxCheckBox* checkVoxelData;
    wxCheckBox* checkRangeData;
    wxStaticText* labelRangeFormat;
    wxChoice* comboRangeFormat;
    wxStaticLine* static_line_1;
    wxStaticText* labelFrame;
    wxSlider* frameSlider;
    wxTextCtrl* textFrame;
    wxGrid* framePropGrid;
    wxPanel* frameViewPane;
    wxNotebook* viewNotebook;
    wxButton* cancelButton;
    wxButton* okButton;
    // end wxGlade

    DECLARE_EVENT_TABLE();

public:
    virtual void OnFilterTreeCtrlSelChanged(wxTreeEvent &event); // wxGlade: <event_handler>
    virtual void OnFilterGridCellChanging(wxPropertyGridEvent &event); // wxGlade: <event_handler>
    virtual void OnFilterGridCellSelected(wxPropertyGridEvent &event); // wxGlade: <event_handler>
    virtual void OnAnimateGridCellEditorShow(wxGridEvent &event); // wxGlade: <event_handler>
    virtual void OnFrameGridCellEditorShow(wxGridEvent &event); // wxGlade: <event_handler>
    virtual void OnButtonKeyFrameRemove(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnOutputDirText(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnButtonWorkDir(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckOutDataChange(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckImageOutput(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnImageFilePrefix(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnBtnResolution(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckPointOutput(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckPlotOutput(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckVoxelOutput(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnCheckRangeOutput(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnRangeTypeCombo(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFrameViewSlider(wxScrollEvent &event); // wxGlade: <event_handler>
    virtual void OnTextFrame(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnButtonCancel(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnButtonOK(wxCommandEvent &event); // wxGlade: <event_handler>
    virtual void OnFilterViewUnsplit(wxSplitterEvent &event); // wxGlade: <event_handler>

}; // wxGlade: end class


#endif // ANIMATEFILTERDIALOG_H
