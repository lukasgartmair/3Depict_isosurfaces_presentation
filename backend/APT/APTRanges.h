/*
 * APTRanges.h - Atom probe rangefile class 
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

#ifndef APTRANGES_H
#define APTRANGES_H

#include <vector>
#include <string>
#include <map>


#include "backend/APT/ionhit.h"
#include "common/basics.h"

enum{	
	RANGE_ERR_OPEN =1, 
	RANGE_ERR_FORMAT_HEADER,
	RANGE_ERR_EMPTY,
	RANGE_ERR_FORMAT_LONGNAME,
	RANGE_ERR_FORMAT_SHORTNAME,
	RANGE_ERR_FORMAT_COLOUR,
	RANGE_ERR_FORMAT_TABLESEPARATOR,
	RANGE_ERR_FORMAT_TABLEHEADER_NUMIONS,
	RANGE_ERR_FORMAT_RANGE_DUMMYCHARS,
	RANGE_ERR_FORMAT_RANGETABLE,
	RANGE_ERR_FORMAT_MASS_PAIR,
	RANGE_ERR_FORMAT_TABLE_ENTRY,
	RANGE_ERR_FORMAT,
	RANGE_ERR_DATA_TOO_MANY_USELESS_RANGES,
	RANGE_ERR_DATA_FLIPPED,
	RANGE_ERR_DATA_INCONSISTENT,
	RANGE_ERR_DATA_NOMAPPED_IONNAME,
	RANGE_ERR_NONUNIQUE_POLYATOMIC,
	RANGE_ERR_FILESIZE,
	RANGE_ERR_ENUM_END
};


//Number of elements stored in the table
const unsigned int NUM_ELEMENTS=119;

enum{ RANGE_FORMAT_ORNL,
	RANGE_FORMAT_DBL_ORNL,
	RANGE_FORMAT_ENV,
	RANGE_FORMAT_RRNG,
	RANGE_FORMAT_END_OF_ENUM //not a format, just end of enumueration.
};

//!Data storage and retrieval class for various range files
class RangeFile
{
	private:
		//These vectors will contain the number of ions
		
		//The first element is the shortname for the Ion
		//the second is the full name
		std::vector<std::pair<std::string,std::string> > ionNames;
		//This holds the colours for the ions
		std::vector<RGBf> colours;
		
		//This will contains the number of ranges
		//
		//This holds the min and max masses for the range
		std::vector<std::pair<float,float> > ranges;
		//The ion ID number for each range 
		//FIXME: Convert to proper uniqueID system
		std::vector<size_t> ionIDs;

		//Should we enforce range consistency?
		bool enforceConsistency;

		unsigned int errState;
		//Warning messages, used when loading rangefiles
		std::vector<std::string> warnMessages;


		//!Erase the contents of the rangefile
		void clear();


		//!Load an ORNL formatted "RNG" rangefile
		// caller must supply and release file pointer
		unsigned int openRNG(FILE *fp);
		
		//Read the header section of an RNG file
		static unsigned int readRNGHeader(FILE *fpRange, 
			std::vector<std::pair<std::string,std::string> > &strNames,
			std::vector<RGBf> &fileColours, unsigned int &numRanges, 
								unsigned int &numIons);

		//Read the range frequency table
		static unsigned int readRNGFreqTable(FILE *fpRange, char *inBuffer, 
				const unsigned int numIons, const unsigned int numRanges,
				const std::vector<std::pair<std::string,std::string> > &names,
					std::vector<std::string> &colHeaders, 
					std::vector<unsigned int > &tableEntries,
					std::vector<std::pair<float,float> > &massData,
					std::vector<std::string> &warnings);

		unsigned int openDoubleRNG(FILE *fp);


		//!Load an RRNG file
		// caller must supply and release file pointer
		unsigned int openRRNG(FILE *fp);
		//!Load an ENV file
		// caller must supply and release file pointer
		unsigned int openENV(FILE *fp);

		//Strip charge state from ENV ion names
		static std::string envDropChargeState(const std::string &strName);

	public:
		RangeFile();

		const RangeFile& operator=(const RangeFile &other);
		//!Open a specified range file, returns zero on success, nonzero on failure
		unsigned int open(const char *rangeFile, unsigned int format=RANGE_FORMAT_ORNL);	
		//!Open a specified range file - returns true on success
		bool openGuessFormat(const char *rangeFile);

		//!is the extension string the same as that for a range file? I don't advocate this method, but it is convenient in a pinch.
		static bool extensionIsRange(const char *ext);
		//!Grab a vector that contains all the extensions that are valid for range files
		static void getAllExts(std::vector<std::string> &exts);

		//Attempt to detect the file format of an unknown rangefile.
		// returns enum value on success, or RANGE_FORMAT_END_OF_ENUM on failure
		static unsigned int detectFileType(const char *file);

		void setEnforceConsistent(bool shouldEnforce=true) { enforceConsistency=shouldEnforce;}

		//!Performs checks for self consistency.
		bool isSelfConsistent() const;
		
		//!Print the translated error associated with the current range file state
		void printErr(std::ostream &strm) const;
		//!Retrieve the translated error associated with the current range file state
		std::string getErrString() const;

		

		//!Get the number of unique ranges
		unsigned int getNumRanges() const;
		//!Get the number of ranges for a given ion ID
		unsigned int getNumRanges(unsigned int ionID) const;
		//!Get the number of unique ions
		unsigned int getNumIons() const;
		//!Retrieve the start and end of a given range as a pair(start,end)
		std::pair<float,float> getRange(unsigned int ) const;

		//!Retrieve the start and end of a given range as a pair(start,end)
		std::pair<float,float> &getRangeByRef(unsigned int );
		//!Retrieve a given colour from the ion ID
		RGBf getColour(unsigned int) const;
		//!Set the colour using the ion ID
		void setColour(unsigned int, const RGBf &r);

		
		//!Retrieve the colour from a given ion ID

		//!Get the ion's ID from a specified mass
		/*! Returns the ions ID if there exists a range that 
		 * contains this mass. Otherwise (unsigned int)-1 is returned
		 */
		unsigned int getIonID(float mass) const;
		//!Get the ion ID from a given range ID
		/*!No validation checks are performed outside debug mode. Ion
		 range *must* exist*/
		unsigned int getIonID(unsigned int range) const;
		//!Get the ion ID from its short or long name, returns -1 if name does not exist. Case must match
		
		unsigned int getIonID(const char *name, bool useShortName=true) const;	
		unsigned int getIonID(const std::string &name) const {return getIonID(name.c_str());};	
		
		//!Set the ion ID for a given range
		void setIonID(unsigned int range, unsigned int newIonId);

		//!returns true if a specified mass is ranged
		bool isRanged(float mass) const;
		//! Returns true if an ion is ranged
		bool isRanged(const IonHit &) const;
		//!Clips out ions that are not inside the range
		void range(std::vector<IonHit> &ionHits);
		//!Clips out ions that dont match the specified ion name
		/*! Returns false if the ion name given doesn't match
		 *  any in the rangefile (case sensitive) 
		 */	
		bool range(std::vector<IonHit> &ionHits,
				std::string shortIonName);

		//!Clip out only a specific subset of ions
		// Selected ions *MUST* be of the same size as getNumIons()
		void rangeByIon(const std::vector<IonHit> & ions,
			const std::vector<bool> &selectedIons, std::vector<IonHit> &output) const;

		//!Clips out ions that dont lie in the specified range number 
		/*! Returns false if the range does not exist 
		 *  any in the rangefile (case sensitive) 
		 */	
		bool rangeByID(std::vector<IonHit> &ionHits,
					unsigned int range);
		void rangeByRangeID(std::vector<IonHit> &ionHits,
					unsigned int rangeID);
		//!Get the short name or long name of a specified ionID
		/*! Pass shortname=false to retireve the long name 
		 * ionID passed in must exist. No checking outside debug mode
		 */
		std::string getName(unsigned int ionID,bool shortName=true) const;

		std::string getName(const IonHit &ion, bool shortName) const;

		//!set the short name for a given ion	
		void setIonShortName(unsigned int ionID, const std::string &newName);

		//!Set the long name for a given ion
		void setIonLongName(unsigned int ionID, const std::string &newName);

		//!Check to see if an atom is ranged
		/*! Returns true if rangefile holds at least one range with shortname
		 * corresponding input value. Case sensitivity search is default
		 */
		bool isRanged(std::string shortName, bool caseSensitive=true);

		//!Write the rangefile to the specified output stream (default ORNL format)
		unsigned int write(std::ostream &o,size_t format=RANGE_FORMAT_ORNL) const;
		//!WRite the rangefile to a file (ORNL format)
		unsigned int write(const char *datafile, size_t format=RANGE_FORMAT_ORNL) const;

		//!Return the atomic number of the element from either the long or short version of the atomic name 
		/*
		 * Short name takes precedence
		 * 
		 * Example : if range is "H" or "Hydrogen" function returns 1 
		 * Returns 0 on error (bad atomic name)
		 */
		unsigned int atomicNumberFromRange(unsigned int range) const;
		
	
		//!Get atomic number from ion ID
		unsigned int atomicNumberFromIonID(unsigned int ionID) const;

		//!Get a range ID from mass to charge 
		unsigned int getRangeID(float mass) const;

		//!Swap a range file with this one
		void swap(RangeFile &rng);

		//!Move a range's mass to a new location
		bool moveRange(unsigned int range, bool limit, float newMass);
		//!Move both of a range's masses to a new location
		bool moveBothRanges(unsigned int range, float newLow, float newHigh);

		//!Add a range to the rangefile. Returns ID number of added range
		// if adding successful, (unsigned int)-1 otherwise.
		/// If enforceConsistency is true, this will disallow addition of ranges that
		// collide with existing ranges
		unsigned int addRange(float start, float end, unsigned int ionID);

		//Add the ion to the database returns ion ID if successful, -1 otherwise
		unsigned int addIon(const std::string &shortName, const std::string &longName, const RGBf &ionCol);
	
		bool setRangeStart(unsigned int rangeID, float v);

		bool setRangeEnd(unsigned int rangeID, float v);

		//Erase given range
		void eraseRange(size_t rangeId) ;

		//erase given ions and associated rnagefes)
		void eraseIon(size_t ionId);

		//can we decompose all composed ranges in this file into
		// other components that already exist within this range?
		bool isSelfDecomposable() const;
		
		//Generate a secondary rangefile with decomposable ranges as needed.	
		bool decompose(RangeFile &rng) const;

		//Obtain the decompsiition for this range. This maps composed ranges to their decomposed ones.
		//  - This will perform a decompsition if needed, and return false if not self decomposable.
		bool getDecomposition(
			std::map<unsigned int, std::vector< std::pair< unsigned int, unsigned int > > > &decomposition) const;
	
		//Break a given string down into a series of substring-count pairs depicting basic ionic components
		static bool decomposeIonNames(const std::string &name,

			std::vector<std::pair<std::string,size_t> > &fragments);
};

#endif
