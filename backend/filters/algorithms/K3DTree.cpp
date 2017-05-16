/* 
 * K3DTree.cpp : 3D Point KD tree 
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
#include "K3DTree.h"

#include "backend/APT/ionhit.h"

using std::vector;

unsigned int *K3DTree::progress=0;
//Pointer for aborting during build process
const ATOMIC_BOOL *K3DTree::abort=0;

//Axis compare
//==========
AxisCompare::AxisCompare() : axis(0)
{
}

void AxisCompare::setAxis(unsigned int sortAxis)
{
	axis=sortAxis;
}

//==========

//K3D node
//==========

void K3DNode::setLoc(const Point3D &locNew)
{
	loc=locNew;
}

Point3D K3DNode::getLoc() const
{
	return loc;
}

void K3DNode::deleteChildren()
{

	if(childLeft)
	{
		childLeft->deleteChildren();
		delete childLeft;
		childLeft=0;
	}
	
	if(childRight)
	{
		childRight->deleteChildren();
		delete childRight;
		childRight=0;
	}
	

}

void K3DNode::dump(std::ostream &strm, unsigned int depth) const
{
	for(unsigned int ui=0;ui<depth; ui++)
		strm << "\t";

	strm << "(" << loc.getValue(0) 
		<< "," << loc.getValue(1) << "," << loc.getValue(2) 
		<< ")" << std::endl;

	if(childLeft)
		childLeft->dump(strm,depth+1);
	
	if(childRight)
		childRight->dump(strm,depth+1);
}

//===========

//K3D Tree
//=============
K3DTree::K3DTree() : treeSize(0),maxDepth(0),root(0)
{
}


K3DTree::~K3DTree()
{
	kill();
}


/*void K3DTree::verify()
{
	std::stack<K3DNode *> nodeStack;
	std::stack<int> visitStack;

	K3DNode *curNode;
	curNode=root;
	unsigned int visit=0;	
	unsigned int totalVisits;
	totalVisits=1;
	unsigned int measuredDepth=0;

	std::stack<BoundCube> bounds;

	BoundCube curBounds;

	//Set to limits of floating point
	curBounds.setInverseLimits();
	bounds.push(curBounds);

	unsigned int curAxis=0;
	do
	{
		//Check to see what the max. depth of the tree really is
		if(visitStack.size() > measuredDepth)
			measuredDepth=visitStack.size();

		switch(visit)
		{
			//Examine left branch
			case 0:
			{
				//verifyChildren(curNode)
				if(curNode->left())
				{
					curBounds.bounds[curAxis][1] = curNode->getLocVal(curAxis);
					totalVisits++;
					curAxis++;
					
					visitStack.push(1);
					nodeStack.push(curNode);
					
					visit=0;
					curNode=curNode->left();
					ASSERT(curBounds.containsPt(curNode->getLoc()));
					continue;
				}
				visit++;
				break;
			}
			//Examine right branch
			case 1:
			{
				if(curNode->right())
				{
					curBounds.bounds[curAxis][0] = curNode->getLocVal(curAxis);
					totalVisits++;
					curAxis++;
				
					visitStack.push(2);
					nodeStack.push(curNode);
				
					visit=0;
					curNode=curNode->right();
					ASSERT(curBounds.containsPt(curNode->getLoc()));
					continue;
				}
				visit++;
				break;
			}
			//Go up
			case 2:
			{
				curNode=nodeStack.top();
				nodeStack.pop();
				visit=visitStack.top();
				visitStack.pop();
				curAxis--;
				break;
			}
		}
		
	//Keep going until we meet the root node for the third time (one left, one right, one finish)	
	}while(!(curNode==root &&  visit== 2));

	std::cerr << "===COMPARE===" << std::endl;
	std::cerr << " -<<<Walk Results>>>-" << std::endl;
	std::cerr << "Nodes walked  : " << totalVisits << std::endl;
	std::cerr << "Measered Depth: " << measuredDepth << std::endl;

	std::cerr << " -<<<Tree Datas>>>-" << std::endl;
	std::cerr << "Tree reports # nodes: " << treeSize << std::endl;
	std::cerr << "Tree reports Max Depth: " << maxDepth << std::endl;	
}*/

