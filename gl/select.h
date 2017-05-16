/*
 * 	select,h - Opengl interaction header. 
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


#ifndef SELECT_H
#define SELECT_H

#include <vector>

class DrawableObj;
class Filter;
class Point3D;

//Mouse button flags
enum
{
	SELECT_BUTTON_LEFT=1,
	SELECT_BUTTON_MIDDLE=2,
	SELECT_BUTTON_RIGHT=4
};

//!Keyboard keydown flags
enum
{
	FLAG_NONE=0,
	FLAG_CMD=1, //control (non-mac) or "clover" key (mac)
	FLAG_SHIFT=2, //Left or right shift key.
};

//!Allowable binding modes
enum
{
	BIND_MODE_FLOAT_SCALE, //Object scaling only (fp value)
	BIND_MODE_FLOAT_TRANSLATE, //Floating point translation
	BIND_MODE_POINT3D_TRANSLATE, //3D point translation in 2D plane perpendicular to camera
	BIND_MODE_POINT3D_SCALE, //3D point translation in 2D plane perpendicular to camera; but indicate to user that this performs some kind of scaling operation
	BIND_MODE_POINT3D_ROTATE, //3D rotation in 2D plane perpendicular to camera
	BIND_MODE_POINT3D_ROTATE_LOCK, // 3D rotation in 2D plane perpendicular to camera, but with locked magnitude
};

//!Bindable data types (data types that SelectionBinding can work with)
enum
{
	BIND_TYPE_FLOAT,
	BIND_TYPE_POINT3D
};

//!This class is used to pool together a graphical representation (via the drawable), of
//an object with its internal data structural representation. This allows the user
//to grapple with the drawable representation and feed this into the scene.
//This class binds ONE drawable object to a set of actions based upon key and button combinations.
class SelectionBinding
{
	private:
		//Pointer to drawable that generates selection events. 
		//calls recomputeParams function
		DrawableObj *obj;
		
		//ID number for parent to know which of its bindings this is
		unsigned int bindingId;

		//ID number to bind the action for the drawable object
		unsigned int drawActionId;

		//Binding type
		unsigned int dataType;


		//Binding button (ORed together)
		unsigned int bindButtons;
		
		//Binding key (ORed together)
		unsigned int bindKeys;

		//Binding mode
		unsigned int bindMode;

		//Original value of data type (probably more mem efficient ot use a void*...)
		float cachedValFloat;
		Point3D cachedValPoint3D;

		bool valModified;

		//limits in floating point
		float fMin,fMax;

	public:
		SelectionBinding();

		//!Returns true if this binding will be activated given the current flags
		bool isActive(unsigned int button,unsigned int curModifierFlags);

		//!Set the binding for a float DO NOT CACHE THE DRAWABLEOBJ-> THAT IS BAD
		void setBinding(unsigned int buttonFlags, unsigned int modifierFlags,
				unsigned int drawActionId, unsigned int bindingID, 
							float initVal, DrawableObj *d);

		//!Set the binding for a Point3D. DO NOT CACHE THE DRAWABLEOBJ-> THAT IS BAD
		void setBinding(unsigned int buttonFlags, unsigned int modifierFlags,
				unsigned int drawActionId,unsigned int bindingID,
			       			const Point3D &initVal, DrawableObj *d);

		//!Set the interaction method. (example translate, scale, rotate etc)
		void setInteractionMode(unsigned int bindMode);
		
		//!Get the interaction mode 
		unsigned int getInteractionMode() const { return bindMode;};
		
		//!Get the mouse button
		unsigned int getMouseButtons() const { return bindButtons;};
		
		//!Get the mouse button
		unsigned int getKeyFlags() const { return bindKeys;};

		//!Set the limits for a floating point data type
		void setFloatLimits(float min,float max);

		//!Is this binding for the following object?
		bool matchesDrawable(const DrawableObj *d, 
				unsigned int mouseFlags, unsigned int keyFlags)  const;
		//!Is this binding for the following object?
		bool matchesDrawable(const DrawableObj *d) const;

		//!Apply the user ineraction specified. set permanent=true to
		//make it such that this is not undone during the next transform, 
		//or call to reset()
		//worldvec is the vector along which to transform the object (subject to
		//interpretation by the "interaction mode" (bindmode) setting)
		void applyTransform(const Point3D &worldVec,bool permanent=false);


		//!Map the screen coords world coords, given the mouse and keyflags
		//coeffs are 0: right 1: forwards 2: up ( right hand rule)
		void computeWorldVectorCoeffs(unsigned int buttonFlags, 
				unsigned int modifierFlags,Point3D &xCoeffs,Point3D  &yCoeffs) const;

		//!Retrieve the current value from the drawable representation
		void getValue(float &f) const;
		//!Retreive the current value from the drawable representation
		void getValue(Point3D &p) const;

		unsigned int getID() const { return bindingId;};

		//!True if the binding has modified the data
		bool modified() const {return valModified;};

		void resetModified() { valModified=false; }
};

class SelectionDevice
{
	private:
		std::vector<SelectionBinding> bindingVec;
		const Filter *target;
public:
		//!Create a new selection device
		SelectionDevice(const Filter *p);

		//!Copy constructor (not implemented)
		SelectionDevice(const SelectionDevice &copySrc);
	
		//!Bind a floating point type between the graphical and internal reps.
		//note that it is a BUG to attempt to bind any object that uses a
		//display list in its internal representation. 
		void addBinding(SelectionBinding b);

		bool getBinding(const DrawableObj *d, unsigned int mouseFlags, 
				unsigned int keyFlags,	SelectionBinding* &b);

		bool getAvailBindings(const DrawableObj *d, std::vector<const SelectionBinding*> &b) const;

		void getModifiedBindings(std::vector<std::pair<const Filter *,SelectionBinding> > &bindings);
		//!Return any devices that have been modified since their creation
		void resetModifiedBindings() ;
			
		size_t getNumBindings() const { return bindingVec.size(); }
};

#endif

