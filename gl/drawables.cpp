/*
 *	drawables.cpp - opengl drawable objects cpp file
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

#if defined(WIN32) || defined(WIN64)
#include <GL/glew.h>
#endif

#include "drawables.h"

#include "common/colourmap.h"

#include "common/voxels.h"

#include "glDebug.h"

#include "backend/filters/openvdb_includes.h"

#include <math.h> // for sqrt

const float DEPTH_SORT_REORDER_EPSILON = 1e-2;

//Static class variables
//====
const Camera *DrawableObj::curCamera = 0;

float DrawableObj::backgroundR;
float DrawableObj::backgroundG;
float DrawableObj::backgroundB;

bool DrawableObj::useAlphaBlend;
TexturePool *DrawableObj::texPool=0;

unsigned int DrawableObj::winX;
unsigned int DrawableObj::winY;

//==


//Draw a cone pointing in the axisVec direction, positioned at 
//	- (if translateAxis is true, origin+axisVec, otherwise origin)
//	- 
void drawCone(const Point3D &axisVec, const Point3D &origin, 
		float radius, float numConeRadiiLen, size_t numSegments,bool translateAxis=false)
{
	Point3D axis;
	axis =axisVec;
	if(axis.sqrMag() < sqrtf(std::numeric_limits<float>::epsilon()))
		axis=Point3D(0,0,1);
	else
		axis.normalise();


	//Tilt space to align to cone axis
	Point3D zAxis(0,0,1);
	float tiltAngle;
	tiltAngle = zAxis.angle(axis);
	
	Point3D rotAxis;
	rotAxis=zAxis.crossProd(axis);
	
	Point3D *ptArray = new Point3D[numSegments];

	const float ROT_TOL=sqrtf(std::numeric_limits<float>::epsilon()) ;

	//Only rotate if the angle is nonzero (note 2PI wraparound is possible from acos)
	if((tiltAngle > ROT_TOL	|| fabs(tiltAngle - 2*M_PI) > ROT_TOL) && 
			rotAxis.sqrMag() > ROT_TOL)
	{

		//Draw an angled cone
		Point3f vertex,r;	
		rotAxis.normalise();


		r.fx=rotAxis[0];
		r.fy=rotAxis[1];
		r.fz=rotAxis[2];

	
		//we have to rotate the cone points around the apex point
		for(unsigned int ui=0; ui<numSegments; ui++)
		{
			//Note that the ordering for theta defines the orientation
			// for the generated triangles. CCW triangles in opengl 
			// are required
			float theta;
			theta = -2.0f*M_PI*(float)ui/(float)(numSegments-1);

			//initial point is at r*(cos(theta),r*sin(theta),-numConeRadiiLen),
			vertex.fx=sin(theta);
			vertex.fy=cos(theta);
			vertex.fz=-numConeRadiiLen;
		
			//rotate to new position
			quat_rot(&vertex,&r,tiltAngle);

			//store the coord
			ptArray[ui]=Point3D(radius*vertex.fx,radius*vertex.fy,radius*vertex.fz);
		}
	}
	else
	{
		if(tiltAngle > ROT_TOL)
		{
			//Downwards pointing cone - note "-radius"
			for(unsigned int ui=0; ui<numSegments; ui++)
			{
				float theta;
				theta = 2.0f*M_PI*(float)ui/(float)(numSegments-1);
				ptArray[ui] =Point3D(-radius*cos(theta),
					radius*sin(theta),numConeRadiiLen*radius);
			}
		}
		else
		{
			//upwards pointing cone
			for(unsigned int ui=0; ui<numSegments; ui++)
			{
				float theta;
				theta = 2.0f*M_PI*(float)ui/(float)(numSegments-1);
				ptArray[ui] =Point3D(radius*cos(theta),
					radius*sin(theta),-numConeRadiiLen*radius);
			}
		}
	}


	//Translation vector
	Point3D trans;
	if(translateAxis)
		trans=(origin+axisVec);
	else
		trans=origin;
	glPushMatrix();
	glTranslatef(trans[0],trans[1],trans[2]);
	
	//Now, having the needed coords, we can draw the cone
	glBegin(GL_TRIANGLE_FAN);
	glNormal3fv(axis.getValueArr());
	glVertex3f(0,0,0);
	for(unsigned int ui=0; ui<numSegments; ui++)
	{
		Point3D n;
		n=ptArray[ui];
		n.normalise();
		glNormal3fv(n.getValueArr());
		glVertex3fv(ptArray[ui].getValueArr());
	}

	glEnd();

	//Now draw the base of the cone, to make it solid
	// Note that traversal order of pt array is also important
	glBegin(GL_TRIANGLE_FAN);
	glNormal3f(-axis[0],-axis[1],-axis[2]);
	for(unsigned int ui=numSegments; ui--;) 
		glVertex3fv(ptArray[ui].getValueArr());
	glEnd();

	glPopMatrix();
	delete[] ptArray;
}


//Common functions
//
void drawBox(Point3D pMin, Point3D pMax, float r,float g, float b, float a)
{
	//TODO: Could speedup with LINE_STRIP/LOOP. This is 
	//not a bottleneck atm though.
	glColor4f(r,g,b,a);
	glBegin(GL_LINES);
		//Bottom corner out (three lines from corner)
		glVertex3f(pMin[0],pMin[1],pMin[2]);
		glVertex3f(pMax[0],pMin[1],pMin[2]);
		
		glVertex3f(pMin[0],pMin[1],pMin[2]);
		glVertex3f(pMin[0],pMax[1],pMin[2]);

		glVertex3f(pMin[0],pMin[1],pMin[2]);
		glVertex3f(pMin[0],pMin[1],pMax[2]);
		
		//Top Corner out (three lines from corner)
		glVertex3f(pMax[0],pMax[1],pMax[2]);
		glVertex3f(pMin[0],pMax[1],pMax[2]);
	
		glVertex3f(pMax[0],pMax[1],pMax[2]);
		glVertex3f(pMax[0],pMin[1],pMax[2]);
		
		glVertex3f(pMax[0],pMax[1],pMax[2]);
		glVertex3f(pMax[0],pMax[1],pMin[2]);

		//Missing pieces - in an "across-down-across" shape
		glVertex3f(pMin[0],pMax[1],pMin[2]);
		glVertex3f(pMax[0],pMax[1],pMin[2]);
		
		glVertex3f(pMax[0],pMax[1],pMin[2]);
		glVertex3f(pMax[0],pMin[1],pMin[2]);

		glVertex3f(pMax[0],pMin[1],pMin[2]);
		glVertex3f(pMax[0],pMin[1],pMax[2]);
		
		glVertex3f(pMax[0],pMin[1],pMax[2]);
		glVertex3f(pMin[0],pMin[1],pMax[2]);
		
		glVertex3f(pMin[0],pMin[1],pMax[2]);
		glVertex3f(pMin[0],pMax[1],pMax[2]);

		glVertex3f(pMin[0],pMax[1],pMax[2]);
		glVertex3f(pMin[0],pMax[1],pMin[2]);
	glEnd();
}


DrawableObj::DrawableObj() : active(true), haveChanged(true), canSelect(false), wantsLight(false)
{
}

DrawableObj::~DrawableObj()
{
}
	
	
float DrawableObj::getHighContrastValue() 
{
	//Perform luminence check on background to try to create most appropriate
	// colour
	//-------
	// TODO: I have this in a few places now, need to refactor into a single colour class

	//weights
 	const float CHANNEL_LUM_WEIGHTS[3] = { 0.299f,0.587f,0.114f};
	float totalBright=backgroundR*CHANNEL_LUM_WEIGHTS[0] +
			backgroundG*CHANNEL_LUM_WEIGHTS[1] +
			backgroundB*CHANNEL_LUM_WEIGHTS[2];

	float contrastCol;
	if(totalBright > 0.5f)
	{
		//"bright" scene, use black text
		contrastCol=0.0f;
	}
	else
	{
		//"Dark" background, use white text
		contrastCol=1.0f;
	}

	return contrastCol;

}

void DrawableObj::explode(std::vector<DrawableObj *> &simpleObjects)
{
	ASSERT(isExplodable());
}

void DrawableObj::setTexPool(TexturePool *t)
{
	if(texPool)
		delete texPool;

	texPool=t;

}

void DrawableObj::clearTexPool()
{
	ASSERT(texPool);
	delete texPool;
	texPool=0;
}

Point3D DrawableObj::getCentroid() const
{
	ASSERT(!isExplodable());
}

//=====

DrawPoint::DrawPoint() : origin(0.0f,0.0f,0.0f), r(1.0f), g(1.0f), b(1.0f), a(1.0f)
{
}

DrawPoint::DrawPoint(float x, float y, float z) : origin(x,y,z), r(1.0f), g(1.0f), b(1.0f)
{
}

DrawPoint::~DrawPoint()
{
}

DrawableObj* DrawPoint::clone() const
{
	DrawPoint *d = new DrawPoint(*this);
	return d;
}



void DrawPoint::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}



void DrawPoint::setOrigin(const Point3D &pt)
{
	origin = pt;
}


void DrawPoint::draw() const
{
	glColor4f(r,g,b,a);
	glBegin(GL_POINT);
	glVertex3fv(origin.getValueArr());
	glEnd();
}

DrawVector::DrawVector() : origin(0.0f,0.0f,0.0f), vector(0.0f,0.0f,1.0f),drawArrow(true),
			arrowSize(1.0f),scaleArrow(true),doubleEnded(false),
			r(1.0f), g(1.0f), b(1.0f), a(1.0f), lineSize(1.0f)
{
}

DrawVector::~DrawVector()
{
}

DrawableObj* DrawVector::clone() const
{
	DrawVector *d = new DrawVector(*this);
	return d;
}

void DrawVector::getBoundingBox(BoundCube &b) const 
{
	b.setBounds(origin,vector+origin);
}

void DrawVector::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}


void DrawVector::setEnds(const Point3D &startNew, const Point3D &endNew)
{
	origin = startNew;
	vector =endNew-startNew;
}

void DrawVector::setOrigin(const Point3D &pt)
{
	origin = pt;
}

void DrawVector::setVector(const Point3D &pt)
{
	vector= pt;
}

void DrawVector::draw() const
{
	const unsigned int NUM_CONE_SEGMENTS=20;
	const float numConeRadiiLen = 1.5f; 
	const float radius= arrowSize;
	
	glColor3f(r,g,b);

	//Disable lighting calculations for arrow stem
	glPushAttrib(GL_LIGHTING_BIT);
	glDisable(GL_LIGHTING);
	float oldLineWidth;
	glGetFloatv(GL_LINE_WIDTH,&oldLineWidth);

	glLineWidth(lineSize);
	glBegin(GL_LINES);

	if(drawArrow)
	{
		//Back off the distance a little, because otherwise the line can poke out
		// the sides of the cone.
		float backoffFactor = std::max(radius/sqrtf(vector.sqrMag()),0.0f);
		Point3D tmpVec=vector*(1.0f-backoffFactor) + origin;
		
		if(doubleEnded)
		{
			Point3D tmpOrigin;
			tmpOrigin = origin+vector*(backoffFactor);
			glVertex3fv(tmpOrigin.getValueArr());
			glVertex3fv(tmpVec.getValueArr());
		}
		else
		{
			glVertex3fv(origin.getValueArr());
			glVertex3fv(tmpVec.getValueArr());
		}
	}
	else
	{
		glVertex3fv(origin.getValueArr());
		glVertex3f(vector[0]+origin[0],vector[1]+origin[1],vector[2]+origin[2]);
	}
	glEnd();

	//restore the old line size
	glLineWidth(oldLineWidth);
	glPopAttrib();

	//If we only wanted the line, then we are done here.
	if(arrowSize < sqrtf(std::numeric_limits<float>::epsilon()) || !drawArrow)
		return ;

	//Now compute & draw the cone tip
	//----
	drawCone(vector, origin, arrowSize,
		numConeRadiiLen,NUM_CONE_SEGMENTS,true);

	if(doubleEnded)
		drawCone(-vector,origin,arrowSize,numConeRadiiLen,NUM_CONE_SEGMENTS);

	//----


}

void DrawVector::recomputeParams(const std::vector<Point3D> &vecs, 
			const std::vector<float> &scalars, unsigned int mode)
{
	switch(mode)
	{
		case DRAW_VECTOR_BIND_ORIENTATION:
			ASSERT(vecs.size() ==1 && scalars.size() ==0);
			vector=vecs[0];
			break;
		case DRAW_VECTOR_BIND_ORIGIN:
			ASSERT(vecs.size() == 1 && scalars.size()==0);
			origin=vecs[0];
			break;
		case DRAW_VECTOR_BIND_ORIGIN_ONLY:
		{
			ASSERT(vecs.size() == 1 && scalars.size()==0);

			Point3D dv;
			dv=vector-origin;
			origin=vecs[0];
			vector=origin+dv;
			break;
		}
		case DRAW_VECTOR_BIND_TARGET:
			ASSERT(vecs.size() == 1 && scalars.size()==0);
			vector=vecs[0]-origin;
			break;
		default:
			ASSERT(false);
	}
}



DrawTriangle::DrawTriangle() : r(1.0f), g(1.0f),b(1.0f),a(1.0f)
{
}

DrawTriangle::~DrawTriangle()
{
}


DrawableObj* DrawTriangle::clone() const
{
	DrawTriangle *d = new DrawTriangle(*this);
	return d;
}

void DrawTriangle::setVertex(unsigned int ui, const Point3D &pt)
{
	ASSERT(ui < 3);
	vertices[ui] = pt;
}

void DrawTriangle::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawTriangle::draw() const
{
	glColor4f(r,g,b,a);
	glBegin(GL_TRIANGLES);
		for(size_t ui=0;ui<3;ui++)
			glVertex3fv(vertices[ui].getValueArr());
	glEnd();
}

DrawableObj* DrawQuad::clone() const
{
	DrawQuad *d = new DrawQuad(*this);
	return d;
}


void DrawQuad::getBoundingBox(BoundCube &b) const
{
	b.setBounds(vertices,4);
}

void DrawQuad::draw() const
{
	ASSERT(false);
}

void DrawQuad::setVertices(const Point3D *v) 
{
	for(size_t ui=0;ui<4;ui++)
		vertices[ui]=v[ui];
}

void DrawQuad::setVertex(unsigned int v, const Point3D &p)
{
	ASSERT(v <4);
	vertices[v] = p;
}

void DrawQuad::setColour(float rNew, float gNew, float bNew, float aNew)
{
	ASSERT(rNew >=0 && rNew <=1.0f);
	ASSERT(gNew >=0 && gNew <=1.0f);
	ASSERT(bNew >=0 && bNew <=1.0f);
	ASSERT(aNew >=0 && aNew <=1.0f);
	for(unsigned int ui=0;ui<4; ui++)
	{
		r[ui]=rNew;		
		g[ui]=gNew;		
		b[ui]=bNew;		
		a[ui]=aNew;		
	}
}

Point3D DrawQuad::getOrigin() const
{
	return Point3D::centroid(vertices,4);
}

void DrawQuad::recomputeParams(const vector<Point3D> &vecs, 
			const vector<float> &scalars, unsigned int mode)
{
	switch(mode)
	{
		case DRAW_QUAD_BIND_ORIGIN:
		{
			ASSERT(vecs.size() ==1 && scalars.size() ==0);
			
			Point3D curOrig=getOrigin();

			Point3D delta = vecs[0]-curOrig;

			for(size_t ui=0;ui<4;ui++)
				vertices[ui]+=delta;


			break;
		}
		default:
			ASSERT(false);
	}
}

DrawTexturedQuad::DrawTexturedQuad() :textureData(0), textureId((unsigned int)-1), noColour(false) , needsBinding(true)
{
}

DrawTexturedQuad::DrawTexturedQuad(const DrawTexturedQuad &oth)
{
	ASSERT(false);
}

DrawTexturedQuad::~DrawTexturedQuad()
{
	//hack to work around static construct/destruct.
	// normally we use the texture pool do to everything
	if(texPool && textureId != -1)
	{
		texPool->closeTexture(textureId);
		textureId=-1;
	}

	if(textureData)
		delete[] textureData;
}

void DrawTexturedQuad::draw() const
{
	if(needsBinding)
	{
		rebindTexture();
	}

	ASSERT(glIsTexture(textureId));
	
	glEnable(GL_TEXTURE_2D);
	glPushAttrib(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);
	
	
	glBindTexture(GL_TEXTURE_2D,textureId);
	const float COORD_SEQ_X[]={ 0,0,1,1};
	const float COORD_SEQ_Y[]={ 0,1,1,0};

	if(!noColour)
	{
		glBegin(GL_QUADS);
		for(size_t ui=0;ui<4;ui++)
		{
			glColor4f(r[ui],g[ui],b[ui],a[ui]);
			glTexCoord2f(COORD_SEQ_X[ui],COORD_SEQ_Y[ui]);
			glVertex3fv(vertices[ui].getValueArr());
		}
		glEnd();
	}	
	else
	{
		glBegin(GL_QUADS);
			for(size_t ui=0;ui<4;ui++)
			{
				glColor4f(1.0f,1.0f,1.0f,a[ui]);
				glTexCoord2f(COORD_SEQ_X[ui],COORD_SEQ_Y[ui]);
				glVertex3fv(vertices[ui].getValueArr());
			}
		glEnd();
	}
	glPopAttrib();
	glDisable(GL_TEXTURE_2D);	
}

//Call sequence
// - resize destination for texture
// - set texture by pixels
// - rebind texture
void DrawTexturedQuad::resize(size_t numX, size_t numY, 
					unsigned int nChannels)
{
	//reallocate texture as required
	if(textureData)
	{
		if( numX*numY*nChannels != nX*nY*channels)
		{
			delete[] textureData;
			textureData = new unsigned char[numX*numY*nChannels];
		}
	}
	else
		textureData = new unsigned char[numX*numY*nChannels];

	nX=numX;
	nY=numY;
	channels=nChannels;

}

void DrawTexturedQuad::rebindTexture(unsigned int mode) const
{
	ASSERT(texPool);
	ASSERT(textureData);
	if(textureId == (unsigned int)-1)
		texPool->genTexID(textureId);

	ASSERT(!(mode == GL_RGB && channels !=3 ));
	ASSERT(!(mode == GL_RGBA && channels !=4 ));

	//Construct the texture
	glBindTexture(GL_TEXTURE_2D,textureId);
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE); 

	glTexImage2D(GL_TEXTURE_2D,0,mode,nX,nY,
		0,mode,GL_UNSIGNED_BYTE,textureData);

	needsBinding=false;	
}

void DrawTexturedQuad::setData(size_t x, size_t y, unsigned char *entry)
{
	ASSERT(textureData);
	ASSERT(x < nX && y < nY);

	for(size_t ui=0;ui<channels;ui++)
		textureData[(y*nX + x)*channels + ui] = entry[ui]; 	
}


DrawSphere::DrawSphere() : radius(1.0f), latSegments(8),longSegments(8)
{
	q=gluNewQuadric();
}

DrawSphere::~DrawSphere()
{
	if(q)
		gluDeleteQuadric(q);
}

DrawableObj *DrawSphere::clone() const
{
	DrawSphere *d = new DrawSphere();
	d->r=r;
	d->g=g;
	d->b=b;
	d->a=a;
	d->origin=origin;
	d->radius=radius;
	d->latSegments=latSegments;
	d->longSegments=longSegments;

	d->q=gluNewQuadric();
	return d;
}

void DrawSphere::getBoundingBox(BoundCube &b) const
{
	for(unsigned int ui=0;ui<3;ui++)
	{
		b.setBound(ui,0,origin[ui] - radius);
		b.setBound(ui,1,origin[ui] + radius);
	}
}

void DrawSphere::setOrigin(const Point3D &p)
{
	origin = p;
}

void DrawSphere::setLatSegments(unsigned int ui)
{
	latSegments = ui;
}

void DrawSphere::setLongSegments(unsigned int ui)
{
	longSegments = ui;
}

void DrawSphere::setRadius(float rad)
{
	radius=rad;
}

void DrawSphere::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawSphere::draw() const 
{
	if(!q)
		return;

	glPushMatrix();
		glTranslatef(origin[0],origin[1],origin[2]);
		glColor4f(r,g,b,a);
		gluSphere(q,radius,latSegments,longSegments);
	glPopMatrix();
}


void DrawSphere::recomputeParams(const vector<Point3D> &vecs, 
			const vector<float> &scalars, unsigned int mode)
{
	switch(mode)
	{
		case DRAW_SPHERE_BIND_ORIGIN:
			ASSERT(vecs.size() ==1 && scalars.size() ==0);
			origin=vecs[0];
			break;
		case DRAW_SPHERE_BIND_RADIUS:
			ASSERT(scalars.size() == 1 && vecs.size()==0);
			radius=scalars[0];
			break;
		default:
			ASSERT(false);
	}
}
//===========

DrawCylinder::DrawCylinder() : radius(1.0f), 
		origin(0.0f,0.0f,0.0f), direction(0.0f,0.0f,1.0f), slices(4),stacks(4)
{
	q= gluNewQuadric();
	qCap[0]= gluNewQuadric();
	if(qCap[0])
		gluQuadricOrientation(qCap[0],GLU_INSIDE);	
	qCap[1]= gluNewQuadric();
	if(qCap[1])
		gluQuadricOrientation(qCap[1],GLU_OUTSIDE);	
	radiiLocked=false;
}

bool DrawCylinder::needsDepthSorting()  const
{
	return a< 1 && a > std::numeric_limits<float>::epsilon();
}

DrawCylinder::~DrawCylinder()
{
	if(q)
		gluDeleteQuadric(q);
	if(qCap[0])
		gluDeleteQuadric(qCap[0]);
	if(qCap[1])
		gluDeleteQuadric(qCap[1]);
}


void DrawCylinder::setOrigin(const Point3D& pt)
{
	origin=pt;
}


void DrawCylinder::setDirection(const Point3D &p)
{
	direction=p;
}


void DrawCylinder::draw() const
{
	if(!q || !qCap[0] || !qCap[1])
		return;

	//Cross product desired direction with default
	//direction to produce rotation vector
	Point3D dir(0.0f,0.0f,1.0f);

	glPushMatrix();
	glTranslatef(origin[0],origin[1],origin[2]);

	Point3D dirNormal(direction);
	dirNormal.normalise();

	float length=sqrtf(direction.sqrMag());
	float angle = dir.angle(dirNormal);
	if(angle < M_PI - sqrtf(std::numeric_limits<float>::epsilon()) &&
		angle > sqrtf(std::numeric_limits<float>::epsilon()))
	{
		//we need to rotate
		dir = dir.crossProd(dirNormal);

		glRotatef(angle*180.0f/M_PI,dir[0],dir[1],dir[2]);
	}

	//OpenGL defined cylinder starting at 0 and going to length. I want it starting at 0 and going to+-l/2
	glTranslatef(0,0,-length/2.0f);
	glColor4f(r,g,b,a);
	
	//Draw the end cap at z=0
	if(radiiLocked)
	{
		gluDisk(qCap[0],0,radius,slices,1);
		gluCylinder(q,radius,radius, length,slices,stacks);

		//Draw the start cap at z=l	
		glTranslatef(0,0,length);
		gluDisk(qCap[1],0,radius,slices,1);
	}
	else
	{
		ASSERT(false);
	}

	glPopMatrix();
}

void DrawCylinder::setSlices(unsigned int i)
{
	slices=i;
}

void DrawCylinder::setStacks(unsigned int i)
{
	stacks=i;
}

void DrawCylinder::setRadius(float rad)
{
	radius=rad;
}

void DrawCylinder::recomputeParams(const vector<Point3D> &vecs, 
			const vector<float> &scalars, unsigned int mode)
{
	switch(mode)
	{
		case DRAW_CYLINDER_BIND_ORIGIN:
			ASSERT(vecs.size() ==1 && scalars.size() ==0);
			origin=vecs[0];
			break;

		case DRAW_CYLINDER_BIND_DIRECTION:
			ASSERT(vecs.size() ==1 && scalars.size() ==0);
			direction=vecs[0];
			break;
		case DRAW_CYLINDER_BIND_RADIUS:
			ASSERT(scalars.size() == 1 && vecs.size()==0);
			radius=scalars[0];
			break;
		default:
			ASSERT(false);
	}
}


void DrawCylinder::setLength(float len)
{
	ASSERT(direction.sqrMag());
	direction=direction.normalise()*len;
}

void DrawCylinder::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawCylinder::getBoundingBox(BoundCube &b) const
{

	float tmp;

	Point3D normAxis(direction);
	normAxis.normalise();
	
	//Height offset for ending circles. 
	//The joint bounding box of these two is the 
	//overall bounding box
	Point3D offset;



	//X component
	tmp=sin(acos(normAxis.dotProd(Point3D(1,0,0))));
	offset[0] = radius*tmp;

	//Y component
	tmp=sin(acos(normAxis.dotProd(Point3D(0,1,0))));
	offset[1] = radius*tmp;

	//Z component
	tmp=sin(acos(normAxis.dotProd(Point3D(0,0,1))));
	offset[2] = radius*tmp;

	vector<Point3D> p;
	p.resize(4);
	p[0]= offset+(direction*0.5+origin);
	p[1]= -offset+(direction*0.5+origin);
	p[2]= offset+(-direction*0.5+origin);
	p[3]= -offset+(-direction*0.5+origin);

	b.setBounds(p);
}


//======

DrawManyPoints::DrawManyPoints() : r(1.0f),g(1.0f),b(1.0f),a(1.0f), size(1.0f)
{
	wantsLight=false;
}

DrawManyPoints::~DrawManyPoints() 
{
}


DrawableObj* DrawManyPoints::clone() const
{
	DrawManyPoints *d = new DrawManyPoints(*this);
	return d;
}

void DrawManyPoints::getBoundingBox(BoundCube &b) const
{

	//Update the cache as needed
	if(!haveCachedBounds)
	{
		haveCachedBounds=true;
		cachedBounds.setBounds(pts);
	}

	b=cachedBounds;
	return;
}

void DrawManyPoints::clear()
{
	pts.clear();
}

void DrawManyPoints::addPoints(const vector<Point3D> &vp)
{
	pts.resize(pts.size()+vp.size());
	std::copy(vp.begin(),vp.end(),pts.begin());
	haveCachedBounds=false;
}

void DrawManyPoints::shuffle()
{
	std::random_shuffle(pts.begin(),pts.end());
}

void DrawManyPoints::resize(size_t resizeVal)
{
	pts.resize(resizeVal);
	haveCachedBounds=false;
}


void DrawManyPoints::setPoint(size_t offset,const Point3D &p)
{
	ASSERT(!haveCachedBounds);
	pts[offset]=p;
}

void DrawManyPoints::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawManyPoints::setSize(float f)
{
	size=f;
}

void DrawManyPoints::draw() const
{
	//Don't draw transparent objects
	if(a < std::numeric_limits<float>::epsilon())
		return;

	glPointSize(size); 
	glBegin(GL_POINTS);
		glColor4f(r,g,b,a);
		//TODO: Consider Vertex buffer objects. would be faster, but less portable.
		for(unsigned int ui=0; ui<pts.size(); ui++)
		{
			glVertex3fv(pts[ui].getValueArr());
		}
	glEnd();
}

//======

DrawDispList::DrawDispList() : listNum(0),listActive(false)
{
}

DrawDispList::~DrawDispList()
{
	if(listNum)
	{
		ASSERT(!listActive);
		ASSERT(glIsList(listNum));
		glDeleteLists(listNum,1);
	}

}

bool DrawDispList::startList(bool execute)
{
	//Ensure that the user has appropriately closed the list
	ASSERT(!listActive);
	boundBox.setInverseLimits();
	
	//If the list is already genned, clear it
	if(listNum)
		glDeleteLists(listNum,1);

	//Create the display list (ask for one)
	listNum=glGenLists(1);

	if(listNum)
	{
		if(execute)
			glNewList(listNum,GL_COMPILE_AND_EXECUTE);
		else
			glNewList(listNum,GL_COMPILE);
		listActive=true;
	}
	return (listNum!=0);
}

void DrawDispList::addDrawable(const DrawableObj *d)
{
	ASSERT(listActive);
	BoundCube b;
	d->getBoundingBox(b);
	boundBox.expand(b);
	d->draw();
}

bool DrawDispList::endList()
{
	glEndList();

	ASSERT(boundBox.isValid());
	listActive=false;	
	return (glGetError() ==0);
}

void DrawDispList::draw() const
{
	ASSERT(!listActive);

	//Cannot select display list objects,
	//as we cannot modify them without a "do-over".
	ASSERT(!canSelect);

	ASSERT(glIsList(listNum));
	//Execute the list	
	glPushMatrix();
	glCallList(listNum);
	glPopMatrix();
}

//========


DrawGLText::DrawGLText(std::string fontFile, unsigned int mode) :font(0),fontString(fontFile),
	curFontMode(mode), origin(0.0f,0.0f,0.0f), 
	r(0.0),g(0.0),b(0.0),a(1.0), up(0.0f,1.0f,0.0f),  
	textDir(1.0f,0.0f,0.0f), readDir(0.0f,0.0f,1.0f), 
	isOK(true),ensureReadFromNorm(true) 
{

	font=0;
	switch(mode)
	{
		case FTGL_BITMAP:
			font = new FTGLBitmapFont(fontFile.c_str());
			break;
		case FTGL_PIXMAP:
			font = new FTGLPixmapFont(fontFile.c_str());
			break;
		case FTGL_OUTLINE:
			font = new FTGLOutlineFont(fontFile.c_str());
			break;
		case FTGL_POLYGON:
			font = new FTGLPolygonFont(fontFile.c_str());
			break;
		case FTGL_EXTRUDE:
			font = new FTGLExtrdFont(fontFile.c_str());
			break;
		case FTGL_TEXTURE:
			font = new FTGLTextureFont(fontFile.c_str());
			break;
		default:
			//Don't do this. Use valid font numbers
			ASSERT(false);
			font=0; 
	}

	//In case of allocation failure or invalid font num
	if(!font || font->Error())
	{
		isOK=false;
		return;
	}

	//Try to make it 100 point
	font->FaceSize(5);
	font->Depth(20);

	//Use unicode
	font->CharMap(ft_encoding_unicode);

	alignMode = DRAWTEXT_ALIGN_LEFT;
}

DrawGLText::DrawGLText(const DrawGLText &oth) : font(0), fontString(oth.fontString),
	curFontMode(oth.curFontMode), origin(oth.origin), r(oth.r),
	g(oth.g), b(oth.b), a(oth.a), up(oth.up), textDir(oth.textDir),
	readDir(oth.readDir),isOK(oth.isOK), ensureReadFromNorm(oth.ensureReadFromNorm)
{

	font=0;
	switch(curFontMode)
	{
		case FTGL_BITMAP:
			font = new FTGLBitmapFont(fontString.c_str());
			break;
		case FTGL_PIXMAP:
			font = new FTGLPixmapFont(fontString.c_str());
			break;
		case FTGL_OUTLINE:
			font = new FTGLOutlineFont(fontString.c_str());
			break;
		case FTGL_POLYGON:
			font = new FTGLPolygonFont(fontString.c_str());
			break;
		case FTGL_EXTRUDE:
			font = new FTGLExtrdFont(fontString.c_str());
			break;
		case FTGL_TEXTURE:
			font = new FTGLTextureFont(fontString.c_str());
			break;
		default:
			//Don't do this. Use valid font numbers
			ASSERT(false);
			font=0; 
	}

	//In case of allocation failure or invalid font num
	if(!font || font->Error())
	{
		isOK=false;
		return;
	}

	//Try to make it 100 point
	font->FaceSize(5);
	font->Depth(20);
	//Use unicode
	font->CharMap(ft_encoding_unicode);
}

void DrawGLText::draw() const
{
	if(!isOK)
		return;

	//Translate the drawing position to the origin
	Point3D offsetVec=textDir;
	float advance, halfHeight;

	{
	FTBBox box;
	box=font->BBox(strText.c_str());
	advance=box.Upper().X()-box.Lower().X();
	
	halfHeight=box.Upper().Y()-box.Lower().Y();
	halfHeight/=2.0f;
	}

	switch(alignMode)
	{
		case DRAWTEXT_ALIGN_LEFT:
			break;
		case DRAWTEXT_ALIGN_CENTRE:
			offsetVec=offsetVec*advance/2.0f;
			break;
		case DRAWTEXT_ALIGN_RIGHT:
			offsetVec=offsetVec*advance;
			break;
		default:
			ASSERT(false);
	}
	

	glPushMatrix();


	glPushAttrib(GL_CULL_FACE);

	glDisable(GL_CULL_FACE);
	if(curFontMode !=FTGL_BITMAP)
	{
		offsetVec=origin-offsetVec;
		glTranslatef(offsetVec[0],offsetVec[1],offsetVec[2]);

		//Rotate such that the new X-Y plane is set to the
		//desired text orientation. (ie. we want to draw the text in the 
		//specified combination of updir-textdir, rather than in the X-y plane)

		//---	
		//Textdir and updir MUST be normal to one another
		ASSERT(textDir.dotProd(up) < sqrtf(std::numeric_limits<float>::epsilon()));

		//rotate around textdir cross X, if the two are not the same
		Point3D newUp=up;
		float angle=textDir.angle(Point3D(1,0,0) );
		if(angle > sqrtf(std::numeric_limits<float>::epsilon()))
		{
			Point3D rotateAxis;
			rotateAxis = textDir.crossProd(Point3D(-1,0,0));
			rotateAxis.normalise();
			
			Point3f tmp,axis;
			tmp.fx=up[0];
			tmp.fy=up[1];
			tmp.fz=up[2];

			axis.fx=rotateAxis[0];
			axis.fy=rotateAxis[1];
			axis.fz=rotateAxis[2];


			glRotatef(angle*180.0f/M_PI,rotateAxis[0],rotateAxis[1],rotateAxis[2]);
			quat_rot(&tmp,&axis,angle); //angle is in radiians

			newUp[0]=tmp.fx;
			newUp[1]=tmp.fy;
			newUp[2]=tmp.fz;
		}

		//rotate new up direction into y around x axis
		angle = newUp.angle(Point3D(0,1,0));
		if(angle > sqrtf(std::numeric_limits<float>::epsilon()) &&
			fabs(angle - M_PI) > sqrtf(std::numeric_limits<float>::epsilon())) 
		{
			Point3D rotateAxis;
			rotateAxis = newUp.crossProd(Point3D(0,-1,0));
			rotateAxis.normalise();
			glRotatef(angle*180.0f/M_PI,rotateAxis[0],rotateAxis[1],rotateAxis[2]);
		}

		//Ensure that the text is not back-culled (i.e. if the
		//text normal is pointing away from the camera, it does not
		//get drawn). Here we have to flip the normal, by spinning the 
		//text by 180 around its up direction (which has been modified
		//by above code to coincide with the y axis.
		if(curCamera)
		{
			//This is not *quite* right in perspective mode
			//but is right in orthogonal

			Point3D textNormal,camVec;
			textNormal = up.crossProd(textDir);
			textNormal.normalise();

			camVec = origin - curCamera->getOrigin();

			//ensure the camera is not sitting on top of the text.			
			if(camVec.sqrMag() > std::numeric_limits<float>::epsilon())
			{

				camVec.normalise();

				if(camVec.dotProd(textNormal) < 0)
				{
					//move halfway along text, noting that 
					//the text direction is now the x-axis
					glTranslatef(advance/2.0f,halfHeight,0);
					//spin text around its up direction 180 degrees
					glRotatef(180,0,1,0);
					//restore back to original position
					glTranslatef(-advance/2.0f,-halfHeight,0);
				}
			
				camVec=curCamera->getUpDirection();	
				if(camVec.dotProd(up) < 0)
				{
					//move halfway along text, noting that 
					//the text direction is now the x-axis
					glTranslatef(advance/2.0f,halfHeight,0);
					//spin text around its front direction 180 degrees
					//no need to translate as text sits at its baseline
					glRotatef(180,0,0,1);
					//move halfway along text, noting that 
					//the text direction is now the x-axis
					glTranslatef(-advance/2.0f,-halfHeight,0);
				}

			}


		}

	}
	else
	{
		//FIXME: The text ends up in a weird location
		//2D coordinate storage for bitmap text
		double xWin,yWin,zWin;
		//Compute the 2D coordinates
		double model_view[16];
		glGetDoublev(GL_MODELVIEW_MATRIX, model_view);

		double projection[16];
		glGetDoublev(GL_PROJECTION_MATRIX, projection);

		int viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);

		//Apply the openGL coordinate transformation pipeline to the
		//specified coords
		gluProject(offsetVec[0],offsetVec[1],offsetVec[2]
				,model_view,projection,viewport,
					&xWin,&yWin,&zWin);

		glRasterPos3f(xWin,yWin,zWin);

	}
	//---


	glColor4f(r,g,b,a);

	//Draw text
	if(curFontMode == FTGL_TEXTURE)
	{
		glPushAttrib(GL_ENABLE_BIT);
		glEnable(GL_TEXTURE_2D);
	
		font->Render(strText.c_str());
		glPopAttrib();
	}
	else
		font->Render(strText.c_str());

	glPopAttrib();
	
	glPopMatrix();

}

DrawGLText::~DrawGLText()
{
	if(font)
		delete font;
}

void DrawGLText::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawGLText::getBoundingBox(BoundCube &b) const
{
	//Box forwards transformations
	// * Translation by [origin-textDir*  (maxx - minx)]
	// * Rotate by textDir.angle([1 0 0 ]), around [ textdir x [ -1 0 0 ] ]  
	// * Rotate by newUp.angle([0,1,0]), around [ newUp x [ 0 -1 0 ] ]
	if(isOK)
	{
		//Obtain the vertices around the untransformed text
		float minX,minY,minZ;
		float maxX,maxY,maxZ;
		font->BBox(strText.c_str(),minX,minY,minZ,maxX,maxY,maxZ);

		float dy=maxY-minY;


		b.setBounds(minX,minY,minZ,
				maxX,maxY,maxZ);
		vector<Point3D> p;
		b.getVertices(p,true);

		for(size_t ui=0;ui<p.size();ui++)
			p[ui]-=Point3D(0,-dy*0.5,0);

		const float TOL_EPS=sqrtf(std::numeric_limits<float>::epsilon());
		
		Point3D r1Axis,r2Axis;
		bool degenR1,degenR2;
		r1Axis=Point3D(1,0,0);
	
		//Compute R1 axis, but do not apply
		//--
		float r1Angle=r1Axis.angle(textDir);
		degenR1=( r1Angle < TOL_EPS || fabs(r1Angle-M_PI) < TOL_EPS ) ;

		Point3D newUp=up;
		if(!degenR1)
		{
			r1Axis=textDir.crossProd(r1Axis);
			r1Axis.normalise();

			quat_rot(newUp,r1Axis,r1Angle);

		}
		//--

		//Compute R2 axis
		//--
		r2Axis=Point3D(0,-1,0);
		//In degenerate case, we don't do anything
		// otherwise we compute R2
		//rotate new up direction into y around x axis
		float angle = newUp.angle(Point3D(0,1,0));
		if(!degenR1 && (angle > sqrtf(std::numeric_limits<float>::epsilon()) &&
			fabs(angle - M_PI) > sqrtf(std::numeric_limits<float>::epsilon())) )
		{
			r2Axis= newUp.crossProd(Point3D(0,-1,0));
			r2Axis.normalise();
		}
		else
			r2Axis=up;

		//--

		//Compute R2'(P)
		//--
		float r2Angle=angle;
		degenR2 = r2Angle < TOL_EPS;
		if(!degenR2)
		{
			Point3f rotAx;
			rotAx.fx = r2Axis[0]; rotAx.fy = r2Axis[1]; rotAx.fz=r2Axis[2];
			quat_rot_array(&p[0], p.size(), &rotAx,r2Angle);
		}
		//--

		//Compute R1'(p)
		if(!degenR1)
		{
			Point3f rotAx;
			rotAx.fx = r1Axis[0]; rotAx.fy = r1Axis[1]; rotAx.fz=r1Axis[2];
			quat_rot_array(&p[0], p.size(), &rotAx,-r1Angle);

		}

		for(size_t ui=0;ui<p.size();ui++)
			p[ui]+=origin ; 

		b.setBounds(p);
	}
	else
		b.setInverseLimits();	
	
}

void DrawGLText::setAlignment(unsigned int newMode)
{
	ASSERT(newMode < DRAWTEXT_ALIGN_ENUM_END);
	alignMode=newMode;
}

void DrawGLText::recomputeParams(const vector<Point3D> &vecs, 
			const vector<float> &scalars, unsigned int mode)
{
	switch(mode)
	{
		case DRAW_TEXT_BIND_ORIGIN:
			ASSERT(vecs.size() ==1 && scalars.size() ==0);
			origin=vecs[0];
			break;
		default:
			ASSERT(false);
	}
}

DrawRectPrism::DrawRectPrism() : drawMode(DRAW_WIREFRAME), r(1.0f), g(1.0f), b(1.0f), a(1.0f), lineWidth(1.0f)
{
}

DrawRectPrism::~DrawRectPrism()
{
}

DrawableObj *DrawRectPrism::clone() const
{
	DrawRectPrism *dR= new DrawRectPrism(*this);
	return dR;
}

void DrawRectPrism::getBoundingBox(BoundCube &b) const
{
	b.setBounds(pMin[0],pMin[1],pMin[2],
			pMax[0],pMax[1],pMax[2]);
}

void DrawRectPrism::draw() const
{
	ASSERT(r <=1.0f && g<=1.0f && b <=1.0f && a <=1.0f);
	ASSERT(r >=0.0f && g>=0.0f && b >=0.0f && a >=0.0f);

	if(!active)
		return;
 
	switch(drawMode)
	{
		case DRAW_WIREFRAME:
		{
			glLineWidth(lineWidth);	
			drawBox(pMin,pMax,r,g,b,a);
			break;
		}
		case DRAW_FLAT:
		{
			glBegin(GL_QUADS);
				glColor4f(r,g,b,a);
			
				glNormal3f(0,0,-1);
				//Along the bottom
				glVertex3f(pMin[0],pMin[1],pMin[2]);
				glVertex3f(pMin[0],pMax[1],pMin[2]);
				glVertex3f(pMax[0],pMax[1],pMin[2]);
				glVertex3f(pMax[0],pMin[1],pMin[2]);
				//Up the side
				glNormal3f(1,0,0);
				glVertex3f(pMax[0],pMax[1],pMax[2]);
				glVertex3f(pMax[0],pMin[1],pMax[2]);
				glVertex3f(pMax[0],pMin[1],pMin[2]);
				glVertex3f(pMax[0],pMax[1],pMin[2]);
				//Over the top
				glNormal3f(0,0,1);
				glVertex3f(pMax[0],pMin[1],pMax[2]);
				glVertex3f(pMax[0],pMax[1],pMax[2]);
				glVertex3f(pMin[0],pMax[1],pMax[2]);
				glVertex3f(pMin[0],pMin[1],pMax[2]);

				//and back down
				glNormal3f(-1,0,0);
				glVertex3f(pMin[0],pMax[1],pMin[2]);
				glVertex3f(pMin[0],pMin[1],pMin[2]);
				glVertex3f(pMin[0],pMin[1],pMax[2]);
				glVertex3f(pMin[0],pMax[1],pMax[2]);

				//Now the other two sides
				glNormal3f(0,-1,0);
				glVertex3f(pMax[0],pMin[1],pMax[2]);
				glVertex3f(pMin[0],pMin[1],pMax[2]);
				glVertex3f(pMin[0],pMin[1],pMin[2]);
				glVertex3f(pMax[0],pMin[1],pMin[2]);
				
				glNormal3f(0,1,0);
				glVertex3f(pMax[0],pMax[1],pMax[2]);
				glVertex3f(pMax[0],pMax[1],pMin[2]);
				glVertex3f(pMin[0],pMax[1],pMin[2]);
				glVertex3f(pMin[0],pMax[1],pMax[2]);

			glEnd();

			break;

		}
		default:
			ASSERT(false);
	}		


}

void DrawRectPrism::setAxisAligned( const Point3D &p1, const Point3D &p2)
{
	for(unsigned int ui=0; ui<3; ui++)
	{
		pMin[ui]=std::min(p1[ui],p2[ui]);
		pMax[ui]=std::max(p1[ui],p2[ui]);
	}

}

void DrawRectPrism::setAxisAligned( const BoundCube &b)
{
	b.getBounds(pMin,pMax);
}

void DrawRectPrism::setColour(float rnew, float gnew, float bnew, float anew)
{
	r=rnew;
	g=gnew;
	b=bnew;
	a=anew;
}

void DrawRectPrism::setLineWidth(float newLineWidth)
{
	ASSERT(newLineWidth > 0.0f);
	lineWidth=newLineWidth;
}

void DrawRectPrism::recomputeParams(const vector<Point3D> &vecs, 
			const vector<float> &scalars, unsigned int mode)
{
	switch(mode)
	{
		case DRAW_RECT_BIND_TRANSLATE:
		{
			ASSERT(vecs.size() ==1);
			Point3D delta;
			delta = (pMax - pMin)*0.5;
			//Object has been translated
			pMin = vecs[0]-delta;
			pMax = vecs[0]+delta;
			break;
		}
		case DRAW_RECT_BIND_CORNER_MOVE:
		{
			ASSERT(vecs.size() ==1);
			//Delta has changed, but origin should stay the same
			Point3D mean, corner;
			mean  = (pMin + pMax)*0.5;

			//Prevent negative offset values, otherwise we can
			//get inside out boxes
			corner=vecs[0];
			for(unsigned int ui=0;ui<3;ui++)
				corner[ui]= fabs(corner[ui]);

			pMin = mean-corner;
			pMax = mean+corner;
			break;
		}
		default:
			ASSERT(false);
	}
}

DrawableOverlay::~DrawableOverlay()
{
}

DrawTexturedQuadOverlay::DrawTexturedQuadOverlay()  
:  textureId(-1),textureOK(false)
{
}

DrawTexturedQuadOverlay::~DrawTexturedQuadOverlay()
{
	texPool->closeTexture(textureId);
}

void DrawTexturedQuadOverlay::draw() const
{
	if(!textureOK)
		return;

	ASSERT(height == width);

	ASSERT(glIsTexture(textureId));
	
	//TODO: Is this redundant? might be already handled
	// by scene?
	glMatrixMode(GL_PROJECTION);	
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, winX, winY, 0);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,textureId);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); 
	
	// Draw overlay quad 
	glColor3f(1.0f,1.0f,1.0f);
	glBegin(GL_QUADS);
		glTexCoord2f(0.0f,0.0f);
		glVertex3f(position[0]-height/2.0,position[1]-height/2.0,0.0);
		glTexCoord2f(0.0f,1.0f);
		glVertex3f(position[0]-height/2.0,position[1]+height/2.0,0.0);
		glTexCoord2f(1.0f,1.0f);
		glVertex3f(position[0]+height/2.0,position[1]+height/2.0,0.0);
		glTexCoord2f(1.0f,0.0f);
		glVertex3f(position[0]+height/2.0,position[1]-height/2.0,0.0);
	glEnd();

	glDisable(GL_TEXTURE_2D);	
	/* draw stuff */

	glPopMatrix(); //Pop modelview matrix

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
}


