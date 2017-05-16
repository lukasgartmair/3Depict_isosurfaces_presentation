/*
 * APTRanges.cpp - Atom probe rangefile class 
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

#include "APTRanges.h"

#include "ionhit.h"

#include "../../common/constants.h"
#include "../../common/stringFuncs.h"
#include "../../common/translation.h"

#include <set>
#include <map>
#include <fstream>
#include <numeric>
#include <cstring>

using std::string;
using std::vector;
using std::pair;
using std::make_pair;
using std::map;
using std::set;
using std::accumulate;

//Arbitrary maximum range file line size
const size_t MAX_LINE_SIZE = 16536;
//Arbitrary maximum range file size, in bytes
const size_t MAX_RANGEFILE_SIZE = 20*1024*1024;
const char *rangeErrStrings[] = 
{
	"",
	NTRANS("Error opening file, check name and permissions."),
	NTRANS("Error interpreting range file header, expecting ion count and range count, respectively."),
	NTRANS("Range file appears to be empty, check file is a proper range file and is not empty."),
	NTRANS("Error reading the long name for ion."),
	NTRANS("Error reading the short name for ion."),
	NTRANS("Error reading colour data in the file, expecting 3 decimal values, space separated."),
	NTRANS("Tried skipping to table separator line (line with dashes), but did not find it."),	
	NTRANS("Number of ions in the table header did not match the number specified at the start of the file"),
	NTRANS("Unexpected failure whilst trying to skip over range lead-in data (bit before range start value)"),
	NTRANS("Range table had an incorrect number of entries, should be 2 or 3 + number of ranges"),
	NTRANS("Unable to read range start and end values"),
	NTRANS("Unable to read range table entry"),
	NTRANS("Error reading file, unexpected format, are you sure it is a proper range file?"),
	NTRANS("Too many ranges appeared to have range entries with no usable data (eg, all blank)"),
	NTRANS("Range file appears to contain malformed data, check things like start and ends of m/c are not equal or flipped."),
	NTRANS("Range file appears to be inconsistent (eg, overlapping ranges)"),
	NTRANS("No ion name mapping found  for multiple ion."),
	NTRANS("Polyatomic extension range matches multiple masses in first section"),
	NTRANS("Range file is exceedingly large. Refusing to open"),
};

const char *RANGE_EXTS[] = { "rng",
			  "env",
			  "rng",
			  "rrng",
				""};

//List of symbols in the periodic table
const char *elementList[] = { 
"H", "He", "Li", "Be",  "B", "C", "N",  "O",  "F", "Ne",

"Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar", 

"K", "Ca", "Sc","Ti",  "V", "Cr",  "Mn", "Fe",  "Co", "Ni",  
   "Cu",  "Zn", "Ga", "Ge", "As", "Se",  "Br", "Kr",

"Rb", "Sr", "Y", "Zr", "Nb", "Mo", "Tc", "Ru", "Rh", "Pd",
   "Ag",  "Cd", "In", "Sn", "Sb", "Te",  "I", "Xe",

"Cs",  "Ba", "La",  "Ce", "Pr",  "Nd", "Pm", "Sm",  "Eu", "Gd",
    "Tb",  "Dy",  "Ho", "Er",  "Tm", "Yb", "Lu",  "Hf", "Ta",
    "W",  "Re", "Os",  "Ir", "Pt", "Au", "Hg",  "Tl", "Pb",
     "Bi",  "Po", "At", "Rn",

"Fr", "Ra",  "Ac", "Th",  "Pa", "U",  "Np", "Pu",  "Am", "Cm",
    "Bk", "Cf",  "Es", "Fm",  "Md",  "No",  "Lr",  "Rf",  "Db",
    "Sg",  "Bh",  "Hs",  "Mt",  "Ds",  "Rg",  "Cn",  "Uut",  "Fl",
     "Uup",  "Lv",  "Uus", "Uuo", ""
};	


//Known issues - will not decompose brackets, eg Fe(OH)2
bool RangeFile::decomposeIonNames(const std::string &name,
		std::vector<pair<string,size_t> > &fragments)
{
	size_t lastMarker=0;
	size_t digitMarker=0;

	if(!name.size())
		return true;

	//Atomic naming systems use uppercase ascii
	// letters, like "A" in Au, or Ag as delimiters.
	//
	// numerals are multipliers, and are forbidden
	// for the first char...
	if(!isascii(name[0]) || 
		isdigit(name[0]) || islower(name[0]))
		return false;

	//true - was last, or now am on ion name
	//false - am still on multiplier
	int nameMode=true;
	for(size_t ui=1;ui<name.size();ui++)
	{
		if(!isascii(name[ui]))
			return false;
		
		if(nameMode)
		{	
			//If we hit a digit, 
			//this means that we 
			//are now on a multiplier
			if(isdigit(name[ui]))
			{
				digitMarker=ui;
				nameMode=false;
				continue;
			}

			if(isupper(name[ui]))
			{
				//Looks like we hit another
				// ion name, without hitting any
				// specification for the number.
				// This means unitary spec.
				std::string s;
				s=name.substr(lastMarker,ui-lastMarker);
				fragments.push_back(make_pair(s,1));
				lastMarker=ui;
			}
		}
		else
		{
			//OK, we still have a digit.
			// keep moving
			if(isdigit(name[ui]))
				continue;				

			if(isalpha(name[ui]))
			{
				//OK, this looks like a new ion
				//but we need to record multiplicity
				std::string s,sDigit;
				s=name.substr(lastMarker,digitMarker-lastMarker);

				sDigit=name.substr(digitMarker,ui-digitMarker);
				
				size_t multiplicity;
				stream_cast(multiplicity,sDigit);

				fragments.push_back(make_pair(s,multiplicity));
				
				lastMarker=ui;
				nameMode=true;
			}

		}
	}

	if(nameMode)
	{
		//Hit end of string.
		//record fragment
		std::string s;
		s=name.substr(lastMarker,name.size()-lastMarker);
		fragments.push_back(make_pair(s,1));
	}
	else
	{
		std::string s,sDigit;
		s=name.substr(lastMarker,digitMarker-lastMarker);
		sDigit=name.substr(digitMarker,name.size()-digitMarker);
		
		size_t multiplicity;
		stream_cast(multiplicity,sDigit);
		
		fragments.push_back(make_pair(s,multiplicity));
	}


	//Spin through dataset and collect the non-unique fragment
	vector<bool> toKill(fragments.size(),false);
	for(size_t ui=0;ui<fragments.size();ui++)
	{
		//skip empty fragments
		if(fragments[ui].first.empty())
			continue;	

		for(size_t uj=ui+1;uj<fragments.size();uj++)
		{
			//skip empty fragments
			if(fragments[uj].first.empty())
				continue;	

			//Collect fragment multiplicities if they have the same name
			if(fragments[uj].first == fragments[ui].first)
			{
				fragments[ui].second+=fragments[uj].second;
				fragments[uj].first=""; //Zero out name so we don't hit it

				toKill[uj]=true;

			}
		}
	}

	vectorMultiErase(fragments,toKill);
	
	return true;
}


//ENV files sometimes have charge state information in the ion
// names, use this function as a helper to strip it out
std::string RangeFile::envDropChargeState(const std::string &strName)
{
	std::string res=strName;
	if(strName[strName.size()-1] == '+')
	{
		size_t chargeStateOffset;
		chargeStateOffset=strName.find_last_of("_");
		if(chargeStateOffset != std::string::npos)
			res=strName.substr(0,chargeStateOffset);
	}

	return res;
}

//Given the name-frequency pairing vector, see if there
// is a match in the map of composed names
bool matchComposedName(const std::map<string,size_t> &composedNames,
	       
		const vector<pair<string,size_t> > &namesToFind, size_t &matchOffset)
{
	//Decomposition of composed names. 
	std::vector<vector<pair<string,size_t> > > fragmentVec;

	//break each composed name into a vector of decomposed fragments
	// and the multiplicity
	//of that fragment (eg, AuHg2 would become  { Au ,1} {Hg,2})
	fragmentVec.reserve(composedNames.size());
	for(std::map<string,size_t>::const_iterator it=composedNames.begin();
			it!=composedNames.end();++it)
	{
		vector<pair<string,size_t> > frags;
		if(!RangeFile::decomposeIonNames(it->first,frags))
			frags.clear();

			
		fragmentVec.push_back(frags);

		frags.clear();
	}


	//Try to match all fragments in "namesToFind" (name-frequency pairings) 
	//in the master list of fragments of 
	// which consists of the decomposed composed names (fragmentVec entries)

	//If the decomposed fragments wholly constitute the
	//master list, then thats good, and we have a match.
	//Note that the master list will not necessarily be in
	//the same order as the fragment list
	//match tally for fragments
	

	vector<bool> excludedMatch;
	excludedMatch.resize(fragmentVec.size(),false);

	for(size_t uj=0;uj<namesToFind.size();uj++)
	{
		pair<string,size_t> curFrag;
		curFrag=namesToFind[uj];
		for(size_t ui=0;ui<fragmentVec.size();ui++)
		{
			//Was disqualified from matching before
			if(excludedMatch[ui])
				continue;

			//If we cannot find this particular fragment, then 
			// this is excluded from future searches
			if(std::find(fragmentVec[ui].begin(),
				fragmentVec[ui].end(),curFrag) == fragmentVec[ui].end())
				excludedMatch[ui]=true;
		}
	}
	//-------

	//Scan through the candidate matches to find if there is
	// a single unique candidate
	matchOffset=-1;
	for(size_t ui=0;ui<fragmentVec.size();ui++)
	{
		if(!excludedMatch[ui])
		{
			//Check for bijection in the mapping. Currently
			// we know that fragmentVec is a superset of 
			// namesToFind, but we don't know it matches -
			// it could exceed it, for example.
			bool doesMatch=true;
			for(size_t uj=0;uj<fragmentVec[ui].size();uj++)
			{
				if(std::find(namesToFind.begin(),
					namesToFind.end(),fragmentVec[ui][uj]) ==
							namesToFind.end())
				{
					doesMatch=false;
					break;

				}
			}

			//Duplicate match
			if(doesMatch)
			{
				if(matchOffset !=(size_t)-1)
					return false;
				else
				{
					//OK, we found a match.
					matchOffset=ui;
				}
			}

		}
	}

	//Return OK, iff we have exactly one match
	return (matchOffset !=(size_t) - 1);
}

RangeFile::RangeFile() : enforceConsistency(true), errState(0)
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(RANGE_EXTS)==RANGE_FORMAT_END_OF_ENUM+1);
}

const RangeFile &RangeFile::operator=(const RangeFile &oth)
{
	ionNames.clear();
	colours.clear();
	ranges.clear();
	ionIDs.clear();

		
	ionNames=oth.ionNames;
	colours=oth.colours;
	ranges=oth.ranges;
	ionIDs=oth.ionIDs;
	enforceConsistency=oth.enforceConsistency;
	errState=oth.errState;
	warnMessages=oth.warnMessages;

	return *this;
}

unsigned int RangeFile::write(std::ostream &f, size_t format) const
{

	using std::endl;
	ASSERT(colours.size() == ionNames.size());

	switch(format)
	{
		case RANGE_FORMAT_ORNL:
		{
			//File header
			f << ionNames.size() << " " ; 
			f << ranges.size() << std::endl;

			//Colour and longname data
			for(unsigned int ui=0;ui<ionNames.size() ; ui++)
			{
				f << ionNames[ui].second<< std::endl;
				f << ionNames[ui].first << " " << colours[ui].red << " " << colours[ui].green << " " << colours[ui].blue << std::endl;

			}	

			//Construct the table header
			f<< "-------------";
			for(unsigned int ui=0;ui<ionNames.size() ; ui++)
			{
				f << " " << ionNames[ui].first;
			}

			f << std::endl;
			//Construct the range table
			for(unsigned int ui=0;ui<ranges.size() ; ui++)
			{
				f << ". " << ranges[ui].first << " " << ranges[ui].second ;

				//Now put the "1" in the correct column
				for(unsigned int uj=0;uj<ionNames.size(); uj++)
				{
					if(uj == ionIDs[ui])
						f << " " << 1;
					else
						f << " " << 0;
				}

				f << std::endl;
				
			}
			break;
		}
		case RANGE_FORMAT_ENV:
		{
			//Comment indicating it came from this program
			f << "#" << PROGRAM_NAME << " " << PROGRAM_VERSION <<std::endl;
			
			//File header
			f << ionNames.size() << " " ; 
			f << ranges.size() << std::endl;

			//Colour and name data
			for(unsigned int ui=0;ui<ionNames.size() ; ui++)
			{
				f << ionNames[ui].first << " " << colours[ui].red << " " << colours[ui].green << " " << colours[ui].blue << std::endl;
			}	
			//Construct the range table
			for(unsigned int ui=0;ui<ranges.size() ; ui++)
			{
				f << ionNames[ionIDs[ui]].first << " " << ranges[ui].first 
					<< " " << ranges[ui].second << "    1.0 1.0" << std::endl;
			}
	
			break;
		}
		case RANGE_FORMAT_RRNG:
		{
	
			set<string> elementSet;

			{
			size_t offset=0;
			while(strlen(elementList[offset]))
			{
				elementSet.insert(elementList[offset]);
				offset++;
			}

			}

			f << "[Ions]" << endl;
			f << "Number=" << ionNames.size() << endl;
			
			for(size_t ui=0;ui<ionNames.size();ui++)
			{
				f << "Ion" << ui+1 <<"=" << ionNames[ui].first << endl;
			}
		
			f << "[Ranges] " << endl;
			f << "Number=" << ranges.size() << endl;



			for(size_t ui=0;ui<ranges.size();ui++)
			{
				ColourRGBA tmpRgba;
				tmpRgba.fromRGBf(colours[ui]);
				std::string colString;
				colString = tmpRgba.rgbString();
				//strip leading #
				colString=colString.substr(1);
				ASSERT(colString.size() == 6);

				//FIXME: This is incomplete. we need to break the species apart into its 
				// components, then decide to use the Element:count notation, or the Name:species notation
				string strName;
				strName=ionNames[ionIDs[ui]].first;
				if(elementSet.find(strName) != elementSet.end())
				{
					f << "Range" << ui+1 <<"=" 
					<< ranges[ui].first << " " << ranges[ui].second <<
					" " << strName << ":1" << 
					" Color:" << colString << endl;
				}
				else
				{
					//Difference is we have to use the "Name" parameter if not in set
					
					f << "Range" << ui+1 <<"=" 
					<< ranges[ui].first << " " << ranges[ui].second <<
					" " << "Name:" << strName << " " << strName << ":1" << 
					" Color:" << colString << endl;

				}
			}

			break;
		}
		
		default:
			ASSERT(false);
	}
	return 0;
}

unsigned int RangeFile::write(const char *filename,size_t format) const
{
	std::ofstream f(filename);
	if(!f)
		return 1;

	return write(f,format);
}

void RangeFile::clear()
{
	ionIDs.clear();
	warnMessages.clear();
	ionNames.clear();
	colours.clear();
	ranges.clear();
	
	errState=0;
}

unsigned int RangeFile::open(const char *rangeFilename, unsigned int fileFormat)
{
	size_t fileSize;
	getFilesize(rangeFilename,fileSize);

	if(fileSize > MAX_RANGEFILE_SIZE)
		return RANGE_ERR_FILESIZE;

	FILE *fpRange;
	fpRange=fopen(rangeFilename,"r");
	if (fpRange== NULL) 
	{
		errState = RANGE_ERR_OPEN;
		return errState;
	}


	pushLocale("C",LC_NUMERIC);
		
	size_t errCode;
	switch(fileFormat)
	{
		case RANGE_FORMAT_ORNL:
		{
			errCode=openRNG(fpRange);
			break;
		}
		//Cameca "ENV" file format.. I have a couple of examples, but nothing more.
		//There is no specification of this format publicly available
		case RANGE_FORMAT_ENV:
		{
			errCode=openENV(fpRange);
			break;
		}
		//Imago (now Cameca), "RRNG" file format. Again, neither standard 
		//nor formal specification exists (sigh). Purely implemented by example.
		case RANGE_FORMAT_RRNG:
			errCode=openRRNG(fpRange);
			break;
		//Cameca "double" rng format, in the wild as of mid 2013
		case RANGE_FORMAT_DBL_ORNL:
			errCode=openDoubleRNG(fpRange);
			break;
		default:
			ASSERT(false);
			fclose(fpRange);
			popLocale();
			return RANGE_ERR_FORMAT;
	}

	popLocale();
	fclose(fpRange);
	if(errCode)
	{
		errState=errCode;

		return errState;
	}

	//Run self consistency check on freshly loaded data
	if(!isSelfConsistent())
	{
		errState=RANGE_ERR_DATA_INCONSISTENT;
		return errState;
	}
	
	return 0;
}

bool RangeFile::openGuessFormat(const char *rangeFilename)
{

	//Check the filesize before attempting to guess the format
	size_t fileSize;
	getFilesize(rangeFilename,fileSize);
	if(fileSize > MAX_RANGEFILE_SIZE)
	{
		errState=RANGE_ERR_FILESIZE;
		return false;
	}
	//Try to auto-detect the filetype
	unsigned int assumedFileFormat;
	assumedFileFormat=detectFileType(rangeFilename);

	if(assumedFileFormat < RANGE_FORMAT_END_OF_ENUM)
	{
		if(!open(rangeFilename,assumedFileFormat))
			return true;
	}
	//Use the guessed format
	unsigned int errStateRestore;
	errStateRestore=errState;
	//If that failed, go to plan B-- Brute force.
	//try all readers
	bool openOK=false;

	for(unsigned int ui=0;ui<RANGE_FORMAT_END_OF_ENUM; ui++)
	{
		if(ui == assumedFileFormat)
			continue;

		if(!open(rangeFilename,ui))
		{
			assumedFileFormat=ui;
			openOK=true;
			break;
		}
	}

	if(!openOK)
	{
		//Restore the error state for the assumed file format
		errState=errStateRestore;
		return false;
	}

	return true;
}

unsigned int RangeFile::openDoubleRNG(FILE *fpRange)
{
	//Cameca modified range file format, as discussed here:
	// https://sourceforge.net/apps/phpbb/threedepict/viewtopic.php?f=1&t=25

	//Basically, the file is two range files back to back, with a single separator line
	// where the multiple ion names and colours and table are given in the second file

	clear();
	RangeFile tmpRange[2];

	unsigned int errCode;
	errCode=tmpRange[0].openRNG(fpRange);
	if(errCode)
		return errCode;

	//Spin forwards to the "polyatomic extension" line

		
	char *inBuffer = new char[MAX_LINE_SIZE];
	// skip over <LF> 
	char *ret;
	ret=fgets((char *)inBuffer, MAX_LINE_SIZE, fpRange); 

	while(ret && strlen(ret) < MAX_LINE_SIZE-1  && ret[0] != '-')
	{
		ret=fgets((char *)inBuffer, MAX_LINE_SIZE, fpRange); 
	}

	if(!ret || strlen(ret) >= MAX_LINE_SIZE -1)
	{
		delete[] inBuffer;
		return RANGE_ERR_FORMAT;
	}
	
	//Read the "polyatomic extension" line
	errCode=tmpRange[1].openRNG(fpRange);

	if(errCode)
	{
		delete[] inBuffer;
		return errCode;
	}
	//Now merge the two range files by using the mass pair data as a key

	//Find the matching ranges
	//range IDs from first and second file who have matching range values
	vector< pair<size_t,size_t> > rangeMatches;
	//IonID from the first dataset that we will need to replace
	vector<size_t> overrideIonID;
	for(size_t ui=0;ui<tmpRange[0].getNumRanges();ui++)
	{
		for(size_t uj=0;uj<tmpRange[1].getNumRanges();uj++)
		{
			if (tmpRange[0].getRange(ui) == tmpRange[1].getRange(uj))
			{
				rangeMatches.push_back(make_pair(ui,uj));
				overrideIonID.push_back(tmpRange[0].getIonID((unsigned int)ui));
			}
		}
	}

	//Take the data from the first range,
	// then discard the overlapping ions
	tmpRange[0].ionNames.swap(ionNames);
	tmpRange[0].colours.swap(colours);
	tmpRange[0].ionIDs.swap(ionIDs);
	tmpRange[0].ranges.swap(ranges);


	//Ensure there are no non-unique ion entries
	{
	vector<size_t> uniqItems=overrideIonID;
	std::sort(uniqItems.begin(),uniqItems.end());

	if(std::unique(uniqItems.begin(),uniqItems.end()) != uniqItems.end())
	{
		delete[] inBuffer;
		return RANGE_ERR_NONUNIQUE_POLYATOMIC;
	}
	}

	//Replace ionnames with new ion name and colour
	for(size_t ui=0;ui<overrideIonID.size();ui++)
	{
		size_t ids[2];
		ids[0]=overrideIonID[ui];
		ids[1] = tmpRange[1].getIonID((unsigned int)rangeMatches[ui].second);
		//Replace first rangefile colour and name with that of the second
		ionNames[ids[0]] = tmpRange[1].ionNames[ids[1]];
		colours[ids[0]] = tmpRange[1].colours[ids[1]];
	}

	ASSERT(isSelfConsistent());

	delete[] inBuffer;
	return 0;

}

unsigned int RangeFile::openRNG( FILE *fpRange)
{
	clear();

	//Oak-Ridge "Format" - this is based purely on example, as no standard exists
	//the classic example is from Miller, "Atom probe: Analysis at the atomic scale"
	//but alternate output forms exist. Our only strategy is to try to be as accommodating
	//as reasonably possible
	unsigned int errCode;

	unsigned int numRanges;
	unsigned int numIons;	

	//Load the range file header
	if((errCode=readRNGHeader(fpRange, ionNames, colours,numRanges,numIons)))
		return errCode;

	char *inBuffer = new char[MAX_LINE_SIZE];
	// skip over <LF> 
	char *ret;
	ret=fgets((char *)inBuffer, MAX_LINE_SIZE, fpRange); 
	if(!ret || strlen(ret) >= MAX_LINE_SIZE-1)
	{
		delete[] inBuffer;
		return RANGE_ERR_FORMAT;
	}
	// read the column header line 
	ret=fgets((char *)inBuffer, MAX_LINE_SIZE, fpRange); 

	if(!ret || strlen(ret) >= MAX_LINE_SIZE-1)
	{
		delete[] inBuffer;
		return RANGE_ERR_FORMAT;
	}

	//We should be at the line which has lots of dashes
	if(inBuffer[0] != '-')
	{
		delete[] inBuffer;
		return RANGE_ERR_FORMAT_TABLESEPARATOR;
	}

	
	//Load the rangefile frequency table
	vector<string> colHeaders;
	vector<unsigned int > frequencyEntries;
	
	{
	vector<string> warnings;
	vector<pair<float,float> > massData;
	errCode=readRNGFreqTable(fpRange,inBuffer,numIons,numRanges,ionNames,
				colHeaders, frequencyEntries,massData,warnings);
	if(errCode)
	{
		delete[] inBuffer;
		return errCode;
	}

	warnings.swap(warnMessages);
	ranges.swap(massData);
	}



	//Because of a certain software's output
	//it can generate a range listing like this (table +example colour line only)
	//
	// these are invalid according to white book
	//  and the "Atom Probe Microscopy", (Springer series in Materials Science
	//   vol. 160) descriptions
	//
	// Cu2 1.0 1.0 1.0 (Cu2)
	// ------------Cu Ni Mg Si CuNi4 Mg3Si2 Cu2
	// . 12.3 23.5 1 4 0 0 0 0 0
	// . 32.1 43.2 0 0 3 2 0 0 0
	// . 56.7 89.0 2 0 0 0 0 0 0
	//
	// which we get blamed for not supporting :(
	//
	//So, we have to scan the lists of ions, and create
	//tentative "combined" ion names, from the list of ions at the top.
	//
	//However, this violates the naming system in the "white book" (Miller, Atom probe: Analysis at the atomic scale)
	// scheme, which might allow something like
	// U235U238
	// as a single range representing a single ion. So they bastardise it by placing the zero columns
	// as a hint marker. Technically speaking, the zero column entries should not exist
	// in the format - as they would correspond to a non-existent species (or rather, an unreferenced species).
	// To check the case of certain programs using cluster ions like this, we have to engage in some
	// hacky-heuristics, check for 
	// 	- Columns with only zero entries, 
	// 	- which have combined numerical headers
	//
	// 	Then handle this as a separate case. FFS.

	//-- Build a sparse vector of composed ions
	std::map<string,size_t> composeMap;

	for(size_t uj=0;uj<numIons;uj++)
	{
		bool maybeComposed;
		maybeComposed=true;
		for(size_t ui=0; ui<numRanges;ui++)
		{
			//Scan through the range listing to try to
			//find a non-zero entries
			if(frequencyEntries[numIons*ui + uj])
			{
				maybeComposed=false;
				break;
			}
		}

		//If the ion has a column full of zeroes, and
		//the ion looks like its composable, then
		//decompose it for later references
		if(maybeComposed)
			composeMap.insert(make_pair(ionNames[uj].first,uj));
	}
	//--
		
	//vector of entries that are multiples, but don't have a 
	// matching compose key
	std::vector<pair<size_t, map<size_t,size_t > > > unassignedMultiples;

	//Loop through each range's freq table entries, to determine if
	// the ion is composed (has more than one "1" in multiplicity listing)
	for(size_t ui=0;ui<numRanges;ui++)
	{
		std::map<size_t,size_t > freqEntries;
		size_t freq;

	
		//Find the nonzero entries in this row
		//--
		freqEntries.clear();
		freq=0;
		for(size_t uj=0;uj<numIons;uj++)
		{
			size_t thisEntry;
			thisEntry=frequencyEntries[numIons*ui+uj];
			if(!thisEntry)
				continue;
		
			//Record nonzero entries
			freq+=thisEntry;
			freqEntries.insert(make_pair(uj,thisEntry));
		}
		//--

		if(freq ==1)
		{
			//Simple case - we only had a single [1] in
			//a row for the 
			ASSERT(freqEntries.size() == 1);
			ionIDs.push_back( freqEntries.begin()->first);
		}
		else if (freq > 1)
		{
			if(composeMap.empty())
			{
				//We have a multiple, but no way of composing it!
				// we will need to build our own table entry.
				// For now, just store the freq tableentry
				unassignedMultiples.push_back(make_pair(ui,freqEntries));
				ionIDs.push_back(-2);
			}
			else
			{
				//More complex case
				// ion appears to be composed of multiple fragments.
				//First entry is the ion name, second is the number of times it occurs 
				// (ie value in freq table, on this range line)
				vector<pair<string,size_t> > entries;

				for(map<size_t,size_t>::iterator it=freqEntries.begin();it!=freqEntries.end();++it)
					entries.push_back(make_pair(ionNames[it->first].first,it->second));

				//try to match the composed name to the entries
				size_t offset;
				if(!matchComposedName(composeMap,entries,offset))
				{
					//We failed to match the ion against a composed name.
					// cannot deal with this case.
					//
					// we can't just build a new ion name,
					// as we don't have a colour specification for this.
					//
					// We can't use the regular ion name, without 
					// tracking multiplicity (and then what to do with it in every case - 
					// seems only a special case for composition? eg. 
					// Is it a sep. species when clustering? Who knows!)
					delete[] inBuffer;
					
					return RANGE_ERR_DATA_NOMAPPED_IONNAME;
				}
				ASSERT(offset < ionNames.size());
				ionIDs.push_back( offset);
			}


		}
		else //0
		{
			//Range was useless - had no nonzero values
			//in frequency table.
			//Set to bad ionID - we will kill this later.
			ionIDs.push_back(-1);
		}
	}

	//Loop through any ranges with a bad ionID (== -1), then delete them by popping
	for(size_t ui=0;ui<ionIDs.size();ui++)
	{
		if(ionIDs[ui] == (size_t)-1)
		{
			std::swap(ranges[ui],ranges.back());
			ranges.pop_back();

			std::swap(ionIDs[ui],ionIDs.back());
			ionIDs.pop_back();
		}
	}


	//Check for any leftover unhandled cases
	if(unassignedMultiples.size())
	{
		//OK, so we didn't deal with a few cases before
		// lets sort these out now

		//Create a name + list of ranges mapping
		map<string,vector<int> > newNames;
		
		for(size_t ui=0;ui<unassignedMultiples.size();ui++)
		{
			ComparePairFirstReverse cmp;
			//create a flattened name from the map as a unique key
			//eg { Cu , 2 } | { Au, 1} | {O , 3} => O3Cu2Au
			//--
			{
			vector<pair<size_t,size_t> > flatData;
			
			std::map<size_t,size_t> &m=unassignedMultiples[ui].second;
			for(std::map<size_t,size_t>::const_iterator it=m.begin(); it!=m.end();++it)
				flatData.push_back(make_pair(it->first,it->second));
			std::sort(flatData.begin(),flatData.end(),cmp);

			string nameStr;
			for(size_t uj=0;uj<flatData.size();uj++)
			{
				string tmpStr;

				stream_cast(tmpStr,flatData[uj].second);
				//Use short name then value
				nameStr+=ionNames[flatData[uj].first].first + tmpStr;
			}
			
			//Append the new range/create a new entry
			newNames[nameStr].push_back(unassignedMultiples[ui].first);


			}
		}

		//Create one new name per new string we generated,
		for(map<string,vector<int> >::iterator it=newNames.begin(); it!=newNames.end();++it)
		{
			for(size_t ui=0;ui<it->second.size();ui++)
			{
				ASSERT(ionIDs[it->second[ui]] == (size_t)-2);
				ionIDs[it->second[ui]] =ionNames.size();
			}
			ionNames.push_back(make_pair(it->first,it->first));
			//make a new random colour
			RGBf col;
			col.red=rand()/(float)std::numeric_limits<int>::max();
			col.green=rand()/(float)std::numeric_limits<int>::max();
			col.blue=rand()/(float)std::numeric_limits<int>::max();

			colours.push_back(col);
		}

	}


	delete[] inBuffer;
	return 0;
}

unsigned int RangeFile::detectFileType(const char *rangeFile)
{
	enum
	{
		STATUS_NOT_CHECKED=0,
		STATUS_IS_NOT,
		STATUS_IS_MAYBE,
	};

#if !defined(__WIN32__) && !defined(__WIN64)
	//Under posix platforms (linux) directories can be opened by fopen. disallow
	if(isNotDirectory(rangeFile) == false)
		return RANGE_FORMAT_END_OF_ENUM;
#endif

	//create a fail-on-unimplemnted type detection scheme
	vector<unsigned int > typeStatus(RANGE_FORMAT_END_OF_ENUM,STATUS_NOT_CHECKED);

	//Check for RNG/Double RNG
	//--
	//first line in file should be two digits
	{
		std::ifstream f(rangeFile);

		if(!f)
			return RANGE_FORMAT_END_OF_ENUM;

		std::string tmpStr;
		size_t nCount;

		//retrieve the line
		getline(f,tmpStr);
		//strip exterior whitespace
		tmpStr=stripWhite(tmpStr);

		//break it apart
		vector<string> strs;
		splitStrsRef(tmpStr.c_str()," ",strs);

		//Drop the whitepace
		stripZeroEntries(strs);	

		if( strs.size() != 2)
		{
			//OK, quick parse failed. give up
			typeStatus[RANGE_FORMAT_ORNL]=STATUS_IS_NOT;
			typeStatus[RANGE_FORMAT_DBL_ORNL]=STATUS_IS_NOT;
			goto skipoutRNGChecks;
		}

		//Check for two space separated markers, parsable as ints
		size_t nIons,nRanges;
		bool castRes[2];
		castRes[0]=stream_cast(nIons,strs[0]);
		castRes[1]=stream_cast(nRanges,strs[1]);
		
		if( castRes[0] || castRes[1])
		{
			//OK, quick parse failed. give up
			typeStatus[RANGE_FORMAT_ORNL]=STATUS_IS_NOT;
			typeStatus[RANGE_FORMAT_DBL_ORNL]=STATUS_IS_NOT;
			goto skipoutRNGChecks;
		}
	

		typeStatus[RANGE_FORMAT_ORNL]=STATUS_IS_MAYBE;
		typeStatus[RANGE_FORMAT_DBL_ORNL]=STATUS_IS_MAYBE;

		//spin forwards to find dash line
		nCount=2*nIons+1;

		while(nCount--)
		{
			getline(f,tmpStr);

			//shouldn't hit eof
			if(f.eof() || (!f.good()) )
			{
				tmpStr.clear();
				break;
			}
		}

		if(!tmpStr.size() || tmpStr[0] != '-')
		{
			typeStatus[RANGE_FORMAT_ORNL]=STATUS_IS_NOT;
			typeStatus[RANGE_FORMAT_DBL_ORNL]=STATUS_IS_NOT;
			goto skipoutRNGChecks;

		}

		//Now, spin forwards until we either hit EOF or our double-dash marker
		while( ! (f.eof() || f.bad()) )
		{
			if(!getline(f,tmpStr))
				break;

			if(tmpStr.size() > 2 &&
				tmpStr[0] == '-' && tmpStr[1] == '-')
			{
				//OK, we saw a double dash. Thats forbidden under
				// ORNL , and allowable under double ORNL
				typeStatus[RANGE_FORMAT_ORNL]=STATUS_IS_NOT;
				break;
			}
		}

		if(!f.good())
		{
			//we did not see a double-dash, must be a vanilla ORNL file
			typeStatus[RANGE_FORMAT_DBL_ORNL]=STATUS_IS_NOT;
		}

skipoutRNGChecks:
		;
	}
	//--

	//Check for RRNG, if RNG did not match
	//--
	if(typeStatus[RANGE_FORMAT_ORNL] != STATUS_IS_MAYBE && typeStatus[RANGE_FORMAT_DBL_ORNL] !=STATUS_IS_MAYBE)
	{
		std::ifstream f(rangeFile);

		if(!f)
			return RANGE_FORMAT_END_OF_ENUM;

		//Check for existence of lines that match format section
		std::vector<string> sections;
		sections.push_back("[ions]");
		sections.push_back("[ranges]");

		vector<bool> haveSection(sections.size(),false);
	
		bool foundAllSections=false;

		//Scan through each line, looking for a matching section header
		while((!f.eof()) &&f.good())
		{

			//Get line, stripped of whitespace
			std::string tmpStr;
			getline(f,tmpStr);
			tmpStr=stripWhite(tmpStr);

			//See if we have this header
			for(size_t ui=0;ui<sections.size();ui++)
			{
				if(sections[ui] == lowercase(tmpStr))
				{
					haveSection[ui]=true;

					//Check to see if we have any sections left
					if(std::find(haveSection.begin(),haveSection.end(),false)
							== haveSection.end())
						foundAllSections=true;

					break;
				}
			}
		
			if(foundAllSections)
				break;
		}

		if(foundAllSections)
			typeStatus[RANGE_FORMAT_RRNG]=STATUS_IS_MAYBE;
		else
			typeStatus[RANGE_FORMAT_RRNG]=STATUS_IS_NOT;

	}
	else
	{
		//cannot be both maybe an RNG/Double RNG and an RRNG
		typeStatus[RANGE_FORMAT_RRNG]=STATUS_IS_NOT;
	}
	//--

	//Check for ENV
	//--
	if(typeStatus[RANGE_FORMAT_ORNL] != STATUS_IS_MAYBE && typeStatus[RANGE_FORMAT_DBL_ORNL] !=STATUS_IS_MAYBE &&
			typeStatus[RANGE_FORMAT_RRNG] != STATUS_IS_MAYBE)
	{
		//TODO: Less lazy implementation
		RangeFile tmpRng;
		FILE *f;
		f=fopen(rangeFile,"r");
		
		if(!f || tmpRng.openENV(f))
			typeStatus[RANGE_FORMAT_ENV]=STATUS_IS_NOT;
		else
			typeStatus[RANGE_FORMAT_ENV]=STATUS_IS_MAYBE;
		if(f)
			fclose(f);
	}
	else
	{
		//cannot be both maybe an RNG/Double RNG/RRNG and an env
		typeStatus[RANGE_FORMAT_ENV]=STATUS_IS_NOT;
	}

	//--

	//Check there is only one STATUS_IS_MAYBE or STATUS_NOT_CHECKED
	if((size_t)std::count(typeStatus.begin(),typeStatus.end(),(unsigned int)STATUS_IS_NOT) == typeStatus.size()-1)
	{
		//OK, there can only be one.  Return the format that has not
		//  been rejected
		for(size_t ui=0;ui<typeStatus.size();ui++)
		{
			if(typeStatus[ui]==STATUS_IS_MAYBE)
				return ui;

		}
		//in this case, we only have  NOT_CHECKED remaining. So, we have no idea
		return RANGE_FORMAT_END_OF_ENUM;

	}


	return RANGE_FORMAT_END_OF_ENUM;

}


unsigned int RangeFile::readRNGHeader(FILE *fpRange, vector<pair<string,string> > &strNames,
			vector<RGBf> &fileColours, unsigned int &numRanges, unsigned int &numIons)
{

	char *inBuffer= new char[MAX_LINE_SIZE];
	
	//Read out the number of ions and ranges in the file	
	if(fscanf(fpRange, "%64u %64u", &numIons, &numRanges) != 2)
	{
		
		delete[] inBuffer;
		return RANGE_ERR_FORMAT_HEADER;
	}
	
	if (!(numIons && numRanges))
	{
		delete[] inBuffer;
		return  RANGE_ERR_EMPTY;
	}
	

	RGBf colourStruct;
	pair<string,string> namePair;
	//Read ion short and full names as well as colour info
	for(unsigned int i=0; i<numIons; i++)
	{
		//Spin until we get to a new line. 
		//Certain programs emit range files that have
		//some string of unknown purpose
		//after the colour specification
		if(fpeek(fpRange)== ' ')
		{
			int peekVal;
			//Gobble chars until we hit the newline
			do
			{
				fgetc(fpRange);
				peekVal=fpeek(fpRange);
			}
			while(peekVal != (int)'\n'&&
				peekVal !=(int)'\r' && peekVal != EOF);

			//eat another char if we are using 
			//windows newlines
			if(peekVal== '\r')
			{
				if(fgetc(fpRange) == EOF)
				{
					delete[] inBuffer;
					return RANGE_ERR_FORMAT_COLOUR;
				}
			}
		}
		//Read the input for long name (max 255 chars)
		if(!fscanf(fpRange, " %255s", inBuffer))
		{
			delete[] inBuffer;
			return RANGE_ERR_FORMAT_LONGNAME;
		}
			
		namePair.second = inBuffer;


		//Read short name
		if(!fscanf(fpRange, " %255s", inBuffer))
		{
			delete[] inBuffer;
			return RANGE_ERR_FORMAT_SHORTNAME;
		}
		
		namePair.first= inBuffer;
		//Read Red green blue data
		if(!fscanf(fpRange,"%128f %128f %128f",&(colourStruct.red),
			&(colourStruct.green),&(colourStruct.blue)))
		{
			delete[] inBuffer;
			return RANGE_ERR_FORMAT_COLOUR;
		}
		
		strNames.push_back(namePair);	
		fileColours.push_back(colourStruct);	
	}	

	delete[] inBuffer;
	return 0;
}

unsigned int RangeFile::readRNGFreqTable(FILE *fpRange, char *inBuffer,const unsigned int numIons, 
			const unsigned int numRanges,const vector<pair<string,string> > &names, 
			vector<string> &colHeaders, vector<unsigned int > &tableEntries,
			vector<pair<float,float> > &massData, vector<string> &warnings)
{
	string entry;
	char *ptrBegin;
	ptrBegin=inBuffer;
	while(*ptrBegin && *ptrBegin == '-')
		ptrBegin++;
	splitStrsRef(ptrBegin," \n",colHeaders);
	if(!colHeaders.size() )
	{
		return RANGE_ERR_FORMAT_TABLESEPARATOR;
	}
	
	//remove whitespace from each entry
	for(size_t ui=0;ui<colHeaders.size();ui++)
	{
		stripChars(colHeaders[ui],"\f\n\r\t ");
	}
	
	stripZeroEntries(colHeaders);


	if(colHeaders.size() > 1)
	{

		if(colHeaders.size() !=numIons)
		{
			// Emit warning
			return RANGE_ERR_FORMAT_TABLEHEADER_NUMIONS;
		}
	
		//Strip any trailing newlines off the last of the  colheaders,
		// to avoid dos->unix conversion problems
		std::string str = colHeaders[colHeaders.size() -1];

		if(str[str.size() -1 ] == '\n')
			colHeaders[colHeaders.size()-1]=str.substr(0,str.size()-1);

		//Each column header should match the original ion name
		for(size_t ui=1;ui<colHeaders.size();ui++)
		{
			//look for a corresponding entry in the column headers
			if(names[ui-1].second != colHeaders[ui])
			{
				warnings.push_back(TRANS("Range headings do not match order of the ions listed in the name specifications. The name specification ordering will be used when reading the range table, as the range heading section is declared as a comment in the file-format specifications, and is not to be intepreted by this program. Check range-species associations actually match what you expect."));
				break;
			}

		}

	}

	tableEntries.resize(numRanges*numIons,0);
	//Load in each range file line
	pair<float,float> massPair;

	for(unsigned int i=0; i<numRanges; i++)
	{

		//grab the line
		if(fgets(inBuffer,MAX_LINE_SIZE,fpRange) == NULL)
			return RANGE_ERR_FORMAT_RANGETABLE;
		
	
		vector<string> entries;
		std::string tmpStr;
		tmpStr=stripWhite(inBuffer);
		
		splitStrsRef(tmpStr.c_str()," ",entries);
		stripZeroEntries(entries);

		//Should be two entries for the mass pair,
		// one entry per ion, and optionally one 
		// entry for the marker (not used)
		if(entries.size() != numIons + 2 &&
			entries.size() !=numIons+3)
		{
			return RANGE_ERR_FORMAT_RANGETABLE;
		}

		size_t entryOff;
		entryOff=0;
		//if we have a leading entry, ignore it
		if(entries.size() == numIons +3)
			entryOff=1;
		
		if(stream_cast(massPair.first,entries[entryOff]))
		{
			return RANGE_ERR_FORMAT_MASS_PAIR;
		}
		if(stream_cast(massPair.second,entries[entryOff+1]))
		{
			return RANGE_ERR_FORMAT_MASS_PAIR;
		}

		if(massPair.first >= massPair.second)
		{
			return RANGE_ERR_DATA_FLIPPED;
		}

		massData.push_back(massPair);

		//Load the range data line
		entryOff+=2;
		for(unsigned int j=0; j<numIons; j++)
		{
			size_t tempInt;
			if(stream_cast(tempInt,entries[entryOff+j]))
			{
				return RANGE_ERR_FORMAT_TABLE_ENTRY;
			}
			
			if(tempInt)
				tableEntries[numIons*i + j]=tempInt;
			
		}

		
	}
	
	
	//Do some post-processing on the range table
	//
	//Prevent rangefiles that have no valid ranges
	//from being loaded
	size_t nMax=std::accumulate(tableEntries.begin(),tableEntries.end(),0);
	if(!nMax)
	{
		return RANGE_ERR_DATA_TOO_MANY_USELESS_RANGES;
	}

	return 0;
}

unsigned int RangeFile::openENV(FILE *fpRange)
{
	clear();
	
	//Ruoen group "environment file" format
	//This is not a standard file format, so the
	//reader is a best-effort implementation, based upon examples
	char *inBuffer = new char[MAX_LINE_SIZE];
	unsigned int numRanges;
	unsigned int numIons;	

	bool beyondRanges=false;
	bool haveNumRanges=false;	
	bool haveNameBlock=false;	
	bool haveSeenRevHeader=false;	
	vector<string> strVec;

	//Read file until we get beyond the range length
	while(!beyondRanges && !feof(fpRange) )
	{
		if(!fgets((char *)inBuffer, MAX_LINE_SIZE, fpRange)
			|| strlen(inBuffer) >=MAX_LINE_SIZE-1)
		{
			delete[] inBuffer;
			return RANGE_ERR_FORMAT;
		}
		//Trick to "strip" the buffer
		nullifyMarker(inBuffer,'#');

		string s;
		s=inBuffer;

		s=stripWhite(s);

		if(!s.size())
			continue;

		//If we have
		if(!haveSeenRevHeader && (s== "Rev_2.0"))
		{
			haveSeenRevHeader=true;
			continue;
		}

		//Try different delimiters to split string
		splitStrsRef(s.c_str(),"\t ",strVec);

		stripZeroEntries(strVec);
		
		//Drop any entry data including and after ';'.
		// Revision 0.3 files show this ;, but it is not clear what
		// it is supposed to be for. Sample files I have show only zeros in these positions
		for(size_t ui=0;ui<strVec.size();ui++)
		{
			size_t offset;
			offset=strVec[ui].find(';');
			if(offset == std::string::npos)
				continue;

			strVec[ui]=strVec[ui].substr(0,offset);
		}


		stripZeroEntries(strVec);
		
		if(strVec.empty())
			continue;
		

		if(!haveNumRanges)
		{
			//num ranges should have two entries, num ions and ranges
			if(strVec.size() != 2)
			{
				delete[] inBuffer;
				return RANGE_ERR_FORMAT;
			}

			if(stream_cast(numIons,strVec[0]))
			{
				delete[] inBuffer;
				return RANGE_ERR_FORMAT;
			}
			if(stream_cast(numRanges,strVec[1]))
			{
				delete[] inBuffer;
				return RANGE_ERR_FORMAT;
			}

			haveNumRanges=true;
		}
		else
		{
			//Do we still have to process the name block?
			if(!haveNameBlock)
			{
				//Just exiting name block,
				if(strVec.size() == 5)
					haveNameBlock=true;
				else if(strVec.size() == 4)
				{
					//Entry in name block
					if(!strVec[0].size())
					{
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;
					}

					//Strip the charge state, if it exists
					strVec[0]=envDropChargeState(strVec[0]);

					//Check that name consists only of 
					//readable ascii chars, or period
					for(unsigned int ui=0; ui<strVec[0].size(); ui++)
					{
						if(!isdigit(strVec[0][ui]) &&
							!isalpha(strVec[0][ui]) && strVec[0][ui] != '.')
						{
							delete[] inBuffer;

							return RANGE_ERR_FORMAT;
						}
					}

					//Env file contains only the
					// long name, so use that for both
					// the short and long names
					bool nameExists=false;
					for(size_t ui=0;ui<ionNames.size();ui++)
					{
						if (ionNames[ui].first == strVec[0])
						{
							nameExists=true;
							break;
						}
					}

					//ion name already exists (eg as another charge state). nothing to do.
					// Note that env files can have two colours for same ion at different states
					// but we don't support this
					if(nameExists)
						continue;

					ionNames.push_back(std::make_pair(strVec[0],strVec[0]));
					//Use the colours (positions 1-3)
					RGBf colourStruct;
					if(stream_cast(colourStruct.red,strVec[1])
						||stream_cast(colourStruct.green,strVec[2])
						||stream_cast(colourStruct.blue,strVec[3]) )
					{
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;

					}

					if(colourStruct.red >1.0 || colourStruct.red < 0.0f ||
					   colourStruct.green >1.0 || colourStruct.green < 0.0f ||
					   colourStruct.blue >1.0 || colourStruct.blue < 0.0f )
					{
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;

					}
					
							
					colours.push_back(colourStruct);
				}
				else 
				{
					//Well thats not right,
					//we should be looking at the first entry in the
					//range block....
					delete[] inBuffer;
					return RANGE_ERR_FORMAT;
				}
			}

			if(haveNameBlock)
			{
				//We are finished
				if(strVec.size() == 5)
				{
					unsigned int thisIonID;
					thisIonID=(unsigned int)-1;
					strVec[0] = envDropChargeState(strVec[0]);
					for(unsigned int ui=0;ui<ionNames.size();ui++)
					{
						if(strVec[0] == ionNames[ui].first)
						{
							thisIonID=ui;
							break;
						}

					}
				
					if(thisIonID==(unsigned int) -1)
					{
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;
					}

					float rangeStart,rangeEnd;
					if(stream_cast(rangeStart,strVec[1]))
					{
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;
					}
					if(stream_cast(rangeEnd,strVec[2]))
					{
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;
					}

					//Disallow reversed ranges
					if(rangeStart > rangeEnd)
					{
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;
					}

					ranges.push_back(std::make_pair(rangeStart,rangeEnd));

					ionIDs.push_back(thisIonID);
				}
				else
					beyondRanges=true;

			}

		}
	
		
	}	

	delete[] inBuffer;

	//Must have encountered a name and range sections,
	// with at least one range
	if(!haveNumRanges || ! haveNameBlock)
		return RANGE_ERR_FORMAT;

	//Note that the in-file reported number of ions might actually be too big
	//as we "fold" some ions together (eg Si_+ and Si_2+ for us are both considered Si)
	if(ionNames.empty() || ionNames.size() > numIons || ranges.size() > numRanges)
		return RANGE_ERR_FORMAT;

	//There should be more data following the range information.
	// if not, this is not really an env file
	if(feof(fpRange))
	{
		return RANGE_ERR_FORMAT;
	}	

	return 0;
}

unsigned int RangeFile::openRRNG(FILE *fpRange)
{
	clear();
	
	char *inBuffer = new char[MAX_LINE_SIZE];
	unsigned int numRanges;
	unsigned int numBasicIons;


	//Different "blocks" that can be read
	enum {
		BLOCK_NONE,
		BLOCK_IONS,
		BLOCK_RANGES,
	};

	unsigned int curBlock=BLOCK_NONE;
	bool haveSeenIonBlock;
	haveSeenIonBlock=false;
	numRanges=0;
	numBasicIons=0;
	vector<string> basicIonNames;

	while (!feof(fpRange))
	{
		if(!fgets((char *)inBuffer, MAX_LINE_SIZE, fpRange)
			|| strlen(inBuffer) >=MAX_LINE_SIZE-1)
			break;
		//Trick to "strip" the buffer, assuming # is a comment
		nullifyMarker(inBuffer,'#');

		string s;
		s=inBuffer;

		s=stripWhite(s);
		if (!s.size())
			continue;

		if (lowercase(s) == "[ions]")
		{
			curBlock=BLOCK_IONS;
			continue;
		}
		else if (lowercase(s) == "[ranges]")
		{
			curBlock=BLOCK_RANGES;
			continue;
		}

		switch (curBlock)
		{
		case BLOCK_NONE:
			break;
		case BLOCK_IONS:
		{

			//The ion section in RRNG seems almost completely redundant.
			//This does not actually contain information about the ions
			//in the file (this is in the ranges section).
			//Rather it tells you what the constituents are from which
			//complex ions may be formed. Apply Palm to Face.

			vector<string> split;
			splitStrsRef(s.c_str(),'=',split);

			if (split.size() != 2)
			{
				delete[] inBuffer;
				return RANGE_ERR_FORMAT;
			}

			std::string stmp;
			stmp=lowercase(split[0]);

			haveSeenIonBlock=true;


			if (stmp == "number")
			{
				//Should not have set the
				//number of ions yet.
				if (numBasicIons !=0)
				{
					delete[] inBuffer;
					return RANGE_ERR_FORMAT;
				}
				//Set the number of ions
				if(stream_cast(numBasicIons, split[1]))
				{
					delete[] inBuffer;
					return RANGE_ERR_FORMAT;
				}

				if (numBasicIons == 0)
				{
					delete[] inBuffer;
					return RANGE_ERR_FORMAT;
				}
			}
			else if (split[0].size() >3)
			{
				stmp = lowercase(split[0].substr(0,3));
				if (stmp == "ion")
				{
					//OK, its an ion.
					basicIonNames.push_back(split[1]);

					if (basicIonNames.size()  > numBasicIons)
					{
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;
					}
				}
				else
				{
					delete[] inBuffer;
					return RANGE_ERR_FORMAT;
				}
			}
			break;
		}
		case BLOCK_RANGES:
		{
			//Although it looks like the blocks are independent.
			//it is more complicated to juggle a parser with them
			//out of dependency order, as  a second pass would
			//need to be done.
			if (!haveSeenIonBlock)
			{
				delete[] inBuffer;
				return RANGE_ERR_FORMAT;
			}
			vector<string> split;

			if (s.size() > 6)
			{
				splitStrsRef(s.c_str(),'=',split);

				if (split.size() != 2)
				{
					delete[] inBuffer;
					return RANGE_ERR_FORMAT;
				}

				if (lowercase(split[0].substr(0,5)) == "numbe")
				{

					//Should not have set the num ranges yet
					if (numRanges)
					{
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;
					}

					if (stream_cast(numRanges,split[1]))
					{
					
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;
					}

					if (!numRanges)
					{
						delete[] inBuffer;
						return RANGE_ERR_FORMAT;
					}

				}
				else if ( lowercase(split[0].substr(0,5)) == "range")
				{
					//Try to decode a range line, as best we can.
					//These appear to come in a variety of flavours,
					//which makes this section quite long.

					//OK, so the format here is a bit weird
					//we first have to strip the = bit
					//then we have to step across fields,
					//I assume the fields are in order (or missing)
					//* denotes 0 or more,  + denotes 1 or more, brackets denote grouping
					// Vol: [0-9]* ([A-z]+: [0-9]+)* (Name:[0-9]*([A-z]+[0-9]*)+ Color:[0-F][0-F][0-F][0-F][0-F][0-F]
					// Examples:
					// Range1=31.8372 32.2963 Vol:0.01521 Zn:1 Color:999999
					// or
					// Range1=31.8372 32.2963 Zn:1 Color:999999
					// or
					// Range1=95.3100 95.5800 Vol:0.04542 Zn:1 Sb:1 Color:00FFFF
					// or
					// Range1=95.3100 95.5800 Vol:0.04542 Name:1Zn1Sb1 Color:00FFFF
					// or
					// Range1=95.3100 95.5800 Vol:0.04542 Zn:1 Sb:1 Name:1Zn1Sb1 Color:00FFFF
					// or
					// Range1= 95.3100 95.5800 Color:00FFFF Vol:0.04542 Zn:1 Sb:1 Name:1Zn1Sb1 

					//Starting positions (string index)
					//of range start and end
					//Range1=31.8372 32.2963 Vol:0.01521 Zn:1 Color:999999
					//	 	^rngmid ^rngend
					string rngStart,rngEnd;
					string strTmp=stripWhite(split[1]);

					//Split the remaining field into key:value pairs, with whitespace delimiters
					split.clear();
					splitStrsRef(strTmp.c_str(),"\t ",split);
					stripZeroEntries(split);
					//Need a minimum of 4 entries here
					if(split.size() < 4)
						return RANGE_ERR_FORMAT;
					
					//First two contains the range data
					rngStart=split[0];
					rngEnd=split[1];

					//Remove the two leading elements, the rest are : separated
					split.erase(split.begin());
					split.erase(split.begin());

					//Retrieve the  remaining fields
					RGBf col;
					bool haveColour,haveNameField;
					haveColour=false;
					haveNameField=false;
					string strIonNameTmp,strNameFieldValue;

					for (unsigned int ui=0; ui<split.size(); ui++)
					{
						//':' is the key:value delimiter (Self reference is hilarious.)
						size_t colonPos;


						colonPos = split[ui].find_first_of(':');

						if (colonPos == std::string::npos)
						{
							delete[] inBuffer;
							return RANGE_ERR_FORMAT;
						}

						//Extract the key value pairs
						string key,value;
						key=split[ui].substr(0,colonPos);
						value=split[ui].substr(colonPos+1);
						if (lowercase(key) == "vol")
						{
							//Do nothing
						}
						else if (lowercase(key) == "name")
						{
							//OK, so we need to handle two weird cases
							//1) we have the name section and the ion section
							//2) we have the name section and no ion section

							//Rant:WTF is the point of the "name" section. They are 
							//encoding information in a redundant and irksome fashion!
							//Better to store the charge (if that's what you want) in a F-ing charge
							//section, and the multiplicity is already bloody defined elsewhere!
							
							//This won't be known until we complete the parse
							haveNameField=true;
							strNameFieldValue=value;
						}
						else if (lowercase(key) == "color")
						{
							haveColour=true;


							if (value.size() !=6)
							{
								delete[] inBuffer;
								return RANGE_ERR_FORMAT;
							}

							//Use our custom string parser,
							//which requires a leading #,
							//in lowercase
							value = string("#") + lowercase(value);
							ColourRGBA tmpRgba;
							if(!tmpRgba.parse(value))
							{
								delete[] inBuffer;
								return RANGE_ERR_FORMAT;
							}

							col.red = (float)tmpRgba.r()/255.0f;
							col.green=(float)tmpRgba.g()/255.0f;
							col.blue=(float)tmpRgba.b()/255.0f;
						}
						else
						{
							//Basic ion was not in the original [Ions] list. This is not right
							unsigned int pos=(unsigned int)-1;
							for (unsigned int uj=0; uj<basicIonNames.size(); uj++)
							{
								if (basicIonNames[uj] == key)
								{
									pos = uj;
									break;
								}
							}

							if (pos == (unsigned int)-1)
							{
								delete[] inBuffer;
								return RANGE_ERR_FORMAT;
							}
							//Check the multiplicity of the ion. Should be an integer > 0
							unsigned int uintVal;
							if (stream_cast(uintVal,value) || !uintVal)
							{
								delete[] inBuffer;
								return RANGE_ERR_FORMAT;
							}

							//If it is  1, then use straight name. Otherwise try to give it a
							//chemical formula look by mixing in the multiplicity after the key
							if (uintVal==1)
								strIonNameTmp+=key;
							else
								strIonNameTmp+=key+value;
						}
					}

					if (!haveColour )
					{
						//Okay, so a colour wasn't provided.
						// to make our users lives a bit less
						// boring, lets just use some semi-random ones
						col.red=rand()/(float)std::numeric_limits<int>::max();
						col.green=rand()/(float)std::numeric_limits<int>::max();
						col.blue=rand()/(float)std::numeric_limits<int>::max();

					}

					//Get the range values
					float rngStartV,rngEndV;
					if(strIonNameTmp.size() || haveNameField)
					{
						if (stream_cast(rngStartV,rngStart))
						{
							delete[] inBuffer;
						
							return RANGE_ERR_FORMAT;
						}

						if (stream_cast(rngEndV,rngEnd))
						{
							delete[] inBuffer;
						
							return RANGE_ERR_FORMAT;
						}
					}

					//So the ion field appears to be optional (that is,
					//ivas emits RRNGs that do not contain an ion listed in the 
					//ion field). It is unclear what the purpose of these lines is,
					//so we shall ignore it, in preference to aborting
					if(strIonNameTmp.size())
					{
						//Check to see if we have this ion.
						//If we don't, we create a new one.
						//if we do, we check that the colours match
						//and if not reject the file parsing.
						unsigned int pos=(unsigned int)(-1);
						for (unsigned int ui=0; ui<ionNames.size(); ui++)
						{
							if (ionNames[ui].first == strIonNameTmp)
							{
								pos=ui;
								break;
							}
						}

						ranges.push_back(std::make_pair(rngStartV,rngEndV));
						if (pos == (unsigned int) -1)
						{
							//This is new. Create a new ion,
							//and use its ionID
							ionNames.push_back(std::make_pair(strIonNameTmp,
										     strIonNameTmp));
							colours.push_back(col);
							ionIDs.push_back(ionNames.size()-1);
						}
						else
						{
							//we have the name already
							ionIDs.push_back(pos);
						}
					}
					else if(haveNameField)
					{
						//OK, so we don't have the ion section
						//but we do have the field section
						//let us decode the name field in order to retrieve 
						//the ion information which was missing (for whatever reason)
						if(!strNameFieldValue.size())
						{
							delete[] inBuffer;
				
		
							return RANGE_ERR_FORMAT;
						}


						unsigned int chargeStrStop=0;
						for(unsigned int ui=0;ui<strNameFieldValue.size();ui++)
						{
							if(strNameFieldValue[ui] <'0' || strNameFieldValue[ui] > '9')
							{
								chargeStrStop =ui;
								break;
							}

						}

						//Strip off the charge value, to get the ion name
						strNameFieldValue =strNameFieldValue.substr(chargeStrStop,strNameFieldValue.size()-chargeStrStop);

						//Check to see if we have this ion.
						//If we dont, we create a new one.
						//if we do, we check that the colours match
						//and if not reject the file parsing.
						unsigned int pos=(unsigned int)(-1);
						for (unsigned int ui=0; ui<ionNames.size(); ui++)
						{
							if (ionNames[ui].first == strNameFieldValue)
							{
								pos=ui;
								break;
							}
						}

						ranges.push_back(std::make_pair(rngStartV,rngEndV));
						if (pos == (unsigned int) -1)
						{
							//This is new. Create a new ion,
							//and use its ionID
							ionNames.push_back(std::make_pair(strNameFieldValue,
										     strNameFieldValue));
							colours.push_back(col);
							ionIDs.push_back(ionNames.size()-1);
						}
						else
						{
							//we have the name already
							ionIDs.push_back(pos);
						}

					}
				}
				else
				{
					delete[] inBuffer;
					return RANGE_ERR_FORMAT;
				}

			}
			break;
		}
		default:
			ASSERT(false);
			break;
		}
	}

	delete[] inBuffer;

	//If we didn't find anything useful, then thats a formatting error
	if(!haveSeenIonBlock || !numRanges  || !numBasicIons)
		return RANGE_ERR_FORMAT;
	
	if(numRanges != ranges.size())
		return RANGE_ERR_FORMAT;

	return 0;
}

bool RangeFile::extensionIsRange(const char *ext) 
{
	bool isRange=false;
	unsigned int extOff=0;
	while(strlen(RANGE_EXTS[extOff]))
	{
		if(!strcmp(ext,RANGE_EXTS[extOff]))
		{
			isRange=true;
			break;
		}
		extOff++;
	}

	return isRange;
}


void RangeFile::getAllExts(std::vector<std::string> &exts)
{
	//subtract one for the guard terminator, which
	// we do not want to return
	exts.resize(THREEDEP_ARRAYSIZE(RANGE_EXTS)-1);

	for(unsigned int ui=0;ui<THREEDEP_ARRAYSIZE(RANGE_EXTS)-1; ui++)
	{
		exts[ui]=RANGE_EXTS[ui];
		ASSERT(exts[ui].size());
	}
}

bool RangeFile::isSelfConsistent() const
{
	for(unsigned int ui=0;ui<ranges.size();ui++)
	{
		//Check for range zero width
		if(ranges[ui].first == ranges[ui].second)
			return false;
	
		//Check that ranges don't overlap.
		for(unsigned int uj=ui+1; uj<ranges.size();uj++)
		{
			//check that not sitting inside a range
			if(ranges[ui].first > ranges[uj].first &&
					ranges[ui].first < ranges[uj].second )
				return false;
			if(ranges[ui].second > ranges[uj].first &&
					ranges[ui].second < ranges[uj].second )
				return false;

			//check for not spanning a range
			if(ranges[ui].first < ranges[uj].first &&
					ranges[ui].second > ranges[uj].second)
				return false;

			//Check for range duplication
			if(ranges[ui].first == ranges[uj].first &&
					ranges[ui].second == ranges[uj].second)
				return false;

		}
	}

	
	//Ensure that the names don't clash
	for(size_t ui=0;ui<ionNames.size();ui++)
	{
		for(size_t uj=ui+1;uj<ionNames.size();uj++)
		{
			if(ionNames[ui].first == ionNames[uj].first ||
				ionNames[ui].second == ionNames[uj].second)
				return false;
		}
	}


	//Ensure that names conform to allowed naming scheme
	for(size_t ui=0;ui<ionNames.size();ui++)
	{
		string tmp;
		tmp=ionNames[ui].first;

		const char *DISALLOWED_ION_NAMES=" \t\r\n";
		//TODO : Use whitelist, rather than blacklist
		if(tmp.find_first_of(DISALLOWED_ION_NAMES)!= string::npos)
			return false;

		tmp=ionNames[ui].second;
		if(tmp.find_first_of(DISALLOWED_ION_NAMES)!= string::npos) 
			return false;
	}


	return true;
}

bool RangeFile::isRanged(float mass) const
{
	unsigned int numRanges = ranges.size();
	
	for(unsigned int ui=0; ui<numRanges; ui++)
	{
		if( mass >= ranges[ui].first  &&
				mass <= ranges[ui].second )
			return true;
	}
	
	return false;
}

bool RangeFile::isRanged(const IonHit &ion) const
{
	return isRanged(ion.getMassToCharge());
}

bool RangeFile::range(vector<IonHit> &ions, string ionShortName) 
{
	vector<IonHit> rangedVec;
	
	//find the Ion ID of what we want
	unsigned int targetIonID=(unsigned int)-1;
	for(unsigned int ui=0; ui<ionNames.size(); ui++)
	{
		if(ionNames[ui].first == ionShortName)
		{
			targetIonID = ui;
			break;
		}
	}

	if(targetIonID == (unsigned int)(-1))
		return false;

	//find the ranges that have that ionID
	vector<unsigned int> subRanges;
	for(unsigned int ui=0; ui<ionIDs.size(); ui++)
	{
		if(ionIDs[ui] == targetIonID)
			subRanges.push_back(ui);
	}
	
	unsigned int numIons=ions.size();
	rangedVec.reserve(numIons);

	unsigned int numSubRanges = subRanges.size();
	
	for(unsigned int ui=0; ui<numIons; ui++)
	{
		for(unsigned int uj=0; uj<numSubRanges; uj++)
		{
			if( ions[ui].getMassToCharge() >= ranges[subRanges[uj]].first  &&
					ions[ui].getMassToCharge() <= ranges[subRanges[uj]].second )
			{
				rangedVec.push_back(ions[ui]);
				break;
			}
		}
	}

	//Do the switcheroonie
	//such that the un-ranged ions are destroyed
	//and the ranged ions are kept 
	ions.swap(rangedVec);
	return true;
}

void RangeFile::range(vector<IonHit> &ions)
{
	vector<IonHit> rangedVec;

	unsigned int numIons=ions.size();
	rangedVec.reserve(numIons);

	for(unsigned int ui=0; ui<numIons; ui++)
	{
		if(isRanged(ions[ui]))
			rangedVec.push_back(ions[ui]);
	}

	//Do the switcheroonie
	//such that the un-ranged ions are destroyed
	//and the ranged ions are kept 
	ions.swap(rangedVec);
}

void RangeFile::rangeByRangeID(vector<IonHit> &ions, unsigned int rangeID)
{
	vector<IonHit> rangedVec;
	
	unsigned int numIons=ions.size();
	rangedVec.reserve(numIons);
	
	for(unsigned int ui=0; ui<numIons; ui++)
	{
		if( ions[ui].getMassToCharge() >= ranges[rangeID].first  &&
		   ions[ui].getMassToCharge() <= ranges[rangeID].second )
		{
			rangedVec.push_back(ions[ui]);
			break;
		}
	}
	
	//Do the switcheroonie
	//such that the un-ranged ions are destroyed
	//and the ranged ions are kept 
	ions.swap(rangedVec);
}

void RangeFile::rangeByIon(const std::vector<IonHit> & ions,
			const vector<bool> &selectedIons, vector<IonHit> &output) const
{
	output.clear();

	//TODO: Play with openmp, to see if this is faster.
	// There will be problems with synchronising writes to the output.
	// Could try a sampling algorithm to estimate output size
	for(size_t ui=0;ui<ions.size();ui++)
	{
		unsigned int id;
		id=getIonID(ions[ui].getMassToCharge());
		if(id == (unsigned int)-1)
			continue;
		if(selectedIons[id])
			output.push_back(ions[ui]);
	}
}

void RangeFile::printErr(std::ostream &strm) const
{
	ASSERT(errState < RANGE_ERR_ENUM_END);
	strm <<TRANS(rangeErrStrings[errState]) << std::endl;
}

std::string RangeFile::getErrString() const
{
	ASSERT(errState < RANGE_ERR_ENUM_END);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(rangeErrStrings) == RANGE_ERR_ENUM_END);

	std::string s;
	s=TRANS(rangeErrStrings[errState]);
	return s;
}

unsigned int RangeFile::getNumRanges() const
{
	return ranges.size();
}

unsigned int RangeFile::getNumRanges(unsigned int ionID) const
{
	unsigned int res=0;
	for(unsigned int ui=0;ui<ranges.size();ui++)
	{
		if( getIonID(ui) == ionID)
			res++;
	}

	return res;
}

unsigned int RangeFile::getNumIons() const
{
	return ionNames.size();
}

pair<float,float> RangeFile::getRange(unsigned int ui) const
{
	return ranges[ui];
}

pair<float,float> &RangeFile::getRangeByRef(unsigned int ui) 
{
	return ranges[ui];
}

RGBf RangeFile::getColour(unsigned int ui) const
{
	ASSERT(ui <  colours.size());
	return colours[ui];
}

unsigned int RangeFile::getIonID(float mass) const
{
	unsigned int numRanges = ranges.size();
	
	for(unsigned int ui=0; ui<numRanges; ui++)
	{
		if( mass >= ranges[ui].first  &&
				mass <= ranges[ui].second )
			return ionIDs[ui];
	}
	
	return (unsigned int)-1;
}

unsigned int RangeFile::getRangeID(float mass) const
{
	unsigned int numRanges = ranges.size();
	
	for(unsigned int ui=0; ui<numRanges; ui++)
	{
		if( mass >= ranges[ui].first  &&
				mass <= ranges[ui].second )
			return ui;
	}
	
	return (unsigned int)-1;
}

unsigned int RangeFile::getIonID(unsigned int range) const
{
	ASSERT(range < ranges.size());

	return ionIDs[range];
}

unsigned int RangeFile::getIonID(const char *name, bool useShortName) const
{
	if(useShortName)
	{
		//find the first element in the sequence
		for(unsigned int ui=0; ui<ionNames.size(); ui++)
		{
			if(ionNames[ui].first == name)
				return ui;
		}
	}
	else
	{
		//Find using the second element in the sequence (longName)
		for(unsigned int ui=0; ui<ionNames.size(); ui++)
		{
			if(ionNames[ui].second == name)
				return ui;
		}

	}
	return (unsigned int)-1;	
}

std::string RangeFile::getName(unsigned int ionID,bool shortName) const
{
	ASSERT(ionID < ionNames.size());
	if(shortName)
		return ionNames[ionID].first;
	else
		return ionNames[ionID].second;
}	

std::string RangeFile::getName(const IonHit &ion, bool shortName) const
{
	ASSERT(isRanged(ion));
	
	if(shortName)
		return ionNames[getIonID(ion.getMassToCharge())].first;
	else
		return ionNames[getIonID(ion.getMassToCharge())].second;
}

bool RangeFile::rangeByID(vector<IonHit> &ionHits, unsigned int rng)
{
	//This is a bit slack, could be faster, but should work.
	return range(ionHits,ionNames[rng].first);
}

bool RangeFile::isRanged(string shortName, bool caseSensitive)
{
	if(caseSensitive)
	{
		for(unsigned int ui=ionNames.size(); ui--; )
		{
			if(ionNames[ui].first == shortName)
				return true;
		}
	}
	else
	{
		for(unsigned int ui=ionNames.size(); ui--; )
		{
			//perform a series of case independent 
			//string comparisons 
			string str;
			str = ionNames[ui].first;

			if(str.size() !=shortName.size())
				continue;
			
			bool next;
			next=false;
			for(unsigned int ui=str.size(); ui--; )
			{
				if(tolower(str[ui]) != 
					tolower(shortName[ui]))
				{
					next=true;
					break;
				}
			}
			
			if(!next)
				return true;
		}
	}

	return false;
}

void RangeFile::setColour(unsigned int id, const RGBf &r) 
{
	ASSERT(id < colours.size());
	colours[id] = r;
}


void RangeFile::setIonShortName(unsigned int id,  const std::string &newName)
{
	ionNames[id].first=newName;
}

void RangeFile::setIonLongName(unsigned int id, const std::string &newName)
{
	ionNames[id].second = newName;
}

bool RangeFile::setRangeStart(unsigned int rangeId, float v)
{
	ASSERT(!enforceConsistency || isSelfConsistent());
	
	float &f=ranges[rangeId].first;
	float tmp;
	tmp=f;
	f=v;
	
	//TODO: I think this does excess calculatioN?
	if(enforceConsistency && !isSelfConsistent())
	{
		//restore original value
		f=tmp;
		return false;
	}

	return true;
}

bool RangeFile::setRangeEnd(unsigned int rangeId, float v)
{

	ASSERT(!enforceConsistency || isSelfConsistent());

	float &f=ranges[rangeId].second;
	float tmp;
	tmp=f;
	f=v;
	
	//TODO: I think this does excess calculatioN?
	if(enforceConsistency && !isSelfConsistent())
	{
		//restore original value
		f=tmp;
		return false;
	}

	return true;
}
void  RangeFile::swap(RangeFile &r)
{
	using std::swap;
	swap(ionNames,r.ionNames);
	swap(colours,r.colours);
	swap(ranges,r.ranges);
	swap(ionIDs,r.ionIDs);
	swap(warnMessages,r.warnMessages);
	swap(errState,r.errState);

}

bool RangeFile::moveRange(unsigned int rangeId, bool upperLimit, float newMass)
{
	if(enforceConsistency)
	{
		//Check for moving past other part of range -- "inversion"
		if(upperLimit)
		{
			//Move upper range
			if(newMass <= ranges[rangeId].first)
				return false;
		}
		else
		{
			if(newMass >= ranges[rangeId].second)
				return false;
		}

		//Check that moving this range will not cause any overlaps with 
		//other ranges
		for(unsigned int ui=0; ui<ranges.size(); ui++)
		{
			if( ui == rangeId)
				continue;

			if(upperLimit)
			{
				//moving high range
				//check for overlap on first
				if((ranges[rangeId].first < ranges[ui].first &&
						newMass > ranges[ui].first))
				       return false;
				
				if((ranges[rangeId].first < ranges[ui].second &&
						newMass > ranges[ui].second))
				       return false;
			}
			else
			{
				//moving low range
				//check for overlap on first
				if((ranges[rangeId].second > ranges[ui].first &&
						newMass < ranges[ui].first))
				       return false;
				
				if((ranges[rangeId].second > ranges[ui].second &&
						newMass < ranges[ui].second))
				       return false;
			}

		}

	}
	
	if(upperLimit)
		ranges[rangeId].second = newMass;
	else
		ranges[rangeId].first= newMass;

	return true;
}

bool RangeFile::moveBothRanges(unsigned int rangeId, float newLow, float newHigh)
{

	//Check that moving this range will not cause any overlaps with 
	//other ranges
	for(unsigned int ui=0; ui<ranges.size(); ui++)
	{
		if( ui == rangeId)
			continue;

		//moving high range
		//check for overlap on first
		if((ranges[rangeId].first < ranges[ui].first &&
				newHigh > ranges[ui].first))
		       return false;
		
		if((ranges[rangeId].first < ranges[ui].second &&
				newHigh > ranges[ui].second))
		       return false;
		//moving low range
		//check for overlap on first
		if((ranges[rangeId].second > ranges[ui].first &&
				newLow < ranges[ui].first))
		       return false;
		
		if((ranges[rangeId].second > ranges[ui].second &&
				newLow < ranges[ui].second))
		       return false;
	}

	ranges[rangeId].second = newHigh;
	ranges[rangeId].first= newLow;

	return true;
}



unsigned int RangeFile::addRange(float start, float end, unsigned int parentIonID) 
{
	ASSERT(start < end);
	if(enforceConsistency)
	{
		//Ensure that they do NOT overlap
		for(unsigned int ui=0;ui<ranges.size();ui++)
		{
			//Check for start end inside other range
			if(start > ranges[ui].first && 
					start<=ranges[ui].second)
				return -1;

			if(end > ranges[ui].first && 
					end<=ranges[ui].second)
				return -1;

			//check for start/end spanning range entirely
			if(start < ranges[ui].first && end > ranges[ui].second)
				return -1;
		}
	}
	//Got this far? Good - valid range. Insert it and move on
	ionIDs.push_back(parentIonID);
	ranges.push_back(std::make_pair(start,end));

#ifdef DEBUG
	if(enforceConsistency)
	{
		ASSERT(isSelfConsistent());
	}
#endif
	return ranges.size();
}

unsigned int RangeFile::addIon(const std::string &shortN, const std::string &longN, const RGBf &newCol)
{
	for(unsigned int ui=9; ui<ionNames.size() ;ui++)
	{
		if(ionNames[ui].first == shortN || ionNames[ui].second == longN)
			return -1;
	}

	ionNames.push_back(make_pair(shortN,longN));
	colours.push_back(newCol);

	ASSERT(isSelfConsistent());
	return ionNames.size()-1;
}

void RangeFile::setIonID(unsigned int range, unsigned int newIonId)
{
	ASSERT(newIonId < ionIDs.size());
	ionIDs[range] = newIonId;
}

void RangeFile::eraseRange(size_t rangeId)
{
	ASSERT(rangeId < ranges.size());
	std::swap(ranges.back(),ranges[rangeId]);
	ranges.pop_back();	
	
	std::swap(ionIDs.back(),ionIDs[rangeId]);
	ionIDs.pop_back();	
}

void RangeFile::eraseIon(size_t ionId)
{
	
	//Kill any ranges that belong to this ion
	vector<bool> killRange(ranges.size(),false);
	for(size_t ui=0;ui<ionIDs.size(); ui++)
	{
		if(ionIDs[ui]  == ionId)
			killRange[ui]=true;
	}

	//Remove the desired range and ionID mappings.
	vectorMultiErase(ranges,killRange);
	vectorMultiErase(ionIDs,killRange);

	//Remove the ion name and colour for the selected ion
	ionNames.erase(ionNames.begin()+ionId);
	colours.erase(colours.begin()+ionId);

	//Now, we have to renumber the existing ionIDs, which have shifted down a peg
	for(size_t ui=0;ui<ionIDs.size();ui++)
	{
		//We should have deleted this ionsID
		ASSERT(ionIDs[ui] != ionId);
		//Shift any ionIDs that are higher down one peg
		if(ionIDs[ui]  >ionId)
			ionIDs[ui]--;
#ifdef DEBUG
		ASSERT(ionIDs[ui] < ionNames.size());
#endif
	}


}

bool RangeFile::decompose(RangeFile &rng) const
{
	//find the list of decomposables
	rng=*this;	
	for(size_t ui=0; ui<ionNames.size(); ui++)
	{	
		vector<pair<string,size_t > > fragments;
		if(!decomposeIonNames(ionNames[ui].first,fragments))
			return false;

		for(size_t uj=0; uj<fragments.size();uj++)
		{
			if(rng.getIonID(fragments[uj].first) == -1)
			{
				//make a new random colour
				RGBf col;
				col.red=rand()/(float)std::numeric_limits<int>::max();
				col.green=rand()/(float)std::numeric_limits<int>::max();
				col.blue=rand()/(float)std::numeric_limits<int>::max();
				//Create a new ion to support this fragment
				rng.addIon(fragments[uj].first,fragments[uj].first,col);
			}
		}	
	}	
	ASSERT(rng.isSelfDecomposable());

	return true;
}

bool RangeFile::isSelfDecomposable() const
{
	for(size_t ui=0; ui<ionNames.size(); ui++)
	{	
		vector<pair<string,size_t > > fragments;
		if(!decomposeIonNames(ionNames[ui].first,fragments))
			return false;
		//ion cannot be decomposed into smaller fragments. 
		//This is not a decomposable rangefile
		if(getIonID(ionNames[ui].first) == -1)
			return false;
	}	

	return true;
}

bool RangeFile::getDecomposition(std::map<unsigned int, vector< std::pair<unsigned int, unsigned int> > > &decomposition) const
{
	decomposition.clear();
	//TODO: Convert to cached, statified map?
	for(size_t ui=0;ui<ionNames.size(); ui++)
	{
		vector<pair<string,size_t> > thisFragment;
		if(!decomposeIonNames(ionNames[ui].first,thisFragment))
			return false;

		//convert the name,count pairs to  ionID, count pairs
		vector<pair<unsigned int, unsigned int> > fragmentAsRanges;
		fragmentAsRanges.resize(thisFragment.size());
		
		for(size_t uj=0;uj<fragmentAsRanges.size();uj++)
		{	
			unsigned int ionID;
			ionID = getIonID(thisFragment[uj].first);
			
			if(ionID ==(unsigned int)-1)
				return false;
	
			fragmentAsRanges[uj] = make_pair(ionID,thisFragment[uj].second);
		}
		decomposition[ui] = fragmentAsRanges;
	
		fragmentAsRanges.clear();	
	}

	return true;
}
