/***********************************************************************
DepthImageRenderer - Class to centralize storage of raw or filtered
depth images on the GPU, and perform simple repetitive rendering tasks
such as rendering elevation values into a frame buffer.
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

/* MM: is this for storing the image of the sandbox from which depth
       is calculated? */

#include "DepthImageRenderer.h"

#include <GL/gl.h>
#include <GL/GLVertexArrayParts.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBMultitexture.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBVertexBufferObject.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/GLTransformationWrappers.h>
#include <iostream> // MM: added

#include "ShaderHelper.h"

/*********************************************
Methods of class DepthImageRenderer::DataItem:
*********************************************/

DepthImageRenderer::DataItem::DataItem(void)
	:vertexBuffer(0),indexBuffer(0),
	 depthTexture(0),depthTextureVersion(0),
	 depthShader(0),elevationShader(0)
	{
	std::cout << "In DepthImageRenderer::DataItem::DataItem." << std::endl; // MM: added
	/* Initialize all required extensions: */
	GLARBFragmentShader::initExtension();
	GLARBMultitexture::initExtension();
	GLARBShaderObjects::initExtension();
	GLARBTextureFloat::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureRg::initExtension();
	GLARBVertexBufferObject::initExtension();
	GLARBVertexShader::initExtension();
	
	/* Allocate the buffers and textures: */
	glGenBuffersARB(1,&vertexBuffer);
	glGenBuffersARB(1,&indexBuffer);
	glGenTextures(1,&depthTexture);
	// MM: generates the specified number of texture objects and places their 
	// handles in the GLuint array pointer (the second parameter)
	std::cout << "Done with DepthImageRenderer::DataItem::DataItem." << std::endl; // MM: added
	}

DepthImageRenderer::DataItem::~DataItem(void)
	{
	/* Release all allocated buffers, textures, and shaders: */
	glDeleteBuffersARB(1,&vertexBuffer);
	glDeleteBuffersARB(1,&indexBuffer);
	glDeleteTextures(1,&depthTexture);
	glDeleteObjectARB(depthShader);
	glDeleteObjectARB(elevationShader);
	}

/***********************************
Methods of class DepthImageRenderer:
***********************************/

DepthImageRenderer::DepthImageRenderer(const unsigned int sDepthImageSize[2])
	:depthImageVersion(0)
	{
	std::cout << "In DepthImageRenderer::DepthImageRenderer." << std::endl; // MM: added
	/* Copy the depth image size: */
	for(int i=0;i<2;++i)
		depthImageSize[i]=sDepthImageSize[i];
	
	/* Initialize the depth image: */
	depthImage=Kinect::FrameBuffer(depthImageSize[0],depthImageSize[1],depthImageSize[1]*depthImageSize[0]*sizeof(float));
	float* diPtr=depthImage.getData<float>();
	for(unsigned int y=0;y<depthImageSize[1];++y)
		for(unsigned int x=0;x<depthImageSize[0];++x,++diPtr)
			*diPtr=0.0f;
	++depthImageVersion;
	std::cout << "Done with DepthImageRenderer::DepthImageRenderer." << std::endl; // MM: added
	}

void DepthImageRenderer::initContext(GLContextData& contextData) const
	{
	std::cout << "In DepthImageRenderer::initContext." << std::endl; // MM: added
	/* Create a data item and add it to the context: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Upload the grid of template vertices into the vertex buffer: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB,depthImageSize[1]*depthImageSize[0]*sizeof(Vertex),0,GL_STATIC_DRAW_ARB);
	Vertex* vPtr=static_cast<Vertex*>(glMapBufferARB(GL_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	for(unsigned int y=0;y<depthImageSize[1];++y)
		for(unsigned int x=0;x<depthImageSize[0];++x,++vPtr)
			{
			/* Set the template vertex' position to the pixel center's position: */
			vPtr->position[0]=GLfloat(x)+0.5f;
			vPtr->position[1]=GLfloat(y)+0.5f;
			}
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	
	/* Upload the surface's triangle indices into the index buffer: */
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,(depthImageSize[1]-1)*depthImageSize[0]*2*sizeof(GLuint),0,GL_STATIC_DRAW_ARB);
	GLuint* iPtr=static_cast<GLuint*>(glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	for(unsigned int y=1;y<depthImageSize[1];++y)
		for(unsigned int x=0;x<depthImageSize[0];++x,iPtr+=2)
			{
			iPtr[0]=GLuint(y*depthImageSize[0]+x);
			iPtr[1]=GLuint((y-1)*depthImageSize[0]+x);
			}
	glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	
	/* Initialize the depth image texture: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture); // MM: texture target and texture name
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_LUMINANCE32F_ARB,depthImageSize[0],depthImageSize[1],0,GL_LUMINANCE,GL_FLOAT,0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0); // MM: texture target and texture name
	
	/* Create the depth rendering shader: */
	dataItem->depthShader=linkVertexAndFragmentShader("SurfaceDepthShader");
	dataItem->depthShaderUniforms[0]=glGetUniformLocationARB(dataItem->depthShader,"depthSampler");
	dataItem->depthShaderUniforms[1]=glGetUniformLocationARB(dataItem->depthShader,"projectionModelviewDepthProjection");
	
	/* Create the elevation rendering shader: */
	dataItem->elevationShader=linkVertexAndFragmentShader("SurfaceElevationShader");
	dataItem->elevationShaderUniforms[0]=glGetUniformLocationARB(dataItem->elevationShader,"depthSampler");
	dataItem->elevationShaderUniforms[1]=glGetUniformLocationARB(dataItem->elevationShader,"basePlaneDic");
	dataItem->elevationShaderUniforms[2]=glGetUniformLocationARB(dataItem->elevationShader,"weightDic");
	dataItem->elevationShaderUniforms[3]=glGetUniformLocationARB(dataItem->elevationShader,"projectionModelviewDepthProjection");
	std::cout << "Done with DepthImageRenderer::initContext." << std::endl; // MM: added
	}

