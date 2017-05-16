/*
 *	geometryHelpers.cpp - Various spatial geometry operators for point clouds
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


#include "geometryHelpers.h"

#include "backend/APT/ionhit.h"

#include "backend/filter.h"

#ifdef _OPENMP
#include <omp.h>
#endif


using std::vector;

const size_t DEFAULT_NUM_CALLBACK=5000;

//These numbers have not been optimised. On a 2CPU system, I cannot find
// a case where the // option is outrun by the linear one, over 3 averages
//---
//Minimum number of input points before we will do reserve testing
const size_t MIN_SAMPLE_TEST = 1000;
//Minimim number of input points before we will engage a parallel algorithm
const size_t MIN_PARALLELISE = 20000;
//---

CropHelper::CropHelper(	size_t totalData,size_t filterMode,
			vector<Point3D> &vectors, vector<float> &scalars)
{
	algorithm=filterMode;
	mapMax=0;
	invertedClip=false;

	switch(algorithm)
	{
		case CROP_SPHERE_OUTSIDE:
			invertedClip=true;
		case CROP_SPHERE_INSIDE:
		{
			ASSERT(vectors.size() == 1);
			ASSERT(scalars.size() == 1);
			ASSERT(scalars[0] >= 0.0f);
		

			fA=scalars[0]*scalars[0];
			fB=scalars[0];
			pA=vectors[0];
			break;
		}
		case CROP_PLANE_BACK:
			invertedClip=true;
		case CROP_PLANE_FRONT:
		{
			ASSERT(vectors.size() == 2);
			ASSERT(scalars.size() == 0);

			pA=vectors[0];
			pB=vectors[1];
			break;
		}
		case CROP_CYLINDER_OUTSIDE:
			invertedClip=true;
		case CROP_CYLINDER_INSIDE_AXIAL:
		case CROP_CYLINDER_INSIDE_RADIAL:
		{
			ASSERT(vectors.size() == 2);
			ASSERT(scalars.size() == 1);

			setupCylinder(vectors[0],scalars[0],vectors[1]);
			break;
		}
		case CROP_AAB_OUTSIDE:
			invertedClip=true;
		case CROP_AAB_INSIDE:
		{
			ASSERT(vectors.size() ==2);
			ASSERT(scalars.size() == 0);

			pA=vectors[0]-vectors[1];
			pB=vectors[0]+vectors[1];
			break;
		}
		
		default:
			ASSERT(false);
	}

	setAlgorithm();


	totalDataCount=totalData;
}


void CropHelper::setAlgorithm()
{
	mapFunc=0;

	//Assign the desired member function
	switch(algorithm)
	{
		case CROP_SPHERE_OUTSIDE:
		case CROP_SPHERE_INSIDE:
			cropFunc=&CropHelper::filterSphereInside;
			mapFunc=&CropHelper::mapSphereInside;
			break;
		case CROP_PLANE_FRONT:
		case CROP_PLANE_BACK:
			cropFunc=&CropHelper::filterPlaneFront;
			break;
		case CROP_CYLINDER_OUTSIDE:
		case CROP_CYLINDER_INSIDE_AXIAL:
			cropFunc=&CropHelper::filterCylinderInside;
			mapFunc=&CropHelper::mapCylinderInsideAxial;
			break;
		case CROP_CYLINDER_INSIDE_RADIAL:
			cropFunc=&CropHelper::filterCylinderInside;
			mapFunc=&CropHelper::mapCylinderInsideRadial;
			break;
		case CROP_AAB_INSIDE:
		case CROP_AAB_OUTSIDE:
			cropFunc=&CropHelper::filterBoxInside;
			break;
		default:
			ASSERT(false);
	}
}

unsigned int CropHelper::runFilter(const vector<IonHit> &dataIn,
				vector<IonHit> &dataOut, float progressStart,
				float progressEnd, unsigned int &progress ) 
{
	//FIXME!: Shouldn't be using this here - should be obeying
	// system-wide rng preferences
	RandNumGen rng;
	rng.initTimer();

	float allocHint=0;
	//If we have enough input data, try sampling
	// the input randomly to test if we can 
	// pre-allocate enough space for output data
	if(dataIn.size() > MIN_SAMPLE_TEST)
	{

		const size_t SAMPLE_SIZE=30;

		vector<size_t> samples;
		unsigned int dummy;
		randomDigitSelection(samples,dataIn.size(),rng, 
				SAMPLE_SIZE,dummy);

		size_t tally=0;
		for(size_t ui=0;ui<SAMPLE_SIZE;ui++)
		{
			if(((this->*cropFunc)(dataIn[samples[ui]].getPosRef())) ^ invertedClip)
				tally++;
		}

		allocHint = (float)tally/(float)SAMPLE_SIZE;
	}


#ifndef _OPENMP
	return runFilterLinear(dataIn,dataOut,allocHint,progressStart,progressEnd,progress);
#else
	if(dataIn.size() < MIN_PARALLELISE  || rng.genUniformDev() < 0.5f)
		return runFilterLinear(dataIn,dataOut,allocHint,progressStart,progressEnd,progress);
	else
	{
		return runFilterParallel(dataIn,dataOut,allocHint,progressStart,progressEnd,progress);
	}
#endif
}

unsigned int CropHelper::runFilterLinear(const vector<IonHit> &dataIn,
				vector<IonHit> &dataOut,float allocHint, float minProg,float maxProg, unsigned int &prog ) 
{
	if(allocHint > 0.0f)
		dataOut.reserve((unsigned int) ( (float)dataIn.size()*allocHint));

	//Run the data filtering using a single threaded algorithm
	// copying to output
	if(!invertedClip)
	{
		for(size_t ui=0; ui<dataIn.size(); ui++)
		{
			if(((this->*cropFunc)(dataIn[ui].getPosRef())))
				dataOut.push_back(dataIn[ui]);

			if(ui & 100)
				prog = (float)ui/(float)dataIn.size() * (maxProg-minProg)+minProg;

			if(*Filter::wantAbort)
				return ERR_CROP_CALLBACK_FAIL;
		}

	}
	else
	{
		for(size_t ui=0; ui<dataIn.size(); ui++)
		{
			if(!((this->*cropFunc)(dataIn[ui].getPosRef())))
				dataOut.push_back(dataIn[ui]);

			if(ui & 100)
				prog = (float)ui/(float)dataIn.size() * (maxProg-minProg)+minProg;

			if(*Filter::wantAbort)
				return ERR_CROP_CALLBACK_FAIL;
		}
	}

	prog=maxProg;
	return 0;
}

unsigned int CropHelper::runFilterParallel(const vector<IonHit> &dataIn,
				vector<IonHit> &dataOut, float allocHint, float minProg,float maxProg, unsigned int &prog )
{
#ifdef _OPENMP

	size_t n=0;
	const size_t PROGRESS_REDUCE=5000;
	size_t curProg=PROGRESS_REDUCE;

	//Create a vector of indices for which 
	// points successfully passed the test
	size_t nThreads=omp_get_max_threads();
	vector<size_t> inside[nThreads];

	if(allocHint > 0.0f)
	{
		try
		{
		for(size_t ui=0;ui<nThreads;ui++)
			inside[ui].reserve((unsigned int)( (float)dataIn.size()*allocHint)/nThreads);
		}
		catch(std::bad_alloc)
		{
			return ERR_CROP_INSUFFICIENT_MEM;
		}
	}
	bool spin=false;
#pragma omp parallel for 
	for(size_t ui=0; ui<dataIn.size(); ui++)
	{
		if(spin)
			continue;

		//Use XOR operand on cropFunc conditional
		if(((this->*cropFunc)(dataIn[ui].getPosRef())) ^ invertedClip)
			inside[omp_get_thread_num()].push_back(ui);
		
		//update progress every PROGRESS_REDUCE ions
		if(!curProg--)
		{
#pragma omp critical
			{
			n+=PROGRESS_REDUCE;
			prog = (float)n/(float)dataIn.size() * (maxProg-minProg)+minProg;
			
			if(*Filter::wantAbort)
				spin=true;
			}
			curProg=PROGRESS_REDUCE;
		}
	}

	if(spin)
		return ERR_CROP_CALLBACK_FAIL;

	//map the successful ions into a single output block
	// --
	vector<size_t> offsets;
	offsets.resize(nThreads);
	size_t totalOut=0;
	for(size_t ui=0;ui<nThreads; ui++)
	{
		offsets[ui]=totalOut;
		totalOut+=inside[ui].size();
	}

	//Do the merge each batch of data back into single output
	dataOut.resize(totalOut);
#pragma omp parallel for 
	for(size_t ui=0;ui<nThreads;ui++)
	{
		size_t offset;
		offset=offsets[ui];
		for(size_t uj=0;uj<inside[ui].size(); uj++)
			dataOut[offset + uj] = dataIn[inside[ui][uj]];
	}
	// -- 
#else
	ASSERT(false); // what are you doing here??
#endif

	prog=maxProg;
	return 0;
}

bool CropHelper::filterSphereInside(const Point3D &p) const
{
	return p.sqrDist(pA) < fA;
}


bool CropHelper::filterPlaneFront(const Point3D &testPt) const
{
	return ((testPt-pA).dotProd(pB) > 0.0f);
}

bool CropHelper::filterBoxInside(const Point3D &testPt) const
{
	return (pA[0] < testPt[0] && pA[1] < testPt[1] && pA[2] < testPt[2] )&&
		 (pB[0] > testPt[0] && pB[1] > testPt[1] && pB[2] > testPt[2] ) ;
}


bool CropHelper::filterCylinderInside(const Point3D &testPt) const
{

	//pA - cylinder origin
	//fA - cylinder half length (origin to end, along axis)
	//fB - cylinder sqr radius
	//qA - rotation quaternion
	Point3D ptmp;
	if(nearAxis)
	{
		ptmp=testPt-pA;

		return (ptmp[2] < fA && ptmp[2] > -fA && 
				ptmp[0]*ptmp[0]+ptmp[1]*ptmp[1] < fB);

	}
	else
	{
		Point3f p;
		//Translate to get position w respect to cylinder centre
		ptmp=testPt-pA;
		p.fx=ptmp[0];
		p.fy=ptmp[1];
		p.fz=ptmp[2];
		//rotate ion position into cylindrical coordinates
		quat_rot_apply_quat(&p,&qA);

		//Check inside upper and lower bound of cylinder
		// and check inside cylinder radius
		return (p.fz < fA && p.fz > -fA && 
				p.fx*p.fx+p.fy*p.fy < fB);
	}
}


unsigned int CropHelper::mapSphereInside(const Point3D &testPt) const
{
	float radius;
	radius = sqrtf(testPt.sqrDist(pA));

	if(radius <=fB)
		return (unsigned int) (mapMax*(radius/fB));
	else
		return -1;
}

unsigned int CropHelper::mapCylinderInsideAxial( const Point3D &testPt) const
{

	//pA - cylinder origin
	//fA - cylinder half length (origin to end, along axis)
	//fB - cylinder sqr radius
	//qA - rotation quaternion
	Point3D ptmp;
	float fZ;
	if(nearAxis)
	{
		ptmp=testPt-pA;

		if(!(ptmp[2] < fA && ptmp[2] > -fA && 
				ptmp[0]*ptmp[0]+ptmp[1]*ptmp[1] < fB) )
			return (unsigned int)-1;
		else
			fZ=ptmp[2];
	}
	else
	{
		Point3f p;
		//Translate to get position w respect to cylinder centre
		ptmp=testPt-pA;
		p.fx=ptmp[0];
		p.fy=ptmp[1];
		p.fz=ptmp[2];
		//rotate ion position into cylindrical coordinates
		quat_rot_apply_quat(&p,&qA);

		//Check inside upper and lower bound of cylinder
		// and check inside cylinder radius
		if(!(p.fz < fA && p.fz > -fA && 
				p.fx*p.fx+p.fy*p.fy < fB))
			return (unsigned int)-1;

		fZ=p.fz;
	}

	return (unsigned int)(((fZ+fA)/(2.0*fA))*(float)mapMax);

}

unsigned int CropHelper::mapCylinderInsideRadial( const Point3D &testPt) const
{

	//pA - cylinder origin
	//fA - cylinder half length (origin to end, along axis)
	//fB - cylinder sqr radius
	//qA - rotation quaternion
	Point3D ptmp;
	float fSqrRad;
	if(nearAxis)
	{
		ptmp=testPt-pA;

		if(!(ptmp[2] < fA && ptmp[2] > -fA && 
				ptmp[0]*ptmp[0]+ptmp[1]*ptmp[1] < fB) )
			return (unsigned int)-1;
		else
		{
			fSqrRad=ptmp[0]*ptmp[0] + ptmp[1]*ptmp[1];
		}
	}
	else
	{
		Point3f p;
		//Translate to get position w respect to cylinder centre
		ptmp=testPt-pA;
		p.fx=ptmp[0];
		p.fy=ptmp[1];
		p.fz=ptmp[2];
		//rotate ion position into cylindrical coordinates
		quat_rot_apply_quat(&p,&qA);

		fSqrRad=p.fx*p.fx + p.fy*p.fy;
		//Check inside upper and lower bound of cylinder
		// and check inside cylinder radius
		if(!( (p.fz < fA && p.fz > -fA ) &&  
					fSqrRad < fB))
			return (unsigned int)-1;

	}

	unsigned int mapPos;
	mapPos=(unsigned int)(fSqrRad/fB*(float)mapMax);
	ASSERT(mapPos < mapMax);

	//Area is constant in square space
	return mapPos;

}

//Direction is the axis along the full length of the cylinder
void CropHelper::setupCylinder(Point3D origin,float radius, Point3D direction)
{
	ASSERT(direction.sqrMag() > sqrtf(std::numeric_limits<float>::epsilon()));
	ASSERT(radius > 0.0f);

	pA=origin;
	//cylinder half length
	fA=sqrtf(direction.sqrMag())/2.0f;
	//cylinder square radius
	fB=radius*radius;
	
	Point3D zDir(0.0f,0.0f,1.0f);
	direction.normalise();

	float angle = zDir.angle(direction);
	//Check that we actually need to rotate, to avoid numerical singularity
	//when cylinder axis is too close to (or is) z-axis
	if(angle > sqrtf(std::numeric_limits<float>::epsilon())
	&& angle < M_PI - sqrtf(std::numeric_limits<float>::epsilon()))
	{
		//Cross product desired direction with 
		//zDirection to produce rotation vector, that when applied
		// moves us from actual cylinder coordinates to normal cylindrical coordinates
		
		//bastardise the Z direction to become our rotation
		// axis that brings us back to Z.
		zDir = zDir.crossProd(direction);
		zDir.normalise();

		Point3f rotVec;
		rotVec.fx=zDir[0];
		rotVec.fy=zDir[1];
		rotVec.fz=zDir[2];

		//Generate the rotating quaternions
		quat_get_rot_quat(&rotVec,-angle,&qA);
		nearAxis=false;

	}
	else
	{
		//Too close to the Z-axis, rotation vector is unable 
		//to be stably computed, and we don't need to rotate anyway
		nearAxis=true;
	}
}

unsigned int CropHelper::mapIon1D(const IonHit &ionIn) const
{
	ASSERT(!invertedClip);
	ASSERT(mapFunc);
	ASSERT(mapMax);
	//return the 1D mapping for the ion, or -1 for not mappable
	unsigned int mappingPos;
	mappingPos=(this->*mapFunc)(ionIn.getPosRef());
	ASSERT(mappingPos < mapMax || mappingPos == (unsigned int) -1);
	return mappingPos; 
}

void CropHelper::setFilterMode(size_t filterMode)
{
	ASSERT(filterMode < CROP_ENUM_END);
	algorithm=filterMode;
	setAlgorithm();
}


