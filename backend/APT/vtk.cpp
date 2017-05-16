/*
 *	vtk.cpp - VTK file Import-export 
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


#include "vtk.h"

#include <fstream>

using std::endl;
using std::vector;
using std::string;
using std::cerr;


//Adapted with permission (2016) from mVTK, by
// guillaume flandin
unsigned int vtk_write_legacy(const std::string &filename, unsigned int format,
		const std::vector<IonHit> &ions)
{

	std::ofstream f;

	if(format != VTK_ASCII)
	{
		cerr << "Binary mode is not implemented"
			<< endl;

		return VTK_ERR_NOT_IMPLEMENTED;
	}

	f.open(filename.c_str());

	if(!f)
		return VTK_ERR_FILE_OPEN_FAIL;
		


	f << "# vtk DataFile Version 2.0\n";
	f << "Saved using AtomProbe Tools\n";
	f << "ASCII\n\n";

	f << "DATASET UNSTRUCTURED_GRID\n";
	f << "POINTS " << ions.size() << " float\n";
	//Write ion data which is the support points for later scalar data
	for(unsigned int ui=0;ui<ions.size(); ui++)
	{
		f << ions[ui][0] << " " << ions[ui][1] << " "<< 
			ions[ui][2] << "\n";
	}	

	f << "POINT_DATA " << ions.size() << endl;

	f << "SCALARS masstocharge float\n"; 
	f << "LOOKUP_TABLE default\n";

	for(unsigned int ui=0;ui<ions.size(); ui++)
	{
		f << ions[ui].getMassToCharge() << "\n";
	}


	return 0;
}

//TODO: This is a template function, we will need to move it to the header
template<class T>
unsigned int vtk_write_legacy(const std::string &filename, unsigned int format,

		const Voxels<T> &vox)
{

	std::ofstream f;

	if(format != VTK_ASCII)
	{
		cerr << "Binary mode is not implemented"
			<< endl;

		return VTK_ERR_NOT_IMPLEMENTED;
	}

	f.open(filename.c_str());

	if(!f)
		return VTK_ERR_FILE_OPEN_FAIL;
		


	f << "# vtk DataFile Version 3.0\n";
	f << "Saved using AtomProbe Tools\n";
	f << "ASCII\n\n";

	size_t nx,ny,nz;
	vox.getSize(nx,ny,nz);
	f << "DATASET RECTILINEAR_GRID\n";
	f << "DIMENSIONS " << nx << " " << ny << " " << nz << endl;


	f << "X_COORDINATES " << nx << " float" << endl;
	for(unsigned int ui=0;ui<nx;ui++)
	{
		f << vox.getPoint((nx-1)-ui,0,0)[0] << " ";
	}
	f << endl; 
	
	f << "Y_COORDINATES " << ny << " float" << endl;
	for(unsigned int ui=0;ui<ny;ui++)
	{
		f << vox.getPoint(0,ui,0)[1] << " ";
	}
	f << endl; 
	
	f << "Z_COORDINATES " << nz << " float" << endl;
	for(unsigned int ui=0;ui<nz;ui++)
	{
		f << vox.getPoint(0,0,ui)[2] << " ";
	}
	f << endl; 
	

	f << "POINT_DATA " << vox.size() << endl;
	f << "SCALARS masstocharge float\n"; 
	f << "LOOKUP_TABLE default\n";

	for(unsigned int ui=0;ui<vox.size(); ui++)
	{
		f << vox.getData(ui)<< "\n";
	}
	return 0;
}


#ifdef DEBUG

bool testVTKExport()
{
	vector<IonHit> ions;

	//make a cube of ions, each with a differing mass.
	for(unsigned int ui=0;ui<8;ui++)
		ions.push_back(IonHit(Point3D(ui &1, (ui & 2) >>1, (ui &4) >>2),ui));

	//export it
	TEST(vtk_write_legacy("debug.vtk",VTK_ASCII,ions) == 0,"VTK write");


	Voxels<float> v;
	v.resize(3,3,3);
	v.setData(0,0,0,1);
	v.setData(1,0,0,2);
	v.setData(2,0,0,3);
	v.setData(2,1,0,4);


	vtk_write_legacy("debug-vox.vtk",VTK_ASCII,v);

	return true;	
}

#endif
