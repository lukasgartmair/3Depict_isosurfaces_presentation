/*
 *	gsl_helper.cpp - gsl assistance routines
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
#include "gsl_helper.h"

#include <iostream>


using std::cerr;
using std::endl;

void gslPrint(const gsl_matrix *m)
{
	for(unsigned int ui=0;ui<m->size1; ui++)
	{
		cerr << "|";
		for(unsigned int uj=0; uj<m->size2; uj++)
		{
		
			cerr << gsl_matrix_get(m,ui,uj);
			
			if (uj +1 < m->size2)
				cerr << ",\t" ;
		}
		cerr << "\t|" << endl;
	}	
}
