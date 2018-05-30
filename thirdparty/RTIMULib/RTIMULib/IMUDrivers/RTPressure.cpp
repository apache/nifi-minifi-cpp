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


#include "RTPressure.h"

#include "RTPressureBMP180.h"
#include "RTPressureLPS25H.h"
#include "RTPressureMS5611.h"
#include "RTPressureMS5637.h"

RTPressure *RTPressure::createPressure(RTIMUSettings *settings)
{
    switch (settings->m_pressureType) {
    case RTPRESSURE_TYPE_BMP180:
        return new RTPressureBMP180(settings);

    case RTPRESSURE_TYPE_LPS25H:
        return new RTPressureLPS25H(settings);

    case RTPRESSURE_TYPE_MS5611:
        return new RTPressureMS5611(settings);

    case RTPRESSURE_TYPE_MS5637:
        return new RTPressureMS5637(settings);

    case RTPRESSURE_TYPE_AUTODISCOVER:
        if (settings->discoverPressure(settings->m_pressureType, settings->m_I2CPressureAddress)) {
            settings->saveSettings();
            return RTPressure::createPressure(settings);
        }
        return NULL;

    case RTPRESSURE_TYPE_NULL:
        return NULL;

    default:
        return NULL;
    }
}


RTPressure::RTPressure(RTIMUSettings *settings)
{
    m_settings = settings;
}

RTPressure::~RTPressure()
{
}
