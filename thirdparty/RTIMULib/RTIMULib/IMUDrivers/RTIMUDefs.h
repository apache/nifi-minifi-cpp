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

//  The MPU-9250 and SPI driver code is based on code generously supplied by
//  staslock@gmail.com (www.clickdrive.io)


#ifndef _RTIMUDEFS_H
#define	_RTIMUDEFS_H

//  IMU type codes
//
//  For compatibility, only add new codes at the end to avoid renumbering

#define RTIMU_TYPE_AUTODISCOVER             0                   // audodiscover the IMU
#define RTIMU_TYPE_NULL                     1                   // if no physical hardware
#define RTIMU_TYPE_MPU9150                  2                   // InvenSense MPU9150
#define RTIMU_TYPE_GD20HM303D               3                   // STM L3GD20H/LSM303D (Pololu Altimu)
#define RTIMU_TYPE_GD20M303DLHC             4                   // STM L3GD20/LSM303DHLC (old Adafruit IMU)
#define RTIMU_TYPE_LSM9DS0                  5                   // STM LSM9DS0 (eg Sparkfun IMU)
#define RTIMU_TYPE_LSM9DS1                  6                   // STM LSM9DS1
#define RTIMU_TYPE_MPU9250                  7                   // InvenSense MPU9250
#define RTIMU_TYPE_GD20HM303DLHC            8                   // STM L3GD20H/LSM303DHLC (new Adafruit IMU)
#define RTIMU_TYPE_BMX055                   9                   // Bosch BMX055
#define RTIMU_TYPE_BNO055                   10                  // Bosch BNO055

//----------------------------------------------------------
//
//  MPU-9150

//  MPU9150 I2C Slave Addresses

#define MPU9150_ADDRESS0            0x68
#define MPU9150_ADDRESS1            0x69
#define MPU9150_ID                  0x68

//  thes magnetometers are on aux bus

#define AK8975_ADDRESS              0x0c
#define HMC5883_ADDRESS             0x1e

//  Register map

#define MPU9150_YG_OFFS_TC          0x01
#define MPU9150_SMPRT_DIV           0x19
#define MPU9150_LPF_CONFIG          0x1a
#define MPU9150_GYRO_CONFIG         0x1b
#define MPU9150_ACCEL_CONFIG        0x1c
#define MPU9150_FIFO_EN             0x23
#define MPU9150_I2C_MST_CTRL        0x24
#define MPU9150_I2C_SLV0_ADDR       0x25
#define MPU9150_I2C_SLV0_REG        0x26
#define MPU9150_I2C_SLV0_CTRL       0x27
#define MPU9150_I2C_SLV1_ADDR       0x28
#define MPU9150_I2C_SLV1_REG        0x29
#define MPU9150_I2C_SLV1_CTRL       0x2a
#define MPU9150_I2C_SLV4_CTRL       0x34
#define MPU9150_INT_PIN_CFG         0x37
#define MPU9150_INT_ENABLE          0x38
#define MPU9150_INT_STATUS          0x3a
#define MPU9150_ACCEL_XOUT_H        0x3b
#define MPU9150_GYRO_XOUT_H         0x43
#define MPU9150_EXT_SENS_DATA_00    0x49
#define MPU9150_I2C_SLV1_DO         0x64
#define MPU9150_I2C_MST_DELAY_CTRL  0x67
#define MPU9150_USER_CTRL           0x6a
#define MPU9150_PWR_MGMT_1          0x6b
#define MPU9150_PWR_MGMT_2          0x6c
#define MPU9150_FIFO_COUNT_H        0x72
#define MPU9150_FIFO_R_W            0x74
#define MPU9150_WHO_AM_I            0x75

//  sample rate defines (applies to gyros and accels, not mags)

#define MPU9150_SAMPLERATE_MIN      5                      // 5 samples per second is the lowest
#define MPU9150_SAMPLERATE_MAX      1000                   // 1000 samples per second is the absolute maximum

//  compass rate defines

#define MPU9150_COMPASSRATE_MIN     1                      // 1 samples per second is the lowest
#define MPU9150_COMPASSRATE_MAX     100                    // 100 samples per second is maximum

//  LPF options (gyros and accels)

#define MPU9150_LPF_256             0                       // gyro: 256Hz, accel: 260Hz
#define MPU9150_LPF_188             1                       // gyro: 188Hz, accel: 184Hz
#define MPU9150_LPF_98              2                       // gyro: 98Hz, accel: 98Hz
#define MPU9150_LPF_42              3                       // gyro: 42Hz, accel: 44Hz
#define MPU9150_LPF_20              4                       // gyro: 20Hz, accel: 21Hz
#define MPU9150_LPF_10              5                       // gyro: 10Hz, accel: 10Hz
#define MPU9150_LPF_5               6                       // gyro: 5Hz, accel: 5Hz

//  Gyro FSR options

#define MPU9150_GYROFSR_250         0                       // +/- 250 degrees per second
#define MPU9150_GYROFSR_500         8                       // +/- 500 degrees per second
#define MPU9150_GYROFSR_1000        0x10                    // +/- 1000 degrees per second
#define MPU9150_GYROFSR_2000        0x18                    // +/- 2000 degrees per second

//  Accel FSR options

#define MPU9150_ACCELFSR_2          0                       // +/- 2g
#define MPU9150_ACCELFSR_4          8                       // +/- 4g
#define MPU9150_ACCELFSR_8          0x10                    // +/- 8g
#define MPU9150_ACCELFSR_16         0x18                    // +/- 16g


//  AK8975 compass registers

#define AK8975_DEVICEID             0x0                     // the device ID
#define AK8975_ST1                  0x02                    // status 1
#define AK8975_CNTL                 0x0a                    // control reg
#define AK8975_ASAX                 0x10                    // start of the fuse ROM data

//  HMC5883 compass registers

#define HMC5883_CONFIG_A            0x00                    // configuration A
#define HMC5883_CONFIG_B            0x01                    // configuration B
#define HMC5883_MODE                0x02                    // mode
#define HMC5883_DATA_X_HI           0x03                    // data x msb
#define HMC5883_STATUS              0x09                    // status
#define HMC5883_ID                  0x0a                    // id


//----------------------------------------------------------
//
//  MPU-9250

//  MPU9250 I2C Slave Addresses

#define MPU9250_ADDRESS0            0x68
#define MPU9250_ADDRESS1            0x69
#define MPU9250_ID                  0x71

#define AK8963_ADDRESS              0x0c

//  Register map

