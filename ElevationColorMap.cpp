/***********************************************************************
ElevationColorMap - Class to represent elevation color maps for
topographic maps.
Copyright (c) 2014-2016 Oliver Kreylos

This file is part of the Augmented Reality Sandbox (SARndbox).

The Augmented Reality Sandbox is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Augmented Reality Sandbox is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

// MM: this extends GLColorMap to manage transforming depth map values
//     to color values

#include "ElevationColorMap.h"

#include <string>
#include <Misc/ThrowStdErr.h>
#include <Misc/FileNameExtensions.h>
#include <IO/ValueSource.h>
#include <GL/gl.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <Vrui/OpenFile.h>
#include <iostream> // MM: added

#include "Types.h"
#include "DepthImageRenderer.h"

#include "Config.h"

/**********************************
Methods of class ElevationColorMap:
**********************************/

ElevationColorMap::ElevationColorMap(const char* heightMapName)
{
  std::cout << "In ElevationColorMap::ElevationColorMap." << std::endl; // MM: added
  /* Load the given height map: */
  load(heightMapName);
  std::cout << "Done with ElevationColorMap::ElevationColorMap." << std::endl; // MM: added
}

void ElevationColorMap::initContext(GLContextData& contextData) const
{
  std::cout << "In ElevationColorMap::initContext." << std::endl; // MM: added
  /* Initialize required OpenGL extensions: */
  GLARBShaderObjects::initExtension();
	
  /* Create the data item and associate it with this object: */
  DataItem* dataItem=new DataItem;
  contextData.addDataItem(this,dataItem);
  std::cout << "Done with ElevationColorMap::initContext." << std::endl; // MM: added
}

void ElevationColorMap::load(const char* heightMapName)
{
  std::cout << "In ElevationColorMap::load." << std::endl; // MM: added
  /* Open the height map file: */
  std::string fullHeightMapName;
  if(heightMapName[0]=='/')
    {
      /* Use the absolute file name directly: */
      fullHeightMapName=heightMapName;
    }
  else
    {
      /* Assemble a file name relative to the configuration file directory: */
      fullHeightMapName=CONFIG_CONFIGDIR;
      fullHeightMapName.push_back('/');
      fullHeightMapName.append(heightMapName);
    }
	
  /* Open the height map file: */
  IO::ValueSource heightMapSource(Vrui::openFile(fullHeightMapName.c_str()));
  /* MM: here, the height map already exists, and it's opened as a standard file
     using some Vrui functions. below we see that the height map file must
     contain a bunch of numbers called key values and color values, with
     one key value followed by 3 color values (probably RGB). these are
     loaded into two different arrays, then passed by reference in the end
     to setColors, which is in Vrui/GL/GLColorMap.cpp. 

     the default file with these values on a machine with SARndbox
     installed is at /usr/local/etc/HeightColorMap.cpt. it is only 
     a dozen lines or so, each with four values (must be the depth
     and 3 color values). these must represent the different depth/
     elevation ranges and their colors. */
	
  /* Load the height color map: */
  std::vector<Color> heightMapColors;
  std::vector<GLdouble> heightMapKeys;
  std::cout << "heightMapName: " << heightMapName << std::endl; //MM: added
  if(Misc::hasCaseExtension(heightMapName,".cpt"))
    {
      heightMapSource.setPunctuation("\n");
      heightMapSource.skipWs();
      int line=1;
      while(!heightMapSource.eof())
	{
	  /* Read the next color map key value: */
	  heightMapKeys.push_back(GLdouble(heightMapSource.readNumber()));
			
			
	  /* Read the next color map color value: */
	  Color color;
	  for(int i=0;i<3;++i)
	    color[i]=Color::Scalar(heightMapSource.readNumber()/255.0);
	  color[3]=Color::Scalar(1);
	  heightMapColors.push_back(color);
			
	  if(!heightMapSource.isLiteral('\n'))
	    Misc::throwStdErr("ElevationColorMap: Color map format error in line %d of file %s",line,fullHeightMapName.c_str());
	  ++line;
	}
    }
  else
    {
      heightMapSource.setPunctuation(",\n");
      heightMapSource.skipWs();
      int line=1;
      while(!heightMapSource.eof())
	{
	  /* Read the next color map key value: */
	  heightMapKeys.push_back(GLdouble(heightMapSource.readNumber()));
	  if(!heightMapSource.isLiteral(','))
	    Misc::throwStdErr("ElevationColorMap: Color map format error in line %d of file %s",line,fullHeightMapName.c_str());
			
	  /* Read the next color map color value: */
	  Color color;
	  for(int i=0;i<3;++i)
	    color[i]=Color::Scalar(heightMapSource.readNumber());
	  color[3]=Color::Scalar(1);
	  heightMapColors.push_back(color);
			
	  if(!heightMapSource.isLiteral('\n'))
	    Misc::throwStdErr("ElevationColorMap: Color map format error in line %d of file %s",line,fullHeightMapName.c_str());
	  ++line;
	}
    }
	
  /* Create the color map: */
  /* MM: heightMapKeys contains height values as keys, which are then 
     mapped to the colors that were calculated above.

     setColors is in Vrui/GL/GLColorMap.cpp; it creates a color map 
     from a piecewise linear color function */
  std::cout << "heightMapKeys.size(): " << heightMapKeys.size() << std::endl; // MM: added
  setColors(heightMapKeys.size(),&heightMapColors[0],&heightMapKeys[0],256);
	
  /* Invalidate the color map texture object: */
  // MM: I'm guessing this is so the surface renderer knows to update its display
  ++textureVersion;
  std::cout << "Done with ElevationColorMap::load." << std::endl; // MM: added
}

