/*
 *	ionInfo.h -Filter to compute various properties of valued point cloud
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
#ifndef IONINFO_H
#define IONINFO_H

#include "../filter.h"
#include "../../common/translation.h"

enum
{
	IONINFO_KEY_TOTALS=1,
	IONINFO_KEY_NORMALISE,
	IONINFO_KEY_VOLUME,
	IONINFO_KEY_VOLUME_ALGORITHM,
	IONINFO_KEY_BACKMODE,
	IONINFO_KEY_BACK_MASSSTART,
	IONINFO_KEY_BACK_MASSEND,
	IONINFO_KEY_BACK_BINSIZE
};

//!Ion derived information filter, things like volume, composition, etc.
class IonInfoFilter : public Filter
{
	private:
		//!Do we want to know information about the number of ions/composition
		bool wantIonCounts;

		//!Do we want to normalise the ion count data?
		bool wantNormalise;


		//!Parent rangefile in tree
		RangeStreamData *range;

		//!Do we want to know about the volume
		bool wantVolume;

		//!Method for volume computation
		unsigned int volumeAlgorithm;

		//Side length for filled cube volume estimation
		float cubeSideLen;

		//mode for performing background correction
		unsigned int fitMode;

		//start/end mass for background correction
		float massBackStart, massBackEnd;

		//binwidth to use when performing background correction
		float binWidth;
#ifdef DEBUG
		float lastVolume;
#endif

		//!String for 
		size_t volumeEstimationStringFromID(const char *str) const;

		//Convex hull volume estimation routine.
		//returns 0 on success. global "qh " "object"  will contain
		//the hull. Volume is computed.
		static unsigned int convexHullEstimateVol(const std::vector<const FilterStreamData*> &data, 
							float &vol);
	public:
		//!Constructor
		IonInfoFilter();

		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;

		//Perform filter intialisation, for pre-detection of range data
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);
		
		//!Apply filter to new data, updating cache as needed. Vector
		// of returned pointers must be deleted manually, first checking
		// ->cached.
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
							std::vector<const FilterStreamData *> &dataOut,
							ProgressData &progress);
		//!Get (approx) number of bytes required for cache
		size_t numBytesForCache(size_t nObjects) const;

		//!return type ID
		unsigned int getType() const { return FILTER_TYPE_IONINFO;}

		//!Return filter type as std::string
		std::string typeString() const { return std::string(TRANS("Ion info"));};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterPropGroup &propertyList) const;

		//!Set the properties for the nth filter,
		//!needUpdate tells us if filter output changes due to property set
		bool setProperty( unsigned int key,
					const std::string &value, bool &needUpdate);


		void setPropFromBinding( const SelectionBinding &b) ;

		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getSpecificErrString(unsigned int code) const;

		//!Dump state to output stream, using specified format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		bool writeState(std::ostream &f, unsigned int format,
							unsigned int depth) const;

		//!Read state from XML  stream, using xml format
		/* Current supported formats are STATE_FORMAT_XML
		 */
		bool readState(xmlNodePtr& n, const std::string &packDir="");

		//!Get the bitmask encoded list of filterStreams that this filter blocks from propagation.
		// i.e. if this filterstream is passed to refresh, it is not emitted.
		// This MUST always be consistent with ::refresh for filters current state.
		unsigned int getRefreshBlockMask() const;

		//!Get the bitmask encoded list of filterstreams that this filter emits from ::refresh.
		// This MUST always be consistent with ::refresh for filters current state.
		unsigned int getRefreshEmitMask() const;
		
		//!Get the bitmask encoded list of filterstreams that this filter may use during ::refresh.
		unsigned int getRefreshUseMask() const;


		//!Does the filter need unranged input?
		bool needsUnrangedData() const; 
#ifdef DEBUG
		bool runUnitTests();

		//Debugging function only; must be called after refresh. 
		//Returns the last estimation for volume.
		float getLastVolume() { float tmp=lastVolume; lastVolume=0;return tmp; } 
#endif
};


#endif
