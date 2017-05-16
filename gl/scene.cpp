/*
 *	scene.cpp  - OpenGL 3D static scene implementation
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

#if defined(WIN32) || defined(WIN64)
#include <GL/glew.h>
#endif

#include "scene.h"

#include "./backend/viscontrol.h"
#include "./backend/filter.h"

using std::vector;
using std::string;


//
const char ANIMATE_PROGRESS_BASENAME[] = {
	"textures/animProgress"};
unsigned int ANIMATE_PROGRESS_NUMFRAMES=3;



Scene::Scene() : tempCam(0), cameraSet(true), outWinAspect(1.0f)
{
	glewInited=false;
	visControl=0;

	lastHovered=lastSelected=(unsigned int)(-1);
	lockInteract=false;
	hoverMode=selectionMode=false;
	useAlpha=true;
	useLighting=true;
	useEffects=false;
	showAxis=true;
	attemptedLoadProgressAnim=false;
	witholdCamUpdate=false;

	//default to black
	rBack=gBack=bBack=0.0f;

	activeCam = new CameraLookAt;

	lightPosition[0]= 1.0;
	lightPosition[1]= 1.0;
	lightPosition[2]= 1.0;
	lightPosition[3]=0.0;

	DrawableObj::setTexPool(new TexturePool);

}

Scene::~Scene()
{
	clearAll();
	DrawableObj::clearTexPool();
	delete activeCam;
}

unsigned int Scene::initDraw()
{
	//Initialise GLEW
#if defined(WIN32) || defined(WIN64)
	if(!glewInited)
	{
		unsigned int errCode;
		errCode=glewInit();
		if(errCode!= GLEW_OK)
		{
			cerr << "Opengl context could not be created, aborting." << endl;
			cerr << "Glew reports:" << glewGetErrorString(errCode) << endl;
			//Blow up without opengl
			exit(1);
		}
		glewInited=true;
	}
#endif


	glClearColor( rBack, gBack, bBack,1.0f );
	glClear(GL_COLOR_BUFFER_BIT |GL_DEPTH_BUFFER_BIT);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glEnable(GL_DEPTH_TEST);

	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial ( GL_FRONT, GL_AMBIENT_AND_DIFFUSE ) ;

	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_LINE_SMOOTH);
	
	//Will it blend? That is the question...
	// let the objects know about this, so they
	// can pick the right algorithm
	DrawableObj::setUseAlphaBlending(useAlpha);
	
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	
	if(useAlpha)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);

	glDisable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glShadeModel(GL_SMOOTH);
	//Set up the scene lights
	//==

	//Set up default lighting
	const float light_ambient[] = { 0.0, 0.0, 0.0, 1.0 };
	const float light_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
	// The direction the light shines in
	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition); 


	//==
	
	//Use the active if set 
	if(cameraSet)
	{
		if(!boundCube.isValid())
			computeSceneLimits();

	}

	//Attempt to load the progress animation, if we have not tried before
	if(!attemptedLoadProgressAnim)
	{
		attemptedLoadProgressAnim=true;

		//Load the animation textures
		vector<string> animFiles;
		animFiles.resize(ANIMATE_PROGRESS_NUMFRAMES);
		string animFilename,tmpStr;
		for(size_t ui=0;ui<ANIMATE_PROGRESS_NUMFRAMES;ui++)
		{
			animFilename=ANIMATE_PROGRESS_BASENAME;
			stream_cast(tmpStr,ui);
			animFilename+=tmpStr + string(".png");
			animFiles[ui]=animFilename;
		}


		progressAnimTex.setTexture(animFiles);
		//Cycle every this many seconds
		progressAnimTex.setRepeatTime(6.0f);
		//Ramp opacity for this long (seconds)
		progressAnimTex.setFadeInTime(2.0f);
		//Don't show the animation until this many sceonds
		progressAnimTex.setShowDelayTime(1.5f);
		
		progressCircle.setFadeInTime(1.5f);
		progressCircle.setShowDelayTime(1.0f);

		updateProgressOverlay();
	}

	//Let the effects objects know about the scene
	Effect::setBoundingCube(boundCube);

	unsigned int passes=1;

	if(useEffects)
	{
		for(unsigned int ui=0;ui<effects.size();ui++)
			passes=std::max(passes,effects[ui]->numPassesNeeded());
	}

	return passes;
}

bool Scene::hasOverlays() const
{
	for(unsigned int ui=0;ui<objects.size();ui++)
	{
		if(objects[ui]->isOverlay())
			return true;
	}
	
	for(unsigned int ui=0;ui<refObjects.size();ui++)
	{
		if(refObjects[ui]->isOverlay())
			return true;
	}

	return false;
}

void Scene::updateCam(const Camera *camToUse, bool useIdent=true) const
{
	Point3D lightNormal;
	
	camToUse->apply(outWinAspect,boundCube,useIdent);

	lightNormal=camToUse->getViewDirection();
	glNormal3f(lightNormal[0],lightNormal[1],lightNormal[2]);	
}

void Scene::updateProgressOverlay()
{
	const float xPos= 0.85*winX;
	const float yPos=0.85*winY;

	progressAnimTex.setPosition(xPos,yPos);
	progressAnimTex.setSize(0.1*winX);
	//Draw the progress animation bar
	progressCircle.setPosition(xPos,yPos);
	progressCircle.setSize(0.15*winX);
}

void Scene::draw(bool noUpdateCam) 
{
	glError();
	ASSERT(visControl);
	glPushMatrix();

	Camera *camToUse;
	if(tempCam)
		camToUse=tempCam;
	else
		camToUse=activeCam;

	//Inform text about current camera, 
	// so it can e.g. billboard if needed
	DrawableObj::setCurCamera(camToUse);
	DrawableObj::setWindowSize(winX,winY);
	DrawableObj::setBackgroundColour(rBack,gBack,bBack);
	Effect::setCurCam(camToUse);


	bool lightsOn=false;
	//Find number of passes to  perform
	unsigned int numberTotalPasses;
	numberTotalPasses=initDraw();

	if(cameraSet && !noUpdateCam)
		updateCam(camToUse);



	for(unsigned int passNumber=0; passNumber<numberTotalPasses; passNumber++)
	{

		if(useEffects)
		{
			bool needCamUpdate=false;
			for(unsigned int ui=0;ui<effects.size();ui++)
			{
				effects[ui]->enable(passNumber);
				needCamUpdate|=effects[ui]->needCamUpdate();
			}

			if(cameraSet && !noUpdateCam && needCamUpdate )
				updateCam(camToUse);
		}
		

		if(showAxis)
		{
			if(useLighting)
				glEnable(GL_LIGHTING);
			DrawAxis a;
			a.setStyle(AXIS_IN_SPACE);
			a.setSize(boundCube.getLargestDim());
			a.setPosition(boundCube.getCentroid());

			a.draw();
			if(useLighting)
				glDisable(GL_LIGHTING);
		}
		
	
		//First sub-pass with opaque objects
		//-----------	
		//Draw the referenced objects
		drawObjectVector(refObjects,lightsOn,true);
		//Draw normal objects
		drawObjectVector(objects,lightsOn,true);
		//-----------	
		
		//Second sub-pass with transparent objects
		//-----------	
		//Draw the referenced objects
		drawObjectVector(refObjects,lightsOn,false);
		//Draw normal objects
		drawObjectVector(objects,lightsOn,false);
		//-----------	
		
		
	}
	
	
	//Disable effects
	if(useEffects)
	{
		//Disable them in reverse order to simulate a stack-type
		//behaviour.
		for(unsigned int ui=effects.size();ui!=0;)
		{
			ui--;
			effects[ui]->disable();
		}
	}


	glPopMatrix();
	
	//Only draw 2D components if we
	// are using normal camera	
	if(!noUpdateCam)
	{
		//Now draw 2D overlays
		if(!lockInteract&& lastHovered != (unsigned int)(-1) )
			drawHoverOverlay();

		drawOverlays(noUpdateCam);

		//Draw progress, if needed
		drawProgressAnim();
	}
	glError();
}

void Scene::drawObjectVector(const vector<const DrawableObj*> &drawObjs, bool &lightsOn, bool drawOpaques) const
{
	for(unsigned int ui=0; ui<drawObjs.size(); ui++)
	{
		//Only draw opaque drawObjs in this pass if not required
		if(drawObjs[ui]->needsDepthSorting() == drawOpaques)
			continue;
	
		//overlays need to be drawn later
		if(drawObjs[ui]->isOverlay())
			continue;

		if(useLighting)
		{	
			if(!drawObjs[ui]->wantsLight && lightsOn )
			{
				//Object prefers doing its thing in the dark
				glDisable(GL_LIGHTING);
				lightsOn=false;
			}
			else if (drawObjs[ui]->wantsLight && !lightsOn)
			{
				glEnable(GL_LIGHTING);
				lightsOn=true;
			}
		}
		
		//If we are in selection mode, draw the bounding box
		//if the object is selected.
		if(ui == lastSelected && selectionMode)
		{
			//May be required for selection box drawing
			BoundCube bObject;
			DrawRectPrism p;
			//Get the bounding box for the object & draw it
			drawObjs[ui]->getBoundingBox(bObject);
			p.setAxisAligned(bObject);
			p.setColour(0,0.2,1,0.5); //blue-greenish
			if(lightsOn)
				glDisable(GL_LIGHTING);
			p.draw();
			if(lightsOn)
				glEnable(GL_LIGHTING);

		}
		


#ifdef DEBUG
		//Ensure that the gl matrix sizes are correctly restored
		int curDepth[3];
		int oldMatMode;
		curDepth[0] = glCurStackDepth(GL_MODELVIEW_STACK_DEPTH);		
		curDepth[1] = glCurStackDepth(GL_PROJECTION_STACK_DEPTH);		
		curDepth[2] = glCurStackDepth(GL_TEXTURE_STACK_DEPTH);		
		glGetIntegerv( GL_MATRIX_MODE, &oldMatMode);
#endif
		drawObjs[ui]->draw();

#ifdef DEBUG
		ASSERT(curDepth[0] == glCurStackDepth(GL_MODELVIEW_STACK_DEPTH));	
		ASSERT(curDepth[1] == glCurStackDepth(GL_PROJECTION_STACK_DEPTH));	
		ASSERT(curDepth[2] == glCurStackDepth(GL_TEXTURE_STACK_DEPTH));		
		ASSERT(curDepth[0] && curDepth[1] && curDepth[2]);
		int newMatMode;
		glGetIntegerv( GL_MATRIX_MODE, &newMatMode);
		ASSERT(oldMatMode == newMatMode);
#endif
	}
}

void Scene::drawOverlays(bool noUpdateCam) const
{

	//Custom projection matrix
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	//Set the opengl camera state back into modelview mode
	if(!noUpdateCam)
	{
		//clear projection and modelview matrices
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		gluOrtho2D(0, outWinAspect, 1.0, 0);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
	}



	for(unsigned int ui=0;ui<refObjects.size();ui++)
	{
		if(refObjects[ui]->isOverlay())
		{
			refObjects[ui]->draw();
		}
	}
	
	for(unsigned int ui=0;ui<objects.size();ui++)
	{
		if(objects[ui]->isOverlay())
		{
			objects[ui]->draw();
		}
	}
	

	if(!noUpdateCam)
	{
		//op our modelview matrix
		glPopMatrix();
		
		//ppop projection atrix
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		//return to modelview mode
		glMatrixMode(GL_MODELVIEW);
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
}

void Scene::drawHoverOverlay()
{

	glEnable(GL_ALPHA_TEST);
	glDisable(GL_DEPTH_TEST);
	//Search for a binding
	bool haveBinding;
	haveBinding=false;

	//Prevent transparent areas from interacting
	//with the depth buffer
	glAlphaFunc(GL_GREATER,0.01f);
	
	vector<const SelectionBinding *> binder;
	//!Devices for interactive object properties
	const std::vector<SelectionDevice *> &selectionDevices = visControl->getSelectionDevices();
	for(unsigned int uj=0;uj<selectionDevices.size();uj++)
	{
		if(selectionDevices[uj]->getAvailBindings(objects[lastHovered],binder))
		{
			haveBinding=true;
			break;
		}
	}


	if(haveBinding)
	{	
		glPushAttrib(GL_LIGHTING);
		glDisable(GL_LIGHTING);

		//Now draw some hints for the binding itself as a 2D overlay
		//
		//Draw the action type (translation, rotation etc)
		//and the button it is bound to
		DrawTexturedQuadOverlay binderIcons,mouseIcons,keyIcons;


		const float ICON_SIZE= 0.05;
		binderIcons.setSize(ICON_SIZE*winY);
		mouseIcons.setSize(ICON_SIZE*winY);
		keyIcons.setSize(ICON_SIZE*winY);

		unsigned int iconNum=0;
		for(unsigned int ui=0;ui<binder.size();ui++)
		{
			bool foundIconTex,foundMouseTex;
			foundIconTex=false;
			foundMouseTex=false;
			switch(binder[ui]->getInteractionMode())
			{
				case BIND_MODE_FLOAT_SCALE:
				case BIND_MODE_FLOAT_TRANSLATE:
				case BIND_MODE_POINT3D_SCALE:
					foundIconTex=binderIcons.setTexture(TEXTURE_OVERLAY_PNG[TEXTURE_ENLARGE]);
					break;
				case BIND_MODE_POINT3D_TRANSLATE:
					foundIconTex=binderIcons.setTexture(TEXTURE_OVERLAY_PNG[TEXTURE_TRANSLATE]);
					break;
				case BIND_MODE_POINT3D_ROTATE:
				case BIND_MODE_POINT3D_ROTATE_LOCK:
					foundIconTex=binderIcons.setTexture(TEXTURE_OVERLAY_PNG[TEXTURE_ROTATE]);
				default:
					break;
			}

			//Draw the mouse action
			switch(binder[ui]->getMouseButtons())
			{
				case SELECT_BUTTON_LEFT:
					foundMouseTex=mouseIcons.setTexture(TEXTURE_OVERLAY_PNG[TEXTURE_LEFT_CLICK]);
					break;
				case SELECT_BUTTON_MIDDLE:
					foundMouseTex=mouseIcons.setTexture(TEXTURE_OVERLAY_PNG[TEXTURE_MIDDLE_CLICK]);
					break;
				case SELECT_BUTTON_RIGHT:
					foundMouseTex=mouseIcons.setTexture(TEXTURE_OVERLAY_PNG[TEXTURE_RIGHT_CLICK]);
					break;
				default:
					//The flags are or'd together, so we can get other combinations
					break;
			}

			bool foundKeyTex;
			foundKeyTex=false;
			//Draw the keyboard action, if any
			switch(binder[ui]->getKeyFlags())
			{
				case FLAG_CMD:
#ifdef __APPLE__
					foundKeyTex=keyIcons.setTexture(TEXTURE_OVERLAY_PNG[TEXTURE_COMMAND]);
#else
					foundKeyTex=keyIcons.setTexture(TEXTURE_OVERLAY_PNG[TEXTURE_CTRL]);
#endif
					break;
				case FLAG_SHIFT:
					foundKeyTex=keyIcons.setTexture(TEXTURE_OVERLAY_PNG[TEXTURE_SHIFT]);
					break;
				default:
					//The flags are or'd together, so we can get other combinations
					break;
			}

			if(foundIconTex && foundMouseTex )
			{
				const float SPACING=0.75*ICON_SIZE;
				if(foundKeyTex)
				{
					//Make room for keyTex
					binderIcons.setPosition((0.93+SPACING)*winX,ICON_SIZE*winY*(1+(float)iconNum));
					keyIcons.setPosition(0.93*winX,ICON_SIZE*winY*(1+(float)iconNum));
					mouseIcons.setPosition((0.93-SPACING)*winX,ICON_SIZE*winY*(1+(float)iconNum));
				}
				else
				{
					binderIcons.setPosition(0.95*winX,ICON_SIZE*winY*(1+(float)iconNum));
					mouseIcons.setPosition(0.90*winX,ICON_SIZE*winY*(1+(float)iconNum));
				}

				binderIcons.draw();
				mouseIcons.draw();

				if(foundKeyTex)
					keyIcons.draw();

				iconNum++;
			}
			
		}

		glPopAttrib();

	}


	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

void Scene::drawProgressAnim() const
{
	if(!visControl->state.treeState.isRefreshing())
		return;

	if(useLighting)
		glDisable(GL_LIGHTING);

	progressCircle.draw();
	if(!progressAnimTex.isOK())
	{
		if(useLighting)
			glEnable(GL_LIGHTING);
		return;
	}

	progressAnimTex.draw();

	if(useLighting)
		glEnable(GL_LIGHTING);
}

void Scene::commitTempCam()
{
	ASSERT(tempCam);
	std::swap(activeCam, tempCam);
	delete tempCam;
	tempCam=0;
}

void Scene::discardTempCam()
{
	delete tempCam;
	tempCam=0;
}

void Scene::setTempCam()
{
	//If a temporary camera is not set, set one.
	//if it is set, update it from the active camera
	if(!tempCam)
		tempCam =activeCam->clone();
	else
		*tempCam=*activeCam;
}

void Scene::addDrawable(DrawableObj const *obj )
{
	objects.push_back(obj);
	BoundCube bc;
	obj->getBoundingBox(bc);

	if(bc.isValid())
		boundCube.expand(bc);
}

void Scene::addRefDrawable(const DrawableObj *obj)
{
	refObjects.push_back(obj);
	BoundCube bc;
	obj->getBoundingBox(bc);

	ASSERT(bc.isValid());
	boundCube.expand(bc);
}	

void Scene::clearAll()
{
	//Invalidate the bounding cube
	boundCube.setInverseLimits();

	clearObjs();
	clearRefObjs();
}

void Scene::clearObjs()
{
	for(unsigned int ui=0; ui<objects.size(); ui++)
		delete objects[ui];
	objects.clear();
	lastHovered=-1;
}


void Scene::clearRefObjs()
{
	refObjects.clear();
}

void Scene::getLightPos(float *f) const
{ 
	for(unsigned int ui=0;ui<4;ui++)
		f[ui] = lightPosition[ui];
}

void Scene::setLightPos(const float *f)
{
	for(unsigned int ui=0;ui<4;ui++)
		lightPosition[ui]=f[ui];
}

void Scene::setAspect(float newAspect)
{
	outWinAspect=newAspect;
}
void Scene::setActiveCam(Camera *c)
{
	if(tempCam)
		discardTempCam();

	if(activeCam)
		delete activeCam;

	activeCam = c;
	cameraSet=true;
}

void Scene::setActiveCamByClone(const Camera *c)
{
	if(tempCam)
		discardTempCam();

	if(activeCam)
		delete activeCam;
	
	activeCam = c->clone();
	cameraSet=true;
}


void Scene::ensureVisible(unsigned int direction)
{
	computeSceneLimits();

	activeCam->ensureVisible(boundCube,direction);
}

void Scene::computeSceneLimits()
{
	boundCube.setInverseLimits();

	BoundCube b;
	for(unsigned int ui=0; ui<objects.size(); ui++)
	{
		objects[ui]->getBoundingBox(b);
	
		if(b.isValid())	
			boundCube.expand(b);
	}

	for(unsigned int ui=0; ui<refObjects.size(); ui++)
	{
		refObjects[ui]->getBoundingBox(b);
		
		if(b.isValid())	
			boundCube.expand(b);
	}


	if(!boundCube.isValid())
	{
		//He's going to spend the rest of his life
		//in a one by one unit box.

		//If there are no objects, then set the bounds
		//to 1x1x1, centered around the origin
		boundCube.setBounds(-0.5,-0.5,-0.5,
					0.5,0.5,0.5);
	}
	//NOw that we have a scene level bounding box,
	//we need to set the camera to ensure that
	//this box is visible
	ASSERT(boundCube.isValid());


	//The scene bounds should be no less than 0.1 units
	BoundCube unitCube;
	Point3D centre;

	centre=boundCube.getCentroid();

	unitCube.setBounds(centre+Point3D(0.05,0.05,0.05),
				centre-Point3D(0.05,0.05,0.05));
	boundCube.expand(unitCube);
}

Camera *Scene::getActiveCam()
{
	return activeCam;
}

Camera *Scene::getTempCam()
{
	ASSERT(tempCam);
	return tempCam;
}

//Adapted from 
//http://chadweisshaar.com/robotics/docs/html/_v_canvas_8cpp-source.html
//GPLv3+ permission obtained by email inquiry.
unsigned int Scene::glSelect(bool storeSelected)
{
	ASSERT(!lockInteract);

	glClear(  GL_DEPTH_BUFFER_BIT );
	//Shouldn't be using a temporary camera.
	//temporary cameras are only active during movement operations
	ASSERT(!tempCam);
	
	
	// Need to load a base name so that the other calls can replace it
	GLuint *selectionBuffer = new GLuint[512];
	glSelectBuffer(512, selectionBuffer);
	glRenderMode(GL_SELECT);
	glInitNames();
	
	if(!boundCube.isValid())
		computeSceneLimits();

	glPushMatrix();
	//Apply the camera, but do NOT load the identity matrix, as
	//we have set the pick matrix
	activeCam->apply(outWinAspect,boundCube,false);

	//Set up the objects. Only NON DISPLAYLIST items can be selected.
	for(unsigned int ui=0; ui<objects.size(); ui++)
	{
		glPushName(ui);
		if(objects[ui]->canSelect)
			objects[ui]->draw();
		glPopName();
	}

	//OpengGL Faq:
	//The number of hit records is returned by the call to
	//glRenderMode(GL_RENDER). Each hit record contains the following
	//information stored as unsigned ints:
	//
	// * Number of names in the name stack for this hit record
	// * Minimum depth value of primitives (range 0 to 2^32-1)
	// * Maximum depth value of primitives (range 0 to 2^32-1)
	// * Name stack contents (one name for each unsigned int).
	//
	//You can use the minimum and maximum Z values with the device
	//coordinate X and Y if known (perhaps from a mouse click)
	//to determine an object coordinate location of the picked
	//primitive. You can scale the Z values to the range 0.0 to 1.0,
	//for example, and use them in a call to gluUnProject().
	glFlush();
	GLint hits = glRenderMode(GL_RENDER);
	
	//The hit query records are stored in an odd manner
	//as the name stack is returned with it. This depends
	//upon how you have constructed your name stack during drawing
	//(clearly).  I didn't bother fully understanding this, as it does
	//what I want.
	GLuint *ptr = selectionBuffer;
	GLuint *closestNames = 0;
	GLuint minZ = 0xffffffff;
	GLuint numClosestNames = 0;
	for ( int i=0; i<hits; ++i )
	{
		if ( *(ptr+1) < minZ )
		{
			numClosestNames = *ptr;
			minZ = *(ptr+1);
			closestNames = ptr+3;
		}
		ptr+=*ptr+3;
	}


	//Record the nearest item
	// There should only be one item on the hit stack for the closest item.
	GLuint closest;
	if(numClosestNames==1)
		closest=closestNames[0]; 
	else 
		closest=(unsigned int)(-1);

	delete[] selectionBuffer;

	glPopMatrix();
	
	//Record the last item if required.
	if(storeSelected)
	{
		lastSelected=closest;
		return lastSelected;
	}
	else
		return closest;

}

void Scene::finaliseCam()
{
	switch(activeCam->type())
	{
		case CAM_LOOKAT:
			((CameraLookAt *)activeCam)->recomputeUpDirection();
			break;
	}
}

void Scene::resetProgressAnim()
{ 	
	progressAnimTex.resetTime(); 
	progressCircle.resetTime(); 
	progressCircle.reset();
};


//Values are in the range [0 1].
void Scene::applyDevice(float startX, float startY, float curX, float curY,
			unsigned int keyFlags, unsigned int mouseFlags,	bool permanent)
{

	ASSERT(!lockInteract);
	if(lastSelected == (unsigned int) (-1))
		return;


	//Object should be in object array, and be selectable
	ASSERT(lastSelected < objects.size())
	ASSERT(objects[lastSelected]->canSelect);

	//Grab basis vectors. (up, forwards and 
	//across from camera view.)
	//---
	Point3D forwardsDir,upDir;
	
	forwardsDir=getActiveCam()->getViewDirection();
	upDir=getActiveCam()->getUpDirection();

	forwardsDir.normalise();
	upDir.normalise();
	Point3D acrossDir;
	acrossDir=forwardsDir.crossProd(upDir);

	acrossDir.normalise();
	//---

	//Compute the distance between the selected object's
	//centroid and the camera
	//---
	float depth;
	BoundCube b;
	objects[lastSelected]->getBoundingBox(b);
	//Camera-> object vector
	Camera *cam=getActiveCam();

	Point3D camToObject;
	//Get the vector to the object	
	camToObject = b.getCentroid() - cam->getOrigin();
	depth = camToObject.dotProd(forwardsDir);
	//---

	//Compute the width of the camera view for the object at 
	//the plane that intersects the object's centroid, and is 
	//normal to the camera direction
	float viewWidth;
	switch(cam->type())
	{
		case CAM_LOOKAT:
			viewWidth=((CameraLookAt*)cam)->getViewWidth(depth);
			break;
		default:
			ASSERT(false);
	}


	//We have the object number, but we don't know which binding
	//corresponds to this object. Search all bindings. It may be that more than one 
	//binding is enabled for this object
	SelectionBinding *binder;

	vector<SelectionBinding*> activeBindings;
	std::vector<SelectionDevice *> &selectionDevices = visControl->getSelectionDevices();
	for(unsigned int ui=0;ui<selectionDevices.size();ui++)
	{
		if(selectionDevices[ui]->getBinding(
			objects[lastSelected],mouseFlags,keyFlags,binder))
			activeBindings.push_back(binder);
	}

	for(unsigned int ui=0;ui<activeBindings.size();ui++)
	{
		//Convert the mouse-XY coords into a world coordinate, depending upon mouse/
		//key cobinations
		Point3D worldVec;
		Point3D vectorCoeffs[2];
		activeBindings[ui]->computeWorldVectorCoeffs(mouseFlags,keyFlags,
					vectorCoeffs[0],vectorCoeffs[1]);	

		//Apply vector coeffs, dependant upon binding
		worldVec = acrossDir*vectorCoeffs[0][0]*(curX-startX)*outWinAspect
				+ upDir*vectorCoeffs[0][1]*(curX-startX)*outWinAspect
				+ forwardsDir*vectorCoeffs[0][2]*(curX-startX)*outWinAspect
				+ acrossDir*vectorCoeffs[1][0]*(curY-startY)
				+ upDir*vectorCoeffs[1][1]*(curY-startY)
				+ forwardsDir*vectorCoeffs[1][2]*(curY-startY);
		worldVec*=viewWidth;
		
		activeBindings[ui]->applyTransform(worldVec,permanent);
	}

	computeSceneLimits();
	//Inform viscontrol about updates, if we have applied any
	if(activeBindings.size() && permanent)
	{
		visControl->state.treeState.setUpdates();
		//If the viscontrol is in the middle of an update,
		//tell it to abort.
		if(visControl->state.treeState.isRefreshing())
			visControl->state.treeState.setAbort();
	}

}

void Scene::getEffects(vector<const Effect *> &eff) const
{
	eff.resize(effects.size());

	for(unsigned int ui=0;ui<effects.size();ui++)	
		eff[ui]=effects[ui];
}	


void Scene::setEffectVec(vector<Effect *> &e)
{
	clearEffects();

	for(size_t ui=0;ui<e.size();ui++)
		addEffect(e[ui]);
	e.clear();
}


unsigned int Scene::addEffect(Effect *e)
{
	ASSERT(e);
	ASSERT(effects.size() == effectIDs.size());
	effects.push_back(e);
	

	return effectIDs.genId(effects.size()-1);
}

void Scene::removeEffect(unsigned int uniqueID)
{
	unsigned int position = effectIDs.getPos(uniqueID);
	delete effects[position];
	effects.erase(effects.begin()+position);
	effectIDs.killByPos(position);
}

void Scene::clearEffects()
{
	for(size_t ui=0;ui<effects.size();ui++)
		delete effects[ui];

	effects.clear();
	effectIDs.clear();
}
