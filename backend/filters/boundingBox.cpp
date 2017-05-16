/*
 *	boundingBox.cpp - Place a bounding box around point datasets
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
#include "boundingBox.h"

#include "filterCommon.h"

using std::vector;
using std::string;
using std::pair;
using std::make_pair;

enum
{
	KEY_VISIBLE=1,
	KEY_COUNT_X,
	KEY_COUNT_Y,
	KEY_COUNT_Z,
	KEY_FONTSIZE,
	KEY_FONTCOLOUR,
	KEY_FIXEDOUT,
	KEY_LINECOLOUR,
	KEY_LINEWIDTH,
	KEY_SPACING_X,
	KEY_SPACING_Y,
	KEY_SPACING_Z,
	KEY_SHOW_TICKS_X,
	KEY_SHOW_TICKS_Y,
	KEY_SHOW_TICKS_Z,
	KEY_STYLE,
	KEY_ABSCOORDS
};

enum
{
	BOUNDINGBOX_ABORT_ERR,
};


enum
{
	BOUND_STYLE_BOX_ONLY,
	BOUND_STYLE_TICKS,
	BOUND_STYLE_DIMENSION,
	BOUND_STYLE_ENUM_END
};

const char *BOUND_STYLE[] = 
{
	NTRANS("Box only"),
	NTRANS("Tick"),
	NTRANS("Dimension")
};


//=== Bounding box filter ==


BoundingBoxFilter::BoundingBoxFilter() : isVisible(true), boundStyle(BOUND_STYLE_TICKS),
	fixedNumTicks(true), fontSize(5), absoluteCoords(false),
	lineColour(0,0,1.0f), lineWidth(2.0f), threeDText(true)
{
	for(unsigned int ui=0;ui<3;ui++)
	{
		numTicks[ui]=12;
		tickSpacing[ui]=5.0f;
		enableTicks[ui]=true;
	}

	cacheOK=false;
	cache=false; 
}

Filter *BoundingBoxFilter::cloneUncached() const
{
	BoundingBoxFilter *p=new BoundingBoxFilter();
	p->fixedNumTicks=fixedNumTicks;
	for(unsigned int ui=0;ui<3;ui++)
	{
		p->numTicks[ui]=numTicks[ui];
		p->tickSpacing[ui]=tickSpacing[ui];
		p->enableTicks[ui]=enableTicks[ui];
	}

	p->isVisible=isVisible;
	p->boundStyle=boundStyle;
	p->absoluteCoords = absoluteCoords;

	p->threeDText=threeDText;	

	p->lineWidth=lineWidth;
	p->fontSize=fontSize;

	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t BoundingBoxFilter::numBytesForCache(size_t nObjects) const
{
	//we don't really know without examining full data. but guess
	return (size_t)10000;
}

void BoundingBoxFilter::drawTicks(const BoundCube &bTotal, DrawStreamData *d) const
{

	//Add the rectangle drawable
	DrawRectPrism *dP = new DrawRectPrism;
	dP->setAxisAligned(bTotal);
	dP->setColour(lineColour.r(),lineColour.g(),lineColour.b(),lineColour.a());
	dP->setLineWidth(lineWidth);
	d->drawables.push_back(dP);

	//Add the tick drawables
	Point3D tickOrigin,tickEnd;
	bTotal.getBounds(tickOrigin,tickEnd);

	float tmpTickSpacing[3];
	float tmpTickCount[3];
	if(fixedNumTicks)
	{
		for(unsigned int ui=0; ui<3;ui++)
		{
			ASSERT(numTicks[ui]);
			tmpTickSpacing[ui]=( (tickEnd[ui] - tickOrigin[ui])/(float)(numTicks[ui]-1));
			tmpTickCount[ui]=numTicks[ui];
		}
	}
	else
	{
		for(unsigned int ui=0; ui<3;ui++)
		{
			ASSERT(numTicks[ui]);
			tmpTickSpacing[ui]= tickSpacing[ui];
			tmpTickCount[ui]=(unsigned int)((tickEnd[ui] - tickOrigin[ui])/tickSpacing[ui])+1;
		}
	}

	//flag to see if we have to draw the 0 corner later on
	bool tickSet=false;
	//Draw the ticks on the box perimeter.
	for(unsigned int ui=0;ui<3;ui++)
	{
		if(!enableTicks[ui])
			continue;

		tickSet=true;

		Point3D tickVector;
		Point3D tickPosition;
		Point3D textVector;

		tickPosition=tickOrigin;
		switch(ui)
		{
			case 0:
				tickVector=Point3D(0,-1,-1);
				textVector=Point3D(0,1,0);
				break;
			case 1:
				tickVector=Point3D(-1,0,-1);
				textVector=Point3D(1,0,0);
				break;
			case 2:
				tickVector=Point3D(-1,-1,0);
				textVector=Point3D(1,1,0);
				break;
		}

		//TODO: This would be more efficient if we made some kind of 
		//"comb" class?
		//Allow up to  128 chars
		char buffer[128];
		for(unsigned int uj=0;uj<tmpTickCount[ui];uj++)
		{
			DrawVector *dV;
			tickPosition[ui]=tmpTickSpacing[ui]*uj + tickOrigin[ui];
			dV = new DrawVector;
	
			dV->setDrawArrow(false);
			dV->setOrigin(tickPosition);
			dV->setVector(tickVector);
			dV->setColour(lineColour.r(),lineColour.g(),
					lineColour.b(), lineColour.a());

			d->drawables.push_back(dV);
	

			//Don't draw the 0 value, as this gets repeated. 
			//we will handle this separately
			if(uj)
			{
				DrawGLText *dT;
				//Draw the tick text
				if( threeDText)	
					dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_POLYGON);
				else
					dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_BITMAP);
				float f;
				if(absoluteCoords)
				{
					f = tmpTickSpacing[ui]*uj + tickOrigin[ui];
				}
				else
					f = tmpTickSpacing[ui]*uj;
				snprintf(buffer,127,"%2.0f",f);
				dT->setString(buffer);
				dT->setSize(fontSize);
				
				dT->setColour(lineColour.r(),lineColour.g(),
						lineColour.b(),lineColour.a());
				dT->setOrigin(tickPosition + tickVector*2);	
				dT->setUp(Point3D(0,0,1));	
				dT->setTextDir(textVector);
				dT->setAlignment(DRAWTEXT_ALIGN_RIGHT);

				d->drawables.push_back(dT);
			}
		}

	}


	if(!absoluteCoords && tickSet)
	{
		DrawGLText *dT; 
		if(threeDText)
			dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_POLYGON);
		else
			dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_BITMAP);

		//Handle "0" text value
		dT->setString("0");
		
		dT->setColour(lineColour.r(),lineColour.g(),
			lineColour.b(),lineColour.a());
		dT->setSize(fontSize);
		dT->setOrigin(tickOrigin+ Point3D(-1,-1,-1));
		dT->setAlignment(DRAWTEXT_ALIGN_RIGHT);
		dT->setUp(Point3D(0,0,1));	
		dT->setTextDir(Point3D(-1,-1,0));
		d->drawables.push_back(dT);
	}

}

void BoundingBoxFilter::drawDimension(const BoundCube &bTotal, DrawStreamData *d) const
{
	//Add the rectangle drawable
	DrawRectPrism *dP = new DrawRectPrism;
	dP->setAxisAligned(bTotal);
	dP->setColour(lineColour.r(),lineColour.g(),
				lineColour.b(),lineColour.a());
	dP->setLineWidth(lineWidth);
	d->drawables.push_back(dP);



	//Add the arrows from the start to the end
	//Create the position from which to draw the tick origins
	Point3D tickOrigin,tickEnd;
	bTotal.getBounds(tickOrigin,tickEnd);



	const float ARROW_SCALE_FACTOR =0.03f;
	const float OFFSET=0.07f;

	Point3D halfPt;
	halfPt=(tickEnd-tickOrigin)*0.5f + tickOrigin;

	
	float maxLen;
	{
		Point3D delta;
		delta=tickEnd-tickOrigin;
		maxLen=std::max(std::max(delta[0],delta[1]),delta[2]);
	}

	float offset;
	offset=maxLen*OFFSET;

	Point3D centrePt[3];
		
	centrePt[0] = Point3D(halfPt[0],tickOrigin[1]-offset,tickOrigin[2]-OFFSET);
	centrePt[1] = Point3D(tickOrigin[0]-offset,halfPt[1],tickOrigin[2]-OFFSET);
	centrePt[2] = Point3D(tickOrigin[0]-offset,tickOrigin[1] -OFFSET , halfPt[2]);


	//Draw the arrows around the edge of the box
	for(size_t ui=0;ui<3;ui++)
	{
		if(!enableTicks[ui])
			continue;

		float len;
		len=(tickEnd[ui]-tickOrigin[ui])*0.5f;
	
		//Draw vector for the axis, and set arrow mode
		DrawVector *dV;
		dV= new DrawVector;

		dV->setColour(lineColour.r(),lineColour.g(),
				lineColour.b(),lineColour.a());	
		dV->wantsLight=true;
		
		dV->setArrowSize(maxLen*ARROW_SCALE_FACTOR);
		dV->setDoubleEnded();
		
		Point3D p;
		p.setValue(0,0,0);
		p.setValue(ui,len);
		
		dV->setOrigin(centrePt[ui]-p);
		dV->setVector(p*2.0f);



		d->drawables.push_back(dV);

	}




	//Draw the values for the box dimensions, as text
	char *buffer=new char[128];
	for(size_t ui=0;ui<3;ui++)
	{
		if(!enableTicks[ui])
			continue;

		BoundCube textCube;
		DrawGLText *dT;
		dT = new DrawGLText(getDefaultFontFile().c_str(),FTGL_POLYGON);

		if(!absoluteCoords)
		{
			float len;
			len=(tickEnd[ui]-tickOrigin[ui]);

			snprintf(buffer,127,"%5.1f",len);
		}
		else
		{
			snprintf(buffer,127,"%5.1f , %5.1f",
				tickOrigin[ui], tickEnd[ui]);

		}
		dT->setString(buffer);
		dT->setSize(fontSize);

		dT->setColour(lineColour.r(),lineColour.g(),
				lineColour.b(),lineColour.a());
		dT->setOrigin(centrePt[ui]);	
		dT->setAlignment(DRAWTEXT_ALIGN_CENTRE);
		
		switch(ui)
		{
			case 0:

				dT->setUp(Point3D(0,0,1));	
				dT->setTextDir(Point3D(1,0,0));
				break;
			case 1:
				dT->setUp(Point3D(1,0,0));	
				dT->setTextDir(Point3D(0,-1,0));
				break;
			case 2:
				dT->setUp(Point3D(0,1,0));	
				dT->setTextDir(Point3D(0,0,1));
				break;
		}


		d->drawables.push_back(dT);
	}

	delete[] buffer;


}

unsigned int BoundingBoxFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{

	if(!isVisible)
	{
		propagateStreams(dataIn,getOut);
		return 0;
	}

	//Compute the bounding box of the incoming streams
	BoundCube bTotal,bThis;
	bTotal.setInverseLimits();

	size_t totalSize=numElements(dataIn);
	size_t n=0;
	Point3D p[4];
	unsigned int ptCount=0;
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				bThis=bTotal;
				//Grab the first four points to define a volume.
				//then expand that volume using the boundcube functions.
				const IonStreamData *d =(const IonStreamData *) dataIn[ui];
				size_t dataPos=0;
				unsigned int curProg=NUM_CALLBACK;
				while(ptCount < 4 && dataPos < d->data.size())
				{
					for(unsigned int ui=0; ui<d->data.size();ui++)
					{
						p[ptCount]=d->data[ui].getPosRef();
						ptCount++;
						dataPos=ui;
						if(ptCount >=4) 
							break;
					}
				}

				//Ptcount will be 4 if we have >=4 points in dataset
				if(ptCount < 4)
					break;
				bThis.setBounds(p,4);
				//Expand the bounding volume
#ifdef _OPENMP
				//Parallel version
				unsigned int nT =omp_get_max_threads(); 

				BoundCube *newBounds= new BoundCube[nT];
				for(unsigned int ui=0;ui<nT;ui++)
					newBounds[ui]=bThis;

				bool spin=false;
				#pragma omp parallel for shared(spin)
				for(unsigned int ui=dataPos;ui<d->data.size();ui++)
				{
					unsigned int thisT=omp_get_thread_num();
					//OpenMP does not allow exiting. Use spin instead
					if(spin)
						continue;

					if(!curProg--)
					{
						#pragma omp critical
						{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
						}


						if(thisT == 0)
						{
							if(*Filter::wantAbort)
								spin=true;
						}
					}

					newBounds[thisT].expand(d->data[ui].getPosRef());
				}
				if(spin)
				{			
					delete d;
					delete[] newBounds;
					return BOUNDINGBOX_ABORT_ERR;
				}

				for(unsigned int ui=0;ui<nT;ui++)
					bThis.expand(newBounds[ui]);

				delete[] newBounds;
#else
				//Single thread version
				for(unsigned int ui=dataPos;ui<d->data.size();ui++)
				{
					bThis.expand(d->data[ui].getPosRef());
					if(!curProg--)
					{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
						if(*Filter::wantAbort)
						{
							delete d;
							return BOUNDINGBOX_ABORT_ERR;
						}
					}
				}
#endif
				bTotal.expand(bThis);
				progress.filterProgress=100;
				break;
			}
			default:
				break;
		}

		//Copy the input data to the output	
		getOut.push_back(dataIn[ui]);	
	}

	//Append the bounding box if it is valid
	if(bTotal.isValid())
	{
		DrawStreamData *d = new DrawStreamData;
		d->parent=this;

		switch(boundStyle)
		{
			case BOUND_STYLE_BOX_ONLY:
			{
				//Add the rectangle drawable
				DrawRectPrism *dP = new DrawRectPrism;
				dP->setAxisAligned(bTotal);
				dP->setColour(lineColour.r(),lineColour.g(),
						lineColour.b(),lineColour.a());
				dP->setLineWidth(lineWidth);
				d->drawables.push_back(dP);
				break;
			}
			case BOUND_STYLE_TICKS:
				drawTicks(bTotal,d);
				break;
			case BOUND_STYLE_DIMENSION:
				drawDimension(bTotal,d);
				break;
			default:
				ASSERT(false);
		}
		d->cached=0;
		
		getOut.push_back(d);
	}
	return 0;
}


void BoundingBoxFilter::getProperties(FilterPropGroup &propertyList) const
{
	FilterProperty p;
	size_t curGroup=0;

	string tmpStr;
	stream_cast(tmpStr,isVisible);
	p.name=TRANS("Visible");
	p.data= tmpStr;
	p.key=KEY_VISIBLE;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("If true, show box, otherwise hide box");
	propertyList.addProperty(p,curGroup);

	if(isVisible)
	{
		vector<pair<unsigned int,string> > choices;
		for(size_t ui=0;ui<BOUND_STYLE_ENUM_END; ui++)
		{
			tmpStr=TRANS(BOUND_STYLE[ui]);
			choices.push_back(make_pair(ui,tmpStr));
		}

		tmpStr= choiceString(choices,boundStyle);
		p.name=TRANS("Style");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_CHOICE;
		p.helpText=TRANS("Box display mode");
		p.key=KEY_STYLE;
		propertyList.addProperty(p,curGroup);
		propertyList.setGroupTitle(curGroup,TRANS("Display mode"));
		curGroup++;

		
		if(boundStyle == BOUND_STYLE_TICKS)
		{

			//Properties are X Y and Z counts on ticks
			stream_cast(tmpStr,fixedNumTicks);
			p.name=TRANS("Fixed Tick Num");
			p.data=tmpStr;
			p.key=KEY_FIXEDOUT;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("If true, evenly use specified number of ticks. Otherwise, use distance to determine tick count");
			propertyList.addProperty(p,curGroup);

			if(fixedNumTicks)
			{
				//Properties are X Y and Z counts on ticks
				stream_cast(tmpStr,numTicks[0]);
				p.key=KEY_COUNT_X;
				p.name=TRANS("Num X");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_INTEGER;
				p.helpText=TRANS("Tick count in X direction");
				propertyList.addProperty(p,curGroup);
				
				stream_cast(tmpStr,numTicks[1]);
				p.key=KEY_COUNT_Y;
				p.name=TRANS("Num Y");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_INTEGER;
				p.helpText=TRANS("Tick count in Y direction");
				propertyList.addProperty(p,curGroup);
				
				stream_cast(tmpStr,numTicks[2]);
				p.key=KEY_COUNT_Z;
				p.name=TRANS("Num Z");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_INTEGER;
				p.helpText=TRANS("Tick count in Z direction");
				propertyList.addProperty(p,curGroup);
			}
			else
			{
				stream_cast(tmpStr,tickSpacing[0]);
				p.name=TRANS("Spacing X");
				p.data= tmpStr;
				p.key=KEY_SPACING_X;
				p.type=PROPERTY_TYPE_REAL;
				p.helpText=TRANS("Distance between ticks on X axis");
				propertyList.addProperty(p,curGroup);

				stream_cast(tmpStr,tickSpacing[1]);
				p.name=TRANS("Spacing Y");
				p.data= tmpStr;
				p.key=KEY_SPACING_Y;
				p.type=PROPERTY_TYPE_REAL;
				p.helpText=TRANS("Distance between ticks on Y axis");
				propertyList.addProperty(p,curGroup);

				stream_cast(tmpStr,tickSpacing[2]);
				p.name=TRANS("Spacing Z");
				p.data= tmpStr;
				p.key=KEY_SPACING_Z;
				p.type=PROPERTY_TYPE_REAL;
				p.helpText=TRANS("Distance between ticks on Z axis");
				propertyList.addProperty(p,curGroup);
			}
		}

		if(boundStyle!=BOUND_STYLE_BOX_ONLY)
		{	
			tmpStr=boolStrEnc(enableTicks[0]);	
			p.name=TRANS("Ticks X");
			p.data= tmpStr;
			p.key=KEY_SHOW_TICKS_X;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Display tick marks on X axis");
			propertyList.addProperty(p,curGroup);

			tmpStr=boolStrEnc(enableTicks[1]);	
			p.name=TRANS("Ticks Y");
			p.data= tmpStr;
			p.key=KEY_SHOW_TICKS_Y;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Display tick marks on Y axis");
			propertyList.addProperty(p,curGroup);
			
			tmpStr=boolStrEnc(enableTicks[2]);	
			p.name=TRANS("Ticks Z");
			p.data= tmpStr;
			p.key=KEY_SHOW_TICKS_Z;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Display tick marks on Z axis");
			propertyList.addProperty(p,curGroup);

			propertyList.setGroupTitle(curGroup,TRANS("Tick marks"));

		}


		//Colour

		p.name=TRANS("Box Colour");
		p.data= lineColour.toColourRGBA().rgbString();
		p.key=KEY_LINECOLOUR;
		p.type=PROPERTY_TYPE_COLOUR;
		p.helpText=TRANS("Colour of the bounding box");
		propertyList.addProperty(p,curGroup);

		//Line thickness
		stream_cast(tmpStr,lineWidth);
		p.name=TRANS("Line thickness");
		p.data= tmpStr;
		p.key=KEY_LINEWIDTH;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Thickness of the lines used to draw the box");
		propertyList.addProperty(p,curGroup);

		//Font size	
		if(boundStyle != BOUND_STYLE_BOX_ONLY)
		{
			stream_cast(tmpStr,fontSize);
			p.key=KEY_FONTSIZE;
			p.name=TRANS("Font Size");
			p.data= tmpStr;
			p.type=PROPERTY_TYPE_INTEGER;
			p.helpText=TRANS("Relative size for text");
			propertyList.addProperty(p,curGroup);

			
			p.key=KEY_ABSCOORDS;
			p.name=TRANS("Abs. Coords");
			p.data = boolStrEnc(absoluteCoords);
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Show labels using aboslute coo-ordinates");
	
			propertyList.addProperty(p,curGroup);
		}
	}
	propertyList.setGroupTitle(curGroup,TRANS("Appearance"));
}

bool BoundingBoxFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_VISIBLE:
		{
			if(!applyPropertyNow(isVisible,value,needUpdate))
				return false;
			break;
		}	
		case KEY_STYLE:
		{
			size_t ltmp=BOUND_STYLE_ENUM_END;
			for(unsigned int ui=0;ui<BOUND_STYLE_ENUM_END;ui++)
			{
				if(value == TRANS(BOUND_STYLE[ui]))
				{
					ltmp=ui;
					break;
				}
			}
			
			if(ltmp>=BOUND_STYLE_ENUM_END)
				return false;
			
			if(ltmp == boundStyle)
				needUpdate=false;
			else
			{
				boundStyle=ltmp;
				needUpdate=true;
				clearCache();
			}

			break;
		}	
		case KEY_FIXEDOUT:
		{
			if(!applyPropertyNow(fixedNumTicks,value,needUpdate))
				return false;
			break;
		}	
		case KEY_COUNT_X:
		case KEY_COUNT_Y:
		case KEY_COUNT_Z:
		{
			ASSERT(fixedNumTicks);
			unsigned int newCount;
			if(stream_cast(newCount,value))
				return false;

			//there is a start and an end tick, at least
			if(newCount < 2)
				return false;

			numTicks[key-KEY_COUNT_X]=newCount;
			needUpdate=true;
			break;
		}
		case KEY_LINECOLOUR:
		{
			ColourRGBA newLineColour;
			if(!newLineColour.parse(value))
				return false;


			if(lineColour.toColourRGBA() != newLineColour) 
				needUpdate=true;
			lineColour=newLineColour.toRGBAf();

			needUpdate=true;
			break;
		}
		case KEY_LINEWIDTH:
		{
			float newWidth;
			if(stream_cast(newWidth,value))
				return false;

			if(newWidth <= 0.0f)
				return false;

			lineWidth=newWidth;
			needUpdate=true;
			break;
		}
		case KEY_SPACING_X:
		case KEY_SPACING_Y:
		case KEY_SPACING_Z:
		{
			ASSERT(!fixedNumTicks);
			float newSpacing;
			if(stream_cast(newSpacing,value))
				return false;

			if(newSpacing <= 0.0f)
				return false;

			tickSpacing[key-KEY_SPACING_X]=newSpacing;
			needUpdate=true;
			break;
		}
		case KEY_SHOW_TICKS_X:
		case KEY_SHOW_TICKS_Y:
		case KEY_SHOW_TICKS_Z:
		{
			bool enabled;
			if(stream_cast(enabled,value))
				return false;

			enableTicks[key-KEY_SHOW_TICKS_X]=enabled;
			needUpdate=true;
			break;
		}
		case KEY_FONTSIZE:
		{
			if(!applyPropertyNow(fontSize,value,needUpdate))
				return false;
			break;
		}
		case KEY_ABSCOORDS:
		{
			if(!applyPropertyNow(absoluteCoords,value,needUpdate))
				return false;
			break;
		}
		default:
			ASSERT(false);
	}	
	return true;
}


std::string  BoundingBoxFilter::getSpecificErrString(unsigned int code) const
{

	//Currently the only error is aborting
	return std::string("Aborted");
}

void BoundingBoxFilter::setPropFromBinding(const SelectionBinding &b)
{
	{ASSERT(false);}
}

bool BoundingBoxFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;
			f << tabs(depth+1) << "<visible value=\"" << isVisible << "\"/>" << endl;
			f << tabs(depth+1) << "<boundstyle value=\"" << boundStyle<< "\"/>" << endl;
			f << tabs(depth+1) << "<fixedticks value=\"" << fixedNumTicks << "\"/>" << endl;
			f << tabs(depth+1) << "<ticknum x=\""<<numTicks[0]<< "\" y=\"" 
				<< numTicks[1] << "\" z=\""<< numTicks[2] <<"\"/>"  << endl;
			f << tabs(depth+1) << "<tickspacing x=\""<<tickSpacing[0]<< "\" y=\"" 
				<< tickSpacing[1] << "\" z=\""<< tickSpacing[2] <<"\"/>"  << endl;
			f << tabs(depth+1) << "<ticksenabled x=\""<<enableTicks[0]<< "\" y=\"" 
				<< enableTicks[1] << "\" z=\""<< enableTicks[2] <<"\"/>"  << endl;
			f << tabs(depth+1) << "<linewidth value=\"" << lineWidth << "\"/>"<<endl;
			f << tabs(depth+1) << "<fontsize value=\"" << fontSize << "\"/>"<<endl;
			f << tabs(depth+1) << "<colour r=\"" <<  lineColour.r()<< "\" g=\"" << lineColour.g() << "\" b=\"" <<lineColour.b()  
								<< "\" a=\"" << lineColour.a() << "\"/>" <<endl;
			f << tabs(depth+1) << "<absolutecoords value=\"" << boolStrEnc(absoluteCoords) << "\"/>" << endl;
			f << tabs(depth) << "</" <<trueName()<< ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool BoundingBoxFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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

	//Retrieve visibility 
	//====
	if(XMLHelpFwdToElem(nodePtr,"visible"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(!boolStrDec(tmpStr,isVisible))
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve box style 
	//====
	if(XMLHelpFwdToElem(nodePtr,"boundstyle"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(boundStyle,tmpStr))
		return false;

	xmlFree(xmlString);
	//====
	
	
	//Retrieve fixed tick num
	//====
	if(XMLHelpFwdToElem(nodePtr,"fixedticks"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(!boolStrDec(tmpStr,fixedNumTicks))
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve num ticks
	//====
	if(XMLHelpFwdToElem(nodePtr,"ticknum"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(numTicks[0],tmpStr))
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(numTicks[1],tmpStr))
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(numTicks[2],tmpStr))
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve spacing
	//====
	if(XMLHelpFwdToElem(nodePtr,"tickspacing"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(tickSpacing[0],tmpStr))
		return false;

	if(tickSpacing[0] < 0.0f)
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(tickSpacing[1],tmpStr))
		return false;
	if(tickSpacing[1] < 0.0f)
		return false;

	xmlFree(xmlString);

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(tickSpacing[2],tmpStr))
		return false;

	if(tickSpacing[2] < 0.0f)
		return false;
	xmlFree(xmlString);
	//====

	//Retrieve ticks enabled (3Depict > 0.0.18)
	//===
	xmlNodePtr tmpNode=nodePtr;
	if(XMLHelpFwdToElem(nodePtr,"ticksenabled"))
	{
		//loading failed, use default (enabled)
		for(unsigned int ui=0; ui<3; ui++)
			enableTicks[ui] =true;

		nodePtr=tmpNode;
	}
	else
	{
		const char *XYZ[]={"x","y","z"};
		for(unsigned int ui=0;ui<3;ui++)
		{
			xmlString=xmlGetProp(nodePtr,(const xmlChar *)XYZ[ui]);
			if(!xmlString)
				return false;
			tmpStr=(char *)xmlString;

			if(!boolStrDec(tmpStr,enableTicks[ui]))
				return false;

			xmlFree(xmlString);
		}
	}

	//===
	
	//Retrieve line width 
	//====
	if(XMLHelpFwdToElem(nodePtr,"linewidth"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(lineWidth,tmpStr))
		return false;

	if(lineWidth < 0)
	       return false;	
	xmlFree(xmlString);
	//====
	
	//Retrieve font size 
	//====
	if(XMLHelpFwdToElem(nodePtr,"fontsize"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(fontSize,tmpStr))
		return false;

	xmlFree(xmlString);
	//====

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;

	ColourRGBAf tmpCol;
	if(!parseXMLColour(nodePtr,tmpCol))
		return false;
	lineColour=tmpCol;
	//====

	//Retrieve absolute coordinates (only for 3Depict > 0.0.18) 
	//====
	if(XMLHelpFwdToElem(nodePtr,"absolutecoords"))
		absoluteCoords=false;
	else
	{
		std::string s;
		if(XMLHelpGetProp(s,nodePtr,"value"))
			return false;
		if(!boolStrDec(s,absoluteCoords))
			return false;
	}
	//====

	return true;	
}

unsigned int BoundingBoxFilter::getRefreshBlockMask() const
{
	//Everything goes through this filter
	return 0;
}

unsigned int BoundingBoxFilter::getRefreshEmitMask() const
{
	if(isVisible)
		return STREAM_TYPE_DRAW;
	else
		return 0;
}

unsigned int BoundingBoxFilter::getRefreshUseMask() const
{
	if(isVisible)
		return  STREAM_TYPE_IONS;
	else
		return 0;
}

#ifdef DEBUG
bool boxVolumeTest();

bool BoundingBoxFilter::runUnitTests() 
{
	if(!boxVolumeTest())
		return false;

	return true;
}


bool boxVolumeTest()
{
	//Synthesise data
	//---
	IonStreamData *d = new IonStreamData;

	vector<const FilterStreamData *> streamIn,streamOut;

	IonHit h;
	h.setMassToCharge(1);
	
	h.setPos(Point3D(0,0,1));
	d->data.push_back(h);
	h.setPos(Point3D(0,1,0));
	d->data.push_back(h);
	h.setPos(Point3D(1,0,0));
	d->data.push_back(h);
	h.setPos(Point3D(0,0,0));
	d->data.push_back(h);
	
	streamIn.push_back(d);
	//---

	
	//Set up and run filter
	//---
	BoundingBoxFilter *b = new BoundingBoxFilter;
	b->setCaching(false);

	bool needUp;
	TEST(b->setProperty(KEY_VISIBLE,"1",needUp),"Set prop");


	ProgressData p;
	TEST(!b->refresh(streamIn,streamOut,p),"Refresh error code");
	//---

	//Run tests 
	//---
	BoundCube bc;
	bool havePrismDrawable=false;
	for(unsigned int ui=0;ui<streamOut.size(); ui++)
	{
		if(streamOut[ui]->getStreamType() != STREAM_TYPE_DRAW)
			continue;
		
		DrawStreamData *drawData;
		drawData=(DrawStreamData*)streamOut[ui];
	
		for(unsigned int uj=0;uj<drawData->drawables.size(); uj++)
		{
			DrawableObj *draw;
			draw= drawData->drawables[uj];

			if(draw->getType() == DRAW_TYPE_RECTPRISM)
			{
				draw->getBoundingBox(bc);

				havePrismDrawable=true;
				break;
			}
		}

		if(havePrismDrawable)
			break;
	
	}


	TEST(havePrismDrawable, "bounding box existence test");
	
	TEST(fabs(bc.volume() - 1.0f) 
		< sqrt(std::numeric_limits<float>::epsilon()),
						"Bounding volume test");

	//Cleanup the emitted pointers
	for(unsigned int ui=0;ui<streamOut.size(); ui++)
		delete streamOut[ui];	

	delete b;	
	
	return true;
}
#endif

