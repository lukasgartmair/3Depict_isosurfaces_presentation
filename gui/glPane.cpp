/*
 *	glPane.cpp - OpenGL panel implementation
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


#include <wx/progdlg.h>

#include "wx/wxcommon.h"

#include "common/stringFuncs.h"
#include "gl/select.h"
#include "glPane.h"


// include OpenGL
#ifdef __WXMAC__
#include "OpenGL/glu.h"
#include "OpenGL/gl.h"

#else
#include <GL/glu.h>
#include <GL/gl.h>





#endif

#include "common/translation.h"

//Unclear why, but windows does not allow GL_BGR, 
//needs some other define BGR_EXT (NFI), which is 0x80E0
#if defined(WIN32) || defined(WIN64)
    #define GL_BGR 0x80E0
#endif

using std::string;

enum
{
	ID_KEYPRESS_TIMER=wxID_ANY+1,
};

//Double tap delay (ms), for axis reversal
const unsigned int DOUBLE_TAP_DELAY=500; 

BEGIN_EVENT_TABLE(BasicGLPane, wxGLCanvas)
EVT_MOTION(BasicGLPane::mouseMoved)
EVT_ERASE_BACKGROUND(BasicGLPane::OnEraseBackground)
EVT_LEFT_DOWN(BasicGLPane::mouseDown)
EVT_LEFT_UP(BasicGLPane::mouseReleased)
EVT_MIDDLE_UP(BasicGLPane::mouseReleased)
EVT_MIDDLE_DOWN(BasicGLPane::mouseDown)
EVT_RIGHT_UP(BasicGLPane::mouseReleased)
EVT_RIGHT_DOWN(BasicGLPane::mouseDown)
EVT_LEAVE_WINDOW(BasicGLPane::mouseLeftWindow)
EVT_SIZE(BasicGLPane::resized)
EVT_KEY_DOWN(BasicGLPane::keyPressed)
EVT_KEY_UP(BasicGLPane::keyReleased)
EVT_MOUSEWHEEL(BasicGLPane::mouseWheelMoved)
EVT_PAINT(BasicGLPane::render)
EVT_TIMER(ID_KEYPRESS_TIMER,BasicGLPane::OnAxisTapTimer)
END_EVENT_TABLE()

//Controls camera pan/translate/pivot speed; Radii per pixel or distance/pixel
const float CAMERA_MOVE_RATE=0.05;

// Controls zoom speed, in err, zoom units.. Ahem. 
const float CAMERA_SCROLL_RATE=0.05;
//Zoom speed for keyboard
const float CAMERA_KEYBOARD_SCROLL_RATE=1;

int attribList[] = {WX_GL_RGBA,
			WX_GL_DEPTH_SIZE,
			16,
			WX_GL_DOUBLEBUFFER,
			1,
			0,0};

BasicGLPane::BasicGLPane(wxWindow* parent) : wxGLCanvas(parent, wxID_ANY,  attribList)
{
	haveCameraUpdates=false;
	applyingDevice=false;
	paneInitialised=false;

	keyDoubleTapTimer=new wxTimer(this,ID_KEYPRESS_TIMER);
	lastKeyDoubleTap=(unsigned int)-1;
	context=0;

	mouseMoveFactor=mouseZoomFactor=1.0f;	
	dragging=false;
	lastMoveShiftDown=false;
	selectionMode=false;
	lastKeyFlags=lastMouseFlags=0;

#ifdef __APPLE__
	requireContextUpdate=false;
#endif

}

BasicGLPane::~BasicGLPane()
{
	keyDoubleTapTimer->Stop();
	delete keyDoubleTapTimer;
	if(context)
		delete context;
}

bool BasicGLPane::displaySupported() const
{
	return IsDisplaySupported(attribList);
}

void BasicGLPane::setSceneInteractionAllowed(bool enabled)
{
	currentScene->lockInteraction(!enabled);
}

unsigned int  BasicGLPane::selectionTest(const wxPoint &p,bool &shouldRedraw)
{

	if(currentScene->isInteractionLocked())
	{
		shouldRedraw=false;
		return -1; 
	}

	//TODO: Refactor. Much of this could be pushed into the scene, 
	//and hence out of this wx panel.
	
	//Push on the matrix stack
	glPushMatrix();
	
	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);
	//5x5px picking region. Picking is done by modifying the view
	//to enlarge the selected region.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPickMatrix(p.x, oldViewport[3]-p.y,5, 5, oldViewport);
	glMatrixMode(GL_MODELVIEW);

	int lastSelected = currentScene->getLastSelected();
	int selectedObject=currentScene->glSelect();

	//If the object selection hasn't changed, we don't need to redraw
	//if it has changed, we should redraw
	shouldRedraw = (lastSelected !=selectedObject);

	//Restore the previous matirx
	glPopMatrix();

	//Restore the viewport
	int w, h;
	GetClientSize(&w, &h);
	glViewport(0, 0, (GLint) w, (GLint) h);

	return selectedObject;
}
 
unsigned int  BasicGLPane::hoverTest(const wxPoint &p,bool &shouldRedraw)
{

	if(currentScene->isInteractionLocked())
	{
		shouldRedraw=false;
		return -1;
	}
	//Push on the matrix stack
	glPushMatrix();
	
	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);
	//5x5px picking region. Picking is done by modifying the view
	//to enlarge the selected region.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPickMatrix(p.x, oldViewport[3]-p.y,5, 5, oldViewport);
	glMatrixMode(GL_MODELVIEW);

	unsigned int lastHover = currentScene->getLastHover();
	unsigned int hoverObject=currentScene->glSelect(false);

	//FIXME: Should be able to make this more efficient	
	shouldRedraw =  lastHover!=(unsigned int)-1;

	//Set the scene's hover value
	currentScene->setLastHover(hoverObject);
	currentScene->setHoverMode(hoverObject != (unsigned int)-1);

	//Restore the previous matirx
	glPopMatrix();

	//Restore the viewport
	int w, h;
	GetClientSize(&w, &h);
	glViewport(0, 0, (GLint) w, (GLint) h);

	return hoverObject;
}

void BasicGLPane::forceRedraw()
{
	//Force a paint update for the scene
	wxPaintEvent ptEvent;
	wxPostEvent(this,ptEvent);

#ifdef WIN32
	//Hack for windows. Does not redraw otherwise.
	// Refresh and Update in tandom dont work.
	Show(false);
	Show(true);
#endif

}

// some useful events to use
void BasicGLPane::mouseMoved(wxMouseEvent& event) 
{
	if (applyingDevice) return;
	enum
	{
		CAM_MOVE, //Movement of some kind
		CAM_TRANSLATE, //translate camera 
		CAM_PIVOT, //Pivot around view and across directions
		CAM_ROLL //Roll around view direction
	};


	if(selectionMode )
	{
		if(currentScene->isInteractionLocked())
		{
			event.Skip();
			return;
		}


		wxPoint p=event.GetPosition();
		
		unsigned int mouseFlags=0;
		unsigned int keyFlags=0;
		wxMouseState wxm = wxGetMouseState();
	
		if(wxm.CmdDown())
			keyFlags|=FLAG_CMD;
		if(wxm.ShiftDown())
			keyFlags|=FLAG_SHIFT;

		if(wxm.LeftIsDown())
		       	mouseFlags|= SELECT_BUTTON_LEFT;
		if(wxm.RightIsDown())
		       	mouseFlags|= SELECT_BUTTON_RIGHT;
		if(wxm.MiddleIsDown())
		       	mouseFlags|= SELECT_BUTTON_MIDDLE;
		
		//We can get  a mouse move event which reports no buttons before a mouse-up event,
		//this occurs frequently under windows, but sometimes under GTK
		if(!mouseFlags)
		{
			event.Skip();
			return;
		}
				
		int w, h;
		GetClientSize(&w, &h);


		currentScene->applyDevice((float)draggingStart.x/(float)w,
					(float)draggingStart.y/(float)h,
					 p.x/(float)w,p.y/(float)h,
					 keyFlags,mouseFlags,
					 false);

		lastMouseFlags=mouseFlags;
		lastKeyFlags=keyFlags;
		Refresh();
		return;
	}

	if(!dragging)
	{
		wxPoint p=event.GetPosition();

		//Do a hover test
		bool doRedraw=false;
		hoverTest(p,doRedraw);

		if(doRedraw)
			Refresh();

		return;
	}

	wxPoint draggingCurrent = event.GetPosition();

	//left-right and up-down move values
	float lrMove,udMove;	

	//Movement rate multiplier -- initialise to user value
	float camMultRate=mouseMoveFactor;
	if(event.m_shiftDown)
	{
		//Commit the current temp cam using the last camera rate
		//and then restart the motion.
		if(!lastMoveShiftDown && currentScene->haveTempCam())
			currentScene->commitTempCam();

		camMultRate*=5.0f;

		lastMoveShiftDown=true;

	}
	else
	{
		//Commit the current temp cam using the last camera rate
		//and then restart the motion.
 		if(lastMoveShiftDown && currentScene->haveTempCam())
			currentScene->commitTempCam();

		lastMoveShiftDown=false;
	}

	lrMove=CAMERA_MOVE_RATE*camMultRate*(draggingCurrent.x - draggingStart.x);
	udMove=CAMERA_MOVE_RATE*camMultRate*(draggingCurrent.y - draggingStart.y);

	lrMove*=2.0f*M_PI/180.0;
	udMove*=2.0f*M_PI/180.0;
	unsigned int camMode=0;
	//Decide camera movement mode
	bool translateMode;

    translateMode=event.CmdDown();

	bool swingMode;
	#if defined(WIN32) || defined(WIN64) || defined(__APPLE__)
		swingMode=wxGetKeyState(WXK_ALT);
	#else
		swingMode=wxGetKeyState(WXK_TAB);
	#endif

	if(translateMode && !swingMode)
		camMode=CAM_TRANSLATE;
	else if(swingMode && !translateMode)
		camMode=CAM_PIVOT;
	else if(swingMode && translateMode)
		camMode=CAM_ROLL;
	else
		camMode=CAM_MOVE;
	
	switch(camMode)
	{
		case CAM_TRANSLATE:
			currentScene->discardTempCam();
			currentScene->setTempCam();
			currentScene->getTempCam()->translate(lrMove,-udMove);
			break;
		case CAM_PIVOT:
			currentScene->discardTempCam();
			currentScene->setTempCam();
			currentScene->getTempCam()->pivot(lrMove,udMove);
			break;
		case CAM_MOVE:
			currentScene->setTempCam();
			currentScene->getTempCam()->move(lrMove,udMove);
			break;
		case CAM_ROLL:
			currentScene->setTempCam();
			currentScene->getTempCam()->roll(atan2(udMove,lrMove));
						
			break;	
		default:
			ASSERT(false);
			break;
	}

	if(!event.m_leftDown)
	{
		dragging=false;
		currentScene->commitTempCam();
	}
	
	haveCameraUpdates=true;

	Refresh(false);
}

void BasicGLPane::mouseDown(wxMouseEvent& event) 
{

	wxPoint p=event.GetPosition();

	//Do not re-trigger if dragging or doing a scene update.
	//This can cause a selection test to occur whilst
	//a temp cam is activated in the scene, or a binding refresh is underway,
	//which is currently considered bad
	if(!dragging && !applyingDevice && !selectionMode 
			&& !currentScene->isInteractionLocked())
	{
		//Check to see if the user has clicked an object in the scene
		bool redraw;
		selectionTest(p,redraw);


		//If the selected object is valid, then
		//we did select an object. Treat this as a selection event
		if(currentScene->getLastSelected() != (unsigned int)-1)
		{
			selectionMode=true;
			currentScene->setSelectionMode(true);
		}
		else
		{
			//we aren't setting, it  -- it shouldn't be the case
			ASSERT(selectionMode==false);

			//Prevent right button from triggering camera drag
			if(!event.LeftDown())
			{
				event.Skip();
				return;
			}

			//If not a valid selection, this is a camera drag.
			dragging=true;
		}

		draggingStart = event.GetPosition();
		//Set keyboard focus to self, to receive key events
		SetFocus();

		if(redraw)
			Refresh();
	}

}

void BasicGLPane::mouseWheelMoved(wxMouseEvent& event) 
{
	const float SHIFT_MULTIPLIER=5;

	float cameraMoveRate=-(float)event.GetWheelRotation()/(float)event.GetWheelDelta();

	cameraMoveRate*=mouseZoomFactor;

	if(event.ShiftDown())
		cameraMoveRate*=SHIFT_MULTIPLIER;

	cameraMoveRate*=CAMERA_SCROLL_RATE;
	//Move by specified delta
	currentScene->getActiveCam()->forwardsDolly(cameraMoveRate);

	//if we are using a temporary camera, update that too
	if(currentScene->haveTempCam())
		currentScene->getTempCam()->forwardsDolly(cameraMoveRate);

	haveCameraUpdates=true;
	Refresh();
}

void BasicGLPane::mouseReleased(wxMouseEvent& event) 
{
	if(currentScene->isInteractionLocked())
	{
		event.Skip();
		return;
	}
		
	if(selectionMode )
	{
		//If user releases all buttons, then allow the up
		if(!event.LeftIsDown() && 
				!event.RightIsDown() && !event.MiddleIsDown())
		{
			wxPoint p=event.GetPosition();
			
			int w, h;
			GetClientSize(&w, &h);
			applyingDevice=true;


			currentScene->applyDevice((float)draggingStart.x/(float)w,
						(float)draggingStart.y/(float)h,
						 p.x/(float)w,p.y/(float)h,
						 lastKeyFlags,lastMouseFlags,
						 true);
			
			applyingDevice=false;


			selectionMode=false;
			currentScene->setSelectionMode(selectionMode);

			Refresh();
		}
		event.Skip();
		return;
	}
	

	if(currentScene->haveTempCam())
		currentScene->commitTempCam();
	currentScene->finaliseCam();

	haveCameraUpdates=true;
	dragging=false;

	Refresh();
	
}

void BasicGLPane::rightClick(wxMouseEvent& event) 
{
}

void BasicGLPane::mouseLeftWindow(wxMouseEvent& event) 
{
	if(selectionMode)
	{
		wxPoint p=event.GetPosition();
		
		int w, h;
		GetClientSize(&w, &h);

		applyingDevice=true;
		currentScene->applyDevice((float)draggingStart.x/(float)w,
					(float)draggingStart.y/(float)h,
					 p.x/(float)w,p.y/(float)h,
					 lastKeyFlags,lastMouseFlags,
					 true);

		selectionMode=false;
		currentScene->setSelectionMode(selectionMode);
		Refresh();
		applyingDevice=false;

		event.Skip();
		return;

	}

	if(event.m_leftDown)
	{
		if(currentScene->haveTempCam())
		{
			currentScene->commitTempCam();
			dragging=false;
		}
	}
}

void BasicGLPane::keyPressed(wxKeyEvent& event) 
{
	
	switch(event.GetKeyCode())
	{
		case WXK_SPACE:
		{
			unsigned int visibleDir;
	
			//Use modifier keys to alter the direction of visibility
			//First compute the part of the keymask that does not
			//reflect the double tap
            // needs to be control in apple as cmd-space open spotlight
			unsigned int keyMask;
#ifdef __APPLE__
			keyMask = (event.RawControlDown() ? 1 : 0);
#else
			keyMask = (event.CmdDown() ? 1 : 0);
#endif
			keyMask |= (event.ShiftDown() ?  2 : 0);

			//Now determine if we are the same mask as last time
			bool isKeyDoubleTap=(lastKeyDoubleTap==keyMask);
			//double tapping allows for selection of reverse direction
			keyMask |= ( isKeyDoubleTap ?  4 : 0);
			
			visibleDir=-1;

			//Hardwire key combo->Mapping
			switch(keyMask)
			{
				//Space only
				case 0: 
					visibleDir=3; 
					break;
				//Command down +space 
				case 1:
					visibleDir=0;
					break;
				//Shift +space 
				case 2:
					visibleDir=2;
					break;
				//NO CASE 3	
				//Double+space
				case 4:
					visibleDir=5; 
					break;
				//Doublespace+Cmd
				case 5:
					visibleDir=4;
					break;

				//Space+Double+shift
				case 6:
					visibleDir=1;
					break;
				default:
					;
			}

			if(visibleDir!=(unsigned int)-1)
			{

				if(isKeyDoubleTap)
				{
					//It was a double tap. Reset the tapping and stop the timer
					lastKeyDoubleTap=(unsigned int)-1;
					keyDoubleTapTimer->Stop();
				}
				else
				{
					lastKeyDoubleTap=keyMask & (~(0x04));
					keyDoubleTapTimer->Start(DOUBLE_TAP_DELAY,wxTIMER_ONE_SHOT);
				}

				
				currentScene->ensureVisible(visibleDir);
				parentStatusBar->SetStatusText(TRANS("Use shift/ctrl-space or double tap to alter reset axis"));
				parentStatusBar->SetBackgroundColour(*wxCYAN)
					;
				parentStatusTimer->Start(statusDelay,wxTIMER_ONE_SHOT);
				Refresh();
				haveCameraUpdates=true;
			}
		}
		break;
		default:
			event.Skip(true);
	}
}

void BasicGLPane::setGlClearColour(float r, float g, float b)
{
	ASSERT(r >= 0.0f && r <= 1.0f);
	ASSERT(g >= 0.0f && g <= 1.0f);
	ASSERT(b >= 0.0f && b <= 1.0f);
	
	currentScene->setBackgroundColour(r,g,b);
	
	Refresh();
}

void BasicGLPane::keyReleased(wxKeyEvent& event) 
{

	float cameraMoveRate=CAMERA_KEYBOARD_SCROLL_RATE;

	if(event.ShiftDown())
		cameraMoveRate*=5;

	bool update=true;
	switch(event.GetKeyCode())
	{
		case '-':
		case '_':
		case WXK_NUMPAD_SUBTRACT:
		case WXK_SUBTRACT:
		{
			//Do a backwards dolly by fixed amount
			currentScene->getActiveCam()->forwardsDolly(cameraMoveRate);
			if(currentScene->haveTempCam())
				currentScene->getTempCam()->forwardsDolly(cameraMoveRate);
			break;
		}
		case '+':
		case '=':
		case WXK_NUMPAD_ADD:
		case WXK_ADD:
		case WXK_NUMPAD_EQUAL:
		{
			//Reverse direction of motion
			cameraMoveRate= -cameraMoveRate;
			
			//Do a forwards dolly by fixed amount
			currentScene->getActiveCam()->forwardsDolly(cameraMoveRate);
			if(currentScene->haveTempCam())
				currentScene->getTempCam()->forwardsDolly(cameraMoveRate);
			break;
		}
		default:
			event.Skip(true);
			update=false;
	}

	if(update)
		Refresh();
}

 
void BasicGLPane::resized(wxSizeEvent& evt)
{
	prepare3DViewport(0,0,getWidth(),getHeight()); 
	wxClientDC *dc=new wxClientDC(this);
	Refresh();

#ifdef __APPLE__
	requireContextUpdate=true;
#endif
	delete dc;
}
 
bool BasicGLPane::prepare3DViewport(int tlx, int tly, int brx, int bry)
{

	if(!paneInitialised)
		return false;

	//Prevent NaN.
	if(!(bry-tly))
		return false;
	GLint dims[2]; 
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims); 

	//Ensure that the openGL function didn't tell us porkies
	//but double check for the non-debug builds next line
	ASSERT(dims[0] && dims[1]);

	//check for exceeding max viewport and we have some space
	if(dims[0] <(brx-tlx) || dims[1] < bry-tly || 
			(!dims[0] || !dims[1] ))
		return false; 

	glViewport( tlx, tly, brx-tlx, bry-tly);

	float aspect = (float)(brx-tlx)/(float)(bry-tly);
	currentScene->setWinSize(brx-tlx,bry-tly);
	currentScene->setAspect(aspect);

	//Set modelview and projection matrices to the identity
	// matrix
	{
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	}	

	{
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	}
	
	return true;
}
 
int BasicGLPane::getWidth()
{
    return GetClientSize().x;
}
 
int BasicGLPane::getHeight()
{
    return GetClientSize().y;
}

void BasicGLPane::render( wxPaintEvent& evt )
{
	//Prevent calls to openGL if pane not visible
	if (!IsShown()) 
		return;
	
	if(!context)
	{
		context = new wxGLContext(this);
		SetCurrent(*context);
#ifdef __APPLE__
		requireContextUpdate=false;
#endif	
	}

	if(!paneInitialised)
	{
		paneInitialised=true;
		prepare3DViewport(0,0,getWidth(),getHeight()); 
	}

//Apple requires a context update on each resize, for some reason
// https://developer.apple.com/library/mac/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_contexts/opengl_contexts.html
//and
// https://forums.wxwidgets.org/viewtopic.php?f=23&t=41592&p=168346
#ifdef __APPLE__
	if(requireContextUpdate)
	{
		SetCurrent(*context);
		prepare3DViewport(0,0,getWidth(),getHeight()); 
		requireContextUpdate=false;
	}
#endif

	wxPaintDC(this); 
	currentScene->draw();
	glFlush();
	SwapBuffers();
}

void BasicGLPane::OnEraseBackground(wxEraseEvent &evt)
{
	//Do nothing. This is to help eliminate flicker apparently
}

void BasicGLPane::updateClearColour()
{
	float rClear,gClear,bClear;
	currentScene->getBackgroundColour(rClear,gClear,bClear);
	//Can't set the opengl window without a proper context
	ASSERT(paneInitialised);
	setGlClearColour(rClear,gClear,bClear);
	//Let openGL know that we have changed the colour.
	glClearColor( rClear, gClear, 
				bClear,1.0f);
}

TRcontext *BasicGLPane::generateTileContext(unsigned int width, unsigned int height, unsigned char *imageBuffer, bool alpha) const
{
	int panelWidth,panelHeight;
	GetClientSize(&panelWidth,&panelHeight);
	
	//Create TR library tile context
	TRcontext *tr = trNew();
	//Tile size
	trTileSize(tr,panelWidth,panelHeight,0);
	//Set overall image size
	trImageSize(tr, width, height);
	//Set buffer for overall image
	if(alpha)
		trImageBuffer(tr, GL_RGBA, GL_UNSIGNED_BYTE, imageBuffer);
	else
		trImageBuffer(tr, GL_RGB, GL_UNSIGNED_BYTE, imageBuffer);
	//Set the row order for the image
	trRowOrder(tr, TR_BOTTOM_TO_TOP);

	return tr;
}

bool BasicGLPane::saveImage(unsigned int width, unsigned int height,
		const char *filename, bool showProgress, bool needPostPaint)
{
	GLint dims[2]; 
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims); 

	//Opengl should not be giving us zero dimensions here.
	// if it does, just abandon saving the image as a fallback
	ASSERT(dims[0] && dims[1]);
	if(!dims[0] || !dims[1])
		return false;
	
	//create new image
	wxImage *image = new wxImage(width,height);


	unsigned char *imageBuffer= (unsigned char*) malloc(3*(width)*height);
	if(!imageBuffer)
		return false;


	glLoadIdentity();
	const Camera *cm = currentScene->getActiveCam();

	//We cannot seem to draw outside the current viewport.
	//in a cross platform manner.
	//fall back to stitching the image together by hand

	//Initialise tile data
	TRcontext *tr; 
	//Inform the tiling system about our camera config
	float aspect=currentScene->getAspect();
	
	{
	float farPlane; 
	tr=generateTileContext(width,height, imageBuffer);
	BoundCube bc = currentScene->getBound();
	farPlane = 1.5*bc.getMaxDistanceToBox(cm->getOrigin());
	
	if(cm->getProjectionMode() == PROJECTION_MODE_PERSPECTIVE)
	{
		if(cm->type() == CAM_LOOKAT)
		{
			const CameraLookAt *cl =(const CameraLookAt*) currentScene->getActiveCam();
			trPerspective(tr,cl->getFOV()/2.0,currentScene->getAspect(),
							cl->getNearPlane(),farPlane);
		}
		else
		{
			//At this time there are no cameras of this type
			ASSERT(false);
		}
	}
	else
	{
		float orthoScale = cm->getOrthoScale();
		trOrtho(tr,-orthoScale*aspect,orthoScale*aspect,
				-orthoScale,orthoScale,0.0f,farPlane);

	}
	}

	//Obtain tile count from the renderer & init progress
	//--
	unsigned int totalTiles;
	{
	unsigned int nRow,nCol,nPass;
	nRow=trGet(tr,TR_ROWS);
	nCol=trGet(tr,TR_COLUMNS);
	if(currentScene->hasOverlays())
		nPass = 2;
	else
		nPass=1;

	totalTiles=nRow*nCol*nPass;
	}

	wxProgressDialog *wxD=0;	
	showProgress=showProgress && ( totalTiles > 1);
	if(showProgress)
	{
		wxD = new wxProgressDialog(TRANS("Image progress"), 
					TRANS("Rendering tiles..."), totalTiles);

	}
	//--


	//We have to do two passes. First we have to
	// do a 3D pass, then we have to separately
	// draw the overlays.

	// As we have 2 cameras, one for the normal scene
	// and one for the overlay, we build the images,
	// then merge the images, rather than trying to composite the entire scene in situ.

	//PASS 1:
	//--------------	

	//HACK: Flip the all but scene's light z coordinate
	// for some reason, the frustum has an inversion
	// somewhere in the coordinate system, and I can't find it!
	// inverting the tile frustum ends up with the depth test 
	// also inverting.
	const bool FLIP_LIGHT_HACK=true;
	//x,y,z and w axis.
	const bool IMPORTANT_AXIS[4]={true,false,true,false};

	//opengl light has 4 
	float oldLightPos[4];
	if(FLIP_LIGHT_HACK)
	{
		currentScene->getLightPos(oldLightPos);
		float newLightPos[4];
		for(size_t ui=0;ui<4;ui++)
		{
			if(IMPORTANT_AXIS[ui])
				newLightPos[ui]=oldLightPos[ui];
			else
				newLightPos[ui]=-oldLightPos[ui];
		
		}
		currentScene->setLightPos(newLightPos);
	}
	
	if(showProgress)
		wxD->Show();

	//Loop through the tiles/ 
	// note that 2D overlays will not be drawn in this pass
	unsigned int thisTileNum=0;
	int haveMoreTiles=1;
	while(haveMoreTiles)
	{
		thisTileNum++;

		//Manually set the camera
		//--
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		
		if(cm->type() == CAM_LOOKAT)
			((const CameraLookAt *)cm)->lookAt();
		//--
	
		//Start the tile
		trBeginTile(tr);
		currentScene->draw(true);

		glPopMatrix();

		//ending the tile copies
		// data 
		haveMoreTiles=trEndTile(tr);
	
		if(showProgress)	
			wxD->Update(thisTileNum);

	}

	if(FLIP_LIGHT_HACK)
	{
		//re-set light coordinates
		currentScene->setLightPos(oldLightPos);
	}

	trDelete(tr);

	//Transfer pointer to image, which will perform free-ing of the buffer
	image->SetData(imageBuffer);
	//HACK : Tiling function returns upside-down image. Fix in post-process
	// argument is to set mirror axis such that x axis is unchanged
	*image=image->Mirror(false);
	//--------------	

	//PASS 2
	//--------------	

	if(currentScene->hasOverlays())
	{
		//alllocate RGBA (4-channel) image
		imageBuffer= (unsigned char*) malloc(4*(width)*height);
		if(!imageBuffer)
			return false;


		tr=generateTileContext(width,height,imageBuffer,true);
		trOrtho(tr,0.0f,aspect,
				0.0f,1.0f,-1.0f,1.0f);
		

		haveMoreTiles=1;

	
		float rClear,gClear,bClear;
		currentScene->getBackgroundColour(rClear,gClear,bClear);
		glClearColor( rClear, gClear, 
					bClear,0.0f);

		//I am unclear why, but the faces are reversed
		glDisable(GL_CULL_FACE);
		while(haveMoreTiles)
		{
			thisTileNum++;
			//Start the tile
			trBeginTile(tr);
			glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
			currentScene->drawOverlays(true);
			//ending the tile copies
			// data 
			haveMoreTiles=trEndTile(tr);
		
			if(showProgress)	
				wxD->Update(thisTileNum);
		}	
		glEnable(GL_CULL_FACE);
		//restore the GL clear colour
		updateClearColour();

		///unpack the tile buffer into a wx image
		wxImage imageOverlay(width,height);
		imageOverlay.InitAlpha();

		//FIXME: HACK - using "blue screen" effect
		//don't use background as mask colour.
		// use depth buffer or gl alpha
		unsigned char clear[3];
		clear[0] = (unsigned char)rClear*255.0f;
		clear[1] = (unsigned char)gClear*255.0f;
		clear[2] = (unsigned char)bClear*255.0f;
		const unsigned char *mask = clear;
		copyRGBAtoWXImage(width,height,imageBuffer,imageOverlay,mask);

		free(imageBuffer);

		combineWxImage(*image,imageOverlay);

		//Free the tile buffer
		trDelete(tr);
	}

	
	//--------------	
	bool isOK=image->SaveFile(filename,wxBITMAP_TYPE_PNG);
	

	if(showProgress)
		wxD->Destroy();
	
	delete image;

	if (needPostPaint) {
		wxPaintEvent event;
		wxPostEvent(this,event);
	}

	return isOK;
}

void BasicGLPane::OnAxisTapTimer(wxTimerEvent &evt)
{
	lastKeyDoubleTap=(unsigned int)-1;
}


bool BasicGLPane::saveImageSequence(unsigned int resX, unsigned int resY, unsigned int nFrames,
		wxString &path,wxString &prefix, wxString &ext)
{
	//OK, lets animate!
	//


	ASSERT(!currentScene->haveTempCam());
	std::string outFile;
	wxProgressDialog *wxD = new wxProgressDialog(TRANS("Animation progress"), 
					TRANS("Rendering sequence..."), nFrames,this,wxPD_CAN_ABORT|wxPD_APP_MODAL );

	wxD->Show();
	std::string tmpStr,tmpStrTwo;
	stream_cast(tmpStrTwo,nFrames);

	Camera *origCam=currentScene->getActiveCam()->clone();
	
	
	for(unsigned int ui=0;ui<nFrames;ui++)
	{
		std::string digitStr;

		//Create a string like 00001, such that there are always leading zeros
		digitStr=digitString(ui,nFrames);

		//Manipulate the camera such that it orbits around its current axis
		//FIXME: Why is this M_PI, not 2*M_PI???
		float angle;
		angle= (float)ui/(float)nFrames*M_PI;

		Camera *modifiedCam;
		modifiedCam=origCam->clone();
		modifiedCam->move(angle,0);
		currentScene->setActiveCam(modifiedCam);

		//Save the result
		outFile = string(stlStr(path))+ string("/") + 
				string(stlStr(prefix))+digitStr+ string(".") + string(stlStr(ext));
		if(!saveImage(resX,resY,outFile.c_str(),false, false))
		{
			currentScene->setActiveCam(origCam);
			return false;
		}

		//Update the progress bar
		stream_cast(tmpStr,ui+1);
		//Tell user which image from the animation we are saving
		tmpStr = std::string(TRANS("Saving Image ")) + tmpStr + std::string(TRANS(" of ")) + tmpStrTwo + "...";
		if(!wxD->Update(ui,tmpStr))
			break;

		Refresh();
	}

	currentScene->setActiveCam(origCam);

	//Discard the current temp. cam to return the scene back to normal
	currentScene->discardTempCam();
	wxD->Destroy();
	
	wxPaintEvent event;
	wxPostEvent(this,event);
	return true;
		
}
