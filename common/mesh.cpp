/* 
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
#include "mesh.h"


#include <string>
#include <algorithm>
#include <limits>
#include <list>
#include <utility>
#include <deque>


using std::deque;
using std::make_pair;
using std::vector;
using std::string;
using std::endl;
using std::list;
using std::cerr;

enum
{
	MESH_LOAD_UNSPECIFIED_ERROR=1,
	MESH_LOAD_BAD_NODECOUNT,
	MESH_LOAD_BAD_ELEMENTCOUNT,
	MESH_LOAD_IS_INSANE,
	MESH_LOAD_ENUM_END
};

const char *MESH_LOAD_ERRS[] = {  "",
	"Missing error message. This is a bug, please report it",
	"Node count was different to number of present nodes",
	"Element count was less than number of present elements",
	"Mesh loaded, but failed to pass sanity checks"
};
			

const size_t PROGRESS_REDUCE=500;				


float signVal(unsigned int val)
{
	if (val &1)
		return 1;
	else
		return -1;
}

size_t findMaxLessThanOrEq(const vector< std::pair<size_t,size_t> > &v,
			size_t value)
{
	ASSERT(v.size());
	size_t curMax=v.begin()->first; 
	size_t curMaxOff=0;
	for(size_t ui=0;ui<v.size();ui++)
	{
		if(v[ui].first >  curMax && v[ui].first <=value)
			curMaxOff=ui;
	}

	return curMaxOff;
}


//   Recursive definition of determinate using expansion by minors.
float Determinant(float **a,int n)
{
	float det = 0;
	ASSERT( n > 1);

	//Fundamental 2x2 det.
	if (n == 2) 
		det = a[0][0] * a[1][1] - a[1][0] * a[0][1];
	else
       	{
		int i,j,j1,j2;
		//recurisve det
		det = 0;
		for (j1=0; j1<n; j1++) {
			float **m = new float*[n-1];
			for (i=0; i<n-1; i++)
				m[i] = new float[n-1];
			for (i=1; i<n; i++) {
				j2 = 0;
				for (j=0; j<n; j++) {
					if (j == j1)
						continue;
					m[i-1][j2] = a[i][j];
					j2++;
				}
			}
			det += signVal(2+j1) * a[0][j1] * Determinant(m,n-1);
			for (i=0; i<n-1; i++)
				delete[] m[i];
			delete[] m;
		}
	}
	return(det);
}

//Special case of the four by four determinant, useful for tetrahedrons
//|a0 a1 a2 1 |
//|b0 b1 b2 1 |
//|c0 c1 c2 1 |
//|d0 d1 d2 1 |
float fourDeterminant(const Point3D &a, const Point3D &b, const Point3D &c, const Point3D &d)
{
	float **rows;
	rows = new float*[4];
	//Malloc cols.
	for(unsigned int ui=0;ui<4;ui++)
		rows[ui] = new float[4];

	for(unsigned int ui=0;ui<3;ui++)
	{
		rows[0][ui] = a[ui];
		rows[1][ui] = b[ui];
		rows[2][ui] = c[ui];
		rows[3][ui] = d[ui];
	}

	for(unsigned int ui=0;ui<4;ui++)
		rows[ui][3] = 1;

	float v;
	v=Determinant(rows,4);

	
	for(unsigned int ui=0;ui<4;ui++)
		delete[] rows[ui];

	delete[] rows;

	return v;

}

//return the edge number for a triangle.
//To see this, draw a triangle, and going clockwise, label the edges 0, 1 and 2
//With the edge facing you, label the left corner with the edge number.
//Draw up the table mapping the edge that is formed by the vertices i,j; 
//that is this function.
unsigned int edgeIdx(unsigned int i,unsigned int j)
{
	ASSERT(i<3 && j < 3);
	switch(i+j)
	{
		case 1:
			return 1;
		case 2:
			return 0;
		case 3:
		       	return 2;
	}
	ASSERT(false);
	return -1;
}


//---- BEGIN This section under specific licence ---
// Copyright 2001, softSurfer (www.softsurfer.com)
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from its use.
// Users of this code must verify correctness for their application.
// R - ray; t, triangle (array of 3 pts)
// returns 
// 	-1 : degenerate case (triangle degen)
// 	0: No intersect
// 	1 : Single Intersection
// 	2: Edge intersection (co-planar)
// I - intersection of ray w triangle; if exists and is unique
int intersect_RayTriangle( const Point3D &rayStart, const Point3D &rayEnd, 
					Point3D *tri, Point3D &I )
{
    Point3D    u, v, n;             // triangle vectors
    Point3D    dir, w0, w;          // ray vectors



    // get triangle edge vectors and plane normal
    u = tri[1] - tri[0];
    v = tri[2] - tri[0];
    n = u.crossProd(v);             // cross product


    if (n.sqrMag() < (std::numeric_limits<float>::epsilon()) )
	return -1;                 // do not deal with this case, the triangle is degenerate

    n.normalise();

    dir = rayEnd - rayStart;             // ray direction vector


    //Check for ray-plane intersection point
    //--
    Point3D rv1,rv2;
    rv1 = rayStart - tri[0];
    rv2 = rayEnd - tri[0];

    //If the dot products do not flip, the ray cannot cross infinite plane
    float dp1 = rv1.dotProd(n);
    float dp2 = rv2.dotProd(n); 
    if(dp1*dp2 > 0) //(signs are the same --> ray is on one side of plane only)
	    return 0;
    else if(rv1.dotProd(n) < std::numeric_limits<float>::epsilon() &&
			   rv2.dotProd(n) < std::numeric_limits<float>::epsilon())

    {
	    //If the ray-ends -> vertex vectors have no component in the normal direction
	    //the ray is coplanar
	    return 2;
    }

    //Project the ray onto the plane to create intersection point
    //Solution is found by parameterising ray and solving for 
    //dot product with normal 
    I=rayStart-dir*rv1.dotProd(n)/dir.dotProd(n);


    //--

    // is I inside T? If so, then the dot product of each edge
    // with the ray from the edge start to the intersection will always
    // be in range [0-1]; otherwise there will be at least one that is negative
    float    uu, uv, vv, wu, wv, D;
    uu = u.dotProd(u);
    uv = u.dotProd(v);
    vv = v.dotProd(v);
    w = I - tri[0];
    wu = w.dotProd(u);
    wv = w.dotProd(v);
    D = uv * uv - uu * vv;

    // get and test parametric coords
    float s, t;
    s = (uv * wv - vv * wu) / D;
    if (s < 0.0 || s > 1.0)        // I is outside T
	return 0;
    t = (uv * wu - uu * wv) / D;
    if (t < 0.0 || (s + t) > 1.0)  // I is outside T
	return 0;

    return 1;                      // I is in T
}

//---- END This section under specific licence ---

//Creates a list of pair,vector of point indices that are within a certain
// tolerance radius of one another. Output first value in list will be strictly increasing. (i.e. it->first < (it+1)->first, regardless of it position)

//FIXME: This algorithm is a poor effort. It just picks a semi-random point, 
//then nicks everything within the capture radius that was not nicked before.
// this will work if the adjacency points are *close*, and well separated, but not otherwise.

//TODO: Unify these two overloads!
//--
void findNearVerticies(float tolerance, const vector<Point3D> &ptVec,
		vector<std::pair<size_t,vector<size_t> > > &clusterList)
{
	ASSERT(!clusterList.size());
	
	vector<bool> marked;
	marked.resize(ptVec.size(),false);
	//Try to find the common points
	for(size_t ui=0;ui<ptVec.size();ui++)
	{
		vector<size_t> curClustered;

		//FIXME: replace with KD tree based search, or some other smart structure
		for(size_t uj=0;uj<ptVec.size();uj++)
		{
			if(ui==uj|| marked[uj])
				continue;

			if(ptVec[ui].sqrDist(ptVec[uj]) < tolerance)
			{
				curClustered.push_back(uj);
				marked[uj]=true;
			}

		}
	
		//If we found any points, then this point has also not been seen before
		// by handshaking lemma
		if(curClustered.size())
		{
			marked[ui]=true;
			clusterList.push_back(std::make_pair(ui,curClustered));
			curClustered.clear();
		}

	}

}

void findNearVerticies(float tolerance, const vector<Point3D> &ptVec,
		std::list<std::pair<size_t,vector<size_t> > > &clusterList)
{
	ASSERT(clusterList.empty());
	
	vector<bool> marked;
	marked.resize(ptVec.size(),false);
	//Try to find the common points
	for(size_t ui=0;ui<ptVec.size();ui++)
	{
		vector<size_t> curClustered;

		//FIXME: replace with KD tree based search, or some other smart structure
		for(size_t uj=0;uj<ptVec.size();uj++)
		{
			if(ui==uj|| marked[uj])
				continue;

			if(ptVec[ui].sqrDist(ptVec[uj]) < tolerance)
			{
				curClustered.push_back(uj);
				marked[uj]=true;
			}

		}
	
		//If we found any points, then this point has also not been seen before
		// by handshaking lemma
		if(curClustered.size())
		{
			marked[ui]=true;
			clusterList.push_back(std::make_pair(ui,curClustered));
			curClustered.clear();
		}

	}
}
//--



void Mesh::print(std::ostream &o) const
{	
	o << " Node count :" << nodes.size() << endl;
	o << " Point count :" << points.size() << endl;
	o << " Line count :" << tetrahedra.size() << endl;
	o << " Triangle count :" << triangles.size() << endl;
	o << " Tetrahedra count :" << tetrahedra.size() << endl;


	BoundCube b;
	b.setBounds(nodes);
	o << "Bounding box:" << endl;
	o << b << endl;
	
	Point3D centroid(0,0,0);
	
	for(size_t ui=0;ui<nodes.size();ui++)
		centroid+=nodes[ui];

	centroid*=1.0/(float)nodes.size();

	o << "Centroid:" << endl;
	o << centroid << endl;
}
//Input must be sorted and unique.
void Mesh::killOrphanNodes(const vector<size_t> &orphans)
{
#ifdef HAVE_CPP11
	ASSERT(std::is_sorted(orphans.begin(),orphans.end()));
#endif
	ASSERT(std::adjacent_find(orphans.begin(),orphans.end()) == orphans.end())

	ASSERT(isSane());


	vector<size_t> offsets;
	offsets.resize(nodes.size());
	size_t curOrphan=0;
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		while(curOrphan < orphans.size() && 
				orphans[curOrphan] <=ui)
			curOrphan++;
		offsets[ui]=curOrphan;
	}

	//renumber the points
	vector<size_t>::iterator itJ;
	for(size_t ui=0;ui<points.size();ui++)
		points[ui]-=offsets[points[ui]];
	
	//renumber the lines 
	for(size_t ui=0;ui<lines.size();ui++)
		for(size_t uj=0;uj<2;uj++)
			lines[ui].p[uj]-=offsets[lines[ui].p[uj]];

	//renumber the triangles 
	for(size_t ui=0;ui<triangles.size();ui++)
	{
		for(size_t uj=0;uj<3;uj++)
		{
			ASSERT(triangles[ui].p[uj] -
			offsets[triangles[ui].p[uj]]< nodes.size());
			triangles[ui].p[uj]-=offsets[triangles[ui].p[uj]];
		}
	}
	
	//renumber the tetrahedra
	for(size_t ui=0;ui<tetrahedra.size();ui++)
		for(size_t uj=0;uj<4;uj++)
			tetrahedra[ui].p[uj]-=offsets[tetrahedra[ui].p[uj]];


	//FIXME: Not efficient
	vector<Point3D> newNodes;
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		if(!binary_search(orphans.begin(),orphans.end(),ui))
			newNodes.push_back(nodes[ui]);
	}
	nodes.swap(newNodes);

	//Even if each object has every vertex with its own node, not
	// shared with any other, it cannot exceed this.
	ASSERT(nodes.size() <= (3*triangles.size() + 
		2*lines.size() + 4*tetrahedra.size() + points.size()));
}

bool Mesh::tetrahedronDegenerate(unsigned int tet) const
{
	//Compute the following determinant:
	//if zero --> degenerate
	//	       |x1 y1 z1 1|
	//        D0 = |x2 y2 z2 1|
	//             |x3 y3 z3 1|
	//             |x4 y4 z4 1|

	return  fourDeterminant(nodes[tetrahedra[tet].p[0]],
			nodes[tetrahedra[tet].p[1]],
			nodes[tetrahedra[tet].p[2]],
			nodes[tetrahedra[tet].p[3]]) < std::numeric_limits<float>::epsilon();
}


bool Mesh::pointInTetrahedron(unsigned int tet, const Point3D &p) const
{
	//Compute the following determinants;
	//if there is a sign change, then 
	//	       |x1 y1 z1 1|
	//        D0 = |x2 y2 z2 1|
	//             |x3 y3 z3 1|
	//             |x4 y4 z4 1|
	
	float f;
	f = fourDeterminant(nodes[tetrahedra[tet].p[0]],
				nodes[tetrahedra[tet].p[1]],
				nodes[tetrahedra[tet].p[2]],
				nodes[tetrahedra[tet].p[3]]);

	//Ensure that we do not get the zero case.
	ASSERT(!tetrahedronDegenerate(tet));

	bool positive;
       	positive= f>0;
	

	//
	//             |x  y  z  1|
	//        D1 = |x2 y2 z2 1|
	//             |x3 y3 z3 1|
	//             |x4 y4 z4 1|
	f = fourDeterminant(p,
				nodes[tetrahedra[tet].p[1]],
				nodes[tetrahedra[tet].p[2]],
				nodes[tetrahedra[tet].p[3]]);
	if(f < 0 &&  positive)
		return false;
	//
	//             |x1 y1 z1 1|
	//        D2 = |x  y  z  1|
	//             |x3 y3 z3 1|
	//             |x4 y4 z4 1|
	f = fourDeterminant(
				nodes[tetrahedra[tet].p[0]],
				p,
				nodes[tetrahedra[tet].p[2]],
				nodes[tetrahedra[tet].p[3]]);
	if(f < 0 &&  positive)
		return false;
	//
	//             |x1 y1 z1 1|
	//        D3 = |x2 y2 z2 1|
	//             |x  y  z  1|
	//             |x4 y4 z4 1|
	f = fourDeterminant(	nodes[tetrahedra[tet].p[0]],
				nodes[tetrahedra[tet].p[1]],
				p,
				nodes[tetrahedra[tet].p[3]]);
	if(f < 0 &&  positive)
		return false;
	//
	//             |x1 y1 z1 1|
	//        D4 = |x2 y2 z2 1|
	//             |x3 y3 z3 1|
	//             |x  y  z  1|
	f = fourDeterminant(	nodes[tetrahedra[tet].p[0]],
				nodes[tetrahedra[tet].p[1]],
				nodes[tetrahedra[tet].p[2]],
				p);

	if(f < 0 &&  positive)
		return false;

	return true;
}

//Tell us if the two triangles are indeed the same
bool Mesh::sameTriangle(unsigned int ui, unsigned int uj) const
{
	vector<unsigned int> t1,t2;

	t1.resize(3);
	t2.resize(3);
	for(unsigned int idx=0;idx<3;idx++)
	{
		t1[idx]= triangles[ui].p[idx];
		t2[idx]= triangles[uj].p[idx];
	}

	std::sort(t1.begin(),t1.end());
	std::sort(t2.begin(),t2.end());

	return std::equal(t1.begin(),t1.end(),t2.begin());

}

bool Mesh::sameTriangle(const TRIANGLE &t1, const TRIANGLE &t2) 
{
	vector<unsigned int> ta,tb;

	ta.resize(3);
	tb.resize(3);
	for(unsigned int idx=0;idx<3;idx++)
	{
		ta[idx]= t1.p[idx];
		tb[idx]= t2.p[idx];
	}

	std::sort(ta.begin(),ta.end());
	std::sort(tb.begin(),tb.end());

	return std::equal(ta.begin(),ta.end(),tb.begin());

}

bool Mesh::isSane(bool output, std::ostream &outStr) const
{
	//Check sanity
	for(size_t ui=0;ui<tetrahedra.size();ui++)
	{
		//Tetrahedra has unique vertices
		for(unsigned int uj=0;uj<4; uj++)
		{
			for(unsigned int uk=0;uk<4;uk++)
			{
				if(uk == uj)
					continue;

				if( tetrahedra[ui].p[uj] == tetrahedra[ui].p[uk])
				{
					if(output)
						outStr << "It's INSANE. " << __LINE__ << std::endl;
					return false;
				}
			}
		
			//tetrahedra point to valid node
			if(tetrahedra[ui].p[uj] > nodes.size())
			{
				if(output)
					outStr << "It's INSANE. " << __LINE__ << std::endl;
				return false;
			}
		}

	}
	
	for(size_t ui=0;ui<triangles.size();ui++)
	{
		//triangle vertices unique test
		for(unsigned int uj=0;uj<3; uj++)
		{
			for(unsigned int uk=0;uk<3;uk++)
			{
				if(uk == uj)
					continue;

				if( triangles[ui].p[uj] == triangles[ui].p[uk])
				{	
					if(output)
					{
						outStr << "It's INSANE. " << __LINE__ << std::endl;
						outStr << "vertex  " << uj << " and " << uk
							<< " of triangle " << ui << "not unique" << endl;
						outStr << triangles[ui].p[uj] << " node is duplicated" << std::endl;
					}
					return false;
				}
			}
		
			//triangle points to valid node
			if(triangles[ui].p[uj] > nodes.size())
			{
				if(output)
					outStr << "It's INSANE. " << __LINE__ << std::endl;
				return false;
			}
		}

	}

	for(size_t ui=0;ui<lines.size();ui++)
	{
		//lines have no common vertices
		for(unsigned int uj=0;uj<2; uj++)
		{
			for(unsigned int uk=0;uk<2;uk++)
			{
				if(uk == uj)
					continue;

				if( lines[ui].p[uj] == lines[ui].p[uk])
				{	
					if(output)	
						outStr << "It's INSANE. " << __LINE__ << std::endl;
					return false;
				}
			}
		
			//lines point to valid node
			if(lines[ui].p[uj] > nodes.size())
			{	
				if(output)	
					outStr << "It's INSANE. " << __LINE__ << std::endl;
				return false;
			}
		}

	}


	//Check that we have enough nodes to support each primitive type
	if(nodes.size() < 4 && tetrahedra.size())
	{	
		if(output)	
			outStr << "It's INSANE. " << __LINE__ << std::endl;
		return false;
	}
	if(nodes.size() < 3 && triangles.size())
	{
		if(output)	
			outStr << "It's INSANE. " << __LINE__ << std::endl;
		return false;
	}
	if(nodes.size() < 2 && lines.size())
	{	
		if(output)
			outStr << "It's INSANE. " << __LINE__ << std::endl;
		return false;
	}

	for(size_t ui=0; ui<tetrahedra.size();ui++)
	{


		vector<size_t> tris,lines;
		//Lets look for triangles and lines attached to this 
		//(really we just want the triangles)
		getAttachedComponents(ui,tris,lines);

		if(tris.size() > 4)
		{
			if(output)
			{
				//Too many triangles
				outStr << "INSANE: Tetrahedron " << ui << " has more than 4 attached triangles.." << std::endl;
			}
			return false;
		}

		if(lines.size() > 6)
		{
			if(output)
			{
				//Too many lines
				outStr << "INSANE: Tetrahedron " << ui << " has more than 6 attached lines.." << std::endl;
			}
			return false;
		}

	}
	return true;
}



void Mesh::getDisconnectedTets(vector<size_t> &tets) const
{
	using std::list;

	//In a fully connected mesh, every tetrahedron must be 
	//matched to at least one other tetrahedron by  a face

	//By definition, this is true for a zero or singly sized mesh	
	if(tetrahedra.size() <=1)
		return;

	//Create a lookup table of vertices -> tetrahedra
	vector<list<size_t> > tetLookup;
	tetLookup.resize(nodes.size());
	for(size_t ui=0; ui<tetrahedra.size();ui++)
	{
		for(unsigned int uj=0;uj<4;uj++)
			tetLookup[tetrahedra[ui].p[uj]].push_back(ui);
	}

	//Now loop through the tetrahedra, and using the previously constructed
	//table, do a lookup to check that there is exactly one or zero attached 
	//tetrahedra to each face, and that at least one face has an attached tetrahedron
	
	//Map that tells us the vertex indices that are incident to each face
	unsigned int faceMap[4][3] = { 
					{0,1,3},
					{0,2,3},
					{1,2,3},
					{0,1,2}
					};


	list<size_t> connectedMap;
	for(size_t ui=0; ui<tetrahedra.size();ui++)
	{


		bool faceConnected;
		faceConnected=false;
		for(unsigned int uj=0;uj<4;uj++)
		{
			
			//Get the tetrahedra coincident to an initial vertex
			//on a given face
			connectedMap = tetLookup[tetrahedra[ui].p[faceMap[uj][0]]];

			ASSERT(connectedMap.size());
			for(unsigned int uk=1;uk<3;uk++)
			{
				size_t nextVert;
				nextVert=tetrahedra[ui].p[faceMap[uj][uk]];
				//do a knockout of all tetrahedra who were initially
				//connected, but are not connected to the current vertex
				for(list<size_t>::iterator it=connectedMap.begin(); 
						 	 it!=connectedMap.end();++it)
				{
					ASSERT(tetLookup[nextVert].size());
					if(find(tetLookup[nextVert].begin(),tetLookup[nextVert].end(),(*it)) == 
						tetLookup[nextVert].end())
					{
						it=connectedMap.erase(it);
						--it;
					}
				}
			}
			
			
			if(!(connectedMap.size() == 2 ||
				connectedMap.size() ==1))
			{
				//this tetrahedron is multiply connected. 
				//(remember, that itself is included in the above test)
				//Thats screwed up.
				tets.push_back(ui);
			}

			if(connectedMap.size() == 2)
				faceConnected=true;
				


		}

		if(!faceConnected)
		{
			tets.push_back(ui);
		}
	}

	return;	
}

bool Mesh::isTetFullyConnected(unsigned int &badTet) const
{
	using std::list;

	//In a fully connected mesh, every tetrahedron must be 
	//matched to at least one other tetrahedron by  a face

	//By definition, this is true for a zero or singly sized mesh	
	if(tetrahedra.size() <=1)
		return true;

	//Create a lookup table of vertices -> tetrahedra
	vector<list<size_t> > tetLookup;
	tetLookup.resize(nodes.size());
	for(size_t ui=0; ui<tetrahedra.size();ui++)
	{
		for(unsigned int uj=0;uj<4;uj++)
			tetLookup[tetrahedra[ui].p[uj]].push_back(ui);
	}

	//Now loop through the tetrahedra, and using the previously constructed
	//table, do a lookup to check that there is exactly one or zero attached 
	//tetrahedra to each face, and that at least one face has an attached tetrahedron
	
	//Map that tells us the vertex indices that are incident to each face
	unsigned int faceMap[4][3] = { 
					{0,1,3},
					{0,2,3},
					{1,2,3},
					{0,1,2}
					};


	list<size_t> connectedMap;
	for(size_t ui=0; ui<tetrahedra.size();ui++)
	{


		bool faceConnected;
		faceConnected=false;
		for(unsigned int uj=0;uj<4;uj++)
		{
			
			//Get the tetrahedra coincident to an initial vertex
			//on a given face
			connectedMap = tetLookup[tetrahedra[ui].p[faceMap[uj][0]]];

			ASSERT(connectedMap.size());
			for(unsigned int uk=1;uk<3;uk++)
			{
				size_t nextVert;
				nextVert=tetrahedra[ui].p[faceMap[uj][uk]];
				//do a knockout of all tetrahedra who were initially
				//connected, but are not connected to the current vertex
				for(list<size_t>::iterator it=connectedMap.begin(); 
						 	 it!=connectedMap.end();++it)
				{
					ASSERT(tetLookup[nextVert].size());
					if(find(tetLookup[nextVert].begin(),tetLookup[nextVert].end(),(*it)) == 
						tetLookup[nextVert].end())
					{
						it=connectedMap.erase(it);
						--it;
					}
				}
			}
			
			
			if(!(connectedMap.size() == 2 ||
				connectedMap.size() ==1))
			{
				//this tetrahedron is multiply connected. 
				//(remember, that itself is included in the above test)
				//Thats screwed up.
				badTet=ui;
				return false;
			}

			if(connectedMap.size() == 2)
			{
				faceConnected=true;
				break;
			}
				


		}

		if(!faceConnected)
		{
			badTet=ui;
			return false;
		}
	}

	return true;	
}

void Mesh::removeDuplicateTris()
{
	ASSERT(isSane());

	using std::list;
	vector<size_t> dups;

	//Create a listing of all the triangles incident to each node
	vector<list<unsigned int> > vl;
	vl.resize(nodes.size());

	for(unsigned int ui=0;ui<triangles.size();ui++)
	{
		for(unsigned int uj=0;uj<3;uj++)
			vl[triangles[ui].p[uj]].push_back(ui);
	}

	for(unsigned int ui=0;ui<vl.size();ui++)
	{
		for(list<unsigned int>::iterator it=vl[ui].begin();
				it!=vl[ui].end();++it)
		{
			//Examine the triangle that is coincident on this vertex, then
			//see if the other triangles on this vertex are duplicates 
			for(list<unsigned int>::iterator itJ=it;
					itJ!=vl[ui].end();++itJ)
			{
				if(itJ == it)
					continue;

				if(sameTriangle(*it,*itJ))
				{
					if(std::find(dups.begin(),dups.end(),*itJ) == dups.end())
						dups.push_back(*itJ);
				}
			}
			
		}
	}
	
	for(unsigned int ui=dups.size();ui--;)
	{
		std::swap(triangles[dups[ui]],triangles.back());
		triangles.pop_back();
	}
}

void Mesh::mergeDuplicateVertices(float tol)
{
	using std::list;
	using std::pair;
	vector<size_t> thisDup;
	vector<pair<size_t,vector<size_t> > > dups;

	//Find the duplicates 
	// placing duplicates into a list of sorted vectors of duplicate indices
	findNearVerticies(tol,nodes,dups);
	for(size_t ui=0;ui<dups.size();ui++)
	{
		std::sort(dups[ui].second.begin(),
				dups[ui].second.end());
	}

	for(vector<pair<size_t,vector<size_t> > >::iterator it=dups.begin();
		it!=dups.end();++it)
	{
		//replace the points
		vector<size_t>::iterator itJ;
		
		for(size_t ui=0;ui<points.size();ui++)
		{
			itJ=find(it->second.begin(),it->second.end(),points[ui]);
			if(itJ !=it->second.end())
				points[ui]=it->first;
		}
		
		//replace the lines 
		for(size_t ui=0;ui<lines.size();ui++)
		{
			for(size_t uj=0;uj<2;uj++)
			{
				itJ=find(it->second.begin(),it->second.end(),lines[ui].p[uj]);
				if(itJ !=it->second.end())
					lines[ui].p[uj]=it->first;
			}
		}
		
		//replace the triangles 
		for(size_t ui=0;ui<triangles.size();ui++)
		{
			for(size_t uj=0;uj<3;uj++)
			{
				itJ=find(it->second.begin(),it->second.end(),triangles[ui].p[uj]);
				if(itJ !=it->second.end())
					triangles[ui].p[uj]=it->first;
			}
		}
	
		//replace the tetrahedra
		for(size_t ui=0;ui<tetrahedra.size();ui++)
		{
			for(size_t uj=0;uj<4;uj++)
			{
				itJ=find(it->second.begin(),it->second.end(),tetrahedra[ui].p[uj]);
				if(itJ !=it->second.end())
					tetrahedra[ui].p[uj]=it->first;
			}
		}
	}
	ASSERT(isSane());	


	//obtain the set of vertices that have now been orphaned
	vector<size_t> toRemove;
	for(vector<pair<size_t,vector<size_t> > >::iterator it=dups.begin();
		it!=dups.end();++it)
	{
		for(size_t ui=0;ui<it->second.size();ui++)
			toRemove.push_back(it->second[ui]);
	}

	std::sort(toRemove.begin(),toRemove.end());
	//hokay, so all we need to do is remove orphans
	killOrphanNodes(toRemove);
	
	ASSERT(isSane());	
}


void Mesh::killOrphanNodes()
{
	vector<bool> referenced(nodes.size(),false);
	for(size_t ui=0;ui<points.size();ui++)
		referenced[points[ui]] =true;

	for(size_t ui=0;ui<lines.size();ui++)
		for(size_t uj=0;uj<2;uj++)
		referenced[lines[ui].p[uj]] =true;

	for(size_t ui=0;ui<triangles.size();ui++)
		for(size_t uj=0;uj<3;uj++)
			referenced[triangles[ui].p[uj]] =true;

	for(size_t ui=0;ui<tetrahedra.size();ui++)
		for(size_t uj=0;uj<4;uj++)
			referenced[tetrahedra[ui].p[uj]] =true;

	vector<size_t> orphans;

	for(size_t ui=0;ui<referenced.size();ui++)
	{
		if(!referenced[ui])
		{
			orphans.push_back(ui);
		}
	}

	if(orphans.size())
		killOrphanNodes(orphans);
	ASSERT(isSane());
}


unsigned int Mesh::numDupTris() const
{
	ASSERT(isSane());

	using std::list;
	vector<size_t> dups;

	//Create a listing of all the triangles incident to each node
	vector<list<unsigned int> > vl;
	vl.resize(nodes.size());

	for(unsigned int ui=0;ui<triangles.size();ui++)
	{
		for(unsigned int uj=0;uj<3;uj++)
			vl[triangles[ui].p[uj]].push_back(ui);
	}

	for(unsigned int ui=0;ui<vl.size();ui++)
	{
		for(list<unsigned int>::iterator it=vl[ui].begin();
				it!=vl[ui].end();++it)
		{
			//Examine the triangle that is coincident on this vertex, then
			//see if the other triangles on this vertex are duplicates 
			for(list<unsigned int>::iterator itJ=it;
					itJ!=vl[ui].end();++itJ)
			{
				if(itJ == it)
					continue;

				if(sameTriangle(*it,*itJ))
				{
					if(std::find(dups.begin(),dups.end(),*itJ) == dups.end())
						dups.push_back(*itJ);
				}
			}
			
		}
	}

	return dups.size();	
}

unsigned int Mesh::numDupVertices(float tolerance) const
{
	float sqrTol;
	sqrTol = tolerance*tolerance;

	
	unsigned int numDups=0;

	//TODO: Non brute force approach (k3d-mk2)
#pragma omp parallel for reduction(+:numDups)
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		for(size_t uj=0;uj<nodes.size();uj++)
		{
			if(ui == uj)
				continue;

			if (nodes[ui].sqrDist(nodes[uj]) < sqrTol)
				numDups++;
		}
	}

	return numDups;
}

void Mesh::clear()
{
	nodes.clear();
	physGroupNames.clear();
	tetrahedra.clear();
	triangles.clear();
	lines.clear();
	points.clear();
}

void Mesh::removeStrayTris()
{
	//NOTE: This algorithm is non-functional in the case whereby
	//we have triangles where two other triangles have matched edges against
	//a "primary" triangle. This is a "non-conformal" mesh or somesuch, and I don't
	//like them.
	
	//This algorithm is  also extremely inefficient
	//Could be better off constructing an edge list
	//and then querying that.
	vector<size_t> trianglesToKill;

	unsigned int triKillCount;
	do
	{
		triKillCount=0;
		std::cerr << "Pass..." << std::endl;
		trianglesToKill.clear();
		#pragma omp parallel for
		for(size_t ui=0;ui<triangles.size();ui++)
		{
			bool coincident[3];
			coincident[0]=coincident[1]=coincident[2]=false;
		
			for(size_t uj=0;uj<triangles.size();uj++)
			{
				//Disallow self-coincidence matching
				if(ui == uj)
					continue;

				for(unsigned int uk=0;uk<3;uk++)
				{
					//opposite vertex, winding in clockwise fashion
					unsigned int nextVert;
					nextVert=(uk+1)%3;

					for(unsigned int um=0;um<3;um++)
					{
						unsigned int nextVertTwo;

						nextVertTwo = (um+1)%3;

						//Check reverse co-incidence
						if(
							(triangles[ui].p[nextVert] == triangles[uj].p[nextVertTwo] && 
							  triangles[ui].p[uk] == triangles[uj].p[um]) ||
							(triangles[ui].p[nextVert] == triangles[uj].p[um] &&
							  triangles[ui].p[uk] == triangles[uj].p[nextVertTwo])
							)

						{
							coincident[edgeIdx(uk,nextVert)]=true;

						}
						
					}

					
				}

				if(coincident[0] && coincident[1] && coincident[2])
					break;

			}
		
		
			//Check to see if triangle is fully coherent int mesh
			if(!coincident[0] || !coincident[1] || !coincident[2])
			{
				#pragma omp critical
				trianglesToKill.push_back(ui);
			}
		
		}

		//Reverse sort
		std::sort(trianglesToKill.begin(),trianglesToKill.end(),std::less<size_t>());

		
		for(unsigned int ui=0;ui<trianglesToKill.size();ui++)
		{
			//Move triangle to kill to back of vector
			std::swap(triangles[trianglesToKill[ui]],triangles.back());
			//pop vector
			triangles.pop_back();

			triKillCount++;
		}

		std::cerr << "Killed" << triKillCount << " stray triangles" << std::endl;
	} while(triKillCount);
}


//Re-surface the mesh by placing triangles over the top of tetrahedra that are not sharing
//a face
//This could be a lot more efficient
void Mesh::resurface(unsigned int newPhys)
{
	ASSERT(isSane());

	//Loop through the tetrahedra in the mesh
	//to determine if it is connected to the 
	using std::list;

	//In a fully connected mesh, every tetrahedron must be 
	//matched to at least one other tetrahedron by  a face

	//Create a lookup table of vertices -> tetrahedra
	//and vertices->triangles
	vector<list<size_t> > tetLookup;
	vector<list<size_t> > triLookup;
	
	tetLookup.resize(nodes.size());
	for(size_t ui=0; ui<tetrahedra.size();ui++)
	{
		for(unsigned int uj=0;uj<4;uj++)
			tetLookup[tetrahedra[ui].p[uj]].push_back(ui);
	}
	
	triLookup.resize(nodes.size());
	for(size_t ui=0; ui<triangles.size();ui++)
	{
		for(unsigned int uj=0;uj<3;uj++)
			triLookup[triangles[ui].p[uj]].push_back(ui);
	}	
	
	//Now loop through the tetrahedra, and using the previously constructed
	//table, do a lookup to check that there is exactly one or zero attached 
	//tetrahedra to each face, and that at least one face has an attached tetrahedron
	
	//Map that tells us the vertex indices that are incident to each face
	unsigned int faceMap[4][3] = { 
					{0,1,3},
					{0,2,3},
					{1,2,3},
					{0,1,2}
					};

	vector<std::pair<size_t,unsigned int> > triMaps;

	list<size_t> connectedMap;
	std::cerr << "Examining " << tetrahedra.size() << " tetrahedra " << std::endl;
	//Draw a progress bar
	unsigned int lastFrac=0;
	std::cerr << std::endl << "|";
	for(unsigned int ui=0; ui<100; ui++)
		std::cerr << ".";
	std::cerr << "| 100%" << std::endl << "|.";

	for(size_t ui=0; ui<tetrahedra.size();ui++)
	{

		for(unsigned int uj=0;uj<4;uj++)
		{
			//Get the tetrahedra coincident to an initial vertex
			//on a given face
			connectedMap = tetLookup[tetrahedra[ui].p[faceMap[uj][0]]];

			ASSERT(connectedMap.size());
			for(unsigned int uk=1;uk<3;uk++)
			{
				size_t nextVert;
				nextVert=tetrahedra[ui].p[faceMap[uj][uk]];
				//do a knockout of all tetrahedra who were initially
				//connected, but are not connected to the current vertex
				for(list<size_t>::iterator it=connectedMap.begin(); 
						 	 it!=connectedMap.end();++it)
				{
					ASSERT(tetLookup[nextVert].size());
					if(find(tetLookup[nextVert].begin(),tetLookup[nextVert].end(),(*it)) == 
						tetLookup[nextVert].end())
					{
						it=connectedMap.erase(it);
						--it;
					}
				}
			}
			


			//Should either face another tet, or be exposed	
			//note that itself is counted in the connectedMap
			ASSERT((connectedMap.size() == 2 ||
				connectedMap.size() ==1));

			if(connectedMap.size() == 1)
			{
				//OK, the only tetrahedron that shares
				//this face is itself, so it does not share
				//a face with another tet.
				//However it could be exposed
				//or covered by an existing triangle


				//See if the triangles are attached to this face
				vector<size_t> tetFaceNodes;
				tetFaceNodes.clear();
				for(unsigned int uk=0;uk<3;uk++)
					tetFaceNodes.push_back(tetrahedra[ui].p[faceMap[uj][uk]]);

				std::sort(tetFaceNodes.begin(),tetFaceNodes.end());


				vector<size_t> attachedTris;
				//Loop over each vertex on this face, creating a list of incident
				//triangles to this vertex
				for(unsigned int uk=0;uk<3;uk++)
				{
					size_t vertex;
					vertex = tetrahedra[ui].p[faceMap[uj][uk]];
					for(list<size_t>::iterator it=triLookup[vertex].begin();
							it!=triLookup[vertex].end();++it)
					{
						//Push on list of attached triangles, if not prev.
						//seen
						if(find(attachedTris.begin(),attachedTris.end(),
									*it) == attachedTris.end())
							attachedTris.push_back(*it);
					}
				}

				//So now we have  list of triangles that are incident to
				//any vertex on this face. now, we need to look through these
				//to determine if any triangle is incident to this face
				vector<size_t> triNodes;
				triNodes.resize(3);
				bool triClothedFace;
				triClothedFace=false;
				for(unsigned int uk=0;uk<attachedTris.size();uk++)
				{
					for(unsigned int um=0;um<3;um++)
						triNodes[um] = triangles[attachedTris[uk]].p[um];

					std::sort(triNodes.begin(),triNodes.end());

					if(std::equal(triNodes.begin(),triNodes.end(),tetFaceNodes.begin()))
					{
						triClothedFace=true;
						break;
					}
				}

				if(!triClothedFace)
					triMaps.push_back(std::make_pair(ui,uj));

			}
				
		}


		if((unsigned int)(((float)ui*100.0f)/(float)tetrahedra.size()) > lastFrac)
		{
			std::cerr << ".";
			lastFrac++;
		}	
	}

	while(lastFrac++ < 100)
		std::cerr << ".";
	std::cerr << "|";	


	ASSERT(triMaps.size() < tetrahedra.size());
	//Trimaps now holds the mapping of all the tetrahedra that are exposed
	//but have no covering triangle. Let us apply a surface to this triangle
	//using the physical group specified above
	std::cerr << "Found " << triMaps.size() << " uncovered tetrahedra " << std::endl;	

	vector<std::pair<unsigned int, size_t> > curPhys;
	getCurPhysGroups(curPhys);
	std::cerr << "DEBUG : Found " << curPhys.size() << " physical groups " << std::endl;
	for(unsigned int ui=0;ui<curPhys.size(); ui++)
		std::cerr << "\t" << curPhys[ui].first << " : " << curPhys[ui].second << std::endl;


	BoundCube nakedTetsBound;
	nakedTetsBound.setInverseLimits();

	if(triMaps.size())
	{
		nakedTetsBound.setBounds(nodes[tetrahedra[triMaps[0].first].p[0]],
					nodes[tetrahedra[triMaps[0].first].p[1]]);
		for(unsigned int ui=0;ui<triMaps.size();ui++)
		{

			for(unsigned int uj=0;uj<4; uj++)
				nakedTetsBound.expand(nodes[tetrahedra[triMaps[ui].first].p[uj]]);
		}
	}
	std::cerr << "Bounding box : " << nakedTetsBound << std::endl;

	triangles.reserve(triangles.size()+triMaps.size());
	for(unsigned int ui=0;ui<triMaps.size();ui++)
	{
		//Look up the specified tetrahedron in the map
		//and the associated face. Use this to construct
		//the new triangle

		TRIANGLE t;

		t.p[0] =tetrahedra[triMaps[ui].first].p[faceMap[triMaps[ui].second][0]];
		t.p[1] =tetrahedra[triMaps[ui].first].p[faceMap[triMaps[ui].second][1]];
		t.p[2] =tetrahedra[triMaps[ui].first].p[faceMap[triMaps[ui].second][2]];

		t.physGroup=newPhys;
		triangles.push_back(t);
	}
	
}

void Mesh::setTriangleMesh(const std::vector<float> &ptsX, 
		const std::vector<float> &ptsY, const std::vector<float> &ptsZ)
{
	//Incoming data streams should describe triangles
	ASSERT(ptsX.size() == ptsY.size() && ptsY.size() == ptsZ.size());
	ASSERT(ptsX.size() %3 == 0);

	clear();


	vector<Point3D> ptVec;
	ptVec.resize(ptsX.size());
#pragma omp parallel for 
	for(size_t ui=0;ui<ptsX.size();ui++)
		ptVec[ui]=Point3D(ptsX[ui],ptsY[ui],ptsZ[ui]);

	
	const float MAX_SQR_RAD=0.001f;
	std::list<std::pair<size_t,vector<size_t> > > clusterList;
	findNearVerticies(MAX_SQR_RAD,ptVec,clusterList);


	//FIXME: This is totally inefficient.
	
	//Now, we have a vector of pts, each group of 3 corresponding to 1 triangle
	//and we have the mapping for the new triangles
       	vector<size_t > triangleMapping;

	triangleMapping.resize(ptVec.size());
#pragma omp parallel for
	for(size_t ui=0;ui<ptVec.size(); ui++)
		triangleMapping[ui]=ui;

	//Now run through the original mapping, and generate the new modified mapping.
	//this maps old point -> "rally pt"
	for(std::list<std::pair<size_t, vector<size_t> > >::iterator it=clusterList.begin();it!=clusterList.end();++it)
	{
		for(size_t uj=0;uj<it->second.size();uj++)
			triangleMapping[it->second[uj]]=it->first;
	}


	//now we have to do an additional step. When we create the new node vector, we are going to exclude points
	//that are no longer referenced. So, to do this (inefficiently), we need to know how many times each point was referenced
	//if it was referenced zero times, we have to modify the triangle mapping for any nodes of higher index
	

	vector<size_t> refCount;
	refCount.resize(ptVec.size(),0);
	for(size_t ui=0;ui<triangleMapping.size();ui++)
		refCount[triangleMapping[ui]]++;

	//modify the triangle mapping such that it points from ptVec->nodeVec (ie the indices of the pts, after dropping unreferenced pts)
	size_t delta=0;
	vector<size_t> numPtsDropped;
	for(size_t ui=0;ui<refCount.size();ui++)
	{
		numPtsDropped.push_back(delta);
		if(!refCount[ui])
		{
			delta++;
			continue;
		}
		nodes.push_back(ptVec[ui]);
	}


	//Build the triangle vector
	for(size_t ui=0;ui<triangleMapping.size()/3; ui++)
	{
		size_t offset;
		offset=ui*3;
		for(size_t uj=0;uj<3;uj++)
		{
			TRIANGLE t;

			//Build the triangle from the triangle mapping, accounting for the shift due
			//to dropped points that have been "clustered"
			t.p[0]=triangleMapping[offset]-numPtsDropped[triangleMapping[offset]] ;
			t.p[1]=triangleMapping[offset+1] - numPtsDropped[triangleMapping[offset+1]];
			t.p[2]=triangleMapping[offset+2]- numPtsDropped[triangleMapping[offset+2]];
			triangles.push_back(t);
		}
	}

	std::cerr << "Input size of " << ptVec.size() << std::endl;
	std::cerr << "found " << clusterList.size() << " shared nodes"  << std::endl;

	ASSERT(isSane());

	std::cerr << "Appears to be sane?? " << std::endl;
}

/*
unsigned int Mesh::loadGmshMesh(const char *meshFile, unsigned int &curLine, bool allowBadMeshes)
{
	//COMPILE_ASSERT(THREEDEP_ARRAYSIZE(MESH_LOAD_ERRS) == MESH_LOAD_ENUM_END);
	vector<string> strVec;	

	tetrahedra.clear();
	triangles.clear();
	lines.clear();
	points.clear();
	nodes.clear();

	curLine=0;
	std::ifstream f(meshFile);

	if(!f)
		return 1;

	curLine=1;
	string line;

	//Read file header
	//---
	getline(f,line);
	if(f.fail())
		return 1;

	//Check first line is "$MeshFormat"
	if(line != "$MeshFormat")
		return 1;

	getline(f,line);
	curLine++;
	if(f.fail())
		return 1;

	//Second line should be versionNumber file-type data-size
	splitStrsRef(line.c_str(),' ',strVec);

	if(strVec.size() != 3)
		return 1;

	//Only going to allow version 2.0 && 2.1 && 2.2; can't guarantee other versions....
	if(!(strVec[0] == "2.1" || strVec[0] =="2" || strVec[0] == "2.2"))
		return 1;

	//file-type should be "0" (for ascii file)
	//Number of bytes in a double is third arg; but I will skip it
	if(strVec[1] != "0")
		return 1;

	getline(f,line);
	curLine++;
	if(f.fail())
		return 1;
	if(line != "$EndMeshFormat")
		return 1;
	//--------

	//Read the nodes header
	getline(f,line);
	curLine++;
	if(f.fail())
		return 1;

	if(line != "$Nodes")
		return 1;

	getline(f,line);
	curLine++;
	if(f.fail())
		return 1;

	unsigned int nodeCount;
	if(stream_cast(nodeCount,line))
		return 1;

	std::cerr << "reading node coords " << std::endl;
	//Read the node XYZ coords
	do
	{
		getline(f,line);
		curLine++;
	
		if(f.fail())
			return 1;

		if(line == "$EndNodes")
			break;

		splitStrsRef(line.c_str(),' ',strVec);

		if(strVec.size() < 4)
			return 1;

		Point3D pt;
		if(stream_cast(pt[0],strVec[1]))
			return 1;

		if(stream_cast(pt[1],strVec[2]))
			return 1;

		if(stream_cast(pt[2],strVec[3]))
			return 1;


		nodes.push_back(pt);
	}
	while(!f.eof());


	if(f.eof())
		return 1;
	//Read the elements header
	getline(f,line);
	curLine++;
	if(f.fail())
		return 1;

	if(line != "$Elements")
		return 1;

	getline(f,line);
	curLine++;
	if(f.fail())
		return 1;

	unsigned int elementCount;
	if(stream_cast(elementCount,line))
		return 1;


	std::cerr << "Reading Element data" << std::endl;
	//Read the element data
	do
	{
		getline(f,line);
		curLine++;
	
		if(f.fail())
			return 1;

		if(line == "$EndElements")
			break;

		splitStrsRef(line.c_str(),' ',strVec);

		if(strVec.size() < 3)
			return 1;

		unsigned int numTags,elemType;
		stream_cast(numTags,strVec[2]);
		stream_cast(elemType,strVec[1]);
		
		bool badNode;
		badNode=false;

		switch(elemType)
		{
			case ELEM_SINGLE_NODE_POINT:
			{
				if(strVec.size() - numTags < 4)
					return 2;

				unsigned int ptNum;
				stream_cast(ptNum,strVec[strVec.size() -1]);
				ptNum--;
				points.push_back(ptNum);
				break;
			}
			case ELEM_TWO_NODE_LINE:
			{
				if(strVec.size()-numTags < 5)
					return 2;

				LINE l;

				if(stream_cast(l.physGroup,strVec[3]))
					return 1;
				if(stream_cast(l.p[0],strVec[strVec.size() -2]))
					return 1;
				if(stream_cast(l.p[1],strVec[strVec.size() -1]))
					return 1;

				if( l.p[0] == l.p[1])
				{
					if(allowBadMeshes)
					{
						badNode=true;
						std::cerr << "WARNING: Bad mesh line element at file line " << curLine << std::endl;
					}
					else
						return 1;
				}
				//Convert from 1-index to zero index notation
				l.p[0]--;
				l.p[1]--;
				lines.push_back(l);
				break;
			}
			case ELEM_THREE_NODE_TRIANGLE:
			{
				if(strVec.size()-numTags < 6)
					return 2;

				TRIANGLE t;
				if(stream_cast(t.physGroup,strVec[3]))
					return 1;
				if(stream_cast(t.p[0],strVec[strVec.size() -3]))
					return 1;
				if(stream_cast(t.p[1],strVec[strVec.size() -2]))
					return 1;
				if(stream_cast(t.p[2],strVec[strVec.size() -1]))
					return 1;

				if( t.p[0] == t.p[1] || t.p[1] == t.p[2] ||
						t.p[2] == t.p[0])
				{
					if(allowBadMeshes)
					{
						badNode=true;
						std::cerr << "WARNING: Bad mesh triangle at line " << curLine << std::endl;
					}
					else
						return 1;
				}
				if(!badNode)
				{
					//Convert from 1-index to zero index notation
					t.p[0]--;
					t.p[1]--;
					t.p[2]--;
					triangles.push_back(t);
				}
				break;
			}
			case ELEM_FOUR_NODE_TETRAHEDRON:
			{
				if(strVec.size()-numTags < 7)
					return 2;

				TETRAHEDRON t;
				if(stream_cast(t.physGroup,strVec[3]))
					return 1;
				if(stream_cast(t.p[0],strVec[strVec.size() -4]))
					return 1;
				if(stream_cast(t.p[1],strVec[strVec.size() -3]))
					return 1;
				if(stream_cast(t.p[2],strVec[strVec.size() -2]))
					return 1;
				if(stream_cast(t.p[3],strVec[strVec.size() -1]))
					return 1;

				for(unsigned int ui=0;ui<4; ui++)
				{
					for(unsigned int uj=0;uj<4;uj++)
					{
						if(ui == uj)
							continue;

						if( t.p[ui] == t.p[uj])
						{
							if(allowBadMeshes)
							{
								std::cerr << "WARNING: Bad mesh tetrahedron at line " << curLine << std::endl;
								badNode=true;
							}
							else
								return 1;
						}
					}
				}

				if(!badNode)
				{
					//Convert from 1-index to zero index notation
					t.p[0]--;
					t.p[1]--;
					t.p[2]--;
					t.p[3]--;
					tetrahedra.push_back(t);
				}
				break;
			}
			default:
				return 3;
		}




	}while(!f.eof());


	//Do some final checks - element count can only be under-counted by our class
	// as there may be some primitives we don't support. However, it should be
	// never over counted
	if(!allowBadMeshes)
	{
		if(elementCount < triangles.size() + lines.size() + points.size() + tetrahedra.size() )
			return MESH_LOAD_BAD_ELEMENTCOUNT;

		if(nodeCount != nodes.size())
			return MESH_LOAD_BAD_NODECOUNT;
	}

	if(!isSane())
		return MESH_LOAD_IS_INSANE;

	return 0;
}
*/

