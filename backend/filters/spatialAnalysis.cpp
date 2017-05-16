/*
 *	spatialAnalysis.cpp - Perform various data analysis on 3D point clouds
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
#include <gsl/gsl_sf_gamma.h>

#include "spatialAnalysis.h"



#include "algorithms/rdf.h"
#include "geometryHelpers.h"
#include "filterCommon.h"
#include "algorithms/binomial.h"
#include "algorithms/K3DTree-mk2.h"
#include "backend/plot.h"
#include "../APT/APTFileIO.h"

using std::vector;
using std::set;
using std::string;
using std::pair;
using std::make_pair;
using std::map;
using std::list;

enum
{
	KEY_STOPMODE,
	KEY_ALGORITHM,
	KEY_DISTMAX,
	KEY_NNMAX,
	KEY_NNMAX_NORMALISE,
	KEY_NNMAX_SHOWRANDOM,
	KEY_NUMBINS,
	KEY_REMOVAL,
	KEY_REDUCTIONDIST,
	KEY_RETAIN_UPPER,
	KEY_CUTOFF,
	KEY_COLOUR,
	KEY_ENABLE_SOURCE_ALL,
	KEY_ENABLE_TARGET_ALL,
	KEY_ENABLE_NUMERATOR_ALL,
	KEY_ENABLE_DENOMINATOR_ALL,
	KEY_ORIGIN,
	KEY_NORMAL,
	KEY_RADIUS,
	KEY_NUMIONS,
	KEY_SHOW_BINOM_FREQ,
	KEY_SHOW_BINOM_NORM_FREQ,
	KEY_SHOW_BINOM_THEOR_FREQ,
	KEY_SHOW_BINOM_3D_GRID,
	KEY_BINOMIAL_MAX_ASPECT,
	KEY_BINOMIAL_EXTRUDE_DIR,
	KEY_REPLACE_FILE,
	KEY_REPLACE_TOLERANCE,
	KEY_REPLACE_ALGORITHM,
	KEY_REPLACE_VALUE,
};

enum 
{ 
	KEYTYPE_ENABLE_SOURCE=1,
	KEYTYPE_ENABLE_TARGET,
	KEYTYPE_ENABLE_NUMERATOR,
	KEYTYPE_ENABLE_DENOMINATOR,
};

enum {
	ALGORITHM_DENSITY, //Local density analysis
	ALGORITHM_DENSITY_FILTER, //Local density filtering
	ALGORITHM_RDF, //Radial Distribution Function
	ALGORITHM_AXIAL_DF, //Axial Distribution Function (aka atomvicinity, sdm, 1D rdf)
	ALGORITHM_BINOMIAL, //Binomial block method for statistical randomness testing
	ALGORITHM_REPLACE, //Remove, set or modify points using an external file
	ALGORITHM_LOCAL_CONCENTRATION, //Obtain a local concentration plot, as described by Hyde and Marquis (TODO : REF)
	ALGORITHM_ENUM_END,
};

enum{
	STOP_MODE_NEIGHBOUR,
	STOP_MODE_RADIUS,
	STOP_MODE_ENUM_END
};

enum
{
	REPLACE_MODE_SUBTRACT,
	REPLACE_MODE_INTERSECT,
	REPLACE_MODE_UNION,
	REPLACE_MODE_ENUM_END
};

//!Error codes
enum
{
	ERR_ABORT_FAIL=1,
	ERR_BINOMIAL_NO_MEM,
	ERR_NO_RANGE,
	ERR_BINOMIAL_BIN_FAIL,
	INSUFFICIENT_SIZE_ERR,
	ERR_FILE_READ_FAIL,
	SPAT_ERR_END_OF_ENUM,
};
// == NN analysis filter ==


//User visible names for the different algorithms
const char *SPATIAL_ALGORITHMS[] = {
	NTRANS("Local Density"),
	NTRANS("Density Filtering"),
	NTRANS("Radial Distribution"),
	NTRANS("Axial Distribution"),
	NTRANS("Binomial Distribution"),
	NTRANS("Point Em/Replacement"),
	NTRANS("Local Concentration"),
	};

const char *STOP_MODES[] = {
	NTRANS("Neighbour Count"),
	NTRANS("Radius")
};

//User viisble names for the replace sub-algorithms
const char *REPLACE_ALGORITHMS[] = { "Subtract",
					"Intersect",
					"Union",
					};
					

//Switch to determine if algorithms need range propagation or not
const bool WANT_RANGE_PROPAGATION[] = { false, 
					true,
					false,
					false,
					false,
					true,
					false,
					};


//Default distance to use when performing axial distance computations
const float DEFAULT_AXIAL_DISTANCE = 1.0f;

const float DISTANCE_EPSILON=sqrt(std::numeric_limits<float>::epsilon());


//Helper function for computing a weighted mean
float weightedMean(const vector<float> &x, const vector<float> &y,bool zeroOutSingularity=true)
{
	ASSERT(x.size() == y.size());

	float num=0,denom=0;
	for(size_t ui=0;ui<y.size();ui++)
	{
		num+=y[ui]*x[ui];
		denom+=y[ui];
	}

	if(zeroOutSingularity)
	{
		if(denom <std::numeric_limits<float>::epsilon())
			return 0;
	}

	ASSERT(denom);
	return num/denom;
}

//Scan input datastreams to build two point vectors,
// one of those with points specified as "target" 
// which is a copy of the input points
//Returns 0 on no error, otherwise nonzero
template<class T>
size_t buildSplitPoints(const vector<const FilterStreamData *> &dataIn,
				ProgressData &progress, size_t totalDataSize,
				const RangeFile *rngF, const vector<bool> &pSourceEnabled, const vector<bool> &pTargetEnabled,
				vector<T> &pSource, vector<T> &pTarget
				)
{
	size_t sizeNeeded[2];
	sizeNeeded[0]=sizeNeeded[1]=0;

	//Presize arrays
	for(unsigned int ui=0; ui<dataIn.size() ; ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				unsigned int ionID;

				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[ui]);
				ionID=getIonstreamIonID(d,rngF);

				if(ionID == (unsigned int)-1)
				{

					//we have ungrouped ions, so work out size individually
					for(unsigned int uj=0;uj<d->data.size();uj++)
					{
						ionID = rngF->getIonID(d->data[uj].getMassToCharge());

						if(ionID == (unsigned int)-1)
							continue;

						if(pSourceEnabled[ionID])
							sizeNeeded[0]++;
						if(pTargetEnabled[ionID])
							sizeNeeded[1]++;
					}
					
					break;
				}

				if(pSourceEnabled[ionID])
					sizeNeeded[0]+=d->data.size();

				if(pTargetEnabled[ionID])
					sizeNeeded[1]+=d->data.size();

				break;
			}
			default:
				break;
		}
	}

	pSource.resize(sizeNeeded[0]);
	pTarget.resize(sizeNeeded[1]);

	//Fill arrays
	size_t curPos[2];
	curPos[0]=curPos[1]=0;

	for(unsigned int ui=0; ui<dataIn.size() ; ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				unsigned int ionID;
				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[ui]);
				ionID=getIonstreamIonID(d,rngF);

				if(ionID==(unsigned int)(-1))
				{
					//we have ungrouped ions, so work out size individually
					for(unsigned int uj=0;uj<d->data.size();uj++)
					{
						ionID = rngF->getIonID(d->data[uj].getMassToCharge());

						if(ionID == (unsigned int)-1)
							continue;

						if(pSourceEnabled[ionID])
						{
							assignIonData(pSource[curPos[0]],d->data[uj]);
							curPos[0]++;
						}

						if(pTargetEnabled[ionID])
						{
							assignIonData(pTarget[curPos[1]],d->data[uj]);
							curPos[1]++;
						}
					}
					
					break;
				}

				unsigned int dummyProgress=0;
				if(pSourceEnabled[ionID])
				{
					if(extendDataVector(pSource,d->data,
					                     dummyProgress,curPos[0]))
						return ERR_ABORT_FAIL;

					curPos[0]+=d->data.size();
				}

				if(pTargetEnabled[ionID])
				{
					if(extendDataVector(pTarget,d->data,
					                     dummyProgress,curPos[1]))
						return ERR_ABORT_FAIL;

					curPos[1]+=d->data.size();
				}

				break;
			}
			default:
				break;
		}
	}


	return 0;
}

SpatialAnalysisFilter::SpatialAnalysisFilter()
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(STOP_MODES) == STOP_MODE_ENUM_END);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(SPATIAL_ALGORITHMS) == ALGORITHM_ENUM_END);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(WANT_RANGE_PROPAGATION) == ALGORITHM_ENUM_END);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(REPLACE_ALGORITHMS) == REPLACE_MODE_ENUM_END);
	
	
	algorithm=ALGORITHM_DENSITY;
	nnMax=1;
	distMax=1;
	stopMode=STOP_MODE_NEIGHBOUR;

	haveRangeParent=false;
	
	//Default colour is red
	rgba=ColourRGBAf(1.0f,0,0);

	//RDF params
	numBins=100;
	excludeSurface=false;
	reductionDistance=distMax;
	normaliseNNHist=true;
	//Density filtering params
	densityCutoff=1.0f;
	keepDensityUpper=true;
	wantRandomNNHist=true;

	//Binomial parameters
	//--
	numIonsSegment = 200;
	showBinomialFrequencies=true;
	showNormalisedBinomialFrequencies=true;
	showTheoreticFrequencies=true;
	extrusionDirection=0;
	maxBlockAspect=2;
	showGridOverlay=true;
	//--

	//replace tolerance
	replaceTolerance=sqrtf(std::numeric_limits<float>::epsilon());
	replaceMode=REPLACE_MODE_SUBTRACT;
	replaceMass=true;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up

}

Filter *SpatialAnalysisFilter::cloneUncached() const
{
	SpatialAnalysisFilter *p=new SpatialAnalysisFilter;

	p->rgba=rgba;
	
	p->algorithm=algorithm;
	p->stopMode=stopMode;
	p->nnMax=nnMax;
	p->distMax=distMax;

	p->numBins=numBins;
	p->excludeSurface=excludeSurface;
	p->reductionDistance=reductionDistance;
	p->normaliseNNHist = normaliseNNHist;
	p->wantRandomNNHist=wantRandomNNHist;
	
	p->keepDensityUpper=keepDensityUpper;
	p->densityCutoff=densityCutoff;
	
	p->numIonsSegment=numIonsSegment;
	p->maxBlockAspect=maxBlockAspect;
	p->binWidth=binWidth;
	p->extrusionDirection=extrusionDirection;
	p->showBinomialFrequencies=showBinomialFrequencies;
	p->showNormalisedBinomialFrequencies=showNormalisedBinomialFrequencies;
	p->showTheoreticFrequencies=showTheoreticFrequencies;
	p->showGridOverlay=showGridOverlay;

	p->replaceFile=replaceFile;
	p->replaceMode=replaceMode;
	p->replaceTolerance=replaceTolerance;
	p->replaceMass=replaceMass;

	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;

	p->vectorParams=vectorParams;
	p->scalarParams=scalarParams;

	p->ionSourceEnabled=ionSourceEnabled;
	p->ionTargetEnabled=ionTargetEnabled;
	p->ionNumeratorEnabled=ionNumeratorEnabled;
	p->ionDenominatorEnabled=ionDenominatorEnabled;
	
	return p;
}

size_t SpatialAnalysisFilter::numBytesForCache(size_t nObjects) const
{
	return nObjects*IONDATA_SIZE;
}

void SpatialAnalysisFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	//Check for range file parent
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
		{
			const RangeStreamData *r;
			r = (const RangeStreamData *)dataIn[ui];

			if(WANT_RANGE_PROPAGATION[algorithm])
				dataOut.push_back(dataIn[ui]);

			bool different=false;
			if(!haveRangeParent)
			{
				//well, things have changed, we didn't have a 
				//range parent before.
				different=true;
			}
			else
			{
				//OK, last time we had a range parent. Check to see 
				//if the ion names are the same. If they are, keep the 
				//current bools, iff the ion names are all the same
				unsigned int numEnabled=std::count(r->enabledIons.begin(),
							r->enabledIons.end(),1);
				if(ionNames.size() == numEnabled)
				{
					unsigned int pos=0;
					for(unsigned int uj=0;uj<r->rangeFile->getNumIons();uj++)
					{
						//Only look at parent-enabled ranges
						if(r->enabledIons[uj])
						{
							if(r->rangeFile->getName(uj) != ionNames[pos])
							{
								different=true;
								break;
							}
							pos++;
			
						}
					}
				}
				else
					different=true;
			}
			haveRangeParent=true;

			if(different)
			{
				//OK, its different. we will have to re-assign,
				//but only allow the ranges enabled in the parent filter
				ionNames.clear();
				ionNames.reserve(r->rangeFile->getNumRanges());
				for(unsigned int uj=0;uj<r->rangeFile->getNumIons();uj++)
				{

					if(r->enabledIons[uj])
						ionNames.push_back(r->rangeFile->getName(uj));
				}

				ionSourceEnabled.resize(ionNames.size(),true);
				ionTargetEnabled.resize(ionNames.size(),true);
				
				ionNumeratorEnabled.resize(ionNames.size(),true);
				ionDenominatorEnabled.resize(ionNames.size(),true);
			}

			return;
		}
	}
	haveRangeParent=false;
}


void SpatialAnalysisFilter::createDevice(vector<const FilterStreamData *> &getOut) 
{
	//Create the user interaction device required for the user
	// to interact with the algorithm parameters 
	SelectionDevice *s=0;
	DrawStreamData *d= new DrawStreamData;
	d->parent=this;
	d->cached=0;

	switch(algorithm)
	{
		case ALGORITHM_AXIAL_DF:
			createCylinder(d,s);
		break;
		default:	
			;
	}

	if(s)
	{
		devices.push_back(s);
		getOut.push_back(d);
	}
	else
		delete d;
}

unsigned int SpatialAnalysisFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		size_t mask=STREAM_TYPE_IONS;
		if(!WANT_RANGE_PROPAGATION[algorithm])
			mask|=STREAM_TYPE_RANGE;


		//create selection device for this algrithm
		createDevice(getOut);

		//Propagate input streams as desired 
		propagateStreams(dataIn,getOut,mask,true);
	
		//Propagate cached objects
		propagateCache(getOut);
		return 0;
	}

	//Set K3D tree abort pointer and progress
	K3DTree::setAbortFlag(Filter::wantAbort);
	K3DTree::setProgressPtr(&progress.filterProgress);

	K3DTreeMk2::setAbortFlag(Filter::wantAbort);
	K3DTreeMk2::setProgressPtr(&progress.filterProgress);

	//Find out how much total size we need in points vector
	size_t totalDataSize=numElements(dataIn,STREAM_TYPE_IONS);

	//Nothing to do, but propagate inputs
	if(!totalDataSize)
	{
		propagateStreams(dataIn,getOut,getRefreshBlockMask());
		return 0;
	}

	const RangeFile *rngF=0;
	if(haveRangeParent)
	{
		//Check we actually have something to do
		if(!std::count(ionSourceEnabled.begin(),
					ionSourceEnabled.end(),true))
			return 0;
		if(!std::count(ionTargetEnabled.begin(),
					ionTargetEnabled.end(),true))
			return 0;

		rngF=getRangeFile(dataIn);
	}

	
	size_t result;
	
	//Run the algorithm
	switch(algorithm)
	{
		case ALGORITHM_DENSITY:
			result=algorithmDensity(progress,totalDataSize,
					dataIn,getOut);
			break;
		case ALGORITHM_RDF:
			result=algorithmRDF(progress,totalDataSize,
					dataIn,getOut,rngF);
			break;
		case ALGORITHM_DENSITY_FILTER:
			result=algorithmDensityFilter(progress,totalDataSize,
					dataIn,getOut);
			break;
		case ALGORITHM_AXIAL_DF:
			result=algorithmAxialDf(progress,totalDataSize,
					dataIn,getOut,rngF);
			break;
		case ALGORITHM_BINOMIAL:
		{
			if(!rngF)
				return ERR_NO_RANGE;
			
			result=algorithmBinomial(progress,totalDataSize,
						dataIn,getOut,rngF);
			break;
		}
		case ALGORITHM_REPLACE:
			result=algorithmReplace(progress,totalDataSize,
						dataIn,getOut);
			break;
		case ALGORITHM_LOCAL_CONCENTRATION:
			if(!rngF)
				return ERR_NO_RANGE;
			result=algorithmLocalConcentration(progress,totalDataSize,
						dataIn,getOut,rngF);
			break;
		default:
			ASSERT(false);
	}
	
	return result;
}

size_t SpatialAnalysisFilter::algorithmReplace(ProgressData &progress, size_t totalDataSize, 
			const vector<const FilterStreamData *>  &dataIn, 
			vector<const FilterStreamData * > &getOut)
{
	progress.maxStep=4;

	progress.step=1;
	progress.stepName=TRANS("Collate");
	progress.filterProgress=0;

	//Merge the ions form the incoming streams
	vector<IonHit> inIons;
	Filter::collateIons(dataIn,inIons,progress,totalDataSize);
	
	progress.step=2;
	progress.stepName=TRANS("Load");
	progress.filterProgress=0;

	vector<IonHit> fileIons;
	const unsigned int loadPositions[] = {
						0,1,2,3};

	//Load the other dataset
	unsigned int errCode=GenericLoadFloatFile(4,4,loadPositions,
			fileIons,replaceFile.c_str(),progress.filterProgress,*Filter::wantAbort);

	if(errCode)
		return ERR_FILE_READ_FAIL;



	progress.step=3;
	progress.stepName=TRANS("Build");
	progress.filterProgress=0;

	//Build the search tree we will use to perform replacement
	K3DTreeMk2 tree;
	tree.resetPts(fileIons,false);
	if(!tree.build())
		return ERR_ABORT_FAIL;
	BoundCube b;
	tree.getBoundCube(b);

	//map the offset of the nearest to
	//the tree ID 
	vector<size_t > nearestVec;
	nearestVec.resize(inIons.size());

	//TODO: pair vector might be faster
	// as we can use it in sequence, and can use openmp
	map<size_t,size_t> matchedMap;

	//Find the nearest point for all points in the dataset

	#pragma omp parallel for 
	for(size_t ui=0;ui<inIons.size();ui++)
	{
		nearestVec[ui]=tree.findNearestUntagged(inIons[ui].getPos(),b,false);
	}

	float sqrReplaceTol=replaceTolerance*replaceTolerance;

	//Filter this to only points that had an NN within range
	#pragma omp parallel for 
	for(size_t ui=0;ui<inIons.size();ui++)
	{
		if(nearestVec[ui]!=(size_t)-1 && inIons[ui].getPos().sqrDist(*tree.getPt(nearestVec[ui])) <=sqrReplaceTol)
		{
			#pragma omp critical
			matchedMap[ui]=tree.getOrigIndex(nearestVec[ui]);
		}
	}

	nearestVec.clear();


	progress.step=4;
	progress.stepName=TRANS("Compute");
	progress.filterProgress=0;

	//Finish if no matches
	if(matchedMap.empty())
	{
		progress.filterProgress=100;
		return 0;
	}

	vector<IonHit> outIons;
	switch(replaceMode)
	{
		case REPLACE_MODE_SUBTRACT:
		{
			//In subtraction mode, we should have
			// at least this many ions
			if(inIons.size() > matchedMap.size())
				outIons.reserve(inIons.size()-matchedMap.size());
			
			//
			#pragma omp parallel for
			for(unsigned int ui=0;ui<inIons.size();ui++)
			{
				map<size_t,size_t>::iterator it;
				it=matchedMap.find(ui);
				if(it != matchedMap.end())
					continue;

				#pragma omp critical
				outIons.push_back(inIons[ui]);
			}
			break;
		}
		case REPLACE_MODE_INTERSECT:
		{
			outIons.reserve(matchedMap.size());

			if(replaceMass)
			{
				for(map<size_t,size_t>::const_iterator it=matchedMap.begin();it!=matchedMap.end();++it)
				{
					outIons.push_back(fileIons[it->second]);
					ASSERT(fileIons[it->second].getPosRef().sqrDist(inIons[it->first].getPosRef()) < sqrReplaceTol);
				}
			}
			else
			{
				for(map<size_t,size_t>::const_iterator it=matchedMap.begin();it!=matchedMap.end();++it)
				{
					outIons.push_back(inIons[it->first]);
				}
			}
			break;
		}
		case REPLACE_MODE_UNION:
		{
			ASSERT(false);
			break;
		}
		default:
			ASSERT(false);
	}

	//Only output ions if any were found
	if(outIons.size())
	{
		IonStreamData *outData = new IonStreamData(this);

		outData->g = outData->b = outData->r = 0.5;
		outData->data.swap(outIons);
		cacheAsNeeded(outData);


		getOut.push_back(outData);
	}

	return 0;
}

void SpatialAnalysisFilter::getProperties(FilterPropGroup &propertyList) const
{
	FilterProperty p;
	size_t curGroup=0;

	string tmpStr;
	vector<pair<unsigned int,string> > choices;

	for(unsigned int ui=0;ui<ALGORITHM_ENUM_END;ui++)
	{
		tmpStr=TRANS(SPATIAL_ALGORITHMS[ui]);
		choices.push_back(make_pair(ui,tmpStr));
	}	
	
	tmpStr= choiceString(choices,algorithm);
	p.name=TRANS("Algorithm");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Spatial analysis algorithm to use");
	p.key=KEY_ALGORITHM;
	propertyList.addProperty(p,curGroup);
	choices.clear();

	propertyList.setGroupTitle(curGroup,TRANS("Algorithm"));
	curGroup++;
	
	//Get the options for the current algorithm
	//---

	//common options between several algorithms
	if(algorithm ==  ALGORITHM_RDF
		||  algorithm == ALGORITHM_DENSITY 
		|| algorithm == ALGORITHM_DENSITY_FILTER 
		|| algorithm == ALGORITHM_AXIAL_DF
		|| algorithm == ALGORITHM_LOCAL_CONCENTRATION)
	{
		tmpStr=TRANS(STOP_MODES[STOP_MODE_NEIGHBOUR]);

		choices.push_back(make_pair((unsigned int)STOP_MODE_NEIGHBOUR,tmpStr));
		tmpStr=TRANS(STOP_MODES[STOP_MODE_RADIUS]);
		choices.push_back(make_pair((unsigned int)STOP_MODE_RADIUS,tmpStr));
		tmpStr= choiceString(choices,stopMode);
		p.name=TRANS("Stop Mode");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_CHOICE;
		p.helpText=TRANS("Method to use to terminate algorithm when examining each point");
		p.key=KEY_STOPMODE;
		propertyList.addProperty(p,curGroup);

		if(stopMode == STOP_MODE_NEIGHBOUR)
		{
			stream_cast(tmpStr,nnMax);
			p.name=TRANS("NN Max");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_INTEGER;
			p.helpText=TRANS("Maximum number of neighbours to examine");
			p.key=KEY_NNMAX;
			propertyList.addProperty(p,curGroup);
			
			if(algorithm == ALGORITHM_RDF)
			{

				p.name=TRANS("Normalise bins");
				p.data=boolStrEnc(normaliseNNHist);
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Normalise counts by binwidth. Needed when comparing NN histograms against one another");
				p.key=KEY_NNMAX_NORMALISE;
				propertyList.addProperty(p,curGroup);



				p.name=TRANS("Show Random");
				p.data=boolStrEnc(wantRandomNNHist);
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Show a fitted (density matched) theoretical distribution");
				p.key=KEY_NNMAX_SHOWRANDOM;
				propertyList.addProperty(p,curGroup);



			}
		}
		else
		{
			stream_cast(tmpStr,distMax);
			p.name=TRANS("Dist Max");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Maximum distance from each point for search");
			p.key=KEY_DISTMAX;
			propertyList.addProperty(p,curGroup);
		}

		propertyList.setGroupTitle(curGroup,TRANS("Stop Mode"));
	}
	
	//Extra options for specific algorithms 
	switch(algorithm)
	{
		case ALGORITHM_RDF:
		{
			stream_cast(tmpStr,numBins);
			p.name=TRANS("Num Bins");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_INTEGER;
			p.helpText=TRANS("Number of bins for output 1D RDF plot");
			p.key=KEY_NUMBINS;
			propertyList.addProperty(p,curGroup);

			tmpStr=boolStrEnc(excludeSurface);

			p.name=TRANS("Surface Remove");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Exclude surface as part of source to minimise bias in RDF (at cost of increased noise)");
			p.key=KEY_REMOVAL;
			propertyList.addProperty(p,curGroup);
			
			if(excludeSurface)
			{
				stream_cast(tmpStr,reductionDistance);
				p.name=TRANS("Remove Dist");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_REAL;
				p.helpText=TRANS("Minimum distance to remove from surface");
				p.key=KEY_REDUCTIONDIST;
				propertyList.addProperty(p,curGroup);

			}
				
			string thisCol;
			//Convert the ion colour to a hex string	
			p.name=TRANS("Plot colour ");
			p.data=rgba.toColourRGBA().rgbaString();
			p.type=PROPERTY_TYPE_COLOUR;
			p.helpText=TRANS("Colour of output plot");
			p.key=KEY_COLOUR;
			propertyList.addProperty(p,curGroup);

			propertyList.setGroupTitle(curGroup,TRANS("Alg. Params."));
			if(haveRangeParent)
			{
				ASSERT(ionSourceEnabled.size() == ionNames.size());
				ASSERT(ionNames.size() == ionTargetEnabled.size());
				curGroup++;

				
				string sTmp;

				sTmp = boolStrEnc((size_t)std::count(ionSourceEnabled.begin(),
					ionSourceEnabled.end(),true) == ionSourceEnabled.size());

				p.name=TRANS("Source");
				p.data=sTmp;
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Ions to use for initiating RDF search");
				p.key=KEY_ENABLE_SOURCE_ALL;
				propertyList.addProperty(p,curGroup);

					
				//Loop over the possible incoming ranges,
				//once to set sources, once to set targets
				for(unsigned int ui=0;ui<ionSourceEnabled.size();ui++)
				{
					sTmp=boolStrEnc(ionSourceEnabled[ui]);
					p.name=ionNames[ui];
					p.data=sTmp;
					p.type=PROPERTY_TYPE_BOOL;
					p.helpText=TRANS("Enable/disable ion as source");
					p.key=muxKey(KEYTYPE_ENABLE_SOURCE,ui);
					propertyList.addProperty(p,curGroup);
				}
				
				propertyList.setGroupTitle(curGroup,TRANS("Source Ion"));

				curGroup++;
				
				sTmp = boolStrEnc((size_t)std::count(ionTargetEnabled.begin(),
					ionTargetEnabled.end(),true) == ionTargetEnabled.size());
				
				p.name=TRANS("Target");
				p.data=sTmp;
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Enable/disable all ions as target");
				p.key=KEY_ENABLE_TARGET_ALL;
				propertyList.addProperty(p,curGroup);
				
				//Loop over the possible incoming ranges,
				//once to set sources, once to set targets
				for(unsigned int ui=0;ui<ionTargetEnabled.size();ui++)
				{
					sTmp=boolStrEnc(ionTargetEnabled[ui]);
					p.name=ionNames[ui];
					p.data=sTmp;
					p.type=PROPERTY_TYPE_BOOL;
					p.helpText=TRANS("Enable/disable this ion as target");
					p.key=muxKey(KEYTYPE_ENABLE_TARGET,ui);
					propertyList.addProperty(p,curGroup);
				}
				propertyList.setGroupTitle(curGroup,TRANS("Target Ion"));

			}
	
			break;
		}	
		case ALGORITHM_DENSITY_FILTER:
		{
		
			stream_cast(tmpStr,densityCutoff);	
			p.name=TRANS("Cutoff");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Remove points with local density above/below this value");
			p.key=KEY_CUTOFF;
			propertyList.addProperty(p,curGroup);
			
		
			tmpStr=boolStrEnc(keepDensityUpper);
			p.name=TRANS("Retain Upper");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Retain either points with density above (enabled) or below cutoff");
			p.key=KEY_RETAIN_UPPER;
			propertyList.addProperty(p,curGroup);
			
			propertyList.setGroupTitle(curGroup,TRANS("Alg. Params."));
			break;
		}
		case ALGORITHM_DENSITY:
		{
			propertyList.setGroupTitle(curGroup,TRANS("Alg. Params."));
			break;
		}
		case ALGORITHM_AXIAL_DF:
		{
			stream_cast(tmpStr,numBins);
			p.name=TRANS("Num Bins");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_INTEGER;
			p.helpText=TRANS("Number of bins for output 1D RDF plot");
			p.key=KEY_NUMBINS;
			propertyList.addProperty(p,curGroup);
			
			
			p.name=TRANS("Plot colour ");
			p.data=rgba.toColourRGBA().rgbString(); 
			p.type=PROPERTY_TYPE_COLOUR;
			p.helpText=TRANS("Colour of output plot");
			p.key=KEY_COLOUR;
			propertyList.addProperty(p,curGroup);
			
			std::string str;
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			stream_cast(str,vectorParams[0]);
			p.key=KEY_ORIGIN;
			p.name=TRANS("Origin");
			p.data=str;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Position for centre of cylinder");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(str,vectorParams[1]);
			p.key=KEY_NORMAL;
			p.name=TRANS("Axis");
			p.data=str;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Vector between centre and end of cylinder");
			propertyList.addProperty(p,curGroup);

			
			stream_cast(str,scalarParams[0]);
			p.key=KEY_RADIUS;
			p.name=TRANS("Radius");
			p.data= str;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Radius of cylinder");
			propertyList.addProperty(p,curGroup);
	
			propertyList.setGroupTitle(curGroup,TRANS("Alg. Params."));
			break;
		}
		case ALGORITHM_BINOMIAL:
		{
			//--
			stream_cast(tmpStr,numIonsSegment);	
			p.name=TRANS("Block size");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_INTEGER;
			p.helpText=TRANS("Number of ions to use per block");
			p.key=KEY_NUMIONS;
			propertyList.addProperty(p,curGroup);
			//--
			
			//--
			stream_cast(tmpStr,maxBlockAspect);	
			p.name=TRANS("Max Block Aspect");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Maximum allowable block aspect ratio. Blocks above this aspect are discarded. Setting too high decreases correlation strength. Too low causes loss of statistical power.");
			p.key=KEY_BINOMIAL_MAX_ASPECT;
			propertyList.addProperty(p,curGroup);
			//--
			
			//--
			vector<pair<unsigned int, string> > choices;
			choices.push_back(make_pair(1,"x"));	
			choices.push_back(make_pair(2,"y"));	
			choices.push_back(make_pair(0,"z"));	
			
			p.name=TRANS("Extrusion Direction");
			p.data=choiceString(choices,extrusionDirection);
			p.type=PROPERTY_TYPE_CHOICE;
			p.helpText=TRANS("Direction in which blocks are extended during construction.");
			p.key=KEY_BINOMIAL_EXTRUDE_DIR;
			propertyList.addProperty(p,curGroup);
			//--
	
			propertyList.setGroupTitle(curGroup,TRANS("Alg. Params."));
			
			curGroup++;
	
			p.name=TRANS("Plot Counts");
			p.data=boolStrEnc(showBinomialFrequencies);
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Show the counts in the binomial histogram");
			p.key=KEY_SHOW_BINOM_FREQ;
			propertyList.addProperty(p,curGroup);

			if(showBinomialFrequencies)
			{
				p.name=TRANS("Normalise");
				p.data=boolStrEnc(showNormalisedBinomialFrequencies);
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Normalise the counts in the binomial histogram to a probability density function");
				p.key=KEY_SHOW_BINOM_NORM_FREQ;
				propertyList.addProperty(p,curGroup);

				/* TODO: IMPLEMENT ME
				p.name=TRANS("Expected Freq");
				p.data=boolStrEnc(showTheoreticFrequencies);
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Normalise the counts in the binomial histogram to a probability density function");
				p.key=KEY_SHOW_BINOM_THEOR_FREQ;
				propertyList.addProperty(p,curGroup);
				*/


				p.name=TRANS("Display Grid");
				p.data=boolStrEnc(showGridOverlay);
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText="Show the extruded grid in the 3D view. This may be slow";
				p.key=KEY_SHOW_BINOM_3D_GRID;
				propertyList.addProperty(p,curGroup);

			}

			propertyList.setGroupTitle(curGroup,TRANS("View Options"));
			break;	
		}
		case ALGORITHM_REPLACE:
		{
			tmpStr = replaceFile;
			p.name=TRANS("Data File");
			p.data=tmpStr;
			p.dataSecondary="Pos File (*.pos)|*.pos|All Files|*";
			p.type=PROPERTY_TYPE_FILE;
			p.helpText=TRANS("Pos file of points to subtract/replace/etc");
			p.key=KEY_REPLACE_FILE;
			propertyList.addProperty(p,curGroup);
		
			stream_cast(tmpStr,replaceTolerance);
			p.name=TRANS("Match Tol.");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Tolerance to allow for matching");
			p.key=KEY_REPLACE_TOLERANCE;
			propertyList.addProperty(p,curGroup);
		
	
			vector<pair<unsigned int,string> > choices;

			for(unsigned int ui=0;ui<REPLACE_MODE_ENUM_END;ui++)
			{
				tmpStr=TRANS(REPLACE_ALGORITHMS[ui]);
				choices.push_back(make_pair(ui,tmpStr));
			}	
		
			p.name=TRANS("Mode");
			p.data= choiceString(choices,replaceMode);
			p.type=PROPERTY_TYPE_CHOICE;
			p.helpText=TRANS("Replacment condition");
			p.key=KEY_REPLACE_ALGORITHM;
			propertyList.addProperty(p,curGroup);

			if(replaceMode != REPLACE_MODE_SUBTRACT)
			{
				p.name=TRANS("Replace value");
				p.data=boolStrEnc(replaceMass);
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Use value data from file when replacing ions");
				p.key=KEY_REPLACE_VALUE;
				propertyList.addProperty(p,curGroup);
			}

			propertyList.setGroupTitle(curGroup,TRANS("Replacement"));
			break;
		}
		case ALGORITHM_LOCAL_CONCENTRATION:
		{
			if(haveRangeParent)
			{
				ASSERT(ionSourceEnabled.size() == ionNames.size());
				ASSERT(ionNames.size() == ionTargetEnabled.size());
				curGroup++;

				
				string sTmp;

				sTmp = boolStrEnc((size_t)std::count(ionSourceEnabled.begin(),
					ionSourceEnabled.end(),true) == ionSourceEnabled.size());

				p.name=TRANS("Source");
				p.data=sTmp;
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Enable/disable all ions as source");
				p.key=KEY_ENABLE_SOURCE_ALL;
				propertyList.addProperty(p,curGroup);

					
				//Loop over the possible incoming ranges,
				//once to set sources, once to set targets
				for(unsigned int ui=0;ui<ionSourceEnabled.size();ui++)
				{
					sTmp=boolStrEnc(ionSourceEnabled[ui]);
					p.name=ionNames[ui];
					p.data=sTmp;
					p.type=PROPERTY_TYPE_BOOL;
					p.helpText=TRANS("Enable/disable ion as source");
					p.key=muxKey(KEYTYPE_ENABLE_SOURCE,ui);
					propertyList.addProperty(p,curGroup);
				}
				
				propertyList.setGroupTitle(curGroup,TRANS("Source Ion"));
				curGroup++;

				sTmp = boolStrEnc((size_t)std::count(ionNumeratorEnabled.begin(),
					ionNumeratorEnabled.end(),true) == ionNumeratorEnabled.size());
				p.name=TRANS("Numerator");
				p.data=sTmp;
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Ions to use as Numerator for conc. calculation");
				p.key=KEY_ENABLE_NUMERATOR_ALL;
				propertyList.addProperty(p,curGroup);

					
				//Loop over the possible incoming ranges,
				//once to set sources, once to set targets
				for(unsigned int ui=0;ui<ionNumeratorEnabled.size();ui++)
				{
					sTmp=boolStrEnc(ionNumeratorEnabled[ui]);
					p.name=ionNames[ui];
					p.data=sTmp;
					p.type=PROPERTY_TYPE_BOOL;
					p.helpText=TRANS("Enable/disable ion as source");
					p.key=muxKey(KEYTYPE_ENABLE_NUMERATOR,ui);
					propertyList.addProperty(p,curGroup);
				}
				
				propertyList.setGroupTitle(curGroup,TRANS("Numerator"));
				curGroup++;

				
				sTmp = boolStrEnc((size_t)std::count(ionTargetEnabled.begin(),
					ionTargetEnabled.end(),true) == ionTargetEnabled.size());
				
				p.name=TRANS("Denominator");
				p.data=sTmp;
				p.type=PROPERTY_TYPE_BOOL;
				p.helpText=TRANS("Enable/disable all ions as target");
				p.key=KEY_ENABLE_TARGET_ALL;
				propertyList.addProperty(p,curGroup);
				
				//Loop over the possible incoming ranges,
				//once to set sources, once to set targets
				for(unsigned int ui=0;ui<ionTargetEnabled.size();ui++)
				{
					sTmp=boolStrEnc(ionTargetEnabled[ui]);
					p.name=ionNames[ui];
					p.data=sTmp;
					p.type=PROPERTY_TYPE_BOOL;
					p.helpText=TRANS("Enable/disable this ion as target");
					p.key=muxKey(KEYTYPE_ENABLE_TARGET,ui);
					propertyList.addProperty(p,curGroup);
				}
				propertyList.setGroupTitle(curGroup,TRANS("Denominator")); 
			}
	
			break;
		}	
		default:
			ASSERT(false);
	}
	
	//---
}

