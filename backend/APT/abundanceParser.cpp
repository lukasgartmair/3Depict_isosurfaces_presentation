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



#include "abundanceParser.h"

#include "common/xmlHelper.h"
#include "common/stringFuncs.h"

#include <iostream>
#include <cstdlib>
#include <stack>
#include <map>
#include <cmath>
#include <limits>
#include <algorithm>
#include <numeric>

using std::vector;
using std::pair;
using std::make_pair;
using std::map;
using std::string;
const char *ABUNDANCE_ERROR[] = { "Unable to read abundance data (opening file)",
				 "Unable to create XML reader.",
				 "Bad property found in XML file",
				 "XML document did not match expected layout (DTD validation)",
				 "Unable to find required node during parse",
				 "Root node missing, expect <atomic-mass-table>!",
				 "Found incorrect root node. Expected <atomic-mass-table>"
};


const char *AbundanceData::getErrorText(size_t errorCode)
{
	ASSERT(errorCode < ABUNDANCE_ERROR_ENUM_END);
	return ABUNDANCE_ERROR[errorCode];
}

size_t AbundanceData::numIsotopes() const
{
	size_t v=0;
	for(size_t ui=0;ui<isotopeData.size();ui++)
		v+=isotopeData[ui].size();
	
	return v;
}

size_t AbundanceData::numElements() const
{
	return isotopeData.size();
}

size_t AbundanceData::open(const char *file, bool strict)
{

	xmlDocPtr doc;
	xmlParserCtxtPtr context;

	context =xmlNewParserCtxt();

	if(!context)
		return ABUNDANCE_ERR_NO_CONTEXT;

	//Open the XML file
	doc = xmlCtxtReadFile(context, file, NULL, XML_PARSE_DTDVALID| XML_PARSE_NOENT | XML_PARSE_NONET);

	if(!doc)
	{
		xmlFreeParserCtxt(context);
		return ABUNDANCE_ERR_BAD_DOC;
	}
	else
	{
		//Check for context validity
		if(!context->valid)
		{
			if(strict)
			{
				xmlFreeDoc(doc);
				xmlFreeParserCtxt(context);
				return ABUNDANCE_ERROR_FAILED_VALIDATION;
			}
			else
			{
				std::cerr << "Unable to validate XML file. Continuing anyway.." << std::endl;
			}
		}
	}

	try
	{
	
	//retrieve root node	
	std::stack<xmlNodePtr> nodeStack;
	xmlNodePtr nodePtr = xmlDocGetRootElement(doc);

	if(!nodePtr)
		throw ABUNDANCE_ERROR_MISSING_ROOT_NODE;
	
	//This *should* be an abundance file
	if(xmlStrcmp(nodePtr->name, (const xmlChar *)"atomic-mass-table"))
		throw ABUNDANCE_ERROR_WRONG_ROOT_NODE;

	nodeStack.push(nodePtr);

	nodePtr=nodePtr->xmlChildrenNode;
	while(!XMLHelpFwdToElem(nodePtr,"entry"))
	{
		ISOTOPE_DATA curIsoData;
		
		if(XMLHelpGetProp(curIsoData.symbol,nodePtr,"symbol"))
			throw ABUNDANCE_ERROR_BAD_VALUE;
		
		if(XMLHelpGetProp(curIsoData.atomicNumber,nodePtr,"atomic-number"))
			throw ABUNDANCE_ERROR_BAD_VALUE;


		nodeStack.push(nodePtr);
		nodePtr=nodePtr->xmlChildrenNode;
		
		//Move to natural-abundance child
		if(XMLHelpFwdToElem(nodePtr,"natural-abundance"))
			throw ABUNDANCE_ERROR_MISSING_NODE;
		
		nodePtr=nodePtr->xmlChildrenNode;

		vector<ISOTOPE_DATA> curIsotopes;	
		//TODO: value checking	
		while(!XMLHelpFwdToElem(nodePtr,"isotope"))
		{
			//Spin to mass node
			if(XMLHelpGetProp(curIsoData.massNumber,nodePtr,"mass-number"))
				throw ABUNDANCE_ERROR_MISSING_NODE;
			
			nodeStack.push(nodePtr);
			nodePtr=nodePtr->xmlChildrenNode;

			//Spin to mass node
			if(XMLHelpFwdToElem(nodePtr,"mass"))
				throw ABUNDANCE_ERROR_MISSING_NODE;

			if(XMLHelpGetProp(curIsoData.mass,nodePtr,"value"))
				throw ABUNDANCE_ERROR_BAD_VALUE;
			
			if(XMLHelpGetProp(curIsoData.massError,nodePtr,"error"))
				throw ABUNDANCE_ERROR_BAD_VALUE;
			else
				curIsoData.massError=0;


			//spin to abundance node
			if(XMLHelpFwdToElem(nodePtr,"abundance"))
				throw ABUNDANCE_ERROR_MISSING_NODE;
			
			if(XMLHelpGetProp(curIsoData.abundance,nodePtr,"value"))
				throw ABUNDANCE_ERROR_BAD_VALUE;
			
			if(XMLHelpGetProp(curIsoData.abundanceError,nodePtr,"error"))
				throw ABUNDANCE_ERROR_BAD_VALUE;
			else
				curIsoData.abundanceError=0;

			curIsotopes.push_back(curIsoData);

			nodePtr=nodeStack.top();
			nodePtr=nodePtr->next;
			nodeStack.pop();
		}

		isotopeData.push_back(curIsotopes);
		curIsotopes.clear();	

		nodePtr=nodeStack.top();
		nodeStack.pop();
		nodePtr=nodePtr->next;
		
	}
	}
	catch( int &excep)
	{
		xmlFreeDoc(doc);
		xmlFreeParserCtxt(context);
		return excep;
	}
		
	xmlFreeDoc(doc);
	xmlFreeParserCtxt(context);
	return 0;
}