unsigned int Mesh::countTriNodes() const
{
	vector<size_t> touchedNodes;

	touchedNodes.resize(triangles.size()*3);
	//Build monolithic list
#pragma omp parallel for
	for(size_t ui=0;ui<triangles.size(); ui++)
	{
		touchedNodes[ui*3]=triangles[ui].p[0];
		touchedNodes[ui*3+1]=triangles[ui].p[1];
		touchedNodes[ui*3+2]=triangles[ui].p[2];
	}

	//Remove non-unique entries
	vector<size_t>::iterator it;
	std::sort(touchedNodes.begin(),touchedNodes.end());
	it=std::unique(touchedNodes.begin(),touchedNodes.end());

	//TODO: Test me...
	touchedNodes.resize(it-touchedNodes.begin());

	return touchedNodes.size();
}

void Mesh::reassignGroups(unsigned int newPhys)
{
#pragma omp parallel for
	for(size_t ui=0;ui<tetrahedra.size();ui++)
		tetrahedra[ui].physGroup = newPhys;
	
#pragma omp parallel for
	for(size_t ui=0;ui<triangles.size();ui++)
		triangles[ui].physGroup = newPhys;

#pragma omp parallel for
	for(size_t ui=0;ui<lines.size();ui++)
		lines[ui].physGroup = newPhys;
}

