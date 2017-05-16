 /*
 * common/voxels.h - Voxelised data manipulation class
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

#ifndef VOXELS_H
#define VOXELS_H


const unsigned int MAX_CALLBACK=500;

#include "common/basics.h"

#include <stack>
#include <numeric>

// Not sure who is defining this, but it is causing problems - mathgl?
#undef I 
#undef Complex
#include <typeinfo>
#if defined(WIN32) || defined(WIN64)
#include <vigra/windows.h>
#endif
#include <vigra/multi_array.hxx>
#include <vigra/multi_convolution.hxx>

#include <typeinfo> //Bug in accumulator.hxx. Needs typeinfo

#include <vigra/accumulator.hxx>

using namespace std;


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _OPENMP
	#include <omp.h>
#endif

//!Boundary clipping mode
/*! These constants defines the behaviour of the routines when presented with a
 * boundary value problem, such as in convolution.
 */
enum{	
	BOUND_CLIP=1,
	BOUND_HOLD,
	BOUND_DERIV_HOLD,
	BOUND_MIRROR,
	BOUND_ZERO,
	BOUND_ENUM_END
};

//Interpolation mode for slice 
enum
{
	VOX_INTERP_NONE,
	VOX_INTERP_LINEAR,
	VOX_INTERP_ENUM_END
};

enum{
	ADJ_ALL,
	ADJ_PLUS
};

//Error codes
enum{
	VOXELS_BAD_FILE_READ=1,
	VOXELS_BAD_FILE_OPEN,
	VOXELS_BAD_FILE_SIZE,
	VOXELS_OUT_OF_MEMORY
};

//Must be power of two (buffer size when loading files, in sizeof(T)s)
const unsigned int ITEM_BUFFER_SIZE=65536;

//!Clipping direction constants
/*! Controls the clipping direction when performing clipping operations
 */
enum {
	CLIP_NONE=0,
       CLIP_LOWER_SOUTH_WEST,
       CLIP_UPPER_NORTH_EAST
};


enum {
	VOXEL_ABORT_ERR,
	VOXEL_MEMORY_ERR,
	VOXEL_BOUNDS_INVALID_ERR
};

#ifdef DEBUG
	bool runVoxelTests();
#endif
static const bool *voxelsWantAbort;
//!Template class that stores 3D voxel data
/*! To instantiate this class, objects must have
 * basic mathematical operators, such as * + - and =
 */