bool SpatialAnalysisFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_ALGORITHM:
		{
			size_t ltmp=ALGORITHM_ENUM_END;
			for(unsigned int ui=0;ui<ALGORITHM_ENUM_END;ui++)
			{
				if(value == TRANS(SPATIAL_ALGORITHMS[ui]))
				{
					ltmp=ui;
					break;
				}
			}


			
			if(ltmp>=ALGORITHM_ENUM_END)
				return false;
		
			if(ltmp == ALGORITHM_LOCAL_CONCENTRATION &&
				nnMax < 2)
			{
				nnMax=2;
			}
	
			algorithm=ltmp;
			resetParamsAsNeeded();
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_STOPMODE:
		{
			switch(algorithm)
			{
				case ALGORITHM_DENSITY:
				case ALGORITHM_DENSITY_FILTER:
				case ALGORITHM_RDF:
				case ALGORITHM_AXIAL_DF:
				case ALGORITHM_LOCAL_CONCENTRATION:
				{
					size_t ltmp=STOP_MODE_ENUM_END;

					for(unsigned int ui=0;ui<STOP_MODE_ENUM_END;ui++)
					{
						if(value == TRANS(STOP_MODES[ui]))
						{
							ltmp=ui;
							break;
						}
					}
					
					if(ltmp>=STOP_MODE_ENUM_END)
						return false;
					
					stopMode=ltmp;
					needUpdate=true;
					clearCache();
					break;
				}
				default:
					//Should know what algorithm we use.
					ASSERT(false);
				break;
			}
			break;
		}	
		case KEY_DISTMAX:
		{
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp<= 0.0)
				return false;
			
			distMax=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_NNMAX:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
		
			//NNmax should be nonzero at all times. For local concentration
			// should be at least 2 (as 1 == 100% all the time)	
			if(ltmp==0 || (algorithm == ALGORITHM_LOCAL_CONCENTRATION  && ltmp < 2))
				return false;
			
			nnMax=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_NNMAX_NORMALISE:
		{
			if(!applyPropertyNow(normaliseNNHist,value,needUpdate))
				return false;
			break;
		}	
		case KEY_NNMAX_SHOWRANDOM:
		{
			if(!applyPropertyNow(wantRandomNNHist,value,needUpdate))
				return false;
			break;
		}	
		case KEY_NUMBINS:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp==0)
				return false;
			
			numBins=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}
		case KEY_REDUCTIONDIST:
		{
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp<= 0.0)
				return false;
			
			reductionDistance=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}	
		case KEY_REMOVAL:
		{
			if(!applyPropertyNow(excludeSurface,value,needUpdate))
				return false;
			break;
		}
		case KEY_COLOUR:
		{
			ColourRGBA tmpRgba;

			if(!tmpRgba.parse(value))
				return false;

			
			if(rgba.toColourRGBA() != tmpRgba)
			{
				rgba=tmpRgba.toRGBAf();

				if(cacheOK)
				{
					for(size_t ui=0;ui<filterOutputs.size();ui++)
					{
						if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_PLOT)
						{
							PlotStreamData *p;
							p =(PlotStreamData*)filterOutputs[ui];

							p->r=rgba.r();
							p->g=rgba.g();
							p->b=rgba.b();
						}
					}

				}

				needUpdate=true;
			}


			break;
		}
		case KEY_ENABLE_SOURCE_ALL:
		{
			ASSERT(haveRangeParent);
			bool allEnabled=true;
			for(unsigned int ui=0;ui<ionSourceEnabled.size();ui++)
			{
				if(!ionSourceEnabled[ui])
				{
					allEnabled=false;
					break;
				}
			}

			//Invert the result and assign
			allEnabled=!allEnabled;
			for(unsigned int ui=0;ui<ionSourceEnabled.size();ui++)
				ionSourceEnabled[ui]=allEnabled;

			needUpdate=true;
			clearCache();
			break;
		}
		case KEY_ENABLE_TARGET_ALL:
		{
			ASSERT(haveRangeParent);
			bool allEnabled=true;
			for(unsigned int ui=0;ui<ionNames.size();ui++)
			{
				if(!ionTargetEnabled[ui])
				{
					allEnabled=false;
					break;
				}
			}

			//Invert the result and assign
			allEnabled=!allEnabled;
			for(unsigned int ui=0;ui<ionNames.size();ui++)
				ionTargetEnabled[ui]=allEnabled;

			needUpdate=true;
			clearCache();
			break;
		}
		case KEY_ENABLE_NUMERATOR_ALL:
		{
			ASSERT(haveRangeParent);
			bool allEnabled=true;
			for(unsigned int ui=0;ui<ionNumeratorEnabled.size();ui++)
			{
				if(!ionNumeratorEnabled[ui])
				{
					allEnabled=false;
					break;
				}
			}

			//Invert the result and assign
			allEnabled=!allEnabled;
			for(unsigned int ui=0;ui<ionNumeratorEnabled.size();ui++)
				ionNumeratorEnabled[ui]=allEnabled;

			needUpdate=true;
			clearCache();
			break;
		}
		case KEY_CUTOFF:
		{
			string stripped=stripWhite(value);

			float ltmp;
			if(stream_cast(ltmp,stripped))
				return false;

			if(ltmp<= 0.0)
				return false;
		
			if(ltmp != densityCutoff)
			{	
				densityCutoff=ltmp;
				needUpdate=true;
				clearCache();
			}
			else
				needUpdate=false;
			break;
		}
		case KEY_RETAIN_UPPER:
		{
			if(!applyPropertyNow(keepDensityUpper,value,needUpdate))
				return false;
			break;
		}
		case KEY_RADIUS:
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
		case KEY_NORMAL:
		{
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

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
		case KEY_ORIGIN:
		{
			if(!applyPropertyNow(vectorParams[0],value,needUpdate))
				return false;
			return true;
		}
		case KEY_NUMIONS:
		{
			unsigned int ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp<=1)
				return false;
			
			numIonsSegment=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}
		case KEY_SHOW_BINOM_FREQ:
		{
			if(!applyPropertyNow(showBinomialFrequencies,value,needUpdate))
				return false;
			break;
		}
		case KEY_SHOW_BINOM_NORM_FREQ:
		{
			if(!applyPropertyNow(showNormalisedBinomialFrequencies,value,needUpdate))
				return false;
			break;
		}
		case KEY_SHOW_BINOM_THEOR_FREQ:
		{
			if(!applyPropertyNow(showTheoreticFrequencies,value,needUpdate))
				return false;
			break;
		}
		case KEY_BINOMIAL_MAX_ASPECT:
		{
			float ltmp;
			if(stream_cast(ltmp,value))
				return false;
			
			if(ltmp<=1)
				return false;
			
			maxBlockAspect=ltmp;
			needUpdate=true;
			clearCache();

			break;
		}
		case KEY_BINOMIAL_EXTRUDE_DIR:
		{
			map<string,unsigned int> choices;
			choices["x"]=0;
			choices["y"]=1;
			choices["z"]=2;

			map<string,unsigned int>::iterator it;
			it=choices.find(value);
			
			if(it == choices.end())
				return false;
			
			extrusionDirection=it->second;
			needUpdate=true;
			clearCache();
			break;
		}
		case KEY_SHOW_BINOM_3D_GRID:
		{
			if(!applyPropertyNow(showGridOverlay,value,needUpdate))
				return false;

			break;
		}
		case KEY_REPLACE_FILE:
		{
			if(!applyPropertyNow(replaceFile,value,needUpdate))
				return false;
			break;
		}
		case KEY_REPLACE_TOLERANCE:
		{
			if(!applyPropertyNow(replaceTolerance,value,needUpdate))
				return false;
			break;
		}
		case KEY_REPLACE_ALGORITHM:
		{
			size_t newVal=REPLACE_MODE_ENUM_END;
			for(size_t ui=0;ui<REPLACE_MODE_ENUM_END; ui++)
			{
				if( value == TRANS(REPLACE_ALGORITHMS[ui]))
				{
					newVal=ui;
					break;
				}
			}
			if(newVal==REPLACE_MODE_ENUM_END)
				return false;

			if(replaceMode != newVal)
			{
				needUpdate=true;
				clearCache();
				replaceMode=newVal;
			}
			break;
		}
		case KEY_REPLACE_VALUE:
		{
			if(!applyPropertyNow(replaceMass,value,needUpdate))
				return false;
			break;
			
		}
		default:
		{
			ASSERT(haveRangeParent);
			//The incoming range keys are dynamically allocated to a 
			//position beyond any reasonable key. Its a hack,
			//but it works, and is entirely contained within the filter code.
			unsigned int ionOffset,keyType;
			demuxKey(key,keyType,ionOffset);

			bool doEnable;
			if(!boolStrDec(value,doEnable))
				return false;

			vector<bool> *vBool=0;
				
			switch(keyType)
			{
				case KEYTYPE_ENABLE_SOURCE:
					vBool=&ionSourceEnabled;
					break;
				case KEYTYPE_ENABLE_TARGET:
					vBool=&ionTargetEnabled;
					break;
				case KEYTYPE_ENABLE_NUMERATOR:
					vBool=&ionNumeratorEnabled;
					break;
				case KEYTYPE_ENABLE_DENOMINATOR:
					vBool=&ionDenominatorEnabled;
					break;
				default:	
					ASSERT(false);
			}
				
			if(vBool)
			{
				bool lastVal = (*vBool)[ionOffset]; 
				if(doEnable)
					(*vBool)[ionOffset]=true;
				else
					(*vBool)[ionOffset]=false;
				
				//if the result is different, the
				//cache should be invalidated
				if(lastVal!=(*vBool)[ionOffset])
				{
					needUpdate=true;
					clearCache();
				}
			}
		}

	}	
	return true;
}

