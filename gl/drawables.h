/*
 *	drawables.h - Opengl drawable objects header
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

#ifndef DRAWABLES_H
#define DRAWABLES_H

//MacOS is "special" and puts it elsewhere
#ifdef __APPLE__ 
	#include <OpenGL/glu.h>
#else
	#include <GL/glu.h>
#endif

#include <sys/time.h>

#include "textures.h"
#include "cameras.h"
#include "isoSurface.h"
#include "../backend/filters/openvdb_includes.h"

template<class T>
class Voxels;

//TODO: Work out if there is any way of obtaining the maximum 
//number of items that can be drawn in an opengl context
//For now Max it out at 10 million (~120MB of vertex data)
const size_t MAX_NUM_DRAWABLE_POINTS=10;


//OK, the new FTGL is fucked up. It actually uses defines from
//freetype  as arguments to #includes. Weird. So this sequence is important
#include <ft2build.h>
#include <FTGL/ftgl.h>

enum
{
	FTGL_BITMAP=0,
	FTGL_PIXMAP,
	FTGL_OUTLINE,
	FTGL_POLYGON,
	FTGL_EXTRUDE,
	FTGL_TEXTURE
};

//!Text aligment modes for DrawGLText
enum
{
	DRAWTEXT_ALIGN_LEFT,
	DRAWTEXT_ALIGN_CENTRE,
	DRAWTEXT_ALIGN_RIGHT,
	DRAWTEXT_ALIGN_ENUM_END
};


//!Primitve drawing mode. (wireframe/solid)
enum
{
	DRAW_WIREFRAME,
	DRAW_FLAT,
	DRAW_SMOOTH,
	DRAW_END_ENUM //Not a mode, just a marker to catch end-of-enum
};

//!Axis styles
enum
{
	AXIS_IN_SPACE
};

//!Drawable types
enum
{
	DRAW_TYPE_POINT,
	DRAW_TYPE_MANYPOINT,
	DRAW_TYPE_VECTOR,
	DRAW_TYPE_TRIANGLE,
	DRAW_TYPE_QUAD,
	DRAW_TYPE_SPHERE,
	DRAW_TYPE_CYLINDER,
	DRAW_TYPE_DISPLAYLIST,
	DRAW_TYPE_GLTEXT,
	DRAW_TYPE_2D_CIRCLE,
	DRAW_TYPE_RECTPRISM,
	DRAW_TYPE_COLOURBAR,
	DRAW_TYPE_TEXTUREDOVERLAY,
	DRAW_TYPE_ANIMATEDOVERLAY,
	DRAW_TYPE_FIELD3D,
	DRAW_TYPE_ISOSURFACE,
	DRAW_TYPE_LUKAS_ISOSURFACE,
	DRAW_TYPE_AXIS,
	DRAW_TYPE_LEGENDOVERLAY,
	DRAW_TYPE_PROGRESSCIRCLE_OVERLAY,
};

//TODO: It seems unnecessary to have multiple types for the bind

//!Binding enums. Needed to bind drawable selection
//to internal modification actions inside the drawable
enum
{
	DRAW_SPHERE_BIND_ORIGIN,
	DRAW_SPHERE_BIND_RADIUS,
	DRAW_VECTOR_BIND_ORIENTATION,
	DRAW_VECTOR_BIND_ORIGIN_ONLY,
	DRAW_VECTOR_BIND_ORIGIN,
	DRAW_VECTOR_BIND_TARGET,
	DRAW_CYLINDER_BIND_ORIGIN,
	DRAW_CYLINDER_BIND_DIRECTION,
	DRAW_CYLINDER_BIND_RADIUS,
	DRAW_RECT_BIND_TRANSLATE,
	DRAW_RECT_BIND_CORNER_MOVE,
	DRAW_TEXT_BIND_ORIGIN,

	DRAW_QUAD_BIND_ORIGIN,
	//DRAW_TEXT_BIND_TEXTDIR, //FIXME: Implement me for annotation todo.
	//DRAW_TEXT_BIND_UPDIR,
	DRAW_BIND_ENUM_END
};




//!An abstract bas class for drawing primitives
class DrawableObj
{
	protected:
		//!Is the drawable active?
		bool active;

		//!Is the object changed since last set?
		bool haveChanged;
		//!Pointer to current scene camera
		static const Camera *curCamera;	
		
		//Background colour
		static float backgroundR,backgroundG,backgroundB;
		//Pointer to texture pool object
		static TexturePool *texPool;

		static bool useAlphaBlend;
	
		//Size of the opengl window
		static unsigned int winX,winY;

		static float getHighContrastValue();
	public: 
		//!Can be selected from openGL viewport interactively?
		bool canSelect;

		//!Wants lighting calculations active during render?
		bool wantsLight;


		static void setUseAlphaBlending(bool willBlend) { useAlphaBlend =willBlend;}

		//!Is this an overlay? By default, no
		virtual bool isOverlay() const { return false;}

		//!Constructor
		DrawableObj();

		//Obtain the type mask for this drawable
		virtual unsigned int getType() const =0;	
		
		//Obtain a copy of this object, which is still valid
		// after destruction of the original
		// Disallowed by default Implement in derived object!
		//TODO: Once most sub-objects have this function, make pure virtual
		virtual DrawableObj *clone() const {ASSERT(false); return 0;}

		//!Do we need to do element based depth sorting?
		virtual bool needsDepthSorting() const { return false; } ;

	

		//!Can we break this object down into constituent elements?
		virtual bool isExplodable() const { return false;};

		//!Break object down into simple elements
		virtual void explode(std::vector<DrawableObj *> &simpleObjects);

		virtual bool hasChanged() const { return haveChanged; }


		//!Set the active state of the object
		void setActive(bool active);
		//!Pure virtual function - draw the object
		virtual void draw() const =0;

		//!Set if user can interact with object, needed for opengl hit testing
		void setInteract(bool canAct){canSelect=canAct;};

		virtual void getBoundingBox(BoundCube &b) const = 0;
		//!Drawable destructor
		virtual ~DrawableObj();

		//!If we offer any kind of external pointer interface; use this to do a recomputation as needed. This is needed for selection binding behaviour
		virtual void recomputeParams(const std::vector<Point3D> &vecs, const std::vector<float> &scalars, unsigned int mode) {};
		
		//!Set the current camera
		static void setCurCamera(const Camera *c){curCamera=c;};
		//!Set the current camera
		static void setTexPool(TexturePool *t);
		static void clearTexPool();

		//!Get the centre of the object. Only valid if object is simple
		virtual Point3D getCentroid() const  ;

		static void setWindowSize(unsigned int x, unsigned int y){winX=x;winY=y;};	
		static void setBackgroundColour(float r, float g,float b)
			{backgroundR=r; backgroundG=g;backgroundB=b;}

};

//A single point drawing class 
class DrawPoint : public DrawableObj
{
	protected:
		//!Point origin
		Point3D origin;
		//!Point colour (r,g,b,a) range: [0.0f,1.0f]
		float r,g,b,a;
	public:
		//!Constructor
		DrawPoint();
		//!Constructor that takes in positional argments
		DrawPoint(float,float,float);
		//!Destructor
		virtual ~DrawPoint();

		unsigned int getType() const {return DRAW_TYPE_POINT;}	

		virtual DrawableObj *clone() const;

		//!Sets the color of the point to be drawn
		void setColour(float r, float g, float b, float alpha);
		//!Draws the points
		void draw() const;
		//!Sets the location of the poitns
		void setOrigin(const Point3D &);

		void getBoundingBox(BoundCube &b) const { b.setInvalid();};

		Point3D getCentroid() const{ return origin;}
};

//!A point drawing class - for many points of same size & colour
class DrawManyPoints : public DrawableObj
{
	protected:
		//!Vector of points to draw
		std::vector<Point3D> pts;
		//!Point colours (r,g,b,a) range: [0.0f,1.0f]
		float r,g,b,a;
		//!Size of the point
		float size;

		mutable bool haveCachedBounds;
		mutable BoundCube cachedBounds;
	public:
		//!Constructor
		DrawManyPoints();
		//!Destructor
		virtual ~DrawManyPoints();
		
		virtual unsigned int getType() const {return DRAW_TYPE_MANYPOINT;};	

		virtual DrawableObj *clone() const;
		
		//!Swap out the internal vector with an extenal one
		void swap(std::vector<Point3D> &);
		//!Remove all points
		void clear();
		//!Add points into the drawing vector
		void addPoints(const std::vector<Point3D> &);
		//!Add a single point into the drawing vector, at a particular offset
		// *must call resize first*
		void setPoint(size_t offset,const Point3D &);

		//!Reset the number of many points to draw
		void resize(size_t newSize);
		//! set the color of the points to be drawn
		void setColour(float r, float g, float b, float alpha);
		//!Set the display size of the drawn poitns
		void setSize(float);
		//!Draw the points
		void draw() const;

		//!Shuffle the points to remove anisotropic drawing effects. Thus must be done prior to draw call.
		void shuffle();
		//!Get the bounding box that encapuslates this object
		void getBoundingBox(BoundCube &b) const; 
		
		//!return number of points
		size_t getNumPts() const { return pts.size();};
};

//!Draw a vector
class DrawVector: public DrawableObj
{
	protected:
		//!Vector origin
		Point3D origin;
		Point3D vector;

		//!Do we draw the arrow head?
		bool drawArrow; 

		//!Radius of tail of arrow
		float arrowSize;

		//!Scale arrow head by vector size
		bool scaleArrow;

		//!Whether to draw the arrow head at both ends
		bool doubleEnded;

		//!Vector colour (r,g,b,a) range: [0.0f,1.0f]
		float r,g,b,a;

		//!Size of "tail" line to draw
		float lineSize;
	public:
		//!Constructor
		DrawVector();
		//!Destructor
		virtual ~DrawVector();
	
		virtual DrawableObj *clone() const;
		
		virtual unsigned int getType() const {return DRAW_TYPE_VECTOR;};	
	
		//!Set if we want to draw the arrow or not
		void setDrawArrow(bool wantDraw) { drawArrow=wantDraw;}
		//!Sets the color of the point to be drawn
		void setColour(float r, float g, float b, float alpha);
		//!Draws the points
		void draw() const;
		//!Sets the location of the poitns
		void setOrigin(const Point3D &);
		//!Sets the location of the poitns
		void setVector(const Point3D &);

		//Set the start/end of vector in one go
		void setEnds(const Point3D &start, const Point3D &end);

		//Set to draw both ends
		void setDoubleEnded(bool wantDoubleEnd=true){doubleEnded=wantDoubleEnd;};

		//!Gets the arrow axis direction
		Point3D getVector() const { return vector;};

		//!Gets the arrow axis direction
		Point3D getOrigin() const{ return origin;};

		//!Set the arrowhead size
		void setArrowSize(float size) { arrowSize=size;}

		//!Set the "tail" line size
		void setLineSize(float size) { lineSize=size;}
		void getBoundingBox(BoundCube &b) const; 


		//!Recompute the internal parameters using the input vector information
		void recomputeParams(const std::vector<Point3D> &vecs, 
					const std::vector<float> &scalars, unsigned int mode);

};

//! A single colour triangle
class DrawTriangle : public DrawableObj
{
	protected:
		//!The vertices of the triangle
		Point3D vertices[3];
		Point3D vertNorm[3];
		//!Colour data - red, green, blue, alpha
		float r,g,b,a;
	public:
		//!Constructor
		DrawTriangle();
		//!Destructor
		virtual ~DrawTriangle();

		virtual DrawableObj *clone() const;

		virtual unsigned int getType() const {return DRAW_TYPE_TRIANGLE;};	
		
		//!Set one of three vertices (0-2) locations
		void setVertex(unsigned int, const Point3D &);
		//!Set the vertex normals
		void setVertexNorm(unsigned int, const Point3D &);
		//!Set the colour of the triangle
		void setColour(float r, float g, float b, float a);
		//!Draw the triangle
		void draw() const;
		//!Get bounding cube
		void getBoundingBox(BoundCube &b) const { b.setBounds(vertices,3);}
};

//!A smooth coloured quad
/* According to openGL, the quad's vertices need not be coplanar, 
 * but they must be convex
 */
