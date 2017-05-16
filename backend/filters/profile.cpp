/*
 *	profile.cpp - Compute composition or density profiles from valued point clouds
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
#include "profile.h"
#include "../plot.h"

#include "filterCommon.h"
#include "geometryHelpers.h"

using std::vector;
using std::string;
using std::pair;
using std::make_pair;
using std::map;


//!Possible primitive types for composition profiles
enum
{
	PRIMITIVE_CYLINDER_AXIAL,
	PRIMITIVE_CYLINDER_RADIAL,
	PRIMITIVE_SPHERE,
	PRIMITIVE_END, //Not actually a primitive, just end of enum
};


//!Error codes
enum
{
	ERR_NUMBINS=1,
	ERR_MEMALLOC,
	ERR_ABORT,
	ERR_COMP_ENUM_END
};

const char *PRIMITIVE_NAME[]={
	NTRANS("Cylinder (axial)"),
	NTRANS("Cylinder (radial)"),
	NTRANS("Sphere")
};

const float DEFAULT_RADIUS = 10.0f;

const unsigned int MINEVENTS_DEFAULT =10;


ProfileFilter::ProfileFilter() : primitiveType(PRIMITIVE_CYLINDER_AXIAL),
	showPrimitive(true), lockAxisMag(false),normalise(true), fixedBins(0),
	nBins(1000), binWidth(0.5f), minEvents(MINEVENTS_DEFAULT), rgba(0,0,1), plotStyle(0)
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(PRIMITIVE_NAME) == PRIMITIVE_END);

	wantDensity=false;	
	errMode.mode=PLOT_ERROR_NONE;
	errMode.movingAverageNum=4;
	
	vectorParams.push_back(Point3D(0.0,0.0,0.0));
	vectorParams.push_back(Point3D(0,20.0,0.0));
	scalarParams.push_back(DEFAULT_RADIUS);

	haveRangeParent=false;
}

//Puts an ion in its appropriate range position, given ionID mapping,
//range data (if any), mass to charge and the output table
void ProfileFilter::binIon(unsigned int targetBin, const RangeStreamData* rng, 
	const map<unsigned int,unsigned int> &ionIDMapping,
	vector<vector<size_t> > &frequencyTable, float massToCharge) 
{
	//if we have no range data, then simply increment its position in a 1D table
	//which will later be used as "count" data (like some kind of density plot)
	if(!rng)
	{
		ASSERT(frequencyTable.size() == 1);
		//There is a really annoying numerical boundary case
		//that makes the target bin equate to the table size. 
		//disallow this.
		if(targetBin < frequencyTable[0].size())
		{
			vector<size_t>::iterator it;
			it=frequencyTable[0].begin()+targetBin;
			#pragma omp critical
			(*it)++;
		}
		return;
	}


	//We have range data, we need to use it to classify the ion and then increment
	//the appropriate position in the table
	unsigned int rangeID = rng->rangeFile->getRangeID(massToCharge);

	if(rangeID != (unsigned int)(-1) && rng->enabledRanges[rangeID])
	{
		unsigned int ionID=rng->rangeFile->getIonID(rangeID); 
		unsigned int pos;
		pos = ionIDMapping.find(ionID)->second;
		vector<size_t>::iterator it;
		it=frequencyTable[pos].begin()+targetBin;
		#pragma omp critical
		(*it)++;
	}
}


Filter *ProfileFilter::cloneUncached() const
{
	ProfileFilter *p = new ProfileFilter();

	p->primitiveType=primitiveType;
	p->showPrimitive=showPrimitive;
	p->vectorParams.resize(vectorParams.size());
	p->scalarParams.resize(scalarParams.size());

	std::copy(vectorParams.begin(),vectorParams.end(),p->vectorParams.begin());
	std::copy(scalarParams.begin(),scalarParams.end(),p->scalarParams.begin());

	p->wantDensity=wantDensity;
	p->normalise=normalise;	
	p->fixedBins=fixedBins;
	p->lockAxisMag=lockAxisMag;
	
	p->rgba=rgba;
	p->binWidth=binWidth;
	p->nBins = nBins;
	p->plotStyle=plotStyle;
	p->errMode=errMode;
	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

void ProfileFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	//Check for range file parent
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
		{
			haveRangeParent=true;
			return;
		}
	}
	haveRangeParent=false;
}

unsigned int ProfileFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
			std::vector<const FilterStreamData *> &getOut, ProgressData &progress) 
{
	//Clear selection devices
	// FIXME: Leaking drawables.
	clearDevices();
	
	if(showPrimitive)
	{
		//TODO: This is a near-copy of ionClip.cpp - refactor
		//construct a new primitive, do not cache
		DrawStreamData *drawData=new DrawStreamData;
		drawData->parent=this;
		switch(primitiveType)
		{
			case PRIMITIVE_CYLINDER_AXIAL:
			case PRIMITIVE_CYLINDER_RADIAL:
			{
				//Origin + normal
				ASSERT(vectorParams.size() == 2);
				//Add drawable components
				DrawCylinder *dC = new DrawCylinder;
				dC->setOrigin(vectorParams[0]);
				dC->setRadius(scalarParams[0]);
				dC->setColour(0.5,0.5,0.5,0.3);
				dC->setSlices(40);
				dC->setLength(sqrtf(vectorParams[1].sqrMag())*2.0f);
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
			default:
				ASSERT(false);
		}
		drawData->cached=0;	
		getOut.push_back(drawData);
	}


	//Propagate all the incoming data (excluding ions)
	propagateStreams(dataIn,getOut,STREAM_TYPE_IONS,true);
	
	//use the cached copy of the data if we have it.
	if(cacheOK)
	{
		//propagate our cached plot data.
		propagateCache(getOut);

		ASSERT(filterOutputs.back()->getStreamType() == STREAM_TYPE_PLOT);

		progress.filterProgress=100;
		return 0;
	}
			

	//Ion Frequencies (composition specific if rangefile present)
	vector<vector<size_t> > ionFrequencies;
	
	RangeStreamData *rngData=0;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
		{
			rngData =((RangeStreamData *)dataIn[ui]);
			break;
		}
	}

	unsigned int numBins, errCode;
	{
	float length;
	errCode=getBinData(numBins,length);

	if(!numBins)
		return 0;
	}

	if(errCode)
		return errCode;

	//Indirection vector to convert ionFrequencies position to ionID mapping.
	//Should only be used in conjunction with rngData == true
	std::map<unsigned int,unsigned int> ionIDMapping,inverseIDMapping;
	//Allocate space for the frequency table
	if(rngData)
	{
		ASSERT(rngData->rangeFile);
		unsigned int enabledCount=0;
		for(unsigned int ui=0;ui<rngData->rangeFile->getNumIons();ui++)
		{
			//TODO: Might be nice to detect if an ions ranges
			//are all, disabled then if they are, enter this "if"
			//anyway
			if(rngData->enabledIons[ui])
			{
				//Keep the forwards mapping for binning
				ionIDMapping.insert(make_pair(ui,enabledCount));
				//Keep the inverse mapping for labelling
				inverseIDMapping.insert(make_pair(enabledCount,ui));
				enabledCount++;
			}

		

		}

		//Nothing to do.
		if(!enabledCount)
			return 0;

		try
		{
			ionFrequencies.resize(enabledCount);
			//Allocate and Initialise all elements to zero
			#pragma omp parallel for
			for(unsigned int ui=0;ui<ionFrequencies.size(); ui++)
				ionFrequencies[ui].resize(numBins,0);
		}
		catch(std::bad_alloc)
		{
			return ERR_MEMALLOC;
		}

	}
	else
	{
		try
		{
			ionFrequencies.resize(1);
			ionFrequencies[0].resize(numBins,0);
		}
		catch(std::bad_alloc)
		{
			return ERR_MEMALLOC;
		}
	}


	size_t n=0;
	size_t totalSize=numElements(dataIn);

	map<size_t,size_t> primitiveMap;
	primitiveMap[PRIMITIVE_CYLINDER_AXIAL] = CROP_CYLINDER_INSIDE_AXIAL;
	primitiveMap[PRIMITIVE_CYLINDER_RADIAL] = CROP_CYLINDER_INSIDE_RADIAL;
	primitiveMap[PRIMITIVE_SPHERE] = CROP_SPHERE_INSIDE;

	CropHelper dataMapping(totalSize,primitiveMap[primitiveType], 
					vectorParams,scalarParams  );
	dataMapping.setMapMaxima(numBins);

	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		//Loop through each element data set
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				const IonStreamData *dIon = (const IonStreamData*)dataIn[ui];
#ifdef _OPENMP
				//OpenMP abort is not v. good, simply spin instead of working
				bool spin=false;
#endif
				//Process ion streams
			
				size_t nIons=dIon->data.size();	
				#pragma omp parallel for shared(n)
				for(size_t uj=0;uj<nIons;uj++)
				{
#ifdef _OPENMP
					//if parallelised, abort computaiton
					if(spin) continue;
#endif
					unsigned int targetBin;
					targetBin=dataMapping.mapIon1D(dIon->data[uj]);

					//Keep ion if inside cylinder 
					if(targetBin!=(unsigned int)-1)
					{
						//Push data into the correct bin.
						// based upon eg ranging information and target 1D bin
						binIon(targetBin,rngData,ionIDMapping,ionFrequencies,
								dIon->data[uj].getMassToCharge());
					}

#ifdef _OPENMP
					#pragma omp atomic
					n++; //FIXME: Performance - we could use a separate non-sahred counter to reduce locking?

					if(omp_get_thread_num() == 0)	
					{
#endif
						progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
						if(*Filter::wantAbort)
						{
							#ifdef _OPENMP
							spin=true;
							#else
							return ERR_ABORT;
							#endif 
						}
#ifdef _OPENMP
					}
#endif
				}

#ifdef _OPENMP
				//Check to see if we aborted the Calculation
				if(spin)
					return ERR_ABORT;
#endif
					
				break;
			}
			default:
				//Do not propagate other types.
				break;
		}
				
	}

#ifdef DEBUG
	ASSERT(ionFrequencies.size());
	//Ion frequencies must be of equal length
	for(unsigned int ui=1;ui<ionFrequencies.size();ui++)
	{
		ASSERT(ionFrequencies[ui].size() == ionFrequencies[0].size());
	}
#endif
	

	vector<float> normalisationFactor;
	vector<unsigned int> normalisationCount;
	normalisationFactor.resize(ionFrequencies[0].size());
	normalisationCount.resize(ionFrequencies[0].size());
	bool needNormalise=false;

	//Perform the appropriate normalisation
	if(!rngData && normalise)
	{
		// For density plots, normalise by
		//  the volume of the primitive's shell
		switch(primitiveType)
		{
			case PRIMITIVE_CYLINDER_AXIAL:
			case PRIMITIVE_CYLINDER_RADIAL:
			{

				float dx;
				if(fixedBins)
					dx=(sqrtf(vectorParams[1].sqrMag())/(float)numBins);

				else
					dx=binWidth;
				needNormalise=true;
				float nFact;
				//Normalise by cylinder slice volume, pi*r^2*h.
				// This is the same in both radial and axial mode as the radial slices are equi-volume, 
				// same as axial mode
				nFact=1.0/(M_PI*scalarParams[0]*scalarParams[0]*dx);
				for(unsigned int uj=0;uj<normalisationFactor.size(); uj++)
					normalisationFactor[uj] = nFact;
				break;
			}
			case PRIMITIVE_SPHERE:
			{
				float dx;
				if(fixedBins)
					dx=(scalarParams[0]/(float)numBins);

				else
					dx=binWidth;
				for(unsigned int uj=0;uj<normalisationFactor.size(); uj++)
				{
					//Normalise by sphere shell volume, 
					// 4/3 *PI*dx^3*((n+1)^3-n^3)
					//  note -> (n+1)^3 -n^3  = (3*n^2) + (3*n) + 1
					normalisationFactor[uj] = 1.0/(4.0/3.0*M_PI*
						dx*(3.0*((float)uj*(float)uj + uj) + 1.0));
				}
				break;
			}
			default:
				ASSERT(false);
		}
	}
	else if(normalise && rngData) //compute normalisation values, if we are in composition mode
	{
		// the loops' nesting is reversed as we need to sum over distinct plots
		//Density profiles (non-ranged plots) have a fixed normalisation factor
		needNormalise=true;

		for(unsigned int uj=0;uj<ionFrequencies[0].size(); uj++)
		{
			float sum;
			sum=0;
			//Loop across each bin type, summing result
			for(unsigned int uk=0;uk<ionFrequencies.size();uk++)
				sum +=(float)ionFrequencies[uk][uj];
			normalisationCount[uj]=sum;

	
			//Compute the normalisation factor
			if(sum)
				normalisationFactor[uj]=1.0/sum;
			else
				normalisationFactor[uj] = 0;
		}

	}

	
	//Create the plots
	PlotStreamData *plotData[ionFrequencies.size()];
	for(unsigned int ui=0;ui<ionFrequencies.size();ui++)
	{
		plotData[ui] = new PlotStreamData;

		plotData[ui]->index=ui;
		plotData[ui]->parent=this;
		plotData[ui]->xLabel= TRANS("Distance");
		plotData[ui]->errDat=errMode;
		if(normalise)
		{
			//If we have composition, normalise against 
			//sum composition = 1 otherwise use volume of bin
			//as normalisation factor
			if(rngData)
				plotData[ui]->yLabel= TRANS("Fraction");
			else
				plotData[ui]->yLabel= TRANS("Density (\\frac{\\#}{len^3})");
		}
		else
			plotData[ui]->yLabel= TRANS("Count");

		//Give the plot a title like TRANS("Myplot:Mg" (if have range) or "MyPlot") (no range)
		if(rngData)
		{
			unsigned int thisIonID;
			thisIonID = inverseIDMapping.find(ui)->second;
			plotData[ui]->dataLabel = getUserString() + string(":") 
					+ rngData->rangeFile->getName(thisIonID);

		
			//Set the plot colour to the ion colour	
			RGBf col;
			col=rngData->rangeFile->getColour(thisIonID);

			plotData[ui]->r =col.red;
			plotData[ui]->g =col.green;
			plotData[ui]->b =col.blue;

		}
		else
		{
			//If it only has one component, then 
			//it's not really a composition profile is it?
			plotData[ui]->dataLabel= TRANS("Freq. Profile");
			plotData[ui]->r = rgba.r();
			plotData[ui]->g = rgba.g();
			plotData[ui]->b = rgba.b();
			plotData[ui]->a = rgba.a();
		}

		plotData[ui]->xyData.reserve(ionFrequencies[ui].size());
	

		//Go through each bin, then perform the appropriate normalisation
		for(unsigned int uj=0;uj<ionFrequencies[ui].size(); uj++)
		{
			float xPos;
			xPos = getBinPosition(uj);

			if(ionFrequencies[ui][uj] < minEvents)
				continue;

			//Recompute normalisation value for this bin, if needed
			if(needNormalise)
			{
				float normFactor=normalisationFactor[uj];

				//keep the data if we are not using minimum threshold for normalisation, or we met the 
				// threhsold
				plotData[ui]->xyData.push_back(
					std::make_pair(xPos,
					normFactor*(float)ionFrequencies[ui][uj]));
			}
			else
			{	
				plotData[ui]->xyData.push_back(
					std::make_pair(xPos,ionFrequencies[ui][uj]) );
			}
		}




		plotData[ui]->plotStyle = plotStyle;
		plotData[ui]->plotMode=PLOT_MODE_1D;

		//If we ended up with any data, display it
		// otherwise, trash the plot info
		if(plotData[ui]->xyData.size())
		{
			cacheAsNeeded(plotData[ui]);
			getOut.push_back(plotData[ui]);
		}
		else
		{
			consoleOutput.push_back(TRANS("No data remained in profile - cannot display result"));
			delete plotData[ui];
		}
	}
	
	progress.filterProgress=100;

	return 0;
}

std::string  ProfileFilter::getSpecificErrString(unsigned int code) const
{
	const char *errCodes[] =   { "",
		"Too many bins in comp. profile.",
		"Not enough memory for comp. profile.",
		"Aborted composition prof." }; 

	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(errCodes) == ERR_COMP_ENUM_END);
	ASSERT(code < ERR_COMP_ENUM_END);

	return errCodes[code];
}

bool ProfileFilter::setProperty( unsigned int key, 
					const std::string &value, bool &needUpdate) 
{
			
	switch(key)
	{
		case PROFILE_KEY_DENSITY_ONLY:
		{
			if(!applyPropertyNow(wantDensity,value,needUpdate))
				return false;
			break;
		}
		case PROFILE_KEY_BINWIDTH:
		{
			float newBinWidth;
			if(stream_cast(newBinWidth,value))
				return false;

			if(newBinWidth < sqrtf(std::numeric_limits<float>::epsilon()))
				return false;

			binWidth=newBinWidth;
			clearCache();
			needUpdate=true;
			break;
		}
		case PROFILE_KEY_FIXEDBINS:
		{
			if(!applyPropertyNow(fixedBins,value,needUpdate))
				return false;
			break;	
		}
		case PROFILE_KEY_NORMAL:
		{
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			if(primitiveType == PRIMITIVE_CYLINDER_AXIAL)
			{
				if(lockAxisMag && 
					newPt.sqrMag() > sqrtf(std::numeric_limits<float>::epsilon()))
				{
					newPt.normalise();
					newPt*=sqrtf(vectorParams[1].sqrMag());
				}
			}
			if(newPt.sqrMag() < sqrtf(std::numeric_limits<float>::epsilon()))
				return false;

			if(!(vectorParams[1] == newPt ))
			{
				vectorParams[1] = newPt;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case PROFILE_KEY_MINEVENTS:
		{
			if(!applyPropertyNow(minEvents,value,needUpdate))
				return false;
			break;	
		}
		case PROFILE_KEY_NUMBINS:
		{
			unsigned int newNumBins;
			if(stream_cast(newNumBins,value))
				return false;

			//zero bins disallowed
			if(!newNumBins)
				return false;

			nBins=newNumBins;

			clearCache();
			needUpdate=true;
			break;
		}
		case PROFILE_KEY_ORIGIN:
		{
			if(!applyPropertyNow(vectorParams[0],value,needUpdate))
				return false;
			return true;
		}
		case PROFILE_KEY_PRIMITIVETYPE:
		{
			unsigned int newPrimitive;
			newPrimitive=getPrimitiveId(value);
			if(newPrimitive >= PRIMITIVE_END)
				return false;

			//set the new primitive type
			primitiveType=newPrimitive;

			//set up the values for the new primitive type,
			// preserving data where possible
			switch(primitiveType)
			{
				case PRIMITIVE_CYLINDER_AXIAL:
				case PRIMITIVE_CYLINDER_RADIAL:
				{
					if(vectorParams.size() != 2)
					{
						if(vectorParams.size() <2 )
						{
							vectorParams.clear();
							vectorParams.push_back(Point3D(0,0,0));
							vectorParams.push_back(Point3D(0,20,0));
						}
						else
							vectorParams.resize(2);
					}

					if(scalarParams.size() != 1)
					{
						if (scalarParams.size() > 1)
						{
							scalarParams.clear();
							scalarParams.push_back(DEFAULT_RADIUS);
						}
						else
							scalarParams.resize(1);
					}

					if(primitiveType == PRIMITIVE_CYLINDER_RADIAL)
						fixedBins=true;

					break;
				}
				case PRIMITIVE_SPHERE:
				{
					if(vectorParams.size() !=1)
					{
						if(vectorParams.size() >1)
							vectorParams.resize(1);
						else
							vectorParams.push_back(Point3D(0,0,0));
					}

					if(scalarParams.size() !=1)
					{
						if(scalarParams.size() > 1)
							scalarParams.resize(1);
						else
							scalarParams.push_back(DEFAULT_RADIUS);
					}
					break;
				}

				default:
					ASSERT(false);
			}
	
			clearCache();	
			needUpdate=true;	
			return true;	
		}
		case PROFILE_KEY_RADIUS:
		{
			float newRad;
			if(stream_cast(newRad,value))
				return false;

			if(newRad < sqrtf(std::numeric_limits<float>::epsilon()))
				return false;

			if(scalarParams[0] != newRad )
			{
				scalarParams[0] = newRad;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case PROFILE_KEY_SHOWPRIMITIVE:
		{
			if(!applyPropertyNow(showPrimitive,value,needUpdate))
				return false;
			break;	
		}
		case PROFILE_KEY_NORMALISE:
		{
			if(!applyPropertyNow(normalise,value,needUpdate))
				return false;
			break;	
		}
		case PROFILE_KEY_LOCKAXISMAG:
		{
			if(!applyPropertyNow(lockAxisMag,value,needUpdate))
				return false;
			break;
		}
		case PROFILE_KEY_PLOTTYPE:
		{
			unsigned int tmpPlotType;

			tmpPlotType=plotID(value);

			if(tmpPlotType >= PLOT_LINE_NONE)
				return false;

			plotStyle = tmpPlotType;
			needUpdate=true;	
			break;
		}
		case PROFILE_KEY_COLOUR:
		{
			ColourRGBA tmpRgba;
			if(!tmpRgba.parse(value))
				return false;
			
			rgba=tmpRgba.toRGBAf();
			needUpdate=true;
			break;	
		}
		case PROFILE_KEY_ERRMODE:
		{
			unsigned int tmpMode;
			tmpMode=plotErrmodeID(value);

			if(tmpMode >= PLOT_ERROR_ENDOFENUM)
				return false;

			errMode.mode= tmpMode;
			needUpdate=true;

			break;
		}
		case PROFILE_KEY_AVGWINSIZE:
		{
			unsigned int tmpNum;
			if(stream_cast(tmpNum,value))
				return false;

			if(tmpNum<=1)
				return false;

			errMode.movingAverageNum=tmpNum;
			needUpdate=true;
			break;
		}
		default:
			ASSERT(false);	
	}

	if(needUpdate)
		clearCache();

	return true;
}

void ProfileFilter::getProperties(FilterPropGroup &propertyList) const
{
	bool doDensityPlot = (!haveRangeParent) || wantDensity; 

	string str,tmpStr;
	FilterProperty p;
	size_t curGroup=0;

	if(haveRangeParent)
	{
		stream_cast(tmpStr,wantDensity);
		p.name=TRANS("Total Density");	
		p.data=tmpStr;
		p.key=PROFILE_KEY_DENSITY_ONLY;
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Do not do per-species analysis, perform density computation only");
		propertyList.addProperty(p,curGroup);
	}

	//Allow primitive selection if we have more than one primitive
	if(PRIMITIVE_END > 1)
	{
		//Choices for primitive type
		vector<pair<unsigned int,string> > choices;
		for(unsigned int ui=0;ui<PRIMITIVE_END;ui++)
		{
			str =TRANS(PRIMITIVE_NAME[ui]);
			choices.push_back(make_pair(ui,str));
		}
		p.name=TRANS("Primitive type");
		p.data=choiceString(choices,primitiveType);
		p.key=PROFILE_KEY_PRIMITIVETYPE;
		p.type=PROPERTY_TYPE_CHOICE;
		p.helpText=TRANS("Basic shape to use for profile");
		propertyList.addProperty(p,curGroup);
		propertyList.setGroupTitle(curGroup,TRANS("Primitive"));	
		curGroup++;
	}

	
	stream_cast(tmpStr,showPrimitive);
	p.name=TRANS("Show Primitive");	
	p.data=tmpStr;
	p.key=PROFILE_KEY_SHOWPRIMITIVE;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Display the 3D composition profile interaction object");
	propertyList.addProperty(p,curGroup);

	switch(primitiveType)
	{
		case PRIMITIVE_CYLINDER_AXIAL:
		case PRIMITIVE_CYLINDER_RADIAL:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			stream_cast(str,vectorParams[0]);
			p.key=PROFILE_KEY_ORIGIN;
			p.name=TRANS("Origin");
			p.data=str;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Position for centre of cylinder");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(str,vectorParams[1]);
			p.key=PROFILE_KEY_NORMAL;
			p.name=TRANS("Axis");
			p.data=str;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Vector between ends of cylinder");
			propertyList.addProperty(p,curGroup);

			str=boolStrEnc(lockAxisMag);
			p.key=PROFILE_KEY_LOCKAXISMAG;
			p.name=TRANS("Lock Axis Mag.");
			p.data= str;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Prevent length of cylinder changing during interaction");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(str,scalarParams[0]);
			p.key=PROFILE_KEY_RADIUS;
			p.name=TRANS("Radius");
			p.data= str;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Radius of cylinder");
			propertyList.addProperty(p,curGroup);
			break;
		}
		case PRIMITIVE_SPHERE:
		{
			
			ASSERT(vectorParams.size() == 1);
			ASSERT(scalarParams.size() == 1);
			stream_cast(str,vectorParams[0]);
			p.key=PROFILE_KEY_ORIGIN;
			p.name=TRANS("Origin");
			p.data=str;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Position for centre of sphere");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(str,scalarParams[0]);
			p.key=PROFILE_KEY_RADIUS;
			p.name=TRANS("Radius");
			p.data= str;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Radius of sphere");
			propertyList.addProperty(p,curGroup);
			break;
		}
		default:
			ASSERT(false);
	}

	//Must be fixed bin num in radial mode. Disallow turning this off
	if(primitiveType!= PRIMITIVE_CYLINDER_RADIAL)
	{
		p.key=PROFILE_KEY_FIXEDBINS;
		stream_cast(str,fixedBins);
		p.name=TRANS("Fixed Bin Num");
		p.data=str;
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("If true, use a fixed number of bins for profile, otherwise use fixed step size");
		propertyList.addProperty(p,curGroup);
	}

	if(fixedBins)
	{
		stream_cast(tmpStr,nBins);
		str = TRANS("Num Bins");
		p.name=str;
		p.data=tmpStr;
		p.key=PROFILE_KEY_NUMBINS;
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of bins to use for profile");
		propertyList.addProperty(p,curGroup);
	}
	else
	{
		ASSERT(primitiveType!=PRIMITIVE_CYLINDER_RADIAL);
		str = TRANS("Bin width");
		stream_cast(tmpStr,binWidth);
		p.name=str;
		p.data=tmpStr;
		p.key=PROFILE_KEY_BINWIDTH;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Size of each bin in profile");
		propertyList.addProperty(p,curGroup);
	}

	stream_cast(tmpStr,normalise);
	p.name= TRANS("Normalise");	
	p.data=tmpStr;
	p.key=PROFILE_KEY_NORMALISE;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Convert bin counts into relative frequencies in each bin");
	propertyList.addProperty(p,curGroup);

	stream_cast(tmpStr,minEvents);
	p.name= TRANS("Min. events");	
	p.data=tmpStr;
	p.key=PROFILE_KEY_MINEVENTS;
	p.type=PROPERTY_TYPE_INTEGER;
	p.helpText=TRANS("Drop data that does not have this many events");
	propertyList.addProperty(p,curGroup);

	propertyList.setGroupTitle(curGroup,TRANS("Settings"));	



	curGroup++;
	
	//use set 2 to store the plot properties
	stream_cast(str,plotStyle);
	//Let the user know what the valid values for plot type are
	vector<pair<unsigned int,string> > choices;


	tmpStr=plotString(PLOT_LINE_LINES);
	choices.push_back(make_pair((unsigned int) PLOT_LINE_LINES,tmpStr));
	tmpStr=plotString(PLOT_LINE_BARS);
	choices.push_back(make_pair((unsigned int)PLOT_LINE_BARS,tmpStr));
	tmpStr=plotString(PLOT_LINE_STEPS);
	choices.push_back(make_pair((unsigned int)PLOT_LINE_STEPS,tmpStr));
	tmpStr=plotString(PLOT_LINE_STEM);
	choices.push_back(make_pair((unsigned int)PLOT_LINE_STEM,tmpStr));

	tmpStr= choiceString(choices,plotStyle);
	p.name=TRANS("Plot Type");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Visual style for plot");
	p.key=PROFILE_KEY_PLOTTYPE;
	propertyList.addProperty(p,curGroup);

	//If we are not doing per-species, then we need colour
	if(doDensityPlot)
	{
		//Convert the colour to a hex string
		p.name=TRANS("Colour");
		p.data=rgba.toColourRGBA().rgbString();
		p.type=PROPERTY_TYPE_COLOUR;
		p.helpText=TRANS("Colour of plot");
		p.key=PROFILE_KEY_COLOUR;
		propertyList.addProperty(p,curGroup);
	}


	propertyList.setGroupTitle(curGroup,TRANS("Appearance"));
	curGroup++;
	
	choices.clear();
	tmpStr=plotErrmodeString(PLOT_ERROR_NONE);
	choices.push_back(make_pair((unsigned int) PLOT_ERROR_NONE,tmpStr));
	tmpStr=plotErrmodeString(PLOT_ERROR_MOVING_AVERAGE);
	choices.push_back(make_pair((unsigned int) PLOT_ERROR_MOVING_AVERAGE,tmpStr));

	tmpStr= choiceString(choices,errMode.mode);
	p.name=TRANS("Err. Estimator");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Method of estimating error associated with each bin");
	p.key=PROFILE_KEY_ERRMODE;
	propertyList.addProperty(p,curGroup);

	if(errMode.mode == PLOT_ERROR_MOVING_AVERAGE)
	{
		stream_cast(tmpStr,errMode.movingAverageNum);
		p.name=TRANS("Avg. Window");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of bins to include in moving average filter");
		p.key=PROFILE_KEY_AVGWINSIZE;
		propertyList.addProperty(p,curGroup);
	}	
	propertyList.setGroupTitle(curGroup,TRANS("Error analysis"));
}

unsigned int ProfileFilter::getBinData(unsigned int &numBins, float &length) const
{
	//Number of bins, having determined if we are using
	//fixed bin count or not
	switch(primitiveType)
	{
		case PRIMITIVE_SPHERE:
			//radius of sphere
			length=scalarParams[0];
			break;
		case PRIMITIVE_CYLINDER_AXIAL:
			//length of cylinder, full axis length
			length=sqrtf(vectorParams[1].sqrMag());
			break;
		case PRIMITIVE_CYLINDER_RADIAL:
			//radius of cylinder
			length =scalarParams[0];
			break;
		default:
			ASSERT(false);
	}
	
	if(fixedBins)
		numBins=nBins;
	else
	{
		switch(primitiveType)
		{
			case PRIMITIVE_CYLINDER_AXIAL:
			case PRIMITIVE_CYLINDER_RADIAL:
			case PRIMITIVE_SPHERE:
			{

				ASSERT(binWidth > std::numeric_limits<float>::epsilon());

				//Check for possible overflow
				if(length/binWidth > (float)std::numeric_limits<unsigned int>::max())
					return ERR_NUMBINS;

				numBins=(unsigned int)(length/binWidth);
				break;
			}
			default:
				ASSERT(false);
		}
		
	}
	
	return 0;
}

float ProfileFilter::getBinPosition(unsigned int nBin) const
{
	unsigned int nBinsMax; float fullLen, xPos;
	getBinData(nBinsMax,fullLen);	
	ASSERT(nBin < nBinsMax)
	xPos = ((float) nBin + 0.5)/(float)nBinsMax;
	if( primitiveType == PRIMITIVE_CYLINDER_RADIAL)
	{
		float maxPosSqr = fullLen*fullLen;
		//compute fraction
		xPos = sqrt ( xPos*maxPosSqr);
	}
	else
	{
		xPos = xPos*fullLen;
	}			

	return xPos;
}

//!Get approx number of bytes for caching output
size_t ProfileFilter::numBytesForCache(size_t nObjects) const
{
	float length;
	unsigned int errCode, numBins;
	errCode=getBinData(numBins,length);

	if(errCode)
		return (unsigned int)-1;
	
	return (numBins*2*sizeof(float));
}

bool ProfileFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

			f << tabs(depth+1) << "<primitivetype value=\"" << primitiveType<< "\"/>" << endl;
			f << tabs(depth+1) << "<showprimitive value=\"" << showPrimitive << "\"/>" << endl;
			f << tabs(depth+1) << "<lockaxismag value=\"" << lockAxisMag<< "\"/>" << endl;
			f << tabs(depth+1) << "<vectorparams>" << endl;
			for(unsigned int ui=0; ui<vectorParams.size(); ui++)
			{
				f << tabs(depth+2) << "<point3d x=\"" << vectorParams[ui][0] << 
					"\" y=\"" << vectorParams[ui][1] << "\" z=\"" << vectorParams[ui][2] << "\"/>" << endl;
			}
			f << tabs(depth+1) << "</vectorparams>" << endl;

			f << tabs(depth+1) << "<scalarparams>" << endl;
			for(unsigned int ui=0; ui<scalarParams.size(); ui++)
				f << tabs(depth+2) << "<scalar value=\"" << scalarParams[0] << "\"/>" << endl; 
			
			f << tabs(depth+1) << "</scalarparams>" << endl;
			f << tabs(depth+1) << "<normalise value=\"" << normalise << "\" minevents=\"" << minEvents << "\" />" << endl;
			f << tabs(depth+1) << "<fixedbins value=\"" << (int)fixedBins << "\"/>" << endl;
			f << tabs(depth+1) << "<nbins value=\"" << nBins << "\"/>" << endl;
			f << tabs(depth+1) << "<binwidth value=\"" << binWidth << "\"/>" << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  rgba.r() << "\" g=\"" << rgba.g() << "\" b=\"" << rgba.b()
				<< "\" a=\"" << rgba.a() << "\"/>" <<endl;

			f << tabs(depth+1) << "<plottype value=\"" << plotStyle << "\"/>" << endl;
			f << tabs(depth) << "</" << trueName()  << ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}


void ProfileFilter::setUserString(const std::string &str)
{
	if(userString != str)
	{
		userString=str;
		clearCache();
	}	
}

bool ProfileFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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
	if(XMLHelpFwdToElem(nodePtr,"primitivetype"))
		return false;


	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(primitiveType,tmpStr))
		return false;

	//FIXME: DEPRECATE 3Depict versions <=0.0.17 had only two primitives,
	// cylinder and sphere. 
/*	if(versionCheckGreater(Filter::stateWriterVersion,("0.0.17")))
	{
		//remap the primitive type as needed
		if(primitiveType == PRIMITIVE_CYLINDER_RADIAL)
			primitiveType=PRIMITIVE_SPHERE;

	}
*/
	if(primitiveType >= PRIMITIVE_END)
	       return false;	
	xmlFree(xmlString);
	//====
	
	//Retrieve primitive visibility 
	//====
	if(XMLHelpFwdToElem(nodePtr,"showprimitive"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(!boolStrDec(tmpStr,showPrimitive))
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve axis lock mode 
	//====
	if(XMLHelpFwdToElem(nodePtr,"lockaxismag"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(!boolStrDec(tmpStr,lockAxisMag))
		return false;

	xmlFree(xmlString);
	//====
	
	//Retrieve vector parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"vectorparams"))
		return false;
	xmlNodePtr tmpNode=nodePtr;

	nodePtr=nodePtr->xmlChildrenNode;

	vectorParams.clear();
	while(!XMLHelpFwdToElem(nodePtr,"point3d"))
	{
		float x,y,z;
		//--Get X value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(x,tmpStr))
			return false;

		//--Get Z value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(y,tmpStr))
			return false;

		//--Get Y value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(z,tmpStr))
			return false;

		vectorParams.push_back(Point3D(x,y,z));
	}
	//===	

	nodePtr=tmpNode;
	//Retrieve scalar parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"scalarparams"))
		return false;
	
	tmpNode=nodePtr;
	nodePtr=nodePtr->xmlChildrenNode;

	scalarParams.clear();
	while(!XMLHelpFwdToElem(nodePtr,"scalar"))
	{
		float v;
		//Get value
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(v,tmpStr))
			return false;
		scalarParams.push_back(v);
	}
	//===	

	//Check the scalar params match the selected primitive	
	switch(primitiveType)
	{
		case PRIMITIVE_CYLINDER_AXIAL:
		case PRIMITIVE_CYLINDER_RADIAL:
			if(vectorParams.size() != 2 || scalarParams.size() !=1)
				return false;
			break;
		case PRIMITIVE_SPHERE:
			if(vectorParams.size() != 1 || scalarParams.size() !=1)
				return false;
			break;

		default:
			ASSERT(false);
			return false;
	}

	nodePtr=tmpNode;

	//Retrieve normalisation on/off 
	//====
	if(XMLHelpFwdToElem(nodePtr,"normalise"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;


	if(!boolStrDec(tmpStr,normalise))
		return false;

	xmlFree(xmlString);
	//====

	//Retrieve fixed bins on/off 
	//====
	if(XMLHelpFwdToElem(nodePtr,"fixedbins"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(!boolStrDec(tmpStr,fixedBins))
		return false;


	xmlFree(xmlString);
	//====

	//Retrieve num bins
	//====
	if(XMLHelpFwdToElem(nodePtr,"nbins"))
		return false;

	
	if(XMLHelpGetProp(nBins,nodePtr,"value"))
		return false;


	if(XMLHelpGetProp(minEvents,nodePtr,"minevents"))
	{
		//FIXME: Deprecate me.
		minEvents=MINEVENTS_DEFAULT;
	}
	//====

	//Retrieve bin width
	//====
	if(XMLHelpFwdToElem(nodePtr,"binwidth"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(binWidth,tmpStr))
		return false;

	xmlFree(xmlString);
	//====

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	if(!parseXMLColour(nodePtr,rgba))
		return false;
	//====
	
	//Retrieve plot type 
	//====
	if(XMLHelpFwdToElem(nodePtr,"plottype"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(plotStyle,tmpStr))
		return false;

	if(plotStyle >= PLOT_LINE_NONE)
	       return false;	
	xmlFree(xmlString);
	//====

	return true;
}

unsigned int ProfileFilter::getRefreshBlockMask() const
{
	//Absolutely anything can go through this filter.
	return 0;
}

unsigned int ProfileFilter::getRefreshEmitMask() const
{
	if(showPrimitive)
		return STREAM_TYPE_PLOT | STREAM_TYPE_DRAW;
	else
		return STREAM_TYPE_PLOT;
}

unsigned int ProfileFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
}

void ProfileFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
		case BINDING_CYLINDER_RADIUS:
		case BINDING_SPHERE_RADIUS:
			b.getValue(scalarParams[0]);
			break;
		case BINDING_CYLINDER_ORIGIN:
		case BINDING_SPHERE_ORIGIN:
			b.getValue(vectorParams[0]);
			break;
		case BINDING_CYLINDER_DIRECTION:
		{
			Point3D pOld=vectorParams[1];
			b.getValue(vectorParams[1]);
			//Test getting the bin data.
			// if something is wrong, abort
			float length;
			unsigned int numBins;
			unsigned int errCode= getBinData(numBins,length);
			if(errCode || !numBins)
			{
				vectorParams[1]=pOld;
				return;
			}

			break;
		}
		default:
			ASSERT(false);
	}

	clearCache();
}

unsigned int ProfileFilter::getPrimitiveId(const std::string &primitiveName) 
{
	for(size_t ui=0;ui<PRIMITIVE_END; ui++)
	{
		if( TRANS(PRIMITIVE_NAME[ui]) == primitiveName)
			return ui;
	}

	ASSERT(false);
}

#ifdef DEBUG

bool testDensityCylinder();
bool testCompositionCylinder();
void synthComposition(const vector<pair<float,float> > &compositionData,
			vector<IonHit> &h);
IonStreamData *synthLinearProfile(const Point3D &start, const Point3D &end,
					float radialSpread,unsigned int numPts);

bool ProfileFilter::runUnitTests()
{
	if(!testDensityCylinder())
		return false;

	if(!testCompositionCylinder())
		return false;

	return true;
}

bool testCompositionCylinder()
{
	IonStreamData *d;
	const size_t NUM_PTS=10000;

	//Create a cylinder of data, forming a linear profile
	Point3D startPt(-1.0f,-1.0f,-1.0f),endPt(1.0f,1.0f,1.0f);
	d= synthLinearProfile(startPt,endPt,
			0.5f, NUM_PTS);

	//Generate two compositions for the test dataset
	{
	vector<std::pair<float,float>  > vecCompositions;
	vecCompositions.push_back(make_pair(2.0f,0.5f));
	vecCompositions.push_back(make_pair(3.0f,0.5f));
	synthComposition(vecCompositions,d->data);
	}

	//Build a faux rangestream
	RangeStreamData *rngStream;
	rngStream = new RangeStreamData;
	rngStream->rangeFile = new RangeFile;
	
	RGBf rgb; rgb.red=rgb.green=rgb.blue=1.0f;

	unsigned int aIon,bIon;
	std::string tmpStr;
	tmpStr="A";
	aIon=rngStream->rangeFile->addIon(tmpStr,tmpStr,rgb);
	tmpStr="B";
	bIon=rngStream->rangeFile->addIon(tmpStr,tmpStr,rgb);
	rngStream->rangeFile->addRange(1.5,2.5,aIon);
	rngStream->rangeFile->addRange(2.5,3.5,bIon);
	rngStream->enabledIons.resize(2,true);
	rngStream->enabledRanges.resize(2,true);

	//Construct the composition filter
	ProfileFilter *f = new ProfileFilter;

	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	
	bool needUp; std::string s;
	stream_cast(s,Point3D((startPt+endPt)*0.5f));
	TEST(f->setProperty(PROFILE_KEY_ORIGIN,s,needUp),"set origin");
	TEST(f->setProperty(PROFILE_KEY_MINEVENTS,"0",needUp),"set origin");
	
	stream_cast(s,Point3D((endPt-startPt)*0.5f));
	TEST(f->setProperty(PROFILE_KEY_NORMAL,s,needUp),"set direction");
	TEST(f->setProperty(PROFILE_KEY_SHOWPRIMITIVE,"1",needUp),"Set cylinder visibility");
	TEST(f->setProperty(PROFILE_KEY_NORMALISE,"1",needUp),"Disable normalisation");
	TEST(f->setProperty(PROFILE_KEY_RADIUS,"5",needUp),"Set radius");
	
	//Inform the filter about the range stream
	streamIn.push_back(rngStream);
	f->initFilter(streamIn,streamOut);
	
	streamIn.push_back(d);
	f->setCaching(false);


	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Refresh error code");

	//2* plot, 1*rng, 1*draw
	TEST(streamOut.size() == 4, "output stream count");

	delete d;

	std::map<unsigned int, unsigned int> countMap;
	countMap[STREAM_TYPE_PLOT] = 0;
	countMap[STREAM_TYPE_DRAW] = 0;
	countMap[STREAM_TYPE_RANGE] = 0;

	for(unsigned int ui=0;ui<streamOut.size();ui++)
	{
		ASSERT(countMap.find(streamOut[ui]->getStreamType()) != countMap.end());
		countMap[streamOut[ui]->getStreamType()]++;
	}

	TEST(countMap[STREAM_TYPE_PLOT] == 2,"Plot count");
	TEST(countMap[STREAM_TYPE_DRAW] == 1,"Draw count");
	TEST(countMap[STREAM_TYPE_RANGE] == 1,"Range count");
	
	const PlotStreamData* plotData=0;
	for(unsigned int ui=0;ui<streamOut.size();ui++)
	{
		if(streamOut[ui]->getStreamType() == STREAM_TYPE_PLOT)
		{
			plotData = (const PlotStreamData *)streamOut[ui];
			break;
		}
	}
	TEST(plotData,"Should have plot data");
	TEST(plotData->xyData.size(),"Plot data size");

	for(size_t ui=0;ui<plotData->xyData.size(); ui++)
	{
		TEST(plotData->xyData[ui].second <= 1.0f && 
			plotData->xyData[ui].second >=0.0f,"normalised data range test"); 
	}

	delete rngStream->rangeFile;
	for(unsigned int ui=0;ui<streamOut.size();ui++)
		delete streamOut[ui];


	delete f;

	return true;
}

bool testDensityCylinder()
{
	IonStreamData *d;
	const size_t NUM_PTS=10000;

	//Create a cylinder of data, forming a linear profile
	Point3D startPt(-1.0f,-1.0f,-1.0f),endPt(1.0f,1.0f,1.0f);
	d= synthLinearProfile(startPt,endPt,
			0.5f, NUM_PTS);

	//Generate two compositions for the test dataset
	{
	vector<std::pair<float,float>  > vecCompositions;
	vecCompositions.push_back(make_pair(2.0f,0.5f));
	vecCompositions.push_back(make_pair(3.0f,0.5f));
	synthComposition(vecCompositions,d->data);
	}

	ProfileFilter *f = new ProfileFilter;
	f->setCaching(false);

	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(d);
	
	bool needUp; std::string s;
	stream_cast(s,Point3D((startPt+endPt)*0.5f));
	TEST(f->setProperty(PROFILE_KEY_ORIGIN,s,needUp),"set origin");
	
	stream_cast(s,Point3D((endPt-startPt)));
	TEST(f->setProperty(PROFILE_KEY_NORMAL,s,needUp),"set direction");
	
	TEST(f->setProperty(PROFILE_KEY_SHOWPRIMITIVE,"1",needUp),"Set cylinder visibility");

	TEST(f->setProperty(PROFILE_KEY_NORMALISE,"0",needUp),"Disable normalisation");
	TEST(f->setProperty(PROFILE_KEY_RADIUS,"5",needUp),"Set radius");

	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Refresh error code");
	delete f;
	delete d;


	TEST(streamOut.size() == 2, "output stream count");

	std::map<unsigned int, unsigned int> countMap;
	countMap[STREAM_TYPE_PLOT] = 0;
	countMap[STREAM_TYPE_DRAW] = 0;

	for(unsigned int ui=0;ui<streamOut.size();ui++)
	{
		ASSERT(countMap.find(streamOut[ui]->getStreamType()) != countMap.end());
		countMap[streamOut[ui]->getStreamType()]++;
	}

	TEST(countMap[STREAM_TYPE_PLOT] == 1,"Plot count");
	TEST(countMap[STREAM_TYPE_DRAW] == 1,"Draw count");

	
	const PlotStreamData* plotData=0;
	for(unsigned int ui=0;ui<streamOut.size();ui++)
	{
		if(streamOut[ui]->getStreamType() == STREAM_TYPE_PLOT)
		{
			plotData = (const PlotStreamData *)streamOut[ui];
			break;
		}
	}

	float sum=0;
	for(size_t ui=0;ui<plotData->xyData.size(); ui++)
		sum+=plotData->xyData[ui].second;


	TEST(sum > NUM_PTS/1.2f,"Number points roughly OK");
	TEST(sum <= NUM_PTS,"No overcounting");
	
	for(unsigned int ui=0;ui<streamOut.size();ui++)
		delete streamOut[ui];

	return true;
}


//first value in pair is target mass, second value is target composition
void synthComposition(const vector<std::pair<float,float> > &compositionData,
			vector<IonHit> &h)
{
	float fractionSum=0;
	for(size_t ui=0;ui<compositionData.size(); ui++)
		fractionSum+=compositionData[ui].second;

	//build the spacings between 0 and 1, so we can
	//randomly select ions by uniform deviates
	vector<std::pair<float,float> > ionCuts;
	ionCuts.resize(compositionData.size());
	//ionCuts.resize[compositionData.size()];
	float runningSum=0;
	for(size_t ui=0;ui<ionCuts.size(); ui++)
	{
		runningSum+=compositionData[ui].second;
		ionCuts[ui]=make_pair(compositionData[ui].first, 
				runningSum/fractionSum);
	}

	RandNumGen rngHere;
	rngHere.initTimer();
	for(size_t ui=0;ui<h.size();ui++)
	{

		float newMass;
		bool haveSetMass;
		
		//keep generating random selections until we hit something.
		// This is to prevent any fp fallthrough
		do
		{
			float uniformDeviate;
			uniformDeviate=rngHere.genUniformDev();

			haveSetMass=false;
			//This is not efficient -- data is sorted,
			//so binary search would work, but whatever.
			for(size_t uj=0;uj<ionCuts.size();uj++)	
			{
				if(uniformDeviate >=ionCuts[uj].second)
				{
					newMass=ionCuts[uj].first;
					haveSetMass=true;
					break;
				}
			}
		}while(!haveSetMass);


		h[ui].setMassToCharge(newMass);
	}
}


//Create a line of points of fixed mass (1), with a top-hat radial spread function
// so we end up with a cylinder of unit mass data along some start-end axis
//you must free the returned value by calling "delete"
IonStreamData *synthLinearProfile(const Point3D &start, const Point3D &end,
					float radialSpread,unsigned int numPts)
{

	ASSERT((start-end).sqrMag() > std::numeric_limits<float>::epsilon());
	IonStreamData *d = new IonStreamData;

	IonHit h;
	h.setMassToCharge(1.0f);

	Point3D delta; 
	delta=(end-start)*1.0f/(float)numPts;

	RandNumGen rngAxial;
	rngAxial.initTimer();
	
	Point3D unitDelta;
	unitDelta=delta;
	unitDelta.normalise();
	
	
	d->data.resize(numPts);
	for(size_t ui=0;ui<numPts;ui++)
	{
		//generate a random offset vector
		//that is normal to the axis of the simulation
		Point3D randomVector;
		do
		{
			randomVector=Point3D(rngAxial.genUniformDev(),
					rngAxial.genUniformDev(),
					rngAxial.genUniformDev());
		}while(randomVector.sqrMag() < std::numeric_limits<float>::epsilon() &&
			randomVector.angle(delta) < std::numeric_limits<float>::epsilon());

		
		randomVector=randomVector.crossProd(unitDelta);
		randomVector.normalise();

		//create the point
		Point3D pt;
		pt=delta*(float)ui + start; //true location
		pt+=randomVector*radialSpread;
		h.setPos(pt);
		d->data[ui] =h;
	}

	return d;
}
#endif