unsigned int Mesh::saveGmshMesh(const char *meshFile) const
{
	ASSERT(isSane());

	using std::endl;
	using std::ofstream;

	ofstream f(meshFile);

	if(!f)
		return 1;

	f << "$MeshFormat" << endl;
	f << "2.1 0 8" << endl;
	f << "$EndMeshFormat" << endl;


	f << "$Nodes" << endl;

	f << nodes.size() << endl;

	for(size_t ui=0;ui<nodes.size();ui++)
	{
		f << ui+1 << " " << nodes[ui][0] << " " << nodes[ui][1] 
			<< " " << nodes[ui][2] << std::endl;
	}

	f << "$EndNodes" << endl;

	f << "$Elements" << endl;
	f << tetrahedra.size() + triangles.size() + lines.size() + points.size() << endl;

	for(size_t ui=0;ui<tetrahedra.size();ui++)
	{
		f <<  ui+1 <<  " " << ELEM_FOUR_NODE_TETRAHEDRON << " 3 " << tetrahedra[ui].physGroup <<  " 1 0 " << 
			tetrahedra[ui].p[0]+1 << " "  << tetrahedra[ui].p[1]+1 << " "
			 << tetrahedra[ui].p[2]+1 << " " << tetrahedra[ui].p[3]+1 << endl;
	}
	
	for(size_t ui=0;ui<triangles.size();ui++)
	{
		f << tetrahedra.size()+ ui+1 <<  " " << ELEM_THREE_NODE_TRIANGLE << " 3 " << triangles[ui].physGroup << " 1 0 " << 
			triangles[ui].p[0]+1 << " "  << triangles[ui].p[1]+1 << " "
			 << triangles[ui].p[2]+1 << endl;
	}

	for(size_t ui=0;ui<lines.size();ui++)
	{
		f << tetrahedra.size() + triangles.size() +  ui+1 <<  " " << ELEM_TWO_NODE_LINE << " 3 " << lines[ui].physGroup << " 1 0 " << 
			lines[ui].p[0]+1 << " "  << lines[ui].p[1]+1 << endl;
	}

	for(size_t ui=0;ui<points.size();ui++)
	{
		f << tetrahedra.size() + triangles.size() + lines.size() +  ui+1 <<  " "
		       	<< ELEM_SINGLE_NODE_POINT<< " 1 0 " << points[ui]+1 << endl;
	}
	f << "$EndElements" << endl;

	return 0;
}


