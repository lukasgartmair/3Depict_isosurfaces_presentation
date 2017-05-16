/*
 * 	scene.h - Opengl interaction header. 
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
#ifndef SCENE_H
#define SCENE_H


class Scene;
class VisController;
class Filter;

#include "drawables.h"

//Custom includes
#include "effect.h"

#include "glDebug.h"

#include <vector>

//!The scene class brings together elements such as objects, lights, and cameras
//to enable scene rendering
class Scene
{
	private:
		bool glewInited;
		//!Viscontroller. Needed for notification of updates during selection binding
		VisController *visControl;
		//!Objects that will be used for drawing
		std::vector<DrawableObj const * > objects;
		
		//!Objects used for drawing that will not be destroyed
		std::vector<const DrawableObj * > refObjects;

		//!Various OpenGL effects
		std::vector<const Effect *> effects;

		Camera *activeCam;

		//!Temporary override camera
		Camera *tempCam;

		//!Texture pool
		TexturePool texPool;

		//!Size of window in px (needed if doing 2D drawing)
		unsigned int winX,winY;

		//!Is there a camera set?
		bool cameraSet;
		//!Aspect ratio of output window (x/y) -- needed for cams
		float outWinAspect;

		//!Effect ID handler
		UniqueIDHandler effectIDs;

		//!Cube that holds the scene bounds
		BoundCube boundCube;


		//!True if user interaction (selection/hovering) is forbidden
		bool lockInteract;
		//!Tells the scene if we are in selection mode or not
		bool selectionMode;

		//!Tells us if we are in hover mode (should we draw hover overlays?)
		bool hoverMode;

		//!Last selected object from call to glSelect(). -1 if last
		// call failed to identify an item
		unsigned int lastSelected;

		//Prevent camera updates from being passed to opengl
		bool witholdCamUpdate;

		//!Last hovered object	
		unsigned int lastHovered;

		//!Should alpha blending be used?
		bool useAlpha;
		//!Should lighting calculations be performed?
		bool useLighting;
		//!Should we be using effects?
		bool useEffects;

		//!Should the world axis be drawn?
		bool showAxis;

		//!Background colour
		float rBack,gBack,bBack;

		//!Have we attempted to load the progress animation
		bool attemptedLoadProgressAnim;

		//texture to use for progress animation
		DrawAnimatedOverlay progressAnimTex;

		//Lighting vector
		float lightPosition[4];

		
		///!Draw the hover overlays
		void drawHoverOverlay();


		void drawProgressAnim() const;

		//!initialise the drawing window
		unsigned int initDraw();

		void updateCam(const Camera *camToUse, bool loadIdentity) const;

		//reset the position of the overlay
		void updateProgressOverlay(); 

		//!Draw a specified vector of objects
		void drawObjectVector(const std::vector<const DrawableObj*> &objects, bool &lightsOn, bool drawOpaques=true) const;

		//!Disable copy constructor by making private
		Scene &operator=(const Scene &);
				
	public:
		DrawProgressCircleOverlay progressCircle;
		
		//!Constructor
		Scene();
		//!Destructor
		virtual ~Scene();

		//!Set the vis control
		void setVisControl(VisController *v) { visControl=v;};
		//!Draw the objects in the active window. May adjust cameras and compute bounding as needed.
		void draw(bool noUpdateCam=false);

		//!Draw the normal overlays
		void drawOverlays(bool noCamUpdate=false) const;
	
		//!clear rendering vectors
		void clearAll();
		//!Clear drawing objects vector
		void clearObjs();
		//! Clear the reference object vector
		void clearRefObjs();

		//!Do we have overlay items?
		bool hasOverlays() const;	

		//!Obtain the scene's light coordinates in camera relative space
		// requires an array of size 4  (xyzw)
		void getLightPos(float *f) const;
		
		//!Obtain the scene's light coordinates in camera relative space
		// requires an array of size 4  (xyzw)
		void setLightPos(const float *f);

		//!Set the aspect ratio of the output window. Required.
		void setAspect(float newAspect);
		//!retrieve aspect ratio (h/w) of output win
		float getAspect() const { return outWinAspect;};
		
		//!Add a drawable object 
		/*!Pointer must be set to a valid (allocated) object.
		 *!Scene will delete upon call to clearAll, clearObjs or
		 *!upon destruction
		 */
		void addDrawable(const DrawableObj *);
		
		//!Add a drawable to the reference only section
		/* Objects referred to will not be modified or destroyed
		 * by this class. It will only be used for drawing purposes
		 * It is up to the user to ensure that they are in a good state
		 */
		void addRefDrawable(const DrawableObj *);
	

		bool setProgressAnimation(const std::vector<std::string> &animFiles);

		void resetProgressAnim() ;


		//!remove a drawable object
		void removeDrawable(unsigned int);

		//!Set the active camera directly
		// note that the pointer becomes "owned" by the scene.
		// any previous active camera will be deleted
		void setActiveCam(Camera *c);
		//! set the active camera
		void setActiveCamByClone(const Camera *c);

		//! get the active camera
		Camera *getActiveCam() ;

		//! get the active camera's location
		Point3D getActiveCamLoc() const;

		//!Construct (or refresh) a temporary camera
		/*! this temporary camera is discarded  with 
		 * either killTempCam or reset to the active
		 * camera with another call to setTempCam().
		 * The temporary camera overrides the existing camera setup
		 */ 
		void setTempCam();

		//!Return pointer to active camera. Must init a temporary camera first!  (use setTempCam)
		Camera *getTempCam() ;

		//!Make the temp camera permanent.
		void commitTempCam();
		
		//!Discard the temporary camera
		void discardTempCam();

		//!Are we using a temporary camera?
		bool haveTempCam() const { return tempCam!=0;};

		//!Clone the active camera
		Camera *cloneActiveCam() const { return activeCam->clone(); };


		//!Modify the active camera position to ensure that scene is visible 
		void ensureVisible(unsigned int direction);

		//!Call if user has stopped interacting with camera briefly.
		void finaliseCam();

		//!perform an openGL selection rendering pass. Return 
		//closest object in depth buffer under position 
		//if nothing, returns -1
		unsigned int glSelect(bool storeSelection=true);

		//!Clear the current selection devices 
		void clearDevices();

		//!Apply the device given the following start and end 
		//viewport coordinates.
		void applyDevice(float startX, float startY,
					float curX, float curY,unsigned int keyFlags, 
					unsigned int mouseflags,bool permanent=true);

		// is interaction currently locked?
		bool isInteractionLocked()  const { return lockInteract;}
		//!Prevent user interaction
		void lockInteraction(bool amLocking=true) { lockInteract=amLocking;};
		//!Set selection mode true=select on, false=select off.
		//All this does internally is modify how drawing works.
		void setSelectionMode(bool selMode) { selectionMode=selMode;};

		//!Set the hover mode to control drawing
		void setHoverMode(bool hMode) { hoverMode=hMode;};

		//!Return the last object over which the cursor was hovered	
		void setLastHover(unsigned int hover) { lastHovered=hover;};
		//!Get the last selected object from call to glSelect()
		unsigned int getLastSelected() const { return lastSelected;};
	
		//!Return the last object over which the cursor was hovered	
		unsigned int getLastHover() const { return lastHovered;};
		//!Duplicates the internal camera vector. return value is active camera
		//in returned vector
		unsigned int duplicateCameras(std::vector<Camera *> &cams) const; 
		//!Get a copy of the effects pointers
		void getEffects(std::vector<const Effect *> &effects) const; 

		//!Set whether to use alpha blending
		void setAlpha(bool newAlpha) { useAlpha=newAlpha;};

		//!Set whether to enable lighting
		void setLighting(bool newLight) { useLighting=newLight;};

		//!Set whether to enable the XYZ world axes
		void setWorldAxisVisible(bool newAxis) { showAxis=newAxis;};
		//!Get whether the XYZ world axes are enabled
		bool getWorldAxisVisible() const { return showAxis;};

		//!Set window size
		void setWinSize(unsigned int x, unsigned int y) {winX=x;winY=y; updateProgressOverlay();}

		//!Get the scene bounding box
		BoundCube getBound() const { return boundCube;}

		//!Set the background colour
		void setBackgroundColour(float newR,float newG,float newB) { rBack=newR;gBack=newG;bBack=newB;};

		void getBackgroundColour(float &newR,float &newG,float &newB) const { newR=rBack;newG=gBack;newB=bBack;};
		
		//!Computes the bounding box for the scene. 
		//this is locked to a minimum of 0.1 unit box around the origin.
		//this avoids nasty camera situations, where lookat cameras are sitting
		//on their targets, and don't know where to look.
		void computeSceneLimits();
		
		//!Set whether to use effects or not
		void setEffects(bool enable) {useEffects=enable;} 

		//!Set the effect vector
		/*! Pointers will become "owned" by scene
		 * and will be deleted during destruction, clear, or next setEffectVec call
		 * input vector will be cleared.
		 */
		void setEffectVec(std::vector<Effect *> &e);

		//!Add an effect
		unsigned int addEffect(Effect *e);
		//!Remove a given effect
		void removeEffect(unsigned int uniqueEffectID);

		//!Clear effects vector
		void clearEffects();

		static std::string getGlVersion() { return  std::string((char *)glGetString(GL_VERSION)); }
};

#endif
