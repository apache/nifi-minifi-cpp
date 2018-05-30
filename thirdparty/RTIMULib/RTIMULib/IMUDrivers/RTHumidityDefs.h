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

#ifndef _RTHUMIDITYDEFS_H
#define _RTHUMIDITYDEFS_H

//  Pressure sensor type codes

#define RTHUMIDITY_TYPE_AUTODISCOVER	0                   // audodiscover the humidity sensor
#define RTHUMIDITY_TYPE_NULL            1                   // if no physical hardware
#define RTHUMIDITY_TYPE_HTS221          2                   // HTS221
#define RTHUMIDITY_TYPE_HTU21D          3                   // HTU21D

//----------------------------------------------------------
//
//  HTS221

//  HTS221 I2C Slave Address

#define HTS221_ADDRESS          0x5f
#define HTS221_REG_ID           0x0f
#define HTS221_ID               0xbc

//  Register map

#define HTS221_WHO_AM_I         0x0f
#define HTS221_AV_CONF          0x10
#define HTS221_CTRL1            0x20
#define HTS221_CTRL2            0x21
#define HTS221_CTRL3            0x22
#define HTS221_STATUS           0x27
#define HTS221_HUMIDITY_OUT_L   0x28
#define HTS221_HUMIDITY_OUT_H   0x29
#define HTS221_TEMP_OUT_L       0x2a
#define HTS221_TEMP_OUT_H       0x2b
#define HTS221_H0_H_2           0x30
#define HTS221_H1_H_2           0x31
#define HTS221_T0_C_8           0x32
#define HTS221_T1_C_8           0x33
#define HTS221_T1_T0            0x35
#define HTS221_H0_T0_OUT        0x36
#define HTS221_H1_T0_OUT        0x3a
#define HTS221_T0_OUT           0x3c
#define HTS221_T1_OUT           0x3e

//----------------------------------------------------------
//
//  HTU21D

//  HTU21D I2C Slave Address

#define HTU21D_ADDRESS          0x40

//  Register map

#define HTU21D_CMD_TRIG_TEMP    0xf3
#define HTU21D_CMD_TRIG_HUM     0xf5
#define HTU21D_WRITE_USER_REG   0xe6
#define HTU21D_READ_USER_REG    0xe7
#define HTU21D_CMD_SOFT_RESET   0xfe

#endif // _RTHUMIDITYDEFS_H