/*
void Mesh::rotate(const Point3f &axis, const Point3f &origin, float angle)
{
	Quaternion q1,q2;

	//generate rotating quat
	quat_get_rot_quats(&axis,angle,&q1,&q2);

	#pragma omp parallel for
	for(size_t ui=0; ui<nodes.size();ui++)
	{
		Point3f p;
		p.fx = nodes[ui][0]-origin.fx;
		p.fy = nodes[ui][1]-origin.fy;
		p.fz = nodes[ui][2]-origin.fz;

		//use quat
		quat_rot_apply_quats(&p,&q1,&q2);

		nodes[ui][0] = p.fx;
		nodes[ui][1] = p.fy;
		nodes[ui][2] = p.fz;

	}
}
*/

void Mesh::translate()
{
	Point3D origin(0,0,0);
	for(size_t ui=0;ui<nodes.size();ui++)
		origin-=nodes[ui];

	origin*=1.0f/(float)nodes.size();

	translate(origin);
}

void Mesh::translate(const Point3f &origin)
{
	#pragma omp parallel for
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		nodes[ui][0]+=origin.fx;
		nodes[ui][1]+=origin.fy;
		nodes[ui][2]+=origin.fz;
	}
}

void Mesh::translate(const Point3D &origin)
{
	#pragma omp parallel for
	for(size_t ui=0;ui<nodes.size();ui++)
		nodes[ui]+=origin;
}

