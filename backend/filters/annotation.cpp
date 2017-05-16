/*
 *	annotation.cpp - 3D annotations filter for 3depict
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
#include "annotation.h"

#include "filterCommon.h"

using std::string;
using std::vector;
using std::pair;
using std::make_pair;

//grab size when doing convex hull calculations
const unsigned int HULL_GRAB_SIZE=4096;

enum
{
	KEY_POSITION=1,
	KEY_MODE,
	KEY_ENABLE,
	KEY_UPVEC,
	KEY_ACROSSVEC,
	KEY_ANNOTATE_TEXT,
	KEY_TARGET,
	KEY_COLOUR,
	KEY_ARROW_SIZE,
	KEY_TEXTSIZE,
	KEY_LINESIZE,
	KEY_REFLEXIVE,
	KEY_SPHERE_ANGLE_SIZE,
	KEY_ANGLE_TEXT_VISIBLE,
	KEY_ANGLE_FORMAT_STRING,
	KEY_LINEAR_FONTSIZE,
	KEY_LINEAR_NUMTICKS,
	KEY_LINEAR_FIXED_TICKS,
	KEY_LINEAR_TICKSPACING,
	KEY_ANGLE_POS_ZERO,
	KEY_ANGLE_POS_ONE,
	KEY_ANGLE_POS_TWO
};

enum
{
	BINDING_TEXT_ORIGIN=1,
	BINDING_ARROW_ORIGIN,
	BINDING_ARROW_VECTOR,
	BINDING_ANGLE_ORIGIN,
	BINDING_ANGLE_FIRST,
	BINDING_ANGLE_SECOND,
	BINDING_ANGLE_SPHERERADIUS,
	BINDING_LINEAR_ORIGIN,
	BINDING_LINEAR_TARGET,
	BINDING_LINEAR_SPHERERADIUS
};


const char *annotationModeStrings[] =
{
	NTRANS("Arrow"),
	NTRANS("Text"),
	NTRANS("Arrow+Text"),
	NTRANS("Angle"),
	NTRANS("Ruler")
};


AnnotateFilter::AnnotateFilter() : annotationMode(ANNOTATION_TEXT),
	position(Point3D(0,0,0)), target(Point3D(1,0,0)), upVec(Point3D(0,0,1)),
	acrossVec(Point3D(0,1,0)), textSize(1.0f), annotateSize(1.0f),
	sphereMarkerSize(1.5f),rgba(0,0,1), active(true),showAngleText(true), 
	reflexAngle(true), angleFormatPreDecimal(0),angleFormatPostDecimal(0),
	linearFixedTicks(true),linearMeasureTicks(10),linearMeasureSpacing(10.0f),
	fontSizeLinearMeasure(5)
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(annotationModeStrings) == ANNOTATION_MODE_END);

	annotationMode=ANNOTATION_TEXT;

	anglePos[0]=Point3D(0,0,0);
	anglePos[1]=Point3D(0,5,5);
	anglePos[2]=Point3D(0,-5,5);
	
	textSize=1;
	annotateSize=1;
	sphereMarkerSize=1.5;
	
	active=true;
	showAngleText=true;

	reflexAngle=false;
	fontSizeLinearMeasure=5;
	linearMeasureTicks=10;
	linearFixedTicks=true;
	linearMeasureSpacing=10.0f;
	lineSize=1.0f;
	angleFormatPreDecimal=angleFormatPostDecimal=0;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up
}

Filter *AnnotateFilter::cloneUncached() const
{
	AnnotateFilter *p=new AnnotateFilter();

	p->annotationMode=annotationMode;
	p->position=position;
	p->target=target;
	p->upVec=upVec;
	p->acrossVec=acrossVec;

	for(unsigned int ui=0;ui<3; ui++)
		p->anglePos[ui]=anglePos[ui];

	p->annotateText=annotateText;
	p->textSize=textSize;
	p->annotateSize=annotateSize;
	p->sphereMarkerSize=sphereMarkerSize;

	p->rgba=rgba;
	p->active=active;
	p->showAngleText=showAngleText;

	p->reflexAngle=reflexAngle;

	p->angleFormatPreDecimal=angleFormatPreDecimal;
	p->angleFormatPostDecimal=angleFormatPostDecimal;


	p->fontSizeLinearMeasure=fontSizeLinearMeasure;
	p->linearFixedTicks=linearFixedTicks;
	p->linearMeasureSpacing=linearMeasureSpacing;
	p->linearMeasureTicks=linearMeasureTicks;
	p->lineSize=lineSize;

	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

unsigned int AnnotateFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{

	//Clear selection devices, first deleting any we have
	clearDevices();

	//Pipe everything through
	propagateStreams(dataIn,getOut);

	//If we are not enabled, do not draw anything into the output
	if(!active)
		return 0;

	DrawStreamData *d; 
	d = new DrawStreamData;
	d->parent=this;

	//Draw text output as needed
	if( annotationMode == ANNOTATION_TEXT ||
		annotationMode== ANNOTATION_TEXT_WITH_ARROW)
	{
		DrawGLText *dt;
		dt = new DrawGLText(getDefaultFontFile().c_str(),FTGL_POLYGON);

		dt->setString(annotateText);
		dt->setOrigin(position);
		dt->setUp(upVec);
		dt->setColour(rgba.r(),rgba.g(),rgba.b(),rgba.a());
		dt->setTextDir(acrossVec);
		dt->setSize((unsigned int)textSize);

		dt->setAlignment(DRAWTEXT_ALIGN_CENTRE);
		
		dt->canSelect=true;
		SelectionDevice *s = new SelectionDevice(this);
		SelectionBinding bind[1];
		
		bind[0].setBinding(SELECT_BUTTON_LEFT,0,DRAW_TEXT_BIND_ORIGIN,
				BINDING_TEXT_ORIGIN,dt->getOrigin(),dt);
		bind[0].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
		s->addBinding(bind[0]);
					

		devices.push_back(s);
		d->drawables.push_back(dt);
	}
	
	//Draw annotation mode as needed
	if(annotationMode==ANNOTATION_ARROW ||
		annotationMode==ANNOTATION_TEXT_WITH_ARROW)
	{
		DrawVector *dv;
		dv = new DrawVector;

		dv->setOrigin(position);
		dv->setVector(target-position);
		dv->setArrowSize(annotateSize);
		dv->setColour(rgba.r(),rgba.g(),rgba.b(),rgba.a());	
		dv->setLineSize(lineSize);
	
		dv->canSelect=true;
		dv->wantsLight=true;

		SelectionDevice *s = new SelectionDevice(this);
		SelectionBinding bind[2];
		
		bind[0].setBinding(SELECT_BUTTON_LEFT,0,DRAW_VECTOR_BIND_TARGET,
				BINDING_ARROW_VECTOR,dv->getVector(),dv);
		bind[0].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
		s->addBinding(bind[0]);
		
		
		bind[1].setBinding(SELECT_BUTTON_LEFT,FLAG_SHIFT,DRAW_VECTOR_BIND_ORIGIN,
				BINDING_ARROW_ORIGIN,dv->getOrigin(),dv);
		bind[1].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
		s->addBinding(bind[1]);
					

		devices.push_back(s);
		d->drawables.push_back(dv);
	}

	if(annotationMode == ANNOTATION_ANGLE_MEASURE)
	{
		//Draw the three spheres that are the handles
		//for the angle motion
		for(unsigned int ui=0;ui<3;ui++)
		{
			DrawSphere *dS;
			SelectionDevice *s= new SelectionDevice(this);
			SelectionBinding bind[2];

			dS=new DrawSphere;
			dS->setOrigin(anglePos[ui]);
			dS->setRadius(sphereMarkerSize);
			dS->setColour(rgba.r(),rgba.g(),rgba.b(),rgba.a());

			dS->canSelect=true;
			dS->wantsLight=true;

			//Create binding for sphere translation.
			//Note that each binding is a bit different, as it
			//affects each sphere separately.
			bind[0].setBinding(SELECT_BUTTON_LEFT,0,DRAW_SPHERE_BIND_ORIGIN,
						BINDING_ANGLE_ORIGIN+ui,anglePos[ui],dS);
			bind[0].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
			s->addBinding(bind[0]);

			//Create binding for sphere scaling, each binding is the same 
			bind[1].setBinding(SELECT_BUTTON_LEFT,FLAG_SHIFT,DRAW_SPHERE_BIND_RADIUS,
						BINDING_ANGLE_SPHERERADIUS,dS->getRadius(),dS);
			bind[1].setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
			bind[1].setFloatLimits(0,std::numeric_limits<float>::max());

			s->addBinding(bind[1]);

			devices.push_back(s);
			d->drawables.push_back(dS);
		}

		//Now draw the two lines that form the angle
		DrawVector *dv;
		dv=new DrawVector;
		dv->setOrigin(anglePos[0]);
		dv->setVector(anglePos[1]-anglePos[0]);
		dv->setColour(rgba.r(),rgba.g(),rgba.b(),rgba.a());
		dv->setDrawArrow(false);
		d->drawables.push_back(dv);

		dv=new DrawVector;
		dv->setOrigin(anglePos[0]);
		dv->setVector(anglePos[2]-anglePos[0]);
		dv->setColour(rgba.r(),rgba.g(),rgba.b(),rgba.a());
		dv->setDrawArrow(false);
		d->drawables.push_back(dv);


		//If required,
		//show the text that 
		//indicates the included or reflexive angle
		if(showAngleText)
		{
			std::string angleString;
			
			Point3D d1,d2;
			float angleVal=0;
			
			d1=anglePos[1]-anglePos[0];
			d2=anglePos[2]-anglePos[0];

			//Work out the angle if there is a non-degenerate vector.
			//otherwise set it to the "undefined" value of zero
			if(fabs(d1.dotProd(d2))  > sqrtf(std::numeric_limits<float>::epsilon()))
				angleVal =d1.angle(d2);

			if(reflexAngle)
				angleVal=2.0*M_PI-angleVal;
			angleVal=180.0f/M_PI*angleVal;
			angleVal=fmod(angleVal,360.0f);

			//FIXME: print specifier computation is still a bit off
			if(angleFormatPreDecimal+angleFormatPostDecimal)
			{
				//One space for the decimal, one for the null
				//and the rest for the actual integer
				size_t num;
				num = angleFormatPreDecimal+angleFormatPostDecimal+1;
				char *buf = new char[num+1];

				std::string tmp,formatStr;
				formatStr="%";
				if(angleFormatPreDecimal)
				{

					stream_cast(tmp,angleFormatPreDecimal + angleFormatPostDecimal+2);
					formatStr+=std::string("0") + tmp;
				}

				if(angleFormatPostDecimal)
				{

					stream_cast(tmp,angleFormatPostDecimal);
					formatStr+=".";
					formatStr+=tmp;
				}
				formatStr+="f";

				snprintf(buf,num,formatStr.c_str(),angleVal);
				angleString=buf;
				delete[] buf;

			}
			else
				stream_cast(angleString, angleVal);

			//Place the string appropriately
			DrawGLText *dt;
			dt = new DrawGLText(getDefaultFontFile().c_str(),FTGL_POLYGON);

			dt->setString(angleString);
			dt->setAlignment(DRAWTEXT_ALIGN_CENTRE);
			//Place the text using
			//a factor of the text size in
			//the direction of the average
			//of the two vector components
			Point3D averageVec(1,0,0);
			if(averageVec.sqrMag() > std::numeric_limits<float>::epsilon())
			{
				averageVec = (d1+d2)*0.5f;
				averageVec.normalise();
				averageVec*=textSize*1.1;
				if(reflexAngle)
					averageVec.negate();
			}
			dt->setOrigin(anglePos[0]+averageVec);
			//Use user-specifications for colour,
			//size and orientation
			dt->setUp(upVec);
			dt->setColour(rgba.r(),rgba.g(),rgba.b(),rgba.a());
			dt->setTextDir(acrossVec);
			dt->setSize((unsigned int)textSize);
		

			d->drawables.push_back(dt);
		}
	}

	if(annotationMode == ANNOTATION_LINEAR_MEASURE)
	{
		DrawVector *dv;
		dv = new DrawVector;

		dv->setOrigin(position);
		dv->setColour(rgba.r(),rgba.g(),rgba.b(),rgba.a());
		dv->setVector(target-position);
		dv->setDrawArrow(false);

		d->drawables.push_back(dv);

		//Compute the tick spacings
		vector<float> tickSpacings;
		if(linearFixedTicks)
		{
			tickSpacingsFromFixedNum(0,sqrtf(target.sqrDist(position)),
					linearMeasureTicks,tickSpacings);
		}
		else
		{
			tickSpacingsFromInterspace(0,sqrtf(target.sqrDist(position)),
						linearMeasureSpacing,tickSpacings);
		}

		if(tickSpacings.size())
		{

			Point3D measureNormal;
			measureNormal = target-position;
			measureNormal.normalise();

			//Construct the drawable text object
			for(unsigned int ui=0;ui<tickSpacings.size();ui++)
			{
				DrawGLText *dT; 
				//Create the tick that will be added to the drawables
				dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_POLYGON);
					
				dT->setColour(rgba.r(),rgba.g()	,rgba.b(),rgba.a());
				dT->setOrigin(measureNormal*tickSpacings[ui] + position);
				dT->setUp(upVec);	
				dT->setTextDir(acrossVec);
				dT->setSize((unsigned int)fontSizeLinearMeasure);

				string s;
				stream_cast(s,tickSpacings[ui]);

				dT->setString(s);

				d->drawables.push_back(dT);
			}


			//Now draw the end markers

			//Start marker
			DrawSphere *dS;
			dS = new DrawSphere;
			dS->setRadius(sphereMarkerSize);
			dS->setOrigin(position);
			dS->setColour(rgba.r(),rgba.g(),rgba.b(),rgba.a());

			dS->canSelect=true;
			dS->wantsLight=true;
			
			SelectionDevice *s= new SelectionDevice(this);
			SelectionBinding bind[4];
			//Create binding for sphere translation.
			//Note that each binding is a bit different, as it
			//affects each sphere separately.
			bind[0].setBinding(SELECT_BUTTON_LEFT,0,DRAW_SPHERE_BIND_ORIGIN,
						BINDING_LINEAR_ORIGIN,position,dS);
			bind[0].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
			s->addBinding(bind[0]);

			//Create binding for sphere scaling, each binding is the same 
			bind[1].setBinding(SELECT_BUTTON_LEFT,FLAG_SHIFT,DRAW_SPHERE_BIND_RADIUS,
						BINDING_LINEAR_SPHERERADIUS,dS->getRadius(),dS);
			bind[1].setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
			bind[1].setFloatLimits(0,std::numeric_limits<float>::max());

			s->addBinding(bind[1]);

			devices.push_back(s);
			d->drawables.push_back(dS);
		
		
			//Now do the second sphere (end marker)
			s= new SelectionDevice(this);
			dS = new DrawSphere;
			
			dS->setRadius(sphereMarkerSize);
			dS->setOrigin(target);
			dS->setColour(rgba.r(),rgba.g(),rgba.b(),rgba.a());

			dS->canSelect=true;
			dS->wantsLight=true;

			bind[2].setBinding(SELECT_BUTTON_LEFT,0,DRAW_SPHERE_BIND_ORIGIN,
						BINDING_LINEAR_TARGET,target,dS);
			bind[2].setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
			s->addBinding(bind[2]);

			//Create binding for sphere scaling, each binding is the same 
			bind[3].setBinding(SELECT_BUTTON_LEFT,FLAG_SHIFT,DRAW_SPHERE_BIND_RADIUS,
						BINDING_LINEAR_SPHERERADIUS,dS->getRadius(),dS);
			bind[3].setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
			bind[3].setFloatLimits(0,std::numeric_limits<float>::max());
			s->addBinding(bind[3]);
		
			
			devices.push_back(s);
			d->drawables.push_back(dS);


		}
	}
	
	d->cached=0;
	getOut.push_back(d);

	return 0;
}

size_t AnnotateFilter::numBytesForCache(size_t nObjects) const
{
	return 0;
}

void AnnotateFilter::getProperties(FilterPropGroup &propertyList) const
{
	string tmpStr;
	FilterProperty p;
	size_t curGroup=0;

	tmpStr=boolStrEnc(active);	

	p.name=TRANS("Enable");
	p.data=tmpStr;
	p.key=KEY_ENABLE;
	p.helpText=TRANS("Enable/disable annotation");
	p.type=PROPERTY_TYPE_BOOL;
	propertyList.addProperty(p,curGroup);

	if(!active)
		return;



	vector<pair<unsigned int,string> > choices;
	
	for(unsigned int ui=0;ui<ANNOTATION_MODE_END; ui++)
	{
		choices.push_back(make_pair((unsigned int)ui,
					TRANS(annotationModeStrings[ui])));
	}

	tmpStr=choiceString(choices,annotationMode);
	p.name=TRANS("Mode");
	p.data=tmpStr;
	p.key=KEY_MODE;
	p.helpText=TRANS("Type or style of annotation");
	p.type=PROPERTY_TYPE_CHOICE;
	propertyList.addProperty(p,curGroup);
	propertyList.setGroupTitle(curGroup,TRANS("Mode"));
	curGroup++;

	switch(annotationMode)
	{
		case ANNOTATION_TEXT:
		{
			//Note to translators, this is short for "annotation text",
			// or similar
			p.name=TRANS("Annotation");
			p.data=annotateText;
			p.type=PROPERTY_TYPE_STRING;
			p.key=KEY_ANNOTATE_TEXT;
			p.helpText=TRANS("Text of annotation");
			propertyList.addProperty(p,curGroup);
		
			stream_cast(tmpStr,position);
			p.name=TRANS("Origin");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.key=KEY_POSITION;
			p.helpText=TRANS("Position of annotation");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(tmpStr,upVec);
			p.name=TRANS("Up dir");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_STRING;
			p.key=KEY_UPVEC;
			p.helpText=TRANS("Vector for up direction of annotation text");
			propertyList.addProperty(p,curGroup);
		
			stream_cast(tmpStr,acrossVec);
			p.name=TRANS("Across dir");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_STRING;
			p.key=KEY_ACROSSVEC;
			p.helpText=TRANS("Reading direction for annotation");
			propertyList.addProperty(p,curGroup);


			stream_cast(tmpStr,textSize);
			p.name=TRANS("Text size");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.key=KEY_TEXTSIZE;
			p.helpText=TRANS("Relative size of annotation text");
			propertyList.addProperty(p,curGroup);

			break;
		}
		case ANNOTATION_ARROW:
		{
			stream_cast(tmpStr,position);
			p.name=TRANS("Start");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.key=KEY_POSITION;
			p.helpText=TRANS("3D position for tail of arrow");
			propertyList.addProperty(p,curGroup);
		
			stream_cast(tmpStr,target);
			p.name=TRANS("End");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.key=KEY_TARGET;
			p.helpText=TRANS("3D Position to which arrow points");
			propertyList.addProperty(p,curGroup);
			
			propertyList.setGroupTitle(curGroup,TRANS("Positioning"));

			curGroup++;

			stream_cast(tmpStr,annotateSize);
			p.name=TRANS("Tip radius");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.key=KEY_ARROW_SIZE;
			p.helpText=TRANS("Size of the arrow head");
			propertyList.addProperty(p,curGroup);

			stream_cast(tmpStr,lineSize);
			p.name=TRANS("Line size");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.key=KEY_LINESIZE;
			p.helpText=TRANS("Thickness of line used to draw arrow stem");
			propertyList.addProperty(p,curGroup);
			

			break;
		}
		case ANNOTATION_TEXT_WITH_ARROW:
		{
			stream_cast(tmpStr,position);
			p.name=TRANS("Start");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.key=KEY_POSITION;
			p.helpText=TRANS("3D position for tail of arrow");
			propertyList.addProperty(p,curGroup);
			

			stream_cast(tmpStr,target);
			p.name=TRANS("End");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.key=KEY_TARGET;
			p.helpText=TRANS("3D Position to which arrow points");
			propertyList.addProperty(p,curGroup);
			
			//Note to translators, this is short for "annotation text",
			// or similar
			p.name=TRANS("Annotation");
			p.data=annotateText;
			p.type=PROPERTY_TYPE_STRING;
			p.key=KEY_ANNOTATE_TEXT;
			p.helpText=TRANS("Text of annotation");
			propertyList.addProperty(p,curGroup);
			
			propertyList.setGroupTitle(curGroup,TRANS("Options"));
			curGroup++;

			stream_cast(tmpStr,textSize);
			p.name=TRANS("Text size");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.key=KEY_TEXTSIZE;
			p.helpText=TRANS("Relative size of annotation text");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(tmpStr,upVec);
			p.name=TRANS("Up dir");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_STRING;
			p.key=KEY_UPVEC;
			propertyList.addProperty(p,curGroup);
		
			stream_cast(tmpStr,acrossVec);
			p.name=TRANS("Across dir");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_STRING;
			p.key=KEY_ACROSSVEC;
			propertyList.addProperty(p,curGroup);

			stream_cast(tmpStr,annotateSize);
			p.name=TRANS("Tip radius");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.key=KEY_ARROW_SIZE;
			propertyList.addProperty(p,curGroup);
			break;
		}
		case ANNOTATION_ANGLE_MEASURE:
		{
			stream_cast(tmpStr,anglePos[0]);
			p.name=TRANS("Position A");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.key=KEY_ANGLE_POS_ZERO;
			p.helpText=TRANS("Location of first non-central vertex");
			propertyList.addProperty(p,curGroup);

			stream_cast(tmpStr,anglePos[1]);
			p.name=TRANS("Origin ");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.key=KEY_ANGLE_POS_ONE;
			p.helpText=TRANS("Location of central vertex");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(tmpStr,anglePos[2]);
			p.name=TRANS("Position B");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.key=KEY_ANGLE_POS_TWO;
			p.helpText=TRANS("Location of second non-central vertex");
			propertyList.addProperty(p,curGroup);
			
			propertyList.setGroupTitle(curGroup,TRANS("Positioning"));
			curGroup++;
			
			stream_cast(tmpStr,acrossVec);
			p.name=TRANS("Across dir");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_STRING;
			p.key=KEY_ACROSSVEC;
			p.helpText=TRANS("Reading direction for angle text");
			propertyList.addProperty(p,curGroup);

			stream_cast(tmpStr,upVec);
			p.name=TRANS("Up dir");
			p.helpText=TRANS("Vector for up direction of angle text");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_STRING;
			p.key=KEY_UPVEC;
			propertyList.addProperty(p,curGroup);


			p.key=KEY_REFLEXIVE;
			p.name=TRANS("Reflexive");
			p.data=boolStrEnc(reflexAngle);
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Measure interor (enabled) or exterior angle (disabled)");
			propertyList.addProperty(p,curGroup);
		
			

			p.name=TRANS("Show Angle");
			p.data=boolStrEnc(showAngleText);
			p.type=PROPERTY_TYPE_BOOL;
			p.key=KEY_ANGLE_TEXT_VISIBLE;
			p.helpText=TRANS("Display angle text (when enabled)");
			propertyList.addProperty(p,curGroup);
		
			if(showAngleText)
			{
				stream_cast(tmpStr,textSize);
				p.name=TRANS("Text size");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_REAL;
				p.key=KEY_TEXTSIZE;
				p.helpText=TRANS("Size of angle text");
				propertyList.addProperty(p,curGroup);


				std::string tmp2;
				tmpStr.clear();
				if(angleFormatPreDecimal)
				{
					tmp2.resize(angleFormatPreDecimal,'#');
					tmpStr=tmp2;
				}
					
				if(angleFormatPostDecimal)
				{
					tmp2.resize(angleFormatPostDecimal,'#');
					tmpStr+=std::string(".") + tmp2;
				}

				p.name=TRANS("Digit format");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_STRING;
				p.key=KEY_ANGLE_FORMAT_STRING;
				p.helpText=TRANS("Format of angle text; # for numeral position, \'.\' for separator, eg ##.## gives 12.34");
				propertyList.addProperty(p,curGroup);

			}
			
			stream_cast(tmpStr,sphereMarkerSize);
			p.name=TRANS("Sphere size");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.key=KEY_SPHERE_ANGLE_SIZE;
			p.helpText=TRANS("Marker sphere size for manipulating tool");
			propertyList.addProperty(p,curGroup);

		

			break;
		}
		case ANNOTATION_LINEAR_MEASURE:
		{
			stream_cast(tmpStr,position);
			p.name=TRANS("Start");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.key=KEY_POSITION;
			p.helpText=TRANS("Ruler beginning 3D location");
			propertyList.addProperty(p,curGroup);
		
			stream_cast(tmpStr,target);
			p.name=TRANS("End");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.key=KEY_TARGET;
			p.helpText=TRANS("Ruler finish 3D location");
			propertyList.addProperty(p,curGroup);

			stream_cast(tmpStr,upVec);
			p.name=TRANS("Up dir");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_STRING;
			p.key=KEY_UPVEC;
			p.helpText=TRANS("Vector for up direction of annotation text");
			propertyList.addProperty(p,curGroup);
		
			stream_cast(tmpStr,acrossVec);
			p.name=TRANS("Across dir");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_STRING;
			p.key=KEY_ACROSSVEC;
			p.helpText=TRANS("Reading direction for annotation");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(tmpStr,fontSizeLinearMeasure);
			p.key=KEY_LINEAR_FONTSIZE;
			p.name=TRANS("Font Size");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_INTEGER;
			p.helpText=TRANS("Relative size of annotation text");
			propertyList.addProperty(p,curGroup);
			
		
			tmpStr=boolStrEnc(linearFixedTicks);
			p.key=KEY_LINEAR_FIXED_TICKS;
			p.name=TRANS("Fixed ticks");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Use fixed (enabled) number of text markers, or one every fixed distance (disabled)");
			propertyList.addProperty(p,curGroup);

			if(linearFixedTicks)
			{
				stream_cast(tmpStr,linearMeasureTicks);
				p.key=KEY_LINEAR_NUMTICKS;
				p.name=TRANS("Num Ticks");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_INTEGER;
				p.helpText=TRANS("Number of tick marks along ruler");
				propertyList.addProperty(p,curGroup);
			}
			else
			{
				stream_cast(tmpStr,linearMeasureSpacing);
				p.key=KEY_LINEAR_TICKSPACING;
				p.name=TRANS("Tick Spacing");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_REAL;
				p.helpText=TRANS("Distance between tick marks along ruler");
				propertyList.addProperty(p,curGroup);
			}
			
			
			stream_cast(tmpStr,sphereMarkerSize);
			p.name=TRANS("Sphere size");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.key=KEY_SPHERE_ANGLE_SIZE;
			p.helpText=TRANS("Marker sphere size for manipulating tool");
			propertyList.addProperty(p,curGroup);

			break;
		}
		default:
			ASSERT(false);
	}


	p.key=KEY_COLOUR;
	p.name=TRANS("Colour");
	p.data=rgba.toColourRGBA().rgbString();
	p.type=PROPERTY_TYPE_COLOUR;
	p.helpText=TRANS("Colour for ruler and ticks");
	propertyList.addProperty(p,curGroup);
	propertyList.setGroupTitle(curGroup,TRANS("Appearance"));
}

bool AnnotateFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{
	string stripped=stripWhite(value);
	switch(key)
	{
		case KEY_ENABLE:
		{
			if(!applyPropertyNow(active,value,needUpdate))
				return false;
			break;
		}
		case KEY_MODE:
		{
			unsigned int newMode;

			for(newMode=0;newMode<ANNOTATION_MODE_END;newMode++)
			{
				if(stripped == TRANS(annotationModeStrings[newMode]))
					break;
			}

			if(newMode == ANNOTATION_MODE_END)
				return false;

			if(newMode!=annotationMode)
			{
				annotationMode=newMode;
				needUpdate=true;
			}

			break;	
		
		}
		case KEY_UPVEC:
		{
			//This sets the up direction
			//which must be normal to the 
			//across direction for the text.
			//
			//Compute the normal component of acrossVec.
			//and override that.
			//
			//Be careful not to "invert" the text, so it
			//does not show.
			Point3D newPt;
			if(!newPt.parse(value))
				return false;
			newPt.normalise();

			//Use double-cross-product method
			//to orthogonalise the two vectors
			Point3D normVec;
			normVec=newPt.crossProd(acrossVec);

			if(normVec.sqrMag() < std::numeric_limits<float>::epsilon())
				return false;

			acrossVec=normVec.crossProd(newPt);

			ASSERT(acrossVec.sqrMag() > std::numeric_limits<float>::epsilon());
		
			if(!(upVec == newPt))
			{
				upVec=newPt;
				needUpdate=true;
			}

			break;	
		}
		case KEY_ACROSSVEC:
		{
			//This sets the up direction
			//which must be normal to the 
			//across direction for the text.
			//
			//Compute the normal component of acrossVec.
			//and override that.
			//
			//Be careful not to "invert" the text, so it
			//does not show.
			Point3D newPt;
			if(!newPt.parse(value))
				return false;
			newPt.normalise();

			//Use double-cross-product method
			//to orthogonalise the two vectors
			Point3D normVec;
			normVec=newPt.crossProd(upVec);

			if(normVec.sqrMag() < std::numeric_limits<float>::epsilon())
				return false;

			upVec=normVec.crossProd(newPt);

			ASSERT(upVec.sqrMag() > std::numeric_limits<float>::epsilon());
		
			if(!(acrossVec == newPt))
			{
				acrossVec=newPt;
				needUpdate=true;
			}

			break;	
		}
		case KEY_POSITION:
		{
			if(!applyPropertyNow(position,value,needUpdate))
				return false;
			break;	
		}
		case KEY_TARGET:
		{
			if(!applyPropertyNow(target,value,needUpdate))
				return false;
			break;	
		}
		case KEY_ANGLE_POS_ZERO:
		{
			if(!applyPropertyNow(anglePos[0],value,needUpdate))
				return false;
			break;
		}
		case KEY_ANGLE_POS_ONE:
		{
			if(!applyPropertyNow(anglePos[1],value,needUpdate))
				return false;
			break;
		}
		case KEY_ANGLE_POS_TWO:
		{
			if(!applyPropertyNow(anglePos[2],value,needUpdate))
				return false;
			break;
		}
		case KEY_ARROW_SIZE:
		{
			if(!applyPropertyNow(annotateSize,value,needUpdate))
				return false;
			break;	
		}
		case KEY_ANNOTATE_TEXT:
		{
			if(!applyPropertyNow(annotateText,value,needUpdate))
				return false;
			break;
		}
		case KEY_COLOUR:
		{
			ColourRGBA tmpRgba;
			tmpRgba.parse(value);
			if(tmpRgba != rgba)
			{
				rgba = tmpRgba.toRGBAf();
				needUpdate=true;
			}
			else
				needUpdate=false;
			break;
		}
		case KEY_TEXTSIZE:
		{
			float tmp;
			stream_cast(tmp,value);
			if(fabs(tmp-textSize) > std::numeric_limits<float>::epsilon() 
				&& tmp > sqrtf(std::numeric_limits<float>::epsilon()))
			{
				needUpdate=true;
				textSize=tmp;
			}

			break;
		}
		case KEY_REFLEXIVE:
		{
			if(!applyPropertyNow(reflexAngle,value,needUpdate))
				return false;
			break;
		}
		case KEY_SPHERE_ANGLE_SIZE:
		{
			if(!applyPropertyNow(sphereMarkerSize,value,needUpdate))
				return false;
			break;
		}
		case KEY_ANGLE_TEXT_VISIBLE:
		{
			if(!applyPropertyNow(showAngleText,value,needUpdate))
				return false;
			break;
		}

		case KEY_ANGLE_FORMAT_STRING:
		{
			//Must contain only #,[0-9]
			if(value.find_first_not_of("#,.0123456789")!=std::string::npos)
				return false;

			if(value.size())
			{
				//Must contain 0 or 1 separator.
				size_t sepCount;
				sepCount=std::count(value.begin(),value.end(),',');
				sepCount+=std::count(value.begin(),value.end(),'.');
				if(sepCount > 1)
					return false;
			
				//If we have a separator,
				//split into two parts	
				if(sepCount)
				{	
					size_t decPos;
					decPos=value.find_first_of(",.");
					angleFormatPreDecimal=decPos;
					angleFormatPostDecimal=value.size()-(decPos+1);
				}
				else
					angleFormatPreDecimal=value.size();

			}
			else
				angleFormatPreDecimal=angleFormatPostDecimal=0;
			
			needUpdate=true;
			break;
		}

		case KEY_LINEAR_FONTSIZE:
		{
			if(!applyPropertyNow(fontSizeLinearMeasure,value,needUpdate))
				return false;
			break;
		}
		case KEY_LINEAR_FIXED_TICKS:
		{
			if(!applyPropertyNow(linearFixedTicks,value,needUpdate))
				return false;
			break;
		}
		case KEY_LINEAR_NUMTICKS:
		{
			if(!applyPropertyNow(linearMeasureTicks,value,needUpdate))
				return false;
			break;
		}
		case KEY_LINEAR_TICKSPACING:
		{
			if(!applyPropertyNow(linearMeasureSpacing,value,needUpdate))
				return false;
			break;
		}
		case KEY_LINESIZE:
		{
			float tmp;
			if(stream_cast(tmp,value))
				return false;

			if(tmp == lineSize || tmp <0)
				return false;

			lineSize=tmp;
			needUpdate=true;
			break;
		}
		
		default:
			ASSERT(false);
	}

	return true;
}

std::string  AnnotateFilter::getSpecificErrString(unsigned int code) const
{
	ASSERT(false);
}

bool AnnotateFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) <<  "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;
			f << tabs(depth+1) << "<annotationmode value=\""<< annotationMode << "\"/>"  << endl;

			f << tabs(depth+1) << "<position value=\""<<position<< "\"/>"  << endl;
			f << tabs(depth+1) << "<target value=\""<<target<< "\"/>"  << endl;
			f << tabs(depth+1) << "<upvec value=\""<<upVec<< "\"/>"  << endl;
			f << tabs(depth+1) << "<acrossvec value=\""<<acrossVec<< "\"/>"  << endl;

			f << tabs(depth+1) << "<anglepos>" << endl;
			for(unsigned int ui=0;ui<3;ui++)
				f << tabs(depth+2) << "<position value=\""<<anglePos[ui]<< "\"/>"  << endl;
			f << tabs(depth+1) << "</anglepos>" << endl;


			f << tabs(depth+1) << "<annotatetext value=\""<<escapeXML(annotateText)<< "\"/>"  << endl;
			f << tabs(depth+1) << "<textsize value=\""<<textSize<< "\"/>"  << endl;
			f << tabs(depth+1) << "<annotatesize value=\""<<annotateSize<< "\"/>"  << endl;
			//DEPRECATE: rename this element. It has been repurposed.
			f << tabs(depth+1) << "<sphereanglesize value=\""<<sphereMarkerSize<< "\"/>"  << endl;
			f << tabs(depth+1) << "<linesize value=\""<<lineSize<< "\"/>"  << endl;
			f << tabs(depth+1) << "<colour value=\""<< rgba.toColourRGBA().rgbaString() << "\"/>"  << endl;

			f << tabs(depth+1) << "<active value=\""<<(active? "1" : "0")<< "\"/>"  << endl;
			f << tabs(depth+1) << "<showangletext value=\""<<(showAngleText ? "1" : "0")<< "\"/>"  << endl;
			f << tabs(depth+1) << "<reflexangle value=\""<<(reflexAngle? "1" : "0")<< "\"/>"  << endl;
			
			f << tabs(depth+1) << "<angleformat predecimal=\""<< angleFormatPreDecimal 
					<< "\" postdecimal=\"" << angleFormatPostDecimal<< "\" />" << endl;

			f << tabs(depth) << "</" <<trueName()<< ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool AnnotateFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;

	xmlChar *xmlString;

	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	//Annotation mode
	if(!XMLGetNextElemAttrib(nodePtr,annotationMode,"annotationmode","value"))
		return false;

	if(annotationMode >=ANNOTATION_MODE_END)
		return false;

	//position
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"position","value"))
		return false;

	if(!position.parse(tmpStr))
		return false;

	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"target","value"))
		return false;

	if(!target.parse(tmpStr))
		return false;
	
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"upvec","value"))
		return false;

	if(!upVec.parse(tmpStr))
		return false;
	
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"acrossvec","value"))
		return false;

	if(!acrossVec.parse(tmpStr))
		return false;
	
	//Ensure acrossVec/upvec orthogonal
	if(!upVec.orthogonalise(acrossVec))
		return false;

	xmlNodePtr tmpPtr;
	tmpPtr=nodePtr;

	if(XMLHelpFwdToElem(nodePtr,"anglepos"))
		return false;
	
	if(!nodePtr->xmlChildrenNode)
		return false;
	
	nodePtr=nodePtr->xmlChildrenNode;

	for(unsigned int ui=0;ui<3;ui++)
	{
		if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"position","value"))
			return false;

		if(!anglePos[ui].parse(tmpStr))
			return false;
	}

	nodePtr=tmpPtr;

	//If it fails, thats OK, just use the empty string.	
	if(!XMLGetNextElemAttrib(nodePtr,annotateText,"annotatetext","value"))
		annotateText="";

	if(!XMLGetNextElemAttrib(nodePtr,textSize,"textsize","value"))
		return false;
	
	if(!XMLGetNextElemAttrib(nodePtr,annotateSize,"annotatesize","value"))
		return false;

	if(annotateSize <0.0f)
		return false;


	if(!XMLGetNextElemAttrib(nodePtr,sphereMarkerSize,"sphereanglesize","value"))
		return false;
	if(sphereMarkerSize<0.0f)
		return false;

	if(!XMLGetNextElemAttrib(nodePtr,lineSize,"linesize","value"))
		return false;
	if(lineSize<0.0f)
		return false;

	//TODO: we have a standardised parsexmlColour func, use it
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"colour","value"))
		return false;
	
	ColourRGBA tmpRgba;

	if(!tmpRgba.parse(tmpStr))
		return false;

	rgba=tmpRgba.toRGBAf();

	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"active","value"))
		return false;

	if(!boolStrDec(tmpStr,active))
		return false;

	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"showangletext","value"))
		return false;

	if(!boolStrDec(tmpStr,showAngleText))
		return false;

	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"reflexangle","value"))
		return false;

	if(!boolStrDec(tmpStr,reflexAngle))
		return false;


	if(!XMLGetNextElemAttrib(nodePtr,angleFormatPreDecimal,"angleformat","predecimal"))
		return false;

	if(!XMLGetAttrib(nodePtr,angleFormatPostDecimal,"predecimal"))
		return false;


	return true;
}

unsigned int AnnotateFilter::getRefreshBlockMask() const
{
	return 0;
}

unsigned int AnnotateFilter::getRefreshEmitMask() const
{
	return  STREAM_TYPE_DRAW;
}

unsigned int AnnotateFilter::getRefreshUseMask() const
{
	//annotate only adds to the ignore mask, so 
	// we now essentially ignore all inputs, other than pass-through 
	return 0;
}

void AnnotateFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
		case BINDING_ARROW_ORIGIN:
		{
			Point3D dv;
			dv=target-position;
			b.getValue(position);
			target=position+dv;
			break;
		}
		case BINDING_LINEAR_ORIGIN:
		case BINDING_TEXT_ORIGIN:
		{
			b.getValue(position);
			break;
		}
		case BINDING_LINEAR_TARGET:
		case BINDING_ARROW_VECTOR:
		{
			b.getValue(target);
			break;
		}
		case BINDING_ANGLE_ORIGIN:
			b.getValue(anglePos[0]);
			break;
		case BINDING_ANGLE_FIRST:
			b.getValue(anglePos[1]);
			break;
		case BINDING_ANGLE_SECOND:
			b.getValue(anglePos[2]);
			break;
		case BINDING_ANGLE_SPHERERADIUS:
			b.getValue(sphereMarkerSize);
			break;
		default:
			ASSERT(false);
	}
}


#ifdef DEBUG

//Test the ruler functionality
bool rulerTest();
//Test the angle measurement tool 
bool angleTest();
//Test the pointing arrow annotation
bool arrowTest();
//Test the text+arrow functionality
bool textArrowTest();


bool AnnotateFilter::runUnitTests()
{
	if(!rulerTest())
		return false;

	if(!angleTest())
		return false;
	
	if(!arrowTest())
		return false;

	if(!textArrowTest())
		return false;

	return true;
}


bool rulerTest()
{
	vector<const FilterStreamData*> streamIn,streamOut;
	AnnotateFilter*f=new AnnotateFilter;
	f->setCaching(false);
	
	bool needUp; std::string s;
	//Set linear ruler mode
	TEST(f->setProperty(KEY_MODE,TRANS(annotationModeStrings[ANNOTATION_LINEAR_MEASURE]),needUp),"Set prop");
	//Set ruler position & length
	stream_cast(s,Point3D(0,0,0));
	TEST(f->setProperty(KEY_POSITION,s,needUp),"Set prop");
	stream_cast(s,Point3D(1,1,1));
	TEST(f->setProperty(KEY_TARGET,s,needUp),"Set prop");
	stream_cast(s,sqrtf(2)/10);
	TEST(f->setProperty(KEY_LINEAR_TICKSPACING,s,needUp),"Set prop");
	
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Refresh error code");

	delete f;


	TEST(streamOut.size(),"stream size");

	//Count the number of text object types
	size_t textCount,vecCount,otherDrawCount;
	textCount=vecCount=otherDrawCount=0;
	for(unsigned int ui=0;ui<streamOut.size(); ui++)
	{
		switch(streamOut[ui]->getStreamType())
		{
			case STREAM_TYPE_DRAW:
			{
				const DrawStreamData* d;
				d= (const DrawStreamData*)streamOut[ui];
				for(unsigned int ui=0;ui<d->drawables.size();ui++)
				{
					switch(d->drawables[ui]->getType())
					{
						case DRAW_TYPE_GLTEXT:
							textCount++;
							break;
						case DRAW_TYPE_VECTOR:
							vecCount++;
							break;
						default:
							otherDrawCount++;
							break;
					}

				}
				break;
			}
			default:
				;
		}

	}

	//We should have a line, one would hope
	TEST(vecCount>0,"Number of lines in ruler test");
	//Floating pt errors, and not setting the zero could alter this
	//so it should be close to, but not exactly 10
	TEST(textCount == 10 || textCount == 9 || textCount == 11,
			"Number of lines in ruler test");

	for(unsigned int ui=0;ui<streamOut.size();ui++)
		delete streamOut[ui];

	return true;
}

bool angleTest()
{
	vector<const FilterStreamData*> streamIn,streamOut;
	AnnotateFilter*f=new AnnotateFilter;
	f->setCaching(false);
	
	bool needUp; std::string s;
	//Set arrow annotation mode
	TEST(f->setProperty(KEY_MODE,
		TRANS(annotationModeStrings[ANNOTATION_ANGLE_MEASURE]),needUp),"set property");
	//Set position & target for arrow
	const Point3D ANGLE_ORIGIN(0,0,0);
	const Point3D ANGLE_A(0,0,1);
	const Point3D ANGLE_B(0,1,0);
	stream_cast(s,ANGLE_ORIGIN);
	TEST(f->setProperty(KEY_ANGLE_POS_ONE,s,needUp),"Set prop");
	stream_cast(s,ANGLE_A);
	TEST(f->setProperty(KEY_ANGLE_POS_ZERO,s,needUp),"Set prop");
	stream_cast(s,ANGLE_B);
	TEST(f->setProperty(KEY_ANGLE_POS_TWO,s,needUp),"Set prop");
	
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Refresh error code");

	delete f;



	TEST(streamOut.size(),"stream size");
	
	//Count the number of text object types
	size_t vecCount,otherDrawCount,textDrawCount,sphereDrawCount;
	vecCount=otherDrawCount=textDrawCount=sphereDrawCount=0;
	for(unsigned int ui=0;ui<streamOut.size(); ui++)
	{
		switch(streamOut[ui]->getStreamType())
		{
			case STREAM_TYPE_DRAW:
			{
				const DrawStreamData* d;
				d= (const DrawStreamData*)streamOut[ui];
				for(unsigned int ui=0;ui<d->drawables.size();ui++)
				{
					switch(d->drawables[ui]->getType())
					{
						case DRAW_TYPE_VECTOR:
							vecCount++;
							break;
						case DRAW_TYPE_GLTEXT:
							textDrawCount++;
							break;
						case DRAW_TYPE_SPHERE:
							sphereDrawCount++;
							break;
						default:
							otherDrawCount++;
							break;
					}

				}
				break;
			}
			default:
				;
		}
	}


	TEST(textDrawCount,"angle text drawable");
	TEST(vecCount,"angle arms drawable");
	TEST(sphereDrawCount,"sphere marker drawable");
	TEST(!otherDrawCount,"unexpected drawable in angle measure");

	for(unsigned int ui=0;ui<streamOut.size();ui++)
		delete streamOut[ui];

	return true;
}

bool arrowTest()
{
	vector<const FilterStreamData*> streamIn,streamOut;
	AnnotateFilter*f=new AnnotateFilter;
	f->setCaching(false);
	
	bool needUp; std::string s;
	//Set arrow annotation mode
	TEST(f->setProperty(KEY_MODE,
		TRANS(annotationModeStrings[ANNOTATION_ARROW]),needUp),"Set arrow mode Property");
	//Set position & target for arrow
	const Point3D ARROW_ORIGIN(-1,-1,-1);
	const Point3D ARROW_TARGET(1,1,1);
	stream_cast(s,ARROW_ORIGIN);
	TEST(f->setProperty(KEY_POSITION,s,needUp),"Set position prop");
	stream_cast(s,ARROW_TARGET);
	TEST(f->setProperty(KEY_TARGET,s,needUp),"Set target prop");
	
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"refresh error code");

	delete f;


	TEST(streamOut.size(),"stream size");

	//Count the number of text object types
	size_t vecCount,otherDrawCount;
	vecCount=otherDrawCount=0;
	for(unsigned int ui=0;ui<streamOut.size(); ui++)
	{
		switch(streamOut[ui]->getStreamType())
		{
			case STREAM_TYPE_DRAW:
			{
				const DrawStreamData* d;
				d= (const DrawStreamData*)streamOut[ui];
				for(unsigned int ui=0;ui<d->drawables.size();ui++)
				{
					switch(d->drawables[ui]->getType())
					{
						case DRAW_TYPE_VECTOR:
						{
							vecCount++;
							const DrawVector *dv;
							dv= (const DrawVector*)d->drawables[ui];
							bool testV;
							testV=(dv->getOrigin() == ARROW_ORIGIN);
							TEST(testV,"Origin test");
							break;
						}
						default:
							otherDrawCount++;
							break;
					}

				}
				break;
			}
			default:
				;
		}

	}

	//We should have a line, one would hope
	TEST(vecCount==1,"Number of lines");
	TEST(otherDrawCount==0,"Draw count");

	for(unsigned int ui=0;ui<streamOut.size();ui++)
		delete streamOut[ui];

	return true;
}

bool textArrowTest()
{
	vector<const FilterStreamData*> streamIn,streamOut;
	AnnotateFilter*f=new AnnotateFilter;
	f->setCaching(false);
	
	bool needUp; std::string s;
	//Set linear ruler mode
	TEST(f->setProperty(KEY_MODE,
		TRANS(annotationModeStrings[ANNOTATION_TEXT_WITH_ARROW]),needUp),"Set Property");
	//Set ruler position & length
	const Point3D ARROW_ORIGIN(-1,-1,-1);
	const Point3D ARROW_TARGET(1,1,1);
	stream_cast(s,ARROW_ORIGIN);
	TEST(f->setProperty(KEY_POSITION,s,needUp),"Set prop");
	stream_cast(s,ARROW_TARGET);
	TEST(f->setProperty(KEY_TARGET,s,needUp),"Set prop");
	
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Refresh error code");

	delete f;


	TEST(streamOut.size(),"stream size");

	//Count the number of text object types
	size_t vecCount,textCount,otherDrawCount;
	vecCount=textCount=otherDrawCount=0;
	for(unsigned int ui=0;ui<streamOut.size(); ui++)
	{
		switch(streamOut[ui]->getStreamType())
		{
			case STREAM_TYPE_DRAW:
			{
				const DrawStreamData* d;
				d= (const DrawStreamData*)streamOut[ui];
				for(unsigned int ui=0;ui<d->drawables.size();ui++)
				{
					switch(d->drawables[ui]->getType())
					{
						case DRAW_TYPE_VECTOR:
						{
							vecCount++;
							const DrawVector *dv;
							dv= (const DrawVector*)d->drawables[ui];
							bool testV;
							testV=(dv->getOrigin() == ARROW_ORIGIN);
							TEST(testV,"Origin test");
							break;
						}
						case DRAW_TYPE_GLTEXT:
							textCount++;
							break;
						default:
							otherDrawCount++;
							break;
					}

				}
				break;
			}
			default:
				;
		}

	}

	//We should have a line, one would hope
	TEST(vecCount==1,"Number of lines");
	TEST(textCount==1,"Number of text objects");
	TEST(otherDrawCount==0,"No other draw items");

	for(unsigned int ui=0;ui<streamOut.size();ui++)
		delete streamOut[ui];

	return true;
}


#endif
