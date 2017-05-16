/*
 *	colourmap.h - Colour continumms definitions
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

#ifndef _COLORMAP_H_
#define _COLORMAP_H_

#include <string>

const unsigned int NUM_COLOURMAPS=8;

//!get colour for specific map
/* 0 jetColorMap  |  5 colorMap 
 * 1 hotColorMap  |  6 blueColorMap
 * 2 coldColorMap |  7 randColorMap
 * 3 grayColorMap |  
 * 4 cyclicColorMap | 
 *
 * returns char in 0->255 range 
 */
void colourMapWrap(unsigned int mapID,unsigned char *rgb, 
		float value, float min,float max,bool reverse);

std::string getColourMapName(unsigned int mapID);

#endif

