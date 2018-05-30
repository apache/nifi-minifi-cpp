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


#include "RTIMUGD20M303DLHC.h"
#include "RTIMUSettings.h"

//  this sets the learning rate for compass running average calculation

#define COMPASS_ALPHA 0.2f

RTIMUGD20M303DLHC::RTIMUGD20M303DLHC(RTIMUSettings *settings) : RTIMU(settings)
{
    m_sampleRate = 100;
}

RTIMUGD20M303DLHC::~RTIMUGD20M303DLHC()
{
}

bool RTIMUGD20M303DLHC::IMUInit()
{
    unsigned char result;

#ifdef GD20M303DLHC_CACHE_MODE
    m_firstTime = true;
    m_cacheIn = m_cacheOut = m_cacheCount = 0;
#endif
    // set validity flags

    m_imuData.fusionPoseValid = false;
    m_imuData.fusionQPoseValid = false;
    m_imuData.gyroValid = true;
    m_imuData.accelValid = true;
    m_imuData.compassValid = true;
    m_imuData.pressureValid = false;
    m_imuData.temperatureValid = false;
    m_imuData.humidityValid = false;

    //  configure IMU

    m_gyroSlaveAddr = m_settings->m_I2CSlaveAddress;
    m_accelSlaveAddr = LSM303DLHC_ACCEL_ADDRESS;
    m_compassSlaveAddr = LSM303DLHC_COMPASS_ADDRESS;

    setCalibrationData();

    //  enable the I2C bus

    if (!m_settings->HALOpen())
        return false;

    //  Set up the gyro

    if (!m_settings->HALWrite(m_gyroSlaveAddr, L3GD20_CTRL5, 0x80, "Failed to boot L3GD20"))
        return false;

    if (!m_settings->HALRead(m_gyroSlaveAddr, L3GD20_WHO_AM_I, 1, &result, "Failed to read L3GD20 id"))
        return false;

    if (result != L3GD20_ID) {
        HAL_ERROR1("Incorrect L3GD20 id %d\n", result);
        return false;
    }

    if (!setGyroSampleRate())
            return false;

    if (!setGyroCTRL2())
            return false;

    if (!setGyroCTRL4())
            return false;

    //  Set up the accel

    if (!setAccelCTRL1())
        return false;

    if (!setAccelCTRL4())
        return false;

    //  Set up the compass

    if (!setCompassCRA())
        return false;

    if (!setCompassCRB())
        return false;

    if (!setCompassCRM())
        return false;

#ifdef GD20M303DLHC_CACHE_MODE

    //  turn on gyro fifo

    if (!m_settings->HALWrite(m_gyroSlaveAddr, L3GD20_FIFO_CTRL, 0x3f, "Failed to set L3GD20 FIFO mode"))
        return false;
#endif

    if (!setGyroCTRL5())
            return false;

    gyroBiasInit();

    HAL_INFO("GD20M303DLHC init complete\n");
    return true;
}

bool RTIMUGD20M303DLHC::setGyroSampleRate()
{
    unsigned char ctrl1;

    switch (m_settings->m_GD20M303DLHCGyroSampleRate) {
    case L3GD20_SAMPLERATE_95:
        ctrl1 = 0x0f;
        m_sampleRate = 95;
        break;

    case L3GD20_SAMPLERATE_190:
        ctrl1 = 0x4f;
        m_sampleRate = 190;
        break;

    case L3GD20_SAMPLERATE_380:
        ctrl1 = 0x8f;
        m_sampleRate = 380;
        break;

    case L3GD20_SAMPLERATE_760:
        ctrl1 = 0xcf;
        m_sampleRate = 760;
        break;

    default:
        HAL_ERROR1("Illegal L3GD20 sample rate code %d\n", m_settings->m_GD20M303DLHCGyroSampleRate);
        return false;
    }

    m_sampleInterval = (uint64_t)1000000 / m_sampleRate;

    switch (m_settings->m_GD20M303DLHCGyroBW) {
    case L3GD20_BANDWIDTH_0:
        ctrl1 |= 0x00;
        break;

    case L3GD20_BANDWIDTH_1:
        ctrl1 |= 0x10;
        break;

    case L3GD20_BANDWIDTH_2:
        ctrl1 |= 0x20;
        break;

    case L3GD20_BANDWIDTH_3:
        ctrl1 |= 0x30;
        break;

    }

    return (m_settings->HALWrite(m_gyroSlaveAddr, L3GD20_CTRL1, ctrl1, "Failed to set L3GD20 CTRL1"));
}

