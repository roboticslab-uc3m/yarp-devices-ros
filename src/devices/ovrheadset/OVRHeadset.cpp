/*
 * Copyright (C) 2015  iCub Facility, Istituto Italiano di Tecnologia
 * Author: Daniele E. Domenichelli <daniele.domenichelli@iit.it>
 *
 * CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 */


#include "OVRHeadset.h"
#include "InputCallback.h"
#include "TextureBuffer.h"

#include <yarp/os/BufferedPort.h>
#include <yarp/os/LogStream.h>
#include <yarp/os/Network.h>
#include <yarp/os/Stamp.h>
#include <yarp/os/Time.h>
#include <yarp/sig/Image.h>

#include <math.h>

#if defined(_WIN32)
 #define GLFW_EXPOSE_NATIVE_WIN32
 #define GLFW_EXPOSE_NATIVE_WGL
 #define OVR_OS_WIN32
#elif defined(__APPLE__)
 #define GLFW_EXPOSE_NATIVE_COCOA
 #define GLFW_EXPOSE_NATIVE_NSGL
 #define OVR_OS_MAC
#elif defined(__linux__)
 #define GLFW_EXPOSE_NATIVE_X11
 #define GLFW_EXPOSE_NATIVE_GLX
 #define OVR_OS_LINUX
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <OVR.h>
#include <OVR_System.h>
#include <OVR_CAPI_GL.h>


#ifdef check
// Undefine the check macro in AssertMacros.h on OSX or the "cfg.check"
// call will fail compiling.
#undef check
#endif


#define checkGlErrorMacro yarp::dev::OVRHeadset::checkGlError(__FILE__, __LINE__)


static void debugFov(const ovrFovPort fov[2]) {
    yDebug("             Left Eye                                           Right Eye\n");
    yDebug("LeftTan    %10f (%5f[rad] = %5f[deg])        %10f (%5f[rad] = %5f[deg])\n",
           fov[0].LeftTan,
           atan(fov[0].LeftTan),
           OVR::RadToDegree(atan(fov[0].LeftTan)),
           fov[1].LeftTan,
           atan(fov[1].LeftTan),
           OVR::RadToDegree(atan(fov[1].LeftTan)));
    yDebug("RightTan   %10f (%5f[rad] = %5f[deg])        %10f (%5f[rad] = %5f[deg])\n",
           fov[0].RightTan,
           atan(fov[0].RightTan),
           OVR::RadToDegree(atan(fov[0].RightTan)),
           fov[1].RightTan,
           atan(fov[1].RightTan),
           OVR::RadToDegree(atan(fov[1].RightTan)));
    yDebug("UpTan      %10f (%5f[rad] = %5f[deg])        %10f (%5f[rad] = %5f[deg])\n",
           fov[0].UpTan,
           atan(fov[0].UpTan),
           OVR::RadToDegree(atan(fov[0].UpTan)),
           fov[1].UpTan,
           atan(fov[1].UpTan),
           OVR::RadToDegree(atan(fov[1].UpTan)));
    yDebug("DownTan    %10f (%5f[rad] = %5f[deg])        %10f (%5f[rad] = %5f[deg])\n",
           fov[0].DownTan,
           atan(fov[0].DownTan),
           OVR::RadToDegree(atan(fov[0].DownTan)),
           fov[1].DownTan,
           atan(fov[0].DownTan),
           OVR::RadToDegree(atan(fov[0].DownTan)));
    yDebug("\n\n\n");
}

yarp::dev::OVRHeadset::OVRHeadset() :
        yarp::dev::DeviceDriver(),
        yarp::os::RateThread(13), // ~75 fps
        orientationPort(NULL),
        positionPort(NULL),
        angularVelocityPort(NULL),
        linearVelocityPort(NULL),
        angularAccelerationPort(NULL),
        linearAccelerationPort(NULL),
        predictedOrientationPort(NULL),
        predictedPositionPort(NULL),
        predictedAngularVelocityPort(NULL),
        predictedLinearVelocityPort(NULL),
        predictedAngularAccelerationPort(NULL),
        predictedLinearAccelerationPort(NULL),
        window(NULL),
        closed(false),
        distortionFrameIndex(0),
        multiSampleEnabled(false),
        overdriveEnabled(true),
        hqDistortionEnabled(true),
        flipInputEnabled(true),
        timeWarpEnabled(true),
        imagePoseEnabled(true),
        userPoseEnabled(false)
{
    yTrace();

    displayPorts[0] = NULL;
    displayPorts[1] = NULL;

    yarp::os::Time::turboBoost();
}

yarp::dev::OVRHeadset::~OVRHeadset()
{
    yTrace();
}