void DepthImageRenderer::setDepthProjection(const PTransform& newDepthProjection)
	{
	std::cout << "In DepthImageRenderer::setDepthProjection." << std::endl; // MM: added
	/* Set the depth unprojection matrix: */
	depthProjection=newDepthProjection;
	
	/* Convert the depth projection matrix to column-major OpenGL format: */
	GLfloat* dpmPtr=depthProjectionMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++dpmPtr)
			*dpmPtr=GLfloat(depthProjection.getMatrix()(i,j));
	
	/* Create the weight calculation equation: */
	for(int i=0;i<4;++i)
		weightDicEq[i]=GLfloat(depthProjection.getMatrix()(3,i));
	
	/* Recalculate the base plane equation in depth image space: */
	setBasePlane(basePlane);
	std::cout << "Done with DepthImageRenderer::setDepthProjection." << std::endl; // MM: added
	}

void DepthImageRenderer::setBasePlane(const Plane& newBasePlane)
	{
	std::cout << "In DepthImageRenderer::setBasePlane." << std::endl; // MM: added
	/* Set the base plane: */
	basePlane=newBasePlane;
	
	/* Transform the base plane to depth image space and into a GLSL-compatible format: */
	const PTransform::Matrix& dpm=depthProjection.getMatrix();
	const Plane::Vector& bpn=basePlane.getNormal();
	Scalar bpo=basePlane.getOffset();
	for(int i=0;i<4;++i)
		basePlaneDicEq[i]=GLfloat(dpm(0,i)*bpn[0]+dpm(1,i)*bpn[1]+dpm(2,i)*bpn[2]-dpm(3,i)*bpo);
	std::cout << "Done with DepthImageRenderer::setBasePlane." << std::endl; // MM: added
	}

void DepthImageRenderer::setDepthImage(const Kinect::FrameBuffer& newDepthImage)
	{
	std::cout << "In DepthImageRenderer::setDepthImage." << std::endl; // MM: added
	/* Update the depth image: */
	depthImage=newDepthImage;
	++depthImageVersion;
	std::cout << "Done with DepthImageRenderer::setDepthImage." << std::endl; // MM: added
	}

Scalar DepthImageRenderer::intersectLine(const Point& p0,const Point& p1,Scalar elevationMin,Scalar elevationMax) const
	{
	std::cout << "In DepthImageRenderer::intersectLine." << std::endl; // MM: added
	/* Initialize the line segment: */
	//Scalar lambda0=Scalar(0);
	//Scalar lambda1=Scalar(1);
	
	/* Intersect the line segment with the upper elevation plane: */
	Scalar d0=basePlane.calcDistance(p0);
	Scalar d1=basePlane.calcDistance(p1);
	if(d0*d1<Scalar(0))
		{
		/* Calculate the intersection parameter: */
		
		// IMPLEMENT ME!
		
		return Scalar(2);
		}
	else if(d1>Scalar(0))
		{
		/* Trivially reject with maximum intercept: */
		return Scalar(2);
		}
	
	std::cout << "Done with DepthImageRenderer::intersectLine." << std::endl; // MM: added
	return Scalar(2);
	}

void DepthImageRenderer::uploadDepthProjection(GLint location) const
	{
	std::cout << "In DepthImageRenderer::uploadDepthProjection." << std::endl; // MM: added
	/* Upload the matrix to OpenGL: */
	glUniformMatrix4fvARB(location,1,GL_FALSE,depthProjectionMatrix);
	std::cout << "Done with DepthImageRenderer::uploadDepthProjection." << std::endl; // MM: added
	}