bool RTIMUGD20M303DLHC::setGyroCTRL2()
{
    if ((m_settings->m_GD20M303DLHCGyroHpf < L3GD20_HPF_0) || (m_settings->m_GD20M303DLHCGyroHpf > L3GD20_HPF_9)) {
        HAL_ERROR1("Illegal L3GD20 high pass filter code %d\n", m_settings->m_GD20M303DLHCGyroHpf);
        return false;
    }
    return m_settings->HALWrite(m_gyroSlaveAddr,  L3GD20_CTRL2, m_settings->m_GD20M303DLHCGyroHpf, "Failed to set L3GD20 CTRL2");
}

bool RTIMUGD20M303DLHC::setGyroCTRL4()
{
    unsigned char ctrl4;

    switch (m_settings->m_GD20M303DLHCGyroFsr) {
    case L3GD20_FSR_250:
        ctrl4 = 0x00;
        m_gyroScale = (RTFLOAT)0.00875 * RTMATH_DEGREE_TO_RAD;
        break;

    case L3GD20_FSR_500:
        ctrl4 = 0x10;
        m_gyroScale = (RTFLOAT)0.0175 * RTMATH_DEGREE_TO_RAD;
        break;

    case L3GD20_FSR_2000:
        ctrl4 = 0x20;
        m_gyroScale = (RTFLOAT)0.07 * RTMATH_DEGREE_TO_RAD;
        break;

    default:
        HAL_ERROR1("Illegal L3GD20 FSR code %d\n", m_settings->m_GD20M303DLHCGyroFsr);
        return false;
    }

    return m_settings->HALWrite(m_gyroSlaveAddr,  L3GD20_CTRL4, ctrl4, "Failed to set L3GD20 CTRL4");
}


bool RTIMUGD20M303DLHC::setGyroCTRL5()
{
    unsigned char ctrl5;

    //  Turn on hpf

    ctrl5 = 0x10;

#ifdef GD20M303DLHC_CACHE_MODE
    //  turn on fifo

    ctrl5 |= 0x40;
#endif

    return m_settings->HALWrite(m_gyroSlaveAddr,  L3GD20_CTRL5, ctrl5, "Failed to set L3GD20 CTRL5");
}


bool RTIMUGD20M303DLHC::setAccelCTRL1()
{
    unsigned char ctrl1;

    if ((m_settings->m_GD20M303DLHCAccelSampleRate < 1) || (m_settings->m_GD20M303DLHCAccelSampleRate > 7)) {
        HAL_ERROR1("Illegal LSM303DLHC accel sample rate code %d\n", m_settings->m_GD20M303DLHCAccelSampleRate);
        return false;
    }

    ctrl1 = (m_settings->m_GD20M303DLHCAccelSampleRate << 4) | 0x07;

    return m_settings->HALWrite(m_accelSlaveAddr,  LSM303DLHC_CTRL1_A, ctrl1, "Failed to set LSM303D CTRL1");
}