bool DrawTexturedQuadOverlay::setTexture(const char *textureFile)
{
	ASSERT(texPool);	
	textureOK= texPool->openTexture(textureFile,textureId);
	return textureOK;
}


DrawProgressCircleOverlay::DrawProgressCircleOverlay()
{
	stepProgress=0;
	step=0;
	maxStep=0;

}

DrawProgressCircleOverlay::~DrawProgressCircleOverlay()
{
}

void DrawProgressCircleOverlay::reset()
{
	stepProgress=0;
	maxStep=0;
	totalFilters=0;
	curFilter=0;	
}

void DrawProgressCircleOverlay::draw( )const
{
	if(!maxStep)
		return;

	ASSERT(curFilter <=totalFilters);
	//TODO: Is this redundant? might be already handled
	// by scene?
	glMatrixMode(GL_PROJECTION);	
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, winX, winY, 0);
	
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	//size of gap to fit, in degrees
	const float FILTER_SPACING_ANGLE= 20.0f/(float)totalFilters;
	//Angular step when drawing wheel
	const float DEG_STEP= 2;
	
	//fraction of radius for inner section
	const float IN_RADIUS_FRACTION=0.85;
	//compute circle radius
	float radiusOut,radiusIn;
	radiusOut = std::min(height,width)/2.0f;
	radiusIn= radiusOut*IN_RADIUS_FRACTION; 



	//Allows for gap between each step, and one at start and end
	float thetaPerFilter = (360.0f - FILTER_SPACING_ANGLE*(totalFilters))/totalFilters;
		
	//Draw the complete filters 
	float curTheta=FILTER_SPACING_ANGLE/2.0f;	
	for(size_t ui=1;ui<curFilter;ui++)
	{
		drawSection(DEG_STEP,
			IN_RADIUS_FRACTION*radiusIn, 
			radiusOut,curTheta, 
			curTheta+thetaPerFilter,true);
		curTheta+=thetaPerFilter+FILTER_SPACING_ANGLE;
	}


	float visGrey=getHighContrastValue();

	//Draw the completed Steps
	float thetaPerStep =  thetaPerFilter/maxStep;
	for(size_t ui=1;ui<step;ui++)
	{
		drawSection(DEG_STEP,
			IN_RADIUS_FRACTION*radiusIn, 
			radiusOut,curTheta, 
			curTheta+thetaPerStep,true);
		curTheta+=thetaPerStep;

		if(ui < step-1)
		{
			//Draw a line to mark the step
			glColor4f(visGrey,0.0f,0.0f,1.0f);
			glBegin(GL_LINES);
				glVertex3f(radiusIn*sin(curTheta),radiusIn*cos(curTheta),0);
				glVertex3f(radiusOut*sin(curTheta),radiusOut*cos(curTheta),0);
			glEnd();
		}
	}
	//Draw the current step
	if(stepProgress == 100)
	{
		drawSection(DEG_STEP,
			IN_RADIUS_FRACTION*radiusIn, 
			radiusOut,curTheta, 
			curTheta+thetaPerStep,true);
	}
	else if (!stepProgress)
	{
		drawSection(DEG_STEP,
			IN_RADIUS_FRACTION*radiusIn, 
			radiusOut,curTheta, 
			curTheta+thetaPerStep,false);
	}
	else
	{
		//draw two segments, one with the current progress value in the complete style
		float interpFrac = (float)stepProgress/100.0f;
		drawSection(DEG_STEP,
			IN_RADIUS_FRACTION*radiusIn, 
			radiusOut,curTheta, 
			curTheta+thetaPerStep*interpFrac,true);
		// and one with the incomplete style
		drawSection(DEG_STEP,
			IN_RADIUS_FRACTION*radiusIn, 
			radiusOut,curTheta+thetaPerStep*interpFrac, 
			curTheta+thetaPerStep,false);

	}
	curTheta+=thetaPerStep;
	//Draw the remaining incomplete steps
	for(size_t ui=step+1; ui<=maxStep;ui++)
	{
		drawSection(DEG_STEP,
			IN_RADIUS_FRACTION*radiusIn, 
			radiusOut,curTheta, 
			curTheta+thetaPerStep,false);
		curTheta+=thetaPerStep;
	}

	curTheta+=FILTER_SPACING_ANGLE;

	//Draw the incomplete filters
	for(size_t ui=curFilter+1; ui<=totalFilters;ui++)
	{
		drawSection(DEG_STEP,
			IN_RADIUS_FRACTION*radiusIn, 
			radiusOut,curTheta, 
			curTheta+thetaPerFilter,false);
		curTheta+=thetaPerFilter+FILTER_SPACING_ANGLE;
	}
	
	glPopMatrix(); //Pop modelview matrix

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}

