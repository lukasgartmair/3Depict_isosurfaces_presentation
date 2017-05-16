/*
 * voxels.cpp - Voxelised data manipulation class 
 * Copyright (C) 2015  D. Haley
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
#include "voxels.h"

#include <utility>

#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_blas.h>

using std::vector;
using std::pair;
using std::numeric_limits;




//Helper function to test if a point lies in a convex polygon. The input points MUST be convex
//	- by default this will re-sort the polygon. This is needed to
//		ensure convex ordering. If data is already ordered, you can
//		safely disable this (angularSort=false).;
bool pointIn2DConvexPoly(float px,float py,
	vector<pair<float,float> > &planarPts2D, bool angularSort=true)
{

	ASSERT(planarPts2D.size() >=3);

	if(angularSort)
	{
		//TODO : Optimise me. Probably not required to calculate angles explicitly

		//Find the centre x,y value
		float midPx =0;
		float midPy =0;
		for(size_t ui=1;ui<planarPts2D.size();ui++)
		{
			midPx +=planarPts2D[ui].first;
			midPy +=planarPts2D[ui].second;
		}
		midPx/=planarPts2D.size();
		midPy/=planarPts2D.size();
	
		//sort points by angle between vector p-p_0 and [1,0]	

		vector<pair<unsigned int, float> > angles;
		angles.resize(planarPts2D.size());
		for(unsigned int ui=0;ui<planarPts2D.size();ui++)
		{
			float dx,dy;	
			dx = planarPts2D[ui].first - midPx;
			dy = planarPts2D[ui].second - midPy;
	
			angles[ui] = make_pair(ui,atan2(dy,dx));
		}

		//--
		//First, sort by angle
		ComparePairSecond cmp;
		std::sort(angles.begin(),angles.end(),cmp); //Sort angle mapping

		//then re-map the original points to the sorted angle
		vector<pair<float,float>  > tmp;
		tmp.resize(planarPts2D.size());
		for(size_t ui=0;ui<planarPts2D.size();ui++)
			tmp[ui] = planarPts2D[angles[ui].first];
		//--
		

		planarPts2D.swap(tmp);
	}

	//find the normal vector. This is achieved by flipping the X/Y values
	float nx =-(planarPts2D[1].second -planarPts2D[0].second);
	float ny =(planarPts2D[1].first -planarPts2D[0].first);

	float dx = (px-planarPts2D[0].first);
	float dy = (py-planarPts2D[0].second);

	//dot-product the result. If positive, is on RHS of line
	bool positive = (nx*dx + ny*dy)>0;


	//repeat, aborting if any half-plane  intersects
	size_t nP = planarPts2D.size();
	for(size_t ui=1;ui<nP;ui++)
	{
		unsigned int next;
		next = 	(ui+1)%nP;

		nx =-( planarPts2D[next].second -planarPts2D[ui].second);
		ny =( planarPts2D[next].first -planarPts2D[ui].first);
		
		dx = (px-planarPts2D[ui].first);
		dy = (py-planarPts2D[ui].second);

		if((nx*dx + ny*dy > 0) != positive)
			return false;
	}

	return true;
}

//FIXME: This code is unfinished.
template<class T>
vector<Point3D> getVoxelIntersectionPoints(const BoundCube &b, const Point3D &p, const Point3D &normal, 
						const Voxels<T> &vox, unsigned int numRequiredSamples, 
							vector<Point3D> &samples, vector<T> &interpVal)
{
	vector<Point3D> pts;
	b.getPlaneIntersectVertices(p,normal,pts);

	//Dont do anything if there is no intersection 
	//(should have at least 3 pts to form a plane
	if(pts.size() < 3)
		return pts;
		

	//Now, using the plane points, rotate these into the Z=0 plane
	gsl_matrix *m = gsl_matrix_alloc(3,3); 
	computeRotationMatrix(Point3D(0,0,1),
		Point3D(1,0,0),normal,Point3D(1,0,0),m);

	//Now rotate them into the Z=0 plane	
	vector<Point3D> planarPts;
	rotateByMatrix(pts,m,planarPts);

	//Find the 2D bounding box, then generate uniform deviate
	// random numbers. Scale these ot fit inside the bbox.

	vector<pair<float,float> > planarPts2D;
	float bounds[2][2];
	bounds[0][0] = std::numeric_limits<float>::max(); //minX
	bounds[0][1] = std::numeric_limits<float>::max(); //minY
	bounds[1][0] = -std::numeric_limits<float>::max(); //maxX
	bounds[1][1] = -std::numeric_limits<float>::max(); //maxY
		
	for(unsigned  int ui=0; ui<planarPts.size();ui++)
	{
		//Should lie pretty close to z=0
		ASSERT(planarPts[ui][2] < sqrt(std::numeric_limits<float>::epsilon()));

		planarPts2D[ui].first = planarPts[ui][0];	
		planarPts2D[ui].second = planarPts[ui][1];	

		bounds[0][0] = std::min(bounds[0][0],planarPts2D[ui].first);
		bounds[0][1] = std::min(bounds[0][1],planarPts2D[ui].second);

		bounds[1][0] = std::max(bounds[1][0],planarPts2D[ui].first);
		bounds[1][1] = std::max(bounds[1][1],planarPts2D[ui].second);
	}

	//Init the random number generator	
	RandNumGen rng;
	rng.initTimer();

	//compute scaling factors
	float ax,ay;
	ax = bounds[1][0] - bounds[0][0];
	ay = bounds[1][1] - bounds[0][1];

	//generate the randomly sampled points
	size_t nSample=0;
	samples.resize(numRequiredSamples);
	while(nSample< numRequiredSamples)
	{
		float px,py;
		px = ax*rng.genUniformDev() + bounds[0][0];
		py = ay*rng.genUniformDev() + bounds[0][0];
		
		if(pointIn2DConvexPoly(px,py,planarPts2D))
		{
			samples[nSample] = Point3D(px,py,0);
			nSample++;
		}
	}


	//Transpose the matrix to obtain the inverse transform 
	// originally rotate from frame to z=0. After transpose, 
	// will rotate from z=0 to frame 
	gsl_matrix_transpose(m);
	gsl_vector *vRot = gsl_vector_alloc(3);
	gsl_vector *vOrig = gsl_vector_alloc(3);

	gsl_vector_set(vOrig,3,0);
	for(size_t ui=0;ui<samples.size(); ui++)
	{
		gsl_vector_set(vOrig,0,samples[ui][0]);
		gsl_vector_set(vOrig,1,samples[ui][1]);
		//compute v = m * pY;
		gsl_blas_dgemv(CblasNoTrans,1.0, m, vOrig,0,vRot);

		samples[ui] = Point3D(vRot->data);
	}	

	gsl_vector_free(vRot);
	gsl_vector_free(vOrig);
	gsl_matrix_free(m);

	
	//Find the interpolated value for each point in the voxel set
	interpVal.resize(samples.size());
	for(size_t ui=0;ui<samples.size();ui++)
	{
		const size_t INTERP_MODE=VOX_INTERP_LINEAR;
		vox.getInterpolatedData(samples[ui],interpVal[ui]);
	}


	//Now delanuay tessalate the random points, with the surrounding polygon

		
}

#ifdef DEBUG
#include <algorithm>

const float FLOAT_SMALL=
	sqrt(numeric_limits<float>::epsilon());

bool simpleMath()
{
	Voxels<float> a,b,c;
	a.resize(3,3,3);
	a.fill(2.0f);

	float f;
	f=a.getSum();
	TEST(fabs(f-3.0*3.0*3.0*2.0 )< FLOAT_SMALL,"getsum test");
	TEST(fabs(a.count(1.0f)- 3.0*3.0*3.0) < FLOAT_SMALL,"Count test"); 

	return true;
}

bool basicTests()
{
	Voxels<float> f;
	f.resize(3,3,3);
	
	size_t xs,ys,zs;
	f.getSize(xs,ys,zs);
	TEST(xs == ys && ys == zs && zs == 3,"resize tests");
	


	f.fill(0);
	f.setData(1,1,1,1.0f);

	TEST(fabs(f.max() - 1.0f) < FLOAT_SMALL,"Fill and data set");

	f.resizeKeepData(2,2,2);
	f.getSize(xs,ys,zs);

	TEST(xs == ys && ys == zs && zs == 2, "resizeKeepData");
	TEST(f.max() == 1.0f,"resize keep data");
	
	//Test slice functions
	//--
	Voxels<float> v;
	v.resize(2,2,2);
	for(size_t ui=0;ui<8;ui++)
		v.setData(ui&1, (ui & 2) >> 1, (ui &4)>>2, ui);

	float *slice = new float[4];
	//Test Z slice
	v.getSlice(2,0,slice);
	for(size_t ui=0;ui<4;ui++)
	{
		ASSERT(slice[ui] == ui);
	}

	//Expected results
	float expResults[4];
	//Test X slice
	expResults[0]=0; expResults[1]=2;expResults[2]=4; expResults[3]=6;
	v.getSlice(0,0,slice);
	for(size_t ui=0;ui<4;ui++)
	{
		ASSERT(slice[ui] == expResults[ui]);
	}

	//Test Y slice
	v.getSlice(1,1,slice);
	expResults[0]=2; expResults[1]=3;expResults[2]=6; expResults[3]=7;
	for(size_t ui=0;ui<4;ui++)
	{
		ASSERT(slice[ui] == expResults[ui]);
	}

	delete[] slice;

	//-- try again with nonuniform voxels
	v.resize(4,3,2);
	for(size_t ui=0;ui<24;ui++)
		v.setData(ui, ui);

	slice = new float[12];
	//Test Z slice
	v.getSlice(2,1,slice);
	for(size_t ui=0;ui<12;ui++)
	{
		ASSERT( slice[ui] >=12);
	}

	delete[] slice;
	//--

	return true;
}


/*
bool edgeCountTests()
{
	Voxels<float> v;
	v.resize(4,4,4);


	TEST(v.getEdgeUniqueIndex(0,0,0,3) == v.getEdgeUniqueIndex(0,1,1,0),"Edge coincidence");
	TEST(v.getEdgeUniqueIndex(0,0,0,6) == v.getEdgeUniqueIndex(1,0,0,4),"Edge coincidence");
	TEST(v.getEdgeUniqueIndex(0,0,0,2) == v.getEdgeUniqueIndex(0,0,1,0),"Edge coincidence");

	//Check for edge -> index -> edge round tripping 
	//for single cell
	size_t x,y,z;
	x=1;
	y=2;
	z=3;
	for(size_t ui=0;ui<12;ui++)
	{
		size_t idx;
		idx= v.getCellUniqueEdgeIndex(x,y,z,ui);
		
		size_t axis;
		size_t xN,yN,zN;
		v.getEdgeCell(idx,xN,yN,zN,axis);
		
		//if we ask for the cell, we should also 
		//get the index
		ASSERT(x == xN && y==yN && z==zN);	

		//TODO: Check that the axis of the edge was preserved (not the edge itself)
		ASSERT( axis == ui/4);
	}
	
	return true;	
}
*/

bool pointInPoly()
{
	vector<pair<float,float> > pts;
	//make a square
	pts.push_back(make_pair(0,0));	
//	pts.push_back(make_pair(0,0.5));	
	pts.push_back(make_pair(0,1));	
	pts.push_back(make_pair(1,0));	
	pts.push_back(make_pair(1,1));	

	//shuffle vertex positions
	std::random_shuffle(pts.begin(),pts.end());

	//Run test
	TEST(pointIn2DConvexPoly(0.5,0.5,pts),"Point-in-poly test");	// Inside
	TEST(!pointIn2DConvexPoly(1.5,0.5,pts),"Point-in-poly test");	//Outside
	TEST(!pointIn2DConvexPoly(1.5,1.5,pts),"Point-in-poly test");	//Diagonal

	return true;
}


bool runVoxelTests()
{
	bool wantAbort=false;
	voxelsWantAbort = &wantAbort;

	TEST(basicTests(),"basic voxel tests");
	TEST(simpleMath(), "voxel simple maths");	

	TEST(pointInPoly(),"point-in-poly tests");
//	TEST(edgeCountTests(), "voxel edge tests");	
	return true;	
}

#endif
