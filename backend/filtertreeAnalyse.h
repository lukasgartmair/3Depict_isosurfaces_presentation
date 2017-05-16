/*
 * 	filtertree.h - Filter tree topology and data propagation handling
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

#ifndef FILTERTREEANALYSE_H
#define FILTERTREEANALYSE_H

#include "filtertree.h"


enum 
{
	ANALYSE_SEVERITY_WARNING, // Probable warning
	ANALYSE_SEVERITY_ERROR, // definite error
	ANALYSE_SEVERITY_END_ENUM // Not a severity, just end of enum
};

struct FILTERTREE_ERR
{
	//The filters that are associated with the error messages
	std::vector<const Filter *> reportedFilters;
	//Error messages associated with the reported filters 
	std::string verboseReportMessage,shortReportMessage;

	unsigned int severity;
};


class FilterTreeAnalyse
{
	private:
		std::vector<FILTERTREE_ERR> analysisResults;


		//Accumulated emit and block masks for the filter tree
		// these are only valid during the call to  ::analyse
		std::map<Filter*, size_t> emitTypes; //Whatever types can be emitted from this filer, considering this filter's ancestors in tree, not incl. self
		std::map<Filter*,size_t> blockTypes; //Whatever types can be blocked by this filter, considering this filter's children in tree, not incl. self

		//!Detect misconfiguration of the filter tree
		// where parent emits something that the child
		// cannot use
		void blockingPairError( const FilterTree &f);

		//!Detect case where algorithms that depend
		// upon there being no spatial sampling
		// are being used with sampling.
		void spatialSampling(const FilterTree &f);

		//check to see if there is a filter who is biasing composition
		void compositionAltered(const FilterTree &f);

		//check to see if there is a filter that needs a particular parent
		void checkRequiredParent(const FilterTree &f);
		
		//check to see if there is a filter that needs unranged data to work,
		// but does not have it 
		void checkUnrangedData(const FilterTree &f);
	public:
		void analyse(const FilterTree &f);

		void getAnalysisResults(std::vector<FILTERTREE_ERR> &errs) const; 

		void clear() {analysisResults.clear();};
};


#endif

