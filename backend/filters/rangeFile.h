/*
 *	rangeFile.h - bins ions into different value ranges given an input range file
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
#ifndef RANGEFILE_H
#define RANGEFILE_H

#include "../filter.h"
#include "../../common/translation.h"

enum
{
	RANGE_KEY_RANGE_ACTIVE=1,
	RANGE_KEY_DROP_UNRANGED,
	RANGE_KEY_RANGE_FILENAME,
	RANGE_KEY_ENABLE_LEGEND,
	RANGE_KEY_ENABLE_ALL_IONS, //Limited to ~100K ions
	RANGE_KEY_ENABLE_ALL_RANGES=100000,
};

//!Range file filter
class RangeFileFilter : public Filter
{
	private:
		std::string rngName;
		//!Vector of chars stating if user has enabled a particular range or not
		std::vector<char> enabledRanges;
		//!Vector of chars stating if user has enabled a particular Ion or not.
		std::vector<char> enabledIons;

		//!Whether to drop unranged ions in our output
		bool dropUnranged;
		
		//!Assumed file format when loading.
		unsigned int assumedFileFormat;

		void guessFormat(const std::string &s);

		//!range file object 
		RangeFile rng;

		//Show a legend of enabled ions
		bool showLegend;

		//create a legend drawable. Note that the pointer 
		// will not be deleted by this function 
		DrawStreamData *createLegend() const;
	public:

		//!Set the format to assume when loading file
		void setFormat(unsigned int format);
	
		std::vector<char> getEnabledRanges() const {return enabledRanges;};
		void setEnabledRanges(const std::vector<char> &i) {enabledRanges = i;};
		
		std::vector<char> getEnabledIons() const {return enabledIons;};


		RangeFileFilter(); 
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		void setRangeFilename(std::string filename){rngName=filename;};

		//!Returns -1, as range file cache size is dependant upon input.
		virtual size_t numBytesForCache(size_t nObjects) const;
		//!Returns FILTER_TYPE_RANGEFILE
		unsigned int getType() const { return FILTER_TYPE_RANGEFILE;};
		
		//!Propagates a range stream data through the filter init stage. Blocks any other range stream datas
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					ProgressData &progress);
		//!Force a re-read of the rangefile, returning false on failure, true on success
		bool updateRng();
		
		const RangeFile &getRange() const { return rng;};
		//!Set the internal data using the specified range object
		void setRangeData(const RangeFile &newRange);

		virtual std::string typeString() const { return std::string(TRANS("Ranging"));};

		//Types that will be dropped during ::refresh
		unsigned int getRefreshBlockMask() const;
		
		//Types that are emitted by filer during ::refrash
		unsigned int getRefreshEmitMask() const;

		//Types that are possibly used by filer during ::refrash
		unsigned int getRefreshUseMask() const;

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterPropGroup &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int key, 
				const std::string &value, bool &needUpdate);
		
		//!Set a region update
		virtual void setPropFromRegion(unsigned int method, unsigned int regionID, float newPos);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getSpecificErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ostream &f,unsigned int format,
						unsigned int depth=0) const;
		
		//!Modified version of writeState for packaging. By default simply calls writeState.
		//value overrides override the values returned by getStateOverrides. In order.	
		virtual bool writePackageState(std::ostream &f, unsigned int format,
				const std::vector<std::string> &valueOverrides,unsigned int depth=0) const;
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		
		//!filter has state overrides	
		virtual void getStateOverrides(std::vector<std::string> &overrides) const; 
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b)  ;

		bool getDropUnranged() const { return dropUnranged; }
#ifdef DEBUG
		bool runUnitTests();
#endif
};

#endif