void Mesh::scale(const Point3f &origin,float scaleFactor)
{
	#pragma omp parallel for
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		nodes[ui][0]=(nodes[ui][0]-origin.fx)*scaleFactor + origin.fx;
		nodes[ui][1]=(nodes[ui][1]-origin.fy)*scaleFactor + origin.fy;
		nodes[ui][2]=(nodes[ui][2]-origin.fz)*scaleFactor + origin.fz;
	}
}

void Mesh::scale(const Point3D &origin,float scaleFactor)
{
	#pragma omp parallel for
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		nodes[ui][0]=(nodes[ui][0]-origin[0])*scaleFactor + origin[0];
		nodes[ui][1]=(nodes[ui][1]-origin[1])*scaleFactor + origin[1];
		nodes[ui][2]=(nodes[ui][2]-origin[2])*scaleFactor + origin[2];
	}
}

void Mesh::scale(float scaleFactor)
{
	#pragma omp parallel for
	for(size_t ui=0;ui<nodes.size();ui++)
		nodes[ui]*=scaleFactor;
}

void Mesh::getBounds(BoundCube &b) const
{
	b.setBounds(nodes);
}

void Mesh::refineTetrahedra(vector<size_t> &refineTets)
{

	//Split the marked tetrahedra into sub-tetrahedra of 4 using a 
	//internal vertex insertion method. 
	//Some strategies are available in the PhD thesis of Wessner,
	//"Mesh Refinement Techniques for TCAD Tools", Vienna
	//which is available at httpP://www.iue.tuwien.ac.at/phd/wessner/
	for(unsigned int ui=0;ui<refineTets.size();ui++)
	{
	
		Point3D accum(0,0,0);
		for(unsigned int uk=0;uk<4;uk++)
		{
			//Compute the central vertex to insert
			accum+=nodes[tetrahedra[refineTets[ui]].p[uk]];
		}	

		accum*=0.25;


		TETRAHEDRON t;

		//T1 := [0,1,4,3],    
		t.p[0] = tetrahedra[refineTets[ui]].p[0];
		t.p[1] = tetrahedra[refineTets[ui]].p[1];
		t.p[2] = nodes.size();
		t.p[3] = tetrahedra[refineTets[ui]].p[3];
		tetrahedra.push_back(t);

		//T2 := [0,4,2,3],    
		t.p[0] = tetrahedra[refineTets[ui]].p[0];
		t.p[1] = nodes.size();
		t.p[2] = tetrahedra[refineTets[ui]].p[2];
		t.p[3] = tetrahedra[refineTets[ui]].p[3];
		tetrahedra.push_back(t);
		
		//T3 := [4,1,2,3],    
		t.p[0] = nodes.size();
		t.p[1] = tetrahedra[refineTets[ui]].p[1];
		t.p[2] = tetrahedra[refineTets[ui]].p[2];
		t.p[3] = tetrahedra[refineTets[ui]].p[3];
		tetrahedra.push_back(t);

		//T4 := [0,1,2,4],    
		t.p[0] = tetrahedra[refineTets[ui]].p[0];
		t.p[1] = tetrahedra[refineTets[ui]].p[1];
		t.p[2] = tetrahedra[refineTets[ui]].p[2];
		t.p[3] = nodes.size();
		tetrahedra.push_back(t);
		

		nodes.push_back(accum);

	}

	//Delete the original unrefined tetrahedra, as we have refined these
	std::sort(refineTets.begin(),refineTets.end(),std::less<size_t>());
	for(unsigned int ui=0;ui<refineTets.size();ui++)
	{
		std::swap(tetrahedra[refineTets[ui]],tetrahedra.back());
		tetrahedra.pop_back();
		
	}
}

