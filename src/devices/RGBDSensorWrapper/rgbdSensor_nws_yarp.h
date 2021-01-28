/*
 * Copyright (C) 2006-2021 Istituto Italiano di Tecnologia (IIT)
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the
 * BSD-3-Clause license. See the accompanying LICENSE file for details.
 */

#ifndef YARP_DEV_RGBDSENSOR_NWS_YARP_H
#define YARP_DEV_RGBDSENSOR_NWS_YARP_H

#include <vector>
#include <iostream>
#include <string>
#include <sstream>

#include <yarp/os/Port.h>
#include <yarp/os/Time.h>
#include <yarp/os/Stamp.h>
#include <yarp/os/Bottle.h>
#include <yarp/os/Network.h>
#include <yarp/os/Property.h>
#include <yarp/os/PeriodicThread.h>
#include <yarp/os/BufferedPort.h>


#include <yarp/sig/Vector.h>

#include <yarp/dev/IWrapper.h>
#include <yarp/dev/IMultipleWrapper.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/IRGBDSensor.h>
#include <yarp/dev/IVisualParamsImpl.h>
#include <yarp/dev/FrameGrabberControlImpl.h>


namespace RGBDImpl
{
    class RGBDSensorParser;
}

#define DEFAULT_THREAD_PERIOD   0.03 // s

// Following three definitions would fit better in a header file
// shared between client and server ... where to place it?
constexpr yarp::conf::vocab32_t VOCAB_PROTOCOL_VERSION = yarp::os::createVocab('p', 'r', 'o', 't');
#define RGBD_WRAPPER_PROTOCOL_VERSION_MAJOR 1
#define RGBD_WRAPPER_PROTOCOL_VERSION_MINOR 0



class RGBDImpl::RGBDSensorParser :
        public yarp::dev::DeviceResponder
{
private:
    yarp::dev::IRGBDSensor  *iRGBDSensor;
    yarp::dev::Implement_RgbVisualParams_Parser  rgbParser;
    yarp::dev::Implement_DepthVisualParams_Parser depthParser;
    yarp::dev::FrameGrabberControls_Parser fgCtrlParsers;

public:
    RGBDSensorParser();
    ~RGBDSensorParser() override = default;
    bool configure(yarp::dev::IRGBDSensor *interface);
    bool configure(yarp::dev::IRgbVisualParams *rgbInterface, yarp::dev::IDepthVisualParams *depthInterface);
    bool configure(yarp::dev::IFrameGrabberControls *_fgCtrl);
    bool respond(const yarp::os::Bottle& cmd, yarp::os::Bottle& response) override;
};


/**
 *  @ingroup dev_impl_wrapper
 *
 * \section RgbdSensor_nws_yarp_device_parameters Description of input parameters
 * A Network grabber for kinect-like devices.
 * This device will produce two streams of data through different ports, one for the color frame and the other one
 * for depth image following Framegrabber and IDepthSensor interfaces specification respectively.
 * See they documentation for more details about each interface.
 *
 * This device is paired with its client called RgbdSensor_nws_yarp to receive the data streams and perform remote operations.
 *
 *   Parameters required by this device are:
 * | Parameter name | SubParameter            | Type    | Units          | Default Value | Required                       | Description                                                                                         | Notes |
 * |:--------------:|:-----------------------:|:-------:|:--------------:|:-------------:|:-----------------------------: |:---------------------------------------------------------------------------------------------------:|:-----:|
 * | period         |      -                  | int     | ms             |   20          | No                             | refresh period of the broadcasted values in ms                                                      | default 20ms |
 * | name           |      -                  | string  | -              |   -           | Yes                            | Prefix name of the ports opened by the RGBD wrapper, e.g. /robotName/RGBD                           | Required suffix like '/rpc' will be added by the device      |
 * | subdevice      |      -                  | string  | -              |   -           | alternative to 'attach' action | name of the subdevice to use as a data source                                                       | when used, parameters for the subdevice must be provided as well |
 *
 * Some example of configuration files:
 *
 * Example of configuration file using .ini format.
 *
 * \code{.unparsed}
 * device RGBDSensorWrapper
 * subdevice <RGBDsensor>
 * period 30
 * name /<robotName>/RGBDSensor
 * \endcode
 */

