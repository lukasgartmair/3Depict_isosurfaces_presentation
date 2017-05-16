/*
 *	filter.h - Data filter header file. 
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
#ifndef FILTER_H
#define FILTER_H

class Filter;
class FilterStreamData;

class ProgressData;
class RangeFileFilter;

#include "APT/ionhit.h"
#include "APT/APTFileIO.h"

#include "APT/APTRanges.h"
#include "common/constants.h"

#include "gl/select.h"
#include "gl/drawables.h"

#include "common/stringFuncs.h"
#include "common/array2D.h"

#include "filters/openvdb_includes.h"


template<typename T>
class Voxels;

//ifdef inclusion as there is some kind of symbol clash...
#ifdef ATTRIBUTE_PRINTF
	#pragma push_macro("ATTRIBUTE_PRINTF")
	#include <libxml/xmlreader.h>
	#pragma pop_macro(" ATTRIBUTE_PRINTF")
#else
	#include <libxml/xmlreader.h>
	#undef ATTRIBUTE_PRINTF
#endif



#include <wx/propgrid/propgrid.h>

const unsigned int NUM_CALLBACK=50000;

const unsigned int IONDATA_SIZE=4;

//!Filter types  -- must match array FILTER_NAMES
enum
{
	FILTER_TYPE_DATALOAD,
	FILTER_TYPE_IONDOWNSAMPLE,
	FILTER_TYPE_RANGEFILE,
	FILTER_TYPE_SPECTRUMPLOT, 
	FILTER_TYPE_IONCLIP,
	FILTER_TYPE_IONCOLOURFILTER,
	FILTER_TYPE_PROFILE,
	FILTER_TYPE_BOUNDBOX,
	FILTER_TYPE_TRANSFORM,
	FILTER_TYPE_EXTERNALPROC,
	FILTER_TYPE_SPATIAL_ANALYSIS,
	FILTER_TYPE_CLUSTER_ANALYSIS,
	FILTER_TYPE_VOXELS,
	FILTER_TYPE_IONINFO,
	FILTER_TYPE_ANNOTATION,
	FILTER_TYPE_PROXIGRAM,
	FILTER_TYPE_ENUM_END // not a filter. just end of enum
};


extern const char *FILTER_NAMES[];



//Stream data types. note that bitmasks are occasionally used, so we are limited in
//the number of stream types that we can have.
//Current bitmask using functions are
//	VisController::safeDeleteFilterList
const unsigned int NUM_STREAM_TYPES=7;
const unsigned int STREAMTYPE_MASK_ALL= ((1<<(NUM_STREAM_TYPES)) -1 ) & 0x000000FF;
enum
{
	STREAM_TYPE_IONS=1,
	STREAM_TYPE_PLOT=2,
	STREAM_TYPE_PLOT2D=4,
	STREAM_TYPE_DRAW=8,
	STREAM_TYPE_RANGE=16,
	STREAM_TYPE_VOXEL=32,
	STREAM_TYPE_OPENVDBGRID = 64
};


//Keys for binding IDs
enum
{
	BINDING_CYLINDER_RADIUS=1,
	BINDING_SPHERE_RADIUS,
	BINDING_CYLINDER_ORIGIN,
	BINDING_SPHERE_ORIGIN,
	BINDING_PLANE_ORIGIN,
	BINDING_CYLINDER_DIRECTION,
	BINDING_PLANE_DIRECTION,
	BINDING_RECT_TRANSLATE,
	BINDING_RECT_CORNER_MOVE
};

extern const char *STREAM_NAMES[];


//Representations
enum 
{
	//VoxelStreamData
	VOXEL_REPRESENT_POINTCLOUD,
	VOXEL_REPRESENT_ISOSURF,
	VOXEL_REPRESENT_AXIAL_SLICE,
	VOXEL_REPRESENT_END
};

//Error codes for each of the filters. 
//These can be passed to the getErrString() function for
//a human readable error message
//---

enum
{
	FILE_TYPE_NULL,
	FILE_TYPE_XML,
	FILE_TYPE_POS
};


//!Generic filter error codes
enum
{
	FILTER_ERR_ABORT = 1000000,
};

//---
//

//!Return the number of elements in a vector of filter data - i.e. the sum of the number of objects within each stream. Only masked streams (STREAM_TYPE_*) will be counted
size_t numElements(const std::vector<const FilterStreamData *> &vm, unsigned int mask=STREAMTYPE_MASK_ALL);



//!Abstract base class for data types that can propagate through filter system
class FilterStreamData
{
	protected:
		unsigned int streamType;
	public:
		//!Parent filter pointer
		const Filter *parent;

		//!Tells us if the filter has cached this data for later use. 
		//this is a boolean value, but not declared as such, as there 
		//are debug traps to tell us if this is not set by looking for non-boolean values.
		unsigned int cached;

		FilterStreamData();
		FilterStreamData(const Filter *);
		virtual ~FilterStreamData() {}; 
		virtual size_t getNumBasicObjects() const =0;
		//!Returns an integer unique to the class to identify type (yes rttid...)
		virtual unsigned int getStreamType() const {return streamType;} ;
		//!Free mem held by objects
		virtual void clear()=0;

#ifdef DEBUG
		//Cross-checks fields to determine if (best guess)
		///data structure has a sane combination of values
		virtual void checkSelfConsistent() const {}
#endif

};

class FilterProperty
{
	public:
	//!Human readable short help (tooltip) for each of the keys
	std::string helpText;
	//!Data type for this element
	unsigned int type;
	//!Unique key value for this element
	size_t key;
	//!Property data
	std::string data;
	//!Secondary property data
	//	- eg for file, contains wildcard mask for filename
	std::string dataSecondary;
	//!name of property
	std::string name;

	bool checkSelfConsistent() const;
};

class FilterPropGroup
{
	private:
		//!The groupings for the keys in contained properties.
		// First entry is the key ID, second is he group that it belongs to
		std::vector<std::pair<unsigned int,unsigned int> > keyGroupings;
		//!Names for each group of keys.
		std::vector<std::string> groupNames;
		//!Key information
		std::vector<FilterProperty > properties;
		
		size_t groupCount;
	public:
		FilterPropGroup() { groupCount=0;}
		//!Add a property to a grouping
		void addProperty(const FilterProperty &prop, size_t group);


		//!Set the title text for a particular group
		void setGroupTitle(size_t group, const std::string &str);

		//!Obtain the title of the nth group
		void getGroupTitle(size_t group, std::string &str) const ;

		//Obtain a property by its key
		const FilterProperty &getPropValue(size_t key) const;

		//Retrieve the number of groups 
		size_t numGroups() const { return groupCount;};

		bool hasProp(size_t key) const;
		//Get number of keys
		size_t numProps() const { return properties.size(); }
		//Erase all stored  information 
		void clear() 
			{ groupNames.clear();keyGroupings.clear(); properties.clear();
				groupCount=0;}

		//!Grab all properties from the specified group
		void getGroup(size_t group, std::vector<FilterProperty> &groupVec) const;
		
		//Confirm a particular group exists
		bool hasGroup(size_t group) const;
		//!Get the nth key
		const FilterProperty &getNthProp(size_t nthProp) const { return properties[nthProp];};

#ifdef DEBUG
		void checkConsistent() const; 
#endif
	
};

//!Point with m-t-c value data
class IonStreamData : public FilterStreamData
{
public:
	IonStreamData();
	IonStreamData(const Filter *f);
	void clear();

	//Sample the data vector to the specified fraction
	void sample(float fraction);

	//Duplicate this object, but only using a sampling of the data
	// vector. The retuned object must be deleted by the
	// caller. Cached status is *not* duplicated
	IonStreamData *cloneSampled(float fraction) const;

	size_t getNumBasicObjects() const;

	//Ion colour + transparancy in [0,1] colour space. 
	float r,g,b,a;

	//Ion Size in 2D opengl units
	float ionSize;
	
	//!The name for the type of data -- nominally "mass-to-charge"
	std::string valueType;
	
	//!Apply filter to input data stream	
	std::vector<IonHit> data;

	//!export given filterstream data pointers as ion data
	static unsigned int exportStreams(const std::vector<const FilterStreamData *> &selected, 
							const std::string &outFile, unsigned int format=IONFORMAT_POS);

	//!Use heuristics to guess best display parameters for this ionstream. May attempt to leave them alone 
	void estimateIonParameters(const std::vector<const FilterStreamData *> &inputData);
	void estimateIonParameters(const IonStreamData *inputFilter);
};

//!Point with m-t-c value data
class VoxelStreamData : public FilterStreamData
{
public:
	VoxelStreamData();
	~VoxelStreamData();
	VoxelStreamData( const Filter *f);
	size_t getNumBasicObjects() const ;
	void clear();
	
	unsigned int representationType;
	float r,g,b,a;
	float splatSize;
	float isoLevel;
	//!Apply filter to input data stream	
	Voxels<float> *data;
		
};

//////////// OPENVDB ////////////////////////////


// OpenVDB Grid object data
class OpenVDBGridStreamData : public FilterStreamData
{
public:
	OpenVDBGridStreamData();
	~OpenVDBGridStreamData();
	OpenVDBGridStreamData(const Filter *f);
	size_t getNumBasicObjects() const ;
	void clear();

	unsigned int representationType;
	float r,g,b,a;
	double isovalue;
	float voxelsize;
	//!Apply filter to input data stream	
	openvdb::FloatGrid::Ptr grid;	
};

///////////////////////////////////////

//!Plotting data
class PlotStreamData : public FilterStreamData
{
	public:
		PlotStreamData();
		PlotStreamData(const Filter *f);

		bool save(const char *filename) const; 

		//erase plot contents	
		void clear() {xyData.clear();};
		//Get data size
		size_t getNumBasicObjects() const { return xyData.size();};

		//Use the contained XY data to set hard plot bounds
		void autoSetHardBounds();
		//plot colour
		float r,g,b,a;
		//plot trace mode - enum PLOT_TRACE
		unsigned int plotStyle;
		//plot mode - enum PLOT_MODE
		unsigned int plotMode;

		//use logarithmic mode?
		bool logarithmic;
		//title for data
		std::string dataLabel;
		//Label for X, Y axes
		std::string xLabel,yLabel;

		//!When showing raw XY data, is the data 
		// label a better descriptor of Y than the y-label?
		bool useDataLabelAsYDescriptor;

		//!XY data pairs for plotting curve
		std::vector<std::pair<float,float> > xyData;
		//!Rectangular marked regions
		std::vector<std::pair<float,float> > regions;
		std::vector<std::string> regionTitle;
		//!Region colours
		std::vector<float> regionR,regionB,regionG;

		//!Region indices from parent region
		std::vector<unsigned int> regionID;

		//!Region parent filter pointer, used for matching interaction 
		// with region to parent property
		Filter *regionParent;
		//!Parent filter index
		unsigned int index;
		//!Error bar mode
		PLOT_ERROR errDat;
		
		//!Hard bounds that cannot be exceeded when drawing plot
		float hardMinX,hardMaxX,hardMinY,hardMaxY;

#ifdef DEBUG
		//Cross-checks fields to determine if (best guess)
		///data structure has a sane combination of values
		virtual void checkSelfConsistent() const; 
#endif

};

//!2D Plotting data
class Plot2DStreamData : public FilterStreamData
{
	public:
		Plot2DStreamData();
		Plot2DStreamData(const Filter *f);

		//erase plot contents	
		void clear() {xyData.clear();};
			
		size_t getNumBasicObjects() const; 
		//title for data
		std::string dataLabel;
		//Label for X, Y axes
		std::string xLabel,yLabel;

		unsigned int plotType;

		//!Structured XY data pairs for plotting curve
		Array2D<float> xyData;
		//Only rqeuired for xy plots
		float xMin,xMax,yMin,yMax;
	

		float r,g,b,a;
	
		//!Unstructured XY points
		std::vector<std::pair<float,float> > scatterData;
		//optional intensity data for scatter plots
		std::vector<float> scatterIntensity;
	
		//Do we want to plot the scatter intensity in lgo mode?
		bool scatterIntensityLog;	

		//!Parent filter index
		unsigned int index;

#ifdef DEBUG
		void checkSelfConsistent() const;
#endif
};

//!Drawable objects, for 3D decoration. 
class DrawStreamData: public FilterStreamData
{
	public:
		//!Vector of 3D objects to draw.
		std::vector<DrawableObj *> drawables;
		//!constructor
		DrawStreamData(){ streamType=STREAM_TYPE_DRAW;};
		DrawStreamData(const Filter *f){ streamType=STREAM_TYPE_DRAW;};
		//!Destructor
		~DrawStreamData();
		//!Returns 0, as this does not store basic object types -- i.e. is not for data storage per se.
		size_t getNumBasicObjects() const { return 0; }

		//!Erase the drawing vector, deleting its components
		void clear();
#ifdef DEBUG
		//Cross-checks fields to determine if (best guess)
		///data structure has a sane combination of values
		void checkSelfConsistent() const; 
#endif
};

//!Range file propagation
class RangeStreamData :  public FilterStreamData
{
	public:
		//!range file filter from whence this propagated. Do not delete[] pointer at all, this class does not OWN the range data
		//it merely provides access to existing data.
		RangeFile *rangeFile;
		//Enabled ranges from source filter
		std::vector<char> enabledRanges;
		//Enabled ions from source filter 
		std::vector<char> enabledIons;

		
		//!constructor
		RangeStreamData();
		RangeStreamData(const Filter *f);
		//!Destructor
		~RangeStreamData() {};
		//!save the range data to a file
		bool save(const char *filename, size_t format) const; 
		//!Returns 0, as this does not store basic object types -- i.e. is not for data storage per se.
		size_t getNumBasicObjects() const { return 0; }

		//!Unlink the pointer
		void clear() { rangeFile=0;enabledRanges.clear();enabledIons.clear();};

#ifdef DEBUG
		void checkSelfConsistent() const ; 
#endif
};

//!Abstract base filter class.
class Filter
{
	protected:

		bool cache, cacheOK;
		static bool strongRandom;



		//!Array of the number of streams propagated on last refresh
		//This is initialised to -1, which is considered invalid
		unsigned int numStreamsLastRefresh[NUM_STREAM_TYPES];
	

		//!Temporary console output. Should be only nonzero size if messages are present
		//after refresh, until cache is cleared
		std::vector<std::string> consoleOutput;
		//!User settable labelling string (human readable ID, etc etc)
		std::string userString;
		//Filter output cache
		std::vector<FilterStreamData *> filterOutputs;
		//!User interaction "Devices" associated with this filter
		std::vector<SelectionDevice *> devices;



		//Collate ions from filterstream data into an ionhit vector
		static unsigned int collateIons(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<IonHit> &outVector, ProgressData &prog, size_t totalDataSize=(size_t)-1);

		//!Propagate the given input data to an output vector
		static void propagateStreams(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut,size_t mask=STREAMTYPE_MASK_ALL,bool invertMask=false) ;

		//!Propagate the cache into output
		void propagateCache(std::vector<const FilterStreamData *> &dataOut) const;

		//Set a property, without any checking of the new value 
		// -clears cache on change
		// - and skipping if no actual change between old and new prop
		// returns true if change applied OK.
		template<class T>	
		bool applyPropertyNow(T &oldProp,const std::string &newVal, bool &needUp);
	

		//place a stream object into the filter cache, if required
		// does not place object into filter output - you need to do that yourself
		void cacheAsNeeded(FilterStreamData *s); 

		//!Get the generic (applies to any filter) error codes
		static std::string getBaseErrString(unsigned int errCode);

		//!Get the per-filter error codes
		virtual std::string getSpecificErrString(unsigned int errCode) const=0;

	
	public:	
		Filter() ;
		virtual ~Filter();
		//Abort pointer . This must be  nonzero during filter refreshes
		static ATOMIC_BOOL *wantAbort;

		//Pure virtual functions
		//====
		//!Duplicate filter contents, excluding cache.
		virtual Filter *cloneUncached() const = 0;

		//!Apply filter to new data, updating cache as needed. Vector of returned pointers must be deleted manually, first checking ->cached.
		virtual unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut,
				ProgressData &progress ) =0;
		//!Erase cache
		virtual void clearCache();

		//!Erase any active devices
		virtual void clearDevices(); 

		//!Get (approx) number of bytes required for cache
		virtual size_t numBytesForCache(size_t nObjects) const =0;

		//!return type ID
		virtual unsigned int getType() const=0;

		//!Return filter type as std::string
		virtual std::string typeString()const =0;
		
		//!Get the properties of the filter, in key-value form. First vector is for each output.
		virtual void getProperties(FilterPropGroup &propertyList) const =0;

		//!Set the properties for the nth filter, 
		//!needUpdate tells us if filter output changes due to property set
		//Note that if you modify a result without clearing the cache,
		//then any downstream decision based upon that may not be noted in an update
		//Take care.
		virtual bool setProperty(unsigned int key,
			const std::string &value, bool &needUpdate) = 0;

		//!Get the human readable error string associated with a particular error code during refresh(...). Do *not* override this for specific filter errors. Override getSpecificErrString
		std::string getErrString(unsigned int code) const;

		//!Dump state to output stream, using specified format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		virtual bool writeState(std::ostream &f, unsigned int format,
			       	unsigned int depth=0) const = 0;
	
		//!Read state from XML  stream, using xml format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		virtual bool readState(xmlNodePtr& n, const std::string &packDir="") = 0; 
		
		//!Get the bitmask encoded list of filterStreams that this filter blocks from propagation.
		// i.e. if this filterstream is passed to refresh, it is not emitted.
		// This MUST always be consistent with ::refresh for filters current state.
		virtual unsigned int getRefreshBlockMask() const =0; 
		
		//!Get the bitmask encoded list of filterstreams that this filter emits from ::refresh.
		// This MUST always be consistent with ::refresh for filters current state.
		virtual unsigned int getRefreshEmitMask() const = 0;


		//!Mask of filter streams that will be examined by the filter in its computation.
		// note that output may be emitted as a pass-through even if the use is not set
		// - check emitmask if needed
		virtual unsigned int getRefreshUseMask() const =0;

		//====
	
		//!Return the unique name for a given filter -- DO NOT TRANSLATE	
		std::string trueName() const { return FILTER_NAMES[getType()];};



		//!Initialise the filter's internal state using limited filter stream data propagation
		//NOTE: CONTENTS MAY NOT BE CACHED.
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);


		//!Return the XML elements that refer to external entities (i.e. files) which do not move with the XML files. At this time, only files are supported. These will be looked for on the filesystem and moved as needed 
		virtual void getStateOverrides(std::vector<std::string> &overrides) const {}; 

		//!Enable/disable caching for this filter
		void setCaching(bool enableCache) {cache=enableCache;};
		
		//!Have cached output data?
		bool haveCache() const;
		

		//!Return a user-specified string, or just the typestring if user set string not active
		virtual std::string getUserString() const ;
		//!Set a user-specified string return value is 
		virtual void setUserString(const std::string &str) { userString=str;}; 
		

		//!Modified version of writeState for packaging. By default simply calls writeState.
		//value overrides override the values returned by getStateOverrides. In order.	
		virtual bool writePackageState(std::ostream &f, unsigned int format,
				const std::vector<std::string> &valueOverrides,unsigned int depth=0) const {return writeState(f,format,depth);};
		


		//!Get the selection devices for this filter. MUST be called after refresh()
		/*No checking is done that the selection devices will not interfere with one
		 * another at this level (for example setting two devices on one primitve,
		 * with the same mouse/key bindings). So dont do that.
		 */
		void getSelectionDevices(std::vector<SelectionDevice *> &devices) const;


		//!Update the output statistics for this filter (num items of streams of each type output)
		void updateOutputInfo(const std::vector<const FilterStreamData *> &dataOut);

		//!Set the binding value for a float
		virtual void setPropFromBinding(const SelectionBinding &b)=0;
		
		//!Set a region update
		virtual void setPropFromRegion(unsigned int method, unsigned int regionID, float newPos);
		
		//!Can this filter perform actions that are potentially a security concern?
		virtual bool canBeHazardous() const {return false;} ;

		//!Get the number of outputs for the specified type during the filter's last refresh
		unsigned int getNumOutput(unsigned int streamType) const;

		//!Get the filter messages from the console. To erase strings, either call erase, or erase cahche
		void getConsoleStrings(std::vector<std::string > &v) const { v=consoleOutput;};
		void clearConsole() { consoleOutput.clear();};

		//!Should filters use strong randomisation (where applicable) or not?
		static void setStrongRandom(bool strongRand) {strongRandom=strongRand;}; 

		//Check to see if the filter needs to be refreshed 
		virtual bool monitorNeedsRefresh() const { return false;};

		//Are we a pure data source  - i.e. can function with no input
		virtual bool isPureDataSource() const { return false;};

		//Can we be a useful filter, even if given no input specified by the Use mask?
		virtual bool isUsefulAsAppend() const { return false;}
	
		template<typename T>	
		static void getStreamsOfType(const std::vector<const FilterStreamData *> &vec, std::vector<const T *> &dataOut);




