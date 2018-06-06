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


#include "RTIMUSettings.h"
#include "IMUDrivers/RTIMUMPU9150.h"
#include "IMUDrivers/RTIMUMPU9250.h"
#include "IMUDrivers/RTIMUGD20HM303D.h"
#include "IMUDrivers/RTIMUGD20M303DLHC.h"
#include "IMUDrivers/RTIMUGD20HM303DLHC.h"
#include "IMUDrivers/RTIMULSM9DS0.h"
#include "IMUDrivers/RTIMULSM9DS1.h"
#include "IMUDrivers/RTIMUBMX055.h"

#include "IMUDrivers/RTPressureBMP180.h"
#include "IMUDrivers/RTPressureLPS25H.h"

#include "IMUDrivers/RTHumidityHTS221.h"
#include "IMUDrivers/RTHumidityHTU21D.h"

#define RATE_TIMER_INTERVAL 2

RTIMUSettings::RTIMUSettings(const char *productType)
{
    if ((strlen(productType) > 200) || (strlen(productType) == 0)) {
        HAL_ERROR("Product name too long or null - using default\n");
        strcpy(m_filename, "RTIMULib.ini");
    } else {
        sprintf(m_filename, "%s.ini", productType);
    }
    loadSettings();
}

RTIMUSettings::RTIMUSettings(const char *settingsDirectory, const char *productType)
{
    if (((strlen(productType) + strlen(settingsDirectory)) > 200) || (strlen(productType) == 0)) {
        HAL_ERROR("Product name too long or null - using default\n");
        strcpy(m_filename, "RTIMULib.ini");
    } else {
        sprintf(m_filename, "%s/%s.ini", settingsDirectory, productType);
    }
    loadSettings();
}


bool RTIMUSettings::discoverIMU(int& imuType, bool& busIsI2C, unsigned char& slaveAddress)
{
    unsigned char result;
    unsigned char altResult;

    //  auto detect on I2C bus

    m_busIsI2C = true;

    if (HALOpen()) {

        if (HALRead(MPU9150_ADDRESS0, MPU9150_WHO_AM_I, 1, &result, "")) {
            if (result == MPU9250_ID) {
                imuType = RTIMU_TYPE_MPU9250;
                slaveAddress = MPU9250_ADDRESS0;
                busIsI2C = true;
                HAL_INFO("Detected MPU9250 at standard address\n");
                return true;
            } else if (result == MPU9150_ID) {
                imuType = RTIMU_TYPE_MPU9150;
                slaveAddress = MPU9150_ADDRESS0;
                busIsI2C = true;
                HAL_INFO("Detected MPU9150 at standard address\n");
                return true;
            }
        }

        if (HALRead(MPU9150_ADDRESS1, MPU9150_WHO_AM_I, 1, &result, "")) {
            if (result == MPU9250_ID) {
                imuType = RTIMU_TYPE_MPU9250;
                slaveAddress = MPU9250_ADDRESS1;
                busIsI2C = true;
                HAL_INFO("Detected MPU9250 at option address\n");
                return true;
            } else if (result == MPU9150_ID) {
                imuType = RTIMU_TYPE_MPU9150;
                slaveAddress = MPU9150_ADDRESS1;
                busIsI2C = true;
                HAL_INFO("Detected MPU9150 at option address\n");
                return true;
            }
        }

        if (HALRead(L3GD20H_ADDRESS0, L3GD20H_WHO_AM_I, 1, &result, "")) {
            if (result == L3GD20H_ID) {
                if (HALRead(LSM303D_ADDRESS0, LSM303D_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM303D_ID) {
                        imuType = RTIMU_TYPE_GD20HM303D;
                        slaveAddress = L3GD20H_ADDRESS0;
                        busIsI2C = true;
                        HAL_INFO("Detected L3GD20H/LSM303D at standard/standard address\n");
                        return true;
                    }
                }
                if (HALRead(LSM303D_ADDRESS1, LSM303D_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM303D_ID) {
                        imuType = RTIMU_TYPE_GD20HM303D;
                        slaveAddress = L3GD20H_ADDRESS0;
                        busIsI2C = true;
                        HAL_INFO("Detected L3GD20H/LSM303D at standard/option address\n");
                        return true;
                    }
                }
                if (HALRead(LSM303DLHC_ACCEL_ADDRESS, LSM303DLHC_STATUS_A, 1, &altResult, "")) {
                    imuType = RTIMU_TYPE_GD20HM303DLHC;
                    slaveAddress = L3GD20H_ADDRESS0;
                    busIsI2C = true;
                    HAL_INFO("Detected L3GD20H/LSM303DLHC at standard/standard address\n");
                    return true;
                }
            } else if (result == LSM9DS0_GYRO_ID) {
                if (HALRead(LSM9DS0_ACCELMAG_ADDRESS0, LSM9DS0_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS0_ACCELMAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS0;
                        slaveAddress = LSM9DS0_GYRO_ADDRESS0;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS0 at standard/standard address\n");
                        return true;
                    }
                }
                if (HALRead(LSM9DS0_ACCELMAG_ADDRESS1, LSM9DS0_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS0_ACCELMAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS0;
                        slaveAddress = LSM9DS0_GYRO_ADDRESS0;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS0 at standard/option address\n");
                        return true;
                    }
                }
            } else if (result == LSM9DS1_ID) {
                if (HALRead(LSM9DS1_MAG_ADDRESS0, LSM9DS1_MAG_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS1_MAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS1;
                        slaveAddress = LSM9DS1_ADDRESS0;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS1 at standard/standard address\n");
                        return true;
                    }
                }
                if (HALRead(LSM9DS1_MAG_ADDRESS1, LSM9DS1_MAG_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS1_MAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS1;
                        slaveAddress = LSM9DS1_ADDRESS0;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS1 at standard/option 1 address\n");
                        return true;
                    }
                }
                if (HALRead(LSM9DS1_MAG_ADDRESS2, LSM9DS1_MAG_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS1_MAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS1;
                        slaveAddress = LSM9DS1_ADDRESS0;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS1 at standard/option 2 address\n");
                        return true;
                    }
                }
                if (HALRead(LSM9DS1_MAG_ADDRESS3, LSM9DS1_MAG_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS1_MAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS1;
                        slaveAddress = LSM9DS1_ADDRESS0;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS1 at standard/option 3 address\n");
                        return true;
                    }
                }
            }
        }

        if (HALRead(L3GD20H_ADDRESS1, L3GD20H_WHO_AM_I, 1, &result, "")) {
            if (result == L3GD20H_ID) {
                if (HALRead(LSM303D_ADDRESS1, LSM303D_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM303D_ID) {
                        imuType = RTIMU_TYPE_GD20HM303D;
                        slaveAddress = L3GD20H_ADDRESS1;
                        busIsI2C = true;
                        HAL_INFO("Detected L3GD20H/LSM303D at option/option address\n");
                        return true;
                    }
                }
                if (HALRead(LSM303D_ADDRESS0, LSM303D_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM303D_ID) {
                        imuType = RTIMU_TYPE_GD20HM303D;
                        slaveAddress = L3GD20H_ADDRESS1;
                        busIsI2C = true;
                        HAL_INFO("Detected L3GD20H/LSM303D at option/standard address\n");
                        return true;
                    }
                }
                if (HALRead(LSM303DLHC_ACCEL_ADDRESS, LSM303DLHC_STATUS_A, 1, &altResult, "")) {
                    imuType = RTIMU_TYPE_GD20HM303DLHC;
                    slaveAddress = L3GD20H_ADDRESS1;
                    busIsI2C = true;
                    HAL_INFO("Detected L3GD20H/LSM303DLHC at option/standard address\n");
                    return true;
                }
            } else if (result == LSM9DS0_GYRO_ID) {
                if (HALRead(LSM9DS0_ACCELMAG_ADDRESS1, LSM9DS0_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS0_ACCELMAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS0;
                        slaveAddress = LSM9DS0_GYRO_ADDRESS1;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS0 at option/option address\n");
                        return true;
                    }
                }
                if (HALRead(LSM9DS0_ACCELMAG_ADDRESS0, LSM9DS0_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS0_ACCELMAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS0;
                        slaveAddress = LSM9DS0_GYRO_ADDRESS1;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS0 at option/standard address\n");
                        return true;
                    }
                }
            } else if (result == LSM9DS1_ID) {
                if (HALRead(LSM9DS1_MAG_ADDRESS0, LSM9DS1_MAG_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS1_MAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS1;
                        slaveAddress = LSM9DS1_ADDRESS1;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS1 at option/standard address\n");
                        return true;
                    }
                }
                if (HALRead(LSM9DS1_MAG_ADDRESS1, LSM9DS1_MAG_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS1_MAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS1;
                        slaveAddress = LSM9DS1_ADDRESS1;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS1 at option/option 1 address\n");
                        return true;
                    }
                }
                if (HALRead(LSM9DS1_MAG_ADDRESS2, LSM9DS1_MAG_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS1_MAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS1;
                        slaveAddress = LSM9DS1_ADDRESS1;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS1 at option/option 2 address\n");
                        return true;
                    }
                }
                if (HALRead(LSM9DS1_MAG_ADDRESS3, LSM9DS1_MAG_WHO_AM_I, 1, &altResult, "")) {
                    if (altResult == LSM9DS1_MAG_ID) {
                        imuType = RTIMU_TYPE_LSM9DS1;
                        slaveAddress = LSM9DS1_ADDRESS1;
                        busIsI2C = true;
                        HAL_INFO("Detected LSM9DS1 at option/option 3 address\n");
                        return true;
                    }
                }
            }
        }

        if (HALRead(L3GD20_ADDRESS0, L3GD20_WHO_AM_I, 1, &result, "")) {
            if (result == L3GD20_ID) {
                imuType = RTIMU_TYPE_GD20M303DLHC;
                slaveAddress = L3GD20_ADDRESS0;
                busIsI2C = true;
                HAL_INFO("Detected L3GD20 at standard address\n");
                return true;
            }
        }

        if (HALRead(L3GD20_ADDRESS1, L3GD20_WHO_AM_I, 1, &result, "")) {
            if (result == L3GD20_ID) {
                imuType = RTIMU_TYPE_GD20M303DLHC;
                slaveAddress = L3GD20_ADDRESS1;
                busIsI2C = true;
                HAL_INFO("Detected L3GD20 at option address\n");
                return true;
            }
        }

        if (HALRead(BMX055_GYRO_ADDRESS0, BMX055_GYRO_WHO_AM_I, 1, &result, "")) {
            if (result == BMX055_GYRO_ID) {
                imuType = RTIMU_TYPE_BMX055;
                slaveAddress = BMX055_GYRO_ADDRESS0;
                busIsI2C = true;
                HAL_INFO("Detected BMX055 at standard address\n");
                return true;
            }
        }
        if (HALRead(BMX055_GYRO_ADDRESS1, BMX055_GYRO_WHO_AM_I, 1, &result, "")) {
            if (result == BMX055_GYRO_ID) {
                imuType = RTIMU_TYPE_BMX055;
                slaveAddress = BMX055_GYRO_ADDRESS1;
                busIsI2C = true;
                HAL_INFO("Detected BMX055 at option address\n");
                return true;
            }
        }

        if (HALRead(BNO055_ADDRESS0, BNO055_WHO_AM_I, 1, &result, "")) {
            if (result == BNO055_ID) {
                imuType = RTIMU_TYPE_BNO055;
                slaveAddress = BNO055_ADDRESS0;
                busIsI2C = true;
                HAL_INFO("Detected BNO055 at standard address\n");
                return true;
            }
        }
        if (HALRead(BNO055_ADDRESS1, BNO055_WHO_AM_I, 1, &result, "")) {
            if (result == BNO055_ID) {
                imuType = RTIMU_TYPE_BNO055;
                slaveAddress = BNO055_ADDRESS1;
                busIsI2C = true;
                HAL_INFO("Detected BNO055 at option address\n");
                return true;
            }
        }
        HALClose();
    }

    //  nothing found on I2C bus - try SPI instead

    m_busIsI2C = false;
    m_SPIBus = 0;

    m_SPISelect = 0;

    if (HALOpen()) {
        if (HALRead(MPU9250_ADDRESS0, MPU9250_WHO_AM_I, 1, &result, "")) {
            if (result == MPU9250_ID) {
                imuType = RTIMU_TYPE_MPU9250;
                slaveAddress = MPU9250_ADDRESS0;
                busIsI2C = false;
                HAL_INFO("Detected MPU9250 on SPI bus 0, select 0\n");
                return true;
            }
        }
        HALClose();
    }

    m_SPISelect = 1;

    if (HALOpen()) {
        if (HALRead(MPU9250_ADDRESS0, MPU9250_WHO_AM_I, 1, &result, "")) {
            if (result == MPU9250_ID) {
                imuType = RTIMU_TYPE_MPU9250;
                slaveAddress = MPU9250_ADDRESS0;
                busIsI2C = false;
                HAL_INFO("Detected MPU9250 on SPI bus 0, select 1\n");
                return true;
            }
        }
        HALClose();
    }

    HAL_ERROR("No IMU detected\n");
    return false;
}

