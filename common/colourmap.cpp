/*
 *	colourmap.cpp  - contiuum colourmap header
 *	Copyright (C) 2010, ViewerGTKQt project
 *	Modifed by D Haley 2013

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


#include <math.h>
#include <stdlib.h>
#include "colourmap.h"
#include <limits>

#include "common/translation.h"

void jetColorMap(unsigned char *rgb,float value,float min,float max)
{
	float max4=(max-min)/4;
	value-=min;
	if (value==HUGE_VAL)
	{
		rgb[0]=rgb[1]=rgb[2]=255;
	}
	else if (value<0)
	{
		rgb[0]=rgb[1]=rgb[2]=0;
	}
	else if (value<max4)
	{
		unsigned char c1=144;
		rgb[0]=0;
		rgb[1]=0;
		rgb[2]=c1+(unsigned char)((255-c1)*value/max4);
	}
	else if (value<2*max4)
	{
		rgb[0]=0;
		rgb[1]=(unsigned char)(255*(value-max4)/max4);
		rgb[2]=255;
	}
	else if (value<3*max4)
	{
		rgb[0]=(unsigned char)(255*(value-2*max4)/max4);
		rgb[1]=255;
		rgb[2]=255-rgb[0];
	}
	else if (value<max)
	{
		rgb[0]=255;
		rgb[1]=(unsigned char)(255-255*(value-3*max4)/max4);
		rgb[2]=0;
	}
	else {
		rgb[0]=255;
		rgb[1]=rgb[2]=0;
	}
}

void hotColorMap(unsigned char *rgb,float value,float min,float max)
{
  float max3=(max-min)/3;
  value-=min;
  if(value==HUGE_VAL)
    {rgb[0]=rgb[1]=rgb[2]=255;}
  else if(value<0)
    {rgb[0]=rgb[1]=rgb[2]=0;}
  else if(value<max3)
    {rgb[0]=(unsigned char)(255*value/max3);rgb[1]=0;rgb[2]=0;}
  else if(value<2*max3)
    {rgb[0]=255;rgb[1]=(unsigned char)(255*(value-max3)/max3);rgb[2]=0;}
  else if(value<max)
    {rgb[0]=255;rgb[1]=255;rgb[2]=(unsigned char)(255*(value-2*max3)/max3);}
  else {rgb[0]=rgb[1]=rgb[2]=255;}
}

void coldColorMap(unsigned char *rgb,float value,float min,float max)
{
  float max3=(max-min)/3;
  value-=min;
  if(value==HUGE_VAL)
    {rgb[0]=rgb[1]=rgb[2]=255;}
  else if(value<0)
    {rgb[0]=rgb[1]=rgb[2]=0;}
  else if(value<max3)
    {rgb[0]=0;rgb[1]=0;rgb[2]=(unsigned char)(255*value/max3);}
  else if(value<2*max3)
    {rgb[0]=0;rgb[1]=(unsigned char)(255*(value-max3)/max3);rgb[2]=255;}
  else if(value<max)
    {rgb[0]=(unsigned char)(255*(value-2*max3)/max3);rgb[1]=255;rgb[2]=255;}
  else {rgb[0]=rgb[1]=rgb[2]=255;}
}

void blueColorMap(unsigned char *rgb,float value,float min,float max)
{
  value-=min;
  if(value==HUGE_VAL)
    {rgb[0]=rgb[1]=rgb[2]=255;}
  else if(value<0)
    {rgb[0]=rgb[1]=rgb[2]=0;}
  else if(value<max)
    {rgb[0]=0;rgb[1]=0;rgb[2]=(unsigned char)(255*value/max);}
  else {rgb[0]=rgb[1]=0;rgb[2]=255;}
}

void positiveColorMap(unsigned char *rgb,float value,float min,float max)
{
  value-=min;
  max-=min;
  value/=max;

  if(value<0){
  rgb[0]=rgb[1]=rgb[2]=0;
    return;
  }
  if(value>1){
  rgb[0]=rgb[1]=rgb[2]=255;
  return;
  }

  rgb[0]=192;rgb[1]=0;rgb[2]=0;
  rgb[0]+=(unsigned char)(63*value);
  rgb[1]+=(unsigned char)(255*value);
  if(value>0.5)
  rgb[2]+=(unsigned char)(255*2*(value-0.5));
}

void negativeColorMap(unsigned char *rgb,float value,float min,float max)
{
  value-=min;
  max-=min;
  rgb[0]=0;rgb[1]=0;rgb[2]=0;
  
  if(max>std::numeric_limits<float>::epsilon())
	  value/=max;
  if(value<0) return;
  if(value>1){
  rgb[1]=rgb[2]=255;
  return;
  }

  rgb[1]+=(unsigned char)(255*value);
  if(value>0.5)
  rgb[2]+=(unsigned char)(255*2*(value-0.5));

}

void colorMap(unsigned char *rgb,float value,float min,float max)
{
  if(value>0) 
    positiveColorMap(rgb,value,0,max);
  else 
    negativeColorMap(rgb,value,min,0);
}

void cyclicColorMap(unsigned char *rgb,float value,float min,float max)
{
  float max3=(max-min)/3;
  value-=(max-min)*(float)floor((value-min)/(max-min));
  if(value<max3)
    {rgb[0]=(unsigned char)(255-255*value/max3);rgb[1]=0;rgb[2]=255-rgb[0];}
  else if(value<2*max3)
    {rgb[0]=0;rgb[1]=(unsigned char)(255*(value-max3)/max3);rgb[2]=255-rgb[1];}
  else if(value<max)
    {rgb[0]=(unsigned char)(255*(value-2*max3)/max3);rgb[1]=255-rgb[0];rgb[2]=0;}

}
void randColorMap(unsigned char *rgb,float value,float min,float max)
{
  srand((int)(65000*(value-min)/(max-min)));
  rgb[0]=(unsigned char)(255*rand());
  rgb[1]=(unsigned char)(255*rand());
  rgb[2]=(unsigned char)(255*rand());
}

void grayColorMap(unsigned char *rgb,float value,float min,float max)
{
  max-=min;
  value-=min;
  rgb[0]=rgb[1]=rgb[2]=(unsigned char)(255*value/max);
}

void colourMapWrap(unsigned int mapID,unsigned char *rgb, float v, 
				float min, float max, bool reverse)

{
	//Colour functions assume  positive value, so remap
	v= v-min;
	max-=min;
	min=0;

	if(reverse)
		v= max-v;

	//Select the desired colour map
	switch(mapID)
	{
		case  0:
			jetColorMap(rgb, v, min, max);
			break;
		case  1:
			hotColorMap(rgb, v, min, max);
			break;
		case  2:
			coldColorMap(rgb, v, min, max);
			break;
		case  3:
			 grayColorMap(rgb, v, min, max);
			break;
		case  4:
			cyclicColorMap(rgb, v, min, max);
			break;
		case  5:
			colorMap(rgb, v, min, max);
			break;
		case  6:
			blueColorMap(rgb, v, min, max);
			break;
		case  7:
			 randColorMap(rgb, v, min, max);
			break;
	}



}

std::string getColourMapName(unsigned int mapID)
{

	const char *mapNames[] = { NTRANS("Jet"),
				NTRANS("Hot"),
				NTRANS("Cold"),
				NTRANS("Grey"),
				NTRANS("Cyclic"),
				NTRANS("General"),
				NTRANS("Blue"),
				NTRANS("Pseudo-Random")};

	return TRANS(mapNames[mapID]);
}


