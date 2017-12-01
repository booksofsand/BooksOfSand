#ifndef KINECTHANDLER_H
#define KINECTHANDLER_H

#include "Config.h"
#include "sandboxwindow.h"
#include <iostream>
#include <QEventLoop>
#include <QTimerEvent>

// Includes from Sandbox.h
#include <Threads/TripleBuffer.h>
//#include <Geometry/Box.h>
//#include <Geometry/OrthonormalTransformation.h>
//#include <Geometry/ProjectiveTransformation.h>
/*
#include <GL/gl.h>
#include <GL/GLColorMap.h>
#include <GL/GLMaterial.h>
#include <GL/GLObject.h>
#include <GL/GLGeometryVertex.h>
#include <GLMotif/ToggleButton.h>
#include <GLMotif/TextFieldSlider.h>
*/
//LJ added
#include <IO/File.h>
#include <IO/ValueSource.h>

#include <Vrui/Tool.h>
#include <Vrui/GenericToolFactory.h>
#include <Vrui/TransparentObject.h>
#include <Vrui/Application.h>
#include <Kinect/FrameBuffer.h>
#include <Kinect/FrameSource.h>

#include "FrameFilter.h"
// Forward declarations from Sandbox.h
namespace Misc {
template <class ParameterParam>
class FunctionCall;
}
//Lj added
class FrameFilter;

namespace Vrui { //LJ added
class Lightsource;
}

namespace Kinect {
class Camera;
}

// Declarations for QT
class SandboxWindow;

class KinectHandler : public QEventLoop, public Vrui::Application
{
    Q_OBJECT
public:
    KinectHandler(int argc,char**& argv, SandboxWindow* box);
    void sendFrameBuffer(const Kinect::FrameBuffer& frameBuffer);

private:
  typedef Misc::FunctionCall<const Kinect::FrameBuffer&> OutputFrameFunction; // Type for functions called when a new output frame is ready
  // Type defs from Sandbox.h
  //typedef Geometry::Box<Scalar,3> Box; // Type for bounding boxes
  //typedef Geometry::OrthonormalTransformation<Scalar,3> ONTransform; // Type for rigid body transformations
  typedef Kinect::FrameSource::DepthCorrection::PixelCorrection PixelDepthCorrection; // Type for per-pixel depth correction factors
    FrameFilter* frameFilter;
    SandboxWindow* box;
    size_t currDepth; // MM: testing only
    Kinect::FrameBuffer frameBuffer;
    
    void timerEvent(QTimerEvent* event);
    void calcDepthsToDisplay(size_t depthsToDisplay[MAXROWS][MAXCOLS]);
    void rawDepthFrameDispatcher(const Kinect::FrameBuffer& frameBuffer);

    // MM: the following copied from Sandbox.h
    Kinect::FrameSource* camera; // The Kinect camera device
    unsigned int frameSize[2]; // Width and height of the camera's depth frames
    PixelDepthCorrection* pixelDepthCorrection; // Buffer of per-pixel depth correction coefficients // ???
    Kinect::FrameSource::IntrinsicParameters cameraIps; // Intrinsic parameters of the Kinect camera
    //FrameFilter* frameFilter; // Processing object to filter raw depth frames from the Kinect camera
    bool pauseUpdates; // Pauses updates of the topography
    Threads::TripleBuffer<Kinect::FrameBuffer> filteredFrames; //LJ added // Triple buffer for incoming filtered depth frames

    void receiveFilteredFrame(const Kinect::FrameBuffer& frameBuffer);
};

#endif // KINECTHANDLER_H
