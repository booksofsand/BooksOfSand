/***********************************************************************
SurfaceRenderer - Class to render a surface defined by a regular grid in
depth image space.
Copyright (c) 2012-2016 Oliver Kreylos

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

#include "SurfaceRenderer.h"

#include <string>
#include <vector>
#include <Misc/PrintInteger.h>
#include <Misc/ThrowStdErr.h>
#include <Misc/MessageLogger.h>
#include <Misc/FunctionCalls.h>  //MM: added bc error "createFunctionCall is not a member of Misc"
#include <GL/gl.h>
#include <GL/GLVertexArrayParts.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBMultitexture.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/Extensions/GLEXTFramebufferObject.h>
#include <GL/GLLightTracker.h>
#include <GL/GLContextData.h>
#include <GL/GLTransformationWrappers.h>
#include <GL/GLGeometryVertex.h>

// MM: added
#include <Math/Math.h>
#include <GL/GLColorTemplates.h>
#include <GL/GLMaterial.h>
#include <GL/GLObject.h>
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
// MM: ^ added

#include "DepthImageRenderer.h"
//#include "ElevationColorMap.h"  // MM: commented out
#include "DEM.h"
#include "ShaderHelper.h"
#include "Config.h"

/******************************************
Methods of class SurfaceRenderer::DataItem:
******************************************/

SurfaceRenderer::DataItem::DataItem(void)
	:contourLineFramebufferObject(0),contourLineDepthBufferObject(0),contourLineColorTextureObject(0),contourLineVersion(0),
	 heightMapShader(0),surfaceSettingsVersion(0),lightTrackerVersion(0),
	 globalAmbientHeightMapShader(0),shadowedIlluminatedHeightMapShader(0)
	{
	std::cout << "In SurfaceRenderer::DataItem::DataItem." << std::endl;  // MM: added
	/* Initialize all required extensions: */
	GLARBFragmentShader::initExtension();
	GLARBMultitexture::initExtension();
	GLARBShaderObjects::initExtension();
	GLARBTextureFloat::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureRg::initExtension();
	GLARBVertexShader::initExtension();
	GLEXTFramebufferObject::initExtension();
	std::cout << "Done with SurfaceRenderer::DataItem::DataItem." << std::endl;  // MM: added
	}

SurfaceRenderer::DataItem::~DataItem(void)
	{
	/* Release all allocated buffers, textures, and shaders: */
	glDeleteFramebuffersEXT(1,&contourLineFramebufferObject);
	glDeleteRenderbuffersEXT(1,&contourLineDepthBufferObject);
	glDeleteTextures(1,&contourLineColorTextureObject);
	glDeleteObjectARB(heightMapShader);
	glDeleteObjectARB(globalAmbientHeightMapShader);
	glDeleteObjectARB(shadowedIlluminatedHeightMapShader);
	}

/********************************
Methods of class SurfaceRenderer:
********************************/

void SurfaceRenderer::shaderSourceFileChanged(const IO::FileMonitor::Event& event)
	{
	/* Invalidate the single-pass surface shader: */
	++surfaceSettingsVersion;
	}

