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

#include "RTHumidityHTU21D.h"
#include "RTHumidityDefs.h"

#define HTU21D_STATE_IN_RESET           0                   // reset in progress
#define HTU21D_STATE_IDLE               1                   // nothing happening
#define HTU21D_STATE_TEMP_REQ           2                   // requested temperature
#define HTU21D_STATE_HUM_REQ            3                   // requested humidity

#define HTU21D_STATE_INTERVAL           100000              // the interval between state changes

RTHumidityHTU21D::RTHumidityHTU21D(RTIMUSettings *settings) : RTHumidity(settings)
{
    m_humidityValid = false;
    m_temperatureValid = false;
    m_humidity = 0;
    m_temperature = 0;
 }

RTHumidityHTU21D::~RTHumidityHTU21D()
{
}

bool RTHumidityHTU21D::humidityInit()
{
    m_humidityAddr = m_settings->m_I2CHumidityAddress;

    if (!m_settings->HALWrite(m_humidityAddr, HTU21D_CMD_SOFT_RESET, 0, NULL, "Failed to reset HTU21D"))
        return false;

    m_state = HTU21D_STATE_IN_RESET;
    m_startTime = RTMath::currentUSecsSinceEpoch();
    return true;
}


bool RTHumidityHTU21D::humidityRead(RTIMU_DATA& data)
{
    if (!processBackground())
        return false;

    data.humidityValid = m_humidityValid;
    data.humidity = m_humidity;
    data.temperatureValid = m_temperatureValid;
    data.temperature = m_temperature;

    return true;
}

bool RTHumidityHTU21D:: processBackground()
{
    unsigned char rawData[3];
    uint64_t now = RTMath::currentUSecsSinceEpoch();
    bool expired = (now - m_startTime) >= HTU21D_STATE_INTERVAL;

    if (!expired)
        return true;

    switch (m_state) {
    case HTU21D_STATE_IN_RESET:
        m_state = HTU21D_STATE_IDLE;
        m_startTime = now;
        break;

    case HTU21D_STATE_IDLE:
        // start a temperature conversion
        if (!m_settings->HALWrite(m_humidityAddr, HTU21D_CMD_TRIG_TEMP, 0, NULL, "Failed to start HTU21D temp conv"))
            return false;
        m_state = HTU21D_STATE_TEMP_REQ;
        m_startTime = now;
        break;

    case HTU21D_STATE_TEMP_REQ:
        // read temp data
        if (!m_settings->HALRead(m_humidityAddr, 3, rawData, "Failed to read HTU21D temperature"))
            return false;
        // remove status bits
        rawData[1] &= 0xfc;
        m_temperature = -46.85 + 175.72 * (RTFLOAT)((((uint16_t)rawData[0]) << 8) | (uint16_t)rawData[1]) / 65536.0;
        m_temperatureValid = true;

        // start humidity conversion
        if (!m_settings->HALWrite(m_humidityAddr, HTU21D_CMD_TRIG_HUM, 0, NULL, "Failed to start HTU21D humidity conv"))
            return false;
        m_state = HTU21D_STATE_HUM_REQ;
        m_startTime = now;
        break;

    case HTU21D_STATE_HUM_REQ:
        // read humidity data
        if (!m_settings->HALRead(m_humidityAddr, 3, rawData, "Failed to read HTU21D humidity"))
            return false;
        // remove status bits
        rawData[1] &= 0xfc;
        m_humidity = -6.0 + 125.0 * (RTFLOAT)((((uint16_t)rawData[0]) << 8) | (uint16_t)rawData[1]) / 65536.0;
        // do temp compensation
        m_humidity += (25.0 - m_temperature) * -0.15;
        m_humidityValid = true;
        m_state = HTU21D_STATE_IDLE;
        m_startTime = now;
        break;
    }
    return true;
}
