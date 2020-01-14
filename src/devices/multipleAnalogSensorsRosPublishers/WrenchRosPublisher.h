/*
 * Copyright (C) 2006-2019 Istituto Italiano di Tecnologia (IIT)
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the
 * BSD-3-Clause license. See the accompanying LICENSE file for details.
 */


#ifndef YARP_DEV_WRENCHROSPUBLISHER_H
#define YARP_DEV_WRENCHROSPUBLISHER_H

#include "GenericSensorRosPublisher.h"
#include <yarp/rosmsg/geometry_msgs/WrenchStamped.h>

    /**
 * @ingroup dev_impl_wrapper
 *
 * \brief This wrapper connects to a device and publishes a ROS topic of type geometry_msgs::WrenchStamped.
 *
 * | YARP device name |
 * |:-----------------:|
 * | `WrenchRosPublisher` |
 *
 * The parameters accepted by this device are:
 * | Parameter name | SubParameter   | Type    | Units          | Default Value    | Required                    | Description                                                       | Notes |
 * |:--------------:|:--------------:|:-------:|:--------------:|:----------------:|:--------------------------: |:-----------------------------------------------------------------:|:-----:|
 * | topic          |      -         | string  | -              |   -              | Yes                         | The name of the ROS topic opened by this device.                  | MUST start with a '/' character |
 * | node_name      |      -         | string  | -              | $topic + "_node" | No                          | The name of the ROS node opened by this device                    | Autogenerated by default |
 * | period         |      -         | double  | s              |   -              | Yes                         | Refresh period of the broadcasted values in seconds               |  |
 */
class WrenchRosPublisher : public GenericSensorRosPublisher<yarp::rosmsg::geometry_msgs::WrenchStamped>
{
    // Interface of the wrapped device
    yarp::dev::ISixAxisForceTorqueSensors* m_iFTsens{ nullptr };

public:
    using GenericSensorRosPublisher<yarp::rosmsg::geometry_msgs::WrenchStamped>::GenericSensorRosPublisher;

    using GenericSensorRosPublisher<yarp::rosmsg::geometry_msgs::WrenchStamped>::open;
    using GenericSensorRosPublisher<yarp::rosmsg::geometry_msgs::WrenchStamped>::close;
    using GenericSensorRosPublisher<yarp::rosmsg::geometry_msgs::WrenchStamped>::attachAll;
    using GenericSensorRosPublisher<yarp::rosmsg::geometry_msgs::WrenchStamped>::detachAll;

    /* PeriodicRateThread methods */
    void run() override;

protected:
    bool viewInterfaces() override;
};

#endif