#define MPU9250_SMPRT_DIV           0x19
#define MPU9250_GYRO_LPF            0x1a
#define MPU9250_GYRO_CONFIG         0x1b
#define MPU9250_ACCEL_CONFIG        0x1c
#define MPU9250_ACCEL_LPF           0x1d
#define MPU9250_FIFO_EN             0x23
#define MPU9250_I2C_MST_CTRL        0x24
#define MPU9250_I2C_SLV0_ADDR       0x25
#define MPU9250_I2C_SLV0_REG        0x26
#define MPU9250_I2C_SLV0_CTRL       0x27
#define MPU9250_I2C_SLV1_ADDR       0x28
#define MPU9250_I2C_SLV1_REG        0x29
#define MPU9250_I2C_SLV1_CTRL       0x2a
#define MPU9250_I2C_SLV2_ADDR       0x2b
#define MPU9250_I2C_SLV2_REG        0x2c
#define MPU9250_I2C_SLV2_CTRL       0x2d
#define MPU9250_I2C_SLV4_CTRL       0x34
#define MPU9250_INT_PIN_CFG         0x37
#define MPU9250_INT_ENABLE          0x38
#define MPU9250_INT_STATUS          0x3a
#define MPU9250_ACCEL_XOUT_H        0x3b
#define MPU9250_GYRO_XOUT_H         0x43
#define MPU9250_EXT_SENS_DATA_00    0x49
#define MPU9250_I2C_SLV1_DO         0x64
#define MPU9250_I2C_MST_DELAY_CTRL  0x67
#define MPU9250_USER_CTRL           0x6a
#define MPU9250_PWR_MGMT_1          0x6b
#define MPU9250_PWR_MGMT_2          0x6c
#define MPU9250_FIFO_COUNT_H        0x72
#define MPU9250_FIFO_R_W            0x74
#define MPU9250_WHO_AM_I            0x75

//  sample rate defines (applies to gyros and accels, not mags)

#define MPU9250_SAMPLERATE_MIN      5                       // 5 samples per second is the lowest
#define MPU9250_SAMPLERATE_MAX      32000                   // 32000 samples per second is the absolute maximum

//  compass rate defines

#define MPU9250_COMPASSRATE_MIN     1                       // 1 samples per second is the lowest
#define MPU9250_COMPASSRATE_MAX     100                     // 100 samples per second is maximum

//  Gyro LPF options

#define MPU9250_GYRO_LPF_8800       0x11                    // 8800Hz, 0.64mS delay
#define MPU9250_GYRO_LPF_3600       0x10                    // 3600Hz, 0.11mS delay
#define MPU9250_GYRO_LPF_250        0x00                    // 250Hz, 0.97mS delay
#define MPU9250_GYRO_LPF_184        0x01                    // 184Hz, 2.9mS delay
#define MPU9250_GYRO_LPF_92         0x02                    // 92Hz, 3.9mS delay
#define MPU9250_GYRO_LPF_41         0x03                    // 41Hz, 5.9mS delay
#define MPU9250_GYRO_LPF_20         0x04                    // 20Hz, 9.9mS delay
#define MPU9250_GYRO_LPF_10         0x05                    // 10Hz, 17.85mS delay
#define MPU9250_GYRO_LPF_5          0x06                    // 5Hz, 33.48mS delay

//  Gyro FSR options

#define MPU9250_GYROFSR_250         0                       // +/- 250 degrees per second
#define MPU9250_GYROFSR_500         8                       // +/- 500 degrees per second
#define MPU9250_GYROFSR_1000        0x10                    // +/- 1000 degrees per second
#define MPU9250_GYROFSR_2000        0x18                    // +/- 2000 degrees per second

//  Accel FSR options

#define MPU9250_ACCELFSR_2          0                       // +/- 2g
#define MPU9250_ACCELFSR_4          8                       // +/- 4g
#define MPU9250_ACCELFSR_8          0x10                    // +/- 8g
#define MPU9250_ACCELFSR_16         0x18                    // +/- 16g

//  Accel LPF options

#define MPU9250_ACCEL_LPF_1130      0x08                    // 1130Hz, 0.75mS delay
#define MPU9250_ACCEL_LPF_460       0x00                    // 460Hz, 1.94mS delay
#define MPU9250_ACCEL_LPF_184       0x01                    // 184Hz, 5.80mS delay
#define MPU9250_ACCEL_LPF_92        0x02                    // 92Hz, 7.80mS delay
#define MPU9250_ACCEL_LPF_41        0x03                    // 41Hz, 11.80mS delay
#define MPU9250_ACCEL_LPF_20        0x04                    // 20Hz, 19.80mS delay
#define MPU9250_ACCEL_LPF_10        0x05                    // 10Hz, 35.70mS delay
#define MPU9250_ACCEL_LPF_5         0x06                    // 5Hz, 66.96mS delay

//  AK8963 compass registers

#define AK8963_DEVICEID             0x48                    // the device ID
#define AK8963_ST1                  0x02                    // status 1
#define AK8963_CNTL                 0x0a                    // control reg
#define AK8963_ASAX                 0x10                    // start of the fuse ROM data

//----------------------------------------------------------
//
//  L3GD20

//  I2C Slave Addresses

#define L3GD20_ADDRESS0             0x6a
#define L3GD20_ADDRESS1             0x6b
#define L3GD20_ID                   0xd4

//  L3GD20 Register map

#define L3GD20_WHO_AM_I        0x0f
#define L3GD20_CTRL1           0x20
#define L3GD20_CTRL2           0x21
#define L3GD20_CTRL3           0x22
#define L3GD20_CTRL4           0x23
#define L3GD20_CTRL5           0x24
#define L3GD20_OUT_TEMP        0x26
#define L3GD20_STATUS          0x27
#define L3GD20_OUT_X_L         0x28
#define L3GD20_OUT_X_H         0x29
#define L3GD20_OUT_Y_L         0x2a
#define L3GD20_OUT_Y_H         0x2b
#define L3GD20_OUT_Z_L         0x2c
#define L3GD20_OUT_Z_H         0x2d
#define L3GD20_FIFO_CTRL       0x2e
#define L3GD20_FIFO_SRC        0x2f
#define L3GD20_IG_CFG          0x30
#define L3GD20_IG_SRC          0x31
#define L3GD20_IG_THS_XH       0x32
#define L3GD20_IG_THS_XL       0x33
#define L3GD20_IG_THS_YH       0x34
#define L3GD20_IG_THS_YL       0x35
#define L3GD20_IG_THS_ZH       0x36
#define L3GD20_IG_THS_ZL       0x37
#define L3GD20_IG_DURATION     0x38

//  Gyro sample rate defines

#define L3GD20_SAMPLERATE_95    0
#define L3GD20_SAMPLERATE_190   1
#define L3GD20_SAMPLERATE_380   2
#define L3GD20_SAMPLERATE_760   3

