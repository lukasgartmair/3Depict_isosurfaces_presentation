/*
 * 	winconsole.h - Windows console debugging header
 *	Copyright (C) 2010, William W.

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


// winconsole.h
#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <fstream>

#if defined(__WIN32) || defined(__WIN64)
class winconsole
{
private:
  std::ofstream m_out;
  std::ofstream m_err;
  std::ifstream m_in;

  std::streambuf* m_old_cout;
  std::streambuf* m_old_cerr;
  std::streambuf* m_old_cin;

public:
  winconsole();
  ~winconsole();

  void hide();
  void show();
};
#endif
#endif