bool RTIMUSettings::discoverPressure(int& pressureType, unsigned char& pressureAddress)
{
    unsigned char result;

    //  auto detect on current bus

    if (HALOpen()) {

        if (HALRead(BMP180_ADDRESS, BMP180_REG_ID, 1, &result, "")) {
            if (result == BMP180_ID) {
                pressureType = RTPRESSURE_TYPE_BMP180;
                pressureAddress = BMP180_ADDRESS;
                HAL_INFO("Detected BMP180\n");
                return true;
            }
        }

        if (HALRead(LPS25H_ADDRESS0, LPS25H_REG_ID, 1, &result, "")) {
            if (result == LPS25H_ID) {
                pressureType = RTPRESSURE_TYPE_LPS25H;
                pressureAddress = LPS25H_ADDRESS0;
                HAL_INFO("Detected LPS25H at standard address\n");
                return true;
            }
        }

        if (HALRead(LPS25H_ADDRESS1, LPS25H_REG_ID, 1, &result, "")) {
            if (result == LPS25H_ID) {
                pressureType = RTPRESSURE_TYPE_LPS25H;
                pressureAddress = LPS25H_ADDRESS1;
                HAL_INFO("Detected LPS25H at option address\n");
                return true;
            }
        }

        // check for MS5611 (which unfortunately has no ID reg)

        if (HALRead(MS5611_ADDRESS0, 0, 1, &result, "")) {
            pressureType = RTPRESSURE_TYPE_MS5611;
            pressureAddress = MS5611_ADDRESS0;
            HAL_INFO("Detected MS5611 at standard address\n");
            return true;
        }
        if (HALRead(MS5611_ADDRESS1, 0, 1, &result, "")) {
            pressureType = RTPRESSURE_TYPE_MS5611;
            pressureAddress = MS5611_ADDRESS1;
            HAL_INFO("Detected MS5611 at option address\n");
            return true;
        }
    }
    HAL_ERROR("No pressure sensor detected\n");
    return false;
}

bool RTIMUSettings::discoverHumidity(int& humidityType, unsigned char& humidityAddress)
{
    unsigned char result;

    //  auto detect on current bus

    if (HALOpen()) {

        if (HALRead(HTS221_ADDRESS, HTS221_REG_ID, 1, &result, "")) {
            if (result == HTS221_ID) {
                humidityType = RTHUMIDITY_TYPE_HTS221;
                humidityAddress = HTS221_ADDRESS;
                HAL_INFO("Detected HTS221 at standard address\n");
                return true;
            }
        }

        if (HALRead(HTU21D_ADDRESS, HTU21D_READ_USER_REG, 1, &result, "")) {
            humidityType = RTHUMIDITY_TYPE_HTU21D;
            humidityAddress = HTU21D_ADDRESS;
            HAL_INFO("Detected HTU21D at standard address\n");
            return true;
        }

    }
    HAL_ERROR("No humidity sensor detected\n");
    return false;
}

