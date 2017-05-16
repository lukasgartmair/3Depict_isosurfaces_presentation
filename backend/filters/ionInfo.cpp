/*
 *	ionInfo.cpp -Filter to compute various properties of valued point cloud
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
#include "ionInfo.h"

#include "filterCommon.h"
#include "algorithms/mass.h"

using std::vector;
using std::string;
using std::pair;
using std::make_pair;


enum
{
	VOLUME_MODE_RECTILINEAR=0,
	VOLUME_MODE_CONVEX,
	VOLUME_MODE_END
};

const char *volumeModeString[] = {
       	NTRANS("Rectilinear"),
	NTRANS("Convex hull")
	};
				
enum
{
	ERR_USER_ABORT=1,
	ERR_BAD_QHULL,
	IONINFO_ERR_ENUM_END
};


bool getRectilinearBounds(const std::vector<const FilterStreamData *> &dataIn, BoundCube &bound,
					unsigned int *progress, unsigned int totalSize)
{
	bound.setInvalid();

	vector<Point3D> overflow;

	size_t n=0;
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
		{

			const IonStreamData *ions;
			ions = ( const IonStreamData *)dataIn[ui];
			n+=ions->data.size();
			BoundCube c;
			if(ions->data.size() >1)
			{
				ions = (const IonStreamData*)dataIn[ui];
				IonHit::getBoundCube(ions->data,c);

				if(c.isValid())
				{
					if(bound.isValid())
						bound.expand(c);
					else
						bound=c;
				}
			}
			else
			{
				//Do we have single ions in their own
				//data structure? if so, they don't have a bound
				//on their own, but may have one collectively.
				if(ions->data.size())
					overflow.push_back(ions->data[0].getPos());
			}
		
			*progress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
			if(*Filter::wantAbort)
				return false;
		}
	}


	//Handle any single ions
	if(overflow.size() > 1)
	{
		BoundCube c;
		c.setBounds(overflow);
		if(bound.isValid())
			bound.expand(c);
		else
			bound=c;
	}
	else if(bound.isValid() && overflow.size() == 1)
		bound.expand(overflow[0]);

	return true;
}

IonInfoFilter::IonInfoFilter() : wantIonCounts(true), wantNormalise(false),
	range(0), wantVolume(false), volumeAlgorithm(VOLUME_MODE_RECTILINEAR),
	cubeSideLen(1.0f), fitMode(FIT_MODE_NONE), massBackStart(1.2),massBackEnd(1.8),binWidth(0.05)
{
	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up
}

void IonInfoFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	const RangeStreamData *c=0;
	//Determine if we have an incoming range
	for (size_t i = 0; i < dataIn.size(); i++) 
	{
		if(dataIn[i]->getStreamType() == STREAM_TYPE_RANGE)
		{
			c=(const RangeStreamData *)dataIn[i];

			break;
		}
	}

	//we no longer (or never did) have any incoming ranges. Not much to do
	if(!c)
	{
		//delete the old incoming range pointer
		if(range)
			delete range;
		range=0;
	}
	else
	{
		//If we didn't have an incoming rsd, then make one up!
		if(!range)
		{
			range= new RangeStreamData;
			*range=*c;
		}
		else
		{

			//OK, so we have a range incoming already (from last time)
			//-- the question is, is it the same one we had before ?
			//Do a pointer comparison (its a hack, yes, but it should work)
			if(range->rangeFile != c->rangeFile)
			{
				//hmm, it is different. well, trash the old incoming rng
				delete range;

				range = new RangeStreamData;
				*range=*c;
			}

		}

	}

}

Filter *IonInfoFilter::cloneUncached() const
{
	IonInfoFilter *p=new IonInfoFilter();

	p->wantIonCounts=wantIonCounts;
	p->wantVolume=wantVolume;
	p->wantNormalise=wantNormalise;
	p->cubeSideLen=cubeSideLen;
	p->volumeAlgorithm=volumeAlgorithm;

	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

unsigned int IonInfoFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{

	//Count the number of ions input
	size_t numTotalPoints = numElements(dataIn,STREAM_TYPE_IONS);
	size_t numRanged=0;


	if(!numTotalPoints)
	{
		consoleOutput.push_back((TRANS("No ions")));
		return 0;
	}

	//Compute ion counts/composition as needed
	if(wantIonCounts)
	{
		
		std::string str;
		//Count the number of ions
		if(range)
		{
			float intensity=0;
			if(fitMode!= FIT_MODE_NONE)
			{
				BACKGROUND_PARAMS backParams;
				backParams.massStart=massBackStart;
				backParams.massEnd=massBackEnd;
				backParams.binWidth=binWidth;
				backParams.mode=fitMode;

			
				//fit a constant tof (1/sqrt (mass)) type background
				if(doFitBackground(dataIn,backParams))
				{
					//display a warning that the background failed
					consoleOutput.push_back(TRANS("Background fit failed - input data was considered ill formed (gauss-test)"));
					consoleOutput.push_back(TRANS("Following data has not been corrected"));
				}
				else 
				{
					intensity=backParams.intensity;
					
				}
			}
			vector<size_t> numIons;

			ASSERT(range);

			const RangeFile *r=range->rangeFile;
			numIons.resize(r->getNumIons()+1,0);

			//count ions per-species. Add a bin on the end for unranged
			for(size_t ui=0;ui<dataIn.size();ui++)
			{
				if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
					continue;

				const IonStreamData  *i; 
				i = (const IonStreamData *)dataIn[ui];

				for(size_t uj=0;uj<i->data.size(); uj++)
				{
					unsigned int idIon;
					idIon = r->getIonID(i->data[uj].getMassToCharge());
					if(idIon != (unsigned int) -1)
						numIons[idIon]++;
					else
						numIons[numIons.size()-1]++;
				}
			}

			if(intensity > 0)
			{
				for(unsigned int ui=0;ui<r->getNumRanges(); ui++)
				{
					pair<float,float> masses;
					float integral;
					
					//compute the integral of the fitted background, then subtract this from the
					// ion count	
					masses=r->getRange(ui);
					integral = 2.0f*intensity*(sqrtf(masses.second) - sqrtf(masses.first));

					numIons[r->getIonID(ui)]-=integral;	
				}
			}
			
			stream_cast(str,numTotalPoints);
			str=std::string(TRANS("--Counts--") );
			consoleOutput.push_back(str);
			
			//sum all ions *except* the unranged.
			for(size_t ui=0;ui<numIons.size()-1;ui++)
				numRanged+=numIons[ui];
				
			if(wantNormalise)
			{
				stream_cast(str,numRanged);
				str=TRANS("Total Ranged\t")+str;
			}
			else
			{
				stream_cast(str,numTotalPoints);
				str=TRANS("Total (incl. unranged)\t")+str;
			}
			consoleOutput.push_back(str);
			consoleOutput.push_back("");

			//Print out the ion count table
			for(size_t ui=0;ui<numIons.size();ui++)
			{
				if(wantNormalise)
				{
					if(numRanged)
						stream_cast(str,((float)numIons[ui])/(float)numRanged);
					else
						str=TRANS("n/a");
				}
				else
					stream_cast(str,numIons[ui]);
			
				if(ui!=numIons.size()-1)
					str=std::string(r->getName(ui)) + std::string("\t") + str;
				else
				{
					//output unranged count 
					str=std::string(TRANS("Unranged")) + std::string("\t") + str;
				}

				consoleOutput.push_back(str);
			}
			str=std::string("----------");
			consoleOutput.push_back(str);
	
		}
		else
		{
			//ok, no ranges -- just give us the total
			stream_cast(str,numTotalPoints);
			str=std::string(TRANS("Number of points : ") )+ str;
			consoleOutput.push_back(str);
		}
	}

	float computedVol=0;
	//Compute volume as needed
	if(wantVolume)
	{
		switch(volumeAlgorithm)
		{
			case VOLUME_MODE_RECTILINEAR:
			{
				BoundCube bound;
				if(!getRectilinearBounds(dataIn,bound,
					&(progress.filterProgress),numTotalPoints))
					return ERR_USER_ABORT;

				if(bound.isValid())
				{
					Point3D low,hi;
					string tmpLow,tmpHi,s;
					bound.getBounds(low,hi);
					computedVol=bound.volume();
					
					
					stream_cast(tmpLow,low);
					stream_cast(tmpHi,hi);
					
					s=TRANS("Rectilinear Bounds : ");
					s+= tmpLow + " / "  + tmpHi;
					consoleOutput.push_back(s);
					
					stream_cast(s,computedVol);
					consoleOutput.push_back(string(TRANS("Volume (len^3): ")) + s);
				}

				break;
			}
			case VOLUME_MODE_CONVEX:
			{
				//OK, so here we need to do a convex hull estimation of the volume.
				unsigned int err;
				err=convexHullEstimateVol(dataIn,computedVol);
				if(err)
					return err;

				std::string s;
				stream_cast(s,computedVol);
				if(computedVol>0)
				{
					consoleOutput.push_back(string(TRANS("Convex Volume (len^3): ")) + s);
				}
				else
					consoleOutput.push_back(string(TRANS("Unable to compute volume")));


				break;
			}
			default:
				ASSERT(false);

		}
	
#ifdef DEBUG
		lastVolume=computedVol;
#endif
	}


	//"Pairwise events" - where we perform an action if both 
	//These
	if(wantIonCounts && wantVolume)
	{
		if(computedVol > sqrtf(std::numeric_limits<float>::epsilon()))
		{
			float density;
			std::string s;
		
			if(range)
			{
				density=(float)numRanged/computedVol;
				stream_cast(s,density);
				consoleOutput.push_back(string(TRANS("Ranged Density (pts/vol):")) + s );
			}
			
			density=(float)numTotalPoints/computedVol;
			stream_cast(s,density);
			consoleOutput.push_back(string(TRANS("Total Density (pts/vol):")) + s );
	
		}
	}

	return 0;
}

size_t IonInfoFilter::numBytesForCache(size_t nObjects) const
{
	return 0;
}


void IonInfoFilter::getProperties(FilterPropGroup &propertyList) const
{
	string str;
	FilterProperty p;
	size_t curGroup=0;

	vector<pair<unsigned int,string> > choices;
	string tmpStr;

	stream_cast(str,wantIonCounts);
	p.key=IONINFO_KEY_TOTALS;
	if(range)
	{
		p.name=TRANS("Compositions");
		p.helpText=TRANS("Display compositional data for points in console");
	}
	else
	{
		p.name=TRANS("Counts");
		p.helpText=TRANS("Display count data for points in console");
	}
	p.data= str;
	p.type=PROPERTY_TYPE_BOOL;
	propertyList.addProperty(p,curGroup);


	propertyList.setGroupTitle(curGroup,TRANS("Ion data"));
	if(wantIonCounts && range)
	{
		stream_cast(str,wantNormalise);
		p.name=TRANS("Normalise");
		p.data=str;
		p.key=IONINFO_KEY_NORMALISE;
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Normalise count data");

		propertyList.addProperty(p,curGroup);

		/*
		p.name=TRANS("Back. Correct");
		choices.clear();
		for(unsigned int ui=0;ui<FIT_MODE_ENUM_END; ui++)
		{
			choices.push_back(make_pair((unsigned int)ui,
						TRANS(BACKGROUND_MODE_STRING[ui])));
		}
		str=choiceString(choices,fitMode);
		p.data=str;
		p.type=PROPERTY_TYPE_CHOICE;
		p.helpText=TRANS("Background correction mode");
		p.key=IONINFO_KEY_BACKMODE;
		propertyList.addProperty(p,curGroup);


		switch(fitMode)
		{
			case FIT_MODE_NONE:
				break;
			case FIT_MODE_CONST_TOF:
				//we need mass start/end for our window
				// and a binwidth to use for TOF binning
				stream_cast(str,massBackStart);
				p.data=str;
				p.name=TRANS("Mass start");
				p.type=PROPERTY_TYPE_REAL;
				p.helpText=TRANS("Background correction fit starting mass");
				p.key=IONINFO_KEY_BACK_MASSSTART;
				propertyList.addProperty(p,curGroup);
				
				stream_cast(str,massBackEnd);
				p.data=str;
				p.name=TRANS("Mass end");
				p.type=PROPERTY_TYPE_REAL;
				p.helpText=TRANS("Background correction fit ending mass");
				p.key=IONINFO_KEY_BACK_MASSEND;
				propertyList.addProperty(p,curGroup);


				stream_cast(str,binWidth);
				p.data=str;
				p.name=TRANS("Mass binning");
				p.type=PROPERTY_TYPE_REAL;
				p.helpText=TRANS("Bin size to use to build spectrum for performing fit");
				p.key=IONINFO_KEY_BACK_BINSIZE;
				propertyList.addProperty(p,curGroup);
				break;
			case FIT_MODE_ENUM_END:
				ASSERT(false);
	
		}
		*/
		
	}

	curGroup++;

	stream_cast(str,wantVolume);
	p.key=IONINFO_KEY_VOLUME;
	p.name=TRANS("Volume");
	p.data= str;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Compute volume for point data");
	propertyList.addProperty(p,curGroup);

	if(wantVolume)
	{	
		choices.clear();
		for(unsigned int ui=0;ui<VOLUME_MODE_END; ui++)
		{
			choices.push_back(make_pair((unsigned int)ui,
						TRANS(volumeModeString[ui])));
		}
		
		tmpStr= choiceString(choices,volumeAlgorithm);
		p.name=TRANS("Algorithm");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_CHOICE;
		p.helpText=TRANS("Select volume counting technique");
		p.key=IONINFO_KEY_VOLUME_ALGORITHM;
		propertyList.addProperty(p,curGroup);


		switch(volumeAlgorithm)
		{
			case VOLUME_MODE_RECTILINEAR:
			case VOLUME_MODE_CONVEX:
				break;
		}
	
	}
	propertyList.setGroupTitle(curGroup,TRANS("Volume data"));
}