GLhandleARB SurfaceRenderer::createSinglePassSurfaceShader(const GLLightTracker& lt,GLint* uniformLocations) const
	{
	std::cout << "In SurfaceRenderer::createSinglePassSurfaceShader." << std::endl;  // MM: added
	GLhandleARB result=0;
	
	std::vector<GLhandleARB> shaders;
	try
		{
		/*********************************************************************
		Assemble and compile the surface rendering vertex shader:
		*********************************************************************/
		
		/* Assemble the function and declaration strings: */
		std::string vertexFunctions="\
			#extension GL_ARB_texture_rectangle : enable\n";
		
		std::string vertexUniforms="\
			uniform sampler2DRect depthSampler; // Sampler for the depth image-space elevation texture\n\
			uniform mat4 depthProjection; // Transformation from depth image space to camera space\n\
			uniform mat4 projectionModelviewDepthProjection; // Transformation from depth image space to clip space\n";
		
		std::string vertexVaryings;
		
		/* Assemble the vertex shader's main function: */
		std::string vertexMain="\
			void main()\n\
				{\n\
				/* Get the vertex' depth image-space z coordinate from the texture: */\n\
				vec4 vertexDic=gl_Vertex;\n\
				vertexDic.z=texture2DRect(depthSampler,gl_Vertex.xy).r;\n\
				\n\
				/* Transform the vertex from depth image space to camera space and normalize it: */\n\
				vec4 vertexCc=depthProjection*vertexDic;\n\
				vertexCc/=vertexCc.w;\n\
				\n";
		
		if(dem!=0)
			{
			/* Add declarations for DEM matching: */
			vertexUniforms+="\
				uniform mat4 demTransform; // Transformation from camera space to DEM space\n\
				uniform sampler2DRect demSampler; // Sampler for the DEM texture\n\
				uniform float demDistScale; // Distance from surface to DEM at which the color map saturates\n";
			
			vertexVaryings+="\
				varying float demDist; // Scaled signed distance from surface to DEM\n";
			
			/* Add DEM matching code to vertex shader's main function: */
			vertexMain+="\
				/* Transform the camera-space vertex to scaled DEM space: */\n\
				vec4 vertexDem=demTransform*vertexCc;\n\
				\n\
				/* Calculate scaled DEM-surface distance: */\n\
				demDist=(vertexDem.z-texture2DRect(demSampler,vertexDem.xy).r)*demDistScale;\n\
				\n";
			}
		/* MM: commented out, replaced with ImageMap info
		else if(elevationColorMap!=0)
			{
			// Add declarations for height mapping: 
			vertexUniforms+="\
				uniform vec4 heightColorMapPlaneEq; // Plane equation of the base plane in camera space, scaled for height map textures\n";
			
			vertexVaryings+="\
				varying float heightColorMapTexCoord; // Texture coordinate for the height color map\n";
			
			// Add height mapping code to vertex shader's main function: 
			vertexMain+="\
				// Plug camera-space vertex into the scaled and offset base plane equation: \n\
				heightColorMapTexCoord=dot(heightColorMapPlaneEq,vertexCc);\n\
				\n";
			}*/
		// MM: is this elevation map specific or just general height map stuff? can leave as is?
		//     (besides changing names from heightColorMap to ImageMap)
		else if(imageMap!=0)
			{
			// Add declarations for height mapping: 
			vertexUniforms+="\
				uniform vec4 imageMapPlaneEq; // Plane equation of the base plane in camera space, scaled for height map textures\n";
			
			vertexVaryings+="\
				varying float imageMapTexCoord; // Texture coordinate for the image map\n";
			
			// Add height mapping code to vertex shader's main function: 
			vertexMain+="\
				// Plug camera-space vertex into the scaled and offset base plane equation: \n\
				imageMapTexCoord=dot(imageMapPlaneEq,vertexCc);\n\
				\n";
			}
		
		if(illuminate)
			{
			/* Add declarations for illumination: */
			vertexUniforms+="\
				uniform mat4 modelview; // Transformation from camera space to eye space\n\
				uniform mat4 tangentModelviewDepthProjection; // Transformation from depth image space to eye space for tangent planes\n";
			
			vertexVaryings+="\
				varying vec4 diffColor,specColor; // Diffuse and specular colors, interpolated separately for correct highlights\n";
			
			/* Add illumination code to vertex shader's main function: */
			vertexMain+="\
				/* Calculate the vertex' tangent plane equation in depth image space: */\n\
				vec4 tangentDic;\n\
				tangentDic.x=texture2DRect(depthSampler,vec2(vertexDic.x-1.0,vertexDic.y)).r-texture2DRect(depthSampler,vec2(vertexDic.x+1.0,vertexDic.y)).r;\n\
				tangentDic.y=texture2DRect(depthSampler,vec2(vertexDic.x,vertexDic.y-1.0)).r-texture2DRect(depthSampler,vec2(vertexDic.x,vertexDic.y+1.0)).r;\n\
				tangentDic.z=2.0;\n\
				tangentDic.w=-dot(vertexDic.xyz,tangentDic.xyz)/vertexDic.w;\n\
				\n\
				/* Transform the vertex and its tangent plane from depth image space to eye space: */\n\
				vec4 vertexEc=modelview*vertexCc;\n\
				vec3 normalEc=normalize((tangentModelviewDepthProjection*tangentDic).xyz);\n\
				\n\
				/* Initialize the color accumulators: */\n\
				diffColor=gl_LightModel.ambient*gl_FrontMaterial.ambient;\n\
				specColor=vec4(0.0,0.0,0.0,0.0);\n\
				\n";
			
			/* Call the appropriate light accumulation function for every enabled light source: */
			bool firstLight=true;
			for(int lightIndex=0;lightIndex<lt.getMaxNumLights();++lightIndex)
				if(lt.getLightState(lightIndex).isEnabled())
					{
					/* Create the light accumulation function: */
					vertexFunctions.push_back('\n');
					vertexFunctions+=lt.createAccumulateLightFunction(lightIndex);
					
					if(firstLight)
						{
						vertexMain+="\
							/* Call the light accumulation functions for all enabled light sources: */\n";
						firstLight=false;
						}
					
					/* Call the light accumulation function from vertex shader's main function: */
					vertexMain+="\
						accumulateLight";
					char liBuffer[12];
					vertexMain.append(Misc::print(lightIndex,liBuffer+11));
					vertexMain+="(vertexEc,normalEc,gl_FrontMaterial.ambient,gl_FrontMaterial.diffuse,gl_FrontMaterial.specular,gl_FrontMaterial.shininess,diffColor,specColor);\n";
					}
			if(!firstLight)
				vertexMain+="\
					\n";
			}
				
		/* Finish the vertex shader's main function: */
		vertexMain+="\
				/* Transform vertex from depth image space to clip space: */\n\
				gl_Position=projectionModelviewDepthProjection*vertexDic;\n\
				}\n";
		
		/* Compile the vertex shader: */
		shaders.push_back(glCompileVertexShaderFromStrings(7,vertexFunctions.c_str(),"\t\t\n",vertexUniforms.c_str(),"\t\t\n",vertexVaryings.c_str(),"\t\t\n",vertexMain.c_str()));
		
		/*********************************************************************
		Assemble and compile the surface rendering fragment shaders:
		*********************************************************************/
		
		/* Assemble the fragment shader's function declarations: */
		std::string fragmentDeclarations;
		
		/* Assemble the fragment shader's uniform and varying variables: */
		std::string fragmentUniforms;
		std::string fragmentVaryings;
		
		/* Assemble the fragment shader's main function: */
		std::string fragmentMain="\
			void main()\n\
				{\n";
		
		if(dem!=0)
			{
			/* Add declarations for DEM matching: */
			fragmentVaryings+="\
				varying float demDist; // Scaled signed distance from surface to DEM\n";
			
			/* Add DEM matching code to the fragment shader's main function: */
			fragmentMain+="\
				/* Calculate the fragment's color from a double-ramp function: */\n\
				vec4 baseColor;\n\
				if(demDist<0.0)\n\
					baseColor=mix(vec4(1.0,1.0,1.0,1.0),vec4(1.0,0.0,0.0,1.0),min(-demDist,1.0));\n\
				else\n\
					baseColor=mix(vec4(1.0,1.0,1.0,1.0),vec4(0.0,0.0,1.0,1.0),min(demDist,1.0));\n\
				\n";
			}
		/* MM: commented out, replaced with ImageMap info
		else if(elevationColorMap!=0)
			{
			// Add declarations for height mapping: 
			fragmentUniforms+="\
				uniform sampler1D heightColorMapSampler;\n";
			fragmentVaryings+="\
				varying float heightColorMapTexCoord; // Texture coordinate for the height color map\n";
			
			// Add height mapping code to the fragment shader's main function: 
			fragmentMain+="\
				// Get the fragment's color from the height color map: \n\
				vec4 baseColor=texture1D(heightColorMapSampler,heightColorMapTexCoord);\n\
				\n";
				}*/
		// MM: is this elevation map specific or just general height map stuff? can leave as is?
		//     (besides changing names from heightColorMap to ImageMap)
		else if(imageMap!=0)
			{
			// Add declarations for height mapping: 
			fragmentUniforms+="\
				uniform sampler1D imageMapSampler;\n";
			fragmentVaryings+="\
				varying float imageMapTexCoord; // Texture coordinate for the image map\n";
			
			// Add height mapping code to the fragment shader's main function: 
			fragmentMain+="\
				// Get the fragment's color from the image map: \n\
				vec4 baseColor=texture1D(imageMapSampler,imageMapTexCoord);\n\
				\n";
			}
		else
			{
			fragmentMain+="\
				/* Set the surface's base color to white: */\n\
				vec4 baseColor=vec4(1.0,1.0,1.0,1.0);\n\
				\n";
			}
		
		/* MM: commented out - think this is just for lining colored sections of ElevationColorMap
		if(drawContourLines)
			{
			// Declare the contour line function
			fragmentDeclarations+="\
				void addContourLines(in vec2,inout vec4);\n";
			
			// Compile the contour line shader
			shaders.push_back(compileFragmentShader("SurfaceAddContourLines"));
			
			// Call contour line function from fragment shader's main function
			fragmentMain+="\
				// Modulate the base color by contour line color: \n\
				addContourLines(gl_FragCoord.xy,baseColor);\n\
				\n";
			}
		*/
		
		if(illuminate)
			{
			/* Declare the illumination function: */
			fragmentDeclarations+="\
				void illuminate(inout vec4);\n";
			
			/* Compile the illumination shader: */
			shaders.push_back(compileFragmentShader("SurfaceIlluminate"));
			
			/* Call illumination function from fragment shader's main function: */
			fragmentMain+="\
				/* Apply illumination to the base color: */\n\
				illuminate(baseColor);\n\
				\n";
			}
				
		/* Finish the fragment shader's main function: */
		fragmentMain+="\
			/* Assign the final color to the fragment: */\n\
			gl_FragColor=baseColor;\n\
			}\n";
		
		/* Compile the fragment shader: */
		shaders.push_back(glCompileFragmentShaderFromStrings(7,fragmentDeclarations.c_str(),"\t\t\n",fragmentUniforms.c_str(),"\t\t\n",fragmentVaryings.c_str(),"\t\t\n",fragmentMain.c_str()));
		
		/* Link the shader program: */
		result=glLinkShader(shaders);
		
		/* Release all compiled shaders: */
		for(std::vector<GLhandleARB>::iterator shIt=shaders.begin();shIt!=shaders.end();++shIt)
			glDeleteObjectARB(*shIt);
		
		/*******************************************************************
		Query the shader program's uniform locations:
		*******************************************************************/
		
		GLint* ulPtr=uniformLocations;
		
		/* Query common uniform variables: */
		*(ulPtr++)=glGetUniformLocationARB(result,"depthSampler");
		*(ulPtr++)=glGetUniformLocationARB(result,"depthProjection");
		if(dem!=0)
			{
			/* Query DEM matching uniform variables: */
			*(ulPtr++)=glGetUniformLocationARB(result,"demTransform");
			*(ulPtr++)=glGetUniformLocationARB(result,"demSampler");
			*(ulPtr++)=glGetUniformLocationARB(result,"demDistScale");
			}
		/* MM: commented out, replaced with ImageMap info
		else if(elevationColorMap!=0)
			{
			  // Query height color mapping uniform variables: 
			*(ulPtr++)=glGetUniformLocationARB(result,"heightColorMapPlaneEq");
			*(ulPtr++)=glGetUniformLocationARB(result,"heightColorMapSampler");
			}*/
		else if(imageMap!=0)
			{
			  // Query image mapping uniform variables: 
			*(ulPtr++)=glGetUniformLocationARB(result,"imageMapPlaneEq");
			*(ulPtr++)=glGetUniformLocationARB(result,"imageMapSampler");
			}
		/* MM: commented out - think this is just for lining colored sections of ElevationColorMap
		if(drawContourLines)
			{
			*(ulPtr++)=glGetUniformLocationARB(result,"pixelCornerElevationSampler");
			*(ulPtr++)=glGetUniformLocationARB(result,"contourLineFactor");
			}
		*/
		if(illuminate)
			{
			/* Query illumination uniform variables: */
			*(ulPtr++)=glGetUniformLocationARB(result,"modelview");
			*(ulPtr++)=glGetUniformLocationARB(result,"tangentModelviewDepthProjection");
			}
		*(ulPtr++)=glGetUniformLocationARB(result,"projectionModelviewDepthProjection");
		}
	catch(...)
		{
		/* Clean up and re-throw the exception: */
		for(std::vector<GLhandleARB>::iterator shIt=shaders.begin();shIt!=shaders.end();++shIt)
			glDeleteObjectARB(*shIt);
		throw;
		}
	
	std::cout << "Done with SurfaceRenderer::createSinglePassSurfaceShader." << std::endl;  // MM: added
	return result;
	}

