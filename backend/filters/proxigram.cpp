/*
 *	Proxigram.cpp - Compute proxigrams based on isosurfaces
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

#include "proxigram.h"
#include "common/colourmap.h"
#include "filterCommon.h"
#include "../plot.h"
#include "openvdb_includes.h"
#include "contribution_transfer_function_TestSuite/CTF_functions.h"
#include <math.h> // pow

#include <map>

enum
{
	KEY_ENABLE_NUMERATOR,
	KEY_ENABLE_DENOMINATOR,
	KEY_VOXELSIZE_LEVELSET,
	KEY_SHELL_WIDTH,
	KEY_MAX_DISTANCE,
	KEY_WEIGHT_FACTOR
};

// == Proxigram filter ==
ProxigramFilter::ProxigramFilter() 
{
 
	voxelsize_levelset = 0.5; // nm
	// the shell width is determined so that in the worst case a voxel is rotated 90 and in the best case 0 degree
	shell_width = (voxelsize_levelset + (sqrt(3)*voxelsize_levelset)) / 2 ; // nm
	max_distance = 0.5; // nm
	numeratorAll = true;
	denominatorAll = true;
	rsdIncoming=0;
	weight_factor = true;
}


Filter *ProxigramFilter::cloneUncached() const
{
	ProxigramFilter *p=new ProxigramFilter();

	p->numeratorAll=numeratorAll;
	p->denominatorAll=denominatorAll;

	p->voxelsize_levelset = voxelsize_levelset;
	p->shell_width = shell_width;
	p->max_distance = max_distance;
	p->weight_factor = weight_factor;

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

	return p;
}

void ProxigramFilter::clearCache() 
{
	Filter::clearCache();
}

size_t ProxigramFilter::numBytesForCache(size_t nObjects) const
{
	return 0;
}

void ProxigramFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
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

unsigned int ProxigramFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
		  std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{
	//Disallow copying of anything in the blockmask. Copy everything else
	propagateStreams(dataIn,getOut,getRefreshBlockMask(),true);

	// Initialize the OpenVDB library.  This must be called at least
    	// once per program and may safely be called multiple times.
	openvdb::initialize();

			//use the cached copy if we have it.
			if(cacheOK)
			{
				propagateCache(getOut);
				return 0;
			}

			// get the vdb grid from the stream	
			const float background_proxi = 0.0;
			openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(background_proxi);
			float isoLevel_proxi = 0;

			for(size_t ui=0;ui<dataIn.size();ui++)
			{
				//Check for vdb stream types. Don't use anything else here
				if(dataIn[ui]->getStreamType() == STREAM_TYPE_OPENVDBGRID)
				{
					const OpenVDBGridStreamData  *vdbgs; 
					vdbgs = (const OpenVDBGridStreamData *)dataIn[ui];

					grid = vdbgs->grid->deepCopy();
					isoLevel_proxi = vdbgs->isovalue;
				}	

			}

			const int xyzs = 3;

			std::vector<openvdb::Vec3s> points;
			std::vector<openvdb::Vec3I> triangles;
			std::vector<openvdb::Vec4I> quads;

			// this is done on the coarse grid
			// recalculate the isosurface mesh to perform calculations based on it
			try
			{
				openvdb::tools::volumeToMesh<openvdb::FloatGrid>(*grid, points, triangles, quads, isoLevel_proxi);	
			}
			catch(const std::exception &e)
			{
				ASSERT(false);
				cerr << "Exception! :" << e.what() << endl;
			}

			openvdb::io::File file("initial_voxelgrid.vdb");
			openvdb::GridPtrVec grids;
			grids.push_back(grid);

			file.write(grids);
			file.close();

			// checking the mesh for nonfinite coordinates like -nan and inf

			int non_finites_counter = 0;

			for (int i=0;i<points.size();i++)
			{
				for (int j=0;j<xyzs;j++)
				{
				    if (std::isfinite(points[i][j]) == false)
					{	
						non_finites_counter += 1;    
					}
				}
			}
			std::cout << "points size" << " = " << points.size() << std::endl;

			//std::cout << "number_of_non_finites" << " = " << non_finites_counter << std::endl;

			// how are the -nans introduced if there is no -nan existing in the grid?! 
			// setting only the nan to zero will of course result in large triangles crossing the scene
			// setting all 3 coordinates to zero is also shit because triangles containing the point are also big
			// how to overcome this without discarding them, which would end up in corrupt faces
			// this behaviour gets checked in the vdb test suite

			for(unsigned int ui=0;ui<points.size();ui++)
			{
				for(unsigned int uj=0;uj<xyzs;uj++)
				{
					if (std::isfinite(points[ui][uj]) == false)
					{
						for(unsigned int uk=0;uk<xyzs;uk++)
						{
							points[ui][uk] = 0.0;
						}
					}
				}
			}

		        // here is no conversion to triangles needed in contrast to drawables, as meshtosigneddistancefield takes quads and triangles

			// calculate a signed distance field

			std::cout << "voxelsize levelset" << " = " << voxelsize_levelset << std::endl;
			std::cout << "shell width" << " = " << shell_width << std::endl;


			// bandwidths are in voxel units
			// the bandwidths have to correlate with the voxelsize of the levelset and the
			// maximum distance below, which is is nm 
			// if i want to calc the max distance 15 nm for example and vs_levelset is only 0.1
			// i need 150 voxels in the outside bandwidth in order to
			// provide the desired information of this regions

			// mesh to signed distance takes floats as input
			// the max distance in the bandwidths has to be greater half the shell width to each side in
			// order to provide the right distances referring to the proximity ranges

			float in_bandwidth = (max_distance + (shell_width/2) ) / voxelsize_levelset;
			float ex_bandwidth = (max_distance + (shell_width/2) ) / voxelsize_levelset;

			openvdb::math::Transform::Ptr transform = openvdb::math::Transform::createLinearTransform(voxelsize_levelset);
			openvdb::FloatGrid::Ptr sdf = openvdb::tools::meshToSignedDistanceField<openvdb::FloatGrid>(*transform, points, triangles, quads, ex_bandwidth, in_bandwidth);

			openvdb::io::File file3("sdf_voxelgrid.vdb");
			openvdb::GridPtrVec grids3;
			grids3.push_back(sdf);

			file3.write(grids3);
			file3.close();

			// two very intgeresting functions in this case are
			// extractActiveVoxelSegmentMasks - 	Return a mask for each connected component of the given grid's active voxels. More...
			// extractIsosurfaceMask - 	Return a mask of the voxels that intersect the implicit surface with the given isovalue. More...	

			// now get a copy of that grid, set all its active values from the narrow band to zero and fill only them with ions of interest
			openvdb::FloatGrid::Ptr numerator_grid_proxi = sdf->deepCopy();
			openvdb::FloatGrid::Ptr denominator_grid_proxi = sdf->deepCopy();

			// initialize another grid with signed distance fields active voxels 
			//give all actives a certain value, which can be asked for in order to retrieve the voxel state
			openvdb::FloatGrid::Ptr voxelstate_grid = sdf->deepCopy();

			// only iterate the active voxels
			// so both the active and inactive voxels should have the value zero
			// but nevertheless different activation states - is that possible?
			// -> yes the result is in the test suite

			// i do have to store the coordinates of all active voxels once here
			// as i cannot find a method to evaluate whether a single voxel is active or inactive 
			openvdb::Coord hkl;

			// set all active voxels in the voxelstate grid to n
			openvdb::FloatGrid::Accessor voxelstate_accessor = voxelstate_grid->getAccessor();
			int active_voxel_state_value = 1.0;

			for (openvdb::FloatGrid::ValueOnIter iter = voxelstate_grid->beginValueOn(); iter; ++iter)
			{
				iter.setValue(active_voxel_state_value);
			}

			for (openvdb::FloatGrid::ValueOnIter iter = numerator_grid_proxi->beginValueOn(); iter; ++iter)
			{   
					iter.setValue(0.0);
			}
			for (openvdb::FloatGrid::ValueOnIter iter = denominator_grid_proxi->beginValueOn(); iter; ++iter)
			{   
					iter.setValue(0.0);
			}

			// set the identical transforms for the other grids if this is even necessary

			voxelstate_grid->setTransform(transform);
			numerator_grid_proxi->setTransform(transform);
			denominator_grid_proxi->setTransform(transform);

			// now run through all ions but only once, and in case they are inside a active voxel write only to them

			openvdb::FloatGrid::Accessor numerator_accessor_proxi = numerator_grid_proxi->getAccessor();
			openvdb::FloatGrid::Accessor denominator_accessor_proxi = denominator_grid_proxi->getAccessor();

			std::cout << " data stream size" << " = " << dataIn.size() << std::endl;

			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				std::cout << " data stream " << ui << " = " << dataIn[ui]->getStreamType() << std::endl;
			}

			//reinitialize the range stream

			const RangeStreamData *c_proxi=0;
			//Determine if we have an incoming range
			for (size_t i = 0; i < dataIn.size(); i++) 
			{
				if(dataIn[i]->getStreamType() == STREAM_TYPE_RANGE)
				{
					c_proxi=(const RangeStreamData *)dataIn[i];

					break;
				}
			}

			//we no longer (or never did) have any incoming ranges. Not much to do
			if(!c_proxi)
			{
				//delete the old incoming range pointer
				if(rsdIncoming)
					delete rsdIncoming;
				rsdIncoming=0;

				enabledIons[0].clear(); //clear numerator options
				enabledIons[1].clear(); //clear denominator options
			}
			else
			{


				//If we didn't have an incoming rsd, then make one up!
				if(!rsdIncoming)
				{
					rsdIncoming = new RangeStreamData;
					*rsdIncoming=*c_proxi;

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
					if(rsdIncoming->rangeFile != c_proxi->rangeFile)
					{
						//hmm, it is different. well, trash the old incoming rng
						delete rsdIncoming;

						rsdIncoming = new RangeStreamData;
						*rsdIncoming=*c_proxi;

						//set the numerator to all disabled
						enabledIons[0].resize(rsdIncoming->rangeFile->getNumIons(),0);
						//set the denominator to have all enabled
						enabledIons[1].resize(rsdIncoming->rangeFile->getNumIons(),1);
					}
				}

			}
	
			for(size_t ui=0;ui<dataIn.size();ui++)
			{
				//Check for ion stream types. Don't use anything else in counting
				if(dataIn[ui]->getStreamType() != STREAM_TYPE_IONS)
					continue;

				const IonStreamData  *ions; 
				ions = (const IonStreamData *)dataIn[ui];

				//get the denominator ions
				unsigned int ionID;
				bool rsd = false;
				if (rsd)
				{
					rsd = true;					
				}
				std::cout << "rsd incoming" << " = " << rsd << std::endl;
				ionID = c_proxi->rangeFile->getIonID(ions->data[0].getMassToCharge());

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
					std::vector<double> atom_position(xyzs); 
					for (int i=0;i<xyzs;i++)
					{
						atom_position[i] = ions->data[uj].getPos()[i];
					}

					// 1st step - project the current atom position to unit voxel i.e. from 0 to 1
					std::vector<double> position_in_unit_voxel;
					position_in_unit_voxel = CTF::projectAtompositionToUnitvoxel(atom_position, voxelsize_levelset);

					// 2nd step - determine each contribution to the adjecent 8 voxels outgoining from the position in the unit voxel
					std::vector<double> volumes_of_subcuboids;
					std::vector<double> contributions_to_adjacent_voxels;
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
					std::vector<std::vector<double> > adjacent_voxel_vertices;
					adjacent_voxel_vertices = CTF::determineAdjacentVoxelVertices(atom_position, voxelsize_levelset);

					// 4th step - assign each of the 8 adjacent voxels the corresponding contribution that results from the atom position in the unit voxel
					const int number_of_adjacent_voxels = 8;
					std::vector<double> current_voxel_index;
					for (int i=0;i<number_of_adjacent_voxels;i++)
					{
						current_voxel_index = adjacent_voxel_vertices[i];
						openvdb::Coord ijk(current_voxel_index[0], current_voxel_index[1], current_voxel_index[2]);

						// write to denominator grid
						if(voxelstate_accessor.getValue(ijk) == active_voxel_state_value)
						{
							denominator_accessor_proxi.setValue(ijk, contributions_to_adjacent_voxels[i] + denominator_accessor_proxi.getValue(ijk));
							// write to numerator grid
							//if(thisNumeratorIonEnabled)
							// test case 								
							if(ionID == 1)								
							{	
								numerator_accessor_proxi.setValue(ijk, contributions_to_adjacent_voxels[i] + numerator_accessor_proxi.getValue(ijk));
							}
							else
							{
								numerator_accessor_proxi.setValue(ijk, 0.0 + numerator_accessor_proxi.getValue(ijk));
							
							}
						}
					}
				}
			}


			openvdb::io::File file2("denominator_grid_proxi.vdb");
			openvdb::GridPtrVec grids2;
			grids2.push_back(denominator_grid_proxi);

			file2.write(grids2);
			file2.close();

			float minVal = 0.0;
			float maxVal = 0.0;

			// now there is a grid with ion information and one grid with distance information

			sdf->evalMinMax(minVal,maxVal);

			std::cout << " eval min max sdf" << " = " << minVal << " , " << maxVal << std::endl;

			// i guess the distances of the sdf are [voxels] -> openvdbtestsuite -> yes it is in the docs of vdb
			// so in order to convert the proximities they should be taken times the voxelsize
			// these proximities are given in nm the conversion is done on the whole sdf grid

			// conversion of the sdf, which is given in voxel units to real world units
			// by multiplication with the higher resolution voxelsize

			openvdb::FloatGrid::Ptr sdf_nm = sdf->deepCopy();

			for (openvdb::FloatGrid::ValueOnIter iter = sdf_nm->beginValueOn(); iter; ++iter)
			{   
					iter.setValue(iter.getValue() * voxelsize_levelset);
			}

			sdf->evalMinMax(minVal,maxVal);

			std::cout << " eval min max sdf_nm" << " = " << minVal << " , " << maxVal << std::endl;
			std::cout << " active voxel count sdf_nm " << " = " << sdf_nm->activeVoxelCount() << std::endl;

			numerator_grid_proxi->evalMinMax(minVal,maxVal);
			std::cout << " eval min max numerator_grid" << " = " << minVal << " , " << maxVal << std::endl;
			std::cout << " active voxel count numerator_grid " << " = " << numerator_grid_proxi->activeVoxelCount() << std::endl;

			denominator_grid_proxi->evalMinMax(minVal,maxVal);
			std::cout << " eval min max denominator_grid" << " = " << minVal << " , " << maxVal << std::endl;
			std::cout << " active voxel count denominator_grid " << " = " << denominator_grid_proxi->activeVoxelCount() << std::endl;

			openvdb::math::CoordBBox bounding_box1 = sdf_nm->evalActiveVoxelBoundingBox();
			openvdb::math::CoordBBox bounding_box2 = denominator_grid_proxi->evalActiveVoxelBoundingBox();

			std::cout << " bounding box sdf_nm " << " = " << bounding_box1 << std::endl;
			std::cout << " bounding box denominator_grid _proxi " << " = " << bounding_box2 << std::endl;

			// for comparison between the coord center of the sampled precipitation and the voxelized one
			std::cout << " bounding_box.getCenter() sdf " << " = " << bounding_box1.getCenter() << std::endl;

			// just get all existing voxel distances of the sdf
			// then get the unique distances
			// then get the appearances of each distance -> simple histogram
			// then get the atom count statistics for each unique distance
			// get the numerators and the denominators
			// summarize the distances
			// then calculate the concentration for each summarized distance
			// then plot concentration over the unique distances

			// dynamic arrays
			std::vector<float> all_distances;
			std::vector<float> unique_distances;

			// 1st get all existing voxel distances of the sdf + 2nd get the unique distances

			for (openvdb::FloatGrid::ValueOnIter iter = sdf_nm->beginValueOn(); iter; ++iter)
			{   
				float current_distance = iter.getValue();

				all_distances.push_back(current_distance);

				if(std::find(unique_distances.begin(), unique_distances.end(), current_distance) != unique_distances.end()) 
				{
				    // v contains x 
				} else {
				    // v does not contain x 
					unique_distances.push_back(current_distance);
				}
			}


			// sort the unique distances in ascending order
			// doing this directly here and mapping the indices is the workaround to
			// avoid sorting two corresponding lists afterwards
	
			std::sort(unique_distances.begin(), unique_distances.end());

			std::cout << " number of unique distances " << " = " << unique_distances.size() << std::endl;
			std::cout << " minimum unique distances " << " = " << *std::min_element(unique_distances.begin(), unique_distances.end()) << std::endl;
			std::cout << " maximum unique distances " << " = " << *std::max_element(unique_distances.begin(), unique_distances.end()) << std::endl;

			std::vector<float> count_distances(unique_distances.size());

			// 3rd count different distances in the sdf
			// http://stackoverflow.com/questions/1425349/how-do-i-find-an-element-position-in-stdvector

			// first create map of each unique distance and its index in the array
	
			std::map<float, int> indicesMap;

			for (int i=0;i<unique_distances.size();i++)
			{
				indicesMap.insert(std::make_pair(unique_distances[i], i));
			}

			for (openvdb::FloatGrid::ValueOnIter iter = sdf_nm->beginValueOn(); iter; ++iter)
			{   
				float current_distance = iter.getValue();
				// find the key in the map to the distance
				int current_index = indicesMap[current_distance];
				count_distances[current_index] += 1;
			}

			// 4th atom count statistics for each unique distance from the denominator grid which holds
			// all atom information (could also be done at the same time as step 3 - separated for better understanding)

			std::vector<float> atomcounts_distances(unique_distances.size());

			for (openvdb::FloatGrid::ValueOnIter iter = sdf_nm->beginValueOn(); iter; ++iter)
			{
				float current_distance = iter.getValue();
				// find the key in the map to the distance
				int current_index = indicesMap[current_distance];
				openvdb::Coord abc;
				abc = iter.getCoord();
				atomcounts_distances[current_index] += denominator_accessor_proxi.getValue(abc);
			}

			// 5th get the numerator and the denominator information for the distances

			std::vector<float> numerators(unique_distances.size());
			std::vector<float> denominators(unique_distances.size());

			for (openvdb::FloatGrid::ValueOnIter iter = sdf_nm->beginValueOn(); iter; ++iter)
 			{
				float current_distance = iter.getValue();
				int current_index = indicesMap[current_distance];
				openvdb::Coord abc;
				abc = iter.getCoord();
				numerators[current_index] += numerator_accessor_proxi.getValue(abc);
				denominators[current_index] += denominator_accessor_proxi.getValue(abc);
			}

			// calculation of the proximity shells
			// with a given shell with of 1 and a max distance of 2 i want to end up with
			// the proximity ends of -1.5,-0.5,0.5,1,5 -1.5, 2.5 
			// so the narrowband has to include the distance -2.5 to 2.5
			// the centers lie at -2,-1,0,1,2

			std::vector<float> proximity_ranges_limits;
			std::vector<float> proximity_ranges_centers;

			int rescue_counter = 10000;
			float current_end = shell_width/2;
			// 1st entries
			proximity_ranges_limits.push_back(current_end);
			proximity_ranges_limits.push_back(-current_end);

			int counter = 0;
			while (current_end < max_distance)
			{	
				proximity_ranges_limits.push_back(current_end + shell_width);
				proximity_ranges_limits.push_back(-(current_end + shell_width));
				current_end += shell_width;
				counter += 1;
				if (counter > rescue_counter)
				{
					break;			
				}
			}

			// now sort the ends
			std::sort(proximity_ranges_limits.begin(), proximity_ranges_limits.end());

			counter = 0;
			float current_center = 0;
			// 1st entry
			proximity_ranges_centers.push_back(current_center);
			while (current_center < max_distance)
			{	
				proximity_ranges_centers.push_back(current_center + shell_width);
				proximity_ranges_centers.push_back(-(current_center + shell_width));
				current_center += shell_width;
				counter += 1;
				if (counter > rescue_counter)
				{
					break;			
				}
			}

			// now sort the centers
			std::sort(proximity_ranges_centers.begin(), proximity_ranges_centers.end());

			int number_of_proximity_ranges = proximity_ranges_centers.size();

			// the theory is describe in 3.3 Internal structure of the proxigram filter
			// example voxelsize 0.3
			// half voxel diagonal = maximal_contribution_distance = (sqrt(3)*0.3)/2 = 0.26
			// half voxel edge = minimal_contribution_distance = (0.3)/2 = 0.15
			// mean_contribution_distance = 0.205
			// a perfectly parallel aligned voxel with distance 0.075 would have right contribution of 
			
			std::vector<float> summarized_numerators(number_of_proximity_ranges);
			std::vector<float> summarized_denominators(number_of_proximity_ranges);


			int proximity_range_index = 0;
			for (int i=0;i<unique_distances.size();i++)
			{

				if ((unique_distances[i] >= proximity_ranges_centers[proximity_ranges_limits.size()-1]) && (unique_distances[i] >= proximity_ranges_limits[proximity_ranges_limits.size()-1]))
				{
					break;
				}

				if (unique_distances[i] >= proximity_ranges_limits[proximity_range_index])
				{

					if (unique_distances[i] > proximity_ranges_limits[proximity_range_index+1])
					{
						proximity_range_index += 1;				
					}				

					summarized_numerators[proximity_range_index] += numerators[i] ;
					summarized_denominators[proximity_range_index] += denominators[i];	
				}
			}	
			
			// 6th calculate the concentration for each unique distance

			std::vector<float> concentrations(number_of_proximity_ranges);
			for (int i=0;i<concentrations.size();i++)
			{
				float concentration = summarized_numerators[i] / summarized_denominators[i];
				concentrations[i] = concentration;
			}

			// write the data to file
			bool export_proxi = true;
			if (export_proxi == true)
			{
				FILE* f = fopen("proxigram_data_3depict.txt","wt");
				fprintf(f, "%s %s %s \n", "distance/nm" , "concentration", "atomcounts");
				for(int i=0;i<concentrations.size();i++) fprintf(f, "%f %f %f\n", proximity_ranges_centers[i], concentrations[i], summarized_denominators[i]);
				fclose(f);
			}

			// manage the filter output

			PlotStreamData *d;
			d=new PlotStreamData;

			d->xyData.resize(number_of_proximity_ranges);

			d->plotStyle = 0;
			d->plotMode=PLOT_MODE_2D;

			d->index=0;
			d->parent=this;

			for(unsigned int ui=0;ui<number_of_proximity_ranges;ui++)
			{
				d->xyData[ui].first = proximity_ranges_centers[ui];
				d->xyData[ui].second = concentrations[ui];

			}

			d->xLabel=TRANS("distance / nm"); 	
			d->yLabel=TRANS("concentration "); 

			d->autoSetHardBounds();

			getOut.push_back(d);


	//Copy the inputs into the outputs, provided they are not voxels
	return 0;
}

void ProxigramFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
			ASSERT(false);
	}

}

void ProxigramFilter::getProperties(FilterPropGroup &propertyList) const
{
	FilterProperty p;
	size_t curGroup=0;

	string tmpStr;


	// group computation
	stream_cast(tmpStr,voxelsize_levelset);
	p.name=TRANS("Voxelsize Levelset / nm");
	p.data=tmpStr;
	p.key=KEY_VOXELSIZE_LEVELSET;
	p.type=PROPERTY_TYPE_REAL;
	p.helpText=TRANS("Voxel size of the levelset in x,y,z direction");
	propertyList.addProperty(p,curGroup);

	propertyList.setGroupTitle(curGroup,TRANS("Computation"));
	curGroup++;

	// group computation
	stream_cast(tmpStr,shell_width);
	p.name=TRANS("Shell width / nm");
	p.data=tmpStr;
	p.key=KEY_SHELL_WIDTH;
	p.type=PROPERTY_TYPE_REAL;
	p.helpText=TRANS("Proximity shell width");
	propertyList.addProperty(p,curGroup);

	propertyList.setGroupTitle(curGroup,TRANS("Computation"));
	curGroup++;

	// group computation
	stream_cast(tmpStr,max_distance);
	p.name=TRANS("Maximal distance / nm");
	p.data=tmpStr;
	p.key=KEY_MAX_DISTANCE;
	p.type=PROPERTY_TYPE_REAL;
	p.helpText=TRANS("Limiting calculation distance");
	propertyList.addProperty(p,curGroup);

	propertyList.setGroupTitle(curGroup,TRANS("Computation"));
	curGroup++;

	stream_cast(tmpStr, weight_factor);
	p.name=TRANS("Distance weight factor");
	p.data=tmpStr;
	p.key=KEY_WEIGHT_FACTOR;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Distance weight factor");
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

}

bool ProxigramFilter::setProperty(unsigned int key,
		  const std::string &value, bool &needUpdate)
{
	
	needUpdate=false;
	switch(key)
	{

		case KEY_VOXELSIZE_LEVELSET:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			voxelsize_levelset=f;
			break;
		}

		case KEY_WEIGHT_FACTOR:
		{
			bool b;
			if(stream_cast(b,value))
				return false;
			weight_factor = b;
			needUpdate=true;
			clearCache();
			break;
		}

		case KEY_SHELL_WIDTH:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			shell_width=f;
			break;
		}


		case KEY_MAX_DISTANCE:
		{
			float f;
			if(stream_cast(f,value))
				return false;
			if(f <= 0.0f)
				return false;
			needUpdate=true;
			max_distance=f;
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

std::string ProxigramFilter::getSpecificErrString(unsigned int code) const
{
	return "";
}

bool ProxigramFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\"" << escapeXML(userString) << "\"/>" << endl;
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
			f << tabs(depth+1) << "<voxelsize_levelset value=\""<<voxelsize_levelset << "\"/>" << endl;
			f << tabs(depth+1) << "<shell_width value=\""<<shell_width << "\"/>" << endl;

			f << tabs(depth+1) << "<max_distance value=\""<<max_distance << "\"/>" << endl;
			f << tabs(depth+1) << "<weight_factor value=\""<<weight_factor << "\"/>" << endl;

			f << tabs(depth) << "</" << trueName() <<">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}
	
	return true;
}

bool ProxigramFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;
	xmlChar *xmlString;
	stack<xmlNodePtr> nodeStack;

	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);


	//--=
	float tmpFloat = 0;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"voxelsize_levelset","value"))
		return false;
	if(tmpFloat <= 0.0f)
		return false;
	voxelsize_levelset=tmpFloat;
	//--=

	//--=
	tmpFloat = 0;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"shell_width","value"))
		return false;
	if(tmpFloat <= 0.0f)
		return false;
	shell_width=tmpFloat;
	//--=

	//--=
	tmpFloat = 0;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"max_distance","value"))
		return false;
	if(tmpFloat <= 0.0f)
		return false;
	max_distance=tmpFloat;
	//--=

	//--=
	bool tmpBool = false;
	if(!XMLGetNextElemAttrib(nodePtr,tmpFloat,"weight_factor","value"))
		return false;
	weight_factor=tmpBool;
	//--=



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

	return true;
	
}

unsigned int ProxigramFilter::getRefreshBlockMask() const
{
	//Ions, plots and voxels cannot pass through this filter
	return STREAM_TYPE_PLOT | STREAM_TYPE_VOXEL;
}

unsigned int ProxigramFilter::getRefreshEmitMask() const
{

	return STREAM_TYPE_OPENVDBGRID | STREAM_TYPE_IONS | STREAM_TYPE_RANGE | STREAM_TYPE_PLOT;

}

unsigned int ProxigramFilter::getRefreshUseMask() const
{

	return STREAM_TYPE_OPENVDBGRID| STREAM_TYPE_IONS | STREAM_TYPE_RANGE;
}