bool IonInfoFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{
	switch(key)
	{
		case IONINFO_KEY_TOTALS:
		{
			if(!applyPropertyNow(wantIonCounts,value,needUpdate))
				return false;
			break;
		}
		case IONINFO_KEY_NORMALISE:
		{
			if(!applyPropertyNow(wantNormalise,value,needUpdate))
				return false;
			break;
		}
		case IONINFO_KEY_VOLUME:
		{
			if(!applyPropertyNow(wantVolume,value,needUpdate))
				return false;
			break;
		}
		case IONINFO_KEY_BACKMODE:
		{
			unsigned int newMode;
			for(size_t ui=0;ui<FIT_MODE_ENUM_END; ui++)
			{
				if(string(BACKGROUND_MODE_STRING[ui]) == string(value))
				{
					newMode=ui;
					break;
				}
			}

			ASSERT(newMode <FIT_MODE_ENUM_END)
			
			fitMode=newMode;
			cacheOK=false;
			needUpdate=true;
			
			break;
		}
		case IONINFO_KEY_BACK_MASSSTART:
		{
			float tmpMass;
			if(stream_cast(tmpMass,value))
				return false;
			if(tmpMass >=massBackEnd)
				return false;
	
			if(!applyPropertyNow(massBackStart,value,needUpdate))
				return false;
			break;
		}
		case IONINFO_KEY_BACK_MASSEND:
		{
			float tmpMass;
			if(stream_cast(tmpMass,value))
				return false;
			if(tmpMass <=massBackStart)
				return false;
	
			if(!applyPropertyNow(massBackEnd,value,needUpdate))
				return false;
			break;
		}

			
		case IONINFO_KEY_VOLUME_ALGORITHM:
		{
			unsigned int newAlg=VOLUME_MODE_END;

			for(unsigned int ui=0;ui<VOLUME_MODE_END; ui++)
			{
				if(volumeModeString[ui] == value)
				{
					newAlg=ui;
					break;
				}
			}

			if(newAlg==volumeAlgorithm || newAlg == VOLUME_MODE_END)
				return false;
			
			volumeAlgorithm=newAlg;	
			needUpdate=true;
			break;	
		}
		case IONINFO_KEY_BACK_BINSIZE:
		{
			float tmpWidth;
			if(stream_cast(tmpWidth,value))
				return false;
			if(tmpWidth <=0.0f)
				return false;
	
			if(tmpWidth > massBackEnd - massBackStart)
				tmpWidth=massBackEnd-massBackStart;
		
			binWidth=tmpWidth;
			
			break;
		}
		default:
			ASSERT(false);
	}

	return true;
}

