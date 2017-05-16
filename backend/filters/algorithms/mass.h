/*
 *	mass.h - Algorithms for computing mass backgrounds 
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
#ifndef MASS_H
#define MASS_H

#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

#include <gsl/gsl_sf_erf.h>
#include <gsl/gsl_histogram.h>

#include "../../filter.h"

enum
{
	FIT_MODE_NONE,
	FIT_MODE_CONST_TOF,
	FIT_MODE_ENUM_END,
};

//Matching strings for FIT_MODE enum. ENUM_END does not have a string
extern const char *BACKGROUND_MODE_STRING[FIT_MODE_ENUM_END];



struct BACKGROUND_PARAMS
{
	enum
	{
		FIT_FAIL_MIN_REQ_BINS=1,
		FIT_FAIL_AVG_COUNTS,
		FIT_FAIL_INSUFF_DATA,
		FIT_FAIL_DATA_NON_GAUSSIAN,
		FIT_FAIL_END
	};
	//background fitting mode to use
	unsigned int mode;
	//-- start/end window for const tof background fit
	float massStart, massEnd;
	//step size in bins for fitting histogram
	float binWidth;

	//result parameters
	// for FIT_MODE_CONST_TOF, this uses 
	float intensity,stdev; //FIXME: the units of this are not fully coherent. Should be in units of (counts/sqrt(amu))

};

template<typename T>
void meanAndStdev(const std::vector<T > &f,float &meanVal, 
			float &stdevVal,bool normalCorrection=true)
{
	meanVal=0;
	for(size_t ui=0;ui<f.size();ui++)
		meanVal+=f[ui];
	meanVal /=f.size();
	
	stdevVal=0;
	for(size_t ui=0;ui<f.size();ui++)
	{
		float delta;
		delta=f[ui]-meanVal;
		stdevVal+=delta*delta;
	}
	stdevVal = sqrtf( stdevVal/float(f.size()-1));

	//Perform bias correction, assuming the input data is normally distributed
	if(normalCorrection)
	{
		float n=f.size();
		//Approximation to C4 = sqrt(2/(n-1))*gamma(n/2)/gamma((n-1)/2)
		// multiplier must be applied to 1/(n-1) normalised standard deviation
		// Citation: 
		//	http://support.sas.com/documentation/cdl/en/qcug/63922/HTML/default/qcug_functions_sect007.htm
		//	https://en.wikipedia.org/wiki/Unbiased_estimation_of_standard_deviation
		stdevVal*=(1.0 - 1.0/(4.0*n) - 7.0/(32.0*n*n) - 19.0/(128.0*n*n*n));
	}
}

//Perform a background fit
//	- background params has the input and output data for the fit.
//	- dataIn requires ion data for a successful fit
//	- returns zero on success, nonzero on error`
unsigned int doFitBackground(const std::vector<const FilterStreamData*> &dataIn, BACKGROUND_PARAMS &params) ;

// Build a histogram of the background
// - Start and end mass, and step size (to get bin count).
// tofBackIntensity is the intensity level per unit time in the background, as obtained by doFitBackground
// the histogram is 
void createMassBackground(float massStart, float massEnd, unsigned int nBinsMass,
			float tofBackIntensity, vector<float> &histogram);


//Anderson. test statistic for gaussian-ness. Returns false if input has insufficient points for test (2 items)
//Implented for unknown (derived from data) mean & variance
// reject statistic if output has this prob. of non-normality:
// 15% - 0.576
// 10% - 0.656
//  5% - 0.787
//2.5% - 0.918
//  1% - 1.092
//See, eg 
// http://itl.nist.gov/div898/handbook/eda/section3/eda35e.htm
template<class T>
bool andersonDarlingStatistic(std::vector<T> vals, float &meanV, float &stdevVal, 
		float &statistic, size_t &undefCount, bool computeMeanAndStdev=true)
{
	size_t n=vals.size();
	//we cannot compute this without more data
	if(n <= 1)
		return false;

	if(computeMeanAndStdev)
		meanAndStdev(vals,meanV,stdevVal);

	//Bring assumed gauss data into a normal dist
	for(size_t ui=0;ui<n;ui++)
		vals[ui]=(vals[ui]-meanV)/stdevVal;

	//For test, data *must be sorted*
	std::sort(vals.begin(),vals.end());

	//Compute the Phi distribution from the error function
	// - also compute log of this for later use
	//--
	std::vector<double> normedPhi,lonCdf;
	std::vector<bool> normedPhiOK;

	normedPhiOK.resize(n,true);
	normedPhi.resize(n);
	for(size_t ui=0;ui<n; ui++)
	{
		normedPhi[ui] = 0.5*(1.0+gsl_sf_erf(vals[ui]/sqrt(2.0)));

		if(normedPhi[ui] < std::numeric_limits<float>::epsilon())
			normedPhiOK[ui]=false;
	}

	lonCdf.resize(n);
	for(size_t ui=0;ui<n; ui++)
	{
		if(normedPhiOK[ui])	
			lonCdf[ui] = log(normedPhi[ui]);
		else
			normedPhi[ui]=2.0f;  //result will imply v 1.0-normedphi[...] < 0 --> Undefined
	}
	//--

	//Compute anderson-darling statistic
	//--
	undefCount=0;	
	double sumV=0.0;
	for(size_t i=0;i<n; i++)
	{
		double v;
		v=1.0-normedPhi[n-(i+1)];
		if( v > 0.0)
			sumV+=(2.0*(i+1.0)-1.0)*(lonCdf[i] + log(v));
		else
			undefCount++;
	}

	n=n-undefCount;
	statistic=-(double)n - sumV/(double)n;

	//Perform correction of Shorak & Wellner
	statistic*=(1.0 + 4.0/(double)n + 25/(double(n)*double(n)));
	
	//--


	//my name... is neo
	return true;
}


void makeHistogram(const std::vector<float> &data, float start, 
			float end, float step, std::vector<float> &histVals);


#ifdef DEBUG

//Unit test function for anderson statistics. Should be able to check that gaussian numbers are in fact gaussian 
bool testAnderson();

//Check that the background fitting routine can fit
// a random TOF data histogram
bool testBackgroundFitMaths();

#endif
#endif