//FIXME: Cross check all algorithsm agree that the centre of the voxel is 
// where the data value is located
template<class T> class Voxels
{
	private:
		//!Number of bins in data set (X,Y,Z)
		size_t binCount[3];

		//!Voxel array 
		vigra::MultiArray<3,T> voxels;
		
		//!Scaling value for furthest bound of the dataset. 
		//Dataset is assumed to sit in a rectilinear volume from minBound
		//to maxBound
		Point3D minBound, maxBound;
	
			

		void localPaddedConvolve(long long ui,long long uj, long long uk, 
			const Voxels<T> &kernel,Voxels<T> &result, unsigned int mode) const;
	public:

		//!Constructor.
		Voxels();
		//!Destructor
		~Voxels();

		//!Swap object contents with other voxel object
		void swap(Voxels<T> &v);

		//!Clone this into another object
		void clone(Voxels<T> &newClone) const;

		//!Set the value of a point in the dataset
		void setPoint(const Point3D &pt, const T &val);
		//!Retrieve the value of a datapoint, this is rounded to the nearest voxel
		T getPointData(const Point3D &pt) const;


		//!Retrieve the XYZ voxel location associated with a given position
		void getIndex(size_t &x, size_t &y,
				size_t &z, const Point3D &p) const;
		
		//!Retrieve the XYZ voxel location associated with a given position,
		// including upper borders
		void getIndexWithUpper(size_t &x, size_t &y,
				size_t &z, const Point3D &p) const;
		
		//!Get the position associated with an XYZ voxel
		Point3D getPoint(size_t x, 
				size_t y, size_t z) const;
		//!Retrieve the value of a specific voxel
		inline T getData(size_t x, size_t y, size_t z) const;
		inline T getData(size_t *array) const;
		//!Retrieve value of the nth voxel
		inline T getData(size_t i) const { return voxels[i];}

		void setEntry(size_t n, const T &val) { voxels[n] = val;};
		//!Retrieve a reference to the data ata  given position
		//const T &getDataRef(size_t x, size_t y, size_t z) const;
		//!Set the value of a point in the dataset
		void setData(size_t x, size_t y, size_t z, const T &val);
		//!Set the value of nth point in the dataset
		void setData(size_t n, const T &val);

		//Obtain an interpolated entry. The interpolated values are obtained by padding
		void getInterpolatedData(const Point3D &pt, T &v) const;

		//Perform in-place gaussian smoothing
		void isotropicGaussianSmooth(float stdev,float windowRatio);

		//Perform in-place laplacian smoothing
		void laplaceOfGaussian(float stdev, float windowRatio);

		//get an interpolated slice from a section of the data
		void getInterpSlice(size_t normal, float offset, T *p, 
			size_t interpMode=VOX_INTERP_NONE) const;

		//Get a specific slice, from an integral offset in the data, no interp
		void getSlice(size_t normal, size_t offset, T *p) const;
		//!Get the size of the data field
		void getSize(size_t &x, size_t &y, size_t &z) const;
		size_t getSize() const {return voxels.size();};

		//!Resize the data field
		/*! This will destroy any data that was already in place
		 * If the data needs to be preserved use "resizeKeepData"
		 * Data will *not* be initialised
		 */
		size_t resize(size_t newX, size_t newY, size_t newZ, const Point3D &newMinBound=Point3D(0.0f,0.0f,0.0f), const Point3D &newMaxBound=Point3D(1.0f,1.0f,1.0f));
	
		size_t resize(const Voxels<T> &v);

		//!Resize the data field, maintaining data as best as possible
		/*! This will preserve data by resizing as much as possible 
		 * about the origin. If the bounds are extended, the "fill" value is used
		 * by default iff doFill is set to true. 
		 */
		size_t resizeKeepData(size_t newX, size_t newY, size_t newZ, 
					unsigned int direction=CLIP_LOWER_SOUTH_WEST, const Point3D &newMinBound=Point3D(0.0f,0.0f,0.0f), const Point3D &newMaxBound=Point3D(1.0f,1.0f,1.0f), const T &fill=T(0),bool doFill=false);



		//!DEPRECATED FUNCTION : Get a unique integer that corresponds to the edge index for the voxel; where edges are shared between voxels
		/*! Each voxel has 12 edges. These are shared (except
		 * voxels that on zero or positive boundary). Return a
	 	 * unique index that corresponds to a specified edge (0->11).
		 * Index *CANNOT* be inverted to yield cell
		 */
		size_t deprecatedGetEdgeUniqueIndex(size_t x,
			size_t y, size_t z, unsigned int edge) const;

		//!Get a unique integer that corresponds to an edge index for the voxel; where edges are shared between voxels
		/*! Each voxel has 12 edges. These are shared (except
		 * voxels that on zero or positive boundary). Return a
	 	 * NON-unique index that corresponds to a specified edge (0->11)
		 * Index can be inverted to yield cell
		 */
		size_t getCellUniqueEdgeIndex(size_t x,
			size_t y, size_t z, unsigned int edge) const;
	
		//!Convert the edge index (as generated by getEdgeUniqueIndex) into a cenre position	
		// returns the axis value so you know edge vector too.
		// NOte that the value to pass as the edge index is (getEdgeIndex>>2)<<2 to
		// make the ownership of the voxel correct 
		void getEdgeEnds(size_t edgeIndex,Point3D &a, Point3D &b) const;


		//!Convert edge index (only as generted by getCellUniqueEdgeIndex) into a cell & axis value
		void getEdgeCell(size_t edgeUniqId, size_t &x,size_t &y, size_t &z, size_t &axis ) const;

		//TODO: there is duplicate code between this and getEdgeEnds. Refactor.
		//!Return the values that are associated with the edge ends, as returned by getEdgeEnds
		void getEdgeEndApproxVals(size_t edgeUniqId, T  &a, T  &b ) const;


		//!Rebin the data by a given rate
		/*! This will perform a quick and dirty rebin operation, where groups of datablocks 
		 * are binned into a single cell. Number of blocks binned is rate^3. Field must be larger than rate
		 * in all directions. Currently only CLIP_NONE is supported.
		 */
		void rebin(Voxels<T> &dest, size_t rate, size_t clipMode=CLIP_NONE) const;
		
		//!Get the total value of the data field.
		/*! An "initial value" is provided to provide the definition of a zero value
		 */
		T getSum(const T &initialVal=T(0.0)) const;

		//!count the number of cells with at least this intensity
		size_t count(const T &minIntensity) const;

		//!Fill all voxels with a given value
		void fill(const T &val);
		//!Get the bounding box vertex (min/max) 
		Point3D getMinBounds() const;
		Point3D getMaxBounds() const;
		//Obtain the ounds for a specified axis
		void getAxisBounds(size_t axis, float &minV, float &maxV) const;
		///! Get the spacing for a unit cell
		Point3D getPitch() const;
		//!Set the bounding size
		void setBounds(const Point3D &pMin, const Point3D &pMax);
		//!Get the bounding size
		void getBounds(Point3D &pMin, Point3D &pMax) const { pMin=minBound;pMax=maxBound;}

		//!Initialise the voxel storage
		size_t init(size_t nX,size_t nY,size_t nZ, const BoundCube &bound);
		//!Initialise the voxel storage
		size_t init(size_t nX,size_t nY, size_t nZ);
		//!Load the voxels from file
		/*! Format is flat size_ts in column major
		 * return codes:
		 * 1: File open error 
		 * 2: Data size error
		 */
		size_t loadFile(const char *cpFilename, size_t nX,
						size_t nY, size_t nZ, bool silent=false);
		//!Write the voxel objects in column major written out to file
		/*! Format is flat objects ("T"s) in column major format.
		 * Returns nonzero on failure
		 */
		size_t writeFile(const char *cpFilename) const;

		//!Run convolution over this data, placing the correlation data into "result"
		/*! 
		 * Datasets MUST have the same pitch (spacing) for the result to be defined
		 * template type must have a  T(0.0) constructor that intialises it to some "zero"
		 */
		size_t convolve(const Voxels<T> &templateVec, Voxels<T> &result,
							 size_t boundMode=BOUND_CLIP) const; 

		
		//!Similar to convolve, but faster -- only works with separable kernels.
		//Destroys original data in process.
		/*! 
		 * Datasets MUST have the same pitch (spacing) for the result to be defined
		 * template type must have a  T(0.0) constructor that intialises it to some "zero"
		 */
		size_t separableConvolve(const Voxels<T> &templateVec, Voxels<T> &result,
							 size_t boundMode=BOUND_CLIP); 
		
		


		//!Find the positions of the voxels that are above or below a given value
		/*! Returns the positions of the voxels' centroids for voxels that have, by default,
		 * a value greater than that of thresh. This behaviour can by reversed to "lesser than"
		 * by setting lowerEq to false
		 */
		void thresholdForPosition(std::vector<Point3D> &p, const T &thresh, bool lowerEq=false) const;




		//!Return the sizeof value for the T type
		/*! Maybe there is a better way to do this, I don't know
		 */
		static size_t sizeofType() { return sizeof(T);}; 


		//!Binarise the data into a result vector
		/* On thresholding (val > thresh), set the value to "onThresh". 
		 * Otherwise set to "offthresh"
		 */
		void binarise(Voxels<T> &result,const T &thresh, const T &onThresh, 
				const T &offThresh) const;
	

		//!Empty the data
		/*Remove the data from the voxel set
		 */
		void clear() { voxels.reshape(vigra::Shape3(0,0,0));};

		//!Find minimum in dataset
		T min() const;

		//!Find maximum in dataset
		T max() const;	
		
		//!Find both min and max in dataset in the same loop
		void minMax(T &min, T &max) const;	


		//!Generate a dataset that consists of the counts of points in a given voxel
		/*! Ensure that the voxel scaling factors 
		 * are set by calling ::setBounds() with the 
		 * appropriate parameters for your data.
		 * Disabling nowrap allows you to "saturate" your
		 * data field in the case of dense regions, rather 
		 * than let wrap-around errors occur
		 */
		int countPoints( const std::vector<Point3D> &points, bool noWrap=true, bool doErase=false);

		//!Integrate the datataset via the trapezoidal method
		T trapezIntegral() const;	
		//! Convert voxel intensity into voxel density
		// this is done by dividing each voxel by its volume 
		void calculateDensity();

		float getBinVolume() const;

		//!Element wise division	
		void operator/=(const Voxels<T> &v);

		void operator/=(const T &v);
		
		bool operator==(const Voxels<T> &v) const;

		size_t size() const { return voxels.size();}
};

