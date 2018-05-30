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

//  Some code from Bosch BMM150 API driver:

/*
****************************************************************************
* Copyright (C) 2011 - 2014 Bosch Sensortec GmbH
*
* bmm050.c
* Date: 2014/12/12
* Revision: 2.0.3 $
*
* Usage: Sensor Driver for  BMM050 and BMM150 sensor
*
****************************************************************************
* License:
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*   Redistributions of source code must retain the above copyright
*   notice, this list of conditions and the following disclaimer.
*
*   Redistributions in binary form must reproduce the above copyright
*   notice, this list of conditions and the following disclaimer in the
*   documentation and/or other materials provided with the distribution.
*
*   Neither the name of the copyright holder nor the names of the
*   contributors may be used to endorse or promote products derived from
*   this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER
* OR CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
* OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
* ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
*
* The information provided is believed to be accurate and reliable.
* The copyright holder assumes no responsibility
* for the consequences of use
* of such information nor for any infringement of patents or
* other rights of third parties which may result from its use.
* No license is granted by implication or otherwise under any patent or
* patent rights of the copyright holder.
**************************************************************************/
/****************************************************************************/

#include "RTIMUBMX055.h"
#include "RTIMUSettings.h"

//  this sets the learning rate for compass running average calculation

#define COMPASS_ALPHA 0.2f

RTIMUBMX055::RTIMUBMX055(RTIMUSettings *settings) : RTIMU(settings)
{
    m_sampleRate = 100;
    m_sampleInterval = (uint64_t)1000000 / m_sampleRate;
}

RTIMUBMX055::~RTIMUBMX055()
{
}

bool RTIMUBMX055::IMUInit()
{
    unsigned char result;

    m_firstTime = true;

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

    if (!m_settings->HALRead(m_gyroSlaveAddr, BMX055_GYRO_WHO_AM_I, 1, &result, "Failed to read BMX055 gyro id"))
        return false;

    if (result !=  BMX055_GYRO_ID) {
        HAL_ERROR1("Incorrect BMX055 id %d\n", result);
        return false;
    }

    // work out accel address

    if (m_settings->HALRead(BMX055_ACCEL_ADDRESS0, BMX055_ACCEL_WHO_AM_I, 1, &result, "")) {
        if (result == BMX055_ACCEL_ID) {
            m_accelSlaveAddr = BMX055_ACCEL_ADDRESS0;
        } else {
            m_accelSlaveAddr = BMX055_ACCEL_ADDRESS1;
        }
    }

    // work out mag address

    int magAddr;

    for (magAddr = BMX055_MAG_ADDRESS0; magAddr <= BMX055_MAG_ADDRESS3; magAddr++) {

        // have to enable chip to get id...

        m_settings->HALWrite(magAddr, BMX055_MAG_POWER, 1, "");

        m_settings->delayMs(50);

        if (m_settings->HALRead(magAddr, BMX055_MAG_WHO_AM_I, 1, &result, "")) {
            if (result == BMX055_MAG_ID) {
                m_magSlaveAddr = magAddr;
                break;
            }
        }
    }

    if (magAddr > BMX055_MAG_ADDRESS3) {
        HAL_ERROR("Failed to find BMX055 mag\n");
        return false;
    }

    setCalibrationData();

    //  enable the I2C bus

    if (!m_settings->HALOpen())
        return false;

    //  Set up the gyro

    if (!m_settings->HALWrite(m_gyroSlaveAddr, BMX055_GYRO_FIFO_CONFIG_1, 0x40, "Failed to set BMX055 FIFO config"))
        return false;

    if (!setGyroSampleRate())
            return false;

    if (!setGyroFSR())
            return false;

    gyroBiasInit();

    //  set up the accel

    if (!setAccelSampleRate())
            return false;

    if (!setAccelFSR())
            return false;


    //  set up the mag

    magInitTrimRegisters();
    setMagPreset();

    HAL_INFO("BMX055 init complete\n");
    return true;
}