std::string  IonInfoFilter::getSpecificErrString(unsigned int code) const
{
	const char *errStrs[] = { "",
		"Aborted",
		"Bug? Problem with qhull library, cannot run convex hull.",
	};
	
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(errStrs) == IONINFO_ERR_ENUM_END);
	ASSERT(code < IONINFO_ERR_ENUM_END);

	return errStrs[code];
}

void IonInfoFilter::setPropFromBinding(const SelectionBinding &b)
{
	ASSERT(false);
}

unsigned int IonInfoFilter::convexHullEstimateVol(const vector<const FilterStreamData*> &data, 
								float &volume)
{
	volume=0;

	//TODO: replace with real progress
	unsigned int dummyProgress;

	//Compute the convex hull, leaving the qhull data structure intact
	const bool NO_FREE_QHULL=false;
	const bool WANT_QHULL_VOL=true;
	vector<Point3D> hullPts;
	if(computeConvexHull(data,&dummyProgress,
			hullPts,WANT_QHULL_VOL,NO_FREE_QHULL))
		return ERR_BAD_QHULL;

	Point3D midPt(0,0,0);

	for(size_t ui=0;ui<hullPts.size();ui++)
		midPt+=hullPts[ui];
	
	midPt*=1.0f/(float)hullPts.size();

	//We don't need this result anymore
	hullPts.clear();

	volume=qh totvol;


	//Free the convex hull mem
	freeConvexHull();

	return 0;
}

