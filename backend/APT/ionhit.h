/*
 * ionhit.h - Ion event data class
 * Copyright (C) 2015  D Haley
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IONHIT_H
#define IONHIT_H

#include "common/basics.h"
class Point3D;


//TODO: Move to member of ionHit itself
//!Allowable export ion formats
enum
{
	IONFORMAT_POS=1,
	IONFORMAT_TEXT,
	IONFORMAT_VTK,
	IONFORMAT_ENUM_END
};

//!This is a data holding class for POS file ions, from
/* Pos ions are typically obtained via reconstructed apt detector hits
 * and are of form (x,y,z mass/charge)
 */
class IonHit
{
	private:
		float massToCharge; // mass to charge ratio in Atomic Mass Units per (charge on electron)
		Point3D pos; //position (xyz) in nm
	public:
		IonHit();
		IonHit(float *);
		//copy constructor
		IonHit(const IonHit &);
		IonHit(const Point3D &p, float massToCharge);

		//Size of data when stored in a file record
		static const  unsigned int DATA_SIZE = 16;

		void setHit(float *arr) { pos.setValueArr(arr); massToCharge=arr[3];};
		void setMassToCharge(float newMassToCharge);
		void setPos(const Point3D &pos);
		void setPos(float fX, float fY, float fZ)
			{ pos.setValue(fX,fY,fZ);};
		Point3D getPos() const;
		inline const Point3D &getPosRef() const {return pos;};
		//returns true if any of the 4 data pts are NaN
		bool hasNaN();
		//returns true if any of the 4 data pts are +-inf
		bool hasInf();

#ifdef __LITTLE_ENDIAN__		
		void switchEndian();
#endif
		//this does the endian switch for you
		//but you must supply a valid array.
		void makePosData(float *floatArr) const;
		float getMassToCharge() const;


		//Helper functions
		//--
		//get the points from a vector of of ionhits
		static void getPoints(const std::vector<IonHit> &ions, std::vector<Point3D> &pts);

		//Get the bounding cube from a vector of ionhits
		static void getBoundCube(const std::vector<IonHit> &p, BoundCube &b);

		//Get the centroid from a vector of ion hits
		static void getCentroid(const std::vector<IonHit> &points, Point3D &centroid);
		
		//Add these points to a formatted file
		static unsigned int appendFile(const std::vector<IonHit> &points, const char *name, const unsigned int format=IONFORMAT_POS);

		//Save a pos file, overwriting any previous data at this location
		static unsigned int makePos(const std::vector<IonHit> &points, const char *name);
		//---

		const IonHit &operator=(const IonHit &obj);
		float operator[](unsigned int ui) const;	
		IonHit operator+(const Point3D &obj);
};

class IonAxisCompare
{
	private:
		unsigned int axis;
	public:
		IonAxisCompare();
		IonAxisCompare(unsigned int newAxis) ;
		void setAxis(unsigned int newAxis);
		inline bool operator()(const IonHit &p1,const IonHit &p2) const
			{return p1.getPos()[axis]<p2.getPos()[axis];};
};

#ifdef DEBUG
//unit testing
bool testIonHit();
#endif

#endif
