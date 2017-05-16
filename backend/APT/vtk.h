/*
 *	vtk.h - VTK file Import-export 
 *	Copyright (C) 2016, D Haley
 
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
#ifndef VTK_H 
#define VTK_H

#include <vector>
#include <string>

#include "ionhit.h"
#include "common/voxels.h"

enum
{
	VTK_ERR_FILE_OPEN_FAIL=1,
	VTK_ERR_NOT_IMPLEMENTED,
	VTK_ERR_ENUM_END
};

enum
{
	VTK_ASCII,
	VTK_BINARY,
	VTK_FORMAT_ENUM_END
};

//write ions to a VTK (paraview compatible) file. 
// FIXME : This currenly only supports ASCII mode. 
//	Need binary mode because of the large files we have
unsigned int vtk_write_legacy(const std::string &filename, 
	unsigned int format, const std::vector<IonHit> &ions);

unsigned int vtk_write_legacy(const std::string &filename, 
	unsigned int format, const Voxels<class T> &vox);

#ifdef DEBUG
//unit testing
bool testVTKExport();
#endif
#endif 
