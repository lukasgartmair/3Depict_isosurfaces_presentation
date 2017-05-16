/* 
 * K3DTreeMk2.h  - Precise KD tree implementation
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

#ifndef K3DTREEMK2_H
#define K3DTREEMK2_H

#include <set>
#include <vector>
#include <utility>

#include "common/basics.h"
#include "backend/APT/ionhit.h"

//This is the second revision of my KD tree implementation
//The goals here are, as compared to the first
//	- Improved build performance by minimising memory allocation calls
//	  and avoiding recursive implementations
//	- index based construction for smaller in-tree storage


//!Functor allowing for sorting of points in 3D
/*! Used by KD Tree to sort points based around which splitting axis is being used
 * once the axis is set, points will be ranked based upon their relative value in
 * that axis only
 */ 
class AxisCompareMk2
{
	private:
		unsigned int axis;
	public:
		void setAxis(unsigned int Axis){axis=Axis;};
		inline bool operator()(const std::pair<Point3D,size_t> &p1,const std::pair<Point3D,size_t> &p2) const
		{return p1.first[axis]<p2.first[axis];};
};


//!Node Class for storing point
class K3DNodeMk2
{
	public:
		//Index of left child in parent tree array. -1 if no child
		size_t childLeft;
		//Index of right child in parent tree array. -1 if no child
		size_t childRight;

		//Has this point been marked by an external algorithm?
		bool tagged;
};

//!3D specific KD tree
class K3DTreeMk2
{
	private:
		//!The maximum depth of the tree
		size_t maxDepth;

		//!Tree array. First element is spatial data. 
		//Second is original array index upon build
		std::vector<std::pair<Point3D,size_t> > indexedPoints;

		//!Tree node array (stores parent->child relations)
		std::vector<K3DNodeMk2> nodes;

		//!total size of array
		size_t arraySize;

		//!Which entry is the root of the tree?
		size_t treeRoot;

		BoundCube treeBounds;
		
		static unsigned int *progress; //Progress counter
		static ATOMIC_BOOL *abort; //set to true if build should abort. Must be initalised prior to build

	public:
		//KD Tree constructor
		K3DTreeMk2(){};

		//!Cleans up tree, deallocates nodes
		~K3DTreeMk2(){};

		static void setProgressPtr(unsigned int *ptr){progress=ptr;}
		static void setAbortFlag(ATOMIC_BOOL *ptr){abort=ptr;}

		//Reset the points
		void resetPts(std::vector<Point3D> &pts, bool clear=true);
		void resetPts(std::vector<IonHit> &pts, bool clear=true);
	
	
		//Set a pointer that can be used to indicate that we need to abort build
		static void setAbortPtr(bool *abortFlag);
		//Set a pointer that can be used to write the current progress
		static void setProgressPointer(unsigned int *p) ;
		

		/*! Builds a balanced KD tree from a list of points
		 *  previously set by "resetPts". returns false if callback returns
		 *  false;
		 */	
		bool build();

		void getBoundCube(BoundCube &b);

		//!Textual output of tree. tabs are used to separate different levels of the tree
		/*!The output from this function can be quite large for even modest trees. 
		 * Recommended for debugging only*/
		void dump(std::ostream &,size_t depth=0, size_t offset=-1) const;


		//Find the nearest "untagged" point's internal index.
		//Mark the found point as "tagged" in the tree. Returns -1 on failure (no untagged points)
		// optionaly, a sub-root branch of the tree can be specified, eg based upon range query,
		// in order to speed up search
		size_t findNearestUntagged(const Point3D &queryPt,
						const BoundCube &b, bool tag=true,size_t pseudoRoot=(size_t)-1);

		//Find the nearest "untagged" point's internal index.
		// Skip any of the listed points.
		size_t findNearestWithSkip(const Point3D &queryPt,
				const BoundCube &b,const std::set<size_t> &skipPts,
					size_t pseudoRoot=(size_t)-1) const;

		//Find the indicies of all points that lie within  the
		// sphere (pts < radius) of given radius, centered upon
		// this origin
		void ptsInSphere(const Point3D &origin, float radius,
			std::vector<size_t> &pts) const;
	
		//!Get the contigous node IDs for a subset of points in the tree that are contained
		// within a sphere positioned about pt, with a sqr radius of sqrDist.
		// 	- This does *NOT* get *all* points - only some. 
		// 	- It should be faster than using findNearestUntagged repeatedly 
		// 	  for large enough sqrDist. 
		// 	- It does not check tags.	
		void getTreesInSphere(const Point3D &pt, float sqrDist, const BoundCube &domainCube,
					std::vector<std::pair<size_t,size_t> > &contigousBlocks ) const;

		//!Get the smallest contigous bounds that will contain a box.
		// - new sub-tree root is returned. 
		// function may only be called if tree is initalised
		size_t getBoxInTree(const BoundCube &box) const;

		//Obtain a point from its internal index
		const Point3D *getPt(size_t index) const ;
		//Obtain a point from its internal index
		const Point3D &getPtRef(size_t index) const ;

		//reset the specified "tags" in the tree
		void clearTags(std::vector<size_t> &tagsToClear);

		//Convert the "tree index" (the position in the tree) into the original point offset in the input array
		size_t getOrigIndex(size_t treeIndex) const ;
		

		//Erase tree contents
		void clear() { nodes.clear(); indexedPoints.clear();};

		//mark a point as "tagged" (or untagged,if tagVal=false) via its tree index.
		void tag(size_t tagID,bool tagVal=true) ;

		//obtain the tag status for a given point, using the tree index
		bool getTag(size_t treeIndex) const ;

		//obtain the number of points in the tree
		size_t size() const; 
		
		//Find the position of the root node in the tree
		size_t rootIdx() const { return treeRoot;}

		//Find the number of tagged items in the tree
		size_t tagCount() const;

		//reset all tagged points to untagged
		void clearAllTags();
};


#ifdef DEBUG
//KD tree internal unit tests
// - return true on OK, false on fail
bool K3DMk2Tests();
#endif
#endif
