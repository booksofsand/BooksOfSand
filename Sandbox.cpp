 /***********************************************************************
Sandbox - Vrui application to drive an augmented reality sandbox.
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

#include "Sandbox.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <Misc/SizedTypes.h>
#include <Misc/SelfDestructPointer.h>
#include <Misc/FixedArray.h>
#include <Misc/FunctionCalls.h>
#include <Misc/FileNameExtensions.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/ArrayValueCoders.h>
#include <Misc/ConfigurationFile.h>
#include <IO/File.h>
#include <IO/ValueSource.h>
#include <iostream> // MM: added

// MM: math includes - I don't know how much of this is only used for the topography calculations
#include <Math/Math.h>
#include <Math/Constants.h>
#include <Math/Interval.h>
#include <Math/MathValueCoders.h>
#include <Geometry/Point.h>
#include <Geometry/AffineCombiner.h>
#include <Geometry/HVector.h>
#include <Geometry/Plane.h>
#include <Geometry/LinearUnit.h>
#include <Geometry/GeometryValueCoders.h>
#include <Geometry/OutputOperators.h>

// MM: graphic library includes - may not need all of these
#include <GL/gl.h>
#include <GL/GLMaterialTemplates.h>
#include <GL/GLColorMap.h>
#include <GL/GLLightTracker.h>
#include <GL/Extensions/GLEXTFramebufferObject.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBDepthTexture.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBMultitexture.h>
#include <GL/GLContextData.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLTransformationWrappers.h>
// MM: GLMotif includes look like UI stuff
#include <GLMotif/StyleSheet.h>
#include <GLMotif/WidgetManager.h>
#include <GLMotif/PopupMenu.h>
#include <GLMotif/Menu.h>
#include <GLMotif/PopupWindow.h>
#include <GLMotif/Margin.h>
#include <GLMotif/Label.h>
#include <GLMotif/TextField.h>

// MM: Vrui 3D library includes
#include <Vrui/Vrui.h>
#include <Vrui/CoordinateManager.h>
#include <Vrui/Lightsource.h>
#include <Vrui/LightsourceManager.h>
#include <Vrui/Viewer.h>
#include <Vrui/ToolManager.h>
#include <Vrui/DisplayState.h>
#include <Vrui/OpenFile.h>
#include <Kinect/FileFrameSource.h>
#include <Kinect/DirectFrameSource.h>
#include <Kinect/OpenDirectFrameSource.h>

// MM: SARndbox defined includes
#include "FrameFilter.h"
#include "DepthImageRenderer.h"
#include "ElevationColorMap.h"
#include "DEM.h"
#include "SurfaceRenderer.h"
#include "DEMTool.h"
#include "Config.h"

#include "kinecthandler.h" // MM: added

/**********************************
Methods of class Sandbox::DataItem:
**********************************/

/* MM: what is a data item? */
Sandbox::DataItem::DataItem(void)
	:shadowFramebufferObject(0),shadowDepthTextureObject(0)
	{
	std::cout << "In Sandbox::DataItem::DataItem." << std::endl; // MM: added
	/* Check if all required extensions are supported: */
	// MM: can change this once we determine which extensions we need
	bool supported=GLEXTFramebufferObject::isSupported();
	supported=supported&&GLARBTextureRectangle::isSupported();
	supported=supported&&GLARBTextureFloat::isSupported();
	supported=supported&&GLARBTextureRg::isSupported();
	supported=supported&&GLARBDepthTexture::isSupported();
	supported=supported&&GLARBShaderObjects::isSupported();
	supported=supported&&GLARBVertexShader::isSupported();
	supported=supported&&GLARBFragmentShader::isSupported();
	supported=supported&&GLARBMultitexture::isSupported();
	if(!supported)
		Misc::throwStdErr("Sandbox: Not all required extensions are supported by local OpenGL");
	
	/* Initialize all required extensions: */
	GLEXTFramebufferObject::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureFloat::initExtension();
	GLARBTextureRg::initExtension();
	GLARBDepthTexture::initExtension();
	GLARBShaderObjects::initExtension();
	GLARBVertexShader::initExtension();
	GLARBFragmentShader::initExtension();
	GLARBMultitexture::initExtension();
	std::cout << "Done with Sandbox::DataItem::DataItem." << std::endl; // MM: added
	}

Sandbox::DataItem::~DataItem(void)
	{
	/* Delete all shaders, buffers, and texture objects: */
	glDeleteFramebuffersEXT(1,&shadowFramebufferObject);
	glDeleteTextures(1,&shadowDepthTextureObject);
	}

/****************************************
Methods of class Sandbox::RenderSettings:
****************************************/

/* MM: looks like these render settings are specific to topography and SARndbox, 
       though the projector settings may communicate with Vrui / Kinect... 
       see Sandbox.h for RenderSettings struct */
Sandbox::RenderSettings::RenderSettings(void)
	:fixProjectorView(false),projectorTransform(PTransform::identity),projectorTransformValid(false),
	 hillshade(false),surfaceMaterial(GLMaterial::Color(1.0f,1.0f,1.0f)),
	 useShadows(false),
	 elevationColorMap(0),
	 useContourLines(true),contourLineSpacing(0.75f),
	 surfaceRenderer(0)
	{
	std::cout << "In Sandbox::RenderSettings::RenderSettings." << std::endl; // MM: added
	/* Load the default projector transformation: */
	loadProjectorTransform(CONFIG_DEFAULTPROJECTIONMATRIXFILENAME);
	std::cout << "Done with Sandbox::RenderSettings::RenderSettings." << std::endl; // MM: added
	}

