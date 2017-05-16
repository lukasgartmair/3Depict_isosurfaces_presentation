 /* 
 * rdf.cpp - Radial distribution function implentation
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

#include "rdf.h"

#include "../filterCommon.h"

#include <gsl/gsl_sf_gamma.h>

using std::vector;

#ifdef _OPENMP
#include <omp.h>
#endif
//QHull library
//Build fix for qhull ; wx defines powerpc without
//assigning a value, causing build fail on powerpc
#ifdef __POWERPC__
	#pragma push_macro("__POWERPC__")
	#define __POWERPC__ 1
#endif
extern "C"
{
	#include <qhull/qhull_a.h>
}
#ifdef __POWERPC__
	#pragma pop_macro("__POWERPC__")
#endif

const unsigned int CALLBACK_REDUCE=5000;

enum PointDir{ 	POINTDIR_TOGETHER =0,
                POINTDIR_IN_COMMON,
                POINTDIR_APART
             };
//!Check which way vectors attached to two 3D points "point", 
/*! Two vectors may point "together", /__\ "apart" \__/  or 
 *  "In common" /__/ or \__\
 */
unsigned int vectorPointDir(const Point3D &pA, const Point3D &pB, 
				const Point3D &vC, const Point3D &vD)
{
	//Check which way vectors attached to two 3D points "point", 
	// - "together", "apart" or "in common"
	
	//calculate AB.CA, BA.DB
	float dot1  = (pB-pA).dotProd(vC - pA);
	float dot2= (pA - pB).dotProd(vD - pB);

	//We shall somewhat arbitrarily call perpendicular cases "together"
	if(dot1 ==0.0f || dot2 == 0.0f)
		return POINTDIR_TOGETHER;

	//If they have opposite signs, then they are "in common"
	if(( dot1  < 0.0f  && dot2 > 0.0f) || (dot1 > 0.0f && dot2 < 0.0f) )
		return POINTDIR_IN_COMMON;

	if( dot1 < 0.0f && dot2 <0.0f )
		return POINTDIR_APART; 

	ASSERT(dot1 > 0.0f && dot2 > 0.0f );
	
	return POINTDIR_TOGETHER;
}

//! Returns the shortest distance between a line segment and a given point
/* The inputs are the ends of the line segment and the point. Uses the formula that 
 * \f$ 
 * D = \abs{\vec{PE}}\f$
 * \f[ 
 * \mathrm{~if~} \vec{PA} \cdot \vec{AB} > 0 
 * \rightarrow \vec{PE} = \vec{A} 
 * \f]
 * \f[
 * \mathrm{~if~} \vec{AB} \cdot \vec{PB} > 0 ~\&~ \vec{PA} \cdot \vec{AB} < 0
 * \rightarrow \vec{PB} \cdot \frac{\vec{AB}}{\abs{\vec{AB}}} 
 * \f]
 * \f[
 * \mathrm{~if~} \vec{PB} \cdot \vec{AB} < 0 
 * \rightarrow \vec{B} 
 * \f]
 */
float distanceToSegment(const Point3D &fA, const Point3D &fB, const Point3D &p)
{

	//If the vectors a pointing "together" then use  point-line formula
	if(vectorPointDir(fA,fB,p,p) == POINTDIR_TOGETHER)
	{
		Point3D closestPt;
		Point3D vAB= fB-fA;

		//Use formula d^2 = |(B-A)(cross)(A-P)|^2/|B-A|^2
		return sqrtf( (vAB.crossProd(fA-p)).sqrMag()/(vAB.sqrMag()));
	}

	return sqrtf( std::min(fB.sqrDist(p), fA.sqrDist(p)) );
}


//!Find the distance between a point, and a triangular facet -- may be positive or negative
/* The inputs are the facet points (ABC) and the point P.
 * distance is shortest using standard plane version 
 * \f$ D = \vec{AB} \cdot \vec{n} \f$ 
 * iff dot products to each combination of \f$ \left( AP,BP,CP \right) \leq 0 \f$
 * otherwise closest point is on the boundary of the simplex.
 * tested by shortest distance to each line segment (E is shortest pt. AB is line segement)
 * \f$ \vec{E} = \frac{\vec{AB}}{|\vec{AB}|} 
 *  ( \vec{PB} \cdot \vec{AB})\f$
 */
