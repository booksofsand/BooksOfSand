 /***********************************************************************
DEM - Class to represent digital elevation models (DEMs) as float-valued
texture objects.
Copyright (c) 2013-2016 Oliver Kreylos

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

#include "DEM.h"

#include <IO/File.h>
#include <IO/OpenFile.h>
#include <GL/gl.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <Geometry/Matrix.h>
#include <iostream> // MM: added

/******************************
Methods of class DEM::DataItem:
******************************/

DEM::DataItem::DataItem(void)
	:textureObjectId(0)
	{
	std::cout << "In DEM::DataItem::DataItem." << std::endl;  // MM: added
	/* Check for and initialize all required OpenGL extensions: */
	GLARBTextureFloat::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureRg::initExtension();
	GLARBShaderObjects::initExtension();
	
	/* Create the texture object: */
	glGenTextures(1,&textureObjectId);
	// MM: generates the specified number of texture objects and places their 
	// handles in the GLuint array pointer (the second parameter)
	std::cout << "Done with DEM::DataItem::DataItem." << std::endl;  // MM: added
	}

DEM::DataItem::~DataItem(void)
	{
	/* Destroy the texture object: */
	glDeleteTextures(1,&textureObjectId);
	}

/********************
Methods of class DEM:
********************/

void DEM::calcMatrix(void)
	{
	std::cout << "In DEM::calcMatrix." << std::endl;  // MM: added
	/* Convert the DEM transformation into a projective transformation matrix: */
	demTransform=PTransform(transform);
	PTransform::Matrix& dtm=demTransform.getMatrix();
	
	/* Pre-multiply the projective transformation matrix with the DEM space to DEM pixel space transformation: */
	PTransform dem;
	dem.getMatrix()(0,0)=Scalar(demSize[0]-1)/(demBox[2]-demBox[0]);
	dem.getMatrix()(0,3)=Scalar(0.5)-Scalar(demSize[0]-1)/(demBox[2]-demBox[0])*demBox[0];
	dem.getMatrix()(1,1)=Scalar(demSize[1]-1)/(demBox[3]-demBox[1]);
	dem.getMatrix()(1,3)=Scalar(0.5)-Scalar(demSize[1]-1)/(demBox[3]-demBox[1])*demBox[1];
	dem.getMatrix()(2,2)=Scalar(1)/verticalScale;
	dem.getMatrix()(2,3)=verticalScaleBase-verticalScaleBase/verticalScale;
	demTransform.leftMultiply(dem);
	
	/* Convert the full transformation to column-major OpenGL format: */
	GLfloat* dtmPtr=demTransformMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++dtmPtr)
			*dtmPtr=GLfloat(dtm(i,j));
	std::cout << "Done with DEM::calcMatrix." << std::endl;  // MM: added
	}

DEM::DEM(void)
	:dem(0),
	 transform(OGTransform::identity),
	 verticalScale(1),verticalScaleBase(0)
	{
	std::cout << "In DEM::DEM." << std::endl;  // MM: added
	demSize[0]=demSize[1]=0;
	std::cout << "Done with DEM::DEM." << std::endl;  // MM: added
	}

DEM::~DEM(void)
	{
	delete[] dem;
	}

void DEM::initContext(GLContextData& contextData) const
	{
	std::cout << "In DEM::initContext." << std::endl;  // MM: added
	/* Create and register a data item: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Upload the DEM array into the texture object: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->textureObjectId); // MM: texture target and texture name
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_LUMINANCE32F_ARB,demSize[0],demSize[1],0,GL_LUMINANCE,GL_FLOAT,dem);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0); // MM: texture target and texture name
	std::cout << "Done with DEM::initContext." << std::endl;  // MM: added
	}

void DEM::load(const char* demFileName)
	{
	std::cout << "In DEM::load." << std::endl;  // MM: added
	/* Read the DEM file: */
	IO::FilePtr demFile=IO::openFile(demFileName);
	demFile->setEndianness(Misc::LittleEndian);
	demFile->read<int>(demSize,2);
	dem=new float[demSize[1]*demSize[0]];
	for(int i=0;i<4;++i)
		demBox[i]=double(demFile->read<float>());
	demFile->read<float>(dem,demSize[1]*demSize[0]);
	
	/* Update the DEM transformation: */
	calcMatrix();
	std::cout << "Done with DEM::load." << std::endl;  // MM: added
	}

float DEM::calcAverageElevation(void) const
	{
	std::cout << "In DEM::calcAverageElevation." << std::endl;  // MM: added
	/* Sum all elevation measurements: */
	double elevSum=0.0;
	const float* demPtr=dem;
	for(int i=demSize[1]*demSize[0];i>0;--i,++demPtr)
		elevSum+=double(*demPtr);
	
	/* Return the average elevation: */
	return float(elevSum/double(demSize[1]*demSize[0]));
	std::cout << "Done with DEM::calcAverageElevation." << std::endl;  // MM: added
	}

void DEM::setTransform(const OGTransform& newTransform,Scalar newVerticalScale,Scalar newVerticalScaleBase)
	{
	std::cout << "In DEM::setTransform." << std::endl;  // MM: added
	transform=newTransform;
	verticalScale=newVerticalScale;
	verticalScaleBase=newVerticalScaleBase;
	
	/* Update the DEM transformation: */
	calcMatrix();
	std::cout << "Done with DEM::setTransform." << std::endl;  // MM: added
	}

void DEM::bindTexture(GLContextData& contextData) const
	{
	std::cout << "In DEM::bindTexture." << std::endl;  // MM: added
	/* Get the context data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the DEM texture: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->textureObjectId); // MM: texture target and texture name
	std::cout << "Done with DEM::bindTexture." << std::endl;  // MM: added
	}

void DEM::uploadDemTransform(GLint location) const
	{
	std::cout << "In DEM::uploadDemTransform." << std::endl;  // MM: added
	/* Upload the matrix to OpenGL: */
	glUniformMatrix4fvARB(location,1,GL_FALSE,demTransformMatrix);
	std::cout << "Done with DEM::uploadDemTransform." << std::endl;  // MM: added
	}