void Mesh::getTriEdgeAdjacencyMap(std::vector<std::list<size_t> > &adj) const
{

	using std::list;

	adj.resize(triangles.size());

	//Create a lookup table of vertices -> triangles
	vector<list<size_t> > triLookup;
	triLookup.resize(nodes.size());
	for(size_t ui=0; ui<triangles.size();ui++)
	{
		for(unsigned int uj=0;uj<3;uj++)
			triLookup[triangles[ui].p[uj]].push_back(ui);
	}



	list<size_t> connectedMap;
	for(size_t ui=0; ui<triangles.size();ui++)
	{
		//Check each vertex pair
		for(unsigned int uj=0;uj<3;uj++)
		{
			size_t v1,v2;

			v1 = triangles[ui].p[uj]; 
			v2 = triangles[ui].p[(uj+1)%3];

			ASSERT(triLookup[v1].size());
			ASSERT(triLookup[v2].size());

			//Compute the intersection of the two tri lookup table rows
			list<size_t> intersect;
			intersect=triLookup[v1];
			for(list<size_t>::iterator it=intersect.begin();it!=intersect.end();)
			{
				if( find(triLookup[v2].begin(),triLookup[v2].end(),*it) == triLookup[v2].end())
				{
					it=intersect.erase(it);
				}
				else
					++it;

			}

			//OK, so the intersection of the two lookups is the triangles attached to the nodes.
			for(list<size_t>::iterator it=intersect.begin();it!=intersect.end();++it)
			{
				//Disallow self adjacency
				if(*it !=ui)
					adj[ui].push_back(*it);
			}


		}

	}
}

unsigned int Mesh::divideMeshSurface(float divisionAngle, unsigned int newPhysGroupStart,
			const vector<size_t> &physGroupsToSplit)
{
	using std::list;

	unsigned int origStart=newPhysGroupStart;
	//Construct the 
	vector<list<size_t> > adjacencyMap;
	vector<bool> touchedTris;
	vector<size_t> boundaryTris;
	getTriEdgeAdjacencyMap(adjacencyMap);
	touchedTris.resize(adjacencyMap.size(),false);

	//OK, so the plan is to pick a triangle (any triangle)
	//then to expand this out until we hit an edge, as defined by the
	//angle between adjacent triangle normals.
	//once we hit the edge, we then don't cross that vertex.
	//
	//this algorithm will FAIL (awh new, bro!)
	//if triangles have more than one neighbour on each edge.
	
	//Once we run out of triangles to try (BFS), we then pick one of the "untouched"
	//tris, and then work from there.

	// Step 1:
	// 	* Remove any triangles that are not in the physical groups of interest
	
	for(size_t ui=0;ui<adjacencyMap.size();ui++)
	{
		ASSERT(adjacencyMap[ui].size());

		//Is this triangle in our list?
		if(find(physGroupsToSplit.begin(),physGroupsToSplit.end(),
				triangles[ui].physGroup) == physGroupsToSplit.end())
		{
			//No? OK, lets kill the adj. list entry at this triangle, we don't need it.
			adjacencyMap[ui].clear();
			touchedTris[ui]=true;
		}
		else
		{
			//It is? Well, remove any triangles that are adjacent, but not in the list.
			for(list<size_t>::iterator it=adjacencyMap[ui].begin(); it!=adjacencyMap[ui].end(); ++it)
			{
				if(find(physGroupsToSplit.begin(),physGroupsToSplit.end(),
						triangles[*it].physGroup) == physGroupsToSplit.end())
				{
					it=adjacencyMap[ui].erase(it);
					--it;
				}
			}
		}
	}



	//OK, so now we have an adjacency list of the interesting phys groups.
	//Step 2:
	//	* search for new triangles to group using an expanding boundary method

	BoundCube debugBounds,dbgTmp;
	debugBounds.setInverseLimits();
	do
	{
		//Find a triangle to use as the "seed"
		size_t curTri;
		curTri= find(touchedTris.begin(),touchedTris.end(),false) - touchedTris.begin();


		//No more triangles.. all touched.
		if(curTri == touchedTris.size())
			break;

		//OK, so now we have a "seed" triangle to work with. 
		//create an expanding boundary via adjacency.

		size_t groupSize=0;
		list<size_t> boundary,moreBoundary;
		boundary.clear();
		boundary.push_back(curTri);

		std::cerr << "Seeded with triangle # " << curTri << std::endl;
		touchedTris[curTri]=true; // we touched it.
		triangles[curTri].physGroup=newPhysGroupStart; // we touched it.

		do
		{
			//Expand the boundary using the current boundary triangles

			//loop over the current boundary
			for(list<size_t>::iterator bIt=boundary.begin();bIt!=boundary.end();++bIt)
			{
				ASSERT(adjacencyMap[*bIt].size());
				//Check the adjacency map of the triangles adjacent to a specific boundary element
				for(list<size_t>::iterator it=adjacencyMap[*bIt].begin();it!=adjacencyMap[*bIt].end();++it)
				{
					if(!touchedTris[*it] && 
						(normalAngle(*bIt,*it) < divisionAngle || fabs(normalAngle(*bIt,*it,true)) < divisionAngle) )
					{
						//Alright then, add this new triangle to the potential new boundary
						//(let us not add straight away, as we would like to expand in a minimum
						//perimeter to surface area manner
						moreBoundary.push_back(*it);
						touchedTris[*it]=true;
						triangles[*it].physGroup=newPhysGroupStart;

						dbgTmp.setBounds(nodes[triangles[*it].p[0]],
								  nodes[triangles[*it].p[1]]);
						dbgTmp.expand(nodes[triangles[*it].p[2]]);

						debugBounds.expand(dbgTmp);
						groupSize++;

					}
				}

			}
			//exchange the new boundary list with the boundary list
			boundary.swap(moreBoundary);

			moreBoundary.clear();
		
		}
		while(!boundary.empty());


		//Debug: print bounding box
		std::cerr << "Group size: "<< groupSize << std::endl;
		std::cerr << debugBounds << std::endl;

		//advance the physical group listing
		newPhysGroupStart++;
	}
	while(true);


	//return the number of divided surfaces
	return newPhysGroupStart-origStart+1;

}




void Mesh::getAttachedComponents(size_t tet, 
			vector<size_t> &tris, vector<size_t> &l) const
{
	ASSERT(tet<tetrahedra.size());
	for(size_t ui=0;ui<lines.size(); ui++)
	{
		int mask;
		mask=0;
		//line shares the edges with the tetrahedron?
		for(unsigned int uj=0;uj<4;uj++)
		{
			if(tetrahedra[tet].p[uj] == lines[ui].p[0])
				mask|=1;
			if(tetrahedra[tet].p[uj] == lines[ui].p[1])
				mask|=2;
		}

		if(mask==3)
		{
			//Line is shared.
			l.push_back(ui);
		}

	}
	
	for(size_t ui=0;ui<triangles.size(); ui++)
	{
		int mask;
		mask=0;
		//Triangle shares the edges with the tetrahedron?
		for(unsigned int uj=0;uj<4;uj++)
		{
			if(tetrahedra[tet].p[uj] == triangles[ui].p[0])
				mask|=1;
			if(tetrahedra[tet].p[uj] == triangles[ui].p[1])
				mask|=2;
			if(tetrahedra[tet].p[uj] == triangles[ui].p[2])
				mask|=4;
		}

		if(mask==7)
		{
			//Triangle  is shared.
			tris.push_back(ui);
		}

	}
}

float Mesh::normalAngle(size_t t1, size_t t2, bool flip) const
{
	Point3D nA,nB;

	//Compute nA.
	getTriNormal(t1,nA);
	getTriNormal(t2,nB);
	
	if(flip)
		return nA.angle(-nB);
	//Compute angle
	return nA.angle(nB);	
}

void Mesh::getTriNormal(size_t tri,Point3D &p) const
{
	ASSERT(tri < triangles.size());
	p= (nodes[triangles[tri].p[1]] - nodes[triangles[tri].p[0]]);
	p = p.crossProd(nodes[triangles[tri].p[2]] - nodes[triangles[tri].p[0]]);
	p.normalise();
}

void Mesh::getContainedNodes(const BoundCube &b, std::vector<size_t> &res) const
{
	ASSERT(!res.size());
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		if(b.containsPt(nodes[ui]))
			res.push_back(ui);
	}

}

