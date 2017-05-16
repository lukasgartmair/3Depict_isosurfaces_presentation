/*
 *	allFilter.h - Filter class factory header
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
#ifndef ALLFILTER_H
#define ALLFILTER_H
#include "boundingBox.h"
#include "ionDownsample.h"
#include "dataLoad.h"
#include "profile.h"
#include "externalProgram.h"
#include "ionClip.h"
#include "ionColour.h"
#include "rangeFile.h"
#include "clusterAnalysis.h"
#include "spatialAnalysis.h"
#include "spectrumPlot.h"
#include "transform.h"
#include "voxelise.h"
#include "ionInfo.h"
#include "annotation.h"


//!Returns true if the string is a valid filter name
bool isValidFilterName(const std::string &s);

//!Create a "true default" filter from its true name string
Filter *makeFilter(const std::string  &s) ;
//!Create a true default filter from its enum value FILTER_TYPE_*
Filter *makeFilter(unsigned int ui) ;

//!Create a true default filter from its type string
Filter *makeFilterFromDefUserString(const std::string &s) ;

#endif
