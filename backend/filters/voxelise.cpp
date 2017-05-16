/*
 *	voxelise.cpp - Compute 3D binning (voxelisation) of point clouds
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

#include "voxelise.h"
#include "common/colourmap.h"
#include "filterCommon.h"

#include "openvdb_includes.h"
#include "contribution_transfer_function_TestSuite/CTF_functions.h"
#include <math.h> // pow

#include <map>

enum
{
	KEY_FIXEDWIDTH,
	KEY_NBINSX,
	KEY_NBINSY,
	KEY_NBINSZ,
	KEY_WIDTHBINSX,
	KEY_WIDTHBINSY,
	KEY_WIDTHBINSZ,

	// vdb
	KEY_VOXELSIZE,
	
	KEY_COUNT_TYPE,
	KEY_NORMALISE_TYPE,
	KEY_SPOTSIZE,
	KEY_TRANSPARENCY,
	KEY_COLOUR,
	KEY_ISOLEVEL,
	KEY_VOXEL_REPRESENTATION_MODE,
	
	KEY_VOXEL_SLICE_COLOURAUTO,
	KEY_MAPEND,
	KEY_MAPSTART,
	KEY_SHOW_COLOURBAR,
	KEY_VOXEL_COLOURMODE,
	KEY_VOXEL_SLICE_AXIS,
	KEY_VOXEL_SLICE_OFFSET,
	KEY_VOXEL_SLICE_INTERP,
	
	KEY_FILTER_MODE,
	KEY_FILTER_RATIO,
	KEY_FILTER_STDEV,
	KEY_ENABLE_NUMERATOR,
	KEY_ENABLE_DENOMINATOR
};

//!Normalisation method
enum
{
	VOXELISE_NORMALISETYPE_NONE,// straight count
	VOXELISE_NORMALISETYPE_VOLUME,// density
	VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL, // concentration
	VOXELISE_NORMALISETYPE_COUNT2INVOXEL,// ratio count1/count2
	VOXELISE_NORMALISETYPE_MAX // keep this at the end so it's a bookend for the last value
};

//!Filtering mode
enum
{
	VOXELISE_FILTERTYPE_NONE,
	VOXELISE_FILTERTYPE_GAUSS,
	VOXELISE_FILTERTYPE_LAPLACE,
	VOXELISE_FILTERTYPE_MAX // keep this at the end so it's a bookend for the last value
};


//Boundary behaviour for filtering 
enum
{
	VOXELISE_FILTERBOUNDMODE_ZERO,
	VOXELISE_FILTERBOUNDMODE_BOUNCE,
	VOXELISE_FILTERBOUNDMODE_MAX// keep this at the end so it's a bookend for the last value
};


//Error codes and corresponding strings
//--
enum
{
	VOXELISE_ABORT_ERR=1,
	VOXELISE_MEMORY_ERR,
	VOXELISE_CONVOLVE_ERR,
	VOXELISE_BOUNDS_INVALID_ERR,
	VOXELISE_ERR_ENUM_END
};
//--


//!Can we keep the cached contents, when transitioning from
// one representation to the other- this is only the case
// when _KEEPCACHE [] is true for both representations
const bool VOXEL_REPRESENT_KEEPCACHE[] = {
	true,
	false,
	true
};

const char *NORMALISE_TYPE_STRING[] = {
		NTRANS("None (Raw count)"),
		NTRANS("Volume (Density)"),
		NTRANS("All Ions (conc)"),
		NTRANS("Ratio (Num/Denom)"),
	};

const char *REPRESENTATION_TYPE_STRING[] = {
		NTRANS("Point Cloud"),
		NTRANS("Isosurface"),
		NTRANS("Axial slice")
	};

const char *VOXELISE_FILTER_TYPE_STRING[]={
	NTRANS("None"),
	NTRANS("Gaussian (blur)"),
	NTRANS("Lapl. of Gauss. (edges)"),
	};

const char *VOXELISE_SLICE_INTERP_STRING[]={
	NTRANS("None"),
	NTRANS("Linear")
	};

//This is not a member of voxels.h, as the voxels do not have any concept of the IonHit
int countPoints(Voxels<float> &v, const std::vector<IonHit> &points, 
				bool noWrap)
{

	size_t x,y,z;
	size_t binCount[3];
	v.getSize(binCount[0],binCount[1],binCount[2]);

	unsigned int downSample=MAX_CALLBACK;
	for (size_t ui=0; ui<points.size(); ui++)
	{
		if(!downSample--)
		{
			if(*Filter::wantAbort)
				return 1;
			downSample=MAX_CALLBACK;
		}
		v.getIndexWithUpper(x,y,z,points[ui].getPos());
		//Ensure it lies within the dataset
		if (x < binCount[0] && y < binCount[1] && z< binCount[2])
		{
			{
				float value;
				value=v.getData(x,y,z)+1.0f;

				ASSERT(value >= 0.0f);
				//Prevent wrap-around errors
				if (noWrap) {
					if (value > v.getData(x,y,z))
						v.setData(x,y,z,value);
				} else {
					v.setData(x,y,z,value);
				}
			}
		}
	}
	return 0;
}

// == Voxels filter ==
VoxeliseFilter::VoxeliseFilter() 
: fixedWidth(false), normaliseType(VOXELISE_NORMALISETYPE_NONE)
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(NORMALISE_TYPE_STRING) ==  VOXELISE_NORMALISETYPE_MAX);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(VOXELISE_FILTER_TYPE_STRING) == VOXELISE_FILTERTYPE_MAX );

	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(REPRESENTATION_TYPE_STRING) == VOXEL_REPRESENT_END);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(VOXEL_REPRESENT_KEEPCACHE) == VOXEL_REPRESENT_END);

	splatSize=1.0f;
	rgba=ColourRGBAf(0.5,0.5,0.5,0.9f);
	isoLevel=0.5;
	
	// vdb
	voxelsize = 2.0; 

	filterRatio=3.0;
	filterMode=VOXELISE_FILTERTYPE_NONE;
	gaussDev=0.5;	
	
	representation=VOXEL_REPRESENT_POINTCLOUD;

	colourMap=0;
	autoColourMap=true;
	colourMapBounds[0]=0;
	colourMapBounds[1]=1;

	// vdb cache 
	vdbCache=openvdb::FloatGrid::create(0.0);
	vdbCache->setTransform(openvdb::math::Transform::createLinearTransform(voxelsize));


	//Fictitious bounds.
	bc.setBounds(Point3D(0,0,0),Point3D(1,1,1));

	for (unsigned int i = 0; i < INDEX_LENGTH; i++) 
		nBins[i] = 50;

	calculateWidthsFromNumBins(binWidth,nBins);

	numeratorAll = true;
	denominatorAll = true;

	sliceInterpolate=VOX_INTERP_NONE;
	sliceAxis=0;
	sliceOffset=0.5;
	showColourBar=false;


	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up


	rsdIncoming=0;
}


Filter *VoxeliseFilter::cloneUncached() const
{
	VoxeliseFilter *p=new VoxeliseFilter();
	p->splatSize=splatSize;
	p->rgba=rgba;

	p->isoLevel=isoLevel;
	p->voxelsize=voxelsize;
	
	p->filterMode=filterMode;
	p->filterRatio=filterRatio;
	p->gaussDev=gaussDev;

	p->representation=representation;

	p->normaliseType=normaliseType;
	p->numeratorAll=numeratorAll;
	p->denominatorAll=denominatorAll;

	p->bc=bc;

	for(size_t ui=0;ui<INDEX_LENGTH;ui++)
	{
		p->nBins[ui] = nBins[ui];
		p->binWidth[ui] = binWidth[ui];
	}

	p->enabledIons[0].resize(enabledIons[0].size());
	std::copy(enabledIons[0].begin(),enabledIons[0].end(),p->enabledIons[0].begin());
	
	p->enabledIons[1].resize(enabledIons[1].size());
	std::copy(enabledIons[1].begin(),enabledIons[1].end(),p->enabledIons[1].begin());

	if(rsdIncoming)
	{
		p->rsdIncoming=new RangeStreamData();
		*(p->rsdIncoming) = *rsdIncoming;
	}
	else
		p->rsdIncoming=0;



	p->colourMap = colourMap;

	p->nColours = nColours;
	p->showColourBar = showColourBar;
	p->autoColourMap = autoColourMap;
	p->colourMapBounds[0] = colourMapBounds[0];
	p->colourMapBounds[1] = colourMapBounds[1];

	p->sliceInterpolate = sliceInterpolate;
	p->sliceAxis = sliceAxis;
	p->sliceOffset = sliceOffset;

	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

void VoxeliseFilter::clearCache() 
{
	voxelCache.clear();
	Filter::clearCache();
}

size_t VoxeliseFilter::numBytesForCache(size_t nObjects) const
{
	//if we are using fixed width, we know the answer.
	//otherwise we dont until we are presented with the boundcube.
	//TODO: Modify the function description to pass in the boundcube
	if(!fixedWidth)
		return 	nBins[0]*nBins[1]*nBins[2]*sizeof(float);
	else
		return 0;
}

void VoxeliseFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
						std::vector<const FilterStreamData *> &dataOut)
{
	const RangeStreamData *c=0;
	//Determine if we have an incoming range
	for (size_t i = 0; i < dataIn.size(); i++) 
	{
		if(dataIn[i]->getStreamType() == STREAM_TYPE_RANGE)
		{
			c=(const RangeStreamData *)dataIn[i];

			break;
		}
	}

	//we no longer (or never did) have any incoming ranges. Not much to do
	if(!c)
	{
		//delete the old incoming range pointer
		if(rsdIncoming)
			delete rsdIncoming;
		rsdIncoming=0;

		enabledIons[0].clear(); //clear numerator options
		enabledIons[1].clear(); //clear denominator options

		//Prevent normalisation type being set incorrectly
		// if we have no incoming range data
		if(normaliseType == VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL || normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL)
			normaliseType= VOXELISE_NORMALISETYPE_NONE;
	}
	else
	{


		//If we didn't have an incoming rsd, then make one up!
		if(!rsdIncoming)
		{
			rsdIncoming = new RangeStreamData;
			*rsdIncoming=*c;

			//set the numerator to all disabled
			enabledIons[0].resize(rsdIncoming->rangeFile->getNumIons(),0);
			//set the denominator to have all enabled
			enabledIons[1].resize(rsdIncoming->rangeFile->getNumIons(),1);
		}
		else
		{

			//OK, so we have a range incoming already (from last time)
			//-- the question is, is it the same
			//one we had before 
			//Do a pointer comparison (its a hack, yes, but it should work)
			if(rsdIncoming->rangeFile != c->rangeFile)
			{
				//hmm, it is different. well, trash the old incoming rng
				delete rsdIncoming;

				rsdIncoming = new RangeStreamData;
				*rsdIncoming=*c;

				//set the numerator to all disabled
				enabledIons[0].resize(rsdIncoming->rangeFile->getNumIons(),0);
				//set the denominator to have all enabled
				enabledIons[1].resize(rsdIncoming->rangeFile->getNumIons(),1);
			}
		}

	}
}

unsigned int VoxeliseFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
		  std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{
	//Disallow copying of anything in the blockmask. Copy everything else
	propagateStreams(dataIn,getOut,getRefreshBlockMask(),true);

	// Initialize the OpenVDB library.  This must be called at least
    	// once per program and may safely be called multiple times.
	openvdb::initialize();

	const float background = 0.0f;	

	// initialize a grid where the division result is stored
	openvdb::FloatGrid::Ptr calculation_result_grid = openvdb::FloatGrid::create(background);

	switch(representation)
	{
		case VOXEL_REPRESENT_ISOSURF:
		{

			//use the cached copy if we have it.
			if(cacheOK)
			{
				propagateCache(getOut);
				return 0;
			}

			std::cout << " enter isosurf representation" << std::endl;
			
			std::cout << "cache = " << cache << std::endl;

			float single_voxel_volume = std::pow(voxelsize,3);

			if(vdbCache->activeVoxelCount() == 0)
			{

				//FIXME: Handle no-range case
				if(!rsdIncoming)
					break;


				// clear the calculation results andd provide an accessor
				calculation_result_grid->clear();
				openvdb::FloatGrid::Accessor calculation_result_accessor = calculation_result_grid->getAccessor();

				// initialize nominator and denominator grids
				openvdb::FloatGrid::Ptr denominator_grid = openvdb::FloatGrid::create(background);
				openvdb::FloatGrid::Accessor denominator_accessor = denominator_grid->getAccessor();

				openvdb::FloatGrid::Ptr numerator_grid = openvdb::FloatGrid::create(background);
				openvdb::FloatGrid::Accessor numerator_accessor = numerator_grid->getAccessor();
	
				for(size_t ui=0;ui<dataIn.size();ui++)
				{
					//Check for ion stream types. Don't use anything else in counting
					if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
						continue;

					const IonStreamData  *ions; 
					ions = (const IonStreamData *)dataIn[ui];
		
					//get the denominator ions
					unsigned int ionID;
					ionID = rsdIncoming->rangeFile->getIonID(ions->data[0].getMassToCharge());

					bool thisDenominatorIonEnabled;
					if(ionID!=(unsigned int)-1)
						thisDenominatorIonEnabled=enabledIons[1][ionID];
					else
						thisDenominatorIonEnabled=false;

					// get the numerator ions
					ionID = getIonstreamIonID(ions,rsdIncoming->rangeFile);

					bool thisNumeratorIonEnabled;
					if(ionID!=(unsigned int)-1)
						thisNumeratorIonEnabled=enabledIons[0][ionID];
					else
						thisNumeratorIonEnabled=false;

					for(size_t uj=0;uj<ions->data.size(); uj++)
					{
						const int xyzs = 3;
						std::vector<float> atom_position(xyzs); 
						for (int i=0;i<xyzs;i++)
						{
							atom_position[i] = ions->data[uj].getPos()[i];
						}
	
						// 1st step - project the current atom position to unit voxel i.e. from 0 to 1
						std::vector<float> position_in_unit_voxel;
						position_in_unit_voxel = CTF::projectAtompositionToUnitvoxel(atom_position, voxelsize);

						// 2nd step - determine each contribution to the adjecent 8 voxels outgoining from the position in the unit voxel
						std::vector<float> volumes_of_subcuboids;
						std::vector<float> contributions_to_adjacent_voxels;
						bool vertex_corner_coincidence = false;
			
						vertex_corner_coincidence = CTF::checkVertexCornerCoincidence(position_in_unit_voxel);
			
						// in case of coincidence of atom and voxel the contribution becomes 100 percent
						if (vertex_corner_coincidence == false)
						{
							volumes_of_subcuboids = CTF::calcSubvolumes(position_in_unit_voxel);
							contributions_to_adjacent_voxels = CTF::HellmanContributions(volumes_of_subcuboids);
						}
						else
						{
							contributions_to_adjacent_voxels = CTF::handleVertexCornerCoincidence(position_in_unit_voxel);
						}
			
						// 3rd step - determine the adjacent voxel indices in the actual grid
						std::vector<std::vector<float> > adjacent_voxel_vertices;
						adjacent_voxel_vertices = CTF::determineAdjacentVoxelVertices(atom_position, voxelsize);
			
						// 4th step - assign each of the 8 adjacent voxels the corresponding contribution that results from the atom position in the unit voxel
						const int number_of_adjacent_voxels = 8;
						std::vector<float> current_voxel_index;
						for (int i=0;i<number_of_adjacent_voxels;i++)
						{
							current_voxel_index = adjacent_voxel_vertices[i];
							// normalized voxel indices based on 00, 01, 02 etc. // very important otherwise there will be spacings
							openvdb::Coord ijk(current_voxel_index[0], current_voxel_index[1], current_voxel_index[2]);


							/// normalization methods
							/// 1 raw count 2 volume (density) 3 all ions (conc) 4 ratio (num/denum)

							// raw count
							if ((normaliseType == VOXELISE_NORMALISETYPE_NONE) || (normaliseType == VOXELISE_NORMALISETYPE_VOLUME))
							{
									calculation_result_accessor.setValue(ijk, contributions_to_adjacent_voxels[i] + numerator_accessor.getValue(ijk));
							}

							// concentration
							else if (normaliseType == VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL)
							{
								// write to all ions to the denominator grid

								denominator_accessor.setValue(ijk, contributions_to_adjacent_voxels[i] + denominator_accessor.getValue(ijk));

								// write selected numerators to numerator grid
								if(thisNumeratorIonEnabled)
								{	
									numerator_accessor.setValue(ijk, contributions_to_adjacent_voxels[i] + numerator_accessor.getValue(ijk));
								}
								else
								{
									numerator_accessor.setValue(ijk, 0.0 + numerator_accessor.getValue(ijk));
								}
							}

							else if (normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL)
							{
								// write to denominator grid
								if(thisDenominatorIonEnabled)
								{
					
									denominator_accessor.setValue(ijk, contributions_to_adjacent_voxels[i] + denominator_accessor.getValue(ijk));
								}
								else
								{
									denominator_accessor.setValue(ijk, 0.0 + denominator_accessor.getValue(ijk));
								}

								// write to numerator grid
								if(thisNumeratorIonEnabled)
								{	
									numerator_accessor.setValue(ijk, contributions_to_adjacent_voxels[i] + numerator_accessor.getValue(ijk));
								}
								else
								{
									numerator_accessor.setValue(ijk, 0.0 + numerator_accessor.getValue(ijk));
								}
							}				

							else 
							{}

						}
					}
				}

				float minVal = 0.0;
				float maxVal = 0.0;
				denominator_grid->evalMinMax(minVal,maxVal);
				numerator_grid->evalMinMax(minVal,maxVal);
				
				if ((normaliseType == VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL) || (normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL))
				{
					// composite.h operations modify the first grid and leave the second grid emtpy !
					// compute a = a / b
					openvdb::tools::compDiv(*numerator_grid, *denominator_grid);
	
					calculation_result_grid = numerator_grid->deepCopy();

					//check for negative nans and infs introduced by the division
					//set them to zero in order not to obtain nan mesh coordinates

					for (openvdb::FloatGrid::ValueAllIter iter = calculation_result_grid->beginValueAll(); iter; ++iter)
					{   
						if (std::isfinite(iter.getValue()) == false)
						{
				    			iter.setValue(0.0);
						}
					}
				}
				
				else if (normaliseType == VOXELISE_NORMALISETYPE_VOLUME)
				{

					for (openvdb::FloatGrid::ValueAllIter iter = calculation_result_grid->beginValueAll(); iter; ++iter)
					{   
				    		iter.setValue(iter.getValue() / single_voxel_volume);
					}

					//normalize these values again in order to obtain values from zero to one
					// so the isovalue still matches
					calculation_result_grid->evalMinMax(minVal,maxVal);

					for (openvdb::FloatGrid::ValueAllIter iter = calculation_result_grid->beginValueAll(); iter; ++iter)
					{   
				    		iter.setValue((iter.getValue() - minVal) / (maxVal - minVal));
					}
	
				}
			
				calculation_result_grid->evalMinMax(minVal,maxVal);

				// Associate a scaling transform with the grid that sets the voxel size
				// to voxelsize units in world space.
				openvdb::math::Transform::Ptr linearTransform = openvdb::math::Transform::createLinearTransform(voxelsize);
				calculation_result_grid->setTransform(linearTransform);

				vdbCache = calculation_result_grid->deepCopy();
				vdbCache->setTransform(linearTransform);
			}

			else
			{
				//Use the cached value
				calculation_result_grid = vdbCache->deepCopy();
			}
			
			// manage the filter output
			std::cerr << "Completing evaluation of VDB grid..." << endl;

			OpenVDBGridStreamData *gs = new OpenVDBGridStreamData();
			gs->parent=this;
			// just like the swap function of the voxelization does pass the grids here to gs->grids
			gs->grid = calculation_result_grid->deepCopy();
			gs->voxelsize = voxelsize;
			gs->representationType = representation;
			gs->isovalue=isoLevel;
			gs->r=rgba.r();
			gs->g=rgba.g();
			gs->b=rgba.b();
			gs->a=rgba.a();
	
			if(cache)
			{
				gs->cached=1;
				cacheOK=true;
				filterOutputs.push_back(gs);
			}
			else
			{
				gs->cached=0;
			}
			//Store the vdbgrid on the output
			getOut.push_back(gs);
			break;

		}

		case VOXEL_REPRESENT_POINTCLOUD:
		case VOXEL_REPRESENT_AXIAL_SLICE:
		{

			//use the cached copy if we have it.
			if(cacheOK)
			{
				propagateCache(getOut);
				return 0;
			}

			Voxels<float> voxelData;
			if(!voxelCache.getSize())
			{
				Point3D minP,maxP;

				bc.setInverseLimits();
			
				for (size_t i = 0; i < dataIn.size(); i++) 
				{
					//Check for ion stream types. Block others from propagation.
					if (dataIn[i]->getStreamType() != STREAM_TYPE_IONS) continue;

					const IonStreamData *is = (const IonStreamData *)dataIn[i];
					//Don't work on empty or single object streams (bounding box needs to be defined)
					if (is->getNumBasicObjects() < 2) continue;
		
					BoundCube bcTmp;
					IonHit::getBoundCube(is->data,bcTmp);

					//Bounds could be invalid if, for example, we had coplanar axis aligned points
					if (!bcTmp.isValid()) continue;

					bc.expand(bcTmp);
				}
				//No bounding box? Tough cookies
				if (!bc.isValid() || bc.isFlat()) return VOXELISE_BOUNDS_INVALID_ERR;

				bc.getBounds(minP,maxP);	
				if (fixedWidth) 
					calculateNumBinsFromWidths(binWidth, nBins);
				else
					calculateWidthsFromNumBins(binWidth, nBins);
		
				//Disallow empty bounding boxes (ie, produce no output)
				if(minP == maxP)
					return 0;
	
				//Rebuild the voxels from the point data
				Voxels<float> vsDenom;
				voxelData.init(nBins[0], nBins[1], nBins[2], bc);
				voxelData.fill(0);
		
				if (normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL ||
					normaliseType == VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL) {
					//Check we actually have incoming data
					ASSERT(rsdIncoming);
					vsDenom.init(nBins[0], nBins[1], nBins[2], bc);
					vsDenom.fill(0);
				}

				const IonStreamData *is;
				if(rsdIncoming)
				{

					for (size_t i = 0; i < dataIn.size(); i++) 
					{
				
						//Check for ion stream types. Don't use anything else in counting
						if (dataIn[i]->getStreamType() != STREAM_TYPE_IONS) continue;
				
						is= (const IonStreamData *)dataIn[i];

				
						//Count the numerator ions	
						if(is->data.size())
						{
							//Check what Ion type this stream belongs to. Assume all ions
							//in the stream belong to the same group
							unsigned int ionID;
							ionID = getIonstreamIonID(is,rsdIncoming->rangeFile);

							bool thisIonEnabled;
							if(ionID!=(unsigned int)-1)
								thisIonEnabled=enabledIons[0][ionID];
							else
								thisIonEnabled=false;

							if(thisIonEnabled)
							{
								countPoints(voxelData,is->data,true);
							}
						}
			
						//If the user requests normalisation, compute the denominator dataset
						if (normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL) {
							if(is->data.size())
							{
								//Check what Ion type this stream belongs to. Assume all ions
								//in the stream belong to the same group
								unsigned int ionID;
								ionID = rsdIncoming->rangeFile->getIonID(is->data[0].getMassToCharge());

								bool thisIonEnabled;
								if(ionID!=(unsigned int)-1)
									thisIonEnabled=enabledIons[1][ionID];
								else
									thisIonEnabled=false;

								if(thisIonEnabled)
									countPoints(vsDenom,is->data,true);
							}
						} else if (normaliseType == VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL)
						{
							countPoints(vsDenom,is->data,true);
						}

						if(*Filter::wantAbort)
							return VOXELISE_ABORT_ERR;
					}
		
					//Perform normalsiation	
					if (normaliseType == VOXELISE_NORMALISETYPE_VOLUME)
						voxelData.calculateDensity();
					else if (normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL ||
							 normaliseType == VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL)
						voxelData /= vsDenom;
				}
				else
				{
					//No range data.  Just count
					for (size_t i = 0; i < dataIn.size(); i++) 
					{
				
						if(dataIn[i]->getStreamType() == STREAM_TYPE_IONS)
						{
							is= (const IonStreamData *)dataIn[i];

							countPoints(voxelData,is->data,true);
					
							if(*Filter::wantAbort)
								return VOXELISE_ABORT_ERR;

						}
					}
					ASSERT(normaliseType != VOXELISE_NORMALISETYPE_COUNT2INVOXEL
							&& normaliseType!=VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL);
					if (normaliseType == VOXELISE_NORMALISETYPE_VOLUME)
						voxelData.calculateDensity();
				}	

				vsDenom.clear();


				//Perform voxel filtering
				switch(filterMode)
				{
					case VOXELISE_FILTERTYPE_NONE:
						break;
					case VOXELISE_FILTERTYPE_GAUSS:
					{	
						voxelData.isotropicGaussianSmooth(gaussDev,filterRatio);
						break;
					}
					case VOXELISE_FILTERTYPE_LAPLACE:
					{
						voxelData.laplaceOfGaussian(gaussDev,filterRatio);
						break;
					}
					default:
						ASSERT(false);
				}
	
				voxelCache=voxelData;
			}
			else
			{
				//Use the cached value
				voxelData=voxelCache;
			}
	
			float min,max;
			voxelData.minMax(min,max);


			string sMin,sMax;
			stream_cast(sMin,min);
			stream_cast(sMax,max);
			consoleOutput.push_back(std::string(TRANS("Voxel Limits (min,max): (") + sMin + string(","))
				       	+  sMax + ")");


			//Update the bounding cube
			{
			Point3D p1,p2;
			voxelData.getBounds(p1,p2);
			lastBounds.setBounds(p1,p2);
			}


		switch(representation)
		{

			case VOXEL_REPRESENT_POINTCLOUD:
			{
				VoxelStreamData *vs = new VoxelStreamData();
				vs->parent=this;
				std::swap(*(vs->data),voxelData);
				vs->representationType= representation;
				vs->splatSize = splatSize;
				vs->isoLevel=isoLevel;
				vs->r=rgba.r();
				vs->g=rgba.g();
				vs->b=rgba.b();
				vs->a=rgba.a();

				if(cache)
				{
					vs->cached=1;
					cacheOK=true;
					filterOutputs.push_back(vs);
				}
				else
					vs->cached=0;
				
				//Store the voxels on the output
				getOut.push_back(vs);
				break;
			}
			case VOXEL_REPRESENT_AXIAL_SLICE:
			{
				DrawStreamData *d = new DrawStreamData;

				//Create the voxel slice
				float minV,maxV;
				{
				DrawTexturedQuad *dq = new DrawTexturedQuad();

				getTexturedSlice(voxelData,sliceAxis,sliceOffset,
							sliceInterpolate,minV,maxV,*dq);

				dq->setColour(1,1,1,rgba.a());
				dq->canSelect=true;


				SelectionDevice *s = new SelectionDevice(this);
				SelectionBinding b;
				//Bind translation to sphere left click
				b.setBinding(SELECT_BUTTON_LEFT,0,DRAW_QUAD_BIND_ORIGIN,
						BINDING_PLANE_ORIGIN,dq->getOrigin(),dq);
				b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
				s->addBinding(b);

				devices.push_back(s);

				d->drawables.push_back(dq);
				}
				
				
				if(showColourBar)
					d->drawables.push_back(makeColourBar(minV,maxV,255,colourMap));
				d->cached=0;
				d->parent=this;
				
				getOut.push_back(d);
			
			
				cacheOK=false;
			}
		}

	}
}

	
	//Copy the inputs into the outputs, provided they are not voxels
	return 0;
}



void VoxeliseFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
		case BINDING_PLANE_ORIGIN:
		{
			ASSERT(representation == VOXEL_REPRESENT_AXIAL_SLICE);
			ASSERT(lastBounds.isValid());
		
			//Convert the world coordinate value into a
			// fractional value of voxel bounds
			Point3D p;
			float f;
			b.getValue(p);
			f=p[sliceAxis];

			float minB,maxB;
			minB = lastBounds.getBound(sliceAxis,0);
			maxB = lastBounds.getBound(sliceAxis,1);
			sliceOffset= (f -minB)/(maxB-minB);
			
			sliceOffset=std::min(sliceOffset,1.0f);
			sliceOffset=std::max(sliceOffset,0.0f);
			ASSERT(sliceOffset<=1 && sliceOffset>=0);
			break;
		}
		default:
			ASSERT(false);
	}

}

std::string VoxeliseFilter::getNormaliseTypeString(int type){
	ASSERT(type < VOXELISE_NORMALISETYPE_MAX);
	return TRANS(NORMALISE_TYPE_STRING[type]);
}

std::string VoxeliseFilter::getRepresentTypeString(int type) {
	ASSERT(type<VOXEL_REPRESENT_END);
	return  std::string(TRANS(REPRESENTATION_TYPE_STRING[type]));
}

std::string VoxeliseFilter::getFilterTypeString(int type)
{
	ASSERT(type < VOXELISE_FILTERTYPE_MAX);
	return std::string(TRANS(VOXELISE_FILTER_TYPE_STRING[type]));
}


void VoxeliseFilter::getProperties(FilterPropGroup &propertyList) const
{
	FilterProperty p;
	size_t curGroup=0;

	string tmpStr;
	stream_cast(tmpStr, fixedWidth);
	p.name=TRANS("Fixed width");
	p.data=tmpStr;
	p.key=KEY_FIXEDWIDTH;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("If true, use fixed size voxels, otherwise use fixed count");
	propertyList.addProperty(p,curGroup);

	if(fixedWidth)
	{
		stream_cast(tmpStr,binWidth[0]);
		p.name=TRANS("Bin width x");
		p.data=tmpStr;
		p.key=KEY_WIDTHBINSX;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Voxel size in X direction");
		propertyList.addProperty(p,curGroup);

		stream_cast(tmpStr,binWidth[1]);
		p.name=TRANS("Bin width y");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Voxel size in Y direction");
		p.key=KEY_WIDTHBINSY;
		propertyList.addProperty(p,curGroup);


		stream_cast(tmpStr,binWidth[2]);
		p.name=TRANS("Bin width z");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Voxel size in Z direction");
		p.key=KEY_WIDTHBINSZ;
		propertyList.addProperty(p,curGroup);
	}
	else
	{
		stream_cast(tmpStr,nBins[0]);
		p.name=TRANS("Num bins x");
		p.data=tmpStr;
		p.key=KEY_NBINSX;
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of voxels to use in X direction");
		propertyList.addProperty(p,curGroup);
		
		stream_cast(tmpStr,nBins[1]);
		p.key=KEY_NBINSY;
		p.name=TRANS("Num bins y");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of voxels to use in Y direction");
		propertyList.addProperty(p,curGroup);
		
		stream_cast(tmpStr,nBins[2]);
		p.key=KEY_NBINSZ;
		p.data=tmpStr;
		p.name=TRANS("Num bins z");
		p.type=PROPERTY_TYPE_INTEGER;
		p.helpText=TRANS("Number of voxels to use in Z direction");
		propertyList.addProperty(p,curGroup);
	}

	//Let the user know what the valid values for voxel value types are
	vector<pair<unsigned int,string> > choices;
	unsigned int defaultChoice=normaliseType;
	tmpStr=getNormaliseTypeString(VOXELISE_NORMALISETYPE_NONE);
	choices.push_back(make_pair((unsigned int)VOXELISE_NORMALISETYPE_NONE,tmpStr));
	tmpStr=getNormaliseTypeString(VOXELISE_NORMALISETYPE_VOLUME);
	choices.push_back(make_pair((unsigned int)VOXELISE_NORMALISETYPE_VOLUME,tmpStr));
	if(rsdIncoming)
	{
		//Concentration mode
		tmpStr=getNormaliseTypeString(VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL);
		choices.push_back(make_pair((unsigned int)VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL,tmpStr));
		//Ratio is only valid if we have a way of separation for the ions i.e. range
		tmpStr=getNormaliseTypeString(VOXELISE_NORMALISETYPE_COUNT2INVOXEL);
		choices.push_back(make_pair((unsigned int)VOXELISE_NORMALISETYPE_COUNT2INVOXEL,tmpStr));
	}
	else
	{
		//prevent the case where we used to have an incoming range stream, but now we don't.
		// selected item within choice string must still be valid
		if(normaliseType > VOXELISE_NORMALISETYPE_VOLUME)
			defaultChoice= VOXELISE_NORMALISETYPE_NONE;
		
	}

	tmpStr= choiceString(choices,defaultChoice);
	p.name=TRANS("Normalise by");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Method to use to normalise scalar value in each voxel");
	p.key=KEY_NORMALISE_TYPE;
	propertyList.addProperty(p,curGroup);
	propertyList.setGroupTitle(curGroup,TRANS("Computation"));

	curGroup++;
	
	// numerator
	if (rsdIncoming) 
	{
		
		p.name=TRANS("Numerator");
		p.data=boolStrEnc(numeratorAll);
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Parmeter \"a\" used in fraction (a/b) to get voxel value");
		p.key=KEY_ENABLE_NUMERATOR;
		propertyList.addProperty(p,curGroup);

		ASSERT(rsdIncoming->enabledIons.size()==enabledIons[0].size());	
		ASSERT(rsdIncoming->enabledIons.size()==enabledIons[1].size());	

		//Look at the numerator	
		for(unsigned  int ui=0; ui<rsdIncoming->enabledIons.size(); ui++)
		{
			string str;
			str=boolStrEnc(enabledIons[0][ui]);

			//Append the ion name with a checkbox
			p.name=rsdIncoming->rangeFile->getName(ui);
			p.data=str;
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Enable this ion for numerator");
			p.key=muxKey(KEY_ENABLE_NUMERATOR,ui);
			propertyList.addProperty(p,curGroup);
		}

		propertyList.setGroupTitle(curGroup,TRANS("Numerator"));
		curGroup++;
	}
	
	
	if (normaliseType == VOXELISE_NORMALISETYPE_COUNT2INVOXEL && rsdIncoming) 
	{
		p.name=TRANS("Denominator");
		p.data=boolStrEnc(denominatorAll );
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Parameter \"b\" used in fraction (a/b) to get voxel value");
		p.key=KEY_ENABLE_DENOMINATOR;
		propertyList.addProperty(p,curGroup);

		for(unsigned  int ui=0; ui<rsdIncoming->enabledIons.size(); ui++)
		{			
			string str;
			str=boolStrEnc(enabledIons[1][ui]);

			//Append the ion name with a checkbox
			p.key=muxKey(KEY_ENABLE_DENOMINATOR,ui);
			p.data=str;
			p.name=rsdIncoming->rangeFile->getName(ui);
			p.type=PROPERTY_TYPE_BOOL;
			p.helpText=TRANS("Enable this ion for denominator contribution");

			propertyList.addProperty(p,curGroup);
		}
		propertyList.setGroupTitle(curGroup,TRANS("Denominator"));
		curGroup++;
	}

	//Start a new set for filtering
	//----
	//TODO: Other filtering? threshold/median? laplacian? etc
	
	choices.clear();
	//Post-filtering method
	for(unsigned int ui=0;ui<VOXELISE_FILTERTYPE_MAX; ui++)
	{
		tmpStr=getFilterTypeString(ui);
		choices.push_back(make_pair(ui,tmpStr));
	}
	tmpStr= choiceString(choices,filterMode);
	choices.clear();

	p.name=TRANS("Filtering");
	p.data=tmpStr;
	p.key=KEY_FILTER_MODE;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Smoothing method to use on voxels");

	propertyList.addProperty(p,curGroup);
	propertyList.setGroupTitle(curGroup,TRANS("Processing"));
	if(filterMode != VOXELISE_FILTERTYPE_NONE)
	{

		//Filter size
		stream_cast(tmpStr,gaussDev);
		p.name=TRANS("Standard Dev");
		p.data=tmpStr;
		p.key=KEY_FILTER_STDEV;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Filtering Scale");
		propertyList.addProperty(p,curGroup);

		
		//Filter size
		stream_cast(tmpStr,filterRatio);
		p.name=TRANS("Kernel Size");
		p.data=tmpStr;
		p.key=KEY_FILTER_RATIO;
		p.type=PROPERTY_TYPE_REAL;
		p.helpText=TRANS("Filter radius, in multiples of std. dev. Larger -> slower, more accurate");
		propertyList.addProperty(p,curGroup);

	}
	propertyList.setGroupTitle(curGroup,TRANS("Filtering"));
	curGroup++;
	//----

	//start a new group for the visual representation
	//----------------------------
	choices.clear();
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_POINTCLOUD);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_POINTCLOUD,tmpStr));
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_ISOSURF);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_ISOSURF,tmpStr));
	tmpStr=getRepresentTypeString(VOXEL_REPRESENT_AXIAL_SLICE);
	choices.push_back(make_pair((unsigned int)VOXEL_REPRESENT_AXIAL_SLICE,tmpStr));
	
	tmpStr= choiceString(choices,representation);

	p.name=TRANS("Representation");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("3D display method");
	p.key=KEY_VOXEL_REPRESENTATION_MODE;
	propertyList.addProperty(p,curGroup);

	switch(representation)
	{
		case VOXEL_REPRESENT_POINTCLOUD:
		{
			propertyList.setGroupTitle(curGroup,TRANS("Appearance"));

			stream_cast(tmpStr,splatSize);
			p.name=TRANS("Spot size");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Size of the spots to use for display");
			p.key=KEY_SPOTSIZE;
			propertyList.addProperty(p,curGroup);

			stream_cast(tmpStr,1.0-rgba.a());
			p.name=TRANS("Transparency");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("How \"see through\" each point is (0 - opaque, 1 - invisible)");
			p.key=KEY_TRANSPARENCY;
			propertyList.addProperty(p,curGroup);
			
			break;
		}
		case VOXEL_REPRESENT_ISOSURF:
		{
			if(!rsdIncoming)
				break;

			// group computation
			stream_cast(tmpStr,voxelsize);
			p.name=TRANS("Voxelsize");
			p.data=tmpStr;
			p.key=KEY_VOXELSIZE;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Voxel size in x,y,z direction");
			propertyList.addProperty(p,curGroup);

			propertyList.setGroupTitle(curGroup,TRANS("Computation"));
			curGroup++;
	
	
			//-- Isosurface parameters --

			stream_cast(tmpStr,isoLevel);
			p.name=TRANS("Isovalue [0,1]");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Scalar value to show as isosurface");
			p.key=KEY_ISOLEVEL;
			propertyList.addProperty(p,curGroup);

			//-- 
			propertyList.setGroupTitle(curGroup,TRANS("Isosurface"));	
			curGroup++;

			//-- Isosurface appearance --
			p.name=TRANS("Colour");
			p.data=rgba.toColourRGBA().rgbString();
			p.type=PROPERTY_TYPE_COLOUR;
			p.helpText=TRANS("Colour of isosurface");
			p.key=KEY_COLOUR;
			propertyList.addProperty(p,curGroup);
			
			break;
		}
		case VOXEL_REPRESENT_AXIAL_SLICE:
		{
			//-- Slice parameters --
			propertyList.setGroupTitle(curGroup,TRANS("Slice param."));

			vector<pair<unsigned int, string> > choices;
			
			choices.push_back(make_pair(0,"x"));	
			choices.push_back(make_pair(1,"y"));	
			choices.push_back(make_pair(2,"z"));	
			p.name=TRANS("Slice Axis");
			p.data=choiceString(choices,sliceAxis);
			p.type=PROPERTY_TYPE_CHOICE;
			p.helpText=TRANS("Normal for the planar slice");
			p.key=KEY_VOXEL_SLICE_AXIS;
			propertyList.addProperty(p,curGroup);
			choices.clear();
			
			stream_cast(tmpStr,sliceOffset);
			p.name=TRANS("Slice Coord");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Fractional coordinate that slice plane passes through");
			p.key=KEY_VOXEL_SLICE_OFFSET;
			propertyList.addProperty(p,curGroup);
		
			
			p.name=TRANS("Interp. Mode");
			for(unsigned int ui=0;ui<VOX_INTERP_ENUM_END;ui++)
			{
				choices.push_back(make_pair(ui,
					TRANS(VOXELISE_SLICE_INTERP_STRING[ui])));
			}
			p.data=choiceString(choices,sliceInterpolate);
			p.type=PROPERTY_TYPE_CHOICE;
			p.helpText=TRANS("Interpolation mode for direction normal to slice");
			p.key=KEY_VOXEL_SLICE_INTERP;
			propertyList.addProperty(p,curGroup);
			choices.clear();
			// ---	
			propertyList.setGroupTitle(curGroup,TRANS("Surface"));	
			curGroup++;

			

			//-- Slice visualisation parameters --
			for(unsigned int ui=0;ui<NUM_COLOURMAPS; ui++)
				choices.push_back(make_pair(ui,getColourMapName(ui)));

			tmpStr=choiceString(choices,colourMap);

			p.name=TRANS("Colour mode");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_CHOICE;
			p.helpText=TRANS("Colour scheme used to assign points colours by value");
			p.key=KEY_VOXEL_COLOURMODE;
			propertyList.addProperty(p,curGroup);
			
			stream_cast(tmpStr,1.0-rgba.a());
			p.name=TRANS("Transparency");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("How \"see through\" each facet is (0 - opaque, 1 - invisible)");
			p.key=KEY_TRANSPARENCY;
			propertyList.addProperty(p,curGroup);

			tmpStr=boolStrEnc(showColourBar);
			p.name=TRANS("Show Bar");
			p.key=KEY_SHOW_COLOURBAR;
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_BOOL;
			propertyList.addProperty(p,curGroup);

			tmpStr=boolStrEnc(autoColourMap);
			p.name=TRANS("Auto Bounds");
			p.helpText=TRANS("Auto-compute min/max values in map"); 
			p.data= tmpStr;
			p.key=KEY_VOXEL_SLICE_COLOURAUTO;;
			p.type=PROPERTY_TYPE_BOOL;
			propertyList.addProperty(p,curGroup);

			if(!autoColourMap)
			{

				stream_cast(tmpStr,colourMapBounds[0]);
				p.name=TRANS("Map start");
				p.helpText=TRANS("Assign points with this value to the first colour in map"); 
				p.data= tmpStr;
				p.key=KEY_MAPSTART;;
				p.type=PROPERTY_TYPE_REAL;
				propertyList.addProperty(p,curGroup);

				stream_cast(tmpStr,colourMapBounds[1]);
				p.name=TRANS("Map end");
				p.helpText=TRANS("Assign points with this value to the last colour in map"); 
				p.data= tmpStr;
				p.key=KEY_MAPEND;
				p.type=PROPERTY_TYPE_REAL;
				propertyList.addProperty(p,curGroup);
			}
			// ---	

			break;
		}
		default:
			ASSERT(false);
			;
	}
	
	propertyList.setGroupTitle(curGroup,TRANS("Appearance"));	
	curGroup++;

	//----------------------------
}

bool VoxeliseFilter::setProperty(unsigned int key,
		  const std::string &value, bool &needUpdate)
{
	
	needUpdate=false;
	switch(key)
	{


		case KEY_VOXELSIZE: 
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			voxelsize=f;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation

			for(unsigned int ui=0;ui<filterOutputs.size();ui++)
			{	
				OpenVDBGridStreamData *vdbgs;
				vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
				vdbgs->voxelsize = voxelsize;
			}			break;
		}

		case KEY_FIXEDWIDTH: 
		{
			if(!applyPropertyNow(fixedWidth,value,needUpdate))
				return false;
			break;
		}	
		case KEY_NBINSX:
		case KEY_NBINSY:
		case KEY_NBINSZ:
		{
			if(!applyPropertyNow(nBins[key-KEY_NBINSX],value,needUpdate))
				return false;
			calculateWidthsFromNumBins(binWidth, nBins);
			break;
		}
		case KEY_WIDTHBINSX:
		case KEY_WIDTHBINSY:
		case KEY_WIDTHBINSZ:
		{
			if(!applyPropertyNow(binWidth[key-KEY_WIDTHBINSX],value,needUpdate))
				return false;
			calculateNumBinsFromWidths(binWidth, nBins);
			break;
		}
		case KEY_NORMALISE_TYPE:
		{
			unsigned int i;
			for(i = 0; i < VOXELISE_NORMALISETYPE_MAX; i++)
				if (value == getNormaliseTypeString(i)) break;
			if (i == VOXELISE_NORMALISETYPE_MAX)
				return false;
			if(normaliseType!=i)
			{
				needUpdate=true;
				clearCache();
				vdbCache->clear();
				normaliseType=i;
			}
			break;
		}
		case KEY_SPOTSIZE:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			if(f !=splatSize)
			{
				splatSize=f;
				needUpdate=true;

				//Go in and manually adjust the cached
				//entries to have the new value, rather
				//than doing a full recomputation
				if(cacheOK)
				{
					for(unsigned int ui=0;ui<filterOutputs.size();ui++)
					{
						VoxelStreamData *d;
						d=(VoxelStreamData*)filterOutputs[ui];
						d->splatSize=splatSize;
					}
				}

			}
			break;
		}
		case KEY_TRANSPARENCY:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f < 0.0f || f > 1.0)
				return false;
			needUpdate=true;
			//Alpha is opacity, which is 1-transparancy
			rgba.a(1.0f-f);
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{	
					OpenVDBGridStreamData *vdbgs;
					vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
					vdbgs->a = rgba.a();
				}
			}
			break;
		}
		case KEY_ISOLEVEL:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			isoLevel=f;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{	
					OpenVDBGridStreamData *vdbgs;
					vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
					vdbgs->isovalue = isoLevel;
				}
			}
			break;
		}
		case KEY_COLOUR:
		{
			ColourRGBA tmpRGBA;

			if(!tmpRGBA.parse(value))
				return false;

			if(tmpRGBA.toRGBAf() != rgba)
			{
				rgba=tmpRGBA.toRGBAf();
				needUpdate=true;
			}

			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation
			if(cacheOK)
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{
					switch(representation)
					{
						case VOXEL_REPRESENT_AXIAL_SLICE:
						case VOXEL_REPRESENT_POINTCLOUD:
						{

							VoxelStreamData *d;
							d=(VoxelStreamData*)filterOutputs[ui];
							d->r=rgba.r();
							d->g=rgba.g();
							d->b=rgba.b();
						}
						case VOXEL_REPRESENT_ISOSURF:
						{
							OpenVDBGridStreamData *vdbgs;
							vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
							vdbgs->r=rgba.r();
							vdbgs->g=rgba.g();
							vdbgs->b=rgba.b();
						}
					}
				}
			}
			break;
		}
		case KEY_VOXEL_REPRESENTATION_MODE:
		{
			unsigned int i;
			for (i = 0; i < VOXEL_REPRESENT_END; i++)
				if (value == getRepresentTypeString(i)) break;
			if (i == VOXEL_REPRESENT_END)
				return false;
			needUpdate=true;
			//Go in and manually adjust the cached
			//entries to have the new value, rather
			//than doing a full recomputation

			//TODO: Can we instead of caching the Stream, simply cache the voxel data?
			representation=i;
			if(cacheOK && (VOXEL_REPRESENT_KEEPCACHE[i] && VOXEL_REPRESENT_KEEPCACHE[representation]))
			{
				for(unsigned int ui=0;ui<filterOutputs.size();ui++)
				{

					switch(representation)
					{
						case VOXEL_REPRESENT_AXIAL_SLICE:
						case VOXEL_REPRESENT_POINTCLOUD:
						{
							VoxelStreamData *d;
							d=(VoxelStreamData*)filterOutputs[ui];
							d->representationType=representation;

						}

						case VOXEL_REPRESENT_ISOSURF:
						{
							OpenVDBGridStreamData *vdbgs;
							vdbgs = (OpenVDBGridStreamData*)filterOutputs[ui];
							vdbgs->representationType=representation;
						}	
					}

				}
			}
			else
			{
				clearCache();
				vdbCache->clear();
			}
			
			break;
		}
		case KEY_ENABLE_NUMERATOR:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
			//Set them all to enabled or disabled as a group	
			for (size_t i = 0; i < enabledIons[0].size(); i++) 
				enabledIons[0][i] = b;
			numeratorAll = b;
			needUpdate=true;
			clearCache();
			vdbCache->clear();
			break;
		}
		case KEY_ENABLE_DENOMINATOR:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
	
			//Set them all to enabled or disabled as a group	
			for (size_t i = 0; i < enabledIons[1].size(); i++) 
				enabledIons[1][i] = b;
			
			denominatorAll = b;
			needUpdate=true;			
			clearCache();
			vdbCache->clear();
			break;
		}
		case KEY_FILTER_MODE:
		{
			//Locate the current string
			unsigned int i;
			for (i = 0; i < VOXELISE_FILTERTYPE_MAX; i++)
			{
				if (value == getFilterTypeString(i)) 
					break;
			}
			if (i == VOXELISE_FILTERTYPE_MAX)
				return false;
			if(i!=filterMode)
			{
				needUpdate=true;
				filterMode=i;
				clearCache();
			}
			break;
		}
		case KEY_FILTER_RATIO:
		{
			float i;
			if(stream_cast(i,value))
				return false;
			//forbid negative sizes
			if(i <= 0)
				return false;
			if(i != filterRatio)
			{
				needUpdate=true;
				filterRatio=i;
				clearCache();
			}
			break;
		}
		case KEY_FILTER_STDEV:
		{
			float i;
			if(stream_cast(i,value))
				return false;
			//forbid negative sizes
			if(i <= 0)
				return false;
			if(i != gaussDev)
			{
				needUpdate=true;
				gaussDev=i;
				clearCache();
			}
			break;
		}
		case KEY_VOXEL_SLICE_COLOURAUTO:
		{
			bool b;
			if(!boolStrDec(value,b))
				return false;

			//if the result is different, the
			//cache should be invalidated
			if(b!=autoColourMap)
			{
				needUpdate=true;
				autoColourMap=b;
				//Clear the generic filter cache, but
				// not the voxel cache
				Filter::clearCache();
			}
			break;
		}	
		case KEY_VOXEL_SLICE_AXIS:
		{
			unsigned int i;
			string axisLabels[3]={"x","y","z"};
			for (i = 0; i < 3; i++)
				if( value == axisLabels[i]) break;
				
			if( i >= 3)
				return false;

			if(i != sliceAxis)
			{
				needUpdate=true;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
				sliceAxis=i;
			}
			break;
		}
		case KEY_VOXEL_SLICE_INTERP:
		{
			unsigned int i;
			for (i = 0; i < VOX_INTERP_ENUM_END; i++)
				if( value == TRANS(VOXELISE_SLICE_INTERP_STRING[i])) break;
				
			if( i >= VOX_INTERP_ENUM_END)
				return false;

			if(i != sliceInterpolate)
			{
				needUpdate=true;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
				sliceInterpolate=i;
			}
			break;
		}
		case KEY_VOXEL_SLICE_OFFSET:
		{
			float f;
			if(stream_cast(f,value))
				return false;

			if(f < 0.0f || f > 1.0f)
				return false;


			if( f != sliceOffset)
			{
				needUpdate=true;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
				sliceOffset=f;
			}

			break;
		}
		case KEY_VOXEL_COLOURMODE:
		{
			unsigned int tmpMap;
			tmpMap=(unsigned int)-1;
			for(unsigned int ui=0;ui<NUM_COLOURMAPS;ui++)
			{
				if(value== getColourMapName(ui))
				{
					tmpMap=ui;
					break;
				}
			}

			if(tmpMap >=NUM_COLOURMAPS || tmpMap ==colourMap)
				return false;

			//clear the generic filter cache (i.e. cached outputs)
			//but not the voxel cache
			Filter::clearCache();
			
			needUpdate=true;
			colourMap=tmpMap;
			break;
		}
		case KEY_SHOW_COLOURBAR:
		{
			bool b;
			if(!boolStrDec(value,b))
				return false;

			//if the result is different, the
			//cache should be invalidated
			if(b!=showColourBar)
			{
				needUpdate=true;
				showColourBar=b;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
			}
			break;
		}	
		case KEY_MAPSTART:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f >= colourMapBounds[1])
				return false;
		
			if(f!=colourMapBounds[0])
			{
				needUpdate=true;
				colourMapBounds[0]=f;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
			}
			break;
		}
		case KEY_MAPEND:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= colourMapBounds[0])
				return false;
		
			if(f!=colourMapBounds[1])
			{
				needUpdate=true;
				colourMapBounds[1]=f;
				//clear the generic filter cache (i.e. cached outputs)
				//but not the voxel cache
				Filter::clearCache();
			}
			break;
		}
		default:
		{
			unsigned int subKeyType,offset;
			demuxKey(key,subKeyType,offset);
			
			//Check for jump to denominator or numerator section
			// TODO: This is a bit of a hack.
			if (subKeyType==KEY_ENABLE_DENOMINATOR) {
				bool b;
				if(!boolStrDec(value,b))
					return false;

				enabledIons[1][offset]=b;
				if (!b) {
					denominatorAll = false;
				}
				needUpdate=true;			
				clearCache();
				vdbCache->clear();
			} else if (subKeyType == KEY_ENABLE_NUMERATOR) {
				bool b;
				if(!boolStrDec(value,b))
					return false;
				
				enabledIons[0][offset]=b;
				if (!b) {
					numeratorAll = false;
				}
				needUpdate=true;			
				clearCache();
				vdbCache->clear();
			}
			else
			{
				ASSERT(false);
			}
			break;
		}
	}
	return true;
}

std::string  VoxeliseFilter::getSpecificErrString(unsigned int code) const
{
	const char *errStrs[]={
	 	"",
		"Voxelisation aborted",
		"Out of memory",
		"Unable to perform filter convolution",
		"Voxelisation bounds are invalid",
	};
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(errStrs) == VOXELISE_ERR_ENUM_END);	
	
	ASSERT(code < VOXELISE_ERR_ENUM_END);
	return errStrs[code];
}

bool VoxeliseFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\"" << escapeXML(userString) << "\"/>" << endl;
			f << tabs(depth+1) << "<voxelsize value=\""<<voxelsize<< "\"/>"  << endl;
			f << tabs(depth+1) << "<fixedwidth value=\""<<fixedWidth << "\"/>"  << endl;
			f << tabs(depth+1) << "<nbins values=\""<<nBins[0] << ","<<nBins[1]<<","<<nBins[2] << "\"/>"  << endl;
			f << tabs(depth+1) << "<binwidth values=\""<<binWidth[0] << ","<<binWidth[1]<<","<<binWidth[2] << "\"/>"  << endl;
			f << tabs(depth+1) << "<normalisetype value=\""<<normaliseType << "\"/>"  << endl;
			f << tabs(depth+1) << "<enabledions>" << endl;

			f << tabs(depth+2) << "<numerator>" << endl;
			for(unsigned int ui=0;ui<enabledIons[0].size(); ui++)
				f << tabs(depth+3) << "<enabled value=\"" << boolStrEnc(enabledIons[0][ui]) << "\"/>" << endl;
			f << tabs(depth+2) << "</numerator>" << endl;

			f << tabs(depth+2) << "<denominator>" << endl;
			for(unsigned int ui=0;ui<enabledIons[1].size(); ui++)
				f << tabs(depth+3) << "<enabled value=\"" << boolStrEnc(enabledIons[1][ui]) << "\"/>" << endl;
			f << tabs(depth+2) << "</denominator>" << endl;

			f << tabs(depth+1) << "</enabledions>" << endl;

			f << tabs(depth+1) << "<representation value=\""<<representation << "\"/>" << endl;
			f << tabs(depth+1) << "<isovalue value=\""<<isoLevel << "\"/>" << endl;
			f << tabs(depth+1) << "<colour r=\"" <<  rgba.r()<< "\" g=\"" << rgba.g() << "\" b=\"" <<rgba.b()
				<< "\" a=\"" << rgba.a() << "\"/>" <<endl;

			f << tabs(depth+1) << "<axialslice>" << endl;
			f << tabs(depth+2) << "<offset value=\""<<sliceOffset<< "\"/>" << endl;
			f << tabs(depth+2) << "<interpolate value=\""<<sliceInterpolate<< "\"/>" << endl;
			f << tabs(depth+2) << "<axis value=\""<<sliceAxis<< "\"/>" << endl;
			f << tabs(depth+2) << "<colourbar show=\""<<boolStrEnc(showColourBar)<< 
						"\" auto=\"" << boolStrEnc(autoColourMap)<< "\" min=\"" <<
					colourMapBounds[0] << "\" max=\"" <<  colourMapBounds[1] << "\"/>" << endl;
			f << tabs(depth+1) << "</axialslice>" << endl;



			f << tabs(depth) << "</" << trueName() <<">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}
	
	return true;
}

bool VoxeliseFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;
	xmlChar *xmlString;
	stack<xmlNodePtr> nodeStack;

	//--=
	float tmpFloat = 0;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"voxelsize","value"))
		return false;
	if(tmpFloat <= 0.0f)
		return false;
	voxelsize=tmpFloat;
	//--=

	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	//Retrieve fixedWidth mode
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"fixedwidth","value"))
		return false;
	if(!boolStrDec(tmpStr,fixedWidth))
		return false;
	
	//Retrieve nBins	
	if(XMLHelpFwdToElem(nodePtr,"nbins"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"values");
	if(!xmlString)
		return false;
	std::vector<string> v1;
	splitStrsRef((char *)xmlString,',',v1);
	for (size_t i = 0; i < INDEX_LENGTH && i < v1.size(); i++)
	{
		if(stream_cast(nBins[i],v1[i]))
			return false;
		
		if(nBins[i] <= 0)
			return false;
	}
	xmlFree(xmlString);
	
	//Retrieve bin width 
	if(XMLHelpFwdToElem(nodePtr,"binwidth"))
		return false;
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"values");
	if(!xmlString)
		return false;
	std::vector<string> v2;
	splitStrsRef((char *)xmlString,',',v2);
	for (size_t i = 0; i < INDEX_LENGTH && i < v2.size(); i++)
	{
		if(stream_cast(binWidth[i],v2[i]))
			return false;
		
		if(binWidth[i] <= 0)
			return false;
	}
	xmlFree(xmlString);
	
	//Retrieve normaliseType
	if(!XMLGetNextElemAttrib(nodePtr,normaliseType,"normalisetype","value"))
		return false;
	if(normaliseType >= VOXELISE_NORMALISETYPE_MAX)
		return false;

	//Look for the enabled ions bit
	//-------	
	//
	
	if(!XMLHelpFwdToElem(nodePtr,"enabledions"))
	{

		nodeStack.push(nodePtr);
		if(!nodePtr->xmlChildrenNode)
			return false;
		nodePtr=nodePtr->xmlChildrenNode;
		
		//enabled ions for numerator
		if(XMLHelpFwdToElem(nodePtr,"numerator"))
			return false;

		nodeStack.push(nodePtr);

		if(!nodePtr->xmlChildrenNode)
			return false;

		nodePtr=nodePtr->xmlChildrenNode;

		while(nodePtr)
		{
			char c;
			//Retrieve representation
			if(!XMLGetNextElemAttrib(nodePtr,c,"enabled","value"))
				break;

			if(c == '1')
				enabledIons[0].push_back(true);
			else
				enabledIons[0].push_back(false);


			nodePtr=nodePtr->next;
		}

		nodePtr=nodeStack.top();
		nodeStack.pop();

		//enabled ions for denominator
		if(XMLHelpFwdToElem(nodePtr,"denominator"))
			return false;


		if(!nodePtr->xmlChildrenNode)
			return false;

		nodeStack.push(nodePtr);
		nodePtr=nodePtr->xmlChildrenNode;

		while(nodePtr)
		{
			char c;
			//Retrieve representation
			if(!XMLGetNextElemAttrib(nodePtr,c,"enabled","value"))
				break;

			if(c == '1')
				enabledIons[1].push_back(true);
			else
				enabledIons[1].push_back(false);
				

			nodePtr=nodePtr->next;
		}


		nodeStack.pop();
		nodePtr=nodeStack.top();
		nodeStack.pop();

		//Check that the enabled ions size makes at least some sense...
		if(enabledIons[0].size() != enabledIons[1].size())
			return false;

	}

	//-------	
	//Retrieve representation
	if(!XMLGetNextElemAttrib(nodePtr,representation,"representation","value"))
		return false;
	if(representation >=VOXEL_REPRESENT_END)
		return false;

	//-------	
	//Retrieve representation
	if(!XMLGetNextElemAttrib(nodePtr,isoLevel,"isovalue","value"))
		return false;

	//Retrieve colour
	//====
	if(XMLHelpFwdToElem(nodePtr,"colour"))
		return false;
	ColourRGBAf tmpRgba;
	if(!parseXMLColour(nodePtr,tmpRgba))
		return false;
	rgba=tmpRgba;

	//====

	//try to retrieve slice, where possible
	if(!XMLHelpFwdToElem(nodePtr,"axialslice"))
	{
		xmlNodePtr sliceNodes;
		sliceNodes=nodePtr->xmlChildrenNode;

		if(!sliceNodes)
			return false;

		if(!XMLGetNextElemAttrib(sliceNodes,sliceOffset,"offset","value"))
			return false;

		sliceOffset=std::min(sliceOffset,1.0f);
		sliceOffset=std::max(sliceOffset,0.0f);
		
		
		if(!XMLGetNextElemAttrib(sliceNodes,sliceInterpolate,"interpolate","value"))
			return false;

		if(sliceInterpolate >=VOX_INTERP_ENUM_END)
			return false;

		if(!XMLGetNextElemAttrib(sliceNodes,sliceAxis,"axis","value"))
			return false;

		if(sliceAxis > 2)
			sliceAxis=2;
		
		if(!XMLGetNextElemAttrib(sliceNodes,showColourBar,"colourbar","show"))
			return false;

		if(!XMLGetAttrib(sliceNodes,autoColourMap,"auto"))
			return false;
		
		if(!XMLGetAttrib(sliceNodes,colourMapBounds[0],"min"))
			return false;

		if(!XMLGetAttrib(sliceNodes,colourMapBounds[1],"max"))
			return false;

		if(colourMapBounds[0] >= colourMapBounds[1])
			return false;
	}
	



	return true;
	
}

unsigned int VoxeliseFilter::getRefreshBlockMask() const
{
	//Ions, plots and voxels cannot pass through this filter
	return STREAM_TYPE_PLOT | STREAM_TYPE_VOXEL;
}

unsigned int VoxeliseFilter::getRefreshEmitMask() const
{

	switch(representation)
	{
		case VOXEL_REPRESENT_ISOSURF:
			{
				return STREAM_TYPE_OPENVDBGRID | STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
			}

		case VOXEL_REPRESENT_POINTCLOUD:
		case VOXEL_REPRESENT_AXIAL_SLICE:
			{
				return STREAM_TYPE_VOXEL | STREAM_TYPE_DRAW;
			}
	}
}

unsigned int VoxeliseFilter::getRefreshUseMask() const
{
	switch(representation)
	{
		case VOXEL_REPRESENT_ISOSURF:
			{
				return STREAM_TYPE_OPENVDBGRID| STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
			}

		case VOXEL_REPRESENT_POINTCLOUD:
		case VOXEL_REPRESENT_AXIAL_SLICE:
			{
				return STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
			}
	}
}

void VoxeliseFilter::getTexturedSlice(const Voxels<float> &v, 
			size_t axis,float offset, size_t interpolateMode,
			float &minV,float &maxV,DrawTexturedQuad &texQ) const
{
	ASSERT(axis < 3);


	size_t dim[3]; //dim0 and 2 are the in-plane axes. dim3 is the normal axis
	v.getSize(dim[0],dim[1],dim[2]);

	switch(axis)
	{
		//x-normal
		case 0:
			rotate3(dim[0],dim[1],dim[2]);
			std::swap(dim[0],dim[1]);
			break;
		//y-normal
		case 1:
			rotate3(dim[2],dim[1],dim[0]);
			break;
		//z-normal
		case 2:
			std::swap(dim[0],dim[1]);
			break;
			
	}


	ASSERT(dim[0] >0 && dim[1] >0);
	
	texQ.resize(dim[0],dim[1],3);

	//Generate the texture from the voxel data
	//---
	float *data = new float[dim[0]*dim[1]];
	

	ASSERT(offset >=0 && offset <=1.0f);

	v.getInterpSlice(axis,offset,data,interpolateMode);

	if(autoColourMap)
	{
		minV=minValue(data,dim[0]*dim[1]);
		maxV=maxValue(data,dim[0]*dim[1]);
	}
	else
	{
		minV=colourMapBounds[0];
		maxV=colourMapBounds[1];

	}
	ASSERT(minV <=maxV);

	unsigned char rgb[3];
	for(size_t ui=0;ui<dim[0];ui++)
	{
		for(size_t uj=0;uj<dim[1];uj++)
		{
			colourMapWrap(colourMap,rgb, data[ui*dim[1] + uj],
					minV,maxV,false);
			
			texQ.setData(ui,uj,rgb);	
		}
	}

	delete[] data;
	//---
	
	
	
	//Set the vertices of the quad
	//--
	//compute the real position of the plane
	float minPos,maxPos,offsetRealPos;
	v.getAxisBounds(axis,minPos,maxPos);
	offsetRealPos = ((float)offset)*(maxPos-minPos) + minPos; 
	
	
	Point3D verts[4];
	v.getBounds(verts[0],verts[2]);
	//set opposite vertices to upper and lower bounds of quad
	verts[0][axis]=verts[2][axis]=offsetRealPos;
	//set other vertices to match, then shift them in the axis plane
	verts[1]=verts[0]; 
	verts[3]=verts[2];


	unsigned int shiftAxis=(axis+1)%3;
	verts[1][shiftAxis] = verts[2][shiftAxis];
	verts[3][shiftAxis] = verts[0][shiftAxis];

	//Correction for y texture orientation
	if(axis==1)
		std::swap(verts[1],verts[3]);

	texQ.setVertices(verts);
	//--


}


#ifdef DEBUG
bool voxelSingleCountTest()
{
	//Test counting a single vector
	
	vector<IonHit> ionVec;

	ionVec.resize(5);
	ionVec[0].setPos(Point3D(0.1,0.1,0.1));
	ionVec[1].setPos(Point3D(0.1,0.0,0.1));
	ionVec[2].setPos(Point3D(0.0,0.1,0.1));
	ionVec[3].setPos(Point3D(0.1,0.1,0.0));
	ionVec[4].setPos(Point3D(0.0,0.1,0.0));

	for(unsigned int ui=0;ui<ionVec.size();ui++)
		ionVec[ui].setMassToCharge(1);

	IonStreamData *ionData = new IonStreamData;
	std::swap(ionData->data,ionVec);
	
	size_t numIons=ionData->data.size();
	
	VoxeliseFilter *f = new VoxeliseFilter;
	f->setCaching(false);

	bool needUpdate;
	TEST(f->setProperty(KEY_NBINSX,"4",needUpdate),"num bins x");
	TEST(f->setProperty(KEY_NBINSY,"4",needUpdate),"num bins y");
	TEST(f->setProperty(KEY_NBINSZ,"4",needUpdate),"num bins z");


	vector<const FilterStreamData*> streamIn,streamOut;
	streamIn.push_back(ionData);

	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Refresh error code");
	delete f;

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_VOXEL,"Stream type");


	const VoxelStreamData *v= (const VoxelStreamData*)streamOut[0];

	TEST(v->data->max() <=numIons,
			"voxel max less than input stream")

	TEST(v->data->min() >= 0.0f,"voxel counting minimum sanity");

	
	float dataSum;
	sumVoxels(*(v->data),dataSum);
	TEST(fabs(dataSum - (float)numIons ) < 
		sqrtf(std::numeric_limits<float>::epsilon()),"voxel counting all input ions ");

	delete ionData;
	delete streamOut[0];

	return true;
}

bool voxelMultiCountTest()
{
	//Test counting multiple data streams containing ranged data 
	
	vector<const FilterStreamData*> streamIn,streamOut;
	vector<IonHit> ionVec;

	ionVec.resize(5);
	ionVec[0].setPos(Point3D(0.1,0.1,0.1));
	ionVec[1].setPos(Point3D(0.1,0.0,0.1));
	ionVec[2].setPos(Point3D(0.0,0.1,0.1));
	ionVec[3].setPos(Point3D(0.1,0.1,0.0));
	ionVec[4].setPos(Point3D(0.0,0.1,0.0));

	IonStreamData *ionData[2];
	RangeStreamData *rngStream;
	rngStream = new RangeStreamData;
	rngStream->rangeFile= new RangeFile;

	RGBf col; col.red=col.green=col.blue=1.0f;

	//create several input ion streams, each
	//containing the above data, but with differeing
	//mass to charge values.
	// - we range this data though!
	const unsigned int MAX_NUM_RANGES=2;
	for(unsigned int ui=0;ui<MAX_NUM_RANGES;ui++)
	{
		size_t ionNum;

		//Add a new ion "a1, a2... etc"
		string sTmp,sTmp2;
		sTmp="a";
		stream_cast(sTmp2,ui);
		sTmp+=sTmp2;
		ionNum=rngStream->rangeFile->addIon(sTmp,sTmp,col);
		rngStream->rangeFile->addRange((float)ui-0.5f,(float)ui+0.5f,ionNum);

		//Change m/c value for ion
		for(unsigned int uj=0;uj<ionVec.size();uj++)
			ionVec[uj].setMassToCharge(ui);
		
		ionData[ui]= new IonStreamData;
		ionData[ui]->data.resize(ionVec.size());
		std::copy(ionVec.begin(),ionVec.end(),ionData[ui]->data.begin());
		streamIn.push_back(ionData[ui]);
	}

	rngStream->enabledIons.resize(rngStream->rangeFile->getNumIons());
	rngStream->enabledRanges.resize(rngStream->rangeFile->getNumRanges());

	streamIn.push_back(rngStream);

	VoxeliseFilter *f = new VoxeliseFilter;

	//Initialise range data
	f->initFilter(streamIn,streamOut);


	f->setCaching(false);
	
	bool needUpdate;
	TEST(f->setProperty(KEY_NBINSX,"4",needUpdate),"num bins x");
	TEST(f->setProperty(KEY_NBINSY,"4",needUpdate),"num bins y");
	TEST(f->setProperty(KEY_NBINSZ,"4",needUpdate),"num bins z");


	TEST(f->setProperty(KEY_NORMALISE_TYPE,
		TRANS(NORMALISE_TYPE_STRING[VOXELISE_NORMALISETYPE_ALLATOMSINVOXEL]),needUpdate), 
				"Set normalise mode");

	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Refresh error code");
	delete f;
	for(unsigned int ui=0;ui<MAX_NUM_RANGES;ui++)
		delete streamIn[ui];
	TEST(streamOut.size() == 2,"stream count");
	TEST(streamOut[1]->getStreamType() == STREAM_TYPE_VOXEL,"Stream type");
	
	const VoxelStreamData *v= (const VoxelStreamData*)streamOut[1];

	TEST(v->data->max() <=1.0f,
			"voxel max less than input stream")
	TEST(v->data->min() >= 0.0f,"voxel counting minimum sanity");


	//all data should lie between 0 and 1
	for(unsigned int ui=0;ui<v->data->getSize();ui++)
	{
		float val;
		val=v->data->getData(ui);
		ASSERT(  val >= 0 && val <= 1.0f); 
	}

	delete v;

	delete rngStream->rangeFile;
	delete rngStream;

	return true;
}


bool VoxeliseFilter::runUnitTests()
{

	if(!voxelSingleCountTest())
		return false;

	if(!voxelMultiCountTest())
		return false;


	return true;
}

#endif