//!Convert one type of voxel into another by assignment operator
template<class T, class U>
void castVoxels(const Voxels<T> &src, Voxels<U> &dest)
{
	//TODO:  Remove me!
	src=dest;
}

//!Use one counting type to sum counts in a voxel of given type
template<class T, class U>
void sumVoxels(const Voxels<T> &src, U &counter)
{
	size_t nx,ny,nz;
	counter=0;
	src.getSize(nx,ny,nz);

	size_t nMax=src.size();
	for(size_t ui=0; ui<nMax; ui++)
	{
		counter+=src.getData(ui);
	}

}

//====
template<class T>
Voxels<T>::Voxels() : voxels(), minBound(Point3D(0,0,0)), maxBound(Point3D(1,1,1))
{
}

template<class T>
Voxels<T>::~Voxels()
{
}


template<class T>
void Voxels<T>::clone(Voxels<T> &newVox) const
{
	newVox.binCount[0]=binCount[0];
	newVox.binCount[1]=binCount[1];
	newVox.binCount[2]=binCount[2];

	newVox.voxels=voxels;
	newVox.minBound=minBound;
	newVox.maxBound=maxBound;


}

template<class T>
void Voxels<T>::setPoint(const Point3D &point,const T &val)
{
	ASSERT(voxels.size());
	size_t pos[3];
	for(size_t ui=0;ui<3;ui++)
		pos[ui] = (size_t)round(point[ui]*(float)binCount[ui]);

#ifdef DEBUG
	vigra::Shape3 s= voxels.shape();
	ASSERT(pos[0]<=s[0] && pos[1] <= s[1] && pos[2] < s[2]);
#endif

	voxels[vigra::Shape3(pos[0],pos[1],pos[2])] = val;
}

template<class T>
void Voxels<T>::setData(size_t x, size_t y, 
			size_t z, const T &val)
{
	ASSERT(voxels.size());

	ASSERT( x < binCount[0] && y < binCount[1] && z < binCount[2]);
	voxels[vigra::Shape3(x,y,z)]=val;
}

template<class T>
inline void Voxels<T>::setData(size_t n, const T &val)
{
	ASSERT(voxels.size());
	ASSERT(n<voxels.size());

	voxels[n] =val; 
}

template<class T>
T Voxels<T>::getPointData(const Point3D &point) const
{
	ASSERT(voxels.size());
	size_t pos[3];
	Point3D offsetFrac;
	offsetFrac=point-minBound;
	for(size_t ui=0;ui<3;ui++)
	{
		offsetFrac[ui]/=(maxBound[ui]-minBound[ui]);
		pos[ui] = (size_t)round(offsetFrac[ui]*(float)binCount[ui]);
	}	

	return voxels[vigra::Shape3(pos[0],pos[1],pos[2])];
}

template<class T>
Point3D Voxels<T>::getPoint(size_t x, size_t y, size_t z) const
{
	//ASSERT(x < binCount[0] && y<binCount[1] && z<binCount[2]);

	return Point3D((float)x/(float)binCount[0]*(maxBound[0]-minBound[0]) + minBound[0],
			(float)y/(float)binCount[1]*(maxBound[1]-minBound[1]) + minBound[1],
			(float)z/(float)binCount[2]*(maxBound[2]-minBound[2]) + minBound[2]);
}

template<class T>
Point3D Voxels<T>::getPitch() const
{
	return Point3D((float)1.0/(float)binCount[0]*(maxBound[0]-minBound[0]),
			(float)1.0/(float)binCount[1]*(maxBound[1]-minBound[1]),
			(float)1.0/(float)binCount[2]*(maxBound[2]-minBound[2]));
}

template<class T>
void Voxels<T>::getSize(size_t &x, size_t &y, size_t &z) const
{
	ASSERT(voxels.size());
	x=binCount[0];
	y=binCount[1];
	z=binCount[2];
}

