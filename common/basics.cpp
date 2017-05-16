/*
 *	basics.cpp  - basic functions header
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

#include "common/basics.h"

#include "common/constants.h"

#include "common/stringFuncs.h"

#include "common/translation.h"

#ifdef __APPLE__
	#include <sys/types.h>
	#include <sys/sysctl.h>
	#include <mach/mach.h>
	#include <unistd.h>
#elif defined __linux__
	//Needed for getting ram total usage under Linux
	#include <sys/sysinfo.h>

#endif

//Needed for stat call on posix systems
#if !defined(__WIN32__) && !defined(__WIN64__)
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <cstring>
#include <clocale>

using std::string;
using std::vector;
using std::list;




//default font to use.
std::string defaultFontFile;

static char *oldLocaleStatic;
static int localeStaticType;

unsigned int getBitNum(unsigned int u)
{
	ASSERT(u);
	unsigned int j=0;
	while(!(u &1) )
	{
		u=u>>1;
		j++;
	}

	return j;
}

std::string boolStrEnc(bool b)
{
	if(b)
		return "1";
	else
		return "0";
}

void pushLocale(const char *newLocale, int type)
{
	ASSERT(!oldLocaleStatic);
	ASSERT(!localeStaticType);

	ASSERT(type == LC_NUMERIC || type == LC_MONETARY || type == LC_CTYPE 
		|| type == LC_COLLATE || type == LC_ALL || type == LC_TIME
		|| type== LC_MESSAGES);

	oldLocaleStatic=setlocale(type,NULL);   

	//setlocale reserves the right to trash the returned pointer      
	// on subsequent calls (i.e. use the returned pointer for later)
	// thus we must duplicate the pointer to own it
	oldLocaleStatic=strdup(oldLocaleStatic);      
	if(strcmp(oldLocaleStatic,newLocale)) 
	{
		setlocale(type,newLocale);        
		localeStaticType=type;
	}
	else
	{
		//record that we did not set this
		localeStaticType=-1;
	}

}

void popLocale()
{
	if(localeStaticType != -1)
		setlocale(localeStaticType,oldLocaleStatic);

	localeStaticType=0;

	free(oldLocaleStatic);
	oldLocaleStatic=0;
}


bool dummyCallback(bool)
{
	return true;
}

void setDefaultFontFile(const std::string &font)
{
	defaultFontFile=font;
}

std::string getDefaultFontFile()
{
	return defaultFontFile;
}

//Compute the number of ticks require to achieve the 
void tickSpacingsFromInterspace(float start, float end, 
		float interSpacing, std::vector<float> &spacings)
{
	ASSERT(interSpacing > sqrtf(std::numeric_limits<float>::epsilon()));
	unsigned int nTicks;

	if(end < start)
		std::swap(end,start);

	nTicks=(unsigned int)((end-start)/interSpacing);
	if(!nTicks)
	{
		ASSERT(!spacings.size());
		return;
	}

	spacings.resize(nTicks);
	for(unsigned int ui=0;ui<nTicks;ui++)
		spacings[ui]=ui*interSpacing+start;

}

void tickSpacingsFromFixedNum(float start, float end, 
		unsigned int nTicks, std::vector<float> &spacings)
{
	if(!nTicks)
	{
		ASSERT(!spacings.size());
		return;
	}

	spacings.resize(nTicks+1);

	float delta;
	delta= (end-start)/nTicks;
	for(unsigned int ui=0;ui<nTicks;ui++)
		spacings[ui]=ui*delta+start;
}

//========

//"Fuzzy" time, like a person might say it
string veryFuzzyTimeSince( time_t origTime, time_t nowTime)
{

	if(nowTime<origTime)
		return string(TRANS("in the future?"));

	time_t delta;

	delta= nowTime-origTime;

	string retString;

	const unsigned int NUM_FUZZY_ENTRIES=16;
	//Sorted sequence of fuzzy, approximate times, from biggest to smallest
	const time_t TIMESTOPS[] = {
					(10*36525*24*6*6), //One decade (factor of 100 taken to prevent invalid C++11 narrowing double)
					(36525*24*6*6), // One year
					(36525/12*24*6*6), // One month (factor of 100 taken from minutes and hrs to prevent invalid C++11 narrowing double code)
					(7*24*60*60), //One week
					(24*60*60), //One day
					(60*60), //One hour
					(45*60),// 45 minutes
					(30*60),// 30 minutes
					(20*60),// 20 minutes
					(15*60),// 15 minutes
					(10*60),// 10 minutes
					(5*60),//5 minutes
					(60), //1 minute 
					30, //30 sec
					10, //10 sec
					1 //1 sec
				};

	//Do these have a meaningful plural?
	bool HAVE_PLURALS[] = { 
					true,	//decade	
					true,	//year
					true,	//month
					true,	//week
					true,	//day
					true,	//hour
					false,	//45m	
					false,	//30m	
					false,	//20m	
					false,	//15m	
					false,	//10m	
					false,	//5m
					true,	//1m
					false,//30s	
					false,//10s	
					true//1s	
				};				
		
	//Singular version
	const char *SINGLE_FUZZY_STRING[] = {
			 NTRANS("a decade ago"),
			 NTRANS("a year ago"),
			 NTRANS("a month ago"),
			 NTRANS("a week ago"),
			 NTRANS("a day ago"),
			 NTRANS("an hour ago"),
			 NTRANS("45 minutes ago"),
			 NTRANS("30 minutes ago"),
			 NTRANS("20 minutes ago"),
			 NTRANS("15 minutes ago"),
			 NTRANS("10 minutes ago"),
			 NTRANS("5 minutes ago"),
			 NTRANS("a minute ago"),
			 NTRANS("30 seconds ago"),
			 NTRANS("10 seconds ago"),
			 NTRANS("a second ago")
				};		

	//Plurals, where they make sense. otherwise empty string
	const char *PLURAL_FUZZY_STRING[] = {
			 NTRANS("a few decades ago"),
			 NTRANS("a few years ago"),
			 NTRANS("a few months ago"),
			 NTRANS("a few weeks ago"),
			 NTRANS("a few days ago"),
			 NTRANS("a few hours ago"),
			 "", //45m
			 "", //30m
			 "", //20m
			 "", //15m
			 "", //10m
			 "", //5m
			 NTRANS("a few minutes ago"),
			 "",//30s
			 "",//10s
			 NTRANS("a few seconds ago")
				};		

	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(PLURAL_FUZZY_STRING)==  NUM_FUZZY_ENTRIES);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(SINGLE_FUZZY_STRING)==  NUM_FUZZY_ENTRIES);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(HAVE_PLURALS)==  NUM_FUZZY_ENTRIES);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(TIMESTOPS)==  NUM_FUZZY_ENTRIES);

	//Find the largest match by descending the timestops
	for(unsigned int ui=0;ui<NUM_FUZZY_ENTRIES; ui++)
	{

		//If we have a plural, and we are double are base timestop, use it.
		if(HAVE_PLURALS[ui] && delta>=2*TIMESTOPS[ui])
		{
#ifdef DEBUG
			std::string s=(PLURAL_FUZZY_STRING[ui]);
			ASSERT(s.size());
#endif
			return TRANS(PLURAL_FUZZY_STRING[ui]);
		}

		//stop descending	
		if ( delta>=TIMESTOPS[ui])
			return TRANS(SINGLE_FUZZY_STRING[ui]);
	}

	return TRANS("moments ago");
}

ColourRGBA::ColourRGBA()
{
}

ColourRGBA::ColourRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) 
{
	data[0]=r;
	data[1]=g;
	data[2]=b;
	data[3]=a;
}


ColourRGBA::ColourRGBA(unsigned char r, unsigned char g, unsigned char b) 
{
	data[0]=r;
	data[1]=g;
	data[2]=b;
}

unsigned char ColourRGBA::at(unsigned int idx) const
{
	ASSERT(idx< 4);
	return data[idx];
}

unsigned char ColourRGBA::r() const
{
    return data[0];
}

unsigned char ColourRGBA::g() const
{
    return data[1];
}

unsigned char ColourRGBA::b() const
{
    return data[2];
}

unsigned char ColourRGBA::a() const
{
    return data[3];
}

bool ColourRGBA::parse(const std::string &str)	
{
	//Input string is in 2 char hex form, 3 or 4 colour, with # leading. RGB order
	//lowercase string.
	if(str.size() != 9 && str.size() != 7)
		return false;

	if(str[0] != '#')
		return false;

	string rS,gS,bS,aS;
	rS=str.substr(1,2);
	gS=str.substr(3,2);
	bS=str.substr(5,2);

	if(!isxdigit(rS[0]) || !isxdigit(rS[1]))
		return false;
	if(!isxdigit(gS[0]) || !isxdigit(gS[1]))
		return false;
	if(!isxdigit(bS[0]) || !isxdigit(bS[1]))
		return false;

	unsigned char r,g,b,a;
	hexStrToUChar(str.substr(1,2),r);	
	hexStrToUChar(str.substr(3,2),g);	
	hexStrToUChar(str.substr(5,2),b);	
	//3 colour must have a=255.
	if(str.size() == 7)
		a = 255;
	else
	{
		aS=str.substr(7,2);
		if(!isxdigit(aS[0]) || !isxdigit(aS[1]))
			return false;
		hexStrToUChar(str.substr(7,2),a);	
	}
	
	data[0]=r;
	data[1]=g;
	data[2]=b;
	data[3]=a;
	return true;
}

std::string ColourRGBA::rgbaString() const
{
	std::string s="#", tmp;
	ucharToHexStr(data[0],tmp);
	s+=tmp;
	ucharToHexStr(data[1],tmp);
	s+=tmp;
	ucharToHexStr(data[2],tmp);
	s+=tmp;
	ucharToHexStr(data[3],tmp);
	s+=tmp;

	return s;
}

std::string ColourRGBA::rgbString() const
{
	string tmp,s;
	s="#";
	ucharToHexStr(data[0],tmp);
	s+=tmp;
	ucharToHexStr(data[1],tmp);
	s+=tmp;
	ucharToHexStr(data[2],tmp);
	s+=tmp;

	return s;
}

RGBf ColourRGBA::toFloat() const
{
	RGBf ret;
	ret.red=(float)data[0]/255.0f;
	ret.green=(float)data[1]/255.0f;
	ret.blue=(float)data[2]/255.0f;

	return ret;
}

ColourRGBAf ColourRGBA::toRGBAf() const
{
	ColourRGBAf tmp;
		
	for(unsigned int ui=0;ui<4;ui++)
	{
		tmp[ui] = (float)data[ui]/255.0f;
	}

	return tmp;
}

void ColourRGBA::fromRGBf(const RGBf &oth) 
{
	data[0]=oth.red*255.0f;
	data[1]=oth.green*255.0f;
	data[2]=oth.blue*255.0f;
	data[3]=255.0f;
}

bool ColourRGBA::operator==(const ColourRGBA &oth) const
{
	for(unsigned int ui=0;ui<4;ui++)
	{
		if(data[ui] != oth.data[ui])
			return false;
	}
	return true;
}

bool ColourRGBA::operator==(const ColourRGBAf &oth) const
{
	for(unsigned int ui=0;ui<4;ui++)
	{
		if(data[ui] != oth.at(ui))
			return false;
	}
	return true;
}

bool ColourRGBA::operator==(const RGBf &oth) const
{
	return (data[0]/255.0f == oth.red && data[1]/255.0f == oth.green && data[2]/255.0f == oth.blue);
		
}

bool ColourRGBA::operator!=(const ColourRGBA &oth) const
{
	return !(*this == oth);
}

bool ColourRGBA::operator!=(const ColourRGBAf &oth) const
{
	return !(*this == oth);
}


ColourRGBAf::ColourRGBAf()
{
}

ColourRGBAf::ColourRGBAf(float r, float g, float b, float a) 
{
	ASSERT(r >=0 && r <=1.0f); 
	ASSERT(g >=0 && g <=1.0f); 
	ASSERT(b >=0 && b <=1.0f); 
	ASSERT(a >=0 && a <=1.0f); 
	data[0]=r;
	data[1]=g;
	data[2]=b;
	data[3]=a;
}


ColourRGBAf::ColourRGBAf(float r, float g, float b) 
{
	ASSERT(r >=0 && r <=1.0f); 
	ASSERT(g >=0 && g <=1.0f); 
	ASSERT(b >=0 && b <=1.0f); 
	data[0]=r;
	data[1]=g;
	data[2]=b;
	data[3]=1.0f;
}
float ColourRGBAf::r() const
{
    return data[0];
}

float ColourRGBAf::g() const
{
    return data[1];
}

float ColourRGBAf::b() const
{
    return data[2];
}

float ColourRGBAf::a() const
{
    return data[3];
}

void ColourRGBAf::r(float v)
{
	ASSERT(v >=0.0f && v <=1.0f);
	data[0]=v;
}

void ColourRGBAf::g(float v)
{
	ASSERT(v >=0.0f && v <=1.0f);
	data[1]=v;
}

void ColourRGBAf::b(float v)
{
	ASSERT(v >=0.0f && v <=1.0f);
	data[2]=v;
}

void ColourRGBAf::a(float v)
{
	ASSERT(v >=0.0f && v <=1.0f);
	data[3]=v;
}
float &ColourRGBAf::operator[](unsigned int idx) 
{
	ASSERT(idx < 4);
	return data[idx];
}

float ColourRGBAf::at(unsigned int idx) const
{
	return data[idx];
}

		
ColourRGBAf ColourRGBAf::interpolate(float delta, const ColourRGBAf &other)
{
	ColourRGBAf result;

	for(unsigned int ui=0;ui<4;ui++)
		result[ui] = data[ui] + (other.data[ui] - data[ui])*delta;
	return result;
}

ColourRGBA ColourRGBAf::toColourRGBA() const
{
	ColourRGBA tmp(data[0]*255.0f,data[1]*255.0f,
			data[2]*255.0f,data[3]*255.0f);
	return tmp;
}

RGBf ColourRGBAf::toRGBf() const
{
	RGBf tmp;
	tmp.red=data[0];
	tmp.green=data[1];
	tmp.blue=data[2];
	return tmp;
}


void ColourRGBAf::operator=(const RGBf &oth)
{
	data[0]= oth.red;
	data[1]= oth.green;
	data[2]= oth.blue;
	data[3]= 1.0f;
}


bool ColourRGBAf::operator==(const ColourRGBA &oth) const
{
	for(unsigned int ui=0;ui<3;ui++)
	{
		if(data[ui] != (float)oth.at(ui)/255.0f)
			return false;
	}

	return true;
}

bool ColourRGBAf::operator==(const ColourRGBAf &oth) const
{
	for(unsigned int ui=0;ui<3;ui++)
	{
		if(data[ui] != (float)oth.data[ui])
			return false;
	}

	return true;
}
bool ColourRGBAf::operator!=(const ColourRGBAf  &oth) const
{
	return !(*this == oth);
}
void BoundCube::getBound(Point3D &retBound, unsigned int minMax) const
{
	retBound=Point3D(bounds[0][minMax],
			bounds[1][minMax],
			bounds[2][minMax]);

}

float BoundCube::getBound(unsigned int bound, unsigned int minMax) const
{
        ASSERT(bound <3 && minMax < 2);
	ASSERT(valid[bound][minMax]==true);
        return bounds[bound][minMax];
}

void BoundCube::setBound(unsigned int bound, unsigned int minMax, float value)
{
        ASSERT(bound <3 && minMax < 2);
        bounds[bound][minMax]=value;
        valid[bound][minMax]=true;
}

void BoundCube::setBounds(const std::vector<Point3D> &points)
{
	
	setInverseLimits();	
	for(unsigned int ui=0; ui<points.size(); ui++)
	{
		for(unsigned int uj=0; uj<3; uj++)
		{
			if(points[ui].getValue(uj) < bounds[uj][0])
			{
				{
				bounds[uj][0] = points[ui].getValue(uj);
				valid[uj][0]=true;
				}
			}
			
			if(points[ui].getValue(uj) > bounds[uj][1])
			{
				{
				bounds[uj][1] = points[ui].getValue(uj);
				valid[uj][1]=true;
				}
			}
		}
	}

#ifdef DEBUG
	for(unsigned int ui=0; ui<points.size(); ui++)
	{
		ASSERT(containsPt(points[ui]));
	}
#endif
}

void BoundCube::setBounds(const Point3D &p, float r)
{
	for(unsigned int dim=0;dim<3;dim++)
	{
		bounds[dim][0] = p[dim] - r;
		bounds[dim][1] = p[dim] + r;
		valid[dim][0]=true;
		valid[dim][1]=true;
	}
}

void BoundCube::getVertices(std::vector<Point3D> &points, bool centre) const
{
	points.resize(8);

	for(size_t ui=0;ui<8;ui++)
		points[ui]=getVertex(ui);

	if(centre)
	{
		Point3D centroid=getCentroid();
		for(size_t ui=0;ui<8;ui++)
			points[ui]-=centroid;
	}
}

void BoundCube::getPlaneIntersectVertices(const Point3D &planeOrigin, const Point3D &normal, vector<Point3D> &intersectPts) const
{
	//To visualise the connections, draw a cube, then label
	// each coordinate using a binary table like so:
	// idx  x y z
	// 0    0 0 0
	// 1    1 0 0 
	// etc ..

	//Now flatten the cube into a graph like so :
	//	0_________ 2
	//	| \4___6/ |	^	y+ ->
	//	|  |   |  |	| x-
	//	|  5---7  |
	//	| /     \ |
	//	1---------- 3

	//Edges are between  (idx  - idx + 4) (idx <4)		: 4 edges (diag) 
	// 		and  (idx, idx +2) (idx in 0,1,4,5)	: 4 edges (across)
	//		and  (idx,idx+1) (idx in 0,2,4,6)	: 4 edges (vertical)

	//Adjacency graph for cube edges
	const unsigned int eStartIdx[12] = {0,1,2,3, 0,1,4,5, 0,2,4,6};
	const unsigned int eEndIdx[12] = {4,5,6,7, 2,3,6,7, 1,3,5,7};
	

	for(unsigned int ui=0;ui<12;ui++)
	{
		Point3D eStart,eEnd;
		eStart = getVertex(eStartIdx[ui]);
		eEnd = getVertex(eEndIdx[ui]);

		float denom = (eEnd-eStart).dotProd(normal);

		//check for intersection. If line vector is perp to
		// plane normal, either is in plane, or no intersection
		// for our purpose, do not report intersections that are in-the-plane
		if(fabs(denom) < sqrtf(std::numeric_limits<float>::epsilon()))
			continue;

		float numerator = (planeOrigin - eStart).dotProd(normal);
		float v;
		v= numerator/denom;	
		intersectPts.push_back((eEnd-eStart)*v+ eStart);
	}
}

Point3D BoundCube::getVertex(unsigned int idx) const
{
	ASSERT(idx < 8);
	
	return Point3D(bounds[0][(idx&1)],bounds[1][(idx&2) >> 1],bounds[2][(idx&4)>>2]);
}

void BoundCube::setInverseLimits(bool setValid)
{
	bounds[0][0] = std::numeric_limits<float>::max();
	bounds[1][0] = std::numeric_limits<float>::max();
	bounds[2][0] = std::numeric_limits<float>::max();
	
	bounds[0][1] = -std::numeric_limits<float>::max();
	bounds[1][1] = -std::numeric_limits<float>::max();
	bounds[2][1] = -std::numeric_limits<float>::max();

	valid[0][0] =setValid;
	valid[1][0] =setValid;
	valid[2][0] =setValid;
	
	valid[0][1] =setValid;
	valid[1][1] =setValid;
	valid[2][1] =setValid;
}

bool BoundCube::isValid() const
{
	for(unsigned int ui=0;ui<3; ui++)
	{
		if(!valid[ui][0] || !valid[ui][1])
			return false;
	}

	return true;
}

bool BoundCube::isFlat() const
{
	//Test the limits being inverted or equated
	for(unsigned int ui=0;ui<3; ui++)
	{
		if(fabs(bounds[ui][0] - bounds[ui][1]) < std::numeric_limits<float>::epsilon())
			return true;
	}	

	return false;
}

bool BoundCube::isNumericallyBig() const
{
	const float TOO_BIG=sqrtf(std::numeric_limits<float>::max());
	for(unsigned int ui=0;ui<2; ui++)
	{
		for(unsigned int uj=0;uj<3; uj++)
		{
			if(TOO_BIG < fabs(bounds[uj][ui]))
				return true;
		}
	}
	return false;
}

void BoundCube::expand(const BoundCube &b) 
{
	//Check both lower and upper limit.
	//Moving to other cubes value as needed

	if(!b.isValid())
		return;

	for(unsigned int ui=0; ui<3; ui++)
	{
		if(b.bounds[ui][0] < bounds[ui][0])
		{
		       bounds[ui][0] = b.bounds[ui][0];	
		       valid[ui][0] = true;
		}

		if(b.bounds[ui][1] > bounds[ui][1])
		{
		       bounds[ui][1] = b.bounds[ui][1];	
		       valid[ui][1] = true;
		}
	}
}

void BoundCube::expand(const Point3D &p) 
{
	//If self not valid, ensure that it will be after this run
	for(unsigned int ui=0; ui<3; ui++)
	{
		//Check lower bound is lower to new pt
		if(bounds[ui][0] > p[ui])
		       bounds[ui][0] = p[ui];

		//Check upper bound is upper to new pt
		if(bounds[ui][1] < p[ui])
		       bounds[ui][1] = p[ui];
	}
}

void BoundCube::expand(float f) 
{
	//If self not valid, ensure that it will be after this run
	for(unsigned int ui=0; ui<3; ui++)
	{
		//Check lower bound is lower to new pt
		bounds[ui][0]-=f;

		//Check upper bound is upper to new pt
		bounds[ui][1]+=f;
	}
}

void BoundCube::setBounds(const Point3D *p, unsigned int n)
{
	bounds[0][0] = std::numeric_limits<float>::max();
	bounds[1][0] = std::numeric_limits<float>::max();
	bounds[2][0] = std::numeric_limits<float>::max();
	
	bounds[0][1] = -std::numeric_limits<float>::max();
	bounds[1][1] = -std::numeric_limits<float>::max();
	bounds[2][1] = -std::numeric_limits<float>::max();
	
	for(unsigned int ui=0;ui<n; ui++)
	{
		for(unsigned int uj=0;uj<3;uj++)
		{
			if(bounds[uj][0] > p[ui][uj])
			{
			       bounds[uj][0] = p[ui][uj];
		       		valid[uj][0] = true;	      
			}	


			if(bounds[uj][1] < p[ui][uj])
			{
			       bounds[uj][1] = p[ui][uj];		
		       		valid[uj][1] = true;	      
			}	
		}	
	}
}

void BoundCube::setBounds( const Point3D &p1, const Point3D &p2)
{
	for(unsigned int ui=0; ui<3; ui++)
	{
		bounds[ui][0]=std::min(p1[ui],p2[ui]);
		bounds[ui][1]=std::max(p1[ui],p2[ui]);
		valid[ui][0]= true;
		valid[ui][1]= true;
	}

}
void BoundCube::getBounds(Point3D &low, Point3D &high) const 
{
	for(unsigned int ui=0; ui<3; ui++) 
	{
		ASSERT(valid[ui][0] && valid[ui][1]);
		low.setValue(ui,bounds[ui][0]);
		high.setValue(ui,bounds[ui][1]);
	}
}

float BoundCube::getLargestDim() const
{
	float f;
	f=getSize(0);
	f=std::max(getSize(1),f);	
	return std::max(getSize(2),f);	
}

bool BoundCube::containsPt(const Point3D &p) const
{
	for(unsigned int ui=0; ui<3; ui++)
	{
		ASSERT(valid[ui][0] && valid[ui][1]);
		if(p.getValue(ui) < bounds[ui][0] || p.getValue(ui) > bounds[ui][1])
			return false;
	}
	return true;
}

bool BoundCube::contains(const BoundCube &b) const
{
	Point3D low,high;
	b.getBounds(low,high);
	return containsPt(low) && containsPt(high); 
}
float BoundCube::getSize(unsigned int dim) const
{
	ASSERT(dim < 3);
#ifdef DEBUG
	for(unsigned int ui=0;ui<3; ui++)
	{
		ASSERT(valid[0][1] && valid [0][0]);
	}
#endif
	return fabs(bounds[dim][1] - bounds[dim][0]);
}

//checks intersection with sphere [centre,centre+radius)
bool BoundCube::intersects(const Point3D &pt, float sqrRad) const
{
	Point3D nearPt;
	
	//Find the closest point on the cube  to the sphere
	for(unsigned int ui=0;ui<3;ui++)
	{
		if(pt.getValue(ui) <= bounds[ui][0])
		{
			nearPt.setValue(ui,bounds[ui][0]);
			continue;
		}
		
		if(pt.getValue(ui) >=bounds[ui][1])
		{
			nearPt.setValue(ui,bounds[ui][1]);
			continue;
		}
	
		nearPt.setValue(ui,pt[ui]);
	}

	//now test the distance from nrPt to pt
	//Note that the touching case is considered to be an intersection
	return (nearPt.sqrDist(pt) <=sqrRad);
}

BoundCube BoundCube::makeUnion(const BoundCube &bC) const
{
	BoundCube res;
	for(unsigned int dim=0;dim<3;dim++)
	{
		float a,b;
		a=bounds[dim][0]; b=bC.bounds[dim][0];
		res.setBound(dim,0,std::max(a,b));
		a=bounds[dim][1]; b=bC.bounds[dim][1];
		res.setBound(dim,1,std::min(a,b));
	}

	return res;
}

unsigned int BoundCube::segmentTriple(unsigned int dim, float slice) const
{
	ASSERT(dim < 3);

	//check lower
	if( slice < bounds[dim][0])
		return 0;
	
	//check upper
	if( slice >=bounds[dim][1])
		return 2;

	return 1;

}


Point3D BoundCube::getCentroid() const
{
#ifdef DEBUG
	for(unsigned int ui=0;ui<3; ui++)
	{
		ASSERT(valid[ui][1] && valid [ui][0]);
	}
#endif
	return Point3D(bounds[0][1] + bounds[0][0],
			bounds[1][1] + bounds[1][0],
			bounds[2][1] + bounds[2][0])/2.0f;
}

float BoundCube::getMaxDistanceToBox(const Point3D &queryPt) const
{
#ifdef DEBUG
	for(unsigned int ui=0;ui<3; ui++)
	{
		ASSERT(valid[ui][1] && valid [ui][0]);
	}
#endif

	float maxDistSqr=0.0f;


	//Set lower and upper corners on the bounding rectangle
	Point3D p[2];
	p[0] = Point3D(bounds[0][0],bounds[1][0],bounds[2][0]);
	p[1] = Point3D(bounds[0][1],bounds[1][1],bounds[2][1]);

	//Count binary-wise selecting upper and lower limits, to enumerate all 8 vertices.
	for(unsigned int ui=0;ui<9; ui++)
	{
		maxDistSqr=std::max(maxDistSqr,
			queryPt.sqrDist(Point3D(p[ui&1][0],p[(ui&2) >> 1][1],p[(ui&4) >> 2][2])));
	}

	return sqrtf(maxDistSqr);
}

bool BoundCube::containedInSphere(const Point3D &queryPt,float sqrDist) const
{
#ifdef DEBUG
	for(unsigned int ui=0;ui<3; ui++)
	{
		ASSERT(valid[ui][1] && valid [ui][0]);
	}
#endif

	//Check all vertices
	for(unsigned int ui=0;ui<8; ui++)
	{
		if(queryPt.sqrDist(getVertex(ui)) > sqrDist)
			return false;
	}

	return true;
}

const BoundCube &BoundCube::operator=(const BoundCube &b)
{
	for(unsigned int ui=0;ui<3; ui++)
	{
		for(unsigned int uj=0;uj<2; uj++)
		{
			valid[ui][uj] = b.valid[ui][uj];
			bounds[ui][uj] = b.bounds[ui][uj];

		}
	}

	return *this;
}

std::ostream &operator<<(std::ostream &stream, const BoundCube& b)
{
	stream << "Bounds :Low (";
	stream << b.bounds[0][0] << ",";
	stream << b.bounds[1][0] << ",";
	stream << b.bounds[2][0] << ") , High (";
	
	stream << b.bounds[0][1] << ",";
	stream << b.bounds[1][1] << ",";
	stream << b.bounds[2][1] << ")" << std::endl;
	
	
	stream << "Bounds Valid: Low (";
	stream << b.valid[0][0] << ",";
	stream << b.valid[1][0] << ",";
	stream << b.valid[2][0] << ") , High (";
	
	stream << b.valid[0][1] << ",";
	stream << b.valid[1][1] << ",";
	stream << b.valid[2][1] << ")" << std::endl;

	return stream;
}

bool getFilesize(const char *fname, size_t  &size)
{
	std::ifstream f(fname,std::ios::binary);

	if(!f)
		return false;

	f.seekg(0,std::ios::end);

	size = f.tellg();

	return true;
}

void UniqueIDHandler::clear()
{
	idList.clear();
}

unsigned int UniqueIDHandler::getPos(unsigned int id) const
{

	for(list<std::pair<unsigned int, unsigned int> >::const_iterator it=idList.begin();
			it!=idList.end(); ++it)
	{
		if(id == it->second)
			return it->first;
	}
	ASSERT(false);
	return 0;
}

void UniqueIDHandler::killByPos(unsigned int pos)
{
	for(list<std::pair<unsigned int, unsigned int> >::iterator it=idList.begin();
			it!=idList.end(); ++it)
	{
		if(pos  == it->first)
		{
			idList.erase(it);
			break;
		}
	}

	//Decrement the items, which were further along, in order to maintain the mapping	
	for(list<std::pair<unsigned int, unsigned int> >::iterator it=idList.begin();
			it!=idList.end(); ++it)
	{
		if( it->first > pos)
			it->first--;
	}
}

unsigned int UniqueIDHandler::getId(unsigned int pos) const
{
	for(list<std::pair<unsigned int, unsigned int> >::const_iterator it=idList.begin();
			it!=idList.end(); ++it)
	{
		if(pos == it->first)
			return it->second;
	}

	ASSERT(false);
	return 0;
}

unsigned int UniqueIDHandler::genId(unsigned int pos)
{
	
	//Look for each element number as a unique value in turn
	//This is guaranteed to return by the pigeonhole principle (we are testing 
	//a target set (note <=)).
	for(unsigned int ui=0;ui<=idList.size(); ui++)
	{
		bool idTaken;
		idTaken=false;
		for(list<std::pair<unsigned int, unsigned int> >::iterator it=idList.begin();
				it!=idList.end(); ++it)
		{
			if(ui == it->second)
			{
				idTaken=true;
				break;
			}
		}

		if(!idTaken)
		{
			idList.push_back(std::make_pair(pos,ui));
			return ui;
		}
	}

	ASSERT(false);
	return 0;
}

void UniqueIDHandler::getIds(std::vector<unsigned int> &idVec) const
{
	//most wordy way of saying "spin through list" ever.
	for(list<std::pair<unsigned int, unsigned int> >::const_iterator it=idList.begin();
			it!=idList.end(); ++it)
		idVec.push_back(it->second);
}



#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	#include <windows.h>
	//Windows.h is a nasty name clashing horrible thing.
	//Put it last to avoid clashing with std:: stuff (eg max & min)

#endif

// Total ram in MB
int getTotalRAM()
{
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    	int ret;
	MEMORYSTATUS MemStat;

	// Zero structure
	memset(&MemStat, 0, sizeof(MemStat));

	// Get RAM snapshot
	::GlobalMemoryStatus(&MemStat);
	ret= MemStat.dwTotalPhys / (1024*1024);
	return ret;
#elif __APPLE__ || __FreeBSD__

    	int ret;
	uint64_t mem;
	size_t len = sizeof(mem);

	sysctlbyname("hw.physmem", &mem, &len, NULL, 0);

	ret = (int)(mem/(1024*1024));
	return ret;
#elif __linux__
	struct sysinfo sysInf;
	sysinfo(&sysInf);
	return ((size_t)(sysInf.totalram)*(size_t)(sysInf.mem_unit)/(1024*1024));
#else
	#error Unknown platform, no getTotalRAM function defined.
#endif
}

size_t getAvailRAM()
{
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)   
	int ret ;
	MEMORYSTATUS MemStat;
	// Zero structure
	memset(&MemStat, 0, sizeof(MemStat));

	// Get RAM snapshot
	::GlobalMemoryStatus(&MemStat);
	ret= MemStat.dwAvailPhys / (1024*1024);
	return ret;

#elif __APPLE__ || __FreeBSD__
	int ret ;
	uint64_t        memsize;
	
	size_t                  pagesize;
	mach_port_t             sls_port = mach_host_self();
	mach_msg_type_number_t  vmCount = HOST_VM_INFO_COUNT;
	vm_statistics_data_t    vm;
	kern_return_t           error;
	
	error = host_statistics(sls_port , HOST_VM_INFO , (host_info_t) &vm, &vmCount);
	pagesize = (unsigned long)sysconf(_SC_PAGESIZE);
	memsize = (vm.free_count + vm.inactive_count) * pagesize;//(vm.wire_count + vm.active_count + vm.inactive_count + vm.free_count + vm.zero_fill_count ) * pagesize;
	ret = (size_t)(memsize/(1024*1024));
	return ret;
#elif __linux__
	struct sysinfo sysInf;
	sysinfo(&sysInf);

	return ((size_t)(sysInf.freeram + sysInf.bufferram)*(size_t)(sysInf.mem_unit)/(1024*1024));
#else
	#error Unknown platform, no getAvailRAM function defined.
#endif
}

bool strhas(const char *cpTest, const char *cpPossible)
{
	while(*cpTest)
	{
		const char *search;
		search=cpPossible;
		while(*search)
		{
			if(*search == *cpTest)
				return true;
			search++;
		}
		cpTest++;
	}

	return false;
}

//A routine for loading numeric data from a text file
unsigned int loadTextData(const char *cpFilename, vector<vector<float> > &dataVec,vector<string> &headerVec, const char *delim)
{
	const unsigned int BUFFER_SIZE=4096;
	char inBuffer[BUFFER_SIZE];
	
	unsigned int num_fields=0;


#if !defined(WIN32) && !defined(WIN64)
	if(isNotDirectory(cpFilename) == false)
		return ERR_FILE_OPEN;
#endif

	dataVec.clear();
	//Open a file in text mode
	std::ifstream CFile(cpFilename);

	if(!CFile)
		return ERR_FILE_OPEN;

	//Drop the headers, if any
	string str;
	vector<string> strVec;	
	bool atHeader=true;

	vector<string> prevStrs;
	while(CFile.good() && !CFile.eof() && atHeader)
	{
		//Grab a line from the file
		CFile.getline(inBuffer,BUFFER_SIZE);

		if(!CFile.good())
			return ERR_FILE_FORMAT;

		prevStrs=strVec;
		//Split the strings around the deliminator c
		splitStrsRef(inBuffer,delim,strVec);		
		stripZeroEntries(strVec);
		
	
		//Skip blank lines or lines that are only spaces
		if(strVec.empty())
			continue;

		num_fields = strVec.size();
		dataVec.resize(num_fields);		
		//Check to see if we are in the header
		if(atHeader)
		{
			//If we have the right number of fields 
			//we might be not in the header anymore
			if(num_fields >= 1 && strVec[0].size())
			{
				float f;
				//Assume we are not reading the header
				atHeader=false;

				vector<float> values;
				//Confirm by checking all values
				for(unsigned int ui=0; ui<num_fields; ui++)
				{
	
					//stream_cast will fail in the case "1 2" if there
					//is a tab delimiter specified. Check for only 02,3	
					if(strVec[ui].find_first_not_of("0123456789.Ee+-") 
										!= string::npos)
					{
						atHeader=true;
						break;
					}
					//If any cast fails
					//we are in the header
					if(stream_cast(f,strVec[ui]))
					{
						atHeader=true;
						break;
					}

					values.push_back(f);
				}

				if(!atHeader)
					break;
			}
		}

	}

	//Drop the blank bits from the field
	stripZeroEntries(prevStrs);
	//switch this out as being the header
	if(prevStrs.size() == num_fields)
		std::swap(headerVec,prevStrs);

	if(atHeader)
	{
		//re-wind back to the beginning of the file
		//as we didn't find a header/data split
		CFile.clear(); // clear EOF bit
		CFile.seekg(0,std::ios::beg);
		CFile.getline(inBuffer,BUFFER_SIZE);
		
		
		splitStrsRef(inBuffer,delim,strVec);	
		stripZeroEntries(strVec);
		num_fields=strVec.size();

	}

	float f;
	std::stringstream ss;
	while(CFile.good() && !CFile.eof())
	{
		if(strhas(inBuffer,"0123456789"))
		{
			//Split the strings around the tab char
			splitStrsRef(inBuffer,delim,strVec);	
			stripZeroEntries(strVec);
			
			//Check the number of fields	
			//=========
			if(strVec.size() != num_fields)
				return ERR_FILE_NUM_FIELDS;	
	

			for(unsigned int ui=0; ui<num_fields; ui++)
			{	
				ss.clear();
				ss.str(strVec[ui]);
				ss >> f;
				if(ss.fail())
					return ERR_FILE_FORMAT;
				dataVec[ui].push_back(f);
			
			}
			//=========
			
		}
		//Grab a line from the file
		CFile.getline(inBuffer,BUFFER_SIZE);
		
		if(!CFile.good() && !CFile.eof())
			return ERR_FILE_FORMAT;
	}

	return 0;
}

unsigned int loadTextStringData(const char *cpFilename, vector<vector<string> > &dataVec,const char *delim)
{
	const unsigned int BUFFER_SIZE=4096;
	
#if !defined(WIN32) && !defined(WIN64)
	if(isNotDirectory(cpFilename) == false)
		return ERR_FILE_OPEN;
#endif
	
	//Open a file in text mode
	std::ifstream CFile(cpFilename);

	if(!CFile)
		return ERR_FILE_OPEN;

	dataVec.clear();

	char *inBuffer= new char[BUFFER_SIZE];
	//Grab a line from the file
	CFile.getline(inBuffer,BUFFER_SIZE);
	while(!CFile.eof())
	{
		vector<string> strVec;
		strVec.clear();
		
		//Split the strings around the tab char
		splitStrsRef(inBuffer,delim,strVec);	
		stripZeroEntries(strVec);
		
		//Check the number of fields	
		//=========
		if(strVec.size())
			dataVec.push_back(strVec);
		//=========
			
		//Grab a line from the file
		CFile.getline(inBuffer,BUFFER_SIZE);
		
		if(!CFile.good() && !CFile.eof())
		{
			delete[] inBuffer;
			return ERR_FILE_FORMAT;
		}
	}

	delete[] inBuffer;
	return 0;
}


#if !defined(__WIN32__) && !defined(__WIN64)
	
bool isNotDirectory(const char *filename)
{
	struct stat statbuf;

	if(stat(filename,&statbuf) == -1)
		return false;

	return (statbuf.st_mode !=S_IFDIR);
}

bool rmFile(const std::string &filename)
{
	return remove(filename.c_str()) == 0;
}
#elif defined(__WIN32) || defined(__WIN64)
bool rmFile(const std::string &filename)
{ 
	return DeleteFile((const wchar_t*)filename.c_str()) == 0;
}
#endif

#ifdef DEBUG
bool isValidXML(const char *filename)
{
	//Debug check to ensure we have written a valid xml file
	std::string command;
	unsigned int result;
	
//Windows doesn't really have  a /dev/null device, rather it has a reserved file name "NUL" or "nul"
//http://technet.microsoft.com/en-gb/library/cc961816.aspx
#if defined(WIN32) || defined(WIN64)
	command = std::string("xmllint --version > NUL 2> NUL");
#else
	command = std::string("xmllint --version >/dev/null 2>/dev/null");
#endif
	result=system(command.c_str());
	if(!result)
	{
	//Windows' shell handles escapes differently, workaround
	#if defined(WIN32) || defined(WIN64)
		command = std::string("xmllint --noout \"") + filename + string("\"");
	#else
		command = std::string("xmllint --noout \'") + filename + string("\'");
	#endif
		result=system(command.c_str());
		return result ==0;
	}

	//Debug check ineffective
	WARN(!result,"xmllint not installed in system PATH, cannot perform debug check")
	return true;
}
#endif
