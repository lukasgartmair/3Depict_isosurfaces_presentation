/*
 *	spatialAnalysis.h - Perform various data analysis on 3D point clouds
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
#ifndef SPATIALANALYSIS_H
#define SPATIALANALYSIS_H
#include "../filter.h"
#include "../../common/translation.h"

//!Spatial analysis filter
class SpatialAnalysisFilter : public Filter
{
	private:
		//!Colour to use for output plots
		ColourRGBAf rgba;

		//!Which algorithm to use
		unsigned int algorithm;

		//!Stopping criterion
		unsigned int stopMode;

		//!NN stopping criterion (max)
		unsigned int nnMax;

		//!Distance maximum
		float distMax;

		//!Do we have range data to use (is nonzero)
		bool haveRangeParent;
		//!The names of the incoming ions
		std::vector<std::string > ionNames;
		//!Are the sources/targets enabled for a  particular incoming range?
		std::vector<bool> ionSourceEnabled,ionTargetEnabled;

		//!work out which ions to count in the numerator vs denominator
		std::vector<bool> ionNumeratorEnabled,ionDenominatorEnabled;

		//RDF specific params
		//--------
		//RDF bin count
		unsigned int numBins;

		//!Optional convex hull reduction
		bool excludeSurface;

		//!Surface reduction distance (convex hull)
		float reductionDistance;

		//!Change the NN histograms from counts to counts/nm
		// - this allows comparing different binwidth histograms
		bool normaliseNNHist;
		
		//!Do we want to display theoretical random NN distances on top?
		bool wantRandomNNHist;
		//--------
		
		//Density filtering specific params
		//-------
		//Do we keep points with density >= cutoff (true), or
		// points with density < cutoff (false)
		bool keepDensityUpper;

		//Cutoff value when performing density filtering
		float densityCutoff; 

		//!Vector paramaters for different primitives
		std::vector<Point3D> vectorParams;
		//!Scalar paramaters for different primitives
		std::vector<float> scalarParams;
	
		//Reset the scalar and vector parameters
		// to their defaults, if the required parameters
		// are not available
		void resetParamsAsNeeded();
		//-------

		//Binomial specific algorithms
		//--------
		//Number of ions to target when segmenting
		unsigned int numIonsSegment;

		//Maximum aspect ratio permissible for grid entry
		float maxBlockAspect;

		//The step size in the composition space
		float binWidth;

		//Direction in which to perform grid extrusion
		size_t extrusionDirection;

		//Do we show the frequency plot?
		bool showBinomialFrequencies;

		bool showNormalisedBinomialFrequencies;

		//Do we show the theoretical frequency distributions?
		bool showTheoreticFrequencies;

		//Do we show the overlaid extruded grid?
		bool showGridOverlay;

		//--------

		//Replace specific code
		//---------
		//file to use as other data source
		std::string replaceFile;

		//replacement operator mode
		unsigned int replaceMode;

		//distance up to which to allow replacement
		float replaceTolerance;
		
		//should we replace the current mass by the other file's ?
		bool replaceMass;
		//---------
	
		//Radial distribution function - creates a 1D histogram of spherical atom counts, centered around each atom
		size_t algorithmRDF(ProgressData &progress, size_t totalDataSize, 
			const std::vector<const FilterStreamData *>  &dataIn, 
			std::vector<const FilterStreamData * > &getOut,const RangeFile *rngF);


		//Local density function - places a sphere around each point to compute per-point density
		size_t algorithmDensity(ProgressData &progress, size_t totalDataSize, 
			const std::vector<const FilterStreamData *>  &dataIn, 
			std::vector<const FilterStreamData * > &getOut);
		
		//Density filter function - same as density function, but then drops points from output
		// based upon their local density and some density cutoff data
		size_t algorithmDensityFilter(ProgressData &progress, size_t totalDataSize, 
			const std::vector<const FilterStreamData *>  &dataIn, 
			std::vector<const FilterStreamData * > &getOut);

		size_t algorithmAxialDf(ProgressData &progress, size_t totalDataSize, 
			const std::vector<const FilterStreamData *>  &dataIn, 
			std::vector<const FilterStreamData * > &getOut,const RangeFile *rngF);
		
		size_t algorithmBinomial(ProgressData &progress, size_t totalDataSize, 
			const std::vector<const FilterStreamData *>  &dataIn, 
			std::vector<const FilterStreamData * > &getOut,const RangeFile *rngF);

		size_t algorithmReplace(ProgressData &progress, size_t totalDataSize, 
			const std::vector<const FilterStreamData *>  &dataIn, 
			std::vector<const FilterStreamData * > &getOut);

		//Local concentration algorithm, as described by 10.1016/j.jnucmat.2014.03.034
		// TODO: Better reference?
		// - I think implementations pre-date this paper, as I know of this algorithm from much earlier than 2014
		size_t algorithmLocalConcentration(ProgressData &progress, size_t totalDataSize, 
			const std::vector<const FilterStreamData *>  &dataIn, 
			std::vector<const FilterStreamData * > &getOut, const RangeFile *rngF);
		//Create a 3D manipulable cylinder as an output drawable
		// using the parameters stored inside the vector/scalar params
		// both parameters are outputs from this function
		void createCylinder(DrawStreamData* &d, SelectionDevice * &s) const;


		//Wrapper routeine to create the appropriate selection
		// device for whatever algorithm is in use; device list will be appended to
		// and if needed, output object will be generated 
		void createDevice(std::vector<const FilterStreamData *> &getOut);


		//From the given input ions, filter them down using the user
		// selection for ranges. If sourceFilter is true, filter by user
		// source selection, otherwise by user target selection
		void filterSelectedRanges(const std::vector<IonHit> &ions, bool sourceFilter, const RangeFile *rngF, std::vector<IonHit> &output) const;
	public:
		SpatialAnalysisFilter(); 
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;

		//!Initialise filter prior to tree propagation
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);

		//!Returns -1, as range file cache size is dependant upon input.
		virtual size_t numBytesForCache(size_t nObjects) const;
		//!Returns FILTER_TYPE_SPATIAL_ANALYSIS
		unsigned int getType() const { return FILTER_TYPE_SPATIAL_ANALYSIS;};
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					ProgressData &progress);
		//!Get the type string  for this fitler
		virtual std::string typeString() const { return std::string(TRANS("Spat. Analysis"));};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterPropGroup &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getSpecificErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ostream &f,unsigned int format,
						unsigned int depth=0) const;
		
		//!write an overridden filename version of the state
		virtual bool writePackageState(std::ostream &f, unsigned int format,
				const std::vector<std::string> &valueOverrides,unsigned int depth=0) const;
		//Obtain the state file override 
		void getStateOverrides(std::vector<string> &externalAttribs) const; 

		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		
		//!Get the stream types that will be dropped during ::refresh	
		unsigned int getRefreshBlockMask() const;

		//!Get the stream types that will be generated during ::refresh	
		unsigned int getRefreshEmitMask() const;	
		
		//!Get the stream types that will be possibly used during ::refresh	
		unsigned int getRefreshUseMask() const;	
		
		//!Set internal property value using a selection binding  
		void setPropFromBinding(const SelectionBinding &b)  ;
		
		//Set the filter's Title "user string"
		void setUserString(const std::string &s); 
#ifdef DEBUG
		bool runUnitTests();
#endif
};

#endif