Sandbox::RenderSettings::RenderSettings(const Sandbox::RenderSettings& source)
	:fixProjectorView(source.fixProjectorView),projectorTransform(source.projectorTransform),projectorTransformValid(source.projectorTransformValid),
	 hillshade(source.hillshade),surfaceMaterial(source.surfaceMaterial),
	 useShadows(source.useShadows),
	 elevationColorMap(source.elevationColorMap!=0?new ElevationColorMap(*source.elevationColorMap):0),
	 useContourLines(source.useContourLines),contourLineSpacing(source.contourLineSpacing),
	 surfaceRenderer(0)
	{
	  std::cout << "In Sandbox::RenderSettings::RenderSettings." << std::endl; // MM: added
	  std::cout << "Done with Sandbox::RenderSettings::RenderSettings." << std::endl; // MM: added
	}

Sandbox::RenderSettings::~RenderSettings(void)
	{
	delete surfaceRenderer;
	delete elevationColorMap;
	}

void Sandbox::RenderSettings::loadProjectorTransform(const char* projectorTransformName)
	{
	std::cout << "In Sandbox::RenderSettings::loadProjectorTransform." << std::endl; // MM: added
	std::string fullProjectorTransformName;
	try
		{
		/* Open the projector transformation file: */
		if(projectorTransformName[0]=='/')
			{
			/* Use the absolute file name directly: */
			fullProjectorTransformName=projectorTransformName;
			}
		else
			{
			/* Assemble a file name relative to the configuration file directory: */
			fullProjectorTransformName=CONFIG_CONFIGDIR;
			fullProjectorTransformName.push_back('/');
			fullProjectorTransformName.append(projectorTransformName);
			}
		IO::FilePtr projectorTransformFile=Vrui::openFile(fullProjectorTransformName.c_str(),IO::File::ReadOnly);
		projectorTransformFile->setEndianness(Misc::LittleEndian);
		
		/* Read the projector transformation matrix from the binary file: */
		Misc::Float64 pt[16];
		projectorTransformFile->read(pt,16);
		projectorTransform=PTransform::fromRowMajor(pt);
		
		projectorTransformValid=true;
		}
	catch(std::runtime_error err)
		{
		/* Print an error message and disable calibrated projections: */
		std::cerr<<"Unable to load projector transformation from file "<<fullProjectorTransformName<<" due to exception "<<err.what()<<std::endl;
		projectorTransformValid=false;
		}
	std::cout << "Done with Sandbox::RenderSettings::loadProjectorTransform." << std::endl; // MM: added
	}

/* MM: this is specific to the topography map, which we may be able to modify. 
       it's called only twice in this file, both times in Sandbox constructor:
       1) with a height map name as an argument
       2) using the default height map */
void Sandbox::RenderSettings::loadHeightMap(const char* heightMapName)
	{
	std::cout << "In Sandbox::RenderSettings::loadHeightMap." << std::endl; // MM: added
	try
		{
		/* Load the elevation color map of the given name: */
		ElevationColorMap* newElevationColorMap=new ElevationColorMap(heightMapName);
		
		/* Delete the previous elevation color map and assign the new one: */
		delete elevationColorMap;
		elevationColorMap=newElevationColorMap;
		}
	catch(std::runtime_error err)
		{
		std::cerr<<"Ignoring height map due to exception "<<err.what()<<std::endl;
		}
	std::cout << "Done with Sandbox::RenderSettings::loadHeightMap." << std::endl; // MM: added
	}

/************************
Methods of class Sandbox:
************************/

/* MM: is a raw depth frame a collection of data representing a single frame received from Kinect? */
void Sandbox::rawDepthFrameDispatcher(const Kinect::FrameBuffer& frameBuffer)
	{
	  std::cout << "In Sandbox::rawDepthFrameDispatcher." << std::endl; // MM: added
	/* Pass the received frame to the frame filter and the hand extractor: */
	if(frameFilter!=0&&!pauseUpdates)
		frameFilter->receiveRawFrame(frameBuffer);
	  std::cout << "Done with Sandbox::rawDepthFrameDispatcher." << std::endl; // MM: added
	}

void Sandbox::receiveFilteredFrame(const Kinect::FrameBuffer& frameBuffer)
	{
	std::cout << "In Sandbox::receiveFilteredFrame." << std::endl; // MM: added
	/* Put the new frame into the frame input buffer: */
	filteredFrames.postNewValue(frameBuffer);
	
	/* Wake up the foreground thread: */
	Vrui::requestUpdate();
	
	sendFrameBuffer(frameBuffer); // MM: added
	std::cout << "Done with Sandbox::receiveFilteredFrame." << std::endl; // MM: added
	}

/* MM: "DEM - Class to represent digital elevation models (DEMs) as float-valued texture objects." - DEM.h 
       does this mean we need to use DEMs? */
void Sandbox::toggleDEM(DEM* dem)
	{
	  std::cout << "In Sandbox::toggleDEM." << std::endl; // MM: added
	/* Check if this is the active DEM: */
	if(activeDem==dem)
		{
		/* Deactivate the currently active DEM: */
		activeDem=0;
		}
	else
		{
		/* Activate this DEM: */
		activeDem=dem;
		}
	
	/* Enable DEM matching in all surface renderers that use a fixed projector matrix, i.e., in all physical sandboxes: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->fixProjectorView)
			rsIt->surfaceRenderer->setDem(activeDem);
	  std::cout << "Done with Sandbox::toggleDEM." << std::endl; // MM: added
	}

/* MM: does this mean don't update the projected output? */
void Sandbox::pauseUpdatesCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	pauseUpdates=cbData->set;
	}


