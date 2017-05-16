/*
 *	binomial.h - Binomia distribution randomness testing
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
#ifndef BINOMIAL_H
#define BINOMIAL_H

#include "common/mathfuncs.h"
#include "backend/APT/ionhit.h"
#include "backend/APT/APTRanges.h"

#include <map>

enum
{
	BINOMIAL_SEGMENT_AUTO_BRICK,
	BINOMIAL_SEGMENT_END
};

struct SEGMENT_OPTION
{
	//Segmentation mode
	size_t strategy;

	//Target number of ions in each segment
	size_t nIons;

	//Directin for extrusion
	size_t extrusionDirection;

	//Extrusion options
	//--
	//Maximum allowed distance for grid extrusion
	float extrudeMaxRatio;
	//--
};

struct BINOMIAL_HIST
{
	//TODO: Each vector is actually the same length. Could place in side
	// another object instead.

	// each element in vector is for each ion type. Map element inside 
	// the vector provides a table of observed counts and their frequency.
	std::vector<std::map<unsigned int,unsigned int> >  mapIonFrequencies;

	// Same as above vector, however, this computes the normalised distribution
	// i.e., the experimental P distribution
	std::vector<std::map<unsigned int,double> > normalisedFrequencies;
	
	std::vector<std::map<unsigned int,double> > theoreticFrequencies;
	std::vector<std::map<unsigned int,double> > theoreticNormalisedFrequencies;
};


struct BINOMIAL_STATS
{
	std::vector<double> mean,chiSquare, comparisonCoeff,pValue;
	std::vector<bool> pValueOK;
	size_t nBlocks,nIons;
};

struct GRID_ENTRY
{
	//Start and end coordinates for the grid.
	//	the algorithm itself only needs the extrusion axis, however for the
	//	visualisation, we use the full coordinates
	Point3D startPt,endPt;
	std::vector<unsigned int> nIons;
	//FIXME: This does not need to be here.
	unsigned int totalIons;
};

//Binomial algorithm error codes
enum
{
	BINOMIAL_NO_MEM=1,
	BINOMIAL_ERR_END
};

//Compute the experimental binomial distribution for the specified ions.
int countBinnedIons(const std::vector<IonHit> &ions, const RangeFile *rng,
			const std::vector<size_t> &selectedIons, const SEGMENT_OPTION &segmentOptions,
			std::vector<GRID_ENTRY> &completedGridEntries);

//Generate a vector of ion frequencies in histogram of segment counts, 
void genBinomialHistogram(const std::vector<GRID_ENTRY> &completedGridEntries,
				unsigned int nSelected, BINOMIAL_HIST &binHist);

//convert grid frequencies to compositions

void binomialConvert(const std::vector<std::map<unsigned int,unsigned int> > &ionFrequencies,float binWidth, 
				std::vector<std::vector<float> > &ionConcentrations); 


void computeBinomialStats(const std::vector<GRID_ENTRY> &gridEntries,const BINOMIAL_HIST &binHist,
		unsigned int nSelected, BINOMIAL_STATS &binStats);

#ifdef DEBUG
//Perform unit tests on binomial algorithm
bool testBinomial();
#endif

#endif