bool RTIMUBMX055::setGyroSampleRate()
{
    unsigned char bw;

    switch (m_settings->m_BMX055GyroSampleRate) {
    case BMX055_GYRO_SAMPLERATE_2000_523:
        bw = 0;
        m_sampleRate = 2000;
        break;

    case BMX055_GYRO_SAMPLERATE_2000_230:
        bw = 1;
        m_sampleRate = 2000;
        break;

    case BMX055_GYRO_SAMPLERATE_1000_116:
        bw = 2;
        m_sampleRate = 1000;
        break;

    case BMX055_GYRO_SAMPLERATE_400_47:
        bw = 3;
        m_sampleRate = 400;
        break;

    case BMX055_GYRO_SAMPLERATE_200_23:
        bw = 4;
        m_sampleRate = 200;
        break;

    case BMX055_GYRO_SAMPLERATE_100_12:
        bw = 5;
        m_sampleRate = 100;
        break;

    case BMX055_GYRO_SAMPLERATE_200_64:
        bw = 6;
        m_sampleRate = 200;
        break;

    case BMX055_GYRO_SAMPLERATE_100_32:
        bw = 7;
        m_sampleRate = 100;
        break;

    default:
        HAL_ERROR1("Illegal BMX055 gyro sample rate code %d\n", m_settings->m_BMX055GyroSampleRate);
        return false;
    }

    m_sampleInterval = (uint64_t)1000000 / m_sampleRate;
    return (m_settings->HALWrite(m_gyroSlaveAddr, BMX055_GYRO_BW, bw, "Failed to set BMX055 gyro rate"));
}

bool RTIMUBMX055::setGyroFSR()
{
    switch(m_settings->m_BMX055GyroFsr) {
    case BMX055_GYRO_FSR_2000:
        m_gyroScale = 0.061 * RTMATH_DEGREE_TO_RAD;
        break;

    case BMX055_GYRO_FSR_1000:
        m_gyroScale = 0.0305 * RTMATH_DEGREE_TO_RAD;
        break;

    case BMX055_GYRO_FSR_500:
        m_gyroScale = 0.0153 * RTMATH_DEGREE_TO_RAD;
        break;

    case BMX055_GYRO_FSR_250:
        m_gyroScale = 0.0076 * RTMATH_DEGREE_TO_RAD;
        break;

    case BMX055_GYRO_FSR_125:
        m_gyroScale = 0.0038 * RTMATH_DEGREE_TO_RAD;
        break;

    default:
        HAL_ERROR1("Illegal BMX055 gyro FSR code %d\n", m_settings->m_BMX055GyroFsr);
        return false;

    }
    return (m_settings->HALWrite(m_gyroSlaveAddr, BMX055_GYRO_RANGE, m_settings->m_BMX055GyroFsr, "Failed to set BMX055 gyro rate"));
}


bool RTIMUBMX055::setAccelSampleRate()
{
    unsigned char reg;

    switch(m_settings->m_BMX055AccelSampleRate) {
    case BMX055_ACCEL_SAMPLERATE_15:
        reg = 0x08;
        break;

    case BMX055_ACCEL_SAMPLERATE_31:
        reg = 0x09;
        break;

    case BMX055_ACCEL_SAMPLERATE_62:
        reg = 0x0a;
        break;

    case BMX055_ACCEL_SAMPLERATE_125:
        reg = 0x0b;
        break;

    case BMX055_ACCEL_SAMPLERATE_250:
        reg = 0x0c;
        break;

    case BMX055_ACCEL_SAMPLERATE_500:
        reg = 0x0d;
        break;

    case BMX055_ACCEL_SAMPLERATE_1000:
        reg = 0x0e;
        break;

    case BMX055_ACCEL_SAMPLERATE_2000:
        reg = 0x0f;
        break;

    default:
        HAL_ERROR1("Illegal BMX055 accel FSR code %d\n", m_settings->m_BMX055AccelSampleRate);
        return false;
    }
    return (m_settings->HALWrite(m_accelSlaveAddr, BMX055_ACCEL_PMU_BW, reg, "Failed to set BMX055 accel rate"));
}

bool RTIMUBMX055::setAccelFSR()
{
    unsigned char reg;

    switch(m_settings->m_BMX055AccelFsr) {
    case BMX055_ACCEL_FSR_2:
        reg = 0x03;
        m_accelScale = 0.00098 / 16.0;
        break;

    case BMX055_ACCEL_FSR_4:
        reg = 0x05;
        m_accelScale = 0.00195 / 16.0;
        break;

    case BMX055_ACCEL_FSR_8:
        reg = 0x08;
        m_accelScale = 0.00391 / 16.0;
        break;

    case BMX055_ACCEL_FSR_16:
        reg = 0x0c;
        m_accelScale = 0.00781 / 16.0;
        break;

    default:
        HAL_ERROR1("Illegal BMX055 accel FSR code %d\n", m_settings->m_BMX055AccelFsr);
        return false;
    }
    return (m_settings->HALWrite(m_accelSlaveAddr, BMX055_ACCEL_PMU_RANGE, reg, "Failed to set BMX055 accel rate"));
}

