/*
 *  Array2D.h: This file was part of RawTherapee, but is
 *  used as a general-purpose array module.
 *
 *  Copyright (c) 2011 Jan Rinze Peterzon (janrinze@gmail.com)
 *
 *  this program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  this program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  Declaration of flexible 2D arrays
 *
 *  Usage:
 *
 *  	Array2D<type> name (X-size,Y-size);
 *		Array2D<type> name (X-size,Y-size type ** data);
 *
 *		creates an array which is valid within the normal C/C++ scope "{ ... }"
 *
 *      access to elements is a simple as:
 *
 *      	Array2D<float> my_array (10,10); // creates 10x10 array of floats
 *      	value =  my_array[3][5];
 *      	my_array[4][6]=value;
 *
 *      or copy an existing 2D array
 *
 *      	float ** mydata;
 *      	Array2D<float> my_array (10,10,mydata);
 *
 *
 *		Useful extra pointers
 *
 *			<type> ** my_array		gives access to the pointer for access with [][]
 *			<type> *  my_array		gives access to the flat stored data.
 *
 *		Advanced usage:
 *			Array2D<float> my_array				; // empty container.
 *			my_array(10,10) 					; // resize to 10x10 array
 *			my_array(10,10,ARRAY2D_CLEAR_DATA)  ; // resize to 10x10 and clear data
 *
 */
#ifndef ARRAY2D_H_
#define ARRAY2D_H_

#include <cstring>
#include <cstdio>

template<typename T>
class Array2D {

private:
	unsigned int x, y;
	T ** ptr;
	T * data;
	void ar_realloc(unsigned int w, unsigned int h) {
		if ((ptr) && ((h > y) || (4 * h < y))) {
			delete[] ptr;
			ptr = NULL;
		}
		if ((data) && (((h * w) > (x * y)) || ((h * w) < ((x * y) / 4)))) {
			delete[] data;
			data = NULL;
		}
		if (ptr == NULL)
			ptr = new T*[h];
		if (data == NULL)
			data = new T[h * w];

		x = w;
		y = h;
		#pragma omp parallel for
		for (unsigned int i = 0; i < h; i++)
			ptr[i] = data + w * i;
	}
public:

	// use as empty declaration, resize before use!
	// very useful as a member object
	Array2D() :
		x(0), y(0), ptr(NULL), data(NULL) {
	}

	// creator type1
	Array2D(unsigned int w, unsigned int h) {
		data = new T[h * w];
		x = w;
		y = h;
		ptr = new T*[h];
		#pragma omp parallel for
		for (unsigned int i = 0; i < h; i++)
			ptr[i] = data + i * w;
	}

	// creator type 2
	Array2D(unsigned int w, unsigned int h, T ** source ) {
		// when by reference
		// TODO: improve this code with ar_realloc()
			data = new T[h * w];
		x = w;
		y = h;
		ptr = new T*[h];
		#pragma omp parallel for
		for (unsigned int i = 0; i < h; i++) {
			ptr[i] = data + i * w;
			for (unsigned int j = 0; j < w; j++)
				ptr[i][j] = source[i][j];
		}
	}

	// destructor
	~Array2D() {

		if (data)
			delete[] data;
		if (ptr)
			delete[] ptr;
	}

	Array2D(const Array2D &other)
	{
		*this=other;
	}
	
	// use with indices
	T * operator[](unsigned int index) const {
        ASSERT((index>=0) && (index < y));
		return ptr[index];
	}

	// use as pointer to T**
	operator T**() const {
		return ptr;
	}

	// use as pointer to data
	operator T*() const {
		return data;
	}


	// useful within init of parent object
	// or use as resize of 2D array
	void operator()(unsigned int w, unsigned int h ) {
		ar_realloc(w,h);
	}

	// import from flat data
	void operator()(unsigned int w, unsigned int h, T* copy) {
		ar_realloc(w,h);
		memcpy(data, copy, w * h * sizeof(T));
	}
	unsigned int width() const {
		return x;
	}
	unsigned int height() const {
		return y;
	}

	bool empty() const
	{
		return data == NULL;
	}

	operator bool() const {
		return (x > 0 && y > 0);
	}


	void resize(unsigned int newX, unsigned int newY)
	{
		ar_realloc(newX,newY);	
	}
	
	unsigned int size() const
	{
		return x*y;
	}
	void clear() 
	{
		delete data;
		data=NULL;
	}

	void unpack(std::vector<std::vector<T> > &data) const
	{
		data.resize(y);
#pragma omp parallel for
		for(unsigned int ui=0;ui<y; ui++)
		{
			data[ui].resize(x);
			for(unsigned int uj=0;uj<x;uj++)
				data[ui][uj]=ptr[ui][uj];
		}
	}

	Array2D<T> & operator=( const Array2D<T> & rhs) {
		if (this != &rhs)

		{
			ar_realloc(rhs.x, rhs.y);
			// we could have been created from a different
			// array format where each row is created by 'new'
			#pragma omp parallel for
			for (unsigned int i=0;i<y;i++)
				memcpy(ptr[i],rhs.ptr[i],x*sizeof(T));
		}
		return *this;
	}

};
#endif /* Array2D_H_ */
