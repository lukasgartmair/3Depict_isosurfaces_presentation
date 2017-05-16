/*
 *	allFilter.cpp - factory functions for filter classes
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
#include "allFilter.h"


//Filter "factory" type function. If this gets too big, 
//we could use a pre-populated hashtable in 
//a static class to speed things up.
//returns null pointer if string is invalid
Filter *makeFilter(const std::string &s)
{
	Filter *f;
	unsigned int type=(unsigned int)-1;
	for(unsigned int ui=0;ui<FILTER_TYPE_ENUM_END; ui++)
	{
		if( s == FILTER_NAMES[ui])
			type=ui;
	}

	f=makeFilter(type);
#ifdef DEBUG
	//Should have set filter
	//type string should match
	if(f)
		ASSERT(f->trueName() ==  s);
#endif
	return  f;
}

bool isValidFilterName(const std::string &s)
{
	for(unsigned int ui=0;ui<FILTER_TYPE_ENUM_END; ui++)
	{
		if(FILTER_NAMES[ui] == s)
			return true;
	}

	return false;
}

Filter *makeFilter(unsigned int ui)
{
	Filter *f;

	switch(ui)
	{
		case FILTER_TYPE_DATALOAD:
			f=new DataLoadFilter;
			break;
		case FILTER_TYPE_IONDOWNSAMPLE:
			f=new IonDownsampleFilter;
			break;
		case FILTER_TYPE_RANGEFILE:
			f=new RangeFileFilter;
			break;
		case FILTER_TYPE_SPECTRUMPLOT: 
			f=new SpectrumPlotFilter;
			break;
		case FILTER_TYPE_IONCLIP:
			f=new IonClipFilter;
			break;
		case FILTER_TYPE_IONCOLOURFILTER:
			f=new IonColourFilter;
			break;
		case FILTER_TYPE_IONINFO:
			f=new IonInfoFilter;
			break;
		case FILTER_TYPE_PROFILE:
			f=new ProfileFilter;
			break;
		case FILTER_TYPE_BOUNDBOX:
			f = new BoundingBoxFilter;
			break;
		case FILTER_TYPE_TRANSFORM:
			f= new TransformFilter;
			break;
		case FILTER_TYPE_EXTERNALPROC:
			f= new ExternalProgramFilter;
			break;
		case FILTER_TYPE_SPATIAL_ANALYSIS:
			f = new SpatialAnalysisFilter;
			break;
		case FILTER_TYPE_CLUSTER_ANALYSIS:
			f = new ClusterAnalysisFilter;
			break;
		case FILTER_TYPE_VOXELS:
			f = new VoxeliseFilter;
			break;
		case FILTER_TYPE_ANNOTATION:
			f = new AnnotateFilter;
			break;	
		default:
			ASSERT(false);
	}

	return  f;
}



Filter *makeFilterFromDefUserString(const std::string &s)
{
	//This is a bit of a hack. Build each object, then retrieve its string.
	//Could probably use static functions and type casts to improve this
	for(unsigned int ui=0;ui<FILTER_TYPE_ENUM_END; ui++)
	{
		Filter *t;
		t = makeFilter(ui);
		if( s == t->typeString())
			return t;

		delete t;
	}

	ASSERT(false);
}