bool yarp::dev::OVRHeadset::open(yarp::os::Searchable& cfg)
{
    yTrace();

    orientationPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!orientationPort->open("/oculus/headpose/orientation:o")) {
        yError() << "Cannot open orientation port";
        this->close();
        return false;
    }
    orientationPort->setWriteOnly();

    positionPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!positionPort->open("/oculus/headpose/position:o")) {
        yError() << "Cannot open position port";
        this->close();
        return false;
    }
    positionPort->setWriteOnly();

    angularVelocityPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!angularVelocityPort->open("/oculus/headpose/angularVelocity:o")) {
        yError() << "Cannot open angular velocity port";
        this->close();
        return false;
    }
    angularVelocityPort->setWriteOnly();

    linearVelocityPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!linearVelocityPort->open("/oculus/headpose/linearVelocity:o")) {
        yError() << "Cannot open linear velocity port";
        this->close();
        return false;
    }
    linearVelocityPort->setWriteOnly();

    angularAccelerationPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!angularAccelerationPort->open("/oculus/headpose/angularAcceleration:o")) {
        yError() << "Cannot open angular acceleration port";
        this->close();
        return false;
    }
    angularAccelerationPort->setWriteOnly();

    linearAccelerationPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!linearAccelerationPort->open("/oculus/headpose/linearAcceleration:o")) {
        yError() << "Cannot open linear acceleration port";
        this->close();
        return false;
    }
    linearAccelerationPort->setWriteOnly();


    predictedOrientationPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!predictedOrientationPort->open("/oculus/predicted/headpose/orientation:o")) {
        yError() << "Cannot open predicted orientation port";
        this->close();
        return false;
    }
    predictedOrientationPort->setWriteOnly();

    predictedPositionPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!predictedPositionPort->open("/oculus/predicted/headpose/position:o")) {
        yError() << "Cannot open predicted position port";
        this->close();
        return false;
    }
    predictedPositionPort->setWriteOnly();

    predictedAngularVelocityPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!predictedAngularVelocityPort->open("/oculus/predicted/headpose/angularVelocity:o")) {
        yError() << "Cannot open predicted angular velocity port";
        this->close();
        return false;
    }
    predictedAngularVelocityPort->setWriteOnly();

    predictedLinearVelocityPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!predictedLinearVelocityPort->open("/oculus/predicted/headpose/linearVelocity:o")) {
        yError() << "Cannot open predicted linear velocity port";
        this->close();
        return false;
    }
    predictedLinearVelocityPort->setWriteOnly();

    predictedAngularAccelerationPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!predictedAngularAccelerationPort->open("/oculus/predicted/headpose/angularAcceleration:o")) {
        yError() << "Cannot open predicted angular acceleration port";
        this->close();
        return false;
    }
    predictedAngularAccelerationPort->setWriteOnly();

    predictedLinearAccelerationPort = new yarp::os::BufferedPort<yarp::os::Bottle>;
    if (!predictedLinearAccelerationPort->open("/oculus/predicted/headpose/linearAcceleration:o")) {
        yError() << "Cannot open predicted linear acceleration port";
        this->close();
        return false;
    }
    predictedLinearAccelerationPort->setWriteOnly();


    for (int i = 0; i < 2; ++i) {
        displayPorts[i] = new InputCallback(i);
        if (!displayPorts[i]->open(i == 0 ? "/oculus/display/left:i" : "/oculus/display/right:i")) {
            yError() << "Cannot open " << (i == 0 ? "left" : "right") << "display port";
            this->close();
            return false;
        }
        displayPorts[i]->setReadOnly();
    }


    texWidth  = cfg.check("w",    yarp::os::Value(640), "Texture width (usually same as camera width)").asInt();
    texHeight = cfg.check("h",    yarp::os::Value(480), "Texture height (usually same as camera height)").asInt();

    // TODO accept different fov for right and left eye?
    double hfov   = cfg.check("hfov", yarp::os::Value(105.),  "Camera horizontal field of view").asDouble();
    camHFOV[0] = hfov;
    camHFOV[1] = hfov;

    if (cfg.check("multisample", "[M] Enable multisample")) {
        multiSampleEnabled = true;
    }

    if (cfg.check("no-overdrive", "[O] Disable overdrive")) {
        overdriveEnabled = false;
    }

    if (cfg.check("no-hqdistortion", "[H] Disable high quality distortion")) {
        hqDistortionEnabled = false;
    }

    if (cfg.check("no-flipinput", "[F] Disable input flipping")) {
        flipInputEnabled = false;
    }

    if (cfg.check("no-timewarp", "[T] Disable timewarp")) {
        timeWarpEnabled = false;
    }

//    if (cfg.check("no-imagepose", "[I] Disable image pose")) {
//        imagePoseEnabled = false;
//    }
    imagePoseEnabled = false;
    if (cfg.check("imagepose", "[I] Enable image pose")) {
        imagePoseEnabled = true;
    }

    userPoseEnabled = false;
    if (cfg.check("userpose", "[U] Use user pose instead of camera pose")) {
        userPoseEnabled = true;
    }

//    userHeight = cfg.check("userHeight", yarp::os::Value(0.),  "User height").asDouble();


    prediction = cfg.check("prediction", yarp::os::Value(0.01), "Prediction [sec]").asDouble();

    displayPorts[0]->rollOffset  = static_cast<float>(cfg.check("left-roll-offset",   yarp::os::Value(0.0), "[LEFT_SHIFT+PAGE_UP][LEFT_SHIFT+PAGE_DOWN] Left eye roll offset").asDouble());
    displayPorts[0]->pitchOffset = static_cast<float>(cfg.check("left-pitch-offset",  yarp::os::Value(0.0), "[LEFT_SHIFT+UP_ARROW][LEFT_SHIFT+DOWN_ARROW] Left eye pitch offset").asDouble());
    displayPorts[0]->yawOffset   = static_cast<float>(cfg.check("left-yaw-offset",    yarp::os::Value(0.0), "[LEFT_SHIFT+LEFT_ARROW][LEFT_SHIFT+RIGHT_ARROW] Left eye yaw offset").asDouble());
    displayPorts[1]->rollOffset  = static_cast<float>(cfg.check("right-roll-offset",  yarp::os::Value(0.0), "[RIGHT_SHIFT+PAGE_UP][RIGHT_SHIFT+PAGE_DOWN] Right eye roll offset").asDouble());
    displayPorts[1]->pitchOffset = static_cast<float>(cfg.check("right-pitch-offset", yarp::os::Value(0.0), "[RIGHT_SHIFT+UP_ARROW][RIGHT_SHIFT+DOWN_ARROW] Right eye pitch offset").asDouble());
    displayPorts[1]->yawOffset   = static_cast<float>(cfg.check("right-yaw-offset",   yarp::os::Value(0.0), "[RIGHT_SHIFT+LEFT_ARROW][RIGHT_SHIFT+RIGHT_ARROW] Right eye yaw offset").asDouble());

    // Start the thread
    if (!this->start()) {
        yError() << "thread start failed, aborting.";
        this->close();
        return false;
    }

    // Enable display port callbacks
    for (int i=0; i<2; i++) {
        displayPorts[i]->useCallback();
    }

    return true;
}

