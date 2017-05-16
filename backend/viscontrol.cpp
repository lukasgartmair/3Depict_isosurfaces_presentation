/*

 * 	viscontrol.h - Visualisation control header; "glue" between user interface and scene
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
#include "viscontrol.h"

#include <wx/combobox.h>

#include "wx/propertyGridUpdater.h"
#include "wx/wxcomponents.h"
#include "common/voxels.h"
#include "filters/openvdb_includes.h"

using std::list;
using std::vector;
using std::pair;

bool VisController::isInstantiated = false;

//TODO: Remove me, and refactor filters
bool dummyRefreshCallback(bool dummy)
{
	return true;
}


RefreshController::RefreshController(TreeState &tC)
{
	treeState=&tC;
}

RefreshController::~RefreshController()
{
}

unsigned int RefreshController::refresh()
{
	ASSERT(treeState);
	return treeState->refresh(refreshData,consoleMessages,curProg);
}


bool VisController::stateIsModified(unsigned int minLevel) const
{
	return state.hasStateData() && getStateModifyLevel();
}

void VisController::transferSceneCameraToState()
{
	const Camera *c=scene.getActiveCam();
	state.setCameraByClone(c,state.getActiveCam());
}

void VisController::setCamProperty(size_t offset, unsigned int key, const std::string &value)
{
	state.setCamProperty(offset,key,value);
	if(offset == state.getActiveCam())
		scene.setActiveCamByClone(state.getCam(offset));
}


void VisController::setWxTreeFilterViewPersistence(size_t filterId)
{
	persistentFilters.push_back(state.treeState.getFilterById(filterId));
}

void VisController::updateCameraPropGrid(wxPropertyGrid *g, size_t camId) const
{
	ASSERT(g);

	const Camera *c;
	c= state.getCam(camId);

	updateCameraPropertyGrid(g,c);
}

void VisController::updateFilterPropGrid(wxPropertyGrid *g,size_t filterId, const std::string &stateStr) const
{
	ASSERT(g);

	const Filter *targetFilter;
	targetFilter=state.treeState.getFilterById(filterId);

	ASSERT(targetFilter);
	
	updateFilterPropertyGrid(g,targetFilter,stateStr);
}


void VisController::updateScene(RefreshController *r)
{
	//Strip data out of the refresh controller (drop the parent filter data)
	list<FILTER_OUTPUT_DATA> &sceneData = r->getRefreshData();
	list< vector<const FilterStreamData *> > dataOnly;

	for(list<FILTER_OUTPUT_DATA>::iterator it=sceneData.begin(); it!=sceneData.end();++it)
	{
		vector<const FilterStreamData *> t;
		t.resize(it->second.size());
		std::copy(it->second.begin(),it->second.end(),t.begin());
		dataOnly.push_back(t);
	}	
	
	updateScene(dataOnly,false);
}

void VisController::updateScene(list<vector<const FilterStreamData *> > &sceneData, 
				bool releaseData)
{
	//Plot window should be set
	ASSERT(plotSelList)

	//Lock the opengl scene interaction,
	// to prevent user interaction (e.g. devices) during callbacks
	scene.lockInteraction();
	targetPlots.lockInteraction();

	//Buffer to transfer to scene
	vector<DrawableObj *> sceneDrawables;
	
	targetPlots.clear(true); //Clear, but preserve selection information.


	//Names for plots
	vector<std::pair<size_t,string> > plotLabels;

	//rate-limit the number of drawables to show in the scene
	map<const IonStreamData *, const IonStreamData*> throttleMap;
	throttleSceneInput(sceneData,throttleMap);

	//-- Build buffer of new objects to send to scene
	for(list<vector<const FilterStreamData *> > ::iterator it=sceneData.begin(); 
							it!=sceneData.end(); ++it)
	{

		ASSERT(it->size());
		for(unsigned int ui=0;ui<it->size(); ui++)
		{
			//Filter must specify whether it is cached or not. Other values
			//are inadmissible, but useful to catch uninitialised values
			ASSERT((*it)[ui]->cached == 0 || (*it)[ui]->cached == 1);
			
			//set to true if we need to free up ram pointed to by 
			// scene object iterator
			bool deleteIt;
			deleteIt=false;

			switch((*it)[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
					//Create a new group for this stream. 
					// We have to have individual groups 
					// because of colouring/sizing concerns.
					DrawManyPoints *curIonDraw;
					curIonDraw=new DrawManyPoints;


					//Obtain the ion data pointer.
					// note that we have to use the throttled points
					// (in throttlemap) if they are there, to prevent
					// overloading the display system.
					const IonStreamData *ionData;
					ionData=((const IonStreamData *)((*it)[ui]));
					if(throttleMap.find(ionData) != throttleMap.end())
					{
						ionData=throttleMap[ionData];
					}


					curIonDraw->resize(ionData->data.size());
					//Slice out just the coordinate data for the 
					// ion pointer, run callback immediately 
					// after, as its a long operation
					#pragma omp parallel for shared(curIonDraw,ionData)
					for(size_t ui=0;ui<ionData->data.size();ui++)
						curIonDraw->setPoint(ui,ionData->data[ui].getPosRef());
					
					//Set the colour from the ionstream data
					curIonDraw->setColour(ionData->r,
								ionData->g,
								ionData->b,
								ionData->a);
					//set the size from the ionstream data
					curIonDraw->setSize(ionData->ionSize);
					//Randomly shuffle the ion data before we draw it
					curIonDraw->shuffle();
				
					//place in special holder for ions,
					// as we need to accumulate for display-listing
					// later.
					sceneDrawables.push_back(curIonDraw);
					break;
				}
				case STREAM_TYPE_PLOT:
				{
					const PlotStreamData *plotData;
					plotData=((PlotStreamData *)((*it)[ui]));
					
					//The plot should have some data in it.
					ASSERT(plotData->getNumBasicObjects());
					//The plot should have an index, so we can keep
					//filter choices between refreshes (where possible)
					ASSERT(plotData->index !=(unsigned int)-1);
					//Construct a new plot
					unsigned int plotID;

					
					//No other plot mode is currently implemented.
					ASSERT(plotData->plotMode == PLOT_MODE_1D);
					
					//Create a 1D plot
					Plot1D *plotNew= new Plot1D;

					plotNew->setData(plotData->xyData);
					plotNew->setLogarithmic(plotData->logarithmic);
					plotNew->titleAsRawDataLabel=plotData->useDataLabelAsYDescriptor;
					plotNew->setErrMode(plotData->errDat);
					//Construct any regions that the plot may have
					for(unsigned int ui=0;ui<plotData->regions.size();ui++)
					{
						//add a region to the plot,
						//using the region data stored
						//in the plot stream
						plotNew->regionGroup.addRegion(plotData->regionID[ui],
							plotData->regionTitle[ui],	
							plotData->regions[ui].first,
							plotData->regions[ui].second,
							plotData->regionR[ui],
							plotData->regionG[ui],
							plotData->regionB[ui],plotData->regionParent);
					}

					//transfer the axis labels
					plotNew->setStrings(plotData->xLabel,
						plotData->yLabel,plotData->dataLabel);
					
					//set the appearance of the plot
					//plotNew->setTraceStyle(plotStyle);
					plotNew->setColour(plotData->r,plotData->g,plotData->b);
					
					
					plotNew->parentObject=plotData->parent;
					plotNew->parentPlotIndex=plotData->index;
					
					plotID=targetPlots.addPlot(plotNew);

					plotLabels.push_back(make_pair(plotID,plotData->dataLabel));
					
					break;
				}
				//TODO: Merge back into STREAM_TYPE_PLOT
				case STREAM_TYPE_PLOT2D:
				{
					const Plot2DStreamData *plotData;
					plotData=((Plot2DStreamData *)((*it)[ui]));
					//The plot should have some data in it.
					ASSERT(plotData->getNumBasicObjects());
					//The plot should have an index, so we can keep
					//filter choices between refreshes (where possible)
					ASSERT(plotData->index !=(unsigned int)-1);
					unsigned int plotID;
		
					PlotBase *plotNew;
					switch(plotData->plotType) 
					{
						case PLOT_2D_DENS:
						{
							//Create a 2D plot
							plotNew= new Plot2DFunc;

							//set the plot info
							((Plot2DFunc*)plotNew)->setData(plotData->xyData,
										plotData->xMin, plotData->xMax, 
										plotData->yMin,plotData->yMax);
						
							break;
						}
						case PLOT_2D_SCATTER:
						{
							//Create a 2D plot
							Plot2DScatter *p = new Plot2DScatter;
							//set the plot info
							if(plotData->scatterIntensity.size())
								p->setData(plotData->scatterData,plotData->scatterIntensity);
							else
								p->setData(plotData->scatterData);

							p->scatterIntensityLog=plotData->scatterIntensityLog;

							plotNew=p;				
							break;
						}
						default:
							ASSERT(false);
					}
					//transfer the axis labels
					plotNew->setStrings(plotData->xLabel,
						plotData->yLabel,plotData->dataLabel);
					
					//transfer the parent info
					plotNew->parentObject=plotData->parent;
					plotNew->parentPlotIndex=plotData->index;
					
					plotID=targetPlots.addPlot(plotNew);
					
					// -----

					plotLabels.push_back(make_pair(plotID,plotData->dataLabel));
					break;
				}
				case STREAM_TYPE_DRAW:
				{
					DrawStreamData *drawData;
					drawData=((DrawStreamData *)((*it)[ui]));
					
					//Retrieve vector
					const std::vector<DrawableObj *> *drawObjs;
					drawObjs = &(drawData->drawables);
					//Loop through vector, Adding each object to the scene
					if(drawData->cached)
					{
						//Create a *copy* for scene. Filter still holds
						//originals, and will dispose of the pointers accordingly
						for(unsigned int ui=0;ui<drawObjs->size();ui++)
							sceneDrawables.push_back((*drawObjs)[ui]->clone());
					}
					else
					{
						//Place the *pointers* to the drawables in the scene
						// list, to avoid copying
						for(unsigned int ui=0;ui<drawObjs->size();ui++)
							sceneDrawables.push_back((*drawObjs)[ui]);

						//Although we do not delete the drawable objects
						//themselves, we do delete the container
						
						//Zero-size the internal vector to 
						//prevent vector destructor from deleting pointers
						//we have transferred ownership of to scene
						drawData->drawables.clear();
						deleteIt=true;
					}
					break;
				}
				case STREAM_TYPE_RANGE:
					//silently drop rangestreams
					break;


				case STREAM_TYPE_OPENVDBGRID:
				{
					OpenVDBGridStreamData *vdbSrc = (OpenVDBGridStreamData *)((*it)[ui]);

					openvdb::initialize();
					
					openvdb::FloatGrid::Ptr vis_grid = openvdb::FloatGrid::create();

					vis_grid = vdbSrc->grid->deepCopy();


					if (vdbSrc->representationType == VOXEL_REPRESENT_ISOSURF)
					{
					
						LukasDrawIsoSurface *ld = new LukasDrawIsoSurface;
						ld->setGrid(vis_grid);
						ld->setColour(vdbSrc->r,vdbSrc->g,
								vdbSrc->b,vdbSrc->a);
						ld->setIsovalue(vdbSrc->isovalue);
						ld->setVoxelsize(vdbSrc->voxelsize);

						ld->wantsLight=true;

						sceneDrawables.push_back(ld);
					}
					else
					{
							ASSERT(false);
							delete &vis_grid;
							break;
					}

					break;
				

				}

				case STREAM_TYPE_VOXEL:
				{
					//Technically, we are violating const-ness
					VoxelStreamData *vSrc = (VoxelStreamData *)((*it)[ui]);
					//Create a new Field3D
					Voxels<float> *v = new Voxels<float>;

					//Make a copy if cached; otherwise just steal it.
					if(vSrc->cached)
						vSrc->data->clone(*v);
					else
						v->swap(*(vSrc->data));

					if (vSrc->representationType == VOXEL_REPRESENT_POINTCLOUD)
					{

						DrawField3D  *d = new DrawField3D;
						d->setField(v);
						d->setColourMapID(0);
						d->setColourMinMax();
						d->setBoxColours(vSrc->r,vSrc->g,vSrc->b,vSrc->a);
						d->setPointSize(vSrc->splatSize);
						d->setAlpha(vSrc->a);
						d->wantsLight=false;

						sceneDrawables.push_back(d);
					}

					else
					{
							ASSERT(false);
							delete v;
							break;
					}
					
					break;

				}
			}
			
			//delete drawables as needed
			if( (!(*it)[ui]->cached && releaseData) || deleteIt)
			{
				//Ensure that we didnt force deletion of a cached obejct
				ASSERT(deleteIt != (*it)[ui]->cached);
				delete (*it)[ui];
				(*it)[ui]=0;	
			}
			

		}
			
	}

	//Free the rate-limited points
	for(map<const IonStreamData *, const IonStreamData*>::iterator it =throttleMap.begin();
		it!=throttleMap.end();++it)
	{
		delete it->second;
	}
	throttleMap.clear();	
	//---

	//Construct an OpenGL display list from the dataset

	//Check how many points we have. Too many can cause the display list to crash
	size_t totalIonCount=0;
	for(unsigned int ui=0;ui<sceneDrawables.size();ui++)
	{
		if(sceneDrawables[ui]->getType() == DRAW_TYPE_MANYPOINT)
			totalIonCount+=((const DrawManyPoints*)(sceneDrawables[ui]))->getNumPts();
	}
	
	
	//Must lock UI controls, or not run callback from here on in!
	//==========

	//Update the plotting UI contols
	//-----------
	plotSelList->Clear(); // erase wx list
	plotMap.clear();
	for(size_t ui=0;ui<plotLabels.size();ui++)
	{
		//Append the plot to the list in the user interface
		plotSelList->Append((plotLabels[ui].second));
		plotMap[ui] = plotLabels[ui].first;
	}

	//If there is only one spectrum, select it
	if(plotSelList->GetCount() == 1 )
		plotSelList->SetSelection(0);
	else if( plotSelList->GetCount() > 1)
	{
		//Otherwise try to use the last visibility information
		//to set the selection
		targetPlots.bestEffortRestoreVisibility();

	}

	for(unsigned int ui=0; ui<plotSelList->GetCount();ui++)
	{
#if defined(__WIN32__) || defined(__WIN64__)
		//Bug under windows. SetSelection(wxNOT_FOUND) does not work for multi-selection list boxes
		plotSelList->SetSelection(-1, false);
#else
 		plotSelList->SetSelection(wxNOT_FOUND); //Clear selection
#endif
		for(unsigned int ui=0; ui<plotSelList->GetCount();ui++)
		{
			//Retrieve the uniqueID
			unsigned int plotID;
			plotID=plotMap[ui];
			if(targetPlots.isPlotVisible(plotID))
				plotSelList->SetSelection(ui);
		}
	}
	targetPlots.lockInteraction(false);
	//-----------
		

	
	scene.clearObjs();
	scene.clearRefObjs();



	//For speed, we have to treat ions specially.
	// for now, use a display list (these are no longer recommended in opengl, 
	// but they are much easier to use than extensions)
	vector<DrawManyPoints *> drawIons;
	for(size_t ui=0;ui<sceneDrawables.size();ui++)
	{
		if(sceneDrawables[ui]->getType() == DRAW_TYPE_MANYPOINT)
		{
			drawIons.push_back((DrawManyPoints*)sceneDrawables[ui]);
			sceneDrawables.erase(sceneDrawables.begin()+ui);
			ui--;

		}
	}

	if(totalIonCount < MAX_NUM_DRAWABLE_POINTS && drawIons.size() >1)
	{
		//Try to use a display list where we can.
		//note that the display list requires a valid bounding box,
		//so single point entities, or overlapped points can
		//produce an invalid bounding box
		DrawDispList *displayList;
		displayList = new DrawDispList();

		bool listStarted=false;
		for(unsigned int ui=0;ui<drawIons.size(); ui++)
		{
			BoundCube b;
			drawIons[ui]->getBoundingBox(b);
			if(b.isValid())
			{

				if(!listStarted)
				{
					displayList->startList(false);
					listStarted=true;
				}
				displayList->addDrawable(drawIons[ui]);
				delete drawIons[ui];
			}
			else
				scene.addDrawable(drawIons[ui]);
		}

		if(listStarted)	
		{
			displayList->endList();
			scene.addDrawable(displayList);
		}
		else
			delete displayList;
	}
	else
	{
		for(unsigned int ui=0;ui<drawIons.size(); ui++)
			scene.addDrawable(drawIons[ui]);
	}

	//add all drawable objects (not ions)	
	for(size_t ui=0;ui<sceneDrawables.size();ui++)
		scene.addDrawable(sceneDrawables[ui]);
	
	sceneDrawables.clear();
	scene.computeSceneLimits();
	scene.lockInteraction(false);
	//===============
}

void VisController::throttleSceneInput(list<vector<const FilterStreamData *> > &sceneData,
		std::map<const IonStreamData *,const IonStreamData *> &throttleMap) const
{
	//Count the number of input ions, as we may need to perform culling,
	if(!limitIonOutput)
		return;

	size_t inputIonCount=0;
	for(list<vector<const FilterStreamData *> >::const_iterator it=sceneData.begin(); 
							it!=sceneData.end(); ++it)
		inputIonCount+=numElements(*it,STREAM_TYPE_IONS);

	//If limit is higher than what we have, no culling required
	if(limitIonOutput >=inputIonCount)
		return;

	//Need to cull
	float cullFraction = (float)limitIonOutput/(float)inputIonCount;

	for(list<vector<const FilterStreamData *> >::iterator it=sceneData.begin(); 
							it!=sceneData.end(); ++it)
	{
		for(unsigned int ui=0;ui<it->size(); ui++)
		{
			if((*it)[ui]->getStreamType() != STREAM_TYPE_IONS)
				continue;
			//Obtain the ion data pointer
			const IonStreamData *ionData;
			ionData=((const IonStreamData *)((*it)[ui]));


			//Duplicate this object, then forget
			// about the old one. The freeing will be done by
			//the refresh thread as needed, so don't free here.

			// We can't modify the input, even when uncached,
			// as the object is const
			IonStreamData *newIonData;
			newIonData=ionData->cloneSampled(cullFraction);
			throttleMap[ionData]  = newIonData;

		}
	}
		


}

void VisController::updateRawGrid() const
{
	vector<vector<vector<float> > > plotData;
	vector<std::vector<std::string> > labels;
	//grab the data for the currently visible plots
	targetPlots.getRawData(plotData,labels);



	//Clear the grid
	if(targetRawGrid->GetNumberCols())
		targetRawGrid->DeleteCols(0,targetRawGrid->GetNumberCols());
	if(targetRawGrid->GetNumberRows())
		targetRawGrid->DeleteRows(0,targetRawGrid->GetNumberRows());
	
	unsigned int curCol=0;
	for(unsigned int ui=0;ui<plotData.size(); ui++)
	{
		unsigned int startCol;
		//Create new columns
		targetRawGrid->AppendCols(plotData[ui].size());
		ASSERT(labels[ui].size() == plotData[ui].size());

		startCol=curCol;
		for(unsigned int uj=0;uj<labels[ui].size();uj++)
		{
			std::string s;
			s=(labels[ui][uj]);
			targetRawGrid->SetColLabelValue(curCol,(s));
			curCol++;
		}

		//set the data
		for(unsigned int uj=0;uj<plotData[ui].size();uj++)
		{
			//Check to see if we need to add rows to make our data fit
			if(plotData[ui][uj].size() > targetRawGrid->GetNumberRows())
				targetRawGrid->AppendRows(plotData[ui][uj].size()-targetRawGrid->GetNumberRows());

			for(unsigned int uk=0;uk<plotData[ui][uj].size();uk++)
			{
				std::string tmpStr;
				stream_cast(tmpStr,plotData[ui][uj][uk]);
				targetRawGrid->SetCellValue(uk,startCol,(tmpStr));
			}
			startCol++;
		}

	}
}

void VisController::updateWxTreeCtrl(wxTreeCtrl *t, const Filter *visibleFilt)
{

	map<size_t,Filter *> filterMap; 
	upWxTreeCtrl(state.treeState.getTreeRef(),t,	
			filterMap,persistentFilters,visibleFilt);

	cerr << "Rebuilt filter map" <<endl;
	state.treeState.swapFilterMap(filterMap);
}


void VisController::updateStashComboBox(wxComboBox *comboStash) const
{
	//HACK: Calling ->Clear() under MSW causes a crash
	while(comboStash->GetCount())
		comboStash->Delete(0);

	unsigned int nStashes = state.getStashCount();	
	for(unsigned int ui=0;ui<nStashes; ui++)
	{
		wxListUint *u;
		u = new wxListUint(ui);
		std::string stashName;
		stashName=state.getStashName(ui);
		comboStash->Append(stashName,(wxClientData *)u);
		ASSERT(comboStash->GetClientObject(comboStash->GetCount()-1));
	}
}

void VisController::updateCameraComboBox(wxComboBox *comboCamera) const
{
	//Update the camera dropdown
	//HACK: Calling ->Clear() under MSW causes a crash
	while(comboCamera->GetCount())
		comboCamera->Delete(0);
	
	size_t nCams = state.getNumCams();
	//The start from 1 is a hack to avoid the unnamed camera
	for(unsigned int ui=1;ui<nCams;ui++)
	{
		std::string camName;
		camName = state.getCamName(ui);
		ASSERT(camName.size());
		//Do not delete as this will be deleted by wx?
		//FIXME: ListUInt is leaking... Is this a wx bug, or a usage bug?
		comboCamera->Append(camName,
				(wxClientData *)new wxListUint(ui));	
		//If this is the active cam (1) set the selection and (2) remember
		//the ID
		if(ui == state.getActiveCam())
			comboCamera->SetSelection(ui-1);
	}

}

size_t VisController::getPlotID(size_t position) const
{ 
	ASSERT(plotMap.size());
	ASSERT(plotMap.find(position)!=plotMap.end());
	return plotMap.find(position)->second;
}


void VisController::setActiveCam(unsigned int newActive) 
{
	//set the active camera in state, then clone it to the scene
	state.setActiveCam(newActive);
	
	scene.setActiveCam(state.getCam(newActive)->clone());
}
