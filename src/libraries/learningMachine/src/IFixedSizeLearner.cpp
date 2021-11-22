/*
 * Copyright (C) 2007-2011 RobotCub Consortium, European Commission FP6 Project IST-004370
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

#include "iCub/learningMachine/IFixedSizeLearner.h"

namespace iCub {
namespace learningmachine {


void IFixedSizeLearner::feedSample(const yarp::sig::Vector& input, const yarp::sig::Vector& output) {
    this->validateDomainSizes(input, output);
}

void IFixedSizeLearner::train() {
}

bool IFixedSizeLearner::configure(yarp::os::Searchable& config) {
    bool success = false;
    // set the domain size (int)
    if(config.find("dom").isInt32()) {
        this->setDomainSize(config.find("dom").asInt32());
        success = true;
    }
    // set the codomain size (int)
    if(config.find("cod").isInt32()) {
        this->setCoDomainSize(config.find("cod").asInt32());
        success = true;
    }

    return success;
}

bool IFixedSizeLearner::checkDomainSize(const yarp::sig::Vector& input) {
    return (input.size() == this->getDomainSize());
}

bool IFixedSizeLearner::checkCoDomainSize(const yarp::sig::Vector& output) {
    return (output.size() == this->getCoDomainSize());
}

void IFixedSizeLearner::validateDomainSizes(const yarp::sig::Vector& input, const yarp::sig::Vector& output) {
    if(!this->checkDomainSize(input)) {
        throw std::runtime_error("Input sample has invalid dimensionality");
    }
    if(!this->checkCoDomainSize(output)) {
        throw std::runtime_error("Output sample has invalid dimensionality");
    }
}

void IFixedSizeLearner::writeBottle(yarp::os::Bottle& bot) const {
    bot.addInt32(this->getDomainSize());
    bot.addInt32(this->getCoDomainSize());
}

void IFixedSizeLearner::readBottle(yarp::os::Bottle& bot) {
    this->setCoDomainSize(bot.pop().asInt32());
    this->setDomainSize(bot.pop().asInt32());
}

std::string IFixedSizeLearner::getInfo() {
    std::ostringstream buffer;
    buffer << this->IMachineLearner::getInfo();
    buffer << "Domain size: " << this->getDomainSize() << std::endl;
    buffer << "Codomain size: " << this->getCoDomainSize() << std::endl;
    return buffer.str();
}

std::string IFixedSizeLearner::getConfigHelp() {
    std::ostringstream buffer;
    buffer << this->IMachineLearner::getConfigHelp();
    buffer << "  dom size              Domain size" << std::endl;
    buffer << "  cod size              Codomain size" << std::endl;
    return buffer.str();
}


} // learningmachine
} // iCub