void SurfaceRenderer::renderPixelCornerElevations(const int viewport[4],const PTransform& projectionModelview,GLContextData& contextData,SurfaceRenderer::DataItem* dataItem) const
	{
	std::cout << "In SurfaceRenderer::renderPixelCornerElevations." << std::endl;  // MM: added

	/* MM: commenting out entire method to attempt no rendering ImageMap display

	// Save the currently-bound frame buffer and clear color
	GLint currentFrameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
	GLfloat currentClearColor[4];
	glGetFloatv(GL_COLOR_CLEAR_VALUE,currentClearColor);
	
	// Check if the contour line rendering frame buffer needs to be created
	if(dataItem->contourLineFramebufferObject==0)
		{
		// Initialize the frame buffer
		for(int i=0;i<2;++i)
			dataItem->contourLineFramebufferSize[i]=0;
		glGenFramebuffersEXT(1,&dataItem->contourLineFramebufferObject);
		glGenRenderbuffersEXT(1,&dataItem->contourLineDepthBufferObject);
		glGenTextures(1,&dataItem->contourLineColorTextureObject);
		// MM: generates the specified number of texture objects and places their 
		// handles in the GLuint array pointer (the second parameter)
		}
	
	// Bind the contour line rendering frame buffer object
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->contourLineFramebufferObject);
	
	// Check if the contour line frame buffer needs to be resized
	if(dataItem->contourLineFramebufferSize[0]!=(unsigned int)(viewport[2]+1)||dataItem->contourLineFramebufferSize[1]!=(unsigned int)(viewport[3]+1))
		{
		// Remember if the render buffers must still be attached to the frame buffer
		bool mustAttachBuffers=dataItem->contourLineFramebufferSize[0]==0&&dataItem->contourLineFramebufferSize[1]==0;
		
		// Update the frame buffer size
		for(int i=0;i<2;++i)
			dataItem->contourLineFramebufferSize[i]=(unsigned int)(viewport[2+i]+1);
		
		// Resize the topographic contour line rendering depth buffer
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,dataItem->contourLineDepthBufferObject);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_DEPTH_COMPONENT,dataItem->contourLineFramebufferSize[0],dataItem->contourLineFramebufferSize[1]);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,0);
		
		// Resize the topographic contour line rendering color texture
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject); // MM: texture target and texture name
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST); // MM: GLenum target, GLenum pname, GLint param
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_R32F,dataItem->contourLineFramebufferSize[0],dataItem->contourLineFramebufferSize[1],0,GL_LUMINANCE,GL_UNSIGNED_BYTE,0);
		// MM: glTexImage2D args: texture target, level of detail (mipmap; 0 is highest resolution), 
		// internal format (e.g. GL_RGBA), width (texels), height (texels), border (0 is none), 
		// source format, source type, source memory address
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0); // MM: texture target and texture name
		
		if(mustAttachBuffers)
			{
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,dataItem->contourLineDepthBufferObject);
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject,0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glReadBuffer(GL_NONE);
			}
		}
	
	// Extend the viewport to render the corners of all pixels
	glViewport(0,0,viewport[2]+1,viewport[3]+1);
	glClearColor(0.0f,0.0f,0.0f,1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	
	// Shift the projection matrix by half a pixel to render the corners of the final pixels
	PTransform shiftedProjectionModelview=projectionModelview;
	PTransform::Matrix& spmm=shiftedProjectionModelview.getMatrix();
	Scalar xs=Scalar(viewport[2])/Scalar(viewport[2]+1);
	Scalar ys=Scalar(viewport[3])/Scalar(viewport[3]+1);
	for(int j=0;j<4;++j)
		{
		spmm(0,j)*=xs;
		spmm(1,j)*=ys;
		}
	
	// Render the surface elevation into the half-pixel offset frame buffer
	depthImageRenderer->renderElevation(shiftedProjectionModelview,contextData);
	
	// Restore the original viewport
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	
	// Restore the original clear color and frame buffer binding
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
	glClearColor(currentClearColor[0],currentClearColor[1],currentClearColor[2],currentClearColor[3]);
	*/
	std::cout << "Done with SurfaceRenderer::renderPixelCornerElevations." << std::endl;  // MM: added
	}

