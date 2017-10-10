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

/* MM: TO DO: adapt existing ImageViewer code and ElevationColorMap code
              to create an image class instead of a color map class */

#include "ImageMap.h"

// MM: includes from ElevationColorMap.cpp:
#include <string>
#include <Misc/ThrowStdErr.h>
#include <Misc/FileNameExtensions.h>
#include <IO/ValueSource.h>
#include <GL/gl.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <Vrui/OpenFile.h>
#include "Types.h"
#include "DepthImageRenderer.h"
#include "Config.h"
#include <iostream> // MM: added

/*
#include <Misc/MessageLogger.h>
#include <Math/Math.h>
#include <GL/gl.h>
#include <GL/GLColorTemplates.h>
#include <GL/GLMaterial.h>
#include <GL/GLObject.h>
#include <GL/GLContextData.h>
#include <GL/GLTransformationWrappers.h>
#include <Images/RGBImage.h>
#include <Images/ReadImageFile.h>
#include <Images/TextureSet.h>
#include <Vrui/Vrui.h>
#include <Vrui/Application.h>
#include <Vrui/Tool.h>
#include <Vrui/GenericToolFactory.h>
#include <Vrui/ToolManager.h>
#include <Vrui/DisplayState.h>
#include <Vrui/OpenFile.h>
#include <iostream>
*/

ImageMap::ImageMap(const char* imageName) {
  load(imageName);
}

virtual void ImageMap::initContext(GLContextData& contextData) const {
  // method from GLObject
  // code from ElevationColorMap.cpp
  
  // Initialize required OpenGL extensions
  // GLARBShaderObjects::initExtension();    // MM: don't think we need GL extensions for image

  // Create the data item and associate it with this object
  DataItem* dataItem=new DataItem;           // what DataItem struct is this from?
  contextData.addDataItem(this,dataItem);
}

