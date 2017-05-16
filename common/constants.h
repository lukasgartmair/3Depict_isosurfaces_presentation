/*
 * common/constants.h  - Common constants used across program
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
#ifndef COMMONCONSTANTS_H
#define COMMONCONSTANTS_H

#include "translation.h"

//Hack-ish variable for minimal size of data to execute
// openMP conditionals as parallel regions. This is highly empirical!
const unsigned int OPENMP_MIN_DATASIZE=1000;


//Plot error types
enum
{
	PLOT_ERROR_NONE,
	PLOT_ERROR_MOVING_AVERAGE,
	PLOT_ERROR_ENDOFENUM
};

//!State file output formats
enum
{
	STATE_FORMAT_XML=1
};

//!Property types for wxPropertyGrid
enum
{
	PROPERTY_TYPE_BOOL=1,
	PROPERTY_TYPE_INTEGER,
	PROPERTY_TYPE_REAL,
	PROPERTY_TYPE_COLOUR,
	PROPERTY_TYPE_STRING,
	PROPERTY_TYPE_POINT3D,
	PROPERTY_TYPE_CHOICE,
	PROPERTY_TYPE_FILE,
	PROPERTY_TYPE_DIR,
	PROPERTY_TYPE_ENUM_END //Not a prop, just end of enum
};


//!Movement types for plot
enum
{
	REGION_MOVE_EXTEND_XMINUS, //Moving (extend/shrink) lower bound of region
	REGION_MOVE_TRANSLATE_X, // Moving regoin
	REGION_MOVE_EXTEND_XPLUS, // Moving (extend/shrink) upper bound
};


//!Structure to handle error bar drawing in plot
struct PLOT_ERROR
{
	//!Plot data estimator mode
	unsigned int mode;
	//!Number of data points for moving average
	unsigned int movingAverageNum;
};

extern const char *RANGEFILE_WX_CONSTANT;

extern const char *DTD_NAME;
extern const char *PROGRAM_NAME;
extern const char *PROGRAM_VERSION;
extern const char *FONT_FILE;

#endif