void K3DTree::kill()
{
	if(root)
	{
		root->deleteChildren();
		delete root;
		root=0;
		treeSize=0;
	}
}

//Build the KD tree
void K3DTree::build(vector<Point3D> pts)
{

	ASSERT(progress); // Check progress pointer is inited
	ASSERT(abort); //Check abort pointer is initialised

	//che. to see if the pts vector is empty
	if(!pts.size())
	{
		maxDepth=0;
		return;	
	}

	if(root)
		kill();

	treeSize=pts.size();	
	maxDepth=1;
	root=buildRecurse(pts.begin(), pts.end(),0);
	
}

//Build the KD tree, shuffling original
void K3DTree::buildByRef(vector<Point3D> &pts)
{
	//che. to see if the pts vector is empty
	if(!pts.size())
	{
		maxDepth=0;
		return;	
	}

	if(root)
		kill();

	treeSize=pts.size();	
	maxDepth=1;
	*progress=0;
	curNodeCount=0;
	root=buildRecurse(pts.begin(), pts.end(),0);
}

K3DNode *K3DTree::buildRecurse(vector<Point3D>::iterator pts_start, vector<Point3D>::iterator pts_end, unsigned int depth)
{

	K3DNode *node= new K3DNode;
	unsigned int curAxis=depth%3;
	unsigned int ptsSize=pts_end - pts_start - 1;//pts.size()-1
	node->setAxis(curAxis);
	
	//if we are deeper, then record that
	if(depth > maxDepth)
		maxDepth=depth;
	
	unsigned int median =(ptsSize)/2;

	//set up initial node
	AxisCompare axisCmp;
	axisCmp.setAxis(curAxis);

	//Find the median value in the current axis
	sort(pts_start,pts_end,axisCmp);

	//allocate node (this stores a copy of the point) and set.
	node->setLoc(*(pts_start + median));
	
	if(median)
	{
		
		//Abort recursion if we need to abort
		if(*abort)
			node->setLeft(0);
		else
		{
			//process data as per normal
			node->setLeft(buildRecurse(pts_start,pts_start + median,depth+1));
			*progress= (unsigned int)((float)curNodeCount/(float)treeSize*100.0f);
		}
	}
	else
		node->setLeft(0);	

	if(median!=ptsSize)
	{
		//Only do process if not aborting
		if(*abort)
			node->setRight(0);
		else
		{
			node->setRight(buildRecurse(pts_start + median + 1, pts_end,depth+1));
			*progress= (unsigned int)((float)curNodeCount/(float)treeSize*100.0f);
		}

	}
	else
		node->setRight(0);

	curNodeCount++;
	return node;	

}

void K3DTree::dump( std::ostream &strm) const
{
	if(root)
		root->dump(strm,0);
}