/* MM: the following method shows how to build a main menu; sounds relevant to our project */
GLMotif::PopupMenu* Sandbox::createMainMenu(void)
	{
	  std::cout << "In Sandbox::createMainMenu." << std::endl; // MM: added
	/* Create a popup shell to hold the main menu: */
	GLMotif::PopupMenu* mainMenuPopup=new GLMotif::PopupMenu("MainMenuPopup",Vrui::getWidgetManager());
	mainMenuPopup->setTitle("Book Of Sands");
	
	/* Create the main menu itself: */
	GLMotif::Menu* mainMenu=new GLMotif::Menu("MainMenu",mainMenuPopup,false);
	
	/* Create a button to pause topography updates: */
	pauseUpdatesToggle=new GLMotif::ToggleButton("PauseUpdatesToggle",mainMenu,"Pause Topography");
	pauseUpdatesToggle->setToggle(false);
	pauseUpdatesToggle->getValueChangedCallbacks().add(this,&Sandbox::pauseUpdatesCallback);
	
	
	/* Finish building the main menu: */
	mainMenu->manageChild();
	
	std::cout << "Done with Sandbox::createMainMenu." << std::endl; // MM: added
	return mainMenuPopup;
	}

/* MM: what does enclosing the following in namespace{} do? */
namespace {

/****************
Helper functions:
****************/
	
/* MM: usage below may be helpful in understanding the project */
void printUsage(void)
	{
	std::cout<<"Usage: SARndbox [option 1] ... [option n]"<<std::endl;
	std::cout<<"  Options:"<<std::endl;
	std::cout<<"  -h"<<std::endl;
	std::cout<<"     Prints this help message"<<std::endl;
	std::cout<<"  -c <camera index>"<<std::endl;
	std::cout<<"     Selects the local 3D camera of the given index (0: first camera"<<std::endl;
	std::cout<<"     on USB bus)"<<std::endl;
	std::cout<<"     Default: 0"<<std::endl;
	std::cout<<"  -f <frame file name prefix>"<<std::endl;
	std::cout<<"     Reads a pre-recorded 3D video stream from a pair of color/depth"<<std::endl;
	std::cout<<"     files of the given file name prefix"<<std::endl;
	std::cout<<"  -s <scale factor>"<<std::endl;
	std::cout<<"     Scale factor from real sandbox to simulated terrain"<<std::endl;
	std::cout<<"     Default: 100.0 (1:100 scale, 1cm in sandbox is 1m in terrain"<<std::endl;
	std::cout<<"  -slf <sandbox layout file name>"<<std::endl;
	std::cout<<"     Loads the sandbox layout file of the given name"<<std::endl;
	std::cout<<"     Default: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTBOXLAYOUTFILENAME<<std::endl;
	std::cout<<"  -er <min elevation> <max elevation>"<<std::endl;
	std::cout<<"     Sets the range of valid sand surface elevations relative to the"<<std::endl;
	std::cout<<"     ground plane in cm"<<std::endl;
	std::cout<<"     Default: Range of elevation color map"<<std::endl;
	std::cout<<"  -hmp <x> <y> <z> <offset>"<<std::endl;
	std::cout<<"     Sets an explicit base plane equation to use for height color mapping"<<std::endl;
	std::cout<<"  -nas <num averaging slots>"<<std::endl;
	std::cout<<"     Sets the number of averaging slots in the frame filter; latency is"<<std::endl;
	std::cout<<"     <num averaging slots> * 1/30 s"<<std::endl;
	std::cout<<"     Default: 30"<<std::endl;
	std::cout<<"  -sp <min num samples> <max variance>"<<std::endl;
	std::cout<<"     Sets the frame filter parameters minimum number of valid samples"<<std::endl;
	std::cout<<"     and maximum sample variance before convergence"<<std::endl;
	std::cout<<"     Default: 10 2"<<std::endl;
	std::cout<<"  -he <hysteresis envelope>"<<std::endl;
	std::cout<<"     Sets the size of the hysteresis envelope used for jitter removal"<<std::endl;
	std::cout<<"     Default: 0.1"<<std::endl;
	std::cout<<"  -dds <DEM distance scale>"<<std::endl;
	std::cout<<"     DEM matching distance scale factor in cm"<<std::endl;
	std::cout<<"     Default: 1.0"<<std::endl;
	std::cout<<"  -wi <window index>"<<std::endl;
	std::cout<<"     Sets the zero-based index of the display window to which the"<<std::endl;
	std::cout<<"     following rendering settings are applied"<<std::endl;
	std::cout<<"     Default: 0"<<std::endl;
	std::cout<<"  -fpv [projector transform file name]"<<std::endl;
	std::cout<<"     Fixes the navigation transformation so that Kinect camera and"<<std::endl;
	std::cout<<"     projector are aligned, as defined by the projector transform file"<<std::endl;
	std::cout<<"     of the given name"<<std::endl;
	std::cout<<"     Default projector transform file name: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTPROJECTIONMATRIXFILENAME<<std::endl;
	std::cout<<"  -nhs"<<std::endl;
	std::cout<<"     Disables hill shading"<<std::endl;
	std::cout<<"  -uhs"<<std::endl;
	std::cout<<"     Enables hill shading"<<std::endl;
	std::cout<<"  -ns"<<std::endl;
	std::cout<<"     Disables shadows"<<std::endl;
	std::cout<<"  -us"<<std::endl;
	std::cout<<"     Enables shadows"<<std::endl;
	std::cout<<"  -nhm"<<std::endl;
	std::cout<<"     Disables elevation color mapping"<<std::endl;
	std::cout<<"  -uhm [elevation color map file name]"<<std::endl;
	std::cout<<"     Enables elevation color mapping and loads the elevation color map from"<<std::endl;
	std::cout<<"     the file of the given name"<<std::endl;
	std::cout<<"     Default elevation color  map file name: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTHEIGHTCOLORMAPFILENAME<<std::endl;
	std::cout<<"  -ncl"<<std::endl;
	std::cout<<"     Disables topographic contour lines"<<std::endl;
	std::cout<<"  -ucl [contour line spacing]"<<std::endl;
	std::cout<<"     Enables topographic contour lines and sets the elevation distance between"<<std::endl;
	std::cout<<"     adjacent contour lines to the given value in cm"<<std::endl;
	std::cout<<"     Default contour line spacing: 0.75"<<std::endl;
	std::cout<<"  -cp <control pipe name>"<<std::endl;
	std::cout<<"     Sets the name of a named POSIX pipe from which to read control commands"<<std::endl;
	}

}

