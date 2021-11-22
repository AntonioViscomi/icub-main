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

#include <stdexcept>
#include <sstream>

#include <yarp/os/Vocab.h>

#include "iCub/learningMachine/DispatcherManager.h"
#include "iCub/learningMachine/IEventListener.h"

namespace iCub {
namespace learningmachine {


DispatcherManager::DispatcherManager() {
    // cache pointer to event dispatcher
    this->dispatcher = &(EventDispatcher::instance());
    // cache pointer to event listener factory
    this->factory = &(EventListenerFactory::instance());
}

bool DispatcherManager::respond(const yarp::os::Bottle& cmd, yarp::os::Bottle& reply) {
    bool success = false;

    try {
        switch(cmd.get(0).asVocab32()) {
            case yarp::os::createVocab32('h','e','l','p'): // print help information
                reply.add(yarp::os::Value::makeVocab32("help"));

                reply.addString("Event Manager configuration options");
                reply.addString("  help                  Displays this message");
                reply.addString("  list                  Print a list of available event listeners");
                reply.addString("  add type [type2 ...]  Adds one or more event listeners");
                reply.addString("  remove [all|idx]      Removes event listener at an index or all");
                reply.addString("  set [all|idx]         Configures a listener");
                reply.addString("  stats                 Prints information");
                success = true;
                break;

            case yarp::os::createVocab32('l','i','s','t'): // print list of available event listeners
                {
                reply.add(yarp::os::Value::makeVocab32("help"));
                std::vector<std::string> keys = FactoryT<std::string, IEventListener>::instance().getKeys();
                for(unsigned int i = 0; i < keys.size(); i++) {
                    reply.addString((std::string("  ") + keys[i]).c_str());
                }
                success = true;
                break;
                }

            case yarp::os::createVocab32('a','d','d'): // add
                { // prevent identifier initialization to cross borders of case
                yarp::os::Bottle list = cmd.tail();
                for(int i = 0; i < list.size(); i++) {
                    IEventListener* listener = this->factory->create(list.get(i).asString().c_str());
                    listener->start();
                    this->dispatcher->addListener(listener);
                }
                reply.addString("Successfully added listener(s)");
                success = true;
                break;
                }

            case yarp::os::createVocab32('r','e','m','o'): // remove
            case yarp::os::createVocab32('d','e','l'): // del(ete)
                { // prevent identifier initialization to cross borders of case
                if(cmd.get(1).isInt32() && cmd.get(1).asInt32() >= 1 && cmd.get(1).asInt32() <= this->dispatcher->countListeners()) {
                    this->dispatcher->removeListener(cmd.get(1).asInt32()-1);
                    reply.addString("Successfully removed listener.");
                    success = true;
                } else if(cmd.get(1).asString() == "all") {
                    this->dispatcher->clear();
                    reply.addString("Successfully removed all listeners.");
                    success = true;
                } else {
                    throw std::runtime_error("Illegal index!");
                }
                break;
                }

            case yarp::os::createVocab32('s','e','t'): // set
                { // prevent identifier initialization to cross borders of case
                yarp::os::Bottle property;
                property.addList() = cmd.tail().tail(); // see comment in TrainModule

                std::string replymsg = "Setting configuration option ";
                if(cmd.get(1).isInt32() && cmd.get(1).asInt32() >= 1 &&
                   cmd.get(1).asInt32() <= this->dispatcher->countListeners()) {

                    bool ok = this->dispatcher->getAt(cmd.get(1).asInt32()-1).configure(property);
                    replymsg += ok ? "succeeded" :
                                     "failed; please check key and value type.";
                    reply.addString(replymsg.c_str());
                    success = true;
                } else if(cmd.get(1).asString() == "all") {
                    for(int i = 0; i < this->dispatcher->countListeners(); i++) {
                        if(i > 0) {
                            replymsg += ", ";
                        }
                        bool ok = this->dispatcher->getAt(i).configure(property);
                        replymsg += ok ? "succeeded" :
                                        "failed; please check key and value type.";
                    }
                    replymsg += ".";
                    reply.addString(replymsg.c_str());
                    success = true;
                } else {
                    throw std::runtime_error("Illegal index!");
                }
                break;
                }

            case yarp::os::createVocab32('i','n','f','o'): // information
            case yarp::os::createVocab32('s','t','a','t'): // statistics
                { // prevent identifier initialization to cross borders of case
                reply.add(yarp::os::Value::makeVocab32("help"));
                std::ostringstream buffer;
                buffer << "Event Manager Information (" << this->dispatcher->countListeners() << " listeners)";
                reply.addString(buffer.str().c_str());
                for(int i = 0; i < this->dispatcher->countListeners(); i++) {
                    buffer.str(""); // why isn't there a proper reset method?
                    buffer << "  [" << (i + 1) << "] " << this->dispatcher->getAt(i).getInfo();
                    reply.addString(buffer.str().c_str());
                }

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

