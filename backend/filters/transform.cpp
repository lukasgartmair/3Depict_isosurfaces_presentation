/*
 *	transform.cpp - Perform geometrical transform operations on point clouds
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
#include "transform.h"

#include "filterCommon.h"

using std::vector;
using std::string;
using std::pair;
using std::make_pair;


enum
{
	KEY_MODE,
	KEY_SCALEFACTOR,
	KEY_SCALEFACTOR_ANISOTROPIC,
	KEY_ORIGIN,
	KEY_TRANSFORM_SHOWORIGIN,
	KEY_ORIGINMODE,
	KEY_NOISELEVEL,
	KEY_NOISETYPE,
	KEY_ROTATE_ANGLE,
	KEY_ROTATE_AXIS,
	KEY_ORIGIN_VALUE,
	KEY_CROP_MINIMUM,
	KEY_CROP_MAXIMUM,
};

//Possible transform modes (scaling, rotation etc)
enum
{
	MODE_TRANSLATE,
	MODE_SCALE_ISOTROPIC,
	MODE_SCALE_ANISOTROPIC,
	MODE_ROTATE,
	MODE_VALUE_SHUFFLE,
	MODE_SPATIAL_NOISE,
	MODE_TRANSLATE_VALUE,
	MODE_CROP_VALUE,
	MODE_ENUM_END
};

//!Possible mode for selection of origin in transform filter
enum
{
	ORIGINMODE_SELECT,
	ORIGINMODE_CENTREBOUND,
	ORIGINMODE_MASSCENTRE,
	ORIGINMODE_END, // Not actually origin mode, just end of enum
};

//!Possible noise modes
enum
{
	NOISETYPE_GAUSSIAN,
	NOISETYPE_WHITE,
	NOISETYPE_END
};

//!Error codes
enum
{
	
	ERR_NOMEM=1,
	TRANSFORM_ERR_ENUM_END
};

const char *TRANSFORM_MODE_STRING[] = { NTRANS("Translate"),
					NTRANS("Scale (isotropic)"),
					NTRANS("Scale (anisotropic)"),
					NTRANS("Rotate"),
					NTRANS("Value Shuffle"),
					NTRANS("Spatial Noise"),
					NTRANS("Translate Value"),
					NTRANS("Crop Value")
					};

const char *TRANSFORM_ORIGIN_STRING[]={ 
					NTRANS("Specify"),
					NTRANS("Boundbox Centre"),
					NTRANS("Mass Centre")
					};
					
	
	
//=== Transform filter === 
TransformFilter::TransformFilter()
{
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(TRANSFORM_MODE_STRING) == MODE_ENUM_END);
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(TRANSFORM_ORIGIN_STRING) == ORIGINMODE_END);

	randGen.initTimer();
	transformMode=MODE_TRANSLATE;
	originMode=ORIGINMODE_SELECT;
	noiseType=NOISETYPE_WHITE;
	//Set up default value
	vectorParams.resize(1);
	vectorParams[0] = Point3D(0,0,0);
	
	showPrimitive=true;
	showOrigin=false;

	cacheOK=false;
	cache=false; 
}

Filter *TransformFilter::cloneUncached() const
{
	TransformFilter *p=new TransformFilter();

	//Copy the values
	p->vectorParams.resize(vectorParams.size());
	p->scalarParams.resize(scalarParams.size());
	
	std::copy(vectorParams.begin(),vectorParams.end(),p->vectorParams.begin());
	std::copy(scalarParams.begin(),scalarParams.end(),p->scalarParams.begin());

	p->showPrimitive=showPrimitive;
	p->originMode=originMode;
	p->transformMode=transformMode;
	p->showOrigin=showOrigin;
	p->noiseType=noiseType;
	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	return p;
}

size_t TransformFilter::numBytesForCache(size_t nObjects) const
{
	return nObjects*sizeof(IonHit);
}

DrawStreamData* TransformFilter::makeMarkerSphere(SelectionDevice* &s) const
{
	//construct a new primitive, do not cache
	DrawStreamData *drawData=new DrawStreamData;
	drawData->parent=this;
	//Add drawable components
	DrawSphere *dS = new DrawSphere;
	dS->setOrigin(vectorParams[0]);
	dS->setRadius(1);
	//FIXME: Alpha blending is all screwed up. May require more
	//advanced drawing in scene. (front-back drawing).
	//I have set alpha=1 for now.
	dS->setColour(0.2,0.2,0.8,1.0);
	dS->setLatSegments(40);
	dS->setLongSegments(40);
	dS->wantsLight=true;
	drawData->drawables.push_back(dS);

	s=0;
	//Set up selection "device" for user interaction
	//Note the order of s->addBinding is critical,
	//as bindings are selected by first match.
	//====
	//The object is selectable
	if (originMode == ORIGINMODE_SELECT )
	{
		dS->canSelect=true;

		s=new SelectionDevice(this);
		SelectionBinding b;

		b.setBinding(SELECT_BUTTON_LEFT,0,DRAW_SPHERE_BIND_ORIGIN,
		             BINDING_SPHERE_ORIGIN,dS->getOrigin(),dS);
		b.setInteractionMode(BIND_MODE_POINT3D_TRANSLATE);
		s->addBinding(b);

	}
	drawData->cached=0;	

	return drawData;
}

unsigned int TransformFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		//Propagate non-ion-types into output
		propagateStreams(dataIn,getOut, getRefreshBlockMask(),true);
		propagateCache(getOut);
		return 0;
	}


	//The user is allowed to choose the mode by which the origin is computed
	//so set the origin variable depending upon this
	switch(originMode)
	{
		case ORIGINMODE_CENTREBOUND:
		{
			BoundCube masterB;
			masterB.setInverseLimits();
			#pragma omp parallel for
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				BoundCube thisB;

				if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
				{
					const IonStreamData* ions;
					ions = (const IonStreamData*)dataIn[ui];
					if(ions->data.size())
					{
						IonHit::getBoundCube(ions->data,thisB);
						#pragma omp critical
						masterB.expand(thisB);
					}
				}
			}

			if(!masterB.isValid())
				vectorParams[0]=Point3D(0,0,0);
			else
				vectorParams[0]=masterB.getCentroid();
			break;
		}
		case ORIGINMODE_MASSCENTRE:
		{
			Point3D massCentre(0,0,0);
			size_t numCentres=0;
			#pragma omp parallel for
			for(unsigned int ui=0;ui<dataIn.size() ;ui++)
			{
				Point3D massContrib;
				if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
				{
					const IonStreamData* ions;
					ions = (const IonStreamData*)dataIn[ui];

					if(ions->data.size())
					{
						Point3D thisCentre;
						thisCentre=Point3D(0,0,0);
						for(unsigned int uj=0;uj<ions->data.size();uj++)
							thisCentre+=ions->data[uj].getPosRef();
						massContrib=thisCentre*1.0/(float)ions->data.size();
						#pragma omp critical
						massCentre+=massContrib;
						numCentres++;
					}
				}
			}
			vectorParams[0]=massCentre*1.0/(float)numCentres;
			break;

		}
		case ORIGINMODE_SELECT:
			break;
		default:
			ASSERT(false);
	}

	//If the user is using a transform mode that requires origin selection 
	if(showOrigin && (transformMode == MODE_ROTATE ||
			transformMode == MODE_SCALE_ANISOTROPIC || 
			transformMode == MODE_SCALE_ISOTROPIC) )
	{
		SelectionDevice *s;
		DrawStreamData *d=makeMarkerSphere(s);
		if(s)
			devices.push_back(s);

		cacheAsNeeded(d);
		
		getOut.push_back(d);
	}
			
	//Apply the transformations to the incoming 
	//ion streams, generating new outgoing ion streams with
	//the modified positions
	size_t totalSize=numElements(dataIn);

	//If there are no ions, nothing to do.
	// just copy non-ion input to output 
	if(!totalSize)
	{
		for(unsigned int ui=0;ui<dataIn.size();ui++)
		{
			if(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS)
				continue;

			getOut.push_back(dataIn[ui]);
		}
		return 0;
	}

	if( transformMode != MODE_VALUE_SHUFFLE)
	{
		//Don't cross the streams. Why? It would be bad.
		//  - I'm fuzzy on the whole good-bad thing, what do you mean bad?
		//  - Every ion in the data body can be operated on independently.
		//		FIXME: I'm still not clear why that is bad. Might have something to do with tracking range parent IDs, or somesuch
		//			may or may not be relevant today 
		//  OK, important safety tip.
		size_t n=0;
		for(unsigned int ui=0;ui<dataIn.size() ;ui++)
		{
			switch(transformMode)
			{
				case MODE_SCALE_ISOTROPIC:
				{
					//We are going to scale the incoming point data
					//around the specified origin.
					ASSERT(vectorParams.size() == 1);
					ASSERT(scalarParams.size() == 1);
					float scaleFactor=scalarParams[0];
					Point3D origin=vectorParams[0];

					switch(dataIn[ui]->getStreamType())
					{
						case STREAM_TYPE_IONS:
						{
							//Set up scaling output ion stream 
							IonStreamData *d=new IonStreamData;
							d->parent=this;
							const IonStreamData *src = (const IonStreamData *)dataIn[ui];

							try
							{
								d->data.resize(src->data.size());
							}
							catch(std::bad_alloc)
							{
								delete d;
								return ERR_NOMEM;
							}
							d->r = src->r;
							d->g = src->g;
							d->b = src->b;
							d->a = src->a;
							d->ionSize = src->ionSize;
							d->valueType=src->valueType;

							ASSERT(src->data.size() <= totalSize);
#ifdef _OPENMP
							unsigned int curProg=PROGRESS_REDUCE;
							//Parallel version
							bool spin=false;
							#pragma omp parallel for shared(spin,n)
							for(unsigned int ui=0;ui<src->data.size();ui++)
							{
								if(spin)
									continue;

								if(!curProg--)
								{
									unsigned int thisT=omp_get_thread_num();
									#pragma omp critical
									{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									}


									if(thisT == 0)
									{
										if(*Filter::wantAbort)
											spin=true;
									}
								}


								//set the position for the given ion
								d->data[ui].setPos((src->data[ui].getPosRef() - origin)*scaleFactor+origin);
								d->data[ui].setMassToCharge(src->data[ui].getMassToCharge());
							}
							if(spin)
							{			
								delete d;
								return FILTER_ERR_ABORT;
							}

#else
							//Single threaded version
							size_t pos=0;
							//Copy across the ions into the target
							for(vector<IonHit>::const_iterator it=src->data.begin();
								       it!=src->data.end(); ++it)
							{
								//set the position for the given ion
								d->data[pos].setPos((it->getPosRef() - origin)*scaleFactor+origin);
								d->data[pos].setMassToCharge(it->getMassToCharge());
								//update progress 
								progress.filterProgress= (unsigned int)((float)(n++)/((float)totalSize)*100.0f);
								if(*Filter::wantAbort)
								{
									delete d;
									return FILTER_ERR_ABORT;
								}
								pos++;
							}

							ASSERT(pos == d->data.size());
#endif
							ASSERT(d->data.size() == src->data.size());

							cacheAsNeeded(d);

							getOut.push_back(d);
							break;
						}
						default:
							//Just copy across the ptr, if we are unfamiliar with this type
							getOut.push_back(dataIn[ui]);	
							break;
					}
					break;
				}
				case MODE_SCALE_ANISOTROPIC:
				{
					//We are going to scale the incoming point data
					//around the specified origin.
					ASSERT(vectorParams.size() == 2);
					
					
					Point3D origin=vectorParams[0];
					Point3D transformVec=vectorParams[1];
					switch(dataIn[ui]->getStreamType())
					{
						case STREAM_TYPE_IONS:
						{
							//Set up scaling output ion stream 
							IonStreamData *d=new IonStreamData;
							d->parent=this;
							const IonStreamData *src = (const IonStreamData *)dataIn[ui];

							try
							{
								d->data.resize(src->data.size());
							}
							catch(std::bad_alloc)
							{
								delete d;
								return ERR_NOMEM;
							}
							d->r = src->r;
							d->g = src->g;
							d->b = src->b;
							d->a = src->a;
							d->ionSize = src->ionSize;
							d->valueType=src->valueType;

							ASSERT(src->data.size() <= totalSize);
#ifdef _OPENMP
							unsigned int curProg=PROGRESS_REDUCE;
							//Parallel version
							bool spin=false;
							#pragma omp parallel for shared(spin)
							for(unsigned int ui=0;ui<src->data.size();ui++)
							{
								unsigned int thisT=omp_get_thread_num();
								if(spin)
									continue;

								if(!curProg--)
								{
									#pragma omp critical
									{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									}


									if(thisT == 0)
									{
										if(*Filter::wantAbort)
											spin=true;
									}
								}


								//set the position for the given ion
								d->data[ui].setPos((src->data[ui].getPosRef() - origin)*transformVec+origin);
								d->data[ui].setMassToCharge(src->data[ui].getMassToCharge());
							}
							if(spin)
							{			
								delete d;
								return FILTER_ERR_ABORT;
							}

#else
							//Single threaded version
							size_t pos=0;
							//Copy across the ions into the target
							for(vector<IonHit>::const_iterator it=src->data.begin();
								       it!=src->data.end(); ++it)
							{
								//set the position for the given ion
								d->data[pos].setPos((it->getPosRef() - origin)*transformVec+origin);
								d->data[pos].setMassToCharge(it->getMassToCharge());
								progress.filterProgress= (unsigned int)((float)(n++)/((float)totalSize)*100.0f);
								if(*Filter::wantAbort)
								{
									delete d;
									return FILTER_ERR_ABORT;
								}
								pos++;
							}

							ASSERT(pos == d->data.size());
#endif
							ASSERT(d->data.size() == src->data.size());

							cacheAsNeeded(d);

							getOut.push_back(d);
							break;
						}
						default:
							//Just copy across the ptr, if we are unfamiliar with this type
							getOut.push_back(dataIn[ui]);	
							break;
					}


					

					break;
				}
				case MODE_TRANSLATE:
				{
					//We are going to scale the incoming point data
					//around the specified origin.
					ASSERT(vectorParams.size() == 1);
					ASSERT(scalarParams.size() == 0);
					Point3D origin =vectorParams[0];
					switch(dataIn[ui]->getStreamType())
					{
						case STREAM_TYPE_IONS:
						{
							//Set up scaling output ion stream 
							IonStreamData *d=new IonStreamData;
							d->parent=this;
							
							const IonStreamData *src = (const IonStreamData *)dataIn[ui];
							try
							{
								d->data.resize(src->data.size());
							}
							catch(std::bad_alloc)
							{
								delete d;
								return ERR_NOMEM;
							}
							d->r = src->r;
							d->g = src->g;
							d->b = src->b;
							d->a = src->a;
							d->ionSize = src->ionSize;
							d->valueType=src->valueType;
							
							ASSERT(src->data.size() <= totalSize);
#ifdef _OPENMP
							//Parallel version
							unsigned int curProg=PROGRESS_REDUCE;
							bool spin=false;
#pragma omp parallel for shared(spin)
							for(unsigned int ui=0;ui<src->data.size();ui++)
							{
								if(spin)
									continue;
								
								if(!curProg--)
								{
#pragma omp critical
									{
										n+=PROGRESS_REDUCE;
										progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									}
									
									
									if(omp_get_thread_num() == 0)
									{
										if(*Filter::wantAbort)
											spin=true;
									}
								}
								
								
								//set the position for the given ion
								d->data[ui].setPos((src->data[ui].getPosRef() - origin));
								d->data[ui].setMassToCharge(src->data[ui].getMassToCharge());
							}
							if(spin)
							{			
								delete d;
								return FILTER_ERR_ABORT;
							}
							
#else
							//Single threaded version
							size_t pos=0;
							//Copy across the ions into the target
							for(vector<IonHit>::const_iterator it=src->data.begin();
								it!=src->data.end(); ++it)
							{
								//set the position for the given ion
								d->data[pos].setPos((it->getPosRef() - origin));
								d->data[pos].setMassToCharge(it->getMassToCharge());
								//update progress every CALLBACK ions
								progress.filterProgress= (unsigned int)((float)(n++)/((float)totalSize)*100.0f);
								if(*Filter::wantAbort)
								{
									delete d;
									return FILTER_ERR_ABORT;
								}
								pos++;
							}
							ASSERT(pos == d->data.size());
#endif
							ASSERT(d->data.size() == src->data.size());

							cacheAsNeeded(d);
							
							getOut.push_back(d);
							break;
						}
						default:
							//Just copy across the ptr, if we are unfamiliar with this type
							getOut.push_back(dataIn[ui]);	
							break;
					}
					break;
				}
				case MODE_TRANSLATE_VALUE:
				{
					//We are going to scale the incoming point data
					//around the specified origin.
					ASSERT(vectorParams.size() == 0);
					ASSERT(scalarParams.size() == 1);
					float origin =scalarParams[0];
					switch(dataIn[ui]->getStreamType())
					{
						case STREAM_TYPE_IONS:
						{
							//Set up scaling output ion stream 
							IonStreamData *d=new IonStreamData;
							d->parent=this;
							
							const IonStreamData *src = (const IonStreamData *)dataIn[ui];
							try
							{
								d->data.resize(src->data.size());
							}
							catch(std::bad_alloc)
							{
								delete d;
								return ERR_NOMEM;
							}
							d->r = src->r;
							d->g = src->g;
							d->b = src->b;
							d->a = src->a;
							d->ionSize = src->ionSize;
							d->valueType=src->valueType;
							
							ASSERT(src->data.size() <= totalSize);
							unsigned int curProg=NUM_CALLBACK;
#ifdef _OPENMP
							//Parallel version
							bool spin=false;
#pragma omp parallel for shared(spin)
							for(unsigned int ui=0;ui<src->data.size();ui++)
							{
								unsigned int thisT=omp_get_thread_num();
								if(spin)
									continue;
								
								if(!curProg--)
								{
#pragma omp critical
									{
										n+=NUM_CALLBACK;
										progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									}
									
									
									if(thisT == 0)
									{
										if(*Filter::wantAbort)
											spin=true;
									}
								}
								
								
								//set the position for the given ion
								d->data[ui].setPos((src->data[ui].getPosRef()));
								d->data[ui].setMassToCharge(src->data[ui].getMassToCharge()+origin);
							}
							if(spin)
							{			
								delete d;
								return FILTER_ERR_ABORT;
							}
							
#else
							//Single threaded version
							size_t pos=0;
							//Copy across the ions into the target
							for(vector<IonHit>::const_iterator it=src->data.begin();
								it!=src->data.end(); ++it)
							{
								//set the position for the given ion
								d->data[pos].setPos((it->getPosRef()));
								d->data[pos].setMassToCharge(it->getMassToCharge() + origin);
								//update progress every CALLBACK ions
								if(!curProg--)
								{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									if(*Filter::wantAbort)
									{
										delete d;
										return FILTER_ERR_ABORT;
									}
									curProg=NUM_CALLBACK;
								}
								pos++;
							}
							ASSERT(pos == d->data.size());
#endif
							ASSERT(d->data.size() == src->data.size());

							cacheAsNeeded(d);
							
							getOut.push_back(d);
							break;
						}
						default:
							//Just copy across the ptr, if we are unfamiliar with this type
							getOut.push_back(dataIn[ui]);	
							break;
					}
					break;
				}
				case MODE_CROP_VALUE:
				{
					ASSERT(scalarParams.size() == 2);

					switch(dataIn[ui]->getStreamType())
					{
						case STREAM_TYPE_IONS:
						{
							//Set up scaling output ion stream 
							IonStreamData *d=new IonStreamData;
							d->parent=this;
							
							const IonStreamData *src = (const IonStreamData *)dataIn[ui];
							for(unsigned int uj=0;uj<src->data.size();uj++)
							{
								float v;
								v=src->data[uj].getMassToCharge();
								if(v >=scalarParams[0] && v < scalarParams[1])
									d->data.push_back(src->data[uj]);
							}

							d->estimateIonParameters(src);
							cacheAsNeeded(d);
							
							getOut.push_back(d);
						}
					}
				}
				break;
				case MODE_ROTATE:
				{
					Point3D origin=vectorParams[0];
					switch(dataIn[ui]->getStreamType())
					{
						case STREAM_TYPE_IONS:
						{

							const IonStreamData *src = (const IonStreamData *)dataIn[ui];
							//Set up output ion stream 
							IonStreamData *d=new IonStreamData;
							d->parent=this;
							try
							{
								d->data.resize(src->data.size());
							}
							catch(std::bad_alloc)
							{
								delete d;
								return ERR_NOMEM;
							}

							d->estimateIonParameters(src);

							//We are going to rotate the incoming point data
							//around the specified origin.
							ASSERT(vectorParams.size() == 2);
							ASSERT(scalarParams.size() == 1);
							Point3D axis =vectorParams[1];
							axis.normalise();
							float angle=scalarParams[0]*M_PI/180.0f;

							unsigned int curProg=NUM_CALLBACK;

							Point3f rotVec,p;
							rotVec.fx=axis[0];
							rotVec.fy=axis[1];
							rotVec.fz=axis[2];

							Quaternion q1;

							//Generate the rotating quaternion
							quat_get_rot_quat(&rotVec,-angle,&q1);
							ASSERT(src->data.size() <= totalSize);


							size_t pos=0;

							//TODO: Parallelise rotation
							//Copy across the ions into the target
							for(vector<IonHit>::const_iterator it=src->data.begin();
								       it!=src->data.end(); ++it)
							{
								p.fx=it->getPosRef()[0]-origin[0];
								p.fy=it->getPosRef()[1]-origin[1];
								p.fz=it->getPosRef()[2]-origin[2];
								quat_rot_apply_quat(&p,&q1);
								//set the position for the given ion
								d->data[pos].setPos(p.fx+origin[0],
										p.fy+origin[1],p.fz+origin[2]);
								d->data[pos].setMassToCharge(it->getMassToCharge());
								//update progress every CALLBACK ions
								if(!curProg--)
								{
									n+=NUM_CALLBACK;
									progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
									if(*Filter::wantAbort)
									{
										delete d;
										return FILTER_ERR_ABORT;
									}
									curProg=NUM_CALLBACK;
								}
								pos++;
							}

							ASSERT(d->data.size() == src->data.size());

							cacheAsNeeded(d);
							
							getOut.push_back(d);
							break;
						}
						default:
							getOut.push_back(dataIn[ui]);
							break;
					}

					break;
				}
				case MODE_SPATIAL_NOISE:
				{
					ASSERT(scalarParams.size() ==1 &&
							vectorParams.size()==0);
					switch(dataIn[ui]->getStreamType())
					{
						case STREAM_TYPE_IONS:
						{
							//Set up scaling output ion stream 
							IonStreamData *d=new IonStreamData;
							d->parent=this;

							const IonStreamData *src = (const IonStreamData *)dataIn[ui];
							try
							{
								d->data.resize(src->data.size());
							}
							catch(std::bad_alloc)
							{
								delete d;
								return ERR_NOMEM;
							}
							d->r = src->r;
							d->g = src->g;
							d->b = src->b;
							d->a = src->a;
							d->ionSize = src->ionSize;
							d->valueType=src->valueType;

							float scaleFactor=scalarParams[0];
							ASSERT(src->data.size() <= totalSize);
							unsigned int curProg=NUM_CALLBACK;

							//NOTE: This *cannot* be parallelised without parallelising the random
							// number generator safely. If using multiple random number generators,
							// one would need to ensure sufficient entropy in EACH generator. This
							// is not trivial to prove, and so has not been done here. Bootstrapping
							// each random number generator using non-random seeds could be problematic
							// same as feeding back a random number into other rng instances 
							//
							// One solution is to use the unix /dev/urandom interface or the windows
							// cryptographic API, alternatively use the TR1 header's Mersenne twister with
							// multi-seeding:
							//  http://theo.phys.sci.hiroshima-u.ac.jp/~ishikawa/PRNG/mt_stream_en.html
							switch(noiseType)
							{
								case NOISETYPE_WHITE:
								{
									for(size_t ui=0;ui<src->data.size();ui++)
									{
										Point3D pt;

										pt.setValue(0,randGen.genUniformDev()-0.5f);
										pt.setValue(1,randGen.genUniformDev()-0.5f);
										pt.setValue(2,randGen.genUniformDev()-0.5f);

										pt*=scaleFactor;

										//set the position for the given ion
										d->data[ui].setPos(src->data[ui].getPosRef() + pt);
										d->data[ui].setMassToCharge(src->data[ui].getMassToCharge());
										
										
										if(!curProg--)
										{
											curProg=NUM_CALLBACK;
											progress.filterProgress= (unsigned int)((float)(ui)/((float)totalSize)*100.0f);
											if(*Filter::wantAbort)
											{
												delete d;
												return FILTER_ERR_ABORT;
											}
										}
									}
									break;
								}
								case NOISETYPE_GAUSSIAN:
								{
									for(size_t ui=0;ui<src->data.size();ui++)
									{
										Point3D pt;

										pt.setValue(0,randGen.genGaussDev());
										pt.setValue(1,randGen.genGaussDev());
										pt.setValue(2,randGen.genGaussDev());

										pt*=scaleFactor;

										//set the position for the given ion
										d->data[ui].setPos(src->data[ui].getPosRef() + pt);
										d->data[ui].setMassToCharge(src->data[ui].getMassToCharge());
										
										
										if(!curProg--)
										{
											curProg=NUM_CALLBACK;
											progress.filterProgress= (unsigned int)((float)(ui)/((float)totalSize)*100.0f);
											if(*Filter::wantAbort)
											{
												delete d;
												return FILTER_ERR_ABORT;
											}
										}
									}

									break;
								}
							}
							
							ASSERT(d->data.size() == src->data.size());

							cacheAsNeeded(d);
							
							getOut.push_back(d);
							break;
						}
						default:
							getOut.push_back(dataIn[ui]);
							break;
					}
					break;
				}
			
		}
		}
	}
	else
	{
		progress.step=1;
		progress.filterProgress=0;
		progress.stepName=TRANS("Collate");
		progress.maxStep=3;
		if(*Filter::wantAbort)
			return FILTER_ERR_ABORT;
		//we have to cross the streams (I thought that was bad?) 
		//  - Each dataset is no longer independent, and needs to
		//  be mixed with the other datasets. Bugger; sounds mem. expensive.
		
		//Set up output ion stream 
		IonStreamData *d=new IonStreamData;
		d->parent=this;
		
		//TODO: Better output colouring/size
		//Set up ion metadata
		d->r = 0.5;
		d->g = 0.5;
		d->b = 0.5;
		d->a = 0.5;
		d->ionSize = 2.0;
		d->valueType=TRANS("Mass-to-Charge (Da/e)");

		size_t curPos=0;
		
		vector<float> massData;

		//TODO: Ouch. Memory intensive -- could do a better job
		//of this?
		try
		{
			massData.resize(totalSize);
			d->data.resize(totalSize);
		}
		catch(std::bad_alloc)
		{
			return ERR_NOMEM; 
		}

		//merge the datasets
		for(size_t ui=0;ui<dataIn.size() ;ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS:
				{
		
					const IonStreamData *src = (const IonStreamData *)dataIn[ui];

					//Loop through the ions in this stream, and copy its data value
#pragma omp parallel for shared(massData,d,curPos,src) 
					for(size_t uj=0;uj<src->data.size();uj++)
					{
						massData[uj+curPos] = src->data[uj].getMassToCharge();
						d->data[uj+curPos].setPos(src->data[uj].getPos());
					}
				
					if(*Filter::wantAbort)
					{
						delete d;
						return FILTER_ERR_ABORT;
					}

					curPos+=src->data.size();
					break;
				}
				default:
					getOut.push_back(dataIn[ui]);
					break;
			}
		}

	
		progress.step=2;
		progress.filterProgress=0;
		progress.stepName=TRANS("Shuffle");
		if(*Filter::wantAbort)
		{
			delete d;
			return FILTER_ERR_ABORT;
		}
		//Shuffle the value data.TODO: callback functor	

#ifndef HAVE_CPP1X
		std::srand(time(0));
		std::random_shuffle(massData.begin(),massData.end());
#else
		std::mt19937_64 r;
		r.seed(time(0));
		std::shuffle(massData.begin(),massData.end(),r);
#endif
		if(*Filter::wantAbort)
		{
			delete d;
			return FILTER_ERR_ABORT;
		}

		progress.step=3;
		progress.filterProgress=0;
		progress.stepName=TRANS("Splice");
		if(*Filter::wantAbort)
		{
			delete d;
			return FILTER_ERR_ABORT;
		}
		
		

		//Set the output data by splicing together the
		//shuffled values and the original position info
#pragma omp parallel for shared(d,massData) 
		for(size_t uj=0;uj<totalSize;uj++)
			d->data[uj].setMassToCharge(massData[uj]);
		
		if(*Filter::wantAbort)
		{
			delete d;
			return FILTER_ERR_ABORT;
		}

		massData.clear();

		cacheAsNeeded(d);
		
		getOut.push_back(d);
		
	}
	return 0;
}


void TransformFilter::getProperties(FilterPropGroup &propertyList) const
{

	FilterProperty p;
	size_t curGroup=0;
	string tmpStr;
	vector<pair<unsigned int,string> > choices;
	for(unsigned int ui=0;ui<MODE_ENUM_END; ui++)
		choices.push_back(make_pair(ui,TRANS(TRANSFORM_MODE_STRING[ui])));
	
	tmpStr=choiceString(choices,transformMode);
	choices.clear();
	
	p.name=TRANS("Mode");
	p.data=tmpStr;
	p.type=PROPERTY_TYPE_CHOICE;
	p.helpText=TRANS("Algorithm to use to transform point data");
	p.key=KEY_MODE;
	propertyList.addProperty(p,curGroup);
	
	propertyList.setGroupTitle(curGroup,TRANS("Algorithm"));
	curGroup++;	
	
	//non-translation transforms require a user to select an origin	
	if( (transformMode == MODE_SCALE_ISOTROPIC  || transformMode == MODE_SCALE_ANISOTROPIC
				|| transformMode == MODE_ROTATE))
	{
		vector<pair<unsigned int,string> > choices;
		for(unsigned int ui=0;ui<ORIGINMODE_END;ui++)
			choices.push_back(make_pair(ui,getOriginTypeString(ui)));
		
		tmpStr= choiceString(choices,originMode);

		p.name=TRANS("Origin mode");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_CHOICE;
		p.helpText=TRANS("Select how transform origin is computed");
		p.key=KEY_ORIGINMODE;
		propertyList.addProperty(p,curGroup);
	
		stream_cast(tmpStr,showOrigin);	
		p.name=TRANS("Show marker");
		p.data=tmpStr;
		p.type=PROPERTY_TYPE_BOOL;
		if( originMode == ORIGINMODE_SELECT)
			p.helpText=TRANS("Display an interactive object to set transform origin");
		else
			p.helpText=TRANS("Display a small marker to denote transform origin");
		p.key=KEY_TRANSFORM_SHOWORIGIN;
		propertyList.addProperty(p,curGroup);
	}

	

	bool haveProps=true;
	switch(transformMode)
	{
		case MODE_TRANSLATE:
		{
			ASSERT(vectorParams.size() == 1);
			ASSERT(scalarParams.size() == 0);
			
			stream_cast(tmpStr,vectorParams[0]);
			p.name=TRANS("Translation");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_POINT3D;
			p.helpText=TRANS("Translation vector for transform");
			p.key=KEY_ORIGIN;
			propertyList.addProperty(p,curGroup);
			break;
		}
		case MODE_TRANSLATE_VALUE:
		{
			ASSERT(vectorParams.size() == 0);
			ASSERT(scalarParams.size() == 1);
			
			
			stream_cast(tmpStr,scalarParams[0]);
			p.name=TRANS("Offset");
			p.data=tmpStr;
			p.key=KEY_ORIGIN_VALUE;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Scalar to use to offset each point's associated value");
			propertyList.addProperty(p,curGroup);
			break;
		}
		case MODE_SCALE_ISOTROPIC:
		{
			ASSERT(vectorParams.size() == 1);
			ASSERT(scalarParams.size() == 1);
			
			
			if(originMode == ORIGINMODE_SELECT)
			{
				stream_cast(tmpStr,vectorParams[0]);
				p.key=KEY_ORIGIN;
				p.name=TRANS("Origin");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_POINT3D;
				p.helpText=TRANS("Origin of scale trasnform");
				propertyList.addProperty(p,curGroup);
			}
			
			stream_cast(tmpStr,scalarParams[0]);
			
			p.key=KEY_SCALEFACTOR;
			p.name=TRANS("Scale Fact.");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Enlargement factor for scaling around origin");
			propertyList.addProperty(p,curGroup);

			break;
		}
		case MODE_SCALE_ANISOTROPIC:
		{
			ASSERT(vectorParams.size() == 2);
			
			
			if(originMode == ORIGINMODE_SELECT)
			{
				stream_cast(tmpStr,vectorParams[0]);
				p.key=KEY_ORIGIN;
				p.name=TRANS("Origin");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_POINT3D;
				p.helpText=TRANS("Origin of scale trasnform");
				propertyList.addProperty(p,curGroup);
			}
			
			stream_cast(tmpStr,vectorParams[1]);
			
			p.key=KEY_SCALEFACTOR_ANISOTROPIC;
			p.name=TRANS("Scale Fact.");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Enlargement factor for scaling around origin");
			propertyList.addProperty(p,curGroup);

			break;
		}
		case MODE_ROTATE:
		{
			ASSERT(vectorParams.size() == 2);
			ASSERT(scalarParams.size() == 1);
			if(originMode == ORIGINMODE_SELECT)
			{
				stream_cast(tmpStr,vectorParams[0]);
				p.key=KEY_ORIGIN;
				p.name=TRANS("Origin");
				p.data=tmpStr;
				p.type=PROPERTY_TYPE_POINT3D;
				p.helpText=TRANS("Origin of rotation");
				propertyList.addProperty(p,curGroup);
			}
			stream_cast(tmpStr,vectorParams[1]);
			p.key=KEY_ROTATE_AXIS;
			p.name=TRANS("Axis");
			p.data=tmpStr;
			p.type=(PROPERTY_TYPE_POINT3D);
			p.helpText=TRANS("Axis around which to revolve");
			propertyList.addProperty(p,curGroup);

			stream_cast(tmpStr,scalarParams[0]);
			p.key=KEY_ROTATE_ANGLE;
			p.name=TRANS("Angle (deg)");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Angle to perform rotation (ACW, as viewed from axis towards origin)");
			propertyList.addProperty(p,curGroup);
			break;
		}
		case MODE_VALUE_SHUFFLE:
		{
			//No options...
			haveProps=false;
			break;	
		}
		case MODE_SPATIAL_NOISE:
		{
			for(unsigned int ui=0;ui<NOISETYPE_END;ui++)
				choices.push_back(make_pair(ui,getNoiseTypeString(ui)));
			tmpStr= choiceString(choices,noiseType);
			choices.clear();
			
			p.name=TRANS("Noise Type");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_CHOICE;
			p.helpText=TRANS("Method to use to degrade point data");
			p.key=KEY_NOISETYPE;
			propertyList.addProperty(p,curGroup);


			stream_cast(tmpStr,scalarParams[0]);
			if(noiseType == NOISETYPE_WHITE)
				p.name=TRANS("Noise level");
			else if(noiseType == NOISETYPE_GAUSSIAN)
				p.name=TRANS("Standard dev.");
			else
			{
				ASSERT(false);
			}
			
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Amplitude of noise");
			p.key=KEY_NOISELEVEL;
			propertyList.addProperty(p,curGroup);


			break;	
		}
		case MODE_CROP_VALUE:
		{
			ASSERT(vectorParams.size() == 0);
			ASSERT(scalarParams.size() == 2);
			
			
			stream_cast(tmpStr,scalarParams[0]);
			p.name=TRANS("Min Value");
			p.data=tmpStr;
			p.key=KEY_CROP_MINIMUM;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Minimum value to use for crop");
			propertyList.addProperty(p,curGroup);
			
			stream_cast(tmpStr,scalarParams[1]);
			p.name=TRANS("Max Value");
			p.data=tmpStr;
			p.key=KEY_CROP_MAXIMUM;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Maximum value to use for crop");
			propertyList.addProperty(p,curGroup);
			break;
		}
		default:
			ASSERT(false);
	}

	if(haveProps)
		propertyList.setGroupTitle(curGroup,TRANS("Transform Params"));

}

bool TransformFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{

	needUpdate=false;
	switch(key)
	{
		case KEY_MODE:
		{
			size_t tmp=-1;
			for(size_t ui=0;ui<MODE_ENUM_END;ui++)
			{
				if(value == TRANS(TRANSFORM_MODE_STRING[ui]))
					tmp=ui;
			}
			if(tmp==(size_t)-1)
			{
				ASSERT(false); // This should not happen
				return false;
			}
			transformMode=tmp;

			vectorParams.clear();
			scalarParams.clear();
			switch(transformMode)
			{
				case MODE_SCALE_ISOTROPIC:
					vectorParams.push_back(Point3D(0,0,0));
					scalarParams.push_back(1.0f);
					break;
				case MODE_SCALE_ANISOTROPIC:
					vectorParams.push_back(Point3D(0,0,0));
					vectorParams.push_back(Point3D(1,1,1));
					break;
				case MODE_TRANSLATE:
					vectorParams.push_back(Point3D(0,0,0));
					break;
				case MODE_TRANSLATE_VALUE:
					scalarParams.push_back(100.0f);
					break;
				case MODE_ROTATE:
					vectorParams.push_back(Point3D(0,0,0));
					vectorParams.push_back(Point3D(1,0,0));
					scalarParams.push_back(0.0f);
					break;
				case MODE_VALUE_SHUFFLE:
					break;
				case MODE_SPATIAL_NOISE:
					scalarParams.push_back(0.1f);
					break;
				case MODE_CROP_VALUE:
					scalarParams.push_back(1.0f);
					scalarParams.push_back(100.0f);
					break;
				default:
					ASSERT(false);
			}
			needUpdate=true;	
			clearCache();
			break;
		}
		//The rotation angle, and the scale factor are both stored
		//in scalaraparams[0]. All we need to do is set that,
		//as either can take any valid floating pt value
		case KEY_ROTATE_ANGLE:
		case KEY_SCALEFACTOR:
		case KEY_NOISELEVEL:
		case KEY_ORIGIN_VALUE:
		{
			if(!applyPropertyNow(scalarParams[0],value,needUpdate))
				return false;
			return true;
		}
		case KEY_SCALEFACTOR_ANISOTROPIC:
		{
			if(!applyPropertyNow(vectorParams[1],value,needUpdate))
				return false;
			return true;
		}
		case KEY_ORIGIN:
		{
			if(!applyPropertyNow(vectorParams[0],value,needUpdate))
				return false;
			return true;
		}
		case KEY_ROTATE_AXIS:
		{
			ASSERT(vectorParams.size() ==2);
			ASSERT(scalarParams.size() ==1);
			Point3D newPt;
			if(!newPt.parse(value))
				return false;

			if(newPt.sqrMag() < std::numeric_limits<float>::epsilon())
				return false;

			if(!(vectorParams[1] == newPt ))
			{
				vectorParams[1] = newPt;
				needUpdate=true;
				clearCache();
			}

			return true;
		}
		case KEY_ORIGINMODE:
		{
			size_t i;
			for (i = 0; i < ORIGINMODE_END; i++)
				if (value == TRANS(getOriginTypeString(i).c_str())) break;
		
			if( i == ORIGINMODE_END)
				return false;

			if(originMode != i)
			{
				originMode = i;
				needUpdate=true;
				clearCache();
			}
			return true;
		}
		case KEY_TRANSFORM_SHOWORIGIN:
		{
			if(!applyPropertyNow(showOrigin,value,needUpdate))
				return false;
			break;
		}
		case KEY_NOISETYPE:
		{
			size_t i;
			for (i = 0; i < NOISETYPE_END; i++)
				if (value == TRANS(getNoiseTypeString(i).c_str())) break;
		
			if( i == NOISETYPE_END)
				return false;

			if(noiseType != i)
			{
				noiseType = i;
				needUpdate=true;
				clearCache();
			}
			break;
		}
		case KEY_CROP_MINIMUM:
		{
			ASSERT(scalarParams.size() ==2);
			if(!applyPropertyNow(scalarParams[0],value,needUpdate))
				return false;
			break;
		}
		case KEY_CROP_MAXIMUM:
		{
			ASSERT(scalarParams.size() ==2);
			if(!applyPropertyNow(scalarParams[1],value,needUpdate))
				return false;
			break;
		}
		default:
			ASSERT(false);
	}	
	return true;
}


std::string  TransformFilter::getSpecificErrString(unsigned int code) const
{
	const char *errStrs[] = { "",
		"Unable to allocate memory"//Caught a memory issue,
	};

	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(errStrs) == TRANSFORM_ERR_ENUM_END);
	ASSERT(code < TRANSFORM_ERR_ENUM_END);
	return errStrs[code]; 
}

bool TransformFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) << "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;
			f << tabs(depth+1) << "<transformmode value=\"" << transformMode<< "\"/>"<<endl;
			f << tabs(depth+1) << "<originmode value=\"" << originMode<< "\"/>"<<endl;
			
			f << tabs(depth+1) << "<noisetype value=\"" << noiseType<< "\"/>"<<endl;

			string tmpStr;
			if(showOrigin)
				tmpStr="1";
			else
				tmpStr="0";
			f << tabs(depth+1) << "<showorigin value=\"" << tmpStr <<  "\"/>"<<endl;
			writeVectorsXML(f,"vectorparams",vectorParams,depth);
			writeScalarsXML(f,"scalarparams",scalarParams,depth);
			f << tabs(depth) << "</" << trueName() << ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool TransformFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	//Retrieve user string
	//===
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlChar *xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);
	//===

	//Retrieve transformation type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,transformMode,"transformmode","value"))
		return false;
	if(transformMode>= MODE_ENUM_END)
	       return false;	
	//====
	
	//Retrieve origination type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,originMode,"originmode","value"))
		return false;
	if(originMode>= ORIGINMODE_END)
	       return false;	
	//====
	
	//Retrieve origination type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,originMode,"noisetype","value"))
		return false;
	if(noiseType>= NOISETYPE_END)
	       return false;	
	//====
	
	//Retrieve origination type 
	//====
	if(!XMLGetNextElemAttrib(nodePtr,originMode,"showorigin","value"))
		return false;
	//====
	
	//Retrieve vector parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"vectorparams"))
		return false;
	xmlNodePtr tmpNode=nodePtr;

	if(!readVectorsXML(nodePtr,vectorParams))
		return false;
	//===	

	nodePtr=tmpNode;
	//Retrieve scalar parameters
	//===
	if(XMLHelpFwdToElem(nodePtr,"scalarparams"))
		return false;

	if(!readScalarsXML(nodePtr,scalarParams))
		return false;
	//===	

	//Check the scalar params match the selected primitive	
	switch(transformMode)
	{
		case MODE_TRANSLATE:
			if(vectorParams.size() != 1 || scalarParams.size() !=0)
				return false;
			break;
		case MODE_SCALE_ISOTROPIC:
			if(vectorParams.size() != 1 || scalarParams.size() !=1)
				return false;
			break;
		case MODE_SCALE_ANISOTROPIC:
			if(vectorParams.size() != 2 || scalarParams.size() !=0)
				return false;
			break;
		case MODE_ROTATE:
			if(vectorParams.size() != 2 || scalarParams.size() !=1)
				return false;
			break;
		case MODE_TRANSLATE_VALUE:
			if(vectorParams.size() != 0 || scalarParams.size() !=1)
				return false;
			break;
		case MODE_VALUE_SHUFFLE:
		case MODE_SPATIAL_NOISE:
			break;
		case MODE_CROP_VALUE:
		{
			if(vectorParams.size() != 0 || scalarParams.size() !=2)
				return false;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}
	return true;
}


unsigned int TransformFilter::getRefreshBlockMask() const
{
	//Only ions cannot go through this filter.
	return STREAM_TYPE_IONS;
}

unsigned int TransformFilter::getRefreshEmitMask() const
{
	if(showPrimitive)
		return STREAM_TYPE_IONS | STREAM_TYPE_DRAW;
	else
		return STREAM_TYPE_IONS;
}

unsigned int TransformFilter::getRefreshUseMask() const
{
	return STREAM_TYPE_IONS;
}

void TransformFilter::setPropFromBinding(const SelectionBinding &b)
{
	switch(b.getID())
	{
		case BINDING_SPHERE_ORIGIN:
			b.getValue(vectorParams[0]);
			break;
		default:
			ASSERT(false);
	}
	clearCache();
}

std::string TransformFilter::getOriginTypeString(unsigned int i)
{
	ASSERT(i<ORIGINMODE_END);
	return TRANSFORM_ORIGIN_STRING[i];
}

std::string TransformFilter::getNoiseTypeString(unsigned int i)
{
	switch(i)
	{
		case NOISETYPE_WHITE:
			return std::string(TRANS("White"));
		case NOISETYPE_GAUSSIAN:
			return std::string(TRANS("Gaussian"));
	}
	ASSERT(false);
}


#ifdef DEBUG

//Generate some synthetic data points, that lie within 0->span.
//span must be  a 3-wide array, and numPts will be generated.
//each entry in the array should be coprime for optimal results.
//filter pointer must be deleted.
IonStreamData *synthDataPoints(unsigned int span[],unsigned int numPts);
bool rotateTest();
bool translateTest();
bool scaleTest();
bool scaleAnisoTest();
bool shuffleTest();

class MassCompare
{
	public:
		inline bool operator()(const IonHit &h1,const IonHit &h2) const
		{return h1.getMassToCharge()<h2.getMassToCharge();};
};

bool TransformFilter::runUnitTests()
{
	if(!rotateTest())
		return false;

	if(!translateTest())
		return false;

	if(!scaleTest())
		return false;

	if(!scaleAnisoTest())
		return false;

	if(!shuffleTest())
		return false;

	return true;
}

IonStreamData *synthDataPoints(unsigned int span[], unsigned int numPts)
{
	IonStreamData *d = new IonStreamData;
	
	for(unsigned int ui=0;ui<numPts;ui++)
	{
		IonHit h;
		h.setPos(Point3D(ui%span[0],
			ui%span[1],ui%span[2]));
		h.setMassToCharge(ui);
		d->data.push_back(h);
	}

	return d;
}

bool rotateTest()
{
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d= new IonStreamData;

	RandNumGen rng;
	rng.initTimer();

	const unsigned int NUM_PTS=10000;

	
	//Build a  sphere of data points
	//by rejection method
	d->data.reserve(NUM_PTS/2);	
	for(unsigned int ui=0;ui<NUM_PTS;ui++)
	{
		Point3D tmp;
		tmp=Point3D(rng.genUniformDev()-0.5f,
				rng.genUniformDev()-0.5f,
				rng.genUniformDev()-0.5f);

		if(tmp.sqrMag() < 1.0f)
		{
			IonHit h;
			h.setPos(tmp);
			h.setMassToCharge(1);
			d->data.push_back(h);
		}
	}
		
	streamIn.push_back(d);

	//Set up the filter itself
	//---
	TransformFilter *f=new TransformFilter;
	f->setCaching(false);


	bool needUp;
	string s;
	TEST(f->setProperty(KEY_MODE,
		TRANS(TRANSFORM_MODE_STRING[MODE_ROTATE]),needUp),"Set transform mode");
	float tmpVal;
	tmpVal=rng.genUniformDev()*M_PI*2.0;
	stream_cast(s,tmpVal);
	TEST(f->setProperty(KEY_ROTATE_ANGLE,s,needUp),"Set rotate angle");
	Point3D tmpPt;

	//NOTE: Technically there is a nonzero chance of this failing.
	tmpPt=Point3D(rng.genUniformDev()-0.5f,
			rng.genUniformDev()-0.5f,
			rng.genUniformDev()-0.5f);
	stream_cast(s,tmpPt);
	TEST(f->setProperty(KEY_ROTATE_AXIS,s,needUp),"set rotate axis");
	
	TEST(f->setProperty(KEY_ORIGINMODE,
		TRANS(TRANSFORM_ORIGIN_STRING[ORIGINMODE_MASSCENTRE]),needUp),"Set origin");
	TEST(f->setProperty(KEY_TRANSFORM_SHOWORIGIN,"0",needUp),"Set no-show origin");
	//---


	//OK, so now do the rotation
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"refresh error code");
	delete f;

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == d->data.size(),"Ion count invariance");

	const IonStreamData *outData=(IonStreamData*)streamOut[0];

	Point3D massCentre[2];
	massCentre[0]=massCentre[1]=Point3D(0,0,0);
	//Now check that the mass centre has not moved
	for(unsigned int ui=0;ui<d->data.size();ui++)
		massCentre[0]+=d->data[ui].getPos();

	for(unsigned int ui=0;ui<outData->data.size();ui++)
		massCentre[1]+=outData->data[ui].getPos();


	TEST((massCentre[0]-massCentre[1]).sqrMag() < 
			2.0*sqrtf(std::numeric_limits<float>::epsilon()),"mass centre invariance");

	//Rotating a sphere around its centre of mass
	// should not massively change the bounding box
	// however we don't quite have  a sphere, so we could have (at the most extreme,
	// a cube)
	BoundCube bc[2];
	IonHit::getBoundCube(d->data,bc[0]);
	IonHit::getBoundCube(outData->data,bc[1]);

	float volumeRat;
	volumeRat = bc[0].volume()/bc[1].volume();

	TEST(volumeRat > 0.5f && volumeRat < 2.0f, "volume ratio test");

	delete streamOut[0];
	delete d;
	return true;
}

bool translateTest()
{
	RandNumGen rng;
	rng.initTimer();
	
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d;
	const unsigned int NUM_PTS=10000;

	unsigned int span[]={ 
			5, 7, 9
			};	
	d=synthDataPoints(span,NUM_PTS);
	streamIn.push_back(d);

	Point3D offsetPt;

	//Set up the filter itself
	//---
	TransformFilter *f=new TransformFilter;

	bool needUp;
	string s;
	TEST(f->setProperty(KEY_MODE,
		TRANSFORM_MODE_STRING[MODE_TRANSLATE],needUp),"set translate mode");

	//NOTE: Technically there is a nonzero chance of this failing.
	offsetPt=Point3D(rng.genUniformDev()-0.5f,
			rng.genUniformDev()-0.5f,
			rng.genUniformDev()-0.5f);
	offsetPt[0]*=span[0];
	offsetPt[1]*=span[1];
	offsetPt[2]*=span[2];

	stream_cast(s,offsetPt);
	TEST(f->setProperty(KEY_ORIGIN,s,needUp),"Set Origin");
	TEST(f->setProperty(KEY_TRANSFORM_SHOWORIGIN,"0",needUp),"Set display origin");
	//---
	
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"Refresh error code");
	delete f;
	
	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == d->data.size(),"Ion count invariance");

	const IonStreamData *outData=(IonStreamData*)streamOut[0];

	//Bound cube should move exactly as per the translation
	BoundCube bc[2];
	IonHit::getBoundCube(d->data,bc[0]);
	IonHit::getBoundCube(outData->data,bc[1]);

	for(unsigned int ui=0;ui<3;ui++)
	{
		for(unsigned int uj=0;uj<2;uj++)
		{
			float f;
			f=bc[0].getBound(ui,uj) -bc[1].getBound(ui,uj);
			TEST(fabs(f-offsetPt[ui]) < sqrtf(std::numeric_limits<float>::epsilon()), "bound translation");
		}
	}

	delete d;
	delete streamOut[0];

	return true;
}


bool scaleTest() 
{
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d;

	RandNumGen rng;
	rng.initTimer();

	const unsigned int NUM_PTS=10000;

	unsigned int span[]={ 
			5, 7, 9
			};	
	d=synthDataPoints(span,NUM_PTS);
	streamIn.push_back(d);

	//Set up the filter itself
	//---
	TransformFilter *f=new TransformFilter;

	bool needUp;
	string s;
	//Switch to scale mode (isotropic)
	TEST(f->setProperty(KEY_MODE,
			TRANS(TRANSFORM_MODE_STRING[MODE_SCALE_ISOTROPIC]),needUp),"Set scale mode");


	//Switch to mass-centre origin
	TEST(f->setProperty(KEY_ORIGINMODE,
		TRANS(TRANSFORM_ORIGIN_STRING[ORIGINMODE_MASSCENTRE]),needUp),"Set origin->mass mode");

	float scaleFact;
	//Pick some scale, both positive and negative.
	if(rng.genUniformDev() > 0.5)
		scaleFact=rng.genUniformDev()*10;
	else
		scaleFact=0.1f/(0.1f+rng.genUniformDev());

	stream_cast(s,scaleFact);

	TEST(f->setProperty(KEY_SCALEFACTOR,s,needUp),"Set scalefactor");
	//Don't show origin marker
	TEST(f->setProperty(KEY_TRANSFORM_SHOWORIGIN,"0",needUp),"Set show origin")
	//---


	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"refresh error code");
	delete f;

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == d->data.size(),"Ion count invariance");

	const IonStreamData *outData=(IonStreamData*)streamOut[0];

	//Scaling around its centre of mass
	// should scale the bounding box by the cube of the scale factor
	BoundCube bc[2];
	IonHit::getBoundCube(d->data,bc[0]);
	IonHit::getBoundCube(outData->data,bc[1]);

	float cubeOfScale=scaleFact*scaleFact*scaleFact;

	float volumeDelta;
	volumeDelta=fabs(bc[1].volume()/cubeOfScale - bc[0].volume() );

	TEST(volumeDelta < 100.0f*sqrtf(std::numeric_limits<float>::epsilon()), "scaled volume test");

	delete streamOut[0];
	delete d;
	return true;
}

bool scaleAnisoTest() 
{
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d;

	RandNumGen rng;
	rng.initTimer();

	const unsigned int NUM_PTS=10000;

	unsigned int span[]={ 
			5, 7, 9
			};	
	d=synthDataPoints(span,NUM_PTS);
	streamIn.push_back(d);

	//Set up the filter itself
	//---
	TransformFilter *f=new TransformFilter;

	bool needUp;
	string s;
	//Switch to scale mode (isotropic)
	TEST(f->setProperty(KEY_MODE,
			TRANS(TRANSFORM_MODE_STRING[MODE_SCALE_ANISOTROPIC]),needUp),"Set scale mode");


	//Switch to mass-centre origin
	TEST(f->setProperty(KEY_ORIGINMODE,
		TRANS(TRANSFORM_ORIGIN_STRING[ORIGINMODE_MASSCENTRE]),needUp),"Set origin->mass mode");

	Point3D scaleFact;
	//Pick some random scale vector, both positive and negative.
	scaleFact=Point3D(rng.genUniformDev()*10,rng.genUniformDev()*10,rng.genUniformDev()*10);

	stream_cast(s,scaleFact);

	TEST(f->setProperty(KEY_SCALEFACTOR_ANISOTROPIC,s,needUp),"Set scalefactor");
	//Don't show origin marker
	TEST(f->setProperty(KEY_TRANSFORM_SHOWORIGIN,"0",needUp),"Set show origin")
	//---


	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"refresh error code");
	delete f;

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == d->data.size(),"Ion count invariance");

	delete streamOut[0];
	delete d;
	return true;
}
bool shuffleTest() 
{
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d;

	RandNumGen rng;
	rng.initTimer();

	const unsigned int NUM_PTS=1000;

	unsigned int span[]={ 
			5, 7, 9
			};	
	d=synthDataPoints(span,NUM_PTS);
	streamIn.push_back(d);

	//Set up the filter itself
	//---
	TransformFilter *f=new TransformFilter;

	bool needUp;
	//Switch to shuffle mode
	TEST(f->setProperty(KEY_MODE,
			TRANS(TRANSFORM_MODE_STRING[MODE_VALUE_SHUFFLE]),needUp),"refresh error code");
	//---


	//OK, so now run the shuffle 
	//Do the refresh
	ProgressData p;
	TEST(!f->refresh(streamIn,streamOut,p),"refresh error code");
	delete f;

	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");
	TEST(streamOut[0]->getNumBasicObjects() == d->data.size(),"Ion count invariance");

	TEST(streamOut[0]->getNumBasicObjects() == d->data.size(),"Ion count invariance");

	IonStreamData *outData=(IonStreamData*)streamOut[0];

	//Check to see that the output masses each exist in the input,
	//but are not in  the same sequence
	//---
	

	bool sequenceDifferent=false;	
	for(size_t ui=0;ui<d->data.size();ui++)
	{
		if(d->data[ui].getMassToCharge() != outData->data[ui].getMassToCharge())
		{
			sequenceDifferent=true;
			break;
		}
	}
	TEST(sequenceDifferent,
		"Should be shuffled - Prob. of sequence being identical in both orig & shuffled cases is very low");
	//Sort masses
	MassCompare cmp;
	std::sort(outData->data.begin(),outData->data.end(),cmp);
	std::sort(d->data.begin(),d->data.end(),cmp);


	for(size_t ui=0;ui<d->data.size();ui++)
	{
		TEST(d->data[ui].getMassToCharge() == outData->data[ui].getMassToCharge(),"Shuffle + Sort mass should be the same");
	
	}



	delete streamOut[0];
	delete d;
	return true;
}

#endif