/* MM: this is a mess, and we'll have to pick through it carefully */
Sandbox::Sandbox(int& argc,char**& argv)
	:Vrui::Application(argc,argv),
	 camera(0),pixelDepthCorrection(0),
	 frameFilter(0),pauseUpdates(false),
	 depthImageRenderer(0),
	 sun(0),
	 activeDem(0),
	 mainMenu(0),pauseUpdatesToggle(0),frameRateTextField(0),
	 controlPipeFd(-1)
	{
	  std::cout << "In Sandbox::Sandbox." << std::endl; // MM: added
	  
	/* Read the sandbox's default configuration parameters: */
	std::string sandboxConfigFileName=CONFIG_CONFIGDIR;
	sandboxConfigFileName.push_back('/');
	sandboxConfigFileName.append(CONFIG_DEFAULTCONFIGFILENAME);
	Misc::ConfigurationFile sandboxConfigFile(sandboxConfigFileName.c_str());
	Misc::ConfigurationFileSection cfg=sandboxConfigFile.getSection("/SARndbox");
	unsigned int cameraIndex=cfg.retrieveValue<int>("./cameraIndex",0);
	std::string cameraConfiguration=cfg.retrieveString("./cameraConfiguration","Camera");
	double scale=cfg.retrieveValue<double>("./scaleFactor",100.0);
	std::string sandboxLayoutFileName=CONFIG_CONFIGDIR;
	sandboxLayoutFileName.push_back('/');
	sandboxLayoutFileName.append(CONFIG_DEFAULTBOXLAYOUTFILENAME);
	sandboxLayoutFileName=cfg.retrieveString("./sandboxLayoutFileName",sandboxLayoutFileName);
	Math::Interval<double> elevationRange=cfg.retrieveValue<Math::Interval<double> >("./elevationRange",Math::Interval<double>(-1000.0,1000.0));

	bool haveHeightMapPlane=cfg.hasTag("./heightMapPlane");
	Plane heightMapPlane;
	if(haveHeightMapPlane)
		heightMapPlane=cfg.retrieveValue<Plane>("./heightMapPlane");
	unsigned int numAveragingSlots=cfg.retrieveValue<unsigned int>("./numAveragingSlots",30);
	unsigned int minNumSamples=cfg.retrieveValue<unsigned int>("./minNumSamples",10);
	unsigned int maxVariance=cfg.retrieveValue<unsigned int>("./maxVariance",2);
	float hysteresis=cfg.retrieveValue<float>("./hysteresis",0.1f);
	Misc::FixedArray<unsigned int,2> wtSize;
	wtSize[0]=640;
	wtSize[1]=480;
	float demDistScale=cfg.retrieveValue<float>("./demDistScale",1.0f);
	std::string controlPipeName=cfg.retrieveString("./controlPipeName","");
	
	/* Process command line parameters: */
	bool printHelp=false;
	const char* frameFilePrefix=0;
	int windowIndex=0;
	renderSettings.push_back(RenderSettings());
	for(int i=1;i<argc;++i)
		{
		if(argv[i][0]=='-')
			{
			if(strcasecmp(argv[i]+1,"h")==0)
				printHelp=true;
			else if(strcasecmp(argv[i]+1,"c")==0)
				{
				++i;
				cameraIndex=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"f")==0)
				{
				++i;
				frameFilePrefix=argv[i];
				}
			else if(strcasecmp(argv[i]+1,"s")==0)
				{
				++i;
				scale=atof(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"slf")==0)
				{
				++i;
				sandboxLayoutFileName=argv[i];
				}
			else if(strcasecmp(argv[i]+1,"er")==0)
				{
				++i;
				double elevationMin=atof(argv[i]);
				++i;
				double elevationMax=atof(argv[i]);
				elevationRange=Math::Interval<double>(elevationMin,elevationMax);
				}
			else if(strcasecmp(argv[i]+1,"hmp")==0)
				{
				/* Read height mapping plane coefficients: */
				haveHeightMapPlane=true;
				double hmp[4];
				for(int j=0;j<4;++j)
					{
					++i;
					hmp[j]=atof(argv[i]);
					}
				heightMapPlane=Plane(Plane::Vector(hmp),hmp[3]);
				heightMapPlane.normalize();
				}
			else if(strcasecmp(argv[i]+1,"nas")==0)
				{
				++i;
				numAveragingSlots=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"sp")==0)
				{
				++i;
				minNumSamples=atoi(argv[i]);
				++i;
				maxVariance=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"he")==0)
				{
				++i;
				hysteresis=float(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"wts")==0)
				{
				for(int j=0;j<2;++j)
					{
					++i;
					wtSize[j]=(unsigned int)(atoi(argv[i]));
					}
				}
			else if(strcasecmp(argv[i]+1,"dds")==0)
				{
				++i;
				demDistScale=float(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"wi")==0)
				{
				++i;
				windowIndex=atoi(argv[i]);
				
				/* Extend the list of render settings if an index beyond the end is selected: */
				while(int(renderSettings.size())<=windowIndex)
					renderSettings.push_back(renderSettings.back());
				
				/* Disable fixed projector view on the new render settings: */
				renderSettings.back().fixProjectorView=false;
				}
			else if(strcasecmp(argv[i]+1,"fpv")==0)
				{
				renderSettings.back().fixProjectorView=true;
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Load the projector transformation file specified in the next argument: */
					++i;
					renderSettings.back().loadProjectorTransform(argv[i]);
					}
				}
			else if(strcasecmp(argv[i]+1,"nhs")==0)
				renderSettings.back().hillshade=false;
			else if(strcasecmp(argv[i]+1,"uhs")==0)
				renderSettings.back().hillshade=true;
			else if(strcasecmp(argv[i]+1,"ns")==0)
				renderSettings.back().useShadows=false;
			else if(strcasecmp(argv[i]+1,"us")==0)
				renderSettings.back().useShadows=true;
			else if(strcasecmp(argv[i]+1,"nhm")==0)
				{
				delete renderSettings.back().elevationColorMap;
				renderSettings.back().elevationColorMap=0;
				}
			else if(strcasecmp(argv[i]+1,"uhm")==0)
				{
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Load the height color map file specified in the next argument: */
					++i;
					renderSettings.back().loadHeightMap(argv[i]);
					}
				else
					{
					/* Load the default height color map: */
					renderSettings.back().loadHeightMap(CONFIG_DEFAULTHEIGHTCOLORMAPFILENAME);
					}
				}
			else if(strcasecmp(argv[i]+1,"ncl")==0)
				renderSettings.back().useContourLines=false;
			else if(strcasecmp(argv[i]+1,"ucl")==0)
				{
				renderSettings.back().useContourLines=true;
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Read the contour line spacing: */
					++i;
					renderSettings.back().contourLineSpacing=GLfloat(atof(argv[i]));
					}
				}
			else if(strcasecmp(argv[i]+1,"cp")==0)
				{
				++i;
				controlPipeName=argv[i];
				}
			else
				std::cerr<<"Ignoring unrecognized command line switch "<<argv[i]<<std::endl;
			}
		}
	
	/* Print usage help if requested: */
	if(printHelp)
		printUsage();
	
	if(frameFilePrefix!=0)
		{
		/* Open the selected pre-recorded 3D video files: */
		std::string colorFileName=frameFilePrefix;
		colorFileName.append(".color");
		std::string depthFileName=frameFilePrefix;
		depthFileName.append(".depth");
		camera=new Kinect::FileFrameSource(Vrui::openFile(colorFileName.c_str()),Vrui::openFile(depthFileName.c_str()));
		}
	else
		{
		/* Open the 3D camera device of the selected index: */
		Kinect::DirectFrameSource* realCamera=Kinect::openDirectFrameSource(cameraIndex);
		Misc::ConfigurationFileSection cameraConfigurationSection=cfg.getSection(cameraConfiguration.c_str());
		realCamera->configure(cameraConfigurationSection);
		camera=realCamera;
		}
	for(int i=0;i<2;++i)
	{
		frameSize[i]=camera->getActualFrameSize(Kinect::FrameSource::DEPTH)[i];
		//std::cout << "FRAME[" << i << "]: " << frameSize[i] << std::endl;  //LJ CHECK WHAT THE FRAME SIZE IS
		// MM: ^ it's 480 by 640
	}
	
	/* Get the camera's per-pixel depth correction parameters and evaluate it on the depth frame's pixel grid: */
	Kinect::FrameSource::DepthCorrection* depthCorrection=camera->getDepthCorrectionParameters();
	if(depthCorrection!=0)
		{
		pixelDepthCorrection=depthCorrection->getPixelCorrection(frameSize);
		delete depthCorrection;
		}
	else
		{
		/* Create dummy per-pixel depth correction parameters: */
		pixelDepthCorrection=new PixelDepthCorrection[frameSize[1]*frameSize[0]];
		PixelDepthCorrection* pdcPtr=pixelDepthCorrection;
		for(unsigned int y=0;y<frameSize[1];++y)
			for(unsigned int x=0;x<frameSize[0];++x,++pdcPtr)
				{
				pdcPtr->scale=1.0f;
				pdcPtr->offset=0.0f;
				}
		}
	
	/* Get the camera's intrinsic parameters: */
	cameraIps=camera->getIntrinsicParameters();
	
	/* MM: this will probably be needed for our project; just basic physical sandbox info? */
	/* Read the sandbox layout file: */
	Geometry::Plane<double,3> basePlane;
	Geometry::Point<double,3> basePlaneCorners[4];
	{
	IO::ValueSource layoutSource(Vrui::openFile(sandboxLayoutFileName.c_str()));
	layoutSource.skipWs();
	std::string s=layoutSource.readLine();
	basePlane=Misc::ValueCoder<Geometry::Plane<double,3> >::decode(s.c_str(),s.c_str()+s.length());
	basePlane.normalize();
	for(int i=0;i<4;++i)
		{
		layoutSource.skipWs();
		s=layoutSource.readLine();
		basePlaneCorners[i]=Misc::ValueCoder<Geometry::Point<double,3> >::decode(s.c_str(),s.c_str()+s.length());
		}
	}
	
	/* Limit the valid elevation range to the intersection of the extents of all height color maps: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->elevationColorMap!=0)
			{
			Math::Interval<double> mapRange(rsIt->elevationColorMap->getScalarRangeMin(),rsIt->elevationColorMap->getScalarRangeMax());
			elevationRange.intersectInterval(mapRange);
			}
	
	/* Scale all sizes by the given scale factor: */
	double sf=scale/100.0; // Scale factor from cm to final units
	for(int i=0;i<3;++i)
		for(int j=0;j<4;++j)
			cameraIps.depthProjection.getMatrix()(i,j)*=sf;
	basePlane=Geometry::Plane<double,3>(basePlane.getNormal(),basePlane.getOffset()*sf);
	for(int i=0;i<4;++i)
		for(int j=0;j<3;++j)
			basePlaneCorners[i][j]*=sf;
	if(elevationRange!=Math::Interval<double>::full)
		elevationRange*=sf;
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		{
		if(rsIt->elevationColorMap!=0)
			rsIt->elevationColorMap->setScalarRange(rsIt->elevationColorMap->getScalarRangeMin()*sf,rsIt->elevationColorMap->getScalarRangeMax()*sf);
		rsIt->contourLineSpacing*=sf;
		for(int i=0;i<4;++i)
			rsIt->projectorTransform.getMatrix()(i,3)*=sf;
		}
	demDistScale*=sf;
	
	/* Create the frame filter object: */
	frameFilter=new FrameFilter(frameSize,numAveragingSlots,pixelDepthCorrection,cameraIps.depthProjection,basePlane);
	frameFilter->setValidElevationInterval(cameraIps.depthProjection,basePlane,elevationRange.getMin(),elevationRange.getMax());
	frameFilter->setStableParameters(minNumSamples,maxVariance);
	frameFilter->setHysteresis(hysteresis);
	frameFilter->setSpatialFilter(true);
	frameFilter->setOutputFrameFunction(Misc::createFunctionCall(this,&Sandbox::receiveFilteredFrame));
	
	
	/* MM: this depth section will be necessary for our project bc it's just 3D space calcs, I think */
	/* Start streaming depth frames: */
	camera->startStreaming(0,Misc::createFunctionCall(this,&Sandbox::rawDepthFrameDispatcher));
	
	/* Create the depth image renderer: */
	depthImageRenderer=new DepthImageRenderer(frameSize);
	depthImageRenderer->setDepthProjection(cameraIps.depthProjection);
	depthImageRenderer->setBasePlane(basePlane);
	
	/* Calculate the transformation from camera space to sandbox space: */
	{
	ONTransform::Vector z=basePlane.getNormal();
	ONTransform::Vector x=(basePlaneCorners[1]-basePlaneCorners[0])+(basePlaneCorners[3]-basePlaneCorners[2]);
	ONTransform::Vector y=z^x;
	boxTransform=ONTransform::rotate(Geometry::invert(ONTransform::Rotation::fromBaseVectors(x,y)));
	ONTransform::Point center=Geometry::mid(Geometry::mid(basePlaneCorners[0],basePlaneCorners[1]),Geometry::mid(basePlaneCorners[2],basePlaneCorners[3]));
	boxTransform*=ONTransform::translateToOriginFrom(basePlane.project(center));
	}
	
	/* Calculate a bounding box around all potential surfaces: */
	bbox=Box::empty;
	for(int i=0;i<4;++i)
		{
		bbox.addPoint(basePlane.project(basePlaneCorners[i])+basePlane.getNormal()*elevationRange.getMin());
		bbox.addPoint(basePlane.project(basePlaneCorners[i])+basePlane.getNormal()*elevationRange.getMax());
		}
		
	/* Initialize all surface renderers: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		{
		/* Calculate the texture mapping plane for this renderer's height map: */
		if(rsIt->elevationColorMap!=0)
			{
			if(haveHeightMapPlane)
				rsIt->elevationColorMap->calcTexturePlane(heightMapPlane);
			else
				rsIt->elevationColorMap->calcTexturePlane(depthImageRenderer);
			}

	        // MM: is this where the image is first drawn?
		/* Initialize the surface renderer: */
		rsIt->surfaceRenderer=new SurfaceRenderer(depthImageRenderer);
		rsIt->surfaceRenderer->setDrawContourLines(rsIt->useContourLines);
		rsIt->surfaceRenderer->setContourLineDistance(rsIt->contourLineSpacing);
		rsIt->surfaceRenderer->setElevationColorMap(rsIt->elevationColorMap);
		rsIt->surfaceRenderer->setIlluminate(rsIt->hillshade);
		rsIt->surfaceRenderer->setDemDistScale(demDistScale);
		}
	
	// MM: this looks important
	/* Create the GUI: */
	mainMenu=createMainMenu();
	Vrui::setMainMenu(mainMenu);
	
	// MM: maybe need DEMTool. addEventTool? 
	/* Initialize the custom tool classes: */
	DEMTool::initClass(*Vrui::getToolManager());
	addEventTool("Pause Topography",0,0);
	
	if(!controlPipeName.empty())
		{
		/* Open the control pipe in non-blocking mode: */
		controlPipeFd=open(controlPipeName.c_str(),O_RDONLY|O_NONBLOCK);
		if(controlPipeFd<0)
			std::cerr<<"Unable to open control pipe "<<controlPipeName<<"; ignoring"<<std::endl;
		}
	
	/* Inhibit the screen saver: */
	Vrui::inhibitScreenSaver();
	
	/* Set the linear unit to support proper scaling: */
	Vrui::getCoordinateManager()->setUnit(Geometry::LinearUnit(Geometry::LinearUnit::METER,scale/100.0));
	
	/* Initialize the navigation transformation: */
	Vrui::Point::AffineCombiner cc;
	for(int i=0;i<4;++i)
		cc.addPoint(Vrui::Point(basePlane.project(basePlaneCorners[i])));
	navCenter=cc.getPoint();
	navSize=Vrui::Scalar(0);
	for(int i=0;i<4;++i)
		{
		Vrui::Scalar dist=Geometry::dist(Vrui::Point(basePlane.project(basePlaneCorners[i])),navCenter);
		if(navSize<dist)
			navSize=dist;
		}
	navUp=Geometry::normal(Vrui::Vector(basePlane.getNormal()));
	std::cout << "Done with Sandbox::Sandbox." << std::endl; // MM: added
	}