void RTIMUSettings::setDefaults()
{
    //  preset general defaults

    m_imuType = RTIMU_TYPE_AUTODISCOVER;
    m_I2CSlaveAddress = 0;
    m_busIsI2C = true;
    m_I2CBus = 1;
    m_SPIBus = 0;
    m_SPISelect = 0;
    m_SPISpeed = 500000;
    m_fusionType = RTFUSION_TYPE_RTQF;
    m_axisRotation = RTIMU_XNORTH_YEAST;
    m_pressureType = RTPRESSURE_TYPE_AUTODISCOVER;
    m_I2CPressureAddress = 0;
    m_humidityType = RTHUMIDITY_TYPE_AUTODISCOVER;
    m_I2CHumidityAddress = 0;
    m_compassCalValid = false;
    m_compassCalEllipsoidValid = false;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            m_compassCalEllipsoidCorr[i][j] = 0;
        }
    }
    m_compassCalEllipsoidCorr[0][0] = 1;
    m_compassCalEllipsoidCorr[1][1] = 1;
    m_compassCalEllipsoidCorr[2][2] = 1;

    m_compassAdjDeclination = 0;

    m_accelCalValid = false;
    m_gyroBiasValid = false;

    //  MPU9150 defaults

    m_MPU9150GyroAccelSampleRate = 50;
    m_MPU9150CompassSampleRate = 25;
    m_MPU9150GyroAccelLpf = MPU9150_LPF_20;
    m_MPU9150GyroFsr = MPU9150_GYROFSR_1000;
    m_MPU9150AccelFsr = MPU9150_ACCELFSR_8;

    //  MPU9250 defaults

    m_MPU9250GyroAccelSampleRate = 80;
    m_MPU9250CompassSampleRate = 40;
    m_MPU9250GyroLpf = MPU9250_GYRO_LPF_41;
    m_MPU9250AccelLpf = MPU9250_ACCEL_LPF_41;
    m_MPU9250GyroFsr = MPU9250_GYROFSR_1000;
    m_MPU9250AccelFsr = MPU9250_ACCELFSR_8;

    //  GD20HM303D defaults

    m_GD20HM303DGyroSampleRate = L3GD20H_SAMPLERATE_50;
    m_GD20HM303DGyroBW = L3GD20H_BANDWIDTH_1;
    m_GD20HM303DGyroHpf = L3GD20H_HPF_4;
    m_GD20HM303DGyroFsr = L3GD20H_FSR_500;

    m_GD20HM303DAccelSampleRate = LSM303D_ACCEL_SAMPLERATE_50;
    m_GD20HM303DAccelFsr = LSM303D_ACCEL_FSR_8;
    m_GD20HM303DAccelLpf = LSM303D_ACCEL_LPF_50;

    m_GD20HM303DCompassSampleRate = LSM303D_COMPASS_SAMPLERATE_50;
    m_GD20HM303DCompassFsr = LSM303D_COMPASS_FSR_2;

    //  GD20M303DLHC defaults

    m_GD20M303DLHCGyroSampleRate = L3GD20_SAMPLERATE_95;
    m_GD20M303DLHCGyroBW = L3GD20_BANDWIDTH_1;
    m_GD20M303DLHCGyroHpf = L3GD20_HPF_4;
    m_GD20M303DLHCGyroFsr = L3GD20H_FSR_500;

    m_GD20M303DLHCAccelSampleRate = LSM303DLHC_ACCEL_SAMPLERATE_50;
    m_GD20M303DLHCAccelFsr = LSM303DLHC_ACCEL_FSR_8;

    m_GD20M303DLHCCompassSampleRate = LSM303DLHC_COMPASS_SAMPLERATE_30;
    m_GD20M303DLHCCompassFsr = LSM303DLHC_COMPASS_FSR_1_3;

    //  GD20HM303DLHC defaults

    m_GD20HM303DLHCGyroSampleRate = L3GD20H_SAMPLERATE_50;
    m_GD20HM303DLHCGyroBW = L3GD20H_BANDWIDTH_1;
    m_GD20HM303DLHCGyroHpf = L3GD20H_HPF_4;
    m_GD20HM303DLHCGyroFsr = L3GD20H_FSR_500;

    m_GD20HM303DLHCAccelSampleRate = LSM303DLHC_ACCEL_SAMPLERATE_50;
    m_GD20HM303DLHCAccelFsr = LSM303DLHC_ACCEL_FSR_8;

    m_GD20HM303DLHCCompassSampleRate = LSM303DLHC_COMPASS_SAMPLERATE_30;
    m_GD20HM303DLHCCompassFsr = LSM303DLHC_COMPASS_FSR_1_3;

    //  LSM9DS0 defaults

    m_LSM9DS0GyroSampleRate = LSM9DS0_GYRO_SAMPLERATE_95;
    m_LSM9DS0GyroBW = LSM9DS0_GYRO_BANDWIDTH_1;
    m_LSM9DS0GyroHpf = LSM9DS0_GYRO_HPF_4;
    m_LSM9DS0GyroFsr = LSM9DS0_GYRO_FSR_500;

    m_LSM9DS0AccelSampleRate = LSM9DS0_ACCEL_SAMPLERATE_50;
    m_LSM9DS0AccelFsr = LSM9DS0_ACCEL_FSR_8;
    m_LSM9DS0AccelLpf = LSM9DS0_ACCEL_LPF_50;

    m_LSM9DS0CompassSampleRate = LSM9DS0_COMPASS_SAMPLERATE_50;
    m_LSM9DS0CompassFsr = LSM9DS0_COMPASS_FSR_2;

    //  LSM9DS1 defaults

    m_LSM9DS1GyroSampleRate = LSM9DS1_GYRO_SAMPLERATE_119;
    m_LSM9DS1GyroBW = LSM9DS1_GYRO_BANDWIDTH_1;
    m_LSM9DS1GyroHpf = LSM9DS1_GYRO_HPF_4;
    m_LSM9DS1GyroFsr = LSM9DS1_GYRO_FSR_500;

    m_LSM9DS1AccelSampleRate = LSM9DS1_ACCEL_SAMPLERATE_119;
    m_LSM9DS1AccelFsr = LSM9DS1_ACCEL_FSR_8;
    m_LSM9DS1AccelLpf = LSM9DS1_ACCEL_LPF_50;

    m_LSM9DS1CompassSampleRate = LSM9DS1_COMPASS_SAMPLERATE_20;
    m_LSM9DS1CompassFsr = LSM9DS1_COMPASS_FSR_4;
    // BMX055 defaults

    m_BMX055GyroSampleRate = BMX055_GYRO_SAMPLERATE_100_32;
    m_BMX055GyroFsr = BMX055_GYRO_FSR_500;

    m_BMX055AccelSampleRate = BMX055_ACCEL_SAMPLERATE_125;
    m_BMX055AccelFsr = BMX055_ACCEL_FSR_8;

    m_BMX055MagPreset = BMX055_MAG_REGULAR;
}

