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

#include "RTPressureBMP180.h"

RTPressureBMP180::RTPressureBMP180(RTIMUSettings *settings) : RTPressure(settings)
{
    m_validReadings = false;
}

RTPressureBMP180::~RTPressureBMP180()
{
}

bool RTPressureBMP180::pressureInit()
{
    unsigned char result;
    unsigned char data[22];

    m_pressureAddr = m_settings->m_I2CPressureAddress;

    // check ID of chip

    if (!m_settings->HALRead(m_pressureAddr, BMP180_REG_ID, 1, &result, "Failed to read BMP180 id"))
        return false;

    if (result != BMP180_ID) {
        HAL_ERROR1("Incorrect BMP180 id %d\n", result);
        return false;
    }

    // get calibration data

    if (!m_settings->HALRead(m_pressureAddr, BMP180_REG_AC1, 22, data, "Failed to read BMP180 calibration data"))
        return false;

    m_AC1 = (int16_t)(((uint16_t)data[0]) << 8) + (uint16_t)data[1];
    m_AC2 = (int16_t)(((uint16_t)data[2]) << 8) + (uint16_t)data[3];
    m_AC3 = (int16_t)(((uint16_t)data[4]) << 8) + (uint16_t)data[4];
    m_AC4 = (((uint16_t)data[6]) << 8) + (uint16_t)data[7];
    m_AC5 = (((uint16_t)data[8]) << 8) + (uint16_t)data[9];
    m_AC6 = (((uint16_t)data[10]) << 8) + (uint16_t)data[11];
    m_B1 = (int16_t)(((uint16_t)data[12]) << 8) + (uint16_t)data[13];
    m_B2 = (int16_t)(((uint16_t)data[14]) << 8) + (uint16_t)data[15];
    m_MB = (int16_t)(((uint16_t)data[16]) << 8) + (uint16_t)data[17];
    m_MC = (int16_t)(((uint16_t)data[18]) << 8) + (uint16_t)data[19];
    m_MD = (int16_t)(((uint16_t)data[20]) << 8) + (uint16_t)data[21];

    m_state = BMP180_STATE_IDLE;
    m_oss = BMP180_SCO_PRESSURECONV_ULP;
    return true;
}

bool RTPressureBMP180::pressureRead(RTIMU_DATA& data)
{
    data.pressureValid = false;
    data.temperatureValid = false;
    data.temperature = 0;
    data.pressure = 0;

    if (m_state == BMP180_STATE_IDLE) {
        // start a temperature conversion
        if (!m_settings->HALWrite(m_pressureAddr, BMP180_REG_SCO, BMP180_SCO_TEMPCONV, "Failed to start temperature conversion")) {
            return false;
        } else {
            m_state = BMP180_STATE_TEMPERATURE;
        }
    }

    pressureBackground();

    if (m_validReadings) {
        data.pressureValid = true;
        data.temperatureValid = true;
        data.temperature = m_temperature;
        data.pressure = m_pressure;
        // printf("P: %f, T: %f\n", m_pressure, m_temperature);
    }
    return true;
}