bool yarp::dev::OVRHeadset::threadInit()
{
    yTrace();

    OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));

    ovrInitParams params;
    params.Flags = 0;
    params.RequestedMinorVersion = 0;
    params.LogCallback = ovrDebugCallback;
    params.ConnectionTimeoutMS = 0;

    //Initialise rift
    if (!ovr_Initialize(&params)) {
        yError() << "Unable to initialize LibOVR. LibOVRRT not found?";
        this->close();
        return false;
    }

    // Detect and initialize Oculus Rift
    hmd = ovrHmd_Create(0);
    if (hmd) {
        yInfo() << "Oculus Rift FOUND.";
    } else {
        yWarning() << "Oculus Rift NOT FOUND. Using debug device.";
        hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
    }

    // If still not existing, we failed initializing it.
    if (!hmd) {
        yError() << "Oculus Rift not detected.";
        this->close();
        return false;
    }

    if (hmd->ProductName[0] == '\0') {
        yWarning() << "Rift detected, display not enabled.";
    }

    DebugHmd(hmd);


    // Initialize the GLFW system for creating and positioning windows
    // GLFW must be initialized after LibOVR
    // see http://www.glfw.org/docs/latest/rift.html
    if( !glfwInit() ) {
        yError() << "Failed to initialize GLFW";
        this->close();
        return false;
    }
    glfwSetErrorCallback(glfwErrorCallback);

//    bool windowed = (hmd->HmdCaps & ovrHmdCap_ExtendDesktop) ? false : true;
    OVR::Sizei windowSize = hmd->Resolution;


    if (!createWindow(windowSize.w, windowSize.h, hmd->WindowsPos.x, hmd->WindowsPos.y)) {
        yError() << "Failed to create window";
        this->close();
        return false;
    }


    // Initialize the GLEW OpenGL 3.x bindings
    // GLEW must be initialized after creating the window
    glewExperimental=GL_TRUE;
    GLenum err = glewInit();
    if(err != GLEW_OK) {
        yError() << "glewInit failed, aborting.";
        this->close();
        return false;
    }
    yInfo() << "Using GLEW" << (const char*)glewGetString(GLEW_VERSION);
    checkGlErrorMacro;


    int fbwidth, fbheight;
    glfwGetFramebufferSize(window, &fbwidth, &fbheight);


    config.OGL.Header.API              = ovrRenderAPI_OpenGL;
    config.OGL.Header.BackBufferSize.w = fbwidth;
    config.OGL.Header.BackBufferSize.h = fbheight;
    config.OGL.Header.Multisample      = (multiSampleEnabled ? 1 : 0);
#if defined(_WIN32)
    config.OGL.Window = glfwGetWin32Window(window);
    config.OGL.DC     = GetDC(glfwGetWin32Window(window));
#elif defined(__APPLE__)
#elif defined(__linux__)
    config.OGL.Disp = glfwGetX11Display();
#endif

    for (int i = 0; i < 2; ++i) {
        camWidth[i] = texWidth;
        camHeight[i] = texHeight;
    }
    reconfigureFOV();

    reconfigureRendering();

    ovrHmd_SetEnabledCaps(hmd, ovrHmdCap_LowPersistence |
                               ovrHmdCap_DynamicPrediction);

#if defined(_WIN32)
    ovrHmd_AttachToWindow(hmd, glfwGetWin32Window(window), NULL, NULL);
#endif

    ovrHmd_DismissHSWDisplay(hmd);


    for (int i=0; i<2; i++)
    {
        displayPorts[i]->eyeRenderTexture = new TextureBuffer(texWidth, texHeight, i);
    }

    // Start the sensor which provides the Rift's pose and motion.
    ovrHmd_ConfigureTracking(hmd, ovrTrackingCap_Orientation |
                                  ovrTrackingCap_MagYawCorrection |
                                  ovrTrackingCap_Position , 0);

    // Recenter position
    ovrHmd_RecenterPose(hmd);

    checkGlErrorMacro;

    return true;
}

void yarp::dev::OVRHeadset::threadRelease()
{
    yTrace();

    // Ensure that threadRelease is not called twice
    if (closed) {
        return;
    }
    closed = true;

    // Shut down GLFW
    glfwTerminate();

    // Shut down LibOVR
    if (hmd) {
        ovrHmd_Destroy(hmd);
        hmd = 0;
        ovr_Shutdown();
    }

    if (orientationPort) {
        orientationPort->interrupt();
        orientationPort->close();
        delete orientationPort;
        orientationPort = NULL;
    }
    if (positionPort) {
        positionPort->interrupt();
        positionPort->close();
        delete positionPort;
        positionPort = NULL;
    }
    if (angularVelocityPort) {
        angularVelocityPort->interrupt();
        angularVelocityPort->close();
        delete angularVelocityPort;
        angularVelocityPort = NULL;
    }
    if (linearVelocityPort) {
        linearVelocityPort->interrupt();
        linearVelocityPort->close();
        delete linearVelocityPort;
        linearVelocityPort = NULL;
    }
    if (angularAccelerationPort) {
        angularAccelerationPort->interrupt();
        angularAccelerationPort->close();
        delete angularAccelerationPort;
        angularAccelerationPort = NULL;
    }
    if (linearAccelerationPort) {
        linearAccelerationPort->interrupt();
        linearAccelerationPort->close();
        delete linearAccelerationPort;
        linearAccelerationPort = NULL;
    }


    if (predictedOrientationPort) {
        predictedOrientationPort->interrupt();
        predictedOrientationPort->close();
        delete predictedOrientationPort;
        predictedOrientationPort = NULL;
    }
    if (predictedPositionPort) {
        predictedPositionPort->interrupt();
        predictedPositionPort->close();
        delete predictedPositionPort;
        predictedPositionPort = NULL;
    }
    if (predictedAngularVelocityPort) {
        predictedAngularVelocityPort->interrupt();
        predictedAngularVelocityPort->close();
        delete predictedAngularVelocityPort;
        predictedAngularVelocityPort = NULL;
    }
    if (predictedLinearVelocityPort) {
        predictedLinearVelocityPort->interrupt();
        predictedLinearVelocityPort->close();
        delete predictedLinearVelocityPort;
        predictedLinearVelocityPort = NULL;
    }
    if (predictedAngularAccelerationPort) {
        predictedAngularAccelerationPort->interrupt();
        predictedAngularAccelerationPort->close();
        delete predictedAngularAccelerationPort;
        predictedAngularAccelerationPort = NULL;
    }
    if (predictedLinearAccelerationPort) {
        predictedLinearAccelerationPort->interrupt();
        predictedLinearAccelerationPort->close();
        delete predictedLinearAccelerationPort;
        predictedLinearAccelerationPort = NULL;
    }


    for (int i = 0; i < 2; ++i) {
        if (displayPorts[i]) {
            displayPorts[i]->disableCallback();
            displayPorts[i]->interrupt();
            displayPorts[i]->close();
            delete displayPorts[i];
            displayPorts[i] = NULL;
        }
    }
}


