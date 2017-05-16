/*
 *	mathfuncs.h - General mathematic functions header
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
#ifndef MATHFUNCS_H
#define MATHFUNCS_H

#include <cmath>
#include <limits>
#include <iostream>
#include <vector>

#include <gsl/gsl_matrix.h>


#include "endianTest.h"


//!A 3D point data class storage
/*! A  3D point data class
 * contains operator overloads and some basic
 * mathematical functions
 */
class Point3D
{
        private:
		//!Value data
                float value[3];
        public:
		//!Constructor
                inline Point3D() {};
		//!Constructor with initialising values
                inline Point3D(float x,float y,float z) 
					{ value[0] = x, value[1] = y, value[2] = z;}
                inline Point3D(float *v)
					{ value[0] = v[0], value[1] = v[1], value[2] = v[2];}
                inline Point3D(double *v)
					{ value[0] = v[0], value[1] = v[1], value[2] = v[2];}
                //!Set by value (ith dim 0, 1 2)
                inline void setValue(unsigned int ui, float val){value[ui]=val;};
				//!Set all values
                inline void setValue(float fX,float fY, float fZ)
                        {value[0]=fX; value[1]=fY; value[2]=fZ;}

                //!Set by pointer
                inline void setValueArr(const float *val)
                        {
                                value[0]=*val;
                                value[1]=*(val+1);
                                value[2]=*(val+2);
                        };

                //!Get value of ith dim (0, 1, 2)
                inline float getValue(unsigned int ui) const {return value[ui];};
		//Retrieve the internal pointer. Only use if you know why.
                inline const float *getValueArr() const { return value;};

                //!get into an array (note array must hold sizeof(float)*3 bytes of valid mem
                void copyValueArr(float *value) const;

                //!Add a point to this, without generating a return value
                void add(const Point3D &obj);

		//Convert a point string from its "C" language representation to a point value
		bool parse(const std::string &str);
		
		//!Equality operator
                bool operator==(const Point3D &pt) const;
		//!assignment operator
                const Point3D &operator=(const Point3D &pt);
		//!+= operator
                const Point3D &operator+=(const Point3D &pt);
		
		//!+= operator
                const Point3D &operator-=(const Point3D &pt);

		const Point3D operator+(float f) const;
		//!multiplication operator
                const Point3D &operator*=(const float scale);
		//!Addition operator
                const Point3D operator+(const Point3D &pt) const;
		//!elemental multiplication
                const Point3D operator*(float scale) const;
		//!multiplication
		const Point3D operator*(const Point3D &pt) const;
		//!Division. 
                const Point3D operator/(float scale) const;

                const Point3D operator/(const Point3D &p) const;
		//!Subtraction
                const Point3D operator-(const Point3D &pt) const;
		//!returns a negative of the existing value
                const Point3D operator-() const;
		//!Output streaming operator. Users (x,y,z) as format for output
                friend std::ostream &operator<<(std::ostream &stream, const Point3D &);
                //!make point unit magnitude, maintaining direction
		Point3D normalise();
                //!returns the square of distance another pt
                float sqrDist(const Point3D &pt) const;

                //!overload for array indexing returns |pt|^2
                float sqrMag() const;
		
		//!Apply float->float transformation
		void sqrt() { for(unsigned int ui=0;ui<3;ui++) value[ui]=sqrtf(value[ui]); }
                
		//ISO31-11 spherical co-ordinates. theta is clockwise rotation around z- axis.
		// phi is elevation from x-y plane
		void sphericalAngles(float &theta, float &phi) const;	

		//!Calculate the dot product of this and another pint
                float dotProd(const Point3D &pt) const;
                //!Calculate the cross product of this and another point
                Point3D crossProd(const Point3D &pt) const;

		//!Calculate the angle between two position vectors in radiians
		float angle(const Point3D &pt) const;

		//Extend the current vector by the specified distance
		void extend(float distance);


		//!Retrieve by value
                float operator[](unsigned int ui) const; 
		//!Retrieve element by referene
                float &operator[](unsigned int ui) ;

                //!Is a given point stored inside a box bounded by orign and this pt?
                /*!returns true if this point is located inside (0,0,0) -> Farpoint
                * assuming box shape (non zero edges return false)
                * farPoint must be positive in all dim
                */
                bool insideBox(const Point3D &farPoint) const;


				//!Tests if this point lies inside the rectangular prism 
				/*!Returns true if this point lies inside the box bounded
				 * by lowPoint and highPoint
				 */
                bool insideBox(const Point3D &lowPoint, const Point3D &highPoint) const;

		//!Makes each value negative of old value
		void negate();