size_t AbundanceData::symbolIndex(const char *symbol) const
{
	for(size_t ui=0;ui<isotopeData.size();ui++)
	{
		if(isotopeData[ui].size() && isotopeData[ui][0].symbol == symbol )
			return ui;
	}


	return (size_t)-1;
}

const ISOTOPE_DATA &AbundanceData::isotope(size_t elemIdx,size_t isotopeIdx) const
{
	return isotopeData[elemIdx][isotopeIdx];
}

void AbundanceData::generateIsotopeDist(const vector<size_t> &elementIdx,
					const vector<size_t> &frequency,
				vector<pair<float,float> > &massDist, size_t chargeCount) const
{
	ASSERT(chargeCount);
	ASSERT(frequency.size() == elementIdx.size());
	//Search out the given isotopes, and compute the peaks
	// that would be seen for this isotope combination

	//map of vectors for the available isotopes
	map<unsigned int, vector< pair<float,float> > > isotopeMassDist;
	vector<float> bulkConcentration(frequency.size());

	size_t total=std::accumulate(frequency.begin(),frequency.end(),0);

	for(size_t ui=0;ui<frequency.size();ui++)
		bulkConcentration[ui]=frequency[ui]/(float)total;

	//For each isotope, retrieve  its (mass,probability dist) values
	// placing into a map
	//--
	for(size_t ui=0;ui<elementIdx.size();ui++)
	{
		size_t curIso;
		curIso = elementIdx[ui];

		//If we have seen isotope before, move on
		if(isotopeMassDist.find(curIso) != isotopeMassDist.end())
			continue;

		vector<pair<float,float> > thisIsotope;
		for(size_t uj=0;uj<isotopeData[curIso].size();uj++)
		{
			//Compute the probability of this isotope's presence in
			// the sampled concentration
			thisIsotope.push_back(make_pair(isotopeData[curIso][uj].mass,
				isotopeData[curIso][uj].abundance));
		}

		isotopeMassDist[curIso] = thisIsotope;
		thisIsotope.clear();

	}
	//--


	//Given the isotopes we have, permute the mass spectra
	vector<pair<float,float> > peakProbs;
	for(size_t ui=0;ui<elementIdx.size();ui++)
	{

		for(size_t repeat=0;repeat<frequency[ui];repeat++)
		{
			vector< pair<float,float> >::const_iterator  isoBegin,isoEnd;
			isoBegin = isotopeMassDist[elementIdx[ui]].begin();
			isoEnd = isotopeMassDist[elementIdx[ui]].end();

			vector<pair<float,float> > newProbs;
			//If this is the very first item in our list,
			// simply push on the value, rather than modifying the
			// distribution
			if(peakProbs.empty())
			{
				//The masses will be added to, and the probabilities multipled	
				for(vector<pair<float,float> >::const_iterator it=isoBegin;
					it!=isoEnd;++it)
					peakProbs.push_back(*it);
			}
			else
			{
				for(size_t uj=0;uj<peakProbs.size();uj++)
				{
				
					//The masses will be added to, and the probabilities multipled	
					for(vector<pair<float,float> >::const_iterator it=isoBegin;
						it!=isoEnd;++it)
					{
						pair<float,float> newMass;

						newMass.first=peakProbs[uj].first+it->first;
						newMass.second=peakProbs[uj].second*it->second;
						newProbs.push_back(newMass);
					}

				}
				
				peakProbs.swap(newProbs);
				newProbs.clear();
			}
		}
	}


	float tolerance=sqrt(std::numeric_limits<float>::epsilon());

	vector<bool> killPeaks(peakProbs.size(),false);
	//Find the non-unique peaks and sum them
	for(size_t ui=0;ui<peakProbs.size();ui++)
	{
		for(size_t uj=ui+1;uj<peakProbs.size();uj++)
		{
			if(fabs(peakProbs[ui].first-peakProbs[uj].first) <tolerance &&
				peakProbs[uj].second > 0.0f)
			{

				peakProbs[ui].second+=peakProbs[uj].second;
				peakProbs[uj].second=0.0f;
				killPeaks[uj]=true;
			}
		}
	}
	
	vectorMultiErase(peakProbs,killPeaks);
	massDist.swap(peakProbs);


	for(size_t ui=0;ui<massDist.size();ui++)
	{
		massDist[ui].first/=(float)chargeCount;
	}

}


