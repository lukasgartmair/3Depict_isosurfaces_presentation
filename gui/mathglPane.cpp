/*
 *	mathglPane.cpp - mathgl-wx interface panel control
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

#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include "wx/wxcomponents.h"

#include "mathglPane.h"

#include "wx/wxcommon.h"
#include "common/translation.h"
#include "backend/plot.h"

#ifndef pow10
#define pow10(x) pow(10,x)
#endif

#include <mgl2/canvas_wnd.h>

using std::string;
using std::vector;

//Panning speed modifier
const float MGL_PAN_SPEED=2.0f;
//Mathgl uses floating point loop computation, and can get stuck. Limit zoom precision
const float MGL_ZOOM_LIMIT=10.0f*sqrt(std::numeric_limits<float>::epsilon());


//Mouse action types
enum
{
	MOUSE_MODE_DRAG, //Free mouse drag on plot
	MOUSE_MODE_DRAG_PAN, //dragging mouse using a "panning" action
	MOUSE_MODE_DRAG_REGION, //Dragging a region
	MOUSE_MODE_ENUM_END
};

//Do the particular enums require a redraw?
const bool MOUSE_ACTION_NEEDS_REDRAW[] = { false,true,true,false};

enum
{
	PLOT_TEXTURE_ZOOM_X,
	PLOT_TEXTURE_ZOOM_Y,
	PLOT_TEXTURE_ZOOM_RESET,
	PLOT_TEXTURE_SLIDE_X,
	PLOT_TEXTURE_ENUM_END
};

const char *mglTextureFile[] = { "textures/plot_zoom_x.png",
				"textures/plot_zoom_y.png",
				"textures/plot_zoom_reset.png",
				"textures/plot_slide_x.png"
				};

using std::ifstream;
using std::ios;

BEGIN_EVENT_TABLE(MathGLPane, wxPanel)
	EVT_MOTION(MathGLPane::mouseMoved)
	EVT_LEFT_DOWN(MathGLPane::leftMouseDown)
	EVT_LEFT_UP(MathGLPane::leftMouseReleased)
	EVT_MIDDLE_DOWN(MathGLPane::middleMouseDown)
	EVT_MIDDLE_UP(MathGLPane::middleMouseReleased)
	EVT_RIGHT_DOWN(MathGLPane::rightClick)
	EVT_LEAVE_WINDOW(MathGLPane::mouseLeftWindow)
	EVT_LEFT_DCLICK(MathGLPane::mouseDoubleLeftClick) 
	EVT_MIDDLE_DCLICK(MathGLPane::mouseDoubleLeftClick) 
	EVT_SIZE(MathGLPane::resized)
	EVT_KEY_DOWN(MathGLPane::keyPressed)
	EVT_KEY_UP(MathGLPane::keyReleased)
	EVT_MOUSEWHEEL(MathGLPane::mouseWheelMoved)
	EVT_PAINT(MathGLPane::render)
END_EVENT_TABLE();


enum
{
	AXIS_POSITION_INTERIOR=1,
	AXIS_POSITION_LOW_X=2,
	AXIS_POSITION_LOW_Y=4,
};


void zoomBounds(float minV,float maxV,  float centre, 
		float zoomFactor,float &newMin, float &newMax)
{
	ASSERT(minV < maxV);
	ASSERT(minV< centre && maxV > centre);
	ASSERT(zoomFactor > 0);
	
	//find deltas, then multiply them out
	float lowerDelta,upperDelta;
	lowerDelta = (centre-minV);
	upperDelta = (maxV-centre);
	upperDelta*=zoomFactor;
	lowerDelta*=zoomFactor;
	ASSERT(upperDelta > 0 && lowerDelta > 0);

	//compute new bounds
	newMin= centre - lowerDelta;
	newMax= centre + upperDelta;

	ASSERT(newMin <=newMax);
}

MathGLPane::MathGLPane(wxWindow* parent, int id) :
wxPanel(parent, id,  wxDefaultPosition, wxDefaultSize)
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(MOUSE_ACTION_NEEDS_REDRAW) == MOUSE_MODE_ENUM_END + 1);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(mglTextureFile) == PLOT_TEXTURE_ENUM_END);

	hasResized=true;
	limitInteract=false;
	mouseDragMode=MOUSE_MODE_ENUM_END;	
	leftWindow=true;
	thePlot=0;	
	gr=0;
	lastEditedPlot=lastEditedRegion=-1;
	regionSelfUpdate=false;
	plotIsLogarithmic=false;

	SetBackgroundStyle(wxBG_STYLE_CUSTOM);

}

MathGLPane::~MathGLPane()
{
	if(gr)
		delete gr;
}


void MathGLPane::setPanCoords() const
{
	float xMin,xMax,yMin,yMax;
	thePlot->getBounds(xMin,xMax,yMin,yMax);

	float pEndX, pStartX,dummy;
	toPlotCoords(draggingCurrent.x,draggingCurrent.y,pEndX,dummy);
	toPlotCoords(draggingStart.x,draggingStart.y,pStartX,dummy);
	
	float offX = pEndX-pStartX;

	//This is not needed if re-using mgl object!
	// - not sure why!
	//offX*=xMax-xMin;

	//Modify for speed
	offX*=MGL_PAN_SPEED;

	thePlot->setBounds(origPanMinX+offX/2,+origPanMaxX + offX/2.0,
				yMin,yMax);
}

bool MathGLPane::readyForInput() const
{
	return (thePlot && gr && 
		!thePlot->isInteractionLocked()  && thePlot->getNumTotal());
}

unsigned int MathGLPane::getAxisMask(int x, int y) const
{

	//Retrieve XY position in graph coordinate space
	// from XY coordinate in window space.
	float mglCurX, mglCurY;
	if(!toPlotCoords(x,y,mglCurX,mglCurY))
		return 0;

	unsigned int retVal=0;

	if(mglCurX < gr->Self()->GetOrgX('x'))
		retVal |=AXIS_POSITION_LOW_X;

	if(mglCurY < gr->Self()->GetOrgY('y'))
		retVal |=AXIS_POSITION_LOW_Y;

	if(!retVal)
		retVal=AXIS_POSITION_INTERIOR;

	return retVal;
}

void MathGLPane::setPlotWrapper(PlotWrapper *newPlot,bool takeOwnPtr)
{
	thePlot=newPlot;

	Refresh();
}



void MathGLPane::render(wxPaintEvent &event)
{
	
	wxAutoBufferedPaintDC   *dc=new wxAutoBufferedPaintDC(this);

	if(!thePlot || thePlot->isInteractionLocked() )
	{
		delete dc;
		return;
	}

	bool hasChanged;
	hasChanged=thePlot->hasChanged();
	int w,h;
	w=0;h=0;

	GetClientSize(&w,&h);
	
	if(!w || !h)
	{
		delete dc;
		return;
	}


	//Set the enabled and disabled plots
	unsigned int nItems=thePlot->getNumVisible();
	

	wxFont font;
	font.SetFamily(wxFONTFAMILY_SWISS);
	if(font.IsOk())
		dc->SetFont(font);
	
	if(!nItems)
	{
#ifdef __WXGTK__
		wxBrush *b = new wxBrush;
		b->SetColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BACKGROUND));
		dc->SetBackground(*b);

		dc->SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#endif
#if !defined(__APPLE__) 
		dc->Clear();
#endif

		int clientW,clientH;
		GetClientSize(&clientW,&clientH);
		
		wxString str=TRANS("No plots selected.");
		dc->GetMultiLineTextExtent(str,&w,&h);
		dc->DrawText(str,(clientW-w)/2, (clientH-h)/2);

#ifdef __WXGTK__
		delete b;
#endif
		delete dc;

		return;

	}

#ifdef DEBUG
	bool doTrap=getTrapfpe();
	if(doTrap)
		trapfpe(false);
#endif
	//If the plot has changed, been resized or is performing
	// a mouse action that requires updating, we need to update it
	//likewise if we don't have a plot, we need one.
	if(!gr || hasChanged || hasResized || 
		MOUSE_ACTION_NEEDS_REDRAW[mouseDragMode])
	{
		//clear the plot drawing entity
		if(!gr)
		{
			gr = new mglGraph(0,w,h);
#ifdef __APPLE__
			//apparenty bug in mgl under osx - font wont load,
			// use random string to force fallback
			gr->LoadFont("asdfrandom");
#endif
		}
		else
		{
			gr->SetSize(w,h);
		}
		
		//change the plot by panningOneD it before we draw.
		//if we need to 
		if(mouseDragMode==MOUSE_MODE_DRAG_PAN)
			setPanCoords();

		//Draw the plot
		thePlot->drawPlot(gr,plotIsLogarithmic);
#ifdef DEBUG
		if(strlen(gr->Message()))
		{
			std::cerr << "Mathgl reports error:" << gr->Message() << std::endl;
		}
#endif
		thePlot->resetChange();
		hasResized=false;

		//Copy the plot's memory buffer into a wxImage object, then draw it	
		char *rgbdata = (char*)malloc(w*h*3);
		gr->GetRGB((char*)rgbdata,w*h*3);
		
		imageCacheBmp=wxBitmap(wxImage(w,h,(unsigned char*)rgbdata,true));
		free(rgbdata);
	}

#ifdef DEBUG
	if(doTrap)
		trapfpe(true);
#endif
	dc->DrawBitmap(wxBitmap(imageCacheBmp),0,0);
	//If we are engaged in a dragging operation
	//draw the nice little bits we need
	switch(mouseDragMode)
	{
		case MOUSE_MODE_DRAG:
		{
			//Draw a rectangle between the start and end positions
			wxCoord tlX,tlY,wRect,hRect;

			if(draggingStart.x < draggingCurrent.x)
			{
				tlX=draggingStart.x;
				wRect = draggingCurrent.x - tlX;
			}
			else
			{
				tlX=draggingCurrent.x;
				wRect = draggingStart.x - tlX;
			}

			if(draggingStart.y < draggingCurrent.y)
			{
				tlY=draggingStart.y;
				hRect = draggingCurrent.y - tlY;
			}
			else
			{
				tlY=draggingCurrent.y;
				hRect = draggingStart.y - tlY;
			}

#if wxCHECK_VERSION(3,1,0)
			dc->SetBrush(wxBrush(*wxBLUE,wxBRUSHSTYLE_TRANSPARENT));	
#else
			dc->SetBrush(wxBrush(*wxBLUE,wxTRANSPARENT));	
#endif
			const int END_MARKER_SIZE=5;

			//If the cursor is wholly below
			//the axis, draw a line rather than abox

			unsigned int startMask, endMask;
			startMask=getAxisMask(draggingStart.x, draggingStart.y);
			endMask=getAxisMask(draggingCurrent.x, draggingCurrent.y);

			if( (startMask & AXIS_POSITION_LOW_X)
				&& (endMask & AXIS_POSITION_LOW_X) )
			{
				if( !((startMask &AXIS_POSITION_LOW_Y) && (endMask & AXIS_POSITION_LOW_Y)))
				{
					//left of X-Axis event
					//Draw a little I beam.
					dc->DrawLine(draggingStart.x,tlY,
							draggingStart.x,tlY+hRect);
					dc->DrawLine(draggingStart.x-END_MARKER_SIZE,tlY+hRect,
							draggingStart.x+END_MARKER_SIZE,tlY+hRect);
					dc->DrawLine(draggingStart.x-END_MARKER_SIZE,tlY,
							draggingStart.x+END_MARKER_SIZE,tlY);
				}
			}
			else if( (startMask & AXIS_POSITION_LOW_Y)
				&& (endMask & AXIS_POSITION_LOW_Y) )
			{
				//below Y axis event
				//Draw a little |-| beam.
				dc->DrawLine(tlX,draggingStart.y,
						tlX+wRect,draggingStart.y);
				dc->DrawLine(tlX+wRect,draggingStart.y-END_MARKER_SIZE,
						tlX+wRect,draggingStart.y+END_MARKER_SIZE);
				dc->DrawLine(tlX,draggingStart.y-END_MARKER_SIZE,
						tlX,draggingStart.y+END_MARKER_SIZE);

			}
			else
				dc->DrawRectangle(tlX,tlY,wRect,hRect);
		
			break;
		}    
		case MOUSE_MODE_DRAG_REGION:	
		{
			drawRegionDraggingOverlay(dc);
			break;
		}
		case MOUSE_MODE_DRAG_PAN:

			break;
		case MOUSE_MODE_ENUM_END:
		{
			drawInteractOverlay(dc);
			break;
		}
		default:
			ASSERT(false);
	}
	
	delete dc;
}

void MathGLPane::resized(wxSizeEvent& evt)
{
	hasResized=true;
	Refresh();
}


void MathGLPane::updateMouseCursor()
{
	int w,h;
	w=0;h=0;

	GetClientSize(&w,&h);
	
	if(!w || !h || !thePlot )
		return;

	//Set cursor to normal by default
	if(!readyForInput())
	{
		SetCursor(wxNullCursor);
		return;
	}

	//Update mouse cursor
	//---------------
	//Draw a rectangle between the start and end positions

	//If we are using shift, we slide along X axis anyway
	if(wxGetKeyState(WXK_SHIFT))
		SetCursor(wxCURSOR_SIZEWE);
	else
	{

		//If the cursor is wholly beloe
		//the axis, draw a line rather than abox
		unsigned int axisMask=getAxisMask(curMouse.x,curMouse.y);

		float xMin,xMax,yMin,yMax;

		thePlot->getBounds(xMin,xMax,yMin,yMax);
		//Look at mouse position relative to the axis position
		//to determine the cursor style.
		switch(axisMask)
			{	
			case AXIS_POSITION_LOW_X:
				//left of X-Axis event, draw up-down arrow
				SetCursor(wxCURSOR_SIZENS);
				break;
			case AXIS_POSITION_LOW_Y:
				//Below Y axis, draw line // to x axis
			SetCursor(wxCURSOR_SIZEWE);
				break;
			case AXIS_POSITION_INTERIOR:
				//SetCursor(wxCURSOR_MAGNIFIER);
				SetCursor(wxNullCursor);
				break;
			default:
				SetCursor(wxNullCursor);
				;
		}
	}
	//---------------
}

bool MathGLPane::getRegionUnderCursor(const wxPoint  &mousePos, unsigned int &plotId,
								unsigned int &regionId) const
{
	ASSERT(gr);

	//Convert the mouse coordinates to data coordinates.
	float xM,yM;
	toPlotCoords(mousePos.x,mousePos.y,xM,yM);
	mglPoint pMouse(xM,yM); 



	if(!readyForInput())
		return false;

	//Only allow  range interaction within the plot bb
	if(pMouse.x > gr->Self()->Max.x || pMouse.x < gr->Self()->Min.x)
		return false;
	
	//check if we actually have a region
	if(!thePlot->getRegionIdAtPosition(pMouse.x,pMouse.y,plotId,regionId))
		return false;
	
	return true;
}


void MathGLPane::mouseMoved(wxMouseEvent& event)
{
	leftWindow=false;
	if(!readyForInput())
	{
		mouseDragMode=MOUSE_MODE_ENUM_END;
		return;
	}

	curMouse=event.GetPosition();	
	
	switch(mouseDragMode)
	{
		case MOUSE_MODE_DRAG:
			if(!event.m_leftDown)
				mouseDragMode=MOUSE_MODE_ENUM_END;
			else
				draggingCurrent=event.GetPosition();
			break;
		case MOUSE_MODE_DRAG_PAN:
			//Can only be dragging with shift/left or middle down
			// we might not receive an left up if the user
			// exits the window and then releases the mouse
			if(!((event.m_leftDown && event.m_shiftDown) || event.m_middleDown))
				mouseDragMode=MOUSE_MODE_ENUM_END;
			else
				draggingCurrent=event.GetPosition();
			break;
		default:
			;
	}
	//Check if we are still dragging
	if(!(event.m_leftDown || event.m_middleDown) || limitInteract)
		mouseDragMode=MOUSE_MODE_ENUM_END;
	else
		draggingCurrent=event.GetPosition();


	updateMouseCursor();

	Refresh();

}

void MathGLPane::mouseDoubleLeftClick(wxMouseEvent& event)
{
	if(!readyForInput())
		return;

	//Cancel any mouse drag mode
	mouseDragMode=MOUSE_MODE_ENUM_END;
	
	int w,h;
	w=0;h=0;
	GetClientSize(&w,&h);
	
	if(!w || !h )
		return;
	
	unsigned int axisMask=getAxisMask(curMouse.x,curMouse.y);

	switch(axisMask)
	{	
		case AXIS_POSITION_LOW_X:
			//left of X-Axis -- plot Y zoom
			thePlot->disableUserAxisBounds(false);	
			break;
		case AXIS_POSITION_LOW_Y:
			//Below Y axis; plot X Zoom
			thePlot->disableUserAxisBounds(true);	
			break;
		case AXIS_POSITION_INTERIOR:
			//reset plot bounds
			thePlot->disableUserBounds();	
			break;
		default:
			//bottom corner
			thePlot->disableUserBounds();	
	}

	Refresh();
}

void MathGLPane::mouseDoubleMiddleClick(wxMouseEvent &event)
{
	mouseDoubleLeftClick(event);
}


void MathGLPane::oneDMouseDownAction(bool leftDown,bool middleDown,
		 bool alternateDown, int dragX,int dragY)
{
	ASSERT(thePlot->getNumVisible());
	
	float xMin,xMax,yMin,yMax;
	thePlot->getBounds(xMin,xMax,yMin,yMax);

	//Set the interaction mode
	if(leftDown && !alternateDown )
	{
		int axisMask;
		axisMask = getAxisMask(curMouse.x,curMouse.y);
		
		draggingStart = wxPoint(dragX,dragY);
		//check to see if we have hit a region
		unsigned int plotId,regionId;
		if(!limitInteract && !(axisMask &(AXIS_POSITION_LOW_X | AXIS_POSITION_LOW_Y))
			&& getRegionUnderCursor(curMouse,plotId,regionId))
		{
			PlotRegion r;
			thePlot->getRegion(plotId,regionId,r);

			//TODO: Implement a more generic region handler?
			ASSERT(thePlot->plotType(plotId) == PLOT_MODE_1D);

			float mglStartX,mglStartY;
			toPlotCoords(draggingStart.x, draggingStart.y,mglStartX,mglStartY);

			//Get the type of move, and the region
			//that is being moved, as well as the plot that this
			//region belongs to.
			regionMoveType=computeRegionMoveType(mglStartX,mglStartY, r);
			startMouseRegion=regionId;
			startMousePlot=plotId;
			mouseDragMode=MOUSE_MODE_DRAG_REGION;
		}
		else
			mouseDragMode=MOUSE_MODE_DRAG;
	
	}
	
	
	if( (leftDown && alternateDown) || middleDown)
	{
		mouseDragMode=MOUSE_MODE_DRAG_PAN;
		draggingStart = wxPoint(dragX,dragY);
		
		origPanMinX=xMin;
		origPanMaxX=xMax;
	}

}

void MathGLPane::twoDMouseDownAction(bool leftDown,bool middleDown,
		 bool alternateDown, int dragX,int dragY)
{
	ASSERT(thePlot->getNumVisible());
	
	float xMin,xMax,yMin,yMax;
	thePlot->getBounds(xMin,xMax,yMin,yMax);

	//Set the interaction mode
	if(leftDown && !alternateDown )
	{
		draggingStart = wxPoint(dragX,dragY);
		mouseDragMode=MOUSE_MODE_DRAG;
	}
	
	
	if( (leftDown && alternateDown) || middleDown)
	{
		mouseDragMode=MOUSE_MODE_DRAG_PAN;
		draggingStart = wxPoint(dragX,dragY);
		
		origPanMinX=xMin;
		origPanMaxX=xMax;
		origPanMinY=yMin;
		origPanMaxY=yMax;
	}

}

void MathGLPane::leftMouseDown(wxMouseEvent& event)
{
	if(!readyForInput())
		return;

	int w,h;
	w=h=0;
	GetClientSize(&w,&h);
	if(!w || !h)
		return;

	//mathgl can't handle coordinate transformations with negative values
	if(event.GetPosition().x > w || event.GetPosition().y > h ||
		event.GetPosition().x < 0 || event.GetPosition().y < 0)
		return;

	switch(thePlot->getVisibleMode())
	{
		case PLOT_MODE_1D:
			oneDMouseDownAction(event.LeftDown(),false,
						event.ShiftDown(),
						event.GetPosition().x,
						event.GetPosition().y);
			break;
		case PLOT_MODE_2D:
		case PLOT_MODE_ENUM_END:
			//Do nothing
			twoDMouseDownAction(event.LeftDown(),false,
						event.ShiftDown(),
						event.GetPosition().x,
						event.GetPosition().y);
			break;
		default:
			ASSERT(false);
	}

	event.Skip();
}

void MathGLPane::middleMouseDown(wxMouseEvent &event)
{
	if(!readyForInput())
		return;
	
	int w,h;
	w=0;h=0;
	GetClientSize(&w,&h);
	
	if(!w || !h)
		return;
	
	switch(thePlot->getVisibleMode())
	{
		case PLOT_MODE_1D:
			oneDMouseDownAction(false,event.MiddleDown(),
						event.ShiftDown(),
						event.GetPosition().x,
						event.GetPosition().y);
			break;
		case PLOT_MODE_ENUM_END:
			//Do nothing
			break;
		default:
			ASSERT(false);
	}
	
	event.Skip();
}

void MathGLPane::mouseWheelMoved(wxMouseEvent& event)
{
	//If no valid plot, don't do anything
	if(!readyForInput())
		return;
	
	//no action if currently dragging
	if(!(mouseDragMode==MOUSE_MODE_ENUM_END))
		return;

	unsigned int axisMask=getAxisMask(curMouse.x,curMouse.y);



	//Bigger numbers mean faster. 
	const float SCROLL_WHEEL_ZOOM_RATE=0.20;

	float zoomRate=(float)event.GetWheelRotation()/(float)event.GetWheelDelta();
	zoomRate=zoomRate*SCROLL_WHEEL_ZOOM_RATE;

	//Convert from additive space to multiplicative
	float zoomFactor;
	if(zoomRate > 0.0f)
	{
		zoomFactor=1.0/(1.0+zoomRate);
		ASSERT(zoomFactor> 1.0f);
	}
	else
	{
		zoomFactor=(1.0-zoomRate);
		ASSERT(zoomFactor < 1.0f);
	}



	//retrieve the mouse position
	mglPoint mousePos;
	float mglX,mglY;
	toPlotCoords(curMouse.x,curMouse.y,mglX,mglY);
	mousePos=mglPoint(mglX,mglY);

	float xPlotMin,xPlotMax,yPlotMin,yPlotMax;
	float xMin,xMax,yMin,yMax;
	//Get the current bounds for the plot
	thePlot->getBounds(xMin,xMax,yMin,yMax);
	//Get the absolute bounds for the plot
	thePlot->scanBounds(xPlotMin,xPlotMax,yPlotMin,yPlotMax);

	
	//Zoom around the point
	switch(axisMask)
	{
		//below x axis -> y zoom only
		case AXIS_POSITION_LOW_X:
		{
			float newYMin,newYMax;
			//work out existing bounds on zooming
			zoomBounds(yMin,yMax,mousePos.y,
				zoomFactor, newYMin,newYMax);
			//clamp to plot
			newYMin=std::max(yPlotMin,newYMin);
			newYMax=std::min(yPlotMax,newYMax);
		
			if(newYMax- newYMin> MGL_ZOOM_LIMIT)
				thePlot->setBounds(xMin,xMax,newYMin,newYMax);
			break;
		}
		//Below y axis -> x zoom only
		case AXIS_POSITION_LOW_Y:
		{
			float newXMin,newXMax;
			//work out existing bounds on zooming
			zoomBounds(xMin,xMax,mousePos.x,
				zoomFactor, newXMin,newXMax);

			newXMin=std::max(xPlotMin,newXMin);
			newXMax=std::min(xPlotMax,newXMax);
		
			if(newXMax - newXMin > MGL_ZOOM_LIMIT)	
				thePlot->setBounds(newXMin,newXMax,yMin,yMax);
			break;
		}
		//Zoom both axes
		case AXIS_POSITION_INTERIOR:
		{
			float newXMax,newXMin;
			float newYMax,newYMin;
			//work out existing bounds on zooming
			zoomBounds(xMin,xMax,mousePos.x,
				zoomFactor, newXMin,newXMax);
			zoomBounds(yMin,yMax,mousePos.y,
				zoomFactor, newYMin,newYMax);
			
				
			newXMin=std::max(xPlotMin,newXMin);
			newXMax=std::min(xPlotMax,newXMax);
			newYMin=std::max(yPlotMin,newYMin);
			newYMax=std::min(yPlotMax,newYMax);
			
			if(newXMax - newXMin > MGL_ZOOM_LIMIT &&
				newYMax - newYMin > MGL_ZOOM_LIMIT)	
				thePlot->setBounds(newXMin,newXMax,newYMin,newYMax);
			break;
		}
		default:
			;
	}

	Refresh();
}

void MathGLPane::leftMouseReleased(wxMouseEvent& event)
{

	if(!readyForInput())
		return;

	//!Do we have region updates?
	bool haveUpdates=false;
	
	switch(mouseDragMode)
	{
		case MOUSE_MODE_DRAG:
		{
			wxPoint draggingEnd = event.GetPosition();
			updateDragPos(draggingEnd);
			Refresh();
			break;
		}
		case MOUSE_MODE_DRAG_REGION:
		{
			if(!limitInteract)
			{
				//we need to tell viscontrol that we have done a region
				//update
				float mglX,mglY;

				toPlotCoords(curMouse.x,curMouse.y,mglX,mglY);
				lastEditedRegion=startMouseRegion;
				lastEditedPlot=startMousePlot;
			
				//Send the movement to the parent filter
				thePlot->moveRegion(startMousePlot,startMouseRegion,
							regionSelfUpdate,regionMoveType,
								mglX,mglY);	
				haveUpdates=true;	

			}
			Refresh();
			break;
		}
		default:
		;
	}




	mouseDragMode=MOUSE_MODE_ENUM_END;
	Refresh();
	
	if(haveUpdates)
	{
		for(size_t ui=0;ui<updateHandlers.size(); ui++)
		{
			std::pair<wxWindow*,UpdateHandler> u;
			u=updateHandlers[ui];

			//Call the function
			UpdateHandler h = u.second;
			wxWindow *w=u.first;
			(w->*h)();
		}
	}
}

void MathGLPane::middleMouseReleased(wxMouseEvent& event)
{

	if(!readyForInput())
		return;

	if(mouseDragMode == MOUSE_MODE_DRAG_PAN)
	{
		mouseDragMode=MOUSE_MODE_ENUM_END;
		//Repaint
		Refresh();
	}
}

void MathGLPane::updateDragPos(const wxPoint &draggingEnd) const
{
	ASSERT(mouseDragMode== MOUSE_MODE_DRAG);

	unsigned int startX, endX,startY,endY;

	int w,h;
	GetSize(&w,&h);
	//Define the rectangle
	if(draggingEnd.x > draggingStart.x)
	{
		startX=draggingStart.x;
		endX=draggingEnd.x;
	}
	else
	{
		startX=draggingEnd.x;
		endX=draggingStart.x;
	}

	if(h-draggingEnd.y > h-draggingStart.y)
	{
		startY=draggingStart.y;
		endY=draggingEnd.y;
	}
	else
	{
		startY=draggingEnd.y;
		endY=draggingStart.y;
	}

	//Check that the start and end were not the same (i.e. null zoom in all cases)
	if(startX == endX && startY == endY )
		return ;


	//Compute the MGL coords
	mglPoint pStart,pEnd;
	float mglX,mglY;
	if(!toPlotCoords(startX,startY,mglX,mglY))
		return;
	pStart= mglPoint(mglX,mglY);

	if(!toPlotCoords(endX,endY,mglX,mglY))
		return;
	pEnd = mglPoint(mglX,mglY);


	mglPoint cA;
	cA.x=gr->Self()->GetOrgX('x');
	cA.y=gr->Self()->GetOrgY('y');
	
	float currentAxisX,currentAxisY;
	currentAxisX=cA.x;
	currentAxisY=cA.y;

	if(pStart.x < currentAxisX  && pEnd.x < currentAxisX )
	{
		if(pStart.y < currentAxisY && pEnd.y < currentAxisY )
		{
			//corner event
			return ; // Do nothing
		}
		else
		{
			//Check if can't do anything with this, as it is a null zoom
			if(startY == endY)
				return;

			//left of X-Axis event
			//Reset the axes such that the
			//zoom is only along one dimension (y)
			pStart.x = gr->Self()->Min.x;
			pEnd.x = gr->Self()->Max.x;
		}
	}
	else if(pStart.y < currentAxisY  && pEnd.y < currentAxisY )
	{
		//Check if can't do anything with this, as it is a null zoom
		if(startX == endX)
			return;
		//below Y axis event
		//Reset the axes such that the
		//zoom is only along one dimension (x)
		pStart.y = gr->Self()->Min.y;
		pEnd.y = gr->Self()->Max.y;
	}


	//now that we have the rectangle defined,
	//Allow for the plot to be zoomed
	//

	float minXZoom,maxXZoom,minYZoom,maxYZoom;

	minXZoom=std::min(pStart.x,pEnd.x);
	maxXZoom=std::max(pStart.x,pEnd.x);

	minYZoom=std::min(pStart.y,pEnd.y);
	maxYZoom=std::max(pStart.y,pEnd.y);


	//Enforce zoom limit to avoid FP aliasing
	if(maxXZoom - minXZoom > MGL_ZOOM_LIMIT &&
	        maxYZoom - minYZoom > MGL_ZOOM_LIMIT)
	{
		thePlot->setBounds(minXZoom,maxXZoom,
		                   minYZoom,maxYZoom);
	}

}

void MathGLPane::rightClick(wxMouseEvent& event) 
{
}

void MathGLPane::mouseLeftWindow(wxMouseEvent& event) 
{
	leftWindow=true;
	Refresh();
}

void MathGLPane::keyPressed(wxKeyEvent& event) 
{
	if(!readyForInput())
		return;
	updateMouseCursor();
}

void MathGLPane::keyReleased(wxKeyEvent& event) 
{
	if(!readyForInput())
		return;
	updateMouseCursor();
}


unsigned int MathGLPane::savePNG(const std::string &filename, 
		unsigned int width, unsigned int height)
{

	if(gr)
		delete gr;

	ASSERT(filename.size());
	try
	{
		gr = new mglGraph(0, width,height);
	}
	catch(std::bad_alloc)
	{
		gr=0;
		return MGLPANE_ERR_BADALLOC;
	}

	gr->SetWarn(0,"");
	
	bool dummy;
	thePlot->drawPlot(gr,dummy);	

	gr->WritePNG(filename.c_str());

	bool doWarn;
	doWarn=gr->GetWarn();
	
	if(doWarn)
	{
		lastMglErr= gr->Self()->Mess;
		
		delete gr;
		gr=0;
		return MGLPANE_ERR_MGLWARN;
	}

	delete gr;
	gr=0;
	//Hack. mathgl does not return an error value from its writer
	//function :(. Check to see that the file is openable, and nonzero sized
	
	ifstream f(filename.c_str(),ios::binary);

	if(!f)
		return MGLPANE_FILE_REOPEN_FAIL;

	f.seekg(0,ios::end);


	if(!f.tellg())
		return MGLPANE_FILE_UNSIZED_FAIL;

	return 0;
}

unsigned int MathGLPane::saveSVG(const std::string &filename)
{
	ASSERT(filename.size());


	mglGraph *grS;
	grS = new mglGraph();

	bool dummy;
	thePlot->drawPlot(grS,dummy);

	grS->SetWarn(0,"");

	//Mathgl does not set locale prior to writing SVG
	// do this by hand
	pushLocale("C",LC_NUMERIC);
	grS->WriteSVG(filename.c_str());
	popLocale();


	bool doWarn;
	doWarn=grS->GetWarn();

	if(doWarn)
	{
		lastMglErr=grS->Self()->Mess;
		delete grS;
		grS=0;
		return MGLPANE_ERR_MGLWARN;
	}
	delete grS;

	//Hack. mathgl does not return an error value from its writer
	//function :(. Check to see that the file is openable, and nonzero sized
	
	ifstream f(filename.c_str(),ios::binary);

	if(!f)
		return MGLPANE_FILE_REOPEN_FAIL;

	f.seekg(0,ios::end);


	if(!f.tellg())
		return MGLPANE_FILE_UNSIZED_FAIL;

	return 0;
}

void MathGLPane::setPlotVisible(unsigned int plotId, bool visible)
{
	thePlot->setVisible(plotId,visible);	
}


std::string MathGLPane::getErrString(unsigned int errCode)
{
	switch(errCode)
	{
		case MGLPANE_ERR_BADALLOC:
			return std::string(TRANS("Unable to allocate requested memory.\n Try a lower resolution, or save as vector (SVG)."));
		case MGLPANE_ERR_MGLWARN:
			return std::string(TRANS("Plotting functions returned an error:\n"))+ lastMglErr;
		case MGLPANE_FILE_REOPEN_FAIL:
			return std::string(TRANS("File readback check failed"));
		case MGLPANE_FILE_UNSIZED_FAIL:
			return std::string(TRANS("Filesize during readback appears to be zero."));
		default:
			ASSERT(false);
	}
	ASSERT(false);
}

unsigned int MathGLPane::computeRegionMoveType(float dataX,float dataY,const PlotRegion &r) const
{

	switch(r.bounds.size())
	{
		case 1:
			ASSERT(dataX >= r.bounds[0].first && dataX <=r.bounds[0].second);
			//Can have 3 different aspects. Left, Centre and Right
			return REGION_MOVE_EXTEND_XMINUS+(unsigned int)(3.0f*((dataX-r.bounds[0].first)/
					(r.bounds[0].second- r.bounds[0].first)));
		default:
			ASSERT(false);
	}

	ASSERT(false);	
}

void MathGLPane::drawInteractOverlay(wxDC *dc) const
{

	int w,h;
	w=0;h=0;
	GetClientSize(&w,&h);

	ASSERT(w && h);


	if(curMouse.x < 0 || curMouse.y < 0 || curMouse.x > w || curMouse.y > h)
		return;

	//Draw the overlay if outside the 
	// axes
	unsigned int axisMask=getAxisMask(curMouse.x,curMouse.y);
	unsigned int regionId,plotId;
	if(getRegionUnderCursor(curMouse,plotId,regionId))
	{

		if(axisMask == AXIS_POSITION_INTERIOR)
		{

			PlotRegion r;
			thePlot->getRegion(plotId,regionId,r);
			wxPen *drawPen;
		
			//Select pen colour depending upon whether interaction
			// is allowed
			if(limitInteract)
{

				#if wxCHECK_VERSION(3,1,0)
					drawPen = new wxPen(*wxLIGHT_GREY,2,wxPENSTYLE_SOLID);
				#else
					drawPen= new wxPen(*wxLIGHT_GREY,2,wxSOLID);
				#endif
}

			else
{
				#if wxCHECK_VERSION(3,1,0)
					drawPen = new wxPen(*wxBLACK,2,wxPENSTYLE_SOLID);
				#else
					drawPen= new wxPen(*wxBLACK,2,wxSOLID);
				#endif
}
			
			dc->SetPen(*drawPen);
			//Draw two arrows < > over the centre of the plot
			//---------
			//Use inverse drawing function so that we don't get 
			//black-on-black type drawing.
			//Other option is to use inverse outlines.
			dc->SetLogicalFunction(wxINVERT);

			const int ARROW_SIZE=8;
			

			float pMouseX,pMouseY;
			//Convert the mouse coordinates to data coordinates.
			if(!toPlotCoords(curMouse.x,curMouse.y,pMouseX,pMouseY))
			{
				delete drawPen;
				return;
			}


			unsigned int regionMoveType=computeRegionMoveType(pMouseX,pMouseY,r);

			switch(regionMoveType)
			{
				//Left hand side of region
				case REGION_MOVE_EXTEND_XMINUS:
					dc->DrawLine(curMouse.x-ARROW_SIZE,h/2-ARROW_SIZE,
						     curMouse.x-2*ARROW_SIZE, h/2);
					dc->DrawLine(curMouse.x-2*ARROW_SIZE, h/2,
						     curMouse.x-ARROW_SIZE,h/2+ARROW_SIZE);
					break;
				//right hand side of region
				case REGION_MOVE_EXTEND_XPLUS:
					dc->DrawLine(curMouse.x+ARROW_SIZE,h/2-ARROW_SIZE,
						     curMouse.x+2*ARROW_SIZE, h/2);
					dc->DrawLine(curMouse.x+2*ARROW_SIZE, h/2,
						     curMouse.x+ARROW_SIZE,h/2+ARROW_SIZE);
					break;

				//centre of region
				case REGION_MOVE_TRANSLATE_X:
					dc->DrawLine(curMouse.x-ARROW_SIZE,h/2-ARROW_SIZE,
						     curMouse.x-2*ARROW_SIZE, h/2);
					dc->DrawLine(curMouse.x-2*ARROW_SIZE, h/2,
						     curMouse.x-ARROW_SIZE,h/2+ARROW_SIZE);
					dc->DrawLine(curMouse.x+ARROW_SIZE,h/2-ARROW_SIZE,
						     curMouse.x+2*ARROW_SIZE, h/2);
					dc->DrawLine(curMouse.x+2*ARROW_SIZE, h/2,
						     curMouse.x+ARROW_SIZE,h/2+ARROW_SIZE);
					break;
				default:
					ASSERT(false);

			}

			dc->SetLogicalFunction(wxCOPY);
			delete drawPen;
			//---------

			//Draw the label for the species being hovered.
			//---------
			string labelText;
			labelText = r.getName();
			wxSize textSize=dc->GetTextExtent((labelText));
			dc->DrawText((labelText),curMouse.x-textSize.GetWidth()/2, 
					h/2-(textSize.GetHeight() + 1.5*ARROW_SIZE));
			//---------
		}	
	}
	else
	{
		//Draw small helper icons in top right of window
		// TODO: Multiple images, and image cache
		vector<unsigned int> textureIDs;

		if(axisMask & AXIS_POSITION_LOW_X && 
			(axisMask & AXIS_POSITION_LOW_Y))
			textureIDs.push_back(PLOT_TEXTURE_ZOOM_RESET);
		else if (axisMask & AXIS_POSITION_LOW_X)
			textureIDs.push_back((PLOT_TEXTURE_ZOOM_Y));
		else if (axisMask & AXIS_POSITION_LOW_Y)
		{
			textureIDs.push_back(PLOT_TEXTURE_ZOOM_X);
			textureIDs.push_back(PLOT_TEXTURE_SLIDE_X);
		}

		const float THUMB_FRACTION=0.1;
		const unsigned int MIN_THUMB_SIZE=10;
		unsigned int thumbSize=THUMB_FRACTION*std::min(h,w);

		

		if(thumbSize > MIN_THUMB_SIZE)
		{

			for(size_t ui=0;ui<textureIDs.size();ui++)
			{
				size_t textureID;
				textureID = textureIDs[ui];

				ASSERT(textureID < PLOT_TEXTURE_ENUM_END);
				std::string filename;
				filename=locateDataFile(mglTextureFile[textureID]);
		
					
				//Need to draw a picture
				wxImage img;
				if(wxFileExists((filename)) && img.LoadFile((filename) ))
				{
					int position[2];
					float tmp;
					
					img.Rescale(thumbSize,thumbSize,wxIMAGE_QUALITY_HIGH);

					wxBitmap bmp(img);
					//Draw in upper right, by one fraction
					tmp= (1.0-1.5*THUMB_FRACTION);
					position[0] = tmp*w;
					
					//Compute the vertical spacing for each icon 
					position[1] = (1.0-(tmp - 2.0*(float)ui*THUMB_FRACTION))*h;

					dc->DrawBitmap(img,position[0],position[1]);
				}
			}
		}
	}

}



bool MathGLPane::toPlotCoords(int winX, int winY,float &resX, float &resY) const
{
	int width, height;
	GetClientSize(&width,&height);
	if(winX < 0 || winY<0 || winX > width || winY > height)
	{
		WARN(false,"DEBUG ONLY - was outside window coord");
		return false;
	}
		
	ASSERT(gr);
	mglPoint pt = gr->CalcXYZ(winX,winY);
	
	resX=pt.x;
	if(plotIsLogarithmic)
	{
		float plotMinY,plotMaxY;
		plotMinY=gr->Self()->Min.y;
		plotMaxY=gr->Self()->Max.y;
		float proportion =(pt.y-plotMinY)/(plotMaxY-plotMinY);
		float tmp = proportion*(log10(plotMaxY)-log10(plotMinY)) + log10(plotMinY); 
		
		resY=pow10(tmp);
	}
	else
		resY=pt.y;

	return true;
}
bool MathGLPane::toWinCoords(float plotX, float plotY, float &winX, float &winY) const
{
	mglPoint tmp;
	tmp=gr->CalcScr(mglPoint(plotX,plotY));
	winX=tmp.x; winY=tmp.y;

	if(plotIsLogarithmic)
	{
		//FIXME: IMPLEMENT ME
		WARN(false,"NOT IMPLEMENTED FOR LOG MODE");
		return true;
	}
	else
		return true;
}




void MathGLPane::drawRegionDraggingOverlay(wxDC *dc) const
{
	int w,h;
	w=0;h=0;
	GetClientSize(&w,&h);
	ASSERT(w && h);
	//Well, we are dragging the region out some.
	//let us draw a line from the original X position to
	//the current mouse position/nearest region position

	float regionLimitX,regionLimitY;
	if(!toPlotCoords(curMouse.x,curMouse.y,regionLimitX,regionLimitY))
		return;


	ASSERT(thePlot->plotType(startMousePlot) == PLOT_MODE_1D);

	//See where extending the region is allowed up to.
	thePlot->findRegionLimit(startMousePlot,startMouseRegion,
					regionMoveType, regionLimitX,regionLimitY);
	
	
	float testX,testY;
	toWinCoords(regionLimitX,regionLimitY,testX,testY);
	
	int deltaDrag = testX-draggingStart.x;

	//Draw some text above the cursor to indicate the current position
	std::string str;
	stream_cast(str,regionLimitX);
	wxString wxs;
	wxs=(str);
	wxCoord textW,textH;
	dc->GetTextExtent(wxs,&textW,&textH);

	wxFont font;
	font.SetFamily(wxFONTFAMILY_SWISS);
	if(font.IsOk())
		dc->SetFont(font);
	
	wxPen *arrowPen;

#if wxCHECK_VERSION(3,1,0)
	arrowPen=  new wxPen(*wxBLACK,2,wxPENSTYLE_SOLID);
#else
	arrowPen=  new wxPen(*wxBLACK,2,wxSOLID);
#endif
	dc->SetPen(*arrowPen);
	const int ARROW_SIZE=8;
	
	dc->SetLogicalFunction(wxINVERT);
	//draw horiz line
	dc->DrawLine(testX,h/2,
		     draggingStart.x,h/2);
	if(deltaDrag > 0)
	{

		dc->DrawText(wxs,testX-textW,h/2-textH*2);
		//Draw arrow head to face right
		dc->DrawLine(testX,h/2,
			     testX-ARROW_SIZE, h/2-ARROW_SIZE);
		dc->DrawLine(testX, h/2,
			     testX-ARROW_SIZE,h/2+ARROW_SIZE);

	}
	else
	{
		dc->DrawText(wxs,testX,h/2-textH*2);
		//Draw arrow head to face left
		dc->DrawLine(testX,h/2,
			     testX+ARROW_SIZE, h/2-ARROW_SIZE);
		dc->DrawLine(testX, h/2,
			     testX+ARROW_SIZE,h/2+ARROW_SIZE);
	}


	float mglCurMouseX,mglCurMouseY;
	if(!toPlotCoords(curMouse.x,curMouse.y,mglCurMouseX,mglCurMouseY))
	{
		dc->SetLogicalFunction(wxCOPY);
		delete arrowPen;
		return;
	}
	
	switch(regionMoveType)
	{
		case REGION_MOVE_EXTEND_XMINUS:
		case REGION_MOVE_EXTEND_XPLUS:
			//No extra markers; we are cool as is
			break;
		case REGION_MOVE_TRANSLATE_X:
		{
			//This needs to be extended to support more
			//plot types.
			ASSERT(thePlot->plotType(startMousePlot) == PLOT_MODE_1D);
			
			//Draw "ghost" limits markers for move,
			//these appear as moving vertical bars to outline
			//where the translation result will be for both
			//upper and lower
			PlotRegion reg;
			thePlot->getRegion(startMousePlot,startMouseRegion,reg);

			//Convert form window to mathgl coordinates
			float mglDragStartX,mglDragStartY;
			if(!toPlotCoords(draggingStart.x,draggingStart.y,
				mglDragStartX,mglDragStartY))
				break;

			float newLower,newUpper;
			newLower = reg.bounds[0].first + (mglCurMouseX-mglDragStartX);
			newUpper = reg.bounds[0].second + (mglCurMouseX-mglDragStartX);

			float newLowerX,newUpperX,dummy;
			toWinCoords(newLower,0.0f,newLowerX,dummy);
			toWinCoords(newUpper,0.0f,newUpperX,dummy);

			dc->DrawLine(newLowerX,h/2+2*ARROW_SIZE,newLowerX,h/2-2*ARROW_SIZE);
			dc->DrawLine(newUpperX,h/2+2*ARROW_SIZE,newUpperX,h/2-2*ARROW_SIZE);
			break;
		}
		default:
			ASSERT(false);
			break;
	}	

	dc->SetLogicalFunction(wxCOPY);
	delete arrowPen;

}
