////////////////////////////////////////////////////////////////////////////
//
//  This file is part of RTIMULib
//
//  Copyright (c) 2014-2015, richards-tech, LLC
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of
//  this software and associated documentation files (the "Software"), to deal in
//  the Software without restriction, including without limitation the rights to use,
//  copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
//  Software, and to permit persons to whom the Software is furnished to do so,
//  subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
//  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//  PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
//  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
//  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef _RTHUMIDITY_H
#define	_RTHUMIDITY_H

#include "RTIMUSettings.h"
#include "RTIMULibDefs.h"
#include "RTHumidityDefs.h"

class RTHumidity
{
public:
    //  Humidity sensor objects should always be created with the following call

    static RTHumidity *createHumidity(RTIMUSettings *settings);

    //  Constructor/destructor

    RTHumidity(RTIMUSettings *settings);
    virtual ~RTHumidity();

    //  These functions must be provided by sub classes

    virtual const char *humidityName() = 0;                 // the name of the humidity sensor
    virtual int humidityType() = 0;                         // the type code of the humidity sensor
    virtual bool humidityInit() = 0;                        // set up the humidity sensor
    virtual bool humidityRead(RTIMU_DATA& data) = 0;        // get latest value

protected:
    RTIMUSettings *m_settings;                              // the settings object pointer

};

#endif // _RTHUMIDITY_H
