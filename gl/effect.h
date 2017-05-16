/*
 *	effect.h - opengl 3D effects header
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

#ifndef EFFECT_H
#define EFFECT_H

#ifdef ATTRIBUTE_PRINTF
	#pragma push_macro("ATTRIBUTE_PRINTF")
	#include <libxml/xmlreader.h>
	#pragma pop_macro(" ATTRIBUTE_PRINTF")
#else
	#include <libxml/xmlreader.h>
	#undef ATTRIBUTE_PRINTF
#endif

#include "cameras.h"

//opengl allows up to 6 clipping planes
const unsigned int MAX_OPENGL_CLIPPLANES=6;

//Effect IDs
enum
{
	EFFECT_BOX_CROP=0,
	EFFECT_ANAGLYPH,
	EFFECT_ENUM_END
};


enum{
	//Colour mask methods
	ANAGLYPH_REDBLUE,
	ANAGLYPH_REDGREEN,
	ANAGLYPH_REDCYAN,
	ANAGLYPH_GREENMAGENTA,
	//Colour matrix +accumulation buffer methods
	ANAGLYPH_HALF_COLOUR,
	ANAGLYPH_MIXED,
	ANAGLYPH_ENUM_END //Not a method. end of enum
};

class Effect
{
	protected:
		static Camera *curCam;
		static BoundCube bc;
		unsigned int effectType;
	public:
		Effect();
		virtual ~Effect() {};

		virtual Effect *clone() const = 0;
		virtual void enable(unsigned int pass=0) const =0;
		virtual void disable() const=0;
		std::string getName() const;


		//Write the effect's state information to file
		virtual bool writeState(std::ofstream &f, 
				unsigned int format, unsigned int depth) const=0;
		//read the effects state information from an XML file
		// Should be pointing to the top-level of effect element (eg <anaglyph>)
		virtual bool readState(xmlNodePtr n)=0;

		virtual bool needCamUpdate() const { return false;}

		//!Returns true if the effect has any influence on the output
		virtual bool willDoSomething() const=0;

		virtual unsigned int numPassesNeeded() const { return 1;}

		virtual unsigned int getType() const { return effectType;};
		static void setCurCam(Camera *c) {curCam=c;}
		static void setBoundingCube(const BoundCube &c) {bc=c;}

};


class BoxCropEffect : public Effect
{
	private:
		//controlling ID values for gl plane. No more than MAX_OPENGL_CLIPPLANES allowed
		unsigned int openGLIdStart,openGLIdEnd;
		//Cropping margins (Fraction from edge towards opposite edge (complete)). 0->1. 
		//Opposing edges must sum to 0->1. (xLo,xHi,yLo...)
		float cropFractions[6];
		//!True if we should transform to camera coordinates before applying crop
		bool useCamCoordinates;

		//!Aspect ratio of output image
		float outputAspect;

		void doClip(const Point3D &origin, const Point3D & normal,unsigned int glOffset) const;
	public:
		BoxCropEffect();
		virtual ~BoxCropEffect(){}; 


		//Duplicate thi
		Effect *clone() const;

		//!Enable the clipping plane. Values *must* be set before calling
		void enable(unsigned int pass) const;

		//!DIsable the clipping plane
		void disable() const;



		//Write the effect's state information to file
		bool writeState(std::ofstream &f, unsigned int format,
				unsigned int depth) const;
		//read the effects state information from an XML file
		bool readState(xmlNodePtr n);

		//!Returns true if the effect has any influence on the output
		bool willDoSomething() const;

		//!Set the fractions of cube from margin
		//-- there should be 6 floats (x,y,z)_(low,high) (x_lo, x_hi....)
		//  each low/hi should form a sum between 0 and 1.
		void setFractions(const float *fractionArray);

		void useCamCoords(bool enable){useCamCoordinates=enable;};

		//!Alters the input box to generate cropping bounding box
		//note the box may be inside out if the cropping limits
		//exceed themselves..
		void getCroppedBounds(BoundCube &b) const;

		float getCropValue(unsigned int pos) const;
};

class AnaglyphEffect : public Effect
{
	private:
		unsigned int colourMode;
		bool eyeFlip;
		
		mutable Camera *oldCam;
		float baseShift;

	public:
		AnaglyphEffect();
		~AnaglyphEffect(){}; 
		//Duplicate thi
		Effect *clone() const;

		//!Enable the clipping plane. Values *must* be set before calling
		void enable(unsigned int pass) const;

		//!DIsable the clipping plane
		void disable() const;

		//Write the effect's state information to file
		bool writeState(std::ofstream &f, unsigned int format,
				unsigned int depth) const;
		//read the effects state information from an XML file
		bool readState(xmlNodePtr n);
		
		//!Whether we should be flipping the lens from its hard-coded left-right
		void setFlip(bool shouldFlip) {eyeFlip=shouldFlip;};

		void setMode(unsigned int mode);
		void setBaseShift(float shift) { baseShift=shift;};

		bool needCamUpdate() const { return true;}
		//!Returns true if the effect has any influence on the output
		bool willDoSomething() const {return true;};

		virtual unsigned int numPassesNeeded() const { return 2;}


		float getBaseShift() const { return baseShift;};
		unsigned int getMode() const { return colourMode;};

			
};


Effect *makeEffect(unsigned int effectID);

Effect *makeEffect(const std::string &s);


#endif
