/* 
 * K3DTree-mk2.cpp : 3D Point KD tree - precise implementation 
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

#include "K3DTree-mk2.h"

#include "backend/APT/ionhit.h"

#include <stack>
#include <queue>

using std::stack;
using std::vector;
using std::pair;

unsigned int *K3DTreeMk2::progress=0;
//Pointer for aborting during build process
ATOMIC_BOOL *K3DTreeMk2::abort=0;

class NodeWalk
{
	public:
		size_t index;
		BoundCube cube;
		unsigned int depth;
		NodeWalk(unsigned int idx, BoundCube bc, unsigned int dpth) : 
			index(idx), cube(bc), depth(dpth) {};
};

void K3DTreeMk2::resetPts(std::vector<Point3D> &p, bool clear)
{
	//Compute bounding box for indexedPoints
	treeBounds.setBounds(p);
	indexedPoints.resize(p.size());

#pragma omp parallel for
	for(size_t ui=0;ui<indexedPoints.size();ui++)
	{
		indexedPoints[ui].first=p[ui];
		indexedPoints[ui].second=ui;
	}

	if(clear)
		p.clear();

	nodes.resize(indexedPoints.size());
}

void K3DTreeMk2::resetPts(std::vector<IonHit> &p, bool clear)
{
	indexedPoints.resize(p.size());
	nodes.resize(p.size());

	if(p.empty())
		return;
	

	//Compute bounding box for indexedPoints
	IonHit::getBoundCube(p,treeBounds);

#pragma omp parallel for
	for(size_t ui=0;ui<indexedPoints.size();ui++)
	{
		indexedPoints[ui].first=p[ui].getPosRef();
		indexedPoints[ui].second=ui;
	}

	if(clear)
		p.clear();

}

void K3DTreeMk2::getBoundCube(BoundCube &b)
{
	ASSERT(treeBounds.isValid());
	b.setBounds(treeBounds);
}

const Point3D *K3DTreeMk2::getPt(size_t index) const
{
	ASSERT(index < indexedPoints.size());
	return &(indexedPoints[index].first);
}

const Point3D &K3DTreeMk2::getPtRef(size_t index) const
{
	ASSERT(index < indexedPoints.size());
	return (indexedPoints[index].first);
}
size_t K3DTreeMk2::getOrigIndex(size_t treeIndex) const 
{
	ASSERT(treeIndex <indexedPoints.size());
	return indexedPoints[treeIndex].second;
}

void K3DTreeMk2::tag(size_t tagID, bool tagVal)
{	
	ASSERT(tagID < nodes.size());
	nodes[tagID].tagged=tagVal;
}

bool K3DTreeMk2::getTag(size_t tagID) const
{
	ASSERT(tagID < nodes.size());
	return nodes[tagID].tagged;
}

size_t K3DTreeMk2::size() const
{
	ASSERT(nodes.size() == indexedPoints.size());
	return indexedPoints.size();
}

bool K3DTreeMk2::build()
{
	ASSERT(progress); // Check progress pointer is inited
	ASSERT(abort); //Check abort pointer is initialised

	using std::make_pair;

	enum
	{
		BUILT_NONE,
		BUILT_LEFT,
		BUILT_BOTH
	};

	
	//Clear any existing tags
	clearAllTags();
	maxDepth=0;

	//No indexedPoints? That was easy.
	if(indexedPoints.empty())
		return true;

	
	ASSERT(treeBounds.isValid());

	//Maintain a stack of nodeoffsets, and whether we have built the left hand side
	stack<pair<size_t,size_t> > limits;
	stack<char> buildStatus;
	stack<size_t> splitStack;

	//Data runs from 0 to size-1 INCLUSIVE
	limits.push(make_pair(0,indexedPoints.size()-1));
	buildStatus.push(BUILT_NONE);
	splitStack.push((size_t)-1);


	AxisCompareMk2 axisCmp;

	size_t numSeen=0; // for progress reporting	
	size_t splitIndex=0;


	
	size_t *childPtr;

#ifdef DEBUG
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		nodes[ui].childLeft=nodes[ui].childRight=(size_t)-2;
	}
#endif
	do
	{

		switch(buildStatus.top())
		{
			case BUILT_NONE:
			{
				//OK, so we have not seen this data at this level before
				int curAxis=(limits.size()-1)%3;
				//First time we have seen this group? OK, we need to sort
				//along its hyper plane.
				axisCmp.setAxis(curAxis);
			
				//Sort data; note that the limits.top().second is the INCLUSIVE
				// upper end.	
				std::sort(indexedPoints.begin()+limits.top().first,
						indexedPoints.begin() + limits.top().second+1,axisCmp);

				//Initially assume that the mid node is the median; then we slide it up 
				splitIndex=(limits.top().second+limits.top().first)/2;
				
				//Keep sliding the split towards the upper boundary until we hit a different
				//data value. This ensure that all data on the left of the sub-tree is <= 
				// to this data value for the specified sort axis
				while(splitIndex != limits.top().second
					&& indexedPoints[splitIndex].first.getValue(curAxis) ==
				       		indexedPoints[splitIndex+1].first.getValue(curAxis))
					splitIndex++;

				buildStatus.top()++; //Increment the build status to "left" case.
						

				if(limits.size() ==1)
				{
					//root node
					treeRoot=splitIndex;
				}
				else
					*childPtr=splitIndex;

				//look to see if there is any left data
				if(splitIndex >limits.top().first)
				{
					//There is; we have to branch again
					limits.push(make_pair(
						limits.top().first,splitIndex-1));
					
					buildStatus.push(BUILT_NONE);
					//Set the child pointer, as we don't know
					//the correct value until the next sort.
					childPtr=&nodes[splitIndex].childLeft;
				}
				else
				{
					//There is not. Set the left branch to null
					nodes[splitIndex].childLeft=(size_t)-1;
				}
				splitStack.push(splitIndex);

				break;
			}

			case BUILT_LEFT:
			{
				//Either of these cases results in use 
				//handling the right branch.
				buildStatus.top()++;
				splitIndex=splitStack.top();
				//Check to see if there is any right data
				if(splitIndex <limits.top().second)
				{
					//There is; we have to branch again
					limits.push(make_pair(
						splitIndex+1,limits.top().second));
					buildStatus.push(BUILT_NONE);

					//Set the child pointer, as we don't know
					//the correct value until the next sort.
					childPtr=&nodes[splitIndex].childRight;
				}
				else
				{
					//There is not. Set the right branch to null
					nodes[splitIndex].childRight=(size_t)-1;
				}

				break;
			}
			case BUILT_BOTH:
			{
				ASSERT(nodes[splitStack.top()].childLeft != (size_t)-2
					&& nodes[splitStack.top()].childRight!= (size_t)-2 );
				maxDepth=std::max(maxDepth,limits.size());
				//pop limits and build status.
				limits.pop();
				buildStatus.pop();
				splitStack.pop();
				ASSERT(limits.size() == buildStatus.size());
				

				numSeen++;
				break;
			}
		}	

		*progress= (unsigned int)((float)numSeen/(float)nodes.size()*100.0f);

		if(*abort)
			return false;

	}while(!limits.empty());

#ifdef DEBUG
	for(unsigned int ui=0;ui<nodes.size();ui++)
	{
		ASSERT(nodes[ui].childLeft != (size_t)-2); 
		ASSERT(nodes[ui].childRight != (size_t)-2); 
	}
#endif

	return true;
}

void K3DTreeMk2::ptsInSphere(const Point3D &origin, float radius,
		vector<size_t> &pts) const
{
	if(!treeBounds.intersects(origin,radius))
		return;

	//parent walking queue. This contains initial parent indices that
	// lie within the sphere.
	const float sqrRadius=radius*radius;
	//contains all completely contained points (these points and children are in sphere)
	std::queue<size_t> idxQueue;
	//queue of points whose children are partly in the sphere
	std::queue<NodeWalk> nodeQueue;
	nodeQueue.push(NodeWalk(treeRoot,treeBounds,0));

	while(!nodeQueue.empty())
	{
		size_t nodeIdx;
		BoundCube curCube;
		unsigned int depth,axis;

		nodeIdx = nodeQueue.front().index;
		curCube=nodeQueue.front().cube;
		depth = nodeQueue.front().depth;
		axis=depth %3;	
		nodeQueue.pop()	;
	

		//obtain the left and right cubes, and see if they
		// -exist
		// -intersect the spehre
		// - if intersects, are they contained entirely by the sphere 
		BoundCube leftCube,rightCube;
		if(nodes[nodeIdx].childLeft != (size_t) -1)
		{
			leftCube=curCube;
			leftCube.bounds[axis][1]=indexedPoints[nodeIdx].first[axis];
			if(leftCube.intersects(origin,sqrRadius) )
			{
				if(leftCube.containedInSphere(origin,sqrRadius))
				{
					ASSERT(indexedPoints[nodeIdx].first.sqrDist(origin) < radius*radius);
					idxQueue.push(nodes[nodeIdx].childLeft);
				}
				else
					nodeQueue.push(NodeWalk(nodes[nodeIdx].childLeft,leftCube,depth+1));
			}
		}	

		if(nodes[nodeIdx].childRight != (size_t) -1)
		{
			rightCube=curCube;
			rightCube.bounds[axis][0]=indexedPoints[nodeIdx].first[axis];
			if(rightCube.intersects(origin,sqrRadius) )
			{
				if(rightCube.containedInSphere(origin,sqrRadius))
				{
					//If the right-hand cube is contained within (origin,radius) sphere, then so are all its chilren

					ASSERT(indexedPoints[nodeIdx].first.sqrDist(origin) < sqrRadius);
					idxQueue.push(nodes[nodeIdx].childRight);
				}
				else
					nodeQueue.push(NodeWalk(nodes[nodeIdx].childRight,rightCube,depth+1));
			}
		}	

		if(indexedPoints[nodeIdx].first.sqrDist(origin) < sqrRadius)
			pts.push_back(nodeIdx);

	}	


	pts.reserve(idxQueue.size());
	//Walk the idx queue to enumerate all children that are in the sphere
	while(!idxQueue.empty())
	{
		size_t curIdx;
		curIdx=idxQueue.front();
		ASSERT(indexedPoints[curIdx].first.sqrDist(origin) < sqrRadius);
		if(nodes[curIdx].childLeft != (size_t)-1)
			idxQueue.push(nodes[curIdx].childLeft);
		
		if(nodes[curIdx].childRight !=(size_t) -1)
			idxQueue.push(nodes[curIdx].childRight);


		ASSERT(curIdx < nodes.size());
		pts.push_back(curIdx);
		idxQueue.pop();
	}
	
}

size_t K3DTreeMk2::findNearestUntagged(const Point3D &searchPt,
				const BoundCube &domainCube, bool shouldTag, size_t pseudoRoot)
{
	//Tree must be built!
	ASSERT(treeRoot < nodes.size() && maxDepth <=nodes.size())
	enum { NODE_FIRST_VISIT, //First visit is when you descend the tree
		NODE_SECOND_VISIT, //Second visit is when you come back from ->Left()
		NODE_THIRD_VISIT // Third visit is when you come back from ->Right()
		};
	
	size_t nodeStack[maxDepth+1];
	float domainStack[maxDepth+1][2];
	unsigned int visitStack[maxDepth+1];

	size_t bestPoint;
	size_t curNode;

	BoundCube curDomain;
	unsigned int visit;
	unsigned int stackTop;
	unsigned int curAxis;
	
	float bestDistSqr;
	float tmpEdge;

	if(nodes.empty())
		return -1;

	bestPoint=(size_t)-1; 
	bestDistSqr =std::numeric_limits<float>::max();
	curDomain=domainCube;
	visit=NODE_FIRST_VISIT;
	curAxis=0;
	stackTop=0;

	//Start at median of array, which is top of tree,
	//by definition, unless an alternative entry point is given
	size_t startNode;
	if(pseudoRoot==(size_t) -1)
		startNode=treeRoot;
	else
		startNode=pseudoRoot;

	curNode=startNode;

	//check start node	
	if(!nodes[curNode].tagged)
	{
		float tmpDistSqr;
		tmpDistSqr = indexedPoints[curNode].first.sqrDist(searchPt); 
		if(tmpDistSqr < bestDistSqr)
		{
			bestDistSqr  = tmpDistSqr;
			bestPoint=curNode;
		}
	}

	do
	{
		switch(visit)
		{
			//Examine left branch
			case NODE_FIRST_VISIT:
			{
				if(searchPt[curAxis] < indexedPoints[curNode].first[curAxis])
				{
					if(nodes[curNode].childLeft!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][1];
						curDomain.bounds[curAxis][1] = indexedPoints[curNode].first[curAxis];
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][1] = tmpEdge; 
							visit++;
							continue;		
						}	
						//Preserve our current state.
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_SECOND_VISIT; //Oh, It will be. It will be.
						domainStack[stackTop][1] = tmpEdge;
						domainStack[stackTop][0]= curDomain.bounds[curAxis][0];
						stackTop++;

						//Update the current information
						curNode=nodes[curNode].childLeft;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;
					}
				}	
				else
				{
					if(nodes[curNode].childRight!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][0];
						curDomain.bounds[curAxis][0] = indexedPoints[curNode].first[curAxis];
						
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][0] =tmpEdge; 
							visit++;
							continue;		
						}	

						//Preserve our current state.
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_SECOND_VISIT; //Oh, It will be. It will be.
						domainStack[stackTop][0] = tmpEdge;
						domainStack[stackTop][1]= curDomain.bounds[curAxis][1];
						stackTop++;

						//Update the information
						curNode=nodes[curNode].childRight;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	
					}
				}
				visit++;
				//Fall through
			}
			//Examine right branch
			case NODE_SECOND_VISIT:
			{
				if(searchPt[curAxis]< indexedPoints[curNode].first[curAxis])
				{
					if(nodes[curNode].childRight!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][0];
						curDomain.bounds[curAxis][0] = indexedPoints[curNode].first[curAxis];
						
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][0] = tmpEdge; 
							visit++;
							continue;		
						}
	
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_THIRD_VISIT; 
						domainStack[stackTop][0] = tmpEdge;
						domainStack[stackTop][1]= curDomain.bounds[curAxis][1];
						stackTop++;
						
						//Update the information
						curNode=nodes[curNode].childRight;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	

					}
				}
				else
				{
					if(nodes[curNode].childLeft!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][1];
						curDomain.bounds[curAxis][1] = indexedPoints[curNode].first[curAxis];
						
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][1] = tmpEdge; 
							visit++;
							continue;		
						}	
						//Preserve our current state.
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_THIRD_VISIT; 
						domainStack[stackTop][1] = tmpEdge;
						domainStack[stackTop][0]= curDomain.bounds[curAxis][0];
						stackTop++;
						
						//Update the information
						curNode=nodes[curNode].childLeft;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	

					}
				}
				visit++;
				//Fall through
			}
			case NODE_THIRD_VISIT:
			{
				//Decide if we should promote the current node
				//to "best" (i.e. nearest untagged) node.
				//To promote, it mustn't be tagged, and it must
				//be closer than cur best estimate.
				if(!nodes[curNode].tagged)
				{
					float tmpDistSqr;
					tmpDistSqr = indexedPoints[curNode].first.sqrDist(searchPt); 
					if(tmpDistSqr < bestDistSqr)
					{
						bestDistSqr  = tmpDistSqr;
						bestPoint=curNode;
					}
				}

				//DEBUG
				ASSERT(stackTop%3 == curAxis)
				//
				if(curAxis)
					curAxis--;
				else
					curAxis=2;


				
				ASSERT(stackTop < maxDepth+1);	
				if(stackTop)
				{
					stackTop--;
					visit=visitStack[stackTop];
					curNode=nodeStack[stackTop];
					curDomain.bounds[curAxis][0]=domainStack[stackTop][0];
					curDomain.bounds[curAxis][1]=domainStack[stackTop][1];
					ASSERT((stackTop)%3 == curAxis);
				}
			
				break;
			}
			default:
				ASSERT(false);


		}
		

	//Keep going until we meet the root node for the third time (one left, one right, one finish)	
	}while(!(curNode== startNode &&  visit== NODE_THIRD_VISIT));

	if(bestPoint != (size_t) -1)
		nodes[bestPoint].tagged|=shouldTag;
	return bestPoint;	

}

size_t K3DTreeMk2::findNearestWithSkip(const Point3D &searchPt,
				const BoundCube &domainCube, const std::set<size_t> &skipPts, size_t pseudoRoot) const
{
	//Tree must be built!
	ASSERT(treeRoot < nodes.size() && maxDepth <=nodes.size())
	enum { NODE_FIRST_VISIT, //First visit is when you descend the tree
		NODE_SECOND_VISIT, //Second visit is when you come back from ->Left()
		NODE_THIRD_VISIT // Third visit is when you come back from ->Right()
		};
	
	size_t nodeStack[maxDepth+1];
	float domainStack[maxDepth+1][2];
	unsigned int visitStack[maxDepth+1];

	size_t bestPoint;
	size_t curNode;

	BoundCube curDomain;
	unsigned int visit;
	unsigned int stackTop;
	unsigned int curAxis;
	
	float bestDistSqr;
	float tmpEdge;

	if(nodes.empty())
		return -1;

	bestPoint=(size_t)-1; 
	bestDistSqr =std::numeric_limits<float>::max();
	curDomain=domainCube;
	visit=NODE_FIRST_VISIT;
	curAxis=0;
	stackTop=0;

	//Start at median of array, which is top of tree,
	//by definition, unless an alternative entry point is given
	size_t startNode;
	if(pseudoRoot==(size_t) -1)
		startNode=treeRoot;
	else
		startNode=pseudoRoot;

	curNode=startNode;

	//check start node and that we have not seen this already	
	if(!(nodes[curNode].tagged  || (skipPts.find(curNode) !=skipPts.end() )) )
	{
		float tmpDistSqr;
		tmpDistSqr = indexedPoints[curNode].first.sqrDist(searchPt); 
		if(tmpDistSqr < bestDistSqr)
		{
			bestDistSqr  = tmpDistSqr;
			bestPoint=curNode;
		}
	}

	do
	{
		switch(visit)
		{
			//Examine left branch
			case NODE_FIRST_VISIT:
			{
				if(searchPt[curAxis] < indexedPoints[curNode].first[curAxis])
				{
					if(nodes[curNode].childLeft!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][1];
						curDomain.bounds[curAxis][1] = indexedPoints[curNode].first[curAxis];
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][1] = tmpEdge; 
							visit++;
							continue;		
						}	
						//Preserve our current state.
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_SECOND_VISIT; //Oh, It will be. It will be.
						domainStack[stackTop][1] = tmpEdge;
						domainStack[stackTop][0]= curDomain.bounds[curAxis][0];
						stackTop++;

						//Update the current information
						curNode=nodes[curNode].childLeft;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;
					}
				}	
				else
				{
					if(nodes[curNode].childRight!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][0];
						curDomain.bounds[curAxis][0] = indexedPoints[curNode].first[curAxis];
						
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][0] =tmpEdge; 
							visit++;
							continue;		
						}	

						//Preserve our current state.
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_SECOND_VISIT; //Oh, It will be. It will be.
						domainStack[stackTop][0] = tmpEdge;
						domainStack[stackTop][1]= curDomain.bounds[curAxis][1];
						stackTop++;

						//Update the information
						curNode=nodes[curNode].childRight;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	
					}
				}
				visit++;
				//Fall through
			}
			//Examine right branch
			case NODE_SECOND_VISIT:
			{
				if(searchPt[curAxis]< indexedPoints[curNode].first[curAxis])
				{
					if(nodes[curNode].childRight!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][0];
						curDomain.bounds[curAxis][0] = indexedPoints[curNode].first[curAxis];
						
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][0] = tmpEdge; 
							visit++;
							continue;		
						}
	
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_THIRD_VISIT; 
						domainStack[stackTop][0] = tmpEdge;
						domainStack[stackTop][1]= curDomain.bounds[curAxis][1];
						stackTop++;
						
						//Update the information
						curNode=nodes[curNode].childRight;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	

					}
				}
				else
				{
					if(nodes[curNode].childLeft!=(size_t)-1)
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][1];
						curDomain.bounds[curAxis][1] = indexedPoints[curNode].first[curAxis];
						
						if(!curDomain.intersects(searchPt,bestDistSqr))
						{
							curDomain.bounds[curAxis][1] = tmpEdge; 
							visit++;
							continue;		
						}	
						//Preserve our current state.
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_THIRD_VISIT; 
						domainStack[stackTop][1] = tmpEdge;
						domainStack[stackTop][0]= curDomain.bounds[curAxis][0];
						stackTop++;
						
						//Update the information
						curNode=nodes[curNode].childLeft;
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	

					}
				}
				visit++;
				//Fall through
			}
			case NODE_THIRD_VISIT:
			{
				//Decide if we should promote the current node
				//to "best" (i.e. nearest untagged) node.
				//To promote, it mustn't be tagged, and it must
				//be closer than cur best estimate.
				if(!(nodes[curNode].tagged || (skipPts.find(curNode) !=skipPts.end()) ) )
				{
					float tmpDistSqr;
					tmpDistSqr = indexedPoints[curNode].first.sqrDist(searchPt); 
					if(tmpDistSqr < bestDistSqr)
					{
						bestDistSqr  = tmpDistSqr;
						bestPoint=curNode;
					}
				}

				//DEBUG
				ASSERT(stackTop%3 == curAxis)
				//
				if(curAxis)
					curAxis--;
				else
					curAxis=2;


				
				ASSERT(stackTop < maxDepth+1);	
				if(stackTop)
				{
					stackTop--;
					visit=visitStack[stackTop];
					curNode=nodeStack[stackTop];
					curDomain.bounds[curAxis][0]=domainStack[stackTop][0];
					curDomain.bounds[curAxis][1]=domainStack[stackTop][1];
					ASSERT((stackTop)%3 == curAxis);
				}
			
				break;
			}
			default:
				ASSERT(false);


		}
		

	//Keep going until we meet the root node for the third time (one left, one right, one finish)	
	}while(!(curNode== startNode &&  visit== NODE_THIRD_VISIT));

	return bestPoint;	

}

void K3DTreeMk2::getTreesInSphere(const Point3D &pt, float sqrDist, const BoundCube &domainCube,
					vector<pair<size_t,size_t> > &contiguousBlocks ) const
{
	using std::queue;
	using std::pair;
	using std::make_pair;

	if(treeRoot == (size_t) -1)
		return;
	
	queue<int> nodeQueue;
	queue<int> axisQueue;
	queue<BoundCube> boundQueue;

	queue<pair<int,int> > limitQueue;


	nodeQueue.push(treeRoot);
	boundQueue.push(domainCube);
	axisQueue.push(0);
	limitQueue.push(make_pair(0,nodes.size()-1));	
	do
	{
		BoundCube tmpCube;
		tmpCube=boundQueue.front();

		ASSERT(nodeQueue.size() == boundQueue.size() &&
			nodeQueue.size() == axisQueue.size() &&
			nodeQueue.size() == limitQueue.size());
		
		//There are three cases here.
		//	- KD limits for this subdomain
		//	   wholly contained by sphere
		//	   	> contiguous subtree is interior.
		//	- KD Limits for this subdomain partially
		//	    contained by sphere.
		//	    	> some subtree may be interior -- refine.
		//	- Does not intersect, do nothing.

		if(tmpCube.containedInSphere(pt,sqrDist))
		{
			//We are? Interesting. We must be a contiguous block from our lower
			//to upper limits
			contiguousBlocks.push_back(limitQueue.front());
		}
		else if(tmpCube.intersects(pt,sqrDist))
		{
			size_t axis,curNode;
			//Not wholly contained... but our kids might be!
			axis=axisQueue.front();
			curNode=nodeQueue.front();

			if(nodes[curNode].childLeft !=(size_t)-1)
			{
				//Construct left hand domain
				tmpCube=boundQueue.front();
				//Set upper bound
				tmpCube.bounds[axis][1] = indexedPoints[curNode].first[axis];
				
				if(tmpCube.intersects(pt,sqrDist))
				{
					//We have to examine left child.
					nodeQueue.push(nodes[curNode].childLeft);
					boundQueue.push(tmpCube);
					limitQueue.push(make_pair(
						limitQueue.front().first,curNode-1));
					axisQueue.push((axis+1)%3);
				}
			}
			
			if(nodes[curNode].childRight !=(size_t)-1)
			{
				//Construct right hand domain
				tmpCube=boundQueue.front();
				//Set lower bound
				tmpCube.bounds[axis][0] = indexedPoints[curNode].first[axis];
				
				if(tmpCube.intersects(pt,sqrDist))
				{
					//We have to examine right child.
					nodeQueue.push(nodes[curNode].childRight);
					boundQueue.push(tmpCube);
					limitQueue.push(make_pair(curNode+1,limitQueue.front().second ));
					axisQueue.push((axis+1)%3);
				}
			}
		}
	
		axisQueue.pop();
		limitQueue.pop();
		boundQueue.pop();
		nodeQueue.pop();	
	}
	while(!nodeQueue.empty());

}

size_t K3DTreeMk2::getBoxInTree(const BoundCube &box) const
{
	ASSERT(treeRoot !=(size_t)-1);

	BoundCube curB;
	curB=treeBounds;
	int curNode=treeRoot;
	int curAxis=0;	

	//user-supplied box can overlap tree area (and thus not contain the box, by loop test)
	// intersect the box with the tree bounds, such that it fits
	BoundCube subBox;
	subBox = curB.makeUnion(box);

	//If our box-to-find fits inside the current bounds,
	// keep refining our search area
	while(curB.contains(subBox))
	{
		//Check for the tree's split axis
		float axisPosition;
		axisPosition=  indexedPoints[curNode].first[curAxis];
		switch(box.segmentTriple(curAxis,axisPosition))
		{
			//query axis is below box - move lower bound up, by searching right child
			case 0:
			{
				curB.setBound(curAxis, 0,axisPosition);
				if(nodes[curNode].childRight == (size_t) -1)
					return curNode;
				curNode=nodes[curNode].childRight;
				break;
			}
			//intersects
			case 1:
				//Nothing we can do any more - return current node as new pseudo-root
				return curNode; 
			//query axis is above target box - move upper bound down, and refine along left child
			case 2:
			{
				curB.setBound(curAxis,1,axisPosition);
				if(nodes[curNode].childLeft == (size_t) -1)
					return curNode;
				curNode=nodes[curNode].childLeft;
				break;
			}
			default:
				ASSERT(false);
				
		}
	
		curAxis++;
		curAxis%=3;
	}

	
	return curNode;
}

size_t K3DTreeMk2::tagCount() const
{
	size_t count=0;
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		if(nodes[ui].tagged)
			count++;
	
	}

	return count;
}

void K3DTreeMk2::clearTags(std::vector<size_t> &tagsToClear)
{
#pragma omp parallel for
	for(size_t ui=0;ui<tagsToClear.size();ui++)
		nodes[tagsToClear[ui]].tagged=false;
}

void K3DTreeMk2::clearAllTags()
{
#pragma omp parallel for
	for(size_t ui=0;ui<nodes.size();ui++)
		nodes[ui].tagged=false;
}


#ifdef DEBUG



bool K3DMk2Tests()
{
	vector<Point3D> pts;

	K3DTreeMk2 tree;
	
	//First test with single point
	//--
	pts.push_back(Point3D(0,0,0));
	tree.resetPts(pts,false);


	//build 
	TEST(tree.build(),"Tree build");
	
	Point3D searchPt=Point3D(1,0,0);
	BoundCube dummyCube;
	tree.getBoundCube(dummyCube);

	size_t resultIdx;
	
	resultIdx=tree.findNearestUntagged(searchPt,dummyCube,false);
	//Only one point to find - should find it
	TEST(resultIdx == 0,"K3D Mk2, single point test");

	//Get the contiguous nodes
	BoundCube testBox;
	testBox.setBounds(Point3D(-2,-2,-2),Point3D(2,2,2));

	TEST(tree.getBoxInTree(testBox) == 0,"subtree test");
	//---

	//Now, try adding more points
	//---
	pts.push_back(Point3D(1,1,1));
	pts.push_back(Point3D(1.1,0.9,0.95));
	
	tree.resetPts(pts,false);
	TEST(tree.build(),"Tree build");
	
	testBox.setBounds(Point3D(1.05,0.5,0.5),Point3D(1.5,1.5,1.5));
	TEST(tree.getBoxInTree(testBox)==2,"subtree test pt2");
	//---

	return true;

}

#endif
