/*
 *	ionColour.cpp - Filter to create coloured batches of ions based upon value
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
#include "ionColour.h"

#include "filterCommon.h"

#include "common/colourmap.h"

using std::vector;
using std::string;
using std::pair;
using std::make_pair;


const unsigned int MAX_NUM_COLOURS=256;
enum
{
	KEY_IONCOLOURFILTER_COLOURMAP,
	KEY_IONCOLOURFILTER_MAPSTART,
	KEY_IONCOLOURFILTER_MAPEND,
	KEY_IONCOLOURFILTER_NCOLOURS,
	KEY_IONCOLOURFILTER_REVERSE,
	KEY_IONCOLOURFILTER_SHOWBAR,
	KEY_IONCOLOURFILTER_ALPHA,
};

enum
{
	IONCOLOUR_ABORT_ERR
};

IonColourFilter::IonColourFilter() : colourMap(0),reverseMap(false), 
		nColours(MAX_NUM_COLOURS),showColourBar(true), alpha(1.0f)
{
	mapBounds[0] = 0.0f;
	mapBounds[1] = 100.0f;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up

}

Filter *IonColourFilter::cloneUncached() const
{
	IonColourFilter *p=new IonColourFilter();
	p->colourMap = colourMap;
	p->mapBounds[0]=mapBounds[0];
	p->mapBounds[1]=mapBounds[1];
	p->nColours =nColours;	
	p->alpha = alpha;
	p->showColourBar =showColourBar;	
	p->reverseMap=reverseMap;	
	
	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t IonColourFilter::numBytesForCache(size_t nObjects) const
{
		return (size_t)((float)(nObjects*IONDATA_SIZE));
}



unsigned int IonColourFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		ASSERT(filterOutputs.size());
		propagateStreams(dataIn,getOut,getRefreshBlockMask(),true);

		propagateCache(getOut);

		return 0;
	}


	ASSERT(nColours >0 && nColours<=MAX_NUM_COLOURS);
	IonStreamData *d[nColours];
	unsigned char rgb[3]; //RGB array
	//Build the colourmap values, each as a unique filter output
	for(unsigned int ui=0;ui<nColours; ui++)
	{
		d[ui]=new IonStreamData;
		d[ui]->parent=this;
		float value;
		value = (float)ui*(mapBounds[1]-mapBounds[0])/(float)nColours + mapBounds[0];
		//Pick the desired colour map
		colourMapWrap(colourMap,rgb,value,mapBounds[0],mapBounds[1],reverseMap);
	
		d[ui]->r=rgb[0]/255.0f;
		d[ui]->g=rgb[1]/255.0f;
		d[ui]->b=rgb[2]/255.0f;
		d[ui]->a=1.0f;
	}



	//Try to maintain ion size if possible
	bool haveIonSize,sameSize; // have we set the ionSize?
	float ionSize;
	haveIonSize=false;
	sameSize=true;

	//Did we find any ions in this pass?
	bool foundIons=false;	
	unsigned int totalSize=numElements(dataIn);
	unsigned int curProg=NUM_CALLBACK;
	size_t n=0;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				foundIons=true;

				//Check for ion size consistency	
				if(haveIonSize)
				{
					sameSize &= (fabs(ionSize-((const IonStreamData *)dataIn[ui])->ionSize) 
									< std::numeric_limits<float>::epsilon());
				}
				else
				{
					ionSize=((const IonStreamData *)dataIn[ui])->ionSize;
					haveIonSize=true;
				}
				for(vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
					       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
				{
					//Work out the colour map assignment from the mass to charge.
					// linear assignment in range
					unsigned int colour;

					float tmp;	
					tmp= (it->getMassToCharge()-mapBounds[0])/(mapBounds[1]-mapBounds[0]);
					tmp = std::max(0.0f,tmp);
					tmp = std::min(tmp,1.0f);
					
					colour=(unsigned int)(tmp*(float)(nColours-1));	
					d[colour]->data.push_back(*it);
				
					//update progress every CALLBACK ions
					if(!curProg--)
					{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
						curProg=NUM_CALLBACK;
						if(*Filter::wantAbort)
						{
							for(unsigned int ui=0;ui<nColours;ui++)
								delete d[ui];
							return IONCOLOUR_ABORT_ERR;
						}
					}
				}

				
				break;
			}
			default:
				getOut.push_back(dataIn[ui]);

		}
	}

	//create the colour bar as needed
	if(foundIons && showColourBar)
	{
		DrawStreamData *d = new DrawStreamData;
		d->drawables.push_back(makeColourBar(mapBounds[0],mapBounds[1],nColours,colourMap,reverseMap,alpha));
		d->parent=this;
		cacheAsNeeded(d);
		getOut.push_back(d);
	}


	//If all the ions are the same size, then propagate
	if(haveIonSize && sameSize)
	{
		for(unsigned int ui=0;ui<nColours;ui++)
			d[ui]->ionSize=ionSize;
	}
	//merge the results as needed
	if(cache)
	{
		for(unsigned int ui=0;ui<nColours;ui++)
		{
			if(d[ui]->data.size())
				d[ui]->cached=1;
			else
				d[ui]->cached=0;
			if(d[ui]->data.size())
				filterOutputs.push_back(d[ui]);
		}
		cacheOK=filterOutputs.size();
	}
	else
	{
		for(unsigned int ui=0;ui<nColours;ui++)
		{
			//NOTE: MUST set cached BEFORE push_back!
			d[ui]->cached=0;
		}
		cacheOK=false;
	}

	//push the colours onto the output. cached or not (their status is set above).
	for(unsigned int ui=0;ui<nColours;ui++)
	{
		if(d[ui]->data.size())
			getOut.push_back(d[ui]);
		else
			delete d[ui];
	}
	
	return 0;
}


void IonColourFilter::getProperties(FilterPropGroup &propertyList) const
{

	FilterProperty p;
	string tmpStr;
	vector<pair<unsigned int, string> > choices;

	size_t curGroup=0;

	for(unsigned int ui=0;ui<NUM_COLOURMAPS; ui++)
		choices.push_back(make_pair(ui,getColourMapName(ui)));

	tmpStr=choiceString(choices,colourMap);
	
	p.name=TRANS("Colour Map"); 
	p.data=tmpStr;
	p.key=KEY_IONCOLOURFILTER_COLOURMAP;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Colour scheme used to assign points colours by value");
	propertyList.addProperty(p,curGroup);

	
	p.name=TRANS("Reverse map");
	p.helpText=TRANS("Reverse the colour scale");
	p.data= boolStrEnc(reverseMap);
	p.key=KEY_IONCOLOURFILTER_REVERSE;
	p.type=PROPERTY_TYPE_BOOL;
	propertyList.addProperty(p,curGroup);
	

	p.name=TRANS("Show Bar");
	p.key=KEY_IONCOLOURFILTER_SHOWBAR;
	p.data=boolStrEnc(showColourBar);
	p.type=PROPERTY_TYPE_BOOL;
	propertyList.addProperty(p,curGroup);
	
	p.name=TRANS("Opacity");
	p.key=KEY_IONCOLOURFILTER_ALPHA;
	stream_cast(p.data,alpha);
	p.type=PROPERTY_TYPE_REAL;
	propertyList.addProperty(p,curGroup);

	stream_cast(tmpStr,nColours);
	p.name=TRANS("Num Colours");
	p.data=tmpStr;
	p.helpText=TRANS("Number of unique colours to use in colour map"); 
	p.key=KEY_IONCOLOURFILTER_NCOLOURS;
	p.type=PROPERTY_TYPE_INTEGER;
	propertyList.addProperty(p,curGroup);

	stream_cast(tmpStr,mapBounds[0]);
	p.name=TRANS("Map start");
	p.helpText=TRANS("Assign points with this value to the first colour in map"); 
	p.data= tmpStr;
	p.key=KEY_IONCOLOURFILTER_MAPSTART;
	p.type=PROPERTY_TYPE_REAL;
	propertyList.addProperty(p,curGroup);

	stream_cast(tmpStr,mapBounds[1]);
	p.name=TRANS("Map end");
	p.helpText=TRANS("Assign points with this value to the last colour in map"); 
	p.data= tmpStr;
	p.key=KEY_IONCOLOURFILTER_MAPEND;
	p.type=PROPERTY_TYPE_REAL;
	propertyList.addProperty(p,curGroup);
	propertyList.setGroupTitle(curGroup,TRANS("Data"));
	

}

bool IonColourFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_IONCOLOURFILTER_COLOURMAP:
		{
			unsigned int tmpMap;
			tmpMap=(unsigned int)-1;
			for(unsigned int ui=0;ui<NUM_COLOURMAPS;ui++)
			{
				if(value== getColourMapName(ui))
				{
					tmpMap=ui;
					break;
				}
			}

			if(tmpMap >=NUM_COLOURMAPS || tmpMap ==colourMap)
				return false;

			clearCache();
			needUpdate=true;
			colourMap=tmpMap;
			break;
		}
		case KEY_IONCOLOURFILTER_REVERSE:
		{
			if(!applyPropertyNow(reverseMap,value,needUpdate))
				return false;
			break;
		}	
		case KEY_IONCOLOURFILTER_MAPSTART:
		{
			float tmpBound;
			if(stream_cast(tmpBound,value))
				return false;

			if(tmpBound >=mapBounds[1])
				return false;

			clearCache();
			needUpdate=true;
			mapBounds[0]=tmpBound;
			break;
		}
		case KEY_IONCOLOURFILTER_MAPEND:
		{
			float tmpBound;
			if(stream_cast(tmpBound,value))
				return false;

			if(tmpBound <=mapBounds[0])
				return false;

			clearCache();
			needUpdate=true;
			mapBounds[1]=tmpBound;
			break;
		}
		case KEY_IONCOLOURFILTER_NCOLOURS:
		{
			unsigned int numColours;
			if(stream_cast(numColours,value))
				return false;

			clearCache();
			needUpdate=true;
			//enforce 1->MAX_NUM_COLOURS range
			nColours=std::min(numColours,MAX_NUM_COLOURS);
			if(!nColours)
				nColours=1;
			break;
		}
		case KEY_IONCOLOURFILTER_SHOWBAR:
		{
			if(!applyPropertyNow(showColourBar,value,needUpdate))
				return false;
			break;
		}	
		case KEY_IONCOLOURFILTER_ALPHA:
		{
			if(!applyPropertyNow(alpha,value,needUpdate))
				return false;
			break;
		}
		default:
			ASSERT(false);
	}	
	return true;
}


std::string  IonColourFilter::getSpecificErrString(unsigned int code) const
{
	//Currently the only error is aborting
	return std::string(TRANS("Aborted"));
}

void IonColourFilter::setPropFromBinding(const SelectionBinding &b)
{
	ASSERT(false); 
}

bool IonColourFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

			f << tabs(depth+1) << "<colourmap value=\"" << colourMap << "\"/>" << endl;
			f << tabs(depth+1) << "<extrema min=\"" << mapBounds[0] << "\" max=\"" 
				<< mapBounds[1] << "\"/>" << endl;
			f << tabs(depth+1) << "<ncolours value=\"" << nColours << "\" opacity=\"" << alpha << "\"/>" << endl;

			f << tabs(depth+1) << "<showcolourbar value=\"" << boolStrEnc(showColourBar)<< "\"/>" << endl;
			f << tabs(depth+1) << "<reversemap value=\"" << boolStrEnc(reverseMap)<< "\"/>" << endl;
			
			f << tabs(depth) << "</" << trueName() << ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool IonColourFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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
	//Retrieve colourmap
	//====
	if(XMLHelpFwdToElem(nodePtr,"colourmap"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(colourMap,tmpStr))
		return false;

	if(colourMap>= NUM_COLOURMAPS)
	       return false;	
	xmlFree(xmlString);

	if(XMLHelpGetProp(alpha, nodePtr,"opacity"))
	{
		alpha=1.0f;
	}
	else
	{
		//clamp alpha to [0,1]
		alpha = std::max(0.0f,std::min(alpha,1.0f));
	}
	//====
	
	//Retrieve Extrema 
	//===
	float tmpMin,tmpMax;
	if(XMLHelpFwdToElem(nodePtr,"extrema"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"min");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(tmpMin,tmpStr))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"max");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(tmpMax,tmpStr))
		return false;

	xmlFree(xmlString);

	if(tmpMin > tmpMax)
		return false;

	mapBounds[0]=tmpMin;
	mapBounds[1]=tmpMax;

	//===
	
	//Retrieve num colours 
	//====
	if(XMLHelpFwdToElem(nodePtr,"ncolours"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(nColours,tmpStr))
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve num colours 
	//====
	if(XMLHelpFwdToElem(nodePtr,"showcolourbar"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(showColourBar,tmpStr))
		return false;

	xmlFree(xmlString);
	//====
	
	//Check for colour map reversal
	//=====
	if(XMLHelpFwdToElem(nodePtr,"reversemap"))
	{
		//Didn't exist prior to 0.0.15, assume off
		reverseMap=false;
	}
	else
	{
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;

		//convert from string to bool 
		if(!boolStrDec(tmpStr,reverseMap))
			return false;
	}

	xmlFree(xmlString);
	//====
	return true;
}

unsigned int IonColourFilter::getRefreshBlockMask() const
{
	//Anything but ions can go through this filter.
	return STREAM_TYPE_IONS;
}

unsigned int IonColourFilter::getRefreshEmitMask() const
{
	return  STREAM_TYPE_DRAW | STREAM_TYPE_IONS;
}

unsigned int IonColourFilter::getRefreshUseMask() const
{
	return  STREAM_TYPE_IONS;
}
#ifdef DEBUG

IonStreamData *sythIonCountData(unsigned int numPts, float mStart, float mEnd)
{
	IonStreamData *d = new IonStreamData;
	d->data.resize(numPts);
	for(unsigned int ui=0; ui<numPts;ui++)
	{
		IonHit h;

		h.setPos(Point3D(ui,ui,ui));
		h.setMassToCharge( (mEnd-mStart)*(float)ui/(float)numPts + mStart);
		d->data[ui] =h;
	}

	return d;
}


bool ionCountTest()
{
	const int NUM_PTS=1000;
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d=sythIonCountData(NUM_PTS,0,100);
	streamIn.push_back(d);


	IonColourFilter *f = new IonColourFilter;
	f->setCaching(false);

	bool needUpdate;
	TEST(f->setProperty(KEY_IONCOLOURFILTER_NCOLOURS,"100",needUpdate),"Set prop");
	TEST(f->setProperty(KEY_IONCOLOURFILTER_MAPSTART,"0",needUpdate),"Set prop");
	TEST(f->setProperty(KEY_IONCOLOURFILTER_MAPEND,"100",needUpdate),"Set prop");
	TEST(f->setProperty(KEY_IONCOLOURFILTER_SHOWBAR,"0",needUpdate),"Set prop");
	
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"refresh error code");
	delete f;
	delete d;
	
	TEST(streamOut.size() == 99,"stream count");

	for(unsigned int ui=0;ui<streamOut.size();ui++)
	{
		TEST(streamOut[ui]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	}

	for(unsigned int ui=0;ui<streamOut.size();ui++)
		delete streamOut[ui];

	return true;
}


bool IonColourFilter::runUnitTests()
{
	if(!ionCountTest())
		return false;

	return true;
}


#endif

