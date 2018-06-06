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

#include "RTHumidityHTS221.h"
#include "RTHumidityDefs.h"

RTHumidityHTS221::RTHumidityHTS221(RTIMUSettings *settings) : RTHumidity(settings)
{
    m_humidityValid = false;
    m_temperatureValid = false;
 }

RTHumidityHTS221::~RTHumidityHTS221()
{
}

bool RTHumidityHTS221::humidityInit()
{
    unsigned char rawData[2];
    uint8_t H0_H_2 = 0;
    uint8_t H1_H_2 = 0;
    uint16_t T0_C_8 = 0;
    uint16_t T1_C_8 = 0;
    int16_t H0_T0_OUT = 0;
    int16_t H1_T0_OUT = 0;
    int16_t T0_OUT = 0;
    int16_t T1_OUT = 0;
    float H0, H1, T0, T1;

    m_humidityAddr = m_settings->m_I2CHumidityAddress;

    if (!m_settings->HALWrite(m_humidityAddr, HTS221_CTRL1, 0x87, "Failed to set HTS221 CTRL_REG_1"))
        return false;

    if (!m_settings->HALWrite(m_humidityAddr, HTS221_AV_CONF, 0x1b, "Failed to set HTS221 AV_CONF"))
        return false;

    // Get calibration data

    if (!m_settings->HALRead(m_humidityAddr, HTS221_T1_T0 + 0x80, 1, &rawData[1], "Failed to read HTS221 T1_T0"))
        return false;
    if (!m_settings->HALRead(m_humidityAddr, HTS221_T0_C_8 + 0x80, 1, rawData, "Failed to read HTS221 T0_C_8"))
        return false;
    T0_C_8 = (((unsigned int)rawData[1] & 0x3 ) << 8) | (unsigned int)rawData[0];
    T0 = (RTFLOAT)T0_C_8 / 8;

    if (!m_settings->HALRead(m_humidityAddr, HTS221_T1_C_8 + 0x80, 1, rawData, "Failed to read HTS221 T1_C_8"))
        return false;
    T1_C_8 = (unsigned int)(((uint16_t)(rawData[1] & 0xC) << 6) | (uint16_t)rawData[0]);
    T1 = (RTFLOAT)T1_C_8 / 8;

    if (!m_settings->HALRead(m_humidityAddr, HTS221_T0_OUT + 0x80, 2, rawData, "Failed to read HTS221 T0_OUT"))
        return false;
    T0_OUT = (int16_t)(((unsigned int)rawData[1]) << 8) | (unsigned int)rawData[0];

    if (!m_settings->HALRead(m_humidityAddr, HTS221_T1_OUT + 0x80, 2, rawData, "Failed to read HTS221 T1_OUT"))
        return false;
    T1_OUT = (int16_t)(((unsigned int)rawData[1]) << 8) | (unsigned int)rawData[0];

    if (!m_settings->HALRead(m_humidityAddr, HTS221_H0_H_2 + 0x80, 1, &H0_H_2, "Failed to read HTS221 H0_H_2"))
        return false;
    H0 = (RTFLOAT)H0_H_2 / 2;

    if (!m_settings->HALRead(m_humidityAddr, HTS221_H1_H_2 + 0x80, 1, &H1_H_2, "Failed to read HTS221 H1_H_2"))
        return false;
    H1 = (RTFLOAT)H1_H_2 / 2;

    if (!m_settings->HALRead(m_humidityAddr, HTS221_H0_T0_OUT + 0x80, 2, rawData, "Failed to read HTS221 H0_T_OUT"))
        return false;
    H0_T0_OUT = (int16_t)(((unsigned int)rawData[1]) << 8) | (unsigned int)rawData[0];


    if (!m_settings->HALRead(m_humidityAddr, HTS221_H1_T0_OUT + 0x80, 2, rawData, "Failed to read HTS221 H1_T_OUT"))
        return false;
    H1_T0_OUT = (int16_t)(((unsigned int)rawData[1]) << 8) | (unsigned int)rawData[0];

    m_temperature_m = (T1-T0)/(T1_OUT-T0_OUT);
    m_temperature_c = T0-(m_temperature_m*T0_OUT);
    m_humidity_m = (H1-H0)/(H1_T0_OUT-H0_T0_OUT);
    m_humidity_c = (H0)-(m_humidity_m*H0_T0_OUT);

    return true;
}


bool RTHumidityHTS221::humidityRead(RTIMU_DATA& data)
{
    unsigned char rawData[2];
    unsigned char status;

    data.humidityValid = false;
    data.temperatureValid = false;
    data.temperature = 0;
    data.humidity = 0;

    if (!m_settings->HALRead(m_humidityAddr, HTS221_STATUS, 1, &status, "Failed to read HTS221 status"))
        return false;

    if (status & 2) {
        if (!m_settings->HALRead(m_humidityAddr, HTS221_HUMIDITY_OUT_L + 0x80, 2, rawData, "Failed to read HTS221 humidity"))
            return false;

        m_humidity = (int16_t)((((unsigned int)rawData[1]) << 8) | (unsigned int)rawData[0]);
        m_humidity = m_humidity * m_humidity_m + m_humidity_c;
        m_humidityValid = true;
    }
    if (status & 1) {
        if (!m_settings->HALRead(m_humidityAddr, HTS221_TEMP_OUT_L + 0x80, 2, rawData, "Failed to read HTS221 temperature"))
            return false;

        m_temperature = (int16_t)((((unsigned int)rawData[1]) << 8) | (unsigned int)rawData[0]);
        m_temperature = m_temperature * m_temperature_m + m_temperature_c;
        m_temperatureValid = true;
    }

    data.humidityValid = m_humidityValid;
    data.humidity = m_humidity;
    data.temperatureValid = m_temperatureValid;
    data.temperature = m_temperature;

    return true;
}