		//Perform a 3x3 matrix transformation. 
		void transform3x3(const float *matrix);

		//Perform a cross-product based orthogonalisation
		//with the specified vector
		bool orthogonalise(const Point3D &p);

		static Point3D centroid(const Point3D *p, unsigned int n);
		
		static Point3D centroid(const std::vector<Point3D> &p); 
#ifdef __LITTLE_ENDIAN__
                //!Flip the endian state for data stored in this point
                void switchEndian();
#endif
};

//IMPORTANT!!!
//===============
//Do NOT use multiple instances of this in your code
//with the same initialisation technique (e.g. initialising from system clock)
//this would be BAD, correlations might well be introduced into your results
//that are simply a result of using correlated random sequences!!! (think about it)
//use ONE random number generator in the project, initialise it and then "register"
//it with any objects that need a random generator. 
//==============
class RandNumGen
{
	private:
		int ma[56];
		int inext,inextp;
		float gaussSpare;
		bool haveGaussian;

	public:
		RandNumGen();
		void initialise(int seedVal);
		int initTimer();

		int genInt();
		float genUniformDev();

		//This generates a number chosen from
		//a gaussian distribution range is (-inf, inf)
		float genGaussDev();
};

//needed for sincos
#ifdef __LINUX__ 
#ifdef __GNUC__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif
#endif

typedef struct 
{
	float a;
	float b;
	float c;
	float d;
} Quaternion;

typedef struct
{
	float fx;
	float fy;
	float fz;
} Point3f;

//Uses quaternion mathematics to perform a rotation around your favourite axis
//IMPORTANT: rotVec must be normalised before passing to this function 
//failure to do so will have weird results
//Note result is stored in  point passed as argument
//angle is in radians.

//Inefficient Point3D version
void quat_rot(Point3D &p, const Point3D &r, float angle);

void quat_rot(Point3f *point, const Point3f *rotVec, float angle);

void quat_rot_array(Point3f *point, unsigned int n, const Point3f *rotVec, float angle);

void quat_rot_array(Point3D *point, unsigned int n, const Point3f *rotVec, float angle);


//Retrieve the quaternion for repeated rotations. Pass to the quat_rot_apply_quats.
//angle is in radians
void quat_get_rot_quat(const Point3f *rotVec, float angle,  Quaternion *rotQuat);

//Use previously generated quats from quat_get_rot_quats to rotate a point
void quat_rot_apply_quat(Point3f *point, const Quaternion *rotQuat);

//This class implements a Linear Feedback Shift Register (in software) 
//This is a mathematical construct based upon polynomials over closed natural numbers (N mod p).
//This will generate a weakly random digit string, but with guaranteed no duplicates, using O(1)
//memory and O(n) calls. The no duplicate guarantee is weak-ish, with no repetition in the
//shift register for 2^n-1 iterations. n can be set by setMaskPeriod.
class LinearFeedbackShiftReg
{
	size_t lfsr;
	size_t maskVal;
	size_t totalMask;
	public:
		//Get a value from the shift register, and advance
		size_t clock();
		//Set the internal lfsr state. Note 0 is the lock-up state.
		void setState(size_t newState) { lfsr=newState;};
		//set the mask to use such that the period is 2^n-1. 3 is minimum 60 is maximum
		void setMaskPeriod(unsigned int newMask);

		//!Check the validity of the table
		bool verifyTable(size_t maxLen=0);
};


//Determines the volume of a quadrilateral pyramid
//input points "planarpts" must be adjacent (connected) by 
//0 <-> 1 <-> 2 <-> 0, all points connected to apex
double pyramidVol(const Point3D *planarPts, const Point3D &apex);

//!Inline func for calculating a(dot)b
inline float dotProduct(float a1, float a2, float a3, 
			float b1, float b2, float b3)
{
	return a1*b1 + a2*b2 + a3* b3;
}

inline unsigned int ilog2(unsigned int value)
{
	unsigned int l = 0;
	while( (value >> l) > 1 ) 
		++l;
	return l;
}


//!Use the TRIAD algorithm to compute the matrix that transforms orthogonal unit vectors
// ur1,ur2 to rotated orthogonal unit vectors r1,r2. MUST be orthogonal and unit. 
// matrix m must be pre-allocated 3x3 matrix
void computeRotationMatrix(const Point3D &ur1, const Point3D &ur2,
	const Point3D &r1, const Point3D &r2, gsl_matrix *m);

//Rotate a set of points by the given 3x3 matrix
void rotateByMatrix(const std::vector<Point3D> &vpts, 
		const gsl_matrix *m, std::vector<Point3D> &r);

#endif