void Mesh::getIntersectingPrimitives(	std::vector<size_t> &searchNodes,
					std::vector<size_t> &lineRes,
					std::vector<size_t> &triangleRes,
					std::vector<size_t> &tetrahedraRes	) const
{

	std::sort(searchNodes.begin(),searchNodes.end());

	bool searchFound;
	ASSERT(lineRes.size() == triangleRes.size() && tetrahedraRes.size() == lineRes.size() && 
			!tetrahedraRes.size());
	for(size_t ui=0;ui<lines.size();ui++)
	{
		searchFound=false;
		for(size_t uj=0;uj<2;uj++)
		{
			searchFound=std::binary_search(searchNodes.begin(),searchNodes.end(),lines[ui].p[uj]);
			if(searchFound)
				break;
		}

		if(searchFound)
			lineRes.push_back(ui);
	}
	
	for(size_t ui=0;ui<triangles.size();ui++)
	{
		searchFound=false;
		for(size_t uj=0;uj<3;uj++)
		{
			searchFound=std::binary_search(searchNodes.begin(),searchNodes.end(),triangles[ui].p[uj]);
			if(searchFound)
				break;
		}

		if(searchFound)
			triangleRes.push_back(ui);

	}

	for(size_t ui=0;ui<tetrahedra.size();ui++)
	{
		searchFound=false;
		for(size_t uj=0;uj<4;uj++)
		{
			searchFound=std::binary_search(searchNodes.begin(),searchNodes.end(),tetrahedra[ui].p[uj]);
			if(searchFound)
				break;
		}

		if(searchFound)
			tetrahedraRes.push_back(ui);

	}

}

void Mesh::getCurPhysGroups(std::vector<std::pair<unsigned int, size_t> > &curPhys) const
{
	ComparePairFirst cmp;

	//TODO: could be more efficient	 by replacing linear search with
	//boolean one 
	for(unsigned int ui=0;ui<triangles.size();ui++)
	{
		bool found=false;
		for(unsigned int uj=0;uj<curPhys.size();uj++)
		{
			if(curPhys[uj].first== triangles[ui].physGroup)
			{
				found=true;
				curPhys[uj].second++;
				break;
			}
		}

		if(!found)
		{
			//we've not seen this before...
			curPhys.push_back(make_pair(triangles[ui].physGroup,1));
			std::sort(curPhys.begin(),curPhys.end(),cmp);
		}
		
	}
/*
	for(unsigned int ui=0;ui<tetrahedra.size();ui++)
	{
		bool found=false;
		for(unsigned int uj=0;uj<curPhys.size();uj++)
		{
			if(curPhys[uj].first== tetrahedra[ui].physGroup)
			{
				found=true;
				curPhys[uj].second++;
				break;
			}
		}

		if(!found)
		{
			//we've not seen this before...
			curPhys.push_back(make_pair(tetrahedra[ui].physGroup,1));
			std::sort(curPhys.begin(),curPhys.end(),cmp);
		}
		
	}
*/
}

void Mesh::erasePhysGroup(unsigned int physGroup, unsigned int typeMask)
{
	std::cerr << "Erasing..." <<  typeMask << std::endl;
	unsigned int eraseCount;
	if((typeMask & ELEMENT_TRIANGLE) != 0 )
	{
		eraseCount=0;
		//Drop any elements that we don't need
		for(size_t ui=triangles.size()-1;ui!=0; ui--)
		{
			if(triangles[ui].physGroup== physGroup)
			{
				std::swap(triangles[ui],*(triangles.end()-(eraseCount+1)));
				eraseCount++;
			}
		}

		std::cerr << "Erasing " << eraseCount << std::endl;	
		triangles.resize(triangles.size()-eraseCount);
	}


	if(typeMask & ELEMENT_TETRAHEDRON)
	{
		eraseCount=0;
		//Drop any elements that we don't need
		for(size_t ui=tetrahedra.size()-1;ui!=0; ui--)
		{
			if(tetrahedra[ui].physGroup== physGroup)
			{
				std::swap(tetrahedra[ui],tetrahedra.back());
				eraseCount++;
			}
		}
		
		//drop the tail elements	
		tetrahedra.resize(tetrahedra.size()-eraseCount);
	}

	if(typeMask & ELEMENT_LINE)
	{
		eraseCount=0;
		//Drop any elements that we don't need
		for(size_t ui=tetrahedra.size()-1;ui!=0; ui++)
		{
			if(tetrahedra[ui].physGroup== physGroup)
			{
				std::swap(tetrahedra[ui],tetrahedra.back());
				eraseCount++;
			}
		}
		
		//drop the tail elements	
		tetrahedra.resize(tetrahedra.size()-eraseCount);
	}

}


//Volume estimation of Zhang and Chen, using overlapping signed tetrahedron
// method
// EFFICIENT FEATURE EXTRACTION FOR 2D/3D OBJECTS
// IN MESH REPRESENTATION. Dept. Electrical and Computer Engineering,
// Carnegie Mellon University
// Original URL :  http://amp.ece.cmu.edu/Publication/Cha/icip01_Cha.pdf
//  Retrieved from internet archive.
float Mesh::getVolume() const
{
	ASSERT(isSane());
	//ASSERT(isOrientedCoherently());

	//Construct triangle volume estimate
	//using V = 1.0/6.0* | a.(b x c) | 
	// where a, b, c are the relative vectors from some vertex d
	//For us, let d = (0,0,0)
	float vol=0;
	for(size_t ui=0;ui<triangles.size();ui++)
	{
		Point3D p[3];
		const TRIANGLE *t;
		t= &(triangles[ui]);

		for(size_t uj=0;uj<3;uj++)
			p[uj]=nodes[t->p[uj]];

		float newVol;
		newVol=p[0].dotProd(p[1].crossProd(p[2]));	
	
		ASSERT(newVol > 0.0f);
		vol+=newVol;
	}

	vol*=1.0/6.0;
	std::cerr << "Signed volume :" << vol << std::endl;
	return fabs(vol);
}

void Mesh::relax(size_t iterations, float relaxFactor)
{
	ASSERT(isSane());
	using std::pair;
	vector<vector<size_t> > adjacencyList;
	adjacencyList.resize(nodes.size());
	
	//Compute the adjacency list for each vertex
	//--
	for(size_t ui=0;ui<lines.size();ui++)
	{
		for(size_t uj=0;uj<2;uj++)
		{
			adjacencyList[triangles[ui].p[uj]].push_back(
						triangles[ui].p[(uj+1)%2]);
		}
	}
	for(size_t ui=0;ui<triangles.size();ui++)
	{
		for(size_t uj=0;uj<3;uj++)
		{
			adjacencyList[triangles[ui].p[uj]].push_back(
						triangles[ui].p[(uj+1)%3]);

			adjacencyList[triangles[ui].p[uj]].push_back(
					 	triangles[ui].p[(uj+2)%3]);
		}
	}

	for(size_t ui=0;ui<tetrahedra.size();ui++)
	{
		for(size_t uj=0;uj<3;uj++)
		{
			adjacencyList[tetrahedra[ui].p[uj]].push_back(
						tetrahedra[ui].p[(uj+1)%4]);

			adjacencyList[tetrahedra[ui].p[uj]].push_back(
					 	tetrahedra[ui].p[(uj+2)%4]);
			adjacencyList[tetrahedra[ui].p[uj]].push_back(
					 	tetrahedra[ui].p[(uj+3)%4]);
		}
	}

	//Measure the mesh volume
	float origVol;
	origVol=getVolume();

	//Compute the centroid for the node set
	Point3D centroid(0,0,0);
	for(size_t ui=0;ui<nodes.size();ui++)
		centroid+=nodes[ui];
	centroid*=1.0f/(float)nodes.size();
	//Re-centre the mesh around the origin
	for(size_t ui=0;ui<nodes.size();ui++)
		nodes[ui]-=centroid;

	//Compute the iteration vertex adjacency weights
	// each node singly connected to this one is worth 1 weight. 
	// If it is multiply connected, it is worth more proportionally
	//-------
	vector<vector<pair<size_t,float> > > adjacencyFactors;
	adjacencyFactors.resize(nodes.size());

	for(size_t ui=0;ui<adjacencyList.size();ui++)
	{
		size_t factor;
		std::sort(adjacencyList[ui].begin(),adjacencyList[ui].end());
	
		for(size_t uj=0;uj<adjacencyList[ui].size();uj++)
		{
			//Skip non-unique entries
			if(uj && (adjacencyList[ui][uj-1] == adjacencyList[ui][uj]))
				continue;

			factor=std::count(adjacencyList[ui].begin(),
					adjacencyList[ui].end(),adjacencyList[ui][uj]);
		
			adjacencyFactors[ui].push_back(
				std::make_pair(adjacencyList[ui][uj],factor));
		}
	}
	adjacencyList.clear();
	//-------


	//Perform main relaxation iteration
	//---
	for(size_t it=0;it<iterations;it++)
	{
		for(size_t ui=0;ui<adjacencyFactors.size();ui++)
		{
			if(adjacencyFactors[ui].empty())
				continue;

			Point3D nodeV;
			nodeV=nodes[ui];
			size_t divisor;
			divisor=1;
			for(size_t uj=0;uj<adjacencyFactors[ui].size();uj++)
			{
				size_t fact,v;
				fact=adjacencyFactors[ui][uj].second;
				v=adjacencyFactors[ui][uj].first;

				nodeV+=(nodes[v]*(float)fact);
				divisor+=fact;
			}
			
			nodeV*=1.0f/((float)divisor);

			nodes[ui] = (nodeV -nodes[ui])*relaxFactor + nodes[ui];
		}
	}	

	//As a *nasty* hack - inflate the mesh such that it will occupy its original volume


	//Equate a sphere (4/3*pi*r^3) to a shrunk sphere (4/3*pi*(r+d)^3)
	// then work out the inflation factor to bring the two volumes into concert. 
	// This becomes less and less valid, the less spherical the object is...
	cerr << "Target (original) volume:"  << origVol << endl;
	for(size_t ui=0;ui<3;ui++)
	{
		float inflationDist;
		float newVol;
		newVol=getVolume();
		cerr << "volume before inflation" << ui <<  " :" << newVol << endl;
		inflationDist = -(pow((3.0/(4.0*M_PI)), (1.0/3.0))*
			(pow(newVol, (1.0/3.0)) - pow(origVol, (1.0/3.0))));
		cerr << "volume after inflation" << ui <<  " :" << getVolume()<< endl;


		for(size_t ui=0;ui<nodes.size();ui++)
			nodes[ui].extend(inflationDist);
	}	
	//Re-position the mesh back around its original centroid
	for(size_t ui=0;ui<nodes.size();ui++)
		nodes[ui]+=centroid;
}

size_t Mesh::elementCount() const
{
	return points.size() + tetrahedra.size() + triangles.size() + lines.size();
}