class RgbdSensor_nws_yarp :
        public yarp::dev::DeviceDriver,
        public yarp::dev::IWrapper,
        public yarp::dev::IMultipleWrapper,
        public yarp::os::PeriodicThread
{
private:
    typedef yarp::sig::ImageOf<yarp::sig::PixelFloat>    DepthImage;
    typedef yarp::os::BufferedPort<DepthImage>           DepthPortType;
    typedef yarp::os::BufferedPort<yarp::sig::FlexImage> ImagePortType;
    typedef unsigned int                                 UInt;

    enum SensorType{COLOR_SENSOR, DEPTH_SENSOR};

    template <class T>
    struct param
    {
        param(T& inVar, std::string inName)
        {
            var          = &inVar;
            parname      = inName;
        }
        T*              var;
        std::string     parname;
    };

    std::string colorFrame_StreamingPort_Name;
    std::string depthFrame_StreamingPort_Name;
    ImagePortType         colorFrame_StreamingPort;
    DepthPortType         depthFrame_StreamingPort;

    // One RPC port should be enough for the wrapper in all cases
    yarp::os::Port        rpcPort;
    std::string           rpcPort_Name;
    std::string           nodeName;
    std::string           depthTopicName;
    std::string           colorTopicName;
    std::string           dInfoTopicName;
    std::string           cInfoTopicName;
    yarp::sig::FlexImage  colorImage;
    DepthImage            depthImage;
    UInt                  nodeSeq;

    // It should be possible to attach this  guy to more than one port, try to see what
    // will happen when receiving 2 calls at the same time (receive one calls while serving
    // another one, it will result in concurrent thread most probably) and buffering issues.
//     sensor::depth::RGBDSensor_RPCMgsParser  RPC_parser;

    //Helper class for RPCs
    RGBDImpl::RGBDSensorParser     rgbdParser;

    // Image data specs
    // int hDim, vDim;
    double                         period;
    std::string                    sensorId;
    yarp::dev::IRGBDSensor*        sensor_p;
    yarp::dev::IFrameGrabberControls* fgCtrl;
    yarp::dev::IRGBDSensor::RGBDSensor_status sensorStatus;
    int                            verbose;
    bool                           forceInfoSync;
    bool                           initialize_YARP(yarp::os::Searchable& config);
    bool                           read(yarp::os::ConnectionReader& connection);

    // Open the wrapper only, the attach method needs to be called before using it
    // Typical usage: yarprobotinterface
    bool                           openDeferredAttach(yarp::os::Searchable& prop);

    // If a subdevice parameter is given, the wrapper will open it and attach to immediately.
    // Typical usage: simulator or command line
    bool                           isSubdeviceOwned;
    yarp::dev::PolyDriver*         subDeviceOwned;
    bool                           openAndAttachSubDevice(yarp::os::Searchable& prop);

    // Synch
    yarp::os::Stamp                colorStamp;
    yarp::os::Stamp                depthStamp;
    yarp::os::Property             m_conf;

    bool writeData();
    bool setCamInfo(const std::string&                     frame_id,
                    const UInt&                            seq,
                    const SensorType&                      sensorType);

public:
    RgbdSensor_nws_yarp();
    RgbdSensor_nws_yarp(const RgbdSensor_nws_yarp&) = delete;
    RgbdSensor_nws_yarp(RgbdSensor_nws_yarp&&) = delete;
    RgbdSensor_nws_yarp& operator=(const RgbdSensor_nws_yarp&) = delete;
    RgbdSensor_nws_yarp& operator=(RgbdSensor_nws_yarp&&) = delete;
    ~RgbdSensor_nws_yarp() override;

    bool        open(yarp::os::Searchable &params) override;
    bool        fromConfig(yarp::os::Searchable &params);
    bool        close() override;

    void        setId(const std::string &id);
    std::string getId();

    /**
      * Specify which sensor this thread has to read from.
      */
    bool        attachAll(const yarp::dev::PolyDriverList &p) override;
    bool        detachAll() override;

    bool        attach(yarp::dev::PolyDriver *poly) override;
    bool        attach(yarp::dev::IRGBDSensor *s);
    bool        detach() override;

    bool        threadInit() override;
    void        threadRelease() override;
    void        run() override;
};

#endif   // YARP_DEV_RGBDSENSOR_NWS_YARP_H
