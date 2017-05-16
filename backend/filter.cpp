/*
 *	filter.cpp - modular data filter implementation 
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

#include "filter.h"
#include "plot.h"

#include "common/stringFuncs.h"
#include "common/translation.h"

#include "wx/wxcomponents.h"

#include "common/voxels.h"
#include "backend/APT/vtk.h"

#include "filters/openvdb_includes.h"

#include <set>
#include <deque>

#ifdef _OPENMP
#include <omp.h>
#endif

using std::list;
using std::vector;
using std::string;
using std::pair;
using std::make_pair;
using std::endl;
using std::deque;




bool Filter::strongRandom= false;
ATOMIC_BOOL *Filter::wantAbort= 0;

const char *STREAM_NAMES[] = { NTRANS("Ion"),
				NTRANS("Plot"),
				NTRANS("2D Plot"),
				NTRANS("Draw"),
				NTRANS("Range"),
				NTRANS("Voxel"),
				NTRANS("OpenVDB_voxel")
				};

//Internal names for each filter
const char *FILTER_NAMES[] = { "posload",
				"iondownsample",
				"rangefile",
				"spectrumplot",
				"ionclip",
				"ioncolour",
				"compositionprofile",
				"boundingbox",
				"transform",
				"externalprog",
				"spatialanalysis",
				"clusteranalysis",
				"voxelise",
				"ioninfo",
				"annotation",
				"proxigram"
				};

size_t numElements(const vector<const FilterStreamData *> &v, unsigned int mask)
{
	size_t nE=0;
	for(unsigned int ui=0;ui<v.size();ui++)
	{
		if((v[ui]->getStreamType() & mask))
			nE+=v[ui]->getNumBasicObjects();
	}

	return nE;
}


template<>
bool Filter::applyPropertyNow(bool &prop, const std::string &val, bool &needUp)
{
	needUp=false;
	bool tmp;
	if(!boolStrDec(val,tmp))
		return false;
	
	//return true, as technically, we did something OK
	// but erasing the cache, and re-setting the value is pointless
	if(tmp == prop)
		return true;
	
	prop=tmp;
	clearCache();

	needUp=true;
	return true;	
}

template<>
bool Filter::applyPropertyNow(Point3D &prop, const std::string &val, bool &needUp)
{
	needUp=false;

	Point3D newPt;	
	if(!newPt.parse(val))
		return false;
	
	//return true, as technically, we did something OK
	// but erasing the cache, and re-setting the value is pointless
	if(newPt== prop)
		return true;
	
	prop=newPt;
	clearCache();
	needUp=true;
	return true;	
}

template<>
bool Filter::applyPropertyNow(std::string &prop, const std::string &val, bool &needUp)
{
	needUp=false;

	//return true, as it is technically ok that we did this
	if(val == prop)
		return true;
	
	prop=val;
	clearCache();
	needUp=true;
	return true;	
}

void Filter::cacheAsNeeded(FilterStreamData *stream)
{
	if(cache)
	{
		stream->cached=1;
		filterOutputs.push_back(stream);
		cacheOK=true;
	}	
	else
	{
		stream->cached=0;
	}
}

//Used by the setProperties to demultiplex "mux" one values into two separate data points
// this is a bit of a hack, and decreases the available range to each type to 16 bits 
void Filter::demuxKey(unsigned int key, unsigned int &keyType, unsigned int &ionOffset)
{
	keyType = key >> 16;
	ionOffset = key & 0xFFFF;
}

unsigned int Filter::muxKey(unsigned int keyType, unsigned int ionOffset)
{
	unsigned int key;
	key = keyType << 16;
	key|=ionOffset;

	return key;
}

std::string Filter::getErrString(unsigned int errCode) const
{
	//First see if we have a generic error code, before attempting to
	// switch to a specific error code
	std::string errString;
	errString = getBaseErrString(errCode);
	if(!errString.empty())
		return errString;
	return this->getSpecificErrString(errCode);
}

//If we recognise a base error string, check that
std::string Filter::getBaseErrString(unsigned int errCode)
{
	if(errCode == FILTER_ERR_ABORT)
		return TRANS("Aborted");

	return string("");
}

#ifdef DEBUG
bool FilterProperty::checkSelfConsistent() const
{	
	//Filter data type must be known
	ASSERT(type < PROPERTY_TYPE_ENUM_END);
	ASSERT(name.size());

	//Check each item is parseable as its own type
	switch(type)
	{
		case PROPERTY_TYPE_BOOL:
		{
			if(data != "0"  && data != "1")
				return false;
			break;
		}
		case PROPERTY_TYPE_REAL:
		{
			float f;
			if(stream_cast(f,data))
				return false;
			break;
		}
		case PROPERTY_TYPE_COLOUR:
		{
			ColourRGBA rgba;
			if(!rgba.parse(data))
				return false;

			break;
		}
		case PROPERTY_TYPE_CHOICE:
		{
			if(!isMaybeChoiceString(data))
				return false;
			break;
		}
		case PROPERTY_TYPE_POINT3D:
		{
			Point3D p;
			if(!p.parse(data))
				return false;
			break;
		}
		case PROPERTY_TYPE_INTEGER:
		{
			int i;
			if(stream_cast(i,data))
				return false;
			break;
		}
		default:
		{
			//Check for the possibility that a choice string is mistyped
			// - this has happened. However, its possible to get a false positive
			if(isMaybeChoiceString(data))
			{
				WARN(false, "Possible property not set as choice? It seems to be a choice string...");
			}
		}
	}


	return true;
}
#endif

void FilterPropGroup::addProperty(const FilterProperty &prop, size_t group)
{
#ifdef DEBUG
	prop.checkSelfConsistent();
#endif
	if(group >=groupCount)
	{
#ifdef DEBUG
		WARN(!(group > (groupCount+1)),"Appeared to add multiple groups in one go - not wrong, just unusual.");
#endif
		groupCount=std::max(group+1,groupCount);
		groupNames.resize(groupCount,string(""));
	}	

	keyGroupings.push_back(make_pair(prop.key,group));
	properties.push_back(prop);
}

void FilterPropGroup::setGroupTitle(size_t group, const std::string &str)
{
	ASSERT(group <numGroups());
	groupNames[group]=str;
}

void FilterPropGroup::getGroup(size_t targetGroup, vector<FilterProperty> &vec) const
{
	ASSERT(targetGroup<groupCount);
	for(size_t ui=0;ui<keyGroupings.size();ui++)
	{
		if(keyGroupings[ui].second==targetGroup)
		{
			vec.push_back(properties[ui]);
		}
	}

#ifdef DEBUG
	checkConsistent();
#endif
}

bool FilterPropGroup::hasGroup(size_t targetGroup) const
{
	for(size_t ui=0;ui<keyGroupings.size();ui++)
	{
		if(keyGroupings[ui].second==targetGroup)
			return true;
	}

	return false;
}
void FilterPropGroup::getGroupTitle(size_t group, std::string &s) const
{
	ASSERT(group < groupNames.size());
	s = groupNames[group];	
}

const FilterProperty &FilterPropGroup::getPropValue(size_t key) const
{
	for(size_t ui=0;ui<keyGroupings.size();ui++)
	{
		if(keyGroupings[ui].first == key)
			return properties[ui];
	}

	ASSERT(false);
}

bool FilterPropGroup::hasProp(size_t key) const
{
	for(size_t ui=0;ui<keyGroupings.size();ui++)
	{
		if(keyGroupings[ui].first == key)
			return true;
	}

	return false;
}

#ifdef DEBUG
void FilterPropGroup::checkConsistent() const
{
	ASSERT(keyGroupings.size() == properties.size());
	std::set<size_t> s;

	//Check that each key is unique in the keyGroupings list
	for(size_t ui=0;ui<keyGroupings.size();ui++)
	{
		ASSERT(std::find(s.begin(),s.end(),keyGroupings[ui].first) == s.end());
		s.insert(keyGroupings[ui].first);
	}

	//Check that each key in the properties is also unique
	s.clear();
	for(size_t ui=0;ui<properties.size(); ui++)
	{
		ASSERT(std::find(s.begin(),s.end(),properties[ui].key) == s.end());
		s.insert(properties[ui].key);
	}

	for(size_t ui=0;ui<properties.size(); ui++)
	{
		ASSERT(properties[ui].helpText.size());	
	}

	//Check that the group names are the same as the number of groups
	ASSERT(groupNames.size() ==groupCount);


	//check that each group has a name
	for(size_t ui=0;ui<groupNames.size(); ui++)
	{
		ASSERT(!groupNames[ui].empty())
	}
}
#endif



void DrawStreamData::clear()
{
	for(unsigned int ui=0; ui<drawables.size(); ui++)
		delete drawables[ui];
}

DrawStreamData::~DrawStreamData()
{
	clear();
}

#ifdef DEBUG
void DrawStreamData::checkSelfConsistent() const
{
	//Drawable pointers should be unique
	for(unsigned int ui=0;ui<drawables.size();ui++)
	{
		for(unsigned int uj=0;uj<drawables.size();uj++)
		{
			if(ui==uj)
				continue;
			//Pointers must be unique
			ASSERT(drawables[ui] != drawables[uj]);
		}
	}
}
#endif

PlotStreamData::PlotStreamData() :  r(1.0f),g(0.0f),b(0.0f),a(1.0f),
	plotStyle(PLOT_LINE_LINES), logarithmic(false) , useDataLabelAsYDescriptor(true), 
	index((unsigned int)-1)
{
	streamType=STREAM_TYPE_PLOT;
	errDat.mode=PLOT_ERROR_NONE;

	hardMinX=hardMinY=-std::numeric_limits<float>::max();
	hardMaxX=hardMaxY=std::numeric_limits<float>::max();
}

void PlotStreamData::autoSetHardBounds()
{
	if(xyData.size())
	{
		hardMinX=std::numeric_limits<float>::max();
		hardMinY=std::numeric_limits<float>::max();
		hardMaxX=-std::numeric_limits<float>::max();
		hardMaxY=-std::numeric_limits<float>::max();

		for(size_t ui=0;ui<xyData.size();ui++)
		{
			hardMinX=std::min(hardMinX,xyData[ui].first);
			hardMinY=std::min(hardMinY,xyData[ui].second);
			hardMaxX=std::max(hardMinX,xyData[ui].first);
			hardMaxY=std::max(hardMaxY,xyData[ui].second);
		}
	}
	else
	{
		hardMinX=hardMinY=-1;
		hardMaxX=hardMaxY=1;
	}
}

bool PlotStreamData::save(const char *filename) const
{

	std::ofstream f(filename);

	if(!f)
		return false;

	bool pendingEndl = false;
	if(xLabel.size())
	{
		f << xLabel;
		pendingEndl=true;
	}
	if(yLabel.size())
	{
		f << "\t" << yLabel;
		pendingEndl=true;
	}

	if(errDat.mode==PLOT_ERROR_NONE)
	{
		if(pendingEndl)
			f << endl;
		for(size_t ui=0;ui<xyData.size();ui++)
			f << xyData[ui].first << "\t" << xyData[ui].second << endl;
	}
	else
	{
		if(pendingEndl)
		{
			f << "\t" << TRANS("Error") << endl;
		}
		else
			f << "\t\t" << TRANS("Error") << endl;
		for(size_t ui=0;ui<xyData.size();ui++)
			f << xyData[ui].first << "\t" << xyData[ui].second << endl;
	}

	return true;
}

#ifdef DEBUG
void PlotStreamData::checkSelfConsistent() const
{
	//Colour vectors should be the same size
	ASSERT(regionR.size() == regionB.size() && regionB.size() ==regionG.size());

	//region's should have a colour and ID vector of same size
	ASSERT(regionID.size() ==regionR.size());

	//log plots should have hardmin >=0
	ASSERT(!(logarithmic && hardMinY < 0) );

	//hardMin should be <=hardMax
	//--
	ASSERT(hardMinX<=hardMaxX);

	ASSERT(hardMinY<=hardMaxY);
	//--

	//Needs to have a title
	ASSERT(dataLabel.size());

	//If we have regions that can be interacted with, need to have parent
	ASSERT(!(regionID.size() && !regionParent));

	//Must have valid trace style
	ASSERT(plotStyle<PLOT_TYPE_ENUM_END);
	//Must have valid error bar style
	ASSERT(errDat.mode<PLOT_ERROR_ENDOFENUM);

	ASSERT(plotMode <PLOT_MODE_ENUM_END);

	//Must set the "index" for this plot 
	ASSERT(index != (unsigned int)-1);
}
#endif

Plot2DStreamData::Plot2DStreamData()
{
	streamType=STREAM_TYPE_PLOT2D;
	r=g=0.0f;
	b=a=1.0f;

	scatterIntensityLog=false;
}

size_t Plot2DStreamData::getNumBasicObjects() const
{
	if(xyData.size())
		return xyData.size();
	else if (scatterData.size())
		return scatterData.size();
	else
		ASSERT(false);

	return 0;
}

#ifdef DEBUG

void Plot2DStreamData::checkSelfConsistent() const
{
	//only using scatter or xy, not both
	ASSERT(xorFunc(xyData.empty(), scatterData.empty()));

	//no intensity without data
	if(scatterData.empty())
		ASSERT(scatterIntensity.empty());


	ASSERT(plotType < PLOT_TYPE_ENUM_END);
}
void RangeStreamData::checkSelfConsistent() const
{
	if(!rangeFile)
		return;

	ASSERT(rangeFile->getNumIons() == enabledIons.size());

	ASSERT(rangeFile->getNumRanges() == enabledIons.size());

}
#endif

FilterStreamData::FilterStreamData() : parent(0),cached((unsigned int)-1)
{
}

FilterStreamData::FilterStreamData(const Filter  *theParent) : parent(theParent),cached((unsigned int)-1)
{
}


unsigned int IonStreamData::exportStreams(const std::vector<const FilterStreamData * > &selectedStreams,
		const std::string &outFile, unsigned int format)
{

	ASSERT(format < IONFORMAT_ENUM_END);

	//test file open, and truncate file to zero bytes
	std::ofstream f(outFile.c_str(),std::ios::trunc);
	
	if(!f)
		return 1;

	f.close();

	if(format != IONFORMAT_VTK)
	{
		for(unsigned int ui=0; ui<selectedStreams.size(); ui++)
		{
			switch(selectedStreams[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
					const IonStreamData *ionData;
					ionData=((const IonStreamData *)(selectedStreams[ui]));

						//Append this ion stream to the posfile
						IonHit::appendFile(ionData->data,outFile.c_str(),format);
				}
			}
		}
	}
	else
	{
		//we don't have an append function, as VTK's legacy
		// format does not really support this AFAIK.
		//so we accumulate first.
		vector<IonHit> ionvec;
		//--
		unsigned int numIons=0;
		for(unsigned int ui=0; ui<selectedStreams.size(); ui++)
		{
			switch(selectedStreams[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
					numIons+=selectedStreams[ui]->getNumBasicObjects();
					break;
				}
			}
		}
		
		ionvec.reserve(numIons);
		for(unsigned int ui=0;ui<selectedStreams.size();ui++)
		{
			switch(selectedStreams[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
					const IonStreamData *ionData;
					ionData=((const IonStreamData *)(selectedStreams[ui]));
					for(unsigned int uj=0;uj<ionData->data.size();uj++)
						ionvec.push_back(ionData->data[uj]);
					break;
				}
			}
		}

		if(vtk_write_legacy(outFile,VTK_ASCII,ionvec))
			return 1;
		//--
	}
	return 0;
}


IonStreamData::IonStreamData() : 
	r(1.0f), g(0.0f), b(0.0f), a(1.0f), 
	ionSize(2.0f), valueType("Mass-to-Charge (amu/e)")
{
	streamType=STREAM_TYPE_IONS;
}

IonStreamData::IonStreamData(const Filter *f) : FilterStreamData(f), 
	r(1.0f), g(0.0f), b(0.0f), a(1.0f), 
	ionSize(2.0f), valueType("Mass-to-Charge (amu/e)")
{
	streamType=STREAM_TYPE_IONS;
}

void IonStreamData::estimateIonParameters(const std::vector<const FilterStreamData *> &inData)
{

	map<float,unsigned int> ionSizeMap;
	map<vector<float>, unsigned int> ionColourMap;
	std::string lastStr;

	//Sum up the relative frequencies
	for(unsigned int ui=0; ui<inData.size(); ui++)
	{
		if(inData[ui]->getStreamType() != STREAM_TYPE_IONS)
			continue;	

		//Keep a count of the number of times we see a particlar
		// size/colour combination	
		map<float,unsigned int>::iterator itSize;
		map<vector<float> ,unsigned int>::iterator itData;
	
		const IonStreamData* p;	
		p= ((const IonStreamData*)inData[ui]);

		itSize=ionSizeMap.find(p->ionSize);
		if(itSize == ionSizeMap.end())
			ionSizeMap[p->ionSize]=1;	
		else
			ionSizeMap[p->ionSize]++;

		vector<float> tmpRgba;
		tmpRgba.push_back(p->r); tmpRgba.push_back(p->g);
		tmpRgba.push_back(p->b); tmpRgba.push_back(p->a);
		
		itData = ionColourMap.find(tmpRgba);
		if(itData == ionColourMap.end())
			ionColourMap[tmpRgba]=1;	
		else
			ionColourMap[tmpRgba]++;
			
		if(lastStr.empty())
			lastStr=p->valueType ;
		else
			lastStr = "Mixed types";	
		 
	}
	
	const vector<float> *tmp=0;
	size_t min=0;
	//find the most frequent ion colour	
	for(map<vector<float>, unsigned int>::iterator it=ionColourMap.begin();
		it!=ionColourMap.end(); ++it)
	{
		if(it->second > min)
		{
			tmp = &(it->first);
			min=it->second;
		}
	}

	//Find the most frequent ion size
	float tmpSize=1.0;
	min=0;
	for(map<float, unsigned int>::iterator it=ionSizeMap.begin();
		it!=ionSizeMap.end(); ++it)
	{
		if(it->second > min)
		{
			tmpSize = it->first;
			min=it->second;
		}
	}

	ionSize=tmpSize;
	if(tmp && tmp->size() == 4)
	{
		r=(*tmp)[0];
		g=(*tmp)[1];
		b=(*tmp)[2];
		a=(*tmp)[3];
	}


	if(lastStr.size())
		valueType=lastStr;
	else
		valueType.clear();
}

void IonStreamData::estimateIonParameters(const IonStreamData *i)
{
	vector<const FilterStreamData *> v;
	v.push_back(i);
	
	estimateIonParameters(v);
}

void IonStreamData::clear()
{
	data.clear();
}

IonStreamData *IonStreamData::cloneSampled(float fraction) const

{
	IonStreamData *out = new IonStreamData;

	out->r=r;
	out->g=g;
	out->b=b;
	out->a=a;
	out->ionSize=ionSize;
	out->valueType=valueType;
	out->parent=parent;
	out->cached=0;


	out->data.reserve(fraction*data.size()*0.9f);

	
	RandNumGen rng;
	rng.initTimer();
	for(size_t ui=0;ui<data.size();ui++)
	{
		if(rng.genUniformDev() < fraction)
			out->data.push_back(data[ui]);	
	}

	return out;
}

size_t IonStreamData::getNumBasicObjects() const
{
	return data.size();
}



VoxelStreamData::VoxelStreamData() : representationType(VOXEL_REPRESENT_POINTCLOUD),
	r(1.0f),g(0.0f),b(0.0f),a(0.3f), splatSize(2.0f),isoLevel(0.05f)
{
	streamType=STREAM_TYPE_VOXEL;
	data = new Voxels<float>;
}

VoxelStreamData::VoxelStreamData(const Filter *f) : FilterStreamData(f), representationType(VOXEL_REPRESENT_POINTCLOUD),
	r(1.0f),g(0.0f),b(0.0f),a(0.3f), splatSize(2.0f),isoLevel(0.05f)
{
	streamType=STREAM_TYPE_VOXEL;
	data = new Voxels<float>;
}

VoxelStreamData::~VoxelStreamData()
{
	if(data)
		delete data;
}

size_t VoxelStreamData::getNumBasicObjects() const
{
	return data->getSize(); 
}

void VoxelStreamData::clear()
{
	data->clear();
}

////////////// openvdb ////////////////////////////////////////////////////////////

OpenVDBGridStreamData::OpenVDBGridStreamData() : representationType(VOXEL_REPRESENT_ISOSURF),
	r(0.5f),g(0.5f),b(0.5f),a(1.0f), isovalue(0.07f), voxelsize(2.0f)
{
	streamType=STREAM_TYPE_OPENVDBGRID;
	openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create();	
}

OpenVDBGridStreamData::OpenVDBGridStreamData(const Filter *f) : FilterStreamData(f), representationType(VOXEL_REPRESENT_ISOSURF),
	r(0.5f),g(0.5f),b(0.5f),a(1.0f), isovalue(0.07f), voxelsize(2.0f)
{
	streamType=STREAM_TYPE_OPENVDBGRID;
	openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create();
}

OpenVDBGridStreamData::~OpenVDBGridStreamData()
{
}

size_t OpenVDBGridStreamData::getNumBasicObjects() const
{
	return grid->activeVoxelCount(); 
}

void OpenVDBGridStreamData::clear()
{
	grid->clear();
}

//////////////////////////////////////////////////////////////////////////////////////////////

RangeStreamData::RangeStreamData() : rangeFile(0)
{
	streamType = STREAM_TYPE_RANGE;
}

RangeStreamData::RangeStreamData(const Filter *f) : FilterStreamData(f), rangeFile(0)
{
	streamType = STREAM_TYPE_RANGE;
}

bool RangeStreamData::save(const char *filename, size_t format) const
{
	return !rangeFile->write(filename,format);
}

Filter::Filter() : cache(true), cacheOK(false)
{
	COMPILE_ASSERT( THREEDEP_ARRAYSIZE(STREAM_NAMES) == NUM_STREAM_TYPES);
	for(unsigned int ui=0;ui<NUM_STREAM_TYPES;ui++)
		numStreamsLastRefresh[ui]=0;
}

Filter::~Filter()
{
    if(cacheOK)
	    clearCache();

    clearDevices();
}

void Filter::setPropFromRegion(unsigned int method, unsigned int regionID, float newPos)
{
	//Must overload if using this function.
	ASSERT(false);
}

void Filter::clearDevices()
{
	for(unsigned int ui=0; ui<devices.size(); ui++)
	{
		delete devices[ui];
	}
	devices.clear();
}

void Filter::clearCache()
{
	using std::endl;
	cacheOK=false; 

	//Free mem held by objects	
	for(unsigned int ui=0;ui<filterOutputs.size(); ui++)
	{
		ASSERT(filterOutputs[ui]->cached);
		delete filterOutputs[ui];
	}

	filterOutputs.clear();	
}

bool Filter::haveCache() const
{
	return cacheOK;
}

void Filter::getSelectionDevices(vector<SelectionDevice *> &outD) const
{
	outD.resize(devices.size());

	std::copy(devices.begin(),devices.end(),outD.begin());

#ifdef DEBUG
	for(unsigned int ui=0;ui<outD.size();ui++)
	{
		ASSERT(outD[ui]);
		//Ensure that pointers coming in are valid, by attempting to perform an operation on them
		vector<std::pair<const Filter *,SelectionBinding> > tmp;
		outD[ui]->getModifiedBindings(tmp);
		tmp.clear();
	}
#endif
}

void Filter::propagateCache(vector<const FilterStreamData *> &getOut) const
{

	ASSERT(filterOutputs.size());
	//Convert to const pointers (C++ workaround)
	//--
	vector<const FilterStreamData *> tmpOut;
	tmpOut.resize(filterOutputs.size());
	std::copy(filterOutputs.begin(),filterOutputs.end(),tmpOut.begin());
	//--

	propagateStreams(tmpOut,getOut);
}

void Filter::propagateStreams(const vector<const FilterStreamData *> &dataIn,
		vector<const FilterStreamData *> &dataOut,size_t mask,bool invertMask)
{
	//Propagate any inputs that we don't normally block
	if(invertMask)
		mask=~mask;
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() & mask)
			dataOut.push_back(dataIn[ui]);
	}
}

unsigned int Filter::collateIons(const vector<const FilterStreamData *> &dataIn,
				vector<IonHit> &outVector, ProgressData &prog,
				size_t totalDataSize)
{
	if(totalDataSize==(size_t)-1)
		totalDataSize=numElements(dataIn,STREAM_TYPE_IONS);


	ASSERT(totalDataSize== numElements(dataIn,STREAM_TYPE_IONS));


	outVector.resize(totalDataSize);
	size_t offset=0;


	for(unsigned int ui=0;ui<dataIn.size() ;ui++)
	{
		switch(dataIn[ui]->getStreamType())
		{
			case STREAM_TYPE_IONS: 
			{
				const IonStreamData *d;
				d=((const IonStreamData *)dataIn[ui]);

				size_t dataSize=d->data.size();

				#pragma omp parallel for if(dataSize > OPENMP_MIN_DATASIZE)
				for(size_t ui=0;ui<dataSize; ui++)
					outVector[offset+ui]=d->data[ui];

				if(Filter::wantAbort)
					return FILTER_ERR_ABORT;
				offset+=d->data.size();


				break;
			}
		}
	}

	return 0;
}
				

void Filter::updateOutputInfo(const std::vector<const FilterStreamData *> &dataOut)
{
	//Reset the number of output streams to zero
	for(unsigned int ui=0;ui<NUM_STREAM_TYPES;ui++)
		numStreamsLastRefresh[ui]=0;
	
	//Count the different types of output stream.
	for(unsigned int ui=0;ui<dataOut.size();ui++)
	{
		ASSERT(getBitNum(dataOut[ui]->getStreamType()) < NUM_STREAM_TYPES);
		numStreamsLastRefresh[getBitNum(dataOut[ui]->getStreamType())]++;
	}
}

unsigned int Filter::getNumOutput(unsigned int streamType) const
{
	ASSERT(streamType < NUM_STREAM_TYPES);
	return numStreamsLastRefresh[streamType];
}

std::string Filter::getUserString() const
{
	if(userString.size())
		return userString;
	else
		return typeString();
}

void Filter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	dataOut.resize(dataIn.size());
	std::copy(dataIn.begin(),dataIn.end(),dataOut.begin());
}

ProgressData::ProgressData()
{
	step=0;
	maxStep=0;
	curFilter=0;
	filterProgress=0;
	totalProgress=0;
	totalNumFilters=0;
}


bool ProgressData::operator==( const ProgressData &oth) const
{
	if(filterProgress!=oth.filterProgress ||
		(totalProgress!=oth.totalProgress) ||
		(totalNumFilters!=oth.totalNumFilters) ||
		(step!=oth.step) ||
		(maxStep!=oth.maxStep) ||
		(curFilter!=oth.curFilter )||
		(stepName!=oth.stepName) )
		return false;

	return true;
}

const ProgressData &ProgressData::operator=(const ProgressData &oth)
{
	filterProgress=oth.filterProgress;
	totalProgress=oth.totalProgress;
	totalNumFilters=oth.totalNumFilters;
	step=oth.step;
	maxStep=oth.maxStep;
	curFilter=oth.curFilter;
	stepName=oth.stepName;

	return *this;
}

#ifdef DEBUG
extern Filter *makeFilter(unsigned int ui);
extern Filter *makeFilter(const std::string &s);

bool Filter::boolToggleTests()
{
	//Each filter should allow user to toggle any boolean value
	// here we just test the default visible ones
	for(unsigned int ui=0;ui<FILTER_TYPE_ENUM_END;ui++)
	{
		Filter *f;
		f=makeFilter(ui);
		
		FilterPropGroup propGroupOrig;

		//Grab all the default properties
		f->getProperties(propGroupOrig);


		for(size_t ui=0;ui<propGroupOrig.numProps();ui++)
		{
			FilterProperty p;
			p=propGroupOrig.getNthProp(ui);
			//Only consider boolean values 
			if( p.type != PROPERTY_TYPE_BOOL )
				continue;

			//Toggle value to other status
			if(p.data== "0")
				p.data= "1";
			else if (p.data== "1")
				p.data= "0";
			else
			{
				ASSERT(false);
			}


			//set value to toggled version
			bool needUp;
			f->setProperty(p.key,p.data,needUp);

			//Re-get properties to find altered property 
			FilterPropGroup propGroup;
			f->getProperties(propGroup);
			
			FilterProperty p2;
			p2 = propGroup.getPropValue(p.key);
			//Check the property values
			TEST(p2.data == p.data,"displayed bool property can't be toggled");
			
			//Toggle value back to original status
			if(p2.data== "0")
				p2.data= "1";
			else 
				p2.data= "0";
			//re-set value to toggled version
			f->setProperty(p2.key,p2.data,needUp);
		
			//Re-get properties to see if original value is restored
			FilterPropGroup fp2;
			f->getProperties(fp2);
			p = fp2.getPropValue(p2.key);

			TEST(p.data== p2.data,"failed trying to set bool value back to original after toggle");
		}
		delete f;
	}

	return true;
}

bool Filter::helpStringTests()
{
	//Each filter should provide help text for each property
	// here we just test the default visible ones
	for(unsigned int ui=0;ui<FILTER_TYPE_ENUM_END;ui++)
	{
		Filter *f;
		f=makeFilter(ui);


		
		FilterPropGroup propGroup;
		f->getProperties(propGroup);
		for(size_t ui=0;ui<propGroup.numProps();ui++)
		{
			FilterProperty p;
			p=propGroup.getNthProp(ui);
			TEST(p.helpText.size(),"Property help text must not be empty");
		}
		delete f;
	}

	return true;
}
#endif