float distanceToFacet(const Point3D &fA, const Point3D &fB, 
			const Point3D &fC, const Point3D &p, const Point3D &normal)
{

	//This will check the magnitude of the incoming normal
	ASSERT( fabs(sqrtf(normal.sqrMag()) - 1.0f) < 2.0* std::numeric_limits<float>::epsilon());
	unsigned int pointDir[3];
	pointDir[0] = vectorPointDir(fA,fB,p,p);
	pointDir[1] = vectorPointDir(fA,fC,p,p);
	pointDir[2] = vectorPointDir(fB,fC,p,p);

	//They can never be "APART" if the
	//vectors point to the same pt
	ASSERT(pointDir[0] != POINTDIR_APART);
	ASSERT(pointDir[1] != POINTDIR_APART);
	ASSERT(pointDir[2] != POINTDIR_APART);

	//Check to see if any of them are "in common"
	if(pointDir[0]  > 0 ||  pointDir[1] >0 || pointDir[2] > 0)
	{
		//if so, we have to check each edge for its closest point
		//then pick the best
		float bestDist[3];
		bestDist[0] = distanceToSegment(fA,fB,p);
		bestDist[1] = distanceToSegment(fA,fC,p);
		bestDist[2] = distanceToSegment(fB,fC,p);
	

		return std::min(bestDist[0],std::min(bestDist[1],bestDist[2]));
	}

	float temp;

	temp = fabs((p-fA).dotProd(normal));

	//check that the other points were not better than this!
	ASSERT(sqrtf(fA.sqrDist(p)) >= temp - std::numeric_limits<float>::epsilon());
	ASSERT(sqrtf(fB.sqrDist(p)) >= temp - std::numeric_limits<float>::epsilon());
	ASSERT(sqrtf(fC.sqrDist(p)) >= temp - std::numeric_limits<float>::epsilon());

	//Point lies above/below facet, use plane formula
	return temp; 
}


//A bigger MAX_NN_DISTS is better because you will attempt to grab more ram
//however there is a chance that memory allocation can fail, which currently I do not grab safely
const unsigned int MAX_NN_DISTS = 0x8000000; //96 MB samples at a time 



