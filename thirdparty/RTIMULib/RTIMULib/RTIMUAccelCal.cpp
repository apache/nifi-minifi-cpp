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

#include "RTIMUAccelCal.h"

//  ACCEL_ALPHA control the smoothing - the lower it is, the smoother it is

#define ACCEL_ALPHA                     0.1f

RTIMUAccelCal::RTIMUAccelCal(RTIMUSettings *settings)
{
    m_settings = settings;
    for (int i = 0; i < 3; i++)
        m_accelCalEnable[i] = false;
}

RTIMUAccelCal::~RTIMUAccelCal()
{

}

void RTIMUAccelCal::accelCalInit()
{
    if (m_settings->m_accelCalValid) {
        m_accelMin = m_settings->m_accelCalMin;
        m_accelMax = m_settings->m_accelCalMax;
    } else {
        m_accelMin = RTVector3(RTIMUCALDEFS_DEFAULT_MIN, RTIMUCALDEFS_DEFAULT_MIN, RTIMUCALDEFS_DEFAULT_MIN);
        m_accelMax = RTVector3(RTIMUCALDEFS_DEFAULT_MAX, RTIMUCALDEFS_DEFAULT_MAX, RTIMUCALDEFS_DEFAULT_MAX);
    }
}

void RTIMUAccelCal::accelCalReset()
{
    for (int i = 0; i < 3; i++) {
        if (m_accelCalEnable[i]) {
            m_accelMin.setData(i, RTIMUCALDEFS_DEFAULT_MIN);
            m_accelMax.setData(i, RTIMUCALDEFS_DEFAULT_MAX);
        }
    }
}

void RTIMUAccelCal::accelCalEnable(int axis, bool enable)
{
    m_accelCalEnable[axis] = enable;
}

void RTIMUAccelCal::newAccelCalData(const RTVector3& data)
{

    for (int i = 0; i < 3; i++) {
        if (m_accelCalEnable[i]) {
            m_averageValue.setData(i, (data.data(i) * ACCEL_ALPHA + m_averageValue.data(i) * (1.0 - ACCEL_ALPHA)));
            if (m_accelMin.data(i) > m_averageValue.data(i))
                m_accelMin.setData(i, m_averageValue.data(i));
            if (m_accelMax.data(i) < m_averageValue.data(i))
                m_accelMax.setData(i, m_averageValue.data(i));
        }
    }
}

bool RTIMUAccelCal::accelCalValid()
{
    bool valid = true;

    for (int i = 0; i < 3; i++) {
        if (m_accelMax.data(i) < m_accelMin.data(i))
            valid = false;
    }
    return valid;
}

bool RTIMUAccelCal::accelCalSave()
{
    if (!accelCalValid())
        return false;

    m_settings->m_accelCalValid = true;
    m_settings->m_accelCalMin = m_accelMin;
    m_settings->m_accelCalMax = m_accelMax;
    m_settings->saveSettings();
    return true;
}