bool RTIMUSettings::loadSettings()
{
    setDefaults();

    char buf[200];
    char key[200];
    char val[200];
    RTFLOAT ftemp;
    //  check to see if settings file exists

    if (!(m_fd = fopen(m_filename, "r"))) {
        HAL_INFO("Settings file not found. Using defaults and creating settings file\n");
        return saveSettings();
    }

    while (fgets(buf, 200, m_fd)) {
        if ((buf[0] == '#') || (buf[0] == ' ') || (buf[0] == '\n'))
            // just a comment
            continue;

        if (sscanf(buf, "%[^=]=%s", key, val) != 2) {
            HAL_ERROR1("Bad line in settings file: %s\n", buf);
            fclose(m_fd);
            return false;
        }

        //  now decode keys

        //  general config

        if (strcmp(key, RTIMULIB_IMU_TYPE) == 0) {
            m_imuType = atoi(val);
        } else if (strcmp(key, RTIMULIB_FUSION_TYPE) == 0) {
            m_fusionType = atoi(val);
        } else if (strcmp(key, RTIMULIB_BUS_IS_I2C) == 0) {
            m_busIsI2C = strcmp(val, "true") == 0;
        } else if (strcmp(key, RTIMULIB_I2C_BUS) == 0) {
            m_I2CBus = atoi(val);
        } else if (strcmp(key, RTIMULIB_SPI_BUS) == 0) {
            m_SPIBus = atoi(val);
        } else if (strcmp(key, RTIMULIB_SPI_SELECT) == 0) {
            m_SPISelect = atoi(val);
        } else if (strcmp(key, RTIMULIB_SPI_SPEED) == 0) {
            m_SPISpeed = atoi(val);
        } else if (strcmp(key, RTIMULIB_I2C_SLAVEADDRESS) == 0) {
            m_I2CSlaveAddress = atoi(val);
        } else if (strcmp(key, RTIMULIB_AXIS_ROTATION) == 0) {
            m_axisRotation = atoi(val);
        } else if (strcmp(key, RTIMULIB_PRESSURE_TYPE) == 0) {
            m_pressureType = atoi(val);
        } else if (strcmp(key, RTIMULIB_I2C_PRESSUREADDRESS) == 0) {
            m_I2CPressureAddress = atoi(val);
        } else if (strcmp(key, RTIMULIB_HUMIDITY_TYPE) == 0) {
            m_humidityType = atoi(val);
        } else if (strcmp(key, RTIMULIB_I2C_HUMIDITYADDRESS) == 0) {
            m_I2CHumidityAddress = atoi(val);

        // compass calibration and adjustment

        } else if (strcmp(key, RTIMULIB_COMPASSCAL_VALID) == 0) {
            m_compassCalValid = strcmp(val, "true") == 0;
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_MINX) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalMin.setX(ftemp);
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_MINY) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalMin.setY(ftemp);
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_MINZ) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalMin.setZ(ftemp);
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_MAXX) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalMax.setX(ftemp);
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_MAXY) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalMax.setY(ftemp);
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_MAXZ) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalMax.setZ(ftemp);
        } else if (strcmp(key, RTIMULIB_COMPASSADJ_DECLINATION) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassAdjDeclination = ftemp;

        // compass ellipsoid calibration

        } else if (strcmp(key, RTIMULIB_COMPASSCAL_ELLIPSOID_VALID) == 0) {
            m_compassCalEllipsoidValid = strcmp(val, "true") == 0;
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_OFFSET_X) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidOffset.setX(ftemp);
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_OFFSET_Y) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidOffset.setY(ftemp);
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_OFFSET_Z) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidOffset.setZ(ftemp);
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_CORR11) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidCorr[0][0] = ftemp;
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_CORR12) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidCorr[0][1] = ftemp;
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_CORR13) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidCorr[0][2] = ftemp;
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_CORR21) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidCorr[1][0] = ftemp;
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_CORR22) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidCorr[1][1] = ftemp;
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_CORR23) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidCorr[1][2] = ftemp;
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_CORR31) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidCorr[2][0] = ftemp;
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_CORR32) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidCorr[2][1] = ftemp;
        } else if (strcmp(key, RTIMULIB_COMPASSCAL_CORR33) == 0) {
            sscanf(val, "%f", &ftemp);
            m_compassCalEllipsoidCorr[2][2] = ftemp;

        // accel calibration

        } else if (strcmp(key, RTIMULIB_ACCELCAL_VALID) == 0) {
            m_accelCalValid = strcmp(val, "true") == 0;
        } else if (strcmp(key, RTIMULIB_ACCELCAL_MINX) == 0) {
            sscanf(val, "%f", &ftemp);
            m_accelCalMin.setX(ftemp);
        } else if (strcmp(key, RTIMULIB_ACCELCAL_MINY) == 0) {
            sscanf(val, "%f", &ftemp);
            m_accelCalMin.setY(ftemp);
        } else if (strcmp(key, RTIMULIB_ACCELCAL_MINZ) == 0) {
            sscanf(val, "%f", &ftemp);
            m_accelCalMin.setZ(ftemp);
        } else if (strcmp(key, RTIMULIB_ACCELCAL_MAXX) == 0) {
            sscanf(val, "%f", &ftemp);
            m_accelCalMax.setX(ftemp);
        } else if (strcmp(key, RTIMULIB_ACCELCAL_MAXY) == 0) {
            sscanf(val, "%f", &ftemp);
            m_accelCalMax.setY(ftemp);
        } else if (strcmp(key, RTIMULIB_ACCELCAL_MAXZ) == 0) {
            sscanf(val, "%f", &ftemp);
            m_accelCalMax.setZ(ftemp);

            // gyro bias

        } else if (strcmp(key, RTIMULIB_GYRO_BIAS_VALID) == 0) {
            m_gyroBiasValid = strcmp(val, "true") == 0;
        } else if (strcmp(key, RTIMULIB_GYRO_BIAS_X) == 0) {
            sscanf(val, "%f", &ftemp);
            m_gyroBias.setX(ftemp);
        } else if (strcmp(key, RTIMULIB_GYRO_BIAS_Y) == 0) {
            sscanf(val, "%f", &ftemp);
            m_gyroBias.setY(ftemp);
        } else if (strcmp(key, RTIMULIB_GYRO_BIAS_Z) == 0) {
            sscanf(val, "%f", &ftemp);
            m_gyroBias.setZ(ftemp);

        //  MPU9150 settings

        } else if (strcmp(key, RTIMULIB_MPU9150_GYROACCEL_SAMPLERATE) == 0) {
            m_MPU9150GyroAccelSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_MPU9150_COMPASS_SAMPLERATE) == 0) {
            m_MPU9150CompassSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_MPU9150_GYROACCEL_LPF) == 0) {
            m_MPU9150GyroAccelLpf = atoi(val);
        } else if (strcmp(key, RTIMULIB_MPU9150_GYRO_FSR) == 0) {
            m_MPU9150GyroFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_MPU9150_ACCEL_FSR) == 0) {
            m_MPU9150AccelFsr = atoi(val);

        //  MPU9250 settings

        } else if (strcmp(key, RTIMULIB_MPU9250_GYROACCEL_SAMPLERATE) == 0) {
            m_MPU9250GyroAccelSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_MPU9250_COMPASS_SAMPLERATE) == 0) {
            m_MPU9250CompassSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_MPU9250_GYRO_LPF) == 0) {
            m_MPU9250GyroLpf = atoi(val);
        } else if (strcmp(key, RTIMULIB_MPU9250_ACCEL_LPF) == 0) {
            m_MPU9250AccelLpf = atoi(val);
        } else if (strcmp(key, RTIMULIB_MPU9250_GYRO_FSR) == 0) {
            m_MPU9250GyroFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_MPU9250_ACCEL_FSR) == 0) {
            m_MPU9250AccelFsr = atoi(val);

        //  GD20HM303D settings

        } else if (strcmp(key, RTIMULIB_GD20HM303D_GYRO_SAMPLERATE) == 0) {
            m_GD20HM303DGyroSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303D_GYRO_FSR) == 0) {
            m_GD20HM303DGyroFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303D_GYRO_HPF) == 0) {
            m_GD20HM303DGyroHpf = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303D_GYRO_BW) == 0) {
            m_GD20HM303DGyroBW = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303D_ACCEL_SAMPLERATE) == 0) {
            m_GD20HM303DAccelSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303D_ACCEL_FSR) == 0) {
            m_GD20HM303DAccelFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303D_ACCEL_LPF) == 0) {
            m_GD20HM303DAccelLpf = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303D_COMPASS_SAMPLERATE) == 0) {
            m_GD20HM303DCompassSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303D_COMPASS_FSR) == 0) {
            m_GD20HM303DCompassFsr = atoi(val);

        //  GD20M303DLHC settings

        } else if (strcmp(key, RTIMULIB_GD20M303DLHC_GYRO_SAMPLERATE) == 0) {
            m_GD20M303DLHCGyroSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20M303DLHC_GYRO_FSR) == 0) {
            m_GD20M303DLHCGyroFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20M303DLHC_GYRO_HPF) == 0) {
            m_GD20M303DLHCGyroHpf = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20M303DLHC_GYRO_BW) == 0) {
            m_GD20M303DLHCGyroBW = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20M303DLHC_ACCEL_SAMPLERATE) == 0) {
            m_GD20M303DLHCAccelSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20M303DLHC_ACCEL_FSR) == 0) {
            m_GD20M303DLHCAccelFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20M303DLHC_COMPASS_SAMPLERATE) == 0) {
            m_GD20M303DLHCCompassSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20M303DLHC_COMPASS_FSR) == 0) {
            m_GD20M303DLHCCompassFsr = atoi(val);

        //  GD20HM303DLHC settings

         } else if (strcmp(key, RTIMULIB_GD20HM303DLHC_GYRO_SAMPLERATE) == 0) {
            m_GD20HM303DLHCGyroSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303DLHC_GYRO_FSR) == 0) {
            m_GD20HM303DLHCGyroFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303DLHC_GYRO_HPF) == 0) {
            m_GD20HM303DLHCGyroHpf = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303DLHC_GYRO_BW) == 0) {
            m_GD20HM303DLHCGyroBW = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303DLHC_ACCEL_SAMPLERATE) == 0) {
            m_GD20HM303DLHCAccelSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303DLHC_ACCEL_FSR) == 0) {
            m_GD20HM303DLHCAccelFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303DLHC_COMPASS_SAMPLERATE) == 0) {
            m_GD20HM303DLHCCompassSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_GD20HM303DLHC_COMPASS_FSR) == 0) {
            m_GD20HM303DLHCCompassFsr = atoi(val);

        //  LSM9DS0 settings

        } else if (strcmp(key, RTIMULIB_LSM9DS0_GYRO_SAMPLERATE) == 0) {
            m_LSM9DS0GyroSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS0_GYRO_FSR) == 0) {
            m_LSM9DS0GyroFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS0_GYRO_HPF) == 0) {
            m_LSM9DS0GyroHpf = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS0_GYRO_BW) == 0) {
            m_LSM9DS0GyroBW = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS0_ACCEL_SAMPLERATE) == 0) {
            m_LSM9DS0AccelSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS0_ACCEL_FSR) == 0) {
            m_LSM9DS0AccelFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS0_ACCEL_LPF) == 0) {
            m_LSM9DS0AccelLpf = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS0_COMPASS_SAMPLERATE) == 0) {
            m_LSM9DS0CompassSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS0_COMPASS_FSR) == 0) {
            m_LSM9DS0CompassFsr = atoi(val);

        //  LSM9DS1 settings

        } else if (strcmp(key, RTIMULIB_LSM9DS1_GYRO_SAMPLERATE) == 0) {
            m_LSM9DS1GyroSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS1_GYRO_FSR) == 0) {
            m_LSM9DS1GyroFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS1_GYRO_HPF) == 0) {
            m_LSM9DS1GyroHpf = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS1_GYRO_BW) == 0) {
            m_LSM9DS1GyroBW = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS1_ACCEL_SAMPLERATE) == 0) {
            m_LSM9DS1AccelSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS1_ACCEL_FSR) == 0) {
            m_LSM9DS1AccelFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS1_ACCEL_LPF) == 0) {
            m_LSM9DS1AccelLpf = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS1_COMPASS_SAMPLERATE) == 0) {
            m_LSM9DS1CompassSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_LSM9DS1_COMPASS_FSR) == 0) {
            m_LSM9DS1CompassFsr = atoi(val);

        //  BMX055 settings

        } else if (strcmp(key, RTIMULIB_BMX055_GYRO_SAMPLERATE) == 0) {
            m_BMX055GyroSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_BMX055_GYRO_FSR) == 0) {
            m_BMX055GyroFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_BMX055_ACCEL_SAMPLERATE) == 0) {
            m_BMX055AccelSampleRate = atoi(val);
        } else if (strcmp(key, RTIMULIB_BMX055_ACCEL_FSR) == 0) {
            m_BMX055AccelFsr = atoi(val);
        } else if (strcmp(key, RTIMULIB_BMX055_MAG_PRESET) == 0) {
            m_BMX055MagPreset = atoi(val);

        //  Handle unrecognized key

        } else {
            HAL_ERROR1("Unrecognized key in settings file: %s\n", buf);
        }
    }
    HAL_INFO1("Settings file %s loaded\n", m_filename);
    fclose(m_fd);
    return saveSettings();                                  // make sure settings file is correct and complete
}