class DrawQuad : public DrawableObj
{
	protected:
		//!Vertices of the quad
		Point3D vertices[4];

		//!Colour data for the quad
		//!The lighting normal of the triangle 
		/*! Lighting for this class is per triangle only no
		 * per vertex lighting */
		Point3D normal;
		//!Colours of the vertices (rgba colour model)
		float r[4],g[4],b[4],a[4];
	public:
		//!Constructor
		DrawQuad() {};
		//!Destructor
		virtual ~DrawQuad() {};
		
		virtual DrawableObj *clone() const;
		
		virtual unsigned int getType() const {return DRAW_TYPE_QUAD;};	

		//!Get bounding cube
		virtual void getBoundingBox(BoundCube &b) const ;
		//!sets the vertices to defautl colours (r g b and white ) for each vertex respectively
		void colourVerticies();
		//!Set vertex's location
		void setVertex(unsigned int, const Point3D &);
		void setVertices(const Point3D *);
		//!Set the colour of a vertex
		void setColour(unsigned int, float r, float g, float b, float a);
		//!Set the colour of all vertices
		void setColour(float r, float g, float b, float a);

		//!Update the normal to the surface from vertices
		/*!Uses the first 3 vertices to calculate the normal.
		 */
		void calcNormal();
		//!Draw the triangle
		void draw() const;
		