std::string  SpatialAnalysisFilter::getSpecificErrString(unsigned int code) const
{
	const char *errStrings[] = {"",
				TRANS("Spatial analysis aborted by user"),
				TRANS("Insufficient memory to complete analysis"),
				TRANS("Required range data not present"), 
				TRANS("Insufficient memory for binomial. Reduce input size?"),
				TRANS("Insufficient points to continue"),
				TRANS("Unable to load file")
				};
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(errStrings) == SPAT_ERR_END_OF_ENUM);
	
	
	ASSERT(code < SPAT_ERR_END_OF_ENUM);

	return std::string(errStrings[code]);
}

void SpatialAnalysisFilter::setUserString(const std::string &str)
{
	//Which algorithms have plot outputs?
	const bool ALGORITHM_HAS_PLOTS[] = { false,false,true,true,true,false,false};

	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(ALGORITHM_HAS_PLOTS) == ALGORITHM_ENUM_END);

	if(userString != str && ALGORITHM_HAS_PLOTS[algorithm])
	{
		userString=str;
		clearCache();
	}	
	else
		userString=str;
}

unsigned int SpatialAnalysisFilter::getRefreshBlockMask() const
{
	//Anything but ions and ranges can go through this filter.
	if(!WANT_RANGE_PROPAGATION[algorithm])
		return STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
	else
		return STREAM_TYPE_IONS;

}

unsigned int SpatialAnalysisFilter::getRefreshEmitMask() const
{
	switch(algorithm)
	{
		case ALGORITHM_RDF:
			return STREAM_TYPE_IONS | STREAM_TYPE_PLOT;
		case ALGORITHM_BINOMIAL:
			return STREAM_TYPE_PLOT | STREAM_TYPE_DRAW;
		case ALGORITHM_AXIAL_DF:
			return STREAM_TYPE_IONS | STREAM_TYPE_PLOT | STREAM_TYPE_DRAW;
		default:
			return STREAM_TYPE_IONS;
	}
}

unsigned int SpatialAnalysisFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_IONS;
}

