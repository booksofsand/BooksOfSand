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
  std::cout << "CREATING NEW IMAGE MAP!" << std::endl;
  load(imageName);
}

void ImageMap::initContext(GLContextData& contextData) const {
  // method from GLObject
  // code from ElevationColorMap.cpp
  
  // Initialize required OpenGL extensions
  // GLARBShaderObjects::initExtension();    // MM: don't think we need GL extensions for image

  // Create the data item and associate it with this object
  DataItem* dataItem=new DataItem;           // this DataItem struct is in GLTextureObject.h
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
  return; // MM: skipping
	
  // Open image file
  Images::TextureSet::Texture& tex=textures.addTexture(Images::readImageFile(imageName,
									     Vrui::openFile(imageName)),
						       GL_TEXTURE_2D,
						       GL_RGB8,
						       0U);
	
  // Set clamping and filtering parameters for mip-mapped linear interpolation
  tex.setMipmapRange(0,1000);
  tex.setWrapModes(GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
  tex.setFilterModes(GL_LINEAR_MIPMAP_LINEAR,GL_LINEAR);
  
  // Invalidate the color map texture object
  ++textureVersion;
}

void ImageMap::calcTexturePlane(const Plane& basePlane) {
  // Calculates the texture mapping plane for the given base plane equation

  // MM: code from ElevationColorMap.cpp
  // TO DO: look at GLColorMap to see what this does. need to adapt?

  /*  
  // Scale and offset the camera-space base plane equation
  const Plane::Vector& bpn=basePlane.getNormal();
  Scalar bpo=basePlane.getOffset();
  Scalar hms=Scalar(getNumEntries()-1)/((getScalarRangeMax()-getScalarRangeMin())*Scalar(getNumEntries()));
  Scalar hmo=Scalar(0.5)/Scalar(getNumEntries())-hms*getScalarRangeMin();

  // MM: texturePlaneEq was declared in .h: GLfloat texturePlaneEq[4];
  for(int i=0;i<3;++i)
    texturePlaneEq[i]=GLfloat(bpn[i]*hms);
  texturePlaneEq[3]=GLfloat(-bpo*hms+hmo);
  */
}

void ImageMap::calcTexturePlane(const DepthImageRenderer* depthImageRenderer) {
  // Calculate texture plane based on the given depth image renderer's base plane
  // MM: code from ElevationColorMap.cpp
  calcTexturePlane(depthImageRenderer->getBasePlane());
}

void ImageMap::bindTexture(GLContextData& contextData) const {
  // Binds the image map texture object to the currently active texture unit
  std::cout << "In ImageMap::bindTexture." << std::endl; // MM: added

  // Retrieve the data item
  //DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);  // from ElevationColorMap
  //std::cout << "Retrieved DataItem." << std::endl;
  
  // Bind the texture object
  //glBindTexture(GL_TEXTURE_2D,dataItem->textureObjectId); // from ElevationColorMap. MM: changed to 2D
  //glBindTexture(GL_TEXTURE_1D,dataItem->textureObjectId); // from ElevationColorMap
  //std::cout << "glBindTexture(GL_TEXTURE_1D,dataItem->textureObjectId); done." << std::endl;
  
  
  
  // Code from ImageViewer.cpp:
  // Set up OpenGL state
  glPushAttrib(GL_ENABLE_BIT);
  glEnable(GL_TEXTURE_2D);
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
  std::cout << "glPushAttrib, glEnable, glTexEnvi done." << std::endl;
  
  // Get the texture set's GL state
  // MM: getGLState returns OpenGL texture state object for the given OpenGL context
  Images::TextureSet::GLState* texGLState=textures.getGLState(contextData);
  std::cout << "Got GLState." << std::endl;  // MM: added
  
  // Bind the texture object
  // MM: bindTexture binds the texture object associated with the given key
  //     to its texture target on the current texture unit, returns texture state.
  //     Note the key is 0U, which was the key in ImageViewer constructor.
  const Images::TextureSet::GLState::Texture& tex=texGLState->bindTexture(0U);
  const Images::BaseImage& image=tex.getImage();
  // MM: try replacing this with Image so can test the setPixel() method (TO DO)
  std::cout << "texGLState->bindTexture and tex.getImage() done." << std::endl;  // MM: added
  
  // Query the range of texture coordinates
  const GLfloat* texMin=tex.getTexCoordMin();
  const GLfloat* texMax=tex.getTexCoordMax();
  // MM: TO DO: determine if these values are accurate (should they match sandbox dimensions?)
  std::cout << "tex.getTexCoordMax() and Min() done." << std::endl;  // MM: added
  
  // Draw the image
  // MM: Note: texture coordinates specify the point in the texture image that will 
  // correspond to the vertex you're specifying them for. see Vrui and OpenGL notes
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

  //Protect the texture object
  //glBindTexture(GL_TEXTURE_2D,0); // MM: commented out bc already do this in SurfaceRenderer?
  // Restore OpenGL state
  glPopAttrib();
  std::cout << "Done with drawing!" << std::endl;
  
  
  
  
  
  // MM: code from ElevationColorMap.cpp
  // TO DO: adapt for image
  /*
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
  */
  std::cout << "Done with ImageMap::bindTexture." << std::endl; // MM: added
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
