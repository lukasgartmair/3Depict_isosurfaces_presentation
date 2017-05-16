/*
 *	select.cpp  - filter selection binding implementation 
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


#include "common/mathfuncs.h"
#include "common/assertion.h"
#include "gl/drawables.h"

#include "select.h"
using std::vector;

SelectionDevice::SelectionDevice(const Filter *p) : target(p)
{
}

void SelectionDevice::addBinding(SelectionBinding b)
{
	bindingVec.push_back(b);
}

bool SelectionDevice::getBinding(const DrawableObj *d,unsigned int mouseFlags, 
					unsigned int keyFlags,SelectionBinding* &b)
{

	unsigned int keyMask=0;


	bool found=false;

	for(unsigned int ui=0;ui<bindingVec.size();ui++)
	{
		if(bindingVec[ui].matchesDrawable(d,mouseFlags,keyFlags))
		{
			if(!found)
			{
				//we found one.
				found=true;
				b=&(bindingVec[ui]);
				keyMask=b->getKeyFlags();
				continue;
			}

			//OK, we already have one, but we can be "trumped"
			//by a more complex keymask.
			if( (keyMask & bindingVec[ui].getKeyFlags() )== keyMask)
			{
				b=&(bindingVec[ui]);
				keyMask=b->getKeyFlags();
			}
		}
	}


	//This selection device does not match
	//the targeted object.
	return found;
}

void SelectionDevice::getModifiedBindings(vector<std::pair<const Filter *,SelectionBinding> > &bindings)
{
	ASSERT(target);
	for(unsigned int ui=0;ui<bindingVec.size();ui++)
	{
		if(bindingVec[ui].modified())
			bindings.push_back(std::make_pair(target,bindingVec[ui]));
	}
}

void SelectionDevice::resetModifiedBindings()
{
	ASSERT(target);
	for(unsigned int ui=0;ui<bindingVec.size();ui++)
	{
		bindingVec[ui].resetModified();
	}
}

bool SelectionDevice::getAvailBindings(const DrawableObj *d,vector<const SelectionBinding*> &b) const
{
	ASSERT(b.empty());
	for(unsigned int ui=0;ui<bindingVec.size();ui++)
	{
		if(bindingVec[ui].matchesDrawable(d))
			b.push_back(&(bindingVec[ui]));
	}


	return b.size();
}


SelectionBinding::SelectionBinding()
{
	obj=0;
	valModified=false;
	bindingId=drawActionId=0;
}

void SelectionBinding::setBinding(unsigned int button, unsigned int modifierFlags,unsigned int actionId,
					unsigned int bindID, float initValue, DrawableObj * d)
{
	//Cache the current value
	cachedValFloat = initValue;
	drawActionId=actionId;
	//Grab the object identifier itself
	obj=d;
	
	bindKeys=modifierFlags;
	bindButtons=button;
	bindingId=bindID;

	fMin=-std::numeric_limits<float>::max();
	fMax=std::numeric_limits<float>::max();

	dataType=BIND_TYPE_FLOAT;
}

void SelectionBinding::setBinding(unsigned int button, unsigned int modifierFlags, unsigned int actionId,
					unsigned int bindID, const  Point3D &initValue, DrawableObj *d)
{
	
	bindingId=bindID;
	drawActionId=actionId;
	obj=d;


	bindKeys=modifierFlags;
	bindButtons=button;

	cachedValPoint3D = initValue;

	dataType=BIND_TYPE_POINT3D;
}

void SelectionBinding::setInteractionMode(unsigned int newBindMode)
{
	//Rotation cannot have associated key flags. These are reserved
	//for changing the orientation of the rotation
	bindMode=newBindMode;
}

void SelectionBinding::setFloatLimits(float newMin,float newMax)
{
	fMin=newMin;
	fMax=newMax;
}

void SelectionBinding::applyTransform(const Point3D &worldVec, bool permanent)
{
	vector<float> scalars;
	vector<Point3D> vecs;
	float fTmp;
	switch(bindMode)
	{
		case BIND_MODE_FLOAT_SCALE:
		{
			//Compute the new scalar as the magnitude of the difference vector
			fTmp = sqrtf(worldVec.sqrMag());
			fTmp = std::max(fMin,fTmp);
			fTmp = std::min(fMax,fTmp);

			scalars.push_back(fTmp);
			break;
		}
		case BIND_MODE_FLOAT_TRANSLATE:
		{
			//Compute the new scalar as an offset by the mag of the scalar
			fTmp =0.5*cachedValFloat+sqrtf(worldVec.sqrMag()); 
			fTmp = std::max(fMin,fTmp);
			fTmp = std::min(fMax,fTmp);
			
			scalars.push_back(fTmp);
			cachedValFloat=fTmp;
			break;
		}
		case BIND_MODE_POINT3D_TRANSLATE:
		case BIND_MODE_POINT3D_SCALE:
		{
			vecs.push_back(cachedValPoint3D+worldVec);

			//Only apply if this is a permanent change,
			//otherwise we will get an integrating effect
			if(permanent)
				cachedValPoint3D+=worldVec;
			break;
		}
		case BIND_MODE_POINT3D_ROTATE:
		{
			if(worldVec.sqrMag() > sqrtf(std::numeric_limits<float>::epsilon()))
			{
				vecs.push_back(worldVec);
				cachedValPoint3D = worldVec;
			}

			break;
		}
		case BIND_MODE_POINT3D_ROTATE_LOCK:
		{
			if(worldVec.sqrMag() > sqrtf(std::numeric_limits<float>::epsilon()))
			{
				//Renormalise the vector back to the same scale as the cached value
				vecs.push_back(worldVec*sqrtf(cachedValPoint3D.sqrMag()/worldVec.sqrMag()));
				if(permanent)
					cachedValPoint3D=vecs.back();
			}

			break;
		}
		default:
			ASSERT(false);
	}

	if(vecs.size() || scalars.size())
	{
		//Force a recomputation of the internal parameters 
		//for the drawable object. Whatever they are.
		obj->recomputeParams(vecs,scalars,drawActionId);

		valModified=true;
	}
}

void SelectionBinding::computeWorldVectorCoeffs(unsigned int buttonFlags, 
			unsigned int modifierFlags,Point3D &xCoeffs,Point3D &yCoeffs) const

{
	switch(bindMode)
	{
		case BIND_MODE_FLOAT_TRANSLATE:
		case BIND_MODE_FLOAT_SCALE:
			//It is of no concern. we are going to pass this to sqrmag
			//anyway during applyTransform.
			xCoeffs=Point3D(1,0,0);
			yCoeffs=Point3D(0,1,0);
			break;
		case BIND_MODE_POINT3D_TRANSLATE:
		case BIND_MODE_POINT3D_SCALE:
		case BIND_MODE_POINT3D_ROTATE:
		case BIND_MODE_POINT3D_ROTATE_LOCK:
		{
			if(modifierFlags == FLAG_CMD && bindKeys!=FLAG_CMD)
			{
				//Mouse movement in x sends you forwards
				//y movement sends you up down (wrt camera)
				xCoeffs=Point3D(0,0,1);
				yCoeffs=Point3D(0,1,0);
			}
			else if(modifierFlags == FLAG_SHIFT && bindKeys != FLAG_SHIFT)
			{
				//Mouse movement in x sends you across
				//y movement sends you forwards (wrt camera)
				xCoeffs=Point3D(1,0,0);
				yCoeffs=Point3D(0,0,1);
			}
			else
			{
				//For example: FLAG_NONE
				//IN plane with camera.
				xCoeffs=Point3D(1,0,0);
				yCoeffs=Point3D(0,1,0);
			}
			break;
		}
		default:
			ASSERT(false);
	}
}


void SelectionBinding::getValue(float &f) const
{
	f=cachedValFloat;
}

void SelectionBinding::getValue(Point3D &f) const
{
	f=cachedValPoint3D;
}

bool SelectionBinding::matchesDrawable(const DrawableObj *d,
			unsigned int mouseFlags, unsigned int keyFlags) const
{
	//Object and mouseflags must match. keyflags must be nonzero after masking with bindKeys
	if(bindKeys)
		return (obj == d && mouseFlags == bindButtons && (keyFlags &bindKeys) == bindKeys);
	else
		return (obj == d && mouseFlags == bindButtons);
};


bool SelectionBinding::matchesDrawable(const DrawableObj *d) const
{
	return (obj == d);
}