//obtains all the input points from ions that lie inside the convex hull after
//it has been shrunk such that the closest distance from the hull to the original data
//is reductionDim 
unsigned int GetReducedHullPts(const vector<Point3D> &points, float reductionDim,  
		unsigned int *progress, ATOMIC_BOOL &wantAbort, vector<Point3D> &pointResult)
{
	//TODO: This could be made to use a fixed amount of ram, by
	//partitioning the input points, and then
	//computing multiple hulls.
	//Take the resultant hull points, then hull again. This would be
	//much more space efficient, and more easily parallellised
	//Alternately, compute a for a randoms K set of points, and reject
	//points that lie in the hull from further computation

	//Need at least 4 points to define a hull in 3D
	if(points.size() < 4) 
		return 1;

	unsigned int dummyProg;
	vector<Point3D> theHull;
	if(computeConvexHull(points,progress,wantAbort,theHull,false,false))
		return 2;

	Point3D midPoint(0,0,0);
	for(size_t ui=0;ui<theHull.size();ui++)
		midPoint+=theHull[ui];
	midPoint *= 1.0f/(float)theHull.size();

	//Now we will find the mass/volume centroid of the hull
	//by constructing sets of pyramids.
	//We cannot use the simple midpoint, as point distribution
	//around the hull surface may be uneven
	
	//Here the faces of the hull are the bases of 
	//pyramids, and the midpoint is the apex
	Point3D hullCentroid(0.0f,0.0f,0.0f);
	float massPyramids=0.0f;
	//Run through the faced list
	facetT *curFac = qh facet_list;
	
	while(curFac != qh facet_tail)
	{
		vertexT *vertex;
		Point3D pyramidCentroid;
		unsigned int ui;
		Point3D ptArray[3];
		float vol;

		//find the pyramid volume + centroid
		pyramidCentroid = midPoint;
		ui=0;
		
		//This assertion fails, some more processing is needed to be done to break
		//the facet into something simplical
		ASSERT(curFac->simplicial);
		vertex  = (vertexT *)curFac->vertices->e[ui].p;
		while(vertex)
		{	//copy the vertex info into the pt array
			(ptArray[ui])[0]  = vertex->point[0];
			(ptArray[ui])[1]  = vertex->point[1];
			(ptArray[ui])[2]  = vertex->point[2];
		
			//aggregate pyramidal points	
			pyramidCentroid += ptArray[ui];
		
			//increment before updating vertex
			//to allow checking for NULL termination	
			ui++;
			vertex  = (vertexT *)curFac->vertices->e[ui].p;
		
		}
		
		//note that this counter has been post incremented. 
		ASSERT(ui ==3);
		vol = pyramidVol(ptArray,midPoint);
		
		ASSERT(vol>=0);
		
		//Find the midpoint of the pyramid, this will be the 
		//same as its centre of mass.
		pyramidCentroid*= 0.25f;
		hullCentroid = hullCentroid + (pyramidCentroid*vol);
		massPyramids+=vol;

		curFac=curFac->next;
	}

	hullCentroid *= 1.0f/massPyramids;
	
	float minDist=std::numeric_limits<float>::max();
	//find the smallest distance between the centroid and the
	//convex hull
       	curFac=qh facet_list;
	while(curFac != qh facet_tail)
	{
		float temp;
		Point3D vertexPt[3];
		
		//The shortest distance from the plane to the point
		//is the dot product of the UNIT normal with 
		//A-B, where B is on plane, A is point in question
		for(unsigned int ui=0; ui<3; ui++)
		{
			vertexT *vertex;
			//grab vertex
			vertex  = ((vertexT *)curFac->vertices->e[ui].p);
			vertexPt[ui] = Point3D(vertex->point[0],vertex->point[1],vertex->point[2]);
		}

		//Find the distance between hull centroid and a given facet
		temp = distanceToFacet(vertexPt[0],vertexPt[1],vertexPt[2],hullCentroid,
					Point3D(curFac->normal[0],curFac->normal[1],curFac->normal[2]));

		if(temp < minDist)
			minDist = temp;
	
		curFac=curFac->next;
	}

	//shrink the convex hull such that it lies at
	//least reductionDim from the original surface of
	//the convex hull
	float scaleFactor;
	scaleFactor = 1  - reductionDim/ minDist;

	if(scaleFactor < 0.0f)
		return RDF_ERR_NEGATIVE_SCALE_FACT;

	
	//now scan through the input points and see if they
	//lie in the reduced convex hull
	vertexT *vertex = qh vertex_list;	

	unsigned int ui=0;
	while(vertex !=qh vertex_tail)
	{
		//Translate around hullCentroid before scaling, 
		//then undo translation after scale
		//Modify the vertex data such that it is scaled around the hullCentroid
		vertex->point[0] = (vertex->point[0] - hullCentroid[0])*scaleFactor + hullCentroid[0];
		vertex->point[1] = (vertex->point[1] - hullCentroid[1])*scaleFactor + hullCentroid[1];
		vertex->point[2] = (vertex->point[2] - hullCentroid[2])*scaleFactor + hullCentroid[2];
	
		vertex = vertex->next;
		ui++;
	}

	//if the dot product of the normal with the point vector of the
	//considered point P, to any vertex on all of the facets of the 
	//convex hull F1, F2, ... , Fn is negative,
	//then P does NOT lie inside the convex hull.
	pointResult.reserve(points.size()/2);
	curFac = qh facet_list;
	
	//minimum distance from centroid to convex hull
	for(unsigned int ui=points.size(); ui--;)
	{
		float fX,fY,fZ;
		double *ptArr,*normalArr;
		fX =points[ui][0];
		fY = points[ui][1];
		fZ = points[ui][2];
		
		//loop through the facets
		curFac = qh facet_list;
		while(curFac != qh facet_tail)
		{
			//Dont ask. It just grabs the first coords of the vertex
			//associated with this facet
			ptArr = ((vertexT *)curFac->vertices->e[0].p)->point;
			
			normalArr = curFac->normal;
			//if the dotproduct is negative, then the point vector from the 
			//point in question to the surface is in opposite to the outwards facing
			//normal, which means the point lies outside the hull	
			if (dotProduct( (float)ptArr[0]  - fX,
					(float)ptArr[1] - fY,
					(float)ptArr[2] - fZ,
					normalArr[0], normalArr[1],
					normalArr[2]) >= 0)
			{
				curFac=curFac->next;
				continue;
			}
			goto reduced_loop_next;
		}
		//we passed all tests, point is inside convex hull
		pointResult.push_back(points[ui]);
		
reduced_loop_next:
	;
	}

	freeConvexHull();

	return 0;
}

