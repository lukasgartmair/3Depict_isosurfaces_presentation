/*
 * common/constants.cpp  - Common constants used across program
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
#include "constants.h"
#include "translation.h"

//pattern to use when looking for rangefiles
const char *RANGEFILE_WX_CONSTANT= NTRANS("Range Files (*.rng; *.env; *.rrng)|*.rng;*.env;*.rrng;*.RRNG;*.RNG;*.ENV|RNG File (*.rng)|*.rng;*.RNG|Environment File (*.env)|*.env;*.ENV|RRNG Files (*.rrng)|*.rrng;*.RRNG|All Files (*)|*");

//Name of the  DTD file for state loading
const char *DTD_NAME="threeDepict-state.dtd";
//Program name
const char *PROGRAM_NAME = "3Depict";
//Program version
const char *PROGRAM_VERSION = "0.0.19";
//Path to font for Default FTGL  font
const char *FONT_FILE= "FreeSans.ttf";
