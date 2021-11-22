/*
 * Copyright (C) 2010-2011 RobotCub Consortium
 * Author: Andrea Del Prete
 * CopyPolicy: Released under the terms of the GNU GPL v2.0.
 *
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

#include <yarp/os/ConnectionReader.h>
#include <yarp/os/ConnectionWriter.h>
#include "iCub/skinDynLib/dynContactList.h"
#include <iCub/ctrl/math.h>

using namespace std;
using namespace yarp::os;
using namespace iCub::skinDynLib;


dynContactList::dynContactList()
:vector<dynContact>(){}

dynContactList::dynContactList(const size_type &n, const dynContact& value)
:vector<dynContact>(n, value){}


//~~~~~~~~~~~~~~~~~~~~~~~~~~
//   SERIALIZATION methods
//~~~~~~~~~~~~~~~~~~~~~~~~~~
bool dynContactList::read(ConnectionReader& connection)
{
    // A dynContactList is represented as a list of list
    // where each list is a skinContact
    if(connection.expectInt32()!=BOTTLE_TAG_LIST)
        return false;

    int listLength = connection.expectInt32();
    if(listLength<0)
        return false;
    if(listLength!=size())
        resize(listLength);

    for(iterator it=begin(); it!=end(); it++)
        if(!it->read(connection))
            return false;

    return !connection.isError();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool dynContactList::write(ConnectionWriter& connection) const
{
    // A dynContactList is represented as a list of list
    // where each list is a skinContact
    connection.appendInt32(BOTTLE_TAG_LIST);
    connection.appendInt32(size());

    for(auto it=begin(); it!=end(); it++)
        if(!it->write(connection))
            return false;

    return !connection.isError();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
string dynContactList::toString(const int &precision) const{
    stringstream ss;
    for(const_iterator it=begin();it!=end();it++)
        ss<<"- "<<it->toString(precision)<<";\n";
    return ss.str();
}