// MM: Sandbox destructor
Sandbox::~Sandbox(void)
	{
	/* Stop streaming depth frames: */
	camera->stopStreaming();
	delete camera;
	delete frameFilter;
	
	/* Delete helper objects: */
	delete depthImageRenderer;
	delete[] pixelDepthCorrection;
	
	delete mainMenu;
	
	close(controlPipeFd);
	}

void Sandbox::toolDestructionCallback(Vrui::ToolManager::ToolDestructionCallbackData* cbData)
	{
	/* Check if the destroyed tool is the active DEM tool: */
	if(activeDem==dynamic_cast<DEM*>(cbData->tool))
		{
		/* Deactivate the active DEM tool: */
		activeDem=0;
		}
	}

/* MM: a Vrui::Application method - necessary for us */
void Sandbox::frame(void)
	{
	  std::cout << "In Sandbox::frame." << std::endl; // MM: added
	/* Check if the filtered frame has been updated: */
	if(filteredFrames.lockNewValue())
		{
		// MM: update the depth map after sand has been moved?
		/* Update the depth image renderer's depth image: */
		depthImageRenderer->setDepthImage(filteredFrames.getLockedValue());
		}
	
	// MM: does this update the displayed image? or just the time?
	/* Update all surface renderers: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		rsIt->surfaceRenderer->setAnimationTime(Vrui::getApplicationTime());
	// getApplicationTime returns seconds since application was started
	
	/* MM: I think the following may just be for manually entered data. could be useful */
	/* Check if there is a control command on the control pipe: */
	if(controlPipeFd>=0)
		{
		/* Try reading a chunk of data (will fail with EAGAIN if no data due to non-blocking access): */
		char command[1024];
		ssize_t readResult=read(controlPipeFd,command,sizeof(command)-1);
		if(readResult>0)
			{
			command[readResult]='\0';
			
			/* Extract the command: */
			char* cPtr;
			for(cPtr=command;*cPtr!='\0'&&!isspace(*cPtr);++cPtr)
				;
			char* commandEnd=cPtr;
			
			/* Find the beginning of an optional command parameter: */
			while(*cPtr!='\0'&&isspace(*cPtr))
				++cPtr;
			char* parameter=cPtr;
			
			/* Find the end of the optional parameter list: */
			while(*cPtr!='\0')
				++cPtr;
			while(cPtr>parameter&&isspace(cPtr[-1]))
				--cPtr;
			*cPtr='\0';
			
			/* Parse the command: */
			*commandEnd='\0';
			// MM: this height map loading - is this just a cmdline option or is it regular?
			if(strcasecmp(command,"colorMap")==0)
				{
				try
					{
					/* Update all height color maps: */
					for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
						if(rsIt->elevationColorMap!=0)
							rsIt->elevationColorMap->load(parameter);
					}
				catch(std::runtime_error err)
					{
					std::cerr<<"Cannot read height color map "<<parameter<<" due to exception "<<err.what()<<std::endl;
					}
				}
			else if(strcasecmp(command,"heightMapPlane")==0)
				{
				/* Read the height map plane equation: */
				double hmp[4];
				char* endPtr=parameter;
				for(int i=0;i<4;++i)
					hmp[i]=strtod(endPtr,&endPtr);
				Plane heightMapPlane=Plane(Plane::Vector(hmp),hmp[3]);
				heightMapPlane.normalize();
				
				/* Override the height mapping planes of all elevation color maps: */
				for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
					if(rsIt->elevationColorMap!=0)
						rsIt->elevationColorMap->calcTexturePlane(heightMapPlane);
				}
			}
		}
		
	/* MM: Asks Vrui to update its internal state and redraw the VR windows 
               at the given application time; must be called from main thread */
	if(pauseUpdates)
		Vrui::scheduleUpdate(Vrui::getApplicationTime()+1.0/30.0);
	std::cout << "Done with Sandbox::frame." << std::endl; // MM: added
	}

