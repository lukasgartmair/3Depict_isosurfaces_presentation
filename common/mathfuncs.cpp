/*
 *	mathfuncs.cpp - Miscellaneous mathematical functions implementation. 
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

#include "../config.h"

#include "common/basics.h"
#include "common/stringFuncs.h"

#include <sys/time.h>

#include <gsl/gsl_blas.h>

using std::string;
using std::vector;

const int MBIG = std::numeric_limits<int>::max();

void Point3D::copyValueArr(float *valArr) const
{
	ASSERT(valArr);
	//compiler should unroll this automatically
	for(unsigned int ui=0; ui<3; ui++)
	{
		*(valArr+ui) = *(value+ui);
	}
}

float Point3D::operator[](unsigned int ui) const 
{
	ASSERT(ui < 3);
	return value[ui];
}

float &Point3D::operator[](unsigned int ui) 
{
	ASSERT(ui < 3);
	return value[ui];
}

bool Point3D::operator==(const Point3D &pt) const
{
	return (value[0] == pt.value[0] && value[1] == pt.value[1] && value[2] == pt.value[2]);
}

const Point3D &Point3D::operator=(const Point3D &pt)
{
	value [0] = pt.value[0];
	value [1] = pt.value[1];
	value [2] = pt.value[2];
	return *this;
}

const Point3D &Point3D::operator+=(const Point3D &pt)
{
	for(unsigned int ui=0;ui<3; ui++)
		value[ui]+= pt.value[ui];
	
	return *this;
}

const Point3D &Point3D::operator-=(const Point3D &pt)
{
	for(unsigned int ui=0;ui<3; ui++)
		value[ui]-= pt.value[ui];
	
	return *this;
}

const Point3D Point3D::operator+(const Point3D &pt) const
{
	Point3D ptTmp;
	
	for(unsigned int ui=0;ui<3; ui++)
		ptTmp.value[ui] = value[ui]  + pt.value[ui];
	
	return ptTmp;
}

const Point3D Point3D::operator+(float f)  const
{
	Point3D pTmp;
	for(unsigned int ui=0;ui<3; ui++)
		pTmp.value[ui] = value[ui]  + f;

	return pTmp;
}

const Point3D Point3D::operator-(const Point3D &pt) const
{
	Point3D ptTmp;
	
	for(unsigned int ui=0;ui<3; ui++)
		ptTmp.value[ui] = value[ui]  - pt.value[ui];
	
	return ptTmp;
}

const Point3D Point3D::operator-() const
{
	Point3D ptTmp;
	
	for(unsigned int ui=0;ui<3; ui++)
		ptTmp.value[ui] = -value[ui];
	
	return ptTmp;
}

const Point3D &Point3D::operator*=(const float scale)
{
	value[0] = value[0]*scale;
	value[1] = value[1]*scale;
	value[2] = value[2]*scale;
	
	return *this;
}

const Point3D Point3D::operator*(float scale) const
{
	Point3D tmpPt;
	
	tmpPt.value[0] = value[0]*scale;
	tmpPt.value[1] = value[1]*scale;
	tmpPt.value[2] = value[2]*scale;
	
	return tmpPt;
}

const Point3D Point3D::operator*(const Point3D &pt) const
{
	Point3D tmpPt;
	
	tmpPt.value[0] = value[0]*pt[0];
	tmpPt.value[1] = value[1]*pt[1];
	tmpPt.value[2] = value[2]*pt[2];
	
	return tmpPt;
}

const Point3D Point3D::operator/(float scale) const
{
	Point3D tmpPt;

	scale = 1.0f/scale;
	tmpPt.value[0] = value[0]*scale;
	tmpPt.value[1] = value[1]*scale;
	tmpPt.value[2] = value[2]*scale;

	return tmpPt;
}

const Point3D Point3D::operator/(const Point3D &pt) const
{
	Point3D tmpPt;
	for(unsigned int ui=0;ui<3;ui++)
		tmpPt.value[ui] = value[ui]/pt[ui];

	return tmpPt;
}

float Point3D::sqrDist(const Point3D &pt) const
{
	return (pt.value[0]-value[0])*(pt.value[0]-value[0])+
		(pt.value[1]-value[1])*(pt.value[1]-value[1])+
		(pt.value[2]-value[2])*(pt.value[2]-value[2]);
}
		
float Point3D::dotProd(const Point3D &pt) const
{
	//Return the inner product
	return value[0]*pt.value[0] + value[1]*pt.value[1] + value[2]*pt.value[2];
}

Point3D Point3D::crossProd(const Point3D &pt) const
{
	Point3D cross;

	cross.value[0] = (pt.value[2]*value[1] - pt.value[1]*value[2]);
	cross.value[1] = -(value[0]*pt.value[2] - pt.value[0]*value[2]);
	cross.value[2] = (value[0]*pt.value[1] - value[1]*pt.value[0]);

	return cross;
}

bool Point3D::insideBox(const Point3D &farPoint) const
{
	
	return (value[0] < farPoint.value[0] && value[0] >=0) &&
		(value[1] < farPoint.value[1] && value[1] >=0) &&
		(value[2] < farPoint.value[2] && value[2] >=0);
}

bool Point3D::insideBox(const Point3D &lowPt,const Point3D &highPt) const
{
	
	return (value[0] < highPt.value[0] && value[0] >=lowPt.value[0]) &&
		(value[1] < highPt.value[1] && value[1] >=lowPt.value[1]) &&
		(value[2] < highPt.value[2] && value[2] >=lowPt.value[2]);
}

//This is different to +=, because it generates no return value
void Point3D::add(const Point3D &obj)
{
	value[0] = obj.value[0] + value[0];
	value[1] = obj.value[1] + value[1];
	value[2] = obj.value[2] + value[2];
}

void Point3D::extend(float distance)
{
	ASSERT(sqrMag() > 0.0f);
	
	Point3D p;
	p=*this;
	p.normalise();
	*this+=p*distance;
}

float Point3D::sqrMag() const
{
	return value[0]*value[0] + value[1]*value[1] + value[2]*value[2];
}

Point3D Point3D::normalise()
{
	float mag = sqrtf(sqrMag());

	value[0]/=mag;
	value[1]/=mag;
	value[2]/=mag;

	return *this;
}

void Point3D::negate() 
{
	value[0] = -value[0];
	value[1] = -value[1];
	value[2] = -value[2];
}

float Point3D::angle(const Point3D &pt) const
{
#ifndef EQ_TOL
	#define EQ_TOL(f,g) (fabs( (f) - (g)) < sqrtf(std::numeric_limits<float>::epsilon()))
#endif
	//Check for near degenerate case. Acos does not appreciate small arguments in the dot product.
	if( EQ_TOL(pt.value[0] , value[0]) &&  
		 EQ_TOL(pt.value[1] , value[1]) &&  
		 EQ_TOL(pt.value[2] , value[2]) )  
		return 0;
	double param = dotProd(pt)/sqrtf(sqrMag()*pt.sqrMag());

	//trap the domain - on some systems, there are tolerance problems here
	if(param > 1.0f || param < -1.0f)
		return M_PI;
	else
		return acos(param); 
}


void Point3D::sphericalAngles(float &theta, float &phi) const
{
	float sqrVal=sqrMag();
	theta=acos(value[2]/sqrtf(sqrVal));
	//phi
	phi=atan2(value[1],value[0]);

#ifdef DEBUG
	static bool amRecursing;
	if(amRecursing)
		return;
	amRecursing=true;
	//Check that the definition of the spherical coordinates matches
	Point3D retreived(sin(theta)*cos(phi),sin(theta)*sin(phi),cos(theta));
	float tmpTheta,tmpPhi;
	retreived.sphericalAngles(tmpTheta,tmpPhi);
	ASSERT(EQ_TOL(tmpTheta,theta));
	ASSERT(EQ_TOL(tmpPhi,phi));

	retreived*=sqrtf(sqrVal);
	for(unsigned int ui=0;ui<3;ui++)
	{
		ASSERT(EQ_TOL(retreived[ui] ,value[ui]));
	}
	amRecursing=false;
#endif
}

bool Point3D::orthogonalise(const Point3D &pt)
{
	Point3D crossp;
	crossp=this->crossProd(pt);

	//They are co-linear, or near-enough to be not resolvable.
	if(crossp.sqrMag()  < sqrtf(std::numeric_limits<float>::epsilon()))
		return false;
	crossp.normalise();

	crossp=crossp.crossProd(pt);
	*this=crossp.normalise()*sqrtf(this->sqrMag());	

	return true;
}

Point3D Point3D::centroid(const Point3D *p, unsigned int n)
{
	ASSERT(p);
	Point3D centroid(0,0,0);

	/* This code should work, but is not profiled. No caller uses
	   this code at time of writing.
#ifdef _OPENMP

	//Parallel version
	//--
	vector<Point3D> centroids(omp_get_max_threads(),Point3D(0,0,0));
#pragma omp parallel for 
	for(size_t ui=0;ui<n;ui++)
		centroids[omp_get_thread_num()]+=p[ui];

	for(size_t ui=0;ui<centroids.size();ui++)
		centroid+=centroids[ui];
	//--
#endif
	*/
	for(unsigned int ui=0;ui<n;ui++)
		centroid+=p[ui];

	centroid*=1.0f/(float)n;

	return centroid;
}

