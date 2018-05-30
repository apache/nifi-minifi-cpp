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


#include "RTFusionRTQF.h"
#include "RTIMUSettings.h"


RTFusionRTQF::RTFusionRTQF()
{
    reset();
}

RTFusionRTQF::~RTFusionRTQF()
{
}

void RTFusionRTQF::reset()
{
    m_firstTime = true;
    m_fusionPose = RTVector3();
    m_fusionQPose.fromEuler(m_fusionPose);
    m_gyro = RTVector3();
    m_accel = RTVector3();
    m_compass = RTVector3();
    m_measuredPose = RTVector3();
    m_measuredQPose.fromEuler(m_measuredPose);
    m_sampleNumber = 0;
 }

void RTFusionRTQF::predict()
{
    RTFLOAT x2, y2, z2;
    RTFLOAT qs, qx, qy,qz;

    if (!m_enableGyro)
        return;

    qs = m_stateQ.scalar();
    qx = m_stateQ.x();
    qy = m_stateQ.y();
    qz = m_stateQ.z();

    x2 = m_gyro.x() / (RTFLOAT)2.0;
    y2 = m_gyro.y() / (RTFLOAT)2.0;
    z2 = m_gyro.z() / (RTFLOAT)2.0;

    // Predict new state

    m_stateQ.setScalar(qs + (-x2 * qx - y2 * qy - z2 * qz) * m_timeDelta);
    m_stateQ.setX(qx + (x2 * qs + z2 * qy - y2 * qz) * m_timeDelta);
    m_stateQ.setY(qy + (y2 * qs - z2 * qx + x2 * qz) * m_timeDelta);
    m_stateQ.setZ(qz + (z2 * qs + y2 * qx - x2 * qy) * m_timeDelta);
    m_stateQ.normalize();
}


void RTFusionRTQF::update()
{
    if (m_enableCompass || m_enableAccel) {

        // calculate rotation delta

        m_rotationDelta = m_stateQ.conjugate() * m_measuredQPose;
        m_rotationDelta.normalize();

        // take it to the power (0 to 1) to give the desired amount of correction

        RTFLOAT theta = acos(m_rotationDelta.scalar());

        RTFLOAT sinPowerTheta = sin(theta * m_slerpPower);
        RTFLOAT cosPowerTheta = cos(theta * m_slerpPower);

        m_rotationUnitVector.setX(m_rotationDelta.x());
        m_rotationUnitVector.setY(m_rotationDelta.y());
        m_rotationUnitVector.setZ(m_rotationDelta.z());
        m_rotationUnitVector.normalize();

        m_rotationPower.setScalar(cosPowerTheta);
        m_rotationPower.setX(sinPowerTheta * m_rotationUnitVector.x());
        m_rotationPower.setY(sinPowerTheta * m_rotationUnitVector.y());
        m_rotationPower.setZ(sinPowerTheta * m_rotationUnitVector.z());
        m_rotationPower.normalize();

        //  multiple this by predicted value to get result

        m_stateQ *= m_rotationPower;
        m_stateQ.normalize();
    }
}

void RTFusionRTQF::newIMUData(RTIMU_DATA& data, const RTIMUSettings *settings)
{
    if (m_debug) {
        HAL_INFO("\n------\n");
        HAL_INFO2("IMU update delta time: %f, sample %d\n", m_timeDelta, m_sampleNumber++);
    }
    m_sampleNumber++;

    if (m_enableGyro)
        m_gyro = data.gyro;
    else
        m_gyro = RTVector3();
    m_accel = data.accel;
    m_compass = data.compass;
    m_compassValid = data.compassValid;

    if (m_firstTime) {
        m_lastFusionTime = data.timestamp;
        calculatePose(m_accel, m_compass, settings->m_compassAdjDeclination);

        //  initialize the poses

        m_stateQ.fromEuler(m_measuredPose);
        m_fusionQPose = m_stateQ;
        m_fusionPose = m_measuredPose;
        m_firstTime = false;
    } else {
        m_timeDelta = (RTFLOAT)(data.timestamp - m_lastFusionTime) / (RTFLOAT)1000000;
        m_lastFusionTime = data.timestamp;
        if (m_timeDelta <= 0)
            return;

        calculatePose(data.accel, data.compass, settings->m_compassAdjDeclination);

        predict();
        update();
        m_stateQ.toEuler(m_fusionPose);
        m_fusionQPose = m_stateQ;

        if (m_debug) {
            HAL_INFO(RTMath::displayRadians("Measured pose", m_measuredPose));
            HAL_INFO(RTMath::displayRadians("RTQF pose", m_fusionPose));
            HAL_INFO(RTMath::displayRadians("Measured quat", m_measuredPose));
            HAL_INFO(RTMath::display("RTQF quat", m_stateQ));
            HAL_INFO(RTMath::display("Error quat", m_stateQError));
         }
    }
    data.fusionPoseValid = true;
    data.fusionQPoseValid = true;
    data.fusionPose = m_fusionPose;
    data.fusionQPose = m_fusionQPose;
}
