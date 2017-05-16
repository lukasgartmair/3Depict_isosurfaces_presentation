/*
 *	boundingBox.h - Bounding boxes for 3D point data
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
#ifndef BOUNDINGBOX_H
#define BOUNDINGBOX_H

#include "../filter.h"
#include "../../common/translation.h"

//!Bounding box filter
class BoundingBoxFilter : public Filter
{
	private:
		//!visibility
		bool isVisible;

		//!visual representation mode
		unsigned int boundStyle;
		//!Should tick positions be computed using fixed tick counts or spacing?
		bool fixedNumTicks;
		//!Number of ticks (XYZ) if using fixed num ticks
		unsigned int numTicks[3];
		//!Spacing of ticks (XYZ) if using fixed spacing ticks
		float tickSpacing[3];

		//!Enable/disable ticks on a given axis
		bool enableTicks[3];
		//!Font size
		unsigned int fontSize;
		//!Should we use absolute coordinate values in box labels?
		bool absoluteCoords;
		//!Line colour
		ColourRGBAf lineColour;
		//!Line width 
		float lineWidth;
		//!Use 3D text?
		bool threeDText;


		//!Draw tick-style bounding box and associated annotations
		void drawTicks(const BoundCube &bTotal,DrawStreamData *d) const;


		//!Draw  "dimension" style bounding box and associated annotation
		void drawDimension(const BoundCube &bTotal, DrawStreamData *d) const;

	public:
		BoundingBoxFilter(); 
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;

		//!Returns -1, as range file cache size is dependant upon input.
		virtual size_t numBytesForCache(size_t nObjects) const;
		//!Returns FILTER_TYPE_RANGEFILE
		unsigned int getType() const { return FILTER_TYPE_BOUNDBOX;};
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					ProgressData &progress);
		//!Force a re-read of the rangefile Return value is range file reading error code
		unsigned int updateRng();
		virtual std::string typeString() const { return std::string(TRANS("Bound box"));};

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
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		
		//!Get the stream types that will be dropped during ::refresh	
		unsigned int getRefreshBlockMask() const;

		//!Get the stream types that will be generated during ::refresh	
		unsigned int getRefreshEmitMask() const;	

		//!Refresh ignore mask, for filter streams that will not be utilised in the computation (except for pass-through)
		unsigned int getRefreshUseMask() const;

		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b)  ;

#ifdef DEBUG
		bool runUnitTests() ;
#endif
};

#endif
