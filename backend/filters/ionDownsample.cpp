/*
 *	ionDownsample.cpp - Filter to perform sampling without replacement on input ion data
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


#include "../../common/translation.h"
#include "filterCommon.h"

#include "ionDownsample.h"

using std::vector;
using std::string;

//!Downsampling filter
enum
{
	IONDOWNSAMPLE_BAD_ALLOC=1,
	IONDOWNSAMPLE_ERR_ENUM_END
};


// == Ion Downsampling filter ==

IonDownsampleFilter::IonDownsampleFilter()
{
	rng.initTimer();
	fixedNumOut=true;
	fraction=0.1f;
	maxAfterFilter=5000;
	rsdIncoming=0;
	perSpecies=false;

	cacheOK=false;
	cache=true; //By default, we should cache, but decision is made higher up

}

void IonDownsampleFilter::initFilter(const std::vector<const FilterStreamData *> &dataIn,
				std::vector<const FilterStreamData *> &dataOut)
{
	const RangeStreamData *c=0;
	//Determine if we have an incoming range
	for (size_t i = 0; i < dataIn.size(); i++) 
	{
		if(dataIn[i]->getStreamType() == STREAM_TYPE_RANGE)
		{
			c=(const RangeStreamData *)dataIn[i];

			dataOut.push_back(dataIn[i]);
			break;
		}
	}

	//we no longer (or never did) have any incoming ranges. Not much to do
	if(!c)
	{
		//delete the old incoming range pointer
		if(rsdIncoming)
			delete rsdIncoming;
		rsdIncoming=0;

		//Well, don't use per-species info anymore
		perSpecies=false;
	}
	else
	{


		//If we didn't have a previously incoming rsd, then make one up!
		// - we can't use a reference, as the rangestreams are technically transient,
		// so we have to copy.
		if(!rsdIncoming)
		{
			rsdIncoming = new RangeStreamData;
			*rsdIncoming=*c;

			if(ionFractions.size() != c->rangeFile->getNumIons())
			{
				//set up some defaults; seeded from normal
				ionFractions.resize(c->rangeFile->getNumIons()+1,fraction);
				ionLimits.resize(c->rangeFile->getNumIons()+1,maxAfterFilter);
			}
		}
		else
		{

			//OK, so we have a range incoming already (from last time)
			//-- the question is, is it the same one we had before ?
			//
			//Do a pointer comparison (its a hack, yes, but it should work)
			if(rsdIncoming->rangeFile != c->rangeFile)
			{
				//hmm, it is different. well, trash the old incoming rng
				delete rsdIncoming;

				rsdIncoming = new RangeStreamData;
				*rsdIncoming=*c;

				ionFractions.resize(c->rangeFile->getNumIons()+1,fraction);
				ionLimits.resize(c->rangeFile->getNumIons()+1,maxAfterFilter);
			}
			else if(ionFractions.size() !=c->rangeFile->getNumIons())
			{
				//well its the same range, but somehow the number of ions 
				//have changed. Could be range was reloaded.
				ionFractions.resize(rsdIncoming->rangeFile->getNumIons()+1,fraction);
				ionLimits.resize(rsdIncoming->rangeFile->getNumIons()+1,maxAfterFilter);
			}

			//Ensure what is enabled and is disabled is up-to-date	
			for(unsigned int ui=0;ui<rsdIncoming->enabledRanges.size();ui++)
				rsdIncoming->enabledRanges[ui] = c->enabledRanges[ui];
			for(unsigned int ui=0;ui<rsdIncoming->enabledIons.size();ui++)
				rsdIncoming->enabledIons[ui] = c->enabledIons[ui];
				
		}

	}


	ASSERT(ionLimits.size() == ionFractions.size());
}

Filter *IonDownsampleFilter::cloneUncached() const
{
	IonDownsampleFilter *p=new IonDownsampleFilter();
	p->rng = rng;
	p->maxAfterFilter=maxAfterFilter;
	p->fraction=fraction;
	p->perSpecies=perSpecies;
	p->rsdIncoming=rsdIncoming;

	p->ionFractions.resize(ionFractions.size());
	std::copy(ionFractions.begin(),ionFractions.end(),p->ionFractions.begin());
	p->ionLimits.resize(ionLimits.size());
	std::copy(ionLimits.begin(),ionLimits.end(),p->ionLimits.begin());


	//We are copying whether to cache or not,
	//not the cache itself
	p->cache=cache;
	p->cacheOK=false;
	p->userString=userString;
	p->fixedNumOut=fixedNumOut;
	return p;
}

size_t IonDownsampleFilter::numBytesForCache(size_t nObjects) const
{
	if(fixedNumOut)
	{
		if(nObjects > maxAfterFilter)
			return maxAfterFilter*IONDATA_SIZE;
		else
			return nObjects*IONDATA_SIZE;
	}
	else
	{
		return (size_t)((float)(nObjects*IONDATA_SIZE)*fraction);
	}
}

unsigned int IonDownsampleFilter::refresh(const std::vector<const FilterStreamData *> &dataIn,
	std::vector<const FilterStreamData *> &getOut, ProgressData &progress)
{
	//use the cached copy if we have it.
	if(cacheOK)
	{
		propagateStreams(dataIn,getOut,STREAM_TYPE_IONS,true);
		propagateCache(getOut);

		return 0;
	}
	
	progress.step=1;
	progress.maxStep=1;
	progress.stepName=TRANS("Sampling");

	size_t totalSize = numElements(dataIn,STREAM_TYPE_IONS);
	if(!perSpecies)	
	{
		for(size_t ui=0;ui<dataIn.size() ;ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS: 
				{
					if(!totalSize)
						continue;

					IonStreamData *d;
					d=new IonStreamData;
					d->parent=this;
					try
					{
						if(fixedNumOut)
						{
							float frac;
							frac = (float)(((const IonStreamData*)dataIn[ui])->data.size())/(float)totalSize;

							randomSelect(d->data,((const IonStreamData *)dataIn[ui])->data,
										rng,maxAfterFilter*frac,progress.filterProgress,
												*wantAbort,strongRandom);

							if(*Filter::wantAbort)
							{
								delete d;
								return FILTER_ERR_ABORT;
							}

						}
						else
						{

							size_t n=0;
							//Reserve 90% of storage needed.
							//highly likely with even modest numbers of ions
							//that this will be exceeded
							d->data.reserve((size_t)(fraction*0.9*totalSize));

							ASSERT(dataIn[ui]->getStreamType() == STREAM_TYPE_IONS);

							for(std::vector<IonHit>::const_iterator it=((const IonStreamData *)dataIn[ui])->data.begin();
								       it!=((const IonStreamData *)dataIn[ui])->data.end(); ++it)
							{
								if(rng.genUniformDev() <  fraction)
									d->data.push_back(*it);
							
								progress.filterProgress= (unsigned int)((float)(n)/((float)totalSize)*100.0f);
								if(*Filter::wantAbort)
								{
									delete d;
									return FILTER_ERR_ABORT;
								}
							}
						}
					}
					catch(std::bad_alloc)
					{
						delete d;
						return IONDOWNSAMPLE_BAD_ALLOC;
					}

					//skip ion output sets with no ions in them
					if(d->data.empty())
					{
						delete d;
						continue;
					}

					//Copy over other attributes
					d->r = ((IonStreamData *)dataIn[ui])->r;
					d->g = ((IonStreamData *)dataIn[ui])->g;
					d->b =((IonStreamData *)dataIn[ui])->b;
					d->a =((IonStreamData *)dataIn[ui])->a;
					d->ionSize =((IonStreamData *)dataIn[ui])->ionSize;
					d->valueType=((IonStreamData *)dataIn[ui])->valueType;

					//getOut is const, so shouldn't be modified
					cacheAsNeeded(d);

					getOut.push_back(d);
					break;
				}
			
				default:
					getOut.push_back(dataIn[ui]);
					break;
			}

		}
	}
	else
	{
		ASSERT(rsdIncoming);
		const IonStreamData *input;

		//Construct two vectors. One with the ion IDs for each input
		//ion stream. the other with the total number of ions in the input
		//for each ion type. There is an extra slot for unranged data
		vector<size_t> numIons,ionIDVec;
		numIons.resize(rsdIncoming->rangeFile->getNumIons()+1,0);

		for(unsigned int uj=0;uj<dataIn.size() ;uj++)
		{
			if(dataIn[uj]->getStreamType() == STREAM_TYPE_IONS)
			{
				input=(const IonStreamData*)dataIn[uj];
				if(input->data.size())
				{
					//Use the first ion to guess the identity of the entire stream
					unsigned int ionID;
					ionID=rsdIncoming->rangeFile->getIonID(
						input->data[0].getMassToCharge()); 
					if(ionID != (unsigned int)-1)
					{
						numIons[ionID]+=input->data.size();
						ionIDVec.push_back(ionID);
					}
					else
					{
						numIons[numIons.size()-1]+=input->data.size();
						ionIDVec.push_back(numIons.size()-1);
					}


				}
			}
		}

		size_t n=0;
		unsigned int idPos=0;
		for(size_t ui=0;ui<dataIn.size() ;ui++)
		{
			switch(dataIn[ui]->getStreamType())
			{
				case STREAM_TYPE_IONS: 
				{
					input=(const IonStreamData*)dataIn[ui];
			
					//Don't process ionstreams that are empty	
					if(input->data.empty())
						continue;

					IonStreamData *d;
					d=new IonStreamData;
					d->parent=this;
					try
					{
						if(fixedNumOut)
						{
							//if we are building the fixed number for output,
							//then compute the relative fraction for this ion set
							float frac;
							frac = (float)(input->data.size())/(float)(numIons[ionIDVec[idPos]]);

							//The total number of ions is the specified value for this ionID, multiplied by
							//this stream's fraction of the total incoming data
							randomSelect(d->data,input->data, rng,(size_t)(frac*ionLimits[ionIDVec[idPos]]),
									progress.filterProgress,*wantAbort,strongRandom);
							if(*Filter::wantAbort)
							{
								delete d;
								return FILTER_ERR_ABORT;
							}
						}
						else
						{
							//Use the direct fractions as entered in by user. 
							float thisFraction = ionFractions[ionIDVec[idPos]];
							
							//Reserve 90% of storage needed.
							//highly likely (Poisson) with even modest numbers of ions
							//that this will be exceeded, and thus we won't over-allocate
							d->data.reserve((size_t)(thisFraction*0.9*numIons[ionIDVec[idPos]]));

							if(thisFraction)
							{
								for(vector<IonHit>::const_iterator it=input->data.begin();
									       it!=input->data.end(); ++it)
								{
									if(rng.genUniformDev() <  thisFraction)
										d->data.push_back(*it);
								
									//update progress
									progress.filterProgress= 
										(unsigned int)((float)(n)/((float)totalSize)*100.0f);
									if(*Filter::wantAbort)
									{
										delete d;
										return FILTER_ERR_ABORT;
									}
								}
						
							}
						}
					}
					catch(std::bad_alloc)
					{
						delete d;
						return IONDOWNSAMPLE_BAD_ALLOC;
					}


					if(d->data.size())
					{
						//Copy over other attributes
						d->r = input->r;
						d->g = input->g;
						d->b =input->b;
						d->a =input->a;
						d->ionSize =input->ionSize;
						d->valueType=input->valueType;


						//getOut is const, so shouldn't be modified
						cacheAsNeeded(d);

						getOut.push_back(d);
					}
					else
						delete d;
					//next ion
					idPos++;
					
					break;
				}
			
				default:
					getOut.push_back(dataIn[ui]);
					break;
			}

		}


	}	

	progress.filterProgress=100;
	return 0;
}


void IonDownsampleFilter::getProperties(FilterPropGroup &propertyList) const
{

	FilterProperty p;
	size_t curGroup=0;

	string tmpStr;
	stream_cast(tmpStr,fixedNumOut);
	p.data=tmpStr;
	p.name=TRANS("By Count");
	p.key=KEY_IONDOWNSAMPLE_FIXEDOUT;
	p.type=PROPERTY_TYPE_BOOL;
	p.helpText=TRANS("Sample up to a fixed number of ions");
	propertyList.addProperty(p,curGroup);

	if(rsdIncoming)
	{
		stream_cast(tmpStr,perSpecies);
		p.name=TRANS("Per Species");
		p.data=tmpStr;
		p.key=KEY_IONDOWNSAMPLE_PERSPECIES;
		p.type=PROPERTY_TYPE_BOOL;
		p.helpText=TRANS("Use species specific (from ranging) sampling values");
		propertyList.addProperty(p,curGroup);
	}	


	propertyList.setGroupTitle(curGroup,TRANS("Mode"));
	curGroup++;
	if(rsdIncoming && perSpecies)
	{
		unsigned int typeVal;
		if(fixedNumOut)
			typeVal=PROPERTY_TYPE_INTEGER;
		else
			typeVal=PROPERTY_TYPE_REAL;

		unsigned int numIons=rsdIncoming->enabledIons.size();
		//create a  single line for each
		for(unsigned  int ui=0; ui<numIons; ui++)
		{
			if(rsdIncoming->enabledIons[ui])
			{
				if(fixedNumOut)
					stream_cast(tmpStr,ionLimits[ui]);
				else
					stream_cast(tmpStr,ionFractions[ui]);

				p.name=rsdIncoming->rangeFile->getName(ui);
				p.data=tmpStr;
				p.type=typeVal;
				p.helpText=TRANS("Sampling value for species");
				p.key=KEY_IONDOWNSAMPLE_DYNAMIC+ui;
				propertyList.addProperty(p,curGroup);
				
			}
		}

		p.name=TRANS("Unranged");
		if(fixedNumOut)
			stream_cast(tmpStr,ionLimits[numIons]);
		else
			stream_cast(tmpStr,ionFractions[numIons]);
		p.data=tmpStr;
		p.key=KEY_IONDOWNSAMPLE_DYNAMIC+numIons;
		propertyList.addProperty(p,curGroup);

		propertyList.setGroupTitle(curGroup,TRANS("Sampling rates"));
	}
	else
	{
		if(fixedNumOut)
		{
			stream_cast(tmpStr,maxAfterFilter);
			p.key=KEY_IONDOWNSAMPLE_COUNT;
			p.name=TRANS("Output Count");
			p.data=tmpStr;
			p.type=PROPERTY_TYPE_INTEGER;
			p.helpText=TRANS("Sample up to this value of points");
		}
		else
		{
			stream_cast(tmpStr,fraction);
			p.name=TRANS("Out Fraction");
			p.data=tmpStr;
			p.key=KEY_IONDOWNSAMPLE_FRACTION;
			p.type=PROPERTY_TYPE_REAL;
			p.helpText=TRANS("Sample this fraction of points");

		}
		propertyList.addProperty(p,curGroup);
		propertyList.setGroupTitle(curGroup,TRANS("Sampling rates"));
	}
}

bool IonDownsampleFilter::setProperty(  unsigned int key,
					const std::string &value, bool &needUpdate)
{
	needUpdate=false;
	switch(key)
	{
		case KEY_IONDOWNSAMPLE_FIXEDOUT: 
		{
			if(!applyPropertyNow(fixedNumOut,value,needUpdate))
				return false;
			break;
		}	
		case KEY_IONDOWNSAMPLE_FRACTION:
		{
			float newFraction;
			if(stream_cast(newFraction,value))
				return false;

			if(newFraction < 0.0f || newFraction > 1.0f)
				return false;

			//In the case of fixed number output, 
			//our cache is invalidated
			if(!fixedNumOut)
			{
				needUpdate=true;
				clearCache();
			}

			fraction=newFraction;
			

			break;
		}
		case KEY_IONDOWNSAMPLE_COUNT:
		{
			if(!applyPropertyNow(maxAfterFilter,value,needUpdate))
				return false;
			break;
		}	
		case KEY_IONDOWNSAMPLE_PERSPECIES: 
		{
			if(!applyPropertyNow(perSpecies,value,needUpdate))
				return false;
			break;
		}	
		default:
		{
			ASSERT(rsdIncoming);
			ASSERT(key >=KEY_IONDOWNSAMPLE_DYNAMIC);
			ASSERT(key < KEY_IONDOWNSAMPLE_DYNAMIC+ionLimits.size());
			ASSERT(ionLimits.size() == ionFractions.size());

			unsigned int offset;
			offset=key-KEY_IONDOWNSAMPLE_DYNAMIC;

			//TODO: Disable this test -
			// offset >=ionLimits.size()  did happen, but should not have. 
			// Can't reproduce bug - something to do with wrong filter being given selected properties in UI
			ASSERT( offset < ionLimits.size());
			if(offset >= ionLimits.size())
				return false;

			//Dynamically generated list of downsamples
			if(fixedNumOut)
			{
				//Fixed count
				size_t v;
				if(stream_cast(v,value))
					return false;
				ionLimits[offset]=v;
			}
			else
			{
				//Fixed fraction
				float v;
				if(stream_cast(v,value))
					return false;

				if(v < 0.0f || v> 1.0f)
					return false;

				ionFractions[offset]=v;
			}
			
			needUpdate=true;
			clearCache();
			break;
		}

	}	
	return true;
}


std::string  IonDownsampleFilter::getSpecificErrString(unsigned int code) const
{
	const char *errStrs[] = { "",	
		"Insuffient memory for downsample",
	};	
	COMPILE_ASSERT(THREEDEP_ARRAYSIZE(errStrs) == IONDOWNSAMPLE_ERR_ENUM_END);
	ASSERT(code < IONDOWNSAMPLE_ERR_ENUM_END);
	return errStrs[code];
}

void IonDownsampleFilter::setPropFromBinding(const SelectionBinding &b)
{
	ASSERT(false);
}


bool IonDownsampleFilter::writeState(std::ostream &f,unsigned int format, unsigned int depth) const
{
	using std::endl;
	switch(format)
	{
		case STATE_FORMAT_XML:
		{	
			f << tabs(depth) <<  "<" << trueName() << ">" << endl;
			f << tabs(depth+1) << "<userstring value=\""<< escapeXML(userString) << "\"/>"  << endl;

			f << tabs(depth+1) << "<fixednumout value=\""<<fixedNumOut<< "\"/>"  << endl;
			f << tabs(depth+1) << "<fraction value=\""<<fraction<< "\"/>"  << endl;
			f << tabs(depth+1) << "<maxafterfilter value=\"" << maxAfterFilter << "\"/>" << endl;
			f << tabs(depth+1) << "<perspecies value=\""<<perSpecies<< "\"/>"  << endl;
		
			writeScalarsXML(f,"fractions",ionFractions,depth+1);
			
			writeScalarsXML(f,"limits",ionLimits,depth+1);
			
			f << tabs(depth) << "</" <<trueName()<< ">" << endl;
			break;
		}
		default:
			ASSERT(false);
			return false;
	}

	return true;
}

bool IonDownsampleFilter::readState(xmlNodePtr &nodePtr, const std::string &stateFileDir)
{
	using std::string;
	string tmpStr;

	xmlChar *xmlString;
	//Retrieve user string
	if(XMLHelpFwdToElem(nodePtr,"userstring"))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)"value");
	if(!xmlString)
		return false;
	userString=(char *)xmlString;
	xmlFree(xmlString);

	//Retrieve number out (yes/no) mode
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"fixednumout","value"))
		return false;

	if(!boolStrDec(tmpStr,fixedNumOut))
		return false;
	//===
		
	//Retrieve Fraction
	//===
	if(!XMLGetNextElemAttrib(nodePtr,fraction,"fraction","value"))
		return false;
	//disallow negative or values gt 1.
	if(fraction < 0.0f || fraction > 1.0f)
		return false;
	//===

	//Retreive maxafterfilter
	//---
	if(!XMLGetNextElemAttrib(nodePtr,maxAfterFilter,"maxafterfilter","value"))
		return false;
	//---
	
	//Retrieve "perspecies" attrib
	if(!XMLGetNextElemAttrib(nodePtr,tmpStr,"perspecies","value"))
		return false;

	if(!boolStrDec(tmpStr,perSpecies))
		return false;

	//Retrieve the ion per-species fractions
	//--
	if(XMLHelpFwdToElem(nodePtr,"fractions"))
		return false;

	//Populate the ion fraction vector
	if(!readScalarsXML(nodePtr,ionFractions))
		return false;

	//Retrieve the ion per-species fractions
	if(XMLHelpFwdToElem(nodePtr,"limits"))
		return false;

	if(!readScalarsXML(nodePtr,ionLimits))
		return false;

	//--
	
	if(ionLimits.size()!=ionFractions.size())
		return false;

	return true;
}


unsigned int IonDownsampleFilter::getRefreshBlockMask() const
{
	return STREAM_TYPE_IONS ;
}

unsigned int IonDownsampleFilter::getRefreshEmitMask() const
{
	return  STREAM_TYPE_IONS;
}

unsigned int IonDownsampleFilter::getRefreshUseMask() const
{
	return  STREAM_TYPE_RANGE  | STREAM_TYPE_IONS;
}
//----------


//Unit testing for this class
///-----
#ifdef DEBUG

//Create a synthetic dataset of points
// returned pointer *must* be deleted. Span must have 3 elements, 
// and for best results could be co-prime with one another; e.g. all prime numbers
IonStreamData *synthDataPts(unsigned int span[],unsigned int numPts);

//Test for fixed number of output ions
bool fixedSampleTest();

//Test for variable number of output ions
bool variableSampleTest();

//Unit tests
bool IonDownsampleFilter::runUnitTests()
{
	if(!fixedSampleTest())
		return false;

	if(!variableSampleTest())
		return false;
	
	return true;
}

bool fixedSampleTest()
{
	//Simulate some data to send to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	IonStreamData *d= new IonStreamData;

	const unsigned int NUM_PTS=10000;
	for(unsigned int ui=0;ui<NUM_PTS;ui++)
	{
		IonHit h;
		h.setPos(Point3D(ui,ui,ui));
		h.setMassToCharge(ui);
		d->data.push_back(h);
	}


	streamIn.push_back(d);
	//Set up the filter itself
	IonDownsampleFilter *f=new IonDownsampleFilter;
	f->setCaching(false);

	bool needUp;
	string s;
	unsigned int numOutput=NUM_PTS/10;
	
	TEST(f->setProperty(KEY_IONDOWNSAMPLE_FIXEDOUT,"1",needUp),"Set prop");
	stream_cast(s,numOutput);
	TEST(f->setProperty(KEY_IONDOWNSAMPLE_COUNT,s,needUp),"Set prop");

	//Do the refresh
	ProgressData p;
	f->refresh(streamIn,streamOut,p);

	delete f;
	delete d;

	//Pass some tests
	TEST(streamOut.size() == 1, "Stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS, "stream type");
	TEST(streamOut[0]->getNumBasicObjects() == numOutput, "output ions (basicobject)"); 
	TEST( ((IonStreamData*)streamOut[0])->data.size() == numOutput, "output ions (direct)")

	delete streamOut[0];
	
	return true;
}

bool variableSampleTest()
{
	//Build some points to pass to the filter
	vector<const FilterStreamData*> streamIn,streamOut;
	
	unsigned int span[]={ 
			5, 7, 9
			};	
	const unsigned int NUM_PTS=10000;
	IonStreamData *d=synthDataPts(span,NUM_PTS);

	streamIn.push_back(d);
	IonDownsampleFilter *f=new IonDownsampleFilter;
	f->setCaching(false);	
	
	bool needUp;
	TEST(f->setProperty(KEY_IONDOWNSAMPLE_FIXEDOUT,"0",needUp),"Set prop");
	TEST(f->setProperty(KEY_IONDOWNSAMPLE_FRACTION,"0.1",needUp),"Set prop");

	//Do the refresh
	ProgressData p;
	TEST(!(f->refresh(streamIn,streamOut,p)),"refresh error code");

	delete f;
	delete d;


	TEST(streamOut.size() == 1,"stream count");
	TEST(streamOut[0]->getStreamType() == STREAM_TYPE_IONS,"stream type");

	//It is HIGHLY improbable that it will be <1/10th of the requested number
	TEST(streamOut[0]->getNumBasicObjects() > 0.01*NUM_PTS 
		&& streamOut[0]->getNumBasicObjects() <= NUM_PTS,"ion fraction");

	delete streamOut[0];


	return true;
}

IonStreamData *synthDataPts(unsigned int span[], unsigned int numPts)
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

#endif
///-----