bool yarp::dev::OVRHeadset::close()
{
    yTrace();
    this->askToStop();
    return true;
}

bool yarp::dev::OVRHeadset::startService()
{
    yTrace();
    return false;
}

bool yarp::dev::OVRHeadset::updateService()
{
    if (closed) {
        return false;
    }

    const double delay = 5.0;
    yDebug("Thread ran %d times, est period %lf[ms], used %lf[ms]",
           getIterations(),
           getEstPeriod(),
           getEstUsed());
    yDebug("Display refresh: %3.1f[hz]", getIterations()/delay);

    for (int i = 0; i < 2; ++i) {
        yDebug("%s eye: %3.1f[hz] - %d of %d frames missing, %d of %d frames dropped",
               (i == 0 ? "Left " : "Right"),
               (getIterations() - displayPorts[i]->eyeRenderTexture->missingFrames) / delay,
               displayPorts[i]->eyeRenderTexture->missingFrames,
               getIterations(),
               displayPorts[i]->droppedFrames,
               getIterations());
        displayPorts[i]->eyeRenderTexture->missingFrames = 0;
        getIterations(),
        displayPorts[i]->droppedFrames = 0;
    }

    resetStat();

    yarp::os::Time::delay(delay);
    return !closed;
}

bool yarp::dev::OVRHeadset::stopService()
{
    yTrace();
    return this->close();
}

// static void debugPose(const ovrPosef headpose,  ovrPosef EyeRenderPose[2])
// {
//     float head[3];
//     float eye0[3];
//     float eye1[3];
//
//     OVR::Quatf horientation = headpose.Orientation;
//     OVR::Quatf e0orientation = EyeRenderPose[0].Orientation;
//     OVR::Quatf e1orientation = EyeRenderPose[1].Orientation;
//
//     horientation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&head[0], &head[1], &head[2]);
//     e0orientation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&eye0[0], &eye0[1], &eye0[2]);
//     e1orientation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&eye1[0], &eye1[1], &eye1[2]);
//
//     double iod0 = sqrt(pow(2, EyeRenderPose[0].Position.x - headpose.Position.x) +
//                        pow(2, EyeRenderPose[0].Position.y - headpose.Position.y) +
//                        pow(2, EyeRenderPose[0].Position.z - headpose.Position.z));
//     double iod1 = sqrt(pow(2, EyeRenderPose[1].Position.x - headpose.Position.x) +
//                        pow(2, EyeRenderPose[1].Position.y - headpose.Position.y) +
//                        pow(2, EyeRenderPose[1].Position.z - headpose.Position.z));
//
//     yDebug("head    yaw: %f, pitch: %f, roll: %f, x: %f, y: %f, z: %f\n", head[0], head[1], head[2], headpose.Position.x,  headpose.Position.y, headpose.Position.z);
//     yDebug("eye0         %f,        %f,       %f     %f     %f     %f\n", eye0[0], eye0[1], eye0[2], EyeRenderPose[0].Position.x,  EyeRenderPose[0].Position.y, EyeRenderPose[0].Position.z);
//     yDebug("eye1         %f,        %f,       %f     %f     %f     %f     %f\n\n", eye1[0], eye1[1], eye1[2], EyeRenderPose[1].Position.x,  EyeRenderPose[1].Position.y, EyeRenderPose[1].Position.z, iod1 - iod0);
// }


// static void debugPose(const ovrPosef pose, const char* name = "")
// {
//     float roll, pitch, yaw;
//     OVR::Quatf orientation = pose.Orientation;
//     orientation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&roll, &pitch, &yaw);
//     yDebug("%s    yaw: %f, pitch: %f, roll: %f, x: %f, y: %f, z: %f\n",
//            name,
//            roll,
//            pitch,
//            yaw,
//            pose.Position.x,
//            pose.Position.y,
//            pose.Position.z);
// }