		//!Gets the arrow axis direction
		Point3D getOrigin() const;
		
		//!Recompute the internal parameters using the input vector information
		// i.e. this is used for (eg) mouse interaction
		void recomputeParams(const std::vector<Point3D> &vecs, 
				const std::vector<float> &scalars, unsigned int mode);
};

class DrawTexturedQuad : public DrawQuad
{
	private:
	//TODO: Move this back
	// into the texture pool
		unsigned char *textureData;
		size_t nX,nY;
		size_t channels;
		size_t displayMode;

		//FIXME: This should be non-mutable.  We need
		// to move texture rebinding to a pre-processing step, not at draw time
		//ID of the texture to use when drawing, -1 if not bound
		// to opengl
		mutable unsigned int textureId;
		
		//!FTGL font instance
		FTFont *font;
		
		//disallow resetting base colour to white 
		bool noColour;

		//we can only bind from the main thread.
		// this is true by default, until the texture is bound
		mutable bool needsBinding;
		
	public:
		DrawTexturedQuad();
		~DrawTexturedQuad();
		DrawTexturedQuad(const DrawTexturedQuad &d);

		//Resize the texture contents, destroying any existing contents
		void resize(size_t nx, size_t nY, unsigned int nChannels);
		//Draw the texture
		void draw() const;
		//Set the specified pixel in the texture to this value 
		void setData(size_t x, size_t y, unsigned char *entry);
		//Send the texture to the video card. 
		void rebindTexture(unsigned int mode=GL_RGB) const;	
		
