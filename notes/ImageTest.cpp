/***********************************************************************
Small image viewer using Vrui. 
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

// MM: trying to modify image viewer Vrui example program to display
//     an image simply; then use this with an ImageMap class to replace
//     ElevationColorMap

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

class ImageViewer:public Vrui::Application
{
private:	
  /* Elements: */
  Images::TextureSet textures; // Texture set containing the image to be displayed
	  
  /* Constructors and destructors: */
public:
  ImageViewer(int& argc,char**& argv);
  virtual ~ImageViewer(void);
	
  /* Methods from Vrui::Application: */
  virtual void display(GLContextData& contextData) const;
  virtual void resetNavigation(void);
};


/****************************
Methods of class ImageViewer:
****************************/

ImageViewer::ImageViewer(int& argc,char**& argv)
  :Vrui::Application(argc,argv)
{
  /* Load the image into the texture set: */
  // MM: addTexture(BaseImage, open file target, internal format, key)
  //     GL_RGB8 must mean RGB 8-bit image. Note the key, 0U, is referenced in display()
  //const char* filename = "/opt/SARndbox-2.3/maya/sample_text.jpg";
  //Images::TextureSet::Texture& tex=textures.addTexture(Images::readImageFile(filename,Vrui::openFile(filename)),GL_TEXTURE_2D,GL_RGB8,0U);
  Images::TextureSet::Texture& tex=textures.addTexture(Images::readImageFile(argv[1],
									     Vrui::openFile(argv[1])),GL_TEXTURE_2D,GL_RGB8,0U);
	
  /* Set clamping and filtering parameters for mip-mapped linear interpolation: */
  tex.setMipmapRange(0,1000);
  //tex.setWrapModes(GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
  tex.setWrapModes(GL_CLAMP,GL_CLAMP);
  tex.setFilterModes(GL_LINEAR_MIPMAP_LINEAR,GL_LINEAR);
	
  const Images::BaseImage& image=tex.getImage();
  std::cout << "width: " << image.getWidth() << std::endl;  // MM: added
  std::cout << "height: " << image.getHeight() << std::endl;  // MM: added
}

ImageViewer::~ImageViewer(void)
{
}

void ImageViewer::display(GLContextData& contextData) const
{
  // MM: TO DO: figure out how to prevent this from making a flippable image
	  
  // Set up OpenGL state
  glPushAttrib(GL_ENABLE_BIT);
  // MM: ^ pushes current state of enabled and disabled functions onto stack.
  //       often used to save current state before calling function that
  //       might affect the state. 
	
  //glEnable(GL_TEXTURE_1D);
  glEnable(GL_TEXTURE_2D);
  // MM: ^ with fixed pipeline, you needed to call glEnable(GL_TEXTURE_2D) to 
  //       enable 2D texturing. Since shaders override these functionalities, 
  //       you don't need to glEnable/glDisable.
	
  //       GL_TEXTURE_2D: Images in this texture all are 2-dimensional. 
  //                      They have width and height, but no depth.
	
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
  // MM: ^ a texture environment specifies how texture values are interpreted
  //       when a fragment is textured. glTexEnvi(target, pname, param)
	
  //       When target is GL_TEXTURE_ENV, pname can be GL_TEXTURE_ENV_MODE, 
  //       GL_TEXTURE_ENV_COLOR, GL_COMBINE_RGB, GL_COMBINE_ALPHA, GL_RGB_SCALE,
  //       GL_ALPHA_SCALE, GL_SRC0_RGB, GL_SRC1_RGB, GL_SRC2_RGB, GL_SRC0_ALPHA,
  //       GL_SRC1_ALPHA, or GL_SRC2_ALPHA. 
	
  //       If pname is GL_TEXTURE_ENV_MODE, then params is (or points to) the 
  //       symbolic name of a texture function. Six texture functions may be 
  //       specified: GL_ADD, GL_MODULATE, GL_DECAL, GL_BLEND, GL_REPLACE, or GL_COMBINE.
	
  //       A texture function acts on the fragment to be textured using the texture
  //       image value that applies to the fragment (see glTexParameter) and produces
  //       an RGBA color for that fragment.
	
  //       GL_REPLACE basically tells the renderer to skip the current color and just
  //       use the texel colors (of the texture..?). 
	
	
  /* Get the texture set's GL state: */
  // MM: getGLState returns OpenGL texture state object for the given OpenGL context
  Images::TextureSet::GLState* texGLState=textures.getGLState(contextData);
	
  /* Bind the texture object: */
  // MM: bindTexture binds the texture object associated with the given key
  //     to its texture target on the current texture unit, returns texture state.
  //     Note the key is 0U, which was the key in ImageViewer constructor.
  const Images::TextureSet::GLState::Texture& tex=texGLState->bindTexture(0U);
  const Images::BaseImage& image=tex.getImage();
  // MM: try replacing this with Image so can test the setPixel() method
	
  /* Query the range of texture coordinates: */
  const GLfloat* texMin=tex.getTexCoordMin();
  const GLfloat* texMax=tex.getTexCoordMax();
  std::cout << "min: " << texMin[0] << ", " << texMin[1] << std::endl;  // MM: added
  std::cout << "max: " << texMax[0] << ", " << texMax[1] << std::endl;  // MM: added
  std::cout << image.getSize(0) << ", " << image.getSize(1) << std::endl; 
	
  // MM: this distorts the image for some reason. can't figure it out. 
  //     (original ImageViewer.cpp does too.)
  //     how to stop it from rotating? doesn't work:
  //      - changing 2D to 1D
  //      - commenting out texcoord
	
  // Draw the image
  // MM: Note: texture coordinates specify the point in the texture image that will 
  //     correspond to the vertex you're specifying them for. see Vrui and OpenGL notes
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
	
  /* ORIGINAL:
  // Draw the image
  // MM: Note: texture coordinates specify the point in the texture image that will 
  //     correspond to the vertex you're specifying them for. see Vrui and OpenGL notes
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
  */
	
  /* Protect the texture object: */
  //glBindTexture(GL_TEXTURE_1D,0);
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
VRUI_APPLICATION_RUN(ImageViewer)