void yarp::dev::OVRHeadset::run()
{
    if (glfwWindowShouldClose(window)) {
        close();
        return;
    }

    // Check window events;
    glfwPollEvents();

    // Begin frame
    ++distortionFrameIndex;
    ovrFrameTiming frameTiming = ovrHmd_BeginFrame(hmd, distortionFrameIndex);

    // Query the HMD for the current tracking state.
    ovrTrackingState ts = ovrHmd_GetTrackingState(hmd, ovr_GetTimeInSeconds());
    ovrPoseStatef headpose = ts.HeadPose;
    yarp::os::Stamp stamp(distortionFrameIndex, ts.HeadPose.TimeInSeconds);

    //Get eye poses, feeding in correct IPD offset
    ovrVector3f ViewOffset[2] = {EyeRenderDesc[0].HmdToEyeViewOffset,EyeRenderDesc[1].HmdToEyeViewOffset};
    ovrPosef EyeRenderPose[2];
    ovrTrackingState ts_eyes;
    ovrHmd_GetEyePoses(hmd, distortionFrameIndex, ViewOffset, EyeRenderPose, &ts_eyes);


    // Query the HMD for the predicted state
    ovrTrackingState predicted_ts = ovrHmd_GetTrackingState(hmd, ovr_GetTimeInSeconds() + prediction);
    ovrPoseStatef predicted_headpose = predicted_ts.HeadPose;
    yarp::os::Stamp predicted_stamp(distortionFrameIndex, predicted_ts.HeadPose.TimeInSeconds);


    // Read orientation and write it on the port
    if (ts.StatusFlags & ovrStatus_OrientationTracked) {

        if (orientationPort->getOutputCount() > 0) {
            OVR::Quatf orientation = headpose.ThePose.Orientation;
            float yaw, pitch, roll;
            orientation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yaw, &pitch, &roll);
            yarp::os::Bottle& output_orientation = orientationPort->prepare();
            output_orientation.clear();
            output_orientation.addDouble(OVR::RadToDegree(pitch));
            output_orientation.addDouble(OVR::RadToDegree(-roll));
            output_orientation.addDouble(OVR::RadToDegree(yaw));
            orientationPort->setEnvelope(stamp);
            orientationPort->write();
        }

        if (angularVelocityPort->getOutputCount() > 0) {
            yarp::os::Bottle& output_angularVelocity = angularVelocityPort->prepare();
            output_angularVelocity.addDouble(OVR::RadToDegree(headpose.AngularVelocity.x));
            output_angularVelocity.addDouble(OVR::RadToDegree(headpose.AngularVelocity.y));
            output_angularVelocity.addDouble(OVR::RadToDegree(headpose.AngularVelocity.z));
            angularVelocityPort->setEnvelope(stamp);
            angularVelocityPort->write();
        }

        if (angularAccelerationPort->getOutputCount() > 0) {
            yarp::os::Bottle& output_angularAcceleration = angularAccelerationPort->prepare();
            output_angularAcceleration.addDouble(OVR::RadToDegree(headpose.AngularAcceleration.x));
            output_angularAcceleration.addDouble(OVR::RadToDegree(headpose.AngularAcceleration.y));
            output_angularAcceleration.addDouble(OVR::RadToDegree(headpose.AngularAcceleration.z));
            angularAccelerationPort->setEnvelope(stamp);
            angularAccelerationPort->write();
        }

    } else {
        // Do not warn more than once every 5 seconds
        static double lastOrientWarnTime = 0;
        double now = yarp::os::Time::now();
        if(now >= lastOrientWarnTime + 5) {
            yDebug() << "Orientation not tracked";
            lastOrientWarnTime = now;
        }
    }

    // Read position and write it on the port
    if (ts.StatusFlags & ovrStatus_PositionTracked) {

        if (positionPort->getOutputCount() > 0) {
            OVR::Vector3f position = headpose.ThePose.Position;
            yarp::os::Bottle& output_position = positionPort->prepare();
            output_position.clear();
            output_position.addDouble(position[0]);
            output_position.addDouble(position[1]);
            output_position.addDouble(position[2]);
            positionPort->setEnvelope(stamp);
            positionPort->write();
        }

        if (linearVelocityPort->getOutputCount() > 0) {
            yarp::os::Bottle& output_linearVelocity = linearVelocityPort->prepare();
            output_linearVelocity.addDouble(headpose.LinearVelocity.x);
            output_linearVelocity.addDouble(headpose.LinearVelocity.y);
            output_linearVelocity.addDouble(headpose.LinearVelocity.z);
            linearVelocityPort->setEnvelope(stamp);
            linearVelocityPort->write();
        }

        if (linearAccelerationPort->getOutputCount() > 0) {
            yarp::os::Bottle& output_linearAcceleration = linearAccelerationPort->prepare();
            output_linearAcceleration.addDouble(headpose.LinearAcceleration.x);
            output_linearAcceleration.addDouble(headpose.LinearAcceleration.y);
            output_linearAcceleration.addDouble(headpose.LinearAcceleration.z);
            linearAccelerationPort->setEnvelope(stamp);
            linearAccelerationPort->write();
        }

    } else {
        // Do not warn more than once every 5 seconds
        static double lastPosWarnTime = 0;
        double now = yarp::os::Time::now();
        if(now >= lastPosWarnTime + 5) {
            yDebug() << "Position not tracked";
            lastPosWarnTime = now;
        }
    }

    // Read predicted orientation and write it on the port
    if (predicted_ts.StatusFlags & ovrStatus_OrientationTracked) {

        if (predictedOrientationPort->getOutputCount() > 0) {
            OVR::Quatf orientation = predicted_headpose.ThePose.Orientation;
            float yaw, pitch, roll;
            orientation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yaw, &pitch, &roll);
            yarp::os::Bottle& output_orientation = predictedOrientationPort->prepare();
            output_orientation.clear();
            output_orientation.addDouble(OVR::RadToDegree(pitch));
            output_orientation.addDouble(OVR::RadToDegree(-roll));
            output_orientation.addDouble(OVR::RadToDegree(yaw));
            predictedOrientationPort->setEnvelope(predicted_stamp);
            predictedOrientationPort->write();
        }

        if (predictedAngularVelocityPort->getOutputCount() > 0) {
            yarp::os::Bottle& output_angularVelocity = predictedAngularVelocityPort->prepare();
            output_angularVelocity.addDouble(OVR::RadToDegree(predicted_headpose.AngularVelocity.x));
            output_angularVelocity.addDouble(OVR::RadToDegree(predicted_headpose.AngularVelocity.y));
            output_angularVelocity.addDouble(OVR::RadToDegree(predicted_headpose.AngularVelocity.z));
            predictedAngularVelocityPort->setEnvelope(predicted_stamp);
            predictedAngularVelocityPort->write();
        }

        if (predictedAngularAccelerationPort->getOutputCount() > 0) {
            yarp::os::Bottle& output_angularAcceleration = predictedAngularAccelerationPort->prepare();
            output_angularAcceleration.addDouble(OVR::RadToDegree(predicted_headpose.AngularAcceleration.x));
            output_angularAcceleration.addDouble(OVR::RadToDegree(predicted_headpose.AngularAcceleration.y));
            output_angularAcceleration.addDouble(OVR::RadToDegree(predicted_headpose.AngularAcceleration.z));
            predictedAngularAccelerationPort->setEnvelope(predicted_stamp);
            predictedAngularAccelerationPort->write();
        }

    } else {
        // Do not warn more than once every 5 seconds
        static double lastPredOrientWarnTime = 0;
        double now = yarp::os::Time::now();
        if(now >= lastPredOrientWarnTime + 5) {
            yDebug() << "Predicted orientation not tracked";
            lastPredOrientWarnTime = now;
        }
    }

    // Read predicted position and write it on the port
    if (predicted_ts.StatusFlags & ovrStatus_PositionTracked) {

        if (predictedPositionPort->getOutputCount() > 0) {
            OVR::Vector3f position = predicted_headpose.ThePose.Position;
            yarp::os::Bottle& output_position = predictedPositionPort->prepare();
            output_position.clear();
            output_position.addDouble(position[0]);
            output_position.addDouble(position[1]);
            output_position.addDouble(position[2]);
            predictedPositionPort->setEnvelope(predicted_stamp);
            predictedPositionPort->write();
        }

        if (predictedLinearVelocityPort->getOutputCount() > 0) {
            yarp::os::Bottle& output_linearVelocity = predictedLinearVelocityPort->prepare();
            output_linearVelocity.addDouble(predicted_headpose.LinearVelocity.x);
            output_linearVelocity.addDouble(predicted_headpose.LinearVelocity.y);
            output_linearVelocity.addDouble(predicted_headpose.LinearVelocity.z);
            predictedLinearVelocityPort->setEnvelope(predicted_stamp);
            predictedLinearVelocityPort->write();
        }

        if (predictedLinearAccelerationPort->getOutputCount() > 0) {
            yarp::os::Bottle& output_linearAcceleration = predictedLinearAccelerationPort->prepare();
            output_linearAcceleration.addDouble(predicted_headpose.LinearAcceleration.x);
            output_linearAcceleration.addDouble(predicted_headpose.LinearAcceleration.y);
            output_linearAcceleration.addDouble(predicted_headpose.LinearAcceleration.z);
            predictedLinearAccelerationPort->setEnvelope(predicted_stamp);
            predictedLinearAccelerationPort->write();
        }

    } else {
        // Do not warn more than once every 5 seconds
        static double lastPredPosWarnTime = 0;
        double now = yarp::os::Time::now();
        if(now >= lastPredPosWarnTime + 5) {
            yDebug() << "Position not tracked";
            lastPredPosWarnTime = now;
        }
    }


    if(displayPorts[0]->eyeRenderTexture && displayPorts[1]->eyeRenderTexture) {
        // Do distortion rendering, Present and flush/sync
        ovrGLTexture eyeTex[2];
        for (int i = 0; i<2; ++i) {
            eyeTex[i].OGL.Header.API = ovrRenderAPI_OpenGL;
            eyeTex[i].OGL.Header.TextureSize = OVR::Sizei(displayPorts[i]->eyeRenderTexture->width, displayPorts[i]->eyeRenderTexture->height);
            eyeTex[i].OGL.Header.RenderViewport = OVR::Recti(0, 0, displayPorts[i]->eyeRenderTexture->width, displayPorts[i]->eyeRenderTexture->height);
            eyeTex[i].OGL.TexId = displayPorts[i]->eyeRenderTexture->texId;
        }

        // Wait till time-warp point to reduce latency.
        ovr_WaitTillTime(frameTiming.TimewarpPointSeconds - 0.001);

        //static double ttt =yarp::os::Time::now();
        //yDebug () << yarp::os::Time::now() - ttt;
        //ttt = yarp::os::Time::now();
        // Update the textures
        for (int i = 0; i<2; i++) {
            displayPorts[i]->eyeRenderTexture->update();
        }


        for (int i = 0; i<2; i++) {
            if (imagePoseEnabled) {
                if (userPoseEnabled) {
                    // Use orientation read from the HMD at the beginning of the frame
                    EyeRenderPose[i].Orientation = headpose.ThePose.Orientation;
                } else {
                    // Use orientation received from the image
                    EyeRenderPose[i].Orientation = displayPorts[i]->eyeRenderTexture->eyePose.Orientation;
                }
            } else {
                EyeRenderPose[i].Orientation.w = -1.0f;
                EyeRenderPose[i].Orientation.x = 0.0f;
                EyeRenderPose[i].Orientation.y = 0.0f;
                EyeRenderPose[i].Orientation.z = 0.0f;
            }
        }

//        if (userHeight != 0) {
//            for (int i = 0; i<2; i++) {
//                EyeRenderPose[i].Position.y += static_cast<float>(userHeight);
//            }
//        }


//          for (int i = 0; i<2; i++) {
//              debugPose(displayPorts[i]->eyeRenderTexture->eyePose, (i==0?"camera left":"camera right"));
//              debugPose(EyeRenderPose[i], (i==0?"render left":"render right"));
//          }

        // If the image size is different from the texture size,
        bool needReconfigureFOV = false;
        for (int i = 0; i<2; i++) {
            if ((displayPorts[i]->eyeRenderTexture->imageWidth != 0 && displayPorts[i]->eyeRenderTexture->imageWidth != camWidth[i]) ||
                (displayPorts[i]->eyeRenderTexture->imageHeight != 0 && displayPorts[i]->eyeRenderTexture->imageHeight != camHeight[i])) {

                camWidth[i] = displayPorts[i]->eyeRenderTexture->imageWidth;
                camHeight[i] = displayPorts[i]->eyeRenderTexture->imageHeight;
                needReconfigureFOV = true;
            }
        }
        if (needReconfigureFOV) {
            reconfigureFOV();
            reconfigureRendering();
        }

        // End frame
        ovrHmd_EndFrame(hmd, EyeRenderPose, &eyeTex[0].Texture);
    } else {
        // Do not warn more than once every 5 seconds
        static double lastImgWarnTime = 0;
        double now = yarp::os::Time::now();
        if(now >= lastImgWarnTime + 5) {
            yDebug() << "No image received";
            lastImgWarnTime = now;
        }
    }
}