bool SpatialAnalysisFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;
			f << tabs(depth+1) << "<algorithm value=\""<<algorithm<< "\"/>"  << endl;
			f << tabs(depth+1) << "<stopmode value=\""<<stopMode<< "\"/>"  << endl;
			f << tabs(depth+1) << "<nnmax value=\""<<nnMax<< "\"/>"  << endl;
			f << tabs(depth+1) << "<normalisennhist value=\""<<boolStrEnc(normaliseNNHist)<< "\"/>"  << endl;
			f << tabs(depth+1) << "<wantrandomnnhist value=\""<<boolStrEnc(wantRandomNNHist)<< "\"/>"  << endl;
			f << tabs(depth+1) << "<distmax value=\""<<distMax<< "\"/>"  << endl;
			f << tabs(depth+1) << "<numbins value=\""<<numBins<< "\"/>"  << endl;
			f << tabs(depth+1) << "<excludesurface value=\""<<excludeSurface<< "\"/>"  << endl;
			f << tabs(depth+1) << "<reductiondistance value=\""<<reductionDistance<< "\"/>"  << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  rgba.r() << "\" g=\"" << rgba.g() << "\" b=\"" <<rgba.b()
				<< "\" a=\"" << rgba.a() << "\"/>" <<endl;
			
			f << tabs(depth+1) << "<densitycutoff value=\""<<densityCutoff<< "\"/>"  << endl;
			f << tabs(depth+1) << "<keepdensityupper value=\""<<(int)keepDensityUpper<< "\"/>"  << endl;
			
			f << tabs(depth+1) << "<replace file=\""<<escapeXML(convertFileStringToCanonical(replaceFile)) << "\" mode=\"" << replaceMode 
				<< "\" tolerance=\"" << replaceTolerance <<  "\" replacemass=\"" << boolStrEnc(replaceMass) << "\" />"  << endl;


			//-- Binomial parameters ---
			f << tabs(depth+1) << "<binomial numions=\""<<numIonsSegment<< "\" maxblockaspect=\"" 
						<< maxBlockAspect << "\" extrusiondirection=\"" 
						<< extrusionDirection << "\"/>"  << endl;
			f << tabs(depth+1) << "<binomialdisplay freqs=\""<<(int)showBinomialFrequencies
						<< "\" normalisedfreqs=\"" << (int)showNormalisedBinomialFrequencies
						<< "\" theoreticfreqs=\""<< (int)showTheoreticFrequencies
						<< "\" gridoverlay=\""<< (int)showGridOverlay 
						<< "\"/>" << endl;

			//--------------------------

			
			writeVectorsXML(f,"vectorparams",vectorParams,depth+1);
			writeScalarsXML(f,"scalarparams",scalarParams,depth+1);
		
			if(ionNames.size())	
			{
				writeIonsEnabledXML(f,"source",ionSourceEnabled,ionNames,depth+1);
				writeIonsEnabledXML(f,"target",ionTargetEnabled,ionNames,depth+1);
				writeIonsEnabledXML(f,"numerator",ionNumeratorEnabled,ionNames,depth+1);
				writeIonsEnabledXML(f,"denominator",ionDenominatorEnabled,ionNames,depth+1);
			}

			f << tabs(depth) << "</" << trueName() << ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}


void SpatialAnalysisFilter::getStateOverrides(std::vector<string> &externalAttribs) const 
{
	externalAttribs.push_back(replaceFile);

}

bool SpatialAnalysisFilter::writePackageState(std::ostream &f, unsigned int format,
			const std::vector<std::string> &valueOverrides, unsigned int depth) const
{
	ASSERT(valueOverrides.size() == 1);

	//Temporarily modify the state of the filter, then call writestate
	string tmpReplaceFile=replaceFile;


	//override const and self-modify
	// this is quite naughty, but we know what we are doing...
	const_cast<SpatialAnalysisFilter *>(this)->replaceFile=valueOverrides[0];
	bool result;
	result=writeState(f,format,depth);

	//restore the filter state, such that the caller doesn't notice that this has been modified
	const_cast<SpatialAnalysisFilter *>(this)->replaceFile=tmpReplaceFile;

	return result;
}

bool SpatialAnalysisFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;

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

	//Retrieve algorithm
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,algorithm,"algorithm","value"))
		return false;
	if(algorithm >=ALGORITHM_ENUM_END)
		return false;
	//===
	
	//Retrieve stop mode 
	//===
	if(!XMLGetNextElemAttrib(nodePtr,stopMode,"stopmode","value"))
		return false;
	if(stopMode >=STOP_MODE_ENUM_END)
		return false;
	//===
	
	//Retrieve nnMax val
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,nnMax,"nnmax","value"))
		return false;
	if(!nnMax)
		return false;
	//===
	
	//Retrieve histogram normalisation 
	//TODO: COMPAT : did not exist prior to 0.0.17
	// internal 5033191f0c61
	//====== 
	xmlNodePtr tmpNode = nodePtr;
	if(!XMLGetNextElemAttrib(tmpNode,normaliseNNHist,"normalisennhist","value"))
	{
		normaliseNNHist=false;
	}
	//===
	
	//Retrieve histogram normalisation 
	//TODO: COMPAT : did not exist prior to 0.0.18
	// internal revision : 2302dbbfb3dd 
	//====== 
	tmpNode = nodePtr;
	if(!XMLGetNextElemAttrib(tmpNode,wantRandomNNHist,"wantrandomnnhist","value"))
	{
		wantRandomNNHist=false;
	}
	//===
	
	//Retrieve distMax val
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,distMax,"distmax","value"))
		return false;
	if(distMax <=0.0)
		return false;
	//===
	
	//Retrieve numBins val
	//====== 
	if(!XMLGetNextElemAttrib(nodePtr,numBins,"numbins","value"))
		return false;
	if(!numBins)
		return false;
	//===
	
	//Retrieve exclude surface on/off
	//===
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"excludesurface","value"))
		return false;
	//check that new value makes sense 
	if(!boolStrDec(tmpStr,excludeSurface))
		return false;
	//===
	

	//Get reduction distance
	//===
	if(!XMLGetNextElemAttrib(nodePtr,reductionDistance,"reductiondistance","value"))
		return false;
	if(reductionDistance < 0.0f)
		return false;
	//===

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	ColourRGBAf tmpRgbaf;
	if(!parseXMLColour(nodePtr,tmpRgbaf))
		return false;
	rgba=tmpRgbaf;
	//====


	//Retrieve density cutoff & upper 
	if(!XMLGetNextElemAttrib(nodePtr,densityCutoff,"densitycutoff","value"))
		return false;
	if(densityCutoff< 0.0f)
		return false;

	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"keepdensityupper","value"))
		return false;
	//check that new value makes sense 
	if(!boolStrDec(tmpStr,keepDensityUpper))
		return false;


	//FIXME:COMPAT_BREAK : 3Depict <= internal fb7d66397b7b does not contain
	tmpNode=nodePtr;
	if(!XMLHelpFwdToElem(nodePtr,"replace"))
	{
		if(XMLHelpGetProp(replaceFile,nodePtr,"file"))
			return false;
	
		if(XMLHelpGetProp(replaceMode,nodePtr,"mode"))
			return false;
		
		if(replaceMode>REPLACE_MODE_ENUM_END)
			return false;
		
		if(XMLHelpGetProp(replaceTolerance,nodePtr,"tolerance"))
			return false;
		if(replaceTolerance < 0)
			return false;
	}
	else
		nodePtr=tmpNode;

	//FIXME:COMPAT_BREAK : 3Depict <= 1796:5639f6d50732 does not contain
	// this section

	tmpNode=nodePtr;
	if(!XMLHelpFwdToElem(nodePtr,"binomial"))
	{
		unsigned int nSegment;
		float maxAspect;

		//Retrieve segmentation count
		if(!XMLGetAttrib(nodePtr,nSegment,"numions"))
			return false;
		if(nSegment <= 1)
			return false;
		numIonsSegment=nSegment;


		//Retrieve and verify aspect ratio
		if(!XMLGetAttrib(nodePtr,maxAspect,"maxblockaspect"))
			return false;

		if(maxAspect<1.0f)
			return false;
		maxBlockAspect=maxAspect;

		//Get the extrusion direction
		unsigned int tmpExtr;
		if(!XMLGetAttrib(nodePtr,tmpExtr,"extrusiondirection"))
			return false;

		if(tmpExtr >=3)
			return false;
		extrusionDirection=tmpExtr;


		//Spin to binomial display
		if(XMLHelpFwdToElem(nodePtr,"binomialdisplay"))
			return false;


		if(!XMLGetAttrib(nodePtr,tmpStr,"freqs"))
			return false;

		if(!boolStrDec(tmpStr,showBinomialFrequencies))
			return false;
		
		if(!XMLGetAttrib(nodePtr,tmpStr,"normalisedfreqs"))
			return false;
		
		if(!boolStrDec(tmpStr,showNormalisedBinomialFrequencies))
			return false;
		
		if(!XMLGetAttrib(nodePtr,tmpStr,"theoreticfreqs"))
			return false;
		
		if(!boolStrDec(tmpStr,showTheoreticFrequencies))
			return false;


	}
	else
		nodePtr=tmpNode;


	//FIXME: COMPAT_BREAK : Earlier versions of the state file <= 1441:adaa3a3daa80
	// do not contain this section, so we must be fault tolerant
	// when we bin backwards compatability, do this one too.

	tmpNode=nodePtr;
	if(!XMLHelpFwdToElem(nodePtr,"scalarparams"))
		readScalarsXML(nodePtr,scalarParams);
	else
		nodePtr=tmpNode;

	if(!XMLHelpFwdToElem(nodePtr,"vectorparams"))
		readVectorsXML(nodePtr,vectorParams);
	else
		nodePtr=tmpNode;

	//FIXME: Remap the ion names  we load from the file to the ion names that we 
	// see in the rangefile

	vector<string> ionNames;
	if(!XMLHelpFwdToElem(nodePtr,"source"))
		readIonsEnabledXML(nodePtr,ionSourceEnabled,ionNames);
	nodePtr=tmpNode;
	if(!XMLHelpFwdToElem(nodePtr,"target"))
		readIonsEnabledXML(nodePtr,ionTargetEnabled,ionNames);

	nodePtr=tmpNode;
	if(!XMLHelpFwdToElem(nodePtr,"numerator"))
		readIonsEnabledXML(nodePtr,ionNumeratorEnabled,ionNames);
	
	nodePtr=tmpNode;
	if(!XMLHelpFwdToElem(nodePtr,"denominator"))
		readIonsEnabledXML(nodePtr,ionDenominatorEnabled,ionNames);

	resetParamsAsNeeded();
	
	return true;
}

void SpatialAnalysisFilter::setPropFromBinding(const SelectionBinding &b)
{
	
	switch(b.getID())
	{
		case BINDING_CYLINDER_RADIUS:
			b.getValue(scalarParams[0]);
			break;
		case BINDING_CYLINDER_DIRECTION:
		{
			Point3D p;
			b.getValue(p);
			if(p.sqrMag() > sqrtf(std::numeric_limits<float>::epsilon()))
				vectorParams[1]=p;
			break;
		}
		case BINDING_CYLINDER_ORIGIN:
			b.getValue(vectorParams[0]);
			break;
		default:
			ASSERT(false);
	}

	clearCache();
}

void SpatialAnalysisFilter::resetParamsAsNeeded()
{
	//Perform any needed
	// transformations to internal vars
	switch(algorithm)
	{
		case ALGORITHM_AXIAL_DF:
		{
			if(vectorParams.size() !=2)
			{
				size_t oldSize=vectorParams.size();
				vectorParams.resize(2);

				if(oldSize== 0)
					vectorParams[0]=Point3D(0,0,0);
				if(oldSize < 2)
					vectorParams[1]=Point3D(0,0,1);
			}

			if(scalarParams.size() !=1)
			{
				size_t oldSize=scalarParams.size();
				scalarParams.resize(1);
				if(!oldSize)
					scalarParams[0]=DEFAULT_AXIAL_DISTANCE;
			}
			break;
		}
		default:
			//fall through
		;
	}
}

void SpatialAnalysisFilter::filterSelectedRanges(const vector<IonHit> &ions, bool sourceFilter, const RangeFile *rngF,
			vector<IonHit> &output) const
{
	if(sourceFilter)
		rngF->rangeByIon(ions,ionSourceEnabled,output);
	else
		rngF->rangeByIon(ions,ionTargetEnabled,output);
}

//FIXME: Move to filter common
//Scan input datastreams to build a single point vector,
// which is a copy of the input points
//Returns 0 on no error, otherwise nonzero
size_t buildMonolithicPoints(const vector<const FilterStreamData *> &dataIn,
				ProgressData &progress, size_t totalDataSize,
				vector<Point3D> &p)
{
	//Build monolithic point set
	//---
	p.resize(totalDataSize);

	size_t dataSize=0;

	progress.filterProgress=0;
	if(*Filter::wantAbort)
		return FILTER_ERR_ABORT;

	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[ui]);

				if(extendDataVector(p,d->data,	progress.filterProgress,
						dataSize))
					return ERR_ABORT_FAIL;

				dataSize+=d->data.size();
			}
			break;	
			default:
				break;
		}
	}
	//---

	return 0;
}
			