bool IonInfoFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) <<  "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

			f << tabs(depth+1) << "<wantioncounts value=\""<<wantIonCounts<< "\"/>"  << endl;
			f << tabs(depth+1) << "<wantnormalise value=\""<<wantNormalise<< "\"/>"  << endl;
			f << tabs(depth+1) << "<wantvolume value=\""<<wantVolume<< "\"/>"  << endl;
			f << tabs(depth+1) << "<volumealgorithm value=\""<<volumeAlgorithm<< "\"/>"  << endl;
			f << tabs(depth+1) << "<cubesidelen value=\""<<cubeSideLen<< "\"/>"  << endl;
			f << tabs(depth+1) << "<background mode=\"" << fitMode << "\">" << endl;
				f << tabs(depth+2) << "<fitwindow start=\"" << massBackStart<< "\" end=\"" << massBackEnd << "\"/>" << endl;
			f << tabs(depth+1) << "</background>" << endl;

			f << tabs(depth) << "</" <<trueName()<< ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool IonInfoFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;

	xmlChar *xmlString;
	//Retrieve user string
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);

	//--
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"wantioncounts","value"))
		return false;
	if(!boolStrDec(tmpStr,wantIonCounts))
		return false;
	//--=
	
	//--
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"wantnormalise","value"))
		return false;
	if(!boolStrDec(tmpStr,wantNormalise))
		return false;
	//--=


	//--
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"wantvolume","value"))
		return false;
	if(!boolStrDec(tmpStr,wantVolume))
		return false;
	//--=

	//--
	unsigned int tmpInt;
	if(!XMLGetNextElemAttrib(nodePtr,tmpInt,"volumealgorithm","value"))
		return false;

	if(tmpInt >=VOLUME_MODE_END)
		return false;
	volumeAlgorithm=tmpInt;
	//--=

	//--
	float tmpFloat;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"cubesidelen","value"))
		return false;

	if(tmpFloat <= 0.0f)
		return false;
	cubeSideLen=tmpFloat;
	//--=

	//Retrieve background fitting mode, if we have it
	// Only available 3Depict >= 0.0.18
	// internal rev > 3e41b89299f4
	if(!XMLHelpFwdToElem(nodePtr,"background"))
	{
		if(XMLHelpGetProp(fitMode,nodePtr,"mode"))
			return false;

		if(!nodePtr->xmlChildrenNode)
			return false;
			
		xmlNodePtr tmpNode=nodePtr;
		nodePtr=nodePtr->xmlChildrenNode;
		
		if(!XMLGetNextElemAttrib(nodePtr,massBackStart,"fitwindow","start"))
			return false;

		if(XMLHelpGetProp(massBackEnd,nodePtr,"end"))
			return false;
		
		nodePtr=tmpNode;
	}

	return true;
}

