/*
 *	ionClip.cpp -3Depict filter to perform clipping of 3D point clouds
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
#include "ionClip.h"


#include "geometryHelpers.h"
#include "filterCommon.h"

#include <map>

using std::vector;
using std::string;
using std::pair;
using std::make_pair;
using std::map;

//!Error codes
enum 
{
	CALLBACK_FAIL=1,
	BAD_ALLOC,
	IONCLIP_ERR_ENUM_END
};

//!Possible primitive types for ion clipping
enum
{
	PRIMITIVE_SPHERE,
	PRIMITIVE_PLANE,
	PRIMITIVE_CYLINDER,
	PRIMITIVE_AAB, //Axis aligned box

	PRIMITIVE_END //Not actually a primitive, just end of enum
};

enum {
	KEY_ORIGIN=1,
	KEY_PRIMITIVE_TYPE,
	KEY_RADIUS,
	KEY_PRIMITIVE_SHOW,
	KEY_PRIMITIVE_INVERTCLIP,
	KEY_NORMAL,
	KEY_CORNER,
	KEY_AXIS_LOCKMAG,
};



const char *PRIMITIVE_NAMES[]  = {
				 NTRANS("Sphere"),
				 NTRANS("Plane"),
				 NTRANS("Cylinder"),
				 NTRANS("Aligned box")
				};



unsigned int primitiveID(const std::string &str)
{
	for(unsigned int ui=0;ui<PRIMITIVE_END;ui++)
	{
		if(str == TRANS(PRIMITIVE_NAMES[ui]))
			return ui;
	}

	ASSERT(false);
	return (unsigned int)-1;
}


std::string primitiveStringFromID(unsigned int id)
{
	ASSERT(id< PRIMITIVE_END);
	return string(TRANS(PRIMITIVE_NAMES[id]));
}

IonClipFilter::IonClipFilter() : primitiveType(PRIMITIVE_PLANE),
	invertedClip(false),showPrimitive(true), lockAxisMag(false)
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(PRIMITIVE_NAMES) == PRIMITIVE_END);

	vectorParams.push_back(Point3D(0.0,0.0,0.0));
	vectorParams.push_back(Point3D(0,1.0,0.0));
};

Filter *IonClipFilter::cloneUncached() const
{
	IonClipFilter *p = new IonClipFilter;
	p->primitiveType=primitiveType;
	p->invertedClip=invertedClip;
	p->showPrimitive=showPrimitive;
	p->vectorParams.resize(vectorParams.size());
	p->scalarParams.resize(scalarParams.size());
	
	std::copy(vectorParams.begin(),vectorParams.end(),p->vectorParams.begin());
	std::copy(scalarParams.begin(),scalarParams.end(),p->scalarParams.begin());

	p->lockAxisMag = lockAxisMag;

	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;

	return p;
}

//!Get approx number of bytes for caching output
size_t IonClipFilter::numBytesForCache(size_t nObjects) const
{
	//Without full processing, we cannot tell, so provide upper estimate.
	return nObjects*IONDATA_SIZE;
}

//!update filter
unsigned int IonClipFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
			std::vector<const FilterStreamData *> &getOut, ProgressData &progress) 
{
	ASSERT(vectorParams.size() || scalarParams.size());	
	//Clear selection devices, first deleting any we have
	clearDevices();

	if(showPrimitive)
	{
		//TODO: This is a near-copy of compositionProfile.cpp - refactor
		//construct a new primitive, do not cache
		DrawStreamData *drawData=new DrawStreamData;
		drawData->parent =this;
		switch(primitiveType)
		{
			case PRIMITIVE_SPHERE:
			{
				//Add drawable components
				DrawSphere *dS = new DrawSphere;
				dS->setOrigin(vectorParams[0]);
				dS->setRadius(scalarParams[0]);
				//FIXME: Alpha blending is all screwed up. May require more
				//advanced drawing in scene. (front-back drawing).
				//I have set alpha=1 for now.
				dS->setColour(0.5,0.5,0.5,1.0);
				dS->setLatSegments(40);
				dS->setLongSegments(40);
				dS->wantsLight=true;
				drawData->drawables.push_back(dS);

				//Set up selection "device" for user interaction
				//Note the order of s->addBinding is critical,
				//as bindings are selected by first match.
				//====
				//The object is selectable
				dS->canSelect=true;

				SelectionDevice *s = new SelectionDevice(this);
				SelectionBinding b[3];

				//Apple doesn't have right click, so we need
				//to hook up an additional system for them.
				//Don't use ifdefs, as this would be useful for
				//normal laptops and the like.
				b[0].setBinding(SELECT_BUTTON_LEFT,FLAG_CMD,DRAW_SPHERE_BIND_ORIGIN,
							BINDING_SPHERE_ORIGIN,dS->getOrigin(),dS);
				b[0].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[0]);

				//Bind the drawable object to the properties we wish
				//to be able to modify
				b[1].setBinding(SELECT_BUTTON_LEFT,0,DRAW_SPHERE_BIND_RADIUS,
					BINDING_SPHERE_RADIUS,dS->getRadius(),dS);
				b[1].setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
				b[1].setFloatLimits(0,std::numeric_limits<float>::max());
				s->addBinding(b[1]);

				b[2].setBinding(SELECT_BUTTON_RIGHT,0,DRAW_SPHERE_BIND_ORIGIN,
					BINDING_SPHERE_ORIGIN,dS->getOrigin(),dS);	
				b[2].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[2]);
					
				devices.push_back(s);
				//=====
				break;
			}
			case PRIMITIVE_PLANE:
			{
				const float drawScale=10.0f;
				//Origin + normal
				ASSERT(vectorParams.size() == 2);

				//Add drawable components
				DrawSphere *dS = new DrawSphere;
				dS->setOrigin(vectorParams[0]);
				dS->setRadius(drawScale/10);
				dS->setColour(0.5,0.5,0.5,1.0);
				dS->setLatSegments(40);
				dS->setLongSegments(40);
				dS->wantsLight=true;
				drawData->drawables.push_back(dS);
				
				DrawVector *dV  = new DrawVector;
				dV->setOrigin(vectorParams[0]);
				dV->setVector(vectorParams[1]*drawScale);
				dV->wantsLight=true;
				drawData->drawables.push_back(dV);
				
				//Set up selection "device" for user interaction
				//====
				//The object is selectable
				dS->canSelect=true;
				dV->canSelect=true;

				SelectionDevice *s = new SelectionDevice(this);
				SelectionBinding b[2];
				//Bind the drawable object to the properties we wish
				//to be able to modify

				//Bind orientation to vector left click
				b[0].setBinding(SELECT_BUTTON_LEFT,0,DRAW_VECTOR_BIND_ORIENTATION,
					BINDING_PLANE_DIRECTION, dV->getVector(),dV);
				b[0].setInteractionMode(BIND_MODE_POINT3D_ROTATE);
				b[0].setFloatLimits(0,std::numeric_limits<float>::max());
				s->addBinding(b[0]);
				
				//Bind translation to sphere left click
				b[1].setBinding(SELECT_BUTTON_LEFT,0,DRAW_SPHERE_BIND_ORIGIN,
						BINDING_PLANE_ORIGIN,dS->getOrigin(),dS);	
				b[1].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[1]);


				devices.push_back(s);
				//=====
				break;
			}
			case PRIMITIVE_CYLINDER:
			{
				//Origin + normal
				ASSERT(vectorParams.size() == 2);
				//Add drawable components
				DrawCylinder *dC = new DrawCylinder;
				dC->setOrigin(vectorParams[0]);
				dC->setRadius(scalarParams[0]);
				dC->setColour(0.5,0.5,0.5,1.0);
				dC->setSlices(40);
				dC->setLength(sqrtf(vectorParams[1].sqrMag()));
				dC->setDirection(vectorParams[1]);
				dC->wantsLight=true;
				drawData->drawables.push_back(dC);
				
				//Set up selection "device" for user interaction
				//====
				//The object is selectable
				dC->canSelect=true;
				//Start and end radii must be the same (not a
				//tapered cylinder)
				dC->lockRadii();

				SelectionDevice *s = new SelectionDevice(this);
				SelectionBinding b;
				//Bind the drawable object to the properties we wish
				//to be able to modify

				//Bind left + command button to move
				b.setBinding(SELECT_BUTTON_LEFT,FLAG_CMD,DRAW_CYLINDER_BIND_ORIGIN,
					BINDING_CYLINDER_ORIGIN,dC->getOrigin(),dC);	
				b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b);

				//Bind left + shift to change orientation
				b.setBinding(SELECT_BUTTON_LEFT,FLAG_SHIFT,DRAW_CYLINDER_BIND_DIRECTION,
					BINDING_CYLINDER_DIRECTION,dC->getDirection(),dC);	

				if(lockAxisMag)
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE_LOCK);
				else
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE);
				s->addBinding(b);

				//Bind right button to changing position 
				b.setBinding(SELECT_BUTTON_RIGHT,0,DRAW_CYLINDER_BIND_ORIGIN,
					BINDING_CYLINDER_ORIGIN,dC->getOrigin(),dC);	
				b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b);
					
				//Bind middle button to changing orientation
				b.setBinding(SELECT_BUTTON_MIDDLE,0,DRAW_CYLINDER_BIND_DIRECTION,
					BINDING_CYLINDER_DIRECTION,dC->getDirection(),dC);	
				if(lockAxisMag)
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE_LOCK);
				else
					b.setInteractionMode(BIND_MODE_POINT3D_ROTATE);
				s->addBinding(b);
					
				//Bind left button to changing radius
				b.setBinding(SELECT_BUTTON_LEFT,0,DRAW_CYLINDER_BIND_RADIUS,
					BINDING_CYLINDER_RADIUS,dC->getRadius(),dC);
				b.setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
				b.setFloatLimits(0,std::numeric_limits<float>::max());
				s->addBinding(b); 
				
				devices.push_back(s);
				//=====
				
				break;
			}
			case PRIMITIVE_AAB:
			{
				//Centre  + corner
				ASSERT(vectorParams.size() == 2);
				ASSERT(scalarParams.size() == 0);

				//Add drawable components
				DrawRectPrism *dR = new DrawRectPrism;
				dR->setAxisAligned(vectorParams[0]-vectorParams[1],
							vectorParams[0] + vectorParams[1]);
				dR->setColour(0.5,0.5,0.5,1.0);
				dR->setDrawMode(DRAW_FLAT);
				dR->wantsLight=true;
				drawData->drawables.push_back(dR);
				
				
				//Set up selection "device" for user interaction
				//====
				//The object is selectable
				dR->canSelect=true;

				SelectionDevice *s = new SelectionDevice(this);
				SelectionBinding b[2];
				//Bind the drawable object to the properties we wish
				//to be able to modify

				//Bind orientation to sphere left click
				b[0].setBinding(SELECT_BUTTON_LEFT,0,DRAW_RECT_BIND_TRANSLATE,
					BINDING_RECT_TRANSLATE, vectorParams[0],dR);
				b[0].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b[0]);


				b[1].setBinding(SELECT_BUTTON_RIGHT,0,DRAW_RECT_BIND_CORNER_MOVE,
						BINDING_RECT_CORNER_MOVE, vectorParams[1],dR);
				b[1].setInteractionMode(BIND_MODE_POINT3D_SCALE);
				s->addBinding(b[1]);

				devices.push_back(s);
				//=====
				break;
			}
			default:
				ASSERT(false);

		}
	
		drawData->cached=0;	
		getOut.push_back(drawData);
	}

	//use the cached copy of the data if we have it.
	if(cacheOK)
	{
		for(unsigned int ui=0;ui<filterOutputs.size(); ui++)
			getOut.push_back(filterOutputs[ui]);

		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			//We don't cache anything but our modification
			//to the ion stream data types. so we propagate
			//these.
			if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
				getOut.push_back(dataIn[ui]);
		}
			
		progress.filterProgress=100;
		return 0;
	}


	IonStreamData *d=0;
	try
	{

		std::map<pair<size_t,bool>,size_t> primitiveTypeMap;

		using std::make_pair;
		//map the primitive enum type, and the clip inversion state
		// to the CropHelper clip mode
		primitiveTypeMap[make_pair((size_t)PRIMITIVE_SPHERE,false)]=CROP_SPHERE_INSIDE;
		primitiveTypeMap[make_pair((size_t)PRIMITIVE_SPHERE,true)]=CROP_SPHERE_OUTSIDE;
		primitiveTypeMap[make_pair((size_t)PRIMITIVE_PLANE,false)]=CROP_PLANE_FRONT;
		primitiveTypeMap[make_pair((size_t)PRIMITIVE_PLANE,true)]=CROP_PLANE_BACK;
		primitiveTypeMap[make_pair((size_t)PRIMITIVE_CYLINDER,false)]=CROP_CYLINDER_INSIDE_AXIAL;
		primitiveTypeMap[make_pair((size_t)PRIMITIVE_CYLINDER,true)]=CROP_CYLINDER_OUTSIDE;
		primitiveTypeMap[make_pair((size_t)PRIMITIVE_AAB,false)]=CROP_AAB_INSIDE;
		primitiveTypeMap[make_pair((size_t)PRIMITIVE_AAB,true)]=CROP_AAB_OUTSIDE;

		size_t mode;
		mode = primitiveTypeMap[make_pair(primitiveType,invertedClip)];
		size_t totalSize=numElements(dataIn);
		CropHelper cropper(totalSize,mode,vectorParams,scalarParams  );

		float minProg,maxProg;
		size_t cumulativeSize=0;
		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			//Loop through each data set
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
					d=new IonStreamData;
					d->parent=this;
					minProg=cumulativeSize/(float)totalSize;
					cumulativeSize+=d->data.size();
					maxProg=cumulativeSize/(float)totalSize;

					//Filter input data to output data.
					if(cropper.runFilter(((const IonStreamData *)dataIn[ui])->data,
						d->data,minProg,maxProg,progress.filterProgress))
					{
						delete d;
						return CALLBACK_FAIL; 
					}

					if(d->data.size())
					{
						//Copy over other attributes
						d->r = ((IonStreamData *)dataIn[ui])->r;
						d->g = ((IonStreamData *)dataIn[ui])->g;
						d->b =((IonStreamData *)dataIn[ui])->b;
						d->a =((IonStreamData *)dataIn[ui])->a;
						d->ionSize =((IonStreamData *)dataIn[ui])->ionSize;

						//getOut is const, so shouldn't be modified
						cacheAsNeeded(d);

						getOut.push_back(d);
						d=0;
					}
					else
						delete d;
					break;
				}
				default:
					//Just copy across the ptr, if we are unfamiliar with this type
					getOut.push_back(dataIn[ui]);	
					break;
			}
		}
	}
	catch(std::bad_alloc)
	{
		if(d)
			delete d;
		return BAD_ALLOC;
	}

	progress.filterProgress=100;
	return 0;

}

//!Get the properties of the filter, in key-value form. First vector is for each output.
void IonClipFilter::getProperties(FilterPropGroup &propertyList) const
{
	ASSERT(vectorParams.size() || scalarParams.size());	

	FilterProperty p;
	
	size_t curGroup=0; 
	//Let the user know what the valid values for Primitive type
	string tmpStr;

	vector<pair<unsigned int,string> > choices;

	choices.push_back(make_pair((unsigned int)PRIMITIVE_SPHERE ,
				primitiveStringFromID(PRIMITIVE_SPHERE)));
	choices.push_back(make_pair((unsigned int)PRIMITIVE_PLANE ,
				primitiveStringFromID(PRIMITIVE_PLANE)));
	choices.push_back(make_pair((unsigned int)PRIMITIVE_CYLINDER ,
				primitiveStringFromID(PRIMITIVE_CYLINDER)));
	choices.push_back(make_pair((unsigned int)PRIMITIVE_AAB,
				primitiveStringFromID(PRIMITIVE_AAB)));

	tmpStr= choiceString(choices,primitiveType);
	p.name=TRANS("Primitive");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Shape of clipping object");
	p.key=KEY_PRIMITIVE_TYPE;
	propertyList.addProperty(p,curGroup);
	
	stream_cast(tmpStr,showPrimitive);
	p.key=KEY_PRIMITIVE_SHOW;
	p.name=TRANS("Show Primitive");
	p.data= tmpStr;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Display the 3D interaction object");
	propertyList.addProperty(p,curGroup);
	
	stream_cast(tmpStr,invertedClip);
	p.key=KEY_PRIMITIVE_INVERTCLIP;
	p.name=TRANS("Invert Clip");
	p.data= tmpStr;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Switch between retaining points inside (false) and outside (true) of primitive");
	propertyList.addProperty(p,curGroup);

	switch(primitiveType)
	{
		case PRIMITIVE_SPHERE:
		{
			ASSERT(vectorParams.size() == 1);
			ASSERT(scalarParams.size() == 1);
			stream_cast(tmpStr,vectorParams[0]);
			p.key=KEY_ORIGIN;
			p.name=TRANS("Origin");
			p.data= tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Position for centre of sphere");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(tmpStr,scalarParams[0]);
			p.key=KEY_RADIUS;
			p.name=TRANS("Radius");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Radius of sphere");
			propertyList.addProperty(p,curGroup);

			break;
		}
		case PRIMITIVE_PLANE:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 0);
			stream_cast(tmpStr,vectorParams[0]);
			p.key=KEY_ORIGIN;
			p.name=TRANS("Origin");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Position that plane passes through");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(tmpStr,vectorParams[1]);
			p.key=KEY_NORMAL;
			p.name=TRANS("Plane Normal");
			p.data= tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Perpendicular direction for plane");
			propertyList.addProperty(p,curGroup);

			break;
		}
		case PRIMITIVE_CYLINDER:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			stream_cast(tmpStr,vectorParams[0]);
			p.key=KEY_ORIGIN;
			p.name=TRANS("Origin");
			p.data= tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Centre of cylinder");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(tmpStr,vectorParams[1]);
			p.key=KEY_NORMAL;
			p.name=TRANS("Axis");
			p.data= tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Positive vector for cylinder");
			propertyList.addProperty(p,curGroup);
			
			tmpStr=boolStrEnc(lockAxisMag);
			p.key=KEY_AXIS_LOCKMAG;
			p.name=TRANS("Lock Axis Mag.");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Prevent changing length of cylinder during 3D interaction");
			propertyList.addProperty(p,curGroup);

			stream_cast(tmpStr,scalarParams[0]);
			p.key=KEY_RADIUS;
			p.name=TRANS("Radius");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Radius of cylinder");
			propertyList.addProperty(p,curGroup);
			break;
		}
		case PRIMITIVE_AAB:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 0);
			stream_cast(tmpStr,vectorParams[0]);
			p.key=KEY_ORIGIN;
			p.name=TRANS("Origin");
			p.data= tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Centre of axis aligned box");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(tmpStr,vectorParams[1]);
			p.key=KEY_CORNER;
			p.name=TRANS("Corner offset");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Vector to corner of box");
			propertyList.addProperty(p,curGroup);
			break;
		}
		default:
			ASSERT(false);
	}

	propertyList.setGroupTitle(curGroup,TRANS("Clipping"));
}

//!Set the properties for the nth filter. Returns true if prop set OK
bool IonClipFilter::setProperty(unsigned int key, 
				const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_PRIMITIVE_TYPE:
		{
			unsigned int newPrimitive;

			newPrimitive=primitiveID(value);
			if(newPrimitive == (unsigned int)-1)
				return false;	

			primitiveType=newPrimitive;

			switch(primitiveType)
			{
				//If we are switching between essentially
				//similar data types, don't reset the data.
				//Otherwise, wipe it and try again
				case PRIMITIVE_SPHERE:
					if(vectorParams.size() !=1)
					{
						vectorParams.clear();
						vectorParams.push_back(Point3D(0,0,0));
					}
					if(scalarParams.size()!=1)
					{
						scalarParams.clear();
						scalarParams.push_back(10.0f);
					}
					break;
				case PRIMITIVE_PLANE:
					if(vectorParams.size() >2)
					{
						vectorParams.clear();
						vectorParams.push_back(Point3D(0,0,0));
						vectorParams.push_back(Point3D(0,1,0));
					}
					else if (vectorParams.size() ==2)
					{
						vectorParams[1].normalise();
					}
					else if(vectorParams.size() ==1)
					{
						vectorParams.push_back(Point3D(0,1,0));
					}

					scalarParams.clear();
					break;
				case PRIMITIVE_CYLINDER:
					if(vectorParams.size()>2)
					{
						vectorParams.resize(2);
					}
					else if(vectorParams.size() ==1)
					{
						vectorParams.push_back(Point3D(0,1,0));
					}
					else if(!vectorParams.size())
					{
						vectorParams.push_back(Point3D(0,0,0));
						vectorParams.push_back(Point3D(0,1,0));

					}

					if(scalarParams.size()!=1)
					{
						scalarParams.clear();
						scalarParams.push_back(10.0f);
					}
					break;
				case PRIMITIVE_AAB:
				
					if(vectorParams.size() >2)
					{
						vectorParams.clear();
						vectorParams.push_back(Point3D(0,0,0));
						vectorParams.push_back(Point3D(1,1,1));
					}
					else if(vectorParams.size() ==1)
					{
						vectorParams.push_back(Point3D(1,1,1));
					}
					//check to see if any components of the
					//corner offset are zero; we disallow a
					//zero, 1 or 2 dimensional box
					for(unsigned int ui=0;ui<3;ui++)
					{
						vectorParams[1][ui]=fabs(vectorParams[1][ui]);
						if(vectorParams[1][ui] <std::numeric_limits<float>::epsilon())
							vectorParams[1][ui] = 1;
					}
					scalarParams.clear();
					break;

				default:
					ASSERT(false);
			}
	
			clearCache();	
			needUpdate=true;	
			return true;	
		}
		case KEY_ORIGIN:
		{
			if(!applyPropertyNow(vectorParams[0],value,needUpdate))
				return false;
			break;
		}
		case KEY_CORNER:
		{
			if(!applyPropertyNow(vectorParams[1],value,needUpdate))
				return false;
			break;
		}
		case KEY_RADIUS:
		{
			if(!applyPropertyNow(scalarParams[0],value,needUpdate))
				return false;
			break;
		}
		case KEY_NORMAL:
		{
			ASSERT(vectorParams.size() >=2);
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			if(primitiveType == PRIMITIVE_CYLINDER)
			{
				if(lockAxisMag && 
					newPt.sqrMag() > sqrtf(std::numeric_limits<float>::epsilon()))
				{
					newPt.normalise();
					newPt*=sqrtf(vectorParams[1].sqrMag());
				}
			}
			if(!(vectorParams[1] == newPt ))
			{
				vectorParams[1] = newPt;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case KEY_PRIMITIVE_SHOW:
		{
			if(!applyPropertyNow(showPrimitive,value,needUpdate))
				return false;
			break;
		}
		case KEY_PRIMITIVE_INVERTCLIP:
		{
			if(!applyPropertyNow(invertedClip,value,needUpdate))
				return false;
			break;
		}
		case KEY_AXIS_LOCKMAG:
		{
			if(!applyPropertyNow(lockAxisMag,value,needUpdate))
				return false;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	ASSERT(vectorParams.size() || scalarParams.size());	

	return true;
}

//!Get the human readable error string associated with a particular error code during refresh(...)
std::string IonClipFilter::getSpecificErrString(unsigned int code) const
{
	const char *errCode[] = { "",
				"Insufficient mem. for Ionclip",
				"Ionclip Aborted"
	};
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(errCode) == IONCLIP_ERR_ENUM_END);
	ASSERT(code < IONCLIP_ERR_ENUM_END);
	return errCode[code];
}

bool IonClipFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<"<< trueName() <<  ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

			f << tabs(depth+1) << "<primitivetype value=\"" << primitiveType<< "\"/>" << endl;
			f << tabs(depth+1) << "<invertedclip value=\"" << invertedClip << "\"/>" << endl;
			f << tabs(depth+1) << "<showprimitive value=\"" << showPrimitive << "\"/>" << endl;
			f << tabs(depth+1) << "<lockaxismag value=\"" << lockAxisMag<< "\"/>" << endl;

			writeVectorsXML(f,"vectorparams",vectorParams, depth+1);
			writeScalarsXML(f,"scalarparams",scalarParams,depth+1);

			f << tabs(depth) << "</ionclip>" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool IonClipFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	std::string tmpStr;	
	//Retrieve primitive type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,primitiveType,"primitivetype","value"))
		return false;
	if(primitiveType >= PRIMITIVE_END)
	       return false;	
	//====
	
	//Retrieve clip inversion
	//====
	//
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"invertedclip","value"))
		return false;

	if(!boolStrDec(tmpStr,invertedClip))
		return false;
	//====
	
	//Retrieve primitive visibility 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"showprimitive","value"))
		return false;
	if(!boolStrDec(tmpStr,showPrimitive))
		return false;
	//====
	
	//Retrieve axis lock mode 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"lockaxismag","value"))
		return false;

	if(!boolStrDec(tmpStr,lockAxisMag))
		return false;
	//====
	
	//Retrieve vector parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"vectorparams"))
		return false;

	if(!readVectorsXML(nodePtr,vectorParams))
		return false;
	//===	

	//Retrieve scalar parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"scalarparams"))
		return false;

	if(!readScalarsXML(nodePtr,scalarParams))
		return false;
	//===	

	//Check the scalar params match the selected primitive	
	switch(primitiveType)
	{
		case PRIMITIVE_SPHERE:
			if(vectorParams.size() != 1 || scalarParams.size() !=1)
				return false;
			break;
		case PRIMITIVE_PLANE:
		case PRIMITIVE_AAB:
			if(vectorParams.size() != 2 || scalarParams.size() !=0)
				return false;
			break;
		case PRIMITIVE_CYLINDER:
			if(vectorParams.size() != 2 || scalarParams.size() !=1)
				return false;
			break;

		default:
			ASSERT(false);
			return false;
	}

	ASSERT(vectorParams.size() || scalarParams.size());	
	return true;
}

unsigned int IonClipFilter::getRefreshBlockMask() const
{
	return STREAM_TYPE_IONS ;
}

unsigned int IonClipFilter::getRefreshEmitMask() const
{
	if(showPrimitive)
		return STREAM_TYPE_IONS | STREAM_TYPE_DRAW;
	else
		return  STREAM_TYPE_IONS ;
}

unsigned int IonClipFilter::getRefreshUseMask() const
{
	return  STREAM_TYPE_IONS;
}

void IonClipFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
		case BINDING_CYLINDER_RADIUS:
		case BINDING_SPHERE_RADIUS:
			b.getValue(scalarParams[0]);
			break;
		case BINDING_CYLINDER_ORIGIN:
		case BINDING_SPHERE_ORIGIN:
		case BINDING_PLANE_ORIGIN:
		case BINDING_RECT_TRANSLATE:
			b.getValue(vectorParams[0]);
			break;
		case BINDING_CYLINDER_DIRECTION:
			b.getValue(vectorParams[1]);
			break;
		case BINDING_PLANE_DIRECTION:
		{
			Point3D p;
			b.getValue(p);
			p.normalise();

			vectorParams[1] =p;
			break;
		}
		case BINDING_RECT_CORNER_MOVE:
		{
			//Prevent the corner offset from acquiring a vector
			//with a negative component.
			Point3D p;
			b.getValue(p);
			for(unsigned int ui=0;ui<3;ui++)
			{
				p[ui]=fabs(p[ui]);
				//Should be positive
				if(p[ui] <std::numeric_limits<float>::epsilon())
					return;
			}

			vectorParams[1]=p;
			break;
		}
		default:
			ASSERT(false);
	}
	clearCache();
}


#ifdef DEBUG

//Create a synthetic dataset of points
// returned pointer *must* be deleted by caller.
//Span must have 3 elements, and for best results should be co-prime with
// one another; eg all prime numbers
IonStreamData *synthData(const unsigned int span[],unsigned int numPts);


//Test the spherical clipping primitive
bool sphereTest();
//Test the plane primitive
bool planeTest();
//Test the cylinder primitive
bool cylinderTest(const Point3D &pAxis, const unsigned int *span, float testRadius);

//Test the axis-aligned box primitve
bool rectTest();


bool IonClipFilter::runUnitTests()
{
	if(!sphereTest())
		return false;

	if(!planeTest())
		return false;

	unsigned int span[]={ 
			5, 7, 9
			};	
	const float TEST_RADIUS=3.0f;
	Point3D axis; 
	axis=Point3D(1,2,3);
	if(!cylinderTest(axis,span,TEST_RADIUS))
		return false;

	axis=Point3D(0,1,0);
	if(!cylinderTest(axis,span,TEST_RADIUS))
		return false;
	
	if(!rectTest())
		return false;

	return true;
}

bool sphereTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	
	unsigned int span[]={ 
			5, 7, 9
			};	
	const unsigned int NUM_PTS=10000;
	IonStreamData *d=synthData(span,NUM_PTS);
	streamIn.push_back(d);

	IonClipFilter *f=new IonClipFilter;
	f->setCaching(false);
	
	bool needUp; std::string s;
	TEST(f->setProperty(KEY_PRIMITIVE_TYPE,
		primitiveStringFromID(PRIMITIVE_SPHERE),needUp),"Set Prop");

	Point3D pOrigin((float)span[0]/2,(float)span[1]/2,(float)span[2]/2);
	stream_cast(s,pOrigin);
	TEST(f->setProperty(KEY_ORIGIN,s,needUp),"Set prop");

	const float TEST_RADIUS=1.2f;
	stream_cast(s,TEST_RADIUS);
	TEST(f->setProperty(KEY_RADIUS,s,needUp),"Set prop");
	
	TEST(f->setProperty(KEY_PRIMITIVE_SHOW,"0",needUp),"Set prop");

	//Do the refresh
	ProgressData p;
	f->refresh(streamIn,streamOut,p);

	delete f;
	delete d;
	
	
	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");

	TEST(streamOut[0]->getNumBasicObjects() > 0, "clipped point count");

	const IonStreamData *dOut=(IonStreamData*)streamOut[0];

	for(unsigned int ui=0;ui<dOut->data.size();ui++)
	{
		Point3D p;
		p=dOut->data[ui].getPos();

		TEST(sqrtf(p.sqrDist(pOrigin)) 
			<= TEST_RADIUS,"Sphere containment");
	}
	
	delete dOut;
	return true;
}

bool planeTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	
	unsigned int span[]={ 
			5, 7, 9
			};	
	const unsigned int NUM_PTS=10000;
	IonStreamData *d=synthData(span,NUM_PTS);
	streamIn.push_back(d);

	IonClipFilter *f=new IonClipFilter;
	f->setCaching(false);
	
	bool needUp; std::string s;
	TEST(f->setProperty(KEY_PRIMITIVE_TYPE,
		primitiveStringFromID(PRIMITIVE_PLANE),needUp),"set prop");

	Point3D pOrigin((float)span[0]/2,(float)span[1]/2,(float)span[2]/2);
	stream_cast(s,pOrigin);
	TEST(f->setProperty(KEY_ORIGIN,s,needUp),"Set prop");

	Point3D pPlaneDir(1,2,3);
	stream_cast(s,pPlaneDir);
	TEST(f->setProperty(KEY_NORMAL,s,needUp),"Set prop");

	TEST(f->setProperty(KEY_PRIMITIVE_SHOW,"0",needUp),"Set prop");

	//Do the refresh
	ProgressData p;
	f->refresh(streamIn,streamOut,p);

	delete f;
	delete d;
	
	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	
	const IonStreamData *dOut=(IonStreamData*)streamOut[0];

	for(unsigned int ui=0;ui<dOut->data.size();ui++)
	{
		Point3D p;
		p=dOut->data[ui].getPos();

		p=p-pOrigin;
		TEST(p.dotProd(pPlaneDir) >=0, "Plane direction");
	}

	delete dOut;
	return true;
}

bool cylinderTest(const Point3D &pAxis, const unsigned int *span, float testRadius)
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	
	const unsigned int NUM_PTS=10000;
	IonStreamData *d=synthData(span,NUM_PTS);
	streamIn.push_back(d);

	IonClipFilter*f=new IonClipFilter;
	f->setCaching(false);

	bool needUp; std::string s;
	TEST(f->setProperty(KEY_PRIMITIVE_TYPE,
		primitiveStringFromID(PRIMITIVE_CYLINDER),needUp),"Set prop");

	Point3D pOrigin((float)span[0]/2,(float)span[1]/2,(float)span[2]/2);
	stream_cast(s,pOrigin);
	TEST(f->setProperty(KEY_ORIGIN,s,needUp),"Set prop");

	stream_cast(s,pAxis);
	TEST(f->setProperty(KEY_NORMAL,s,needUp),"Set prop");

	stream_cast(s,testRadius);
	TEST(f->setProperty(KEY_RADIUS,s,needUp),"Set prop");

	TEST(f->setProperty(KEY_PRIMITIVE_SHOW,"0",needUp),"Set prop");
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Refresh error code");
	delete f;
	delete d;
	
	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	
	const IonStreamData *dOut=(IonStreamData*)streamOut[0];

	DrawCylinder *dC = new DrawCylinder;
	dC->setRadius(testRadius);
	dC->setOrigin(pOrigin);
	float len = sqrtf(pAxis.sqrMag());

	Point3D axisNormal(pAxis);
	axisNormal.normalise();
	
	dC->setDirection(pAxis);
	dC->setLength(len);

	BoundCube b;
	dC->getBoundingBox(b);

	delete dC;

	b.expand(sqrtf(std::numeric_limits<float>::epsilon()));
	for(unsigned int ui=0;ui<dOut->data.size();ui++)
	{
		Point3D p;
		p=dOut->data[ui].getPos();
		//FIXME: This fails, but appears to work, depending upon the 
		// tests I put it through???
		TEST(b.containsPt(p), "Bounding box containment");
	}

	delete dOut;

	return true;
}

bool rectTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	
	unsigned int span[]={ 
			5, 7, 9
			};	
	const unsigned int NUM_PTS=10000;
	IonStreamData *d=synthData(span,NUM_PTS);
	streamIn.push_back(d);

	IonClipFilter*f=new IonClipFilter;
	f->setCaching(false);
	
	bool needUp; std::string s;
	TEST(f->setProperty(KEY_PRIMITIVE_TYPE,
		primitiveStringFromID(PRIMITIVE_AAB),needUp),"set prop");
	TEST(f->setProperty(KEY_PRIMITIVE_SHOW,"0",needUp),"Set prop");
	TEST(f->setProperty(KEY_PRIMITIVE_INVERTCLIP,"0",needUp),"Set prop");

	Point3D pOrigin(span[0],span[1],span[2]);
	pOrigin*=0.25f;
	stream_cast(s,pOrigin);
	TEST(f->setProperty(KEY_ORIGIN,s,needUp),"Set prop");
		
	Point3D pCorner(span[0],span[1],span[2]);
	pCorner*=0.25f;
	stream_cast(s,pCorner);
	TEST(f->setProperty(KEY_CORNER,s,needUp),"Set prop");

	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Refresh error code");
	delete f;
	delete d;


	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	const IonStreamData *dOut=(IonStreamData*)streamOut[0];
	
	BoundCube b;
	b.setBounds(pOrigin-pCorner,pOrigin+pCorner);
	for(unsigned int ui=0;ui<dOut->data.size();ui++)
	{
		Point3D p;
		p=dOut->data[ui].getPos();

		TEST(b.containsPt(p), "Bounding box containment");
	}
	

	delete dOut;
	return true;
}


IonStreamData *synthData(const unsigned int span[], unsigned int numPts)
{
	IonStreamData *d = new IonStreamData;
	
	for(unsigned int ui=0;ui<numPts;ui++)
	{
		IonHit h;
		h.setPos(Point3D(ui%span[0],
			ui%span[1],ui%span[2]));
		h.setMassToCharge(ui);
		d->data.push_back(h);
	}

	return d;
}



#endif
