#////////////////////////////////////////////////////////////////////////////
#//
#//  This file is part of RTIMULib
#//
#//  Copyright (c) 2014-2015, richards-tech, LLC
#//
#//  Permission is hereby granted, free of charge, to any person obtaining a copy of
#//  this software and associated documentation files (the "Software"), to deal in
#//  the Software without restriction, including without limitation the rights to use,
#//  copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
#//  Software, and to permit persons to whom the Software is furnished to do so,
#//  subject to the following conditions:
#//
#//  The above copyright notice and this permission notice shall be included in all
#//  copies or substantial portions of the Software.
#//
#//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
#//  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
#//  PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
#//  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
#//  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
#//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

HEADERS += $$PWD/RTIMULib.h \
    $$PWD/RTIMULibDefs.h \
    $$PWD/RTMath.h \
    $$PWD/RTIMUHal.h \
    $$PWD/RTFusion.h \
    $$PWD/RTFusionKalman4.h \
    $$PWD/RTFusionRTQF.h \
    $$PWD/RTIMUSettings.h \
    $$PWD/RTIMUMagCal.h \
    $$PWD/RTIMUAccelCal.h \
    $$PWD/RTIMUCalDefs.h \
    $$PWD/IMUDrivers/RTIMU.h \
    $$PWD/IMUDrivers/RTIMUDefs.h \
    $$PWD/IMUDrivers/RTIMUMPU9150.h \
    $$PWD/IMUDrivers/RTIMUMPU9250.h \
    $$PWD/IMUDrivers/RTIMUGD20HM303D.h \
    $$PWD/IMUDrivers/RTIMUGD20M303DLHC.h \
    $$PWD/IMUDrivers/RTIMUGD20HM303DLHC.h \
    $$PWD/IMUDrivers/RTIMULSM9DS0.h \
    $$PWD/IMUDrivers/RTIMULSM9DS1.h \
    $$PWD/IMUDrivers/RTIMUBMX055.h \
    $$PWD/IMUDrivers/RTIMUBNO055.h \
    $$PWD/IMUDrivers/RTIMUNull.h \
    $$PWD/IMUDrivers/RTPressure.h \
    $$PWD/IMUDrivers/RTPressureDefs.h \
    $$PWD/IMUDrivers/RTPressureBMP180.h \
    $$PWD/IMUDrivers/RTPressureLPS25H.h \
    $$PWD/IMUDrivers/RTPressureMS5611.h \
    $$PWD/IMUDrivers/RTPressureMS5637.h \
    $$PWD/IMUDrivers/RTHumidity.h \
    $$PWD/IMUDrivers/RTHumidityDefs.h \
    $$PWD/IMUDrivers/RTHumidityHTS221.h \
    $$PWD/IMUDrivers/RTHumidityHTU21D.h \

SOURCES += $$PWD/RTMath.cpp \
    $$PWD/RTIMUHal.cpp \
    $$PWD/RTFusion.cpp \
    $$PWD/RTFusionKalman4.cpp \
    $$PWD/RTFusionRTQF.cpp \
    $$PWD/RTIMUSettings.cpp \
    $$PWD/RTIMUMagCal.cpp \
    $$PWD/RTIMUAccelCal.cpp \
    $$PWD/IMUDrivers/RTIMU.cpp \
    $$PWD/IMUDrivers/RTIMUMPU9150.cpp \
    $$PWD/IMUDrivers/RTIMUMPU9250.cpp \
    $$PWD/IMUDrivers/RTIMUGD20HM303D.cpp \
    $$PWD/IMUDrivers/RTIMUGD20M303DLHC.cpp \
    $$PWD/IMUDrivers/RTIMUGD20HM303DLHC.cpp \
    $$PWD/IMUDrivers/RTIMULSM9DS0.cpp \
    $$PWD/IMUDrivers/RTIMULSM9DS1.cpp \
    $$PWD/IMUDrivers/RTIMUBMX055.cpp \
    $$PWD/IMUDrivers/RTIMUBNO055.cpp \
    $$PWD/IMUDrivers/RTIMUNull.cpp \
    $$PWD/IMUDrivers/RTPressure.cpp \
    $$PWD/IMUDrivers/RTPressureBMP180.cpp \
    $$PWD/IMUDrivers/RTPressureLPS25H.cpp \
    $$PWD/IMUDrivers/RTPressureMS5611.cpp \
    $$PWD/IMUDrivers/RTPressureMS5637.cpp \
    $$PWD/IMUDrivers/RTHumidity.cpp \
    $$PWD/IMUDrivers/RTHumidityHTS221.cpp \
    $$PWD/IMUDrivers/RTHumidityHTU21D.cpp \



