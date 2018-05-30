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


#include "RTHumidity.h"

#include "RTHumidityHTS221.h"
#include "RTHumidityHTU21D.h"

RTHumidity *RTHumidity::createHumidity(RTIMUSettings *settings)
{
    switch (settings->m_humidityType) {
    case RTHUMIDITY_TYPE_HTS221:
        return new RTHumidityHTS221(settings);

    case RTHUMIDITY_TYPE_HTU21D:
        return new RTHumidityHTU21D(settings);

    case RTHUMIDITY_TYPE_AUTODISCOVER:
        if (settings->discoverHumidity(settings->m_humidityType, settings->m_I2CHumidityAddress)) {
            settings->saveSettings();
            return RTHumidity::createHumidity(settings);
        }
        return NULL;

    case RTHUMIDITY_TYPE_NULL:
        return NULL;

    default:
        return NULL;
    }
}


RTHumidity::RTHumidity(RTIMUSettings *settings)
{
    m_settings = settings;
}

RTHumidity::~RTHumidity()
{
}