//  Gyro banwidth defines

#define L3GD20_BANDWIDTH_0     0
#define L3GD20_BANDWIDTH_1     1
#define L3GD20_BANDWIDTH_2     2
#define L3GD20_BANDWIDTH_3     3

//  Gyro FSR defines

#define L3GD20_FSR_250         0
#define L3GD20_FSR_500         1
#define L3GD20_FSR_2000        2

//  Gyro high pass filter defines

#define L3GD20_HPF_0           0
#define L3GD20_HPF_1           1
#define L3GD20_HPF_2           2
#define L3GD20_HPF_3           3
#define L3GD20_HPF_4           4
#define L3GD20_HPF_5           5
#define L3GD20_HPF_6           6
#define L3GD20_HPF_7           7
#define L3GD20_HPF_8           8
#define L3GD20_HPF_9           9

//----------------------------------------------------------
//
//  L3GD20H

//  I2C Slave Addresses

#define L3GD20H_ADDRESS0        0x6a
#define L3GD20H_ADDRESS1        0x6b
#define L3GD20H_ID              0xd7

//  L3GD20H Register map

#define L3GD20H_WHO_AM_I        0x0f
#define L3GD20H_CTRL1           0x20
#define L3GD20H_CTRL2           0x21
#define L3GD20H_CTRL3           0x22
#define L3GD20H_CTRL4           0x23
#define L3GD20H_CTRL5           0x24
#define L3GD20H_OUT_TEMP        0x26
#define L3GD20H_STATUS          0x27
#define L3GD20H_OUT_X_L         0x28
#define L3GD20H_OUT_X_H         0x29
#define L3GD20H_OUT_Y_L         0x2a
#define L3GD20H_OUT_Y_H         0x2b
#define L3GD20H_OUT_Z_L         0x2c
#define L3GD20H_OUT_Z_H         0x2d
#define L3GD20H_FIFO_CTRL       0x2e
#define L3GD20H_FIFO_SRC        0x2f
#define L3GD20H_IG_CFG          0x30
#define L3GD20H_IG_SRC          0x31
#define L3GD20H_IG_THS_XH       0x32
#define L3GD20H_IG_THS_XL       0x33
#define L3GD20H_IG_THS_YH       0x34
#define L3GD20H_IG_THS_YL       0x35
#define L3GD20H_IG_THS_ZH       0x36
#define L3GD20H_IG_THS_ZL       0x37
#define L3GD20H_IG_DURATION     0x38
#define L3GD20H_LOW_ODR         0x39

//  Gyro sample rate defines

#define L3GD20H_SAMPLERATE_12_5 0
#define L3GD20H_SAMPLERATE_25   1
#define L3GD20H_SAMPLERATE_50   2
#define L3GD20H_SAMPLERATE_100  3
#define L3GD20H_SAMPLERATE_200  4
#define L3GD20H_SAMPLERATE_400  5
#define L3GD20H_SAMPLERATE_800  6

//  Gyro banwidth defines

#define L3GD20H_BANDWIDTH_0     0
#define L3GD20H_BANDWIDTH_1     1
#define L3GD20H_BANDWIDTH_2     2
#define L3GD20H_BANDWIDTH_3     3

//  Gyro FSR defines

#define L3GD20H_FSR_245         0
#define L3GD20H_FSR_500         1
#define L3GD20H_FSR_2000        2

//  Gyro high pass filter defines

#define L3GD20H_HPF_0           0
#define L3GD20H_HPF_1           1
#define L3GD20H_HPF_2           2
#define L3GD20H_HPF_3           3
#define L3GD20H_HPF_4           4
#define L3GD20H_HPF_5           5
#define L3GD20H_HPF_6           6
#define L3GD20H_HPF_7           7
#define L3GD20H_HPF_8           8
#define L3GD20H_HPF_9           9

//----------------------------------------------------------
//
//  LSM303D

#define LSM303D_ADDRESS0        0x1e
#define LSM303D_ADDRESS1        0x1d
#define LSM303D_ID              0x49

//  LSM303D Register Map

#define LSM303D_TEMP_OUT_L      0x05
#define LSM303D_TEMP_OUT_H      0x06
#define LSM303D_STATUS_M        0x07
#define LSM303D_OUT_X_L_M       0x08
#define LSM303D_OUT_X_H_M       0x09
#define LSM303D_OUT_Y_L_M       0x0a
#define LSM303D_OUT_Y_H_M       0x0b
#define LSM303D_OUT_Z_L_M       0x0c
#define LSM303D_OUT_Z_H_M       0x0d
#define LSM303D_WHO_AM_I        0x0f
#define LSM303D_INT_CTRL_M      0x12
#define LSM303D_INT_SRC_M       0x13
#define LSM303D_INT_THS_L_M     0x14
#define LSM303D_INT_THS_H_M     0x15
#define LSM303D_OFFSET_X_L_M    0x16
#define LSM303D_OFFSET_X_H_M    0x17
#define LSM303D_OFFSET_Y_L_M    0x18
#define LSM303D_OFFSET_Y_H_M    0x19
#define LSM303D_OFFSET_Z_L_M    0x1a
#define LSM303D_OFFSET_Z_H_M    0x1b
#define LSM303D_REFERENCE_X     0x1c
#define LSM303D_REFERENCE_Y     0x1d
#define LSM303D_REFERENCE_Z     0x1e
#define LSM303D_CTRL0           0x1f
#define LSM303D_CTRL1           0x20
#define LSM303D_CTRL2           0x21
#define LSM303D_CTRL3           0x22
#define LSM303D_CTRL4           0x23
#define LSM303D_CTRL5           0x24
#define LSM303D_CTRL6           0x25
#define LSM303D_CTRL7           0x26
#define LSM303D_STATUS_A        0x27
#define LSM303D_OUT_X_L_A       0x28
#define LSM303D_OUT_X_H_A       0x29
#define LSM303D_OUT_Y_L_A       0x2a
#define LSM303D_OUT_Y_H_A       0x2b
#define LSM303D_OUT_Z_L_A       0x2c
#define LSM303D_OUT_Z_H_A       0x2d
#define LSM303D_FIFO_CTRL       0x2e
#define LSM303D_FIFO_SRC        0x2f
#define LSM303D_IG_CFG1         0x30
#define LSM303D_IG_SRC1         0x31
#define LSM303D_IG_THS1         0x32
#define LSM303D_IG_DUR1         0x33
#define LSM303D_IG_CFG2         0x34
#define LSM303D_IG_SRC2         0x35
#define LSM303D_IG_THS2         0x36
#define LSM303D_IG_DUR2         0x37
#define LSM303D_CLICK_CFG       0x38
#define LSM303D_CLICK_SRC       0x39
#define LSM303D_CLICK_THS       0x3a
#define LSM303D_TIME_LIMIT      0x3b
#define LSM303D_TIME_LATENCY    0x3c
#define LSM303D_TIME_WINDOW     0x3d
#define LSM303D_ACT_THIS        0x3e
#define LSM303D_ACT_DUR         0x3f

