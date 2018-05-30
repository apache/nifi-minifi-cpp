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

#include "RTPressureMS5637.h"

RTPressureMS5637::RTPressureMS5637(RTIMUSettings *settings) : RTPressure(settings)
{
    m_validReadings = false;
}

RTPressureMS5637::~RTPressureMS5637()
{
}

bool RTPressureMS5637::pressureInit()
{
    unsigned char cmd = MS5611_CMD_PROM + 2;
    unsigned char data[2];

    m_pressureAddr = m_settings->m_I2CPressureAddress;

    // get calibration data

    for (int i = 0; i < 6; i++) {
        if (!m_settings->HALRead(m_pressureAddr, cmd, 2, data, "Failed to read MS5611 calibration data"))
            return false;
        m_calData[i] = (((uint16_t)data[0]) << 8) | ((uint16_t)data[1]);
        // printf("Cal index: %d, data: %d\n", i, m_calData[i]);
        cmd += 2;
    }

    m_state = MS5637_STATE_IDLE;
    return true;
}

bool RTPressureMS5637::pressureRead(RTIMU_DATA& data)
{
    data.pressureValid = false;
    data.temperatureValid = false;
    data.temperature = 0;
    data.pressure = 0;

    if (m_state == MS5637_STATE_IDLE) {
        // start pressure conversion
        if (!m_settings->HALWrite(m_pressureAddr, MS5611_CMD_CONV_D1, 0, 0, "Failed to start MS5611 pressure conversion")) {
            return false;
        } else {
            m_state = MS5637_STATE_PRESSURE;
            m_timer = RTMath::currentUSecsSinceEpoch();
        }
    }

    pressureBackground();

    if (m_validReadings) {
        data.pressureValid = true;
        data.temperatureValid = true;
        data.temperature = m_temperature;
        data.pressure = m_pressure;
    }
    return true;
}


void RTPressureMS5637::pressureBackground()
{
    uint8_t data[3];

    switch (m_state) {
        case MS5637_STATE_IDLE:
        break;

        case MS5637_STATE_PRESSURE:
        if ((RTMath::currentUSecsSinceEpoch() - m_timer) < 10000)
            break;                                          // not time yet
        if (!m_settings->HALRead(m_pressureAddr, MS5611_CMD_ADC, 3, data, "Failed to read MS5611 pressure")) {
            break;
        }
        m_D1 = (((uint32_t)data[0]) << 16) | (((uint32_t)data[1]) << 8) | ((uint32_t)data[2]);

        // start temperature conversion

        if (!m_settings->HALWrite(m_pressureAddr, MS5611_CMD_CONV_D2, 0, 0, "Failed to start MS5611 temperature conversion")) {
            break;
        } else {
            m_state = MS5637_STATE_TEMPERATURE;
            m_timer = RTMath::currentUSecsSinceEpoch();
        }
        break;

        case MS5637_STATE_TEMPERATURE:
        if ((RTMath::currentUSecsSinceEpoch() - m_timer) < 10000)
            break;                                          // not time yet
        if (!m_settings->HALRead(m_pressureAddr, MS5611_CMD_ADC, 3, data, "Failed to read MS5611 temperature")) {
            break;
        }
        m_D2 = (((uint32_t)data[0]) << 16) | (((uint32_t)data[1]) << 8) | ((uint32_t)data[2]);

        //  call this function for testing only
        //  should give T = 2000 (20.00C) and pressure 110002 (1100.02hPa)

        // setTestData();

        //  now calculate the real values

        int64_t deltaT = (int32_t)m_D2 - (((int32_t)m_calData[4]) << 8);

        int32_t temperature = 2000 + ((deltaT * (int64_t)m_calData[5]) >> 23); // note - this still needs to be divided by 100

        int64_t offset = (((int64_t)m_calData[1]) << 17) + ((m_calData[3] * deltaT) >> 6);
        int64_t sens = (((int64_t)m_calData[0]) << 16) + ((m_calData[2] * deltaT) >> 7);

        //  do second order temperature compensation

        if (temperature < 2000) {
            int64_t T2 = (3 * (deltaT * deltaT)) >> 33;
            int64_t offset2 = 61 * ((temperature - 2000) * (temperature - 2000)) / 16;
            int64_t sens2 = 29 * ((temperature - 2000) * (temperature - 2000)) / 16;
            if (temperature < -1500) {
                offset2 += 17 * (temperature + 1500) * (temperature + 1500);
                sens2 += 9 * ((temperature + 1500) * (temperature + 1500));
            }
            temperature -= T2;
            offset -= offset2;
            sens -=sens2;
        } else {
            temperature -= (5 * (deltaT * deltaT)) >> 38;
        }

        m_pressure = (RTFLOAT)(((((int64_t)m_D1 * sens) >> 21) - offset) >> 15) / (RTFLOAT)100.0;
        m_temperature = (RTFLOAT)temperature/(RTFLOAT)100;

        // printf("Temp: %f, pressure: %f\n", m_temperature, m_pressure);

        m_validReadings = true;
        m_state = MS5637_STATE_IDLE;
        break;
    }
}

void RTPressureMS5637::setTestData()
{
    m_calData[0] = 46372;
    m_calData[1] = 43981;
    m_calData[2] = 29059;
    m_calData[3] = 27842;
    m_calData[4] = 31553;
    m_calData[5] = 28165;

    m_D1 = 6465444;
    m_D2 = 8077636;
}