		void setUseColouring(bool useColouring) {noColour= !useColouring;};
};



//!A sphere drawing 
class DrawSphere : public DrawableObj
{
	protected:
	
		//!Pointer to the GLU quadric doohicker
		GLUquadricObj *q;
		//!Origin of the object
		Point3D origin;
		//!Colour data - rgba
		float r,g,b,a;
		//!Sphere radius
		float radius;
		//!Number of lateral and longitudinal segments 
		unsigned int latSegments,longSegments;
	public:
		//!Default Constructor
		DrawSphere();
		//! Destructor
		virtual ~DrawSphere();

		virtual DrawableObj *clone() const;

		virtual unsigned int getType() const {return DRAW_TYPE_SPHERE;};	
		//!Sets the location of the sphere's origin
		void setOrigin(const Point3D &p);
		//!Gets the location of the sphere's origin
		Point3D getOrigin() const { return origin;};
		//!Set the number of lateral segments
		void setLatSegments(unsigned int);
		//!Set the number of longitudinal segments
		void setLongSegments(unsigned int);
		//!Set the radius
		void setRadius(float);
		//!get the radius
		float getRadius() const { return radius;};
		//!Set the colour (rgba) of the object
		void setColour(float r,float g,float b,float a);
		//!Draw the sphere
		void draw() const;
		//!Get the bounding box that encapuslates this object
		void getBoundingBox(BoundCube &b) const ;

		//!Recompute the internal parameters using the input vector information
		// i.e. this is used for (eg) mouse interaction
		void recomputeParams(const std::vector<Point3D> &vecs, 
				const std::vector<float> &scalars, unsigned int mode);

};

//!A tapered cylinder drawing class
class DrawCylinder : public DrawableObj
{
	protected:
		//!Pointer to quadric, required for glu. Note the caps are defined as well
		GLUquadricObj *q,*qCap[2];
		//!Colours for cylinder
		float r,g,b,a;
		//!Cylinder start and end radii
		float radius;
		//!Length of the cylinder
		float length;

		//!Origin of base and direction of cylinder
		Point3D origin, direction;
		//!number of sectors (pie slices) 
		unsigned int slices;
		//!number of vertical slices (should be 1 if endradius equals start radius
		unsigned int stacks;

		//!Do we lock the drawing to only use the first radius?
		bool radiiLocked;
	public:
		//!Constructor
		DrawCylinder();
		//!Destructor
		virtual ~DrawCylinder();

		virtual unsigned int getType() const {return DRAW_TYPE_CYLINDER;};	
		//!Set the location of the base of the cylinder
		void setOrigin(const Point3D &pt);
		//!Number of cuts perpendicular to axis - ie disks
		void setSlices(unsigned int i);
		//!Number of cuts  along axis - ie segments
		void setStacks(unsigned int i);

		//!Gets the location of the origin
		Point3D getOrigin() const { return origin;};
		//!Gets the cylinder axis direction
		Point3D getDirection() const{ return direction;};
		//!Set end radius
		void setRadius(float val);
		//!get the radius
		float getRadius() const { return radius;};
		//!Set the orientation of cylinder
		void setDirection(const Point3D &pt);
		//!Set the length of cylinder
		void setLength(float len);
		//!Set the color of the cylinder
		void setColour(float r,float g,float b,float a);
	
		//!Draw the clyinder
		void draw() const;
		//!Get the bounding box that encapuslates this object
		void getBoundingBox(BoundCube &b) const ;

		//!Recompute the internal parameters using the input vector information
		void recomputeParams(const std::vector<Point3D> &vecs, const std::vector<float> &scalars, unsigned int mode);

		virtual bool needsDepthSorting() const;



		//!Lock (or unlock) the radius to the start radius (i.e. synch the two)
		void lockRadii(bool doLock=true) {radiiLocked=doLock;};
};

