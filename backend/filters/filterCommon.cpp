/*
 *	filterCommon.cpp - Helper routines for filter classes
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

#include "filterCommon.h"

#include "common/colourmap.h"
#include "wx/wxcommon.h"


//TODO: Work out where the payoff for this is
//grab size when doing convex hull calculations
const unsigned int HULL_GRAB_SIZE=4096;

using std::ostream;
using std::vector;
using std::endl;
using std::string;

bool qhullInited=false;

//Wrapper for qhull single-pass run
unsigned int doHull(unsigned int bufferSize, double *buffer, 
			vector<Point3D> &resHull, Point3D &midPoint,	
			bool wantVolume, bool freeHullOnExit);

void writeVectorsXML(ostream &f,const char *containerName,
		const vector<Point3D> &vectorParams, unsigned int depth)
{
	f << tabs(depth+1) << "<" << containerName << ">" << endl;
	for(unsigned int ui=0; ui<vectorParams.size(); ui++)
	{
		f << tabs(depth+2) << "<point3d x=\"" << vectorParams[ui][0] << 
			"\" y=\"" << vectorParams[ui][1] << "\" z=\"" << vectorParams[ui][2] << "\"/>" << endl;
	}
	f << tabs(depth+1) << "</" << containerName << ">" << endl;
}

void writeIonsEnabledXML(ostream &f, const char *containerName, 
		const vector<bool> &enabledState, const vector<string> &names, 
			unsigned int depth)
{
	if(enabledState.size()!=names.size())
		return;

	f << tabs(depth) << "<" << containerName << ">"  << endl;
	for(size_t ui=0;ui<enabledState.size();ui++)
	{
		f<< tabs(depth+1) << "<ion enabled=\"" << (int)enabledState[ui] 
			<< "\" name=\"" << names[ui] << "\"/>" <<  std::endl; 
	}
	f << tabs(depth) << "</" << containerName << ">"  << endl;
}

void readIonsEnabledXML(xmlNodePtr nodePtr,  vector<bool> &enabledStatus,vector<string> &ionNames)
{
	//skip conatainer name
	nodePtr=nodePtr->xmlChildrenNode;

	if(!nodePtr)
		return;

	enabledStatus.clear();
	while(!XMLHelpFwdToElem(nodePtr,"ion"))
	{
		int enabled;
		if(!XMLGetAttrib(nodePtr,enabled,"enabled"))
			return ;

		std::string tmpName;
		if(!XMLGetAttrib(nodePtr,tmpName,"name"))
			return;
	
		enabledStatus.push_back(enabled);
		
		ionNames.push_back(tmpName);
	}
	
}
bool readVectorsXML(xmlNodePtr nodePtr,	std::vector<Point3D> &vectorParams) 
{
	nodePtr=nodePtr->xmlChildrenNode;
	vectorParams.clear();
	
	while(!XMLHelpFwdToElem(nodePtr,"point3d"))
	{
		std::string tmpStr;
		xmlChar* xmlString;
		float x,y,z;
		//--Get X value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"x");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(x,tmpStr))
			return false;

		//--Get Z value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"y");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(y,tmpStr))
			return false;

		//--Get Y value--
		xmlString=xmlGetProp(nodePtr,(const xmlChar *)"z");
		if(!xmlString)
			return false;
		tmpStr=(char *)xmlString;
		xmlFree(xmlString);

		//Check it is streamable
		if(stream_cast(z,tmpStr))
			return false;

		vectorParams.push_back(Point3D(x,y,z));
	}

	return true;
}

bool parseXMLColour(xmlNodePtr &nodePtr, ColourRGBAf &rgba)
{
	xmlChar *xmlString;

	float r,g,b,a;
	std::string tmpStr;
	//--red--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"r");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(r,tmpStr))
		return false;

	//disallow negative or values gt 1.
	if(r < 0.0f || r > 1.0f)
		return false;
	xmlFree(xmlString);


	//--green--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"g");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(g,tmpStr))
	{
		xmlFree(xmlString);
		return false;
	}
	
	xmlFree(xmlString);

	//disallow negative or values gt 1.
	if(g < 0.0f || g > 1.0f)
		return false;

	//--blue--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"b");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(b,tmpStr))
	{
		xmlFree(xmlString);
		return false;
	}
	xmlFree(xmlString);
	
	//disallow negative or values gt 1.
	if(b < 0.0f || b > 1.0f)
		return false;

	//--Alpha--
	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"a");
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	//convert from string to digit
	if(stream_cast(a,tmpStr))
	{
		xmlFree(xmlString);
		return false;
	}
	xmlFree(xmlString);

	//disallow negative or values gt 1.
	if(a < 0.0f || a > 1.0f)
		return false;
	rgba.r(r);
	rgba.g(g);
	rgba.b(b);
	rgba.a(a);

	return true;
}

const RangeFile *getRangeFile(const std::vector<const FilterStreamData*> &dataIn)
{
	for(size_t ui=0;ui<dataIn.size();ui++)
	{
		if(dataIn[ui]->getStreamType() == STREAM_TYPE_RANGE)
			return ((const RangeStreamData*)(dataIn[ui]))->rangeFile;
	}

	ASSERT(false);
}

unsigned int getIonstreamIonID(const IonStreamData *d, const RangeFile *r)
{
	if(d->data.empty())
		return (unsigned int)-1;

	unsigned int tentativeRange;

	tentativeRange=r->getIonID(d->data[0].getMassToCharge());


	//TODO: Currently, we have no choice but to brute force it.
	//In the future, it might be worth storing some data inside the IonStreamData itself
	//and to use that first, rather than try to brute force the result
#ifdef _OPENMP
	bool spin=false;
	#pragma omp parallel for shared(spin)
	for(size_t ui=1;ui<d->data.size();ui++)
	{
		if(spin)
			continue;
		if(r->getIonID(d->data[ui].getMassToCharge()) !=tentativeRange)
			spin=true;
	}

	//Not a range
	if(spin)
		return (unsigned int)-1;

#else
	for(size_t ui=1;ui<d->data.size();ui++)
	{
		if(r->getIonID(d->data[ui].getMassToCharge()) !=tentativeRange)
			return (unsigned int)-1;
	}
#endif

	return tentativeRange;	
}


//FIXME: Abort pointer?
unsigned int computeConvexHull(const vector<const FilterStreamData*> &data, unsigned int *progress,
					std::vector<Point3D> &curHull, bool wantVolume,bool freeHull)
{

	size_t numPts;
	numPts=numElements(data,STREAM_TYPE_IONS);
	//Easy case of no data
	if(numPts < 4)
		return 0;

	double *buffer;
	double *tmp;
	//Use malloc so we can re-alloc
	buffer =(double*) malloc(HULL_GRAB_SIZE*3*sizeof(double));

	if(!buffer)
		return HULL_ERR_NO_MEM;

	size_t bufferOffset=0;

	//Do the convex hull in steps for two reasons
	// 1) qhull chokes on large data
	// 2) we need to check for abort every now and again, so we have to
	//   work in batches.
	Point3D midPoint;
	float maxSqrDist=-1;
	size_t n=0;
	for(size_t ui=0; ui<data.size(); ui++)
	{
		if(data[ui]->getStreamType() != STREAM_TYPE_IONS)
			continue;

		const IonStreamData* ions=(const IonStreamData*)data[ui];

		for(size_t uj=0; uj<ions->data.size(); uj++)
		{		//Do contained-in-sphere check
			if(!curHull.size() || midPoint.sqrDist(ions->data[uj].getPos())>= maxSqrDist)
			{
				//Copy point data into hull buffer
				buffer[3*bufferOffset]=ions->data[uj].getPos()[0];
				buffer[3*bufferOffset+1]=ions->data[uj].getPos()[1];
				buffer[3*bufferOffset+2]=ions->data[uj].getPos()[2];
				bufferOffset++;

				//If we have hit the hull grab size, perform a hull

				if(bufferOffset == HULL_GRAB_SIZE)
				{
					bufferOffset+=curHull.size();
					tmp=(double*)realloc(buffer,
							     3*bufferOffset*sizeof(double));
					if(!tmp)
					{
						free(buffer);

						return HULL_ERR_NO_MEM;
					}

					buffer=tmp;
					//Copy in the old hull
					for(size_t uk=0; uk<curHull.size(); uk++)
					{
						buffer[3*(HULL_GRAB_SIZE+uk)]=curHull[uk][0];
						buffer[3*(HULL_GRAB_SIZE+uk)+1]=curHull[uk][1];
						buffer[3*(HULL_GRAB_SIZE+uk)+2]=curHull[uk][2];
					}

					unsigned int errCode=0;
					
					errCode=doHull(bufferOffset,buffer,curHull,midPoint,wantVolume,freeHull);
					if(errCode)
					{
						free(buffer);
						return errCode;
					}


					//Now compute the min sqr distance
					//to the vertex, so we can fast-reject
					maxSqrDist=std::numeric_limits<float>::max();
					for(size_t ui=0; ui<curHull.size(); ui++)
						maxSqrDist=std::min(maxSqrDist,curHull[ui].sqrDist(midPoint));
					//reset buffer size
					bufferOffset=0;
				}

			}
			n++;

			//Update the progress information, and run abort check
			if(*Filter::wantAbort)
			{
				free(buffer);
				return HULL_ERR_USER_ABORT;
			}

			*progress= (unsigned int)((float)(n)/((float)numPts)*100.0f);

		}
	}

	//Need at least 4 objects to construct a sufficiently large buffer
	if(bufferOffset + curHull.size() > 4)
	{
		//Re-allocate the buffer to determine the last hull size
		tmp=(double*)realloc(buffer,
		                     3*(bufferOffset+curHull.size())*sizeof(double));
		if(!tmp)
		{
			free(buffer);
			return HULL_ERR_NO_MEM;
		}
		buffer=tmp;

		#pragma omp parallel for
		for(unsigned int ui=0; ui<curHull.size(); ui++)
		{
			buffer[3*(bufferOffset+ui)]=curHull[ui][0];
			buffer[3*(bufferOffset+ui)+1]=curHull[ui][1];
			buffer[3*(bufferOffset+ui)+2]=curHull[ui][2];
		}

		unsigned int errCode=doHull(bufferOffset+curHull.size(),buffer,
						curHull,midPoint,wantVolume,freeHull);

		if(errCode)
		{
			free(buffer);
			//FIXME: Free the last convex hull mem??
			return errCode;
		}
	}


	free(buffer);
	return 0;
}

unsigned int computeConvexHull(const vector<Point3D> &data, unsigned int *progress,
				const bool &abortPtr,std::vector<Point3D> &curHull, bool wantVolume, bool freeHull)
{

	//Easy case of no data
	if(data.size()< 4)
		return 0;

	double *buffer;
	double *tmp;
	//Use malloc so we can re-alloc
	buffer =(double*) malloc(HULL_GRAB_SIZE*3*sizeof(double));

	if(!buffer)
		return HULL_ERR_NO_MEM;

	size_t bufferOffset=0;

	//Do the convex hull in steps for two reasons
	// 1) qhull chokes on large data
	// 2) we need to run the callback every now and again, so we have to
	//   work in batches.
	Point3D midPoint;
	float maxSqrDist=-1;



	for(size_t uj=0; uj<data.size(); uj++)
	{
		//Do contained-in-sphere check
		if(!curHull.size() || midPoint.sqrDist(data[uj])>= maxSqrDist)
		{
		
			//Copy point data into hull buffer
			buffer[3*bufferOffset]=data[uj][0];
			buffer[3*bufferOffset+1]=data[uj][1];
			buffer[3*bufferOffset+2]=data[uj][2];
			bufferOffset++;

			//If we have hit the hull grab size, perform a hull

			if(bufferOffset == HULL_GRAB_SIZE)
			{
				bufferOffset+=curHull.size();
				tmp=(double*)realloc(buffer,
						     3*bufferOffset*sizeof(double));
				if(!tmp)
				{
					free(buffer);

					return HULL_ERR_NO_MEM;
				}

				buffer=tmp;
				//Copy in the old hull
				for(size_t uk=0; uk<curHull.size(); uk++)
				{
					buffer[3*(HULL_GRAB_SIZE+uk)]=curHull[uk][0];
					buffer[3*(HULL_GRAB_SIZE+uk)+1]=curHull[uk][1];
					buffer[3*(HULL_GRAB_SIZE+uk)+2]=curHull[uk][2];
				}

				unsigned int errCode=0;
				
				errCode=doHull(bufferOffset,buffer,curHull,midPoint,wantVolume,freeHull);
				if(errCode)
					return errCode;


				//Now compute the min sqr distance
				//to the vertex, so we can fast-reject
				maxSqrDist=std::numeric_limits<float>::max();
				for(size_t ui=0; ui<curHull.size(); ui++)
					maxSqrDist=std::min(maxSqrDist,curHull[ui].sqrDist(midPoint));
				//reset buffer size
				bufferOffset=0;
			}
		}

		if(*Filter::wantAbort)
		{
			free(buffer);
			return HULL_ERR_USER_ABORT;
		}

		*progress= (unsigned int)((float)(uj)/((float)data.size())*100.0f);

	}


	//Build the final hull, using the remaining points, and the
	// filtered hull points
	//Need at least 4 objects to construct a sufficiently large buffer
	if(bufferOffset + curHull.size() > 4)
	{
		//Re-allocate the buffer to determine the last hull size
		tmp=(double*)realloc(buffer,
		                     3*(bufferOffset+curHull.size())*sizeof(double));
		if(!tmp)
		{
			free(buffer);
			return HULL_ERR_NO_MEM;
		}
		buffer=tmp;

		#pragma omp parallel for
		for(unsigned int ui=0; ui<curHull.size(); ui++)
		{
			buffer[3*(bufferOffset+ui)]=curHull[ui][0];
			buffer[3*(bufferOffset+ui)+1]=curHull[ui][1];
			buffer[3*(bufferOffset+ui)+2]=curHull[ui][2];
		}

		unsigned int errCode=doHull(bufferOffset+curHull.size(),buffer,curHull,midPoint,wantVolume,freeHull);

		if(errCode)
		{
			free(buffer);
			//Free the last convex hull mem
			return errCode;
		}
	}


	free(buffer);
	return 0;
}

unsigned int doHull(unsigned int bufferSize, double *buffer, 
			vector<Point3D> &resHull, Point3D &midPoint, bool wantVolume ,
			bool freeHullOnExit)
{
	if(qhullInited)
	{
		qh_freeqhull(qh_ALL);
		int curlong,totlong;
		//This seems to be required? Cannot find any documentation on the difference
		// between qh_freeqhull and qh_memfreeshort. qhull appears to leak when just using qh_freeqhull
		qh_memfreeshort (&curlong, &totlong);    
		qhullInited=false;
	}


	const int dim=3;
	//Now compute the new hull
	//Generate the convex hull
	//(result is stored in qh's globals :(  )
	//note that the input is "joggled" to 
	//ensure simplicial facet generation

	//Qhull >=2012 has a "feature" where it won't accept null arguments for the output
	// there is no clear way to shut it up.
	FILE *outSquelch=0;
#if defined(__linux__) || defined(__APPLE__) || defined(__BSD__)
	outSquelch=fopen("/dev/null","w");
#elif defined(__win32__) || defined(__win64__)
	outSquelch=fopen("NUL","w");
#endif

	if(!outSquelch)
	{
		//Give up, just let qhull output random statistics to stderr
		outSquelch=stderr;
	}

	 //Joggle the output, such that only simplical facets are generated, Also compute area/volume
	const char *argsOptions[]= { "qhull QJ FA",
				   "qhull QJ"
					};

	const char *args;
	if(wantVolume)
		args = argsOptions[0];
	else
		args=argsOptions[1];
	

	qh_new_qhull(	dim,
			bufferSize,
			buffer,
			false,
			(char *)args ,
			outSquelch, //QHULL's interface is bizarre, no way to set null pointer in qhull 2012 - result is inf. loop in qhull_fprintf and error reporting func. 
			outSquelch);
	qhullInited=true;

	if(outSquelch !=stderr)
	{
		fclose(outSquelch);
	}

	unsigned int numPoints=0;
	//count points
	//--	
	//OKay, whilst this may look like invalid syntax,
	//qh is actually a macro from qhull
	//that creates qh. or qh-> as needed
	numPoints = qh num_vertices;	
	//--	
	midPoint=Point3D(0,0,0);	

	if(!numPoints)
		return 0;

	//store points in vector
	//--
	try
	{
		resHull.resize(numPoints);	
	}
	catch(std::bad_alloc)
	{
		return HULL_ERR_NO_MEM;
	}
	//--

	//Compute mean point
	//--
	vertexT *vertex;
	vertex= qh vertex_list;
	int curPt=0;
	while(vertex != qh vertex_tail)
	{
		resHull[curPt]=Point3D(vertex->point[0],
				vertex->point[1],
				vertex->point[2]);
		midPoint+=resHull[curPt];
		curPt++;
		vertex = vertex->next;
	}
	midPoint*=1.0f/(float)numPoints;
	//--
	if(freeHullOnExit)
	{
		qh_freeqhull(qh_ALL);
		int curlong,totlong;
		//This seems to be required? Cannot find any documentation on the difference
		// between qh_freeqhull and qh_memfreeshort. qhull appears to leak when just using qh_freeqhull
		qh_memfreeshort (&curlong, &totlong);    
		qhullInited=false;
	
	}

	return 0;
}


void freeConvexHull()
{
	qh_freeqhull(qh_ALL);
	int curlong,totlong;
	//This seems to be required? Cannot find any documentation on the difference
	// between qh_freeqhull and qh_memfreeshort. qhull appears to leak when just using qh_freeqhull
	qh_memfreeshort (&curlong, &totlong);    
	qhullInited=false;
}

DrawColourBarOverlay *makeColourBar(float minV, float maxV,size_t nColours,size_t colourMap, bool reverseMap, float alpha) 
{
	//Set up the colour bar. Place it in a draw stream type
	DrawColourBarOverlay *dc = new DrawColourBarOverlay;

	vector<float> r,g,b;
	r.resize(nColours);
	g.resize(nColours);
	b.resize(nColours);

	for (unsigned int ui=0;ui<nColours;ui++)
	{
		unsigned char rgb[3]; //RGB array
		float value;
		value = (float)(ui)*(maxV-minV)/(float)nColours + minV;
		//Pick the desired colour map
		colourMapWrap(colourMap,rgb,value,minV,maxV,reverseMap);
		r[ui]=rgb[0]/255.0f;
		g[ui]=rgb[1]/255.0f;
		b[ui]=rgb[2]/255.0f;
	}

	dc->setColourVec(r,g,b);

	dc->setSize(0.08,0.6);
	dc->setPosition(0.1,0.1);
	dc->setMinMax(minV,maxV);
	dc->setAlpha(alpha);

	return dc;
}

//creates a temporary filename for use
std::string createTmpFilename(const char *dir,const char *extension)
{
	wxString tmpFilename,tmpDir;
	
	if(!dir)
	{
		tmpDir=wxFileName::GetTempDir();


	#if defined(__WIN32__) || defined(__WIN64__)
		tmpDir=tmpDir + wxT("\\3Depict\\");

	#else
		tmpDir=tmpDir + wxT("/3Depict/");
	#endif

	}
	else
		tmpDir=dir;
	
	if(!wxDirExists(tmpDir))
		wxMkdir(tmpDir);
	tmpFilename=wxFileName::CreateTempFileName(tmpDir+ wxT("unittest-"));
	wxRemoveFile(tmpFilename);
	if(extension)
		tmpFilename+=wxT(".pos");

	return stlStr(tmpFilename);
}