void ImageMap::load(const char* imageName) {
  // Overrides image map by loading the given image file
  // MM: TO DO - implement

  // Get image file name
  std::string fullImageName;
  if(imageName[0]=='/') {
      // Use the absolute file name directly
      fullImageName=imageName;
  }
  else {
      // Assemble a file name relative to the configuration file directory
      fullImageName=CONFIG_CONFIGDIR;
      fullImageName.push_back('/');
      fullImageName.append(imageName);
  }
	
  // Open image file
  char* filename = "/opt/SARndbox-2.3/maya/sample_text.jpg"; // MM: not sure correct path
  Images::TextureSet::Texture& tex=textures.addTexture(Images::readImageFile(filename,Vrui::openFile(filename)),GL_TEXTURE_2D,GL_RGB8,0U);
  // MM: this isn't going to work. there wasn't Images::TextureSet in the normal SARndbox
  //     how to bind image to texture?
	
  // Set clamping and filtering parameters for mip-mapped linear interpolation
  tex.setMipmapRange(0,1000);
  tex.setWrapModes(GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
  tex.setFilterModes(GL_LINEAR_MIPMAP_LINEAR,GL_LINEAR);
  
  // Load the height color map
  // setColors(heightMapKeys.size(),&heightMapColors[0],&heightMapKeys[0],256);
	
  // Invalidate the color map texture object
  ++textureVersion;
}

void ImageMap::calcTexturePlane(const Plane& basePlane) {
  // Calculates the texture mapping plane for the given base plane equation

  // MM: code from ElevationColorMap.cpp
  // TO DO: look at GLColorMap to see what this does. need to adapt?
  
  // Scale and offset the camera-space base plane equation
  const Plane::Vector& bpn=basePlane.getNormal();
  Scalar bpo=basePlane.getOffset();
  Scalar hms=Scalar(getNumEntries()-1)/((getScalarRangeMax()-getScalarRangeMin())*Scalar(getNumEntries()));
  Scalar hmo=Scalar(0.5)/Scalar(getNumEntries())-hms*getScalarRangeMin();

  // MM: texturePlaneEq was declared in .h: GLfloat texturePlaneEq[4];
  for(int i=0;i<3;++i)
    texturePlaneEq[i]=GLfloat(bpn[i]*hms);
  texturePlaneEq[3]=GLfloat(-bpo*hms+hmo);
}

void ImageMap::calcTexturePlane(const DepthImageRenderer* depthImageRenderer) {
  // Calculate texture plane based on the given depth image renderer's base plane
  // MM: code from ElevationColorMap.cpp
  calcTexturePlane(depthImageRenderer->getBasePlane());
}

void ImageMap::bindTexture(GLContextData& contextData) const {
  // Binds the image map texture object to the currently active texture unit

  /* Set up OpenGL state: */
  glPushAttrib(GL_ENABLE_BIT);
  glEnable(GL_TEXTURE_2D);
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
	
  /* Get the texture set's GL state: */
  // MM: getGLState returns OpenGL texture state object for the given OpenGL context
  Images::TextureSet::GLState* texGLState=textures.getGLState(contextData);
	
  /* Bind the texture object: */
  // MM: bindTexture binds the texture object associated with the given key
  //     to its texture target on the current texture unit, returns texture state.
  //     Note the key is 0U, which was the key in ImageViewer constructor.
  const Images::TextureSet::GLState::Texture& tex=texGLState->bindTexture(0U);
  const Images::BaseImage& image=tex.getImage();
  // MM: try replacing this with Image so can test the setPixel() method (TO DO)
	
  /* Query the range of texture coordinates: */
  const GLfloat* texMin=tex.getTexCoordMin();
  const GLfloat* texMax=tex.getTexCoordMax();
	
  /* Draw the image: */
  /* MM: Note: texture coordinates specify the point in the texture image that will 
     correspond to the vertex you're specifying them for. see Vrui and OpenGL notes */
  glBegin(GL_QUADS);
  // MM: ^ specifies the following vertices as groups of 4 to interpret as quadrilaterals
  glTexCoord2f(texMin[0],texMin[1]);  // MM: texture coords. 2f means two floats (for 2D)
  glVertex2i(0,0);                    // MM: vertex points. 2i means two ints (for 2D)
  glTexCoord2f(texMax[0],texMin[1]);
  glVertex2i(image.getSize(0),0);
  glTexCoord2f(texMax[0],texMax[1]);
  glVertex2i(image.getSize(0),image.getSize(1));
  glTexCoord2f(texMin[0],texMax[1]);
  glVertex2i(0,image.getSize(1));
  glEnd();
  // MM: ^ ends the listing of vertices
	
  /* Protect the texture object: */
  glBindTexture(GL_TEXTURE_2D,0);
  /* Restore OpenGL state: */
  glPopAttrib();
  return;
  
  // MM: code from ElevationColorMap.cpp
  // TO DO: adapt for image
  // Retrieve the data item
  DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
  // Bind the texture object
  glBindTexture(GL_TEXTURE_1D,dataItem->textureObjectId);
	
  // Check if the color map texture is outdated
  if(dataItem->textureObjectVersion!=textureVersion) {
    // Upload the color map entries as a 1D texture
    glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D,0,GL_RGB8,getNumEntries(),0,GL_RGBA,GL_FLOAT,getColors());
		
    dataItem->textureObjectVersion=textureVersion;
  }
}

void ImageMap::uploadTexturePlane(GLint location) const {
  // Uploads the texture mapping plane equation into the GLSL 4-vector at the given uniform location
  
  // MM: code from ElevationColorMap.cpp
  // TO DO: adapt for image if necessary
  glUniformARB<4>(location,1,texturePlaneEq);
  /* MM: texturePlaneEq was declared in .h: GLfloat texturePlaneEq[4];

     glUniform operates on the program object that was made part of current state 
     by calling glUseProgram. glUseProgram is called in DepthImageRenderer.cpp and 
     SurfaceRenderer.cpp. glUniformARB is deprecated, possibly equivalent to
     glUniform2f or glUniform4fv (see Vrui and OpenGL notes). */
}



/****************************
Methods of class ImageViewer:
****************************/

