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

#ifndef _RTIMUNULL_H
#define	_RTIMUNULL_H

//  IMUNull is a dummy IMU that assumes sensor data is coming from elsewhere,
//  for example, across a network.
//
//  Call IMUInit in the normal way. Then for every update, call setIMUData and then IMURead
//  to kick the kalman filter.

#include "RTIMU.h"

class RTIMUSettings;

class RTIMUNull : public RTIMU
{
public:
    RTIMUNull(RTIMUSettings *settings);
    ~RTIMUNull();

    // The timestamp parameter is assumed to be from RTMath::currentUSecsSinceEpoch()

    void setIMUData(const RTIMU_DATA& data);

    virtual const char *IMUName() { return "Null IMU"; }
    virtual int IMUType() { return RTIMU_TYPE_NULL; }
    virtual bool IMUInit();
    virtual int IMUGetPollInterval();
    virtual bool IMURead();
    virtual bool IMUGyroBiasValid() { return true; }

private:
    uint64_t m_timestamp;
};

#endif // _RTIMUNULL_H