Point3D Point3D::centroid(const std::vector<Point3D> &p)
{
	Point3D centroid(0,0,0);

	for(unsigned int ui=0;ui<p.size();ui++)
		centroid+=p[ui];

	centroid*=1.0f/(float)p.size();

	return centroid;
}


bool Point3D::parse(const std::string &str)
{
	//Needs to be at minimum #,#,#
	if(str.size()< 5)
		return false;

	string tmpStr;
	tmpStr=stripWhite(str);


	//Two strings must be in sync
	std::string allowableStartChars, allowableEndChars;
	allowableStartChars="([{<'";
	allowableEndChars=")]}>'";

	size_t startPos,endPos;
	startPos=allowableStartChars.find(tmpStr[0]);
	endPos=allowableEndChars.find(tmpStr[tmpStr.size()-1]);

	//Strip the start/end chars 
	if(startPos !=std::string::npos && endPos != std::string::npos)
		tmpStr=tmpStr.substr(1,tmpStr.size()-1);
	else if (startPos !=endPos)
		return false; //we had one start bracket, but not the other...

	//First try splitting with non-whitespace separators,
	// there should be exactly 3 components
	vector<string> components;
	const char *NONWHITE_SEPARATOR=",;|_";
	splitStrsRef(tmpStr.c_str(),NONWHITE_SEPARATOR,
			components);
	if(components.size()!=3)
		return false;
	components.clear();

	//Now try splitting with whitespace components, dropping empty
	// strings. As we have already checked the non-whitespace components
	// additional components must be whitespace only, which is fine.

	const char *ALLOWABLE_SEPARATORS=",; \t|_";

	splitStrsRef(tmpStr.c_str(),ALLOWABLE_SEPARATORS,
			components);
	for(size_t ui=0;ui<components.size();ui++)
		components[ui]=stripWhite(components[ui]);
	
	//Drop the blank bits from the field
	vector<string>::iterator rmIt;
	rmIt=std::remove(components.begin(),components.end(),string(""));
	components.erase(rmIt,components.end());

	if(components.size()!=3)
		return false;

	float p[3];
	for(size_t ui=0;ui<3;ui++)
	{
		if(stream_cast(p[ui],components[ui]))
			return false;
	}


	setValueArr(p);
	
	return true;
}

