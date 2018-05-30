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


#include "RTIMULSM9DS0.h"
#include "RTIMUSettings.h"

//  this sets the learning rate for compass running average calculation

#define COMPASS_ALPHA 0.2f

RTIMULSM9DS0::RTIMULSM9DS0(RTIMUSettings *settings) : RTIMU(settings)
{
    m_sampleRate = 100;
}

RTIMULSM9DS0::~RTIMULSM9DS0()
{
}

bool RTIMULSM9DS0::IMUInit()
{
    unsigned char result;

#ifdef LSM9DS0_CACHE_MODE
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

    // work out accelmag address

    if (m_settings->HALRead(LSM9DS0_ACCELMAG_ADDRESS0, LSM9DS0_WHO_AM_I, 1, &result, "")) {
        if (result == LSM9DS0_ACCELMAG_ID) {
            m_accelCompassSlaveAddr = LSM9DS0_ACCELMAG_ADDRESS0;
        }
    } else {
        m_accelCompassSlaveAddr = LSM9DS0_ACCELMAG_ADDRESS1;
    }

    setCalibrationData();

    //  enable the I2C bus

    if (!m_settings->HALOpen())
        return false;

    //  Set up the gyro

    if (!m_settings->HALWrite(m_gyroSlaveAddr, LSM9DS0_GYRO_CTRL5, 0x80, "Failed to boot LSM9DS0"))
        return false;

    if (!m_settings->HALRead(m_gyroSlaveAddr, LSM9DS0_GYRO_WHO_AM_I, 1, &result, "Failed to read LSM9DS0 gyro id"))
        return false;

    if (result != LSM9DS0_GYRO_ID) {
        HAL_ERROR1("Incorrect LSM9DS0 gyro id %d\n", result);
        return false;
    }

    if (!setGyroSampleRate())
            return false;

    if (!setGyroCTRL2())
            return false;

    if (!setGyroCTRL4())
            return false;

    //  Set up the accel

    if (!m_settings->HALRead(m_accelCompassSlaveAddr, LSM9DS0_WHO_AM_I, 1, &result, "Failed to read LSM9DS0 accel/mag id"))
        return false;

    if (result != LSM9DS0_ACCELMAG_ID) {
        HAL_ERROR1("Incorrect LSM9DS0 accel/mag id %d\n", result);
        return false;
    }

    if (!setAccelCTRL1())
        return false;

    if (!setAccelCTRL2())
        return false;

    if (!setCompassCTRL5())
        return false;

    if (!setCompassCTRL6())
        return false;

    if (!setCompassCTRL7())
        return false;

#ifdef LSM9DS0_CACHE_MODE

    //  turn on gyro fifo

    if (!m_settings->HALWrite(m_gyroSlaveAddr, LSM9DS0_GYRO_FIFO_CTRL, 0x3f, "Failed to set LSM9DS0 FIFO mode"))
        return false;
#endif

    if (!setGyroCTRL5())
            return false;

    gyroBiasInit();

    HAL_INFO("LSM9DS0 init complete\n");
    return true;
}

bool RTIMULSM9DS0::setGyroSampleRate()
{
    unsigned char ctrl1;

    switch (m_settings->m_LSM9DS0GyroSampleRate) {
    case LSM9DS0_GYRO_SAMPLERATE_95:
        ctrl1 = 0x0f;
        m_sampleRate = 95;
        break;

    case LSM9DS0_GYRO_SAMPLERATE_190:
        ctrl1 = 0x4f;
        m_sampleRate = 190;
        break;

    case LSM9DS0_GYRO_SAMPLERATE_380:
        ctrl1 = 0x8f;
        m_sampleRate = 380;
        break;

    case LSM9DS0_GYRO_SAMPLERATE_760:
        ctrl1 = 0xcf;
        m_sampleRate = 760;
        break;

    default:
        HAL_ERROR1("Illegal LSM9DS0 gyro sample rate code %d\n", m_settings->m_LSM9DS0GyroSampleRate);
        return false;
    }

    m_sampleInterval = (uint64_t)1000000 / m_sampleRate;

    switch (m_settings->m_LSM9DS0GyroBW) {
    case LSM9DS0_GYRO_BANDWIDTH_0:
        ctrl1 |= 0x00;
        break;

    case LSM9DS0_GYRO_BANDWIDTH_1:
        ctrl1 |= 0x10;
        break;

    case LSM9DS0_GYRO_BANDWIDTH_2:
        ctrl1 |= 0x20;
        break;

    case LSM9DS0_GYRO_BANDWIDTH_3:
        ctrl1 |= 0x30;
        break;

    }

    return (m_settings->HALWrite(m_gyroSlaveAddr, LSM9DS0_GYRO_CTRL1, ctrl1, "Failed to set LSM9DS0 gyro CTRL1"));
}