//  Accel sample rate defines

#define LSM303D_ACCEL_SAMPLERATE_3_125 1
#define LSM303D_ACCEL_SAMPLERATE_6_25 2
#define LSM303D_ACCEL_SAMPLERATE_12_5 3
#define LSM303D_ACCEL_SAMPLERATE_25   4
#define LSM303D_ACCEL_SAMPLERATE_50   5
#define LSM303D_ACCEL_SAMPLERATE_100  6
#define LSM303D_ACCEL_SAMPLERATE_200  7
#define LSM303D_ACCEL_SAMPLERATE_400  8
#define LSM303D_ACCEL_SAMPLERATE_800  9
#define LSM303D_ACCEL_SAMPLERATE_1600 10

//  Accel FSR

#define LSM303D_ACCEL_FSR_2     0
#define LSM303D_ACCEL_FSR_4     1
#define LSM303D_ACCEL_FSR_6     2
#define LSM303D_ACCEL_FSR_8     3
#define LSM303D_ACCEL_FSR_16    4

//  Accel filter bandwidth

#define LSM303D_ACCEL_LPF_773   0
#define LSM303D_ACCEL_LPF_194   1
#define LSM303D_ACCEL_LPF_362   2
#define LSM303D_ACCEL_LPF_50    3

//  Compass sample rate defines

#define LSM303D_COMPASS_SAMPLERATE_3_125    0
#define LSM303D_COMPASS_SAMPLERATE_6_25     1
#define LSM303D_COMPASS_SAMPLERATE_12_5     2
#define LSM303D_COMPASS_SAMPLERATE_25       3
#define LSM303D_COMPASS_SAMPLERATE_50       4
#define LSM303D_COMPASS_SAMPLERATE_100      5

//  Compass FSR

#define LSM303D_COMPASS_FSR_2   0
#define LSM303D_COMPASS_FSR_4   1
#define LSM303D_COMPASS_FSR_8   2
#define LSM303D_COMPASS_FSR_12  3

//----------------------------------------------------------
//
//  LSM303DLHC

#define LSM303DLHC_ACCEL_ADDRESS    0x19
#define LSM303DLHC_COMPASS_ADDRESS  0x1e

//  LSM303DLHC Accel Register Map

#define LSM303DLHC_CTRL1_A         0x20
#define LSM303DLHC_CTRL2_A         0x21
#define LSM303DLHC_CTRL3_A         0x22
#define LSM303DLHC_CTRL4_A         0x23
#define LSM303DLHC_CTRL5_A         0x24
#define LSM303DLHC_CTRL6_A         0x25
#define LSM303DLHC_REF_A           0x26
#define LSM303DLHC_STATUS_A        0x27
#define LSM303DLHC_OUT_X_L_A       0x28
#define LSM303DLHC_OUT_X_H_A       0x29
#define LSM303DLHC_OUT_Y_L_A       0x2a
#define LSM303DLHC_OUT_Y_H_A       0x2b
#define LSM303DLHC_OUT_Z_L_A       0x2c
#define LSM303DLHC_OUT_Z_H_A       0x2d
#define LSM303DLHC_FIFO_CTRL_A     0x2e
#define LSM303DLHC_FIFO_SRC_A      0x2f

//  LSM303DLHC Compass Register Map

#define LSM303DLHC_CRA_M            0x00
#define LSM303DLHC_CRB_M            0x01
#define LSM303DLHC_CRM_M            0x02
#define LSM303DLHC_OUT_X_H_M        0x03
#define LSM303DLHC_OUT_X_L_M        0x04
#define LSM303DLHC_OUT_Y_H_M        0x05
#define LSM303DLHC_OUT_Y_L_M        0x06
#define LSM303DLHC_OUT_Z_H_M        0x07
#define LSM303DLHC_OUT_Z_L_M        0x08
#define LSM303DLHC_STATUS_M         0x09
#define LSM303DLHC_TEMP_OUT_L_M     0x31
#define LSM303DLHC_TEMP_OUT_H_M     0x32

//  Accel sample rate defines

#define LSM303DLHC_ACCEL_SAMPLERATE_1       1
#define LSM303DLHC_ACCEL_SAMPLERATE_10      2
#define LSM303DLHC_ACCEL_SAMPLERATE_25      3
#define LSM303DLHC_ACCEL_SAMPLERATE_50      4
#define LSM303DLHC_ACCEL_SAMPLERATE_100     5
#define LSM303DLHC_ACCEL_SAMPLERATE_200     6
#define LSM303DLHC_ACCEL_SAMPLERATE_400     7

//  Accel FSR

#define LSM303DLHC_ACCEL_FSR_2     0
#define LSM303DLHC_ACCEL_FSR_4     1
#define LSM303DLHC_ACCEL_FSR_8     2
#define LSM303DLHC_ACCEL_FSR_16    3

//  Compass sample rate defines

#define LSM303DLHC_COMPASS_SAMPLERATE_0_75      0
#define LSM303DLHC_COMPASS_SAMPLERATE_1_5       1
#define LSM303DLHC_COMPASS_SAMPLERATE_3         2
#define LSM303DLHC_COMPASS_SAMPLERATE_7_5       3
#define LSM303DLHC_COMPASS_SAMPLERATE_15        4
#define LSM303DLHC_COMPASS_SAMPLERATE_30        5
#define LSM303DLHC_COMPASS_SAMPLERATE_75        6
#define LSM303DLHC_COMPASS_SAMPLERATE_220       7

//  Compass FSR

#define LSM303DLHC_COMPASS_FSR_1_3      1
#define LSM303DLHC_COMPASS_FSR_1_9      2
#define LSM303DLHC_COMPASS_FSR_2_5      3
#define LSM303DLHC_COMPASS_FSR_4        4
#define LSM303DLHC_COMPASS_FSR_4_7      5
#define LSM303DLHC_COMPASS_FSR_5_6      6
#define LSM303DLHC_COMPASS_FSR_8_1      7

//----------------------------------------------------------
//
//  LSM9DS0

//  I2C Slave Addresses

#define LSM9DS0_GYRO_ADDRESS0       0x6a
#define LSM9DS0_GYRO_ADDRESS1       0x6b
#define LSM9DS0_GYRO_ID             0xd4