ImageViewer::ImageViewer()
	:Vrui::Application()
	{
	/* Load the image into the texture set: */
        // MM: addTexture(BaseImage, open file target, internal format, key)
	//     GL_RGB8 must mean RGB 8-bit image. Note the key, 0U, is referenced in display()
	char* filename = "sample_text.jpg";
	Images::TextureSet::Texture& tex=textures.addTexture(Images::readImageFile(filename,Vrui::openFile(filename)),GL_TEXTURE_2D,GL_RGB8,0U);
	
	/* Set clamping and filtering parameters for mip-mapped linear interpolation: */
	tex.setMipmapRange(0,1000);
	tex.setWrapModes(GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	tex.setFilterModes(GL_LINEAR_MIPMAP_LINEAR,GL_LINEAR);
	}

ImageViewer::~ImageViewer(void)
	{
	}

void ImageViewer::display(GLContextData& contextData) const
	{
	/* Set up OpenGL state: */
	glPushAttrib(GL_ENABLE_BIT);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
	
	/* Get the texture set's GL state: */
	// MM: getGLState returns OpenGL texture state object for the given OpenGL context
	Images::TextureSet::GLState* texGLState=textures.getGLState(contextData);
	
	/* Bind the texture object: */
	// MM: bindTexture binds the texture object associated with the given key
	//     to its texture target on the current texture unit, returns texture state.
	//     Note the key is 0U, which was the key in ImageViewer constructor.
	const Images::TextureSet::GLState::Texture& tex=texGLState->bindTexture(0U);
	const Images::BaseImage& image=tex.getImage();
	// MM: try replacing this with Image so can test the setPixel() method (TO DO)
	
	/* Query the range of texture coordinates: */
	const GLfloat* texMin=tex.getTexCoordMin();
	const GLfloat* texMax=tex.getTexCoordMax();
	
	/* Draw the image: */
	/* MM: Note: texture coordinates specify the point in the texture image that will 
               correspond to the vertex you're specifying them for. see Vrui and OpenGL notes */
	glBegin(GL_QUADS);
	// MM: ^ specifies the following vertices as groups of 4 to interpret as quadrilaterals
	glTexCoord2f(texMin[0],texMin[1]);  // MM: texture coords. 2f means two floats (for 2D)
	glVertex2i(0,0);                    // MM: vertex points. 2i means two ints (for 2D)
	glTexCoord2f(texMax[0],texMin[1]);
	glVertex2i(image.getSize(0),0);
	glTexCoord2f(texMax[0],texMax[1]);
	glVertex2i(image.getSize(0),image.getSize(1));
	glTexCoord2f(texMin[0],texMax[1]);
	glVertex2i(0,image.getSize(1));
	glEnd();
	// MM: ^ ends the listing of vertices
	
	/* Protect the texture object: */
	glBindTexture(GL_TEXTURE_2D,0);

	/*
	// Draw the image's backside:
	// MM: this is because ImageViewer allows you to tilt & move the image
	//     so it needs a blank "back" for if you flip the image over
	glDisable(GL_TEXTURE_2D);
	glMaterial(GLMaterialEnums::FRONT,GLMaterial(GLMaterial::Color(0.7f,0.7f,0.7f)));
	
	glBegin(GL_QUADS);
	glNormal3f(0.0f,0.0f,-1.0f);
	glVertex2i(0,0);
	glVertex2i(0,image.getSize(1));
	glVertex2i(image.getSize(0),image.getSize(1));
	glVertex2i(image.getSize(0),0);
	glEnd();
	*/	
	/* Restore OpenGL state: */
	glPopAttrib();
	}

void ImageViewer::resetNavigation(void) {
  /* Access the image: */
  const Images::BaseImage& image=textures.getTexture(0U).getImage();
	
  /* Reset the Vrui navigation transformation: */
  Vrui::Scalar w=Vrui::Scalar(image.getSize(0));
  Vrui::Scalar h=Vrui::Scalar(image.getSize(1));
  Vrui::Point center(Math::div2(w),Math::div2(h),Vrui::Scalar(0.01));
  Vrui::Scalar size=Math::sqrt(Math::sqr(w)+Math::sqr(h));
  Vrui::setNavigationTransformation(center,size,Vrui::Vector(0,1,0));
}

