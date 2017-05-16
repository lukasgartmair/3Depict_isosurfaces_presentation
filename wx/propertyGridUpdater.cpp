/*
 * propertyGridUpdater.cpp  - Update a  propertgy grid, using 3depict backend data
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

#include  "propertyGridUpdater.h"

#include "wxcommon.h"
#include "common/stringFuncs.h"
#include "common/basics.h"

//For colour property
#include <wx/propgrid/advprops.h>

//workaround for decimal separator bug
#include <wx/numformatter.h>

#include <vector>
#include <string>

using std::vector;
using std::string;

void updateFilterPropertyGrid(wxPropertyGrid *g, const Filter *f, const string &stateString)
{

	ASSERT(f);
	ASSERT(g);



	FilterPropGroup p;
	f->getProperties(p);
#ifdef DEBUG
	//If debugging, test self consistency
	p.checkConsistent();
#endif	
	g->Clear();
	//Create the keys to add to the grid
	for(size_t ui=0;ui<p.numGroups();ui++)
	{
		vector<FilterProperty> propGrouping;
		p.getGroup(ui,propGrouping);

		std::string title;
		p.getGroupTitle(ui,title);
		
		//Title must be present, or restorestate doesn't work correctly
		ASSERT(!title.empty());

		//Set the name that is to be displayed for this grouping
		// of properties
		g->Append(new wxPropertyCategory(string("") + title,title));
		
		
		//Set the children of this property
		for(size_t uj=0;uj<propGrouping.size();uj++)
		{
			FilterProperty fp;
			fp =propGrouping[uj];

			std::string keyStr;
			stream_cast(keyStr,fp.key);
			
			wxPGProperty *pgp;
			switch(fp.type)
			{
				case PROPERTY_TYPE_BOOL:
				{
					bool boolVal,decOK;
					decOK=boolStrDec(fp.data,boolVal);
					ASSERT(decOK);

					pgp =new wxBoolProperty( fp.name, keyStr,
							boolVal);
					break;
				};
				//TODO: we need a PROPERTY_TYPE_UINT
				case PROPERTY_TYPE_INTEGER:
				{
					long long iV;
					stream_cast(iV,fp.data);

					pgp =new wxIntProperty(fp.name,keyStr,iV);
					break;
				}
				case PROPERTY_TYPE_REAL:
				{
					//workaround for bug in wxFloatProperty under non-english locales.
					if(wxNumberFormatter::GetDecimalSeparator() == '.')
					{
						float fV;
						stream_cast(fV,fp.data);
						pgp =new wxFloatProperty(fp.name,keyStr,fV);
					}
					else
						pgp =new wxStringProperty(fp.name,keyStr,fp.data);
					break;
				};
				case PROPERTY_TYPE_POINT3D:
				case PROPERTY_TYPE_STRING:
				{
					pgp =new wxStringProperty(fp.name,keyStr, fp.data);
					break;
				}
				case PROPERTY_TYPE_CHOICE:
				{
					vector<string> choices;
					unsigned int selected;
					choiceStringToVector(fp.data,choices,selected);

					wxPGChoices pgChoices;
					for(unsigned int ui=0;ui<choices.size();ui++)
					{
						pgChoices.Add(choices[ui],ui);
					}
					pgp = new wxEnumProperty(fp.name,keyStr,pgChoices,selected);
					break;
				}
				case PROPERTY_TYPE_COLOUR:
				{
					bool res;
					ColourRGBA rgba;

					res=rgba.parse(fp.data);
	
					ASSERT(res);
					pgp =  new wxColourProperty(fp.name,keyStr,
								 wxColour(rgba.r(),rgba.g(),rgba.b()) ) ;
					break;
				}
				case PROPERTY_TYPE_FILE:
				{
					pgp =new wxFileProperty(fp.name,keyStr, fp.data);
					
					if(fp.dataSecondary.size())
						pgp->SetAttribute(wxPG_FILE_WILDCARD,fp.dataSecondary);
					
					break;
				}
				case PROPERTY_TYPE_DIR:
				{
					pgp = new wxDirProperty(fp.name,keyStr,fp.data);
					break;
				}
			}

			//Set the tooltip
			pgp->SetHelpString(fp.helpText);

			//add the property to the grid
			g->Append(pgp);

			switch(fp.type)
			{
				case PROPERTY_TYPE_BOOL:
				{
					//if a bool property, use a checkbox to edit
					g->SetPropertyEditor(pgp,wxPGEditor_CheckBox);
					break;
				}
				default:
					;
			}
		}
	}

	//Restore the selected property, if possible
	if(stateString.size())
		g->RestoreEditableState(stateString);
}

void updateCameraPropertyGrid(wxPropertyGrid *g, const Camera *c)
{
	ASSERT(c);
	ASSERT(g);

	g->Clear();

	//Obtain the properties of the currently active camera
	CameraProperties p;
	c->getProperties(p);
	
	for(unsigned int ui=0;ui<p.props.size();ui++)
	{
		for(unsigned int uj=0;uj<p.props[ui].size();uj++)
		{
			CameraProperty camProp;
			camProp=p.props[ui][uj];

			string keyStr;
			stream_cast(keyStr,camProp.key);
			
			wxPGProperty *pgp;
			switch(camProp.type)
			{
				case PROPERTY_TYPE_BOOL:
				{

					bool boolVal,decOK;
					decOK=boolStrDec(camProp.data,boolVal);
					ASSERT(decOK);

					pgp =new wxBoolProperty( camProp.name, keyStr, boolVal);
							
					break;
				};
				case PROPERTY_TYPE_INTEGER:
				{
					long long iV;
					stream_cast(iV,camProp.data);

					pgp =new wxIntProperty(camProp.name,keyStr,iV);
					break;
				}
				case PROPERTY_TYPE_REAL:
				{
					float fV;
					stream_cast(fV,camProp.data);
					pgp =new wxFloatProperty(camProp.name,keyStr,fV);
					break;
				};
				case PROPERTY_TYPE_POINT3D:
				case PROPERTY_TYPE_STRING:
				{
					pgp =new wxStringProperty(camProp.name,keyStr, camProp.data);
					break;
				}
				case PROPERTY_TYPE_CHOICE:
				{
					vector<string> choices;
					unsigned int selected;
					choiceStringToVector(camProp.data,choices,selected);

					wxPGChoices pgChoices;
					for(unsigned int ui=0;ui<choices.size();ui++)
					{
						pgChoices.Add(choices[ui],ui);
					}
					pgp = new wxEnumProperty(camProp.name,keyStr,pgChoices,selected);
					break;
				}
				case PROPERTY_TYPE_COLOUR:
				{
					ColourRGBA rgba;
					rgba.parse(camProp.data);
					pgp =  new wxColourProperty(camProp.name,keyStr,
								 wxColour(rgba.r(),rgba.g(),rgba.b()) ) ;
					break;
				}
			}
			g->Append(pgp);

			switch(camProp.type)
			{
				case PROPERTY_TYPE_BOOL:
				{
					g->SetPropertyEditor(pgp,wxPGEditor_CheckBox);
					break;
				}
				default:
					;
			}

		}
	}
}


std::string getPropValueFromEvent(wxPropertyGridEvent &event)
{
	std::string newValue;

	std::string eventType;
	eventType=event.GetValue().GetType();
	if(eventType == "wxColour")
	{
		wxColour col;
		col << event.GetValue();
		//Convert the colour to a string, so we can 
		// send it to the backend.
		ColourRGBA rgba(col.Red(),col.Green(),col.Blue());
		newValue=rgba.rgbString();
	}
	else if (eventType == "long")
	{
		//So wx is a bit confused here
		// we can either be an integer property, OR
		// we can be an enum property.

		//integer property
		wxLongLong ll;
		ll=event.GetValue().GetLong();
		
		const wxPGChoices &choices = event.GetProperty()->GetChoices();
		if(!choices.IsOk())
		{
			stream_cast(newValue,ll);
		}
		else
		{
			//So wx makes life hard here. We need to do a dance to get the selection
			// as a string
			unsigned int ul;
			ul=ll.ToLong();

			wxArrayString arrStr;
			arrStr=choices.GetLabels();
			newValue=arrStr[ul];
		}
	}
	else
	{
		newValue =  event.GetValue().GetString();
	}

	return newValue;
}
