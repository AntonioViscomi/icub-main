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

// e.g. ./merge --format "(/foo:o[3,1] /bar:o[2,3][1-4] (/baz:o))"

/**
 *
 * Format grammar:
 *

<format>            : <bottle_specifier>

<specifier>         : <port_specifier>
                    | <bottle_specifier>

<bottle_specifier>  : '(' <specifier> (' ' <specifier>)* ')'

<port_specifier>    : <port_name> ( '[' <indices> ']' )*

<indices>           : <index> ( ',' <index> )+

<index>             : <single_index>
                    | <range_index>

<port_name>         : [a-zA-Z0-9_:/]+

<single_index>      : [0-9]+

<range_index>       : [0-9]+ '-' [0-9]+


 *
 *
 */

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <list>

#include <yarp/sig/Vector.h>
#include <yarp/os/Port.h>
#include <yarp/os/ResourceFinder.h>
#include <yarp/os/RFModule.h>
//#include <yarp/os/BufferedPort.h>
#include <yarp/os/Network.h>
#include <yarp/os/Time.h>
#include <yarp/os/Vocab.h>


using namespace yarp::os;
using namespace yarp::sig;

namespace iCub {
namespace learningmachine {
namespace merge {

/**
 * The PortSource collects data from a source port and caches the most recent
 * Bottle of data.
 */
class PortSource {
protected:
    /**
     * Cached data.
     */
    Bottle data;

    /**
     * The port for incoming data.
     */
    Port port;

    /**
     * Returns the first free port given a prefix appended by an integer.
     *
     * @param prefix  the prefix for the port.
     * @return the string for the first available portname given the prefix
     */
    std::string getInputName(std::string prefix) {
        std::ostringstream buffer;
        int i = 1;
        do {
            // standard prefix + i
            buffer.str(""); // clear buffer
            buffer << prefix << i++ << ":i";
        } while (Network::queryName(buffer.str().c_str()).isValid());
        return buffer.str();
    }

    /**
     * Copy constructor (private and unimplemented on purpose).
     */
    PortSource(const PortSource& other);

    /**
     * Assignment operator (private and unimplemented on purpose).
     */
    PortSource& operator=(const PortSource& other);

public:
    /**
     * Default constructor.
     */
    PortSource(std::string name, std::string pp) {
        this->initPort(pp);
    }

    /**
     * Default destructor.
     */
    ~PortSource() {
        this->interrupt();
        this->close();
    }

    /**
     * Opens the first free incoming port with the given prefix.
     * @param prefix  the prefix for the name
     */
    virtual void initPort(std::string prefix) {
        port.open(this->getInputName(prefix).c_str());
    }

    /**
     * Connects the incoming port to the specified port.
     * @param dst  the destination port
     */
    virtual void connect(std::string dst) {
        if (Network::queryName(dst.c_str()).isValid()) {
            Network::connect(dst.c_str(), this->port.where().getName().c_str());
        } else {
            throw std::runtime_error("Cannot find requested port: " + dst);
        }
    }

    /**
     * Reads new data from the port and caches it locally.
     */
    virtual void update() {
        this->port.read(data);
    }

    /**
     * Returns the locally cached data.
     * @return a reference to the locally stored bottle
     */
    virtual Bottle& getData() {
        return this->data;
    }

    /**
     * Interrupts the port.
     */
    virtual void interrupt() {
        this->port.interrupt();
    }

    /**
     * Closes the port.
     */
    virtual void close() {
        this->port.close();
    }
};

/**
 * The SourceList manages a map of PortSource objects.
 */
class SourceList {
protected:
    typedef std::map<std::string, PortSource*> SourceMap;

    /**
     * Prefix for ports.
     */
    std::string portPrefix;

    /**
     * Map that links port names to the PortSource objects that are connected to
     * them.
     */
    SourceMap sourceMap;

public:
    /**
     * Default constructor.
     */
    SourceList(std::string pp = "/lm/merge/source") : portPrefix(pp) { }

