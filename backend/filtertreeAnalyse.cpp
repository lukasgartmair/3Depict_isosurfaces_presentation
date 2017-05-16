/*
 *	filtertreeAnalyse.cpp - Performs correctness checking of filter trees
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

#include "filtertreeAnalyse.h"



//Needed to obtain filter data keys
//----
#include "filters/allFilter.h"
//----

#include <numeric>

bool filterIsSampling(const Filter *f)
{
	bool affectsSampling=false;


	FilterPropGroup props;
	f->getProperties(props);



	switch(f->getType())
	{
		case FILTER_TYPE_DATALOAD:
		{
			//Check if load limiting is on
			//Not strictly true. If data file is smaller (in MB) than this number
			// (which we don't know here), then this will be false.
			if(props.hasProp(DATALOAD_KEY_SAMPLE))
				affectsSampling = (props.getPropValue(DATALOAD_KEY_SAMPLE).data!= "0");
			else
				affectsSampling=false;
			break;
		}
		case FILTER_TYPE_IONDOWNSAMPLE:
		{
			FilterProperty p;
			if(props.hasProp(KEY_IONDOWNSAMPLE_FIXEDOUT))
			{
				p=props.getPropValue(KEY_IONDOWNSAMPLE_FIXEDOUT);
				//If using  fixed output mode, then
				// we may affect the output ion density
				// if the count is low. How low? 
				// We don't know with the information to hand...
				affectsSampling=(p.data== "1");
			}
			else
			{
				//If randomly sampling, then we are definitely affecting the results
				//if we are not including every ion
				if(props.hasProp(KEY_IONDOWNSAMPLE_FRACTION))
				{
					p=props.getPropValue(KEY_IONDOWNSAMPLE_FRACTION);
					float sampleFrac;
					stream_cast(sampleFrac,p.data);
					affectsSampling=(sampleFrac < 1.0f);
				}
				else
					affectsSampling=false;
			}

			break;
		}
	}


	return affectsSampling;
}

bool affectedBySampling(const Filter *f, bool haveRngParent)
{
	FilterPropGroup props;
	f->getProperties(props);
	
	bool affected=false;
	//See if filter is configured to affect spatial analysis
	switch(f->getType())
	{
		case FILTER_TYPE_CLUSTER_ANALYSIS:
		{
			affected=haveRngParent;
			break;
		}
		case FILTER_TYPE_PROFILE:
		{
			FilterProperty p;
			p=props.getPropValue(PROFILE_KEY_NORMALISE);

			//If using normalise mode, and we do not have a range parent
			//then filter is in "density" plotting mode, which is affected by
			//this analysis
			affected= (p.data== "1" && !haveRngParent);
			break;
		}
		case FILTER_TYPE_SPATIAL_ANALYSIS:
		{
			affected=true;
			break;
		}
	}

	return affected;
}

bool needsRangeParent(const Filter *f)
{
	switch(f->getType())
	{
		case FILTER_TYPE_CLUSTER_ANALYSIS:
			return true;
		default:
			return false;
	}

}

unsigned int needsUnrangedData(const Filter *f)
{
	if(f->getType() == FILTER_TYPE_IONINFO)
	{
		const IonInfoFilter *fInfo = (const IonInfoFilter *)f;

		return fInfo->needsUnrangedData();
	}

	if(f->getType() == FILTER_TYPE_SPECTRUMPLOT)
	{
		const SpectrumPlotFilter *fSp = (const SpectrumPlotFilter *)f;

		return fSp->needsUnrangedData(); 
	}
	return false;
}

void FilterTreeAnalyse::getAnalysisResults(std::vector<FILTERTREE_ERR> &errs) const
{
	errs.resize(analysisResults.size());
	std::copy(analysisResults.begin(),analysisResults.end(),errs.begin());
}

void FilterTreeAnalyse::analyse(const FilterTree &f)
{
	clear();

	f.getAccumulatedPropagationMaps(emitTypes,blockTypes);

	//Check for a data pair where the output is entirely blocked,
	// rendering computation of filter useless
	blockingPairError(f);

	//Check for spatial sampling altering some results in later analyses
	spatialSampling(f);
	
	//Check for compositional biasing altering some later anaylsis
	compositionAltered(f);

	//Check for filters that do not have a parent, which is required
	checkRequiredParent(f);

	//check for unranged data required by child
	checkUnrangedData(f);


	emitTypes.clear();
	blockTypes.clear();

}


void FilterTreeAnalyse::blockingPairError(const FilterTree &f)
{
	//Examine the emit and block/use masks for each filter's parent (emit)
	// child relationship(block/use), such that in the case of a child filter that is expecting
	// a particular input, but the parent cannot generate it

	const tree<Filter *> &treeFilt=f.getTree();
	for(tree<Filter*>::pre_order_iterator it = treeFilt.begin(); it!=treeFilt.end(); ++it)
	{

		tree_node_<Filter*> *myNode=it.node->first_child;
		
		size_t parentEmit;	
		parentEmit = emitTypes[(*it) ]| (*it)->getRefreshEmitMask();

		while(myNode)
		{
			Filter *childFilter;
			childFilter = myNode->data;

			size_t curBlock,curUse;
			curBlock=blockTypes[childFilter] | childFilter->getRefreshBlockMask();
			curUse=childFilter->getRefreshUseMask();

			//If the child filter cannot use and blocks all parent emit values
			// emission of the all possible output filters,
			// then this is a bad filter pairing
			bool passedThrough;
			passedThrough=parentEmit & ~curBlock;

			if(!parentEmit && curUse)
			{
				FILTERTREE_ERR treeErr;
				treeErr.reportedFilters.push_back(childFilter);
				treeErr.reportedFilters.push_back(*it);
				treeErr.verboseReportMessage = TRANS("Parent filter has no output, but filter requires input -- there is no point in placing a child filter here.");
				treeErr.shortReportMessage = TRANS("Leaf-only filter with child");
				treeErr.severity=ANALYSE_SEVERITY_ERROR; //This is definitely a bad thing.
			
				analysisResults.push_back(treeErr);
			}
			else if(!(parentEmit & curUse) && !passedThrough )
			{
				FILTERTREE_ERR treeErr;
				treeErr.reportedFilters.push_back(childFilter);
				treeErr.reportedFilters.push_back(*it);
				treeErr.verboseReportMessage = TRANS("Parent filters' output will be blocked by child, without use. Parent results will be dropped.");
				treeErr.shortReportMessage = TRANS("Bad parent->child pair");
				treeErr.severity=ANALYSE_SEVERITY_ERROR; //This is definitely a bad thing.
			
				analysisResults.push_back(treeErr);
			}
			//If the parent does not emit a usable objects 
			//for the child filter, this is bad too.
			// - else if, so we don't double up on warnings
			else if( !(parentEmit & curUse) && !childFilter->isUsefulAsAppend())
			{
				FILTERTREE_ERR treeErr;
				treeErr.reportedFilters.push_back(childFilter);
				treeErr.reportedFilters.push_back(*it);
				treeErr.verboseReportMessage = TRANS("First filter does not output anything useable by child filter. Child filter not useful.");
				treeErr.shortReportMessage = TRANS("Bad parent->child pair");
				treeErr.severity=ANALYSE_SEVERITY_ERROR; //This is definitely a bad thing.
			
				analysisResults.push_back(treeErr);

			}


			//Move to next sibling
			myNode = myNode->next_sibling;
		}


	}	

}


void FilterTreeAnalyse::spatialSampling(const FilterTree &f)
{
	//True if spatial sampling is (probably) happening for children of 
	//filter. 
	vector<int> affectedFilters;
	affectedFilters.push_back(FILTER_TYPE_CLUSTER_ANALYSIS); //If have range parent
	affectedFilters.push_back(FILTER_TYPE_PROFILE); //If using density
	affectedFilters.push_back(FILTER_TYPE_SPATIAL_ANALYSIS); 
	affectedFilters.push_back(FILTER_TYPE_IONINFO); 

	const tree<Filter *> &treeFilt=f.getTree();
	for(tree<Filter*>::pre_order_iterator it(treeFilt.begin()); it!=treeFilt.end(); ++it)
	{
		//Check to see if we have a filter that can cause sampling
		if(filterIsSampling(*it))
		{
			tree_node_<Filter*> *childNode=it.node->first_child;

			if(childNode)
			{		

				//TODO: Not the most efficient method of doing this...
				//shouldn't need to continually compute depth to iterate over children	
				size_t minDepth=treeFilt.depth(it);	
				for(tree<Filter*>::pre_order_iterator itJ(childNode); treeFilt.depth(itJ) > minDepth;++itJ)
				{
					//ignore filters that are not affected by spatial sampling
					size_t filterType;
					filterType=(*itJ)->getType();
					if(std::find(affectedFilters.begin(),affectedFilters.end(),filterType)== affectedFilters.end())
						continue;

					childNode=itJ.node;

					//Check to see if we have a "range" type ancestor
					// - we will need to know this in a second
					bool haveRngParent=false;
					{
						tree_node_<Filter*> *ancestor;
						ancestor = childNode->parent;
						while(true) 
						{
							if(ancestor->data->getType() == FILTER_TYPE_RANGEFILE)
							{
								haveRngParent=true;
								break;
							}

							if(!ancestor->parent)
								break;

							ancestor=ancestor->parent;

						}
					}

					if(affectedBySampling(*itJ,haveRngParent))
					{
						FILTERTREE_ERR treeErr;
						treeErr.reportedFilters.push_back(*it);
						treeErr.reportedFilters.push_back(*itJ);
						treeErr.shortReportMessage=TRANS("Spatial results possibly altered");
						treeErr.verboseReportMessage=TRANS("Filters and settings selected that could alter reported results that depend upon density. Check to see if spatial sampling may be happening in the filter tree - this warning is provisional only.");						
						treeErr.severity=ANALYSE_SEVERITY_WARNING;

						analysisResults.push_back(treeErr);
					}
				}
			}

			//No need to walk child nodes	
			it.skip_children();
		}
		


	}
}

void FilterTreeAnalyse::checkRequiredParent(const FilterTree &f)
{
	const tree<Filter *> &treeFilt=f.getTree();
	vector<pair< tree<Filter*>::pre_order_iterator , size_t> > childrenNeedsParent;

	for(tree<Filter*>::pre_order_iterator it = treeFilt.begin(); it!=treeFilt.end(); ++it)
	{
		//Enumerate all the filters that need a range parent
		if(needsRangeParent(*it))
			childrenNeedsParent.push_back(make_pair(it,(size_t)FILTER_TYPE_RANGEFILE));
	}

	//Check each of the reported children, each time it was reported
	for(size_t ui=0;ui<childrenNeedsParent.size();ui++)
	{
		tree<Filter *>::pre_order_iterator it;
		size_t type;

		it = childrenNeedsParent[ui].first;
		type = childrenNeedsParent[ui].second;

		tree<Filter *>::pre_order_iterator parentIt;
		bool foundParent;
		foundParent=false;

		//walk back up the tree, to locate the parent (technically ancestor)
		// we are looking for
		while(treeFilt.depth(it))
		{
			it= treeFilt.parent(it);
			if((*it)->getType() == type)
			{
				foundParent=true;
				break;
			}
		}

		//If we couldn't find a parent, then this is an error.
		// let the user know
		if(!foundParent)
		{

			std::string tmpStr;
			Filter *tmpFilt = makeFilter(type);
			tmpStr=tmpFilt->typeString();
			delete tmpFilt;


			FILTERTREE_ERR treeErr;
			treeErr.reportedFilters.push_back(*(childrenNeedsParent[ui].first));

			treeErr.verboseReportMessage = TRANS("Filter needs parent \"")  + 
					tmpStr + TRANS("\" but does not have one. Filter may not function correctly until this parent is given.");
			treeErr.shortReportMessage = TRANS("Filter missing needed parent");
			treeErr.severity=ANALYSE_SEVERITY_ERROR; //This is definitely a bad thing.
			analysisResults.push_back(treeErr);
		}

	}
}

void FilterTreeAnalyse::checkUnrangedData(const FilterTree &f)
{
	const tree<Filter *> &treeFilt=f.getTree();
	for(tree<Filter*>::pre_order_iterator it(treeFilt.begin()); it!=treeFilt.end(); ++it)
	{
		//Check to see if we have a filter that can be affected by unranged data, missing or present

		if((*it)->getType() == FILTER_TYPE_RANGEFILE)
		{
			const RangeFileFilter *rngF = (const RangeFileFilter *)(*it);

			//we only need to investigate filters which drop data
			if( !rngF->getDropUnranged() )
				continue;

			for(tree<Filter*>::pre_order_iterator itJ(it); itJ!=treeFilt.end();++itJ)
			{
				//we need ranged data, but don't have it. Warn 
				if(needsUnrangedData(*itJ))
				{
					FILTERTREE_ERR treeErr;
					treeErr.reportedFilters.push_back(*it);
					treeErr.reportedFilters.push_back(*itJ);
					treeErr.shortReportMessage=TRANS("Bad range filter settings");
					treeErr.verboseReportMessage=TRANS("Rangefile set to drop unranged data, however a child filter requires it.");						
					treeErr.severity=ANALYSE_SEVERITY_WARNING;

					analysisResults.push_back(treeErr);
				} 
			}

		}
	} 
}

bool filterAltersComposition(const Filter *f)
{
	bool affectsComposition=false;


	FilterPropGroup props;
	f->getProperties(props);



	switch(f->getType())
	{
		case FILTER_TYPE_IONDOWNSAMPLE:
		{
			FilterProperty p;
			if(!props.hasProp(KEY_IONDOWNSAMPLE_PERSPECIES))
				return false;

			p=props.getPropValue(KEY_IONDOWNSAMPLE_PERSPECIES);

			const int GROUP_SAMPLING=1;
			
			if(p.data== "1" && props.hasGroup(GROUP_SAMPLING))
			{
				vector<FilterProperty> propVec;
				props.getGroup(GROUP_SAMPLING,propVec);


				//If using per-species mode, then
				// we may affect the output ion composition
				// if we have differing values
				for(size_t ui=1;ui<propVec.size(); ui++)
				{
					if(propVec[ui-1].data != propVec[ui].data)
					{
						affectsComposition=true;
						break;
					}

				}
			}
			break;
		}
		case FILTER_TYPE_RANGEFILE:
		{
			const RangeFileFilter *r;
			r = (const RangeFileFilter*)f;

			vector<char> enabledIons,enabledRanges;
			enabledIons = r->getEnabledIons();

			if(enabledIons.size() > 1)
			{
				size_t nEnabled=std::accumulate(enabledIons.begin(),enabledIons.end(),0);

				if(nEnabled > 0 && nEnabled < enabledIons.size())
					return true;
			}
	
			enabledRanges=r->getEnabledRanges();
			if(enabledRanges.size() > 1)
			{
				size_t nEnabled=std::accumulate(enabledRanges.begin(),enabledRanges.end(),0);

				if(nEnabled > 0 && nEnabled < enabledRanges.size())
					return true;
			}

			break;
		}
	}


	return affectsComposition;
}

bool filterAffectedByComposition(const Filter *f, bool haveRngParent)
{
	FilterPropGroup props;
	f->getProperties(props);
	
	bool affected=false;
	//See if filter is configured to affect spatial analysis
	switch(f->getType())
	{
		case FILTER_TYPE_CLUSTER_ANALYSIS:
		{
			affected=haveRngParent;
			break;
		}
		case FILTER_TYPE_PROFILE:
		{
			FilterProperty p;
			p=props.getPropValue(PROFILE_KEY_NORMALISE);

			//Affected if using normalise mode, and we do have a range parent
			affected= (p.data== "1" && haveRngParent);
			break;
		}
		case FILTER_TYPE_SPATIAL_ANALYSIS:
		{
			affected=true;
			break;
		}
	}

	return affected;
}

//FIXME: This is largely a cut and paste of ::spatialSampling - could be unified through
// function pointers and friends
void FilterTreeAnalyse::compositionAltered(const FilterTree &f)
{
	//True if composition biasing is (probably) happening for children of 
	//filter. 
	vector<int> affectedFilters;
	affectedFilters.push_back(FILTER_TYPE_CLUSTER_ANALYSIS); //If have range parent
	affectedFilters.push_back(FILTER_TYPE_PROFILE); //By definition
	affectedFilters.push_back(FILTER_TYPE_IONINFO); //If using composition 

	const tree<Filter *> &treeFilt=f.getTree();
	for(tree<Filter*>::pre_order_iterator it(treeFilt.begin()); it!=treeFilt.end(); ++it)
	{
		//Check to see if we have a filter that can cause sampling
		if(filterAltersComposition(*it))
		{
			tree_node_<Filter*> *childNode=it.node->first_child;

			if(childNode)
			{		

				//TODO: Not the most efficient method of doing this...
				//shouldn't need to continually compute depth to iterate over children	
				size_t minDepth=treeFilt.depth(it);	
				for(tree<Filter*>::pre_order_iterator itJ(childNode); treeFilt.depth(itJ) > minDepth;++itJ)
				{
					//ignore filters that are not affected by spatial sampling
					size_t filterType;
					filterType=(*itJ)->getType();
					if(std::find(affectedFilters.begin(),affectedFilters.end(),filterType)== affectedFilters.end())
						continue;

					childNode=itJ.node;

					//Check to see if we have a "range" type ancestor
					// - we will need to know this in a second
					bool haveRngParent=false;
					{
						tree_node_<Filter*> *ancestor;
						ancestor = childNode->parent;
						while(true) 
						{
							if(ancestor->data->getType() == FILTER_TYPE_RANGEFILE)
							{
								haveRngParent=true;
								break;
							}

							if(!ancestor->parent)
								break;

							ancestor=ancestor->parent;

						}
					}

					if(filterAffectedByComposition(*itJ,haveRngParent))
					{
						FILTERTREE_ERR treeErr;
						treeErr.reportedFilters.push_back(*it);
						treeErr.reportedFilters.push_back(*itJ);
						treeErr.shortReportMessage=TRANS("Composition results possibly altered");
						treeErr.verboseReportMessage=TRANS("Filters and settings selected that could bias reported composition. Check to see if species biasing may occcur in the filter tree - this warning is provisional only.");						
						treeErr.severity=ANALYSE_SEVERITY_WARNING;

						analysisResults.push_back(treeErr);
					}
				}
			}

			//No need to walk child nodes	
			it.skip_children();
		}
		


	}
}