void DrawProgressCircleOverlay::drawSection(unsigned int degreeStep, 
	float rIn, float rOut,float startTheta, float stopTheta, bool complete) const
{
	//TODO: all calulations that call this should use radiians	
	float startThetaRad = startTheta*M_PI/180.0f;
	float endThetaRad = stopTheta*M_PI/180.0f;
	float degStepRad = degreeStep*M_PI/180.0f;

	unsigned int nSegments = (endThetaRad-startThetaRad)/degStepRad;

	if(!nSegments)
		return;

	float alphaBase,dt;
	getAnimationStat(alphaBase,dt);	
	if(alphaBase == 0.0f)
		return;


	float visGrey= getHighContrastValue();


	const float ALPHA_COMPLETE=0.5*alphaBase;
	const float ALPHA_INCOMPLETE=0.15*alphaBase;
	if(complete)	
		glColor4f(visGrey,visGrey,visGrey,ALPHA_COMPLETE);
	else
		glColor4f(visGrey,visGrey,visGrey,ALPHA_INCOMPLETE);

	//Draw arc
	glBegin(GL_TRIANGLE_STRIP);
	float thetaOne=startThetaRad;
	float thetaTwo=startThetaRad+degStepRad;
	glVertex2f(position[0] + rIn*cos(thetaOne),position[1] + rIn*sin(thetaOne));
	for(size_t ui=0;ui<nSegments;ui++)
	{
		thetaOne=startThetaRad + ui*degStepRad;;
		thetaTwo=startThetaRad + (ui+1)*degStepRad;
		
		glVertex2f(position[0] + rOut*cos(thetaOne),position[1] + rOut*sin(thetaOne));
		glVertex2f(position[0] + rIn*cos(thetaTwo),position[1] + rIn*sin(thetaTwo));


	}
	glVertex2f(position[0] + rOut*cos(thetaTwo),position[1] + rOut*sin(thetaTwo));
	glEnd();
}