/* MM: a Vrui::Application method - necessary for us */
void Sandbox::display(GLContextData& contextData) const
	{
	std::cout << "In Sandbox::display." << std::endl; // MM: added
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Get the rendering settings for this window: */
	const Vrui::DisplayState& ds=Vrui::getDisplayState(contextData);
	const Vrui::VRWindow* window=ds.window;
	int windowIndex;
	for(windowIndex=0;windowIndex<Vrui::getNumWindows()&&window!=Vrui::getWindow(windowIndex);++windowIndex)
		;
	const RenderSettings& rs=windowIndex<int(renderSettings.size())?renderSettings[windowIndex]:renderSettings.back();
	
	/* MM: necessary? I think so */
	/* Calculate the projection matrix: */
	PTransform projection=ds.projection;
	if(rs.fixProjectorView&&rs.projectorTransformValid)
		{
		/* Use the projector transformation instead: */
		projection=rs.projectorTransform;
		
		/* Multiply with the inverse modelview transformation so that lighting still works as usual: */
		projection*=Geometry::invert(ds.modelviewNavigational);
		}
		
  	        // MM: I think this is where the image is displayed through the projector
	        // (see SurfaceRenderer.cpp)
		{
		/* Render the surface in a single pass: */
		//rs.surfaceRenderer->renderSinglePass(ds.viewport,projection,ds.modelviewNavigational,contextData);
		}
	
	const Kinect::FrameBuffer& f = filteredFrames.getLockedValue();
	//std::cout << std::endl << "DEPTH IMAGE" << std::endl << f.getData<GLfloat>()[2] << std::endl << std::endl;  //LJ PRINT STUFF
	// MM: f.getData<GLfloat>() is a pointer to a list of 307200 floats (the frame is 480 by 640 in dimension)

	/*
	std::cout << std::endl;
	for (int row = 0; row < 480; row = row+50) {
	  for (int col = 0; col < 640; col+=50)
	    std::cout << f.getData<GLfloat>()[(row*640)+col] << "\t";
	  std::cout << std::endl;
	}
	*/
	
	std::cout << "Done with Sandbox::display." << std::endl; // MM: added
	}