#ifdef DEBUG
		//!Run all the registered unit tests for this filter
		virtual bool runUnitTests() { std::cerr << "No test for " << typeString() << std::endl; return true;} ;
		//!Is the filter caching?
		bool cacheEnabled() const {return cache;};

		static bool boolToggleTests() ;
		
		static bool helpStringTests() ;
#endif
		

//These functions are private for non-debug builds, to allow unit tests to access these
#ifndef DEBUG
	protected:
#endif
		//!Hack to merge/extract two bits of information into a single property key.
		//It does this by abusing some bitshifting, to make use of usually unused key range
		static void demuxKey(unsigned int key, unsigned int &keyType, unsigned int &ionOffset);
		static unsigned int muxKey(unsigned int keyType, unsigned int ionOffset);

};

template<typename T>
void Filter::getStreamsOfType(const std::vector<const FilterStreamData *> &vec, std::vector<const T *> &dataOut)
{
	T dummyInstance;
	for(size_t ui=0;ui<vec.size() ; ui++)
	{
		if(vec[ui]->getStreamType() == dummyInstance.getStreamType())
			dataOut.push_back((const T*)vec[ui]);
	}
}

//Template specialisations & def for  applyPropertyNow
//--
template<>
bool Filter::applyPropertyNow(Point3D &prop, const std::string &val, bool &needUp);