SurfaceRenderer::SurfaceRenderer(const DepthImageRenderer* sDepthImageRenderer)
	:depthImageRenderer(sDepthImageRenderer),
	 //drawContourLines(true),contourLineFactor(1.0f), MM: commented out
	 //elevationColorMap(0), MM: commented out
	 imageMap(0), // MM: added
	 dem(0),demDistScale(1.0f),
	 illuminate(false),
	 surfaceSettingsVersion(1)
	{
	std::cout << "In SurfaceRenderer::SurfaceRenderer." << std::endl;  // MM: added
	/* Copy the depth image size: */
	for(int i=0;i<2;++i)
		depthImageSize[i]=depthImageRenderer->getDepthImageSize(i);
	
	/* Check if the depth projection matrix retains right-handedness: */
	const PTransform& depthProjection=depthImageRenderer->getDepthProjection();
	Point p1=depthProjection.transform(Point(0,0,0));
	Point p2=depthProjection.transform(Point(1,0,0));
	Point p3=depthProjection.transform(Point(0,1,0));
	Point p4=depthProjection.transform(Point(0,0,1));
	bool depthProjectionInverts=((p2-p1)^(p3-p1))*(p4-p1)<Scalar(0);
	
	/* Calculate the transposed tangent plane depth projection: */
	tangentDepthProjection=Geometry::invert(depthProjection);
	if(depthProjectionInverts)
		tangentDepthProjection*=PTransform::scale(PTransform::Scale(-1,-1,-1));

	/* MM: commenting out section to attempt no rendering ImageMap display
	// Monitor the external shader source files
	fileMonitor.addPath((std::string(CONFIG_SHADERDIR)+std::string("/SurfaceAddContourLines.fs")).c_str(),IO::FileMonitor::Modified,Misc::createFunctionCall(this,&SurfaceRenderer::shaderSourceFileChanged));
	fileMonitor.addPath((std::string(CONFIG_SHADERDIR)+std::string("/SurfaceIlluminate.fs")).c_str(),IO::FileMonitor::Modified,Misc::createFunctionCall(this,&SurfaceRenderer::shaderSourceFileChanged));
	fileMonitor.startPolling();
	*/

	std::cout << "Done with SurfaceRenderer::SurfaceRenderer." << std::endl;  // MM: added
	}

