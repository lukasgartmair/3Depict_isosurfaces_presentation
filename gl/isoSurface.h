/*
 * isoSurface.h  - Marching cubes implementation 
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


#ifndef ISOSURFACE_H
#define ISOSURFACE_H

#include "common/voxels.h"

class TriangleWithVertexNorm
{
	public:
		Point3D p[3];
	        Point3D normal[3];	

		void getCentroid(Point3D &p) const;
		void computeACWNormal(Point3D &p) const;
		void safeComputeACWNormal(Point3D &p) const;
		float computeArea() const;
		bool isDegenerate() const;
};

struct TriangleWithIndexedVertices
{
	size_t p[3];
};


//Perform marching cube algorithm
void marchingCubes(const Voxels<float> &v,float isoValue, 
		std::vector<TriangleWithVertexNorm> &tVec);

#ifdef DEBUG
bool testIsoSurface();
#endif

#endif