#define LSM9DS0_ACCELMAG_ADDRESS0   0x1e
#define LSM9DS0_ACCELMAG_ADDRESS1   0x1d
#define LSM9DS0_ACCELMAG_ID         0x49

//  LSM9DS0 Register map

#define LSM9DS0_GYRO_WHO_AM_I       0x0f
#define LSM9DS0_GYRO_CTRL1          0x20
#define LSM9DS0_GYRO_CTRL2          0x21
#define LSM9DS0_GYRO_CTRL3          0x22
#define LSM9DS0_GYRO_CTRL4          0x23
#define LSM9DS0_GYRO_CTRL5          0x24
#define LSM9DS0_GYRO_OUT_TEMP       0x26
#define LSM9DS0_GYRO_STATUS         0x27
#define LSM9DS0_GYRO_OUT_X_L        0x28
#define LSM9DS0_GYRO_OUT_X_H        0x29
#define LSM9DS0_GYRO_OUT_Y_L        0x2a
#define LSM9DS0_GYRO_OUT_Y_H        0x2b
#define LSM9DS0_GYRO_OUT_Z_L        0x2c
#define LSM9DS0_GYRO_OUT_Z_H        0x2d
#define LSM9DS0_GYRO_FIFO_CTRL      0x2e
#define LSM9DS0_GYRO_FIFO_SRC       0x2f
#define LSM9DS0_GYRO_IG_CFG         0x30
#define LSM9DS0_GYRO_IG_SRC         0x31
#define LSM9DS0_GYRO_IG_THS_XH      0x32
#define LSM9DS0_GYRO_IG_THS_XL      0x33
#define LSM9DS0_GYRO_IG_THS_YH      0x34
#define LSM9DS0_GYRO_IG_THS_YL      0x35
#define LSM9DS0_GYRO_IG_THS_ZH      0x36
#define LSM9DS0_GYRO_IG_THS_ZL      0x37
#define LSM9DS0_GYRO_IG_DURATION    0x38

//  Gyro sample rate defines

#define LSM9DS0_GYRO_SAMPLERATE_95  0
#define LSM9DS0_GYRO_SAMPLERATE_190 1
#define LSM9DS0_GYRO_SAMPLERATE_380 2
#define LSM9DS0_GYRO_SAMPLERATE_760 3

//  Gyro banwidth defines

#define LSM9DS0_GYRO_BANDWIDTH_0    0
#define LSM9DS0_GYRO_BANDWIDTH_1    1
#define LSM9DS0_GYRO_BANDWIDTH_2    2
#define LSM9DS0_GYRO_BANDWIDTH_3    3

//  Gyro FSR defines

#define LSM9DS0_GYRO_FSR_250        0
#define LSM9DS0_GYRO_FSR_500        1
#define LSM9DS0_GYRO_FSR_2000       2

//  Gyro high pass filter defines

#define LSM9DS0_GYRO_HPF_0          0
#define LSM9DS0_GYRO_HPF_1          1
#define LSM9DS0_GYRO_HPF_2          2
#define LSM9DS0_GYRO_HPF_3          3
#define LSM9DS0_GYRO_HPF_4          4
#define LSM9DS0_GYRO_HPF_5          5
#define LSM9DS0_GYRO_HPF_6          6
#define LSM9DS0_GYRO_HPF_7          7
#define LSM9DS0_GYRO_HPF_8          8
#define LSM9DS0_GYRO_HPF_9          9

//  Accel/Mag Register Map

#define LSM9DS0_TEMP_OUT_L      0x05
#define LSM9DS0_TEMP_OUT_H      0x06
#define LSM9DS0_STATUS_M        0x07
#define LSM9DS0_OUT_X_L_M       0x08
#define LSM9DS0_OUT_X_H_M       0x09
#define LSM9DS0_OUT_Y_L_M       0x0a
#define LSM9DS0_OUT_Y_H_M       0x0b
#define LSM9DS0_OUT_Z_L_M       0x0c
#define LSM9DS0_OUT_Z_H_M       0x0d
#define LSM9DS0_WHO_AM_I        0x0f
#define LSM9DS0_INT_CTRL_M      0x12
#define LSM9DS0_INT_SRC_M       0x13
#define LSM9DS0_INT_THS_L_M     0x14
#define LSM9DS0_INT_THS_H_M     0x15
#define LSM9DS0_OFFSET_X_L_M    0x16
#define LSM9DS0_OFFSET_X_H_M    0x17
#define LSM9DS0_OFFSET_Y_L_M    0x18
#define LSM9DS0_OFFSET_Y_H_M    0x19
#define LSM9DS0_OFFSET_Z_L_M    0x1a
#define LSM9DS0_OFFSET_Z_H_M    0x1b
#define LSM9DS0_REFERENCE_X     0x1c
#define LSM9DS0_REFERENCE_Y     0x1d
#define LSM9DS0_REFERENCE_Z     0x1e
#define LSM9DS0_CTRL0           0x1f
#define LSM9DS0_CTRL1           0x20
#define LSM9DS0_CTRL2           0x21
#define LSM9DS0_CTRL3           0x22
#define LSM9DS0_CTRL4           0x23
#define LSM9DS0_CTRL5           0x24
#define LSM9DS0_CTRL6           0x25
#define LSM9DS0_CTRL7           0x26
#define LSM9DS0_STATUS_A        0x27
#define LSM9DS0_OUT_X_L_A       0x28
#define LSM9DS0_OUT_X_H_A       0x29
#define LSM9DS0_OUT_Y_L_A       0x2a
#define LSM9DS0_OUT_Y_H_A       0x2b
#define LSM9DS0_OUT_Z_L_A       0x2c
#define LSM9DS0_OUT_Z_H_A       0x2d
#define LSM9DS0_FIFO_CTRL       0x2e
#define LSM9DS0_FIFO_SRC        0x2f
#define LSM9DS0_IG_CFG1         0x30
#define LSM9DS0_IG_SRC1         0x31
#define LSM9DS0_IG_THS1         0x32
#define LSM9DS0_IG_DUR1         0x33
#define LSM9DS0_IG_CFG2         0x34
#define LSM9DS0_IG_SRC2         0x35
#define LSM9DS0_IG_THS2         0x36
#define LSM9DS0_IG_DUR2         0x37
#define LSM9DS0_CLICK_CFG       0x38
#define LSM9DS0_CLICK_SRC       0x39
#define LSM9DS0_CLICK_THS       0x3a
#define LSM9DS0_TIME_LIMIT      0x3b
#define LSM9DS0_TIME_LATENCY    0x3c
#define LSM9DS0_TIME_WINDOW     0x3d
#define LSM9DS0_ACT_THIS        0x3e
#define LSM9DS0_ACT_DUR         0x3f

