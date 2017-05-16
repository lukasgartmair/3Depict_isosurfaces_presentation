/* 
 * Copyright (C) 2015  Daniel Haley
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
#ifndef MESH_H
#define MESH_H

#include "common/basics.h"
#include "common/mathfuncs.h"

#include <vector>
#include <fstream>
#include <list>

//GMSH load constants
//-----------
//Constants are defined in the GMSH documentation
//under section MSH-ASCII-file-format
//http://geuz.org/gmsh/doc/texinfo/gmsh.html#MSH-ASCII-file-format
const unsigned int ELEM_SINGLE_NODE_POINT=15;
const unsigned int ELEM_TWO_NODE_LINE=1;
const unsigned int ELEM_THREE_NODE_TRIANGLE=2;
const unsigned int ELEM_FOUR_NODE_TETRAHEDRON=4;
//-----------


extern const char *MESH_LOAD_ERRS[];

enum
{
	ELEMENT_TRIANGLE=1,
	ELEMENT_TETRAHEDRON=2,
	ELEMENT_LINE=4,
};

class TETRAHEDRON{ 
	public:
	unsigned int p[4];
	unsigned int physGroup;
};

class TRIANGLE{ 
	public:
	unsigned int p[3];
	unsigned int physGroup;

	bool isSane(size_t pLimit=(size_t)-1) const;
	bool edgesMismatch(const TRIANGLE &tOther) const;
};

class LINE{
	public:
	unsigned int p[2];
	unsigned int physGroup;
};

class Mesh
{
	private:

		//!Returns true if ui and uj are the same
		bool sameTriangle(unsigned int ui, unsigned int uj) const;
		static bool sameTriangle(const TRIANGLE &t1, const TRIANGLE &t2);

		//!Returns true if the group of triangles are coincident to the nominated triangle
		bool trianglesCoincident(unsigned int ui, const std::vector<size_t> &v) const;

		//!Return true if the specified tet is degenerate (coplanar)
		bool tetrahedronDegenerate(unsigned int tet) const;

		//!Return true if  a specified point is in the tet.
		bool pointInTetrahedron(unsigned int tet, const Point3D &p) const;
		
		//!Get a list of all disconnected tetrahedra	
		void getDisconnectedTets(std::vector<size_t> &tetIdx) const;	

		//!Build an adjacency listing for the triangles in the mesh
		//using vertex sharing rules
		//i.e. this will retrieve all vector of all triangles who share edge incidence 
		// for a specified vertex, eg: <|>  vertex only 
		//incident triangles eg : |>*<| , will be excluded.
		//The returned map can be used to perform triangle -> surrounding triangle lookups for shared edge
		// incidence
		void getTriEdgeAdjacencyMap(std::vector<std::list<size_t> > &map) const;

		//!Return the normal angle (in rad) between two triangles
		float normalAngle(size_t triOne,size_t triTwo,bool flip=false ) const;

		//!Reverse triangle vertex (triTwo) to reverse shared edge to invert implicit normal
		void flipTriNormalCoherently(size_t triOne, size_t triTwo); 

		//!Kill specified orphan nodes within this dataset
		void killOrphanNodes(const std::vector<size_t> &orphans) ;
	public:
		//!Point storage for 3D Data (nodes/coords/vertices..)
		std::vector<Point3D> nodes;

		//!Physical group storage
		std::vector<std::string> physGroupNames;
		
		
		//==== Element Storage ====
		//!Storage for node connectivity in tet. form 
		std::vector<TETRAHEDRON> tetrahedra;
		//!Storage for node connectivity in triangle form (take in groups of 3)
		//triangles.size() %3 should always == 0.
		std::vector<TRIANGLE> triangles;
		//!Storage for line segments. .size()%2 should always==0
		std::vector<LINE> lines;
		//!		
		std::vector<unsigned long long> points;

		//Returns 0 on OK, nonzero on error.
		unsigned int loadGmshMesh(const char *meshfile, unsigned int &curLine, bool allowBadMeshes=true);
		unsigned int saveGmshMesh(const char *meshfile) const;

		//Return sum of all element sizes (total lines, points, triangles, tets, etc)
		size_t elementCount() const;

		//Set the triangle mesh from the following pt triplet, each vector being the same size.
		// Function will clear any existing data
		void setTriangleMesh(const std::vector<float> &ptsA, 
				const std::vector<float> &ptsB, const std::vector<float> &ptsC);

		//!reassign the physical groups to a single number
		void reassignGroups(unsigned int i);

		//!Remove exact duplicate triangles
		void removeDuplicateTris();
		//Remove triangles that are not fully connected to mesh (ie have all edges shared)
		void removeStrayTris();

		//Merge vertices that lie  within tolerance distance of one another into a single vertex.
		// note that this can produced degenerate objects in the mesh, if tolerance is large comapred to the smallest element in the mesh
		void mergeDuplicateVertices(float tolerance);

		//!Perform various sanity tests on mesh. Should return true if your mesh is sane.
		//Returning true is not a guarantee of anything however.
		bool isSane(bool output=false,std::ostream &outStream=std::cerr) const;

		//!Get the Axis aligned bounding box for this mesh
		void getBounds(BoundCube &b) const;

		//!Count the number of unique nodes shared by triangles
		unsigned int countTriNodes() const;

		//!Translate mesh around node centroid
		void translate();
		//!Translate mesh to specified position 
		void translate(const Point3f &origin);
		void translate(const Point3D &origin);
		
		//!Scale the mesh around a specified origin
		void scale(const Point3f &origin, float scalefactor);

		//!Scale the mesh around a specified origin
		void scale(const Point3D &origin,float scaleFactor);

		//!Scale the mesh around origin
		void scale(float scaleFactor);

		//!Rotate mesh
		void rotate(const Point3f &axis, const Point3f &origin, float angle);

		//!Obtain the volume of the triangulated space
		// triangles must be correctly oriented, and closed
		float getVolume() const;

		//!place triangles over exposed tetrahedral faces
		void resurface(unsigned int newPhys);

		//!Clear the mesh
		void clear();
		
		//!Check to see if the mesh is a single unit of tetrahedra
		bool isTetFullyConnected(unsigned int &badTet) const;

		//!Refine the selected tetrahedra using a midpoint division method
		void refineTetrahedra(std::vector<size_t> &refineTets); 


		//!Get the line and triangle segments that are connected to a particular tetrahedron
		void getAttachedComponents(size_t tet, 
			std::vector<size_t> &tris, std::vector<size_t> &l) const;

		//!Return all the nodes that are contained within specified bounding box
		void getContainedNodes(const BoundCube &b,
					std::vector<size_t> &nodes) const;

		//!Return all primitives that are WHOLLY contained withing bounding box
		void getIntersectingPrimitives(	std::vector<size_t> &searchNodes,
					std::vector<size_t> &lines,
					std::vector<size_t> &triangles,
					std::vector<size_t> &tetrahedra	) const;

		unsigned int divideMeshSurface(float divisionAngle, unsigned int newPhysGroupStart,
			const std::vector<size_t> &physGroupsToSplit) ;

		void getCurPhysGroups(std::vector<std::pair<unsigned int,size_t> > &curPhys) const;

		void erasePhysGroup(unsigned int group, unsigned int typeMask);

	
		//obtain the number of duplicate vertices in the mesh
		unsigned int numDupVertices(float tolerance) const;

	
		//Obtain the number of duplicate triangles in the mesh
		unsigned int numDupTris() const;

		//Print some statistics about the mesh data
		void print(std::ostream &o) const;

		//Flip normals in the mesh in order to have coherently oriented normals for all contigous objects.
		//No guarantee is given as to which surface will be inside and which will be outside.
		//FIXME: BROKEN
		void orientTriEdgesCoherently() ;
		
		//Returns true if the mesh is coherently oriented
		bool isOrientedCoherently()  const;

		//!Kill specified orphan nodes within this dataset
		void killOrphanNodes();

		//!Perform vertex weighted relaxation
		void relax(size_t iterations, float relaxFactor);

		//Find the poitns that lie inside a this mesh
		void pointsInside(const std::vector<Point3D> &p,
			std::vector<bool> &meshResults, std::ostream &msgs, bool wantProg) const ;

		//Find the nearest triangle to a particular point
		size_t getNearestTri(const Point3D &p,float &distance)  const;

		void getTriNormal(size_t tri, Point3D &normal) const;
};

#ifdef DEBUG
//Run unit tests for mesh
bool meshTests();
#endif

#endif