template<class T>
size_t Voxels<T>::deprecatedGetEdgeUniqueIndex(size_t x,size_t y, size_t z, unsigned int index) const
{
	//This provides a reversible mapping of x,y,z 
	//X aligned edges are first
	//Y second
	//Z third
	

	//Consider each parallel set of edges (eg all the X aligned edges)
	//to be the dual grid of the actual grid. From this you can visualise the
	//cell centres moving -1/2 -/12 units in the direction normal to the edge direction
	//to produce the centres of the edge. An additional vertex needs to be created at
	//the end of each dimension not equal to the alignement dim.


	//  		    ->ASCII ART TIME<-
	// In each individual cube, the offsets look like this:
        //		------------7-----------
        //		\		       |\ .
        //		|\ 		       | \ .
        //		| 10		       |  11
        //		|  \		       |   \ .
        //		|   \		       |    \ .
        //		|    \ --------6-------------|
        //		|     |                |     |
        //	      2 |                3     |     |
        //		|     |                |     |
        //		|     |                |     |
        //		|     |                |     |
        //		|     0                |     1
        //		\-----|----5-----------      |
        //	 	 \    |                 \    |
        //	 	  8   |                  9   |
        //	 	   \  |                   \  |
        //		    \ |---------4------------
	//
	//   	^x
	//  z|\	|
	//    \ |
	//     \-->y
	//

	switch(index)
	{
		//X axis aligned
		//--
		case 0:
			break;	
		case 1:
			y++; // one across in Y
			break;	
		case 2:
			z++;//One across in Z
			break;	
		case 3:
			y++;
			z++;
			break;	
		//--

		//Y Axis aligned
		//--
		case 4:
			break;	
		case 5:
			z++;
			break;	
		case 6:
			x++;
			break;	
		case 7:
			z++;
			x++;
			break;	
		//--
		
		//Z Axis aligned
		//--
		case 8:
			break;	
		case 9:
			y++;
			break;	
		case 10:
			x++;
			break;	
		case 11:
			x++;
			y++;
			break;	
		//--
	}


	size_t result = 12*(z + y*(binCount[2]+1) + x*(binCount[2]+1)*(binCount[1]+1)) +
	index;
	
	return result;

}

/*
template<class T>
size_t Voxels<T>::getEdgeUniqueIndex(size_t x,size_t y, size_t z, unsigned int index) const
{
	//This provides a reversible mapping of x,y,z 
	//X aligned edges are first
	//Y second
	//Z third
	

	//Consider each parallel set of edges (eg all the X aligned edges)
	//to be the dual grid of the actual grid. From this you can visualise the
	//cell centres moving -1/2 -/12 units in the direction normal to the edge direction
	//to produce the centres of the edge. An additional vertex needs to be created at
	//the end of each dimension not equal to the alignement dim.


	//  		    ->ASCII ART TIME<-
	// In each individual cube, the offsets look like this:
        //		------------7-----------
        //		\		       |\ .
        //		|\ 		       | \ .
        //		| 10		       |  11
        //		|  \		       |   \ .
        //		|   \		       |    \ .
        //		|    \ --------6-------------|
        //		|     |                |     |
        //	      2 |                3     |     |
        //		|     |                |     |
        //		|     |                |     |
        //		|     |                |     |
        //		|     0                |     1
        //		\-----|----5-----------      |
        //	 	 \    |                 \    |
        //	 	  8   |                  9   |
        //	 	   \  |                   \  |
        //		    \ |---------4------------
	//
	//   	^x
	//  z|\	|
	//    \ |
	//     \-->y
	//

	switch(index)
	{
		//X axis aligned
		//--
		case 0:
			break;	
		case 1:
			y++; // one across in Y
			break;	
		case 2:
			z++;//One across in Z
			break;	
		case 3:
			y++;
			z++;
			break;	
		//--

		//Y Axis aligned
		//--
		case 4:
			break;	
		case 5:
			z++;
			break;	
		case 6:
			x++;
			break;	
		case 7:
			z++;
			x++;
			break;	
		//--
		
		//Z Axis aligned
		//--
		case 8:
			break;	
		case 9:
			y++;
			break;	
		case 10:
			x++;
			break;	
		case 11:
			x++;
			y++;
			break;	
		//--
	}

	unsigned int axis = index/4;
	size_t result = 3*(z + y*(binCount[2]+1) + x*(binCount[2]+1)*(binCount[1]+1)) + axis;
	
	return result;

}
*/
template<class T>
size_t Voxels<T>::getCellUniqueEdgeIndex(size_t x,size_t y, size_t z, unsigned int index) const
{
	//This provides a reversible mapping of x,y,z 
	//X aligned edges are first
	//Y second
	//Z third
	

	//Consider each parallel set of edges (eg all the X aligned edges)
	//to be the dual grid of the actual grid. From this you can visualise the
	//cell centres moving -1/2 -/12 units in the direction normal to the edge direction
	//to produce the centres of the edge. An additional vertex needs to be created at
	//the end of each dimension not equal to the alignement dim.


	//  		    ->ASCII ART TIME<-
	// In each individual cube, the offsets look like this:
        //		------------7-----------
        //		\		       |\ .
        //		|\ 		       | \ .
        //		| 10		       |  11
        //		|  \		       |   \ .
        //		|   \		       |    \ .
        //		|    \ --------6-------------|
        //		|     |                |     |
        //	      2 |                3     |     |
        //		|     |                |     |
        //		|     |                |     |
        //		|     |                |     |
        //		|     0                |     1
        //		\-----|----5-----------      |
        //	 	 \    |                 \    |
        //	 	  8   |                  9   |
        //	 	   \  |                   \  |
        //		    \ |---------4------------
	//
	//   	^x
	//  z|\	|
	//    \ |
	//     \-->y
	//

	size_t cellIdx = 12*(z + y*(binCount[2]+1) + x*(binCount[2]+1)*(binCount[1]+1)) ;

	cellIdx+=index;
	return cellIdx;

}

template<class T>
void Voxels<T>::getEdgeCell(size_t edgeUniqId, size_t &x,size_t &y, size_t &z, size_t &axis ) const
{
	//Invert the mapping generated by the edgeUniqId function
	//to retrieve the XYZ and axis values
	//--
	axis=(edgeUniqId%12)/4;

	size_t tmp = edgeUniqId/12;
	x = tmp/((binCount[2]+1)*(binCount[1]+1));
	tmp-=x*((binCount[2]+1)*(binCount[1]+1));

	y=tmp/(binCount[2]+1);
	tmp-=y*(binCount[2]+1);

	z=tmp;
	//--

	ASSERT(x< binCount[0]+1 && y<binCount[1]+1 && z<binCount[2]+1);
}
template<class T>
void Voxels<T>::getEdgeEnds(size_t edgeUniqId, Point3D &a, Point3D &b ) const
{
	size_t x,y,z;
	size_t axis;
	getEdgeCell(edgeUniqId,x,y,z,axis);

	Point3D delta=getPitch();
	Point3D cellCentre=getPoint(x,y,z);

	//Generate ends of edge, as seen in ascii diagram in uniqueID
	switch(axis)
	{
		case 0:
			//|| x
			a=cellCentre;
			b=cellCentre + Point3D(delta[0],0,0);
			break;

		case 1:
			//|| y
			a=cellCentre;
			b=cellCentre + Point3D(0,delta[1],0);
			break;
		case 2:
			//|| z
			a=cellCentre; 
			b=cellCentre + Point3D(0,0,delta[2]);
			break;
		default:
			ASSERT(false);
	}


#ifdef DEBUG
	BoundCube bc;
	bc.setBounds(getMinBounds(), getMaxBounds());
	bc.expand(sqrtf(std::numeric_limits<float>::epsilon()));
	ASSERT(bc.containsPt(a) && bc.containsPt(b));
#endif
}

