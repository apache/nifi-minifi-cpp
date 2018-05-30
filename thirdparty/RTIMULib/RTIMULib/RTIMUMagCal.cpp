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


#include "RTIMUMagCal.h"

RTIMUMagCal::RTIMUMagCal(RTIMUSettings *settings)
{
    m_settings = settings;
}

RTIMUMagCal::~RTIMUMagCal()
{

}

void RTIMUMagCal::magCalInit()
{
    magCalReset();
}

void RTIMUMagCal::magCalReset()
{
    m_magMin = RTVector3(RTIMUCALDEFS_DEFAULT_MIN, RTIMUCALDEFS_DEFAULT_MIN, RTIMUCALDEFS_DEFAULT_MIN);
    m_magMax = RTVector3(RTIMUCALDEFS_DEFAULT_MAX, RTIMUCALDEFS_DEFAULT_MAX, RTIMUCALDEFS_DEFAULT_MAX);

    m_magCalCount = 0;
    for (int i = 0; i < RTIMUCALDEFS_OCTANT_COUNT; i++)
        m_octantCounts[i] = 0;
    m_magCalInIndex = m_magCalOutIndex = 0;

    //  throw away first few samples so we don't see any old calibrated samples
    m_startCount = 100;
}

void RTIMUMagCal::newMinMaxData(const RTVector3& data)
{
    if (m_startCount > 0) {
        m_startCount--;
        return;
    }

    for (int i = 0; i < 3; i++) {
        if (m_magMin.data(i) > data.data(i)) {
            m_magMin.setData(i, data.data(i));
        }

        if (m_magMax.data(i) < data.data(i)) {
            m_magMax.setData(i, data.data(i));
        }
    }
}

bool RTIMUMagCal::magCalValid()
{
    bool valid = true;

     for (int i = 0; i < 3; i++) {
        if (m_magMax.data(i) < m_magMin.data(i))
            valid = false;
    }
    return valid;

}

void RTIMUMagCal::magCalSaveMinMax()
{
    m_settings->m_compassCalValid = true;
    m_settings->m_compassCalMin = m_magMin;
    m_settings->m_compassCalMax = m_magMax;
    m_settings->m_compassCalEllipsoidValid = false;
    m_settings->saveSettings();

    //  need to invalidate ellipsoid data in order to use new min/max data

    m_magCalCount = 0;
    for (int i = 0; i < RTIMUCALDEFS_OCTANT_COUNT; i++)
        m_octantCounts[i] = 0;
    m_magCalInIndex = m_magCalOutIndex = 0;

    // and set up for min/max calibration

    setMinMaxCal();
}

void RTIMUMagCal::newEllipsoidData(const RTVector3& data)
{
    RTVector3 calData;

    //  do min/max calibration first

    for (int i = 0; i < 3; i++)
        calData.setData(i, (data.data(i) - m_minMaxOffset.data(i)) * m_minMaxScale.data(i));

    //  now see if it's already there - we want them all unique and slightly separate (using a fuzzy compare)

    for (int index = m_magCalOutIndex, i = 0; i < m_magCalCount; i++) {
        if ((abs(calData.x() - m_magCalSamples[index].x()) < RTIMUCALDEFS_ELLIPSOID_MIN_SPACING) &&
            (abs(calData.y() - m_magCalSamples[index].y()) < RTIMUCALDEFS_ELLIPSOID_MIN_SPACING) &&
            (abs(calData.z() - m_magCalSamples[index].z()) < RTIMUCALDEFS_ELLIPSOID_MIN_SPACING)) {
                return;                                         // too close to another sample
        }
        if (++index == RTIMUCALDEFS_MAX_MAG_SAMPLES)
            index = 0;
    }


    m_octantCounts[findOctant(calData)]++;

    m_magCalSamples[m_magCalInIndex++] = calData;
    if (m_magCalInIndex == RTIMUCALDEFS_MAX_MAG_SAMPLES)
        m_magCalInIndex = 0;

    if (++m_magCalCount == RTIMUCALDEFS_MAX_MAG_SAMPLES) {
        // buffer is full - pull oldest
        removeMagCalData();
    }
}

bool RTIMUMagCal::magCalEllipsoidValid()
{
    bool valid = true;

    for (int i = 0; i < RTIMUCALDEFS_OCTANT_COUNT; i++) {
        if (m_octantCounts[i] < RTIMUCALDEFS_OCTANT_MIN_SAMPLES)
            valid = false;
    }
    return valid;
}