//!Generate an NN histogram using NN-max cutoffs 
unsigned int generateNNHist( const vector<Point3D> &pointList, 
			const K3DTree &tree,unsigned int nnMax, unsigned int numBins,
		       	vector<vector<size_t> > &histogram, float *binWidth , unsigned int *progressPtr,
			ATOMIC_BOOL &wantAbort)
{
	if(pointList.size() <=nnMax)
		return RDF_ERR_INSUFFICIENT_INPUT_POINTS;
	
	//Disallow exact matching for NNs
	float deadDistSqr;
	deadDistSqr= std::numeric_limits<float>::epsilon();
	
	//calclate NNs
	BoundCube cube;
	cube.setBounds(pointList);

	//Allocate and assign the initial max distances
	float *maxSqrDist= new float[nnMax];
	for(unsigned int ui=0; ui<nnMax; ui++)
		maxSqrDist[ui] =0.0f; 
	


	int callbackReduce=CALLBACK_REDUCE;
#ifdef _OPENMP
	size_t numAnalysed=0;
	bool spin=false;
#endif
	//do NN search
#pragma omp parallel for shared(spin,numAnalysed) firstprivate(callbackReduce)
	for(unsigned int ui=0; ui<pointList.size(); ui++)
	{
#ifdef _OPENMP
		if(spin)
			continue;
#endif
		vector<const Point3D *> nnPoints;	
		tree.findKNearest(pointList[ui],cube,
					nnMax,nnPoints,deadDistSqr);


		
		for(unsigned int uj=0; uj<nnPoints.size(); uj++)
		{
			float temp;
			temp = nnPoints[uj]->sqrDist(pointList[ui]);	
			if(temp > maxSqrDist[uj])
				maxSqrDist[uj] = temp;
		}
			

		//Callbacks to perform UI updates as needed
		if(!(callbackReduce--))
		{
#ifdef _OPENMP 
			#pragma omp critical
			{
				numAnalysed+=CALLBACK_REDUCE;
				*progressPtr= (unsigned int)((float)(numAnalysed)/((float)pointList.size())*100.0f);
				if(wantAbort)
					spin=true;
			}			
#else
			*progressPtr= (unsigned int)((float)(ui)/((float)pointList.size())*50.0f);
			if(wantAbort)
			{
				delete[] maxSqrDist;
				return RDF_ABORT_FAIL;
			}
#endif
			callbackReduce=CALLBACK_REDUCE;
		}

	}
#ifdef _OPENMP
	if(spin)
	{
		delete[] maxSqrDist;
		return RDF_ABORT_FAIL;
	}
#endif


	float maxOfMaxDists=0;
	float *maxDist=new float[nnMax];
	for(unsigned int ui=0; ui<nnMax; ui++)
	{
		if(maxOfMaxDists < maxSqrDist[ui])
			maxOfMaxDists = maxSqrDist[ui];

		//convert maxima from sqrDistance 
		//to normal =distance	
		maxDist[ui] =sqrtf(maxSqrDist[ui]);
	}	

	maxOfMaxDists=sqrtf(maxOfMaxDists);
	maxDist[nnMax-1] = maxOfMaxDists;	

	//Cacluate the bin widths required to accommodate this
	//distribution
	for(unsigned int ui=0; ui<nnMax; ui++)
		binWidth[ui]= maxDist[ui]/(float)numBins;
	delete[] maxDist;
	
	//Generate histogram for what we have already
	//zero out memory
	histogram.resize(nnMax);
	for(unsigned int ui=0;ui<nnMax;ui++)
		histogram[ui].resize(numBins,0);

	//we know the bin that things will fall into now, so we can scan 
	//remaining points and place into the histogram on the fly now
	
#ifdef _OPENMP
	spin=false;
	numAnalysed=0;
#endif
	
	callbackReduce=CALLBACK_REDUCE;
#pragma omp parallel for firstprivate(callbackReduce)
	for(unsigned int ui=0; ui<pointList.size(); ui++)
	{
		vector<const Point3D *> nnPoints;
#ifdef _OPENMP
		if(spin)
			continue;
#endif

		tree.findKNearest(pointList[ui],cube,
					nnMax, nnPoints);

		for(unsigned int uj=0; uj<nnPoints.size(); uj++)
		{
			unsigned int offsetTemp;
			float temp;

			temp=sqrtf(nnPoints[uj]->sqrDist(pointList[ui]));
			offsetTemp = (unsigned int)(temp/binWidth[uj]);
			
			//Prevent overflow due to temp/binWidth exceeding array dimension 
			//as (temp is <= binwidth, not < binWidth)
			if(offsetTemp == numBins)
				offsetTemp--;
			ASSERT(offsetTemp < nnMax*numBins);

			(histogram[uj])[offsetTemp]++;
		}
	

		//Callbacks to perform UI updates as needed
#ifdef _OPENMP 
		if(!(callbackReduce--))
		{
		#pragma omp critical
		{
			*progressPtr= (unsigned int)((float)(numAnalysed)/((float)pointList.size())*50.0f + 50.0f);
			if(wantAbort)
				spin=true;
			numAnalysed+=CALLBACK_REDUCE;
		}			
			callbackReduce=CALLBACK_REDUCE;
		}	
#else
		if(!(callbackReduce--))
		{
			*progressPtr= (unsigned int)((float)(ui)/((float)pointList.size())*50.0f + 50.0f);
			if(wantAbort)
			{
				delete[] maxSqrDist;
				return RDF_ABORT_FAIL;
			}
			callbackReduce=CALLBACK_REDUCE;
		}
#endif
	}
	delete[] maxSqrDist;	

#ifdef _OPENMP
	if(spin)
		return RDF_ABORT_FAIL;
#endif

	return 0;
}