    /**
     * Default destructor.
     */
    ~SourceList() {
        for (SourceMap::iterator it = this->sourceMap.begin(); it != this->sourceMap.end(); it++) {
            delete it->second;
        }
    }

    /**
     * Copy constructor.
     */
    SourceList(const SourceList& other);

    /**
     * Assignment operator.
     */
    SourceList& operator=(const SourceList& other);

    /**
     * Updates each registered port with new data.
     */
    virtual void update() {
        // for each in portmap, port.read, store in datamap
        SourceMap::iterator it;
        for (it = this->sourceMap.begin(); it != this->sourceMap.end(); it++ ) {
            it->second->update();
        }
    }

    /**
     * Returns true iff a PortSource has been registered for the given port name.
     */
    virtual bool hasSource(std::string name) {
        return (this->sourceMap.count(name) > 0);
    }

    /**
     * Adds a source port for the given name. It does nothing if a source port
     * with the given name already exists.
     *
     * @param name  the name
     */
    virtual void addSource(std::string name) {
        if (!this->hasSource(name)) {
            this->sourceMap[name] = new PortSource(name, this->portPrefix);
            this->sourceMap[name]->connect(name);
        }
    }

    /**
     * Retrives the port source for a given name.
     *
     * @param name  the name
     * @throw a runtime error if the name has not been registered
     */
    virtual PortSource& getSource(std::string name) {
        if (!this->hasSource(name)) {
            throw std::runtime_error("Attempt to retrieve inexistent source.");
        }
        return *(this->sourceMap[name]);
    }

    /**
     * Recursively interrupt all sources.
     */
    virtual void interrupt() {
        for (SourceMap::iterator it = this->sourceMap.begin(); it != this->sourceMap.end(); it++) {
            it->second->interrupt();
        }
    }

    /**
     * Recursively interrupt all sources.
     */
    virtual void close() {
        for (SourceMap::iterator it = this->sourceMap.begin(); it != this->sourceMap.end(); it++) {
            it->second->close();
        }
    }

    /**
     * Returns the prefix for the source ports.
     * @return the port prefix
     */
    virtual std::string getPortPrefix() {
        return this->portPrefix;
    }

    /**
     * Sets the prefix for the source ports.
     * @param pp the port prefix
     */
    virtual void setPortPrefix(std::string pp) {
        this->portPrefix = pp;
    }
};


/**
 * The DataSelector is an interface for an object that selects data from one or
 * more DataSources. The structure of DataSelector and its subclasses follows
 * the composite pattern.
 */
class DataSelector {
protected:

public:
    /**
     * Returns a string specification of the data selector.
     * @param indent  a number of spaces of indentation
     */
    virtual std::string toString(int indent = 0) = 0;

    /**
     * Declares the required sources for this data selector to the source list.
     * @param sl the source list
     */
    virtual void declareSources(SourceList& sl) = 0;

    /**
     * Selectively adds data from the source list to an output bottle.
     * @param bot a reference to the output Bottle
     * @param sl the source list
     */
    virtual void select(Bottle& bot, SourceList& sl) = 0;

};


/**
 * The IndexSelector selects the components at specified indices from the
 * source. It supports an arbitrary number of dimensions and indices can be
 * specified using a range. If not indices are specified, it returns all of the
 * source data.
 */
class IndexSelector : public DataSelector {
protected:
    /**
     * The name of the source port.
     */
    std::string name;

    /**
     * A list of a list of indices. For each dimension, there is a list of
     * indices.
     */
    std::list<std::list<int> > indices;