RTVector3 RTIMUMagCal::removeMagCalData()
{
    RTVector3 ret;

    if (m_magCalCount == 0)
        return ret;

    ret = m_magCalSamples[m_magCalOutIndex++];
    if (m_magCalOutIndex == RTIMUCALDEFS_MAX_MAG_SAMPLES)
        m_magCalOutIndex = 0;
    m_magCalCount--;
    m_octantCounts[findOctant(ret)]--;
    return ret;
}

bool RTIMUMagCal::magCalSaveRaw(const char *ellipsoidFitPath)
{
    FILE *file;
    char *rawFile;

    if (ellipsoidFitPath != NULL) {
        // need to deal with ellipsoid fit processing
        rawFile = (char *)malloc(strlen(RTIMUCALDEFS_MAG_RAW_FILE) + strlen(ellipsoidFitPath) + 2);
        sprintf(rawFile, "%s/%s", ellipsoidFitPath, RTIMUCALDEFS_MAG_RAW_FILE);
        if ((file = fopen(rawFile, "w")) == NULL) {
            HAL_ERROR("Failed to open ellipsoid fit raw data file\n");
            return false;
        }
        while (m_magCalCount > 0) {
            RTVector3 sample = removeMagCalData();
            fprintf(file, "%f %f %f\n", sample.x(), sample.y(), sample.z());
        }
        fclose(file);
    }
    return true;
}

bool RTIMUMagCal::magCalSaveCorr(const char *ellipsoidFitPath)
{
    FILE *file;
    char *corrFile;
    float a[3];
    float b[9];

    if (ellipsoidFitPath != NULL) {
        corrFile = (char *)malloc(strlen(RTIMUCALDEFS_MAG_CORR_FILE) + strlen(ellipsoidFitPath) + 2);
        sprintf(corrFile, "%s/%s", ellipsoidFitPath, RTIMUCALDEFS_MAG_CORR_FILE);
        if ((file = fopen(corrFile, "r")) == NULL) {
            HAL_ERROR("Failed to open ellipsoid fit correction data file\n");
            return false;
        }
        if (fscanf(file, "%f %f %f %f %f %f %f %f %f %f %f %f",
            a + 0, a + 1, a + 2, b + 0, b + 1, b + 2, b + 3, b + 4, b + 5, b + 6, b + 7, b + 8) != 12) {
            HAL_ERROR("Ellipsoid corrcetion file didn't have 12 floats\n");
            fclose(file);
            return false;
        }
        fclose(file);
        m_settings->m_compassCalEllipsoidValid = true;
        m_settings->m_compassCalEllipsoidOffset = RTVector3(a[0], a[1], a[2]);
        memcpy(m_settings->m_compassCalEllipsoidCorr, b, 9 * sizeof(float));
        m_settings->saveSettings();
        return true;
    }
    return false;
}


void RTIMUMagCal::magCalOctantCounts(int *counts)
{
    memcpy(counts, m_octantCounts, RTIMUCALDEFS_OCTANT_COUNT * sizeof(int));
}

int RTIMUMagCal::findOctant(const RTVector3& data)
{
    int val = 0;

    if (data.x() >= 0)
        val = 1;
    if (data.y() >= 0)
        val |= 2;
    if (data.z() >= 0)
        val |= 4;

    return val;
}

void RTIMUMagCal::setMinMaxCal()
{
    float maxDelta = -1;
    float delta;

    //  find biggest range

    for (int i = 0; i < 3; i++) {
        if ((m_magMax.data(i) - m_magMin.data(i)) > maxDelta)
            maxDelta = m_magMax.data(i) - m_magMin.data(i);
    }
    if (maxDelta < 0) {
        HAL_ERROR("Error in min/max calibration data\n");
        return;
    }
    maxDelta /= 2.0f;                                       // this is the max +/- range

    for (int i = 0; i < 3; i++) {
        delta = (m_magMax.data(i) -m_magMin.data(i)) / 2.0f;
        m_minMaxScale.setData(i, maxDelta / delta);            // makes everything the same range
        m_minMaxOffset.setData(i, (m_magMax.data(i) + m_magMin.data(i)) / 2.0f);
    }
}