template<>
bool Filter::applyPropertyNow(bool &prop, const std::string &val, bool &needUp);

template<>
bool Filter::applyPropertyNow(std::string &prop, const std::string &val, bool &needUp);

template<class T>
bool Filter::applyPropertyNow(T &prop, const std::string &val, bool &needUp)
{
	// no update initially needed
	needUp=false;

	//convert to type T
	std::string s;
	s=stripWhite(val);
	T tmp;
	if(stream_cast(tmp,s))
		return false;
	
	//return true, as it is technically ok that we assign to self.
	// needUp however stays false, as the property is the same.
	if(tmp == prop)
		return true;
	
	prop=tmp;
	clearCache();
	needUp=true;
	return true;	
}
//--

//!Class that tracks the progress of scene updates
class ProgressData
{
	public:
		//!Progress of filter (out of 100, or -1 for no progress information) for current filter
		unsigned int filterProgress;
		//!Number of filters (n) that we have processed (n out of m filters)
		unsigned int totalProgress;

		//!number of filters which need processing for this update
		unsigned int totalNumFilters;

		//!Current step
		unsigned int step;
		//!Maximum steps
		unsigned int maxStep;
		
		//!Pointer to the current filter that is being updated. 
		const Filter *curFilter;

		//!Name of current operation, if specified
		std::string stepName;

		ProgressData(); 

		bool operator==(const ProgressData &o) const;
		const ProgressData &operator=(const ProgressData &o);

		void reset() { filterProgress=(unsigned int) -1; totalProgress=step=maxStep=0;curFilter=0; totalNumFilters=1; stepName.clear();};
		void clock() { filterProgress=(unsigned int)-1; step=maxStep=0;curFilter=0;totalProgress++; stepName.clear();};
};



#endif
