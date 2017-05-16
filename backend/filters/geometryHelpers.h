/*
 *	geometryHelpers.h - 3D Geometric operations helper classes
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
#ifndef GEOMETRYHELPER_H
#define GEOMETRYHELPER_H

#include "backend/APT/ionhit.h"

#include <vector>

enum
{
	CROP_SPHERE_INSIDE,
	CROP_SPHERE_OUTSIDE,
	CROP_PLANE_FRONT,
	CROP_PLANE_BACK,
	CROP_CYLINDER_INSIDE_AXIAL,
	CROP_CYLINDER_INSIDE_RADIAL,
	CROP_CYLINDER_OUTSIDE,
	CROP_AAB_OUTSIDE,
	CROP_AAB_INSIDE,
	CROP_ENUM_END
};


enum
{
	ERR_CROP_CALLBACK_FAIL=1,
	ERR_CROP_INSUFFICIENT_MEM,
};

class CropHelper;

//Type declaration for pointer to constant member function.
// typename is in the middle of the declaration (i.e. "CropFuncPtr")
typedef bool (CropHelper::* CropFuncPtr)(const Point3D &p) const;
typedef unsigned int (CropHelper::* MapFuncPtr)(const Point3D &p) const;




//Assistance class for helping cropping
// see the end of this file (.h) for 
// mathematical description of each primitve
class CropHelper
{
	private:
		//Filter parameters
		// ----
		size_t algorithm;

		//Geometric parameters, whose meaning is determined
		// by the choice of geometric algorithm
		Point3D pA,pB; //3D vector params
		float fA,fB; //scalar params
		Quaternion qA ; //rotation quaternion
		bool nearAxis; //true if rotation is near-zaxis (ie rotation doesn't need ot be done)
		bool invertedClip;

		size_t mapMax; //Mapping maxima.


		// -- 

		//Helper values
		//---
		//Algorithm to use
		CropFuncPtr cropFunc;
		MapFuncPtr mapFunc;

		size_t totalDataCount;
		//--
	


		//Various testing point containment against primitive
		// functions
		//----
		//PA - Cylinder origin
		//PB - Cylinder 1/2 axis vector
		//fA - Cylinder radius
		bool filterCylinderInside(const Point3D &p) const;

		//returns true if point p is wholly inside sphere
		//PA - sphere origin
		//fA - square of sphere radius
		bool filterSphereInside(const Point3D &p) const;

		//Returns true if point p is in front of the plane 
		//pA - plane origin
		//pB - plane normal
		bool filterPlaneFront(const Point3D &p) const;

		//returns true if the point is inside the box
		//pA - Lower point of box
		//pB - upper point of box
		bool filterBoxInside(const Point3D &testPt) const;
		//----

		//Mapping functions. returns 0 -> mapMax, along axial direction
		unsigned int mapCylinderInsideAxial(const Point3D &p) const;
		unsigned int mapCylinderInsideRadial(const Point3D &p) const;


		//
		unsigned int mapSphereInside(const Point3D &p) const;


		//Initialise the data variables for the specified cylinder
		void setupCylinder(Point3D origin,
				float radius,Point3D direction);

		//intialise the function pointer for the chosen algorithm
		void setAlgorithm();


		//Run the input filtering in linear (single CPU) mode
		// allocHint, if >0 , is the recommended fraction of input to reserve
		// ahead of copying
		unsigned int runFilterLinear(const std::vector<IonHit> &dataIn,
				std::vector<IonHit> &dataOut,float allocHint, 
				float minProg, float maxProg, unsigned int &prog);
	
		//Run the input filtering in parallel (multi CPU) mode
		unsigned int runFilterParallel(const std::vector<IonHit> &dataIn,
				std::vector<IonHit> &dataOut,float allocHint,
				float minProg, float maxProg, unsigned int &prog);
	public:
	
		//Input vectors and scalars represent the fundamental
		// basis for the desired geometry
		CropHelper(size_t totalData,size_t filterMode,
			std::vector<Point3D> &vectors, std::vector<float> &scalars);
		
		//Filter the input ion data in order to generate output points
		// output data may contain previous data - this will be appended to,
		// not overwritten
		unsigned int runFilter(const std::vector<IonHit> &dataIn,
				std::vector<IonHit> &dataOut,
				float progStart, float progEnd,unsigned int &prog) ;


		void setMapMaxima(size_t maxima){ASSERT(maxima); mapMax=maxima;};
		//Map an ion from its 3D coordinate to a 1D coordinate along the 
		// selected geometric primitive. Returns true if the ion is mappable (i.e. inside selected primitive mode)
		unsigned int mapIon1D(const IonHit &ionIn ) const;

		//Choose the cropping mode for the filter
		void setFilterMode(size_t filterMode);

};


//Primitive Descriptions: What you need to pass in
//---

//Sphere Primitive:
// Vectors
//	0 - Origin
// Scalars
//	0 - Radius

//Cylinder Primitive
// Vectors
//	0 - Origin
//	1 - Axis : This is the distance from one end of the cylinder
//			to the other, ie from the centre of one circle 
//			end-cap to the other
// Scalars
//	0 - radius




//---
#endif
