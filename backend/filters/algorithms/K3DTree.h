/* 
 * K3DTree.h - limited precision KD tree implementation
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
#ifndef K3DTREE_H
#define K3DTREE_H



#include "common/basics.h" //For BoundCube

class K3DNode;
class AxisCompare;
class K3DTree;



//!Functor allowing for sorting of points in 3D
/*! Used by KD Tree to sort points based around which splitting axis is being used
 * once the axis is set, points will be ranked based upon their relative value in
 * that axis only
 */ 
class AxisCompare
{
	private:
		unsigned int axis;
	public:
		AxisCompare();
		void setAxis(unsigned int Axis);
		inline bool operator()(const Point3D &p1,const Point3D &p2) const
		{return p1[axis]<p2[axis];};
};

//!Node Class for storing point
class K3DNode
{
	private:
		K3DNode *childLeft;
		K3DNode *childRight;
		Point3D loc;
		//!The axis being stored here is redundant, but the original idea was to speed up acces times in K3DTree::findNearest
		unsigned int axis;
	public:
		//K3DNode();
		//get and sets.
		
		//!Return the point data from the ndoe
		Point3D getLoc() const;
		//!Return a pointer to the point from the node
		inline const Point3D *getLocRef() const { return &loc;};
		//!Set the left child node
		inline void setLeft(K3DNode *node) {childLeft= node;};
		//!Set the right child node
		inline void setRight(K3DNode *node) {childRight=node;};

		//!Set the point data associated with this node
		void setLoc(const Point3D &);
		//!Set the axis that this node operates on
		inline void setAxis(unsigned int newAxis) {axis=newAxis;};
		//!Retrieve the axis that this node operates on
		inline unsigned int getAxis() const{ return axis;};
		//!retrieve the value associated with this axis
		inline float getAxisVal() const { return loc.getValue(axis);};
		inline float getLocVal(unsigned int pos) const {return loc.getValue(pos);};	
		inline float sqrDist(const Point3D &pt) const {return loc.sqrDist(pt);};	
		//TODO: make this return const pointer?
		inline K3DNode *left() const {return childLeft;};
		inline K3DNode *right() const {return childRight;};

		//!Recursively delete this node and all children
		void deleteChildren();
		//!print the node data out to a given stream
		void dump(std::ostream &, unsigned int) const;
};

//!3D specific KD tree
class K3DTree
{
	private:
		//!Total points in tree
		unsigned int treeSize;
		//!The maximum depth of the tree
		unsigned int maxDepth;
		
		//!Root node of tree
		K3DNode *root;
		//!Build tree recursively
		K3DNode *buildRecurse(std::vector<Point3D>::iterator pts_start,
			       	std::vector<Point3D>::iterator pts_end, unsigned int depth );
		
		size_t curNodeCount; //Counter for build operations

		static unsigned int *progress; //Progress counter
		static const ATOMIC_BOOL *abort; //aborting flag
	public:
	
		//KD Tree constructor
		K3DTree();

		//!Cleans up tree, deallocates nodes
		~K3DTree();

		static void setProgressPtr(unsigned int *ptr){progress=ptr;}
		static void setAbortFlag(const ATOMIC_BOOL *ptr){abort=ptr;}
	
		/*! Builds a balanced KD tree from a list of points
		 * This call is being passed by copy in order to prevent
		 * re-ordering of the points. It may be worth having two calls
		 * a pass by ref version where the points get scrambled and this one
		 * which preserves input point ordering (due to the copy) 
		 */	
		void build(std::vector<Point3D> pts);

		/*! Builds a balanced KD tree from a list of points
		 *  This uses a pass by ref where the points get scrambled (sorted). 
		 *  Resultant tree should be identical to that generated by build() 
		 */	
		void buildByRef(std::vector<Point3D> &pts);
	

		//Tree walker that counts the number of nodes
		//void verify();

		//! Clean the tree
		void kill();
		
		//!Find the nearest point to a given P that lies outsid some dead zone
		/*!deadDistSqr can be used ot disallow exact matching on NN searching
		 * simply pass epsilon =std::numeric_limits<float>::epsilon() 
		 * (#include <limits>) 	
		 * Boundcube is the bounding box around the entire dataset
		 */
		const Point3D *findNearest(const Point3D &, const BoundCube &,
		
				float deadDistSqr) const;

		//!Find the nearest N points 
		/*!Finds the nearest N points, that lie outside a 
		 * dead distance of deadDistSqr. k is the number to find
		 */
		void findKNearest(const Point3D & sourcePoint, const BoundCube& treeDomain, 
					unsigned int numNNs, std::vector<const Point3D *> &results,
					float deadDistSqr=0.0f) const;

		//!Textual output of tree. tabs are used to separate different levels of the tree
		/*!The output from this function can be quite large for even modest trees. 
		 * Recommended for debugging only*/
		void dump(std::ostream &) const;
		
		//!Print the number of nodes stored in the tree
		inline unsigned int nodeCount() const{return treeSize;};
};

	

#endif
