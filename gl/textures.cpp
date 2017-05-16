/*
 *	textures.cpp - texture wrapper class implementation
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

#if defined(WIN32) || defined(WIN64)
#include <GL/glew.h>
#endif

#include "textures.h"

#include "wx/wxcommon.h"
#include "common/pngread.h"

#include <string>

#include <iostream>

using std::vector;
using std::string;

const char *TEXTURE_OVERLAY_PNG[] = { 
				"textures/Left_clicked_mouse.png",
				"textures/Left-Right-arrow.png",
				"textures/Right_clicked_mouse.png",
				"textures/rotateArrow.png", 
				"textures/middle_clicked_mouse.png",
				"textures/scroll_wheel_mouse.png",
				"textures/enlarge.png",
			
				"textures/keyboard-ctrl.png",
				"textures/keyboard-command.png",
				"textures/keyboard-alt.png",
				"textures/keyboard-tab.png",
				"textures/keyboard-shift.png",
			};

TexturePool::~TexturePool()
{
	closeAll();
}

bool TexturePool::openTexture(const char *texName, unsigned int &texID)
{
	std::string texPath;

	texPath = locateDataFile(texName);

	if(texPath.empty())
		return false;

	//See if we already have this texture (use first frame as keyname)
	for(unsigned int ui=0;ui<openTextures.size();ui++)
	{
		if(openTextures[ui].first == texPath)
		{
			texID = openTextures[ui].second.glID;
			return true;
		}
	}


	//Try to load the texture, as we don't have it
	texture tex;	
	if(pngTexture2D(&tex,texPath.c_str()))
		return false;

	//record the texture in list of textures
	openTextures.push_back(
		make_pair(texPath,tex));

	texID=tex.glID;
	return true;
}

bool TexturePool::openTexture3D(const std::vector<std::string> &fileNames, unsigned int &texId) 
{
	ASSERT(fileNames.size());

	vector<string> fullNames;
	fullNames.resize(fileNames.size());
	for(size_t ui=0;ui<fileNames.size();ui++)
	{
		std::string texPath;
		texPath = locateDataFile(fileNames[ui].c_str());

		if(!texPath.size())
			return false;
		fullNames[ui]=texPath;
	}
	
	//See if we already have this texture (use abs. name)
	for(unsigned int ui=0;ui<openTextures.size();ui++)
	{
		//Use the first name of the file as the key
		if(openTextures[ui].first == fullNames[0])
		{
			texId = openTextures[ui].second.glID;
			return true;
		}
	}


	//Try to load the texture, as we don't have it
	texture tex;	
	if(pngTexture3D(&tex,fullNames))
		return false;
	
	//TODO: Better key storage method! (eg combined hash)
	//Store the texture in the list of open textures,
	// using the first frame of the sequence as the key
	openTextures.push_back(
		std::make_pair(fullNames[0],tex));


	texId=tex.glID;
	return true;
}

//TODO: Refactor to remove this routine
void TexturePool::genTexID(unsigned int &texID, size_t texType) 
{
	texture tex;
	tex.data=0;
 
	glGenTextures(1,&tex.glID);
	texID = tex.glID;
	
	openTextures.push_back(
		make_pair(std::string(""),tex));
}


void TexturePool::closeTexture(unsigned int texId)
{
	for(unsigned int ui=0;ui<openTextures.size();ui++)
	{
		if(openTextures[ui].second.glID == texId)
		{
			glDeleteTextures(1,&openTextures[ui].second.glID);
			delete [] openTextures[ui].second.data;
			openTextures.erase(openTextures.begin()+ui);
			return;
		}
	}
}

void TexturePool::closeAll()
{
	for(unsigned int ui=0;ui<openTextures.size();ui++)
	{
		if(openTextures[ui].second.data)
		{
			delete[] openTextures[ui].second.data;
			glDeleteTextures(1,&openTextures[ui].second.glID);
		}
	}

	openTextures.clear();
}


int pngTexture(texture* dest, const char* filename, GLenum type) 
{
	FILE *fp;
	unsigned int x, y, z;
	png_uint_32 width, height;
	GLint curtex;
	png_bytep *texture_rows;

	if (!check_if_png((char *)filename, &fp, 8))
	{
		if(fp)
			fclose(fp);
		return 1; // could not open, or was not a valid .png
	}

	if (read_png(fp, 8, &texture_rows, &width, &height))
		return 2; // something is wrong with the .png 

	z=0;
	dest->width = width;
	dest->height = height;
	dest->data = new unsigned char[4*width*height];
	for (y=0; y<height; y++)
	{
		for (x=0; x<4*(width); x++)
		{
			dest->data[z++] = texture_rows[y][x];
		}
	}
	free_pngrowpointers(texture_rows,height);

	//Retrieve the in-use texture, which we will reset later
	if (type == GL_TEXTURE_1D)
		glGetIntegerv(GL_TEXTURE_BINDING_1D, &curtex);
	else 
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &curtex);
	
	glGenTextures(1, &(dest->glID));
	glBindTexture(type, dest->glID);
	
	//Send texture to video card
	if (type == GL_TEXTURE_1D)
	{
		glTexImage1D(type, 0, GL_RGBA, dest->width, 0, 
					GL_RGBA, GL_UNSIGNED_BYTE, dest->data);
	}
	else 
	{
		glTexImage2D(type, 0, GL_RGBA, dest->width, dest->height, 0, 
					GL_RGBA, GL_UNSIGNED_BYTE, dest->data);
	}

	//Sett scale-down 
	// and scale-up interpolation to LINEAR
	glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
	glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	//Restore the current opengl texture
	glBindTexture(type, curtex);

	return (0);
}

int pngTexture3D(texture *dest, const vector<string> &fileNames)
{
	if(fileNames.empty())
		return 0;

	dest->depth=fileNames.size();

	//Copy data from disk into temporary storage
	vector<unsigned char> *dataArray=new vector<unsigned char>[fileNames.size()];
	for(size_t ui=0; ui<fileNames.size(); ui++)
	{
		//Pointer is closed by readpng
		FILE *fp;
		png_bytep *texture_rows;
		png_uint_32 width, height;
		if (!check_if_png((char *)fileNames[ui].c_str(), &fp, 8)) 
		{
			if(fp)
				fclose(fp);
			delete[] dataArray;
			return 1; 
		}

		/* something is wrong with the .png */
		if (read_png(fp, 8, &texture_rows, &width, &height))
		{
			delete[] dataArray;
			return 2;
		}

		if(ui)
		{
			//Check to see image is the same size
			if(width != dest->width || height !=dest->height)
			{
				delete[] dataArray;
				free_pngrowpointers(texture_rows,height);
				return 3;
			}
		}

		//Copy data into texture structure
		dest->width = width;
		dest->height = height;

		dataArray[ui].resize(width*height*4);
		size_t arrayDest;
		arrayDest=0;
		for (size_t y=0; y<height; y++)
		{
			for (size_t x=0; x<4*(width); x++)
				dataArray[ui][arrayDest++] = texture_rows[y][x];
		}

		//Free PNG image pointers
		free_pngrowpointers(texture_rows,height);

	}
				

	size_t offset=0;
	//Copy data into cube that we will send to video card
	dest->data = new unsigned char[4*dest->width*dest->height*dest->depth];
	for(size_t ui=0;ui<dest->depth;ui++)
	{
		for(size_t uj=0;uj<dataArray[ui].size();uj++)
		{
			dest->data[offset++]=dataArray[ui][uj];
		}
	}

	delete[] dataArray;
	
	glGenTextures(1, &(dest->glID));
	glBindTexture(GL_TEXTURE_3D, dest->glID);
	
	//Send texture to video card
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, dest->width, dest->height, dest->depth, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, dest->data);

	//Sett scale-down 
	// and scale-up interpolation to LINEAR
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return 0;
}

int pngTexture2D(texture* dest, const char* filename)
{
	return (pngTexture(dest, filename, GL_TEXTURE_2D));
}

int pngTexture1D(texture* dest, const char* filename)
{
	return (pngTexture(dest, filename, GL_TEXTURE_1D));
}