//!Drawing mode enumeration for scalar field
enum
{
	VOLUME_POINTS=0
};

//!A display list generating class
/*! This class allows for the creation of display lists for openGL objects
 *  You can use this class to create a display list which will allow you to
 *  reference cached openGL calls already stored in the video card.
 *  This can be  used to reduce the overhead in the drawing routines
 */
class DrawDispList : public DrawableObj
{
	private:
		//!Static variable for the next list number to generate
		unsigned int listNum;
		//!True if the list is active (collecting/accumulating)
		bool listActive;
		//!Bounding box of objects in display list
		BoundCube boundBox;
	public:
		//!Constructor
		DrawDispList();
		//!Destructor
		virtual ~DrawDispList();
		
		virtual unsigned int getType() const {return DRAW_TYPE_DISPLAYLIST;}

		//!Execute the display list
		void draw() const;		

		//!Set it such that many openGL calls map to the display list.
		/*!If "execute" is true, the commands will be excuted after
		 * accumulating the display list buffer
		 * For a complete list of which calls map to the dispaly list,
		 * see the openGL spec, "Display lists"
		 */
		bool startList(bool execute);
	
		//!Add a drawable object - list MUST be active
		/* If the list is not active, this will simply exectue
		 * the draw function of the drawable
		 */
		void addDrawable(const DrawableObj *);
			
		//!Close the list - this *will* clear the gl error system
		bool endList();
		//!Get bounding cube
		void getBoundingBox(BoundCube &b) const { b=boundBox;}

};

//!A class to draw text obects using FTGL
/*May not be the best way to do this.
 * MIght be better to have static instances
 * of each possible type of font, then
 * render the text appropriately
 */
class DrawGLText : public DrawableObj
{

	private:

		//!FTGL font instance
		FTFont *font;

		//!Font string
		std::string fontString;
		//!Current font mode
		unsigned int curFontMode;
		
		//!Text string
		std::string strText;
		//!Origin of text
		Point3D origin;
		//!Alignment mode
		unsigned int alignMode;

		//!Colours for text 
		float r,g,b,a;

		//Two vectors required to define 
		//these should always give a dot prod of
		//zero

		//!Up direction for text
		Point3D up;

		//!Text flow direction
		Point3D textDir;
	
		//!Direction for which text should be legible
		/*! This will ensure that the text is legible from 
		 * the direction being pointed to by normal. It is
		 * not the true normal of the quad. as the origin and the
		 * up direction specify some of that data already
		 */
		Point3D readDir;
		
		//!True if no erro
		bool isOK;

		//!Ensure that the text is never mirrored from view direction
		bool ensureReadFromNorm;
	public:
		//!Constructor (font file & text mode)
		/* Valid font types are FTGL_x where x is
		 * 	BITMAP
		 * 	PIXMAP
		 * 	OUTLINE
		 * 	POLYGON
		 * 	EXTRUDE
		 * 	 TEXTURE
		 */
		DrawGLText(std::string fontFile,
					unsigned int ftglTextMode);
		DrawGLText(const DrawGLText &other);

		//!Destructor
		virtual ~DrawGLText();

		virtual unsigned int getType() const {return DRAW_TYPE_GLTEXT;}
		//!Set the size of the text (in points (which may be GL units,
		//unsure))
		inline void setSize(unsigned int size)
			{if(isOK){font->FaceSize(size);}};
		//!Set the depth of the text (in points, may be GL units, unsure)
		inline void setDepth(unsigned int depth)
			{if(isOK){font->Depth(depth);}};
		//!Returs true if the class data is good
		inline bool ok() const
			{return isOK;};

		//!Set the text string to be displayed
		inline void setString(const std::string &str)
			{strText=str;};

		//!Render the text string
		void draw() const;

		//!Set the up direction for the text
		/*!Note that this must be orthogonal to
		 * the reading direction
		 */
		inline void setUp(const Point3D &p) 
			{ up=p;up.normalise();};
		//!Set the origin
		inline void setOrigin(const Point3D &p) 
			{ origin=p;};
		//!Set the reading direction
		/*!The reading direction is the direction
		 * from which the text should be legible
		 * This direction is important only if ensureReadFromNorm
		 * is set
		 */
		inline void setReadDir(const Point3D &p) 
			{ readDir=p;}; 

		//!Set the text flow direction
		/*! This *must* be orthogonal to the up vector
		 * i.e. forms a right angle with
		 */
		inline void setTextDir(const Point3D &p) 
			{textDir=p;textDir.normalise();}
		//!Return the location of the lower-left of the text
		inline Point3D getOrigin() const 
			{return origin;};

		inline void setReadFromNorm(bool b)
			{ensureReadFromNorm=b;}

		//!Set the colour (rgba) of the object
		void setColour(float r,float g,float b,float a);
		
