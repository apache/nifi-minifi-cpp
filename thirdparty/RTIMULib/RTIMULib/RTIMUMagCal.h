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


#ifndef _RTIMUMAGCAL_H
#define	_RTIMUMAGCAL_H

#include "RTIMUCalDefs.h"
#include "RTIMULib.h"

class RTIMUMagCal
{

public:
    RTIMUMagCal(RTIMUSettings *settings);
    virtual ~RTIMUMagCal();

    void magCalInit();                                      // inits everything
    void magCalReset();                                     // clears everything

    // newMinMaxData() is used to submit a new sample for min/max processing
    void newMinMaxData(const RTVector3& data);

    // newEllipsoidData is used to save data to the ellipsoid sample array
    void newEllipsoidData(const RTVector3& data);

    // magCalValid() determines if the min/max data is basically valid
    bool magCalValid();

    // magCalEllipsoidValid() determines if enough samples have been collected for a valid ellipsoid fit
    bool magCalEllipsoidValid();

    // magCalSaveMinMax() saves the current min/max values to settings
    void magCalSaveMinMax();

    // magCalSaveRaw saves the ellipsoid fit data and then
    // saves data to the .ini file.
    //
    // Returns true if everything worked correctly.

    bool magCalSaveRaw(const char *ellipsoidFitPath);

    // magCalSaveCorr loads the correction data from the ellipsoid fit program and saves it in the
    // .ini

    bool magCalSaveCorr(const char *ellipsoidFitPath);

    //  magCalSaveEllipsoid retrieves the ellipsoid fit calibration data
    //  and saves it in the .ini file.

    void magCalOctantCounts(int *counts);                   // returns a count for each of the 8 octants

    // these vars used during the calibration process

    RTVector3 m_magMin;                                     // the min values
    RTVector3 m_magMax;                                     // the max values

    RTIMUSettings *m_settings;

private:
    RTVector3 removeMagCalData();                           // takes an entry out of the buffer
    int findOctant(const RTVector3& data);                  // works out which octant the data is in
    void setMinMaxCal();                                    // get ready for the ellipsoid mode

    int m_startCount;                                       // need to throw way first few samples
    RTVector3 m_magCalSamples[RTIMUCALDEFS_MAX_MAG_SAMPLES];// the saved samples for ellipsoid fit
    int m_magCalInIndex;                                    // current in index into the data
    int m_magCalOutIndex;                                   // current out index into the data
    int m_magCalCount;                                      // how many samples in the buffer

    RTVector3 m_minMaxOffset;                               // the min/max calibration offset
    RTVector3 m_minMaxScale;                                // the min/max scale

    int m_octantCounts[RTIMUCALDEFS_OCTANT_COUNT];          // counts in each octant

};

#endif // _RTIMUMAGCAL_H
