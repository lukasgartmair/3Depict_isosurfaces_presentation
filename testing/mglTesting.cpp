/*
 *	mglTesting.cpp - unit testing implementation for mgl code
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

#ifdef DEBUG
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <limits>

#include "config.h"

#include "common/assertion.h"

#include <mgl2/canvas_wnd.h>

#include "common/basics.h"
#include "common/stringFuncs.h"


//Create a fixed type of mathgl graph, then compare it to
// a reference image (if possible)
bool mglTest()
{

	//something is wrong with mathgl's FP handling
	bool fpeTrapped=getTrapfpe();
	if(fpeTrapped)
		trapfpe(false);

	unsigned int w=1024,h=768;
	mglGraph *grS;
	grS = new mglGraph(w,h);

	//Create some fake data
	mglData someDataX,someDataY;
	float *bufferX,*bufferY;
	bufferX=new float[100];
	bufferY=new float[100];
	for(unsigned int ui=0; ui<100; ui++)
	{
		bufferX[ui]=ui;
		bufferY[ui]=ui;
	}
	someDataX.Set(bufferX, 100);
	someDataY.Set(bufferY, 100);

	//Set up the plot area
	grS->SetRanges(0,100);
	grS->SetOrigin(mglPoint(0,0));
	grS->Label('x',"axis one");
	grS->Label('y',"axis two",0);
	grS->SetCut(true);
	//set up the axes a little
	mglCanvas *canvas = dynamic_cast<mglCanvas*>(grS->Self());
	canvas->AdjustTicks("x");
	canvas->SetTickTempl('x',"%g");
	canvas->Axis("xy");

	grS->Plot(someDataX,someDataY,"r");

	std::string s,t;
	genRandomFilename(t);
	s=t+".svg";
	
	pushLocale("C",LC_NUMERIC);
	grS->WriteSVG(s.c_str());
	popLocale();

	//Check that the SVG was written
	{
	std::ifstream f(s.c_str());
	if(!f)
	{
		WARN(false,"MGL Did not generate SVG");
		delete[] bufferX;
		delete[] bufferY;
		delete grS;
		return false;
	}
	}

	//Check that mathgl was OK with this
	if(grS->GetWarn())
	{
		WARN(false,"MGL functions returned an error");
		std::cerr << "warncode :" << grS->Self()->GetWarn() << " message:" << grS->Message()<< std::endl;
		delete[] bufferX;
		delete[] bufferY;
		delete grS;

		return false;
	}


	//Try writing a PNG
	s=t+".png";
	grS->WritePNG(s.c_str());
	{
	std::ifstream f(s.c_str());
	if(!f)
	{
		WARN(false,"MGL Did not generate PNG");
		delete[] bufferX;
		delete[] bufferY;
		delete grS;
		return false;
	}
	}

	delete[] bufferX;
	delete[] bufferY;

	//Check that the PNG write was OK
	if(grS->GetWarn())
	{
		WARN(false,"MGL functions returned an error");
		std::cerr << "warncode :" << grS->Self()->GetWarn() << " message:" << grS->Message()<< std::endl;
		delete grS;
		
		return false;
	}

	delete grS;


	//TODO: write non-hack image comparison function
	{
	std::string call="/usr/bin/python ../extras/image-compare-hist.py ";
	call+=s;
	call += " ../test/ref-images/plot-ref.png";
	if(!system(call.c_str()))
	{
		const char *FILE_COMPARE_OUT="img-compare-result-arkd.txt";
		std::ifstream f(FILE_COMPARE_OUT);
		if(f)
		{
			float answer=std::numeric_limits<float>::max();
			f >> answer;

			//As an example, an "OK" image gave 177, a broken image 13000
			const float THRESHOLD=2000;
			TEST(answer < THRESHOLD,"Image comparison failed")
				
		}
		rmFile(FILE_COMPARE_OUT);
	}
	else
	{
		WARN(false,"Unable to execute rather hacky image comparison code");
	}
	}


	rmFile(s);
	rmFile(t+".svg");

	if(fpeTrapped)
		trapfpe(true);
	return true;
}

#endif