//  Accel sample rate defines

#define LSM9DS0_ACCEL_SAMPLERATE_3_125 1
#define LSM9DS0_ACCEL_SAMPLERATE_6_25 2
#define LSM9DS0_ACCEL_SAMPLERATE_12_5 3
#define LSM9DS0_ACCEL_SAMPLERATE_25   4
#define LSM9DS0_ACCEL_SAMPLERATE_50   5
#define LSM9DS0_ACCEL_SAMPLERATE_100  6
#define LSM9DS0_ACCEL_SAMPLERATE_200  7
#define LSM9DS0_ACCEL_SAMPLERATE_400  8
#define LSM9DS0_ACCEL_SAMPLERATE_800  9
#define LSM9DS0_ACCEL_SAMPLERATE_1600 10

//  Accel FSR

#define LSM9DS0_ACCEL_FSR_2     0
#define LSM9DS0_ACCEL_FSR_4     1
#define LSM9DS0_ACCEL_FSR_6     2
#define LSM9DS0_ACCEL_FSR_8     3
#define LSM9DS0_ACCEL_FSR_16    4

//  Accel filter bandwidth

#define LSM9DS0_ACCEL_LPF_773   0
#define LSM9DS0_ACCEL_LPF_194   1
#define LSM9DS0_ACCEL_LPF_362   2
#define LSM9DS0_ACCEL_LPF_50    3

//  Compass sample rate defines

#define LSM9DS0_COMPASS_SAMPLERATE_3_125    0
#define LSM9DS0_COMPASS_SAMPLERATE_6_25     1
#define LSM9DS0_COMPASS_SAMPLERATE_12_5     2
#define LSM9DS0_COMPASS_SAMPLERATE_25       3
#define LSM9DS0_COMPASS_SAMPLERATE_50       4
#define LSM9DS0_COMPASS_SAMPLERATE_100      5

//  Compass FSR

#define LSM9DS0_COMPASS_FSR_2   0
#define LSM9DS0_COMPASS_FSR_4   1
#define LSM9DS0_COMPASS_FSR_8   2
#define LSM9DS0_COMPASS_FSR_12  3

//----------------------------------------------------------
//
//  LSM9DS1

//  I2C Slave Addresses

#define LSM9DS1_ADDRESS0  0x6a
#define LSM9DS1_ADDRESS1  0x6b
#define LSM9DS1_ID        0x68

#define LSM9DS1_MAG_ADDRESS0        0x1c
#define LSM9DS1_MAG_ADDRESS1        0x1d
#define LSM9DS1_MAG_ADDRESS2        0x1e
#define LSM9DS1_MAG_ADDRESS3        0x1f
#define LSM9DS1_MAG_ID              0x3d

//  LSM9DS1 Register map

#define LSM9DS1_ACT_THS             0x04
#define LSM9DS1_ACT_DUR             0x05
#define LSM9DS1_INT_GEN_CFG_XL      0x06
#define LSM9DS1_INT_GEN_THS_X_XL    0x07
#define LSM9DS1_INT_GEN_THS_Y_XL    0x08
#define LSM9DS1_INT_GEN_THS_Z_XL    0x09
#define LSM9DS1_INT_GEN_DUR_XL      0x0A
#define LSM9DS1_REFERENCE_G         0x0B
#define LSM9DS1_INT1_CTRL           0x0C
#define LSM9DS1_INT2_CTRL           0x0D
#define LSM9DS1_WHO_AM_I            0x0F
#define LSM9DS1_CTRL1               0x10
#define LSM9DS1_CTRL2               0x11
#define LSM9DS1_CTRL3               0x12
#define LSM9DS1_ORIENT_CFG_G        0x13
#define LSM9DS1_INT_GEN_SRC_G       0x14
#define LSM9DS1_OUT_TEMP_L          0x15
#define LSM9DS1_OUT_TEMP_H          0x16
#define LSM9DS1_STATUS              0x17
#define LSM9DS1_OUT_X_L_G           0x18
#define LSM9DS1_OUT_X_H_G           0x19
#define LSM9DS1_OUT_Y_L_G           0x1A
#define LSM9DS1_OUT_Y_H_G           0x1B
#define LSM9DS1_OUT_Z_L_G           0x1C
#define LSM9DS1_OUT_Z_H_G           0x1D
#define LSM9DS1_CTRL4               0x1E
#define LSM9DS1_CTRL5               0x1F
#define LSM9DS1_CTRL6               0x20
#define LSM9DS1_CTRL7               0x21
#define LSM9DS1_CTRL8               0x22
#define LSM9DS1_CTRL9               0x23
#define LSM9DS1_CTRL10              0x24
#define LSM9DS1_INT_GEN_SRC_XL      0x26
#define LSM9DS1_STATUS2             0x27
#define LSM9DS1_OUT_X_L_XL          0x28
#define LSM9DS1_OUT_X_H_XL          0x29
#define LSM9DS1_OUT_Y_L_XL          0x2A
#define LSM9DS1_OUT_Y_H_XL          0x2B
#define LSM9DS1_OUT_Z_L_XL          0x2C
#define LSM9DS1_OUT_Z_H_XL          0x2D
#define LSM9DS1_FIFO_CTRL           0x2E
#define LSM9DS1_FIFO_SRC            0x2F
#define LSM9DS1_INT_GEN_CFG_G       0x30
#define LSM9DS1_INT_GEN_THS_XH_G    0x31
#define LSM9DS1_INT_GEN_THS_XL_G    0x32
#define LSM9DS1_INT_GEN_THS_YH_G    0x33
#define LSM9DS1_INT_GEN_THS_YL_G    0x34
#define LSM9DS1_INT_GEN_THS_ZH_G    0x35
#define LSM9DS1_INT_GEN_THS_ZL_G    0x36
#define LSM9DS1_INT_GEN_DUR_G       0x37

//  Gyro sample rate defines

#define LSM9DS1_GYRO_SAMPLERATE_14_9    0
#define LSM9DS1_GYRO_SAMPLERATE_59_5    1
#define LSM9DS1_GYRO_SAMPLERATE_119     2
#define LSM9DS1_GYRO_SAMPLERATE_238     3
#define LSM9DS1_GYRO_SAMPLERATE_476     4
#define LSM9DS1_GYRO_SAMPLERATE_952     5

//  Gyro banwidth defines

#define LSM9DS1_GYRO_BANDWIDTH_0    0
#define LSM9DS1_GYRO_BANDWIDTH_1    1
#define LSM9DS1_GYRO_BANDWIDTH_2    2
#define LSM9DS1_GYRO_BANDWIDTH_3    3