DrawAnimatedOverlay::DrawAnimatedOverlay()
{
	fadeIn=0.0f;
	delayBeforeShow=0.0f;
	textureOK=false;
	resetTime();
}

DrawAnimatedOverlay::~DrawAnimatedOverlay()
{
}

void DrawAnimatedOverlay::resetTime()
{
	gettimeofday(&animStartTime,NULL);
}

bool DrawAnimatedOverlay::setTexture(const vector<string> &texFiles,
			float replayTime)
{
	repeatInterval=replayTime;

	textureOK=texPool->openTexture3D(texFiles, textureId);
	return textureOK;
}

void DrawAnimatedOverlay::getAnimationStat(float &alpha , float &animDeltaTime) const
{
		
	timeval t;
	gettimeofday(&t,NULL);
	animDeltaTime=(float)(t.tv_sec - animStartTime.tv_sec) +
		(t.tv_usec-animStartTime.tv_usec)/1.0e6;


	//Skip if we wish to show later
	if(animDeltaTime < delayBeforeShow)
	{
		alpha= 0;
		return;
	}

	animDeltaTime-=delayBeforeShow;

	if(fadeIn > 0.0f && (fadeIn > animDeltaTime) ) 
	{
		alpha= (animDeltaTime )/(fadeIn) ;
	}
	else
		alpha= 1.0f;
	
}

