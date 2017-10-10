/***********************************************************************
Small image viewer using Vrui, modified by M. Montgomery. 
Copyright (c) 2011-2016 Oliver Kreylos

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#ifndef IMAGEMAP_INCLUDED
#define IMAGEMAP_INCLUDED

//#include <Misc/MessageLogger.h>
//#include <Math/Math.h>
#include <GL/gl.h>               // MM: present in ElevationColorMap.h
//#include <GL/GLColorTemplates.h>
//#include <GL/GLMaterial.h>
#include <GL/GLObject.h>         // MM: present in ElevationColorMap.h
//#include <GL/GLContextData.h>
//#include <GL/GLTransformationWrappers.h>
#include <GL/GLTextureObject.h>  // MM: added from ElevationColorMap.h
#include <Images/RGBImage.h>
#include <Images/ReadImageFile.h>
#include <Images/TextureSet.h>
//#include <Vrui/Vrui.h>
//#include <Vrui/Application.h>
//#include <Vrui/Tool.h>
//#include <Vrui/GenericToolFactory.h>
//#include <Vrui/ToolManager.h>
//#include <Vrui/DisplayState.h>
#include <Vrui/OpenFile.h>
#include <iostream>  // MM: added

#include "Types.h"   // MM: added from ElevationColorMap.h

class DepthImageRenderer;

class ImageMap:public GLTextureObject
	{
	// MM: the following have all been added from ElevationColorMap.h;
	//     may not all be necessary
	  
	/* Elements: */
	private:	
	GLfloat texturePlaneEq[4]; // Texture mapping plane equation in GLSL-compatible format
        Images::TextureSet textures;
			  
	/* Constructors and destructors: */
	public:
	ImageMap(const char* imageName);

	/* Methods */
	virtual void initContext(GLContextData& contextData) const; // from GLObject
	void load(const char* imageName); // Overrides image map by loading the given image file
	void calcTexturePlane(const Plane& basePlane); // Calculates the texture mapping plane for the given base plane equation
	void calcTexturePlane(const DepthImageRenderer* depthImageRenderer); // Calculates the texture mapping plane for the given depth image renderer
	void bindTexture(GLContextData& contextData) const; // Binds the image map texture object to the currently active texture unit
	void uploadTexturePlane(GLint location) const; // Uploads the texture mapping plane equation into the GLSL 4-vector at the given uniform location
	};

#endif