const Point3D *K3DTree::findNearest(const Point3D &searchPt, const BoundCube &domainCube,
									const float deadDistSqr) const
{
	enum { NODE_FIRST_VISIT, //First visit is when you descend the tree
		NODE_SECOND_VISIT, //Second visit is when you come back from ->Left()
		NODE_THIRD_VISIT // Third visit is when you come back from ->Right()
		};


	//The stacks are for the nodes above the current Node.
	const K3DNode *nodeStack[maxDepth+1];
	float domainStack[maxDepth+1][2];
	unsigned int visitStack[maxDepth+1];

	const Point3D *bestPoint;
	const K3DNode *curNode;

	BoundCube curDomain;
	unsigned int visit;
	unsigned int stackTop;
	unsigned int curAxis;
	
	float bestDistSqr;
	float tmpEdge;

	//Set the root as the best estimate
	
	bestPoint =0; 
	bestDistSqr =std::numeric_limits<float>::max();
	curDomain=domainCube;
	visit=NODE_FIRST_VISIT;
	curAxis=0;

	stackTop=0;
	
	curNode=root;	

	do
	{

		switch(visit)
		{
			//Examine left branch
			case NODE_FIRST_VISIT:
			{
				if(searchPt[curAxis] < curNode->getLocVal(curAxis))
				{
					if(curNode->left())
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][1];
						curDomain.bounds[curAxis][1] = curNode->getLocVal(curAxis);
						if(bestPoint && !curDomain.intersects(*bestPoint,bestDistSqr))
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
						curNode=curNode->left();
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	
					}
				}
				else
				{
					if(curNode->right())
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][0];
						curDomain.bounds[curAxis][0] = curNode->getLocVal(curAxis);
						
						if(bestPoint && !curDomain.intersects(*bestPoint,bestDistSqr))
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
						curNode=curNode->right();
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
				if(searchPt[curAxis]< curNode->getLocVal(curAxis))
				{
					if(curNode->right())
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][0];
						curDomain.bounds[curAxis][0] = curNode->getLocVal(curAxis);
						
						if(bestPoint && !curDomain.intersects(*bestPoint,bestDistSqr))
						{
							curDomain.bounds[curAxis][0] = tmpEdge; 
							visit++;
							continue;		
						}
	
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_THIRD_VISIT; //Oh, It will be. It will be.
						domainStack[stackTop][0] = tmpEdge;
						domainStack[stackTop][1]= curDomain.bounds[curAxis][1];
						stackTop++;
						
						//Update the information
						curNode=curNode->right();
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	

					}
				}
				else
				{
					if(curNode->left())
					{
						//Check bounding box when shrunk overlaps best
						//estimate sphere
						tmpEdge= curDomain.bounds[curAxis][1];
						curDomain.bounds[curAxis][1] = curNode->getLocVal(curAxis);
						
						if(bestPoint && !curDomain.intersects(*bestPoint,bestDistSqr))
						{
							curDomain.bounds[curAxis][1] = tmpEdge; 
							visit++;
							continue;		
						}	
						//Preserve our current state.
						nodeStack[stackTop]=curNode;
						visitStack[stackTop] = NODE_THIRD_VISIT; //Oh, It will be. It will be.
						domainStack[stackTop][1] = tmpEdge;
						domainStack[stackTop][0]= curDomain.bounds[curAxis][0];
						stackTop++;
						
						//Update the information
						curNode=curNode->left();
						visit=NODE_FIRST_VISIT;
						curAxis++;
						curAxis%=3;
						continue;	

					}
				}
				visit++;
				//Fall through
			}
			//Go up
			case NODE_THIRD_VISIT:
			{
				float tmpDistSqr;
				tmpDistSqr = curNode->sqrDist(searchPt); 
				if(tmpDistSqr < bestDistSqr &&  tmpDistSqr > deadDistSqr)
				{
					bestDistSqr  = tmpDistSqr;
					bestPoint=curNode->getLocRef();
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
					ASSERT((stackTop)%3 == curAxis)
				}
				
				break;
			}
			default:
				ASSERT(false);


		}
		
	//Keep going until we meet the root nde for the third time (one left, one right, one finish)	
	}while(!(curNode==root &&  visit== NODE_THIRD_VISIT));

	return bestPoint;	

}


void K3DTree::findKNearest(const Point3D &searchPt, const BoundCube &domainCube,
	       			unsigned int num, vector<const Point3D *> &bestPts,
				float deadDistSqr) const
{
	//find the N nearest points
	bestPts.clear();
	bestPts.reserve(num);

	for(unsigned int ui=0; ui<num; ui++)
	{
		const Point3D *p;
		float sqrDist;
		p= findNearest(searchPt, domainCube,
						deadDistSqr);

		if(!p)
			return;
		else
			bestPts.push_back(p);

		sqrDist = p->sqrDist(searchPt);
		deadDistSqr = sqrDist+std::numeric_limits<float>::epsilon();

	}
	
}
//=============
