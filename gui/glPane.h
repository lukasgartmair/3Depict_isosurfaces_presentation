/*
 *	gLPane.h - WxWidgets opengl Pane. 
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
#ifndef GLPANE_H
#define GLPANE_H
 

#include "gl/scene.h"
#include "gl/tr.h"

#include <wx/glcanvas.h>
#include <wx/timer.h>
#include <wx/statusbr.h>

class BasicGLPane : public wxGLCanvas
{
private:

	wxGLContext *context;

	Scene *currentScene;

	wxStatusBar *parentStatusBar;
	wxTimer *parentStatusTimer;
	unsigned int statusDelay;

#ifdef __APPLE__ 
	bool requireContextUpdate;
#endif
	
	//In some implementation of openGL in wx. 
	//calling GL funcs before Paint() will crash program
	bool paneInitialised;
	//Is the user engaged in a drag operation?
	bool dragging;

	//Where is the start of the mouse drag?
	wxPoint draggingStart;
	bool lastMoveShiftDown;
	
	//True if an object has been mouse-overed for selection
	bool selectionMode;
	//The scene ID value for the currently selected object
	unsigned int curSelectedObject;
	//The scene ID value for object currently being "hovered" over
	unsigned int hoverObject;

	//!Last mouseflags/keyflags during selection event
	unsigned int lastMouseFlags,lastKeyFlags;

	//Test for a object selection. Returns -1 if no selection
	//or object ID if selection OK. Also sets lastSelected & scene
	unsigned int selectionTest(const wxPoint &p,  bool &shouldRedraw);

	//Test for a object hover under cursor Returns -1 if no selection
	//or object ID if selection OK. Also sets last hover and scene
	unsigned int hoverTest(const wxPoint &p,  bool &shouldRedraw);

	//!Are there updates to the camera Properties due to camera motion?
	bool haveCameraUpdates;

	//!Are we currently applying a device in the scene?
	bool applyingDevice;

	//Parameters for modifying mouse speed
	float mouseZoomFactor,mouseMoveFactor;

	unsigned int lastKeyDoubleTap;

	wxTimer *keyDoubleTapTimer;

	//build a TR tile context to allow drawing the image in chunks.
	// width and height are the output image size. 
	// wantAlpha is used if we want to retrieve the alpha (transpar.) channel
	TRcontext *generateTileContext(unsigned int width, unsigned int height, 
			unsigned char *buffer, bool wantAlpha=false) const;
public:
	bool displaySupported() const;

	void setScene(Scene *s) { currentScene=s;}

	//Enable/Disable the scene interaction for user objects?
	void setSceneInteractionAllowed(bool enabled=true);

	//!Must be called before user has a chance to perform interaction
	void setParentStatus(wxStatusBar *statusBar,
			wxTimer *timer,unsigned int statDelay) 
		{ parentStatusBar=statusBar;parentStatusTimer=timer;statusDelay=statDelay;};

	bool hasCameraUpdates() const {return haveCameraUpdates;};
	
	void clearCameraUpdates() {haveCameraUpdates=false;};

	BasicGLPane(wxWindow* parent);
	~BasicGLPane();
    
	void resized(wxSizeEvent& evt);
    
	int getWidth();
	int getHeight();

	//Panel will not update if a child window for some platforms, using
	// normal wx reresh code. Force it.
	// See, eg http://stackoverflow.com/questions/6458451/force-repaint-of-wxpython-window-wxmpl-plot
	void forceRedraw();


	void setMouseMoveFactor(float f) { mouseMoveFactor=f;};
	void setMouseZoomFactor(float f) { mouseZoomFactor=f;};

	//!Is the window initialised?
	bool isInited() { return paneInitialised;}
       	
	//!Set the background colour (openGL clear colour)
	void setGlClearColour(float r,float g,float b);	
	//!Pull in the colour from the scene 
	void updateClearColour();
	//!Render the view using the scene
	void render(wxPaintEvent& evt);
	//!Construct a 3D viewport, ready for openGL output. Returns false if initialisation failed
	bool prepare3DViewport(int topleft_x, int topleft_y, int bottomrigth_x, int bottomrigth_y);
   	//!Save an image to file, return false on failure
	bool saveImage(unsigned int width, unsigned int height,const char *filename, bool showProgress=true, bool needPostPaint=true);
	//!Save an image sequence to files by orbiting the camera
	bool saveImageSequence(unsigned int width, unsigned int height, unsigned int nFrames,
			wxString &path, wxString &prefix, wxString &extension);

	//!Get the background colour
	void getGlClearColour(float &r,float &g,float &b) { currentScene->getBackgroundColour(r,g,b);}
	// events
	void mouseMoved(wxMouseEvent& event);
	void mouseDown(wxMouseEvent& event);
	void mouseWheelMoved(wxMouseEvent& event);
	void mouseReleased(wxMouseEvent& event);
	void rightClick(wxMouseEvent& event);
	void mouseLeftWindow(wxMouseEvent& event);
	void keyPressed(wxKeyEvent& event);
	void keyReleased(wxKeyEvent& event);
	void charEvent(wxKeyEvent& event);
	void OnEraseBackground(wxEraseEvent &);
	void OnAxisTapTimer(wxTimerEvent &);
	bool setFullscreen(bool fullscreen);
	bool setMouseVisible(bool visible);
	   
	
	DECLARE_EVENT_TABLE()
};
 
#endif 
