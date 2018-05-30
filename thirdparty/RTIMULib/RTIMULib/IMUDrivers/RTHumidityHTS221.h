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

#ifndef _RTHUMIDITYHTS221_H_
#define _RTHUMIDITYHTS221_H_

#include "RTHumidity.h"

class RTIMUSettings;

class RTHumidityHTS221 : public RTHumidity
{
public:
    RTHumidityHTS221(RTIMUSettings *settings);
    ~RTHumidityHTS221();

    virtual const char *humidityName() { return "HTS221"; }
    virtual int humidityType() { return RTHUMIDITY_TYPE_HTS221; }
    virtual bool humidityInit();
    virtual bool humidityRead(RTIMU_DATA& data);

private:
    unsigned char m_humidityAddr;                           // I2C address

    RTFLOAT m_humidity;                                     // the current humidity
    RTFLOAT m_temperature;                                  // the current temperature
    RTFLOAT m_temperature_m;                                // temperature calibration slope
    RTFLOAT m_temperature_c;                                // temperature calibration y intercept
    RTFLOAT m_humidity_m;                                   // humidity calibration slope
    RTFLOAT m_humidity_c;                                   // humidity calibration y intercept
    bool m_humidityValid;
    bool m_temperatureValid;

};

#endif // _RTHUMIDITYHTS221_H_

