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


#ifndef _RTIMUBMX055_H
#define	_RTIMUBMX055_H

#include "RTIMU.h"

class RTIMUBMX055 : public RTIMU
{
public:
    RTIMUBMX055(RTIMUSettings *settings);
    ~RTIMUBMX055();

    virtual const char *IMUName() { return "BMX055"; }
    virtual int IMUType() { return RTIMU_TYPE_BMX055; }
    virtual bool IMUInit();
    virtual int IMUGetPollInterval();
    virtual bool IMURead();

private:
    bool setGyroSampleRate();
    bool setGyroFSR();
    bool setAccelSampleRate();
    bool setAccelFSR();
    bool magInitTrimRegisters();
    bool setMagPreset();
    void processMagData(unsigned char *v_data_uint8_t, float& magX, float& magY, float& magZ);
    float bmm050_compensate_X_float(int16_t mag_data_x, uint16_t data_r);
    float bmm050_compensate_Y_float(int16_t mag_data_y, uint16_t data_r);
    float bmm050_compensate_Z_float(int16_t mag_data_z, uint16_t data_r);

    unsigned char m_gyroSlaveAddr;                          // I2C address of gyro
    unsigned char m_accelSlaveAddr;                         // I2C address of accel
    unsigned char m_magSlaveAddr;                           // I2C address of mag

    bool m_firstTime;                                       // if first sample

    RTFLOAT m_gyroScale;
    RTFLOAT m_accelScale;

    int8_t m_dig_x1;/**< trim x1 data */
    int8_t m_dig_y1;/**< trim y1 data */

    int8_t m_dig_x2;/**< trim x2 data */
    int8_t m_dig_y2;/**< trim y2 data */

    uint16_t m_dig_z1;/**< trim z1 data */
    int16_t m_dig_z2;/**< trim z2 data */
    int16_t m_dig_z3;/**< trim z3 data */
    int16_t m_dig_z4;/**< trim z4 data */

    uint8_t m_dig_xy1;/**< trim xy1 data */
    int8_t m_dig_xy2;/**< trim xy2 data */

    uint16_t m_dig_xyz1;/**< trim xyz1 data */
};

#endif // _RTIMUBMX055_H
