/*
 *	spectrumPlot.h - Compute histograms of values for valued 3D point data
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
#ifndef SPECTRUMPLOT_H
#define SPECTRUMPLOT_H
#include "../filter.h"
#include "../../common/translation.h"


//!Spectrum plot filter
class SpectrumPlotFilter : public Filter
{
	private:
		//minimum and maximum plot bounds
		float minPlot,maxPlot;
		//step size to use for histogram binning
		float binWidth;
		//automatically determine plot limits?
		bool autoExtrema;
		//plots should be sown in log mode?
		bool logarithmic;
	
		//perform fitting	
		unsigned int fitMode;
		// when fitting, only show the corrected spectrum, rather than fit itself 
		bool showOnlyCorrected;

		//start/end of mass values to use when fitting mass spectrum
		float massBackStart, massBackEnd;


		//Vector of spectra. Each spectra is comprised of a sorted Y data
		std::vector< std::vector<float > > spectraCache;
		ColourRGBAf rgba;
		unsigned int plotStyle;

		//!Normalisation mode for scaling plot intensity
		unsigned int normaliseMode;

		//!Lower and upper bound for normalisation of spectrum.
		// used to "crop" spectra when searching for normalisation
		std::pair<float,float> normaliseBounds;


		void normalise(std::vector<std::pair<float,float> > &spectrumData) const;

	public:
		SpectrumPlotFilter();
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		//!Returns FILTER_TYPE_SPECTRUMPLOT
		unsigned int getType() const { return FILTER_TYPE_SPECTRUMPLOT;};

		//!Get approx number of bytes for caching output
		size_t numBytesForCache(size_t nObjects) const;

		//!update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
			std::vector<const FilterStreamData *> &getOut, 
			ProgressData &progress);
		
		virtual std::string typeString() const { return std::string(TRANS("Spectrum"));};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterPropGroup &propertyList) const;

		//!Set the properties for the nth filter. Returns true if prop set OK
		bool setProperty(unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getSpecificErrString(unsigned int code) const;

		//!Set the user string.
		void setUserString(const std::string &s);

		//!Dump state to output stream, using specified format
		bool writeState(std::ostream &f,unsigned int format, unsigned int depth=0) const;
		
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
	
		//!Get the stream types that will be dropped during ::refresh	
		unsigned int getRefreshBlockMask() const;

		//!Get the stream types that will be generated during ::refresh	
		unsigned int getRefreshEmitMask() const;	
		
		//!Get the stream types that will be possibly used during ::refresh	
		unsigned int getRefreshUseMask() const;	

		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b)  ;

		//!Does the filer need unranged data to operate?
		bool needsUnrangedData() const;

#ifdef DEBUG
		bool runUnitTests() ;

#endif
};

#endif