#ifdef __LITTLE_ENDIAN__

void Point3D::switchEndian()
{
	floatSwapBytes(&value[0]);
	floatSwapBytes(&value[1]);
	floatSwapBytes(&value[2]);
}
#endif

std::ostream& operator<<(std::ostream &stream, const Point3D &pt)
{
	stream << "(" << pt.value[0] << "," << pt.value[1] << "," << pt.value[2] << ")";
	return stream;
}


void Point3D::transform3x3(const float *matrix)
{
	for(unsigned int ui=0;ui<3;ui++)
	{
		value[ui] = value[ui]*matrix[ui*3] + 
			value[ui]*matrix[ui*3+1] + value[ui]*matrix[ui*3+2];
	}
}
RandNumGen::RandNumGen()
{
	//Initialisation is NOT performed here, because we need a random seed
	//to generate our sequence....

	//we don't initially have Gaussian
	//value to spare
	haveGaussian=false;	
}

void RandNumGen::initialise(int seed)
{
	long mj,mk;

	//initialise ma[55] with seed
	mj=labs((MBIG-labs(seed)));
	mj%=MBIG;
	ma[55]=mj;
	mk=1;


	//Initialise the rest of the table
	for(unsigned int i=1; i<55; i++)
	{
		int ii;
		ii=(21*i)%55;
	
		ma[ii]=mk;
		mk=mj-mk;
		
		if(mk<0)
			mk+=MBIG;
		mj=ma[ii];
	}

	//"warm up" the rng
	for(unsigned int j=1;j<=4;j++)
	{
		for(unsigned int i=1;i<=55;i++)
		{
			ma[i] -=ma[1+ (i+30)%55];
			if(ma[i] < 0)
				ma[i]+=MBIG;
		}
	}
	//the constant 31 is special
	inext=0;
	inextp=31;
}