void DrawAnimatedOverlay::draw() const
{
	if(!textureOK)
		return;

	float alphaVal, animDeltaTime;
	getAnimationStat(alphaVal,animDeltaTime);

	if(alphaVal== 0.0f)
		return;
	float texCoordZ;
	texCoordZ=fmod(animDeltaTime,repeatInterval);
	texCoordZ/=repeatInterval;

	ASSERT(glIsTexture(textureId));
	
	glMatrixMode(GL_PROJECTION);	
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, winX, winY, 0);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glEnable(GL_TEXTURE_3D);
	glBindTexture(GL_TEXTURE_3D,textureId);

	//TODO: Find correct blending mode. Default is good, but may change...	
//	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND); 
//	glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);

	// Draw overlay quad 
	ASSERT(width == height); // width/height should be the same
	glColor4f(1.0f,1.0f,1.0f,alphaVal);

	glBegin(GL_QUADS);
		glTexCoord3f(0.0f,0.0f,texCoordZ);
		glVertex3f(position[0]-width/2.0,position[1]-width/2.0,0.0);
		glTexCoord3f(0.0f,1.0f,texCoordZ);
		glVertex3f(position[0]-width/2.0,position[1]+width/2.0,0.0);
		glTexCoord3f(1.0f,1.0f,texCoordZ);
		glVertex3f(position[0]+width/2.0,position[1]+width/2.0,0.0);
		glTexCoord3f(1.0f,0.0f,texCoordZ);
		glVertex3f(position[0]+width/2.0,position[1]-width/2.0,0.0);
	glEnd();

	glDisable(GL_TEXTURE_3D);	

	glPopMatrix(); //Pop modelview matrix

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
}



