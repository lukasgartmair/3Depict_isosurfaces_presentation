/*
 *	animator.h - animation classes for 3Depict
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
#ifndef ANIMATOR_H
#define ANIMATOR_H

#include "filter.h"
#include "common/xmlHelper.h"


#include <set>
#include <map>

enum
{
	INTERP_STEP,
	INTERP_LINEAR_FLOAT,
	INTERP_LINEAR_COLOUR,
	INTERP_LIST,
	INTERP_LINEAR_POINT3D,
	INTERP_STEP_POINT3D,
	INTERP_END
};

extern const char *INTERP_NAME[];

class FrameProperties;


class InterpData
{
	public:
		size_t interpMode;
	
		//!Obtain the interpolated data at the specified frame,
		// for the properties given as a parameter.
		// should only be called for frames that lie within the interpolated
		// range
		std::string getInterpolatedData(const std::vector<std::pair<size_t,
 					std::string>  > &keyData,size_t frame) const;

		float interpLinearRamp(size_t startFrame, size_t endFrame, size_t curFrame,
					float a, float b) const;
};

//!Frame-by-frame properties for a specific filter
class FrameProperties
{
	private:
		//ID of the filter that whose properties are to be altered 	
		size_t filterId;
	
		//Property Key for filter
		size_t propertyKey;
		//!First in pair is frame offset, second is property at that frame
		std::vector<std::pair<size_t,std::string> > frameData;

		//!Interpolation information
		InterpData interpData;
	public:
		FrameProperties() {};
		FrameProperties(size_t filterIdVal,size_t propertyKeyVal);
		~FrameProperties();

		//!Get the minimal frame (which is affected)
		size_t getMinFrame() const;
		//!Get tha maximal frame (which is affected)
		size_t getMaxFrame() const;
	
		//!Add a key frame to the dataset
		void addKeyFrame(size_t frame, const FilterProperty &p);
		//!Add a key frame to the dataset
		void addKeyFrame(size_t frame, const std::string &p);

		
		//Set the interpolation mode
		void setInterpMode(size_t mode) ;

		//obtain the interpolation method
		size_t getInterpMode() const { return interpData.interpMode;};

		size_t getFilterId() const { return filterId;}
		size_t getPropertyKey() const { return propertyKey;}
		
		std::string getInterpolatedData(size_t frame) const 
			{ return interpData.getInterpolatedData(frameData,frame);}
	
		//!Dump state to output stream, using specified format
		/* Current supported formats are STATE_FORMAT_XML.
		 * Depth is indentation depth (for pretty-printing) 
		 */ 
		bool writeState(std::ostream &f, unsigned int format,
			       	unsigned int depth=0) const ;


		bool loadState(xmlNodePtr &nodePtr ) ;

		void remapId(size_t newId);
};

//!Animation of filter properties
class PropertyAnimator
{
	private:
		//Vector containing each properties new
		// value/key pairing
		std::vector<FrameProperties> keyFrames;
	public:
		PropertyAnimator();

		PropertyAnimator(const PropertyAnimator &p);

		//!Are the properties self-consistent - returns true if OK
		bool checkSelfConsistent(std::set<size_t> &conflictingFrames) const;
		
		//!Obtain the maximal frame for animation
		size_t getMaxFrame() const;

		//!Get all the properties that intersect or precede 
		// a particular keyframe.
		void getPropertiesAtFrame(size_t keyframe, std::vector<size_t> &propIds,
			std::vector<FrameProperties> &props) const;

		//Obtain the as-animated version of a specific filter for a particular frame.
		// returns empty string if the filter ID/key is not known.
		// Otherwise returns best-effort interpolated data
		std::string getInterpolatedFilterData(size_t id, size_t propKey, size_t frame) const;

		//-- Data modification funcs --
		//!Add a property to the list of available props
		void addProp(const FrameProperties &p) { keyFrames.push_back(p);}
		//!Set a particlar property
		void setProp(size_t id, const FrameProperties &p);
		
		//!Remove frame by its unique ID
		void removeProp(size_t id);

		//!Remove all stored information 
		void clear();

		//!Get the number of properties stored
		size_t getNumProps() const { return keyFrames.size();} 


		//Obtain the frame property by its position
		void getNthKeyFrame(size_t frameNum,FrameProperties &f) const ;

		//Remove this particular keyframe
		void removeNthKeyFrame(size_t frameNum);

		//Remove the specified key frames. Input vector contents will be sorted.
		void removeKeyFrames(std::vector<size_t> &vec);

		//!Dump state to output stream, using specified format
		/* Current supported formats are STATE_FORMAT_XML.
		 * Depth is indentation depth (for pretty-printing) 
		 */ 
		bool writeState(std::ostream &f, unsigned int format,
			       	unsigned int depth=0) const;


		bool loadState(xmlNodePtr &nodePtr);


		//!Obtain the complete listing of IDs used internally
		void getIdList(std::vector<unsigned int> &ids) const;

		//!Force the internal IDs for filters to a new value
		void updateMappings(const std::map<size_t,size_t> &newMap);
};



#endif