    /**
     * Select data from source recursively using the index specifiers.
     *
     * @param out a reference to the output Bottle
     * @param in a reference to the input bottle
     * @param it the iterator
     * @throw a runtime error if trying to index a non-list type
     */
    virtual void selectRecursive(Bottle& out, Bottle& in, std::list< std::list<int> >::iterator it) {
        std::list<int>::iterator it2;
        for (it2 = (*it).begin(); it2 != (*it).end(); ++it2) {
            it++;
            int idx = *it2 - 1;
            if(it == this->indices.end()) {
                if(in.get(idx).isList()) {
                    // add unwrapped bottle
                    this->addBottle(out, *(in.get(idx).asList()));
                } else {
                    // add value directly
                    out.add(in.get(idx));
                }
            } else {
                if(!in.get(idx).isList()) {
                    throw std::runtime_error("Cannot index non-list type");
                }
                this->selectRecursive(out, *(in.get(idx).asList()), it);
            }
            it--;

        }

    }

    /**
     * Adds all the elements in one bottle to an output bottle.
     *
     * @param out a reference to the output bottle
     * @param in a reference to the input bottle
     */
    virtual void addBottle(Bottle& out, const Bottle& in) {
        for(int i = 0; i < in.size(); i++) {
            out.add(in.get(i));
        }
    }

public:
    /**
     * Default constructor.
     *
     * @param format a string specifying the format
     */
    IndexSelector(std::string format) {
        this->loadFormat(format);
    }

    /*
     * Inherited from DataSelector.
     */
    std::string toString(int indent = 0) {
        std::ostringstream buffer;
        buffer << std::string(indent, ' ') << this->name;
        std::list< std::list<int> >::iterator it1;
        for (it1 = this->indices.begin(); it1 != this->indices.end(); ++it1) {
            buffer << "[";
            std::list<int>::iterator it2;
            for (it2 = (*it1).begin(); it2 != (*it1).end(); ++it2) {
                if (it2 != (*it1).begin())
                    buffer << ",";
                buffer << *it2;
            }
            buffer << "]";
        }
        buffer << std::endl;
        return buffer.str();
    }

    /*
     * Inherited from DataSelector.
     */
    virtual void declareSources(SourceList& sl) {
        sl.addSource(this->name);
    }

    /*
     * Inherited from DataSelector.
     */
    virtual void select(Bottle& bot, SourceList& sl) {
        if(this->indices.size() == 0) {
            // no indices, select all
            this->addBottle(bot, sl.getSource(this->name).getData());
        } else {
            // select sub-bottles and items recursively
            std::list< std::list<int> >::iterator it1;
            it1 = this->indices.begin();
            this->selectRecursive(bot, sl.getSource(this->name).getData(), it1);
        }
    }


    /**
     * Loads the format of the IndexSelector from a string.
     *
     * @param format the format string
     * @throw a runtime error if parsing fails
     */
    virtual void loadFormat(std::string format) {
        //std::cout << "Parsing format: " << format << std::endl;
        // find indexing specifier
        std::string::size_type idxStart = format.find("[");
        this->name = format.substr(0, idxStart);

        std::string::size_type idxEnd;
        while (idxStart != std::string::npos) {
            idxEnd = format.find("]", idxStart);
            if (idxEnd == std::string::npos) {
                throw std::runtime_error("Missing closing bracket ']'");
            }
            this->loadIndices(format.substr(idxStart + 1, idxEnd - idxStart - 1));
            idxStart = format.find("[", idxStart + 1);
            if (idxStart != std::string::npos && (idxStart < idxEnd)) {
                throw std::runtime_error("Unexpected opening bracket '['");
            }
        }
    }

    /**
     * Loads index specifiers from a string format.
     *
     * @param format the format string
     * @throw a runtime error if parsing the index specifiers fails
     */
    virtual void loadIndices(std::string format) {
        std::list<int> idxList;
        std::vector<std::string> indexSplit = this->split(format, ",");
        for (unsigned int i = 0; i < indexSplit.size(); i++) {
            std::vector<std::string> rangeSplit = this->split(indexSplit[i], "-");

            if (rangeSplit.size() == 0) {
                // should be impossible
                throw std::runtime_error("Unexpected problem parsing: " + indexSplit[i]);

            } else if (rangeSplit.size() == 1) {
                // single index specification
                idxList.push_back(this->stringToInt(rangeSplit[0]));

            } else if (rangeSplit.size() == 2) {
                // start-end index specification
                int start = this->stringToInt(rangeSplit[0]);
                int end = this->stringToInt(rangeSplit[1]);
                if (start > end) {
                    throw std::runtime_error("End of range before start of range: " + indexSplit[i]);
                }
                for (int idx = start; idx <= end; idx++) {
                    idxList.push_back(idx);
                }

            } else if (rangeSplit.size() > 2) {
                // illegal
                throw std::runtime_error("Illegal range specification: " + indexSplit[i]);
            }
        }
        this->indices.push_back(idxList);
    }

