/*
 *	voxelise.h - Compute 3D binning (voxelisation) of point clouds
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
#ifndef VOXELISE_H
#define VOXELISE_H
#include "../filter.h"

#include "common/voxels.h"

#include "../../common/translation.h"

#include "openvdb_includes.h"
#include "contribution_transfer_function_TestSuite/CTF_functions.h"

//!Filter that does voxelisation for various primitives (copied from CompositionFilter)
class VoxeliseFilter : public Filter
{
private:
	const static size_t INDEX_LENGTH = 3;

	//Enabled ions for numerator/denom
	std::vector<char> enabledIons[2];

	//!Stepping mode - fixed width or fixed number of bins
	bool fixedWidth;

	float voxelsize; // declaration here, definition in the source file

	//Cache to use for voxel info
	Voxels<float> voxelCache;

	//!number of bins (if using fixed bins)
	unsigned long long nBins[INDEX_LENGTH];
	//!Width of each bin (if using fixed wdith)
	Point3D binWidth;
	//!boundcube for the input data points
	BoundCube bc;
	
	//!density-based or count-based	
	unsigned int normaliseType;
	bool numeratorAll, denominatorAll;
	//This is filter's enabled ranges
	RangeStreamData *rsdIncoming;
	
	ColourRGBAf rgba;

	//!Filter mode to apply to data before output
	unsigned int filterMode;

	//!How do we treat boundaries when applying filters
	unsigned int filterBoundaryMode;

	//!Filter size, in units of gaussDevs 
	float filterRatio;

	//!Gaussian filter standard deviation
	float gaussDev;

	//!3D Point Representation size
	float splatSize;

	//!Isosurface level
	float isoLevel;
	//!Default output representation mode
	unsigned int representation;

	//!Colour map to use when using axial slices
	unsigned int colourMap;

	//Number of colour levels for colour map
	size_t nColours;
	//Whether to show the colour map bar or not
	bool showColourBar;
	//Whether to use an automatic colour bound, or to use user spec
	bool autoColourMap;
	//Colour map start/end
	float colourMapBounds[2];

	//Interpolation mode to use when slicing	
	size_t sliceInterpolate;
	//Axis that is normal to the slice 0,1,2 => x,y,z
	size_t sliceAxis;
	//Fractional offset from lower bound of data cube [0,1]
	float sliceOffset;

	//Obtain a textured slice from the given voxel set
	void getTexturedSlice(const Voxels<float> &f,
		size_t axis,float offset, size_t interpolateMode,
		float &minV, float &maxV, DrawTexturedQuad &texQ) const;

	BoundCube lastBounds;

	//Cache to use for vdbgrid info
	// console warning: non-static data member initializers only available with -std=c++11 or -std=gnu++11
	openvdb::FloatGrid::Ptr vdbCache;

public:
	VoxeliseFilter();
	~VoxeliseFilter() { if(rsdIncoming) delete rsdIncoming;}
	//!Duplicate filter contents, excluding cache.
	Filter *cloneUncached() const;

	virtual void clearCache();
	
	//!Get approx number of bytes for caching output
	size_t numBytesForCache(size_t nObjects) const;

	unsigned int getType() const { return FILTER_TYPE_VOXELS;};
	
	virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &dataOut);
	//!update filter
	unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
						 std::vector<const FilterStreamData *> &getOut, 
						 ProgressData &progress);
	
	virtual std::string typeString() const { return std::string(TRANS("Voxelisation"));};

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
	
	
	//!calculate the widths of the bins in 3D
	void calculateWidthsFromNumBins(Point3D &widths, unsigned long long *nb) const{
		Point3D low, high;
		bc.getBounds(low, high);
		for (unsigned int i = 0; i < 3; i++) {
			widths[i] = (high[i] - low[i])/(float)nb[i];
		}
	}
	//!set the number of the bins in 3D
	void calculateNumBinsFromWidths(Point3D &widths, unsigned long long *nb) const{
		Point3D low, high;
		bc.getBounds(low, high);
		for (unsigned int i = 0; i < 3; i++) {
			if (low[i] == high[i]) nb[i] = 1;
			else nb[i] = (unsigned long long)((high[i] - low[i])/(float)widths[i]) + 1;
		}
	}

#ifdef DEBUG
	bool runUnitTests();
#endif

};

#endif

