/*
 *	cameras.h - 3D cameras for opengl
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

#ifndef CAMERAS_H
#define CAMERAS_H

#include "common/basics.h"

//libxml2 headers
#ifdef ATTRIBUTE_PRINTF
	#pragma push_macro("ATTRIBUTE_PRINTF")
	#include <libxml/xmlreader.h>
	#pragma pop_macro(" ATTRIBUTE_PRINTF")
#else
	#include <libxml/xmlreader.h>
	#undef ATTRIBUTE_PRINTF
#endif

enum CAM_ENUM
{
	CAM_FREE=1,
	CAM_LOOKAT
};

enum 
{
	PROJECTION_MODE_PERSPECTIVE,
	PROJECTION_MODE_ORTHOGONAL,
	PROJECTION_MODE_ENUM_END //not a valid mode.
};

//!Key types for property setting and getting properties
enum
{
	CAMERA_KEY_LOOKAT_LOCK,
	CAMERA_KEY_LOOKAT_ORIGIN,
	CAMERA_KEY_LOOKAT_TARGET,
	CAMERA_KEY_LOOKAT_UPDIRECTION,
	CAMERA_KEY_LOOKAT_FOV,
	CAMERA_KEY_LOOKAT_PROJECTIONMODE,
	CAMERA_KEY_LOOKAT_ORTHOSCALE
};

//visible direction enum
enum
{
	CAMERA_DIR_ZPLUS, //0
	CAMERA_DIR_YMINUS, //1
	CAMERA_DIR_YPLUS, //2
	CAMERA_DIR_XPLUS, //3
	CAMERA_DIR_ZMINUS, //4
	CAMERA_DIR_XMINUS, //5
};

class CameraProperty
{
	public:
		unsigned int type;
		unsigned int key;
		std::string data;
		std::string name;
};

class CameraProperties 
{
	public:
		std::vector<std::vector<CameraProperty>  > props;
		void clear() { props.clear();};	
		void addGroup() {props.resize(props.size()+1);}
		void addEntry(CameraProperty &p) { ASSERT(props.size()); props.back().push_back(p);}
};




//!An abstract base class for a camera
class Camera
{
	protected:

		bool lock;
		//!Camera location
		Point3D origin;
		//!Direction camera is looking in
		Point3D viewDirection;
		//!Up direction for camera (required to work out "roll")
		Point3D upDirection;

		//!Projection mode (otho, perspective...)_
		unsigned int projectionMode;

		//!The current orthographic scaling
		float orthoScale;

		//!Type number
		unsigned int typeNum;
		//!user string, e.g. camera name
		std::string userString;
	public:
		//!constructor
		Camera();
		//!Destructor
		virtual ~Camera();
		//!Duplication routine. Must delete returned pointer manually. 
		virtual Camera *clone() const=0;

		//!Streaming output operator, presents human readable text
                friend std::ostream &operator<<(std::ostream &stream, const Camera &);

		//!Return the origin of the camera
		Point3D getOrigin() const;
		//!Return the view direction for the camera
		Point3D getViewDirection() const;
		//!Return the up direction for the camera
		Point3D getUpDirection() const;

		//!return the projection mode
		unsigned int getProjectionMode() const{ return projectionMode;};

		float getOrthoScale() const { return orthoScale; }
		
		//!Set the camera's position
		virtual void setOrigin(const Point3D &);
		//!set the direction that the camera looks towards
		void setViewDirection(const Point3D &);
		//!set the direction that the camera considers "up"
		void setUpDirection(const Point3D &);
		
		//!Set the user string
		void setUserString(const std::string &newString){ userString=newString;};
		//!Get the user string
		std::string getUserString() const { return userString;};

		//!Do a forwards "dolly",where the camera moves along its viewing axis. In ortho mode, instead of moving along axis, a scaling is performed
		virtual void forwardsDolly(float dollyAmount);

		//!Move the camera origin
		virtual void move(float leftRightAmount,float UpDownAmount);

		//!Move the camera origin
		virtual void translate(float leftRightAmount,float UpDownAmount);

		//!pivot the camera
		/* First pivots the camera around the across direction
		 * second pivot sthe camera around the up direction
		 */
		virtual void pivot(float rollAroundAcross, float rollaroundUp);

		//!Roll around the view direction
		virtual void roll(float roll) =0;
		//!Applies the camera settings to openGL. Ensures the far planes
		//is set to make the whole scene visible
		virtual void apply(float outputRatio,const BoundCube &b,bool loadIdentity=true) const=0;
		//!Ensures that the given boundingbox should look nice, and be visible
		virtual void ensureVisible(const BoundCube &b, unsigned int face=3)=0;

		//!Obtain the properties specific to a camera
		virtual void getProperties(CameraProperties &p) const =0;
		//!Set the camera property from a key & string pair
		virtual bool setProperty(unsigned int key, const std::string &value) =0;

		unsigned int type() const {return typeNum;};

		//!Write the state of the camera
		virtual bool writeState(std::ostream &f, unsigned int format, unsigned int tabs) const =0;
		
		//!Read the state of the camera from XML document
		virtual bool readState(xmlNodePtr nodePtr)=0;

};