template<class T>
void Voxels<T>::getEdgeEndApproxVals(size_t edgeUniqId, T  &a, T  &b ) const
{

	//TODO: Speed me up? Could not use
	// 	other routines to do this access.
	//	I think some redundant calculations are done
	Point3D ptA,ptB;
	getEdgeEnds(edgeUniqId,ptA,ptB);
	size_t x,y,z;
	getIndex(x,y,z,ptA);	
	a = getPointData(ptA);
	b=getPointData(ptB);
}

template<class T>
void Voxels<T>::getAxisBounds(size_t axis, float &minV, float &maxV ) const
{
	maxV=maxBound[axis];
	minV=minBound[axis];
}

template<class T>
size_t Voxels<T>::resize(size_t x, size_t y, size_t z, const Point3D &newMinBound, const Point3D &newMaxBound) 
{
	binCount[0] = x;
	binCount[1] = y;
	binCount[2] = z;


	minBound=newMinBound;
	maxBound=newMaxBound;

	try
	{
	voxels.reshape(vigra::Shape3(x,y,z));
	}
	catch(...)
	{
		return 1;
	}

	return 0;
}

template<class T>
size_t Voxels<T>::resize(const Voxels<T> &oth)
{
	return resize(oth.binCount[0],oth.binCount[1],oth.binCount[2],oth.minBound,oth.maxBound);
}

template<class T>
size_t Voxels<T>::resizeKeepData(size_t newX, size_t newY, size_t newZ, 
			unsigned int direction, const Point3D &newMinBound, const Point3D &newMaxBound, const T &fill,bool doFill)
{

	ASSERT(direction==CLIP_LOWER_SOUTH_WEST);

	Voxels<T> v;
	
	if(v.resize(newX,newY,newZ))
		return 1;

	switch(direction)
	{
		case CLIP_LOWER_SOUTH_WEST:
		{
			minBound=newMinBound;
			maxBound=newMaxBound;

			if(doFill)
			{
				size_t itStop[3];
				itStop[0]=std::min(newX,binCount[0]);
				itStop[1]=std::min(newY,binCount[1]);
				itStop[2]=std::min(newZ,binCount[2]);

				size_t itMax[3];
				itMax[0]=std::max(newX,binCount[0]);
				itMax[1]=std::max(newY,binCount[1]);
				itMax[2]=std::max(newZ,binCount[2]);
				//Duplicate into new value, if currently inside bounding box
				//This logic will be a bit slow, owing to repeated "if"ing, but
				//it is easy to understand. Other logics would have many more
				//corner cases
				bool spin=false;
#pragma omp parallel for
				for(size_t ui=0;ui<itMax[0];ui++)
				{
					if(spin)
						continue;

					for(size_t uj=0;uj<itMax[1];uj++)
					{
						for(size_t uk=0;uk<itMax[2];uk++)
						{
							if(itStop[0]< binCount[0] && 
								itStop[1]<binCount[1] && itStop[2] < binCount[2])
								v.setData(ui,uj,uk,getData(ui,uj,uk));
							else
								v.setData(ui,uj,uk,fill);
						}
					}

#pragma omp critical
					{
					if(*voxelsWantAbort)
						spin=true;
					}
				}

				if(spin)
					return VOXEL_ABORT_ERR;
			}
			else
			{
				//Duplicate into new value, if currently inside bounding box
				bool spin=false;
#pragma omp parallel for
				for(size_t ui=0;ui<newX;ui++)
				{
					if(spin)
						continue;

					for(size_t uj=0;uj<newY;uj++)
					{
						for(size_t uk=0;uk<newZ;uk++)
							v.setData(ui,uj,uk,getData(ui,uj,uk));
					}

#pragma omp critical
					{
					if(*voxelsWantAbort)
						spin=true;
					}
				}

				if(spin)
					return VOXEL_ABORT_ERR;
			}



			break;
		}

		default:
			//Not implemented
			ASSERT(false);
	}

	swap(v);
	return 0;
}

template<class T>
Point3D Voxels<T>::getMinBounds() const
{
	ASSERT(voxels.size());
	return minBound;
}
										 
template<class T>
Point3D Voxels<T>::getMaxBounds() const
{
	ASSERT(voxels.size());
	return maxBound;
}
										 
template<class T>
void Voxels<T>::setBounds(const Point3D &pMin, const Point3D &pMax)
{
	ASSERT(voxels.size());
	minBound=pMin;
	maxBound=pMax;
}

template<class T>
size_t Voxels<T>::init(size_t nX, size_t nY,
	       				size_t nZ, const BoundCube &bound)
{
	binCount[0]=nX;
	binCount[1]=nY;
	binCount[2]=nZ;

	bound.getBounds(minBound, maxBound);

	voxels.reshape(vigra::Shape3(nX,nY,nZ));

	voxels=0;

	return 0;
}

template<class T>
size_t Voxels<T>::init(size_t nX, size_t nY, size_t nZ)

{
	Point3D pMin(0,0,0), pMax(nX,nY,nZ); 

	return init(nX,nY,nZ,pMin,pMax);
}

