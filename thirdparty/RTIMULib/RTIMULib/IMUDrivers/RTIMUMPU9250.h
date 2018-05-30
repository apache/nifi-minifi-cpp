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

//  The MPU-9250 and SPI driver code is based on code generously supplied by
//  staslock@gmail.com (www.clickdrive.io)


#ifndef _RTIMUMPU9250_H
#define	_RTIMUMPU9250_H

#include "RTIMU.h"

//  Define this symbol to use cache mode

#define MPU9250_CACHE_MODE

//  FIFO transfer size

#define MPU9250_FIFO_CHUNK_SIZE     12                      // gyro and accels take 12 bytes

#ifdef MPU9250_CACHE_MODE

//  Cache mode defines

#define MPU9250_CACHE_SIZE          16                      // number of chunks in a block
#define MPU9250_CACHE_BLOCK_COUNT   16                      // number of cache blocks

typedef struct
{
    unsigned char data[MPU9250_FIFO_CHUNK_SIZE * MPU9250_CACHE_SIZE];
    int count;                                              // number of chunks in the cache block
    int index;                                              // current index into the cache
    unsigned char compass[8];                               // the raw compass readings for the block

} MPU9250_CACHE_BLOCK;

#endif


class RTIMUMPU9250 : public RTIMU
{
public:
    RTIMUMPU9250(RTIMUSettings *settings);
    ~RTIMUMPU9250();

    bool setGyroLpf(unsigned char lpf);
    bool setAccelLpf(unsigned char lpf);
    bool setSampleRate(int rate);
    bool setCompassRate(int rate);
    bool setGyroFsr(unsigned char fsr);
    bool setAccelFsr(unsigned char fsr);

    virtual const char *IMUName() { return "MPU-9250"; }
    virtual int IMUType() { return RTIMU_TYPE_MPU9250; }
    virtual bool IMUInit();
    virtual bool IMURead();
    virtual int IMUGetPollInterval();

protected:

    RTFLOAT m_compassAdjust[3];                             // the compass fuse ROM values converted for use

private:
    bool setGyroConfig();
    bool setAccelConfig();
    bool setSampleRate();
    bool compassSetup();
    bool setCompassRate();
    bool resetFifo();
    bool bypassOn();
    bool bypassOff();

    bool m_firstTime;                                       // if first sample

    unsigned char m_slaveAddr;                              // I2C address of MPU9150

    unsigned char m_gyroLpf;                                // gyro low pass filter setting
    unsigned char m_accelLpf;                               // accel low pass filter setting
    int m_compassRate;                                      // compass sample rate in Hz
    unsigned char m_gyroFsr;
    unsigned char m_accelFsr;

    RTFLOAT m_gyroScale;
    RTFLOAT m_accelScale;


#ifdef MPU9250_CACHE_MODE

    MPU9250_CACHE_BLOCK m_cache[MPU9250_CACHE_BLOCK_COUNT]; // the cache itself
    int m_cacheIn;                                          // the in index
    int m_cacheOut;                                         // the out index
    int m_cacheCount;                                       // number of used cache blocks

#endif

};

#endif // _RTIMUMPU9250_H