//  Gyro FSR defines

#define LSM9DS1_GYRO_FSR_250        0
#define LSM9DS1_GYRO_FSR_500        1
#define LSM9DS1_GYRO_FSR_2000       3

//  Gyro high pass filter defines

#define LSM9DS1_GYRO_HPF_0          0
#define LSM9DS1_GYRO_HPF_1          1
#define LSM9DS1_GYRO_HPF_2          2
#define LSM9DS1_GYRO_HPF_3          3
#define LSM9DS1_GYRO_HPF_4          4
#define LSM9DS1_GYRO_HPF_5          5
#define LSM9DS1_GYRO_HPF_6          6
#define LSM9DS1_GYRO_HPF_7          7
#define LSM9DS1_GYRO_HPF_8          8
#define LSM9DS1_GYRO_HPF_9          9

//  Mag Register Map

#define LSM9DS1_MAG_OFFSET_X_L      0x05
#define LSM9DS1_MAG_OFFSET_X_H      0x06
#define LSM9DS1_MAG_OFFSET_Y_L      0x07
#define LSM9DS1_MAG_OFFSET_Y_H      0x08
#define LSM9DS1_MAG_OFFSET_Z_L      0x09
#define LSM9DS1_MAG_OFFSET_Z_H      0x0A
#define LSM9DS1_MAG_WHO_AM_I        0x0F
#define LSM9DS1_MAG_CTRL1           0x20
#define LSM9DS1_MAG_CTRL2           0x21
#define LSM9DS1_MAG_CTRL3           0x22
#define LSM9DS1_MAG_CTRL4           0x23
#define LSM9DS1_MAG_CTRL5           0x24
#define LSM9DS1_MAG_STATUS          0x27
#define LSM9DS1_MAG_OUT_X_L         0x28
#define LSM9DS1_MAG_OUT_X_H         0x29
#define LSM9DS1_MAG_OUT_Y_L         0x2A
#define LSM9DS1_MAG_OUT_Y_H         0x2B
#define LSM9DS1_MAG_OUT_Z_L         0x2C
#define LSM9DS1_MAG_OUT_Z_H         0x2D
#define LSM9DS1_MAG_INT_CFG         0x30
#define LSM9DS1_MAG_INT_SRC         0x31
#define LSM9DS1_MAG_INT_THS_L       0x32
#define LSM9DS1_MAG_INT_THS_H       0x33

//  Accel sample rate defines

#define LSM9DS1_ACCEL_SAMPLERATE_14_9    1
#define LSM9DS1_ACCEL_SAMPLERATE_59_5    2
#define LSM9DS1_ACCEL_SAMPLERATE_119     3
#define LSM9DS1_ACCEL_SAMPLERATE_238     4
#define LSM9DS1_ACCEL_SAMPLERATE_476     5
#define LSM9DS1_ACCEL_SAMPLERATE_952     6

//  Accel FSR

#define LSM9DS1_ACCEL_FSR_2     0
#define LSM9DS1_ACCEL_FSR_16    1
#define LSM9DS1_ACCEL_FSR_4     2
#define LSM9DS1_ACCEL_FSR_8     3

//  Accel filter bandwidth

#define LSM9DS1_ACCEL_LPF_408   0
#define LSM9DS1_ACCEL_LPF_211   1
#define LSM9DS1_ACCEL_LPF_105   2
#define LSM9DS1_ACCEL_LPF_50    3

//  Compass sample rate defines

#define LSM9DS1_COMPASS_SAMPLERATE_0_625    0
#define LSM9DS1_COMPASS_SAMPLERATE_1_25     1
#define LSM9DS1_COMPASS_SAMPLERATE_2_5      2
#define LSM9DS1_COMPASS_SAMPLERATE_5        3
#define LSM9DS1_COMPASS_SAMPLERATE_10       4
#define LSM9DS1_COMPASS_SAMPLERATE_20       5
#define LSM9DS1_COMPASS_SAMPLERATE_40       6
#define LSM9DS1_COMPASS_SAMPLERATE_80       7

//  Compass FSR

#define LSM9DS1_COMPASS_FSR_4   0
#define LSM9DS1_COMPASS_FSR_8   1
#define LSM9DS1_COMPASS_FSR_12  2
#define LSM9DS1_COMPASS_FSR_16  3

//----------------------------------------------------------
//
//  BMX055

//  I2C Slave Addresses

#define BMX055_GYRO_ADDRESS0        0x68
#define BMX055_GYRO_ADDRESS1        0x69
#define BMX055_GYRO_ID              0x0f

#define BMX055_ACCEL_ADDRESS0       0x18
#define BMX055_ACCEL_ADDRESS1       0x19
#define BMX055_ACCEL_ID             0xfa

#define BMX055_MAG_ADDRESS0         0x10
#define BMX055_MAG_ADDRESS1         0x11
#define BMX055_MAG_ADDRESS2         0x12
#define BMX055_MAG_ADDRESS3         0x13

#define BMX055_MAG_ID               0x32

//  BMX055 Register map

#define BMX055_GYRO_WHO_AM_I        0x00
#define BMX055_GYRO_X_LSB           0x02
#define BMX055_GYRO_X_MSB           0x03
#define BMX055_GYRO_Y_LSB           0x04
#define BMX055_GYRO_Y_MSB           0x05
#define BMX055_GYRO_Z_LSB           0x06
#define BMX055_GYRO_Z_MSB           0x07
#define BMX055_GYRO_INT_STATUS_0    0x09
#define BMX055_GYRO_INT_STATUS_1    0x0a
#define BMX055_GYRO_INT_STATUS_2    0x0b
#define BMX055_GYRO_INT_STATUS_3    0x0c
#define BMX055_GYRO_FIFO_STATUS     0x0e
#define BMX055_GYRO_RANGE           0x0f
#define BMX055_GYRO_BW              0x10
#define BMX055_GYRO_LPM1            0x11
#define BMX055_GYRO_LPM2            0x12
#define BMX055_GYRO_RATE_HBW        0x13
#define BMX055_GYRO_SOFT_RESET      0x14
#define BMX055_GYRO_INT_EN_0        0x15
#define BMX055_GYRO_1A              0x1a
#define BMX055_GYRO_1B              0x1b
#define BMX055_GYRO_SOC             0x31
#define BMX055_GYRO_FOC             0x32
#define BMX055_GYRO_FIFO_CONFIG_0   0x3d
#define BMX055_GYRO_FIFO_CONFIG_1   0x3e
#define BMX055_GYRO_FIFO_DATA       0x3f

