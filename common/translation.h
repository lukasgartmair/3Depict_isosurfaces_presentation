/*
 * common/translation.h  - Program gettext translation macros
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

#ifndef TRANSLATION_H
#define TRANSLATION_H

#include <locale>


#if defined(__APPLE__) || defined(__WIN32__) || defined(__WIN64__)
#include <libintl.h>
#endif

//!Gettext translation macro
#define TRANS(x) (gettext(x))

//!Gettext null-translation macro (mark for translation, but do nothing)
#define NTRANS(x) (x)

#endif