		//!Get the bounding box for the text
		void getBoundingBox(BoundCube &b) const; 

		//!Set the text alignment (default is left)
		void setAlignment(unsigned int mode);
		
		//Binding parameter recomputation
		void recomputeParams(const std::vector<Point3D> &vecs, 
				const std::vector<float> &scalars, unsigned int mode);
};




//!A class to draw rectangluar prisms
class DrawRectPrism  : public DrawableObj
{
	private:
		//!Drawing mode, (line or surface);
		unsigned int drawMode;
		//!Colours for prism
		float r,g,b,a;
		//!Lower left and upper right of box
		Point3D pMin, pMax;
		//!Thickness of line
		float lineWidth;
	public:
		DrawRectPrism();
		~DrawRectPrism();

		virtual unsigned int getType() const {return DRAW_TYPE_RECTPRISM;}
	
		virtual DrawableObj *clone() const;

		//!Draw object
		void draw() const;

		//!Set the draw mode
		void setDrawMode(unsigned int n) { drawMode=n;};
		//!Set colour of box
		void setColour(float rnew, float gnew, float bnew, float anew=1.0f);
		//!Set thickness of box
		void setLineWidth(float lineWidth);
		//!Set up box as axis-aligned rectangle using two points
		void setAxisAligned(const Point3D &p1,const Point3D &p2);
		//!Set up box as axis-aligned rectangle using bounding box 
		void setAxisAligned(const BoundCube &b);

		void getBoundingBox(BoundCube &b) const;
		
		//!Recompute the internal parameters using the input vector information
		void recomputeParams(const std::vector<Point3D> &vecs, const std::vector<float> &scalars, unsigned int mode);
};

struct RGBFloat
{
	float v[3];
};

//Abstract class as base for overlays
class DrawableOverlay : public DrawableObj
{
	protected:
		//alpha (transparancy) value
		float a;
		//!Height and width of overlay (total)
		float height,width;
		//Fractional coordinates for the  top left of the overlay
		float position[2];
	public:
		DrawableOverlay() {} ;
		//Declared as pure virtual to force ABC
		virtual ~DrawableOverlay() =0;
		void setAlpha(float alpha) { a=alpha;};
		void setSize(float widthN, float heightN) {height=heightN, width=widthN;} 
		void setSize(float size) {width=height=size;};
		void setPosition(float newTLX,float newTLY) { position[0]=newTLX; position[1]=newTLY;}

		void getBoundingBox(BoundCube &b) const {b.setInvalid();};
		//!This is an overlay
		bool isOverlay() const {return true;};
};

class DrawColourBarOverlay : public DrawableOverlay
{
	private:
		FTFont *font;

		//!Colours for each element
		std::vector<RGBFloat> rgb;
		//!Minimum and maximum values for the colour bar (for ticks)
		float min,max;

	public:
	
		DrawColourBarOverlay();
		DrawColourBarOverlay(const DrawColourBarOverlay &o);
		~DrawColourBarOverlay(){delete font;};
		
		virtual DrawableObj *clone() const; 

		virtual unsigned int getType() const {return DRAW_TYPE_COLOURBAR;}

		void setColourVec(const std::vector<float> &r,
					const std::vector<float> &g,
					const std::vector<float> &b);
		//!Draw object
		void draw() const;

		void setMinMax(float minNew,float maxNew) { min=minNew;max=maxNew;};
		
};

//!A class to hande textures to draw
class DrawTexturedQuadOverlay : public DrawableOverlay
{
	private:
		unsigned int textureId;
	
		bool textureOK;

	public:
		DrawTexturedQuadOverlay();
		~DrawTexturedQuadOverlay();

		virtual unsigned int getType() const {return DRAW_TYPE_TEXTUREDOVERLAY;}
	
		static void setWindowSize(unsigned int x, unsigned int y){winX=x;winY=y;};	
		//!Set the texture by name
		bool setTexture(const char *textureFile);
		//!Draw object
		void draw() const;
};

//!Multi-frame texture - Animated overlay
class DrawAnimatedOverlay : public DrawableOverlay
{
	private:
		//ID of the texture to use when drawing, -1 if not bound
		// to opengl
		unsigned int textureId;

		timeval animStartTime;

		bool textureOK;

		//Time delta before repeating animation
		float repeatInterval;
		
		//Time before showing the image
		float delayBeforeShow;

		//Time for fadein after show
		float fadeIn;

	protected:		
		void getAnimationStat(float &alpha, float &deltaTime) const;
	public:
		DrawAnimatedOverlay();
		~DrawAnimatedOverlay();

		virtual unsigned int getType() const {return DRAW_TYPE_ANIMATEDOVERLAY;}

		//Set the time between repeats for the animation
		void setRepeatTime(float timeV) { repeatInterval=timeV;}