unsigned int generate1DAxialDistHist(const vector<Point3D> &pointList, const K3DTree &tree,
		const Point3D &axisDir, unsigned int *histogram, float distMax, unsigned int numBins,
		unsigned int *progressPtr, ATOMIC_BOOL &wantAbort)
{
	ASSERT(fabs(axisDir.sqrMag() -1.0f) < sqrt(std::numeric_limits<float>::epsilon()));
#ifdef DEBUG
	for(unsigned int ui=0;ui<numBins;ui++)
	{
		ASSERT(!histogram[ui]);
	}
#endif

	if(pointList.empty())
		return 0;

	BoundCube cube;
	cube.setBounds(pointList);
	//We don't know how much ram we will need
	//one could estimate an upper bound by 
	//tree.numverticies*pointlist.size()
	//but I don't have  a tree.numvertices
	float maxSqrDist = distMax*distMax;

	unsigned int warnBiasCount=0;
#ifdef _OPENMP
	bool spin=false;
#endif
	
	//Main r-max searching routine
	const unsigned int CALLBACK_REDUCE_VAL=CALLBACK_REDUCE/100;
	unsigned int callbackReduce=CALLBACK_REDUCE_VAL;
	unsigned int numAnalysed=0;
#pragma omp parallel for firstprivate(callbackReduce) shared(numAnalysed)
	for(unsigned int ui=0; ui<pointList.size(); ui++)
	{
#ifdef _OPENMP
		if(spin)
			continue;
#endif
		float sqrDist,deadDistSqr;
		Point3D sourcePoint;
		const Point3D *nearPt=0;
		//Go through each point and grab up to the maximum distance
		//that we need
		
		//Loop from this ion, up to its max	
		//disable exact matching, by requiring d^2 > epsilon
		deadDistSqr=std::numeric_limits<float>::epsilon();
		sqrDist=0;
		sourcePoint=pointList[ui];
		while(deadDistSqr < maxSqrDist)
		{

			//Grab the nearest point
			nearPt = tree.findNearest(sourcePoint, cube,
							 deadDistSqr);

			if(nearPt)
			{
				//Cacluate the sq of the distance to the point
				sqrDist = nearPt->sqrDist(sourcePoint);
				
				//if sqrDist is = maxSqrdist then this will cause
				//the histogram indexing to trash alternate memory
				//- this is bad - prevent this please.	
				if(sqrDist < maxSqrDist)
				{
					//Compute the projection of
					// the point onto the axis of the
					// primary analysis direction
					float distance;
					distance=(*nearPt-sourcePoint).dotProd(axisDir);
				
					//update the histogram with the new position.
					// centre of the distribution function lies at the analysis point,
					// and can be either negative or positive. 
					// Shift the zero to the center of the histogram
					int offset=(int)(((0.5f*distance)/distMax+0.5f)*(float)numBins);
					if(offset < (int)numBins && offset >=0)
					{
#pragma omp critical
						histogram[offset]++;
					}
				}

				//increase the dead distance to the last distance
				deadDistSqr = sqrDist+std::numeric_limits<float>::epsilon();
			}
			else
			{		
				//Oh no, we had a problem, somehow we couldn't find enough
#pragma omp critical
				warnBiasCount++;
				break;
			}
			
		}
		
		
		//Run callbacks as needed
		if(!(callbackReduce--))
		{
#pragma omp critical
			{
			numAnalysed+=CALLBACK_REDUCE_VAL;
			*progressPtr= (unsigned int)((float)(numAnalysed)/((float)pointList.size())*100.0f);
			if(wantAbort)
			{
#ifdef _OPENMP
				spin=true;
#else
				return RDF_ABORT_FAIL;
#endif
			}
			}
			callbackReduce=CALLBACK_REDUCE_VAL;
		}

	}
#ifdef _OPENMP
	if(spin)
		return RDF_ABORT_FAIL;
#endif
	*progressPtr=100;
	return 0;
}


