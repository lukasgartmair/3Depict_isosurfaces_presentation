/*
 *	externalProgram.h - Call out external programs as data sources/sinks
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
#ifndef EXTERNALPROGRAM_H
#define EXTERNALPROGRAM_H

#include "../filter.h"
#include "../../common/translation.h"

enum
{
	EXTERNALPROGRAM_KEY_COMMAND,
	EXTERNALPROGRAM_KEY_WORKDIR,
	EXTERNALPROGRAM_KEY_ALWAYSCACHE,
	EXTERNALPROGRAM_KEY_CLEANUPINPUT
};

//!External program filter
class ExternalProgramFilter : public Filter
{

		//!The command line strings; prior to expansion 
		std::string commandLine;

		//!Working directory for program
		std::string workingDir;

		//!Always cache output from program
		bool alwaysCache;
		//!Erase generated input files for ext. program after running?
		bool cleanInput;

		static size_t substituteVariables(const std::string &commandStr,
				const std::vector<std::string> &ions, const std::vector<std::string> &plots, 
							std::string &substitutedCommand);

	public:
		//!As this launches external programs, this could be misused.
		bool canBeHazardous() const {return true;}

		ExternalProgramFilter();
		virtual ~ExternalProgramFilter(){};

		Filter *cloneUncached() const;
		//!Returns cache size as a function fo input
		virtual size_t numBytesForCache(size_t nObjects) const;
		
		//!Returns FILTER_TYPE_EXTERNALPROC
		unsigned int getType() const { return FILTER_TYPE_EXTERNALPROC;};
		//update filter
		unsigned int refresh(const std::vector<const FilterStreamData *> &dataIn,
					std::vector<const FilterStreamData *> &getOut, 
					ProgressData &progress);
		
		virtual std::string typeString() const { return std::string(TRANS("Ext. Program"));};

		//!Get the properties of the filter, in key-value form. First vector is for each output.
		void getProperties(FilterPropGroup &propertyList) const;

		//!Set the properties for the nth filter
		bool setProperty(unsigned int key, 
				const std::string &value, bool &needUpdate);
		//!Get the human readable error string associated with a particular error code during refresh(...)
		std::string getSpecificErrString(unsigned int code) const;
		
		//!Dump state to output stream, using specified format
		bool writeState(std::ostream &f,unsigned int format,
						unsigned int depth=0) const;
		//!Read the state of the filter from XML file. If this
		//fails, filter will be in an undefined state.
		bool readState(xmlNodePtr &node, const std::string &packDir);
		
		//!Get the stream types that will be dropped during ::refresh	
		unsigned int getRefreshBlockMask() const;

		//!Get the stream types that will be generated during ::refresh	
		unsigned int getRefreshEmitMask() const;	
		
		//!Get the stream types possibly used during ::refresh	
		unsigned int getRefreshUseMask() const;	
		
		//!Set internal property value using a selection binding  (Disabled, this filter has no bindings)
		void setPropFromBinding(const SelectionBinding &b)  ;

#ifdef DEBUG
		bool runUnitTests();

		bool substituteTest();
#endif
};

#endif