float RandNumGen::genUniformDev()
{
	long mj;

	if(++inext==56)
		inext=1;
	if(++inextp == 56)
		inextp=1;

	mj=ma[inext]-ma[inextp];
	if(mj<0)
		mj+=MBIG;

	ma[inext]=mj;

	return mj*(1.0/MBIG);
}


int RandNumGen::genInt()
{
	long mj;

	if(++inext==56)
		inext=1;
	if(++inextp == 56)
		inextp=1;

	mj=ma[inext]-ma[inextp];
	if(mj<0)
		mj+=MBIG;

	ma[inext]=mj;

	return mj;
}


//This is known as the Box-Muller transform
//You can change it from being a uniform variance 
//by simply multiplying by the std deviation of your choice
//this will cause the random number returned to be stretched 
//in the x axis from unit variance to your number
float RandNumGen::genGaussDev()
{
	float v1,v2,rsq,fac;
	//This algorithm generates
	//two Gaussian numbers from two Uniform Devs,
	//however we only want one. So to speed things up
	//remember the second and spit it out as required
	if(haveGaussian)
	{
		haveGaussian=false;
		return gaussSpare;
	}

	do
	{
		//grab two uniform Devs and 
		//move them into (-1,+1) domain
		v1=2.0f*genUniformDev()-1.0f;
		v2=2.0f*genUniformDev()-1.0f;
		rsq=v1*v1+v2*v2;
	//reject them if they don't lie in unit circle
	//or if rsq is at the origin of the unit circle
	//(as eqn below is undefined at origin)
	}while(rsq>=1.0f || rsq==0.0f);

	fac=sqrtf(-2.0f*log(rsq)/rsq);
	gaussSpare=v1*fac;
	haveGaussian=true;		
	return v2*fac;
	
}

int RandNumGen::initTimer()
{
	timeval tp;

	gettimeofday(&tp,NULL);

	initialise(tp.tv_sec+ tp.tv_usec);
	
	return tp.tv_sec + tp.tv_usec;
}