void SurfaceRenderer::initContext(GLContextData& contextData) const
	{
	std::cout << "In SurfaceRenderer::initContext." << std::endl;  // MM: added
	

	// Create a data item and add it to the context
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);

	// Create the height map render shader
	dataItem->heightMapShader=createSinglePassSurfaceShader(*contextData.getLightTracker(),dataItem->heightMapShaderUniforms);
	dataItem->surfaceSettingsVersion=surfaceSettingsVersion;
	dataItem->lightTrackerVersion=contextData.getLightTracker()->getVersion();

	/* MM: commenting out entire method to attempt no rendering ImageMap display
	
	// Create the global ambient height map render shader
	dataItem->globalAmbientHeightMapShader=linkVertexAndFragmentShader("SurfaceGlobalAmbientHeightMapShader");
	dataItem->globalAmbientHeightMapShaderUniforms[0]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"depthSampler");
	dataItem->globalAmbientHeightMapShaderUniforms[1]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"depthProjection");
	dataItem->globalAmbientHeightMapShaderUniforms[2]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"basePlane");
	dataItem->globalAmbientHeightMapShaderUniforms[3]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"pixelCornerElevationSampler");
	dataItem->globalAmbientHeightMapShaderUniforms[4]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"contourLineFactor");
	dataItem->globalAmbientHeightMapShaderUniforms[5]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"imageMapSampler");    // MM: changed to imageMap
	dataItem->globalAmbientHeightMapShaderUniforms[6]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"imageMapTransformation");  // MM: changed to imageMap

	// Create the shadowed illuminated height map render shader
	dataItem->shadowedIlluminatedHeightMapShader=linkVertexAndFragmentShader("SurfaceShadowedIlluminatedHeightMapShader");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[0]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"depthSampler");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[1]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"depthProjection");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[2]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"tangentDepthProjection");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[3]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"basePlane");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[4]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"pixelCornerElevationSampler");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[5]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"contourLineFactor");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[6]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"imageMapSampler");  // MM: changed to imageMap
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[7]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"imageMapTransformation");  // MM: changed to imageMap
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[11]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"shadowTextureSampler");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[12]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"shadowProjection");
	*/
	std::cout << "Done with SurfaceRenderer::initContext." << std::endl;  // MM: added
	}

