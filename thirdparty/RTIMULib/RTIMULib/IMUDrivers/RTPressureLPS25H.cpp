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

#include "RTPressureLPS25H.h"
#include "RTPressureDefs.h"

RTPressureLPS25H::RTPressureLPS25H(RTIMUSettings *settings) : RTPressure(settings)
{
    m_pressureValid = false;
    m_temperatureValid = false;
 }

RTPressureLPS25H::~RTPressureLPS25H()
{
}

bool RTPressureLPS25H::pressureInit()
{
    m_pressureAddr = m_settings->m_I2CPressureAddress;

    if (!m_settings->HALWrite(m_pressureAddr, LPS25H_CTRL_REG_1, 0xc4, "Failed to set LPS25H CTRL_REG_1"))
        return false;

    if (!m_settings->HALWrite(m_pressureAddr, LPS25H_RES_CONF, 0x05, "Failed to set LPS25H RES_CONF"))
        return false;

    if (!m_settings->HALWrite(m_pressureAddr, LPS25H_FIFO_CTRL, 0xc0, "Failed to set LPS25H FIFO_CTRL"))
        return false;

    if (!m_settings->HALWrite(m_pressureAddr, LPS25H_CTRL_REG_2, 0x40, "Failed to set LPS25H CTRL_REG_2"))
        return false;

    return true;
}


bool RTPressureLPS25H::pressureRead(RTIMU_DATA& data)
{
    unsigned char rawData[3];
    unsigned char status;

    data.pressureValid = false;
    data.temperatureValid = false;
    data.temperature = 0;
    data.pressure = 0;

    if (!m_settings->HALRead(m_pressureAddr, LPS25H_STATUS_REG, 1, &status, "Failed to read LPS25H status"))
        return false;

    if (status & 2) {
        if (!m_settings->HALRead(m_pressureAddr, LPS25H_PRESS_OUT_XL + 0x80, 3, rawData, "Failed to read LPS25H pressure"))
            return false;

        m_pressure = (RTFLOAT)((((unsigned int)rawData[2]) << 16) | (((unsigned int)rawData[1]) << 8) | (unsigned int)rawData[0]) / (RTFLOAT)4096;
        m_pressureValid = true;
    }
    if (status & 1) {
        if (!m_settings->HALRead(m_pressureAddr, LPS25H_TEMP_OUT_L + 0x80, 2, rawData, "Failed to read LPS25H temperature"))
            return false;

        m_temperature = (int16_t)((((unsigned int)rawData[1]) << 8) | (unsigned int)rawData[0]) / (RTFLOAT)480 + (RTFLOAT)42.5;
        m_temperatureValid = true;
    }

    data.pressureValid = m_pressureValid;
    data.pressure = m_pressure;
    data.temperatureValid = m_temperatureValid;
    data.temperature = m_temperature;

    return true;
}
