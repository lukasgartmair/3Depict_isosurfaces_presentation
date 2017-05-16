/*
 * 	textures.h - Texture control classes header
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


#ifndef TEXTURES_H
#define TEXTURES_H

#include "common/basics.h"

#ifdef CreateDialog
#undef CreateDialog
#endif

#ifdef Yield
#undef Yield
#endif


#ifdef __APPLE__ 
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <vector>
#include <string>


//Named Textures
enum
{
	TEXTURE_LEFT_CLICK=0,
	TEXTURE_TRANSLATE,
	TEXTURE_RIGHT_CLICK,
	TEXTURE_ROTATE,
	TEXTURE_MIDDLE_CLICK,
	TEXTURE_SCROLL_WHEEL,
	TEXTURE_ENLARGE,

	TEXTURE_CTRL,
	TEXTURE_COMMAND, 
	TEXTURE_ALT,
	TEXTURE_TAB,
	TEXTURE_SHIFT
};

//Paths to named textures
extern const char *TEXTURE_OVERLAY_PNG[]; 

typedef struct {
GLuint glID; /* OpenGL name assigned by by glGenTexture*/
GLuint width;
GLuint height;
GLuint depth;
unsigned char *data;
} texture;

class TexturePool
{
private:
		//Filename of textures, or "" if using a generated texture, bound to the texture data
		std::vector<std::pair<std::string,texture> > openTextures;

	public:
		TexturePool() {} ;
		~TexturePool();
		//Open the texture specified by the following file, and
		//then return the texture ID; or just return the texture 
		//if already loaded. Return true on success.
		bool openTexture(const char *texName,unsigned int &texID);
		//Open a set of identically sized images  into a 3D texture object
		bool openTexture3D(const std::vector<std::string> &texName,unsigned int &texID);

		void genTexID(unsigned int &textureID, size_t texType=GL_TEXTURE_2D)  ;

		//Close the specified texture, using its texture ID 
		void closeTexture(unsigned int texID);

		//Close all textures
		void closeAll();
			
};

//!Type can be GL_TEXTURE_1D or GL_TEXTURE_2D
int pngTexture(texture* dest, const char* filename, GLenum type);

//Read a stack of equi-sized PNG images into a 3D opengl texture
int pngTexture3D(texture*, const std::vector<std::string> &filenames);
//read a single PNG image as an opengl texture
int pngTexture2D(texture*, const char*);
int pngTexture1D(texture*, const char*);


#endif