#define BMX055_ACCEL_WHO_AM_I       0x00
#define BMX055_ACCEL_X_LSB          0x02
#define BMX055_ACCEL_X_MSB          0x03
#define BMX055_ACCEL_Y_LSB          0x04
#define BMX055_ACCEL_Y_MSB          0x05
#define BMX055_ACCEL_Z_LSB          0x06
#define BMX055_ACCEL_Z_MSB          0x07
#define BMX055_ACCEL_TEMP           0x08
#define BMX055_ACCEL_INT_STATUS_0   0x09
#define BMX055_ACCEL_INT_STATUS_1   0x0a
#define BMX055_ACCEL_INT_STATUS_2   0x0b
#define BMX055_ACCEL_INT_STATUS_3   0x0c
#define BMX055_ACCEL_FIFO_STATUS    0x0e
#define BMX055_ACCEL_PMU_RANGE      0x0f
#define BMX055_ACCEL_PMU_BW         0x10
#define BMX055_ACCEL_PMU_LPW        0x11
#define BMX055_ACCEL_PMU_LOW_POWER  0x12
#define BMX055_ACCEL_HBW            0x13
#define BMX055_ACCEL_SOFT_RESET     0x14
#define BMX055_ACCEL_FIFO_CONFIG_0  0x30
#define BMX055_ACCEL_OFC_CTRL       0x36
#define BMX055_ACCEL_OFC_SETTING    0x37
#define BMX055_ACCEL_FIFO_CONFIG_1  0x3e
#define BMX055_ACCEL_FIFO_DATA      0x3f

#define BMX055_MAG_WHO_AM_I         0x40
#define BMX055_MAG_X_LSB            0x42
#define BMX055_MAG_X_MSB            0x43
#define BMX055_MAG_Y_LSB            0x44
#define BMX055_MAG_Y_MSB            0x45
#define BMX055_MAG_Z_LSB            0x46
#define BMX055_MAG_Z_MSB            0x47
#define BMX055_MAG_RHALL_LSB        0x48
#define BMX055_MAG_RHALL_MSB        0x49
#define BMX055_MAG_INT_STAT         0x4a
#define BMX055_MAG_POWER            0x4b
#define BMX055_MAG_MODE             0x4c
#define BMX055_MAG_INT_ENABLE       0x4d
#define BMX055_MAG_AXIS_ENABLE      0x4e
#define BMX055_MAG_REPXY            0x51
#define BMX055_MAG_REPZ             0x52

#define BMX055_MAG_DIG_X1               0x5D
#define BMX055_MAG_DIG_Y1               0x5E
#define BMX055_MAG_DIG_Z4_LSB           0x62
#define BMX055_MAG_DIG_Z4_MSB           0x63
#define BMX055_MAG_DIG_X2               0x64
#define BMX055_MAG_DIG_Y2               0x65
#define BMX055_MAG_DIG_Z2_LSB           0x68
#define BMX055_MAG_DIG_Z2_MSB           0x69
#define BMX055_MAG_DIG_Z1_LSB           0x6A
#define BMX055_MAG_DIG_Z1_MSB           0x6B
#define BMX055_MAG_DIG_XYZ1_LSB         0x6C
#define BMX055_MAG_DIG_XYZ1_MSB         0x6D
#define BMX055_MAG_DIG_Z3_LSB           0x6E
#define BMX055_MAG_DIG_Z3_MSB           0x6F
#define BMX055_MAG_DIG_XY2              0x70
#define BMX055_MAG_DIG_XY1              0x71

//  Gyro sample rate defines

#define BMX055_GYRO_SAMPLERATE_100_32  0x07
#define BMX055_GYRO_SAMPLERATE_200_64  0x06
#define BMX055_GYRO_SAMPLERATE_100_12  0x05
#define BMX055_GYRO_SAMPLERATE_200_23  0x04
#define BMX055_GYRO_SAMPLERATE_400_47  0x03
#define BMX055_GYRO_SAMPLERATE_1000_116  0x02
#define BMX055_GYRO_SAMPLERATE_2000_230  0x01
#define BMX055_GYRO_SAMPLERATE_2000_523  0x00

//  Gyro FSR defines

#define BMX055_GYRO_FSR_2000        0x00
#define BMX055_GYRO_FSR_1000        0x01
#define BMX055_GYRO_FSR_500         0x02
#define BMX055_GYRO_FSR_250         0x03
#define BMX055_GYRO_FSR_125         0x04

//  Accel sample rate defines

#define BMX055_ACCEL_SAMPLERATE_15  0X00
#define BMX055_ACCEL_SAMPLERATE_31  0X01
#define BMX055_ACCEL_SAMPLERATE_62  0X02
#define BMX055_ACCEL_SAMPLERATE_125  0X03
#define BMX055_ACCEL_SAMPLERATE_250  0X04
#define BMX055_ACCEL_SAMPLERATE_500  0X05
#define BMX055_ACCEL_SAMPLERATE_1000  0X06
#define BMX055_ACCEL_SAMPLERATE_2000  0X07

//  Accel FSR defines

#define BMX055_ACCEL_FSR_2          0x00
#define BMX055_ACCEL_FSR_4          0x01
#define BMX055_ACCEL_FSR_8          0x02
#define BMX055_ACCEL_FSR_16         0x03


//  Mag preset defines

#define BMX055_MAG_LOW_POWER        0x00
#define BMX055_MAG_REGULAR          0x01
#define BMX055_MAG_ENHANCED         0x02
#define BMX055_MAG_HIGH_ACCURACY    0x03

//----------------------------------------------------------
//
//  BNO055

//  I2C Slave Addresses

#define BNO055_ADDRESS0             0x28
#define BNO055_ADDRESS1             0x29
#define BNO055_ID                   0xa0

//  Register map

#define BNO055_WHO_AM_I             0x00
#define BNO055_PAGE_ID              0x07
#define BNO055_ACCEL_DATA           0x08
#define BNO055_MAG_DATA             0x0e
#define BNO055_GYRO_DATA            0x14
#define BNO055_FUSED_EULER          0x1a
#define BNO055_FUSED_QUAT           0x20
#define BNO055_UNIT_SEL             0x3b
#define BNO055_OPER_MODE            0x3d
#define BNO055_PWR_MODE             0x3e
#define BNO055_SYS_TRIGGER          0x3f
#define BNO055_AXIS_MAP_CONFIG      0x41
#define BNO055_AXIS_MAP_SIGN        0x42

//  Operation modes

#define BNO055_OPER_MODE_CONFIG     0x00
#define BNO055_OPER_MODE_NDOF       0x0c

//  Power modes

#define BNO055_PWR_MODE_NORMAL      0x00

#endif // _RTIMUDEFS_H
