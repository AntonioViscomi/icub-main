/*
 * Copyright (C) 2007-2010 RobotCub Consortium, European Commission FP6 Project IST-004370
 * author:  Arjan Gijsberts
 * email:   arjan.gijsberts@iit.it
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

#include <iostream>
#include <stdexcept>
#include <cassert>

#include <yarp/os/Network.h>
#include <yarp/os/Vocab.h>

#include "iCub/learningMachine/Prediction.h"
#include "iCub/learningMachine/PredictModule.h"
#include "iCub/learningMachine/EventDispatcher.h"
#include "iCub/learningMachine/PredictEvent.h"

namespace iCub {
namespace learningmachine {

bool PredictProcessor::read(yarp::os::ConnectionReader& connection) {
    if(!this->getMachinePortable().hasWrapped()) {
        return false;
    }

    yarp::sig::Vector input;
    Prediction prediction;
    bool ok = input.read(connection);
    if(!ok) {
        return false;
    }
    try {
        prediction = this->getMachine().predict(input);

        // Event Code
        if(EventDispatcher::instance().hasListeners()) {
            PredictEvent pe(input, prediction);
            EventDispatcher::instance().raise(pe);
        }
        // Event Code
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }

    yarp::os::ConnectionWriter* replier = connection.getWriter();
    if(replier != (yarp::os::ConnectionWriter*) 0) {
        prediction.write(*replier);
    }
    return true;
}


void PredictModule::printOptions(std::string error) {
    if(error != "") {
        std::cout << "Error: " << error << std::endl;
    }
    std::cout << "Available options for prediction module" << std::endl;
    std::cout << "--help                 Display this help message" << std::endl;
    std::cout << "--load file            Load serialized machine from a file" << std::endl;
    std::cout << "--port pfx             Prefix for registering the ports" << std::endl;
    std::cout << "--modelport port       Model port of the training module" << std::endl;
    std::cout << "--commands file        Load configuration commands from a file" << std::endl;
}

void PredictModule::registerAllPorts() {
    this->registerPort(this->model_in, this->portPrefix + "/model:i");
    this->registerPort(this->predict_inout, this->portPrefix + "/predict:io");
    this->predict_inout.setStrict();
    this->registerPort(this->cmd_in, this->portPrefix + "/cmd:i");
}

void PredictModule::unregisterAllPorts() {
    this->model_in.close();
    this->cmd_in.close();
    this->predict_inout.close();
}

bool PredictModule::interruptModule() {
    this->cmd_in.interrupt();
    this->predict_inout.interrupt();
    this->model_in.interrupt();
    return true;
}

bool PredictModule::configure(yarp::os::ResourceFinder& opt) {
    // read for the general specifiers:
    yarp::os::Value* val;

    // cache resource finder
    this->setResourceFinder(&opt);

    // check for help request
    if(opt.check("help")) {
        this->printOptions();
        return false;
    }

    // check for port specifier: portSuffix
    if(opt.check("port", val)) {
        this->portPrefix = val->asString().c_str();
    }

    // check for filename to load machine from
    if(opt.check("load", val)) {
        this->getMachinePortable().readFromFile(val->asString().c_str());
    }

    // register ports before connecting
    this->registerAllPorts();

    // check for model input port specifier and connect if found
    if(opt.check("modelport", val)) {
        yarp::os::Network::connect(val->asString().c_str(),
                         this->model_in.where().getName().c_str());
    }

    // add reader for models
    this->model_in.setReader(this->machinePortable);

    // add replier for incoming data (prediction requests)
    this->predict_inout.setReplier(this->predictProcessor);

    // and finally load command file
    if(opt.check("commands", val)) {
        this->loadCommandFile(val->asString().c_str());
    }

    // attach to the incoming command port and terminal
    this->attach(cmd_in);
    this->attachTerminal();

    return true;
}



bool PredictModule::respond(const yarp::os::Bottle& cmd, yarp::os::Bottle& reply) {
    bool success = false;

    try {
        switch(cmd.get(0).asVocab32()) {
            case yarp::os::createVocab32('h','e','l','p'): // print help information
                {
                reply.addVocab32("help");

                reply.addString("Training module configuration options");
                reply.addString("  help                  Displays this message");
                reply.addString("  reset                 Resets the machine to its current state");
                reply.addString("  info                  Outputs information about the machine");
                reply.addString("  load fname            Loads a machine from a file");
                reply.addString("  cmd fname             Loads commands from a file");
                //reply.addString(this->getMachine()->getConfigHelp().c_str());
                success = true;
                break;
                }

            case yarp::os::createVocab32('c','l','e','a'): // clear the machine
            case yarp::os::createVocab32('c','l','r'):
            case yarp::os::createVocab32('r','e','s','e'):
            case yarp::os::createVocab32('r','s','t'):
                {
                this->getMachine().reset();
                reply.addString("Machine reset.");
                success = true;
                break;
                }

            case yarp::os::createVocab32('i','n','f','o'): // information
            case yarp::os::createVocab32('s','t','a','t'): // print statistics
                {
                reply.addVocab32("help");
                reply.addString("Machine Information: ");
                reply.addString(this->getMachine().getInfo().c_str());
                success = true;
                break;
                }

            case yarp::os::createVocab32('l','o','a','d'): // load
                { // prevent identifier initialization to cross borders of case
                reply.add(yarp::os::Value::makeVocab32("help"));
                std::string replymsg = std::string("Loading machine from '") +
                                       cmd.get(1).asString().c_str() + "'... " ;
                if(!cmd.get(1).isString()) {
                    replymsg += "failed";
                } else {
                    this->getMachinePortable().readFromFile(cmd.get(1).asString().c_str());
                    replymsg += "succeeded";
                }
                reply.addString(replymsg.c_str());
                success = true;
                break;
                }

            case yarp::os::createVocab32('c','m','d'): // cmd
            case yarp::os::createVocab32('c','o','m','m'): // command
                { // prevent identifier initialization to cross borders of case
                reply.add(yarp::os::Value::makeVocab32("help"));
                std::string replymsg;
                if(!cmd.get(1).isString()) {
                    replymsg = "Please supply a valid filename.";
                } else {
                    std::string full_fname = this->findFile(cmd.get(1).asString().c_str());
                    replymsg = std::string("Loading commands from '") +
                                           full_fname + "'... " ;
                    this->loadCommandFile(full_fname, &reply);
                    replymsg += "succeeded";
                }
                reply.addString(replymsg.c_str());
                success = true;
                break;
                }

            default:
                break;

        }
    } catch(const std::exception& e) {
        std::string msg = std::string("Error: ") + e.what();
        reply.addString(msg.c_str());
        success = true;
    }
    return success;
}

} // learningmachine
} // iCub