    /**
     * Splits a string into parts at the given delimiter.
     *
     * @param input the input string
     * @param delimiter the delimiter
     * @return a vector with the string parts
     */
    static std::vector<std::string> split(std::string input, std::string delimiter) {
        std::string::size_type start = 0;
		std::string::size_type end = 0;
        std::vector<std::string> output;
        while (end != std::string::npos) {
            end = input.find(delimiter, start);
            output.push_back(input.substr(start, (end == std::string::npos) ? end : end - start));
            start = end + 1;
        }
        return output;
    }

    /**
     * Converts a string to an integer in a proper C++ way.
     * @param str the string
     * @return an integer
     * @throw a runtime error if the string cannot be parsed as an integer
     */
    static int stringToInt(std::string str) {
        std::istringstream buffer(str);
        int ret;
        if (buffer >> ret) {
            return ret;
        } else {
            throw std::runtime_error("Could not read integer from '" + str + "'");
        }
    }
};


/**
 * The composite selector groups other data selectors.
 *
 */
class CompositeSelector : public DataSelector {
protected:
    std::vector<DataSelector*> children;
public:
    /**
     * Default constructor.
     */
    CompositeSelector(Bottle& format) {
        this->loadFormat(format);
    }

    /**
     * Default destructor.
     */
    ~CompositeSelector() {
        // clear children vector
        for (unsigned int i = 0; i < this->children.size(); i++) {
            delete this->children[i];
        }
        this->children.clear();
        this->children.resize(0);
    }

    /**
     * Copy constructor.
     */
    CompositeSelector(const CompositeSelector& other);

    /**
     * Assignment operator.
     */
    CompositeSelector& operator=(const CompositeSelector& other);

    void addChild(DataSelector* ds) {
        this->children.push_back(ds);
    }

    /**
     * Loads the format of this composite selector from a Bottle.
     *
     * @param format the bottle specifying the format
     * @throw a runtime error if parsing fails
     */
    void loadFormat(Bottle& format) {
        int i = 0;
        int len = format.size();
        while (i < len) {
            if (format.get(i).isString()) {
                //std::cout << "Adding Index for " << format.get(i).asString().c_str() << std::endl;
                this->addChild(new IndexSelector(format.get(i).asString().c_str()));
            } else if (format.get(i).isList()) {
                //std::cout << "Adding Composite for " << format.get(i).asList()->toString().c_str() << std::endl;
                this->addChild(new CompositeSelector(*(format.get(i).asList())));
            } else {
                throw std::runtime_error(std::string("Unexpected token during parsing: ") +
                                         format.get(i).asString().c_str());
            }
            i++;
        }
    }

    /*
     * Inherited from DataSelector.
     */
    std::string toString(int indent = 0) {
        std::ostringstream buffer;
        buffer << std::string(indent, ' ') << "(" << std::endl;
        for (unsigned int i = 0; i < this->children.size(); i++) {
            buffer << this->children[i]->toString(indent + 2);
        }
        buffer << std::string(indent, ' ') << ")" << std::endl;
        return buffer.str();
    }

    /*
     * Inherited from DataSelector.
     */
    virtual void declareSources(SourceList& sl) {
        for (unsigned int i = 0; i < this->children.size(); i++) {
            this->children[i]->declareSources(sl);
        }
    }

