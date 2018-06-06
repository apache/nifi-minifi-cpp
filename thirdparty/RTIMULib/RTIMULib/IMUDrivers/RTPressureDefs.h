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

#ifndef _RTPRESSUREDEFS_H
#define	_RTPRESSUREDEFS_H

//  Pressure sensor type codes

#define RTPRESSURE_TYPE_AUTODISCOVER        0                   // audodiscover the pressure sensor
#define RTPRESSURE_TYPE_NULL                1                   // if no physical hardware
#define RTPRESSURE_TYPE_BMP180              2                   // BMP180
#define RTPRESSURE_TYPE_LPS25H              3                   // LPS25H
#define RTPRESSURE_TYPE_MS5611              4                   // MS5611
#define RTPRESSURE_TYPE_MS5637              5                   // MS5637

//----------------------------------------------------------
//
//  BMP180

//  BMP180 I2C Slave Addresses

#define BMP180_ADDRESS              0x77
#define BMP180_REG_ID               0xd0
#define BMP180_ID                   0x55

//	Register map

#define BMP180_REG_AC1              0xaa
#define BMP180_REG_SCO              0xf4
#define BMP180_REG_RESULT           0xf6
#define BMP180_REG_XLSB             0xf8

//----------------------------------------------------------
//
//  LPS25H

//  LPS25H I2C Slave Addresses

#define LPS25H_ADDRESS0             0x5c
#define LPS25H_ADDRESS1             0x5d
#define LPS25H_REG_ID               0x0f
#define LPS25H_ID                   0xbd

//	Register map

#define LPS25H_REF_P_XL             0x08
#define LPS25H_REF_P_XH             0x09
#define LPS25H_RES_CONF             0x10
#define LPS25H_CTRL_REG_1           0x20
#define LPS25H_CTRL_REG_2           0x21
#define LPS25H_CTRL_REG_3           0x22
#define LPS25H_CTRL_REG_4           0x23
#define LPS25H_INT_CFG              0x24
#define LPS25H_INT_SOURCE           0x25
#define LPS25H_STATUS_REG           0x27
#define LPS25H_PRESS_OUT_XL         0x28
#define LPS25H_PRESS_OUT_L          0x29
#define LPS25H_PRESS_OUT_H          0x2a
#define LPS25H_TEMP_OUT_L           0x2b
#define LPS25H_TEMP_OUT_H           0x2c
#define LPS25H_FIFO_CTRL            0x2e
#define LPS25H_FIFO_STATUS          0x2f
#define LPS25H_THS_P_L              0x30
#define LPS25H_THS_P_H              0x31
#define LPS25H_RPDS_L               0x39
#define LPS25H_RPDS_H               0x3a

//----------------------------------------------------------
//
//  MS5611 and MS5637

//  MS5611 I2C Slave Addresses

#define MS5611_ADDRESS0             0x76
#define MS5611_ADDRESS1             0x77

//	commands

#define MS5611_CMD_RESET            0x1e
#define MS5611_CMD_CONV_D1          0x48
#define MS5611_CMD_CONV_D2          0x58
#define MS5611_CMD_PROM             0xa0
#define MS5611_CMD_ADC              0x00

#endif // _RTPRESSUREDEFS_H