bool RTIMUBMX055::setMagPreset()
{
    unsigned char mode, repXY, repZ;

    switch (m_settings->m_BMX055MagPreset) {
    case BMX055_MAG_LOW_POWER:                              // ODR=10, RepXY=3, RepZ=3
        mode = 0;
        repXY = 1;
        repZ = 2;
        break;

    case BMX055_MAG_REGULAR:                                // ODR=10, RepXY=9, RepZ=15
        mode = 0;
        repXY = 4;
        repZ = 14;
        break;

    case BMX055_MAG_ENHANCED:                               // ODR=10, RepXY=15, RepZ=27
        mode = 0;
        repXY = 7;
        repZ = 26;
        break;

    case BMX055_MAG_HIGH_ACCURACY:                          // ODR=10, RepXY=47, RepZ=83
        mode = 0;
        repXY = 23;
        repZ = 82;
        break;

    default:
        HAL_ERROR1("Illegal BMX055 mag preset code %d\n", m_settings->m_BMX055MagPreset);
        return false;
    }

    if (!m_settings->HALWrite(m_magSlaveAddr, BMX055_MAG_MODE, mode, "Failed to set BMX055 mag mode"))
        return false;
    if (!m_settings->HALWrite(m_magSlaveAddr, BMX055_MAG_REPXY, repXY, "Failed to set BMX055 mag repXY"))
        return false;
    if (!m_settings->HALWrite(m_magSlaveAddr, BMX055_MAG_REPZ, repZ, "Failed to set BMX055 mag repZ"))
        return false;
    return true;
}

int RTIMUBMX055::IMUGetPollInterval()
{
    if (m_sampleRate > 400)
        return 1;
    else
        return (400 / m_sampleRate);
}

bool RTIMUBMX055::IMURead()
{
    unsigned char status;
    unsigned char gyroData[6];
    unsigned char accelData[6];
    unsigned char magData[8];

    if (!m_settings->HALRead(m_gyroSlaveAddr, BMX055_GYRO_FIFO_STATUS, 1, &status, "Failed to read BMX055 gyro fifo status"))
        return false;

    if (status & 0x80) {
        // fifo overflowed
        HAL_ERROR("BMX055 fifo overflow\n");

        // this should clear it
        if (!m_settings->HALWrite(m_gyroSlaveAddr, BMX055_GYRO_FIFO_CONFIG_1, 0x40, "Failed to set BMX055 FIFO config"))
            return false;

        m_imuData.timestamp = RTMath::currentUSecsSinceEpoch();  // try to fix timestamp
        return false;
    }

    if (status == 0)
        return false;

    if (!m_settings->HALRead(m_gyroSlaveAddr, BMX055_GYRO_FIFO_DATA, 6, gyroData, "Failed to read BMX055 gyro data"))
            return false;

    if (!m_settings->HALRead(m_accelSlaveAddr, BMX055_ACCEL_X_LSB, 6, accelData, "Failed to read BMX055 accel data"))
        return false;

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_X_LSB, 8, magData, "Failed to read BMX055 mag data"))
        return false;

    RTMath::convertToVector(gyroData, m_imuData.gyro, m_gyroScale, false);

    //  need to prepare accel data

    accelData[0] &= 0xf0;
    accelData[2] &= 0xf0;
    accelData[4] &= 0xf0;

    RTMath::convertToVector(accelData, m_imuData.accel, m_accelScale, false);

    float mx, my, mz;

    processMagData(magData, mx, my, mz);
    m_imuData.compass.setX(mx);
    m_imuData.compass.setY(my);
    m_imuData.compass.setZ(mz);

    //  sort out gyro axes

    m_imuData.gyro.setY(-m_imuData.gyro.y());
    m_imuData.gyro.setZ(-m_imuData.gyro.z());

    //  sort out accel axes

    m_imuData.accel.setX(-m_imuData.accel.x());

    //  sort out mag axes

#ifdef BMX055_REMAP
    m_imuData.compass.setY(-m_imuData.compass.y());
    m_imuData.compass.setZ(-m_imuData.compass.z());
#else
    RTFLOAT temp =  m_imuData.compass.x();
    m_imuData.compass.setX(-m_imuData.compass.y());
    m_imuData.compass.setY(-temp);
    m_imuData.compass.setZ(-m_imuData.compass.z());
#endif
    //  now do standard processing

    handleGyroBias();
    calibrateAverageCompass();
    calibrateAccel();

    if (m_firstTime)
        m_imuData.timestamp = RTMath::currentUSecsSinceEpoch();
    else
        m_imuData.timestamp += m_sampleInterval;

    m_firstTime = false;

    //  now update the filter

    updateFusion();

    return true;
}

