/* 
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

#ifndef ABUNDANCE_H
#define ABUNDANCE_H

#include <vector>
#include <string>
#include <utility>

// Example
//======
//  |------|
//  | 2    |<- mass number
//  |    H |<- Symbol
//  | 1    |<-  atomic number
//  |------|
//    2.014101 <- mass
//    0.00000000006 <- mass error
//    0.000115 <- abundance
//    0.000070 <- abundance error
//======

struct ISOTOPE_DATA
{
	std::string symbol;
	size_t massNumber,atomicNumber;
	float mass;
	float massError; //positive if known, zero if unknown
	float abundance;
	float abundanceError; //positive if known, zero if unknown
};

//!Class to load abundance information for natural isotopse
class AbundanceData
{
	enum
	{
		ABUNDANCE_ERR_BAD_DOC=1,
		ABUNDANCE_ERR_NO_CONTEXT,
		ABUNDANCE_ERROR_BAD_VALUE,
		ABUNDANCE_ERROR_FAILED_VALIDATION,
		ABUNDANCE_ERROR_MISSING_NODE,
		ABUNDANCE_ERROR_MISSING_ROOT_NODE,
		ABUNDANCE_ERROR_WRONG_ROOT_NODE,
		ABUNDANCE_ERROR_ENUM_END
	};
	
	//!vector of atoms, containing a vector of isotope datasets
	// First vector is indexed by atomic number
	std::vector<std::vector<ISOTOPE_DATA> >  isotopeData;

	//atomic numbers for each atom's isotope entry	
	// this is esentially a lookup (isotope # -> atom #)
	std::vector<size_t> atomicNumber;

	//Check the abundance table for inconsistenceis
	void checkErrors() const; 

	public:
		//!Attempt to open the abundance data file, return 0 on success
		size_t open(const char *file, bool strict=false);	

		static const char *getErrorText(size_t errorCode);

		size_t numIsotopes() const;
		size_t numElements() const;
		//Return the elemnl
		//case sensitive, must match chemistry style Upper[lower], eg Fe.	
		size_t symbolIndex(const char *symbol) const;
		//Return a vector of symbol indices
		void getSymbolIndices(const std::vector<std::string> &symbols,std::vector<size_t> &indices) const;

		std::string elementName(size_t elemIdx ) const { return isotopeData[elemIdx][0].symbol;}
	
		const std::vector<ISOTOPE_DATA> &isotopes(size_t offset) const { return isotopeData[offset];}

		//Compute the mass-probability distribution for a set of ions
		void generateIsotopeDist(const std::vector<size_t> &elementIdx,
					const std::vector<size_t> &frequency,
					std::vector<std::pair<float,float> > &massDist,size_t solutionCharge=1) const;

		void generateSingleAtomDist(size_t atomIdx, unsigned int repeatCount, std::vector<std::pair<float,float> > &massDist,size_t solutionCharge=1) const;

		const ISOTOPE_DATA &isotope(size_t elementIdx, size_t isotopeIdx) const;

#ifdef DEBUG
		//Run the unit esting code
		static bool runUnitTests(const char *tableFile);
#endif
};

#endif
