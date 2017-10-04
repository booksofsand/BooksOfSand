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
#include <IOstream>

class ImageViewer:public Vrui::Application
	{
	private:	
	/* Elements: */
	  Images::TextureSet textures; // Texture set containing the image to be displayed
	  String filename;
	  
	/* Constructors and destructors: */
	public:
	  ImageViewer();
	  virtual ~ImageViewer(void);
	
	  /* Methods from Vrui::Application: */
	  virtual void display(GLContextData& contextData) const;
	  virtual void resetNavigation(void);
	};


/****************************
Methods of class ImageViewer:
****************************/

ImageViewer::ImageViewer()
	:Vrui::Application()
	{
	/* Load the image into the texture set: */
        // MM: addTexture(BaseImage, open file target, internal format, key)
	//     GL_RGB8 must mean RGB 8-bit image. Note the key, 0U, is referenced in display()
	  filename = "sample_text.jpg";
	Images::TextureSet::Texture& tex=textures.addTexture(Images::readImageFile(filename,Vrui::openFile(filename),GL_TEXTURE_2D,GL_RGB8,0U);
	
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

void ImageViewer::resetNavigation(void)
	{
	/* Access the image: */
	const Images::BaseImage& image=textures.getTexture(0U).getImage();
	
	/* Reset the Vrui navigation transformation: */
	Vrui::Scalar w=Vrui::Scalar(image.getSize(0));
	Vrui::Scalar h=Vrui::Scalar(image.getSize(1));
	Vrui::Point center(Math::div2(w),Math::div2(h),Vrui::Scalar(0.01));
	Vrui::Scalar size=Math::sqrt(Math::sqr(w)+Math::sqr(h));
	Vrui::setNavigationTransformation(center,size,Vrui::Vector(0,1,0));
	}

/* Create and execute an application object: */
//VRUI_APPLICATION_RUN(ImageViewer)