unsigned int generate1DAxialNNHist(const vector<Point3D> &pointList, const K3DTree &tree,
		const Point3D &axisDir, unsigned int *histogram, float &binWidth, unsigned int nnMax, unsigned int numBins,
		unsigned int *progressPtr, ATOMIC_BOOL &wantAbort)
{
#ifdef DEBUG
	for(unsigned int ui=0;ui<numBins;ui++)
	{
		ASSERT(!histogram[ui]);
	}
#endif

	if(pointList.size() <=nnMax)
		return RDF_ERR_INSUFFICIENT_INPUT_POINTS;
	
	//Disallow exact matching for NNs
	float deadDistSqr;
	deadDistSqr= std::numeric_limits<float>::epsilon();
	
	//calclate NNs
	BoundCube cube;
	cube.setBounds(pointList);

	//Allocate and assign the initial max distances
	float *maxAxialDist= new float[nnMax];
	for(unsigned int ui=0; ui<nnMax; ui++)
		maxAxialDist[ui] =0.0f; 
	

	int callbackReduce=CALLBACK_REDUCE;
#ifdef _OPENMP
	size_t numAnalysed=0;
	bool spin=false;
#endif
	//do NN search - first pass we are only looking for the maximum distance
	// for the distribution. We do not update the histogram
	//------
#pragma omp parallel for shared(spin,numAnalysed) firstprivate(callbackReduce)
	for(unsigned int ui=0; ui<pointList.size(); ui++)
	{
#ifdef _OPENMP
		if(spin)
			continue;
#endif
		vector<const Point3D *> nnPoints;	
		tree.findKNearest(pointList[ui],cube,
					nnMax,nnPoints,deadDistSqr);

		for(unsigned int uj=0; uj<nnPoints.size(); uj++)
		{
			//compute upper bound for plot output distance
			float temp;
			temp=fabs((*nnPoints[uj]-pointList[ui]).dotProd(axisDir));
			if(temp > maxAxialDist[uj])
				maxAxialDist[uj] = temp;
		}
			

		//Callbacks to perform UI updates as needed
		if(!(callbackReduce--))
		{
#ifdef _OPENMP 
			#pragma omp critical
			{
				numAnalysed+=CALLBACK_REDUCE;
				*progressPtr= (unsigned int)((float)(numAnalysed)/((float)pointList.size())*100.0f);
				if(wantAbort)
					spin=true;
			}			
#else
			*progressPtr= (unsigned int)((float)(ui)/((float)pointList.size())*100.0f);
			if(wantAbort)
			{
				delete[] maxAxialDist;
				return RDF_ABORT_FAIL;
			}
#endif
			callbackReduce=CALLBACK_REDUCE;
		}

	}
#ifdef _OPENMP
	if(spin)
	{
		delete[] maxAxialDist;
		return RDF_ABORT_FAIL;
	}
#endif


	float maxOfMaxDists=0;
	for(unsigned int ui=0; ui<nnMax; ui++)
	{
		if(maxOfMaxDists < maxAxialDist[ui])
			maxOfMaxDists = maxAxialDist[ui];
	}	

	maxOfMaxDists=sqrtf(maxOfMaxDists);

	//Cacluate the bin widths required to accommodate this
	//distribution
	for(unsigned int ui=0; ui<nnMax; ui++)
		binWidth= maxOfMaxDists/(float)numBins;
	//------
	

	//we know the bin that things will fall into now, so we can scan 
	//points for their distance values
	// and place into the histogram now 
	//----------------------	
#ifdef _OPENMP
	spin=false;
	numAnalysed=0;
#endif
	
	callbackReduce=CALLBACK_REDUCE;
#pragma omp parallel for firstprivate(callbackReduce)
	for(unsigned int ui=0; ui<pointList.size(); ui++)
	{
		vector<const Point3D *> nnPoints;
#ifdef _OPENMP
		if(spin)
			continue;
#endif

		tree.findKNearest(pointList[ui],cube,
					nnMax, nnPoints);

		for(unsigned int uj=0; uj<nnPoints.size(); uj++)
		{
			float temp;
			temp=(*nnPoints[uj]-pointList[ui]).dotProd(axisDir);
			int offset=(int)(((0.5f*temp)/maxOfMaxDists+0.5f)*numBins);

			if(offset < numBins && offset >=0)	
			{
				//TODO: OpenMP could use multiple histograms
				// rather than locking
#pragma omp critical 
				histogram[offset]++;
			}
		}
	

		//Callbacks to check for abort as needed
#ifdef _OPENMP 
		if(!(callbackReduce--))
		{
		#pragma omp critical
		{
			*progressPtr= (unsigned int)((float)(numAnalysed)/((float)pointList.size())*100.0f);
			if(wantAbort)
				spin=true;
			numAnalysed+=CALLBACK_REDUCE;
		}			
			callbackReduce=CALLBACK_REDUCE;
		}	
#else
		if(!(callbackReduce--))
		{
			*progressPtr= (unsigned int)((float)(ui)/((float)pointList.size())*100.0f);
			if(wantAbort)
			{
				delete[] maxAxialDist;
				return RDF_ABORT_FAIL;
			}
			callbackReduce=CALLBACK_REDUCE;
		}
#endif
	}
	delete[] maxAxialDist;	

#ifdef _OPENMP
	if(spin)
		return RDF_ABORT_FAIL;
#endif

	return 0;
}