bool RTIMUGD20M303DLHC::setAccelCTRL4()
{
    unsigned char ctrl4;

    switch (m_settings->m_GD20M303DLHCAccelFsr) {
    case LSM303DLHC_ACCEL_FSR_2:
        m_accelScale = (RTFLOAT)0.001 / (RTFLOAT)16;
        break;

    case LSM303DLHC_ACCEL_FSR_4:
        m_accelScale = (RTFLOAT)0.002 / (RTFLOAT)16;
        break;

    case LSM303DLHC_ACCEL_FSR_8:
        m_accelScale = (RTFLOAT)0.004 / (RTFLOAT)16;
        break;

    case LSM303DLHC_ACCEL_FSR_16:
        m_accelScale = (RTFLOAT)0.012 / (RTFLOAT)16;
        break;

    default:
        HAL_ERROR1("Illegal LSM303DLHC accel FSR code %d\n", m_settings->m_GD20M303DLHCAccelFsr);
        return false;
    }

    ctrl4 = 0x80 + (m_settings->m_GD20M303DLHCAccelFsr << 4);

    return m_settings->HALWrite(m_accelSlaveAddr,  LSM303DLHC_CTRL4_A, ctrl4, "Failed to set LSM303DLHC CTRL4");
}


bool RTIMUGD20M303DLHC::setCompassCRA()
{
    unsigned char cra;

    if ((m_settings->m_GD20M303DLHCCompassSampleRate < 0) || (m_settings->m_GD20M303DLHCCompassSampleRate > 7)) {
        HAL_ERROR1("Illegal LSM303DLHC compass sample rate code %d\n", m_settings->m_GD20M303DLHCCompassSampleRate);
        return false;
    }

    cra = (m_settings->m_GD20M303DLHCCompassSampleRate << 2);

    return m_settings->HALWrite(m_compassSlaveAddr,  LSM303DLHC_CRA_M, cra, "Failed to set LSM303DLHC CRA_M");
}

bool RTIMUGD20M303DLHC::setCompassCRB()
{
    unsigned char crb;

    //  convert FSR to uT

    switch (m_settings->m_GD20M303DLHCCompassFsr) {
    case LSM303DLHC_COMPASS_FSR_1_3:
        crb = 0x20;
        m_compassScaleXY = (RTFLOAT)100 / (RTFLOAT)1100;
        m_compassScaleZ = (RTFLOAT)100 / (RTFLOAT)980;
        break;

    case LSM303DLHC_COMPASS_FSR_1_9:
        crb = 0x40;
        m_compassScaleXY = (RTFLOAT)100 / (RTFLOAT)855;
        m_compassScaleZ = (RTFLOAT)100 / (RTFLOAT)760;
       break;

    case LSM303DLHC_COMPASS_FSR_2_5:
        crb = 0x60;
        m_compassScaleXY = (RTFLOAT)100 / (RTFLOAT)670;
        m_compassScaleZ = (RTFLOAT)100 / (RTFLOAT)600;
        break;

    case LSM303DLHC_COMPASS_FSR_4:
        crb = 0x80;
        m_compassScaleXY = (RTFLOAT)100 / (RTFLOAT)450;
        m_compassScaleZ = (RTFLOAT)100 / (RTFLOAT)400;
        break;

    case LSM303DLHC_COMPASS_FSR_4_7:
        crb = 0xa0;
        m_compassScaleXY = (RTFLOAT)100 / (RTFLOAT)400;
        m_compassScaleZ = (RTFLOAT)100 / (RTFLOAT)355;
        break;

    case LSM303DLHC_COMPASS_FSR_5_6:
        crb = 0xc0;
        m_compassScaleXY = (RTFLOAT)100 / (RTFLOAT)330;
        m_compassScaleZ = (RTFLOAT)100 / (RTFLOAT)295;
        break;

    case LSM303DLHC_COMPASS_FSR_8_1:
        crb = 0xe0;
        m_compassScaleXY = (RTFLOAT)100 / (RTFLOAT)230;
        m_compassScaleZ = (RTFLOAT)100 / (RTFLOAT)205;
        break;

    default:
        HAL_ERROR1("Illegal LSM303DLHC compass FSR code %d\n", m_settings->m_GD20M303DLHCCompassFsr);
        return false;
    }

    return m_settings->HALWrite(m_compassSlaveAddr,  LSM303DLHC_CRB_M, crb, "Failed to set LSM303DLHC CRB_M");
}