		//Set the time before the texture appears
		void setShowDelayTime(float showDelayTime) 
			{ ASSERT(showDelayTime >=0.0f);  delayBeforeShow = showDelayTime;}

		//Set the time during which the alpha value will be ramped up.
		// activated after the delay time (ie time before 100% visible is fadeInTime + 
		//	delayTime.
		void setFadeInTime(float fadeInTime)
			{ ASSERT(fadeInTime >=0.0f); fadeIn=fadeInTime;}

		//!Set the texture by name
		bool setTexture(const std::vector<std::string> &textureFiles, float timeRepeat=1.0f);

		void resetTime() ;

		//!Draw object
		void draw() const;

		bool isOK() const { return textureOK; }
};

//!Draw a progress (segments with completion) overlay
class DrawProgressCircleOverlay : public DrawAnimatedOverlay
{

	//Shows the progress of K filters, each with M steps,  and each
	// step has a (0-100) progress. Result is drawn as filled arcs.
	// Each filter is one arc, and this is divided into steps.
	// each step then fills up

	private:
		//Progress in the current step, range [0,100]
		unsigned int stepProgress;
		//Number of steps in process 
		unsigned int maxStep;
		//The step that we are currently in
		unsigned int step;
		//Number of filters that are to be analysed 
		unsigned int totalFilters;
		//Filter that we are analysing (0->n-1)
		unsigned int curFilter;
		

		//Draw a 2D wheel shaped section. Complete variable toggles the style of the drawing from a completed, to a n incompleted segment
		void drawSection(unsigned int degreeStep, 
	float rIn, float rOut,float startTheta, float stopTheta, bool complete) const;
	public:
		DrawProgressCircleOverlay();
		~DrawProgressCircleOverlay();
		void setCurFilter(unsigned int v) { curFilter= v;}
		void setMaxStep(unsigned int v) { maxStep= v;}
		void setNumFilters(unsigned int v) { totalFilters= v;}
		void setProgress(unsigned int newProg) { ASSERT(newProg <=100); stepProgress = newProg;}
		void setStep(unsigned int v) { ASSERT(v<=maxStep); step= v;}

		void reset();

		static void setWindowSize(unsigned int x, unsigned int y){winX=x;winY=y;};
		virtual unsigned int getType() const {return DRAW_TYPE_PROGRESSCIRCLE_OVERLAY;}
		void draw() const;
};

class DrawPointLegendOverlay : public DrawableOverlay
{
	private:
	static bool quadSet;

	FTFont *font;
	//Items to draw n overlay, and colour to use to draw
	std::vector<std::pair<std::string,RGBFloat> > legendItems;
	bool enabled;
	public:
		DrawPointLegendOverlay();
		~DrawPointLegendOverlay();
		
		DrawPointLegendOverlay(const DrawPointLegendOverlay &);

		DrawableObj *clone() const;
		virtual unsigned int getType() const {return DRAW_TYPE_LEGENDOVERLAY;}
		void draw() const;

		void clear(); 
		void addItem(const std::string &s, float r, float g, float b);

};

struct RGBThis
{
	unsigned char v[3];
};

//!This class allows for the visualisation of 3D scalar fields
class DrawField3D : public DrawableObj
{
	private:
		mutable std::vector<std::pair<Point3D,RGBThis> > ptsCache;
		mutable bool ptsCacheOK;
	protected:
		//!Alpha transparancy of objects in field
		float alphaVal;

		//!Size of points in the field -
		//only meaningful if the render mode is set to alpha blended points
		float pointSize;

		//!True if the scalar field's bounding box is to be drawn
		bool drawBoundBox;

		//!Colours for the bounding boxes
		float boxColourR,boxColourG,boxColourB,boxColourA;
		
		//!True if volume grid is enabled
		bool volumeGrid;

		//!Colour map lower and upper bounds
		float colourMapBound[2];

		//!Which colourmap to use
		unsigned int colourMapID;

		//!Sets the render mode for the 3D volume 
		/* Possible modes
		 * 0: Alpha blended points
		 */
		unsigned int volumeRenderMode;
		//!The scalar field - used to store data values
		const Voxels<float> *field;
	public:
		//!Default Constructor
		DrawField3D();
		//!Destructor
		virtual ~DrawField3D();

		virtual unsigned int getType() const {return DRAW_TYPE_FIELD3D;}

		//!Get the bounding box for this object
		void getBoundingBox(BoundCube &b) const;
		
		//!Set the render mode (see volumeRenderMode variable for details)
		void setRenderMode(unsigned int);
		
		//!Set the field pointer 
		void setField(const Voxels<float> *field); 

		//!Set the alpha value for elemnts
		void setAlpha(float alpha);

		//!Set the colour bar minima and maxima from current field values
		void setColourMinMax();

		//!Set the colourMap ID
		void setColourMapID(unsigned int i){ colourMapID=i;};