unsigned int IonInfoFilter::getRefreshBlockMask() const
{
	return STREAMTYPE_MASK_ALL;
}

unsigned int IonInfoFilter::getRefreshEmitMask() const
{
	return  0;
}

unsigned int IonInfoFilter::getRefreshUseMask() const
{
	return  STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
}

bool IonInfoFilter::needsUnrangedData() const
{ 
	return fitMode == FIT_MODE_CONST_TOF;
}
#ifdef DEBUG

void makeBox(float boxSize,IonStreamData *d)
{
	d->data.clear();
	for(unsigned int ui=0;ui<8;ui++)
	{
		IonHit h;
		float x,y,z;

		x= (float)(ui &1)*boxSize;
		y= (float)((ui &2) >> 1)*boxSize;
		z= (float)((ui &4) >> 2)*boxSize;

		h.setPos(Point3D(x,y,z));
		h.setMassToCharge(1);
		d->data.push_back(h);
	}
}
void makeSphereOutline(float radius, float angularStep,
			IonStreamData *d)
{
	d->clear();
	ASSERT(angularStep > 0.0f);
	unsigned int numAngles=(unsigned int)( 180.0f/angularStep);

	for( unsigned int  ui=0; ui<numAngles; ui++)
	{
		float longit;
		//-0.5->0.5
		longit = (float)((int)ui-(int)(numAngles/2))/(float)(numAngles);
		//longitude test
		longit*=180.0f;

		for( unsigned int  uj=0; uj<numAngles; uj++)
		{
			float latit;
			//0->1
			latit = (float)((int)uj)/(float)(numAngles);
			latit*=180.0f;

			float x,y,z;
			x=radius*cos(longit)*sin(latit);
			y=radius*sin(longit)*sin(latit);
			z=radius*cos(latit);

			IonHit h;
			h.setPos(Point3D(x,y,z));
			h.setMassToCharge(1);
			d->data.push_back(h);
		}
	}
}

