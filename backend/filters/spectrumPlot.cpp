/*
 *	spectrumPlot.cpp - Compute histograms of values for valued 3D point data
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
#include "spectrumPlot.h"
#include "algorithms/mass.h"

#include "../plot.h"

#include "filterCommon.h"

using std::string;
using std::vector;
using std::pair;


//!Error codes
enum 
{
	SPECTRUM_BAD_ALLOC=1,
	SPECTRUM_BAD_BINCOUNT,
	SPECTRUM_ABORT_FAIL,
	SPECTRUM_ERR_ENUM_END,
};


enum
{
	KEY_SPECTRUM_BINWIDTH,
	KEY_SPECTRUM_AUTOEXTREMA,
	KEY_SPECTRUM_MIN,
	KEY_SPECTRUM_MAX,
	KEY_SPECTRUM_LOGARITHMIC,
	KEY_SPECTRUM_NORMALISE,
	KEY_SPECTRUM_NORMALISE_LOWERBOUND,
	KEY_SPECTRUM_NORMALISE_UPPERBOUND,
	KEY_SPECTRUM_PLOTTYPE,
	KEY_SPECTRUM_COLOUR,
	KEY_SPECTRUM_BACKMODE,
	KEY_SPECTRUM_BACKMODE_FLAT_START,
	KEY_SPECTRUM_BACKMODE_FLAT_END,
	KEY_SPECTRUM_CORRECTED_ONLY
};

//Limit user to two :million: bins
const unsigned int SPECTRUM_MAX_BINS=2000000;

const unsigned int SPECTRUM_AUTO_MAX_BINS=45000;


//String to use on plot's y label
const char *YLABEL_STRING=NTRANS("Count");

enum
{
	NORMALISE_NONE,
	NORMALISE_MAX,
	NORMALISE_MAX_IN_BOUND,
	NORMALISE_INTEGRAL_ONE,
	NORMALISE_ENUM_END
};

const char *NORMALISE_STRING[] = {NTRANS("None"),
				NTRANS("Maximum"),
				NTRANS("Max in limit"),
				NTRANS("Probability"),
					};

SpectrumPlotFilter::SpectrumPlotFilter()
{
	minPlot=0;
	maxPlot=150;
	autoExtrema=true;	
	binWidth=0.05;
	plotStyle=0;
	logarithmic=1;
	fitMode=0;
	massBackStart=1.2;
	massBackEnd=1.9;
	showOnlyCorrected=false;
	normaliseMode=NORMALISE_NONE;
	normaliseBounds=std::make_pair(0.0,100.0);

	//Default to blue plot
	rgba = ColourRGBAf(0,0,1.0f,1.0f);
}

Filter *SpectrumPlotFilter::cloneUncached() const
{
	SpectrumPlotFilter *p = new SpectrumPlotFilter();

	p->minPlot=minPlot;
	p->maxPlot=maxPlot;
	p->binWidth=binWidth;
	p->autoExtrema=autoExtrema;
	p->rgba=rgba;	
	p->plotStyle=plotStyle;
	p->logarithmic = logarithmic;
	p->fitMode = fitMode;
	p->massBackStart = massBackStart;
	p->massBackEnd = massBackEnd;

	p->normaliseMode=normaliseMode;
	p->normaliseBounds=normaliseBounds;

	p->showOnlyCorrected= showOnlyCorrected;



	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

//!Get approx number of bytes for caching output
size_t SpectrumPlotFilter::numBytesForCache(size_t nObjects) const
{
	//Check that we have good plot limits, and bin width. if not, we cannot estimate cache size
	if(minPlot ==std::numeric_limits<float>::max() ||
		maxPlot==-std::numeric_limits<float>::max()  || 
		binWidth < sqrtf(std::numeric_limits<float>::epsilon()))
	{
		return (size_t)(-1);
	}

	return (size_t)((float)(maxPlot- minPlot)/(binWidth))*2*sizeof(float);
}

unsigned int SpectrumPlotFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{

	if(cacheOK)
	{
		//Only report the spectrum plot
		propagateCache(getOut);

		return 0;
	}



	size_t totalSize=numElements(dataIn,STREAM_TYPE_IONS);
	
	unsigned int nBins=2;
	if(totalSize)
	{

		//Determine min and max of input
		if(autoExtrema)
		{
			progress.maxStep=2;
			progress.step=1;
			progress.stepName=TRANS("Extrema");
		
			size_t n=0;
			minPlot = std::numeric_limits<float>::max();
			maxPlot =-std::numeric_limits<float>::max();
			//Loop through each type of data
			
			unsigned int curProg=NUM_CALLBACK;	
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				//Only process stream_type_ions. Do not propagate anything,
				//except for the spectrum
				if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
				{
					IonStreamData *ions;
					ions = (IonStreamData *)dataIn[ui];
					for(unsigned int uj=0;uj<ions->data.size(); uj++)
					{
						minPlot = std::min(minPlot,
							ions->data[uj].getMassToCharge());
						maxPlot = std::max(maxPlot,
							ions->data[uj].getMassToCharge());


						if(!curProg--)
						{
							n+=NUM_CALLBACK;
							progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
							curProg=NUM_CALLBACK;
							if(*Filter::wantAbort)
								return SPECTRUM_ABORT_FAIL;
						}
					}
		
				}
				
			}
		
			//Check that the plot values have been set (ie not same as initial values)
			if(minPlot !=std::numeric_limits<float>::max() &&
				maxPlot!=-std::numeric_limits<float>::max() )
			{
				//push them out a bit to make the edges visible
				maxPlot=maxPlot+1;
				minPlot=minPlot-1;
			}

			//Time to move to phase 2	
			progress.step=2;
			progress.stepName=TRANS("count");
		}



		double delta = ((double)maxPlot - (double)(minPlot))/(double)binWidth;


		//Check that we have good plot limits.
		if(minPlot ==std::numeric_limits<float>::max() || 
			minPlot ==-std::numeric_limits<float>::max() || 
			fabs(delta) >  std::numeric_limits<float>::max() || // Check for out-of-range
			 binWidth < sqrtf(std::numeric_limits<float>::epsilon())	)
		{
			//If not, then simply set it to some defaults.
			minPlot=0; maxPlot=1.0; binWidth=0.1;
		}



		//Estimate number of bins in floating point, and check for potential overflow .
		float tmpNBins = (float)((maxPlot-minPlot)/binWidth);

		//If using autoextrema, use a lower limit for max bins,
		//as we may just hit a nasty data point
		if(autoExtrema)
			tmpNBins = std::min(SPECTRUM_AUTO_MAX_BINS,(unsigned int)tmpNBins);
		else
			tmpNBins = std::min(SPECTRUM_MAX_BINS,(unsigned int)tmpNBins);
		
		nBins = (unsigned int)tmpNBins;

		if (!nBins)
		{
			nBins = 10;
			binWidth = (maxPlot-minPlot)/nBins;
		}
	}


	PlotStreamData *d;
	d=new PlotStreamData;
	try
	{
		d->xyData.resize(nBins);
	}
	catch(std::bad_alloc)
	{

		delete d;
		return SPECTRUM_BAD_ALLOC;
	}

	
	d->r =rgba.r();
	d->g = rgba.g();
	d->b = rgba.b();
	d->a = rgba.a();

	d->logarithmic=logarithmic;
	d->plotStyle = plotStyle;
	d->plotMode=PLOT_MODE_1D;

	d->index=0;
	d->parent=this;
	d->dataLabel = getUserString();
	d->yLabel=TRANS(YLABEL_STRING);

	//Check all the incoming ion data's type name
	//and if it is all the same, use it for the plot X-axis
	std::string valueName;
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *ionD;
				ionD=(const IonStreamData *)dataIn[ui];
				if(!valueName.size())
					valueName=ionD->valueType;
				else
				{
					if(ionD->valueType != valueName)
					{
						valueName=TRANS("Mixed data");
						break;
					}
				}
			}
		}
	}

	d->xLabel=valueName;


	//Look for any ranges in input stream, and add them to the plot
	//while we are doing that, count the number of ions too
	for(unsigned int ui=0;ui<dataIn.size();ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_RANGE:
			{
				const RangeStreamData *rangeD;
				rangeD=(const RangeStreamData *)dataIn[ui];
				for(unsigned int uj=0;uj<rangeD->rangeFile->getNumRanges();uj++)
				{
					unsigned int ionId;
					ionId=rangeD->rangeFile->getIonID(uj);
					//Only append the region if both the range
					//and the ion are enabled
					if((rangeD->enabledRanges)[uj] && 
						(rangeD->enabledIons)[ionId])
					{
						//save the range as a "region"
						d->regions.push_back(rangeD->rangeFile->getRange(uj));
						d->regionTitle.push_back(rangeD->rangeFile->getName(ionId));
						d->regionID.push_back(uj);
						d->parent=this;
						//FIXME: Const correctness
						d->regionParent=(Filter*)rangeD->parent;

						RGBf colour;
						//Use the ionID to set the range colouring
						colour=rangeD->rangeFile->getColour(ionId);

						//push back the range colour
						d->regionR.push_back(colour.red);
						d->regionG.push_back(colour.green);
						d->regionB.push_back(colour.blue);
					}
				}
				break;
			}
			default:
				break;
		}
	}

#pragma omp parallel for
	for(unsigned int ui=0;ui<nBins;ui++)
	{
		d->xyData[ui].first = minPlot + ui*binWidth;
		d->xyData[ui].second=0;
	}	
	//Compute the plot bounds
	d->autoSetHardBounds();
	//Limit them to 1.0 or greater (due to log)
	d->hardMinY=std::min(1.0f,d->hardMaxY);


	//Number of ions currently processed
	size_t n=0;
	unsigned int curProg=NUM_CALLBACK;
	//Loop through each type of data	
	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *ions;
				ions = (const IonStreamData *)dataIn[ui];



				//Sum the data bins as needed
				for(unsigned int uj=0;uj<ions->data.size(); uj++)
				{
					unsigned int bin;
					bin = (unsigned int)((ions->data[uj].getMassToCharge()-minPlot)/binWidth);
					//Dependant upon the bounds,
					//actual data could be anywhere. >=0 is implicit
					if( bin < d->xyData.size())
						d->xyData[bin].second++;

					//update progress every CALLBACK ions
					if(!curProg--)
					{
						n+=NUM_CALLBACK;
						progress.filterProgress= (unsigned int)(((float)(n)/((float)totalSize))*100.0f);
						curProg=NUM_CALLBACK;
						if(*Filter::wantAbort)
						{
							delete d;
							return SPECTRUM_ABORT_FAIL;
						}
					}
				}

				break;
			}
			default:
				//Don't propagate any type.
				break;
		}

	}

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
		}
		else 
		{
			if (!showOnlyCorrected)
			{
				vector<float> backgroundHist;
				createMassBackground(backParams.massStart,backParams.massEnd,
					nBins,backParams.intensity,backgroundHist);

				//Create a new plot which shows the spectrum's background
				PlotStreamData *plotBack = new PlotStreamData;
				plotBack->parent = this;
				plotBack->dataLabel=string(TRANS("Background:")) + d->dataLabel;	
				plotBack->plotMode=d->plotMode;
				plotBack->xLabel=d->xLabel;	
				plotBack->index=d->index+1;	
				plotBack->yLabel=d->yLabel;
				plotBack->xyData.reserve(d->xyData.size());


				float intensityPerBin = backParams.intensity;
				for(size_t ui=0;ui<d->xyData.size(); ui++)

				{
					//negative sqrt cannot does not work. Equation only valid for positive masses
					if(d->xyData[ui].first <=0)
						continue;
		
					plotBack->xyData.push_back( std::make_pair(d->xyData[ui].first,
							intensityPerBin/(2.0*sqrtf(d->xyData[ui].first))));
			
				}

				cacheAsNeeded(plotBack);

				getOut.push_back(plotBack);
			}
			else
			{
				//Correct the mass spectrum that we display
				for(size_t ui=0;ui<d->xyData.size(); ui++)
				{
					//negative sqrt cannot does not work. Equation only valid for positive masses
					if(d->xyData[ui].first <=0)
					{
						d->xyData[ui].second =0;
						continue;
					}
	
					d->xyData[ui].second-=backParams.intensity/sqrtf(d->xyData[ui].first);

			
				}
				//prevent negative values in log mode
				if(logarithmic)
				{
					for(size_t ui=0;ui<d->xyData.size(); ui++)
						d->xyData[ui].second=std::max(0.0f,d->xyData[ui].second);
				}
				
			}
		}
	}

	if(normaliseMode != NORMALISE_NONE)
	{
		normalise(d->xyData);
		switch(normaliseMode)
		{
			case NORMALISE_MAX:
			case NORMALISE_MAX_IN_BOUND:
				d->yLabel=TRANS("Relative ") + d->yLabel;
				break;
			case NORMALISE_INTEGRAL_ONE:
				d->yLabel=TRANS("Probability Density"); 
				
				break;	

			default:
				ASSERT(false);

		}
	}	
	cacheAsNeeded(d);
	
	getOut.push_back(d);

	return 0;
}


void SpectrumPlotFilter::normalise(vector<pair<float,float> > &xyData) const
{
	float scaleFact=0;
	switch(normaliseMode)
	{
		case NORMALISE_NONE:
			return;
		case NORMALISE_MAX:
			for(size_t ui=0;ui<xyData.size();ui++)
			{
				scaleFact=std::max(xyData[ui].second,scaleFact);
			}
			break;
		case NORMALISE_MAX_IN_BOUND:
			for(size_t ui=0;ui<xyData.size();ui++)
			{
				if(xyData[ui].first < normaliseBounds.second &&
					xyData[ui].first >=normaliseBounds.first)
					scaleFact=std::max(xyData[ui].second,scaleFact);
			}
			break;
		case NORMALISE_INTEGRAL_ONE:
		{
			#pragma omp parallel for reduction(+:scaleFact)
			for(size_t ui=0;ui<xyData.size();ui++)
				scaleFact+=xyData[ui].second;
			float binDelta=1.0;
			if(xyData.size() > 1)
				binDelta=xyData[1].first - xyData[0].first;
			scaleFact*=binDelta;
			break;
		}
			
		default:
			ASSERT(false);
	}

	if(scaleFact > 0 )
	{
		#pragma omp parallel for
		for(size_t ui=0;ui<xyData.size();ui++)
		{
			xyData[ui].second/=scaleFact;
		}
	}
}


void SpectrumPlotFilter::getProperties(FilterPropGroup &propertyList) const
{

	FilterProperty p;
	size_t curGroup=0;
	string str;

	stream_cast(str,binWidth);
	p.name=TRANS("Bin width");
	p.data=str;
	p.key=KEY_SPECTRUM_BINWIDTH;
	p.type=PROPERTY_TYPE_REAL;
	p.helpText=TRANS("Step size for spectrum");
	propertyList.addProperty(p,curGroup);

	str=boolStrEnc(autoExtrema);

	p.name=TRANS("Auto Min/max");
	p.data=str;
	p.key=KEY_SPECTRUM_AUTOEXTREMA;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Automatically compute spectrum upper and lower bound");
	propertyList.addProperty(p,curGroup);

	stream_cast(str,minPlot);
	p.data=str;
	p.name=TRANS("Min");
	p.key=KEY_SPECTRUM_MIN;
	p.type=PROPERTY_TYPE_REAL;
	p.helpText=TRANS("Starting position for spectrum");
	propertyList.addProperty(p,curGroup);

	stream_cast(str,maxPlot);
	p.key=KEY_SPECTRUM_MAX;
	p.name=TRANS("Max");
	p.data=str;
	p.type=PROPERTY_TYPE_REAL;
	p.helpText=TRANS("Ending position for spectrum");
	propertyList.addProperty(p,curGroup);

	propertyList.setGroupTitle(curGroup,TRANS("Data"));
	curGroup++;

	str=boolStrEnc(logarithmic);
	p.key=KEY_SPECTRUM_LOGARITHMIC;
	p.name=TRANS("Logarithmic");
	p.data=str;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Convert the plot to logarithmic mode");
	propertyList.addProperty(p,curGroup);

	vector<pair<unsigned int,string> > choices;
	string tmpStr;
	for(unsigned int ui=0;ui<NORMALISE_ENUM_END;ui++)
	{	
		tmpStr=TRANS(NORMALISE_STRING[ui]);
		choices.push_back(make_pair( ui,tmpStr));
	}
	
	tmpStr= choiceString(choices,normaliseMode);
	p.name=TRANS("Normalisation");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Rescale the plot height, to make inter-spectrum comparisons easier");
	p.key=KEY_SPECTRUM_NORMALISE;
	propertyList.addProperty(p,curGroup);


	if(normaliseMode == NORMALISE_MAX_IN_BOUND)
	{
		p.name=TRANS("Lower Bound");
		stream_cast(tmpStr,normaliseBounds.first);
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Do not use data below this x-value for normalisation");
		p.key=KEY_SPECTRUM_NORMALISE_LOWERBOUND;
		propertyList.addProperty(p,curGroup);

		p.name=TRANS("Upper Bound");
		stream_cast(tmpStr,normaliseBounds.second);
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Do not use data above this x-value for normalisation");
		p.key=KEY_SPECTRUM_NORMALISE_UPPERBOUND;
		propertyList.addProperty(p,curGroup);

	}
	
	//Let the user know what the valid values for plot type are
	choices.clear();
	for(unsigned int ui=PLOT_LINE_LINES; ui<PLOT_LINE_STEM+1;ui++)
	{
		tmpStr=plotString(ui);
		choices.push_back(make_pair( ui,tmpStr));
	}

	tmpStr= choiceString(choices,plotStyle);
	p.name=TRANS("Plot Type");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Visual style of plot");
	p.key=KEY_SPECTRUM_PLOTTYPE;
	propertyList.addProperty(p,curGroup);

	p.name=TRANS("Colour");
	p.data=rgba.toColourRGBA().rgbaString(); 
	p.type=PROPERTY_TYPE_COLOUR;
	p.helpText=TRANS("Colour of plotted spectrum");
	p.key=KEY_SPECTRUM_COLOUR;
	propertyList.addProperty(p,curGroup);

	propertyList.setGroupTitle(curGroup,TRANS("Appearance"));

	curGroup++;

	choices.clear();
	for(unsigned int ui=0;ui<FIT_MODE_ENUM_END; ui++)
	{
		tmpStr=TRANS(BACKGROUND_MODE_STRING[ui]);
		choices.push_back(make_pair(ui,tmpStr));
	}
	
	p.name=TRANS("Model");
	p.key=KEY_SPECTRUM_BACKMODE;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Fitting method to use");
	p.data=choiceString(choices,fitMode);
	propertyList.addProperty(p,curGroup);


	switch(fitMode)
	{
		case FIT_MODE_NONE:
			break;
		case FIT_MODE_CONST_TOF:
			//Add start/end TOF 
			p.name=TRANS("Fit Start");
			p.helpText=TRANS("Start mass value for fitting background");
			p.type=PROPERTY_TYPE_REAL;
			p.key=KEY_SPECTRUM_BACKMODE_FLAT_START;
			stream_cast(p.data,massBackStart);
			propertyList.addProperty(p,curGroup);

			p.name=TRANS("Fit End");
			p.helpText=TRANS("End mass value for fitting background");
			p.type=PROPERTY_TYPE_REAL;
			p.key=KEY_SPECTRUM_BACKMODE_FLAT_END;
			stream_cast(p.data,massBackEnd);

			propertyList.addProperty(p,curGroup);
			break;
		default:
			ASSERT(false);
	}

	if(fitMode != FIT_MODE_NONE)
	{
		p.name=TRANS("Corr. Only");
		p.helpText=TRANS("Only show corrected spectrum, not fit");
		p.key=KEY_SPECTRUM_CORRECTED_ONLY;
		p.type=PROPERTY_TYPE_BOOL;
		p.data=boolStrEnc(showOnlyCorrected);
		propertyList.addProperty(p,curGroup);

	}

	propertyList.setGroupTitle(curGroup,TRANS("Background Mode"));
	
}

bool SpectrumPlotFilter::setProperty( unsigned int key, 
					const std::string &value, bool &needUpdate) 
{
	needUpdate=false;
	switch(key)
	{
		//Bin width
		case KEY_SPECTRUM_BINWIDTH:
		{
			float newWidth;
			if(stream_cast(newWidth,value))
				return false;

			if(newWidth < std::numeric_limits<float>::epsilon())
				return false;

			//Prevent overflow on next line
			if(maxPlot == std::numeric_limits<float>::max() ||
				minPlot == std::numeric_limits<float>::min())
				return false;

			if(newWidth < 0.0f || newWidth > (maxPlot - minPlot))
				return false;



			needUpdate=true;
			binWidth=newWidth;
			clearCache();

			break;
		}
		//Auto min/max
		case KEY_SPECTRUM_AUTOEXTREMA:
		{
			if(!applyPropertyNow(autoExtrema,value,needUpdate))
				return false;
			break;

		}
		//Plot min
		case KEY_SPECTRUM_MIN:
		{
			if(autoExtrema)
				return false;

			float newMin;
			if(stream_cast(newMin,value))
				return false;

			if(newMin >= maxPlot)
				return false;

			needUpdate=true;
			minPlot=newMin;

			clearCache();
			break;
		}
		//Plot max
		case KEY_SPECTRUM_MAX:
		{
			if(autoExtrema)
				return false;
			float newMax;
			if(stream_cast(newMax,value))
				return false;

			if(newMax <= minPlot)
				return false;

			needUpdate=true;
			maxPlot=newMax;
			clearCache();

			break;
		}
		case KEY_SPECTRUM_LOGARITHMIC:
		{
			//Only allow valid values
			unsigned int valueInt;
			if(stream_cast(valueInt,value))
				return false;

			//Only update as needed
			if(valueInt ==0 || valueInt == 1)
			{
				if(logarithmic != (bool)valueInt)
				{
					needUpdate=true;
					logarithmic=valueInt;
				}
				else
					needUpdate=false;

			}
			else
				return false;		
			
			if(cacheOK)
			{
				//Change the output of the plot streams that
				//we cached, in order to avoid recomputation
				for(size_t ui=0;ui<filterOutputs.size();ui++)
				{
					if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_PLOT )
					{
						PlotStreamData *p;
						p =(PlotStreamData*)filterOutputs[ui];

						p->logarithmic=logarithmic;
					}
				}

			}
	
			break;

		}
		//Plot type
		case KEY_SPECTRUM_PLOTTYPE:
		{
			unsigned int tmpPlotType;

			tmpPlotType=plotID(value);

			if(tmpPlotType >= PLOT_LINE_NONE)
				return false;

			plotStyle = tmpPlotType;
			needUpdate=true;	


			//Perform introspection on 
			//cache
			if(cacheOK)
			{
				for(size_t ui=0;ui<filterOutputs.size();ui++)
				{
					if(filterOutputs[ui]->getStreamType() == STREAM_TYPE_PLOT)
					{
						PlotStreamData *p;
						p =(PlotStreamData*)filterOutputs[ui];

						p->plotStyle=plotStyle;
					}
				}

			}
			else
			{
				clearCache();
			}

			break;
		}
		case KEY_SPECTRUM_COLOUR:
		{
			ColourRGBA tmpRgb;
			tmpRgb.parse(value);

			if(tmpRgb.toRGBAf() != rgba)
			{
				rgba=tmpRgb.toRGBAf();
				needUpdate=true;
			}
			
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
			else
				clearCache();
			break;
		}
		case KEY_SPECTRUM_BACKMODE:
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
			clearCache();
			
			break;
		}
	
		case KEY_SPECTRUM_BACKMODE_FLAT_START:
		{
			float tmpStart;
			if(stream_cast(tmpStart,value))
				return false;
			if( tmpStart>=massBackEnd)
				return false;

			if(!applyPropertyNow(massBackStart,value,needUpdate))
				return false;
			break;
		}
		case KEY_SPECTRUM_BACKMODE_FLAT_END:
		{
			float tmpEnd;
			if(stream_cast(tmpEnd,value))
				return false;
			if( tmpEnd<=massBackStart)
				return false;

			if(!applyPropertyNow(massBackEnd,value,needUpdate))
				return false;
			break;
		}
		case KEY_SPECTRUM_CORRECTED_ONLY:
		{
			if(!applyPropertyNow(showOnlyCorrected,value,needUpdate))
				return false;
			break;
		}
		case KEY_SPECTRUM_NORMALISE:
		{
			unsigned int newMode;
			for(size_t ui=0;ui<NORMALISE_ENUM_END; ui++)
			{
				if(string(NORMALISE_STRING[ui]) == string(value))
				{
					newMode=ui;
					break;
				}
			}
			ASSERT(newMode <NORMALISE_ENUM_END)
		
			//TODO: Cache introspection?	
			normaliseMode=newMode;
			cacheOK=false;
			needUpdate=true;
			clearCache();
			break;

		}
		case KEY_SPECTRUM_NORMALISE_LOWERBOUND:
		{
			float tmpVal;
			if(stream_cast(tmpVal,value))
				return false;
			if( tmpVal>=normaliseBounds.second)
				return false;

			if(!applyPropertyNow(normaliseBounds.first,value,needUpdate))
				return false;
			
		
			//TODO: Cache introspection?	
			normaliseBounds.first=tmpVal;
			cacheOK=false;
			break;
		}
		case KEY_SPECTRUM_NORMALISE_UPPERBOUND:
		{
			float tmpVal;
			if(stream_cast(tmpVal,value))
				return false;
			if( tmpVal<=normaliseBounds.first)
				return false;

			if(!applyPropertyNow(normaliseBounds.second,value,needUpdate))
				return false;
			
		
			//TODO: Cache introspection?	
			normaliseBounds.second=tmpVal;
			cacheOK=false;
			break;
		}
		default:
			ASSERT(false);
			break;

	}

	
	return true;
}

void SpectrumPlotFilter::setUserString(const std::string &s)
{
	if(userString !=s)
	{
		userString=s;
		clearCache();
		cacheOK=false;
	}
}


std::string  SpectrumPlotFilter::getSpecificErrString(unsigned int code) const
{
	const char *errStrs[] = {
		"",
		"Insufficient memory for spectrum filter.",
		"Bad bincount value in spectrum filter.",
		"Aborted."
	};
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(errStrs) == SPECTRUM_ERR_ENUM_END);
	ASSERT(code < SPECTRUM_ERR_ENUM_END);
	return errStrs[code];
}



void SpectrumPlotFilter::setPropFromBinding(const SelectionBinding &b)
{
	ASSERT(false);
}

bool SpectrumPlotFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<"  << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

			f << tabs(depth+1) << "<extrema min=\"" << minPlot << "\" max=\"" <<
					maxPlot  << "\" auto=\"" << autoExtrema << "\"/>" << endl;
			f << tabs(depth+1) << "<binwidth value=\"" << binWidth<< "\"/>" << endl;

			f << tabs(depth+1) << "<colour r=\"" <<  rgba.r() << "\" g=\"" << rgba.g() << "\" b=\"" << rgba.b()
				<< "\" a=\"" << rgba.a() << "\"/>" <<endl;
			
			f << tabs(depth+1) << "<logarithmic value=\"" << logarithmic<< "\"/>" << endl;

			f << tabs(depth+1) << "<plottype value=\"" << plotStyle<< "\"/>" << endl;
			f << tabs(depth+1) << "<background mode=\"" << fitMode << "\">" << endl;
				f << tabs(depth+2) << "<fitwindow start=\"" << massBackStart<< "\" end=\"" << massBackEnd << "\"/>" << endl;
				f << tabs(depth+2) << "<showonlycorrected value=\"" << boolStrEnc(showOnlyCorrected)  <<  "\"/>" << endl;


			f << tabs(depth+1) << "</background>" << endl;
			f << tabs(depth+1) << "<normalise mode=\"" << normaliseMode << "\" lowbound=\"" << normaliseBounds.first << "\" highbound=\"" << normaliseBounds.second << "\"/>" << endl;
			
			f << tabs(depth) << "</" << trueName() <<  ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool SpectrumPlotFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
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

	//Retrieve Extrema 
	//===
	float tmpMin,tmpMax;
	if(XMLHelpFwdToElem(nodePtr,"extrema"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"min");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);

	//convert from string to digit
	if(stream_cast(tmpMin,tmpStr))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"max");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;
	xmlFree(xmlString);
	
	//convert from string to digit
	if(stream_cast(tmpMax,tmpStr))
		return false;


	if(tmpMin >=tmpMax)
		return false;

	minPlot=tmpMin;
	maxPlot=tmpMax;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"auto");
	if(!xmlString)
		return false;
	
	tmpStr=(char *)xmlString;
	if(!boolStrDec(tmpStr,autoExtrema))
	{
		xmlFree(xmlString);
		return false;
	}

	xmlFree(xmlString);
	//===
	
	//Retrieve bin width 
	//====
	//
	if(!XMLGetNextElemAttrib(nodePtr,binWidth,"binwidth","value"))
		return false;
	if(binWidth  <= 0.0)
		return false;

	if(!autoExtrema && binWidth > maxPlot - minPlot)
		return false;
	//====
	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	ColourRGBAf tmpRgba;
	if(!parseXMLColour(nodePtr,tmpRgba))
		return false;
	rgba=tmpRgba;
	//====
	
	//Retrieve logarithmic mode
	//====
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"logarithmic","value"))
		return false;
	if(!boolStrDec(tmpStr,logarithmic))
		return false;
	//====

	//Retrieve plot type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,plotStyle,"plottype","value"))
		return false;
	if(plotStyle >= PLOT_LINE_NONE)
	       return false;	
	//====

	//Retrieve background fitting mode, if we have it
	// Only available 3Depict >= 0.0.18
	// internal rev > 5e7b66e5ce3d
	xmlNodePtr  tmpNode=nodePtr;
	if(!XMLHelpFwdToElem(nodePtr,"background"))
	{
		if(XMLHelpGetProp(fitMode,nodePtr,"mode"))
			return false;

		if(!nodePtr->xmlChildrenNode)
			return false;
			
		tmpNode=nodePtr;
		nodePtr=nodePtr->xmlChildrenNode;
		
		if(!XMLGetNextElemAttrib(nodePtr,massBackStart,"fitwindow","start"))
			return false;

		if(XMLHelpGetProp(massBackEnd,nodePtr,"end"))
			return false;
		
		if(!XMLGetNextElemAttrib(nodePtr,showOnlyCorrected,"showonlycorrected","value"))
			return false;
		nodePtr=tmpNode;
	}
	else
	{
		nodePtr=tmpNode;
	}
	
	// Only available 3Depict >= 0.0.18
	// internal rev > 73289623683a tip
	if(!XMLHelpFwdToElem(nodePtr,"normalise"))
	{
		if(XMLHelpGetProp(normaliseMode,nodePtr,"mode"))
			return false;

		float tmpLow,tmpHigh;		
		if(XMLHelpGetProp(tmpLow,nodePtr,"lowbound"))
			return false;
	
		if(XMLHelpGetProp(tmpHigh,nodePtr,"highbound"))
			return false;

		if(tmpLow >=tmpHigh)
			return false;

		normaliseBounds.first=tmpLow;
		normaliseBounds.second=tmpHigh;
	}
	else
	{
		//initialise to some default value
		normaliseBounds=std::make_pair(0,100);
		normaliseMode=NORMALISE_NONE;
	}

	return true;
}

unsigned int SpectrumPlotFilter::getRefreshBlockMask() const
{
	//Absolutely nothing can go through this filter.
	return STREAMTYPE_MASK_ALL;
}

unsigned int SpectrumPlotFilter::getRefreshEmitMask() const
{
	return STREAM_TYPE_PLOT;
}

unsigned int SpectrumPlotFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_IONS;
}

bool SpectrumPlotFilter::needsUnrangedData() const
{
	return fitMode == FIT_MODE_CONST_TOF;
}

#ifdef DEBUG
#include <memory>

IonStreamData *synDataPoints(const unsigned int span[], unsigned int numPts)
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

bool countTest()
{
	using std::auto_ptr;
	auto_ptr<IonStreamData> d;
	const unsigned int VOL[]={
				10,10,10
				};
	const unsigned int NUMPTS=100;
	d.reset(synDataPoints(VOL,NUMPTS));

	SpectrumPlotFilter *f;
	f = new SpectrumPlotFilter;


	bool needUp;
	TEST(f->setProperty(KEY_SPECTRUM_LOGARITHMIC,"0",needUp),"Set prop");
	
	ColourRGBA tmpRGBA(255,0,0);
	TEST(f->setProperty(KEY_SPECTRUM_COLOUR,tmpRGBA.rgbString(),needUp),"Set prop");

	vector<const FilterStreamData*> streamIn,streamOut;

	streamIn.push_back(d.get());

	//OK, so now do the rotation
	//Do the refresh
	ProgressData p;
	f->setCaching(false);
	TEST(!f->refresh(streamIn,streamOut,p),"refresh error code");
	delete f;
	
	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_PLOT,"stream type");

	PlotStreamData *plot;
	plot = (PlotStreamData*)streamOut[0];

	
	//Check the plot colour is what we want.
	TEST(fabs(plot->r -1.0f) < sqrtf(std::numeric_limits<float>::epsilon()),"colour (r)");
	TEST(plot->g< 
		sqrtf(std::numeric_limits<float>::epsilon()),"colour (g)");
	TEST(plot->b < sqrtf(std::numeric_limits<float>::epsilon()),"colour (b)");

	//Count the number of ions in the plot, and check that it is equal to the number of ions we put in.
	float sumV=0;
	
	for(unsigned int ui=0;ui<plot->xyData.size();ui++)
		sumV+=plot->xyData[ui].second;

	TEST(fabs(sumV - (float)NUMPTS) < 
		std::numeric_limits<float>::epsilon(),"ion count");


	delete plot;
	return true;
}

bool SpectrumPlotFilter::runUnitTests() 
{
	if(!countTest())
		return false;

	return true;
}

#endif