bool RTIMULSM9DS0::setGyroCTRL2()
{
    if ((m_settings->m_LSM9DS0GyroHpf < LSM9DS0_GYRO_HPF_0) || (m_settings->m_LSM9DS0GyroHpf > LSM9DS0_GYRO_HPF_9)) {
        HAL_ERROR1("Illegal LSM9DS0 gyro high pass filter code %d\n", m_settings->m_LSM9DS0GyroHpf);
        return false;
    }
    return m_settings->HALWrite(m_gyroSlaveAddr,  LSM9DS0_GYRO_CTRL2, m_settings->m_LSM9DS0GyroHpf, "Failed to set LSM9DS0 gyro CTRL2");
}

bool RTIMULSM9DS0::setGyroCTRL4()
{
    unsigned char ctrl4;

    switch (m_settings->m_LSM9DS0GyroFsr) {
    case LSM9DS0_GYRO_FSR_250:
        ctrl4 = 0x00;
        m_gyroScale = (RTFLOAT)0.00875 * RTMATH_DEGREE_TO_RAD;
        break;

    case LSM9DS0_GYRO_FSR_500:
        ctrl4 = 0x10;
        m_gyroScale = (RTFLOAT)0.0175 * RTMATH_DEGREE_TO_RAD;
        break;

    case LSM9DS0_GYRO_FSR_2000:
        ctrl4 = 0x20;
        m_gyroScale = (RTFLOAT)0.07 * RTMATH_DEGREE_TO_RAD;
        break;

    default:
        HAL_ERROR1("Illegal LSM9DS0 gyro FSR code %d\n", m_settings->m_LSM9DS0GyroFsr);
        return false;
    }

    return m_settings->HALWrite(m_gyroSlaveAddr,  LSM9DS0_GYRO_CTRL4, ctrl4, "Failed to set LSM9DS0 gyro CTRL4");
}


bool RTIMULSM9DS0::setGyroCTRL5()
{
    unsigned char ctrl5;

    //  Turn on hpf

    ctrl5 = 0x10;

#ifdef LSM9DS0_CACHE_MODE
    //  turn on fifo

    ctrl5 |= 0x40;
#endif

    return m_settings->HALWrite(m_gyroSlaveAddr,  LSM9DS0_GYRO_CTRL5, ctrl5, "Failed to set LSM9DS0 gyro CTRL5");
}


bool RTIMULSM9DS0::setAccelCTRL1()
{
    unsigned char ctrl1;

    if ((m_settings->m_LSM9DS0AccelSampleRate < 0) || (m_settings->m_LSM9DS0AccelSampleRate > 10)) {
        HAL_ERROR1("Illegal LSM9DS0 accel sample rate code %d\n", m_settings->m_LSM9DS0AccelSampleRate);
        return false;
    }

    ctrl1 = (m_settings->m_LSM9DS0AccelSampleRate << 4) | 0x07;

    return m_settings->HALWrite(m_accelCompassSlaveAddr,  LSM9DS0_CTRL1, ctrl1, "Failed to set LSM9DS0 accell CTRL1");
}

bool RTIMULSM9DS0::setAccelCTRL2()
{
    unsigned char ctrl2;

    if ((m_settings->m_LSM9DS0AccelLpf < 0) || (m_settings->m_LSM9DS0AccelLpf > 3)) {
        HAL_ERROR1("Illegal LSM9DS0 accel low pass fiter code %d\n", m_settings->m_LSM9DS0AccelLpf);
        return false;
    }

    switch (m_settings->m_LSM9DS0AccelFsr) {
    case LSM9DS0_ACCEL_FSR_2:
        m_accelScale = (RTFLOAT)0.000061;
        break;

    case LSM9DS0_ACCEL_FSR_4:
        m_accelScale = (RTFLOAT)0.000122;
        break;

    case LSM9DS0_ACCEL_FSR_6:
        m_accelScale = (RTFLOAT)0.000183;
        break;

    case LSM9DS0_ACCEL_FSR_8:
        m_accelScale = (RTFLOAT)0.000244;
        break;

    case LSM9DS0_ACCEL_FSR_16:
        m_accelScale = (RTFLOAT)0.000732;
        break;

    default:
        HAL_ERROR1("Illegal LSM9DS0 accel FSR code %d\n", m_settings->m_LSM9DS0AccelFsr);
        return false;
    }

    ctrl2 = (m_settings->m_LSM9DS0AccelLpf << 6) | (m_settings->m_LSM9DS0AccelFsr << 3);

    return m_settings->HALWrite(m_accelCompassSlaveAddr,  LSM9DS0_CTRL2, ctrl2, "Failed to set LSM9DS0 accel CTRL2");
}


