// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

// Copyright (C) 2015 Istituto Italiano di Tecnologia - iCub Facility
// Author: Marco Randazzo <marco.randazzo@iit.it>
// CopyPolicy: Released under the terms of the GNU GPL v2.0.

#include "positionDirectThread.h"
#include <cstring>
#include <string>
#include <cmath>

#include <yarp/os/Log.h>
#include <yarp/os/LogStream.h>

using namespace yarp::dev;
using namespace yarp::os;
using namespace yarp::sig;

positionDirectControlThread::positionDirectControlThread(int period):
                yarp::os::PeriodicThread((double)period/1000.0)
{
    control_period = period;
    suspended = true;
    control_joints_list = 0;
    joints_limiter = 2;
    target_limiter = 1;
}

positionDirectControlThread::~positionDirectControlThread()
{
}

void positionDirectControlThread::run()
{
    double curr_time = yarp::os::Time::now()-t_start;

    if (getIterations()>100)
    {
        yDebug("Thread ran %d times, est period %lf[ms], used %lf[ms]\n",
                getIterations(),
                1000.0*getEstimatedPeriod(),
                1000.0*getEstimatedUsed());
        resetStat();
    }
    std::lock_guard<std::mutex> lck(_mutex);

    //read the position targets
    yarp::os::Bottle *bot = command_port.read(false);
    if(bot!=NULL)
    {
        unsigned int botsize = bot->size();
        if (botsize == control_joints)
        {
            prev_targets=targets;
            for (unsigned int i=0; i< control_joints; i++)
            {
                targets[i] = bot->get(i).asFloat64();
            }
        }
        else
        {
            yError ("Your bottle does not have the right size: module is configured to control %d joints", control_joints);
        }
    }
    else
    {
        return;
    }

    //apply the joints limits
    for (unsigned int i=0; i< control_joints; i++)
    {
        if (targets[i]>max_limits[i]) targets[i]=max_limits[i];
        if (targets[i]<min_limits[i]) targets[i]=min_limits[i];
    }

    //get the current position
    for (unsigned int i=0; i< control_joints; i++)
    {
        double val =0;
        ienc->getEncoder(control_joints_list[i],&val);
        encoders[i]=val;
    }

    //apply a limit on the difference between prev target and current target
    for (unsigned int i=0; i< control_joints; i++)
    {
        double diff = (targets[i]-prev_targets[i]);
        if      (diff > +target_limiter) targets[i]=prev_targets[i]+ target_limiter;
        else if (diff < -target_limiter) targets[i]=prev_targets[i]- target_limiter;
    }

    //slew rate limiter
    for(unsigned int i = 0; i<control_joints; i++)
    {
        double diff = (targets[i]-encoders[i]);
        if      (diff > +joints_limiter) targets[i]=encoders[i]+ joints_limiter;
        else if (diff < -joints_limiter) targets[i]=encoders[i]- joints_limiter;
    }

    //appy the command
#if 0
    for(unsigned int i = 0; i<control_joints; i++)
    {
        idir->setPosition(control_joints_list[i],targets[i]);
    }
#endif
    idir->setPositions(control_joints, control_joints_list, targets.data());

    yDebug() << curr_time << targets[0] << targets[1] << targets[2];
}

bool positionDirectControlThread::threadInit()
{
    suspended=true;
    t_start = yarp::os::Time::now();
    return true;
}

void positionDirectControlThread::threadRelease()
{
    for(unsigned int i=0; i<control_joints; i++)
    {
       imod->setControlMode(control_joints_list[i], VOCAB_CM_POSITION);
    }

    suspended = true;
    command_port.close();
}

bool positionDirectControlThread::init(PolyDriver *d, std::string moduleName, std::string partName, std::string robotName, Bottle* jointsList)
{
    ///opening port command input
    char tmp[255];
    sprintf(tmp, "/%s/%s/%s/command:i", moduleName.c_str(), robotName.c_str(), partName.c_str());
    yInfo("opening port for part %s\n",tmp);
    command_port.open(tmp);

    if (d==0)
    {
        yError ("Invalid device driver pointer");
        return false;
    }

    driver=d;
    driver->view(idir);
    driver->view(ipos);
    driver->view(ienc);
    driver->view(imod);
    driver->view(ilim);

    if ( (idir==0)||(ienc==0) || (imod==0) || (ipos==0) || (ilim==0))
    {
        yError ("Failed to view motor interfaces");
        return false;
    }

    int tmpj=0;
    ipos->getAxes(&tmpj);
    part_joints=tmpj;
    control_joints= jointsList->size();
    if (control_joints>part_joints)
    {
        yError ("you cannot control more of %d joints for this robot part", part_joints);
        return false;
    }
    else if (control_joints<=0)
    {
        yError ("invalid number of control joints (%d)", control_joints);
        return false;
    }
    else
    {
        control_joints_list = new int [control_joints];
        for (unsigned int i=0; i< control_joints; i++)
        {
            if (jointsList->get(i).isInt32() && jointsList->get(i).asInt32()>=0)
            {
                control_joints_list[i] = jointsList->get(i).asInt32();
            }
            else
            {
                yError ("invalid list of jonts to control");
                return false;
            }
        }
    }
    yInfo("part has %d joints, controlling %d joints\n",part_joints, control_joints);

    Vector speeds;
    speeds.resize(control_joints);
    speeds=10.0;
    for (unsigned int i=0; i<control_joints; i++)
    {
        ipos->setRefSpeed(control_joints_list[i],speeds[i]);
    }

    encoders.resize(control_joints);
    targets.resize(control_joints);
    prev_targets.resize(control_joints);
    error.resize(control_joints);
    encoders.zero();
    targets.zero();
    prev_targets.zero();
    error.zero();

    min_limits.resize(control_joints);
    max_limits.resize(control_joints);
    for (unsigned int i=0; i<control_joints; i++)
    {
        double min=0;
        double max=0;
        ilim->getLimits(control_joints_list[i],&min,&max);
        min_limits[i]=min;
        max_limits[i]=max;
    }

    for (unsigned int i=0; i<control_joints; i++)
    {
        imod->setControlMode(control_joints_list[i],VOCAB_CM_POSITION_DIRECT);
    }

    //get the current position
    for (unsigned int i=0; i< control_joints; i++)
    {
        double val =0;
        ienc->getEncoder(control_joints_list[i],&val);
        targets[i] = encoders[i] = val;
        prev_targets[i] = encoders[i];
    }

    return true;
}

void positionDirectControlThread::halt()
{
    suspended=true;
    yInfo("Suspended\n");
}

void positionDirectControlThread::go()
{
    suspended=false;
    yInfo("Run\n");
}

void positionDirectControlThread::setVel(int i, double vel)
{
    std::lock_guard<std::mutex> lck(_mutex);
}

void positionDirectControlThread::setGain(int i, double gain)
{
    std::lock_guard<std::mutex> lck(_mutex);
}


void positionDirectControlThread::limitSpeed(Vector &v)
{

}