/*!
 *	@brief This API reads remapped compensated Magnetometer
 *	data of X,Y,Z values
 *	@note The output value of compensated X, Y, Z as float
 *
 *	@note In this function X and Y axis is remapped
 *	@note X is read from the address 0x44 & 0x45
 *	@note Y is read from the address 0x42 & 0x43
 *	@note this API is only applicable for BMX055 sensor
 * *
 *
*/

/****************************************************/
/**\name	ARRAY PARAMETERS      */
/***************************************************/
#define LSB_ZERO	0
#define MSB_ONE		1
#define LSB_TWO		2
#define MSB_THREE	3
#define LSB_FOUR	4
#define MSB_FIVE	5
#define LSB_SIX		6
#define MSB_SEVEN	7

/********************************************/
/**\name BIT MASK, LENGTH AND POSITION OF REMAPPED DATA REGISTERS   */
/********************************************/
/* Data X LSB Remapped Register only applicable for BMX055 */
#define BMM050_BMX055_REMAPPED_DATAX_LSB_VALUEX__POS        3
#define BMM050_BMX055_REMAPPED_DATAX_LSB_VALUEX__LEN        5
#define BMM050_BMX055_REMAPPED_DATAX_LSB_VALUEX__MSK        0xF8
#define BMM050_BMX055_REMAPPED_DATAX_LSB_VALUEX__REG\
BMM050_BMX055_REMAPPED_DATAX_LSB

/* Data Y LSB Remapped Register only applicable for BMX055  */
#define BMM050_BMX055_REMAPPED_DATAY_LSB_VALUEY__POS        3
#define BMM050_BMX055_REMAPPED_DATAY_LSB_VALUEY__LEN        5
#define BMM050_BMX055_REMAPPED_DATAY_LSB_VALUEY__MSK        0xF8
#define BMM050_BMX055_REMAPPED_DATAY_LSB_VALUEY__REG\
BMM050_BMX055_REMAPPED_DATAY_LSB

/********************************************/
/**\name BIT MASK, LENGTH AND POSITION OF DATA REGISTERS   */
/********************************************/
/* Data X LSB Register */
#define BMM050_DATAX_LSB_VALUEX__POS        3
#define BMM050_DATAX_LSB_VALUEX__LEN        5
#define BMM050_DATAX_LSB_VALUEX__MSK        0xF8
#define BMM050_DATAX_LSB_VALUEX__REG        BMM050_DATAX_LSB

/* Data X SelfTest Register */
#define BMM050_DATAX_LSB_TESTX__POS         0
#define BMM050_DATAX_LSB_TESTX__LEN         1
#define BMM050_DATAX_LSB_TESTX__MSK         0x01
#define BMM050_DATAX_LSB_TESTX__REG         BMM050_DATAX_LSB

/* Data Y LSB Register */
#define BMM050_DATAY_LSB_VALUEY__POS        3
#define BMM050_DATAY_LSB_VALUEY__LEN        5
#define BMM050_DATAY_LSB_VALUEY__MSK        0xF8
#define BMM050_DATAY_LSB_VALUEY__REG        BMM050_DATAY_LSB

/* Data Y SelfTest Register */
#define BMM050_DATAY_LSB_TESTY__POS         0
#define BMM050_DATAY_LSB_TESTY__LEN         1
#define BMM050_DATAY_LSB_TESTY__MSK         0x01
#define BMM050_DATAY_LSB_TESTY__REG         BMM050_DATAY_LSB

/* Data Z LSB Register */
#define BMM050_DATAZ_LSB_VALUEZ__POS        1
#define BMM050_DATAZ_LSB_VALUEZ__LEN        7
#define BMM050_DATAZ_LSB_VALUEZ__MSK        0xFE
#define BMM050_DATAZ_LSB_VALUEZ__REG        BMM050_DATAZ_LSB

/* Data Z SelfTest Register */
#define BMM050_DATAZ_LSB_TESTZ__POS         0
#define BMM050_DATAZ_LSB_TESTZ__LEN         1
#define BMM050_DATAZ_LSB_TESTZ__MSK         0x01
#define BMM050_DATAZ_LSB_TESTZ__REG         BMM050_DATAZ_LSB

/* Hall Resistance LSB Register */
#define BMM050_R_LSB_VALUE__POS             2
#define BMM050_R_LSB_VALUE__LEN             6
#define BMM050_R_LSB_VALUE__MSK             0xFC
#define BMM050_R_LSB_VALUE__REG             BMM050_R_LSB

#define BMM050_DATA_RDYSTAT__POS            0
#define BMM050_DATA_RDYSTAT__LEN            1
#define BMM050_DATA_RDYSTAT__MSK            0x01
#define BMM050_DATA_RDYSTAT__REG            BMM050_R_LSB