    /*
     * Inherited from DataSelector.
     */
    virtual void select(Bottle& bot, SourceList& sl) {
        Bottle& bot2 = bot.addList();
        for (unsigned int i = 0; i < this->children.size(); i++) {
            this->children[i]->select(bot2, sl);
        }
    }
};

/**
 * The RootSelector is entry point for a format bottle. It inherits most of its
 * functionality from the CompositeSelector, with the primary difference being
 * that it does _not_ wrap its contents in another Bottle.
 */
class RootSelector : public CompositeSelector {
public:
    /**
     * Default constructor.
     */
    RootSelector(Bottle& format) : CompositeSelector(format) { }

    /*
     * Inherited from DataSelector.
     */
    virtual void select(Bottle& bot, SourceList& sl) {
        for (unsigned int i = 0; i < this->children.size(); i++) {
            this->children[i]->select(bot, sl);
        }
    }
};

/**
 * \ingroup icub_libLM_modules
 *
 * The MergeModule merges data from several input ports into a single output
 * port.
 *
 * \author Arjan Gijsberts
 */
class MergeModule : public RFModule {
protected:
    /**
     * Prefix for the ports.
     */
    std::string portPrefix;

    /**
     * Desired period of the module updates.
     */
    double desiredPeriod;

    /**
     * The collecting resource for all data from all sources.
     */
    SourceList sourceList;

    /**
     * A pointer to the root DataSelector.
     */
    DataSelector* dataSelector;

    /**
     * Output port.
     */
    Port output;

    void printOptions(std::string error = "") {
        if (error != "") {
            std::cerr << "Error: " << error << std::endl;
        }
        std::cout << "Available options" << std::endl;
        std::cout << "--format               The format for the output (required)" << std::endl;
        std::cout << "--frequency f          Sampling frequency in Hz" << std::endl;
        std::cout << "--port pfx             Prefix for registering the ports" << std::endl;
    }

    /**
     * Register a port at a specified name.
     *
     * @param port the port
     * @param name the name
     * @throw a runtime error if the port could not be registered
     */
    void registerPort(Contactable& port, std::string name) {
        if (port.open(name.c_str()) != true) {
            std::string msg("could not register port ");
            msg+=name;
            throw std::runtime_error(msg);
        }
    }

    /**
     * Register all ports for this module.
     */
    void registerAllPorts() {
        this->registerPort(this->output, this->portPrefix + "/output:o");
    }

    /**
     * Attempts to unregister all ports used by this module.
     *
     * @throw a runtime error if unregistering the port fails
     */
    void unregisterAllPorts() {
        this->sourceList.close();
        this->output.close();
    }


public:
    /**
     * Constructor.
     */
    MergeModule(std::string pp = "/lm/merge")
            : portPrefix(pp), desiredPeriod(0.1), dataSelector((DataSelector*) 0) { }

    /**
     * Destructor.
     */
    ~MergeModule() {
        delete this->dataSelector;
    }

    /*
     * Inherited from yarp::os::RFModule
     */
    virtual double getPeriod() {
        return this->desiredPeriod;
    }

    /*
     * Inherited from yarp::os::RFModule
     */
    virtual bool interruptModule() {
        this->sourceList.interrupt();
        this->output.interrupt();
        return true;
    }

    /*
     * Inherited from yarp::os::RFModule
     */
    virtual bool close() {
        this->unregisterAllPorts();
        return true;
    }

    /*
     * Inherited from yarp::os::RFModule
     */
    virtual bool configure(ResourceFinder& opt) {
        // read for the general specifiers:
        Value* val;
        bool success = false;

        if (opt.check("help")) {
            this->printOptions();
            return false;
        }

        // check for port specifier: portSuffix
        if (opt.check("port", val)) {
            this->portPrefix = val->asString().c_str();
        }

        // set port prefix
        this->sourceList.setPortPrefix(this->portPrefix + "/source");

        // read and parse format
        if (opt.check("format", val)) {
            if (val->isList()) {
                this->dataSelector = new RootSelector(*(val->asList()));
                this->dataSelector->declareSources(this->sourceList);
                success = true;
            } else {
                throw std::runtime_error("The format must be a list!");
            }
        } else {
            // error, no format!
            this->printOptions("Please supply a format!");
            return false;
        }


        if (opt.check("frequency", val)) {
            if (val->isFloat64() || val->isInt32()) {
                this->setFrequency(val->asFloat64());
            }
        }

        this->registerAllPorts();

        this->attachTerminal();

        return success;
    }


