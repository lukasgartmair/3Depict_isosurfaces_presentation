/*
 * 	filtertree.cpp - Filter tree topology and data propagation handling
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



#include "filtertree.h"
#include "filters/allFilter.h"

#include "common/xmlHelper.h"
#include "common/stringFuncs.h"

using std::string;

enum
{
	CACHE_DEPTH_FIRST=1,
	CACHE_NEVER,
};

//Unlock helper class for toggling a  boolean value at exit
class AutoUnlocker
{
	private:
		bool *lockBool;
	public:
		AutoUnlocker(bool *b) { lockBool= b; *lockBool=true;}
		~AutoUnlocker() { *lockBool=false;}
};



//Simple garbage collector for FilterTree::refresh
// does not have to be efficient, as it is assumed that this is not a bottleneck
class FilterRefreshCollector
{
	private:
		//Pile of lists of pointers that we are tracking
		vector<list<const FilterStreamData *> > nodes;

		//List of pointers we should *not* erase
		set<const FilterStreamData *> forgottenNodes;

		//Find out if a filter tracks itself or not
		static bool tracksSelf(const FilterStreamData *p) { return p->cached;}

#ifdef DEBUG
		void checkSanity();
#endif

	public:
		FilterRefreshCollector() {};
		~FilterRefreshCollector() ;
	
		//Add pointers that are to be garbage collected, if they don't maintain
		// their own ownership
		void trackPointers(const vector<const FilterStreamData *> &vecPtrs);

		//Stop tracking the specified pointers
		void forgetPointers(const vector<const FilterStreamData *> &vecPtrs);

		//clean up all pointers in list
		void collectAll();
		unsigned int getLevel() const { return nodes.size();}
		void collectToLevel(unsigned int level);

#ifdef DEBUG
		bool  isTracked(const FilterStreamData *ptr) const;
#endif	
};

#ifdef DEBUG
void FilterRefreshCollector::checkSanity()
{
	//Should never have a duplicate - flatten object to find out
	set<const FilterStreamData *> s;
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		for(list<const FilterStreamData *>::iterator it=nodes[ui].begin();
				it!=nodes[ui].end(); ++it)
		{
			//Should never have something that tracks itself
			ASSERT(!tracksSelf(*it) )
			//Check that we have not already inserted this
			ASSERT(s.find(*it) == s.end())
			s.insert(*it);

			ASSERT(forgottenNodes.find(*it) == forgottenNodes.end());
		}
	}
	s.clear();

}

bool FilterRefreshCollector::isTracked(const FilterStreamData *ptr) const
{
	for(unsigned int ui=0;ui<nodes.size();ui++)
	{
		for(list<const FilterStreamData *>::const_iterator it=nodes[ui].begin();
			it!=nodes[ui].end(); ++it)
		{
			if(*it == ptr)
				return true;
		}
	}

	return false;
}
#endif

FilterRefreshCollector::~FilterRefreshCollector()
{
#ifdef DEBUG
	checkSanity();
#endif
	collectAll();
}

void FilterRefreshCollector::trackPointers(const vector<const FilterStreamData *> &v)
{
	//Just remember the pointers we want to track
	list<const FilterStreamData *> lKeep;
	for(size_t ui=0;ui<v.size();ui++)
	{
		if(!tracksSelf(v[ui]))
		{
			bool found;
			found=false;
			for(size_t uj=0;uj<nodes.size();uj++)
			{
				if(std::find(nodes[uj].begin(),nodes[uj].end(),v[ui]) != nodes[uj].end())
				{
					found=true;
					break;
				}

			}

			found|=(forgottenNodes.find(v[ui])  != forgottenNodes.end());

			if(!found)
				lKeep.push_back(v[ui]);
		}
	}
	nodes.push_back(lKeep);
#ifdef DEBUG
	checkSanity();
#endif

}

void FilterRefreshCollector::forgetPointers(const vector<const FilterStreamData *> &v)
{
#ifdef DEBUG
	checkSanity();
#endif
	for(size_t uj=0;uj<v.size();uj++)
	{
		for(size_t ui=0;ui<nodes.size();ui++)
		{
			list<const FilterStreamData *>::iterator it;
			it=std::find(nodes[ui].begin(),nodes[ui].end(),v[uj]) ;
			if(it != nodes[ui].end())
			{
				forgottenNodes.insert(*it);
				//We deleted the source of this, no need to continue
				// checking for this particular pointer.
				nodes[ui].erase(it);
				break;
			}
		}
	}

}

void FilterRefreshCollector::collectAll()
{
#ifdef DEBUG
	checkSanity();
#endif
	for(size_t ui=0;ui< nodes.size() ;ui++)
	{
		for(list<const FilterStreamData*>::iterator it=nodes[ui].begin(); it!=nodes[ui].end(); ++it)
			delete *it;
	}

	nodes.clear();
}


void FilterRefreshCollector::collectToLevel(unsigned int level)
{
#ifdef DEBUG
	checkSanity();
#endif
	for(size_t ui=level+1;ui<nodes.size();ui++)
	{
		for(list<const FilterStreamData*>::iterator it=nodes[ui].begin(); it!=nodes[ui].end(); ++it)
			delete *it;
	}

	nodes.resize(level);
}



const float DEFAULT_MAX_CACHE_PERCENT=50;

void popPointerStack(std::stack<vector<const FilterStreamData * > > &inDataStack, 
							unsigned int depth)
{

	while(inDataStack.size() > depth)
	{
		//We no longer need this level
		inDataStack.pop();
	}
}

FilterTree::FilterTree()
{
	maxCachePercent=DEFAULT_MAX_CACHE_PERCENT;
	cacheStrategy=CACHE_DEPTH_FIRST;
	amRefreshing=false;
}

FilterTree::~FilterTree()
{
	clear();
}

FilterTree::FilterTree(const FilterTree &orig) :
	cacheStrategy(orig.cacheStrategy), maxCachePercent(orig.maxCachePercent),
	filters(orig.filters)
{
	//Don't grab a direct copy of the tree, but rather an cloned duplicate,
	// without the internal cache data
	for(tree<Filter *>::pre_order_iterator it=filters.begin();
		it!=filters.end();++it)
		(*it)=(*it)->cloneUncached();
	
	amRefreshing=false;
}

size_t FilterTree::depth(const tree<Filter*>::pre_order_iterator &it) const
{
	ASSERT(std::find(filters.begin(),filters.end(),*it)!=filters.end());
	return filters.depth(it);
}

void FilterTree::swap(FilterTree &other) 
{
	std::swap(cacheStrategy,other.cacheStrategy);
	std::swap(maxCachePercent,other.maxCachePercent);
	std::swap(filters,other.filters);
}

const FilterTree &FilterTree::operator=(const FilterTree &orig)
{
	ASSERT(!amRefreshing)
	clear();

	cacheStrategy=orig.cacheStrategy;
	maxCachePercent=orig.maxCachePercent;

	//Make a duplicate of the filter pointers from the other tree
	// we will overwrite them in a second
	filters=orig.filters;

	//Don't grab a direct copy of the tree, but rather an cloned duplicate,
	// without the internal cache data.
	// No need to free here, as the orig tree still has the pointers
	for(tree<Filter *>::pre_order_iterator it=filters.begin();
		it!=filters.end();++it)
	{
		(*it)=(*it)->cloneUncached();
	}

	return *this;
}

size_t FilterTree::maxDepth() const
{
	return filters.max_depth();
}

void FilterTree::initFilterTree() const
{
	vector< const FilterStreamData *> curData;
	stack<vector<const FilterStreamData * > > inDataStack;
	
	FilterRefreshCollector refreshCollector;

	//Do not allow stack to empty
	inDataStack.push(curData);
	refreshCollector.trackPointers(curData);

	//Depth-first search from root node, refreshing filters as we proceed.
	for(tree<Filter * >::pre_order_iterator filtIt=filters.begin();
					filtIt!=filters.end(); ++filtIt)
	{
		//Step 0 : Pop the cache until we reach our current level, 
		//	deleting any pointers that would otherwise be lost.
		//---
		size_t popLevel;
		popLevel=filters.depth(filtIt)+1;
		popPointerStack(inDataStack,popLevel);
		refreshCollector.collectToLevel(popLevel);
		ASSERT(refreshCollector.getLevel() == inDataStack.size());
		//---
			
		//Step 1: Take the stack top, and turn it into "curdata" using the filter
		//	record the result on the stack
		//---
		//Take the stack top, filter it and generate "curData"
		(*filtIt)->initFilter(inDataStack.top(),curData);

#ifdef DEBUG
		//Peform some quick sanity checks
		for(unsigned int ui=0;ui<curData.size(); ui++)
		{
			//Pointer should be nonzero
			ASSERT(curData[ui]);
			
			//Caching is *Forbidden* in filter initialisation
			ASSERT(!curData[ui]->cached);
		}
#endif
		
		
		//Step 2: Put output in the intermediary stack, 
		//so it is available for any other children at this level.
		inDataStack.push(curData);

		//Track pointers for garbage collection
		refreshCollector.trackPointers(curData);

		curData.clear();
		//---
	}
}

void FilterTree::clear() 
{
	for(tree<Filter *>::iterator filterIt=filters.begin();
			filterIt!=filters.end();++filterIt)
		delete (*filterIt);

	filters.clear();
}

void FilterTree::getAccumulatedPropagationMaps(map<Filter*, size_t> &emitTypes, map<Filter*,size_t> &blockTypes) const
{
	//Build the  emit type map. This describes
	//what possible types can be emitted at any point in the tree.
	for(tree<Filter *>::iterator it=filters.begin_breadth_first();
					it!=filters.end_breadth_first(); ++it)
	{
		//FIXME: HACK -- why does the BFS not terminate correctly?
		if(!filters.is_valid(it))
			break;

		size_t curEmit;
		//Root node is special, does not combine with the
		//previous filter
		if(!filters.depth(it))
			curEmit=(*it)->getRefreshEmitMask();
		else
		{
			//Normal child. We need to remove any types that
			//are blocked (& (~blocked)), then add any types that are emitted
			//(|)
			curEmit=emitTypes[*(filters.parent(it))];
			curEmit&=(~(*it)->getRefreshBlockMask() )& STREAMTYPE_MASK_ALL;
			curEmit|=(*it)->getRefreshEmitMask();
		}

		ASSERT(curEmit < STREAMTYPE_MASK_ALL);
		emitTypes.insert(make_pair(*it,curEmit));

	}
	


	
	//Build the accumulated block map;  this describes
	//what types, if emitted, will NOT be propagated to the final output
	//Nor affect any downstream filters

	//TODO: Why not implement as  a reverse BFS?? Would be more efficient...
	for(size_t ui=filters.max_depth()+1; ui; )
	{
		ui--;

		for(tree<Filter * >::iterator it=filters.begin(); it!=filters.end(); ++it)
		{
			//Check to see if we are at the correct depth
			if(filters.depth(it) != ui)
				continue;


			//Initially assume that everything is passed through
			//filter
			int blockMask=0x0;


			if((*it)->haveCache())
			{
				//Loop over the children of this filter, grab their block masks
				for(tree<Filter *>::sibling_iterator itJ=it.begin(); itJ!=it.end(); ++itJ)
				{

					if((*itJ)->haveCache())
					{
						int curBlockMask;
						curBlockMask=(*itJ)->getRefreshBlockMask();
						blockMask= (blockMask & curBlockMask);

					}
					else
					{
						blockMask&=0;
						//The only reason to keep looping is to
						//alter the blockmask. If it is at any point zero,
						//then the answer will be zero, due to the & operator.
						break;
					}

				}

				//OK, so we now know which filters the children will ALL block.
				//Combine this with our block list for this filter, and we this will give ush
				//the blocklist for this subtree section
				blockMask|=(*it)->getRefreshBlockMask();
			}
			else
				blockMask=0;

			blockTypes.insert(make_pair(*it,blockMask));
		}

	}



}


void FilterTree::getFilterRefreshStarts(vector<tree<Filter *>::iterator > &propStarts) const
{

	if(!filters.size())
		return;

	const bool STUPID_ALGORITHM=false;
	if(STUPID_ALGORITHM)
	{
		//Stupid version
		//start at root every time
		propStarts.push_back(filters.begin());
	}
	else
	{
		//Do something hopefully non-stupid. Here we examine the types of data that are
		//propagated through the tree, and which filters emit, or block transmission
		//of any given type (ie their output is influenced only by certain data types).

		//From this information, and the cache status of each filter
		//(recall caches only cache data generated inside the filter), it is possible to
		//skip certain initial element refreshes.

		//Block and emit adjuncts for tree
		map<Filter *, size_t> accumulatedEmitTypes,accumulatedBlockTypes;
		getAccumulatedPropagationMaps(accumulatedEmitTypes,accumulatedBlockTypes);

		vector<tree<Filter *>::iterator > seedFilts;


		//BUild a filter->iterator mapping
		map<Filter *,tree<Filter *>::iterator > leafMap;
		for(tree<Filter*>::leaf_iterator  it=filters.begin_leaf();
				it!=filters.end_leaf(); ++it)
			leafMap[*it]=it;


		for(tree<Filter *>::iterator it=filters.begin_breadth_first();
		it!=filters.end_breadth_first(); ++it)
		{
			//FIXME: HACK -- why does the BFS not terminate correctly?
			if(!filters.is_valid(it))
				break;

			//Check to see if we have an insertion point above us.
			//if so, we cannot press on, as we have determined that
			//we must start higher up.
			// (TODO : Just terminate child enumeration for BFS 
			// for seed filter iterators, instead of this hack-ish method
			bool isChildFilt;
			isChildFilt=false;
			for(unsigned int ui=0; ui<seedFilts.size(); ui++)
			{
				if(isChild(filters,seedFilts[ui],it))
				{
					isChildFilt=true;
					continue;
				}
			}

			if(isChildFilt)
				continue;

			//If we are a leaf, and not a child of a seed,
			//then we have to do our work, or nothing will be generated
			//so check that
			if(leafMap.find(*it) != leafMap.end())
			{
				seedFilts.push_back(it);
				continue;
			}


			//Check to see if we can use these children as insertion
			//points in the tree
			//i.e., ask, "Do all subtrees block everything we emit from here?"
			int emitMask,blockMask;
			emitMask=accumulatedEmitTypes[*it];
			blockMask=~0;
			for(tree<Filter *>::sibling_iterator itJ=filters.begin(it); itJ!=filters.end(it); ++itJ)
				blockMask&=accumulatedBlockTypes[*itJ];





			if( emitMask & ((~blockMask) & STREAMTYPE_MASK_ALL))
			{

				//Oh noes! we don't block, we will have to stop here,
				// for this subtree. We cannot go further down.
				seedFilts.push_back(it);
			}

		}

		propStarts.swap(seedFilts);
	}

#ifdef DEBUG
	for(unsigned int ui=0; ui<propStarts.size(); ui++)
	{
		for(unsigned int uj=ui+1; uj<propStarts.size(); uj++)
		{
			//Check for uniqueness
			ASSERT(propStarts[ui] !=propStarts[uj]);


			//Check for no-parent relation (either direction)
			ASSERT(!isChild(filters,propStarts[ui],propStarts[uj]) &&
			       !isChild(filters,propStarts[uj],propStarts[ui]));
		}
	}
#endif



}

void FilterTree::getConsoleMessagesToNodes(std::vector<tree<Filter *>::iterator > &nodes,
			std::vector<pair<const Filter*,string> > &messages) const
{


	//obtain a unique list of all filters who are parents of the nodes
	if(!nodes.size())
		return;

	std::set<Filter*> filterSet;
	for(unsigned int ui=0;ui<nodes.size();ui++)
	{
		tree<Filter*>::iterator it;
		it=nodes[ui];

		if(it==filters.begin())
			filterSet.insert(*it);
		
		while(filters.is_valid(it) && 
			filters.parent(it)!= filters.begin())
		{
			filterSet.insert(*it);	
			it=filters.parent(it);
		}
	}

	filterSet.insert(*(filters.begin()));


	//now loop through the filters and obtain the console messages
	for(set<Filter *>::iterator it=filterSet.begin();it!=filterSet.end();++it)
	{
		vector<string> tmpMsgs;
		(*it)->getConsoleStrings(tmpMsgs);

		for(size_t ui=0;ui<tmpMsgs.size();ui++)
		{
			messages.push_back(make_pair(*it,tmpMsgs[ui]));
		}
	}
}

unsigned int FilterTree::refreshFilterTree(list<FILTER_OUTPUT_DATA > &outData, 
		std::vector<SelectionDevice *> &devices,
		vector<pair<const Filter* , string> > &consoleMessages,
	       	ProgressData &curProg, ATOMIC_BOOL &abortRefresh) const
{

	//initially, we should not want to abort refreshing 
	ASSERT(!abortRefresh);
	//Tell the filter system about our abort flag
	Filter::wantAbort=&abortRefresh;


	//Lock the refresh state.
	AutoUnlocker unlocker(&amRefreshing);
	
	unsigned int errCode=0;

	if(!filters.size())
		return 0;	

	//Destroy any caches that belong to monitored filters that need
	//refreshing. Failing to do this can lead to filters being skipped
	//during the refresh 
	for(tree<Filter *>::iterator filterIt=filters.begin();
			filterIt!=filters.end();++filterIt)
	{
		//We need to clear the cache of *all* 
		//downstream filters, as otherwise
		//their cache's could block our update.
		if((*filterIt)->monitorNeedsRefresh())
		{
			for(tree<Filter *>::pre_order_iterator it(filterIt);it!= filters.end(); ++it)
			{
				//Do not traverse siblings
				if(filters.depth(filterIt) >= filters.depth(it) && it!=filterIt )
					break;
			
				(*it)->clearCache();
			}
		}
	}


	initFilterTree();

	// -- Build data streams --	
	vector< const FilterStreamData *> curData;
	stack<vector<const FilterStreamData * > > inDataStack;
	FilterRefreshCollector refreshCollector;

	//Push some dummy data onto the stack to prime first-pass (call to refresh(..) requires stack
	//size to be non-zero)
	inDataStack.push(curData);


	std::set<Filter*> leafFilters;
	for(tree<Filter*>::leaf_iterator  it=filters.begin_leaf();
			it!=filters.end_leaf(); ++it)
		leafFilters.insert(*it);
	

	//Keep redoing the refresh until the user stops fiddling with the filter tree.
	vector<tree<Filter *>::iterator> baseTreeNodes;
	baseTreeNodes.clear();

	//Find the minimal starting locations for the refresh - eg. we can skip certain filters
	// depending upon filter cache status and dependency data, and just start from sub-nodes
	getFilterRefreshStarts(baseTreeNodes);
	curProg.totalNumFilters=countChildFilters(filters,baseTreeNodes)+baseTreeNodes.size();



	for(unsigned int itPos=0;itPos<baseTreeNodes.size(); itPos++)
	{
		ASSERT(!curData.size());

		refreshCollector.collectAll();
		refreshCollector.trackPointers(curData);

		//Depth-first search from root node, refreshing filters as we proceed.
		for(tree<Filter * >::pre_order_iterator filtIt=baseTreeNodes[itPos];
						filtIt!=filters.end(); ++filtIt)
		{
			//Check to see if this node is a child of the base node.
			//if not, move on.
			if( filtIt!= baseTreeNodes[itPos] &&
				!isChild(filters,baseTreeNodes[itPos],filtIt))
				continue;

			Filter *currentFilter;
			currentFilter=*filtIt;

			//Step 0 : Pop the cache until we reach our current level, 
			//	delete any pointers that would otherwise be lost.
			//	Recall that the zero size in the stack may not correspond to the
			//	tree root, but rather corresponds to the filter we started refreshing from
			//---
			size_t popLevel;
			popLevel=filters.depth(filtIt) - filters.depth(baseTreeNodes[itPos])+1;
			popPointerStack(inDataStack,popLevel);
			refreshCollector.collectToLevel(popLevel);
			//---

			//Step 1: Set up the progress system
			//---
			curProg.clock();
			curProg.curFilter=currentFilter;	
			//---
		
		

			//Step 2: Check if we should cache this filter or not.
			//Get the number of bytes that the filter expects to use
			//---
			if(!currentFilter->haveCache())
			{
				unsigned long long cacheBytes;
				if(inDataStack.empty())
					cacheBytes=currentFilter->numBytesForCache(0);
				else
					cacheBytes=currentFilter->numBytesForCache(numElements(inDataStack.top()));

				if(cacheBytes != (unsigned long long)(-1))
				{
					//As long as we have caching enabled, let us cache according to the
					//selected strategy
					switch(cacheStrategy)
					{
						case CACHE_NEVER:
							currentFilter->setCaching(false);
							break;
						case CACHE_DEPTH_FIRST:
						{
							float ramFreeForUse;
							ramFreeForUse= maxCachePercent/(float)100.0f*getAvailRAM();

							bool cache;
							cache=((float)cacheBytes/(1024*1024) ) < ramFreeForUse;

							currentFilter->setCaching( cache);
							break;
						}
					}
				}
				else
					currentFilter->setCaching(false);
			}
			//---

			//Step 3: Take the stack top, and turn it into "curdata" and refresh using the filter.
			//	Record the result on the stack.
			//	We also record any Selection devices that are generated by the filter.
			//	This is the guts of the system.
			//---
			//

			if(!currentFilter->haveCache())
				currentFilter->clearConsole();

			currentFilter->clearDevices();

			curProg.maxStep=curProg.step=1;
			curProg.filterProgress=0;

			//Take the stack top, filter it and generate "curData"
			try
			{

				errCode=currentFilter->refresh(inDataStack.top(),curData,curProg);
			}
			catch(std::bad_alloc)
			{
				//Should catch bad mem cases in filter, wherever possible
				WARN(false,"Memory exhausted during refresh");
				errCode=FILTERTREE_REFRESH_ERR_MEM;
			}

#ifdef DEBUG
			//Perform sanity checks on filter output
			checkRefreshValidity(curData,currentFilter);
			ASSERT(curProg.step == curProg.maxStep || errCode);
			//when completing, we should have full progress 
			std::string progWarn = std::string("Progress did not reach 100\% for filter: ");
			progWarn+=currentFilter->getUserString();
			
			WARN( (curProg.filterProgress == 100 || errCode),progWarn.c_str());
#endif
			//Ensure that (1) yield is called, regardless of what filter does
			//(2) yield is called after 100% update	
			curProg.filterProgress=100;	


			vector<SelectionDevice *> curDevices;
			//Retrieve the user interaction "devices", and send them to the scene
			currentFilter->getSelectionDevices(curDevices);

			//Add them to the total list of devices
			for(size_t ui=0;ui<curDevices.size();ui++)
				devices.push_back(curDevices[ui]);

			curDevices.clear();

			//Retrieve any console messages from the filter
			vector<string> tmpMessages;
			currentFilter->getConsoleStrings(tmpMessages);
			//Accumulate the messages
			consoleMessages.reserve(consoleMessages.size()+tmpMessages.size());
			for(size_t ui=0;ui<tmpMessages.size();ui++)
				consoleMessages.push_back(make_pair(currentFilter,tmpMessages[ui]));

			//check for any error in filter update (including user abort)
			if(errCode || abortRefresh)
			{
				//clear any intermediary pointers
				popPointerStack(inDataStack,0);
				ASSERT(inDataStack.empty());
				
				//remove duplicates, as more than one output data may
				// output the same pointer
				std::set<const FilterStreamData *> uniqSet;
				for(list<FILTER_OUTPUT_DATA>::iterator it=outData.begin();
						it!=outData.end();++it)
				{
					for(size_t ui=0;ui<it->second.size();ui++)
						uniqSet.insert(it->second[ui]);
				}

	
				//Clean up the output that we didn't use
				for(std::set<const FilterStreamData *>::iterator it=uniqSet.begin();
					it!=uniqSet.end(); ++it)
				{
					const FilterStreamData *data;
					data = *it;
					//Output data is uncached - it is our job to delete it
					if(!data->cached)
						delete data;
				}
				if(abortRefresh)
					return FILTER_ERR_ABORT;
				return errCode;
			}


			//Update the filter output statistics, e.g. num objects of each type output 
			currentFilter->updateOutputInfo(curData);
			
			
			
			//If this is not a leaf, keep track of intermediary pointers
			if(leafFilters.find(currentFilter)== leafFilters.end())
			{
				//The filter will generate a list of new pointers. If any out-going data 
				//streams are un-cached, track them
				refreshCollector.trackPointers(curData);
				
				//Put this in the intermediary stack, 
				//so it is available for any other children at this level.
				inDataStack.push(curData);
			}
			else if(curData.size())
			{
				//The filter has created an output. Record it for passing to updateScene
				outData.push_back(make_pair(currentFilter,curData));
				refreshCollector.forgetPointers(curData);
			}	
			//Cur data is recorded either in outData or on the data stack
			curData.clear();
			//---
			
		}

	}
	
	popPointerStack(inDataStack,0);
	//Clean up any remaining intermediary pointers
	refreshCollector.collectAll();

	//====Output scrubbing ===

	//Should be no duplicate pointers in output data.
	//(this makes preventing double frees easier, and
	// minimises unnecessary output)
	//Construct a single list of all pointers in output,
	//checking for uniqueness. Delete duplicates
	
	std::set<const FilterStreamData *> uniqueSet;
	for(list<FILTER_OUTPUT_DATA>::iterator it=outData.begin();it!=outData.end(); )
	{

		vector<const FilterStreamData *>::iterator itJ;
		itJ=it->second.begin();
		while(itJ!=it->second.end()) 
		{
			//Each stream data pointer should only occur once in the entire lot.
			if(uniqueSet.find(*itJ) == uniqueSet.end())
			{
				uniqueSet.insert(*itJ);
				++itJ;
			}
			else
			{
				itJ=it->second.erase(itJ);
			}
		}

		if(it->second.empty())
			it=outData.erase(it);
		else
			++it;
	}
	//======

	return 0;
}

string FilterTree::getRefreshErrString(unsigned int code)
{
	
	const char *REFRESH_ERR_STRINGS[] = {"",
		"Insufficient memory for refresh",
		};
	
	unsigned int delta=code-FILTERTREE_REFRESH_ERR_BEGIN;

	return string(REFRESH_ERR_STRINGS[delta]);
}

bool FilterTree::setFilterProperty(Filter *targetFilter, unsigned int key,
				const std::string &value, bool &needUpdate)
{
	ASSERT(std::find(filters.begin(),filters.end(),targetFilter) != filters.end());
	if(!targetFilter->setProperty(key,value,needUpdate))
		return false;

	//If we no longer have a cache, and the filter needs an update, then we must
	//modify the downstream objects
	if(needUpdate)
	{
		for(tree<Filter * >::pre_order_iterator filtIt=filters.begin();
						filtIt!=filters.end(); ++filtIt)
		{
			if(targetFilter == *filtIt)
			{
				//Kill all cache below filtIt
				for(tree<Filter *>::pre_order_iterator it(filtIt);it!= filters.end(); ++it)
				{
					//Do not traverse siblings
					if(filters.depth(filtIt) >= filters.depth(it) && it!=filtIt )
						break;

					//Do not clear the cache for the target filter. 
					//This is the responsibility of the setProperty function for the filter
					if(*it !=targetFilter)
						(*it)->clearCache();
				}
				break;
			}
		}

	}

	initFilterTree();
	return true;

}

void FilterTree::serialiseToStringPaths(std::map<const Filter *, string > &serialisedPaths) const
{

	stack<string> pathStack;
	pathStack.push("");

	set<string> enumeratedPaths;

	//Unlikely text string that can be appended to tree path
	const char *PATH_NONCE="$>";

	unsigned int lastDepth=0;
	for(tree<Filter *>::iterator filterIt=filters.begin();
			filterIt!=filters.end();++filterIt)
	{
		//if this is a new depth, pop the stack until
		// we hit the correct level
		unsigned int curDepth;
		curDepth=depth(filterIt);
		//Add one for base element
		while(pathStack.size() > curDepth +1)
		{
			pathStack.pop();
			lastDepth--;
		}

		std::string testPath;
		testPath = pathStack.top() + string("/")  + (*filterIt)->typeString();

		unsigned int nonceIncrement;
		nonceIncrement=0;
		while(enumeratedPaths.find(testPath) != enumeratedPaths.end())
		{
			std::string tailStr;
			nonceIncrement++;
			stream_cast(tailStr,nonceIncrement);

			//Keep trying new path with nonce
			testPath=pathStack.top()+(*filterIt)->typeString() + string(PATH_NONCE) + tailStr;
		}

		enumeratedPaths.insert(testPath);
		const Filter *f;
		f=(const Filter*)(*filterIt);
		serialisedPaths.insert(make_pair(f,testPath));

		pathStack.push(testPath);

	}

	ASSERT(serialisedPaths.size() == filters.size());
}


void FilterTree::serialiseToStringPaths(map<string, const Filter * > &serialisedPaths) const
{
	//Build one-way mapping
	map<const Filter *, string> singleMap;
	serialiseToStringPaths(singleMap);


	serialisedPaths.clear();
	for(map<const Filter*,string>::iterator it=singleMap.begin();
		it!=singleMap.end();++it)
	{
		ASSERT(serialisedPaths.find(it->second) == serialisedPaths.end());
		serialisedPaths[it->second]=it->first;	
	}

}

unsigned int FilterTree::loadXML(const xmlNodePtr &treeParent, std::ostream &errStream,const std::string &stateFileDir)

{
	clear();
	
	//Parse the filter tree in the XML file.
	//generating a filter tree
	bool inTree=true;
	tree<Filter *>::iterator lastFilt=filters.begin();
	tree<Filter *>::iterator lastTop=filters.begin();
	stack<tree<Filter *>::iterator> treeNodeStack;

	xmlNodePtr nodePtr = treeParent->xmlChildrenNode;


	//push root tag	
	std::stack<xmlNodePtr>  nodeStack;
	nodeStack.push(nodePtr);

	bool needCleanup=false;
	while (inTree)
	{
		//Jump to the next XML node at this depth
		if (XMLHelpNextType(nodePtr,XML_ELEMENT_NODE))
		{
			//If there is not one, pop the tree stack
			if (!treeNodeStack.empty())
			{
				//Pop the node stack for the XML and filter trees.
				nodePtr=nodeStack.top();
				nodeStack.pop();
				lastFilt=treeNodeStack.top();
				treeNodeStack.pop();
			}
			else
			{
				//Did we run out of stack?
				//then we have finished the tree.
				inTree=false;
			}
			continue;
		}

		Filter *newFilt;
		bool nodeUnderstood;
		newFilt=0;
		nodeUnderstood=true; //assume by default we understand, and set false if not

		//If we encounter a "children" node. Then we need to look at the children of this filter
		if (!xmlStrcmp(nodePtr->name,(const xmlChar*)"children"))
		{
			//Can't have children without parent
			if (!filters.size())
			{
				needCleanup=true;
				break;
			}

			//Child node should have its own child
			if (!nodePtr->xmlChildrenNode)
			{
				needCleanup=true;
				break;
			}

			nodeStack.push(nodePtr);
			treeNodeStack.push(lastFilt);

			nodePtr=nodePtr->xmlChildrenNode;
			continue;
		}
		else
		{
			//Well, its not  a "children" node, so it could
			//be a filter... Lets find out
			std::string tmpStr;
			tmpStr=(char *)nodePtr->name;

			if(isValidFilterName(tmpStr))
			{
				newFilt=makeFilter(tmpStr);
				if (!newFilt->readState(nodePtr->xmlChildrenNode,stateFileDir))
				{
					needCleanup=true;
					break;
				}
			}
			else
			{
				errStream << TRANS("WARNING: Skipping node ") << (const char *)nodePtr->name << TRANS(" as it was not recognised") << endl;
				nodeUnderstood=false;
			}
		}


		//Skip this item
		if (nodeUnderstood)
		{
			ASSERT(newFilt);

			//Add the new item the tree
			if (filters.empty())
				lastFilt=filters.insert(filters.begin(),newFilt);
			else
			{
				if (!treeNodeStack.empty())
					lastFilt=filters.append_child(treeNodeStack.top(),newFilt);
				else
				{
					lastTop=filters.insert(lastTop,newFilt);
					lastFilt=lastTop;
				}


			}

		}
	}


	//All good?
	if(!needCleanup)
		return 0;

	//OK, we hit an error, we need to delete any pointers on the
	//cleanup list
	if(nodePtr)
		errStream << TRANS("Error processing node: ") << (const char *)nodePtr->name << endl;

	clear();	

	//No good..
	return 1;

}

bool FilterTree::saveXML(std::ofstream &f,std::map<string,string> &fileMapping, 
					bool writePackage, bool useRelativePaths, unsigned int minTabDepth) const
{
	set<string> existingFiles;

	f << tabs(minTabDepth+1) << "<filtertree>" << endl;
	//Depth-first search, enumerate all filters in depth-first fashion
	unsigned int depthLast=0;
	unsigned int child=0;
	for(tree<Filter * >::pre_order_iterator filtIt=filters.begin();
					filtIt!=filters.end(); ++filtIt)
	{
		unsigned int depth;
		depth = filters.depth(filtIt);
		if(depth >depthLast)
		{
			while(depthLast++ < depth)
			{
				f << tabs(minTabDepth+depthLast+1);
				f  << "<children>" << endl; 
				child++;
			}
		}
		else if (depth < depthLast)
		{
			while(depthLast-- > depth)
			{
				f << tabs(minTabDepth+depthLast+2);
				f  << "</children>" << endl; 
				child--;
			}
		}

		//If we are writing a package, override the filter storage values
		if(writePackage || useRelativePaths)
		{
			vector<string> valueOverrides;
			(*filtIt)->getStateOverrides(valueOverrides);

			//The overrides, at the moment, only are files. 
			//So lets find them & move them
			for(unsigned int ui=0;ui<valueOverrides.size();ui++)
			{
				string newFilename;
				newFilename=string("./") + onlyFilename(valueOverrides[ui]);

				//resolve naming clashes (eg if we had /path1/file.pos and /path2/file.pos, we need to ensure
				// these are named such that we dont collide
				//--
				unsigned int offset=0;
				string a,b,c;
				splitFileData(newFilename,a,b,c);
				while(existingFiles.find(newFilename) != existingFiles.end())
				{
					std::string s;
					stream_cast(s,offset);
					newFilename = a + b + "-" + s+ "." + c;
					offset++;	
				}

				//record the new choice for filename, so we can check for future collisions
				existingFiles.insert(newFilename);
				//--
				
				map<string,string>::const_iterator it;
				it =fileMapping.find(valueOverrides[ui]);
				
				if(it == fileMapping.end()) 
				{
					//map does not exist, so make it!
					fileMapping[newFilename]=valueOverrides[ui];
				}
				else if (it->second !=valueOverrides[ui])
				{
					//Keep adding a prefix until we find a valid new filename
					while(fileMapping.find(newFilename) != fileMapping.end())
						newFilename="remap"+newFilename;
					//map does not exist, so make it!
					fileMapping[newFilename]=valueOverrides[ui];
				}
				valueOverrides[ui] = newFilename;
			}			
		
			if(!(*filtIt)->writePackageState(f,STATE_FORMAT_XML,valueOverrides,depth+2))
				return false;
		}
		else
		{
			if(!(*filtIt)->writeState(f,STATE_FORMAT_XML,depth+2))
				return false;
		}
		depthLast=depth;
	}

	//Close out filter tree.
	while(child--)
	{
		f << tabs(minTabDepth+child+2) << "</children>" << endl;
	}
	f << tabs(minTabDepth+1) << "</filtertree>" << endl;

	return true;
}
	


bool FilterTree::hasHazardousContents() const
{
	//Check the filter system for "hazardous" contents.
	// each filter defines what it believes is "hazardous"
	for(tree<Filter * >::pre_order_iterator it=filters.begin();
					it!=filters.end(); ++it)
	{
		if ((*it)->canBeHazardous())
			return true;
	}

	return false;
}

void FilterTree::stripHazardousContents()
{
	for(tree<Filter * >::pre_order_iterator it=filters.begin();
					it!=filters.end(); ++it)
	{
		if ((*it)->canBeHazardous())
		{
			//delete filters from this branch
			for(tree<Filter *>::pre_order_iterator itj(it); itj!=filters.end(); ++itj)
				delete *itj;
	

			//nuke this branch
			it=filters.erase(it);
			--it;
		}
	}

}

bool FilterTree::isChild(const tree<Filter *> &treeInst,
				const tree<Filter *>::iterator &testParent,
				tree<Filter *>::iterator testChild) 
{
	// NOTE: A comparison against tree root (treeInst.begin())is INVALID
	// for trees that have multiple base nodes.
	while(treeInst.depth(testChild))
	{
		testChild=treeInst.parent(testChild);
		
		if(testChild== testParent)
			return true;
	}

	return false;
}

bool FilterTree::contains(const Filter *f) const
{
	return std::find(filters.begin(),filters.end(),f) != filters.end();
}

size_t FilterTree::countChildFilters(const tree<Filter *> &treeInst,
			const vector<tree<Filter *>::iterator> &nodes)
{
	set<Filter*> childIts;
	for(size_t ui=0;ui<nodes.size();ui++)
	{
		for(tree<Filter*>::pre_order_iterator it=nodes[ui];
				it!=treeInst.end();++it)
			childIts.insert(*it);

	}

	return childIts.size()-nodes.size();
}


#ifdef DEBUG
void FilterTree::checkRefreshValidity(const vector< const FilterStreamData *> &curData, 
							const Filter *refreshFilter) const
{

	//Filter outputs should
	//	- never be null pointers.
	for(size_t ui=0; ui<curData.size(); ui++)
	{
		ASSERT(curData[ui]);
	}

	//Filter outputs should have a parent that exists somewhere in the tree
	for(size_t ui=0;ui<curData.size();ui++)
	{
		ASSERT(contains(curData[ui]->parent));
	}

	//Filter outputs should
	//	- never contain duplicate pointers
	for(size_t ui=0; ui<curData.size(); ui++)
	{
		for(size_t uj=ui+1; uj<curData.size(); uj++)
		{
			ASSERT(curData[ui]!= curData[uj]);
		}
	}


	//Filter outputs should
	//	- only use valid stream types
	//	- Not contain zero sized point streams
	for(size_t ui=0; ui<curData.size(); ui++)
	{
		const FilterStreamData *f;
		f=(curData[ui]);

		//No stream type mask bits, other than valid stream types,  should be set
		ASSERT( (f->getStreamType() & ~( STREAMTYPE_MASK_ALL)) == 0);


		switch(f->getStreamType())
		{
			case STREAM_TYPE_IONS:
			{
				const IonStreamData *ionData;
				ionData=((const IonStreamData *)f);

				ASSERT(ionData->data.size());
				break;
			}
			default:
				;
		}

	}

	//Filter outputs should
	//	- Always have isCached set to 0 or 1.
	//	- Filter should report that it has a cache, if it is emitting cached objects
	bool hasSomeCached=false;
	for(size_t ui=0; ui<curData.size(); ui++)
	{
		ASSERT(curData[ui]->cached == 1 ||
				curData[ui]->cached == 0);

		if(curData[ui]->parent == refreshFilter)
			hasSomeCached|=curData[ui]->cached;
	}

	ASSERT(!(hasSomeCached == false && refreshFilter->haveCache()));

	//Filter outputs for this filter should 
	//	 -only be from those specified in filter emit mask
	for(size_t ui=0; ui<curData.size(); ui++)
	{
		if(!curData[ui]->parent)
		{
			cerr << "Warning: orphan filter stream (FilterStreamData::parent == 0)." <<
				"This must be set when creating new filter streams in the ::refresh function for the filter." << endl;
			cerr << "Filter :"  << refreshFilter->getUserString() << "Stream Type: " << 
				STREAM_NAMES[getBitNum(curData[ui]->getStreamType())] << endl;
		}
		else if(curData[ui]->parent == refreshFilter)
		{
			//Check we emitted something that our parent's emit mask said we should
			// by performing bitwise ops 
			ASSERT(curData[ui]->getStreamType() & 
				refreshFilter->getRefreshEmitMask());
		}
	}

	//plot output streams should only have known types
	//for various identifiers
	for(size_t ui=0; ui<curData.size(); ui++)
	{
		if(curData[ui]->getStreamType() != STREAM_TYPE_PLOT)
			continue;

		const PlotStreamData *p;
		p =(const PlotStreamData*)curData[ui];

		p->checkSelfConsistent();
	}
	
	//Voxel output streams should only have known types
	for(size_t ui=0; ui<curData.size(); ui++)
	{
		if(curData[ui]->getStreamType() != STREAM_TYPE_VOXEL)
			continue;

		const VoxelStreamData *p;
		p =(const VoxelStreamData*)curData[ui];

		//Must have valid representation 
		ASSERT(p->representationType< VOXEL_REPRESENT_END);
	}

	//Ensure that any output drawables that are selectable have
	// parent filters with selection devices
	for(size_t ui=0; ui<curData.size();ui++)
	{
		if(curData[ui]->getStreamType() != STREAM_TYPE_DRAW)
			continue;

		const DrawStreamData *p;
		p =(const DrawStreamData*)curData[ui];
	
		for(size_t uj=0;uj<p->drawables.size();uj++)
		{
			if ( p->drawables[uj]->canSelect )
			{
				vector<SelectionDevice *> devices;
				p->parent->getSelectionDevices(devices);
				ASSERT(devices.size());

				for(size_t uk=0;uk<devices.size();uk++)
				{
					ASSERT(devices[uk]->getNumBindings());
				}
				
				//Drawables with selection devices cannot be cached
				ASSERT(!p->cached);
			}
		
		}

	}

}
#endif

void FilterTree::safeDeleteFilterList( std::list<FILTER_OUTPUT_DATA> &outData, 
						size_t typeMask, bool maskPrevents) 
{
	//Loop through the list of vectors of filterstreamdata, then drop any elements that are deleted
	for(list<FILTER_OUTPUT_DATA> ::iterator it=outData.begin(); 
							it!=outData.end(); ) 
	{
		vector<bool> killV;
		killV.resize(it->second.size(),false);
		//Note the No-op at the loop iterator. this is needed so we can safely .erase()
		for(size_t ui=0;ui<it->second.size();ui++)
		{
			const FilterStreamData* f;
			f= it->second[ui];
			//Don't operate on streams if we have a nonzero mask, and the (mask is active XOR mask mode)
			//NOTE: the XOR flips the action of the mask. if maskprevents is true, then this logical switch
			//prevents the masked item from being deleted. If not, ONLY the masked types are deleted.
			//In any case, a zero mask makes this whole thing not do anything, and everything gets deleted.
			if(typeMask && ( ((bool)(f->getStreamType() & typeMask)) ^ !maskPrevents)) 
				continue;
			
			//Output data is uncached - delete it
			if(!f->cached)
				delete f;
	
			killV[ui]=true;
		}


		vectorMultiErase(it->second,killV);

		//Check to see if this element still has any items in its vector. if not,
		//then discard the element
		if(!(it->second.size()))
			it=outData.erase(it);
		else
			++it;

	}
}

void FilterTree::getFiltersByType(std::vector<const Filter *> &filtersOut, unsigned int type) const
{
	for(tree<Filter * >::iterator it=filters.begin(); 
						it!=filters.end(); ++it)
	{
		if((*it)->getType() == type)
			filtersOut.push_back(*it);
	}
}

void FilterTree::purgeCache()
{
	for(tree<Filter *>::iterator it=filters.begin();it!=filters.end();++it)
		(*it)->clearCache();
}

bool FilterTree::hasStateOverrides() const
{
	for(tree<Filter *>::iterator it=filters.begin(); it!=filters.end(); ++it)
	{
		vector<string> overrides;
		(*it)->getStateOverrides(overrides);

		if(overrides.size())
			return true;
	}

	return false;
}


void FilterTree::addFilter(Filter *f,const Filter *parentFilter)
{
	if(parentFilter)
	{
		tree<Filter *>::iterator it= std::find(filters.begin(),filters.end(),parentFilter);

		ASSERT(it != filters.end());

		//Add the child to the tree
		filters.append_child(it,f);
	}
	else
	{
		if(filters.empty())
			filters.insert(filters.begin(),f);
		else
			filters.insert_after(filters.begin(),f);
	}

	//Topology has changed, notify filters
	initFilterTree();
}

void FilterTree::addFilterTree(FilterTree &f, const Filter *parent)
{
	//The insert_subtree and insert_subtree_after algorithms
	// apparently work across multiple trees, I think, after examining tree::merge
	if(parent)
	{
		tree<Filter*>::pre_order_iterator it;
		it=std::find(filters.begin(),filters.end(),parent);
		ASSERT(it!=filters.end());

		it=filters.append_child(it,0);
		filters.insert_subtree(it,f.filters.begin());
		filters.erase(it);
	}
	else
	{
		if(f.size())
		{
			if(filters.empty())
				filters.insert_subtree(filters.begin(),f.filters.begin());
			else
				filters.insert_subtree_after(filters.begin(),f.filters.begin());
		}
	}

	f.filters.clear();
}

bool FilterTree::copyFilter(Filter *toCopy,const Filter *newParent)
{
	//Copy a filter child to a different filter child
	if(newParent)
	{

		ASSERT(toCopy && newParent && 
			!(toCopy==newParent));

		//Look for both newparent and sibling iterators	
		tree<Filter *>::iterator moveFilterIt,parenterIt;
		moveFilterIt=std::find(filters.begin(),filters.end(),toCopy);
		parenterIt=std::find(filters.begin(),filters.end(),newParent);
		
		ASSERT(moveFilterIt !=filters.end() &&
			parenterIt != filters.end());

		if(parenterIt == moveFilterIt)
			return false;




		//ensure that we are not trying to move a parent filter to one
		// of its children
		if(isChild(filters,moveFilterIt,parenterIt))
			return false;
		
		//Move the "tomove" filter, and its children to be a child of the
		//newly nominated parent (DoCS* "adoption" you might say.) 
		//*DoCs : Department of Child Services (bad taste .au joke)
		//Create a temporary tree and copy the contents into here
		tree<Filter *> tmpTree;
		tree<Filter *>::iterator node= tmpTree.insert(tmpTree.begin(),0);
		tmpTree.replace(node,moveFilterIt); //Note this doesn't kill the original
		
		//Replace each of the filters in the temporary_tree with a clone of the original
		for(tree<Filter*>::iterator it=tmpTree.begin();it!=tmpTree.end(); ++it)
			*it= (*it)->cloneUncached();

		//In the original tree, create a new null node
		node = filters.append_child(parenterIt,0);
		//Replace the node with the tmpTree's contents
		filters.replace(node,tmpTree.begin()); 

		
		initFilterTree();
		return parenterIt != moveFilterIt;
	}
	else
	{
		//copy a selected base of the tree to a new base component

		//Look for both newparent and sibling iterators	
		bool found = false;
		tree<Filter *>::iterator moveFilterIt;
		for(tree<Filter * >::iterator it=filters.begin(); 
							it!=filters.end(); ++it)
		{
			if(*it == toCopy)
			{
				moveFilterIt=it;
				found=true;
				break;
			}
		}

		if(!found)
			return false;

		//Create a temporary tree and copy the contents into here
		tree<Filter *> tmpTree;
		tree<Filter *>::iterator node= tmpTree.insert(tmpTree.begin(),0);
		tmpTree.replace(node,moveFilterIt); //Note this doesn't kill the original
		
		//Replace each of the filters in the temporary_tree with a clone of the original
		for(tree<Filter*>::iterator it=tmpTree.begin();it!=tmpTree.end(); ++it)
			*it= (*it)->cloneUncached();

		//In the original tree, create a new null node
		node = filters.insert_after(filters.begin(),0);
		//Replace the node with the tmpTree's contents
		filters.replace(node,tmpTree.begin()); 
		initFilterTree();
		return true;
	}

	ASSERT(false);
}


void FilterTree::removeSubtree(Filter *removeFilt)
{
	ASSERT(removeFilt);	

	//Remove element and all children
	for(tree<Filter * >::pre_order_iterator filtIt=filters.begin();
					filtIt!=filters.end(); ++filtIt)
	{
		if(removeFilt == *filtIt)
		{

			for(tree<Filter *>::pre_order_iterator it(filtIt);it!= filters.end(); ++it)
			{
				//Do not traverse siblings
				if(filters.depth(filtIt) >= filters.depth(it) && it!=filtIt )
					break;
				
				//Delete the children filters.
				delete *it;
			}

			//Remove the children from the tree
			filters.erase_children(filtIt);
			filters.erase(filtIt);
			break;
		}

	}

	//Topology has changed, notify filters
	initFilterTree();
}

void FilterTree::cloneSubtree(FilterTree &f,const Filter *targetFilt) const
{
	ASSERT(!f.filters.size()); //Should only be passing empty trees
	
	tree<Filter *>::iterator targetIt=std::find(filters.begin(),filters.end(),targetFilt);
	//Filter should exist.
	ASSERT(targetIt!=filters.end());

	tree<Filter *>::iterator node= f.filters.insert(f.filters.begin(),0);
	f.filters.replace(node,targetIt); //Note this doesn't kill the original


	//Replace each of the filters in the output tree with a clone of the original
	//rather than the actual subtree
	for(tree<Filter*>::iterator it=f.filters.begin();it!=f.filters.end(); ++it)
		*it= (*it)->cloneUncached();

	
}

void FilterTree::setCachePercent(unsigned int newCache)
{
	ASSERT(newCache <= 100);
	if(!newCache)
		cacheStrategy=CACHE_NEVER;
	else
	{
		cacheStrategy=CACHE_DEPTH_FIRST;
		maxCachePercent=newCache;
	}
}

bool FilterTree::hasUpdates() const
{
	for(tree<Filter *>::iterator it=filters.begin();it!=filters.end();++it)
	{
		if((*it)->monitorNeedsRefresh())
			return true;
	}

	return false;
}


bool FilterTree::reparentFilter(Filter *f, const Filter *newParent)
{

	ASSERT(f && !(f==newParent));

	tree<Filter*>::iterator replaceNode,parentFilterIt ;
	tree<Filter *>::iterator moveFilterIt=filters.end();
	//If we are moving to the base, then that is a special case.
	if(!newParent)
	{
		moveFilterIt=std::find(filters.begin(),filters.end(),f);
	}
	else
	{

		//Look for both newparent and sibling iterators	
		bool found[2] = {false,false};
		for(tree<Filter * >::iterator it=filters.begin(); 
							it!=filters.end(); ++it)
		{
			if(!found[0])
			{
				if(*it == f)
				{
					moveFilterIt=it;
					found[0]=true;
				}
			}
			if(!found[1])
			{
				if(*it == newParent)
				{
					parentFilterIt=it;
					found[1]=true;
				}
			}

			if(found[0] && found[1] )
				break;
		}
	
		ASSERT(parentFilterIt!=moveFilterIt);	
		ASSERT(found[0] && found[1] );
		
		//ensure that this is actually a parent-child relationship, and not the other way around!
		for(tree<Filter *>::pre_order_iterator it(moveFilterIt);it!= filters.end(); ++it)
		{
			//Do not traverse siblings
			if(filters.depth(moveFilterIt) >= filters.depth(it) && it!=moveFilterIt )
				break;
			
			if(it == parentFilterIt)
				return false;
		}

	}
	
	ASSERT(moveFilterIt!=filters.end());

	//clear the cache of filters
	//----
	//clear children
	for(tree<Filter *>::pre_order_iterator it(moveFilterIt);it!= filters.end(); ++it)
	{
		//Do not traverse siblings
		if(filters.depth(moveFilterIt) == filters.depth(it))
			continue;
		(*it)->clearCache();
	}

	//Erase the cache of moveFilterIt, and then move it to a new location
	(*moveFilterIt)->clearCache();
	if(!newParent)	
	{
		//create a dummy node, ready to be replaced 
		replaceNode=filters.insert_after(filters.begin(),0);
	}
	else
	{
		//Set the new target location to replace	
		replaceNode= filters.append_child(parentFilterIt,0);
	}
	//----

	//Create a dummy node after this parent
	//This doesn't actually nuke the original subtree, but rather copies it, 
	//replacing the dummy node.
	filters.replace(replaceNode,moveFilterIt); 
	//Nuke the original subtree
	filters.erase(moveFilterIt);
	//--------

	//Topology of filter tree has changed.
	//some filters may need to know about this	
	initFilterTree();

	return true;
}


void FilterTree::clearCache(const Filter *filter,bool includeSelf)
{
	if(!filter)
	{
		//Invalidate everything
		for(tree<Filter * >::iterator it=filters.begin(); 
						it!=filters.end(); ++it)
			(*it)->clearCache();
	}
	else
	{
		//Find the filter in the tree
		tree<Filter *>::iterator filterIt;	
		for(tree<Filter * >::iterator it=filters.begin(); 
							it!=filters.end(); ++it)
		{
			if(*it == filter)
			{
				filterIt=it;
				break;
			}
		}

		for(tree<Filter *>::pre_order_iterator it(filterIt);it!= filters.end(); ++it)
		{
			//Do not traverse siblings
			if(filters.depth(filterIt) >= filters.depth(it) && it!=filterIt )
				break;

			//If we dont want to include self,  then skip
			if( !includeSelf && *it == filter)
				continue;

			(*it)->clearCache();
		}
	}
}

void FilterTree::clearCacheByType(unsigned int type)
{
	//Build a list of all filters who we need to invalidate
	// Note that we cannot do this directly on the filter ptr,
	// as we also need to invalidate children, so re-use the clearCache function
	for(tree<Filter * >::iterator it=filters.begin(); 
					it!=filters.end(); ++it)
	{
		if((*it)->getType() == type)
			clearCache(*it);
	}

}

size_t FilterTree::cacheCount(unsigned int typeMask) const
{
	size_t count=0;
	for(tree<Filter * >::iterator it=filters.begin(); 
					it!=filters.end(); ++it)
	{
		if(((*it)->getType() & typeMask) && (*it)->haveCache())
			count++;
	}

	return count;
}
		
void FilterTree::modifyRangeFiles(const map<const RangeFile *, const RangeFile *> &toModify)
{
	for(tree<Filter *>::iterator it=filters.begin();it!=filters.end();++it)
	{
		//TODO: refactor to introduce filter->hasRange () ? 
		if((*it)->getType() != FILTER_TYPE_RANGEFILE)
			continue;

		RangeFileFilter *rngFilt=(RangeFileFilter* )(*it);

		const RangeFile *r = &(rngFilt->getRange());
		if( toModify.find(r) == toModify.end() )
			continue;
		
		const RangeFile *modRng =toModify.at(r);
		rngFilt->setRangeData(*modRng);

		//Erase all downstream objects' caches
		clearCache(rngFilt,true);
	}

}