//!Generate an NN histogram using distance max cutoffs. Input histogram must be zeroed,
unsigned int generateDistHist(const vector<Point3D> &pointList, const K3DTree &tree,
			unsigned int *histogram, float distMax,
			unsigned int numBins, unsigned int &warnBiasCount,
			unsigned int *progressPtr,ATOMIC_BOOL &wantAbort)
{


#ifdef DEBUG
	for(unsigned int ui=0;ui<numBins;ui++)
	{
		ASSERT(!histogram[ui]);
	}
#endif

	if(pointList.empty())
		return 0;

	BoundCube cube;
	cube.setBounds(pointList);
	//We dont know how much ram we will need
	//one could estimate an upper bound by 
	//tree.numverticies*pointlist.size()
	//but i dont have  a tree.numvertcies
	float maxSqrDist = distMax*distMax;

	warnBiasCount=0;
	
	//Main r-max searching routine
	unsigned int callbackReduce=CALLBACK_REDUCE;
#ifdef _OPENMP
	bool spin=false;
	size_t numAnalysed=0;
	size_t numAnalysedThread=0;

	unsigned int threadHist[numBins][omp_get_max_threads()];
	for(size_t ui=0; ui<omp_get_max_threads(); ui++)
	{
		for (size_t uj = 0; uj < numBins; uj++)
			threadHist[uj][ui] = 0;
	}
#endif

#pragma omp parallel for shared(spin,histogram,numAnalysed,threadHist) firstprivate(callbackReduce,numAnalysedThread) default(shared)
	for(unsigned int ui=0; ui<pointList.size(); ui++)
	{
#ifdef _OPENMP
		if(spin)
			continue;
#endif

		float sqrDist,deadDistSqr;
		Point3D sourcePoint;
		const Point3D *nearPt=0;
		//Go through each point and grab up to the maximum distance
		//that we need
		
		//Loop from this ion, up to its max	
		//disable exact matching, by requiring d^2 > epsilon
		deadDistSqr=std::numeric_limits<float>::epsilon();
		sqrDist=0;
		sourcePoint=pointList[ui];
		while(deadDistSqr < maxSqrDist)
		{

			//Grab the nearest point
			nearPt = tree.findNearest(sourcePoint, cube,
							 deadDistSqr);

			if(nearPt)
			{
				//Cacluate the sq of the distance to the point
				sqrDist = nearPt->sqrDist(sourcePoint);
				
				//if sqrDist is = maxSqrdist then this will cause
				//the histogram indexing to trash alternate memory
				//- this is bad - prevent this please.	
				if(sqrDist < maxSqrDist)
				{
					//Add the point to the histogram
#ifdef _OPENMP
					threadHist[(size_t) ((sqrtf(sqrDist/maxSqrDist)*(float)numBins))] [omp_get_thread_num()]++;
#else
					histogram[(size_t)((sqrtf(sqrDist/maxSqrDist)*(float)numBins))]++;
#endif
				}

				//increase the dead distance to the last distance
				deadDistSqr = sqrDist+std::numeric_limits<float>::epsilon();
			}
			else
			{		
				//Oh no, we had a problem, somehow we couldn't find enough
#pragma omp critical
				warnBiasCount++;
				break;
			}


			if(!(callbackReduce--))
			{
#ifdef _OPENMP 
				#pragma omp critical
				{
					numAnalysed+=numAnalysedThread;
					*progressPtr= (unsigned int)((float)(numAnalysed)/((float)pointList.size())*100.0f);
					if(wantAbort)
						spin=true;
				}
				if(spin)
					break;
				numAnalysedThread=0;
#else
				*progressPtr= (unsigned int)((float)(ui)/((float)pointList.size())*100.0f);
				if(wantAbort)
					return RDF_ABORT_FAIL;
#endif
				callbackReduce=CALLBACK_REDUCE;
			}
		}
#ifdef _OPENMP
		numAnalysedThread++;
#endif
	}

#ifdef _OPENMP
	if(spin)
		return RDF_ABORT_FAIL;
    
	for (size_t i = 0; i < numBins; i++)
	{
		for (size_t j = 0; j < omp_get_max_threads(); j++)
			histogram[i] += threadHist[i][j];
	}
#endif

	//Calculations complete!
	return 0;
}