size_t SpatialAnalysisFilter::algorithmRDF(ProgressData &progress, size_t totalDataSize, 
		const vector<const FilterStreamData *>  &dataIn, 
		vector<const FilterStreamData * > &getOut,const RangeFile *rngF)
{
	progress.step=1;
	progress.stepName=TRANS("Collate");
	progress.filterProgress=0;
	if(excludeSurface)
		progress.maxStep=4;
	else
		progress.maxStep=3;
	
	if(*Filter::wantAbort)
		return FILTER_ERR_ABORT;

	K3DTree kdTree;
	
	//Source points
	vector<Point3D> p;
	bool needSplitting;

	needSplitting=false;
	//We only need to split up the data if we have to 
	if((size_t)std::count(ionSourceEnabled.begin(),ionSourceEnabled.end(),true)!=ionSourceEnabled.size()
		|| (size_t)std::count(ionTargetEnabled.begin(),ionTargetEnabled.end(),true)!=ionTargetEnabled.size() )
		needSplitting=true;

	if(haveRangeParent && needSplitting)
	{
		vector<Point3D> pts[2];
		ASSERT(ionNames.size());
		size_t errCode;
		if((errCode=buildSplitPoints(dataIn,progress,totalDataSize,
				rngF,ionSourceEnabled, ionTargetEnabled,pts[0],pts[1])))
			return errCode;

		progress.step=2;
		progress.stepName=TRANS("Build");

		//Build the tree using the target ions
		//(its roughly nlogn timing, but worst case n^2)
		kdTree.buildByRef(pts[1]);
		if(*Filter::wantAbort)
			return FILTER_ERR_ABORT;
		pts[1].clear();
		
		//Remove surface points from sources if desired
		if(excludeSurface)
		{
			ASSERT(reductionDistance > 0);
			progress.step++;
			progress.stepName=TRANS("Surface");

			if(*Filter::wantAbort)
				return FILTER_ERR_ABORT;


			//Take the input points, then use them
			//to compute the convex hull reduced 
			//volume. 
			vector<Point3D> returnPoints;
			errCode=GetReducedHullPts(pts[0],reductionDistance,
					&progress.filterProgress,*(Filter::wantAbort),
					returnPoints);
			if(errCode ==1)
				return INSUFFICIENT_SIZE_ERR;
			else if(errCode)
			{
				ASSERT(false);
				return ERR_ABORT_FAIL;
			}
			
			if(*Filter::wantAbort)
				return FILTER_ERR_ABORT;

			pts[0].clear();
			//Forget the original points, and use the new ones
			p.swap(returnPoints);
		}
		else
			p.swap(pts[0]);

	}
	else
	{
		size_t errCode;
		if((errCode=buildMonolithicPoints(dataIn,progress,totalDataSize,p)))
			return errCode;
		
		progress.step=2;
		progress.stepName=TRANS("Build");
		BoundCube treeDomain;
		treeDomain.setBounds(p);

		//Build the tree (its roughly nlogn timing, but worst case n^2)
		kdTree.buildByRef(p);
		if(*Filter::wantAbort)
			return FILTER_ERR_ABORT;

		//Remove surface points if desired
		if(excludeSurface)
		{
			ASSERT(reductionDistance > 0);
			progress.step++;
			progress.stepName=TRANS("Surface");
		
			if(*Filter::wantAbort)
				return FILTER_ERR_ABORT;


			//Take the input points, then use them
			//to compute the convex hull reduced 
			//volume. 
			vector<Point3D> returnPoints;
			size_t errCode;
			if((errCode=GetReducedHullPts(p,reductionDistance,
					&progress.filterProgress, *Filter::wantAbort,
					returnPoints)) )
			{
				if(errCode ==1)
					return INSUFFICIENT_SIZE_ERR;
				else if(errCode ==2)
					return ERR_ABORT_FAIL;
				else
				{
					ASSERT(false);
					return ERR_ABORT_FAIL;
				}
			}



			//Forget the original points, and use the new ones
			p.swap(returnPoints);
			
			if(*Filter::wantAbort)
				return FILTER_ERR_ABORT;

		}
		
	}

	//Let us perform the desired analysis
	progress.step++;
	progress.stepName=TRANS("Analyse");

	//If there is no data, there is nothing to do.
	if(p.empty() || !kdTree.nodeCount())
		return	0;
	
	//OK, at this point, the KD tree contains the target points
	//of interest, and the vector "p" contains the source points
	//of interest, whatever they might be.
	switch(stopMode)
	{
		case STOP_MODE_NEIGHBOUR:
		{
			//User is after an NN histogram analysis

			//Histogram is output as a per-NN histogram of frequency.
			vector<vector<size_t> > histogram;
			
			//Bin widths for the NN histograms (each NN hist
			//is scaled separately). The +1 is due to the tail bin
			//being the totals
			float *binWidth = new float[nnMax];


			unsigned int errCode;
			//Run the analysis
			errCode=generateNNHist(p,kdTree,nnMax,
					numBins,histogram,binWidth,
					&(progress.filterProgress),*Filter::wantAbort);
			switch(errCode)
			{
				case 0:
					break;
				case RDF_ERR_INSUFFICIENT_INPUT_POINTS:
				{
					delete[] binWidth;
					return INSUFFICIENT_SIZE_ERR;
				}
				case RDF_ABORT_FAIL:
				{
					delete[] binWidth;
					return ERR_ABORT_FAIL;
				}
				default:
					ASSERT(false);
			}

		
			vector<vector<float> > histogramFloat;
			histogramFloat.resize(nnMax); 
			//Normalise the NN histograms to a per bin width as required
			for(unsigned int ui=0;ui<nnMax; ui++)
			{
				histogramFloat[ui].resize(numBins);
				if(normaliseNNHist)
				{
					for(unsigned int uj=0;uj<numBins;uj++)
						histogramFloat[ui][uj] = (float)histogram[ui][uj]/binWidth[ui] ;
				}
				else
				{
					for(unsigned int uj=0;uj<numBins;uj++)
						histogramFloat[ui][uj] = (float)histogram[ui][uj];
				}
			}
			histogram.clear();

	
			//Alright then, we have the histogram in x-{y1,y2,y3...y_n} form
			//lets make some plots shall we?
			{
			PlotStreamData *plotData[nnMax];

			for(unsigned int ui=0;ui<nnMax;ui++)
			{
				plotData[ui] = new PlotStreamData;
				plotData[ui]->index=ui;
				plotData[ui]->parent=this;
				plotData[ui]->plotMode=PLOT_MODE_1D;
				plotData[ui]->xLabel=TRANS("Radial Distance");
				if(normaliseNNHist)
					plotData[ui]->yLabel=TRANS("Count/Distance");
				else
					plotData[ui]->yLabel=TRANS("Count");
				std::string tmpStr;
				stream_cast(tmpStr,ui+1);
				plotData[ui]->dataLabel=getUserString() + string(" ") +tmpStr + TRANS("NN Freq.");

				//Red plot.
				plotData[ui]->r=rgba.r();
				plotData[ui]->g=rgba.g();
				plotData[ui]->b=rgba.b();
				plotData[ui]->xyData.resize(numBins);

				for(unsigned int uj=0;uj<numBins;uj++)
				{
					float dist;
					ASSERT(ui < histogramFloat.size() && uj<histogramFloat[ui].size());
					dist = (float)uj*binWidth[ui];
					plotData[ui]->xyData[uj] = std::make_pair(dist,
							histogramFloat[ui][uj]);
				}

				cacheAsNeeded(plotData[ui]);
				
				getOut.push_back(plotData[ui]);
			}
			}

			//If requested, add a probability distribution.
			// we need to scale it to match the displayed observed
			// histogram
			if(wantRandomNNHist)
			{
				vector<vector<float> > nnTheoHist;
				nnTheoHist.resize(nnMax);
				#pragma omp parallel for
				for(unsigned int ui=0;ui<nnMax;ui++)
				{

					float total=0;
					for(unsigned int uj=0;uj<numBins;uj++)
						total+=histogramFloat[ui][uj]*binWidth[ui];


					//Generate the eval points
					vector<float> evalDist;
					evalDist.resize(numBins);	
					for(unsigned int uj=0;uj<numBins;uj++)
						evalDist[uj] = (float)uj*binWidth[ui];

					//Compute the random Knn density parameter from the histogram
					//equation is from L. Stephenson PhD Thesis, Eq7.8, pp91, 2009,
					// Univ. Sydney.
					//--
					// gamma(3/2+1)^(1/3)
					const float GAMMA_FACTOR = 1.09954261650577;
					const float SQRT_PI = 1.77245385090552;
					

					float mean=weightedMean(evalDist,histogramFloat[ui]);
					
					float densNumerator, densDenom;
					densNumerator= gsl_sf_gamma( (ui+1) + 1.0/3.0);
					densNumerator*=GAMMA_FACTOR;
					densDenom=mean*SQRT_PI*gsl_sf_fact(ui);
					float density;
					density=densNumerator/densDenom;
					density*=density*density; //Cubed

					//--
					//create the distribution
					generateKnnTheoreticalDist(evalDist,density, ui+1,nnTheoHist[ui]);

					//scale the dist
					for(size_t uj=0;uj<nnTheoHist[ui].size();uj++)
					{
						nnTheoHist[ui][uj]*= total;
					}

				}

				PlotStreamData *plotData[nnMax];
				for(unsigned int ui=0;ui<nnMax;ui++)
				{
					plotData[ui] = new PlotStreamData;
					plotData[ui]->index=ui+nnMax;
					plotData[ui]->parent=this;
					plotData[ui]->plotMode=PLOT_MODE_1D;
					plotData[ui]->xLabel=TRANS("Radial Distance");

//					plotData[ui]->lineStyle=LINE_STYLE_DASH;

					if(normaliseNNHist)
						plotData[ui]->yLabel=TRANS("Count/Distance");
					else
						plotData[ui]->yLabel=TRANS("Count");
					std::string tmpStr;
					stream_cast(tmpStr,ui+1);
					plotData[ui]->dataLabel=getUserString() + string(" Random ") +tmpStr + TRANS("NN Freq.");

					//Red plot.
					plotData[ui]->r=rgba.r();
					plotData[ui]->g=rgba.g();
					plotData[ui]->b=rgba.b();
					plotData[ui]->xyData.resize(numBins);

					for(unsigned int uj=0;uj<numBins;uj++)
					{
						float dist;
						ASSERT(ui < histogramFloat.size() && uj<histogramFloat[ui].size());
						dist = (float)uj*binWidth[ui];
						plotData[ui]->xyData[uj] = std::make_pair(dist,
								nnTheoHist[ui][uj]);
					}

					cacheAsNeeded(plotData[ui]);
					
					getOut.push_back(plotData[ui]);
				}
			}
			delete[] binWidth;
			break;
		}
		case STOP_MODE_RADIUS:
		{
			unsigned int warnBiasCount=0;
			
			//Histogram is output as a histogram of frequency vs distance
			unsigned int *histogram = new unsigned int[numBins];
			for(unsigned int ui=0;ui<numBins;ui++)
				histogram[ui] =0;
		
			//User is after an RDF analysis. Run it.
			unsigned int errcode;
			errcode=generateDistHist(p,kdTree,histogram,distMax,numBins,
					warnBiasCount,&(progress.filterProgress),*(Filter::wantAbort));

			if(errcode)
				return ERR_ABORT_FAIL;

			if(warnBiasCount)
			{
				string sizeStr;
				stream_cast(sizeStr,warnBiasCount);
			
				consoleOutput.push_back(std::string(TRANS("Warning, "))
						+ sizeStr + TRANS(" points were unable to find neighbour points that exceeded the search radius, and thus terminated prematurely"));
			}

			PlotStreamData *plotData = new PlotStreamData;

			plotData->plotMode=PLOT_MODE_1D;
			plotData->index=0;
			plotData->parent=this;
			plotData->xLabel=TRANS("Radial Distance");
			plotData->yLabel=TRANS("Count");
			plotData->dataLabel=getUserString() + TRANS(" RDF");

			plotData->r=rgba.r();
			plotData->g=rgba.g();
			plotData->b=rgba.b();
			plotData->xyData.resize(numBins);

			for(unsigned int uj=0;uj<numBins;uj++)
			{
				float dist;
				dist = (float)uj/(float)numBins*distMax;
				plotData->xyData[uj] = std::make_pair(dist,
						histogram[uj]);
			}

			delete[] histogram;
			
			cacheAsNeeded(plotData);
			
			getOut.push_back(plotData);

			//Propagate non-ion/range data
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				switch(dataIn[ui]->getStreamType())
				{
					case STREAM_TYPE_IONS:
					case STREAM_TYPE_RANGE: 
						//Do not propagate ranges, or ions
					break;
					default:
						getOut.push_back(dataIn[ui]);
						break;
				}
				
			}

			break;
		}
		default:
			ASSERT(false);
	}

	return 0;
}

size_t SpatialAnalysisFilter::algorithmDensity(ProgressData &progress, 
	size_t totalDataSize, const vector<const FilterStreamData *>  &dataIn, 
		vector<const FilterStreamData * > &getOut)
{
	vector<Point3D> p;
	size_t errCode;
	progress.step=1;
	progress.stepName=TRANS("Collate");
	progress.maxStep=3;
	if((errCode=buildMonolithicPoints(dataIn,progress,totalDataSize,p)))
		return errCode;

	progress.step=2;
	progress.stepName=TRANS("Build");
	progress.filterProgress=0;
	if(*Filter::wantAbort)
		return FILTER_ERR_ABORT;

	BoundCube treeDomain;
	treeDomain.setBounds(p);

	//Build the tree (its roughly nlogn timing, but worst case n^2)
	K3DTree kdTree;
	kdTree.buildByRef(p);


	if(*Filter::wantAbort)
		return FILTER_ERR_ABORT;

	p.clear(); //We don't need pts any more, as tree *is* a copy.

	//Its algorithm time!
	//----
	//Update progress stuff
	size_t n=0;
	progress.step=3;
	progress.stepName=TRANS("Analyse");
	progress.filterProgress=0;
	if(*Filter::wantAbort)
		return FILTER_ERR_ABORT;

	//List of points for which there was a failure
	//first entry is the point Id, second is the 
	//dataset id.
	std::list<std::pair<size_t,size_t> > badPts;
	for(size_t ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[ui]);
				IonStreamData *newD = new IonStreamData;
				newD->parent=this;

				//Adjust this number to provide more update than usual, because we
				//are not doing an o(1) task between updates; yes, it is a hack
				unsigned int curProg=NUM_CALLBACK/(10*nnMax);
				newD->data.resize(d->data.size());
				if(stopMode == STOP_MODE_NEIGHBOUR)
				{
					bool spin=false;
					#pragma omp parallel for shared(spin)
					for(size_t uj=0;uj<d->data.size();uj++)
					{
						if(spin)
							continue;
						Point3D r;
						vector<const Point3D *> res;
						r=d->data[uj].getPosRef();
						
						//Assign the mass to charge using nn density estimates
						kdTree.findKNearest(r,treeDomain,nnMax,res);

						if(res.size())
						{	
							float maxSqrRad;

							//Get the radius as the furthest object
							maxSqrRad= (res[res.size()-1]->sqrDist(r));

							//Set the mass as the volume of sphere * the number of NN
							newD->data[uj].setMassToCharge(res.size()/(4.0/3.0*M_PI*powf(maxSqrRad,3.0/2.0)));
							//Keep original position
							newD->data[uj].setPos(r);
						}
						else
						{
							#pragma omp critical
							badPts.push_back(make_pair(uj,ui));
						}

						res.clear();
						
						//Update progress as needed
						if(!curProg--)
						{
							#pragma omp critical 
							{
							n+=NUM_CALLBACK/(nnMax);
							progress.filterProgress= (unsigned int)(((float)n/(float)totalDataSize)*100.0f);
							if(*Filter::wantAbort)
								spin=true;
							curProg=NUM_CALLBACK/(nnMax);
							}
						}
					}

					if(spin)
					{
						delete newD;
						return ERR_ABORT_FAIL;
					}


				}
				else if(stopMode == STOP_MODE_RADIUS)
				{
#ifdef _OPENMP
					bool spin=false;
#endif
					float maxSqrRad = distMax*distMax;
					float vol = 4.0/3.0*M_PI*maxSqrRad*distMax; //Sphere volume=4/3 Pi R^3
					#pragma omp parallel for shared(spin) firstprivate(treeDomain,curProg)
					for(size_t uj=0;uj<d->data.size();uj++)
					{
						Point3D r;
						const Point3D *res;
						float deadDistSqr;
						unsigned int numInRad;
#ifdef _OPENMP
						if(spin)
							continue;
#endif	
						r=d->data[uj].getPosRef();
						numInRad=0;
						deadDistSqr=0;

						//Assign the mass to charge using nn density estimates
						//TODO: Use multi-neareast search algorithm?
						do
						{
							res=kdTree.findNearest(r,treeDomain,deadDistSqr);

							//Check to see if we found something
							if(!res)
							{
#pragma omp critical
								badPts.push_back(make_pair(uj, ui));
								break;
							}
							
							if(res->sqrDist(r) >maxSqrRad)
								break;
							numInRad++;
							//Advance ever so slightly beyond the next ion
							deadDistSqr = res->sqrDist(r)+std::numeric_limits<float>::epsilon();
							//Update progress as needed
							if(!curProg--)
							{
#pragma omp critical
								{
								progress.filterProgress= (unsigned int)((float)n/(float)totalDataSize*100.0f);
								if(*Filter::wantAbort)
								{
#ifdef _OPENMP
									spin=true;
#else
									delete newD;
									return ERR_ABORT_FAIL;
#endif
								}
								}
#ifdef _OPENMP
								if(spin)
									break;
#endif
								curProg=NUM_CALLBACK/(10*nnMax);
							}
						}while(true);
						
						n++;
						//Set the mass as the volume of sphere * the number of NN
						newD->data[uj].setMassToCharge(numInRad/vol);
						//Keep original position
						newD->data[uj].setPos(r);
						
					}

#ifdef _OPENMP
					if(spin)
					{
						delete newD;
						return ERR_ABORT_FAIL;
					}
#endif
				}
				else
				{
					//Should not get here.
					ASSERT(false);
				}


				//move any bad points from the array to the end, then drop them
				//To do this, we have to reverse sort the array, then
				//swap the output ion vector entries with the end,
				//then do a resize.
				ComparePairFirst cmp;
				badPts.sort(cmp);
				badPts.reverse();

				//Do some swappage
				size_t pos=1;
				for(std::list<std::pair<size_t,size_t> >::iterator it=badPts.begin(); it!=badPts.end();++it)
				{
					newD->data[(*it).first]=newD->data[newD->data.size()-pos];
				}

				//Trim the tail of bad points, leaving only good points
				newD->data.resize(newD->data.size()-badPts.size());


				if(newD->data.size())
				{
					//Use default colours
					newD->r=d->r;
					newD->g=d->g;
					newD->b=d->b;
					newD->a=d->a;
					newD->ionSize=d->ionSize;
					newD->valueType=TRANS("Number Density (\\#/Vol^3)");

					//Cache result as neede
					cacheAsNeeded(newD);
					getOut.push_back(newD);
				}
				else
					delete newD;
			}
			break;	
			case STREAM_TYPE_RANGE: 
			break;
			default:
				getOut.push_back(dataIn[ui]);
				break;
		}
	}

	progress.filterProgress=100;

	//If we have bad points, let the user know.
	if(!badPts.empty())
	{
		std::string sizeStr;
		stream_cast(sizeStr,badPts.size());
		consoleOutput.push_back(std::string(TRANS("Warning,")) + sizeStr + 
				TRANS(" points were un-analysable. These have been dropped"));

		//Print out a list of points if we can

		size_t maxPrintoutSize=std::min(badPts.size(),(size_t)200);
		list<pair<size_t,size_t> >::iterator it;
		it=badPts.begin();
		while(maxPrintoutSize--)
		{
			std::string s;
			const IonStreamData *d;
			d=((const IonStreamData *)dataIn[it->second]);

			Point3D getPos;
			getPos=	d->data[it->first].getPosRef();
			stream_cast(s,getPos);
			consoleOutput.push_back(s);
			++it;
		}

		if(badPts.size() > 200)
		{
			consoleOutput.push_back(TRANS("And so on..."));
		}


	}	
	
	return 0;
}

