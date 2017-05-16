/*
 *	LukasAnalysis.h - Compute 3D binning (voxelisation) of point clouds
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
#ifndef Proxigram_H
#define Proxigram_H
#include "../filter.h"

#include "common/voxels.h"

#include "../../common/translation.h"

#include "openvdb_includes.h"
#include "contribution_transfer_function_TestSuite/CTF_functions.h"

//!Filter that does voxelisation for various primitives (copied from CompositionFilter)
class ProxigramFilter : public Filter
{
private:
	const static size_t INDEX_LENGTH = 3;

	//Enabled ions for numerator/denom
	std::vector<char> enabledIons[2];

	float max_distance; // nm

	float voxelsize_levelset;
	float shell_width;
	bool weight_factor;
	
	//!density-based or count-based	
	unsigned int normaliseType;
	bool numeratorAll, denominatorAll;
	//This is filter's enabled ranges
	RangeStreamData *rsdIncoming;

public:
	ProxigramFilter();
	~ProxigramFilter() { if(rsdIncoming) delete rsdIncoming;}
	//!Duplicate filter contents, excluding cache.
	Filter *cloneUncached() const;

	virtual void clearCache();
	
	//!Get approx number of bytes for caching output
	size_t numBytesForCache(size_t nObjects) const;

	unsigned int getType() const { return FILTER_TYPE_PROXIGRAM;};
	
	virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &dataOut);
	//!update filter
	unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
						 std::vector<const FilterStreamData *> &getOut, 
						 ProgressData &progress);
	
	virtual std::string typeString() const { return std::string(TRANS("Proxigram"));};

	//!Get the human-readable options for the normalisation, based upon enum 
	static std::string getNormaliseTypeString(int type);
	//!Get the human-readable options for filtering, based upon enum 
	static std::string getFilterTypeString(int type);
	//!Get the human-readable options for the visual representation (enum)
	static std::string getRepresentTypeString(int type);
	//!Get the human-readable options for boundary behaviour during filtering, based upon enum 
	static std::string getFilterBoundTypeString(int type);
	
	//!Get the properties of the filter, in key-value form. First vector is for each output.
	void getProperties(FilterPropGroup &propertyList) const;
	
	//!Set the properties for the nth filter. Returns true if prop set OK
	bool setProperty(unsigned int key, 
					 const std::string &value, bool &needUpdate);
	//!Get the human readable error string associated with a particular error code during refresh(...)
	std::string getSpecificErrString(unsigned int code) const;
	
	//!Dump state to output stream, using specified format
	bool writeState(std::ostream &f,unsigned int format, 
					unsigned int depth=0) const;
	//!Read the state of the filter from XML file. If this
	//fails, filter will be in an undefined state.
	bool readState(xmlNodePtr &node, const std::string &packDir);

	//!Get the stream types that will be dropped during ::refresh	
	unsigned int getRefreshBlockMask() const;

	//!Get the stream types that will be generated during ::refresh	
	unsigned int getRefreshEmitMask() const;	

	//!Get the stream types that will be possibly ued during ::refresh	
	unsigned int getRefreshUseMask() const;	
	//!Set internal property value using a selection binding  
	void setPropFromBinding(const SelectionBinding &b) ;

};

#endif