//quaternion multiplication, assuming q2 has no "a" component
void quat_mult_no_second_a(Quaternion *result, const Quaternion *q1, const Quaternion *q2)
{
	result->a = (-q1->b*q2->b-q1->c*q2->c -q1->d*q2->d);
	result->b = (q1->a*q2->b +q1->c*q2->d -q1->d*q2->c);
	result->c = (q1->a*q2->c -q1->b*q2->d +q1->d*q2->b);
	result->d = (q1->a*q2->d +q1->b*q2->c -q1->c*q2->b );
}

//this is a little optimisation that doesn't calculate the "a" component for
//the returned quaternion, and implicitly performs conjugation. 
//Note that the result is specific to quaternion rotation 
void quat_pointmult(Point3f *result, const Quaternion *q1, const Quaternion *q2)
{
	result->fx = (-q1->a*q2->b +q1->b*q2->a -q1->c*q2->d +q1->d*q2->c);
	result->fy = (-q1->a*q2->c +q1->b*q2->d +q1->c*q2->a -q1->d*q2->b);
	result->fz = (-q1->a*q2->d -q1->b*q2->c +q1->c*q2->b +q1->d*q2->a);

}

//Inefficient Point3D version
void quat_rot(Point3D &p, const Point3D &r, float angle)
{
	Point3f pP,rR;

	pP.fx =p[0]; pP.fy =p[1]; pP.fz =p[2]; 	
	rR.fx =r[0]; rR.fy =r[1]; rR.fz =r[2];

	quat_rot(&pP,&rR,angle);

	p[0] = pP.fx; p[1] =pP.fy; p[2] = pP.fz;
}


//Uses quaternion mathematics to perform a rotation around your favourite axis
//IMPORTANT: Rotvec must be normalised before passing to this function 
//failure to do so will have weird results. 
//For better performance on multiple rotations, use other function
//Note result is stored in returned point
void quat_rot(Point3f *point, const Point3f *rotVec, float angle)
{
	ASSERT(rotVec->fx*rotVec->fx + rotVec->fy*rotVec->fy + rotVec->fz*rotVec->fz - 1.0f < 
			5.0f*sqrtf(std::numeric_limits<float>::epsilon()));

	double sinCoeff;
       	Quaternion rotQuat;
	Quaternion pointQuat;
	Quaternion temp;
	
	//remember this value so we don't recompute it
#ifdef _GNU_SOURCE
	double cosCoeff;
	//GNU provides sincos which is about twice the speed of sin/cos separately
	sincos(angle*0.5f,&sinCoeff,&cosCoeff);
	rotQuat.a=cosCoeff;
#else
	angle*=0.5f;
	sinCoeff=sin(angle);
	
	rotQuat.a = cos(angle);
#endif	
	rotQuat.b=sinCoeff*rotVec->fx;
	rotQuat.c=sinCoeff*rotVec->fy;
	rotQuat.d=sinCoeff*rotVec->fz;

//	pointQuat.a =0.0f; This is implied in the pointQuat multiplication function
	pointQuat.b = point->fx;
	pointQuat.c = point->fy;
	pointQuat.d = point->fz;


	//perform  rotation
	quat_mult_no_second_a(&temp,&rotQuat,&pointQuat);
	quat_pointmult(point, &temp,&rotQuat);

}

//TODO: Make as efficient as possible