void DepthImageRenderer::bindDepthTexture(GLContextData& contextData) const
	{
	std::cout << "In DepthImageRenderer::bindDepthTexture." << std::endl; // MM: added
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the depth image texture: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture); // MM: texture target and texture name
	
	/* Check if the texture is outdated: */
	if(dataItem->depthTextureVersion!=depthImageVersion)
		{
		/* Upload the new depth texture: */
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,depthImageSize[0],depthImageSize[1],GL_LUMINANCE,GL_FLOAT,depthImage.getData<GLfloat>());
		
		/* Mark the depth texture as current: */
		dataItem->depthTextureVersion=depthImageVersion;
		}
	std::cout << "Done with DepthImageRenderer::bindDepthTexture." << std::endl; // MM: added
	}

// MM: I think this does the actual drawing/projecting of the image. not sure
//     (what counts as surface template? the entire color map?)
void DepthImageRenderer::renderSurfaceTemplate(GLContextData& contextData) const
	{
	std::cout << "In DepthImageRenderer::renderSurfaceTemplate." << std::endl; // MM: added
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer); // MM: target and buffer (unsigned int)
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Draw the surface template: */
	GLVertexArrayParts::enable(Vertex::getPartsMask()); 
	// MM: ^ might be doing glEnableClientState(GL_COLOR_ARRAY) ?
	//     Of the enable options in GL/GLVertexArrayParts.h, COLOR_ARRAY seems to make most sense
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<depthImageSize[1];++y,indexPtr+=depthImageSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,depthImageSize[0]*2,GL_UNSIGNED_INT,indexPtr); 
		// MM: ^ renders multiple primitives from array
		//     does this function draw the active texture or something? 
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Unbind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);         // MM: entering 0 is like binding to a null buffer
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	std::cout << "Done with DepthImageRenderer::renderSurfaceTemplate." << std::endl; // MM: added
	}

void DepthImageRenderer::renderDepth(const PTransform& projectionModelview,GLContextData& contextData) const
	{
	std::cout << "In DepthImageRenderer::renderDepth." << std::endl; // MM: added
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the depth rendering shader: */
	glUseProgramObjectARB(dataItem->depthShader);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Bind the depth image texture: */
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture); // MM: texture target and texture name
	
	/* Check if the texture is outdated: */
	if(dataItem->depthTextureVersion!=depthImageVersion)
		{
		/* Upload the new depth texture: */
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,depthImageSize[0],depthImageSize[1],GL_LUMINANCE,GL_FLOAT,depthImage.getData<GLfloat>());
		
		/* Mark the depth texture as current: */
		dataItem->depthTextureVersion=depthImageVersion;
		}
	glUniform1iARB(dataItem->depthShaderUniforms[0],0); // Tell the shader that the depth texture is in texture unit 0
	
	/* Upload the combined projection, modelview, and depth projection matrix: */
	PTransform pmvdp=projectionModelview;
	pmvdp*=depthProjection;
	glUniformARB(dataItem->depthShaderUniforms[1],pmvdp);
	
	/* Draw the surface: */
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<depthImageSize[1];++y,indexPtr+=depthImageSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,depthImageSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Unbind all textures and buffers: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0); // MM: texture target and texture name
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	
	/* Unbind the depth rendering shader: */
	glUseProgramObjectARB(0);
	std::cout << "Done with DepthImageRenderer::renderDepth." << std::endl; // MM: added
	}

void DepthImageRenderer::renderElevation(const PTransform& projectionModelview,GLContextData& contextData) const
	{
	std::cout << "In DepthImageRenderer::renderElevation." << std::endl; // MM: added
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the elevation rendering shader: */
	glUseProgramObjectARB(dataItem->elevationShader);
	
	/* Set up the depth image texture: */
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture); // MM: texture target and texture name
	
	/* Check if the texture is outdated: */
	if(dataItem->depthTextureVersion!=depthImageVersion)
		{
		/* Upload the new depth texture: */
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,depthImageSize[0],depthImageSize[1],GL_LUMINANCE,GL_FLOAT,depthImage.getData<GLfloat>());
		
		/* Mark the depth texture as current: */
		dataItem->depthTextureVersion=depthImageVersion;
		}
	glUniform1iARB(dataItem->elevationShaderUniforms[0],0); // Tell the shader that the depth texture is in texture unit 0
	
	/* Upload the base plane equation in depth image space: */
	glUniformARB<4>(dataItem->elevationShaderUniforms[1],1,basePlaneDicEq);
	
	/* Upload the base weight equation in depth image space: */
	glUniformARB<4>(dataItem->elevationShaderUniforms[2],1,weightDicEq);
	
	/* Upload the combined projection, modelview, and depth projection matrix: */
	PTransform pmvdp=projectionModelview;
	pmvdp*=depthProjection;
	glUniformARB(dataItem->elevationShaderUniforms[3],pmvdp);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Draw the surface: */
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<depthImageSize[1];++y,indexPtr+=depthImageSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,depthImageSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Unbind all textures and buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0); // MM: texture target and texture name
	
	/* Unbind the elevation rendering shader: */
	glUseProgramObjectARB(0);
	std::cout << "Done with DepthImageRenderer::renderElevation." << std::endl; // MM: added
	}
