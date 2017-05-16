/*
 * ionhit.cpp - Ion event data class
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


#include "ionhit.h"
#ifdef _OPENMP
#include <omp.h>
#endif
using std::vector;

IonAxisCompare::IonAxisCompare()
{
}

IonAxisCompare::IonAxisCompare(unsigned int newAxis)
{
	ASSERT(newAxis<3);
	axis=newAxis;
}

void IonAxisCompare::setAxis(unsigned int newAxis)
{
	axis=newAxis;
}

IonHit::IonHit() 
{
	//At this point I deliberately don't initialise the point class
	//as in DEBUG mode, the point class will catch failure to init
}

IonHit::IonHit(float *buffer)
{
	pos.setValueArr(buffer);
	massToCharge=buffer[3];
}


IonHit::IonHit(const IonHit &obj2) : massToCharge(obj2.massToCharge), pos(obj2.pos)
{
}

IonHit::IonHit(const Point3D &p, float newMass) : massToCharge(newMass), pos(p)
{
}

void IonHit::setMassToCharge(float newMass)
{
	massToCharge=newMass;
}

float IonHit::getMassToCharge() const
{
	return massToCharge;
}	


void IonHit::setPos(const Point3D &p)
{
	pos=p;
}

#ifdef __LITTLE_ENDIAN__
void IonHit::switchEndian()
{
	
	pos.switchEndian();
	floatSwapBytes(&(massToCharge));
}
#endif

const IonHit &IonHit::operator=(const IonHit &obj)
{
	massToCharge=obj.massToCharge;
	pos = obj.pos;

	return *this;
}


float IonHit::operator[](unsigned int idx) const
{
	ASSERT(idx <4);
		
	if(idx < 3)
		return pos[idx];
	else
		return massToCharge; 
}

//!Create an pos file from a vector of IonHits
unsigned int IonHit::makePos(const vector<IonHit> &ionVec, const char *filename)
{
	std::ofstream CFile(filename,std::ios::binary);
	float floatBuffer[4];

	if (!CFile)
		return 1;

	for (unsigned int ui=0; ui<ionVec.size(); ui++)
	{
		ionVec[ui].makePosData(floatBuffer);
		CFile.write((char *)floatBuffer,4*sizeof(float));
	}
	return 0;
}

unsigned int IonHit::appendFile(const vector<IonHit> &points, const char *name, unsigned int format)
{
	switch(format)
	{
		case IONFORMAT_POS:
		{
			//Write a "pos" formatted file
			std::ofstream posFile(name,std::ios::binary|std::ios::app);	


			if(!posFile)
				return 1;

			float data[4];	
			
			for(unsigned int ui=0; ui< points.size(); ui++)
			{
				points[ui].makePosData(data);
				posFile.write((char *)data, 4*sizeof(float));
			}


			if(posFile.good())
				return 0;
			else
				return 1;
		}
		case IONFORMAT_TEXT:
		{
			std::ofstream textFile(name,std::ios::app);
			if(!textFile)
				return 1;

			for(unsigned int ui=0;ui<points.size();ui++)
				textFile << points[ui][0] << " " << points[ui][1] << " " << points[ui][2]  << " " << points[ui][3] << std::endl;

			if(textFile.good())
				return 0;
			else
				return 1;
		}
		default:
			ASSERT(false);
	}
}

void IonHit::getPoints(const vector<IonHit> &ions, vector<Point3D> &p)
{
	p.resize(ions.size());
#pragma omp parallel for
	for(size_t ui=0;ui<ions.size();ui++)
		p[ui] = ions[ui].getPosRef();
}

void IonHit::makePosData(float *floatArr) const
{
	ASSERT(floatArr);
	//copy positional information
	pos.copyValueArr(floatArr);

	//copy mass to charge data
	*(floatArr+3) = massToCharge;
		
	#ifdef __LITTLE_ENDIAN__
		floatSwapBytes(floatArr);
		floatSwapBytes((floatArr+1));
		floatSwapBytes((floatArr+2));
		floatSwapBytes((floatArr+3));
	#endif
}

Point3D IonHit::getPos() const
{
	return pos;
}	

bool IonHit::hasNaN()
{
	return (std::isnan(massToCharge) || std::isnan(pos[0]) || 
				std::isnan(pos[1]) || std::isnan(pos[2]));
}

bool IonHit::hasInf()
{
	return (std::isinf(massToCharge) || std::isinf(pos[0]) || 
				std::isinf(pos[1]) || std::isinf(pos[2]));
}

void IonHit::getCentroid(const std::vector<IonHit> &points,Point3D &centroid)
{
	centroid=Point3D(0,0,0);
	size_t nPoints=points.size();
#ifdef _OPENMP
	
	//Parallel version
	//--
	vector<Point3D> centroids(omp_get_max_threads(),Point3D(0,0,0));
#pragma omp parallel for 
	for(size_t ui=0;ui<nPoints;ui++)
		centroids[omp_get_thread_num()]+=points[ui].getPos();

	for(size_t ui=0;ui<centroids.size();ui++)
		centroid+=centroids[ui];
	//--

#else
	for(unsigned int ui=0;ui<nPoints;ui++)
		centroid+=points[ui].getPos();
#endif
	
	centroid*=1.0f/(float)nPoints;
}

void IonHit::getBoundCube(const std::vector<IonHit> &points,BoundCube &b)
{
	ASSERT(points.size());

#ifndef _OPENMP
	float bounds[3][2];
	for(unsigned int ui=0;ui<3;ui++)
	{
		bounds[ui][0]=std::numeric_limits<float>::max();
		bounds[ui][1]=-std::numeric_limits<float>::max();
	}
	
	for(unsigned int ui=0; ui<points.size(); ui++)
	{
		Point3D p;
		p=points[ui].getPos();
		for(unsigned int uj=0; uj<3; uj++)
		{
			bounds[uj][0] = std::min(p.getValue(uj),bounds[uj][0]);
			bounds[uj][1] = std::max(p.getValue(uj),bounds[uj][1]);
		}
	}

	b.setBounds(bounds[0][0],bounds[1][0],
			bounds[2][0],bounds[0][1],
			bounds[1][1],bounds[2][1]);
#else
	// parallel version
	unsigned int nT=omp_get_max_threads();
	vector<BoundCube> cubes(nT);

	for(unsigned int ui=0;ui<cubes.size();ui++)
		cubes[ui].setInverseLimits(true);

	#pragma omp parallel for 
	for(unsigned int ui=0;ui<points.size();ui++)
	{
		Point3D p;
		p=points[ui].getPos();

		size_t tid=omp_get_thread_num();

		//Move upper and lower bounds
		for(unsigned int uj=0;uj<3;uj++)
		{
			BoundCube &threadCube=cubes[tid];

			threadCube.setBound(uj,0,std::min(threadCube.getBound(uj,0),p[uj]));
			threadCube.setBound(uj,1,std::max(threadCube.getBound(uj,1),p[uj]));
		}
	}

	b.setInverseLimits(true);	
	for(unsigned int ui=0;ui<nT;ui++)
		b.expand(cubes[ui]);
#endif

}



#ifdef DEBUG

bool testIonHit()
{
	//tgest the boundcube function
	vector<IonHit> h;
	IonHit hit;
	hit.setMassToCharge(1);
	
	for(size_t ui=0;ui<8;ui++)
	{
		hit.setPos(Point3D(ui&4 >> 2,ui&2 >> 1,ui&1));
		h.push_back(hit);
	}

	BoundCube bc;
	IonHit::getBoundCube(h,bc);
	TEST(bc.isValid(),"check boundcube");

	BoundCube biggerBox;
	for(size_t ui=0;ui<3;ui++)
	{
		biggerBox.setBound(ui,0,-1.5f);
		biggerBox.setBound(ui,1,1.5f);
	}

	TEST(biggerBox.contains(bc),"Check boundcube size");

	return true;
}

#endif