//This is a somewhat efficient algorithm to compute if a set of points lies
//inside or outside a sealed mesh. With work, could be better.
//Mesh must:
//	- Consist only of triangles
//	- Be water tight (no holes in mesh)
//	- be non-self intersecting
//	- have coherently oriented triangle normals
void Mesh::pointsInside(const vector<Point3D> &p,
			vector<bool> &meshResults, std::ostream &msgs, bool wantProg) const
{
//	ASSERT(trianglesFullyConnected()); TODO: Implement me
	//ASSERT(isOrientedCoherently());
	ASSERT(!tetrahedra.size());

	Point3D centre=Point3D(0,0,0);;
	//Find the bounding box of the triangle component of the mesh
	for(size_t ui=0;ui<triangles.size();ui++)
	{
		//create a point cloud consisting of triangle vertices
		centre+=(nodes[triangles[ui].p[0]]);
		centre+=(nodes[triangles[ui].p[1]]);
		centre+=(nodes[triangles[ui].p[2]]);
	}

	centre=centre*1.0/(3.0f*(float)triangles.size());

	
	//Find the minimal and maximal distances from the centre of the mesh
	// to the surface
	float maxSqrDistance;
	maxSqrDistance=0; 
	for(size_t ui=0;ui<triangles.size();ui++)
	{
		maxSqrDistance=std::max(maxSqrDistance,
			centre.sqrDist(nodes[triangles[ui].p[0]]));
		maxSqrDistance=std::max(maxSqrDistance,
			centre.sqrDist(nodes[triangles[ui].p[1]]));
		maxSqrDistance=std::max(maxSqrDistance,
			centre.sqrDist(nodes[triangles[ui].p[2]]));
	}


	//Two points outside from which we can shoot rays for Jordan 
	//curve theorem
	Point3D outsideMesh[2];
	outsideMesh[0] = centre + Point3D(0,0,1.1)*maxSqrDistance;
	outsideMesh[1] = centre - Point3D(0,0,1.1)*maxSqrDistance;
	meshResults.resize(p.size(),false);
	BoundCube c;
	c.setBounds(nodes);
	
	
	//Draw a progress bar
	if(wantProg)
	{
		msgs << endl << "|";
		for(unsigned int ui=0; ui<100; ui++)
			msgs << ".";
		msgs << "| 100%" << endl << "|.";
	}

	size_t actualProg=0;
	size_t reportedProg=0;
	size_t curProg=0;
	size_t progReduce=PROGRESS_REDUCE;
	//Loop through the point array; generate the final mesh
#pragma omp parallel for firstprivate(progReduce) 
	for(unsigned int ui=0;ui<p.size();ui++)
	{
		//Do  a quick spherical shell test, 
		//then a cube test before doing more complex 
		//polygonal containment test
		float sqrDist;
		sqrDist=p[ui].sqrDist(centre);
		if(sqrDist <= maxSqrDistance && 
			c.containsPt(p[ui]) )
		{

			//So the sphere test didn't give us a clear answer.
			// use a complete polygonal test to check if the point is inside or outside
			unsigned int intersectionCount;
			intersectionCount=0;
			BoundCube rayBound;
			Point3D *externPt;

			if(p[ui].sqrDist(outsideMesh[0])<
				p[ui].sqrDist(outsideMesh[1]))
			{
				externPt=outsideMesh;
				rayBound.setBounds(p[ui],outsideMesh[0]);
			}
			else
			{
				externPt=outsideMesh+1;
				rayBound.setBounds(p[ui],outsideMesh[1]);
			}

			//Test each triangle for intersection with the
			//ray coming from outside the mesh. If even,
			//point is outside. If odd, inside.
			for(size_t uj=0;uj<triangles.size();uj++)
			{
				Point3D triangle[3];
				Point3D dummy;

				triangle[0] = nodes[triangles[uj].p[0]];
				triangle[1] = nodes[triangles[uj].p[1]];
				triangle[2] = nodes[triangles[uj].p[2]];

				unsigned int intersectType;
				intersectType = intersect_RayTriangle(*externPt,
								p[ui],triangle,dummy);
				if(intersectType == 1) 
					intersectionCount++;
			}

			//If is odd, due to boundary crossings, it must be inside
			if(intersectionCount%2)	
			{
				meshResults[ui]=true;
			}
		}

		if(wantProg && !progReduce-- )
		{
#ifdef _OPENMP
			if(!omp_get_thread_num())
			{
#endif
			//Compute actual progress
			actualProg= (unsigned int)(((float)curProg*100.0f)/(float)p.size());

			//Update the user on our actual progress
			while(reportedProg < actualProg)
			{
				msgs << ".";
				reportedProg++;
			}
#ifdef _OPENMP
			}
#endif

#pragma omp critical
			curProg+=PROGRESS_REDUCE;
			
			progReduce=PROGRESS_REDUCE;
		}	
	}
	
	if(wantProg)
	{
		while(reportedProg++ < 100)
			msgs << ".";
		msgs << "|";	
	}
}


/*
size_t Mesh::getNearestTri(const Point3D &p,float &signedDistance) const
{
	//Loop over all the triangles in order to locate the nearest
	size_t nearTri  = (size_t)-1;
	float distance=std::numeric_limits<float>::max();
	for(size_t ui=0;ui<triangles.size();ui++)
	{
		Point3D normal;
		getTriNormal(ui,normal);
		
		float newDist;
		newDist=signedDistanceToFacet(nodes[triangles[ui].p[0]],
					nodes[triangles[ui].p[1]],
					nodes[triangles[ui].p[2]],normal,p);
		if(fabs(newDist) < distance)
		{
			nearTri=ui;
			distance= fabs(newDist);
			signedDistance=newDist;
		}
	}

	return nearTri;
}


bool Mesh::isOrientedCoherently() const
{
	ASSERT(isSane());
	//Need to check circulation of triangles, (edge A->B on one
	// triangle matches that of the other).
	//for all triangles in the mesh
	
	vector<bool> seenTri;
	seenTri.resize(triangles.size(),false);

	deque<size_t> triQueue;

	std::vector<std::list<size_t> > adjacency;
	getTriEdgeAdjacencyMap(adjacency);

	for(size_t ui=0;ui<triangles.size();ui++)
	{
		//Add unvisited to queue,
		if(!seenTri[ui])
		{
			triQueue.push_back(ui);
			seenTri[ui] = true;
		}

		//Visit all triangles contiguous to whatever is in the queue
		while(triQueue.size())
		{
			size_t tri;
			tri = triQueue.front();
			triQueue.pop_front();
				
			list<size_t> *curAdjT;
			curAdjT= &(adjacency[tri]);

			for(list<size_t>::const_iterator it=curAdjT->begin(); it !=curAdjT->end();++it)
			{
				if(*it ==  tri || seenTri[*it]) 
					continue;

				if(triangles[tri].edgesMismatch(triangles[*it]))
					return false;

				seenTri[*it]= true;
				triQueue.push_back(*it);

			}

		}
	}


	return true;
}



void Mesh::orientTriEdgesCoherently() 
{
	//Need to check circulation of triangles, (edge A->B on one triangle matches that of the other).
	//for all triangles in the mesh
	vector<bool> seenTri;
	seenTri.resize(triangles.size(),false);

	deque<size_t> triQueue;

	std::vector<std::list<size_t> > adjacency;
	getTriEdgeAdjacencyMap(adjacency);




	for(size_t ui=0;ui<triangles.size();ui++)
	{
		//Add unvisited to queue,
		if(!seenTri[ui])
		{
			triQueue.push_back(ui);
			seenTri[ui] = true;
		}

		//Visit all triangles contiguous to whatever is in the queue
		while(triQueue.size())
		{
			size_t tri;
			tri = triQueue.front();
			triQueue.pop_front();
				
			list<size_t> *curAdjT;
			curAdjT= &(adjacency[tri]);

			for(list<size_t>::const_iterator it=curAdjT->begin(); it !=curAdjT->end();++it)
			{
				if(*it ==  tri || seenTri[*it]) 
					continue;

				if(triangles[tri].edgesMismatch(triangles[*it]))
				{
					//Reverse the vertex order on the triangle
					std::swap(triangles[*it].p[0],
							triangles[*it].p[1]);

				}
				seenTri[*it]= true;
				triQueue.push_back(*it);

			}

		}
	}

	//ASSERT(isOrientedCoherently());
}
*/

bool TRIANGLE::isSane(size_t nMax) const
{

	for(size_t ui=0;ui<3;ui++)
	{
		if ( p[ui] == p[(ui+1)%3])
			return false;

		//if nMax supplied, use it
		if(nMax != (size_t) -1 &&  p[ui] > nMax )
			return false;
	}

	return true;
}

/*
bool TRIANGLE::edgesMismatch(const TRIANGLE &other) const
{
	ASSERT(isSane());
	ASSERT(other.isSane());


	vector<size_t> commonV;
	for(size_t ui=0;ui<3;ui++)
	{
		for(size_t uj=0;uj<3;uj++)
		{
			if ( other.p[uj] == p[ui])
			{
				commonV.push_back(p[ui]);
				break;
			}
		}
	}

	ASSERT(commonV.size() <=3);

	//If either zero or one common vertices, then there is no edge
	// mismatch
	if(commonV.size() < 2)
		return false;
	else 
	{
		unsigned int pA[3],pB[3];

		for(size_t ui=0;ui<3;ui++)
		{
			//If common vertex cannot be found, replace with "-1"
			if(std::find(commonV.begin(),commonV.end(),p[ui]) == commonV.end())
				pA[ui]=-1;
			else
				pA[ui]=p[ui];
			
			//If common vertex cannot be found, replace with "-1"
			if(std::find(commonV.begin(),commonV.end(),other.p[ui]) == commonV.end())
				pB[ui]=-1;
			else
				pB[ui]=other.p[ui];
		}

		if (commonV.size() == 3)
		{

			//If the triangles have all 3 vertices in common, they will match IFF they
			// have a rotationally invariant sequence that matches (3 matching edges). As permutations
			// other than vertex sequence rotation will flip the triangle normal
			// egg : 1-2-3 matches 2-3-1,  but not  1-3-2
			return !rotateMatch(pA,pB,3);
		}
		else
		{
			//If the triangles have 2 vs in common, then they have one edge in common.
			// this will match IFF the circulation (edge ordering) of the two triangles is opposite
			return !antiRotateMatch(pA,pB,3);
		}
	}

	ASSERT(false);	
}
*/

#ifdef DEBUG

bool coherencyTests()
{
	//Create a perfects valid mesh of tris
	//---
	Mesh m;

	m.nodes.push_back(Point3D(0,0,0));
	m.nodes.push_back(Point3D(0,0,1));
	m.nodes.push_back(Point3D(1,0,0));
	m.nodes.push_back(Point3D(0,1,0));

	TRIANGLE t;
	t.p[0] = 0;
	t.p[1] = 1;
	t.p[2] = 2;
	m.triangles.push_back(t);

	t.p[0]=1;
	t.p[1]=0;
	t.p[2]=3;
	m.triangles.push_back(t);
	
	t.p[0]=3;
	t.p[1]=2;
	t.p[2]=1;
	m.triangles.push_back(t);
	//---

	//TEST(m.isOrientedCoherently(),"mesh coherency check");


	//Flip the shared edge representation for a tri, so we get an inverted
	//  normal on one tri
	m.triangles[1].p[0]=0;
	m.triangles[1].p[1]=1;

	//TEST(!m.isOrientedCoherently(),"check incoherent mesh detection");

	//Attempt to reorient the mesh coherently
	//m.orientTriEdgesCoherently();

	//check it worked
	//TEST(m.isOrientedCoherently(), "Mesh auto-reorient")

	return true;
}

bool nearestTriTest()
{
	Mesh m;

	//Make an L shaped edge
	m.nodes.push_back(Point3D(1,0,0));
	m.nodes.push_back(Point3D(-1,0,0));
	m.nodes.push_back(Point3D(0,0,1));
	m.nodes.push_back(Point3D(0,1,0));


	//0,3,1
	TRIANGLE t;
	t.p[0]=0;
	t.p[1]=3;
	t.p[2]=1;
	m.triangles.push_back(t);

	//0,1,2
	t.p[0]=0;
	t.p[1]=1;
	t.p[2]=2;
	m.triangles.push_back(t);
	
	//Test that the exterior test works, 
	// using point (0.5,0,0.4)
	//float dist;
	//TEST(m.getNearestTri(Point3D(0,0.5,0.4),dist) == 0,"Nearest tri");
	//TEST(m.getNearestTri(Point3D(0,0.5,0.6),dist) == 1,"Nearest tri");

	return true;

}

bool meshTests()
{
	TEST(coherencyTests(),"Mesh coherency checks");
	TEST(nearestTriTest(),"Mesh nearest tri");
	return true;
}

#endif