void generateKnnTheoreticalDist(const std::vector<float> &radii, float density, unsigned int nn,
					std::vector<float> &nnDist)
{
	ASSERT(density >=0);
	ASSERT(nn>0);
	//Reference :Stephenson, 2009, simplified using D=3

	//Distribution requires evaluation of :
	//- the gamma function gamma(5/2)
	const float GAMMA_2PT5 = 1.32934038817914;
	// - power of pi : pi^(D/2)
	const float PI_POW_D_2 = pow(M_PI,3.0/2.0);
	
	const float LAMBDA_FACTOR =  PI_POW_D_2/GAMMA_2PT5;

	double pBase,lambda;

	//radius independent component
	lambda = density*LAMBDA_FACTOR;
	pBase = 3.0/gsl_sf_fact(nn-1);
	pBase*= pow(lambda,nn);
	

	nnDist.resize(radii.size());
	for(size_t ui=0;ui<radii.size();ui++)
	{
		double r,p;
		p=pBase;
		r =radii[ui];	
		p*= pow(r,3*nn-1);
		p*=exp(-lambda*r*r*r);
		nnDist[ui]=p;
	}

}

bool qhullTest()
{
#if defined(__WIN64)
	//If using a cross-compile (at least)
	// qhull under win64 must use long long, or we get random crashes
	// The definition is set in qhull/mem.h
	COMPILE_ASSERT(sizeof(ptr_intT) == sizeof(long long))
#endif

	return true;
}