DrawColourBarOverlay::DrawColourBarOverlay() 
{
	a=1.0;
	string f;
	f=getDefaultFontFile();
	font = new FTGLPolygonFont(f.c_str());
};

DrawColourBarOverlay::DrawColourBarOverlay(const DrawColourBarOverlay &oth) 
{
	string f;
	f=getDefaultFontFile();
	
	font = new FTGLPolygonFont(f.c_str());
	a=oth.a;

	rgb=oth.rgb;
	min=oth.min;
	max=oth.max;
	
	height=oth.height;
	width=oth.width;
	
	position[0]=oth.position[0];
	position[1]=oth.position[1];
};

void DrawColourBarOverlay::draw() const
{
	//Draw quads
	float elemHeight;
	//80% of bar width is for the actual colour bar itself.
	float barWidth=0.8*width;
	elemHeight=height/(float)rgb.size();
	glBegin(GL_QUADS);
	for(unsigned int ui=0;ui<rgb.size();ui++)
	{
		//Set the quad colour for bar element
		glColor4f(rgb[rgb.size()-(ui+1)].v[0],
				rgb[rgb.size()-(ui+1)].v[1],
				rgb[rgb.size()-(ui+1)].v[2],a);

		//draw this quad (bar element)
		glVertex2f(position[0],position[1]+(float)ui*elemHeight);
		glVertex2f(position[0],position[1]+(float)(ui+1)*elemHeight);
		glVertex2f(position[0]+barWidth,position[1]+(float)(ui+1)*elemHeight);
		glVertex2f(position[0]+barWidth,position[1]+(float)(ui)*elemHeight);
	}

	glEnd();


	//-------

	float textGrey=getHighContrastValue();
	//Draw ticks on colour bar
	glBegin(GL_LINES);
		glColor4f(textGrey,textGrey,textGrey,a);
		//Top tick
		glVertex2f(position[0],position[1]);
		glVertex2f(position[0]+width,position[1]);
		//Bottom tick
		glVertex2f(position[0],position[1]+height);
		glVertex2f(position[0]+width,position[1]+height);
	glEnd();




	if(!font || font->Error())
	{
#ifdef DEBUG
		std::cerr << "Ah bugger. No font!" << std::endl;
#endif
		return;
	}



	//FTGL units are a pain; The devs could not decide
	//whether to implement them in opengl coords or real coords
	//so they did neither, and implemented them in "points".
	//here we assume that we can transform 1 ftgl unit
	//to 1 opengl unit by inversion 
	const float FTGL_DEFAULT_UNIT_SCALE=1.0/72.0;

	font->FaceSize(3);
	glDisable(GL_CULL_FACE);
	glPushMatrix();
	glTranslatef(position[0]+width,position[1],0);
	string s;
	stream_cast(s,max);
	//Note negative sign to flip from y-down screen (opengl) to text dir
	//(y up)
	glScaled(FTGL_DEFAULT_UNIT_SCALE,
			-FTGL_DEFAULT_UNIT_SCALE,FTGL_DEFAULT_UNIT_SCALE);
	font->Render(s.c_str());
	glPopMatrix();

	glPushMatrix();
	glTranslatef(position[0]+width,position[1]+height,0);
	stream_cast(s,min);
	//Note negative sign to flip from y-down screen (opengl) to text dir
	//(y up)
	glScaled(FTGL_DEFAULT_UNIT_SCALE,
			-FTGL_DEFAULT_UNIT_SCALE,FTGL_DEFAULT_UNIT_SCALE);
	font->Render(s.c_str());
	glPopMatrix();
	glEnable(GL_CULL_FACE);

}

DrawableObj *DrawColourBarOverlay::clone() const
{
	DrawColourBarOverlay *newBar = new DrawColourBarOverlay(*this);
	return newBar;
}

void DrawColourBarOverlay::setColourVec(const vector<float> &r,
					const vector<float> &g,
					const vector<float> &b)
{
	ASSERT(r.size() == g.size());
	ASSERT(g.size() == b.size());
	rgb.resize(r.size());
	for(unsigned int ui=0;ui<r.size();ui++)
	{
		rgb[ui].v[0]=r[ui];
		rgb[ui].v[1]=g[ui];
		rgb[ui].v[2]=b[ui];
	}


}

DrawPointLegendOverlay::DrawPointLegendOverlay() : enabled(true)
{
	a=1.0f;

	std::string tmpStr =getDefaultFontFile();
	font = new FTGLPolygonFont(tmpStr.c_str());
}

DrawPointLegendOverlay::~DrawPointLegendOverlay()
{
}

DrawableObj *DrawPointLegendOverlay::clone() const
{
	DrawPointLegendOverlay *dp = new DrawPointLegendOverlay(*this);

	return dp;
}

DrawPointLegendOverlay::DrawPointLegendOverlay(const DrawPointLegendOverlay &oth)
{
	string f;
	f=getDefaultFontFile();
	
	font = new FTGLPolygonFont(f.c_str());
	a=oth.a;
	legendItems = oth.legendItems;
	enabled = oth.enabled;

	height=oth.height;
	width=oth.width;
	
	position[0]=oth.position[0];
	position[1]=oth.position[1];
}

void DrawPointLegendOverlay::draw() const
{

	if(!enabled || legendItems.empty())
		return;

	ASSERT(winX >0 && winY > 0);
	float curX = position[0];
	float curY = position[1];

	float delta = std::max(std::min(1.0f/legendItems.size(),0.02f),0.05f);
	float size = delta*0.9f; 

	float maxTextWidth=0;


	if(font)	
		font->FaceSize(1);
	for(unsigned int ui=0; ui<legendItems.size();ui++)
	{
		for(;ui<legendItems.size();ui++)
		{
			Draw2DCircle dCirc;

			//Draw circle
			//--
			dCirc.setCentre(curX+size/2.0f,curY+size/2.0f);
			dCirc.setRadius(size/2.0f);

			const RGBFloat *f;
			f = &legendItems[ui].second;
			dCirc.setColour(f->v[0],f->v[1],f->v[2]);
			dCirc.draw();


			//--

			//Draw text, if possible
			if( font && !font->Error())
			{
				float textGrey=getHighContrastValue();
				glColor3f(textGrey,textGrey,textGrey);
				float fminX,fminY,fminZ;
				float fmaxX,fmaxY,fmaxZ;
				font->BBox(legendItems[ui].first.c_str(),fminX,
						fminY,fminZ,fmaxX,fmaxY,fmaxZ);
				glPushMatrix();
				glTranslatef(curX+1.5*size,curY+0.85*size,0.0f);
				glScalef(size,-size,0);
				font->Render(legendItems[ui].first.c_str());
				glPopMatrix();
				maxTextWidth=std::max(fmaxX-fminX,maxTextWidth);
			}
			
			
			curY+=delta;
		}

		curX+=maxTextWidth + size;
		curY=position[1] + 0.5*delta;
	}
}

void DrawPointLegendOverlay::addItem(const std::string &s, float r, float g, float b)
{
	RGBFloat rgb;
	rgb.v[0]=r;
	rgb.v[1]= g;
	rgb.v[2]= b;
	legendItems.push_back(make_pair(s,rgb));
}


DrawField3D::DrawField3D() : ptsCacheOK(false), alphaVal(0.2f), pointSize(1.0f), drawBoundBox(true),
	boxColourR(1.0f), boxColourG(1.0f), boxColourB(1.0f), boxColourA(1.0f),
	volumeGrid(false), volumeRenderMode(0), field(0) 
{
}

DrawField3D::~DrawField3D()
{
	if(field)
		delete field;
}


void DrawField3D::getBoundingBox(BoundCube &b) const
{
	ASSERT(field)
	b.setBounds(field->getMinBounds(),field->getMaxBounds());
}


void DrawField3D::setField(const Voxels<float> *newField)
{
	field=newField;
}

void DrawField3D::setRenderMode(unsigned int mode)
{
	volumeRenderMode=mode;
}

