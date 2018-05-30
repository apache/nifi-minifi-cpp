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


#ifndef _RTIMUACCELCAL_H
#define	_RTIMUACCELCAL_H

#include "RTIMUCalDefs.h"
#include "RTIMULib.h"

//  RTIMUAccelCal is a helper class for performing accelerometer calibration

class RTIMUAccelCal
{

public:
    RTIMUAccelCal(RTIMUSettings *settings);
    virtual ~RTIMUAccelCal();

    //  This should be called at the start of the calibration process
    //  Loads previous values if available
    void accelCalInit();

    //  This should be called to clear enabled axes for a new run
    void accelCalReset();

    //  accelCalEnable() controls which axes are active - largely so that each can be done separately
    void accelCalEnable(int axis, bool enable);

    // newAccalCalData() adds a new sample for processing but only the axes enabled previously
    void newAccelCalData(const RTVector3& data);            // adds a new accel sample

    // accelCalValid() checks if all values are reasonable. Should be called before saving
    bool accelCalValid();

    //  accelCalSave() should be called at the end of the process to save the cal data
    //  to the settings file. Returns false if invalid data
    bool accelCalSave();                                    // saves the accel cal data for specified axes

    // these vars used during the calibration process

    bool m_accelCalValid;                                   // true if the mag min/max data valid
    RTVector3 m_accelMin;                                   // the min values
    RTVector3 m_accelMax;                                   // the max values

    RTVector3 m_averageValue;                               // averaged value actually used

    bool m_accelCalEnable[3];                               // the enable flags

    RTIMUSettings *m_settings;

};

#endif // _RTIMUACCELCAL_H
