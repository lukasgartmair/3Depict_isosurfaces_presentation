/*
 * propertyGridUpdater.h  - Update a  propertgy grid, using 3depict backend data
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

#ifndef PROPERTYGRIDUPDATER_H
#define PROPERTYGRIDUPDATER_H

#include <wx/propgrid/propgrid.h>
#include "backend/filter.h"

#include <map>

const long PROPERTY_GRID_STYLE= wxPG_SPLITTER_AUTO_CENTER;
const long PROPERTY_GRID_EXTRA_STYLE= wxPG_EX_HELP_AS_TOOLTIPS;

//Build a property grid for the 
// The filter key is stored as a string in the property name, for
// each grid item in the property.
// Due to a wx bug, the grid cannot contain items and be shown
// when passed ot this function
// statestring contains the previous grid' state (also part of bug workaround)
void updateFilterPropertyGrid(wxPropertyGrid *g, const Filter *f, const std::string &stateString="");

void updateCameraPropertyGrid(wxPropertyGrid *g, const Camera *c); 

//Convert the property grid value into a 3depict-usable string
std::string getPropValueFromEvent(wxPropertyGridEvent &event);

#endif