/* MM: commented out - think this is just for lining colored sections of ElevationColorMap
void SurfaceRenderer::setDrawContourLines(bool newDrawContourLines)
	{
	drawContourLines=newDrawContourLines;
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::setContourLineDistance(GLfloat newContourLineDistance)
	{
	// Set the new contour line factor
	contourLineFactor=1.0f/newContourLineDistance;
	}
*/

/* MM: commented out - replaced with ImageMap
void SurfaceRenderer::setElevationColorMap(ElevationColorMap* newElevationColorMap)
	{
	// Check if setting this elevation color map invalidates the shader:
	if(dem==0&&((newElevationColorMap!=0&&elevationColorMap==0)||(newElevationColorMap==0&&elevationColorMap!=0)))
		++surfaceSettingsVersion;
	
	// Set the elevation color map:
	elevationColorMap=newElevationColorMap;
	}*/

void SurfaceRenderer::setImageMap(ImageMap* newImageMap)
	{
	imageMap=newImageMap;
	}

void SurfaceRenderer::setDem(DEM* newDem)
	{
	/* Check if setting this DEM invalidates the shader: */
	if((newDem!=0&&dem==0)||(newDem==0&&dem!=0))
		++surfaceSettingsVersion;
	
	/* Set the new DEM: */
	dem=newDem;
	}