bool volumeBoxTest()
{
	//Construct a few boxes, then test each of their volumes
	IonStreamData *d=new IonStreamData();

	const float SOMEBOX=7.0f;
	makeBox(7.0,d);


	//Construct the filter, and then set up the options we need
	IonInfoFilter *f  = new IonInfoFilter;	
	f->setCaching(false);

	//activate volume measurement
	bool needUp;
	TEST(f->setProperty(IONINFO_KEY_VOLUME,"1",needUp),"Set prop");
	string s;
	stream_cast(s,(int)VOLUME_MODE_RECTILINEAR);

	//Can return false if algorithm already selected. Do not
	// test return
	f->setProperty(IONINFO_KEY_VOLUME_ALGORITHM, s,needUp);
	
	
	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(d);
	
	ProgressData p;
	f->refresh(streamIn,streamOut,p);

	//No ions come out of the info
	TEST(streamOut.empty(),"stream size test");

	vector<string> consoleStrings;
	f->getConsoleStrings(consoleStrings); 
	
	//weak test for the console string size
	TEST(consoleStrings.size(), "console strings existence test");


	//Ensure that the rectilinear volume is the same as
	// the theoretical value
	float volMeasure,volReal;;
	volMeasure=f->getLastVolume();
	volReal =SOMEBOX*SOMEBOX*SOMEBOX; 

	TEST(fabs(volMeasure -volReal) < 
		10.0f*sqrtf(std::numeric_limits<float>::epsilon()),
					"volume estimation test (rect)");

	
	//Try again, but with convex hull
	stream_cast(s,(int)VOLUME_MODE_CONVEX);
	f->setProperty(IONINFO_KEY_VOLUME_ALGORITHM, s,needUp);
	
	TEST(!f->refresh(streamIn,streamOut,p), "refresh");
	volMeasure=f->getLastVolume();

	TEST(fabs(volMeasure -volReal) < 
		10.0f*sqrtf(std::numeric_limits<float>::epsilon()),
				"volume estimation test (convex)");
	



	delete d;
	delete f;
	return true;
}

