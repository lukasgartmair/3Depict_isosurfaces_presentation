/*
 * art.h  - Program icons header
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

#ifndef ART_H
#define ART_H

#include <wx/artprov.h>

#include "../data/3Depict.xpm"

class MyArtProvider : public wxArtProvider
{
	public:
		MyArtProvider() {}
		virtual ~MyArtProvider() {}

	protected:
		wxBitmap CreateBitmap(const wxArtID& id,
							  const wxArtClient& client,
							  const wxSize& size)
		{
			if (id == _T("MY_ART_ID_ICON"))
				return wxBitmap(_Depict);

			wxBitmap b;
			return b;

		}
};

#endif // ART_H