void quat_rot_array(Point3D  *pointArr, unsigned int n, 
			const Point3f *rotVec, float angle)
{
	Point3f *fArr;
	fArr = new Point3f[n];

	for(size_t ui=0;ui<n;ui++)
	{
		fArr[ui].fx = pointArr[ui][0];
		fArr[ui].fy = pointArr[ui][1];
		fArr[ui].fz = pointArr[ui][2];
	}

	quat_rot_array(fArr,n,rotVec,angle);

	for(size_t ui=0;ui<n;ui++)
	{
		 pointArr[ui][0]=fArr[ui].fx; 
		 pointArr[ui][1]=fArr[ui].fy;
		 pointArr[ui][2]=fArr[ui].fz;
	}

	delete[] fArr;
}
void quat_rot_array(Point3f *pointArr, unsigned int n,
			const Point3f *rotVec, float angle)
{
	Quaternion rotQuat;
	Quaternion pointQuat;
	Quaternion temp;
	{
		ASSERT(rotVec->fx*rotVec->fx + rotVec->fy*rotVec->fy + rotVec->fz*rotVec->fz - 1.0f < 
				5.0f*sqrtf(std::numeric_limits<float>::epsilon()));

		double sinCoeff;
		
		//remember this value so we don't recompute it
	#ifdef _GNU_SOURCE
		double cosCoeff;
		//GNU provides sincos which is about twice the speed of sin/cos separately
		sincos(angle*0.5f,&sinCoeff,&cosCoeff);
		rotQuat.a=cosCoeff;
	#else
		angle*=0.5f;
		sinCoeff=sin(angle);
		
		rotQuat.a = cos(angle);
	#endif	
		rotQuat.b=sinCoeff*rotVec->fx;
		rotQuat.c=sinCoeff*rotVec->fy;
		rotQuat.d=sinCoeff*rotVec->fz;

		for(unsigned int ui=0;ui<n; ui++)
		{
		//	pointQuat.a =0.0f; This is implied in the pointQuat multiplication function
			pointQuat.b = pointArr[ui].fx;
			pointQuat.c = pointArr[ui].fy;
			pointQuat.d = pointArr[ui].fz;


			//perform  rotation
			quat_mult_no_second_a(&temp,&rotQuat,&pointQuat);
			quat_pointmult(pointArr+ui, &temp,&rotQuat);

		}
	}
}

//Retrieve the quaternion for repeated rotation. Pass to the quat_rot_apply_quats
void quat_get_rot_quat(const Point3f *rotVec, float angle,Quaternion *rotQuat) 
{
	ASSERT(rotVec->fx*rotVec->fx + rotVec->fy*rotVec->fy + rotVec->fz*rotVec->fz - 1.0f < 
			5.0f*sqrtf(std::numeric_limits<float>::epsilon()));
	double sinCoeff;
#ifdef _GNU_SOURCE
	double cosCoeff;
	//GNU provides sincos which is about twice the speed of sin/cos separately
	sincos(angle*0.5f,&sinCoeff,&cosCoeff);
	rotQuat->a=cosCoeff;
#else
	angle*=0.5f;
	sinCoeff=sin(angle);
	rotQuat->a = cos(angle);
#endif	
	
	rotQuat->b=sinCoeff*rotVec->fx;
	rotQuat->c=sinCoeff*rotVec->fy;
	rotQuat->d=sinCoeff*rotVec->fz;
}

//Use previously generated quats from quat_get_rot_quats to rotate a point
void quat_rot_apply_quat(Point3f *point, const Quaternion *rotQuat)
{
	Quaternion pointQuat,temp;
//	pointQuat.a =0.0f; No need to set this, as we do not use it in the multiplication function
	pointQuat.b = point->fx;
	pointQuat.c = point->fy;
	pointQuat.d = point->fz;
	//perform  rotation
	quat_mult_no_second_a(&temp,rotQuat,&pointQuat);
	quat_pointmult(point, &temp,rotQuat);
}
	
//For the table to work, we need the sizeof(size_T) at preprocess time
#ifndef SIZEOF_SIZE_T
#error sizeof(size_t) macro is undefined... At time of writing, this is usually 4 (32 bit) or 8. You can work it out from a simple C++ program which prints out sizeof(size_t). This cant be done automatically due to preprocessor behaviour.
#endif