bool volumeSphereTest()
{
	//Construct a few boxes, then test each of their volumes
	IonStreamData *d=new IonStreamData();

	const float OUTLINE_RADIUS=7.0f;
	const float ANGULAR_STEP=2.0f;
	makeSphereOutline(OUTLINE_RADIUS,ANGULAR_STEP,d);

	//Construct the filter, and then set up the options we need
	IonInfoFilter *f  = new IonInfoFilter;	
	f->setCaching(false);

	//activate volume measurement
	bool needUp;
	TEST(f->setProperty(IONINFO_KEY_VOLUME,"1",needUp),"Set prop");

	//Can return false if the default algorithm is the same
	//  as the selected algorithm
	f->setProperty(IONINFO_KEY_VOLUME_ALGORITHM, 
		volumeModeString[VOLUME_MODE_RECTILINEAR],needUp);
	
	
	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(d);
	
	ProgressData p;
	f->refresh(streamIn,streamOut,p);

	//No ions come out of the info
	TEST(streamOut.empty(),"stream size test");

	vector<string> consoleStrings;
	f->getConsoleStrings(consoleStrings); 

	//weak test for the console string size
	TEST(consoleStrings.size(), "console strings existence test");


	float volMeasure,volReal;
	volMeasure=f->getLastVolume();
	//Bounding box for sphere is diameter^3.
	volReal =8.0f*OUTLINE_RADIUS*OUTLINE_RADIUS*OUTLINE_RADIUS;
	TEST(fabs(volMeasure -volReal) < 0.05*volReal,"volume test (rect est of sphere)");

	
	//Try again, but with convex hull
	TEST(f->setProperty(IONINFO_KEY_VOLUME_ALGORITHM,
		volumeModeString[VOLUME_MODE_CONVEX],needUp),"Set prop");
	
	vector<string> dummy;
	f->getConsoleStrings(dummy);

	TEST(!f->refresh(streamIn,streamOut,p),"refresh error code");

	volMeasure=f->getLastVolume();

	//Convex volume of sphere
	volReal =4.0f/3.0f*M_PI*OUTLINE_RADIUS*OUTLINE_RADIUS*OUTLINE_RADIUS;
	TEST(fabs(volMeasure -volReal) < 0.05*volReal, "volume test, convex est. of sphere");
	
	TEST(consoleStrings.size(), "console strings existence test");

	delete d;
	delete f;
	return true;
}

bool IonInfoFilter::runUnitTests()
{
	if(!volumeBoxTest())
		return false;
	
	if(!volumeSphereTest())
		return false;
	
	return true;
}
#endif

