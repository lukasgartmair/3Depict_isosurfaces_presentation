/*
 *	ionDownsample.h - Filter to perform sampling without replacement on input ion data
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
#ifndef IONDOWNSAMPLE_H
#define IONDOWNSAMPLE_H

#include "../filter.h"

enum
{
	KEY_IONDOWNSAMPLE_FRACTION=1,
	KEY_IONDOWNSAMPLE_FIXEDOUT,
	KEY_IONDOWNSAMPLE_COUNT,
	KEY_IONDOWNSAMPLE_PERSPECIES,
	KEY_IONDOWNSAMPLE_ENABLE,
	//Dynamic area for this filter class. May validly use any index after this value
	KEY_IONDOWNSAMPLE_DYNAMIC, 
};

//!Random picker filter
class IonDownsampleFilter : public Filter
{
	private:
		RandNumGen rng;
		//When using fixed number output, maximum to allow out.
		size_t maxAfterFilter;
		//!Allow only a fixed number at output, alternate is random fraction (binomial dist).
		bool fixedNumOut;
		//Fraction to output
		float fraction;
	

		//!Should we use a per-species split or not?
		bool perSpecies;	
		//This is filter's enabled ranges. 0 if we don't have a range
		RangeStreamData *rsdIncoming;

		//!Fractions to output for species specific
		std::vector<float> ionFractions;
		std::vector<size_t> ionLimits;
	public:
		IonDownsampleFilter();
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		//!Returns FILTER_TYPE_IONDOWNSAMPLE
		unsigned int getType() const { return FILTER_TYPE_IONDOWNSAMPLE;};
		//!Initialise filter prior to tree propagation
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);
		
		//!Set mode, fixed num out/approximate out (fraction)
		void setControlledOut(bool controlled) {fixedNumOut=controlled;};

		//!Set the number of ions to generate after the filtering (when using count based fitlering).
		void setFilterCount(size_t nMax) { maxAfterFilter=nMax;};

		//!Get (approx) number of bytes required for cache
		virtual size_t numBytesForCache(size_t nObjects) const;
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &getOut, 
				 ProgressData &progress);

		//!return string naming the human readable type of this class
		virtual std::string typeString() const { return std::string(TRANS("Ion Sampler"));}

		
		
		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterPropGroup &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty( unsigned int key, const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getSpecificErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ostream &f,unsigned int format, 
						unsigned int depth=0) const;
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b)  ;
	
		//!Get the stream types that will be dropped during ::refresh	
		unsigned int getRefreshBlockMask() const;

		//!Get the stream types that will be generated during ::refresh	
		unsigned int getRefreshEmitMask() const;	
		
		//!Get the stream types that will be possibly used during ::refresh	
		unsigned int getRefreshUseMask() const;	
		
#ifdef DEBUG
		//Fire off the unit tests for this class. returns false if *any* test fails
		bool runUnitTests();
#endif
};

#endif