GLFWmonitor* yarp::dev::OVRHeadset::detectMonitor()
{
    int count;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    for (int i = 0;  i < count;  i++) {
#if defined(_WIN32)
        if (strcmp(glfwGetWin32Monitor(monitors[i]), hmd->DisplayDeviceName) == 0) {
            return monitors[i];
        }
#elif defined(__APPLE__)
        if (glfwGetCocoaMonitor(monitors[i]) == hmd->DisplayId) {
            return monitors[i];
        }
#elif defined(__linux__)
        int xpos, ypos;
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        glfwGetMonitorPos(monitors[i], &xpos, &ypos);
        // NOTE on linux the screen is rotated, so we must match hmd width with
        //      screen height and vice versa
        if (hmd->WindowsPos.x == xpos &&
            hmd->WindowsPos.y == ypos &&
            hmd->Resolution.w == mode->height &&
            hmd->Resolution.h == mode->width) {
            return monitors[i];
        }
#endif
    }
    return NULL;
}


bool yarp::dev::OVRHeadset::createWindow(int w, int h, int x, int y)
{
    yTrace();

#if !defined(_WIN32)
    GLFWmonitor* monitor = detectMonitor();
    if (monitor) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        glfwWindowHint(GLFW_DECORATED, GL_FALSE);
    } else {
       yWarning() << "Could not detect monitor";
    }
