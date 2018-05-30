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

#ifndef _RTMATH_H_
#define _RTMATH_H_

#include "RTIMUHal.h"

//  The fundamental float type

#ifdef RTMATH_USE_DOUBLE
typedef double RTFLOAT;
#else
typedef float RTFLOAT;
#endif

//  Useful constants

#define	RTMATH_PI					3.1415926535
#define	RTMATH_DEGREE_TO_RAD		(RTMATH_PI / 180.0)
#define	RTMATH_RAD_TO_DEGREE		(180.0 / RTMATH_PI)

class RTVector3;
class RTMatrix4x4;
class RTQuaternion;

class RTMath
{
public:
    // convenient display routines

    static const char *displayRadians(const char *label, RTVector3& vec);
    static const char *displayDegrees(const char *label, RTVector3& vec);
    static const char *display(const char *label, RTQuaternion& quat);
    static const char *display(const char *label, RTMatrix4x4& mat);

    //  currentUSecsSinceEpoch() is the source of all timestamps and
    //  is the number of uS since the standard epoch

    static uint64_t currentUSecsSinceEpoch();

    //  poseFromAccelMag generates pose Euler angles from measured settings

    static RTVector3 poseFromAccelMag(const RTVector3& accel, const RTVector3& mag);

    //  Takes signed 16 bit data from a char array and converts it to a vector of scaled RTFLOATs

    static void convertToVector(unsigned char *rawData, RTVector3& vec, RTFLOAT scale, bool bigEndian);

    //  Takes a pressure in hPa and returns height above sea level in meters

    static RTFLOAT convertPressureToHeight(RTFLOAT pressure, RTFLOAT staticPressure = 1013.25);

private:
    static char m_string[1000];                             // for the display routines
};


class RTVector3
{
public:
    RTVector3();
    RTVector3(RTFLOAT x, RTFLOAT y, RTFLOAT z);

    const RTVector3&  operator +=(RTVector3& vec);
    const RTVector3&  operator -=(RTVector3& vec);

    RTVector3& operator =(const RTVector3& vec);

    RTFLOAT length();
    void normalize();
    void zero();
    const char *display();
    const char *displayDegrees();

    static float dotProduct(const RTVector3& a, const RTVector3& b);
    static void crossProduct(const RTVector3& a, const RTVector3& b, RTVector3& d);

    void accelToEuler(RTVector3& rollPitchYaw) const;
    void accelToQuaternion(RTQuaternion& qPose) const;

    inline RTFLOAT x() const { return m_data[0]; }
    inline RTFLOAT y() const { return m_data[1]; }
    inline RTFLOAT z() const { return m_data[2]; }
    inline RTFLOAT data(const int i) const { return m_data[i]; }

    inline void setX(const RTFLOAT val) { m_data[0] = val; }
    inline void setY(const RTFLOAT val) { m_data[1] = val; }
    inline void setZ(const RTFLOAT val) { m_data[2] = val; }
    inline void setData(const int i, RTFLOAT val) { m_data[i] = val; }
    inline void fromArray(RTFLOAT *val) { memcpy(m_data, val, 3 * sizeof(RTFLOAT)); }
    inline void toArray(RTFLOAT *val) const { memcpy(val, m_data, 3 * sizeof(RTFLOAT)); }

private:
    RTFLOAT m_data[3];
};


class RTQuaternion
{
public:
    RTQuaternion();
    RTQuaternion(RTFLOAT scalar, RTFLOAT x, RTFLOAT y, RTFLOAT z);

    RTQuaternion& operator +=(const RTQuaternion& quat);
    RTQuaternion& operator -=(const RTQuaternion& quat);
    RTQuaternion& operator *=(const RTQuaternion& qb);
    RTQuaternion& operator *=(const RTFLOAT val);
    RTQuaternion& operator -=(const RTFLOAT val);

    RTQuaternion& operator =(const RTQuaternion& quat);
    const RTQuaternion operator *(const RTQuaternion& qb) const;
    const RTQuaternion operator *(const RTFLOAT val) const;
    const RTQuaternion operator -(const RTQuaternion& qb) const;
    const RTQuaternion operator -(const RTFLOAT val) const;

    void normalize();
    void toEuler(RTVector3& vec);
    void fromEuler(RTVector3& vec);
    RTQuaternion conjugate() const;
    void toAngleVector(RTFLOAT& angle, RTVector3& vec);
    void fromAngleVector(const RTFLOAT& angle, const RTVector3& vec);

    void zero();
    const char *display();

    inline RTFLOAT scalar() const { return m_data[0]; }
    inline RTFLOAT x() const { return m_data[1]; }
    inline RTFLOAT y() const { return m_data[2]; }
    inline RTFLOAT z() const { return m_data[3]; }
    inline RTFLOAT data(const int i) const { return m_data[i]; }

    inline void setScalar(const RTFLOAT val) { m_data[0] = val; }
    inline void setX(const RTFLOAT val) { m_data[1] = val; }
    inline void setY(const RTFLOAT val) { m_data[2] = val; }
    inline void setZ(const RTFLOAT val) { m_data[3] = val; }
    inline void setData(const int i, RTFLOAT val) { m_data[i] = val; }
    inline void fromArray(RTFLOAT *val) { memcpy(m_data, val, 4 * sizeof(RTFLOAT)); }
    inline void toArray(RTFLOAT *val) const { memcpy(val, m_data, 4 * sizeof(RTFLOAT)); }

private:
    RTFLOAT m_data[4];
};

class RTMatrix4x4
{
public:
    RTMatrix4x4();

    RTMatrix4x4& operator +=(const RTMatrix4x4& mat);
    RTMatrix4x4& operator -=(const RTMatrix4x4& mat);
    RTMatrix4x4& operator *=(const RTFLOAT val);

    RTMatrix4x4& operator =(const RTMatrix4x4& vec);
    const RTQuaternion operator *(const RTQuaternion& q) const;
    const RTMatrix4x4 operator *(const RTFLOAT val) const;
    const RTMatrix4x4 operator *(const RTMatrix4x4& mat) const;
    const RTMatrix4x4 operator +(const RTMatrix4x4& mat) const;

    inline RTFLOAT val(int row, int col) const { return m_data[row][col]; }
    inline void setVal(int row, int col, RTFLOAT val) { m_data[row][col] = val; }
    void fill(RTFLOAT val);
    void setToIdentity();

    RTMatrix4x4 inverted();
    RTMatrix4x4 transposed();

private:
    RTFLOAT m_data[4][4];                                   // row, column

    RTFLOAT matDet();
    RTFLOAT matMinor(const int row, const int col);
};

#endif /* _RTMATH_H_ */