template<class T>
size_t Voxels<T>::loadFile(const char *cpFilename, size_t nX, size_t nY, size_t nZ , bool silent)
{
	std::ifstream CFile(cpFilename,std::ios::binary);

	if(!CFile)
		return VOXELS_BAD_FILE_OPEN;
	
	CFile.seekg(0,std::ios::end);
	
	
	size_t fileSize = CFile.tellg();
	if(fileSize !=nX*nY*nZ*sizeof(T))
		return VOXELS_BAD_FILE_SIZE;

	resize(nX,nY,nZ,Point3D(nX,nY,nZ));
	
	CFile.seekg(0,std::ios::beg);

	unsigned int curBufferSize=ITEM_BUFFER_SIZE*sizeof(T);
	unsigned char *buffer = new unsigned char[curBufferSize];

	//Shrink the buffer size by factors of 2
	//in the case of small datasets
	while(fileSize < curBufferSize)
		curBufferSize = curBufferSize >> 1;

	
	//Draw a progress bar
	if(!silent)
	{
		cerr << std::endl << "|";
		for(unsigned int ui=0; ui<100; ui++)
			cerr << ".";
		cerr << "| 100%" << std::endl << "|";
	}
		
	unsigned int lastFrac=0;
	unsigned int ui=0;
	unsigned int pts=0;
	do
	{
	
		//Still have data? Keep going	
		while((size_t)CFile.tellg() <= fileSize-curBufferSize)
		{
			//Update progress bar
			if(!silent && ((unsigned int)(((float)CFile.tellg()*100.0f)/(float)fileSize) > lastFrac))
			{
				cerr << ".";
				pts++;
				lastFrac=(unsigned int)(((float)CFile.tellg()*100.0f)/(float)fileSize) ;	
			}

			//Read a chunk from the file
			CFile.read((char *)buffer,curBufferSize);
	
			if(!CFile.good())
				return VOXELS_BAD_FILE_READ;

			//Place the chunk contents into ram
			for(size_t position=0; position<curBufferSize; position+=(sizeof(T)))
				voxels[ui++] = (*((T *)(buffer+position)));
		}
				
		//Halve the buffer size
		curBufferSize = curBufferSize >> 1 ;

	}while(curBufferSize> sizeof(T)); //This does a few extra loops. Not many 

	delete[] buffer;

	//Fill out the progress bar
	if(!silent)
	{
		while(pts++ <100)
			cerr << ".";
	
		cerr << "| done" << std::endl;
	}

	return 0;
}

template<class T>
size_t Voxels<T>::writeFile(const char *filename) const
{

	ASSERT(voxels.size())

	std::ofstream file(filename, std::ios::binary);

	if(!file)
		return 1;

	
	for(size_t ui=0; ui<voxels.size(); ui++)
	{
		T v;
		v=voxels[ui];
		file.write((char *)&v,sizeof(T));
		if(!file.good())
			return 2;
	}
	return 0;
}


template<class T>
T Voxels<T>::getSum(const T &initialValue) const
{
	ASSERT(voxels.size());

	T tmp(initialValue);
	size_t n=voxels.size();
#pragma omp parallel for reduction(+:tmp)
	for(size_t ui=0;ui<n;ui++)
		tmp+=voxels[ui];

	return tmp;
}

template<class T>
T Voxels<T>::trapezIntegral() const
{
	//Compute volume prefactor - volume of cube of each voxel
	//--
	float prefactor=1.0;
	for(size_t ui=0;ui<3;ui++)
	{
		prefactor*=(maxBound[ui]-
			minBound[ui])/
			(float)binCount[ui];
	}

	//--


	double accumulation(0.0);
	//Loop across dataset integrating along z direction
#pragma omp parallel for reduction(+:accumulation)
	for(size_t ui=0;ui<voxels.size(); ui++)
		accumulation+=voxels[ui];

	return prefactor*accumulation;
}


template<class T>
size_t Voxels<T>::count(const T &minIntensity) const
{
	size_t bins;
	bins=binCount[0]*binCount[1]*binCount[2];

	size_t sum=0;
#pragma omp parallel for reduction(+:sum)
	for(size_t ui=0;ui<bins; ui++)
	{
		if(voxels[ui]>=minIntensity)
			sum++;
	}

	return sum;
}

template<class T>
void Voxels<T>::swap(Voxels<T> &other)
{
	std::swap(binCount[0],other.binCount[0]);
	std::swap(binCount[1],other.binCount[1]);
	std::swap(binCount[2],other.binCount[2]);

	voxels.swap(other.voxels);
	
	std::swap(maxBound,other.maxBound);
	std::swap(minBound,other.minBound);
}

template<class T>
T Voxels<T>::getData(size_t x, size_t y, size_t z) const
{
	ASSERT(x < binCount[0] && y < binCount[1] && z < binCount[2]);
	return	voxels[vigra::Shape3(x,y,z)]; 
}

template<class T>
void Voxels<T>::thresholdForPosition(std::vector<Point3D> &p, const T &thresh, bool lowerEq) const
{
	p.clear();

	if(lowerEq)
	{
#pragma omp parallel for 
		for(size_t ui=0;ui<binCount[0]; ui++)
		{
			for(size_t uj=0;uj<binCount[1]; uj++)
			{
				for(size_t uk=0;uk<binCount[2]; uk++)
				{
					if( getData(ui,uj,uk) < thresh)
					{
#pragma omp critical
						p.push_back(getPoint(ui,uj,uk));
					}

				}
			}
		}
	}
	else
	{
#pragma omp parallel for 
		for(size_t ui=0;ui<binCount[0]; ui++)
		{
			for(size_t uj=0;uj<binCount[1]; uj++)
			{
				for(size_t uk=0;uk<binCount[2]; uk++)
				{
					if( getData(ui,uj,uk) > thresh)
					{
#pragma omp critical
						p.push_back(getPoint(ui,uj,uk));
					}

				}
			}
		}
	}
}