//!A perspective camera that looks at a specific location
class CameraLookAt : public Camera
{
	protected:
		//!Location for camera to look at
		Point3D target;
		
		void recomputeViewDirection();
		
		//!Perspective FOV
		float fovAngle;

		//!Near clipping plane distance. 
		float nearPlane;
		//!Far plane is computed on-the-fly. cannot be set directly. Oh no! mutable. gross!
		mutable float farPlane;

		//!Distort to the viewing frustum. (eg for stero) ( a frustum is a rectangular pyramid with the top cut off)
		float frustumDistortion;

	public:
		//!Constructor
		CameraLookAt();

		//!Streaming output operator, presents human readable text
                friend std::ostream &operator<<(std::ostream &stream, const CameraLookAt &);
		//!clone function
		Camera *clone() const;	
		//!Destructor
		virtual ~CameraLookAt();
		//!Set the look at target
		void setOrigin(const Point3D &);
		//!Set the look at target
		void setTarget(const Point3D &);
		//!Get the look at target
		Point3D getTarget() const;
		
		//!Get the camera's FOV angle (full angle across)
		float getFOV() const {return fovAngle;}

		float getNearPlane() const { return nearPlane; }

		//!Applies the view transform 
		void apply(float outAspect, const BoundCube &boundCube,bool loadIdentity=true) const;
	
		//!Only apply the look-at opengl transform
		void lookAt() const;
		//!Do a forwards "dolly",where the camera moves along its viewing axis
		void forwardsDolly(float dollyAmount);

		//!Move the camera origin
		void move(float leftRightAmount,float UpDownAmount);
		//!Simulate pivot of camera
		/* Actually I pivot by moving the target internally.
		*/
		void pivot(float lrRad,float udRad);

		void translate(float lrTrans, float udTrans);



		//Clockwise roll looking from camera view by rollRad radians
		void roll(float rollRad);
		
		//!Ensure that up direction is perpendicular to view direction
		void recomputeUpDirection();
		
		void repositionAroundTarget(unsigned int direction);

		//!Ensure that the box is visible
		/*! Face is set by cube net
					0
		 	 	    1   2   3
				  	4
				  	5
		2 is the face directed to the +ve x axis,
		with the "up"" vector on the 3 aligned to z,
		so "0" is perpendicular to the Z axis and is "visible"
		 */
		virtual void ensureVisible(const BoundCube &b, unsigned int face=3);
		
		//!Return the user-settable properties of the camera
		void getProperties(CameraProperties &p) const;
		//!Set the camera property from a key & string pair
		bool setProperty(unsigned int key, const std::string &value);

		//!Write the state of the camera
		bool writeState(std::ostream &f, unsigned int format, unsigned int tabs=0) const;

		//!Read the state of the camera
		bool readState(xmlNodePtr nodePtr) ;
		

		float getViewWidth(float depth) const;

		void setFrustumDistort(float offset){frustumDistortion=offset;};

};

#endif
