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


#ifndef _RTIMULIB_H
#define	_RTIMULIB_H

#include "RTIMULibDefs.h"

#include "RTMath.h"

#include "RTFusion.h"
#include "RTFusionKalman4.h"

#include "RTIMUHal.h"
#include "IMUDrivers/RTIMU.h"
#include "IMUDrivers/RTIMUNull.h"
#include "IMUDrivers/RTIMUMPU9150.h"
#include "IMUDrivers/RTIMUGD20HM303D.h"
#include "IMUDrivers/RTIMUGD20M303DLHC.h"
#include "IMUDrivers/RTIMULSM9DS0.h"

#include "IMUDrivers/RTPressure.h"
#include "IMUDrivers/RTPressureBMP180.h"
#include "IMUDrivers/RTPressureLPS25H.h"
#include "IMUDrivers/RTPressureMS5611.h"

#include "IMUDrivers/RTHumidity.h"
#include "IMUDrivers/RTHumidityHTS221.h"

#include "RTIMUSettings.h"


#endif // _RTIMULIB_H
