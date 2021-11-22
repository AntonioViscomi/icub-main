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

#include "iCub/learningMachine/TransformModule.h"

namespace iCub {
namespace learningmachine {

bool TransformPredictProcessor::read(yarp::os::ConnectionReader& connection) {
    if(!this->getTransformerPortable().hasWrapped()) {
        return false;
    }

    yarp::sig::Vector input;
    Prediction prediction;
    bool ok = input.read(connection);
    if(!ok) {
        return false;
    }

    try {
        yarp::sig::Vector trans_input = this->getTransformer().transform(input);
        this->getOutputPort().write(trans_input, prediction);
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


void TransformTrainProcessor::onRead(yarp::os::PortablePair<yarp::sig::Vector,yarp::sig::Vector>& input) {
    if(this->getTransformerPortable().hasWrapped()) {
        try {
            yarp::os::PortablePair<yarp::sig::Vector,yarp::sig::Vector>& output = this->getOutputPort().prepare();
            output.head = this->getTransformer().transform(input.head);
            output.body = input.body;
            this->getOutputPort().writeStrict();
        } catch(const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    return;
}


void TransformModule::printOptions(std::string error) {
    if(error != "") {
        std::cout << "Error: " << error << std::endl;
    }
    std::cout << "Available options" << std::endl;
    std::cout << "--help                 Display this help message" << std::endl;
    std::cout << "--list                 Print a list of available algorithms" << std::endl;
    std::cout << "--load file            Load serialized transformer from a file" << std::endl;
    std::cout << "--transformer type     Desired type of transformer" << std::endl;
    std::cout << "--trainport port       Data port for the training samples" << std::endl;
    std::cout << "--predictport port     Data port for the prediction samples" << std::endl;
    std::cout << "--port pfx             Prefix for registering the ports" << std::endl;
    std::cout << "--commands file        Load configuration commands from a file" << std::endl;
}

void TransformModule::printTransformerList() {
    std::vector<std::string> keys = FactoryT<std::string, ITransformer>::instance().getKeys();
    std::cout << "Available Transformers:" << std::endl;
    for(unsigned int i = 0; i < keys.size(); i++) {
      std::cout << "  " << keys[i] << std::endl;
    }
}

void TransformModule::registerAllPorts() {
    //this->registerPort(this->model_out, "/" + this->portPrefix + "/model:o");

    this->registerPort(this->train_in, this->portPrefix + "/train:i");
    this->train_in.setStrict();

    this->registerPort(this->train_out, this->portPrefix + "/train:o");
    this->train_out.setStrict();

    this->registerPort(this->predict_inout, this->portPrefix + "/predict:io");
    this->predict_inout.setStrict();

    this->registerPort(this->predictRelay_inout, this->portPrefix + "/predict_relay:io");
    //this->predict_relay_inout.setStrict();

    this->registerPort(this->cmd_in, this->portPrefix + "/cmd:i");
}

void TransformModule::unregisterAllPorts() {
    this->cmd_in.close();
    this->train_in.close();
    this->train_out.close();
    this->predict_inout.close();
    this->predictRelay_inout.close();
}

bool TransformModule::interruptModule() {
    cmd_in.interrupt();
    train_in.interrupt();
    train_out.interrupt();
    predict_inout.interrupt();
    predictRelay_inout.interrupt();
    return true;
}

bool TransformModule::configure(yarp::os::ResourceFinder& opt) {
    // read for the general specifiers:
    yarp::os::Value* val;
    std::string transformerName;

    // cache resource finder
    this->setResourceFinder(&opt);

    // check for help request
    if(opt.check("help")) {
        this->printOptions();
        return false;
    }

    // check for algorithm listing request
    if(opt.check("list")) {
        this->printTransformerList();
        return false;
    }

    // check for port specifier: portSuffix
    if(opt.check("port", val)) {
        this->portPrefix = val->asString().c_str();
    }

    if(opt.check("load", val)) {
        this->getTransformerPortable().readFromFile(val->asString().c_str());
    } else{
        // check for transformer specifier: transformerName
        if(opt.check("transformer", val)) {
            transformerName = val->asString().c_str();
        } else {
            this->printOptions("no transformer type specified");
            return false;
        }

        // construct transformer
        this->getTransformerPortable().setWrapped(transformerName);

        // send configuration options to the transformer
        this->getTransformer().configure(opt);
    }

    // add processor for incoming data (training samples)
    this->train_in.useCallback(trainProcessor);

    // add replier for incoming data (prediction requests)
    this->predict_inout.setReplier(this->predictProcessor);

    // register ports
    this->registerAllPorts();

    // check for train data port
    if(opt.check("trainport", val)) {
        yarp::os::Network::connect(this->train_out.where().getName().c_str(),
                         val->asString().c_str());
    } else {
        // add message here if necessary
    }

    // check for predict data port
    if(opt.check("predictport", val)) {
        yarp::os::Network::connect(this->predictRelay_inout.where().getName().c_str(),
                         val->asString().c_str());
    } else {
        // add message here if necessary
    }

    // and finally load command file
    if(opt.check("commands", val)) {
        std::string full_fname = this->findFile(val->asString().c_str());
        this->loadCommandFile(full_fname);
    }

    // attach to the incoming command port and terminal
    this->attach(cmd_in);
    this->attachTerminal();

    return true;
}


bool TransformModule::respond(const yarp::os::Bottle& cmd, yarp::os::Bottle& reply) {
    // NOTE: the module class spawns a new thread, which implies that exception
    // handling needs to be done in this thread, so not the 'main' thread.
    bool success = false;

    try {
        switch(cmd.get(0).asVocab32()) {
            case yarp::os::createVocab32('h','e','l','p'): // print help information
                reply.add(yarp::os::Value::makeVocab32("help"));

                reply.addString("Transform module configuration options");
                reply.addString("  help                  Displays this message");
                reply.addString("  reset                 Resets the machine to its current state");
                reply.addString("  info                  Outputs information about the transformer");
                reply.addString("  load fname            Loads a transformer from a file");
                reply.addString("  save fname            Saves the current transformer to a file");
                reply.addString("  set key val           Sets a configuration option for the transformer");
                reply.addString("  cmd fname             Loads commands from a file");
                reply.addString(this->getTransformer().getConfigHelp().c_str());
                success = true;
                break;

            case yarp::os::createVocab32('c','l','e','a'): // clear the machine
            case yarp::os::createVocab32('c','l','r'):
            case yarp::os::createVocab32('r','e','s','e'):
            case yarp::os::createVocab32('r','s','t'):
                this->getTransformer().reset();
                reply.addString("Transformer reset.");
                success = true;
                break;

            case yarp::os::createVocab32('i','n','f','o'): // information
            case yarp::os::createVocab32('s','t','a','t'): // print statistics
                { // prevent identifier initialization to cross borders of case
                reply.add(yarp::os::Value::makeVocab32("help"));
                reply.addString("Transformer Information: ");
                reply.addString(this->getTransformer().getInfo().c_str());
                success = true;
                break;
                }

            case yarp::os::createVocab32('s','e','t'): // set a configuration option for the machine
                { // prevent identifier initialization to cross borders of case
                yarp::os::Bottle property;
                /*
                 * This is a simple hack to enable multiple parameters The need
                 * for this hack lies in the fact that a group can only be found
                 * using findGroup if it is a nested list in a Bottle. If the
                 * Bottle itself is the list, then the group will _not_ be found.
                 */
                property.addList() = cmd.tail();
                std::string replymsg = "Setting configuration option ";
                bool ok = this->getTransformer().configure(property);
                replymsg += ok ? "succeeded" :
                                 "failed; please check key and value type.";
                reply.addString(replymsg.c_str());
                success = true;
                break;
                }

            case yarp::os::createVocab32('l','o','a','d'): // load
                { // prevent identifier initialization to cross borders of case
                reply.add(yarp::os::Value::makeVocab32("help"));
                std::string replymsg = std::string("Loading transformer from '") +
                                       cmd.get(1).asString().c_str() + "'... " ;
                if(!cmd.get(1).isString()) {
                    replymsg += "failed";
                } else {
                    this->getTransformerPortable().readFromFile(cmd.get(1).asString().c_str());
                    replymsg += "succeeded";
                }
                reply.addString(replymsg.c_str());
                success = true;
                break;
                }

            case yarp::os::createVocab32('s','a','v','e'): // save
                { // prevent identifier initialization to cross borders of case
                reply.add(yarp::os::Value::makeVocab32("help"));
                std::string replymsg = std::string("Saving transformer to '") +
                                       cmd.get(1).asString().c_str() + "'... " ;
                if(!cmd.get(1).isString()) {
                    replymsg += "failed";
                } else {
                    this->getTransformerPortable().writeToFile(cmd.get(1).asString().c_str());
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
