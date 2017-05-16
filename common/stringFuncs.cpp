/*
 *	stringFuncs.cpp - string manipulation routines
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

#include "common/stringFuncs.h"

#include "common/basics.h"

#include <sys/time.h>

using std::string;
using std::vector;

std::string getMaxVerStr(const std::vector<std::string> &verStrings)
{
	std::vector<std::pair<size_t,std::vector<unsigned int> > > verNum;
	std::vector<unsigned int> thisVer;


	//break string up into numeric components
	for(unsigned int ui=0;ui<verStrings.size();ui++)
	{
		std::vector<std::string> strVerNum;
		strVerNum.clear();

		// period or hyphen are valid version number separators
		splitStrsRef(verStrings[ui].c_str(),".-",strVerNum);

		//Check to see if we can interpret the values
		for(unsigned int uj=0;uj<strVerNum.size();uj++)
		{
			int i;
			
			//Try to cast the string (returns true on failure)
			if(!stream_cast(i,strVerNum[uj]))
				thisVer.push_back(i);

		}

		if(thisVer.size())
		{
			verNum.push_back(make_pair(ui,thisVer));
			thisVer.clear();
		}
	}
		
	
	if(verNum.empty())
		return std::string("");

	//OK, so now we have an integral list.
	//Find the minimal element in each set until we 
	//knock out all elements but one
	size_t maxVerLen=0;
	for(unsigned int ui=0;ui<verNum.size();ui++)
		maxVerLen=std::max(maxVerLen,verNum[ui].second.size());

		
	unsigned int pos=0;
	while(pos<maxVerLen && verNum.size() > 1)
	{
		unsigned int thisMax;
		thisMax=0;

		for(unsigned int ui=0;ui<verNum.size();ui++)
		{
			//If the version string has enough digits, check to see if it is the maximum for this digit
			if(pos < verNum[ui].second.size() )
				thisMax=std::max(thisMax,verNum[ui].second[pos]);
		}

		//Kill off any version numbers that were not 
		//the max value, or had insufficient numbers
		for(unsigned int ui=verNum.size();ui;)
		{
			ui--;

			if(verNum[ui].second.size() <=pos || 
				verNum[ui].second[pos] < thisMax )
			{
				std::swap(verNum[ui],verNum.back());
				verNum.pop_back();
			}
		}

		//move to next number
		pos++;
	}


	//Should contain at least one version (ie the maximum, or multiple copies thereof)
	ASSERT(verNum.size());


	return verStrings[verNum[0].first];
}

bool isVersionNumberString(const std::string &s)
{
	for(unsigned int ui=0;ui<s.size();ui++)
	{
		if(!isdigit(s[ui]) )
		{

			if(s[ui] !='.' || ui ==0 || ui ==s.size())
				return false;
		}
	}

	return true;
}


bool boolStrDec(const std::string &s, bool &b)
{
	std::string tmp;
	tmp=stripWhite(s);

	if(tmp == "0")
		b=false;
	else if(tmp == "1")
		b=true;
	else 
		return false; // Failed to decode

	return true;
}


void splitFileData(const std::string &fileWithPath, std::string &path, std::string &basename, std::string &extension)
{
	path.clear(); basename.clear(); extension.clear();

	if(fileWithPath.empty())
		return;

	basename= onlyFilename(fileWithPath);
	path = onlyDir(fileWithPath);

	unsigned int extPosition=(unsigned int)-1;
	for(unsigned int ui=basename.size();ui--;)
	{
		if(basename[ui] =='.')
		{
			extPosition=ui;
			break;
		}
		
	} 

	if(extPosition != (unsigned int)-1)
	{
		extension = basename.substr(extPosition+1,basename.size()-(extPosition+1));
		basename = basename.substr(0,extPosition);
	}
}
std::string onlyFilename( const std::string& path) 
{
#if defined(_WIN32) || defined(_WIN64)
	//windows uses "\" as path sep, just to be different
	return path.substr( path.find_last_of( '\\' ) +1 );
#else
	//The entire world, including the interwebs, uses  "/" as the path sep.
	return path.substr( path.find_last_of( '/' ) +1 );
#endif
}

std::string onlyDir( const std::string& path) 
{
#if defined(_WIN32) || defined(_WIN64)
	//windows uses "\" as path sep, just to be different
	return path.substr(0, path.find_last_of( '\\' ) +1 );
#else
	//The entire world, including the interwebs, uses  "/" as the path sep.
	return path.substr(0, path.find_last_of( '/' ) +1 );
#endif
}

std::string convertFileStringToCanonical(const std::string &s)
{
	//We call unix the "canonical" format. 
	//otherwise we substitute "\"s for "/"s
#if (__WIN32) || (__WIN64)
	string r;
	for(unsigned int ui=0;ui<s.size();ui++)
	{
		if(s[ui] == '\\')
			r+="/";
		else
			r+=s[ui];
	}
	return r;
#else
	return s;
#endif
}

std::string convertFileStringToNative(const std::string &s)
{
	//We call unix the "canonical" format. 
	//otherwise we substitute "/"s for "\"s
#if (__WIN32) || (__WIN64)
	string r;
	for(unsigned int ui=0;ui<s.size();ui++)
	{
		if(s[ui] == '/')
			r+="\\";
		else
			r+=s[ui];
	}
	return r;
#else
	return s;
#endif
}

bool genRandomFilename(std::string &s,bool seedRand) 
{
	if(seedRand)
		srand(time(NULL));
	const size_t FILELEN=15;

	//some valid chars for generating random strings
	static const char validChars[] =
	    "0123456789_"
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz";

	const size_t MAX_RETRY=10;
	size_t retry=0;	
	s.resize(FILELEN);

	//try to open the file for read. if it works, it means 
	// we had the file, so we need to try again :/
	std::ifstream f;
	do
	{
		f.close();
		for (size_t i = 0; i < FILELEN; i++) 
			s[i] = validChars[rand() % (sizeof(validChars) - 1)];

		f.open(s.c_str());
		retry++;
	}
	while(!f && (retry < MAX_RETRY) );

	return f.good();

}


void nullifyMarker(char *buffer, char marker)
{
	while(*(buffer))
	{
		if(*buffer == marker)
		{
			*buffer='\0';
			break;
		}
		buffer++;
	}
}

void ucharToHexStr(unsigned char c, std::string &s)
{
	s="  ";
	char h1,h2;

	h1 = c/16;
	h2 = c%16;

	if(h1 < 10)
	{
		s[0]=(h1+'0');
	}
	else
		s[0]=((h1-10)+'a');

	if(h2 < 10)
	{
		s[1]=(h2+'0');
	}
	else
		s[1]=((h2-10)+'a');
	
}

void hexStrToUChar(const std::string &s, unsigned char &c)
{
	ASSERT(s.size() ==2);

	ASSERT((s[0] >= '0' && s[0] <= '9') ||
	      		(s[0] >= 'a' && s[0] <= 'f'));
	ASSERT((s[1] >= '0' && s[1] <= '9') ||
			(s[1] >= 'a' && s[1] <= 'f'));

	int high,low;
	if(s[0] <= '9' && s[0] >='0')
		high = s[0]-(int)'0';
	else
		high = s[0] -(int)'a' + 10;	
	
	if(s[1] <= '9' && s[1] >='0')
		low = s[1]-(int)'0';
	else
		low = s[1] -(int)'a' + 10;	

	c = 16*high + low;
}

std::string digitString(unsigned int thisDigit, unsigned int maxDigit)
{
	std::string s,thisStr;
	stream_cast(thisStr,thisDigit);

	stream_cast(s,maxDigit);
	for(unsigned int ui=0;ui<s.size();ui++)
		s[ui]='0';


	s=s.substr(0,s.size()-thisStr.size());	

	return  s+thisStr;
}

std::string choiceString(std::vector<std::pair<unsigned int, std::string> > comboString, 
									unsigned int curChoice)
{
	ASSERT(curChoice < comboString.size())

	string s,sTmp;
	stream_cast(sTmp,curChoice);
	s=sTmp + string(":");
	for(unsigned int ui=0;ui<comboString.size();ui++)
	{
		//Should not contain a comma or vert. bar in user string
		ASSERT(comboString[ui].second.find(",") == string::npos);
		ASSERT(comboString[ui].second.find("|") == string::npos);

		stream_cast(sTmp,comboString[ui].first);
		if(ui < comboString.size()-1)
			s+=sTmp + string("|") + comboString[ui].second + string(",");
		else
			s+=sTmp + string("|") + comboString[ui].second;

	}

	return s;
}


//Strip "whitespace"
std::string stripWhite(const std::string &str)
{
	return stripChars(str,"\f\n\r\t ");
}

std::string stripChars(const std::string &str, const char *chars)
{
	using std::string;

	size_t start;
	size_t end;
	if(!str.size())
	      return str;

 	start = str.find_first_not_of(chars);
	end = str.find_last_not_of(chars);
	if(start == string::npos) 
		return string("");
	else
		return string(str, start, 
				end-start+1);
}

void stripZeroEntries(std::vector<std::string> &sVec)
{
	//Create a truncated vector and reserve mem.
	std::vector<std::string> tVec;
	tVec.reserve(sVec.size());
	std::string s;
	for(unsigned int ui=0;ui<sVec.size(); ui++)
	{
		//Only copy entries with a size.
		if(sVec[ui].size())
		{
			//Push dummy string and swap,
			// to avoid copy
			tVec.push_back(s);
			tVec.back().swap(sVec[ui]);
		}
	}
	//Swap out the truncated vector with the source
	tVec.swap(sVec);
}	

std::string lowercase(std::string s)
{
	for(unsigned int ui=0;ui<s.size();ui++)
	{
		if(isascii(s[ui]) && isupper(s[ui]))
			s[ui] = tolower(s[ui]);
	}
	return s;
}

std::string uppercase(std::string s)
{
	for(unsigned int ui=0;ui<s.size();ui++)
	{
		if(isascii(s[ui]) && islower(s[ui]))
			s[ui] = toupper(s[ui]);
	}
	return s;
}

//Split strings around a delimiter
void splitStrsRef(const char *cpStr, const char delim,std::vector<string> &v )
{
	const char *thisMark, *lastMark;
	string str;
	v.clear();

	//check for null string
	if(!*cpStr)
		return;
	thisMark=cpStr; 
	lastMark=cpStr;
	while(*thisMark)
	{
		if(*thisMark==delim)
		{
			str.assign(lastMark,thisMark-lastMark);
			v.push_back(str);
		
			thisMark++;
			lastMark=thisMark;
		}
		else
			thisMark++;
	}

	if(thisMark!=lastMark)
	{
		str.assign(lastMark,thisMark-lastMark);
		v.push_back(str);
	}	
		
}

//Split strings around any of a string of delimiters
void splitStrsRef(const char *cpStr, const char *delim,std::vector<string> &v )
{
	const char *thisMark, *lastMark;
	string str;
	v.clear();

	//check for null string
	if(!(*cpStr))
		return;
	thisMark=cpStr; 
	lastMark=cpStr;
	while(*thisMark)
	{
		//Loop over possible delimiters to determine if this char is a delimiter
		bool isDelim;
		const char *tmp;
		tmp=delim;
		isDelim=false;
		while(*tmp)
		{
			if(*thisMark==*tmp)
			{
				isDelim=true;
				break;
			}
			tmp++;
		}
		
		if(isDelim)
		{
			str.assign(lastMark,thisMark-lastMark);
			v.push_back(str);
		
			thisMark++;
			lastMark=thisMark;
		}
		else
			thisMark++;
	}

	if(thisMark!=lastMark)
	{
		str.assign(lastMark,thisMark-lastMark);
		v.push_back(str);
	}	
		
}

//!Returns Choice ID from string (see choiceString(...) for string format)
//FIXME: Does not work if the choicestring starts from a number other than zero...
std::string getActiveChoice(const std::string &choiceString)
{
	size_t colonPos;
	colonPos = choiceString.find(":");
	ASSERT(colonPos!=string::npos);

	//Extract active selection
	string tmpStr;	
	tmpStr=choiceString.substr(0,colonPos);
	unsigned int activeChoice;
	stream_cast(activeChoice,tmpStr);

	//Convert ID1|string 1, .... IDN|string n to vectors
	std::string s;
	s=choiceString.substr(colonPos,choiceString.size()-colonPos);
	vector<string> choices;
	splitStrsRef(s.c_str(),',',choices);

	ASSERT(activeChoice < choices.size());
	tmpStr = choices[activeChoice];

	return tmpStr.substr(tmpStr.find("|")+1,tmpStr.size()-1);
}

void choiceStringToVector(const std::string &choiceString,
		std::vector<std::string> &choices, unsigned int &selected)
{
	ASSERT(isMaybeChoiceString(choiceString));

	//Convert ID1|string 1, .... IDN|string n to vectors,
	// stripping off the ID value
	size_t colonPos;
	colonPos = choiceString.find(":");

	std::string s;
	s=choiceString.substr(colonPos,choiceString.size()-colonPos);
	splitStrsRef(s.c_str(),',',choices);

	for(size_t ui=0;ui<choices.size();ui++)
	{
		choices[ui]=choices[ui].substr(
			choices[ui].find("|")+1,choices[ui].size()-1);

	}

	string tmpStr;	
	tmpStr=choiceString.substr(0,colonPos);
	stream_cast(selected,tmpStr);

	ASSERT(selected < choices.size());
}


#ifdef DEBUG
//Returns false if string fails heuristic test for being a choice string
// failure indicates definitely not a choice string, success guarantees nothing
bool isMaybeChoiceString(const std::string &s)
{
	
	if(s.size() < 3)
		return false;

	if(!isdigit(s[0]))
		return false;

	if(s[1] == '|')
		return false;

	size_t colonPos;
	colonPos = s.find(":");

	if(colonPos == std::string::npos ||
		colonPos == s.size() || colonPos < 1)
		return false;
	

	return true;
}

bool testStringFuncs()
{
	TEST(isMaybeChoiceString("1:0|Box only,1|Tick,2|Dimension"),"choice string");
	
	//Test getMaxVerStr
	{
	vector<string> verStrs;

	verStrs.push_back("0.0.9");
	verStrs.push_back("0.0.10");

	TEST(getMaxVerStr(verStrs) == "0.0.10","version string maximum testing");

	verStrs.clear();
	
	verStrs.push_back("0.0.9");
	verStrs.push_back("0.0.9");
	TEST(getMaxVerStr(verStrs) == "0.0.9","version string maximum testing");

	
	verStrs.push_back("0.0.9");
	verStrs.push_back("0.0.blah");
	TEST(getMaxVerStr(verStrs) == "0.0.9","version string maximum testing");
	}

#if !(defined(__WIN32) || defined(__WIN64))
	{
	string filename;
	filename="/path/blah.dir/basefile.test.ext";
	string a,b,c;
	splitFileData(filename, a,b,c);

	TEST(a == "/path/blah.dir/","path split");	
	TEST(b == "basefile.test","basename split");	
	TEST(c == "ext","extension split");	
	}

#endif


	return true;
}


#endif