void ElevationColorMap::calcTexturePlane(const Plane& basePlane)
{
  std::cout << "In ElevationColorMap::calcTexturePlane(const Plane& basePlane)." << std::endl; // MM: added
  /* Scale and offset the camera-space base plane equation: */
  const Plane::Vector& bpn=basePlane.getNormal();
  Scalar bpo=basePlane.getOffset();
  Scalar hms=Scalar(getNumEntries()-1)/((getScalarRangeMax()-getScalarRangeMin())*Scalar(getNumEntries()));
  Scalar hmo=Scalar(0.5)/Scalar(getNumEntries())-hms*getScalarRangeMin();
  for(int i=0;i<3;++i)
    texturePlaneEq[i]=GLfloat(bpn[i]*hms);
  texturePlaneEq[3]=GLfloat(-bpo*hms+hmo);
  std::cout << "Done with ElevationColorMap::calcTexturePlane." << std::endl; // MM: added
}

void ElevationColorMap::calcTexturePlane(const DepthImageRenderer* depthImageRenderer)
{
  std::cout << "In ElevationColorMap::calcTexturePlane(const DepthImageRenderer* depthImageRenderer)." << std::endl; // MM: added
  /* Calculate texture plane based on the given depth image renderer's base plane: */
  calcTexturePlane(depthImageRenderer->getBasePlane());
  std::cout << "Done with ElevationColorMap::calcTexturePlane." << std::endl; // MM: added
}

void ElevationColorMap::bindTexture(GLContextData& contextData) const
{
  std::cout << "In ElevationColorMap::bindTexture." << std::endl; // MM: added
  /* Retrieve the data item: */
  DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
  /* Bind the texture object: */
  glBindTexture(GL_TEXTURE_1D,dataItem->textureObjectId);
	
  /* Check if the color map texture is outdated: */
  if(dataItem->textureObjectVersion!=textureVersion)
    {
      /* Upload the color map entries as a 1D texture: */
      glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
      glTexImage1D(GL_TEXTURE_1D,0,GL_RGB8,getNumEntries(),0,GL_RGBA,GL_FLOAT,getColors());
		
      dataItem->textureObjectVersion=textureVersion;
    }
  std::cout << "Done with ElevationColorMap::bindTexture." << std::endl; // MM: added
}

void ElevationColorMap::uploadTexturePlane(GLint location) const
{
  std::cout << "In ElevationColorMap::uploadTexturePlane." << std::endl; // MM: added
  /* Upload the texture mapping plane equation: */
  glUniformARB<4>(location,1,texturePlaneEq);
  std::cout << "Done with ElevationColorMap::uploadTexturePlane." << std::endl; // MM: added
}