bool RTIMUSettings::saveSettings()
{
    if (!(m_fd = fopen(m_filename, "w"))) {
        HAL_ERROR("Failed to open settings file for save");
        return false;
    }

    //  General settings

    setComment("#####################################################################");
    setComment("");
    setComment("RTIMULib settings file");
    setBlank();
    setComment("General settings");
    setComment("");

    setBlank();
    setComment("IMU type - ");
    setComment("  0 = Auto discover");
    setComment("  1 = Null (used when data is provided from a remote IMU");
    setComment("  2 = InvenSense MPU-9150");
    setComment("  3 = STM L3GD20H + LSM303D");
    setComment("  4 = STM L3GD20 + LSM303DLHC");
    setComment("  5 = STM LSM9DS0");
    setComment("  6 = STM LSM9DS1");
    setComment("  7 = InvenSense MPU-9250");
    setComment("  8 = STM L3GD20H + LSM303DLHC");
    setComment("  9 = Bosch BMX055");
    setComment("  10 = Bosch BNX055");
    setValue(RTIMULIB_IMU_TYPE, m_imuType);

    setBlank();
    setComment("");
    setComment("Fusion type type - ");
    setComment("  0 - Null. Use if only sensor data required without fusion");
    setComment("  1 - Kalman STATE4");
    setComment("  2 - RTQF");
    setValue(RTIMULIB_FUSION_TYPE, m_fusionType);

    setBlank();
    setComment("");
    setComment("Is bus I2C: 'true' for I2C, 'false' for SPI");
    setValue(RTIMULIB_BUS_IS_I2C, m_busIsI2C);

    setBlank();
    setComment("");
    setComment("I2C Bus (between 0 and 7) ");
    setValue(RTIMULIB_I2C_BUS, m_I2CBus);

    setBlank();
    setComment("");
    setComment("SPI Bus (between 0 and 7) ");
    setValue(RTIMULIB_SPI_BUS, m_SPIBus);

    setBlank();
    setComment("");
    setComment("SPI select (between 0 and 1) ");
    setValue(RTIMULIB_SPI_SELECT, m_SPISelect);

    setBlank();
    setComment("");
    setComment("SPI Speed in Hz");
    setValue(RTIMULIB_SPI_SPEED, (int)m_SPISpeed);

    setBlank();
    setComment("");
    setComment("I2C slave address (filled in automatically by auto discover) ");
    setValue(RTIMULIB_I2C_SLAVEADDRESS, m_I2CSlaveAddress);

    setBlank();
    setComment("");
    setComment("IMU axis rotation - see RTIMU.h for details");
    setValue(RTIMULIB_AXIS_ROTATION, m_axisRotation);

    setBlank();
    setComment("Pressure sensor type - ");
    setComment("  0 = Auto discover");
    setComment("  1 = Null (no hardware or don't use)");
    setComment("  2 = BMP180");
    setComment("  3 = LPS25H");
    setComment("  4 = MS5611");
    setComment("  5 = MS5637");

    setValue(RTIMULIB_PRESSURE_TYPE, m_pressureType);

    setBlank();
    setComment("");
    setComment("I2C pressure sensor address (filled in automatically by auto discover) ");
    setValue(RTIMULIB_I2C_PRESSUREADDRESS, m_I2CPressureAddress);

    setBlank();
    setComment("Humidity sensor type - ");
    setComment("  0 = Auto discover");
    setComment("  1 = Null (no hardware or don't use)");
    setComment("  2 = HTS221");
    setComment("  3 = HTU21D");

    setValue(RTIMULIB_HUMIDITY_TYPE, m_humidityType);

    setBlank();
    setComment("");
    setComment("I2C humidity sensor address (filled in automatically by auto discover) ");
    setValue(RTIMULIB_I2C_HUMIDITYADDRESS, m_I2CHumidityAddress);

    //  Compass settings

    setBlank();
    setComment("#####################################################################");
    setComment("");

    setBlank();
    setComment("Compass calibration settings");
    setValue(RTIMULIB_COMPASSCAL_VALID, m_compassCalValid);
    setValue(RTIMULIB_COMPASSCAL_MINX, m_compassCalMin.x());
    setValue(RTIMULIB_COMPASSCAL_MINY, m_compassCalMin.y());
    setValue(RTIMULIB_COMPASSCAL_MINZ, m_compassCalMin.z());
    setValue(RTIMULIB_COMPASSCAL_MAXX, m_compassCalMax.x());
    setValue(RTIMULIB_COMPASSCAL_MAXY, m_compassCalMax.y());
    setValue(RTIMULIB_COMPASSCAL_MAXZ, m_compassCalMax.z());

    setBlank();
    setComment("#####################################################################");
    setComment("");

    setBlank();
    setComment("Compass adjustment settings");
    setComment("Compass declination is in radians and is subtracted from calculated heading");
    setValue(RTIMULIB_COMPASSADJ_DECLINATION, m_compassAdjDeclination);

    //  Compass ellipsoid calibration settings

    setBlank();
    setComment("#####################################################################");
    setComment("");

    setBlank();
    setComment("Compass ellipsoid calibration");
    setValue(RTIMULIB_COMPASSCAL_ELLIPSOID_VALID, m_compassCalEllipsoidValid);
    setValue(RTIMULIB_COMPASSCAL_OFFSET_X, m_compassCalEllipsoidOffset.x());
    setValue(RTIMULIB_COMPASSCAL_OFFSET_Y, m_compassCalEllipsoidOffset.y());
    setValue(RTIMULIB_COMPASSCAL_OFFSET_Z, m_compassCalEllipsoidOffset.z());
    setValue(RTIMULIB_COMPASSCAL_CORR11, m_compassCalEllipsoidCorr[0][0]);
    setValue(RTIMULIB_COMPASSCAL_CORR12, m_compassCalEllipsoidCorr[0][1]);
    setValue(RTIMULIB_COMPASSCAL_CORR13, m_compassCalEllipsoidCorr[0][2]);
    setValue(RTIMULIB_COMPASSCAL_CORR21, m_compassCalEllipsoidCorr[1][0]);
    setValue(RTIMULIB_COMPASSCAL_CORR22, m_compassCalEllipsoidCorr[1][1]);
    setValue(RTIMULIB_COMPASSCAL_CORR23, m_compassCalEllipsoidCorr[1][2]);
    setValue(RTIMULIB_COMPASSCAL_CORR31, m_compassCalEllipsoidCorr[2][0]);
    setValue(RTIMULIB_COMPASSCAL_CORR32, m_compassCalEllipsoidCorr[2][1]);
    setValue(RTIMULIB_COMPASSCAL_CORR33, m_compassCalEllipsoidCorr[2][2]);

    //  Accel calibration settings

    setBlank();
    setComment("#####################################################################");
    setComment("");

    setBlank();
    setComment("Accel calibration");
    setValue(RTIMULIB_ACCELCAL_VALID, m_accelCalValid);
    setValue(RTIMULIB_ACCELCAL_MINX, m_accelCalMin.x());
    setValue(RTIMULIB_ACCELCAL_MINY, m_accelCalMin.y());
    setValue(RTIMULIB_ACCELCAL_MINZ, m_accelCalMin.z());
    setValue(RTIMULIB_ACCELCAL_MAXX, m_accelCalMax.x());
    setValue(RTIMULIB_ACCELCAL_MAXY, m_accelCalMax.y());
    setValue(RTIMULIB_ACCELCAL_MAXZ, m_accelCalMax.z());

    //  Gyro bias settings

    setBlank();
    setComment("#####################################################################");
    setComment("");

    setBlank();
    setComment("Saved gyro bias data");
    setValue(RTIMULIB_GYRO_BIAS_VALID, m_gyroBiasValid);
    setValue(RTIMULIB_GYRO_BIAS_X, m_gyroBias.x());
    setValue(RTIMULIB_GYRO_BIAS_Y, m_gyroBias.y());
    setValue(RTIMULIB_GYRO_BIAS_Z, m_gyroBias.z());

    //  MPU-9150 settings

    setBlank();
    setComment("#####################################################################");
    setComment("");
    setComment("MPU-9150 settings");
    setComment("");

    setBlank();
    setComment("Gyro sample rate (between 5Hz and 1000Hz) ");
    setValue(RTIMULIB_MPU9150_GYROACCEL_SAMPLERATE, m_MPU9150GyroAccelSampleRate);

    setBlank();
    setComment("");
    setComment("Compass sample rate (between 1Hz and 100Hz) ");
    setValue(RTIMULIB_MPU9150_COMPASS_SAMPLERATE, m_MPU9150CompassSampleRate);

    setBlank();
    setComment("");
    setComment("Gyro/accel low pass filter - ");
    setComment("  0 - gyro: 256Hz, accel: 260Hz");
    setComment("  1 - gyro: 188Hz, accel: 184Hz");
    setComment("  2 - gyro: 98Hz, accel: 98Hz");
    setComment("  3 - gyro: 42Hz, accel: 44Hz");
    setComment("  4 - gyro: 20Hz, accel: 21Hz");
    setComment("  5 - gyro: 10Hz, accel: 10Hz");
    setComment("  6 - gyro: 5Hz, accel: 5Hz");
    setValue(RTIMULIB_MPU9150_GYROACCEL_LPF, m_MPU9150GyroAccelLpf);

    setBlank();
    setComment("");
    setComment("Gyro full scale range - ");
    setComment("  0  - +/- 250 degress per second");
    setComment("  8  - +/- 500 degress per second");
    setComment("  16 - +/- 1000 degress per second");
    setComment("  24 - +/- 2000 degress per second");
    setValue(RTIMULIB_MPU9150_GYRO_FSR, m_MPU9150GyroFsr);

    setBlank();
    setComment("");
    setComment("Accel full scale range - ");
    setComment("  0  - +/- 2g");
    setComment("  8  - +/- 4g");
    setComment("  16 - +/- 8g");
    setComment("  24 - +/- 16g");
    setValue(RTIMULIB_MPU9150_ACCEL_FSR, m_MPU9150AccelFsr);

    //  MPU-9250 settings

    setBlank();
    setComment("#####################################################################");
    setComment("");
    setComment("MPU-9250 settings");
    setComment("");

    setBlank();
    setComment("Gyro sample rate (between 5Hz and 1000Hz plus 8000Hz and 32000Hz) ");
    setValue(RTIMULIB_MPU9250_GYROACCEL_SAMPLERATE, m_MPU9250GyroAccelSampleRate);

    setBlank();
    setComment("");
    setComment("Compass sample rate (between 1Hz and 100Hz) ");
    setValue(RTIMULIB_MPU9250_COMPASS_SAMPLERATE, m_MPU9250CompassSampleRate);

    setBlank();
    setComment("");
    setComment("Gyro low pass filter - ");
    setComment("  0x11 - 8800Hz, 0.64mS delay");
    setComment("  0x10 - 3600Hz, 0.11mS delay");
    setComment("  0x00 - 250Hz, 0.97mS delay");
    setComment("  0x01 - 184Hz, 2.9mS delay");
    setComment("  0x02 - 92Hz, 3.9mS delay");
    setComment("  0x03 - 41Hz, 5.9mS delay");
    setComment("  0x04 - 20Hz, 9.9mS delay");
    setComment("  0x05 - 10Hz, 17.85mS delay");
    setComment("  0x06 - 5Hz, 33.48mS delay");
    setValue(RTIMULIB_MPU9250_GYRO_LPF, m_MPU9250GyroLpf);

    setBlank();
    setComment("");
    setComment("Accel low pass filter - ");
    setComment("  0x08 - 1130Hz, 0.75mS delay");
    setComment("  0x00 - 460Hz, 1.94mS delay");
    setComment("  0x01 - 184Hz, 5.80mS delay");
    setComment("  0x02 - 92Hz, 7.80mS delay");
    setComment("  0x03 - 41Hz, 11.80mS delay");
    setComment("  0x04 - 20Hz, 19.80mS delay");
    setComment("  0x05 - 10Hz, 35.70mS delay");
    setComment("  0x06 - 5Hz, 66.96mS delay");
    setValue(RTIMULIB_MPU9250_ACCEL_LPF, m_MPU9250AccelLpf);

    setBlank();
    setComment("");
    setComment("Gyro full scale range - ");
    setComment("  0  - +/- 250 degress per second");
    setComment("  8  - +/- 500 degress per second");
    setComment("  16 - +/- 1000 degress per second");
    setComment("  24 - +/- 2000 degress per second");
    setValue(RTIMULIB_MPU9250_GYRO_FSR, m_MPU9250GyroFsr);

    setBlank();
    setComment("");
    setComment("Accel full scale range - ");
    setComment("  0  - +/- 2g");
    setComment("  8  - +/- 4g");
    setComment("  16 - +/- 8g");
    setComment("  24 - +/- 16g");
    setValue(RTIMULIB_MPU9250_ACCEL_FSR, m_MPU9250AccelFsr);

    //  GD20HM303D settings

    setBlank();
    setComment("#####################################################################");
    setComment("");
    setComment("L3GD20H + LSM303D settings");

    setBlank();
    setComment("");
    setComment("Gyro sample rate - ");
    setComment("  0 = 12.5Hz ");
    setComment("  1 = 25Hz ");
    setComment("  2 = 50Hz ");
    setComment("  3 = 100Hz ");
    setComment("  4 = 200Hz ");
    setComment("  5 = 400Hz ");
    setComment("  6 = 800Hz ");
    setValue(RTIMULIB_GD20HM303D_GYRO_SAMPLERATE, m_GD20HM303DGyroSampleRate);

    setBlank();
    setComment("");
    setComment("Gyro full scale range - ");
    setComment("  0 = 245 degrees per second ");
    setComment("  1 = 500 degrees per second ");
    setComment("  2 = 2000 degrees per second ");
    setValue(RTIMULIB_GD20HM303D_GYRO_FSR, m_GD20HM303DGyroFsr);

    setBlank();
    setComment("");
    setComment("Gyro high pass filter - ");
    setComment("  0 - 9 but see the L3GD20H manual for details");
    setValue(RTIMULIB_GD20HM303D_GYRO_HPF, m_GD20HM303DGyroHpf);

    setBlank();
    setComment("");
    setComment("Gyro bandwidth - ");
    setComment("  0 - 3 but see the L3GD20H manual for details");
    setValue(RTIMULIB_GD20HM303D_GYRO_BW, m_GD20HM303DGyroBW);

    setBlank();
    setComment("Accel sample rate - ");
    setComment("  1 = 3.125Hz ");
    setComment("  2 = 6.25Hz ");
    setComment("  3 = 12.5Hz ");
    setComment("  4 = 25Hz ");
    setComment("  5 = 50Hz ");
    setComment("  6 = 100Hz ");
    setComment("  7 = 200Hz ");
    setComment("  8 = 400Hz ");
    setComment("  9 = 800Hz ");
    setComment("  10 = 1600Hz ");
    setValue(RTIMULIB_GD20HM303D_ACCEL_SAMPLERATE, m_GD20HM303DAccelSampleRate);

    setBlank();
    setComment("");
    setComment("Accel full scale range - ");
    setComment("  0 = +/- 2g ");
    setComment("  1 = +/- 4g ");
    setComment("  2 = +/- 6g ");
    setComment("  3 = +/- 8g ");
    setComment("  4 = +/- 16g ");
    setValue(RTIMULIB_GD20HM303D_ACCEL_FSR, m_GD20HM303DAccelFsr);

    setBlank();
    setComment("");
    setComment("Accel low pass filter - ");
    setComment("  0 = 773Hz");
    setComment("  1 = 194Hz");
    setComment("  2 = 362Hz");
    setComment("  3 = 50Hz");
    setValue(RTIMULIB_GD20HM303D_ACCEL_LPF, m_GD20HM303DAccelLpf);

    setBlank();
    setComment("");
    setComment("Compass sample rate - ");
    setComment("  0 = 3.125Hz ");
    setComment("  1 = 6.25Hz ");
    setComment("  2 = 12.5Hz ");
    setComment("  3 = 25Hz ");
    setComment("  4 = 50Hz ");
    setComment("  5 = 100Hz ");
    setValue(RTIMULIB_GD20HM303D_COMPASS_SAMPLERATE, m_GD20HM303DCompassSampleRate);


    setBlank();
    setComment("");
    setComment("Compass full scale range - ");
    setComment("  0 = +/- 200 uT ");
    setComment("  1 = +/- 400 uT ");
    setComment("  2 = +/- 800 uT ");
    setComment("  3 = +/- 1200 uT ");
    setValue(RTIMULIB_GD20HM303D_COMPASS_FSR, m_GD20HM303DCompassFsr);

    //  GD20M303DLHC settings

    setBlank();
    setComment("#####################################################################");
    setComment("");
    setComment("L3GD20 + LSM303DLHC settings");
    setComment("");

    setBlank();
    setComment("Gyro sample rate - ");
    setComment("  0 = 95z ");
    setComment("  1 = 190Hz ");
    setComment("  2 = 380Hz ");
    setComment("  3 = 760Hz ");
    setValue(RTIMULIB_GD20M303DLHC_GYRO_SAMPLERATE, m_GD20M303DLHCGyroSampleRate);

    setBlank();
    setComment("");
    setComment("Gyro full scale range - ");
    setComment("  0 = 250 degrees per second ");
    setComment("  1 = 500 degrees per second ");
    setComment("  2 = 2000 degrees per second ");
    setValue(RTIMULIB_GD20M303DLHC_GYRO_FSR, m_GD20M303DLHCGyroFsr);

    setBlank();
    setComment("");
    setComment("Gyro high pass filter - ");
    setComment("  0 - 9 but see the L3GD20 manual for details");
    setValue(RTIMULIB_GD20M303DLHC_GYRO_HPF, m_GD20M303DLHCGyroHpf);

    setBlank();
    setComment("");
    setComment("Gyro bandwidth - ");
    setComment("  0 - 3 but see the L3GD20 manual for details");
    setValue(RTIMULIB_GD20M303DLHC_GYRO_BW, m_GD20M303DLHCGyroBW);

    setBlank();
    setComment("Accel sample rate - ");
    setComment("  1 = 1Hz ");
    setComment("  2 = 10Hz ");
    setComment("  3 = 25Hz ");
    setComment("  4 = 50Hz ");
    setComment("  5 = 100Hz ");
    setComment("  6 = 200Hz ");
    setComment("  7 = 400Hz ");
    setValue(RTIMULIB_GD20M303DLHC_ACCEL_SAMPLERATE, m_GD20M303DLHCAccelSampleRate);

    setBlank();
    setComment("");
    setComment("Accel full scale range - ");
    setComment("  0 = +/- 2g ");
    setComment("  1 = +/- 4g ");
    setComment("  2 = +/- 8g ");
    setComment("  3 = +/- 16g ");
    setValue(RTIMULIB_GD20M303DLHC_ACCEL_FSR, m_GD20M303DLHCAccelFsr);

    setBlank();
    setComment("");
    setComment("Compass sample rate - ");
    setComment("  0 = 0.75Hz ");
    setComment("  1 = 1.5Hz ");
    setComment("  2 = 3Hz ");
    setComment("  3 = 7.5Hz ");
    setComment("  4 = 15Hz ");
    setComment("  5 = 30Hz ");
    setComment("  6 = 75Hz ");
    setComment("  7 = 220Hz ");
    setValue(RTIMULIB_GD20M303DLHC_COMPASS_SAMPLERATE, m_GD20M303DLHCCompassSampleRate);

    setBlank();
    setComment("");
    setComment("Compass full scale range - ");
    setComment("  1 = +/- 130 uT ");
    setComment("  2 = +/- 190 uT ");
    setComment("  3 = +/- 250 uT ");
    setComment("  4 = +/- 400 uT ");
    setComment("  5 = +/- 470 uT ");
    setComment("  6 = +/- 560 uT ");
    setComment("  7 = +/- 810 uT ");
    setValue(RTIMULIB_GD20M303DLHC_COMPASS_FSR, m_GD20M303DLHCCompassFsr);

    //  GD20HM303DLHC settings

    setBlank();
    setComment("#####################################################################");
    setComment("");
    setComment("L3GD20H + LSM303DLHC settings");
    setComment("");

    setBlank();
    setComment("");
    setComment("Gyro sample rate - ");
    setComment("  0 = 12.5Hz ");
    setComment("  1 = 25Hz ");
    setComment("  2 = 50Hz ");
    setComment("  3 = 100Hz ");
    setComment("  4 = 200Hz ");
    setComment("  5 = 400Hz ");
    setComment("  6 = 800Hz ");
    setValue(RTIMULIB_GD20HM303DLHC_GYRO_SAMPLERATE, m_GD20HM303DLHCGyroSampleRate);

    setBlank();
    setComment("");
    setComment("Gyro full scale range - ");
    setComment("  0 = 245 degrees per second ");
    setComment("  1 = 500 degrees per second ");
    setComment("  2 = 2000 degrees per second ");
    setValue(RTIMULIB_GD20HM303DLHC_GYRO_FSR, m_GD20HM303DLHCGyroFsr);

    setBlank();
    setComment("");
    setComment("Gyro high pass filter - ");
    setComment("  0 - 9 but see the L3GD20H manual for details");
    setValue(RTIMULIB_GD20HM303DLHC_GYRO_HPF, m_GD20HM303DLHCGyroHpf);

    setBlank();
    setComment("");
    setComment("Gyro bandwidth - ");
    setComment("  0 - 3 but see the L3GD20H manual for details");
    setValue(RTIMULIB_GD20HM303DLHC_GYRO_BW, m_GD20HM303DLHCGyroBW);
    setBlank();
    setComment("Accel sample rate - ");
    setComment("  1 = 1Hz ");
    setComment("  2 = 10Hz ");
    setComment("  3 = 25Hz ");
    setComment("  4 = 50Hz ");
    setComment("  5 = 100Hz ");
    setComment("  6 = 200Hz ");
    setComment("  7 = 400Hz ");
    setValue(RTIMULIB_GD20HM303DLHC_ACCEL_SAMPLERATE, m_GD20HM303DLHCAccelSampleRate);

    setBlank();
    setComment("");
    setComment("Accel full scale range - ");
    setComment("  0 = +/- 2g ");
    setComment("  1 = +/- 4g ");
    setComment("  2 = +/- 8g ");
    setComment("  3 = +/- 16g ");
    setValue(RTIMULIB_GD20HM303DLHC_ACCEL_FSR, m_GD20HM303DLHCAccelFsr);

    setBlank();
    setComment("");
    setComment("Compass sample rate - ");
    setComment("  0 = 0.75Hz ");
    setComment("  1 = 1.5Hz ");
    setComment("  2 = 3Hz ");
    setComment("  3 = 7.5Hz ");
    setComment("  4 = 15Hz ");
    setComment("  5 = 30Hz ");
    setComment("  6 = 75Hz ");
    setComment("  7 = 220Hz ");
    setValue(RTIMULIB_GD20HM303DLHC_COMPASS_SAMPLERATE, m_GD20HM303DLHCCompassSampleRate);


    setBlank();
    setComment("");
    setComment("Compass full scale range - ");
    setComment("  1 = +/- 130 uT ");
    setComment("  2 = +/- 190 uT ");
    setComment("  3 = +/- 250 uT ");
    setComment("  4 = +/- 400 uT ");
    setComment("  5 = +/- 470 uT ");
    setComment("  6 = +/- 560 uT ");
    setComment("  7 = +/- 810 uT ");
    setValue(RTIMULIB_GD20HM303DLHC_COMPASS_FSR, m_GD20HM303DLHCCompassFsr);

    //  LSM9DS0 settings

    setBlank();
    setComment("#####################################################################");
    setComment("");
    setComment("LSM9DS0 settings");
    setComment("");

    setBlank();
    setComment("Gyro sample rate - ");
    setComment("  0 = 95z ");
    setComment("  1 = 190Hz ");
    setComment("  2 = 380Hz ");
    setComment("  3 = 760Hz ");
    setValue(RTIMULIB_LSM9DS0_GYRO_SAMPLERATE, m_LSM9DS0GyroSampleRate);

    setBlank();
    setComment("");
    setComment("Gyro full scale range - ");
    setComment("  0 = 250 degrees per second ");
    setComment("  1 = 500 degrees per second ");
    setComment("  2 = 2000 degrees per second ");
    setValue(RTIMULIB_LSM9DS0_GYRO_FSR, m_LSM9DS0GyroFsr);

    setBlank();
    setComment("");
    setComment("Gyro high pass filter - ");
    setComment("  0 - 9 but see the LSM9DS0 manual for details");
    setValue(RTIMULIB_LSM9DS0_GYRO_HPF, m_LSM9DS0GyroHpf);

    setBlank();
    setComment("");
    setComment("Gyro bandwidth - ");
    setComment("  0 - 3 but see the LSM9DS0 manual for details");
    setValue(RTIMULIB_LSM9DS0_GYRO_BW, m_LSM9DS0GyroBW);

    setBlank();
    setComment("Accel sample rate - ");
    setComment("  1 = 3.125Hz ");
    setComment("  2 = 6.25Hz ");
    setComment("  3 = 12.5Hz ");
    setComment("  4 = 25Hz ");
    setComment("  5 = 50Hz ");
    setComment("  6 = 100Hz ");
    setComment("  7 = 200Hz ");
    setComment("  8 = 400Hz ");
    setComment("  9 = 800Hz ");
    setComment("  10 = 1600Hz ");
    setValue(RTIMULIB_LSM9DS0_ACCEL_SAMPLERATE, m_LSM9DS0AccelSampleRate);

    setBlank();
    setComment("");
    setComment("Accel full scale range - ");
    setComment("  0 = +/- 2g ");
    setComment("  1 = +/- 4g ");
    setComment("  2 = +/- 6g ");
    setComment("  3 = +/- 8g ");
    setComment("  4 = +/- 16g ");
    setValue(RTIMULIB_LSM9DS0_ACCEL_FSR, m_LSM9DS0AccelFsr);

    setBlank();
    setComment("");
    setComment("Accel low pass filter - ");
    setComment("  0 = 773Hz");
    setComment("  1 = 194Hz");
    setComment("  2 = 362Hz");
    setComment("  3 = 50Hz");
    setValue(RTIMULIB_LSM9DS0_ACCEL_LPF, m_LSM9DS0AccelLpf);

    setBlank();
    setComment("");
    setComment("Compass sample rate - ");
    setComment("  0 = 3.125Hz ");
    setComment("  1 = 6.25Hz ");
    setComment("  2 = 12.5Hz ");
    setComment("  3 = 25Hz ");
    setComment("  4 = 50Hz ");
    setComment("  5 = 100Hz ");
    setValue(RTIMULIB_LSM9DS0_COMPASS_SAMPLERATE, m_LSM9DS0CompassSampleRate);


    setBlank();
    setComment("");
    setComment("Compass full scale range - ");
    setComment("  0 = +/- 200 uT ");
    setComment("  1 = +/- 400 uT ");
    setComment("  2 = +/- 800 uT ");
    setComment("  3 = +/- 1200 uT ");
    setValue(RTIMULIB_LSM9DS0_COMPASS_FSR, m_LSM9DS0CompassFsr);

//  LSM9DS1 settings

    setBlank();
    setComment("#####################################################################");
    setComment("");
    setComment("LSM9DS1 settings");
    setComment("");

    setBlank();
    setComment("Gyro sample rate - ");
    setComment("  0 = 95Hz ");
    setComment("  1 = 190Hz ");
    setComment("  2 = 380Hz ");
    setComment("  3 = 760Hz ");
    setValue(RTIMULIB_LSM9DS1_GYRO_SAMPLERATE, m_LSM9DS1GyroSampleRate);

    setBlank();
    setComment("");
    setComment("Gyro full scale range - ");
    setComment("  0 = 250 degrees per second ");
    setComment("  1 = 500 degrees per second ");
    setComment("  2 = 2000 degrees per second ");
    setValue(RTIMULIB_LSM9DS1_GYRO_FSR, m_LSM9DS1GyroFsr);

    setBlank();
    setComment("");
    setComment("Gyro high pass filter - ");
    setComment("  0 - 9 but see the LSM9DS1 manual for details");
    setValue(RTIMULIB_LSM9DS1_GYRO_HPF, m_LSM9DS1GyroHpf);

    setBlank();
    setComment("");
    setComment("Gyro bandwidth - ");
    setComment("  0 - 3 but see the LSM9DS1 manual for details");
    setValue(RTIMULIB_LSM9DS1_GYRO_BW, m_LSM9DS1GyroBW);

    setBlank();
    setComment("Accel sample rate - ");
    setComment("  1 = 14.9Hz ");
    setComment("  2 = 59.5Hz ");
    setComment("  3 = 119Hz ");
    setComment("  4 = 238Hz ");
    setComment("  5 = 476Hz ");
    setComment("  6 = 952Hz ");
    setValue(RTIMULIB_LSM9DS1_ACCEL_SAMPLERATE, m_LSM9DS1AccelSampleRate);

    setBlank();
    setComment("");
    setComment("Accel full scale range - ");
    setComment("  0 = +/- 2g ");
    setComment("  1 = +/- 16g ");
    setComment("  2 = +/- 4g ");
    setComment("  3 = +/- 8g ");
    setValue(RTIMULIB_LSM9DS1_ACCEL_FSR, m_LSM9DS1AccelFsr);

    setBlank();
    setComment("");
    setComment("Accel low pass filter - ");
    setComment("  0 = 408Hz");
    setComment("  1 = 211Hz");
    setComment("  2 = 105Hz");
    setComment("  3 = 50Hz");
    setValue(RTIMULIB_LSM9DS1_ACCEL_LPF, m_LSM9DS1AccelLpf);

    setBlank();
    setComment("");
    setComment("Compass sample rate - ");
    setComment("  0 = 0.625Hz ");
    setComment("  1 = 1.25Hz ");
    setComment("  2 = 2.5Hz ");
    setComment("  3 = 5Hz ");
    setComment("  4 = 10Hz ");
    setComment("  5 = 20Hz ");
    setComment("  6 = 40Hz ");
    setComment("  7 = 80Hz ");
    setValue(RTIMULIB_LSM9DS1_COMPASS_SAMPLERATE, m_LSM9DS1CompassSampleRate);

    setBlank();
    setComment("");
    setComment("Compass full scale range - ");
    setComment("  0 = +/- 400 uT ");
    setComment("  1 = +/- 800 uT ");
    setComment("  2 = +/- 1200 uT ");
    setComment("  3 = +/- 1600 uT ");
    setValue(RTIMULIB_LSM9DS1_COMPASS_FSR, m_LSM9DS1CompassFsr);

    //  BMX055 settings

    setBlank();
    setComment("#####################################################################");
    setComment("");
    setComment("BMX055 settings");
    setComment("");

    setBlank();
    setComment("");
    setComment("Gyro sample rate - ");
    setComment("  0 = 2000Hz (532Hz filter)");
    setComment("  1 = 2000Hz (230Hz filter)");
    setComment("  2 = 1000Hz (116Hz filter)");
    setComment("  3 = 400Hz (47Hz filter)");
    setComment("  4 = 200Hz (23Hz filter)");
    setComment("  5 = 100Hz (12Hz filter)");
    setComment("  6 = 200Hz (64Hz filter)");
    setComment("  7 = 100Hz (32Hz filter)");
    setValue(RTIMULIB_BMX055_GYRO_SAMPLERATE, m_BMX055GyroSampleRate);

    setBlank();
    setComment("");
    setComment("Gyro full scale range - ");
    setComment("  0 = 2000 deg/s");
    setComment("  1 = 1000 deg/s");
    setComment("  2 = 500 deg/s");
    setComment("  3 = 250 deg/s");
    setComment("  4 = 125 deg/s");
    setValue(RTIMULIB_BMX055_GYRO_FSR, m_BMX055GyroFsr);

    setBlank();
    setComment("");
    setComment("Accel sample rate - ");
    setComment("  0 = 15.63Hz");
    setComment("  1 = 31.25");
    setComment("  2 = 62.5");
    setComment("  3 = 125");
    setComment("  4 = 250");
    setComment("  5 = 500");
    setComment("  6 = 1000");
    setComment("  7 = 2000");
    setValue(RTIMULIB_BMX055_ACCEL_SAMPLERATE, m_BMX055AccelSampleRate);

    setBlank();
    setComment("");
    setComment("Accel full scale range - ");
    setComment("  0 = +/- 2g");
    setComment("  1 = +/- 4g");
    setComment("  2 = +/- 8g");
    setComment("  3 = +/- 16g");
    setValue(RTIMULIB_BMX055_ACCEL_FSR, m_BMX055AccelFsr);

    setBlank();
    setComment("");
    setComment("Mag presets - ");
    setComment("  0 = Low power");
    setComment("  1 = Regular");
    setComment("  2 = Enhanced");
    setComment("  3 = High accuracy");
    setValue(RTIMULIB_BMX055_MAG_PRESET, m_BMX055MagPreset);

    fclose(m_fd);
    return true;
}

void RTIMUSettings::setBlank()
{
    fprintf(m_fd, "\n");
}

void RTIMUSettings::setComment(const char *comment)
{
    fprintf(m_fd, "# %s\n", comment);
}

void RTIMUSettings::setValue(const char *key, const bool val)
{
    fprintf(m_fd, "%s=%s\n", key, val ? "true" : "false");
}

void RTIMUSettings::setValue(const char *key, const int val)
{
    fprintf(m_fd, "%s=%d\n", key, val);
}

void RTIMUSettings::setValue(const char *key, const RTFLOAT val)
{
    fprintf(m_fd, "%s=%f\n", key, val);
}


