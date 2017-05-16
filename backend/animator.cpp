/*
 *	animatior.cpp - animation interopolator implementation
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

#include "animator.h"
#include "common/stringFuncs.h"
#include "common/basics.h"

#include <map>
#include <set>

using std::string;
using std::map;
using std::vector;
using std::pair;
using std::make_pair;
using std::endl;
using std::set;

const char *INTERP_NAME[] ={ "Step",
	"Linear",
	"RGB Linear",
	"List",
	"3D Linear",
	"3D Step"
};

PropertyAnimator::PropertyAnimator()
{
}

PropertyAnimator::PropertyAnimator(const PropertyAnimator &p) : keyFrames(p.keyFrames)
{

}

void PropertyAnimator::getNthKeyFrame(size_t frameNum,FrameProperties &f) const 
{
	ASSERT(frameNum < keyFrames.size());
	f=keyFrames[frameNum];
}

size_t PropertyAnimator::getMaxFrame() const
{
	size_t maxFrame=0;
	
	for(size_t ui=0; ui<keyFrames.size();ui++)
		maxFrame=std::max(maxFrame,keyFrames[ui].getMaxFrame());
	return maxFrame;
}

void PropertyAnimator::removeNthKeyFrame(size_t frameNum)
{
	//FIXME: There is totally an STL algorithm for this
	// but I have no internets, and can't remember (so bad!). 
	for(size_t ui=frameNum; ui<keyFrames.size()-1;ui++)
		keyFrames[ui]=keyFrames[ui+1];
	keyFrames.pop_back();
}

void PropertyAnimator::removeKeyFrames(vector<size_t> &vec)
{
	std::sort(vec.begin(),vec.end());

	//FIXME: LAME!! Very inefficient
	for(size_t ui=vec.size();ui;)
	{
		ui--;
		removeNthKeyFrame(vec[ui]);
	}

}

void PropertyAnimator::getPropertiesAtFrame(size_t keyframe, 
			vector<size_t> &propIds,
			vector<FrameProperties> &props) const
{

#ifdef DEBUG
	std::set<size_t> conflicts;
	ASSERT(checkSelfConsistent(conflicts));
#endif

	std::set<size_t> seenIds;

	//Find the frames that are active
	for(size_t ui=0;ui<keyFrames.size();ui++)
	{
		if(keyFrames[ui].getMaxFrame() >= keyframe
			&& keyFrames[ui].getMinFrame() <=keyframe)
		{
			propIds.push_back(ui);
			props.push_back(keyFrames[ui]);
			seenIds.insert(keyFrames[ui].getFilterId());
		}
	}

	//Map the filter Id's to the most recently active keyframe
	map<size_t,size_t> bestFrames;

	//Find the frames that have been active, and not overridden by a newer frame
	// and are thus still in effect
	for(size_t ui=0;ui<keyFrames.size();ui++)
	{
		size_t filterId,maxFrame;
		filterId=keyFrames[ui].getFilterId();

		//This filterId has already been seen, and has a filter
		// already active. If so: Move along.
		if(seenIds.find(filterId)!=seenIds.end())
			continue;

		//See if keyframe can't have been in effect, i.e. if it 
		// occurs after the current frame
		maxFrame=keyFrames[ui].getMaxFrame();
		if(maxFrame >keyframe)
			continue;

		//OK, so we could be active, or there could be a newer
		//frame in effect we haven't seen
		if(bestFrames.find(filterId)==bestFrames.end())
		{
			bestFrames[filterId] = ui;
			continue;
		}
		
		if(maxFrame >=keyFrames[bestFrames[filterId]].getMaxFrame())
		{
			bestFrames[filterId]=ui;
			continue;
		}
	}

	//Now sweep up all the best frames
	for(map<size_t,size_t>::iterator it=bestFrames.begin(); it!=bestFrames.end(); ++it)
	{
		propIds.push_back(it->second);
		props.push_back(keyFrames[it->second]);
	}

}

bool PropertyAnimator::checkSelfConsistent(std::set<size_t> &conflictFrames) const
{
	for(size_t ui=0;ui<keyFrames.size();ui++)
	{
		size_t matchKey,matchFilter;
		matchKey=keyFrames[ui].getPropertyKey();
		matchFilter=keyFrames[ui].getFilterId();
		for(size_t uj=ui+1;uj<keyFrames.size();uj++)
		{
			size_t curKey,curFilter;
			curKey=keyFrames[uj].getPropertyKey();
			curFilter=keyFrames[uj].getFilterId();
			//If we are trying to control the same filter,
			// we need to check to see if the time overlaps
			if(matchKey == curKey && matchFilter == curFilter)
			{
				size_t minA,maxA;
				size_t minB,maxB;
				
				minA=keyFrames[ui].getMinFrame();
				minB=keyFrames[uj].getMinFrame();
				maxA=keyFrames[ui].getMaxFrame();
				maxB=keyFrames[uj].getMaxFrame();

				if(rangesOverlap(minA,maxA,minB,maxB))				
				{
					conflictFrames.insert(ui);
					conflictFrames.insert(uj);
				}

			}
		}
	}

	return (conflictFrames.empty());
}

std::string PropertyAnimator::getInterpolatedFilterData(size_t filterId, 
					size_t propertyKey,size_t frame) const
{
	//NOTE: there seems to be a lot of needless linear sweeping
	// might be a performance increase to restructure the class to
	// use presorted keyframe vectors? Currently, performance-wise
	// we assume very few entries in the keyframe vector
	size_t modifyingId=(size_t)-1;

#ifdef DEBUG
	std::set<size_t> conflicts;
	ASSERT(checkSelfConsistent(conflicts));
#endif
	//Search for the unique keyframe that alters our target property
	for(size_t ui=0;ui<keyFrames.size();ui++)
	{
		if(filterId  == keyFrames[ui].getFilterId()
			&& propertyKey == keyFrames[ui].getPropertyKey())
		{
			modifyingId=ui;
			break;
		}
	}

	if(modifyingId == (size_t)-1)
		return std::string("");

	//Scan to see which keyframe has our time in mind
	size_t keyFrameId=(size_t)-1;
	for(size_t ui=0;ui<keyFrames.size();ui++)
	{
		if(filterId == keyFrames[ui].getFilterId() && 
			propertyKey == keyFrames[ui].getPropertyKey() &&
			frame >=keyFrames[ui].getMinFrame()   &&
			frame <=keyFrames[ui].getMaxFrame() )
		{
			keyFrameId=ui;
			break;
		}
	}

	if(keyFrameId==(size_t)-1)
	{
		//So there is no interpolated data within this run
		// check again for a "latest" modified version
		// and "hold" that value to generate our interpolated result
		
		//First in pair is frame ID, second is max frame that this occurred at
		std::vector<pair<size_t,size_t> > seenFrames;
		for(size_t ui=0;ui<keyFrames.size();ui++)
		{
			if(filterId==keyFrames[ui].getFilterId()
					&& propertyKey == keyFrames[ui].getPropertyKey())
			{
				if(keyFrames[ui].getMaxFrame() <= frame)
					seenFrames.push_back(make_pair(ui,keyFrames[ui].getMaxFrame()));
			}
		}

		//No frame yet? Return empty string
		if(!seenFrames.size())
			return std::string("");

		ComparePairSecond cmpSec;
		std::sort(seenFrames.begin(),seenFrames.end(),cmpSec);

		keyFrameId=seenFrames.back().first;
		frame=seenFrames.back().second;

	}
	//Now we know we have an active modifier.
	// Run the interpolator such that we obtain the
	// desired value of the property
	return keyFrames[keyFrameId].getInterpolatedData(frame);

}

bool PropertyAnimator::writeState(std::ostream &f, unsigned int format, unsigned int depth) const
{
	
	f << tabs(depth) << "<propertyanimator>" << endl;
	
	for(size_t ui=0;ui<keyFrames.size();ui++)
	{
		keyFrames[ui].writeState(f,format,depth+1);
	}

	f << tabs(depth) << "</propertyanimator>" << endl;

	return true;
}


//Should be pointing to a propertyanimator node
bool PropertyAnimator::loadState(xmlNodePtr &nodePtr)
{
	keyFrames.clear();	
	
	
	while(!XMLHelpFwdToElem(nodePtr, "frame"))
	{

		xmlNodePtr childPtr;
		size_t filterId, propKey;
	
		//obtain the frame content XML pointer
		childPtr=nodePtr->xmlChildrenNode;
		if(!childPtr)
			return false;

		//Read the contents of this frame
		if(XMLHelpFwdToElem(childPtr,"filterid"))
			return false;
	
		if(XMLHelpGetProp(filterId,childPtr,"val"))
			return false;


		if(XMLHelpFwdToElem(childPtr,"propertykey"))
			return false;
	
		if(XMLHelpGetProp(propKey,childPtr,"val"))
			return false;

		//Read the <framedata> tag and its children
		{
		FrameProperties fp(filterId,propKey);
		
		if(XMLHelpFwdToElem(childPtr,"framedata"))
			return false;


		xmlNodePtr framePtr;
		framePtr=childPtr->xmlChildrenNode;
		
		if(!framePtr)
			return false;

		do
		{
			size_t offsetVal;
			string data;

			if(XMLHelpFwdToElem(framePtr,"frame"))
				return false;

			if(XMLHelpGetProp(offsetVal,framePtr,"offset"))
				return false;
			
			if(XMLHelpGetProp(data,framePtr,"data"))
				return false;
			
			fp.addKeyFrame(offsetVal,data);

		} while(!XMLHelpFwdToElem(childPtr, "frame")) ;


		if(XMLHelpFwdToElem(framePtr,"interpdata"))
			return false;

		size_t mode;
		if(XMLHelpGetProp(mode,framePtr,"mode"))
			return false;
		
		fp.setInterpMode(mode);


		//save the keyframe
		keyFrames.push_back(fp);
		}


		
	} 

	return true;
}

void PropertyAnimator::getIdList(vector<unsigned int> &ids) const
{
	set<unsigned int> s;
	for(size_t ui=0;ui<keyFrames.size();ui++)
		s.insert(keyFrames[ui].getFilterId());

	ids.resize(s.size());
	std::copy(s.begin(),s.end(),ids.begin());
}

void PropertyAnimator::updateMappings(const map<size_t,size_t> &newIdMap)
{
	vector<bool> killItems;
	killItems.resize(keyFrames.size(),false);

	//Remap the keyframes that we can map
	for(size_t ui=0;ui<keyFrames.size();ui++)
	{
		size_t oldId;
		oldId=keyFrames[ui].getFilterId();

		map<size_t,size_t>::const_iterator it;
		it=newIdMap.find(oldId);

		//Update the mappings we can,
		// Delete keyFrames we cannot remap
		if(it == newIdMap.end()) 
			killItems[ui]=true;
		else
			keyFrames[ui].remapId(it->second);

	}

	//Perform erase
	vectorMultiErase(keyFrames,killItems);
}



FrameProperties::FrameProperties(size_t idFilt,size_t idKey)
{
	filterId=idFilt;
	propertyKey=idKey;
}

FrameProperties::~FrameProperties()
{
}

void FrameProperties::setInterpMode(size_t mode)
{
	ASSERT(mode<INTERP_END); 
	interpData.interpMode=mode;
}

void FrameProperties::remapId(size_t newId)
{
	filterId=newId;
}

size_t FrameProperties::getMinFrame() const
{
	size_t minFrame=std::numeric_limits<size_t>::max();
	
	for(size_t ui=0; ui<frameData.size();ui++)
		minFrame=std::min(minFrame,frameData[ui].first);
	return minFrame;
}

size_t FrameProperties::getMaxFrame() const
{
	size_t maxFrame=0;
	
	for(size_t ui=0; ui<frameData.size();ui++)
		maxFrame=std::max(maxFrame,frameData[ui].first);
	return maxFrame;
}

void FrameProperties::addKeyFrame(size_t frame, 
		const FilterProperty &p)
{
	frameData.push_back(make_pair(frame,p.data));	
}

void FrameProperties::addKeyFrame(size_t frame, 
		const std::string &s)
{
	frameData.push_back(make_pair(frame,s));	
}

bool FrameProperties::writeState(std::ostream &f, unsigned int format, unsigned int depth) const
{
	f << tabs(depth) << "<frame>" << endl;
	
	f <<tabs(depth+1) << "<filterid val=\"" << filterId << "\"/>" << endl; 
	f <<tabs(depth+1) << "<propertykey val=\"" << propertyKey<< "\"/>" << endl; 

	//Dump the frame data vector
	f << tabs(depth+1) << "<framedata>" << endl;
	for(unsigned int ui=0;ui<frameData.size();ui++)
	{
		f << tabs(depth+2) << "<frame offset=\"" << 
			frameData[ui].first << "\" data=\"" <<
				frameData[ui].second << "\"/>" << endl;
	}
	f << tabs(depth+2) << "<interpdata mode=\"" << getInterpMode() <<  "\"/>" << endl; 
	f << tabs(depth+1) << "</framedata>" << endl;


	f << tabs(depth) << "</frame>" << endl;
	return true;
}


std::string InterpData::getInterpolatedData(const vector<pair<size_t,
 					std::string>  > &keyData,size_t frame) const
{

	std::string resStr;

	switch(interpMode)
	{
		case INTERP_STEP:
		case INTERP_STEP_POINT3D:
		{
			ASSERT(keyData.size() ==1);

			ASSERT(keyData[0].first==frame)
		
			return keyData[0].second;
		}
		case INTERP_LINEAR_FLOAT:
		{
			ASSERT(keyData.size() ==2);

			float a, b;
			size_t startF,endF;

			//Either way around, it should successfully
			// transfer
			ASSERT(!stream_cast(a,keyData[0].second));
			ASSERT(!stream_cast(b,keyData[1].second));

			//Flip the key data such that it is the correct way 'round
			if(keyData[0].first < keyData[1].first)
			{
				stream_cast(a,keyData[0].second);
				stream_cast(b,keyData[1].second);
				startF=keyData[0].first;
				endF=keyData[1].first;
			}
			else
			{
				stream_cast(a,keyData[1].second);
				stream_cast(b,keyData[0].second);
				startF=keyData[0].first;
				endF=keyData[1].first;
			}

			//Obtain the linearly interpolated result
			float res;
			res=interpLinearRamp(startF,endF,frame,a,b);

			stream_cast(resStr,res);
			return resStr;
		}
		case INTERP_LINEAR_COLOUR:
		{
			ASSERT(keyData.size() ==2);
			//TODO: I don't have the internets here,
			// so I can't look up the RGB->HSV interpolation
			// matrix. HSV interpolation should look more natural
			
			//Perform linear RGB interpolation

			//Parse the colour start and end strings
			//---------
			ColourRGBA tmpCol[2];

			
			tmpCol[0].parse(keyData[0].second);
			tmpCol[1].parse(keyData[1].second);
		
			//---------
			
			//Get and flip the key data such that it is the correct way 'round
			size_t startF,endF;
			if(keyData[0].first < keyData[1].first)
			{
				startF=keyData[0].first;
				endF=keyData[1].first;
			}
			else
			{
				startF=keyData[1].first;
				endF=keyData[0].first;
			}
		
			//interpolate the colour value
			ColourRGBAf interpCol;
			float delta;
			delta = (float)(frame - startF )/ (float)(endF - startF);
			interpCol=tmpCol[0].toRGBAf().interpolate(delta,tmpCol[1].toRGBAf());
			return interpCol.toColourRGBA().rgbaString();
		}
		case INTERP_LIST:
		{
			ASSERT(keyData.size());

			size_t frameOffset=keyData[0].first;
			ASSERT(frame-frameOffset <keyData.size());
			ASSERT(keyData[frame-frameOffset].first ==frame);
			return keyData[frame-frameOffset].second;

		}
		case INTERP_LINEAR_POINT3D:
		{
			ASSERT(keyData.size() ==2);

			Point3D a, b;
			size_t startF,endF;


			//Flip the key data such that it is the correct way 'round
			if(keyData[0].first < keyData[1].first)
			{
				a.parse(keyData[0].second);
				b.parse(keyData[1].second);
				startF=keyData[0].first;
				endF=keyData[1].first;
			}
			else
			{
				a.parse(keyData[1].second);
				b.parse(keyData[0].second);
				startF=keyData[1].first;
				endF=keyData[0].first;
			}

			Point3D interpPt;
			for(size_t ui=0;ui<3;ui++)
			{
				interpPt[ui] = interpLinearRamp(startF,endF,frame,
									a[ui],b[ui]);
			}
			std::string res;
			stream_cast(res,interpPt);
			return res;
		}
		default:
			ASSERT(false);
	}

	//Should have returned by now...
	ASSERT(false);
}

float InterpData::interpLinearRamp(size_t startFrame, size_t endFrame, size_t curFrame,
					float a, float b) const
{
	ASSERT(startFrame!=endFrame);
	ASSERT(curFrame >=startFrame && curFrame <=endFrame);

	float frac;
	frac = ((float)(curFrame-startFrame)) / (float)(endFrame - startFrame);

	return frac*(b-a) + a;
}