template<class T>
void Voxels<T>::binarise(Voxels<T> &result, const T &thresh, 
				const T &onThresh, const T &offThresh) const
{

	result.resize(binCount[0],binCount[1],
			binCount[2],minBound,maxBound);
#pragma omp parallel for
	for(size_t ui=0;ui<(size_t)binCount[0]; ui++)
	{
		for(size_t uj=0;uj<binCount[1]; uj++)
		{
			for(size_t uk=0;uk<binCount[2]; uk++)
			{
				if( getData(ui,uj,uk) < thresh)
					result.setData(ui,uj,uk,offThresh);
				else
				{
					result.setData(ui,uj,uk,onThresh);
				}
			}
		}
	}
}



template<class T>
T Voxels<T>::min() const
{
	ASSERT(voxels.size());

	using namespace vigra::acc;
	AccumulatorChain<T,Select<Minimum> > s;
	extractFeatures(voxels.begin(),voxels.end(),s);
	return get<Minimum>(s);
}

template<class T>
T Voxels<T>::max() const
{
	ASSERT(voxels.size());
	
	using namespace vigra::acc;
	AccumulatorChain<T,Select<Maximum> > s;
	extractFeatures(voxels.begin(),voxels.end(),s);
	return get<Maximum>(s);
}


template<class T>
void Voxels<T>::minMax(T &min, T&max) const
{
	ASSERT(voxels.size());
	
	using namespace vigra::acc;
	AccumulatorChain<T,Select<Minimum,Maximum> > s;
	extractFeatures(voxels.begin(),voxels.end(),s);
	min=get<Minimum>(s);
	max=get<Maximum>(s);
}

template<class T>
int Voxels<T>::countPoints( const std::vector<Point3D> &points, bool noWrap, bool doErase)
{
	if(doErase)
	{
		fill(0);	
	}

	size_t x,y,z;

	for(size_t ui=0; ui<points.size(); ui++)
	{
		if(*voxelsWantAbort)
			return VOXEL_ABORT_ERR;
		
		T value;
		getIndex(x,y,z,points[ui]);

		//Ensure it lies within the dataset	
		if(x < binCount[0] && y < binCount[1] && z< binCount[2])
		{
			{
				value=getData(x,y,z)+T(1);

				//Prevent wrap-around errors
				if (noWrap) {
					if(value > getData(x,y,z))
						setData(x,y,z,value);
				} else {
					setData(x,y,z,value);
				}
			}
		}	
	}

	return 0;
}

template<class T>
void Voxels<T>::calculateDensity()
{
	Point3D size = maxBound - minBound;
	// calculate the volume of a voxel
	double volume = 1.0;
	for (int i = 0; i < 3; i++)
		volume *= size[i] / binCount[i];
	
	// normalise the voxel value based on volume
#pragma omp parallel for
	for(size_t ui=0; ui<voxels.size(); ui++) 
		voxels[ui] /= volume;

}

template<class T>
float Voxels<T>::getBinVolume() const
{
	Point3D size = maxBound - minBound;
	double volume = 1.0;
	for (int i = 0; i < 3; i++)
		volume *= size[i] / binCount[i];

	return volume;
}

template<class T>
void Voxels<T>::getIndex(size_t &x, size_t &y,
	       			size_t &z, const Point3D &p) const
{

	ASSERT(p[0] >=minBound[0] && p[1] >=minBound[1] && p[2] >=minBound[2] &&
		   p[0] <=maxBound[0] && p[1] <=maxBound[1] && p[2] <=maxBound[2]);
	x=(size_t)((p[0]-minBound[0])/(maxBound[0]-minBound[0])*(float)binCount[0]);
	y=(size_t)((p[1]-minBound[1])/(maxBound[1]-minBound[1])*(float)binCount[1]);
	z=(size_t)((p[2]-minBound[2])/(maxBound[2]-minBound[2])*(float)binCount[2]);
}

template<class T>
void Voxels<T>::getIndexWithUpper(size_t &x, size_t &y,
	       			size_t &z, const Point3D &p) const
{
	//Get the data index as per normal
	getIndex(x,y,z,p);

	//but, as a special case, if the index is the same as our bincount, check
	//to see if it is positioned on an edge
	if(x==binCount[0] &&  
		fabs(p[0] -maxBound[0]) < sqrtf(std::numeric_limits<float>::epsilon()))
		x--;
	if(y==binCount[1] &&  
		fabs(p[1] -maxBound[1]) < sqrtf(std::numeric_limits<float>::epsilon()))
		y--;
	if(z==binCount[2] &&  
		fabs(p[2] -maxBound[2]) < sqrtf(std::numeric_limits<float>::epsilon()))
		z--;

}

template<class T>
void Voxels<T>::fill(const T &v)
{
	voxels=v;
}

//Obtain a slice of the voxel data. Data output will be in column order
// p[posB*nA + posA]. Input slice must be sufficiently sized and allocated
// to hold the output data
template<class T>
void Voxels<T>::getSlice(size_t normalAxis, size_t offset, T *p) const
{
	ASSERT(normalAxis < 3);

	size_t dimA,dimB,nA;
	switch(normalAxis)
	{
		case 0:
		{
			dimA=1;
			dimB=2;
			nA=binCount[dimA];
			break;
		}
		case 1:
		{	
			dimA=0;
			dimB=2;
			nA=binCount[dimA];
			break;
		}
		case 2:
		{
			dimA=0;
			dimB=1;
			nA=binCount[dimA];
			break;
		}
		default:
			ASSERT(false); //WTF - how did you get here??
	}
		

	//We are within bounds, use normal access functions
	switch(normalAxis)
	{
		case 0:
		{
			for(size_t ui=0;ui<binCount[dimA];ui++)
			{
				for(size_t uj=0;uj<binCount[dimB];uj++)
					p[uj*nA + ui] =	getData(offset,ui,uj);
			}
			break;
		}
		case 1:
		{	
			for(size_t ui=0;ui<binCount[dimA];ui++)
			{
				for(size_t uj=0;uj<binCount[dimB];uj++)
					p[uj*nA + ui] =	getData(ui,offset,uj);
			}
			break;
		}
		case 2:
		{
			for(size_t ui=0;ui<binCount[dimA];ui++)
			{
				for(size_t uj=0;uj<binCount[dimB];uj++)
					p[uj*nA + ui] =	getData(ui,uj,offset);
			}
			break;
		}
		default:
			ASSERT(false);
	}
}