/********************************************/
/**\name GET AND SET BITSLICE FUNCTIONS  */
/********************************************/
/* get bit slice  */
#define BMM050_GET_BITSLICE(regvar, bitname)\
    ((regvar & bitname##__MSK) >> bitname##__POS)

/* Set bit slice */
#define BMM050_SET_BITSLICE(regvar, bitname, val)\
    ((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

/********************************************/
/**\name BIT SHIFTING DEFINITIONS  */
/********************************************/
/*Shifting Constants*/
#define SHIFT_RIGHT_1_POSITION                  1
#define SHIFT_RIGHT_2_POSITION                  2
#define SHIFT_RIGHT_3_POSITION                  3
#define SHIFT_RIGHT_4_POSITION                  4
#define SHIFT_RIGHT_5_POSITION                  5
#define SHIFT_RIGHT_6_POSITION                  6
#define SHIFT_RIGHT_7_POSITION                  7
#define SHIFT_RIGHT_8_POSITION                  8
#define SHIFT_RIGHT_9_POSITION                  9
#define SHIFT_RIGHT_12_POSITION                 12
#define SHIFT_RIGHT_13_POSITION                 13
#define SHIFT_RIGHT_16_POSITION                 16

#define SHIFT_LEFT_1_POSITION                   1
#define SHIFT_LEFT_2_POSITION                   2
#define SHIFT_LEFT_3_POSITION                   3
#define SHIFT_LEFT_4_POSITION                   4
#define SHIFT_LEFT_5_POSITION                   5
#define SHIFT_LEFT_6_POSITION                   6
#define SHIFT_LEFT_7_POSITION                   7
#define SHIFT_LEFT_8_POSITION                   8
#define SHIFT_LEFT_14_POSITION                  14
#define SHIFT_LEFT_15_POSITION                  15
#define SHIFT_LEFT_16_POSITION                  16

/****************************************************/
/**\name	COMPENSATED FORMULA DEFINITIONS      */
/***************************************************/
#define BMM050_HEX_FOUR_THOUSAND			0x4000
#define BMM050_HEX_ONE_LACK					0x100000
#define BMM050_HEX_A_ZERO					0xA0

#define	BMM050_FLOAT_ONE_SIX_THREE_EIGHT_FOUR		16384.0f
#define	BMM050_FLOAT_2_6_8_4_3_5_4_5_6_DATA			268435456.0f
#define	BMM050_FLOAT_1_6_3_8_4_DATA					16384.0f
#define	BMM050_FLOAT_2_5_6_DATA						256.0f
#define	BMM050_FLOAT_1_6_0_DATA						160.0f
#define	BMM050_FLOAT_8_1_9_2_DATA					8192.0f
#define	BMM050_FLOAT_EIGHT_DATA						8.0f
#define	BMM050_FLOAT_SIXTEEN_DATA					16.0f
#define	BMM050_FLOAT_1_3_1_0_7_2_DATA				131072.0f
#define	BMM050_FLOAT_3_2_7_6_8_DATA					32768.0
#define	BMM050_FLOAT_4_DATA                         4.0

/********************************************/
/**\name ENABLE/DISABLE DEFINITIONS  */
/********************************************/
#define BMM050_ZERO_U8X                         0
#define BMM050_DISABLE                          0
#define BMM050_ENABLE                           1
#define BMM050_CHANNEL_DISABLE                  1
#define BMM050_CHANNEL_ENABLE                   0
#define BMM050_INTPIN_LATCH_ENABLE              1
#define BMM050_INTPIN_LATCH_DISABLE             0
#define BMM050_OFF                              0
#define BMM050_ON                               1

/********************************************/
/**\name OVERFLOW DEFINITIONS  */
/********************************************/
/* compensated output value returned if sensor had overflow */
#define BMM050_OVERFLOW_OUTPUT			-32768
#define BMM050_OVERFLOW_OUTPUT_S32		((int32_t)(-2147483647-1))
#define BMM050_OVERFLOW_OUTPUT_FLOAT	0.0f
#define BMM050_FLIP_OVERFLOW_ADCVAL		-4096
#define BMM050_HALL_OVERFLOW_ADCVAL		-16384

/********************************************/
/**\name NUMERIC DEFINITIONS  */
/********************************************/
#define         C_BMM050_ZERO_U8X				((uint8_t)0)
#define         C_BMM050_ONE_U8X				((uint8_t)1)
#define         C_BMM050_TWO_U8X				((uint8_t)2)
#define         C_BMM050_FOUR_U8X				((uint8_t)4)
#define         C_BMM050_FIVE_U8X				((uint8_t)5)
#define         C_BMM050_EIGHT_U8X				((uint8_t)8)

#define BMM0505_HEX_ZERO_ZERO	0x00
/* Conversion factors*/
#define BMM050_CONVFACTOR_LSB_UT                6

/********************************************/
/**\name BIT MASK, LENGTH AND POSITION OF TRIM REGISTER   */
/********************************************/
/* Register 6D */
#define BMM050_DIG_XYZ1_MSB__POS         0
#define BMM050_DIG_XYZ1_MSB__LEN         7
#define BMM050_DIG_XYZ1_MSB__MSK         0x7F
#define BMM050_DIG_XYZ1_MSB__REG         BMM050_DIG_XYZ1_MSB

#ifdef BMX055_REMAP
void RTIMUBMX055::processMagData(unsigned char *v_data_uint8_t, float& magX, float& magY, float& magZ)
{
    /* structure used to store the mag raw xyz and r data */
    struct {
        int16_t raw_data_x;
        int16_t raw_data_y;
        int16_t raw_data_z;
        uint16_t raw_data_r;
    } raw_data_xyz_t;

    /* Reading data for Y axis */
    v_data_uint8_t[LSB_ZERO] = BMM050_GET_BITSLICE(v_data_uint8_t[LSB_ZERO],
    BMM050_BMX055_REMAPPED_DATAY_LSB_VALUEY);
    raw_data_xyz_t.raw_data_y = (int16_t)((((int32_t)
    ((int8_t)v_data_uint8_t[MSB_ONE])) <<
    SHIFT_LEFT_5_POSITION) | v_data_uint8_t[LSB_ZERO]);

    /* Reading data for X axis */
    v_data_uint8_t[LSB_TWO] = BMM050_GET_BITSLICE(v_data_uint8_t[LSB_TWO],
    BMM050_BMX055_REMAPPED_DATAX_LSB_VALUEX);
    raw_data_xyz_t.raw_data_x = (int16_t)((((int32_t)
    ((int8_t)v_data_uint8_t[MSB_THREE])) <<
    SHIFT_LEFT_5_POSITION) | v_data_uint8_t[LSB_TWO]);
    raw_data_xyz_t.raw_data_x = -raw_data_xyz_t.raw_data_x;

    /* Reading data for Z axis */
    v_data_uint8_t[LSB_FOUR] = BMM050_GET_BITSLICE(v_data_uint8_t[LSB_FOUR],
    BMM050_DATAZ_LSB_VALUEZ);
    raw_data_xyz_t.raw_data_z = (int16_t)((((int32_t)
    ((int8_t)v_data_uint8_t[MSB_FIVE])) <<
    SHIFT_LEFT_7_POSITION) | v_data_uint8_t[LSB_FOUR]);

    /* Reading data for Resistance*/
    v_data_uint8_t[LSB_SIX] = BMM050_GET_BITSLICE(v_data_uint8_t[LSB_SIX],
    BMM050_R_LSB_VALUE);
    raw_data_xyz_t.raw_data_r = (uint16_t)((((uint32_t)
    v_data_uint8_t[MSB_SEVEN]) <<
    SHIFT_LEFT_6_POSITION) | v_data_uint8_t[LSB_SIX]);

    /* Compensation for X axis */
    magX = bmm050_compensate_X_float(
    raw_data_xyz_t.raw_data_x,
    raw_data_xyz_t.raw_data_r);

    /* Compensation for Y axis */
    magY = bmm050_compensate_Y_float(
    raw_data_xyz_t.raw_data_y,
    raw_data_xyz_t.raw_data_r);

    /* Compensation for Z axis */
    magZ = bmm050_compensate_Z_float(
    raw_data_xyz_t.raw_data_z,
    raw_data_xyz_t.raw_data_r);

    /* Output raw resistance value */
//    mag_data->resistance = raw_data_xyz_t.raw_data_r;

}
#else
void RTIMUBMX055::processMagData(unsigned char *v_data_u8, float& magX, float& magY, float& magZ)
{
    /* structure used to store the mag raw xyz and r data */
    struct {
        int16_t raw_data_x;
        int16_t raw_data_y;
        int16_t raw_data_z;
        int16_t raw_data_r;
    } raw_data_xyz_t;

    /* Reading data for X axis */
    v_data_u8[LSB_ZERO] = BMM050_GET_BITSLICE(v_data_u8[LSB_ZERO],
    BMM050_DATAX_LSB_VALUEX);
    raw_data_xyz_t.raw_data_x = (int16_t)((((int32_t)
    ((int8_t)v_data_u8[MSB_ONE])) <<
    SHIFT_LEFT_5_POSITION) | v_data_u8[LSB_ZERO]);

    /* Reading data for Y axis */
    v_data_u8[LSB_TWO] = BMM050_GET_BITSLICE(v_data_u8[LSB_TWO],
    BMM050_DATAY_LSB_VALUEY);
    raw_data_xyz_t.raw_data_y = (int16_t)((((int32_t)
    ((int8_t)v_data_u8[MSB_THREE])) <<
    SHIFT_LEFT_5_POSITION) | v_data_u8[LSB_TWO]);

    /* Reading data for Z axis */
    v_data_u8[LSB_FOUR] = BMM050_GET_BITSLICE(v_data_u8[LSB_FOUR],
    BMM050_DATAZ_LSB_VALUEZ);
    raw_data_xyz_t.raw_data_z = (int16_t)((((int32_t)
    ((int8_t)v_data_u8[MSB_FIVE])) <<
    SHIFT_LEFT_7_POSITION) | v_data_u8[LSB_FOUR]);

    /* Reading data for Resistance*/
    v_data_u8[LSB_SIX] = BMM050_GET_BITSLICE(v_data_u8[LSB_SIX],
    BMM050_R_LSB_VALUE);
    raw_data_xyz_t.raw_data_r = (uint16_t)((((uint32_t)
    v_data_u8[MSB_SEVEN]) <<
    SHIFT_LEFT_6_POSITION) | v_data_u8[LSB_SIX]);

    /* Compensation for X axis */
    magX = bmm050_compensate_X_float(
    raw_data_xyz_t.raw_data_x,
    raw_data_xyz_t.raw_data_r);

    /* Compensation for Y axis */
    magY = bmm050_compensate_Y_float(
    raw_data_xyz_t.raw_data_y,
    raw_data_xyz_t.raw_data_r);

    /* Compensation for Z axis */
    magZ = bmm050_compensate_Z_float(
    raw_data_xyz_t.raw_data_z,
    raw_data_xyz_t.raw_data_r);

    /* Output raw resistance value */
//    mag_data->resistance = raw_data_xyz_t.raw_data_r;

}
#endif

float RTIMUBMX055::bmm050_compensate_X_float(int16_t mag_data_x, uint16_t data_r)
{
    float inter_retval = BMM050_ZERO_U8X;
    if (mag_data_x != BMM050_FLIP_OVERFLOW_ADCVAL	/* no overflow */
       ) {
        if (data_r != C_BMM050_ZERO_U8X) {
            inter_retval = ((((float)m_dig_xyz1)
            * BMM050_FLOAT_ONE_SIX_THREE_EIGHT_FOUR
                /data_r)
                - BMM050_FLOAT_ONE_SIX_THREE_EIGHT_FOUR);
        } else {
            inter_retval = BMM050_OVERFLOW_OUTPUT_FLOAT;
            return inter_retval;
        }
        inter_retval = (((mag_data_x * ((((((float)m_dig_xy2) *
            (inter_retval*inter_retval /
            BMM050_FLOAT_2_6_8_4_3_5_4_5_6_DATA) +
            inter_retval * ((float)m_dig_xy1)
            / BMM050_FLOAT_1_6_3_8_4_DATA))
            + BMM050_FLOAT_2_5_6_DATA) *
            (((float)m_dig_x2) + BMM050_FLOAT_1_6_0_DATA)))
            / BMM050_FLOAT_8_1_9_2_DATA)
            + (((float)m_dig_x1) *
            BMM050_FLOAT_EIGHT_DATA))/
            BMM050_FLOAT_SIXTEEN_DATA;
    } else {
        inter_retval = BMM050_OVERFLOW_OUTPUT_FLOAT;
    }
    return inter_retval;
}

float RTIMUBMX055::bmm050_compensate_Y_float(int16_t mag_data_y, uint16_t data_r)
{
    float inter_retval = BMM050_ZERO_U8X;
    if (mag_data_y != BMM050_FLIP_OVERFLOW_ADCVAL /* no overflow */
       ) {
        if (data_r != C_BMM050_ZERO_U8X) {
            inter_retval = ((((float)m_dig_xyz1)
            * BMM050_FLOAT_ONE_SIX_THREE_EIGHT_FOUR
            /data_r) - BMM050_FLOAT_ONE_SIX_THREE_EIGHT_FOUR);
        } else {
            inter_retval = BMM050_OVERFLOW_OUTPUT_FLOAT;
            return inter_retval;
        }
        inter_retval = (((mag_data_y * ((((((float)m_dig_xy2) *
            (inter_retval*inter_retval
            / BMM050_FLOAT_2_6_8_4_3_5_4_5_6_DATA) +
            inter_retval * ((float)m_dig_xy1)
            / BMM050_FLOAT_1_6_3_8_4_DATA)) +
            BMM050_FLOAT_2_5_6_DATA) *
            (((float)m_dig_y2) + BMM050_FLOAT_1_6_0_DATA)))
            / BMM050_FLOAT_8_1_9_2_DATA) +
            (((float)m_dig_y1) * BMM050_FLOAT_EIGHT_DATA))
            / BMM050_FLOAT_SIXTEEN_DATA;
    } else {
        /* overflow, set output to 0.0f */
        inter_retval = BMM050_OVERFLOW_OUTPUT_FLOAT;
    }
    return inter_retval;
}

float RTIMUBMX055::bmm050_compensate_Z_float (int16_t mag_data_z, uint16_t data_r)
{
    float inter_retval = BMM050_ZERO_U8X;
     /* no overflow */
    if (mag_data_z != BMM050_HALL_OVERFLOW_ADCVAL) {
        if ((m_dig_z2 != BMM050_ZERO_U8X)
        && (m_dig_z1 != BMM050_ZERO_U8X)
        && (data_r != BMM050_ZERO_U8X)) {
            inter_retval = ((((((float)mag_data_z)-
            ((float)m_dig_z4))*
            BMM050_FLOAT_1_3_1_0_7_2_DATA)-
            (((float)m_dig_z3)*(((float)data_r)
            -((float)m_dig_xyz1))))
            /((((float)m_dig_z2)+
            ((float)m_dig_z1)*((float)data_r) /
            BMM050_FLOAT_3_2_7_6_8_DATA)
            * BMM050_FLOAT_4_DATA))
            / BMM050_FLOAT_SIXTEEN_DATA;
        }
    } else {
        /* overflow, set output to 0.0f */
        inter_retval = BMM050_OVERFLOW_OUTPUT_FLOAT;
    }
    return inter_retval;
}

bool RTIMUBMX055::magInitTrimRegisters()
{
    unsigned char data[2];
    data[0] = 0;
    data[1] = 0;

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_DIG_X1, 1, (uint8_t *)&m_dig_x1, "Failed to read BMX055 mag trim x1"))
        return false;

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_DIG_Y1, 1, (uint8_t *)&m_dig_y1, "Failed to read BMX055 mag trim y1"))
        return false;

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_DIG_X2, 1, (uint8_t *)&m_dig_x2, "Failed to read BMX055 mag trim x2"))
        return false;

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_DIG_Y2, 1, (uint8_t *)&m_dig_y2, "Failed to read BMX055 mag trim y2"))
        return false;

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_DIG_XY1, 1, &m_dig_xy1, "Failed to read BMX055 mag trim xy1"))
        return false;

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_DIG_XY2, 1, (uint8_t *)&m_dig_xy2, "Failed to read BMX055 mag trim xy2"))
        return false;

    /* shorts can not be recast into (uint8_t*)
    * due to possible mix up between trim data
    * arrangement and memory arrangement */

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_DIG_Z1_LSB, 2, data, "Failed to read BMX055 mag trim z1"))
        return false;

    m_dig_z1 = (uint16_t)((((uint32_t)((uint8_t)
    data[MSB_ONE])) <<
    SHIFT_LEFT_8_POSITION) | data[LSB_ZERO]);

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_DIG_Z2_LSB, 2, data, "Failed to read BMX055 mag trim z2"))
        return false;

    m_dig_z2 = (int16_t)((((int32_t)(
    (int8_t)data[MSB_ONE])) <<
    SHIFT_LEFT_8_POSITION) | data[LSB_ZERO]);

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_DIG_Z3_LSB, 2, data, "Failed to read BMX055 mag trim z3"))
        return false;

    m_dig_z3 = (int16_t)((((int32_t)(
    (int8_t)data[MSB_ONE])) <<
    SHIFT_LEFT_8_POSITION) | data[LSB_ZERO]);

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_DIG_Z4_LSB, 2, data, "Failed to read BMX055 mag trim z4"))
        return false;

    m_dig_z4 = (int16_t)((((int32_t)(
    (int8_t)data[MSB_ONE])) <<
    SHIFT_LEFT_8_POSITION) | data[LSB_ZERO]);

    if (!m_settings->HALRead(m_magSlaveAddr, BMX055_MAG_DIG_XYZ1_LSB, 2, data, "Failed to read BMX055 mag trim xyz1"))
        return false;

    data[MSB_ONE] = BMM050_GET_BITSLICE(data[MSB_ONE],
    BMM050_DIG_XYZ1_MSB);
    m_dig_xyz1 = (uint16_t)((((uint32_t)
    ((uint8_t)data[MSB_ONE])) <<
    SHIFT_LEFT_8_POSITION) | data[LSB_ZERO]);

    return true;
}
