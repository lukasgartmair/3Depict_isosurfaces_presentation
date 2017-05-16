/*
 *	Winconsole.cpp - windows debugging console implementation
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

//Prevent 
#if defined(__WIN32) || defined(__WIN64)

// winconsole.cpp
#include "winconsole.h"
#include <cstdio>
#include <iostream>
#include <windows.h>

winconsole::winconsole()
{
  // create a winconsole window
  AllocConsole();

  // redirect std::cout to our winconsole window
  m_old_cout = std::cout.rdbuf();
  m_out.open("CONOUT$");
  std::cout.rdbuf(m_out.rdbuf());

  // redirect std::cerr to our winconsole window
  m_old_cerr = std::cerr.rdbuf();
  m_err.open("CONOUT$");
  std::cerr.rdbuf(m_err.rdbuf());

  // redirect std::cin to our winconsole window
  m_old_cin = std::cin.rdbuf();
  m_in.open("CONIN$");
  std::cin.rdbuf(m_in.rdbuf());
}

winconsole::~winconsole()
{
  // reset the standard streams
  std::cin.rdbuf(m_old_cin);
  std::cerr.rdbuf(m_old_cerr);
  std::cout.rdbuf(m_old_cout);

  // remove the winconsole window
  FreeConsole();
}

#endif
