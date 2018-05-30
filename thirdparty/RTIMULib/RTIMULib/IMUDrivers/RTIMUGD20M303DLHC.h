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


#ifndef _RTIMUGD20M303DLHC_H
#define	_RTIMUGD20M303DLHC_H

#include "RTIMU.h"

//  Define this symbol to use cache mode

//#define GD20M303DLHC_CACHE_MODE   // not reliable at the moment

#ifdef GD20M303DLHC_CACHE_MODE

//  Cache defs

#define GD20M303DLHC_FIFO_CHUNK_SIZE    6                       // 6 bytes of gyro data
#define GD20M303DLHC_FIFO_THRESH        16                      // threshold point in fifo
#define GD20M303DLHC_CACHE_BLOCK_COUNT  16                      // number of cache blocks

typedef struct
{
    unsigned char data[GD20M303DLHC_FIFO_THRESH * GD20M303DLHC_FIFO_CHUNK_SIZE];
    int count;                                              // number of chunks in the cache block
    int index;                                              // current index into the cache
    unsigned char accel[6];                                 // the raw accel readings for the block
    unsigned char compass[6];                               // the raw compass readings for the block

} GD20M303DLHC_CACHE_BLOCK;

#endif

class RTIMUGD20M303DLHC : public RTIMU
{
public:
    RTIMUGD20M303DLHC(RTIMUSettings *settings);
    ~RTIMUGD20M303DLHC();

    virtual const char *IMUName() { return "L3GD20 + LSM303DLHC"; }
    virtual int IMUType() { return RTIMU_TYPE_GD20M303DLHC; }
    virtual bool IMUInit();
    virtual int IMUGetPollInterval();
    virtual bool IMURead();

private:
    bool setGyroSampleRate();
    bool setGyroCTRL2();
    bool setGyroCTRL4();
    bool setGyroCTRL5();
    bool setAccelCTRL1();
    bool setAccelCTRL4();
    bool setCompassCRA();
    bool setCompassCRB();
    bool setCompassCRM();

    unsigned char m_gyroSlaveAddr;                          // I2C address of L3GD20
    unsigned char m_accelSlaveAddr;                         // I2C address of LSM303DLHC accel
    unsigned char m_compassSlaveAddr;                       // I2C address of LSM303DLHC compass

    RTFLOAT m_gyroScale;
    RTFLOAT m_accelScale;
    RTFLOAT m_compassScaleXY;
    RTFLOAT m_compassScaleZ;

#ifdef GD20M303DLHC_CACHE_MODE
    bool m_firstTime;                                       // if first sample

    GD20M303DLHC_CACHE_BLOCK m_cache[GD20M303DLHC_CACHE_BLOCK_COUNT]; // the cache itself
    int m_cacheIn;                                          // the in index
    int m_cacheOut;                                         // the out index
    int m_cacheCount;                                       // number of used cache blocks

#endif
};

#endif // _RTIMUGD20M303DLHC_H
