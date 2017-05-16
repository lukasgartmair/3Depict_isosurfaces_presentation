/*
 *	profile.h - Composition/density profiles of 3D point clouds
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
#ifndef COMPPROFILE_H
#define COMPPROFILE_H
#include "../filter.h"
#include "../../common/translation.h"

#include <map>

enum
{
	PROFILE_KEY_BINWIDTH=1,
	PROFILE_KEY_FIXEDBINS,
	PROFILE_KEY_DENSITY_ONLY,
	PROFILE_KEY_NORMAL,
	PROFILE_KEY_MINEVENTS,
	PROFILE_KEY_NUMBINS,
	PROFILE_KEY_ORIGIN,
	PROFILE_KEY_PLOTTYPE,
	PROFILE_KEY_PRIMITIVETYPE,
	PROFILE_KEY_RADIUS,
	PROFILE_KEY_SHOWPRIMITIVE,
	PROFILE_KEY_NORMALISE,
	PROFILE_KEY_COLOUR,
	PROFILE_KEY_ERRMODE,
	PROFILE_KEY_AVGWINSIZE,
	PROFILE_KEY_LOCKAXISMAG
};
//!Filter that does composition or density profiles for various primitives
class ProfileFilter : public Filter
{
	private:

		//!Number explaining basic primitive type
		/* Possible Modes:
		 * Cylindrical (origin + axis + length)
		 */
		unsigned int primitiveType;
		//!Whether to show the primitive or not
		bool showPrimitive;
		//Lock the primitive axis during for cylinder?
		bool lockAxisMag; 
		//!Vector parameters for different primitives
		std::vector<Point3D> vectorParams;
		//!Scalar parameters for different primitives
		std::vector<float> scalarParams;

		//! Does the user explicitly want a density plot? 
		bool wantDensity;
		//!Frequency or percentile mode (0 - frequency; 1-normalised (ion freq))
		bool normalise;
		//!Use fixed bins?
		bool fixedBins;
		
		//!number of bins (if using fixed bins)
		unsigned int nBins;
		//!Width of each bin (if using fixed width)
		float binWidth;

		//!Number of events required for an entry to be logged in a normalised
		// histogram
		unsigned int minEvents;
		
		//Plotting stuff
		//--
		//colour of plot
		ColourRGBAf rgba;
		//Mode for plotting (eg lines, steps)
		unsigned int plotStyle;
	
		PLOT_ERROR errMode;

		//!Do we have a range file above us in our filter tree? This is set by ::initFilter
		bool haveRangeParent;
		//--
		
		//!internal function for binning an ion dependant upon range data
		static void binIon(unsigned int targetBin, const RangeStreamData* rng, const std::map<unsigned int,unsigned int> &ionIDMapping,
			std::vector<std::vector<size_t> > &frequencyTable, float massToCharge);

		static unsigned int getPrimitiveId(const std::string &s);;

		//obtain the size of each bin, and number of bins required for profile
		unsigned int getBinData(unsigned int &numBins, float &binLength) const;

		//Obtain the X coordinate of a given bin's centre, given the bin value
		float getBinPosition(unsigned int nBin) const;

	public:
		ProfileFilter();
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		//!Returns FILTER_TYPE_PROFILE
		unsigned int getType() const { return FILTER_TYPE_PROFILE;};

		//!Get approx number of bytes for caching output
		size_t numBytesForCache(size_t nObjects) const;
		

		//!Initialise filter, check for upstream range
		virtual void initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut);
		//!update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
						std::vector<const FilterStreamData *> &getOut, 
						ProgressData &progress);
		
		virtual std::string typeString() const { return std::string(TRANS("Comp. Prof."));};

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
		
		//!Get the stream types that may be utilised in computation during ::refresh
		unsigned int getRefreshUseMask() const;	
	
		//!Set internal property value using a selection binding  
		void setPropFromBinding(const SelectionBinding &b) ;

		void setUserString(const std::string &s); 

#ifdef DEBUG
		bool runUnitTests() ;
#endif
};

#endif