bool RTIMULSM9DS0::setCompassCTRL5()
{
    unsigned char ctrl5;

    if ((m_settings->m_LSM9DS0CompassSampleRate < 0) || (m_settings->m_LSM9DS0CompassSampleRate > 5)) {
        HAL_ERROR1("Illegal LSM9DS0 compass sample rate code %d\n", m_settings->m_LSM9DS0CompassSampleRate);
        return false;
    }

    ctrl5 = (m_settings->m_LSM9DS0CompassSampleRate << 2);

#ifdef LSM9DS0_CACHE_MODE
    //  enable fifo

    ctrl5 |= 0x40;
#endif

    return m_settings->HALWrite(m_accelCompassSlaveAddr,  LSM9DS0_CTRL5, ctrl5, "Failed to set LSM9DS0 compass CTRL5");
}

bool RTIMULSM9DS0::setCompassCTRL6()
{
    unsigned char ctrl6;

    //  convert FSR to uT

    switch (m_settings->m_LSM9DS0CompassFsr) {
    case LSM9DS0_COMPASS_FSR_2:
        ctrl6 = 0;
        m_compassScale = (RTFLOAT)0.008;
        break;

    case LSM9DS0_COMPASS_FSR_4:
        ctrl6 = 0x20;
        m_compassScale = (RTFLOAT)0.016;
        break;

    case LSM9DS0_COMPASS_FSR_8:
        ctrl6 = 0x40;
        m_compassScale = (RTFLOAT)0.032;
        break;

    case LSM9DS0_COMPASS_FSR_12:
        ctrl6 = 0x60;
        m_compassScale = (RTFLOAT)0.0479;
        break;

    default:
        HAL_ERROR1("Illegal LSM9DS0 compass FSR code %d\n", m_settings->m_LSM9DS0CompassFsr);
        return false;
    }

    return m_settings->HALWrite(m_accelCompassSlaveAddr,  LSM9DS0_CTRL6, ctrl6, "Failed to set LSM9DS0 compass CTRL6");
}

bool RTIMULSM9DS0::setCompassCTRL7()
{
     return m_settings->HALWrite(m_accelCompassSlaveAddr,  LSM9DS0_CTRL7, 0x60, "Failed to set LSM9DS0CTRL7");
}



int RTIMULSM9DS0::IMUGetPollInterval()
{
    return (400 / m_sampleRate);
}