bool RTIMUGD20M303DLHC::setCompassCRM()
{
     return m_settings->HALWrite(m_compassSlaveAddr,  LSM303DLHC_CRM_M, 0x00, "Failed to set LSM303DLHC CRM_M");
}


int RTIMUGD20M303DLHC::IMUGetPollInterval()
{
    return (400 / m_sampleRate);
}

bool RTIMUGD20M303DLHC::IMURead()
{
    unsigned char status;
    unsigned char gyroData[6];
    unsigned char accelData[6];
    unsigned char compassData[6];


#ifdef GD20M303DLHC_CACHE_MODE
    int count;

    if (!m_settings->HALRead(m_gyroSlaveAddr, L3GD20_FIFO_SRC, 1, &status, "Failed to read L3GD20 fifo status"))
        return false;

    if ((status & 0x40) != 0) {
        HAL_INFO("L3GD20 fifo overrun\n");
        if (!m_settings->HALWrite(m_gyroSlaveAddr, L3GD20_CTRL5, 0x10, "Failed to set L3GD20 CTRL5"))
            return false;

        if (!m_settings->HALWrite(m_gyroSlaveAddr, L3GD20_FIFO_CTRL, 0x0, "Failed to set L3GD20 FIFO mode"))
            return false;

        if (!m_settings->HALWrite(m_gyroSlaveAddr, L3GD20_FIFO_CTRL, 0x3f, "Failed to set L3GD20 FIFO mode"))
            return false;

        if (!setGyroCTRL5())
            return false;

        m_imuData.timestamp += m_sampleInterval * 32;
        return false;
    }

    // get count of samples in fifo
    count = status & 0x1f;

    if ((m_cacheCount == 0) && (count > 0) && (count < GD20M303DLHC_FIFO_THRESH)) {
        // special case of a small fifo and nothing cached - just handle as simple read

        if (!m_settings->HALRead(m_gyroSlaveAddr, 0x80 | L3GD20_OUT_X_L, 6, gyroData, "Failed to read L3GD20 data"))
            return false;

        if (!m_settings->HALRead(m_accelSlaveAddr, 0x80 | LSM303DLHC_OUT_X_L_A, 6, accelData, "Failed to read LSM303DLHC accel data"))
            return false;

        if (!m_settings->HALRead(m_compassSlaveAddr, 0x80 | LSM303DLHC_OUT_X_H_M, 6, compassData, "Failed to read LSM303DLHC compass data"))
            return false;

        if (m_firstTime)
            m_imuData.timestamp = RTMath::currentUSecsSinceEpoch();
        else
            m_imuData.timestamp += m_sampleInterval;

        m_firstTime = false;
    } else {
        if (count >=  GD20M303DLHC_FIFO_THRESH) {
            // need to create a cache block

            if (m_cacheCount == GD20M303DLHC_CACHE_BLOCK_COUNT) {
                // all cache blocks are full - discard oldest and update timestamp to account for lost samples
                m_imuData.timestamp += m_sampleInterval * m_cache[m_cacheOut].count;
                if (++m_cacheOut == GD20M303DLHC_CACHE_BLOCK_COUNT)
                    m_cacheOut = 0;
                m_cacheCount--;
            }

            if (!m_settings->HALRead(m_gyroSlaveAddr, 0x80 | L3GD20_OUT_X_L, GD20M303DLHC_FIFO_CHUNK_SIZE * GD20M303DLHC_FIFO_THRESH,
                         m_cache[m_cacheIn].data, "Failed to read L3GD20 fifo data"))
                return false;

            if (!m_settings->HALRead(m_accelSlaveAddr, 0x80 | LSM303DLHC_OUT_X_L_A, 6,
                         m_cache[m_cacheIn].accel, "Failed to read LSM303DLHC accel data"))
                return false;

            if (!m_settings->HALRead(m_compassSlaveAddr, 0x80 | LSM303DLHC_OUT_X_H_M, 6,
                         m_cache[m_cacheIn].compass, "Failed to read LSM303DLHC compass data"))
                return false;

            m_cache[m_cacheIn].count = GD20M303DLHC_FIFO_THRESH;
            m_cache[m_cacheIn].index = 0;

            m_cacheCount++;
            if (++m_cacheIn == GD20M303DLHC_CACHE_BLOCK_COUNT)
                m_cacheIn = 0;

        }

        //  now fifo has been read if necessary, get something to process

        if (m_cacheCount == 0)
            return false;

        memcpy(gyroData, m_cache[m_cacheOut].data + m_cache[m_cacheOut].index, GD20M303DLHC_FIFO_CHUNK_SIZE);
        memcpy(accelData, m_cache[m_cacheOut].accel, 6);
        memcpy(compassData, m_cache[m_cacheOut].compass, 6);

        m_cache[m_cacheOut].index += GD20M303DLHC_FIFO_CHUNK_SIZE;

        if (--m_cache[m_cacheOut].count == 0) {
            //  this cache block is now empty

            if (++m_cacheOut == GD20M303DLHC_CACHE_BLOCK_COUNT)
                m_cacheOut = 0;
            m_cacheCount--;
        }
        if (m_firstTime)
            m_imuData.timestamp = RTMath::currentUSecsSinceEpoch();
        else
            m_imuData.timestamp += m_sampleInterval;

        m_firstTime = false;
    }

#else
    if (!m_settings->HALRead(m_gyroSlaveAddr, L3GD20_STATUS, 1, &status, "Failed to read L3GD20 status"))
        return false;

    if ((status & 0x8) == 0)
        return false;

    if (!m_settings->HALRead(m_gyroSlaveAddr, 0x80 | L3GD20_OUT_X_L, 6, gyroData, "Failed to read L3GD20 data"))
        return false;

    m_imuData.timestamp = RTMath::currentUSecsSinceEpoch();

    if (!m_settings->HALRead(m_accelSlaveAddr, 0x80 | LSM303DLHC_OUT_X_L_A, 6, accelData, "Failed to read LSM303DLHC accel data"))
        return false;

    if (!m_settings->HALRead(m_compassSlaveAddr, 0x80 | LSM303DLHC_OUT_X_H_M, 6, compassData, "Failed to read LSM303DLHC compass data"))
        return false;

#endif

    RTMath::convertToVector(gyroData, m_imuData.gyro, m_gyroScale, false);
    RTMath::convertToVector(accelData, m_imuData.accel, m_accelScale, false);

    m_imuData.compass.setX((RTFLOAT)((int16_t)(((uint16_t)compassData[0] << 8) | (uint16_t)compassData[1])) * m_compassScaleXY);
    m_imuData.compass.setY((RTFLOAT)((int16_t)(((uint16_t)compassData[2] << 8) | (uint16_t)compassData[3])) * m_compassScaleXY);
    m_imuData.compass.setZ((RTFLOAT)((int16_t)(((uint16_t)compassData[4] << 8) | (uint16_t)compassData[5])) * m_compassScaleZ);

    //  sort out gyro axes

    m_imuData.gyro.setX(m_imuData.gyro.x());
    m_imuData.gyro.setY(-m_imuData.gyro.y());
    m_imuData.gyro.setZ(-m_imuData.gyro.z());

    //  sort out accel data;

    m_imuData.accel.setX(-m_imuData.accel.x());

    //  sort out compass axes

    RTFLOAT temp;

    temp = m_imuData.compass.z();
    m_imuData.compass.setZ(-m_imuData.compass.y());
    m_imuData.compass.setY(-temp);

    //  now do standard processing

    handleGyroBias();
    calibrateAverageCompass();
    calibrateAccel();

    //  now update the filter

    updateFusion();

    return true;
}
