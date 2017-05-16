/*
 * configFile.h - Configuration file management header 
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

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <deque>

#include "backend/filter.h"


//!Startup panel identifiers. 
enum {
	CONFIG_STARTUPPANEL_RAWDATA,
	CONFIG_STARTUPPANEL_CONTROL,
	CONFIG_STARTUPPANEL_PLOTLIST,
	CONFIG_STARTUPPANEL_END_ENUM
};

enum
{
	CONFIG_PANELMODE_NONE,
	CONFIG_PANELMODE_REMEMBER,
	CONFIG_PANELMODE_SPECIFY,
	CONFIG_PANELMODE_END_ENUM
};

enum
{
	CONFIG_ERR_NOFILE=1,
	CONFIG_ERR_BADFILE,
	CONFIG_ERR_NOPARSER
};


class ConfigFile 
{
	private:
		std::deque<std::string> recentFiles;
		std::vector<Filter *> filterDefaults;
		
		//!Did the configuration load from file OK?
		bool configLoadOK;
		
		//!Panel 
		std::vector<bool> startupPanelView;

		//!Any errors that occur during file IO. Set by class members during read()/write()
		std::string errMessage;

		//!Method for showing/hiding panel at startup
		unsigned int panelMode;

		//!Initial application window size in pixels
		unsigned int initialSizeX,initialSizeY;
		//!Do we have a valid initial app size?
		bool haveInitialAppSize;

		//!Percentile speeds for mouse zoom and move 
		unsigned int mouseZoomRatePercent,mouseMoveRatePercent;

		//!Want startup orthographic cam
		bool wantStartupOrthoCam;

		//!Master allow the program to do stuff online check. This is AND-ed, so cannot override disabled items
		bool allowOnline;

		//!Should the program perform online version number checking?
		bool allowOnlineVerCheck;	

		//!fractional initial positions of sashes in main UI
		float leftRightSashPos,topBottomSashPos,
		      		filterSashPos,plotListSashPos;

		//!True if config has a valid maxPoints value
		bool haveMaxPoints;

		//!Max. number of points to display in 3D scene
		// 0 for unlimited
		size_t maxPointsScene;

		//!Does the user want to be shown a startup tip dialog?
		bool doWantStartupTips;
	public:
		ConfigFile(); 
		~ConfigFile(); 
		void addRecentFile(const std::string &str);
		void getRecentFiles(std::vector<std::string> &filenames) const; 
		void removeRecentFile(const std::string &str);
		
		unsigned int read();
		bool write();
		
		bool configLoadedOK() const { return configLoadOK;}
			
		//Create the configuration folder, if needed.
		static bool createConfigDir() ;
		//Get the configuration dir path
		static std::string getConfigDir();
		
		std::string getErrMessage() const { return errMessage;};

		static unsigned int getMaxHistory();

		//Get a vector of the default filter pointers
		void getFilterDefaults(std::vector<Filter* > &defs);
		//Set the default filter pointers (note this will take ownership of the pointer)
		void setFilterDefaults(const std::vector<Filter* > &defs);

		//Get a clone of the default filter for a given type,
		//even if it is not in the array (use hardcoded)
		Filter *getDefaultFilter(unsigned int type) const;

		bool getHaveMaxPoints() const { return haveMaxPoints;}
		size_t getMaxPoints() const { return maxPointsScene;}
		void setMaxPoints(size_t maxP) { haveMaxPoints=true; maxPointsScene=maxP;}

		//!Return startup status of UI panels
		bool getPanelEnabled(unsigned int panelID) const;
		
		//!Return startup status of UI panels
		void setPanelEnabled(unsigned int panelID,bool enabled, bool permanent=false);

		//!Get the mouse movement rate (for all but zoom)
		unsigned int getMouseMoveRate() const { return mouseMoveRatePercent; }
		//!Get the mouse movement rate for zoom
		unsigned int getMouseZoomRate() const { return mouseZoomRatePercent; }

		//Set the mouse zoom rate(percent)
		void setMouseZoomRate(unsigned int rate) { mouseZoomRatePercent=rate;};
		//Set the mouse move rate (percent)
		void setMouseMoveRate(unsigned int rate) { mouseMoveRatePercent=rate;};

		//!Return the current panelmode
		unsigned int getStartupPanelMode() const;
		//!Set the mode to use for recalling the startup panel layout
		void setStartupPanelMode(unsigned int panelM);

		//!Returns true if we have a suggested initial window size; with x & y being the suggestion
		bool getInitialAppSize(unsigned int &x, unsigned int &y) const;
		//!Set the initial window suggested size
		void setInitialAppSize(unsigned int x, unsigned int y);

		bool getAllowOnlineVersionCheck() const;

		//!Set if the program is allowed to access network resources
		void setAllowOnline(bool v);
		//!Set if the program is allowed to phone home to get latest version #s
		void setAllowOnlineVersionCheck(bool v);

		//!Set the position for the main window left/right sash 
		void setLeftRightSashPos(float fraction);
		//!Set the position for the top/bottom sash
		void setTopBottomSashPos(float fraction);
		//!Set the position for the filter property/tree sash
		void setFilterSashPos(float fraction);
		//!Set the position for the plot list panel
		void setPlotListSashPos(float fraction);

		//!Set the position for the main window left/right sash 
		float getLeftRightSashPos() const { return leftRightSashPos;};
		//!Set the position for the top/bottom sash
		float getTopBottomSashPos() const{ return topBottomSashPos;}
		//!Set the position for the filter property/tree sash
		float getFilterSashPos() const { return filterSashPos;};
		//!Set the position for the plot list panel
		float getPlotListSashPos()const { return plotListSashPos;};

		//!Does the user want startup tips
		bool wantStartupTips() const { return doWantStartupTips;}
		//!Set if tips are to be shown on startup
		void setWantStartupTips(bool want)  { doWantStartupTips=want;}

		//!Get if user wants an orhtographic camera on startup
		bool getWantStartupOrthoCam() const { return wantStartupOrthoCam;} 

		void setWantStartupOrthoCam(bool want) { wantStartupOrthoCam=want;}
};

#endif