bool RTIMULSM9DS0::IMURead()
{
    unsigned char status;
    unsigned char gyroData[6];
    unsigned char accelData[6];
    unsigned char compassData[6];


#ifdef LSM9DS0_CACHE_MODE
    int count;

    if (!m_settings->HALRead(m_gyroSlaveAddr, LSM9DS0_GYRO_FIFO_SRC, 1, &status, "Failed to read LSM9DS0 gyro fifo status"))
        return false;

    if ((status & 0x40) != 0) {
        HAL_INFO("LSM9DS0 gyro fifo overrun\n");
        if (!m_settings->HALWrite(m_gyroSlaveAddr, LSM9DS0_GYRO_CTRL5, 0x10, "Failed to set LSM9DS0 gyro CTRL5"))
            return false;

        if (!m_settings->HALWrite(m_gyroSlaveAddr, LSM9DS0_GYRO_FIFO_CTRL, 0x0, "Failed to set LSM9DS0 gyro FIFO mode"))
            return false;

        if (!m_settings->HALWrite(m_gyroSlaveAddr, LSM9DS0_GYRO_FIFO_CTRL, 0x3f, "Failed to set LSM9DS0 gyro FIFO mode"))
            return false;

        if (!setGyroCTRL5())
            return false;

        m_imuData.timestamp += m_sampleInterval * 32;
        return false;
    }

    // get count of samples in fifo
    count = status & 0x1f;

    if ((m_cacheCount == 0) && (count > 0) && (count < LSM9DS0_FIFO_THRESH)) {
        // special case of a small fifo and nothing cached - just handle as simple read

        if (!m_settings->HALRead(m_gyroSlaveAddr, 0x80 | LSM9DS0_GYRO_OUT_X_L, 6, gyroData, "Failed to read LSM9DS0 gyro data"))
            return false;

        if (!m_settings->HALRead(m_accelCompassSlaveAddr, 0x80 | LSM9DS0_OUT_X_L_A, 6, accelData, "Failed to read LSM9DS0 accel data"))
            return false;

        if (!m_settings->HALRead(m_accelCompassSlaveAddr, 0x80 | LSM9DS0_OUT_X_L_M, 6, compassData, "Failed to read LSM9DS0 compass data"))
            return false;

        if (m_firstTime)
            m_imuData.timestamp = RTMath::currentUSecsSinceEpoch();
        else
            m_imuData.timestamp += m_sampleInterval;

        m_firstTime = false;
   } else {
        if (count >=  LSM9DS0_FIFO_THRESH) {
            // need to create a cache block

            if (m_cacheCount == LSM9DS0_CACHE_BLOCK_COUNT) {
                // all cache blocks are full - discard oldest and update timestamp to account for lost samples
                m_imuData.timestamp += m_sampleInterval * m_cache[m_cacheOut].count;
                if (++m_cacheOut == LSM9DS0_CACHE_BLOCK_COUNT)
                    m_cacheOut = 0;
                m_cacheCount--;
            }

            if (!m_settings->HALRead(m_gyroSlaveAddr, 0x80 | LSM9DS0_GYRO_OUT_X_L, LSM9DS0_FIFO_CHUNK_SIZE * LSM9DS0_FIFO_THRESH,
                         m_cache[m_cacheIn].data, "Failed to read LSM9DS0 fifo data"))
                return false;

            if (!m_settings->HALRead(m_accelCompassSlaveAddr, 0x80 | LSM9DS0_OUT_X_L_A, 6,
                         m_cache[m_cacheIn].accel, "Failed to read LSM9DS0 accel data"))
                return false;

            if (!m_settings->HALRead(m_accelCompassSlaveAddr, 0x80 | LSM9DS0_OUT_X_L_M, 6,
                         m_cache[m_cacheIn].compass, "Failed to read LSM9DS0 compass data"))
                return false;

            m_cache[m_cacheIn].count = LSM9DS0_FIFO_THRESH;
            m_cache[m_cacheIn].index = 0;

            m_cacheCount++;
            if (++m_cacheIn == LSM9DS0_CACHE_BLOCK_COUNT)
                m_cacheIn = 0;

        }

        //  now fifo has been read if necessary, get something to process

        if (m_cacheCount == 0)
            return false;

        memcpy(gyroData, m_cache[m_cacheOut].data + m_cache[m_cacheOut].index, LSM9DS0_FIFO_CHUNK_SIZE);
        memcpy(accelData, m_cache[m_cacheOut].accel, 6);
        memcpy(compassData, m_cache[m_cacheOut].compass, 6);

        m_cache[m_cacheOut].index += LSM9DS0_FIFO_CHUNK_SIZE;

        if (--m_cache[m_cacheOut].count == 0) {
            //  this cache block is now empty

            if (++m_cacheOut == LSM9DS0_CACHE_BLOCK_COUNT)
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
    if (!m_settings->HALRead(m_gyroSlaveAddr, LSM9DS0_GYRO_STATUS, 1, &status, "Failed to read LSM9DS0 status"))
        return false;

    if ((status & 0x8) == 0)
        return false;

    if (!m_settings->HALRead(m_gyroSlaveAddr, 0x80 | LSM9DS0_GYRO_OUT_X_L, 6, gyroData, "Failed to read LSM9DS0 gyro data"))
        return false;

    m_imuData.timestamp = RTMath::currentUSecsSinceEpoch();

    if (!m_settings->HALRead(m_accelCompassSlaveAddr, 0x80 | LSM9DS0_OUT_X_L_A, 6, accelData, "Failed to read LSM9DS0 accel data"))
        return false;

    if (!m_settings->HALRead(m_accelCompassSlaveAddr, 0x80 | LSM9DS0_OUT_X_L_M, 6, compassData, "Failed to read LSM9DS0 compass data"))
        return false;

#endif

    RTMath::convertToVector(gyroData, m_imuData.gyro, m_gyroScale, false);
    RTMath::convertToVector(accelData, m_imuData.accel, m_accelScale, false);
    RTMath::convertToVector(compassData, m_imuData.compass, m_compassScale, false);

    //  sort out gyro axes and correct for bias

    m_imuData.gyro.setX(m_imuData.gyro.x());
    m_imuData.gyro.setY(-m_imuData.gyro.y());
    m_imuData.gyro.setZ(-m_imuData.gyro.z());

    //  sort out accel data;

    m_imuData.accel.setX(-m_imuData.accel.x());

    //  sort out compass axes

    m_imuData.compass.setY(-m_imuData.compass.y());

    //  now do standard processing

    handleGyroBias();
    calibrateAverageCompass();
    calibrateAccel();

    //  now update the filter

    updateFusion();

    return true;
}