template<class T>
void Voxels<T>::getInterpSlice(size_t normal, float offset, 
		T *p, size_t interpMode) const
{
	ASSERT(offset <=1.0f && offset >=0.0f);

	//Obtain the appropriately interpolated slice
	switch(interpMode)
	{
		case VOX_INTERP_NONE:
		{
			size_t slicePos;
			slicePos=roundf(offset*binCount[normal]);
			slicePos=std::min(slicePos,binCount[normal]-1);
			getSlice(normal,slicePos,p);
			break;
		}
		case VOX_INTERP_LINEAR:
		{
			//Find the upper and lower bounds, then
			// limit them so we don't fall off the end of the dataset
			size_t sliceUpper,sliceLower;
			if(binCount[0] == 1)
				sliceUpper=sliceLower=0;
			else
			{
				sliceUpper=ceilf(offset*binCount[normal]);

				if(sliceUpper >=binCount[normal])
					sliceUpper=binCount[normal]-1;
				else if(sliceUpper==0)
					sliceUpper=1;
				
				sliceLower=sliceUpper-1;
			}

			{
				T *pLower;
				size_t numEntries=binCount[(normal+1)%3]*binCount[(normal+2)%3];
				
				pLower  = new T[numEntries];

				getSlice(normal,sliceLower,pLower);
				getSlice(normal,sliceUpper,p);

				//Get the decimal part of the float
				float integ;
				float delta=modff(offset*binCount[normal],&integ);
				for(size_t ui=0;ui<numEntries;ui++)
					p[ui] = delta*(p[ui]-pLower[ui]) + pLower[ui];

				delete[] pLower;
			}
			break;
		}
		default:
			ASSERT(false);
			
	}

}

//FIXME: I think this has a slight shift as we are moving the data voxels
// definition of the voxel centre by 1/2 a pitch, I think
template<class T>
void Voxels<T>::getInterpolatedData(const Point3D &p, T &v) const

{
#ifdef DEBUG
	BoundCube bc(minBound,maxBound);
	ASSERT(bc.containsPt(p));
#endif

	size_t index[3];
	getIndex(index[0],index[1],index[2],p);

	Point3D pitch =getPitch();
	
	//Find the offset to the voxel that we are in.
	//fraction should be in range [0,1)
	Point3D fraction = p - (minBound + Point3D(index[0],index[1],index[2])*pitch);
	fraction =fraction/pitch;


	size_t iPlus[3];
	//0.5 corresponds to voxel centre.
	for(unsigned int ui=0;ui<3;ui++)
	{
		if(index[ui] == (binCount[ui]-1))
			iPlus[ui]=0;
		else
			iPlus[ui]=1;
	}	
	

	float c[2][2];
	//Tri-linear interpolation

	//interpolate data values at cube vertices that surround point. We are coming from below the point
	// so we are simply extending the field on the upper edge by duplicating values as needed
	for(unsigned int ui=0;ui<4;ui++)
	{
		c[(ui&1)][(ui&2)>>1] = getData( index[0],index[1]+ iPlus[1]*(ui&1),index[2] + iPlus[2]*(ui&2) )*(1-fraction[0]) 
				+  getData(index[0]+iPlus[0],index[1]+ iPlus[1]*(ui&1),index[2] + iPlus[2]*(ui&2));
	}


	float c0,c1;
	c0 = c[0][0]*(1-fraction[1]) + c[1][0]*fraction[1];
	c1 = c[0][1]*(1-fraction[1]) + c[1][1]*fraction[1];

	v= c0*(1-fraction[2])*c1;	
	
}	

template<class T>
void Voxels<T>::isotropicGaussianSmooth(float stdev,float windowRatio)
{
	//perform in-place smoothing
	vigra::ConvolutionOptions<3> opt  = vigra::ConvolutionOptions<3>().filterWindowSize(windowRatio);
	vigra::gaussianSmoothMultiArray(vigra::srcMultiArrayRange(voxels),
			vigra::destMultiArray(voxels),stdev,opt);

}

template<class T>
void Voxels<T>::laplaceOfGaussian(float stdev, float windowRatio)
{
	//perform in-place smoothing
	vigra::ConvolutionOptions<3> opt  = vigra::ConvolutionOptions<3>().filterWindowSize(windowRatio);
	vigra::laplacianOfGaussianMultiArray(vigra::srcMultiArrayRange(voxels),
			vigra::destMultiArray(voxels),stdev,opt);

}

template<class T>
void Voxels<T>::operator/=(const Voxels<T> &v)
{
	ASSERT(v.voxels.size() == voxels.size());

	//don't use the built-in /, this 
	// can generate inf values (which is correct)
	// that we want to avoid.
	for(size_t ui=0;ui<voxels.size();ui++)
	{
		if(v.voxels[ui] )
			voxels[ui]/=v.voxels[ui];
		else
		{
			ASSERT(!voxels[ui]);
		} 
	}
}	

	
template<class T>
void Voxels<T>::operator/=(const T &v)
{
	ASSERT(v.voxels.size() == voxels.size());

	//don't use the built-in /, this 
	// can generate inf values (which is correct)
	// that we want to avoid.
	for(size_t ui=0;ui<voxels.size();ui++)
	{
		if( v > T(0) )
			voxels[ui]/=v;
		else
		{
			voxels[ui]=0;
		} 
	}
}	

template<class T>
bool Voxels<T>::operator==(const Voxels<T> &v) const
{
	for(size_t ui=0;ui<3;ui++)
	{
		
		if(v.binCount[ui] != binCount[ui])
			return false;
	}

	return v.voxels == voxels;
}

//===


#endif