    /*
     * Inherited from yarp::os::RFModule
     */
    virtual bool updateModule() {
        assert(this->dataSelector != (DataSelector*) 0);
        try {
            this->sourceList.update();
            Bottle out;
            this->dataSelector->select(out, this->sourceList);
            //std::cout << "Bottle: " << out.toString().c_str() << std::endl;
            this->output.write(out);
            //this->listeners.process(this->portSource);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Error... something bad happened, but I wouldn't know what!" << std::endl;
        }
        return true;
    }

    /*
     * Inherited from yarp::os::RFModule
     */
    bool respond(const Bottle& cmd, Bottle& reply) {
        bool success = false;

        try {
            switch (cmd.get(0).asVocab32()) {
            case yarp::os::createVocab32('h','e','l','p'): // print help information
                            success = true;
                reply.add(Value::makeVocab32("help"));

                reply.addString("Merge module configuration options");
                reply.addString("  help                  Displays this message");
                reply.addString("  info                  Prints information");
                reply.addString("  freq f                Sampling frequency in Hertz (0 for disabled)");
                break;

            case yarp::os::createVocab32('i','n','f','o'): // print information
            {
                reply.add(Value::makeVocab32("help"));
                success = true;
                reply.addString(this->dataSelector->toString().c_str());
                break;
            }

            case yarp::os::createVocab32('f','r','e','q'): // set sampling frequency
            {
                if (cmd.size() > 1 && (cmd.get(1).isInt32() || cmd.get(1).isFloat64())) {
                    success = true;
                    this->setDesiredPeriod(1. / cmd.get(1).asFloat64());
                    //reply.addString((std::string("Current frequency: ") + cmd.get(1).toString().c_str()).c_str());
                }
                break;
            }

            default:
                break;
            }
        } catch (const std::exception& e) {
            success = true; // to make sure YARP prints the error message
            std::string msg = std::string("Error: ") + e.what();
            reply.addString(msg.c_str());
            this->close();
        } catch (...) {
            success = true; // to make sure YARP prints the error message
            std::string msg = std::string("Error. (something bad happened, but I wouldn't know what!)");
            reply.addString(msg.c_str());
            this->close();
        }

        return success;
    }

    /**
     * Mutator for the desired period.
     * @param p  the desired period in seconds
     * @return
     */
    virtual void setDesiredPeriod(double p) {
        this->desiredPeriod = p;
    }

    /**
     * Mutator for the desired period by means of setting the frequency.
     * @param f  the desired frequency
     * @return
     */
    virtual void setFrequency(double f) {
        if (f <= 0) {
            throw std::runtime_error("Frequency must be larger than 0");
        }
        this->setDesiredPeriod(1. / f);
    }

    /**
     * Accessor for the desired period.
     * @return the desired period in seconds
     */
    virtual double getDesiredPeriod() {
        return this->desiredPeriod;
    }
};

} // merge
} // learningmachine
} // iCub

using namespace iCub::learningmachine::merge;

int main(int argc, char *argv[]) {
    Network yarp;
    int ret;

    ResourceFinder rf;
    rf.setDefaultContext("learningMachine");
    rf.configure(argc, argv);
    MergeModule module;
    try {
        ret = module.runModule(rf);
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        module.close();
        return 1;
    } catch(char* msg) {
        std::cerr << "Error: " << msg << std::endl;
        module.close();
        return 1;
    }
    return ret;
}

