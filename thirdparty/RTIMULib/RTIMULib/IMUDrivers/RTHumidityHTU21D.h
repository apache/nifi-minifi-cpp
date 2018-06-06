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

#ifndef _RTHUMIDITYHTU21D_H_
#define _RTHUMIDITYHTU21D_H_

#include "RTHumidity.h"

class RTIMUSettings;

class RTHumidityHTU21D : public RTHumidity
{
public:
    RTHumidityHTU21D(RTIMUSettings *settings);
    ~RTHumidityHTU21D();

    virtual const char *humidityName() { return "HTU21D"; }
    virtual int humidityType() { return RTHUMIDITY_TYPE_HTU21D; }
    virtual bool humidityInit();
    virtual bool humidityRead(RTIMU_DATA& data);

private:
    bool processBackground();

    unsigned char m_humidityAddr;                           // I2C address

    int m_state;
    uint64_t m_startTime;
    RTFLOAT m_humidity;                                     // the current humidity
    RTFLOAT m_temperature;                                  // the current temperature
    bool m_humidityValid;
    bool m_temperatureValid;

};

#endif // _RTHUMIDITYHTU21D_H_