size_t SpatialAnalysisFilter::algorithmDensityFilter(ProgressData &progress, 
		size_t totalDataSize, const vector<const FilterStreamData *>  &dataIn, 
		vector<const FilterStreamData * > &getOut)
{
	vector<Point3D> p;
	size_t errCode;
	progress.step=1;
	progress.stepName=TRANS("Collate");
	progress.maxStep=3;
	if((errCode=buildMonolithicPoints(dataIn,progress,totalDataSize,p)))
		return errCode;

	progress.step=2;
	progress.stepName=TRANS("Build");
	progress.filterProgress=0;
	if(*Filter::wantAbort)
		return FILTER_ERR_ABORT;

	BoundCube treeDomain;
	treeDomain.setBounds(p);

	//Build the tree (its roughly nlogn timing, but worst case n^2)
	K3DTree kdTree;
	kdTree.buildByRef(p);


	//Update progress 
	if(*Filter::wantAbort)
		return FILTER_ERR_ABORT;
	p.clear(); //We don't need pts any more, as tree *is* a copy.


	//Its algorithm time!
	//----
	//Update progress stuff
	size_t n=0;
	progress.step=3;
	progress.stepName=TRANS("Analyse");
	progress.filterProgress=0;
	if(*Filter::wantAbort)
		return FILTER_ERR_ABORT;

	//List of points for which there was a failure
	//first entry is the point Id, second is the 
	//dataset id.
	std::list<std::pair<size_t,size_t> > badPts;
	for(size_t ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[ui]);
				IonStreamData *newD = new IonStreamData;
				newD->parent=this;

				//Adjust this number to provide more update thanusual, because we
				//are not doing an o(1) task between updates; yes, it is a hack
				unsigned int curProg=NUM_CALLBACK/(10*nnMax);
				newD->data.reserve(d->data.size());
				if(stopMode == STOP_MODE_NEIGHBOUR)
				{
					bool spin=false;
					#pragma omp parallel for shared(spin)
					for(size_t uj=0;uj<d->data.size();uj++)
					{
						if(spin)
							continue;
						Point3D r;
						vector<const Point3D *> res;
						r=d->data[uj].getPosRef();
						
						//Assign the mass to charge using nn density estimates
						kdTree.findKNearest(r,treeDomain,nnMax,res);

						if(res.size())
						{	
							float maxSqrRad;

							//Get the radius as the furthest object
							maxSqrRad= (res[res.size()-1]->sqrDist(r));


							float density;
							density = res.size()/(4.0/3.0*M_PI*powf(maxSqrRad,3.0/2.0));

							if(xorFunc((density <=densityCutoff), keepDensityUpper))
							{
#pragma omp critical
								newD->data.push_back(d->data[uj]);
							}

						}
						else
						{
							#pragma omp critical
							badPts.push_back(make_pair(uj,ui));
						}

						res.clear();
						
						//Update progress as needed
						if(!curProg--)
						{
							#pragma omp critical 
							{
							n+=NUM_CALLBACK/(nnMax);
							progress.filterProgress= (unsigned int)(((float)n/(float)totalDataSize)*100.0f);
							if(*Filter::wantAbort)
								spin=true;
							curProg=NUM_CALLBACK/(nnMax);
							}
						}
					}

					if(spin)
					{
						delete newD;
						return ERR_ABORT_FAIL;
					}


				}
				else if(stopMode == STOP_MODE_RADIUS)
				{
#ifdef _OPENMP
					bool spin=false;
#endif
					float maxSqrRad = distMax*distMax;
					float vol = 4.0/3.0*M_PI*maxSqrRad*distMax; //Sphere volume=4/3 Pi R^3
					#pragma omp parallel for shared(spin) firstprivate(treeDomain,curProg)
					for(size_t uj=0;uj<d->data.size();uj++)
					{
						Point3D r;
						const Point3D *res;
						float deadDistSqr;
						unsigned int numInRad;
#ifdef _OPENMP
						if(spin)
							continue;
#endif	
						r=d->data[uj].getPosRef();
						numInRad=0;
						deadDistSqr=0;

						//Assign the mass to charge using nn density estimates
						do
						{
							res=kdTree.findNearest(r,treeDomain,deadDistSqr);

							//Check to see if we found something
							if(!res)
							{
#pragma omp critical
								badPts.push_back(make_pair(uj, ui));
								break;
							}
							
							if(res->sqrDist(r) >maxSqrRad)
								break;
							numInRad++;
							//Advance ever so slightly beyond the next ion
							deadDistSqr = res->sqrDist(r)+std::numeric_limits<float>::epsilon();
							//Update progress as needed
							if(!curProg--)
							{
#pragma omp critical
								{
								progress.filterProgress= (unsigned int)((float)n/(float)totalDataSize*100.0f);
								if(*Filter::wantAbort)
								{
#ifdef _OPENMP
									spin=true;
#else
									delete newD;
									return ERR_ABORT_FAIL;
#endif
								}
								}
#ifdef _OPENMP
								if(spin)
									break;
#endif
								curProg=NUM_CALLBACK/(10*nnMax);
							}
						}while(true);
						
						n++;
						float density;
						density = numInRad/vol;

						if(xorFunc((density <=densityCutoff), keepDensityUpper))
						{
#pragma omp critical
							newD->data.push_back(d->data[uj]);
						}
						
					}

#ifdef _OPENMP
					if(spin)
					{
						delete newD;
						return ERR_ABORT_FAIL;
					}
#endif
				}
				else
				{
					//Should not get here.
					ASSERT(false);
				}


				//move any bad points from the array to the end, then drop them
				//To do this, we have to reverse sort the array, then
				//swap the output ion vector entries with the end,
				//then do a resize.
				ComparePairFirst cmp;
				badPts.sort(cmp);
				badPts.reverse();

				//Do some swappage
				size_t pos=1;
				for(std::list<std::pair<size_t,size_t> >::iterator it=badPts.begin(); it!=badPts.end();++it)
				{
					newD->data[(*it).first]=newD->data[newD->data.size()-pos];
				}

				//Trim the tail of bad points, leaving only good points
				newD->data.resize(newD->data.size()-badPts.size());


				if(newD->data.size())
				{
					//Use default colours
					newD->r=d->r;
					newD->g=d->g;
					newD->b=d->b;
					newD->a=d->a;
					newD->ionSize=d->ionSize;
					newD->valueType=TRANS("Number Density (\\#/Vol^3)");

					//Cache result as needed
					cacheAsNeeded(newD);
					getOut.push_back(newD);
				}
				else
					delete newD;
			}
			break;	
			default:
				getOut.push_back(dataIn[ui]);
				break;
		}
	}
	//If we have bad points, let the user know.
	if(!badPts.empty())
	{
		std::string sizeStr;
		stream_cast(sizeStr,badPts.size());
		consoleOutput.push_back(std::string(TRANS("Warning,")) + sizeStr + 
				TRANS(" points were un-analysable. These have been dropped"));

		//Print out a list of points if we can

		size_t maxPrintoutSize=std::min(badPts.size(),(size_t)200);
		list<pair<size_t,size_t> >::iterator it;
		it=badPts.begin();
		while(maxPrintoutSize--)
		{
			std::string s;
			const IonStreamData *d;
			d=((const IonStreamData *)dataIn[it->second]);

			Point3D getPos;
			getPos=	d->data[it->first].getPosRef();
			stream_cast(s,getPos);
			consoleOutput.push_back(s);
			++it;
		}

		if(badPts.size() > 200)
		{
			consoleOutput.push_back(TRANS("And so on..."));
		}


	}	
	
	return 0;
}

void SpatialAnalysisFilter::createCylinder(DrawStreamData * &drawData,
		SelectionDevice * &s) const
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

	s = new SelectionDevice(this);
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
	b.setInteractionMode(BIND_MODE_POINT3D_ROTATE);
	s->addBinding(b);
		
	//Bind left button to changing radius
	b.setBinding(SELECT_BUTTON_LEFT,0,DRAW_CYLINDER_BIND_RADIUS,
		BINDING_CYLINDER_RADIUS,dC->getRadius(),dC);
	b.setInteractionMode(BIND_MODE_FLOAT_TRANSLATE);
	b.setFloatLimits(0,std::numeric_limits<float>::max());
	s->addBinding(b); 
	
	//=====

}

size_t SpatialAnalysisFilter::algorithmAxialDf(ProgressData &progress, 
		size_t totalDataSize, const vector<const FilterStreamData *>  &dataIn, 
		vector<const FilterStreamData * > &getOut,const RangeFile *rngF)
{
	//Need bins to perform histogram
	ASSERT(numBins);

	progress.step=1;
	progress.stepName=TRANS("Extract");
	progress.filterProgress=0;
	progress.maxStep=4;

	//Ions inside the selected cylinder,
	// which are to be used as source points for dist. function query
	vector<IonHit> ionsInside;
	{
		//Crop out a cylinder as the source data 
		CropHelper cropHelp(totalDataSize, CROP_CYLINDER_INSIDE_AXIAL,
				vectorParams,scalarParams);
		
		float minProg,maxProg;
		size_t cumulativeCount=0;		
		//Run cropping over the input datastreams
		for(size_t ui=0; ui<dataIn.size();ui++)
		{
			if( dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
			{
				const IonStreamData* d;
				d=(const IonStreamData *)dataIn[ui];
				size_t errCode;
				minProg=cumulativeCount/(float)totalDataSize;
				cumulativeCount+=d->data.size();
				maxProg=cumulativeCount/(float)totalDataSize;

				errCode=cropHelp.runFilter(d->data,ionsInside,
					minProg,maxProg,progress.filterProgress);
			
				if(errCode == ERR_CROP_INSUFFICIENT_MEM)
					return INSUFFICIENT_SIZE_ERR;
				else if(errCode)
				{
					//If we fail, abort, but we should use
					// the appropriate error code
					ASSERT(errCode == ERR_CROP_CALLBACK_FAIL);
					break;
				}
			}

		
		}
	}

	if(*Filter::wantAbort)
		return ERR_ABORT_FAIL;

	//Now, the ions outside the targeting volume may be reduced 
	vector<IonHit> ionsOutside;
	ionsOutside.resize(totalDataSize);
	//Build complete set of input data
	{
	size_t offset=0;
	
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
			continue;


		//Copy input data in its entirety into a single vector
		const IonStreamData *d;
		d=(const IonStreamData*)dataIn[ui];
		for(size_t uj=0;uj<d->data.size();uj++)
		{
			ionsOutside[offset]=d->data[uj];
			offset++;
		}

	}
	}

	progress.step=2;
	progress.stepName=TRANS("Reduce");
	progress.filterProgress=0;

	//TODO: Improve progress
	switch(stopMode)
	{
		case STOP_MODE_RADIUS:
		{
			//In this case we can pull a small trick - 
			// do an O(n) pass to crop out the data we want, before doing the O(nlogn) tree build.
			// We however must build a slightly larger cylinder to do the crop

			vector<Point3D> vP;
			vector<float> sP;
			vP=vectorParams;
			sP=scalarParams;

			//Expand radius to encapsulate cylinder contents+dMax
			sP[0]+= distMax;
			//Similarly expand axis (vp[0] is origin, vp[1] is zis)
			vP[1].extend(distMax);

			//Crop out a cylinder as the source data 
			CropHelper cropHelp(totalDataSize, CROP_CYLINDER_INSIDE_AXIAL,
					vP,sP);

			vector<IonHit> tmp;
			size_t errCode=cropHelp.runFilter(ionsOutside,tmp,0,100,progress.filterProgress);

			switch(errCode)
			{
				case 0:
					break;
				case ERR_CROP_INSUFFICIENT_MEM:
					return INSUFFICIENT_SIZE_ERR;
				case ERR_CROP_CALLBACK_FAIL:
					return ERR_ABORT_FAIL;
				default: 
					ASSERT(false);
					return ERR_ABORT_FAIL;

			}
			tmp.swap(ionsOutside);
			break;
		}
		case STOP_MODE_NEIGHBOUR:
		{
			//Nothing to do here!
			break;
		}
		default:
			ASSERT(false);
	}

	//We only need to slice down the data if required
	if(haveRangeParent)
	{

#pragma omp parallel
		{
		//For the ions "inside", we only care about the 
		// source data, not the target. Filter further using this info
		#pragma omp task
		{
		bool sourceReduce;
		sourceReduce=((size_t)std::count(ionSourceEnabled.begin(),ionSourceEnabled.end(),true)
						!=ionSourceEnabled.size());
		if(sourceReduce)
		{
			vector<IonHit> tmp;
			filterSelectedRanges(ionsInside,true,rngF,tmp);
			ionsInside.swap(tmp);
		}
		}
		
		#pragma omp task
		{
		bool targetReduce;
		targetReduce=((size_t)std::count(ionTargetEnabled.begin(),ionTargetEnabled.end(),true)
						!=ionTargetEnabled.size() );
		if(targetReduce)
		{
			vector<IonHit> tmp;
			filterSelectedRanges(ionsOutside,false,rngF,tmp);
			ionsOutside.swap(tmp);
		}
		}
		}
	}

	progress.step=3;
	progress.stepName=TRANS("Build");
	progress.filterProgress=0;
	
	//Strip away the real value information, leaving just point data
	vector<Point3D> src,dest;
	IonHit::getPoints(ionsInside,src);
	ionsInside.clear();
	
	IonHit::getPoints(ionsOutside,dest);
	ionsOutside.clear();

	K3DTree tree;

	tree.buildByRef(dest);
	if(*Filter::wantAbort)
		return FILTER_ERR_ABORT;

	progress.step=4;
	progress.stepName=TRANS("Compute");
	progress.filterProgress=0;
	unsigned int *histogram = new unsigned int[numBins];
	for(unsigned int ui=0;ui<numBins;ui++)
		histogram[ui] =0;

	float binWidth;
	//OK, so now we have two datasets that need to be analysed. Lets do it
	unsigned int errCode;

	bool histOK=false;
	switch(stopMode)
	{
		case STOP_MODE_NEIGHBOUR:
		{
			Point3D axisNormal=vectorParams[1];
			axisNormal.normalise();

			errCode=generate1DAxialNNHist(src,tree,axisNormal, histogram,
					binWidth,nnMax,numBins,&progress.filterProgress,
					*Filter::wantAbort);

			break;
		}
		case STOP_MODE_RADIUS:
		{
			Point3D axisNormal=vectorParams[1];
			axisNormal.normalise();

			errCode=generate1DAxialDistHist(src,tree,axisNormal, histogram,
					distMax,numBins,&progress.filterProgress,*Filter::wantAbort);

			histOK = (errCode ==0);
			break;
		}
		default:
			ASSERT(false);
	}
	
	//Remap the underlying function code to that for this function
	switch(errCode)
	{
		case 0:
			histOK=true;
			break;
		case RDF_ERR_INSUFFICIENT_INPUT_POINTS:
			consoleOutput.push_back(TRANS("Insufficient points to complete analysis"));
			errCode=0;
			break;
		case RDF_ABORT_FAIL:
			errCode=ERR_ABORT_FAIL;
			break;
		default:
			ASSERT(false);
	}

	if(errCode)
	{
		delete[] histogram;
		return errCode;
	}

	if(histOK)
	{
		PlotStreamData *plotData = new PlotStreamData;

		plotData->plotMode=PLOT_MODE_1D;
		plotData->index=0;
		plotData->parent=this;
		plotData->xLabel=TRANS("Axial Distance");
		plotData->yLabel=TRANS("Count");
		plotData->dataLabel=getUserString() + TRANS(" 1D Dist. Func.");

		plotData->r=rgba.r();
		plotData->g=rgba.g();
		plotData->b=rgba.b();
		plotData->xyData.resize(numBins);

		for(unsigned int uj=0;uj<numBins;uj++)
		{
			float dist;
			switch(stopMode)
			{
				case STOP_MODE_RADIUS:
					dist = ((float)uj - (float)numBins/2.0f)/(float)numBins*distMax*2.0;
					break;
				case STOP_MODE_NEIGHBOUR:
					dist= (float)uj*binWidth;
					break;
				default:
					ASSERT(false);
			}
			plotData->xyData[uj] = std::make_pair(dist,
					histogram[uj]);
		}

		cacheAsNeeded(plotData);
		getOut.push_back(plotData);
	}

	delete[] histogram;

	//Propagate non-ion/range data
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS:
			case STREAM_TYPE_RANGE: 
				//Do not propagate ranges, or ions
			break;
			default:
				getOut.push_back(dataIn[ui]);
				break;
		}
		
	}

	//create the selection device for this algorithm
	createDevice(getOut);	

	return 0;
}