void RTPressureBMP180::pressureBackground()
{
    uint8_t data[2];

    switch (m_state) {
        case BMP180_STATE_IDLE:
        break;

        case BMP180_STATE_TEMPERATURE:
        if (!m_settings->HALRead(m_pressureAddr, BMP180_REG_SCO, 1, data, "Failed to read BMP180 temp conv status")) {
            break;
        }
        if ((data[0] & 0x20) == 0x20)
            break;                                      // conversion not finished
        if (!m_settings->HALRead(m_pressureAddr, BMP180_REG_RESULT, 2, data, "Failed to read BMP180 temp conv result")) {
            m_state = BMP180_STATE_IDLE;
            break;
        }
        m_rawTemperature = (((uint16_t)data[0]) << 8) + (uint16_t)data[1];

        data[0] = 0x34 + (m_oss << 6);
        if (!m_settings->HALWrite(m_pressureAddr, BMP180_REG_SCO, 1, data, "Failed to start pressure conversion")) {
            m_state = BMP180_STATE_IDLE;
            break;
        }
        m_state = BMP180_STATE_PRESSURE;
        break;

        case BMP180_STATE_PRESSURE:
        if (!m_settings->HALRead(m_pressureAddr, BMP180_REG_SCO, 1, data, "Failed to read BMP180 pressure conv status")) {
            break;
        }
        if ((data[0] & 0x20) == 0x20)
            break;                                      // conversion not finished
        if (!m_settings->HALRead(m_pressureAddr, BMP180_REG_RESULT, 2, data, "Failed to read BMP180 temp conv result")) {
            m_state = BMP180_STATE_IDLE;
            break;
        }
        m_rawPressure = (((uint16_t)data[0]) << 8) + (uint16_t)data[1];

        if (!m_settings->HALRead(m_pressureAddr, BMP180_REG_XLSB, 1, data, "Failed to read BMP180 XLSB")) {
            m_state = BMP180_STATE_IDLE;
            break;
        }

        // call this function for testing only
        // should give T = 150 (15.0C) and pressure 6996 (699.6hPa)

        // setTestData();

        int32_t pressure = ((((uint32_t)(m_rawPressure)) << 8) + (uint32_t)(data[0])) >> (8 - m_oss);

        m_state = BMP180_STATE_IDLE;

        // calculate compensated temperature

        int32_t X1 = (((int32_t)m_rawTemperature - m_AC6) * m_AC5) / 32768;

        if ((X1 + m_MD) == 0) {
            break;
        }

        int32_t X2 = (m_MC * 2048)  / (X1 + m_MD);
        int32_t B5 = X1 + X2;
        m_temperature = (RTFLOAT)((B5 + 8) / 16) / (RTFLOAT)10;

        // calculate compensated pressure

        int32_t B6 = B5 - 4000;
        //          printf("B6 = %d\n", B6);
        X1 = (m_B2 * ((B6 * B6) / 4096)) / 2048;
        //          printf("X1 = %d\n", X1);
        X2 = (m_AC2 * B6) / 2048;
        //          printf("X2 = %d\n", X2);
        int32_t X3 = X1 + X2;
        //          printf("X3 = %d\n", X3);
        int32_t B3 = (((m_AC1 * 4 + X3) << m_oss) + 2) / 4;
        //          printf("B3 = %d\n", B3);
        X1 = (m_AC3 * B6) / 8192;
        //          printf("X1 = %d\n", X1);
        X2 = (m_B1 * ((B6 * B6) / 4096)) / 65536;
        //          printf("X2 = %d\n", X2);
        X3 = ((X1 + X2) + 2) / 4;
        //          printf("X3 = %d\n", X3);
        int32_t B4 = (m_AC4 * (unsigned long)(X3 + 32768)) / 32768;
        //          printf("B4 = %d\n", B4);
        uint32_t B7 = ((unsigned long)pressure - B3) * (50000 >> m_oss);
        //          printf("B7 = %d\n", B7);

        int32_t p;
        if (B7 < 0x80000000)
        p = (B7 * 2) / B4;
            else
        p = (B7 / B4) * 2;

        //          printf("p = %d\n", p);
        X1 = (p / 256) * (p / 256);
        //          printf("X1 = %d\n", X1);
        X1 = (X1 * 3038) / 65536;
        //          printf("X1 = %d\n", X1);
        X2 = (-7357 * p) / 65536;
        //          printf("X2 = %d\n", X2);
        m_pressure = (RTFLOAT)(p + (X1 + X2 + 3791) / 16) / (RTFLOAT)100;      // the extra 100 factor is to get 1hPa units

        m_validReadings = true;

        // printf("UP = %d, P = %f, UT = %d, T = %f\n", m_rawPressure, m_pressure, m_rawTemperature, m_temperature);
        break;
    }
}

void RTPressureBMP180::setTestData()
{
    m_AC1 = 408;
    m_AC2 = -72;
    m_AC3 = -14383;
    m_AC4 = 32741;
    m_AC5 = 32757;
    m_AC6 = 23153;
    m_B1 = 6190;
    m_B2 = 4;
    m_MB = -32767;
    m_MC = -8711;
    m_MD = 2868;

    m_rawTemperature = 27898;
    m_rawPressure = 23843;
}