		//!Render the field
		void draw() const;

		//!Set the size of points
		void setPointSize(float size);
		
		//!Set the colours that ar ebeing used in the tempMap
		void setMapColours(unsigned int map);

		//!Set the coour of the bounding box
		void setBoxColours(float r, float g, float b, float a);

};

class DrawIsoSurface: public DrawableObj
{
private:

	mutable bool cacheOK;

	//!should we draw the thing 
	//	- in wireframe
	//	-Flat
	//	-Smooth
	//
	unsigned int drawMode;

	//!Isosurface scalar threshold
	float threshold;

	Voxels<float> *voxels;	

	mutable std::vector<TriangleWithVertexNorm> mesh;

	//!Warning. Although I declare this as const, I do some naughty mutating to the cache.
	void updateMesh() const;	
	
	//!Point colour (r,g,b,a) range: [0.0f,1.0f]
	float r,g,b,a;
public:

	DrawIsoSurface();
	~DrawIsoSurface();

	virtual unsigned int getType() const {return DRAW_TYPE_ISOSURFACE;}
	//!Transfer ownership of data pointer to class
	void swapVoxels(Voxels<float> *v);

	//!Set the drawing method

	//Draw
	void draw() const;

	//!Set the isosurface value
	void setScalarThresh(float thresh) { threshold=thresh;cacheOK=false;mesh.clear();};

	//!Get the bouding box (of the entire scalar field)	
	void getBoundingBox(BoundCube &b) const ;
		
	//!Sets the color of the point to be drawn
	void setColour(float rP, float gP, float bP, float alpha) { r=rP;g=gP;b=bP;a=alpha;} ;
	
	//!Do we need depth sorting?
	bool needsDepthSorting() const;
};

/////////////// OPENVDB ///////////////////////////////////////////////////////////////////////////

class LukasDrawIsoSurface: public DrawableObj
{
private:

	mutable bool cacheOK;
	openvdb::FloatGrid::Ptr grid;

	//!Warning. Although I declare this as const, I do some naughty mutating to the cache.
	void updateMesh() const;

	mutable std::vector<openvdb::Vec3s> points;
  	mutable std::vector<openvdb::Vec3I> triangles;
  	mutable std::vector<openvdb::Vec4I> quads;	
	
	double isovalue;
	double voxelsize;

	//!Point colour (r,g,b,a) range: [0.0f,1.0f]
	float r,g,b,a;
public:

	LukasDrawIsoSurface();
	~LukasDrawIsoSurface();

	virtual unsigned int getType() const;
	//!Get the bouding box (of the entire scalar field)	
	void getBoundingBox(BoundCube &b) const ;

	//Draw
	void draw() const;
	
	// Set the grid
	void setGrid(openvdb::FloatGrid::Ptr g) {grid=g->deepCopy();cacheOK=false;};

	//!Set the isosurface value
	void setIsovalue(float iso) {isovalue=iso;cacheOK=false;};
	
	//!Set the voxelsize of the isosurface
	void setVoxelsize(float voxel_size) {voxelsize=voxel_size;cacheOK=false;};

	//!Sets the color of the point to be drawn
	void setColour(float rP, float gP, float bP, float alpha) { r=rP;g=gP;b=bP;a=alpha;} ;

};

///////////////////////////////////////////////////////////////////////////////////////////////////////

class DrawAxis : public DrawableObj
{
	private:
		//!Drawing style
		unsigned int style;
		Point3D position;
		//!size
		float size;

	public:
		DrawAxis();
		~DrawAxis();
	
		virtual unsigned int getType() const {return DRAW_TYPE_AXIS;}
		
		virtual DrawableObj *clone() const;

		//!Draw object
		void draw() const;


		void setStyle(unsigned int style);
		void setSize(float newSize);
		void setPosition(const Point3D &p);

		void getBoundingBox(BoundCube &b) const;

};


//Draw a 2D filled circle
class Draw2DCircle : public DrawableObj
{
	private:
		float centre[2];
		float angularStep;
		float radius; 

		//Circle colour
		float r,g,b;

		//Should the circle be drawn as an outline, or as a filled object
		bool filled;
	public:
		Draw2DCircle();
		
		void result() const; 
		//Obtain the type mask for this drawable
		virtual unsigned int getType() const;	
		virtual DrawableObj *clone() const; 
		virtual void getBoundingBox(BoundCube &b) const;

		virtual void draw() const;

		void setCentre(float fx,float fy) { centre[0] = fx; centre[1]= fy;};
		void setRadius(float r) { radius=r;}
		//Angular step in radiians
		void setAngularStep(float da) { angularStep = da;};
	
		void setColour(float rP, float gP, float bP) { r=rP;g=gP;b=bP;} ;
			
};

#endif
