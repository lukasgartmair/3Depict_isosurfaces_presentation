/*
 *	ionClip.h - Clipping of 3D point clouds
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
#ifndef IONCLIP_H
#define IONCLIP_H

#include "../filter.h"
#include "../../common/translation.h"

//!Ion spatial clipping filter
class IonClipFilter :  public Filter
{
	protected:
		//!Number explaining basic primitive type
		/* Possible Modes:
		 * Planar clip (origin + normal)
		 * spherical clip (origin + radius)
		 * Cylindrical clip (origin + axis + length)
		 * Axis aligned box (origin + corner)
		 */
		unsigned int primitiveType;

		//!Whether to reverse the clip. True means that the interior is excluded
		bool invertedClip;
		//!Whether to show the primitive or not
		bool showPrimitive;
		//!Vector paramaters for different primitives
		std::vector<Point3D> vectorParams;
		//!Scalar paramaters for different primitives
		std::vector<float> scalarParams;
		//Lock the primitive axis during for cylinder?
		bool lockAxisMag; 

	public:
		IonClipFilter();
		
		//!Duplicate filter contents, excluding cache.
		Filter *cloneUncached() const;
		
		//!Returns FILTER_TYPE_IONCLIP
		unsigned int getType() const { return FILTER_TYPE_IONCLIP;};
		
		//!Get approx number of bytes for caching output
		size_t numBytesForCache(size_t nObjects) const;

		//!update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
			std::vector<const FilterStreamData *> &getOut, 
			ProgressData &progress);
	
		//!Return human readable name for filter	
		virtual std::string typeString() const { return std::string(TRANS("Clipping"));};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterPropGroup &propertyList) const;

		//!Set the properties for the nth filter. Returns true if prop set OK
		bool setProperty(unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getSpecificErrString(unsigned int code) const;

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
		
		
		//!Set internal property value using a selection binding 
		void setPropFromBinding(const SelectionBinding &b);

#ifdef DEBUG
		bool runUnitTests();
#endif
};

#endif