void DrawField3D::setColourMinMax()
{
	colourMapBound[0]=field->min();
	colourMapBound[1]=field->max();

	ASSERT(colourMapBound[0] <=colourMapBound[1]);
}

			
void DrawField3D::draw() const
{
	if(alphaVal < sqrtf(std::numeric_limits<float>::epsilon()))
		return;

	ASSERT(field);

	//Depend upon the render mode
	switch(volumeRenderMode)
	{
		case VOLUME_POINTS:
		{
			size_t fieldSizeX,fieldSizeY,fieldSizeZ;
			Point3D p;

			field->getSize(fieldSizeX,fieldSizeY, fieldSizeZ);

			Point3D delta;
			delta = field->getPitch();
			delta*=0.5;
			if(!ptsCacheOK)
			{
				ptsCache.clear();
				for(unsigned int uiX=0; uiX<fieldSizeX; uiX++)
				{
					for(unsigned int uiY=0; uiY<fieldSizeY; uiY++)
					{
						for(unsigned int uiZ=0; uiZ<fieldSizeZ; uiZ++)
						{
							float v;
							v=field->getData(uiX,uiY,uiZ);
							if(v > std::numeric_limits<float>::epsilon())
							{
								RGBThis rgb;
								//Set colour and point loc
								colourMapWrap(colourMapID,rgb.v,
										field->getData(uiX,uiY,uiZ), 
										colourMapBound[0],colourMapBound[1],false);
								
								ptsCache.push_back(make_pair(field->getPoint(uiX,uiY,uiZ)+delta,rgb));
							}
						}
					}
				}
					

				ptsCacheOK=true;
			}

			if(alphaVal < 1.0f && useAlphaBlend)
			{
				//We need to generate some points, then sort them by distance
				//from eye (back to front), otherwise they will not blend properly
				std::vector<std::pair<float,unsigned int >  > eyeDists;

				Point3D camOrigin = curCamera->getOrigin();

				eyeDists.resize(ptsCache.size());
				
				//Set up an original index for the eye distances
				#pragma omp parallel for
				for(unsigned int ui=0;ui<ptsCache.size();ui++)
				{
					eyeDists[ui].first=ptsCache[ui].first.sqrDist(camOrigin);
					eyeDists[ui].second=ui;
				}

				ComparePairFirstReverse cmp;
				std::sort(eyeDists.begin(),eyeDists.end(),cmp);	

				//render each element in the field as a point
				//the colour of the point is determined by its scalar value
				glDepthMask(GL_FALSE);
				glPointSize(pointSize);
				glBegin(GL_POINTS);
				for(unsigned int ui=0;ui<ptsCache.size();ui++)
				{
					unsigned int idx;
					idx=eyeDists[ui].second;
					//Tell openGL about it
					glColor4f(((float)(ptsCache[idx].second.v[0]))/255.0f, 
							((float)(ptsCache[idx].second.v[1]))/255.0f,
							((float)(ptsCache[idx].second.v[2]))/255.0f, 
							alphaVal);
					glVertex3fv(ptsCache[idx].first.getValueArr());
				}
				glEnd();
				glDepthMask(GL_TRUE);
			}
			else
			{
				glPointSize(pointSize);
				glBegin(GL_POINTS);
				for(unsigned int ui=0;ui<ptsCache.size();ui++)
				{
					//Tell openGL about it
					glColor4f(((float)(ptsCache[ui].second.v[0]))/255.0f, 
							((float)(ptsCache[ui].second.v[1]))/255.0f,
							((float)(ptsCache[ui].second.v[2]))/255.0f, 
							1.0f);
					glVertex3fv(ptsCache[ui].first.getValueArr());
				}
				glEnd();
			}
			break;
		}

		default:
			//Not implemented
			ASSERT(false); 
	}

	//Draw the bounding box as required
	if(drawBoundBox)
	{
		float alphaUse;

		if(useAlphaBlend)
			alphaUse=boxColourA;
		else
			alphaUse=1.0f;
		
		drawBox(field->getMinBounds(),field->getMaxBounds(),
			boxColourR, boxColourG,boxColourB,alphaUse);
	}
}

void DrawField3D::setAlpha(float newAlpha)
{
	alphaVal=newAlpha;
}

void DrawField3D::setPointSize(float size)
{
	pointSize=size;
}

void DrawField3D::setMapColours(unsigned int mapID)
{
	ASSERT(mapID < NUM_COLOURMAPS);
	colourMapID= mapID;
}

void DrawField3D::setBoxColours(float rNew, float gNew, float bNew, float aNew)
{
	boxColourR = rNew;
	boxColourG = gNew;
	boxColourB = bNew;
	boxColourA = aNew;

}


DrawIsoSurface::DrawIsoSurface() : cacheOK(false),  drawMode(DRAW_SMOOTH),
	threshold(0.5f), r(0.5f), g(0.5f), b(0.5f), a(0.5f) 
{
}

DrawIsoSurface::~DrawIsoSurface()
{
	if(voxels)
		delete voxels;
}


bool DrawIsoSurface::needsDepthSorting()  const
{
	return a< 1 && a > std::numeric_limits<float>::epsilon();
}

void DrawIsoSurface::swapVoxels(Voxels<float> *f)
{
	std::swap(f,voxels);
	cacheOK=false;
	mesh.clear();
}


void DrawIsoSurface::updateMesh() const
{

	mesh.clear();
	marchingCubes(*voxels, threshold,mesh);

	cacheOK=true;

}

void DrawIsoSurface::getBoundingBox(BoundCube &b) const
{
	if(voxels)
	{
		b.setBounds(voxels->getMinBounds(),
				voxels->getMaxBounds());
	}
	else
		b.setInverseLimits();
}


void DrawIsoSurface::draw() const
{
	if(a< sqrtf(std::numeric_limits<float>::epsilon()))
		return;

	if(!cacheOK)
	{
		//Hmm, we don't have a cached copy of the isosurface mesh.
		//we will need to compute one, it would seem.
		updateMesh();
	}


	//This could be optimised by using triangle strips
	//rather than direct triangles.
	if(a < 1.0f && useAlphaBlend )
	{
		//We need to sort them by distance
		//from eye (back to front), otherwise they will not blend properly
		std::vector<std::pair<float,unsigned int >  > eyeDists;

		Point3D camOrigin = curCamera->getOrigin();
		eyeDists.resize(mesh.size());
		
		//Set up an original index for the eye distances
		#pragma omp parallel for shared(camOrigin)
		for(unsigned int ui=0;ui<mesh.size();ui++)
		{
			Point3D centroid;
			mesh[ui].getCentroid(centroid);

			eyeDists[ui].first=centroid.sqrDist(camOrigin);
			eyeDists[ui].second=ui;
		}

		ComparePairFirstReverse cmp;
		std::sort(eyeDists.begin(),eyeDists.end(),cmp);	
					

		glDepthMask(GL_FALSE);
		glColor4f(r,g,b,a);
		glPushAttrib(GL_CULL_FACE);
		glDisable(GL_CULL_FACE);

		glBegin(GL_TRIANGLES);	
		for(unsigned int ui=0;ui<mesh.size();ui++)
		{
			unsigned int idx;
			idx=eyeDists[ui].second;
			glNormal3fv(mesh[idx].normal[0].getValueArr());
			glVertex3fv(mesh[idx].p[0].getValueArr());
			glNormal3fv(mesh[idx].normal[1].getValueArr());
			glVertex3fv(mesh[idx].p[1].getValueArr()),
			glNormal3fv(mesh[idx].normal[2].getValueArr());
			glVertex3fv(mesh[idx].p[2].getValueArr());
		}
		glEnd();


		glPopAttrib();
		glDepthMask(GL_TRUE);

	}
	else
	{
		glColor4f(r,g,b,a);
		glPushAttrib(GL_CULL_FACE);
		glDisable(GL_CULL_FACE);	
		glBegin(GL_TRIANGLES);	
		for(unsigned int ui=0;ui<mesh.size();ui++)
		{
			glNormal3fv(mesh[ui].normal[0].getValueArr());
			glVertex3fv(mesh[ui].p[0].getValueArr());
			glNormal3fv(mesh[ui].normal[1].getValueArr());
			glVertex3fv(mesh[ui].p[1].getValueArr()),
			glNormal3fv(mesh[ui].normal[2].getValueArr());
			glVertex3fv(mesh[ui].p[2].getValueArr());
		}
		glEnd();
		glPopAttrib();
	}
}


////////////////////////////////  OPENVDB  /////////////////////////////////////////////

LukasDrawIsoSurface::LukasDrawIsoSurface() : cacheOK(false),
	 r(0.5f), g(0.5f), b(0.5f), a(1.0f), isovalue(0.07)
{

}

LukasDrawIsoSurface::~LukasDrawIsoSurface()
{

}

unsigned int LukasDrawIsoSurface::getType() const
{
	return DRAW_TYPE_LUKAS_ISOSURFACE; 	
}

void LukasDrawIsoSurface::getBoundingBox(BoundCube &b) const
{

	//obtain the bounding box from the openvdb grid
	openvdb::CoordBBox box;
	box=grid->evalActiveVoxelBoundingBox();
	openvdb::Coord cStart,cEnd;
	cStart=box.getStart();
	cEnd=box.getEnd();

	//set the bounds
	for(unsigned int ui=0;ui<3;ui++)
	{
		b.setBound(ui,0,cStart[ui]);
		b.setBound(ui,1,cEnd[ui]);
	}

}

void LukasDrawIsoSurface::updateMesh() const
{

	try
	{
		openvdb::tools::volumeToMesh<openvdb::FloatGrid>(*grid, points, triangles, quads, isovalue);	
		cacheOK=true;
	}
	catch(const std::exception &e)
	{
		ASSERT(false);
		cerr << "Exception! :" << e.what() << endl;
	}

}

