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

#ifndef _RTPRESSUREMS5637_H_
#define _RTPRESSUREMS5637_H_

#include "RTPressure.h"

//  State definitions

#define MS5637_STATE_IDLE               0
#define MS5637_STATE_TEMPERATURE        1
#define MS5637_STATE_PRESSURE           2

class RTIMUSettings;

class RTPressureMS5637 : public RTPressure
{
public:
    RTPressureMS5637(RTIMUSettings *settings);
    ~RTPressureMS5637();

    virtual const char *pressureName() { return "MS5637"; }
    virtual int pressureType() { return RTPRESSURE_TYPE_MS5611; }
    virtual bool pressureInit();
    virtual bool pressureRead(RTIMU_DATA& data);

private:
    void pressureBackground();
    void setTestData();

    unsigned char m_pressureAddr;                           // I2C address
    RTFLOAT m_pressure;                                     // the current pressure
    RTFLOAT m_temperature;                                  // the current temperature

    int m_state;

    uint16_t m_calData[6];                                  // calibration data

    uint32_t m_D1;
    uint32_t m_D2;

    uint64_t m_timer;                                       // used to time coversions

    bool m_validReadings;
};

#endif // _RTPRESSUREMS5637_H_