size_t SpatialAnalysisFilter::algorithmBinomial(ProgressData &progress, 
		size_t totalDataSize, const vector<const FilterStreamData *>  &dataIn, 
		vector<const FilterStreamData * > &getOut,const RangeFile *rngF)
{
	vector<IonHit> ions;

	progress.step=1;
	progress.stepName=TRANS("Collate");
	progress.filterProgress=0;
	progress.maxStep=2;

	//Merge the ions form the incoming streams
	Filter::collateIons(dataIn,ions,progress,totalDataSize);

	//Tell user we are on next step
	progress.step++;
	progress.stepName=TRANS("Binomial");
	progress.filterProgress=0;

	size_t errCode;

	SEGMENT_OPTION segmentOpts;

	segmentOpts.nIons=numIonsSegment;
	segmentOpts.strategy=BINOMIAL_SEGMENT_AUTO_BRICK;
	segmentOpts.extrusionDirection=extrusionDirection;
	segmentOpts.extrudeMaxRatio=maxBlockAspect;

	vector<GRID_ENTRY> gridEntries;

	vector<size_t> selectedIons;

	for(size_t ui=0;ui<ionSourceEnabled.size();ui++)
	{
		if(ionSourceEnabled[ui])
			selectedIons.push_back(ui);
	}


	errCode=countBinnedIons(ions,rngF,selectedIons,segmentOpts,gridEntries);

	switch(errCode)
	{
		case 0:
			break;
		case BINOMIAL_NO_MEM:
			return ERR_BINOMIAL_NO_MEM;
		default:
			ASSERT(false);
			return SPAT_ERR_END_OF_ENUM;
	}


	//If the user wants to see the overlaid grid, show this
	if(showGridOverlay)
	{
		DrawStreamData *draw=new DrawStreamData;
		draw->parent=this;

		for(size_t ui=0;ui<gridEntries.size();ui++)
		{
			DrawRectPrism *dR = new DrawRectPrism;

			dR->setAxisAligned(gridEntries[ui].startPt,
						gridEntries[ui].endPt);
			dR->setColour(0.0f,1.0f,0.0f,1.0f);
			dR->setLineWidth(2);

			draw->drawables.push_back(dR);
		}
		
		draw->cached=1;
		filterOutputs.push_back(draw);

		getOut.push_back(draw);
	}


	//Vector of ion frequencies in histogram of segment counts, 
	// each element in vector is for each ion type
	BINOMIAL_HIST binHist;
	genBinomialHistogram(gridEntries,selectedIons.size(),binHist);

	//If the histogram is empty, we cannot do any more
	if(!gridEntries.size())
		return ERR_BINOMIAL_BIN_FAIL;

	BINOMIAL_STATS binStats;
	computeBinomialStats(gridEntries,binHist,selectedIons.size(),binStats);

	//Show binomial statistics
	consoleOutput.push_back(" ------ Binomial statistics ------");
	string tmpStr;
	stream_cast(tmpStr,gridEntries.size());
	consoleOutput.push_back(string("Block count:\t") + tmpStr);
	consoleOutput.push_back("Name\t\tMean\t\tChiSquare\t\tP_rand\t\tmu");
	for(size_t ui=0;ui<binStats.mean.size();ui++)
	{
		string lineStr;
		lineStr=rngF->getName(selectedIons[ui]) + std::string("\t\t");

		if(!binStats.pValueOK[ui])
		{
			lineStr+="\t\t Not computable ";
			consoleOutput.push_back(lineStr);
			continue;
		}
		
		stream_cast(tmpStr,binStats.mean[ui]);
		lineStr+=tmpStr + string("\t\t");

		stream_cast(tmpStr,binStats.chiSquare[ui]);
		lineStr+=tmpStr + string("\t\t");

		stream_cast(tmpStr,binStats.pValue[ui]);
		lineStr+=tmpStr + string("\t\t");

		stream_cast(tmpStr,binStats.comparisonCoeff[ui]);
		lineStr+=tmpStr ;

		consoleOutput.push_back(lineStr);

	}
	consoleOutput.push_back(" ---------------------------------");


	ASSERT(binHist.mapIonFrequencies.size() ==
			binHist.normalisedFrequencies.size());

	if(!showBinomialFrequencies)
		return 0;


	for(size_t ui=0;ui<binHist.mapIonFrequencies.size(); ui++)
	{
		if(binHist.mapIonFrequencies[ui].empty())
			continue;

		//Create a plot for this range
		PlotStreamData* plt;
		plt = new PlotStreamData;
		plt->index=ui;
		plt->parent=this;
		plt->plotMode=PLOT_MODE_1D;
		plt->plotStyle=PLOT_LINE_STEM;
		plt->xLabel=TRANS("Block size");
		if(showNormalisedBinomialFrequencies)
			plt->yLabel=TRANS("Rel. Frequency");
		else
			plt->yLabel=TRANS("Count");

		//set the title
		string ionName;
		ionName+=rngF->getName(selectedIons[ui]);
		plt->dataLabel = string("Binomial:") + ionName;

		//Set the colour to match that of the range
		RGBf colour;	
		colour=rngF->getColour(selectedIons[ui]);
				
		plt->r=colour.red;
		plt->g=colour.green;
		plt->b=colour.blue;
		plt->xyData.resize(binHist.mapIonFrequencies[ui].size());

		size_t offset=0;
		if(showNormalisedBinomialFrequencies)
		{
			for(map<unsigned int, double>::const_iterator it=binHist.normalisedFrequencies[ui].begin();
					it!=binHist.normalisedFrequencies[ui].end();++it)
			{
				plt->xyData[offset]=std::make_pair(it->first,it->second);
				offset++;
			}
		}
		else
		{
			for(map<unsigned int, unsigned int >::const_iterator it=binHist.mapIonFrequencies[ui].begin();
					it!=binHist.mapIonFrequencies[ui].end();++it)
			{
				plt->xyData[offset]=std::make_pair(it->first,it->second);
				offset++;
			}
		}

		cacheAsNeeded(plt);
		getOut.push_back(plt);

	}

	if(!showTheoreticFrequencies)
		return 0;
	for(size_t ui=0;ui<binHist.theoreticNormalisedFrequencies.size(); ui++)
	{
		if(binHist.theoreticFrequencies[ui].empty())
			continue;

		//Create a plot for this range
		PlotStreamData* plt;
		plt = new PlotStreamData;
		plt->index=ui + binHist.mapIonFrequencies.size();
		plt->parent=this;
		plt->plotMode=PLOT_MODE_1D;
		plt->plotStyle=PLOT_LINE_STEM;
		plt->xLabel=TRANS("Block size");
		if(showNormalisedBinomialFrequencies)
			plt->yLabel=TRANS("Rel. Frequency");
		else
			plt->yLabel=TRANS("Count");

		//set the title
		string ionName;
		ionName+=rngF->getName(selectedIons[ui]);
		plt->dataLabel = string("Binomial (theory):") + ionName;

		//Set the colour to match that of the range
		RGBf colour;	
		colour=rngF->getColour(selectedIons[ui]);
				
		plt->r=colour.red;
		plt->g=colour.green;
		plt->b=colour.blue;
		plt->xyData.resize(binHist.theoreticFrequencies[ui].size());

		size_t offset=0;
		if(showNormalisedBinomialFrequencies)
		{
			for(map<unsigned int, double>::const_iterator it=binHist.theoreticNormalisedFrequencies[ui].begin();
					it!=binHist.theoreticNormalisedFrequencies[ui].end();++it)
			{
				plt->xyData[offset]=std::make_pair(it->first,it->second);
				offset++;
			}
		}
		else
		{
			for(map<unsigned int, double >::const_iterator it=binHist.theoreticFrequencies[ui].begin();
					it!=binHist.theoreticFrequencies[ui].end();++it)
			{
				plt->xyData[offset]=std::make_pair(it->first,it->second);
				offset++;
			}
		}

		cacheAsNeeded(plt);

		getOut.push_back(plt);

	}
	
	return 0;
}


size_t SpatialAnalysisFilter::algorithmLocalConcentration(ProgressData &progress, 
		size_t totalDataSize, const vector<const FilterStreamData *>  &dataIn, 
		vector<const FilterStreamData * > &getOut,const RangeFile *rngF)
{


	vector<IonHit> pSource;

#ifdef _OPENMP
	bool spin=false;	
#endif
	if(stopMode == STOP_MODE_RADIUS)
	{
		vector<Point3D> numeratorPts,denominatorPts;

		progress.step=1;
		progress.stepName=TRANS("Collate");
		progress.filterProgress=0;
		progress.maxStep=4;

		//Build the numerator and denominator points
		unsigned int errCode;
		errCode = buildSplitPoints(dataIn, progress, totalDataSize, rngF, 
				ionNumeratorEnabled,ionDenominatorEnabled,numeratorPts,denominatorPts);
		if(errCode)
			return errCode;	

		if(*Filter::wantAbort)
			return ERR_ABORT_FAIL;
		progress.step=2;
		progress.stepName = TRANS("Build Numerator");
		progress.filterProgress=0;


		//Build the tree (its roughly nlogn timing, but worst case n^2)
		K3DTreeMk2 treeNumerator,treeDenominator;
		treeNumerator.resetPts(numeratorPts);
		if(*Filter::wantAbort)
			return ERR_ABORT_FAIL;
		treeNumerator.build();
		if(*Filter::wantAbort)
			return ERR_ABORT_FAIL;

		progress.step=3;
		progress.stepName = TRANS("Build Denominator");
		progress.filterProgress=0;

		treeDenominator.resetPts(denominatorPts);
		treeDenominator.build();
		if(*Filter::wantAbort)
			return ERR_ABORT_FAIL;

		unsigned int sizeNeeded=0;
		//Count the array size that we need to store the points 
		for(unsigned int ui=0; ui<dataIn.size() ; ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
					const IonStreamData *d;
					d=((const IonStreamData *)dataIn[ui]);
					unsigned int ionID;
					ionID=getIonstreamIonID(d,rngF);

					//Check to see if we have a grouped set of ions
					if(ionID == (unsigned int)-1)
					{
						//we have ungrouped ions, so work out size individually
						for(unsigned int uj=0;uj<d->data.size();uj++)
						{
							ionID = rngF->getIonID(d->data[uj].getMassToCharge());
							if(ionID != (unsigned int)-1 && ionSourceEnabled[ionID])
								sizeNeeded++;
						}
						break;
					}
					
					if(ionSourceEnabled[ionID])
						sizeNeeded+=d->data.size();
				}
			}

		}

		pSource.resize(sizeNeeded);
		

		//Build the array of output points
		//--
		size_t curOffset=0;	
		for(unsigned int ui=0; ui<dataIn.size() ; ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
					unsigned int ionID;
					const IonStreamData *d;
					d=((const IonStreamData *)dataIn[ui]);
					ionID=getIonstreamIonID(d,rngF);

					if(ionID==(unsigned int)(-1))
					{
						//we have ungrouped ions, so work out size individually
						for(unsigned int uj=0;uj<d->data.size();uj++)
						{
							ionID = rngF->getIonID(d->data[uj].getMassToCharge());
							if(ionID != (unsigned int)-1 && ionSourceEnabled[ionID])
							{
								pSource[curOffset] = d->data[uj];
								curOffset++;
							}
						}
						break;
					}

					if(ionSourceEnabled[ionID])
					{
						std::copy(d->data.begin(),d->data.end(),pSource.begin()+curOffset);
						curOffset+=d->data.size();
					}

					break;
				}
				default:
					break;
			}

			if(*Filter::wantAbort)
				return false;
		}

		ASSERT(curOffset == pSource.size());
		//--

		progress.step=4;
		progress.stepName = TRANS("Compute");
		progress.filterProgress=0;

		//Loop through the array, and perform local search on each tree
#pragma omp parallel for schedule(dynamic)
		for(unsigned int ui=0;ui<pSource.size(); ui++)
		{
#ifdef _OPENMP
			if(spin)
				continue;
#endif

			vector<size_t> ptsNum,ptsDenom;
			//Find the points that are within the search radius
			treeNumerator.ptsInSphere(pSource[ui].getPosRef(),distMax,ptsNum);
			treeDenominator.ptsInSphere(pSource[ui].getPosRef(),distMax,ptsDenom);

			//Check to see if there is any self-matching going on. Don't allow zero-distance matches
			// as this biases the composition towards the chosen source points
			//TODO: Is there a faster way to do this? We might be able to track the original index of the point
			// that we built, and map it back to the input?
			//--
			unsigned int nCount,dCount;
			nCount=0;
			for(unsigned int uj=0;uj<ptsNum.size(); uj++)
			{
				size_t ptIdx;
				ptIdx=ptsNum[uj];
				float dist;
				dist = treeNumerator.getPtRef(ptIdx).sqrDist(pSource[ui].getPosRef());
				if(dist > DISTANCE_EPSILON)
					nCount++;
			}

			dCount=0;
			for(unsigned int uj=0;uj<ptsDenom.size(); uj++)
			{
				size_t ptIdx;
				ptIdx=ptsDenom[uj];
				float dist;
				dist = treeDenominator.getPtRef(ptIdx).sqrDist(pSource[ui].getPosRef());
				if(dist> DISTANCE_EPSILON)
					dCount++;
			}
			//--
			
			//compute concentration
			if( nCount + dCount )
				pSource[ui].setMassToCharge((float)nCount/(float)(nCount + dCount)*100.0f);
			else
				pSource[ui].setMassToCharge(-1.0f);
			

#ifdef _OPENMP 
			#pragma omp critical
			if(!omp_get_thread_num())
			{
#endif
				//let master thread do update	
				progress.filterProgress= (unsigned int)((float)ui/(float)pSource.size()*100.0f);

				if(*Filter::wantAbort)
				{
#ifndef _OPENMP
					return ERR_ABORT_FAIL;
#else
					#pragma atomic
					spin=true;
#endif			
				}
#ifdef _OPENMP
			}
#endif

			
		}
	}
	else if(stopMode == STOP_MODE_NEIGHBOUR)
	{

		//Merge the numerator and denominator ions into a single search tree
		vector<bool> enabledSearchIons;
		enabledSearchIons.resize(rngF->getNumIons());
		
		for(unsigned int ui=0;ui<enabledSearchIons.size(); ui++)	
		{
			enabledSearchIons[ui] = (ionNumeratorEnabled[ui] 
						|| ionDenominatorEnabled[ui]); 
		}	

		
		progress.step=1;
		progress.stepName=TRANS("Collate");
		progress.filterProgress=0;
		progress.maxStep=3;
	
		vector<IonHit> pTarget;

		//FIXME: This is highly memory inefficient - 
		//	we build points, then throw them awaway.
		// We should build and range at the same time
		buildSplitPoints(dataIn,progress,totalDataSize,rngF,
					ionSourceEnabled,enabledSearchIons,
						pSource, pTarget);
		if(*Filter::wantAbort)
			return ERR_ABORT_FAIL;

		if(pTarget.size() < nnMax)
			return INSUFFICIENT_SIZE_ERR;

		progress.step=2;
		progress.stepName=TRANS("Build");
		progress.filterProgress=0;

		//Keep a copy of the mass to charge data
		vector<float> dataMasses;
		dataMasses.resize(pTarget.size());
		#pragma omp parallel for
		for(unsigned int ui=0;ui<pTarget.size();ui++)
			dataMasses[ui]=pTarget[ui].getMassToCharge();

		K3DTreeMk2 searchTree;
		searchTree.resetPts(pTarget);
		searchTree.build();
		if(*Filter::wantAbort)
			return ERR_ABORT_FAIL;

		progress.step=3;
		progress.stepName=TRANS("Compute");
		progress.filterProgress=0;


		//Loop through the array, and perform local search on each tree
		BoundCube bc;
		searchTree.getBoundCube(bc);

#pragma omp parallel for schedule(dynamic) 
		for(unsigned int ui=0;ui<pSource.size(); ui++)
		{
#ifdef _OPENMP
			//If user requests abort, then do not process any more
			if(spin)
				continue;
#endif
			set<size_t> ptsFound;

			//Points from the tree we have already found. Abort if we cannot find enough NNs to satisfy search
			while(ptsFound.size()<nnMax)
			{
				size_t ptIdx;
				ptIdx=searchTree.findNearestWithSkip(pSource[ui].getPosRef(),bc,ptsFound);

				//Check that we have a valid NN
				if(ptIdx == (size_t)-1)
				{
					ptsFound.clear();
					break;
				}

				//distance between search pt and found pt
				float sqrDistance;
				sqrDistance = searchTree.getPtRef(ptIdx).sqrDist(pSource[ui].getPosRef());

				if(sqrDistance > DISTANCE_EPSILON)
					ptsFound.insert(ptIdx);
			}


			unsigned int nCount;
			unsigned int dCount;
			nCount=dCount=0;	
			//Count the number of numerator and denominator ions, using the masses we set aside earlier
			for(set<size_t>::iterator it=ptsFound.begin(); it!=ptsFound.end(); ++it)
			{
				float ionMass;
				//check that the distance is non-zero, to force no self-matching
				ionMass = dataMasses[searchTree.getOrigIndex(*it)];

				unsigned int ionID;
				ionID = rngF->getIonID(ionMass);


				//Ion can be either numerator or denominator OR BOTH.
				if(ionNumeratorEnabled[ionID])
					nCount++;
				if(ionDenominatorEnabled[ionID])
					dCount++;
			}

			//compute concentration
			pSource[ui].setMassToCharge((float)nCount/(float)(nCount + dCount)*100.0f);

#ifdef _OPENMP 
			if(!omp_get_thread_num())
			{
#endif
				//let master thread do update	
				progress.filterProgress= (unsigned int)((float)ui/(float)pSource.size()*100.0f);
				if(*Filter::wantAbort)
				{
#ifndef _OPENMP
					return ERR_ABORT_FAIL;
#else
					#pragma atomic
					spin=true;
#endif			
				}

#ifdef _OPENMP
			}
#endif

			
		}
	
	}
	else
	{
		//Should not get here...
		ASSERT(false);
		return ERR_ABORT_FAIL;
	}