void SurfaceRenderer::setDemDistScale(GLfloat newDemDistScale)
	{
	demDistScale=newDemDistScale;
	}

void SurfaceRenderer::setIlluminate(bool newIlluminate)
	{
	illuminate=newIlluminate;
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::renderSinglePass(const int viewport[4],const PTransform& projection,const OGTransform& modelview,GLContextData& contextData) const
	{
	std::cout << "In SurfaceRenderer::renderSinglePass." << std::endl;  // MM: added
	
	// Get the data item
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	// Calculate the required matrices
	PTransform projectionModelview=projection;
	projectionModelview*=modelview;

	/* MM: commenting out entire method to attempt no rendering ImageMap display
	
	/* MM: commented out - think this is just for lining colored sections of ElevationColorMap
	// Check if contour line rendering is enabled
	if(drawContourLines)
		{
		  // Run the first rendering pass to create a half-pixel offset texture of surface elevations:
		renderPixelCornerElevations(viewport,projectionModelview,contextData,dataItem);
		}
	else if(dataItem->contourLineFramebufferObject!=0)
		{
		  // Delete the contour line rendering frame buffer
		glDeleteFramebuffersEXT(1,&dataItem->contourLineFramebufferObject);
		dataItem->contourLineFramebufferObject=0;
		glDeleteRenderbuffersEXT(1,&dataItem->contourLineDepthBufferObject);
		dataItem->contourLineDepthBufferObject=0;
		glDeleteTextures(1,&dataItem->contourLineColorTextureObject);
		dataItem->contourLineColorTextureObject=0;
		}
	*/

	std::cout << "Checking if single-pass surface shader is outdated." << std::endl;

	// Check if the single-pass surface shader is outdated
	if(dataItem->surfaceSettingsVersion!=surfaceSettingsVersion||(illuminate&&dataItem->lightTrackerVersion!=contextData.getLightTracker()->getVersion()))
		{
		  // Rebuild the shader
		try
			{
			std::cout << "Single-pass surface shader is outdated." << std::endl;
			GLhandleARB newShader=createSinglePassSurfaceShader(*contextData.getLightTracker(),dataItem->heightMapShaderUniforms);
			glDeleteObjectARB(dataItem->heightMapShader);
			dataItem->heightMapShader=newShader;
			std::cout << "Done with single-pass surface shader updating." << std::endl;
			}
		catch(std::runtime_error err)
			{
			Misc::formattedUserError("SurfaceRenderer::renderSinglePass: Caught exception %s while rebuilding surface shader",err.what());
			}
		
		// Mark the shader as up-to-date
		std::cout << "Mark single-pass surface shader as updated." << std::endl;
		dataItem->surfaceSettingsVersion=surfaceSettingsVersion;
		dataItem->lightTrackerVersion=contextData.getLightTracker()->getVersion();
		}
	
	std::cout << "Bind the single-pass surface shader." << std::endl;
	// Bind the single-pass surface shader
	glUseProgramObjectARB(dataItem->heightMapShader);
	const GLint* ulPtr=dataItem->heightMapShaderUniforms;
	
	std::cout << "Bind the current depth image texture." << std::endl;
	// Bind the current depth image texture
	glActiveTextureARB(GL_TEXTURE0_ARB);
	depthImageRenderer->bindDepthTexture(contextData);
	glUniform1iARB(*(ulPtr++),0);
	
	std::cout << "Upload the depth projection matrix." << std::endl;
	// Upload the depth projection matrix
	depthImageRenderer->uploadDepthProjection(*(ulPtr++));
	
	/* MM: commenting out entire method to attempt no rendering ImageMap display
	
	if(dem!=0)
		{
		// Upload the DEM transformation
		dem->uploadDemTransform(*(ulPtr++));
		
		// Bind the DEM texture
		glActiveTextureARB(GL_TEXTURE1_ARB);
		dem->bindTexture(contextData);
		glUniform1iARB(*(ulPtr++),1);
		
		// Upload the DEM distance scale factor
		glUniform1fARB(*(ulPtr++),1.0f/(demDistScale*dem->getVerticalScale()));
		}
	/* MM: commented out, replaced with Image Map info
	else if(elevationColorMap!=0)
		{
		  // Upload the texture mapping plane equation:
		elevationColorMap->uploadTexturePlane(*(ulPtr++));
		
		// Bind the height color map texture:
		glActiveTextureARB(GL_TEXTURE1_ARB);
		elevationColorMap->bindTexture(contextData);
		glUniform1iARB(*(ulPtr++),1);
		}*/

	
	//else if(imageMap!=0) {
	if(imageMap!=0) {
	  // Upload the texture mapping plane equation:
	  imageMap->uploadTexturePlane(*(ulPtr++));
		
	  // Bind the image map texture:
	  glActiveTextureARB(GL_TEXTURE1_ARB); // MM: ??
	  imageMap->bindTexture(contextData);
	  glUniform1iARB(*(ulPtr++),1);        // MM: ??
	}
	else
	  std::cout << "imageMap == 0!" << std::endl;
	
	/* MM: commented out - think this is just for lining colored sections of ElevationColorMap
	if(drawContourLines)
		{
		// Bind the pixel corner elevation texture
		glActiveTextureARB(GL_TEXTURE2_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject); // MM: texture target and texture name
		glUniform1iARB(*(ulPtr++),2);
		
		// Upload the contour line distance factor
		glUniform1fARB(*(ulPtr++),contourLineFactor);
		}
	*/

	/* MM: commenting out entire method to attempt no rendering ImageMap display
	if(illuminate)
		{
		// Upload the modelview matrix
		glUniformARB(*(ulPtr++),modelview);
		
		// Calculate and upload the tangent-plane modelview depth projection matrix
		PTransform tangentModelviewDepthProjection=tangentDepthProjection;
		tangentModelviewDepthProjection*=Geometry::invert(modelview);
		const Scalar* tmdpPtr=tangentModelviewDepthProjection.getMatrix().getEntries();
		GLfloat matrix[16];
		GLfloat* mPtr=matrix;
		for(int i=0;i<16;++i,++tmdpPtr,++mPtr)
				*mPtr=GLfloat(*tmdpPtr);
		glUniformMatrix4fvARB(*(ulPtr++),1,GL_FALSE,matrix);
		}
	*/
	
	// Upload the combined projection, modelview, and depth unprojection matrix
	PTransform projectionModelviewDepthProjection=projectionModelview;
	projectionModelviewDepthProjection*=depthImageRenderer->getDepthProjection();
	glUniformARB(*(ulPtr++),projectionModelviewDepthProjection);

	// MM: here's where the display happens, I think
	// Draw the surface
	depthImageRenderer->renderSurfaceTemplate(contextData);  // MM: not sure if necessary
	
	
	/*
	// Unbind all textures and buffers
	/* MM: commented out - think this is just for lining colored sections of ElevationColorMap
	if(drawContourLines)
		{
		glActiveTextureARB(GL_TEXTURE2_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0); // MM: texture target and texture name
		}
	*/
	/* MM: commenting out entire method to attempt no rendering ImageMap display
	if(dem!=0)
		{
		glActiveTextureARB(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0); // MM: texture target and texture name
		}
	/* MM: commented out, replaced with ImageMap info
	else if(elevationColorMap!=0)
		{
		glActiveTextureARB(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_1D,0); // MM: texture target and texture name
		}*/
	
	//else if(imageMap!=0)
	if(imageMap!=0)
	  {
	    glActiveTextureARB(GL_TEXTURE1_ARB);
	    //glBindTexture(GL_TEXTURE_1D,0);
	    glBindTexture(GL_TEXTURE_2D,0);
	  }
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0); // MM: texture target and texture name
	
	// Unbind the height map shader
	glUseProgramObjectARB(0);
	
	
	std::cout << "Done with SurfaceRenderer::renderSinglePass." << std::endl;  // MM: added
	}