//Maximum period linear shift register values (computed by
//other program for Galois polynomial)
//Unless otherwise noted, all these entries have been verified using the
//verifyTable routine. 
//
//If you don't trust me, (who doesn't trust some random person on the internet?) 
//you can re-run the verification routine. 
//
//Note that verification time *doubles* with every entry, so full 64-bit verification
//is computationally intensive. I achieved 40 bits in half a day. 48 bits took over a week.
size_t maximumLinearTable[] = {
	  0x03,
	  0x06,
	  0x0C,
	  0x14,
	  0x30,
	  0x60,
	  0xb8,
	0x0110,
	0x0240,
	0x0500,
	0x0e08,
	0x1c80,
	0x3802,
	0x6000,
	0xb400,
	0x12000,
	0x20400,
	0x72000,
	0x90000,
	0x140000,
	0x300000,
	0x420000,
	0xD80000,
	0x1200000,
	0x3880000,
	0x7200000,
	0x9000000,
	0x14000000,
	0x32800000,
	0x48000000,

#if (SIZEOF_SIZE_T > 4)
	0xA3000000,
	0x100080000,
	0x262000000,
	0x500000000,
	0x801000000,
	0x1940000000,
	0x3180000000,
	0x4400000000,
	0x9C00000000,
	0x12000000000,
	0x29400000000,
	0x63000000000,
	0xA6000000000,
	0x1B0000000000,
	0x20E000000000,
	0x420000000000,
	0x894000000000,
	//Maximal linear table entries below line are unverified 
	//Verifying the full table might take a few months of computing time
	//But who needs to count beyond 2^49-1 (~10^14) anyway??
	0x1008000000000,

	//Really, there are more entries beyond this, but I consider it pretty much not worth the effort.
#endif
};


void LinearFeedbackShiftReg::setMaskPeriod(unsigned int newMask)
{
	//Don't fall off the table
	ASSERT((newMask-3) < sizeof(maximumLinearTable)/sizeof(size_t));

	maskVal=maximumLinearTable[newMask-3];

	//Set the mask to be all ones
	totalMask=0;
	for(size_t ui=0;ui<newMask;ui++)
		totalMask|= (size_t)(1)<<ui;


}

bool LinearFeedbackShiftReg::verifyTable(size_t maxLen)
{
	size_t tableLen =  sizeof(maximumLinearTable)/sizeof(size_t);
	
	//check each one is actually the full 2^n-1 period
	if(maxLen)
	{
		ASSERT(maxLen < tableLen);
		tableLen=maxLen;
	}

	//For the 32 bit table, this works pretty quickly.
	//for the 64  bit table, this takes a month or so
	for(size_t n=3;n<tableLen+3;n++)
	{
		size_t period;
		setState(1);
		setMaskPeriod(n);
		period=0;
		do
		{
			clock();
			period++;
		}
		while(lfsr!=1);


		//we should have counted every bit position in the polynomial (except 0)
		//otherwise, this is not the maximal linear sequence
		if(period != ((size_t)(1)<<(n-(size_t)1)) -(size_t)(1))
			return false;
	}
	return true;
}

size_t LinearFeedbackShiftReg::clock()
{
	typedef size_t ull;

	lfsr = (lfsr >> 1) ^  ( (-(lfsr & (ull)(1u))) & maskVal ); 
	lfsr&=totalMask;
	if( lfsr == 0u)
		lfsr=1u;

	return lfsr;
}

double det3by3(const double *ptArray)
{
	return (ptArray[0]*(ptArray[4]*ptArray[8]
			      	- ptArray[7]*ptArray[5]) 
		- ptArray[1]*(ptArray[3]*ptArray[8]
		       		- ptArray[6]*ptArray[5]) 
		+ ptArray[2]*(ptArray[3]*ptArray[7] 
				- ptArray[4]*ptArray[6]));
}

