/*
 *	effect.cpp - 3D visuals effects implementation
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


#include "effect.h"



#include "common/xmlHelper.h"
#include "common/stringFuncs.h"
#include "common/constants.h"

//OpenGL includes
//MacOS is "special" and puts it elsewhere
#ifdef __APPLE__ 
	#include <OpenGL/glu.h>
#else
	#include <GL/glu.h>
#endif



Camera* Effect::curCam=0;
BoundCube Effect::bc;
float MIN_CROP_FRACTION=0.0001;

const unsigned int NUM_EFFECTS=2;
const char *EFFECT_NAMES[] = { "boxcrop",
				"anaglyph"
				};

//factory functions
Effect *makeEffect(unsigned int effectID)
{
	Effect *e;
	switch(effectID)
	{
		case EFFECT_ANAGLYPH:
			e = new AnaglyphEffect;
			break;

		case EFFECT_BOX_CROP:
			e=new BoxCropEffect;
			break;
	
		default:
			ASSERT(false);
	}

	return e;
}



Effect *makeEffect(const std::string &str)
{
	Effect *e=0;

	for(unsigned int ui=0;ui<NUM_EFFECTS; ui++)
	{
		if(str == EFFECT_NAMES[ui])
		{
			e=makeEffect(ui);
			break;
		}
	}

	return e;
}

//Colour matrices, for accumulation buffer stereo

//Green blue matrix. Useful in both "mixed" and "half" modes
float gbMatrix[]={0.0f,0.0f,0.0f,0,
			0.0f,1.0f,0.0f,0,
			0.0f,0.0f,1.0f,0,
			0,   0,   0,   1};
//"Mixed" matrix -- useful in , well, mixed mode
float mixedMatrix[]={	0.0f,0.7f,0.3f,0,
				0.0f,0.0f,0.0f,0,
				0.0f,0.0f,0.0f,0,
				0,   0,   0,   1};

float halfMatrix[]={	0.299f, 0.587f,	0.114f,	0,
				0.0f,	0.0f,	0.0f,	0,
				0.0f,	0.0f,	0.0f,	0,
				0,   	0,   	0,   	1};

Effect::Effect()
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(EFFECT_NAMES) == NUM_EFFECTS);
}

std::string Effect::getName() const
{
	return EFFECT_NAMES[this->getType()];
}


BoxCropEffect::BoxCropEffect() : openGLIdStart(0), useCamCoordinates(false)
{
	effectType=EFFECT_BOX_CROP;
}

Effect *BoxCropEffect::clone() const
{
	Effect *e = new BoxCropEffect;

	*e=*this;
	return e;
}

void BoxCropEffect::enable(unsigned int pass) const
{

	//we only need to do anything on the first pass. All other passes are unchanged
	if(pass)
		return;
	//Compute the bounding box that is the clipped boundary
	Point3D pAAB[2]; //Axis aligned box
	bc.getBounds(pAAB[0],pAAB[1]);

	Point3D pCentre;
	pCentre = (pAAB[0] + pAAB[1])*0.5f;
		
	unsigned int glOffset=openGLIdStart;

	if(useCamCoordinates)
	{
		Point3f pBox[8];
		for(unsigned int ui=0;ui<8;ui++)
		{
			//Counting in binary to generate box corner vertices.
			pBox[ui].fx = pAAB[(ui>>2) &1][0];
			pBox[ui].fy = pAAB[(ui>>1) &1][1];
			pBox[ui].fz = pAAB[ui&1][2];
		}
		//Translate to rotate around the box centre 
		for(unsigned int ui=0;ui<8;ui++)
		{
			pBox[ui].fx-=pCentre[0]; 
			pBox[ui].fy-=pCentre[1]; 
			pBox[ui].fz-=pCentre[2]; 
		}

		Point3D x,y,z;
		//get camera orientation data		
		z= curCam->getUpDirection();
		y= curCam->getViewDirection();

		//We need to first do a "passive" coordinate transformation on the box coordinates
		//to determine the box coordinates in camera basis vectors (after translation such tat
		//centre of box is in centre of world).

		//Active transformations are v'=Tv_orig. Passive transformation is that v_prime = T^-1 v_orig
		//

		z.normalise(); //Not needed, I think.. but can't hurt
		y.normalise();
		x= z.crossProd(y);

		float angle;
		angle=z.angle(Point3D(0,0,1));



		//If needed, perform a rotation to align the box up
		//vector with the camera up vector
		Point3f r;
		Point3D yTmpRot;
		if(fabs(angle) > sqrtf(std::numeric_limits<float>::epsilon()))
		{
			Point3D rotateAxis;


			//Check for numerical stability problem when camera 
			//& world z axes point exactly apart			
			if( fabs(angle-M_PI) <sqrtf(std::numeric_limits<float>::epsilon()))
				rotateAxis=Point3D(1,0,0); //Pick *any* vector in X-Y plane.
			else
				rotateAxis = z.crossProd(Point3D(0,0,1));
			rotateAxis.normalise();

			r.fx=rotateAxis[0];
			r.fy=rotateAxis[1];
			r.fz=rotateAxis[2];
			
			for(unsigned int ui=0;ui<8;ui++)
				quat_rot(&(pBox[ui]),&r,angle);


			Point3f yRot;
			yRot.fx=0;
			yRot.fy=1;
			yRot.fz=0;
			quat_rot(&yRot,&r,angle);

			yTmpRot=Point3D(yRot.fx,yRot.fy,yRot.fz);

			ASSERT(yTmpRot.sqrMag() > sqrtf(std::numeric_limits<float>::epsilon()));

		}
		else
			yTmpRot=Point3D(0,1,0);

		//Rotating around the z axis to set "spin"
		r.fx=z[0];
		r.fy=z[1];
		r.fz=z[2];

		angle=y.angle(yTmpRot);


		if( fabs(angle) > sqrtf(std::numeric_limits<float>::epsilon()))
		{
			//Spin the box around to match the final coordinate system
			for(unsigned int ui=0;ui<8;ui++)
				quat_rot(&(pBox[ui]),&r,angle);
		}


		//Now compute the box coordinates, then break their position
		//vectors (from box centre) down into the basis
		//coordinates of our camera
		Point3D pBoxVertices[8];

		for(unsigned int ui=0;ui<8;ui++)
			pBoxVertices[ui]=  Point3D(pBox[ui].fx,
						pBox[ui].fy,pBox[ui].fz);
			
		float dotValue[3];
		dotValue[0]=dotValue[1]=dotValue[2]=-std::numeric_limits<float>::max();
		//Find the largest positive basis components (these form the camera BB limits)
		for(unsigned int ui=0;ui<8;ui++)
		{
			float tmp;
			tmp =x.dotProd(pBoxVertices[ui]); 
			if(tmp > dotValue[0])
				dotValue[0]=tmp;
			
			tmp =y.dotProd(pBoxVertices[ui]); 
			if(tmp > dotValue[1])
				dotValue[1]=tmp;
		
			tmp =z.dotProd(pBoxVertices[ui]); 
			if(tmp > dotValue[2])
				dotValue[2]=tmp;
			
		}

		//Compute the cropping deltas in the range [-1,1]
		float dC[6];
		for(unsigned int ui=0;ui<6;ui++)
		{
			
			if(ui&1)
			{
				//upper
				dC[ui] =2.0*(0.5- cropFractions[ui]);
			}
			else
			{
				//Lower
				dC[ui]=2.0*(cropFractions[ui]-0.5);
			}
		}
	
		//Cropping delta * dotproduct *basisvector == crop point
		//Note the reversal of the Z and Y vectors
		pAAB[0] = pCentre + x*dotValue[0]*dC[0] 
				+y*dotValue[1]*dC[2]+z*dotValue[2]*dC[4];
		pAAB[1] = pCentre + x*dotValue[0]*dC[1] 
				+y*dotValue[1]*dC[3]+z*dotValue[2]*dC[5];


		//Draw crop iff crop fractions are +ve

		//X
		if(cropFractions[0] >=MIN_CROP_FRACTION)
		{
			doClip(pAAB[0],x,glOffset);
			glOffset++;
		}
		if(cropFractions[1] >=MIN_CROP_FRACTION)
		{
			doClip(pAAB[1],-x,glOffset);
			glOffset++;
		}

		//Y
		if(cropFractions[2] >=MIN_CROP_FRACTION)
		{
			doClip(pAAB[0],y,glOffset);
			glOffset++;
		}
		if(cropFractions[3] >=MIN_CROP_FRACTION)
		{
			doClip(pAAB[1],-y,glOffset);
			glOffset++;
		}

		//Z
		if(cropFractions[4] >=MIN_CROP_FRACTION)
		{
			doClip(pAAB[0],z,glOffset);
			glOffset++;
		}
		if(cropFractions[5] >=MIN_CROP_FRACTION)
		{
			doClip(pAAB[1],-z,glOffset++);
			glOffset++;
		}
	}
	else
	{
		pAAB[0] = pCentre + Point3D(0.5-cropFractions[0],
				0.5-cropFractions[2],0.5-cropFractions[4])*(pAAB[0]-pCentre)*2.0;
		pAAB[1] = pCentre + Point3D(0.5-cropFractions[1],
				0.5-cropFractions[3],0.5-cropFractions[5])*(pAAB[1]-pCentre)*2.0;

		for(unsigned int ui=0;ui<6;ui++)
		{
			Point3D normal;

			//Don't  update minimum crop fractions
			if(cropFractions[ui] < MIN_CROP_FRACTION)
				continue;
			//Set up the normal & origin (use rectangular prism vertex as origin)
			normal=Point3D(0,0,0);
			normal.setValue(ui/2,1);
			if(ui&1)
			{
				normal=-normal;
				doClip(pAAB[1],normal,glOffset);
			}
			else
				doClip(pAAB[0],normal,glOffset);

			glOffset++;
		}
	}
}

void BoxCropEffect::doClip(const Point3D &origin, const Point3D &normal, unsigned int glOffset) const
{
	double array[4]; 
	//Ax + By + Cz + D =0. Prove from 
	//n.dot(v-p_0)=0
	array[0]=normal[0];
	array[1]=normal[1];
	array[2]=normal[2];
	array[3] = -normal.dotProd(origin);


	glMatrixMode(GL_MODELVIEW);

	//Set up the effect
	glClipPlane(GL_CLIP_PLANE0 +glOffset,
			array);
	glEnable(GL_CLIP_PLANE0+glOffset);


}

void BoxCropEffect::disable() const
{
	unsigned int startId=openGLIdStart;

	for(unsigned int ui=0; ui<6;ui++)
	{
		if(cropFractions[ui]>= MIN_CROP_FRACTION)
		{
			glDisable(GL_CLIP_PLANE0+startId);
			startId++;
		}
	}
}

bool BoxCropEffect::willDoSomething() const
{
	for(unsigned int ui=0;ui<6;ui++)
	{
		if(cropFractions[ui]>=MIN_CROP_FRACTION)
			return true;
	}

	return false;
}

void BoxCropEffect::setFractions(const float *frac)
{
	for(unsigned int ui=0;ui<6;ui++)
		cropFractions[ui]=frac[ui];
}

float BoxCropEffect::getCropValue(unsigned int pos) const
{
	ASSERT(pos<6);
	return cropFractions[pos];
}


void BoxCropEffect::getCroppedBounds(BoundCube &b)  const
{
	Point3D pLow,pHi;
	b.getBounds(pLow,pHi);

	Point3D pCentre = (pLow+pHi)*0.5;
	pLow = pCentre + Point3D(0.5-cropFractions[0],
			0.5-cropFractions[2],0.5-cropFractions[4])*(pLow-pCentre)*2.0;
	pHi = pCentre + Point3D(0.5-cropFractions[1],
			0.5-cropFractions[3],0.5-cropFractions[5])*(pHi-pCentre)*2.0;

	b.setBounds(pLow,pHi);
}

bool BoxCropEffect::writeState(std::ofstream &f, unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
			f << tabs(depth+1) << "<boxcrop>" << endl;

			f << tabs(depth+2) << "<cropvalues>" << endl;
			for(unsigned int ui=0;ui<6;ui++)
			{
				f << tabs(depth+3) << "<scalar value=\"" << 
					cropFractions[ui] << "\"/>" << endl;
			}

			f << tabs(depth+2) << "</cropvalues>" << endl;


			f << tabs(depth+2) << "<usecamcoordinates value=\"" << (int)useCamCoordinates<<   "\"/>" << endl;
			f << tabs(depth+1) << "</boxcrop>" << endl;
			break;

		default:
			ASSERT(false);
			return false;

	}

	return true;
}


bool BoxCropEffect::readState(xmlNodePtr nodePtr)
{
	using std::string;

	if(!nodePtr->xmlChildrenNode)
		return false;

	nodePtr=nodePtr->xmlChildrenNode;
	xmlNodePtr scalars;
	if(XMLHelpFwdToElem(nodePtr,"cropvalues"))
		return false;

	scalars=nodePtr->xmlChildrenNode;

	for(unsigned int ui=0;ui<6;ui++)
	{
		if(!XMLGetNextElemAttrib(scalars,cropFractions[ui],"scalar","value"))
			return false;
	}

	string s;
	if(!XMLGetNextElemAttrib(nodePtr,s,"usecamcoordinates","value"))
		return false;

	if(s=="0")
		useCamCoordinates=false;
	else if (s == "1")
		useCamCoordinates=true;
	else
		return false;


	return true;
}


AnaglyphEffect::AnaglyphEffect() : colourMode(ANAGLYPH_REDBLUE), eyeFlip(false),
	oldCam(0),baseShift(0.01f)
{
	effectType=EFFECT_ANAGLYPH;
}

Effect *AnaglyphEffect::clone() const
{
	Effect *e = new AnaglyphEffect;

	*e=*this;
	return e;
}

void AnaglyphEffect::enable(unsigned int passNumber) const
{
	if(passNumber >1 || curCam->type() !=CAM_LOOKAT) 
		return;

	if(passNumber==0)
	{
		ASSERT(!oldCam);
		oldCam=curCam->clone();
		//Translate both the target, and the origin
		curCam->translate(baseShift,0);
		//Apply the frustum offset to restore the shifted focal plane
		((CameraLookAt*)curCam)->setFrustumDistort(baseShift);

	}
	else
	{
		*curCam=*oldCam;
		//this time, in the reverse direction;
		//Translate both the target, and the origin
		curCam->translate(-baseShift,0);
		//Apply the frustum offset to restore the shifted focal plane
		((CameraLookAt*)curCam)->setFrustumDistort(-baseShift);
	}


	//Different type of glasses use different colour masks.
	const bool maskArray[][6] = { {true,false,false,false,false,true}, //Red-blue
					{true,false,false,false,true,false}, //red-green
					{true,false,false,false,true,true}, // red-cyan
					{false,true,false,true,false,true} //green-magenta
				};
	
	//Colour buffer masking method
	if(passNumber == 0)
	{
		glClear(GL_COLOR_BUFFER_BIT );
	}	
	else
		glClear(GL_DEPTH_BUFFER_BIT);


	//we flip either due to the pass, or because the user has
	//back to front glasses.
	unsigned int offset;
	if(passNumber ^ eyeFlip)
		offset=0;
	else
		offset=3;
	unsigned int idx = colourMode-ANAGLYPH_REDBLUE;

	glColorMask(maskArray[idx][offset],maskArray[idx][offset+1],
			maskArray[idx][offset+2],true);

	

}

void AnaglyphEffect::setMode(unsigned int mode)
{
	ASSERT(colourMode<ANAGLYPH_ENUM_END);
	colourMode=mode;
}


void AnaglyphEffect::disable() const
{
	//disable the colour mask
	glColorMask(true,true,true,true);


	ASSERT(oldCam);
	*curCam=*oldCam;
	delete oldCam;
#ifdef DEBUG
	oldCam=0;
#endif
}

bool AnaglyphEffect::writeState(std::ofstream &f, unsigned int format,unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
			f << tabs(depth+1) << "<anaglyph>" << endl;
			f << tabs(depth+2) << "<colourmode value=\"" << colourMode << "\"/>" << endl;

			f << tabs(depth+2) << "<eyeflip value=\"" << (int)eyeFlip <<   "\"/>" << endl;
			f << tabs(depth+2) << "<baseshift value=\"" << baseShift<<   "\"/>" << endl;
			f << tabs(depth+1) << "</anaglyph>" << endl;
			break;

		default:
			ASSERT(false);
			return false;

	}

	return true;
}


bool AnaglyphEffect::readState(xmlNodePtr nodePtr)
{
	using std::string;

	if(!nodePtr->xmlChildrenNode)
		return false;
	nodePtr=nodePtr->xmlChildrenNode;
	
	if(!XMLGetNextElemAttrib(nodePtr,colourMode,"colourmode","value"))
		return false;
	if(colourMode >= ANAGLYPH_HALF_COLOUR)
		return false;


	string s;	
	if(!XMLGetNextElemAttrib(nodePtr,s,"eyeflip","value"))
		return false;


	if(s == "0")
		eyeFlip=false;
	else if(s == "1")
		eyeFlip=true;
	else
		return false;

	if(!XMLGetNextElemAttrib(nodePtr,baseShift,"baseshift","value"))
		return false;

	return true;
}