void AbundanceData::getSymbolIndices(const vector<string> &symbols,vector<size_t> &indices) const
{
	indices.resize(symbols.size());
	for(size_t ui=0;ui<symbols.size(); ui++)
		indices[ui]=symbolIndex(symbols[ui].c_str());
}


#ifdef DEBUG

#include <set>

using std::set;

void AbundanceData::checkErrors() const
{
	//Ensure all isotopes sum to 1-ish
	// Rounding errors limit our correctness here.
	for(size_t ui=0;ui<isotopeData.size();ui++)
	{
		if(!isotopeData[ui].size())
			continue;

		float sum;
		sum=0.0f;
		for(size_t uj=0; uj<isotopeData[ui].size();uj++)
		{
			sum+=isotopeData[ui][uj].abundance;
		}

		ASSERT(fabs(sum -1.0f) < 0.000001);

	}


	//Ensure Ti has 5 isotopes (original data file was missing)
	ASSERT(isotopeData[symbolIndex("Ti")].size() == 5);

	//Enusre all isotopes are uniquely numbered
	// - loop over each atom
	for(size_t ui=0;ui<isotopeData.size();ui++)
	{
		//now ovre each isotope
		std::set<size_t> uniqNums;
		uniqNums.clear();
		for(size_t uj=0; uj<isotopeData[ui].size();uj++)
		{
			ASSERT(uniqNums.find(isotopeData[ui][uj].massNumber) == uniqNums.end());
			uniqNums.insert(isotopeData[ui][uj].massNumber);
			
		}
	}
}

bool AbundanceData::runUnitTests(const char *tableFile)
{
	AbundanceData massTable;
	TEST(massTable.open(tableFile) == 0,"load table");
	//FIXME: Getting the isotope dis

	size_t ironIndex=massTable.symbolIndex("Fe");
	TEST(ironIndex != (size_t)-1,"symbol lookup");

	//Generate the mass peak dist for iron
	vector<size_t> elements;
	vector<size_t> concentrations;
	elements.push_back(ironIndex);
	concentrations.push_back(1);

	std::vector<std::pair<float,float> > massDist;
	massTable.generateIsotopeDist(elements,concentrations,massDist);

	TEST(massDist.size() == 4, "Iron has 4 isotopes");

	massTable.checkErrors();

	return true;	

}
#endif
