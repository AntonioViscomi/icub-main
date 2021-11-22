/*
* Copyright (C)2013  iCub Facility - Istituto Italiano di Tecnologia
* Author: Marco Randazzo
* email:  marco.randazzo@iit.it
* website: www.robotcub.org
* Permission is granted to copy, distribute, and/or modify this program
* under the terms of the GNU General Public License, version 2 or any
* later version published by the Free Software Foundation.
*
* A copy of the license can be found at
* http://www.robotcub.org/icub/license/gpl.txt
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
* Public License for more details
*/

#include <yarp/os/Network.h>
#include <yarp/os/Time.h>
#include <yarp/os/Log.h>
#include <yarp/os/LogStream.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/sig/Vector.h>
#include <yarp/math/Math.h>

#include <yarp/dev/ControlBoardInterfaces.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/os/PeriodicThread.h>
#include <yarp/os/Thread.h>

#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <deque>
#include <map>

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::dev;
using namespace yarp::math;

#define VCTP_TIME       yarp::os::createVocab32('t','i','m','e')
#define VCTP_OFFSET     yarp::os::createVocab32('o','f','f')
#define VCTP_CMD_NOW    yarp::os::createVocab32('c','t','p','n')
#define VCTP_CMD_QUEUE  yarp::os::createVocab32('c','t','p','q')
#define VCTP_CMD_FILE   yarp::os::createVocab32('c','t','p','f')
#define VCTP_POSITION   yarp::os::createVocab32('p','o','s')
#define VCTP_WAIT       yarp::os::createVocab32('w','a','i','t')

#define ACTION_IDLE    0
#define ACTION_START   1
#define ACTION_RUNNING 2
#define ACTION_STOP    3
#define ACTION_RESET   4

// ******************** ACTION CLASS
class action_struct
{
    int         N_JOINTS;
public:
    int         counter;
    double      time;
    double*     q_joints;
    string      tag;

public:
    int get_n_joints();
    action_struct(int n);
    action_struct(const action_struct& as);
    action_struct & operator=(const action_struct & as);
    ~action_struct();
};

class action_class
{
public:
    size_t         current_action;
    int            current_status;
    bool           forever;
    std::deque<action_struct> action_vector;
    std::deque<action_struct>::iterator action_it;

    void clear();
    action_class();
    void print();
    bool openFile(string filename, int n_joints);
    bool parseCommandLineFixTime(const char* command_line, int line, double fixTime, int n_joints);
    bool parseCommandLine(const char* command_line, int line, int n_joints);
};

// ******************** ROBOT DRIVER CLASS
class robotDriver
{
    friend class BroadcastingThread;
private:
    bool verbose;
    bool drv_connected;
    Property          drvOptions_ll;
    PolyDriver       *drv_ll;
    IPositionControl *ipos_ll;
    IPositionDirect  *iposdir_ll;
    IPidControl      *ipid_ll;
    IControlMode     *icmd_ll;
    IEncoders        *ienc_ll;
    IMotorEncoders   *imotenc_ll;

public:
    int              n_joints;
    std::map<int, int> joints_map;

public:
    robotDriver();
    bool configure(const Property &copt);
    bool init();
    ~robotDriver();
    bool setControlMode(const int j, const int mode);
    bool setPosition(int j, double ref);
    bool getEncoder(int j, double *v);
    bool positionMove(int j, double ref);
};