void LukasDrawIsoSurface::draw() const
{

	cerr << "Starting lukas draw :" << endl;

	if(!cacheOK)
	{
		cerr << "Recalculation of the mesh!" << endl;
		updateMesh();
	}

	// checking the mesh for nonfinite coordinates like -nan and inf

	int non_finites_counter = 0;
	const unsigned int xyzs = 3;
	for (unsigned int i=0;i<points.size();i++)
	{
		for (unsigned int j=0;j<xyzs;j++)
		{
		    if (std::isfinite(points[i][j]) == false)
			{	
				non_finites_counter += 1;    
			}
		}
	}
	//std::cout << "points size" << " = " << points.size() << std::endl;
	//std::cout << "number_of_non_finites" << " = " << non_finites_counter << std::endl;

	// how are the -nans introduced if there is no -nan existing in the grid?! 
	// setting only the nan to zero will of course result in large triangles crossing the scene
	// setting all 3 coordinates to zero is also shit because triangles containing the point are also big
	// how to overcome this without discarding them, which would end up in corrupt faces
	// this behaviour gets checked in the vdb test suite

	for(unsigned int ui=0;ui<points.size();ui++)
	{
		for(unsigned int uj=0;uj<xyzs;uj++)
		{
			if (std::isfinite(points[ui][uj]) == false)
			{
				for(unsigned int uk=0;uk<xyzs;uk++)
				{
					points[ui][uk] = 0.0;
				}
			}
		}
	}

	//std::cout << "points size" << " = " << points.size() << std::endl;
	//std::cout << "triangles size" << " = " << triangles.size() << std::endl;
	//std::cout << " active voxel count subgrid div" << " = " << grid->activeVoxelCount() << std::endl;

	// create a triangular mesh
	unsigned int number_of_splitted_triangles = 2*quads.size();
	std::vector<std::vector<unsigned int> > triangles_from_splitted_quads(number_of_splitted_triangles, std::vector<unsigned int>(xyzs));

	triangles_from_splitted_quads = VDB::splitQuadsToTriangles(points, quads);

	std::vector<std::vector<unsigned int> > triangles_combined;
	triangles_combined = VDB::concatenateTriangleVectors(triangles, triangles_from_splitted_quads);

	// initialize triangle normals vector
	int vertices_per_triangle = 3;

	std::cout << "number_of_triangles" << " = " << triangles_combined.size() << std::endl;

	std::vector<std::vector<float> > triangle_normals(triangles_combined.size(), std::vector<float>(vertices_per_triangle));

	// calculate triangle normals
	triangle_normals = VDB::computeTriangleNormalsVDB(points, triangles_combined);

	unsigned int non_finite_tris_counter = 0;
	for (unsigned int i=0;i<triangle_normals.size();i++)
	{
		for (unsigned int j=0;j<xyzs;j++)
		{
		    if (std::isfinite(triangle_normals[i][j]) == false)
			{	
				non_finite_tris_counter += 1; 
				triangle_normals[i][j] = 0;   
			}
		}
	}	

	std::cout << "nans in triangle normals" << " = " << non_finite_tris_counter << std::endl;

	// initialize vertex normals vector
	std::vector<std::vector<float> > vertex_normals(points.size(),std::vector<float>(xyzs));
	// calculate vertex normals 
	vertex_normals = VDB::computeVertexNormals(triangles_combined, points, triangle_normals);

	std::cout << "vertex normals size" << " = " << vertex_normals.size() << std::endl;
	
	// check for corrupt vertex normals and if so set them to zero

	unsigned int non_finite_verts_counter = 0;
	for (unsigned int i=0;i<vertex_normals.size();i++)
	{
		for (unsigned int j=0;j<xyzs;j++)
		{
		    if (std::isfinite(vertex_normals[i][j]) == false)
			{	
				non_finite_verts_counter += 1;    
				vertex_normals[i][j] = 0;
			}
		}
	}

	//std::cout << "nans in vertex normals" << " = " << non_finite_tris_counter << std::endl;

	// visual comparisons with Marching Cubes
	bool flat_shading = true;

	if (flat_shading == true)
	{

		glColor4f(r,g,b,a);
		glPushAttrib(GL_CULL_FACE);
		glDisable(GL_CULL_FACE);
	
		glBegin(GL_TRIANGLES);	
		for(unsigned int ui=0;ui<triangles_combined.size();ui++)
		{
			openvdb::Vec3s v1 = points[triangles_combined[ui][0]];
			openvdb::Vec3s v2 = points[triangles_combined[ui][1]];
			openvdb::Vec3s v3 = points[triangles_combined[ui][2]];

			// conversion guessed but from here https://www.opengl.org/wiki/Common_Mistakes
			GLfloat vertex1[] = {v1.x(),v1.y(),v1.z()};
			GLfloat vertex2[] = {v2.x(),v2.y(),v2.z()};
			GLfloat vertex3[] = {v3.x(),v3.y(),v3.z()};

			glVertex3fv(vertex1);
			glVertex3fv(vertex2);
			glVertex3fv(vertex3);
			glNormal3f(triangle_normals[ui][0], triangle_normals[ui][1], triangle_normals[ui][2]);
		}

		glEnd();
		glPopAttrib();
	}

	else
	{
		glColor4f(r,g,b,a);
		glPushAttrib(GL_CULL_FACE);
		glDisable(GL_CULL_FACE);
	
		glBegin(GL_TRIANGLES);	
		for(unsigned int ui=0;ui<triangles_combined.size();ui++)
		{
			openvdb::Vec3s v1 = points[triangles_combined[ui][0]];
			openvdb::Vec3s v2 = points[triangles_combined[ui][1]];
			openvdb::Vec3s v3 = points[triangles_combined[ui][2]];

			// conversion guessed but from here https://www.opengl.org/wiki/Common_Mistakes
			GLfloat vertex1[] = {v1.x(),v1.y(),v1.z()};
			GLfloat vertex2[] = {v2.x(),v2.y(),v2.z()};
			GLfloat vertex3[] = {v3.x(),v3.y(),v3.z()};
		
			GLfloat vertex_normal1[] = {vertex_normals[triangles_combined[ui][0]][0], vertex_normals[triangles_combined[ui][0]][1], vertex_normals[triangles_combined[ui][0]][2]};
			GLfloat vertex_normal2[] = {vertex_normals[triangles_combined[ui][1]][0], vertex_normals[triangles_combined[ui][1]][1], vertex_normals[triangles_combined[ui][1]][2]};
			GLfloat vertex_normal3[] = {vertex_normals[triangles_combined[ui][2]][0], vertex_normals[triangles_combined[ui][2]][1], vertex_normals[triangles_combined[ui][2]][2]};

			glNormal3fv(vertex_normal1);
			glVertex3fv(vertex1);
			glNormal3fv(vertex_normal2);
			glVertex3fv(vertex2);
			glNormal3fv(vertex_normal3);
			glVertex3fv(vertex3);
		}

		glEnd();
		glPopAttrib();
	}

	// export section for evaluation puposes
	/*
	std::vector<float> triangle_areas(triangles_combined.size());
	triangle_areas = ComputeTriangleAreas(points, triangles_combined);
	VDB::exportTriangleAreas(triangle_areas);
	
	VDB::exportTriangleMeshAsObj(points, triangles_combined);
	VDB::exportVDBMeshAsObj(points, triangles, quads);

	*/
}

DrawAxis::DrawAxis()
{
}

DrawAxis::~DrawAxis()
{
}


DrawableObj* DrawAxis::clone() const
{
	DrawAxis *d = new DrawAxis(*this);
	return d;
}

void DrawAxis::setStyle(unsigned int s)
{
	style=s;
}

void DrawAxis::setSize(float s)
{
	size=s;
}

void DrawAxis::setPosition(const Point3D &p)
{
	position=p;
}

void DrawAxis::draw() const

{
	float halfSize=size/2.0f;
	glPushAttrib(GL_LIGHTING_BIT);
	glDisable(GL_LIGHTING);
	
	glLineWidth(1.0f);
	glBegin(GL_LINES);
	//Draw lines
	glColor3f(1.0f,0.0f,0.0f);
	glVertex3f(position[0]-halfSize,
	           position[1],position[2]);
	glVertex3f(position[0]+halfSize,
	           position[1],position[2]);

	glColor3f(0.0f,1.0f,0.0f);
	glVertex3f(position[0],
	           position[1]-halfSize,position[2]);
	glVertex3f(position[0],
	           position[1]+halfSize,position[2]);

	glColor3f(0.0f,0.0f,1.0f);
	glVertex3f(position[0],
	           position[1],position[2]-halfSize);
	glVertex3f(position[0],
	           position[1],position[2]+halfSize);
	glEnd();
	glPopAttrib();



	float numSections=20.0f;
	float twoPi = 2.0f *M_PI;
	float radius = 0.1*halfSize;
	//Draw axis cones

	//+x
	glPushMatrix();
	glTranslatef(position[0]+halfSize,position[1],position[2]);

	glColor3f(1.0f,0.0f,0.0f);
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(radius,0,0);
	glNormal3f(1,0,0);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glNormal3f(0,cos(i*twoPi/numSections),sin(i*twoPi/numSections));
		glVertex3f(0,radius * cos(i *  twoPi / numSections),
		           radius* sin(i * twoPi / numSections));
	}
	glEnd();
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0,0,0);
	glNormal3f(-1,0,0);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glVertex3f(0,-radius * cos(i *  twoPi / numSections),
		           radius* sin(i * twoPi / numSections));
	}
	glEnd();
	glPopMatrix();

	//+y
	glColor3f(0.0f,1.0f,0.0f);
	glPushMatrix();
	glTranslatef(position[0],position[1]+halfSize,position[2]);
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0,radius,0);
	glNormal3f(0,1,0);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glNormal3f(sin(i*twoPi/numSections),0,cos(i*twoPi/numSections));
		glVertex3f(radius * sin(i *  twoPi / numSections),0,
		           radius* cos(i * twoPi / numSections));
	}
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0,0,0);
	glNormal3f(0,-1,0);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glVertex3f(radius * cos(i *  twoPi / numSections),0,
		           radius* sin(i * twoPi / numSections));
	}
	glEnd();

	glPopMatrix();



	//+z
	glColor3f(0.0f,0.0f,1.0f);
	glPushMatrix();
	glTranslatef(position[0],position[1],position[2]+halfSize);
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0,0,radius);
	glNormal3f(0,0,1);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glNormal3f(cos(i*twoPi/numSections),sin(i*twoPi/numSections),0);
		glVertex3f(radius * cos(i *  twoPi / numSections),
		           radius* sin(i * twoPi / numSections),0);
	}
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0,0,0);
	glNormal3f(0,0,-1);
	for (unsigned int i = 0; i<=numSections; i++)
	{
		glVertex3f(-radius * cos(i *  twoPi / numSections),
		           radius* sin(i * twoPi / numSections),0);
	}
	glEnd();
	glPopMatrix();
}

void DrawAxis::getBoundingBox(BoundCube &b) const
{
	b.setInvalid();
}

Draw2DCircle::Draw2DCircle()
{
	angularStep = 2.0f*M_PI/180.0f;
	filled=true;
}

void Draw2DCircle::draw() const
{

	float nSteps = 2.0* M_PI/angularStep;
	WARN(nSteps > 1,"Draw2D Circle, too few steps");
	glColor4f(r,g,b,1.0f);

	if(filled)
	{
		glBegin(GL_TRIANGLE_FAN);
			//Central vertex
			glVertex2fv(centre);

			//vertices from [0,2PI)
			for(unsigned int ui=0;ui<nSteps;ui++)
			{
				float fx,fy,theta;
				theta = angularStep*ui;	
				fx = centre[0]+cos(-theta)*radius;
				fy = centre[1]+sin(-theta)*radius;

				glVertex2f(fx,fy);
			}

			//2PI vertex
			glVertex2f(centre[0]+radius,centre[1]);
		glEnd();
	}
	else
	{
		glBegin(GL_LINE_LOOP);
		//Central vertex
		for(unsigned int ui=0;ui<nSteps;ui++)
		{
			float fx,fy,theta;
			theta = angularStep*ui;	
			fx = centre[0]+cos(theta)*radius;
			fy = centre[1]+sin(theta)*radius;

			glVertex2f(fx,fy);
		}
		glEnd();
	}
}

void Draw2DCircle::getBoundingBox(BoundCube &b) const
{

	b.setBounds(centre[0]-radius, centre[1]-radius,
			centre[0]+radius, centre[1]+radius,
			0,0);
}

unsigned int Draw2DCircle::getType() const
{
	return DRAW_TYPE_2D_CIRCLE; 	
}


DrawableObj *Draw2DCircle::clone() const
{
	Draw2DCircle *p = new Draw2DCircle;
	*p = *this;	

	return p;	
}