//Determines the volume of a quadrilateral pyramid
//input points "planarpts" must be adjacent (connected) by 
//0 <-> 1 <-> 2 <-> 0, all points connected to apex
double pyramidVol(const Point3D *planarPts, const Point3D &apex)
{

	//Array for 3D simplex volumed determination
	//		| (a_x - b_x)   (b_x - c_x)   (c_x - d_x) |
	//v_simplex =1/6| (a_y - b_y)   (b_y - c_y)   (c_y - d_y) |
	//		| (a_z - b_z)   (b_z - c_z)   (c_z - d_z) |
	double simplexA[9];

	//simplex A (a,b,c,apex) is as follows
	//a=planarPts[0] b=planarPts[1] c=planarPts[2]
	
	simplexA[0] = (double)( (planarPts[0])[0] - (planarPts[1])[0] );
	simplexA[1] = (double)( (planarPts[1])[0] - (planarPts[2])[0] );
	simplexA[2] = (double)( (planarPts[2])[0] - (apex)[0] );
	
	simplexA[3] = (double)( (planarPts[0])[1] - (planarPts[1])[1] );
	simplexA[4] = (double)( (planarPts[1])[1] - (planarPts[2])[1] );
	simplexA[5] = (double)( (planarPts[2])[1] - (apex)[1] );
	
	simplexA[6] = (double)( (planarPts[0])[2] - (planarPts[1])[2] );
	simplexA[7] = (double)( (planarPts[1])[2] - (planarPts[2])[2] );
	simplexA[8] = (double)( (planarPts[2])[2] - (apex)[2] );
	
	return 1.0/6.0 * (fabs(det3by3(simplexA)));	
}

void computeRotationMatrix(const Point3D &ur1, const Point3D &ur2,
	const Point3D &r1, const Point3D &r2, gsl_matrix *m)
{
	//TRIAD algorithm, for determining rotation matrix from two
	// linearly independant vector pairs.
	// This is a specific case of "Wahba's Problem", where no noise is present

	//unrotated and rotated vectors should be linearly independant
	ASSERT(ur1.crossProd(ur2).sqrMag() < 0.001);
	ASSERT(r1.crossProd(r2).sqrMag() < 0.001);

	//vectors should be pre-normalised
//	ASSERT(TOL_EQ(ur1.sqrMag(),1) && TOL_EQ(ur2.sqrMag(),1));
//	ASSERT(TOL_EQ(r1.sqrMag(),1) && TOL_EQ(r2.sqrMag(),1));

//	ASSERT(gsl_matrix_num_rows(m) == 3);
//	ASSERT(gsl_matrix_num_cols(m) == 3);

	Point3D rCross,urCross;
	rCross = r1.crossProd(r2);
	urCross = ur1.crossProd(r2);

	gsl_matrix *a,*b;
	a = gsl_matrix_alloc(3,3);
	b = gsl_matrix_alloc(3,3);

	for(unsigned int ui=0;ui<3;ui++)
	{
		//build A matrix, row by row
		gsl_matrix_set(a,ui,0,ur1[ui]);
		gsl_matrix_set(a,ui,1,ur2[ui]);
		gsl_matrix_set(a,ui,2,urCross[ui]);

		//build B^T matrix, row by row
		gsl_matrix_set(b,0,ui,r1[ui]);
		gsl_matrix_set(b,1,ui,r2[ui]);
		gsl_matrix_set(b,2,ui,rCross[ui]);

	}

	//Compute m = a*b;
	gsl_blas_dgemm (CblasNoTrans, CblasNoTrans,
			    1.0, a, b,
			    0.0, m);

	gsl_matrix_free(a);
	gsl_matrix_free(b);

}

void rotateByMatrix(const vector<Point3D> &vpts, const gsl_matrix *m, vector<Point3D> &r)
{
	r.resize(vpts.size());

	gsl_vector *v = gsl_vector_alloc(3);
	gsl_vector *rv = gsl_vector_alloc(3);


	for(unsigned int ui=0;ui<vpts.size();ui++)
	{
		gsl_vector_set(v,0,vpts[ui][0]);
		gsl_vector_set(v,1,vpts[ui][1]);
		gsl_vector_set(v,2,vpts[ui][2]);

		//compute v = m * pY;
		gsl_blas_dgemv(CblasNoTrans,1.0, m, v,0,rv);

		r[ui][0] = gsl_vector_get(rv,0);
		r[ui][1] = gsl_vector_get(rv,1);
		r[ui][2] = gsl_vector_get(rv,2);
	}


	gsl_vector_free(v);
	gsl_vector_free(rv);
}

	