#ifdef _OPENMP
	if(spin)
	{
		ASSERT(*Filter::wantAbort);
		return ERR_ABORT_FAIL;
	}
#endif
	progress.filterProgress=100;

	if(pSource.size())
	{
		IonStreamData *outData = new IonStreamData(this);
		//make a guess as to desired size/colour
		outData->estimateIonParameters(dataIn);
		//override colour to grey
		outData->g = outData->b = outData->r = 0.5;
		outData->valueType = "Relative Conc. (%)";
		outData->data.swap(pSource);
		cacheAsNeeded(outData);

		getOut.push_back(outData);
	}

	return 0;
}

#ifdef DEBUG

bool densityPairTest();
bool nnHistogramTest();
bool rdfPlotTest();
bool axialDistTest();
bool replaceTest();
bool localConcTestRadius();
bool localConcTestNN();

bool SpatialAnalysisFilter::runUnitTests()
{
	if(!densityPairTest())
		return false;

	if(!nnHistogramTest())
		return false;

	if(!rdfPlotTest())
		return false;

	if(!axialDistTest())
		return false;
	if(!replaceTest())
		return false;
	if(!localConcTestRadius())
		return false;

	if(!localConcTestNN())
		return false;
	return true;
}


bool densityPairTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;

	

	IonStreamData*d = new IonStreamData;
	IonHit h;
	h.setMassToCharge(1);

	//create two points, 1 unit apart	
	h.setPos(Point3D(0,0,0));
	d->data.push_back(h);

	h.setPos(Point3D(0,0,1));
	d->data.push_back(h);

	streamIn.push_back(d);
	//---------
	
	//Create a spatial analysis filter
	SpatialAnalysisFilter *f=new SpatialAnalysisFilter;
	f->setCaching(false);	
	//Set it to do an NN terminated density computation
	bool needUp;
	string s;
	s=TRANS(STOP_MODES[STOP_MODE_NEIGHBOUR]);
	TEST(f->setProperty(KEY_STOPMODE,s,needUp),"Set prop");
	s=TRANS(SPATIAL_ALGORITHMS[ALGORITHM_DENSITY]);
	TEST(f->setProperty(KEY_ALGORITHM,s,needUp),"Set prop");


	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"refresh OK");
	delete f;
	//Kill the input ion stream
	delete d; 
	streamIn.clear();

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");

	const IonStreamData* dOut = (const IonStreamData*)streamOut[0];

	TEST(dOut->data.size() == 2, "ion count");

	for(unsigned int ui=0;ui<2;ui++)
	{
		TEST( fabs( dOut->data[0].getMassToCharge()  - 1.0/(4.0/3.0*M_PI))
			< sqrtf(std::numeric_limits<float>::epsilon()),"NN density test");
	}	


	delete streamOut[0];

	return true;
}

bool nnHistogramTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;

	

	IonStreamData*d = new IonStreamData;
	IonHit h;
	h.setMassToCharge(1);

	//create two points, 1 unit apart	
	h.setPos(Point3D(0,0,0));
	d->data.push_back(h);

	h.setPos(Point3D(0,0,1));
	d->data.push_back(h);

	streamIn.push_back(d);
	
	//Create a spatial analysis filter
	SpatialAnalysisFilter *f=new SpatialAnalysisFilter;
	f->setCaching(false);	
	//Set it to do an NN terminated density computation
	bool needUp;
	TEST(f->setProperty(KEY_STOPMODE,
		STOP_MODES[STOP_MODE_NEIGHBOUR],needUp),"set stop mode");
	TEST(f->setProperty(KEY_ALGORITHM,
			SPATIAL_ALGORITHMS[ALGORITHM_RDF],needUp),"set Algorithm");
	TEST(f->setProperty(KEY_NNMAX,"1",needUp),"Set NNmax");
	
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"refresh OK");
	delete f;

	streamIn.clear();

	TEST(streamOut.size() == 2,"stream count");
	delete streamOut[1]; //wont use this
	
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_PLOT,"plot outputting");
	const PlotStreamData* dPlot=(const PlotStreamData *)streamOut[0];


	float fMax=0;
	for(size_t ui=0;ui<dPlot->xyData.size();ui++)
	{
		fMax=std::max(fMax,dPlot->xyData[ui].second);
	}

	TEST(fMax > 0 , "plot has nonzero contents");
	//Kill the input ion stream
	delete d; 

	delete dPlot;

	return true;
}

bool rdfPlotTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;

	IonStreamData*d = new IonStreamData;
	IonHit h;
	h.setMassToCharge(1);

	//create two points, 1 unit apart	
	h.setPos(Point3D(0,0,0));
	d->data.push_back(h);

	h.setPos(Point3D(0,0,1));
	d->data.push_back(h);

	streamIn.push_back(d);
	
	//Create a spatial analysis filter
	SpatialAnalysisFilter *f=new SpatialAnalysisFilter;
	f->setCaching(false);	
	//Set it to do an NN terminated density computation
	bool needUp;
	TEST(f->setProperty(KEY_STOPMODE,
		TRANS(STOP_MODES[STOP_MODE_RADIUS]),needUp),"set stop mode");
	TEST(f->setProperty(KEY_ALGORITHM,
			TRANS(SPATIAL_ALGORITHMS[ALGORITHM_RDF]),needUp),"set Algorithm");
	TEST(f->setProperty(KEY_DISTMAX,"2",needUp),"Set NNmax");
	
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"refresh OK");
	delete f;


	streamIn.clear();

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_PLOT,"plot outputting");
	const PlotStreamData* dPlot=(const PlotStreamData *)streamOut[0];


	float fMax=0;
	for(size_t ui=0;ui<dPlot->xyData.size();ui++)
	{
		fMax=std::max(fMax,dPlot->xyData[ui].second);
	}

	TEST(fMax > 0 , "plot has nonzero contents");


	//kill output data
	delete dPlot;

	//Kill the input ion stream
	delete d; 

	return true;
}

bool axialDistTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;


	//Create some input data
	//--
	IonStreamData*d = new IonStreamData;
	IonHit h;
	h.setMassToCharge(1);

	//create two points along axis
	h.setPos(Point3D(0,0,0));
	d->data.push_back(h);

	h.setPos(Point3D(0.5,0.5,0.5));
	d->data.push_back(h);

	streamIn.push_back(d);
	//--
	
	
	//Create a spatial analysis filter
	SpatialAnalysisFilter *f=new SpatialAnalysisFilter;
	f->setCaching(false);	

	//Set it to do an axial-dist calculation, 
	// - NN mode termination,
	// - Axial mode
	// - /0 origin, /1 direction, r=1
	// 
	//---
	bool needUp;
	string s;
	s=TRANS(SPATIAL_ALGORITHMS[ALGORITHM_AXIAL_DF]);
	TEST(f->setProperty(KEY_ALGORITHM,s,needUp),"Set prop (algorithm)");
	
	s=TRANS(STOP_MODES[STOP_MODE_NEIGHBOUR]);
	TEST(f->setProperty(KEY_STOPMODE,s,needUp),"Set prop (stopmode)");
	
	Point3D originPt(0,0,0), axisPt(1.1,1.1,1.1);
	float radiusCyl;
	radiusCyl = 1.0f;


	stream_cast(s,originPt);
	TEST(f->setProperty(KEY_ORIGIN,s,needUp),"Set prop (origin)");
	
	stream_cast(s,axisPt);
	TEST(f->setProperty(KEY_NORMAL,s,needUp),"Set prop (axis)");

	stream_cast(s,radiusCyl);
	TEST(f->setProperty(KEY_RADIUS,s,needUp),"Set prop (radius)");
	
	TEST(f->setProperty(KEY_REMOVAL,"0",needUp),"Set prop (disable surface removal)");
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Checking refresh code");
	delete f;
	//Kill the input ion stream
	delete d; 
	streamIn.clear();
	
	//1 plot, one set of ions	
	TEST(streamOut.size() == 2,"stream count");


	size_t streamMask=0;

	for(size_t ui=0;ui<streamOut.size();ui++)
	{
		streamMask|=streamOut[ui]->getStreamType();
		delete streamOut[ui];
	}



	TEST(streamMask == ( STREAM_TYPE_DRAW | STREAM_TYPE_PLOT) , "Stream type checking");

	return true;
}

bool replaceTest()
{
	std::string ionFile=createTmpFilename(NULL,".pos");
		
	vector<IonHit> ions;
	const unsigned int NIONS=10;
	for(unsigned int ui=0;ui<NIONS;ui++)
		ions.push_back(IonHit(Point3D(ui,ui,ui),1));

	IonHit::makePos(ions,ionFile.c_str());
	
	for(unsigned int ui=0;ui<NIONS;ui++)
		ions[ui].setMassToCharge(2);

	IonStreamData *d = new IonStreamData;
	d->data.swap(ions);

	//Create a spatial analysis filter
	SpatialAnalysisFilter *f=new SpatialAnalysisFilter;
	f->setCaching(false);	
	
	//Set it to do a union calculation 
	bool needUp;
	string s;
	s=TRANS(SPATIAL_ALGORITHMS[ALGORITHM_REPLACE]);
	TEST(f->setProperty(KEY_ALGORITHM,s,needUp),"Set prop");
	TEST(f->setProperty(KEY_REPLACE_FILE,ionFile,needUp),"Set prop");
	s=TRANS(REPLACE_ALGORITHMS[REPLACE_MODE_INTERSECT]);
	TEST(f->setProperty(KEY_REPLACE_ALGORITHM,s,needUp),"Set prop");
	
	s="1";
	TEST(f->setProperty(KEY_REPLACE_VALUE,s,needUp),"Set prop");


	//Do the refresh
	ProgressData p;
	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(d);
	TEST(!f->refresh(streamIn,streamOut,p),"refresh OK");
	delete f;
	delete d;
	streamIn.clear();

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == NIONS,"Number objects");

	//we should have taken the mass-to-charge from the file
	const IonStreamData *outIons = (const IonStreamData*)streamOut[0];
	for(unsigned int ui=0;ui<NIONS; ui++)
	{
		ASSERT(outIons->data[ui].getMassToCharge() == 1); 
	}

	wxRemoveFile(ionFile);

	delete streamOut[0];
	
	return true;
}


//--- Local concentration tests --
const IonStreamData *createLCIonStream()
{
	IonStreamData*d = new IonStreamData;
	IonHit h;

	//create some points, of differing mass-to-charge 

	//1 "A" ion, mass 1
	//1 "B" ion, mass 2
	//2 "C" ions,mass 3
	h.setPos(Point3D(0,0,0));
	h.setMassToCharge(1);
	d->data.push_back(h);

	h.setPos(Point3D(0.49,0.0,0.0));
	h.setMassToCharge(2);
	d->data.push_back(h);

	h.setPos(Point3D(0.0,0.5,0.0));
	h.setMassToCharge(3);
	d->data.push_back(h);
	
	h.setPos(Point3D(0.0,0.0,0.51));
	h.setMassToCharge(3);
	d->data.push_back(h);
	
	return d;
}

RangeStreamData *createLCRangeStream()
{
	//Create a fake rangefile
	RangeStreamData *r= new RangeStreamData;
	RangeFile *rng = new RangeFile;

	RGBf colour;
	colour.red=colour.blue=colour.green=0.5;
	unsigned int iid[3], rid[3]; 
	iid[0] = rng->addIon("A","A",colour);
	iid[1] = rng->addIon("B","B",colour);
	iid[2] = rng->addIon("C","C",colour);

	rid[0]=rng->addRange(0.5,1.5,iid[0]);
	rid[1]=rng->addRange(1.51,2.5,iid[1]);
	rid[2]=rng->addRange(2.51,3.5,iid[2]);

	r->rangeFile=rng;
	r->enabledRanges.resize(3,1);
	r->enabledIons.resize(3,1);
	return r;
}

SpatialAnalysisFilter *createLCTestSpatialFilter(const vector<const FilterStreamData *>  &in)
{
	//Create a spatial analysis filter
	SpatialAnalysisFilter *f=new SpatialAnalysisFilter;
	f->setCaching(false);
	//inform it about the rangefile	
	vector< const FilterStreamData *> out;
	f->initFilter(in,out);
	//Set Filter to perform local concentration analysis 
	// - dist termination,
	//---
	bool needUp;
	string s;
	s=TRANS(SPATIAL_ALGORITHMS[ALGORITHM_LOCAL_CONCENTRATION]);
	if(!(f->setProperty(KEY_ALGORITHM,s,needUp)) )
	{
		cerr << "Failed Set prop (algorithm)";
		return 0;
	}
	
	
	//Set enable/disable status (one for each)
	// A ions - source. B ions - numerator, C ions - denominator
	for(unsigned int ui=0; ui<3; ui++)
	{
		if(ui!=0)
		{
			if(!(f->setProperty(Filter::muxKey(KEYTYPE_ENABLE_SOURCE,ui),"0",needUp)) )
				return 0;
		}
		if(ui!=1)
		{
			if(!(f->setProperty(Filter::muxKey(KEYTYPE_ENABLE_NUMERATOR,ui),"0",needUp)) )
				return 0;
		}
		if(ui!=2)
		{
			if(!(f->setProperty(Filter::muxKey(KEYTYPE_ENABLE_DENOMINATOR,ui),"0",needUp)))
				return 0;
		}
	}
	//---

	return f;
}

bool localConcTestRadius()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	
	//Create some input data
	//--
	RangeStreamData *rngStream=createLCRangeStream();
	streamIn.push_back(rngStream);
	streamIn.push_back(createLCIonStream());

	//--
	
	SpatialAnalysisFilter *f=createLCTestSpatialFilter(streamIn);
	f->initFilter(streamIn,streamOut);

	bool needUp;	
	string s;
	s=TRANS(STOP_MODES[STOP_MODE_RADIUS]);
	TEST(f->setProperty(KEY_STOPMODE,s,needUp),"Failed Set prop (stop mode)");
	s="1.0";
	TEST(f->setProperty(KEY_DISTMAX,s,needUp),"Failed Set prop (maxDist)");
	
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Checking refresh code");
	delete f;

	//FIXME: Check the data coming out
	TEST(streamOut.size() == 1,"stream size");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == 1,"output ion count");

	IonStreamData *ionD = (IonStreamData *)streamOut[0];

	float localConc = ionD->data[0].getMassToCharge(); 
	TEST(EQ_TOL(localConc,1.0/3.0*100.0),"Local Concentration check");

	delete rngStream->rangeFile;

	for(unsigned int ui=0;ui<streamIn.size(); ui++)
		delete streamIn[ui];
	streamIn.clear();

	//kill the output ion stream
	for(unsigned int ui=0;ui<streamOut.size(); ui++)
		delete streamOut[ui];
	streamOut.clear();

	return true;
}

bool localConcTestNN()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	
	//Create some input data
	//--

	RangeStreamData *rngStream=createLCRangeStream();
	streamIn.push_back(rngStream);
	streamIn.push_back(createLCIonStream());

	//--
	
	SpatialAnalysisFilter *f=createLCTestSpatialFilter(streamIn);
	f->initFilter(streamIn,streamOut);
	
	bool needUp;	
	string s;
	s=TRANS(STOP_MODES[STOP_MODE_NEIGHBOUR]);
	TEST(f->setProperty(KEY_STOPMODE,s,needUp),"Failed Set prop (stop mode)");
	s="3";
	TEST(f->setProperty(KEY_NNMAX,s,needUp),"Failed Set prop (nnMax)");
	
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Checking refresh code");
	delete f;

	//FIXME: Check the data coming out
	TEST(streamOut.size() == 1,"stream size");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == 1,"output ion count");

	IonStreamData *ionD = (IonStreamData *)streamOut[0];

	float localConc = ionD->data[0].getMassToCharge(); 
	TEST(EQ_TOL(localConc,1.0/3.0*100.0),"Local Concentration check");


	delete rngStream->rangeFile;
	for(unsigned int ui=0;ui<streamIn.size(); ui++)
		delete streamIn[ui];
	streamIn.clear();

	//kill the output ion stream
	for(unsigned int ui=0;ui<streamOut.size(); ui++)
		delete streamOut[ui];
	streamOut.clear();

	return true;
}
//--------------------------------

#endif

