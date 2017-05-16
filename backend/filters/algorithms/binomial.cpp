/*
 *	binomial.cpp - Binomia distribution randomness testing
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

#include "binomial.h"

#include "common/basics.h"

#include <gsl/gsl_randist.h>
#include <gsl/gsl_cdf.h>

#include <map>
#include <vector>
#include <utility>
#include <numeric>

using std::pair;
using std::vector;
using std::map;
using std::unique;

template<class T> 
class CompareMagFloating
{
	public:
		inline bool operator()(const T &a,const T &b) const
			{return fabs(a) < fabs(b); }
};

const unsigned int BINOMIAL_LOWCOUNT_SKEW_THRESHOLD =5;


//Helper functions
//---
//Obtain an index to the row-major coordinate in array
int rowMajorOffset(unsigned int x, unsigned int y, unsigned int nY);
//obtain xy position from array index
void rowMajorIndicies(unsigned int idx, unsigned int nY, 
			unsigned int &x, unsigned int &y);

//set the non-extrusion  coordinates for a grid entry
void setGridABCoords(unsigned int binIdx, unsigned int *direction,unsigned int *nBins,
		float *binLen, const BoundCube  &totalBound, GRID_ENTRY &gridEntry);
//---

int countBinnedIons(const std::vector<IonHit> &ions, const RangeFile *rng,
			const std::vector<size_t> &selectedIons, const SEGMENT_OPTION &segmentOptions,
			vector<GRID_ENTRY> &completedGridEntries)
{

	//Step 1 - filter the ions to only the ranges we want to examine
	std::vector<IonHit> filteredIons;

	//Convert the selection into a map.
	map<size_t,size_t> selectionMapping;
	for(size_t ui=0;ui<selectedIons.size();ui++)
	{
		ASSERT(selectedIons[ui] < rng->getNumIons());
		selectionMapping[selectedIons[ui]] = ui;
	}

	//Filter ions by ranging
	for(size_t ui=0;ui<ions.size();ui++)
	{
		unsigned int ionID;
		ionID = rng->getIonID(ions[ui].getMassToCharge());
		
		//Skip unranged ions
		if(ionID == (unsigned int)-1)
			continue;

		filteredIons.push_back(ions[ui]);
	}
	
	//Obtain the bounding box for the filtered ions
	BoundCube totalBound;
	IonHit::getBoundCube(filteredIons,totalBound);

	// Sort the ions according to their Z value
	//--
	unsigned int extrusionAxis=segmentOptions.extrusionDirection;
	IonAxisCompare axisCmp(extrusionAxis);

	std::sort(filteredIons.begin(),
			filteredIons.end(),axisCmp);

	//--

	unsigned int direction[2];
	float binLen[2];
	unsigned int nBins[2];

	direction[0]=(extrusionAxis+1)%3;
	direction[1]=(extrusionAxis+2)%3;


	float targetL;
	//For now, only support autodetection of grid
	switch(segmentOptions.strategy)
	{
		case BINOMIAL_SEGMENT_AUTO_BRICK:
		{
			//Attempt to compute the number of ions to segment
			
			//The target volume for each grid cube
			float desiredVolume;
			desiredVolume= (float)segmentOptions.nIons/(float)filteredIons.size()*totalBound.volume();

			//Compute the target cube size
			targetL = powf(desiredVolume,1.0f/3.0f);

			//Compute the actual bin size, by attempting
			// to make them as even as possible
			for(size_t ui=0; ui<2;ui++)
			{
				float s;
				s= totalBound.getSize(direction[ui]);
				nBins[ui] = s/targetL+1;
				binLen[ui] = s/nBins[ui]+1;
			}

			break;
		}
		default:
			ASSERT(false);
	}

	//Now extrude the grid through the points in the given extrusion direction

	GRID_ENTRY *gridEntries;
	size_t nGrids=nBins[0]*nBins[1];
	try
	{
		gridEntries = new GRID_ENTRY[nGrids];
	}
	catch(std::bad_alloc)
	{
		return BINOMIAL_NO_MEM;
	}

#ifdef DEBUG
	BoundCube wrappingBound=totalBound;
	wrappingBound.expand(0.1f);
#endif

	//Initialise the grid entries
	//--
	float zStart = totalBound.getBound(extrusionAxis,0);
#pragma omp parallel for 
	for(size_t ui=0;ui<nGrids;ui++)
	{
		//Set the start and end 
		gridEntries[ui].nIons.resize(selectedIons.size(),0);
		gridEntries[ui].totalIons=0;
		gridEntries[ui].startPt[extrusionAxis]=gridEntries[ui].endPt[extrusionAxis]=zStart;

		setGridABCoords(ui,direction,nBins,binLen,totalBound,gridEntries[ui]);
	}
	//--

	Point3D lowBound;
	totalBound.getBound(lowBound,0);

	completedGridEntries.reserve(((float)filteredIons.size()/segmentOptions.nIons)*0.5f);
	for(size_t ui=0;ui<filteredIons.size(); ui++)
	{	
		//Find the X y division for the ion
		unsigned int xPos,yPos;
		Point3D ionOffset;
		ionOffset=filteredIons[ui].getPos() - lowBound;
		xPos =ionOffset[direction[0]]/binLen[0];
		yPos = ionOffset[direction[1]]/binLen[1];

		
		//Find the bin that this new ion is in,
		// and its range value
		unsigned int binIdx,selectionId,range;
		binIdx = rowMajorOffset(xPos,yPos,nBins[1]);
		
		//get range
		range=rng->getIonID(filteredIons[ui].getMassToCharge());
		ASSERT(range!=(unsigned int)-1);

		//convert range ID to selection ID
		//TODO: It might be faster not use the map, but rather
		// to use a fixed (but oversized) array to do the idx remapping.
		// the mem cost is pretty negligible for any sensible use case
		ASSERT(selectionMapping.find(range) != selectionMapping.end());
		selectionId= selectionMapping.find(range)->second;

		//Increment the nIons for the given bin
		gridEntries[binIdx].nIons[selectionId]++;
		gridEntries[binIdx].totalIons++;

		//Update grid end
		gridEntries[binIdx].endPt[extrusionAxis]=ionOffset[extrusionAxis];

		//Check to see if we need to finish this grid entry
		if(gridEntries[binIdx].totalIons ==segmentOptions.nIons)
		{
#ifdef DEBUG
			//Set the grid end
			gridEntries[binIdx].endPt[extrusionAxis] = filteredIons[ui].getPos()[extrusionAxis];
#endif
			completedGridEntries.push_back(gridEntries[binIdx]);

			//Reset the grid for the next round
			// TODO: Should we initialise zStart to this ion,
			//   or should we snap it to the next ion we encounter?
			gridEntries[binIdx].startPt[extrusionAxis] =filteredIons[ui].getPos()[extrusionAxis];
			gridEntries[binIdx].endPt[extrusionAxis]=filteredIons[ui].getPos()[extrusionAxis];

			//Set the box x-y (where extrusion=z)  coordinates.
			setGridABCoords(binIdx,direction,nBins,
					binLen,totalBound,gridEntries[binIdx]);
	
			//Set the start for the next grid, but not the end
			gridEntries[binIdx].startPt.setValue(extrusionAxis,gridEntries[binIdx].startPt[extrusionAxis]);
			
			for(size_t ui=0;ui<selectedIons.size();ui++)
				gridEntries[binIdx].nIons[ui]=0;
			gridEntries[binIdx].totalIons=0;

		}

		ASSERT(gridEntries[binIdx].totalIons < segmentOptions.nIons);

	}

	delete[] gridEntries;


	//Go through the grid entries, and delete the ones we don't want
	vector<bool> killEntries;
	killEntries.resize(completedGridEntries.size());
	for(size_t ui=0;ui<completedGridEntries.size();ui++)
	{
		float aspect;
		aspect=completedGridEntries[ui].endPt[extrusionAxis]-completedGridEntries[ui].startPt[extrusionAxis];

		aspect/=targetL;
		bool doKill;
		doKill=((aspect > segmentOptions.extrudeMaxRatio) || 
				(aspect < 1.0f/segmentOptions.extrudeMaxRatio));

		killEntries[ui]=doKill;
	}

	vectorMultiErase(completedGridEntries,killEntries);

	return 0;
}

//Vector output is a vector of frequency vectors
void genBinomialHistogram(const vector<GRID_ENTRY> &completedGridEntries,
				unsigned int nSelected, BINOMIAL_HIST &binHist)
				
{
	vector<map<unsigned int,unsigned int> >  &mapIonFrequencies = binHist.mapIonFrequencies;

	mapIonFrequencies.resize(nSelected);
	for(size_t ui=0;ui<completedGridEntries.size();ui++)
	{
		ASSERT(completedGridEntries[ui].nIons.size() == nSelected);
		//Insert all the frequencies for each selected ion into the vector
		for(size_t uj=0;uj<nSelected;uj++)
		{
			//find the frequency we need to update
			map<unsigned int, unsigned int>::iterator it;
			size_t val;
			val=completedGridEntries[ui].nIons[uj];
			it=mapIonFrequencies[uj].find(val);

			//Add these ions to the running total
			if(it==mapIonFrequencies[uj].end())
			{
				//we don't have an entry for this total, so make a new one
				mapIonFrequencies[uj][val]=1;
			}
			else
			{
				//we do have an entry, so accumulate
				it->second++;
			}
		}
	}

	//Compute the normalised frequencies
	vector<map<unsigned int, double> > &normFreq= binHist.normalisedFrequencies;

	normFreq.resize(mapIonFrequencies.size());
	for(size_t ui=0;ui<mapIonFrequencies.size();ui++)
	{
		//For each type (vector entry), compute the composition of each block
		map<unsigned int,unsigned int>::const_iterator it;
		size_t total;
		total=0;
		for(it=mapIonFrequencies[ui].begin(); it!=mapIonFrequencies[ui].end();++it)
		{
			total+=it->second;
		}
		//Create the entries for the normalised frequency, watching out for /= 0
		for(it=mapIonFrequencies[ui].begin(); it!=mapIonFrequencies[ui].end();++it)
		{
			if(total)
			{
				normFreq[ui][it->first]=(double)it->second/(double)total;
			}
			else
			{
				normFreq[ui][it->first]=0;
			}
		}
	}


}


void computeBinomialStats(const vector<GRID_ENTRY> &gridEntries, const BINOMIAL_HIST &binHist,
				unsigned int nSelected, BINOMIAL_STATS &stats)
{
	stats.nBlocks=gridEntries.size();	
	stats.nIons=gridEntries[0].totalIons;

	//Compute mean
	//--
	stats.mean.resize(nSelected);
	for(size_t ui=0;ui<stats.nBlocks; ui++)
	{
		for(size_t uj=0;uj<gridEntries[ui].nIons.size();uj++)
			stats.mean[uj]+=gridEntries[ui].nIons[uj];
	}
	for(size_t ui=0;ui<stats.mean.size();ui++)
		stats.mean[ui]/=(float)stats.nBlocks;
	//--
	

	//Compute chi-square, by comparing the observed frequency 
	// distribution with the theoretical (binomial) one
	// chi_Square = sigma( x_i - mean)  / mean
	//--

	vector<size_t> nChiCounted;
	nChiCounted.resize(nSelected,0);
	stats.chiSquare.resize(nSelected,0);

	CompareMagFloating<double> cmpMag;
	for(size_t ui=0;ui<nSelected; ui++)
	{
		//Probability of "success" - which here is the concentration
		// of the current species.
		double p;
		p = stats.mean[ui]/(double)stats.nIons;
		unsigned int nTotal;
		nTotal= gridEntries[ui].totalIons;

		//There is a numerical stability concern here, as we will add small
		// and large numbers repeatedly. This can cause "drift" when summing. To limit it,
		// one can sim over the smaller entries first, before the larger
		std::vector<double> sortedNumbers;
		sortedNumbers.clear();

		for(map<unsigned int,unsigned int>::const_iterator it=binHist.mapIonFrequencies[ui].begin();
				it!=binHist.mapIonFrequencies[ui].end();++it)
				
		{


			//Theoretical number of observation (probability*trials) 
			// of the binomial 
			double binThrObs;
			
			//The number of times we observed (numInBlock) ions.
			unsigned int nTimesObs;
			nTimesObs=it->second;

			//Don't count bins with a low count, as per Moody et al, we set this to 5.
			// This can skew the chi-square statistic
			if(nTimesObs <BINOMIAL_LOWCOUNT_SKEW_THRESHOLD)
				continue;


			//Number of times we should have observed blocks with
			// current count (it->first) of ion
			binThrObs=gsl_ran_binomial_pdf (it->first,p,nTotal)*stats.nBlocks;

			if(!binThrObs)
				continue;

			//Compute difference between experimental and theoretical dist func, for this bin
			double delta;
			delta=(nTimesObs-binThrObs);
			sortedNumbers.push_back(delta*delta/binThrObs);
			nChiCounted[ui]++;
		}
		std::sort(sortedNumbers.begin(),sortedNumbers.end(),cmpMag);
		
		stats.chiSquare[ui]=std::accumulate(sortedNumbers.begin(),
					sortedNumbers.end(),0.0);
	}
	//--

	//Compute the normalised comparison coefficient, "mu"
	// Moody et al, Microscopy Research and Techniques. 2008
	//--- 
	stats.comparisonCoeff.resize(nSelected);
	for(size_t ui=0;ui<stats.comparisonCoeff.size();ui++)
	{
		if(stats.mean[ui])
			stats.comparisonCoeff[ui]=sqrt(stats.chiSquare[ui]/(stats.mean[ui]*stats.nBlocks + stats.chiSquare[ui]));
		else 
			stats.comparisonCoeff[ui]=0;
	}
	//--- 

	//Compute the sampling probability values for drawing
	// this chi-square from the chi-square distribution
	//---
	stats.pValueOK.resize(nSelected);
	stats.pValue.resize(nSelected);
	for(size_t ui=0;ui<stats.pValue.size();ui++)
	{
		if(nChiCounted[ui] < 2)
		{
			stats.pValue[ui]=0;
			stats.pValueOK[ui]=false;
		}
		else
		{
			stats.pValue[ui]=1.0-gsl_cdf_chisq_P(stats.chiSquare[ui],nChiCounted[ui]-1);
			stats.pValueOK[ui]=true;
		}
	}

	///--


}

int rowMajorOffset(unsigned int x, unsigned int y, unsigned int nY)
{
	ASSERT( y < nY);
	return x*nY + y;
}

void rowMajorIndicies(unsigned int idx, unsigned int nY, 
		unsigned int &x, unsigned int &y)
{

	x = idx/nY;
	y = idx-nY*x;
}

//binIdx is the offset for the extruded grid (as viewed down grid extrusion axis)
//direction is the coordinate axes for the grid, binLen is the length of the bin
// and totalBound is the bounding cube that 
void setGridABCoords(unsigned int binIdx, unsigned int *direction,unsigned int *nBins,
		float *binLen, const BoundCube  &totalBound, GRID_ENTRY &gridEntry)
{
	float tmpX,tmpY;
	unsigned int tmpIdx[2];
	rowMajorIndicies(binIdx,nBins[1],tmpIdx[0],tmpIdx[1]);
	tmpX = tmpIdx[0]*binLen[0]+totalBound.getBound(direction[0],0);
	tmpY = tmpIdx[1]*binLen[1]+totalBound.getBound(direction[1],0);

	gridEntry.startPt[direction[0]]=tmpX;
	gridEntry.startPt[direction[1]]=tmpY;
	gridEntry.endPt[direction[0]]=tmpX+binLen[0];
	gridEntry.endPt[direction[1]]=tmpY+binLen[1];

}

#ifdef DEBUG
#include "common/assertion.h"
#include "backend/APT/APTRanges.h"
#include "common/mathfuncs.h"

#include <sys/time.h>

bool testBinomialBinning();
bool testBinomialGSLChi();
bool testBinomialRandomnessTruePositive();
bool testBinomialRandomnessTrueNegative();


bool testBinomial()
{
	TEST(testBinomialGSLChi(),"Binomial GSL");
	TEST(testBinomialBinning(),"Binomial Binning");
	TEST(testBinomialRandomnessTruePositive(),"Binomial random correctly detected");
	TEST(testBinomialRandomnessTrueNegative(),"Binomial non-random correclty deteced");
	return true;
}


void generateTestGridEntries(vector<GRID_ENTRY> &gridEntries, unsigned int &nSelected,
		double underSkewFactor, double PVAL, double NTRIALS, unsigned int NSAMPLE)
{
	nSelected=2;

	gsl_rng *r=gsl_rng_alloc(gsl_rng_ranlxs2);
	timeval tv;
	gettimeofday(&tv,NULL);
	long seed =tv.tv_usec+tv.tv_sec; 
	gsl_rng_set(r, seed);                  // set seed

	gridEntries.resize(NSAMPLE);

	//Fake some binomially sampled data.
	// The first species will be binomially distributed. The second
	// is fixed to allow the first to fill quota
	for(size_t ui=0;ui<gridEntries.size();ui++)
	{
		// Should give a p-value of < 0.05
		gridEntries[ui].nIons.resize(2);
		gridEntries[ui].nIons[0]=gsl_ran_binomial(r,PVAL,NTRIALS)/underSkewFactor;
		gridEntries[ui].nIons[1]=NTRIALS-gridEntries[ui].nIons[0];

		gridEntries[ui].totalIons=NTRIALS;
		gridEntries[ui].startPt[2]=0.1;
		gridEntries[ui].endPt[2]=0.2;
	}
	gsl_rng_free(r);
}

bool testBinomialRandomnessTrueNegative()
{
	const double PVAL=0.7;
	const double NTRIALS=30;

	const unsigned int NSAMPLE=5000;
	
	unsigned int nSelected;
	vector<GRID_ENTRY> gridEntries;
	generateTestGridEntries(gridEntries,nSelected,1.2,PVAL,NTRIALS,NSAMPLE);

	//Build the frequency histograms
	BINOMIAL_HIST binHist;
	genBinomialHistogram(gridEntries,nSelected,binHist);

	//Compute chi-square
	//-----------------
	double chiSq=0;
	for(map<unsigned int, unsigned int>::iterator it = binHist.mapIonFrequencies[0].begin();
			it!=binHist.mapIonFrequencies[0].end();++it)
	{
		double delta,expected;
		//This function computes the probability p(k) of obtaining k from a binomial distribution with parameters p and n,
		// gsl_ran_binomial_pdf(k,p,n)
		expected = gsl_ran_binomial_pdf(it->first,PVAL,NTRIALS)*NSAMPLE;

		delta=it->second-expected;
		chiSq+=delta*delta/expected;
	}
	//-----------------
	double pValue=1.0-gsl_cdf_chisq_P(chiSq,binHist.mapIonFrequencies[0].size()-1);

	//Check non-random detected (use a high threshold, as we might run this a lot, 
	// and triggering failure should be done carefully)
	TEST(pValue < 0.2,"Confirmation of randomness by pvalue");
	


	BINOMIAL_STATS binStats;
	computeBinomialStats(gridEntries,binHist,nSelected,binStats);

	TEST(binStats.pValue[0] < 0.2,"Confirmation of binomial stats pvalue");
	TEST(binStats.pValueOK[0],"Pvalue reported as correctly computed");
	TEST(fabs(binStats.pValue[0]-pValue)< 0.01,"cross-check pvalue computation");

	return true; 
}

bool testBinomialRandomnessTruePositive()
{
	//Probability of an individual event being first type
	const double PVAL=0.7;
	//Number of trials per sample
	const double NTRIALS=100;
	//Number of total samples
	const unsigned int NSAMPLE=500;

	//Make a fake dataset
	//---
	unsigned int nSelected;
	vector<GRID_ENTRY> gridEntries;
	generateTestGridEntries(gridEntries,nSelected,1.0,PVAL,NTRIALS,NSAMPLE);
	//---

	//Build the frequency histograms
	BINOMIAL_HIST binHist;
	genBinomialHistogram(gridEntries,nSelected,binHist);

	//Compute chi-square
	//-----------------
	double chiSq=0;
	vector<double> sortedNumbers;
	for(map<unsigned int, unsigned int>::iterator it = binHist.mapIonFrequencies[0].begin();
			it!=binHist.mapIonFrequencies[0].end();++it)
	{
		//TODO: There are correction factors that we could use here, rather than a full discard
		if(it->second < BINOMIAL_LOWCOUNT_SKEW_THRESHOLD)
			continue;

		double delta,expected;
		//This function computes the probability p(k) of obtaining k from a binomial distribution with parameters p and n,
		// gsl_ran_binomial_pdf(k,p,n)
		expected = gsl_ran_binomial_pdf(it->first,PVAL,NTRIALS)*NSAMPLE;


		delta=it->second-expected;
		sortedNumbers.push_back(delta/expected);
	}


	CompareMagFloating<double> cmpMag;
	std::sort(sortedNumbers.begin(),sortedNumbers.end(),cmpMag);
	chiSq=std::accumulate(sortedNumbers.begin(),sortedNumbers.end(),0.0f);
	//-----------------
	
	if(sortedNumbers.size() <=2)
	{
		//Skip tests if no frequency is above the skew threshold
		WARN(false,"Unlikely (but possible) situation occured - all binomial ions were insufficiently frequent. skipping Chi-square");
	}
	else
	{
		double pValue=1.0-gsl_cdf_chisq_P(chiSq,binHist.mapIonFrequencies[0].size()-1);


		//Its random, but because we might run this test a lot, set a very low statistical threhsold
		TEST(pValue > 0.00001,"Confirmation of randomness by pvalue");
		


		BINOMIAL_STATS binStats;
		computeBinomialStats(gridEntries,binHist,nSelected,binStats);

		TEST(binStats.pValue[0] > 0.00001,"Confirmation of binomial stats pvalue");
		TEST(binStats.pValueOK[0],"Pvalue reported as correctly computed");
		//Note that this next test is quite wide, as the pvalues are for 
		// *two different observation underlying probabilities*.
		//In one, the binomial prob is known (PVAL, e.g. =0.7), in the other we estimate it 
		//from observation - which has an error associated with it due to the 
		//finite number of observations (e.g. pObs = 0.698). Chi-square is quite sensitive to this
		//difference.
		TEST(fabs(binStats.pValue[0]-pValue)/pValue < 2.0,"cross-check pvalue computation");
	}

	return true; 
}

bool testBinomialGSLChi()
{
	//A p-table says we should get for chisq=3.94, and df = 10, p ~= 0.95
	//--
	double pdf=1.0-gsl_cdf_chisq_P(3.94,10);

	TEST(fabs(pdf - 0.95) < 0.01,"Check chi-square distribution definition (chi=3.94,df=10)");
	
	pdf=1.0-gsl_cdf_chisq_P(10.83,1);
	
	TEST(fabs(pdf - 0.001) < 0.005,"Check Chi-square distribution definition (chi=10.83,df=1)");


	pdf=1.0-gsl_cdf_chisq_P(94.9543,100);
	TEST(fabs(pdf-0.6238) < 0.001, "Check chi-square, chi=94.9543. Df=100");


	//---

	return true;
}

bool testBinomialBinning()
{
	RangeFile rng;

	RGBf col;
	
	col.red=1.0f;
	col.green=col.blue=0.0f;
	rng.addIon("A","A",col);
	
	col.red=0.0f;
	col.blue=1.0f;

	rng.addIon("B","B",col);
	rng.addRange(0.5,1.5,rng.getIonID("B"));
	rng.addRange(1.5,2.5,rng.getIonID("B"));

	vector<IonHit> ions;
	ions.resize(100);

	RandNumGen rnd;
	rnd.initTimer();

	for(unsigned int ui=0;ui<100; ui++)
	{
		ions[ui].setPos(rnd.genUniformDev(),
		
				rnd.genUniformDev(),
				rnd.genUniformDev());

		ions[ui].setMassToCharge( 1 + (ui %2));
	}


	vector<size_t> selectedIons;
	selectedIons.push_back(0);
	selectedIons.push_back(1);

	vector<GRID_ENTRY> g;

	SEGMENT_OPTION segOpt;
	segOpt.nIons=10;
	segOpt.extrusionDirection=0;
	segOpt.extrudeMaxRatio=1000;
	segOpt.strategy=BINOMIAL_SEGMENT_AUTO_BRICK;

	

	//Perform binomial segmentation
	TEST(!countBinnedIons(ions,&rng,selectedIons,segOpt,g),
				"binomial binning (auto brick mode)");

	//Check that the number of grids is less than the number of ions
	TEST(g.size() < ions.size()/segOpt.nIons,"Full bricks only");

	size_t total=0;
	for(size_t ui=0;ui<g.size(); ui++)
	{
		//Check grid extrusion is positive
		TEST(g[ui].startPt[2] < g[ui].endPt[2], "grid extrusion direction");
		TEST(g[ui].totalIons == segOpt.nIons, "grid ion reported count");
		size_t kIons;
		kIons=0;
		for(size_t uj=0;uj<g[ui].nIons.size();uj++)
			kIons+=g[ui].nIons[uj];

		TEST(kIons == segOpt.nIons,"Ion recount");
		total+=kIons;
	}

	TEST(total <= ions.size(),"Ion count checking");

	BINOMIAL_HIST binomialHistogram;
	genBinomialHistogram(g,selectedIons.size(),binomialHistogram);

	TEST(binomialHistogram.mapIonFrequencies.size() == selectedIons.size(),"map size")

	for(unsigned int ui=0;ui<binomialHistogram.mapIonFrequencies.size();ui++)
	{
		unsigned int binnedTotal=0;
		for(map<unsigned int, unsigned int>::const_iterator it=
			binomialHistogram.mapIonFrequencies[ui].begin(); 
			it!=binomialHistogram.mapIonFrequencies[ui].end();++it)
		{
			binnedTotal+=it->second;
		}
		TEST(binnedTotal < segOpt.nIons, "Number of observations at given freq should be < number total observations");
	}

	return true;
}
#endif


				