#endif

    glfwWindowHint(GLFW_DEPTH_BITS, 16);

#if defined(_WIN32)
    window = glfwCreateWindow(w/2, h/2, "YARP Oculus", NULL, NULL);
#elif !defined(__linux__)
    window = glfwCreateWindow(w, h, "YARP Oculus", monitor, NULL);
#else
    // On linux, the display is rotated
    if (monitor) {
        window = glfwCreateWindow(h, w, "YARP Oculus", monitor, NULL);
    } else {
        // Using debug hmd
        window = glfwCreateWindow(w/2, h/2, "YARP Oculus", monitor, NULL);
    }
#endif

    if (!window) {
        yError() << "Could not create window";
        return false;
    }

    glfwSetWindowUserPointer(window, this);
#if !defined(_WIN32)
    glfwSetWindowPos(window, x, y);
#endif
    glfwSetKeyCallback(window, glfwKeyCallback);
    glfwMakeContextCurrent(window);

    return true;
}



void yarp::dev::OVRHeadset::onKey(int key, int scancode, int action, int mods)
{
    yTrace();

    if (GLFW_PRESS != action) {
        return;
    }

    bool leftShiftPressed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
    bool rightShiftPressed = (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
    bool leftCtrlPressed = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS);
    bool rightCtrlPressed = (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

    switch (key) {
    case GLFW_KEY_R:

        if (!leftShiftPressed && !rightShiftPressed) {
            yDebug() << "Recentering pose";
            ovrHmd_RecenterPose(hmd);
        } else {
            yDebug() << "Resetting yaw offset to current position";
            for (int i = 0; i < 2; ++i) {
                float iyaw, ipitch, iroll;
                if (imagePoseEnabled) {
                    OVR::Quatf imageOrientation = displayPorts[i]->eyeRenderTexture->eyePose.Orientation;
                    imageOrientation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&iyaw, &ipitch, &iroll);
                } else {
                    iyaw = 0.0f;
                    ipitch = 0.0f;
                    iyaw = 0.0f;
                }

                iyaw -= displayPorts[i]->yawOffset;
                displayPorts[i]->yawOffset = - iyaw;
                yDebug() << (i==0? "Left" : "Right") << "eye yaw offset =" << displayPorts[i]->yawOffset;
            }
        }
        break;
    case GLFW_KEY_T:
        timeWarpEnabled = !timeWarpEnabled;
        yDebug() << "Timewarp" << (timeWarpEnabled ? "ON" : "OFF");
        reconfigureRendering();
        break;
    case GLFW_KEY_M:
        multiSampleEnabled = !multiSampleEnabled;
        yDebug() << "Multisample" << (multiSampleEnabled ? "ON" : "OFF");
        reconfigureRendering();
        break;
    case GLFW_KEY_F:
        flipInputEnabled = !flipInputEnabled;
        yDebug() << "Flip input" << (flipInputEnabled ? "ON" : "OFF");
        reconfigureRendering();
        break;
    case GLFW_KEY_H:
        hqDistortionEnabled = !hqDistortionEnabled;
        yDebug() << "High quality distortion" << (hqDistortionEnabled ? "ON" : "OFF");
        reconfigureRendering();
        break;
    case GLFW_KEY_O:
        overdriveEnabled = !overdriveEnabled;
        yDebug() << "Overdrive" << (overdriveEnabled ? "ON" : "OFF");
        reconfigureRendering();
        break;
    case GLFW_KEY_I:
        imagePoseEnabled = !imagePoseEnabled;
        yDebug() << "Image pose" << (imagePoseEnabled ? "ON" : "OFF");
        break;
    case GLFW_KEY_U:
        userPoseEnabled = !userPoseEnabled;
        yDebug() << "User pose" << (userPoseEnabled ? "ON" : "OFF");
        break;
    case GLFW_KEY_ESCAPE:
        this->close();
        break;
    case GLFW_KEY_Z:
        if (!rightShiftPressed) {
            --camHFOV[0];
            yDebug() << "Left eye HFOV =" << camHFOV[0];
        }
        if (!leftShiftPressed) {
            --camHFOV[1];
            yDebug() << "Right eye HFOV =" << camHFOV[1];
        }
        reconfigureFOV();
        reconfigureRendering();
        break;
    case GLFW_KEY_X:
        if (!rightShiftPressed) {
            ++camHFOV[0];
            yDebug() << "Left eye HFOV =" << camHFOV[0];
        }
        if (!leftShiftPressed) {
            ++camHFOV[1];
            yDebug() << "Right eye HFOV =" << camHFOV[1];
        }
        reconfigureFOV();
        reconfigureRendering();
        break;
    case GLFW_KEY_UP:
        if (!rightShiftPressed) {
            displayPorts[0]->pitchOffset += rightCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Left eye pitch offset =" << displayPorts[0]->pitchOffset;
        }
        if (!leftShiftPressed) {
            displayPorts[1]->pitchOffset += leftCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Right eye pitch offset =" << displayPorts[1]->pitchOffset;
        }
        break;
    case GLFW_KEY_DOWN:
        if (!rightShiftPressed) {
            displayPorts[0]->pitchOffset -= rightCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Left eye pitch offset =" << displayPorts[0]->pitchOffset;
        }
        if (!leftShiftPressed) {
            displayPorts[1]->pitchOffset -= leftCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Right eye pitch offset =" << displayPorts[1]->pitchOffset;
        }
        break;
    case GLFW_KEY_LEFT:
        if (!rightShiftPressed) {
            displayPorts[0]->yawOffset += rightCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Left eye yaw offset =" << displayPorts[0]->yawOffset;
        }
        if (!leftShiftPressed) {
            displayPorts[1]->yawOffset += leftCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Right eye yaw offset =" << displayPorts[1]->yawOffset;
        }
        break;
    case GLFW_KEY_RIGHT:
        if (!rightShiftPressed) {
            displayPorts[0]->yawOffset -= rightCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Left eye yaw offset =" << displayPorts[0]->yawOffset;
        }
        if (!leftShiftPressed) {
            displayPorts[1]->yawOffset -= leftCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Right eye yaw offset =" << displayPorts[1]->yawOffset;
        }
        break;
    case GLFW_KEY_PAGE_UP:
        if (!rightShiftPressed) {
            displayPorts[0]->rollOffset += rightCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Left eye roll offset =" << displayPorts[0]->rollOffset;
        }
        if (!leftShiftPressed) {
            displayPorts[1]->rollOffset += leftCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Right eye roll offset =" << displayPorts[1]->rollOffset;
        }
        break;
    case GLFW_KEY_PAGE_DOWN:
        if (!rightShiftPressed) {
            displayPorts[0]->rollOffset -= rightCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Left eye roll offset =" << displayPorts[0]->rollOffset;
        }
        if (!leftShiftPressed) {
            displayPorts[1]->rollOffset -= leftCtrlPressed ? 0.05f : 0.0025f;
            yDebug() << "Right eye roll offset =" << displayPorts[1]->rollOffset;
        }
        break;
    default:
        break;
    }
}


