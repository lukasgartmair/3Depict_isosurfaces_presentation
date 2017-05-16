/* 
 * rdf.h - Radial distribution function implementation header
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

#ifndef RDF_H
#define RDF_H

#include "K3DTree.h"


//RDF error codes
enum
{
	RDF_ERR_NEGATIVE_SCALE_FACT=1,
	RDF_ERR_INSUFFICIENT_INPUT_POINTS,
	RDF_FILE_OPEN_FAIL,
	RDF_ABORT_FAIL
};

//!Generate the NN histogram specified up to a given NN
unsigned int generateNNHist( const std::vector<Point3D> &pointList, 
			const K3DTree &tree,unsigned int nnMax, unsigned int numBins,
		       	std::vector<std::vector<size_t> > &histogram, float *binWidth,
		       	unsigned int *progressPtr,ATOMIC_BOOL &wantAbort);

//!Generate an NN histogram using distance max cutoffs. Input histogram must be zeroed,
//if a voxelsname is given, a 3D RDF will be recorded. in this case voxelBins must be nonzero
unsigned int generateDistHist(const std::vector<Point3D> &pointList, const K3DTree &tree,
			unsigned int *histogram, float distMax,
			unsigned int numBins, unsigned int &warnBiasCount,
			unsigned int *progressPtr,ATOMIC_BOOL &wantAbort);

//!Returns a subset of points guaranteed to lie at least reductionDim inside hull of input points
/*! Calculates the hull of the input ions and then scales the hull such that the 
 * smallest distance between the scaled hull and the original hull is  exactly
 * reductionDim
 */
unsigned int GetReducedHullPts(const std::vector<Point3D> &pts, float reductionDim,
		unsigned int  *progress, ATOMIC_BOOL &wantAbort, std::vector<Point3D> &returnIons );


//Return a 1D histogram of NN frequencies, by projecting the NNs within a given search onto a specified axis, stopping at some fixed sstance
// radius onto a specified vector prior to histogram summation. 
//	- axisDir  must be normalised.
unsigned int generate1DAxialDistHist(const std::vector<Point3D> &pointList, const K3DTree &tree,
		const Point3D &axisDir, unsigned int *histogram, float distMax, unsigned int numBins,
		unsigned int *progressPtr, ATOMIC_BOOL &wantAbort);


//Generate a 1D distribution of NN distances s projected onto a specified axis
// Inputs are the axis to project onto, a prezeroed 1D histogram array (size numBIns),
//  and the input data points (search src) and tree (search target)
// Outputs are the histogram values , and the bin width for the histogram
unsigned int generate1DAxialNNHist(const std::vector<Point3D> &pointList, const K3DTree &tree,
			const Point3D &axisDir, unsigned int *histogram, 
			float &binWidth, unsigned int nnMax, unsigned int numBins,
			unsigned int *progressPtr, ATOMIC_BOOL &wantAbort);


//generate the Knn probability distribution for a given nn occurring at a radius in 3D space
// with a fixed density parameter. Radii are the positions to evaluate the distribution
// nnDist will store the answer.  It is required that both density >=0 and nn >0.
void generateKnnTheoreticalDist(const std::vector<float> &radii, float density, unsigned int nn,
					std::vector<float> &nnDist);
#endif