/* MM: a Vrui::Application method, don't know what it does */
void Sandbox::resetNavigation(void)
	{
	/* Set the navigation transformation from the previously computed parameters: */
	Vrui::setNavigationTransformation(navCenter,navSize,navUp);
	}

/* MM: a Vrui::Application method, looks like an event handler from a button click or something */
void Sandbox::eventCallback(Vrui::Application::EventID eventId,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	if(cbData->newButtonState)
		{
		switch(eventId)
			{
			case 0:
				/* Invert the current pause setting: */
				pauseUpdates=!pauseUpdates;
				
				/* Update the main menu toggle: */
				pauseUpdatesToggle->setToggle(pauseUpdates);
				
				break;
			}
		}
	}

/* MM: a GLObject method - necessary for us */
void Sandbox::initContext(GLContextData& contextData) const
	{
	  std::cout << "In Sandbox::initContext." << std::endl; // MM: added
	/* Create a data item and add it to the context: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	{
	/* Save the currently bound frame buffer: */
	GLint currentFrameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
	
	/* MM: don't think we need any of the following shadow lines 
	///////////////////////////////////////////////////////// */
	/* Set the default shadow buffer size: */
	dataItem->shadowBufferSize[0]=1024;
	dataItem->shadowBufferSize[1]=1024;
	/* Generate the shadow rendering frame buffer: */
	glGenFramebuffersEXT(1,&dataItem->shadowFramebufferObject);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->shadowFramebufferObject);
	/* Generate a depth texture for shadow rendering: */
	glGenTextures(1,&dataItem->shadowDepthTextureObject);
	// MM: generates the specified number of texture objects and places their 
	// handles in the GLuint array pointer (the second parameter)
	glBindTexture(GL_TEXTURE_2D,dataItem->shadowDepthTextureObject); // MM: texture target and texture name
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE_ARB,GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC_ARB,GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D,GL_DEPTH_TEXTURE_MODE_ARB,GL_INTENSITY);
	glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24_ARB,dataItem->shadowBufferSize[0],dataItem->shadowBufferSize[1],0,GL_DEPTH_COMPONENT,GL_UNSIGNED_BYTE,0);
	glBindTexture(GL_TEXTURE_2D,0); // MM: texture target and texture name
	
	/* MM: without shadows, do we need this? */
	/* Attach the depth texture to the frame buffer object: */
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,dataItem->shadowDepthTextureObject,0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
	} 
	std::cout << "Done with Sandbox::initContext." << std::endl; // MM: added
	}

VRUI_APPLICATION_RUN(Sandbox)
/* MM: ^^ This goes to Vrui/Vrui/Application.h and calls run in
Vrui/Vrui/Application.cpp which calls mainLoop() in 
Vrui/Vrui/Internal/Vrui.Workbench.cpp. 

mainLoop does a bunch of set up and eventually calls either 
vruiInnerLoopMultiWindow or vruiInnerLoopSingleWindow,
which hold the actual while loops that run the whole process. */