void yarp::dev::OVRHeadset::reconfigureRendering()
{
    config.OGL.Header.Multisample = multiSampleEnabled ? 1 : 0;

    unsigned int distortionCaps = 0;
    distortionCaps |= ovrDistortionCap_Vignette;
    if (timeWarpEnabled)      distortionCaps |= ovrDistortionCap_TimeWarp;
    if (overdriveEnabled)     distortionCaps |= ovrDistortionCap_Overdrive;
    if (hqDistortionEnabled)  distortionCaps |= ovrDistortionCap_HqDistortion;
    if (flipInputEnabled)     distortionCaps |= ovrDistortionCap_FlipInput;

    ovrHmd_ConfigureRendering(hmd, &config.Config, distortionCaps, fov, EyeRenderDesc);
}



void yarp::dev::OVRHeadset::reconfigureFOV()
{
    for (int i = 0; i < 2; ++i) {
        double camHFOV_rad = OVR::DegreeToRad(camHFOV[i]);
        double texCamRatio = static_cast<double>(texWidth)/camWidth[i];
        double texHFOV_rad = 2 * (atan(texCamRatio * tan(camHFOV_rad/2)));

        double aspectRatio = static_cast<double>(texWidth)/texHeight;
        fov[i].UpTan    = static_cast<float>(fabs(tan(texHFOV_rad/2)/aspectRatio));
        fov[i].DownTan  = static_cast<float>(fabs(tan(texHFOV_rad/2)/aspectRatio));
        fov[i].LeftTan  = static_cast<float>(fabs(tan(texHFOV_rad/2)));
        fov[i].RightTan = static_cast<float>(fabs(tan(texHFOV_rad/2)));
    }
    debugFov(fov);
}

void yarp::dev::OVRHeadset::glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    OVRHeadset* instance = (OVRHeadset*)glfwGetWindowUserPointer(window);
    instance->onKey(key, scancode, action, mods);
}

void yarp::dev::OVRHeadset::glfwErrorCallback(int error, const char* description)
{
    yError() << error << description;
    // FIXME abort?
}


void yarp::dev::OVRHeadset::checkGlError(const char* file, int line) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        switch(error) {
        case GL_INVALID_ENUM:
            yError() << "OpenGL Error GL_INVALID_ENUM: GLenum argument out of range";
            break;
        case GL_INVALID_VALUE:
            yError() << "OpenGL Error GL_INVALID_VALUE: Numeric argument out of range";
            break;
        case GL_INVALID_OPERATION:
            yError() << "OpenGL Error GL_INVALID_OPERATION: Operation illegal in current state";
            break;
        case GL_STACK_OVERFLOW:
            yError() << "OpenGL Error GL_STACK_OVERFLOW: Command would cause a stack overflow";
            break;
        case GL_OUT_OF_MEMORY:
            yError() << "OpenGL Error GL_OUT_OF_MEMORY: Not enough memory left to execute command";
            break;
        default:
            yError() << "OpenGL Error " << error;
            break;
        }
    }
    yAssert(error == 0);
}

void yarp::dev::OVRHeadset::ovrDebugCallback(int level, const char* message)
{
    switch (level) {
    case ovrLogLevel_Debug:
        yDebug() << "ovrDebugCallback" << message;
        break;
    case ovrLogLevel_Info:
        yInfo() << "ovrDebugCallback" << message;
        break;
    case ovrLogLevel_Error:
        yError() << "ovrDebugCallback" << message;
        break;
    default:
        yWarning() << "ovrDebugCallback" << message;
        break;
    }
}

void yarp::dev::OVRHeadset::DebugHmd(ovrHmd hmd)
{
    yDebug("  * ProductName: %s", hmd->ProductName);
    yDebug("  * Manufacturer: %s", hmd->Manufacturer);
    yDebug("  * VendorId:ProductId: %04X:%04X", hmd->VendorId, hmd->ProductId);
    yDebug("  * SerialNumber: %s", hmd->SerialNumber);
    yDebug("  * Firmware Version: %d.%d", hmd->FirmwareMajor, hmd->FirmwareMinor);
    yDebug("  * Resolution: %dx%d", hmd->Resolution.w, hmd->Resolution.h);
}
