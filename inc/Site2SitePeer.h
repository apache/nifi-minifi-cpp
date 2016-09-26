/**
 * @file Site2SitePeer.h
 * Site2SitePeer class declaration for site to site peer  
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __SITE2SITE_PEER_H__
#define __SITE2SITE_PEER_H__

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <string>
#include <errno.h>
#include <mutex>
#include <atomic>
#include "TimeUtil.h"
#include "Logger.h"
#include "Configure.h"
#include "Property.h"

class CRC32
{
public:
    CRC32() {
    	crc = 0;

    	if (tableInit)
    		return;

    	tableInit = true;
        unsigned int poly = 0xedb88320;
        unsigned int temp = 0;
        for(unsigned int i = 0; i < 256; ++i) {
            temp = i;
            for(int j = 8; j > 0; --j) {
                if((temp & 1) == 1) {
                    temp = (unsigned int)((temp >> 1) ^ poly);
                }else {
                    temp >>= 1;
                }
            }
            table[i] = temp;
        }
    }

    unsigned int update(uint8_t * bytes, size_t size) {
    	crc = crc ^ ~0U;
        for(unsigned int i = 0; i < size; ++i) {
            uint8_t index = (uint8_t)(((crc) & 0xff) ^ bytes[i]);
            crc = (unsigned int)((crc >> 8) ^ table[index]);
        }
        crc = crc ^ ~0U;
        return crc;
    }

    long getCRC()
    {
    	return crc;
    }

private:
    static unsigned int table[256];
    static std::atomic<bool> tableInit;
    unsigned int crc;
};

static const char MAGIC_BYTES[] = {'N', 'i', 'F', 'i'};

//! Site2SitePeer Class
class Site2SitePeer
{
public:
	//! Constructor
	/*!
	 * Create a new site2site peer
	 */
	Site2SitePeer(std::string host, uint16_t port) {
		_logger = Logger::getLogger();
		_configure = Configure::getConfigure();
		_socket = 0;
		_host = host;
		_port = port;
		_yieldExpiration = 0;
		_timeOut = 30000; // 30 seconds
		_url = "nifi://" + _host + ":" + std::to_string(_port);
	}
	//! Destructor
	virtual ~Site2SitePeer() { Close();}
	//! Set Processor yield period in MilliSecond
	void setYieldPeriodMsec(uint64_t period) {
		_yieldPeriodMsec = period;
	}
	//! get URL
	std::string getURL() {
		return _url;
	}
	//! Get Processor yield period in MilliSecond
	uint64_t getYieldPeriodMsec(void) {
		return(_yieldPeriodMsec);
	}
	//! Yield based on the yield period
	void yield()
	{
		_yieldExpiration = (getTimeMillis() + _yieldPeriodMsec);
	}
	//! setHostName
	void setHostName(std::string host)
	{
		_host = host;
		_url = "nifi://" + _host + ":" + std::to_string(_port);
	}
	//! setPort
	void setPort(uint16_t port)
	{
		_port = port;
		_url = "nifi://" + _host + ":" + std::to_string(_port);
	}
	//! getHostName
	std::string getHostName()
	{
		return _host;
	}
	//! getPort
	uint16_t getPort()
	{
		return _port;
	}
	//! Yield based on the input time
	void yield(uint64_t time)
	{
		_yieldExpiration = (getTimeMillis() + time);
	}
	//! whether need be to yield
	bool isYield()
	{
		if (_yieldExpiration > 0)
			return (_yieldExpiration >= getTimeMillis());
		else
			return false;
	}
	// clear yield expiration
	void clearYield()
	{
		_yieldExpiration = 0;
	}
	//! Yield based on the yield period
	void yield(std::string portId)
	{
		std::lock_guard<std::mutex> lock(_mtx);
		uint64_t yieldExpiration = (getTimeMillis() + _yieldPeriodMsec);
		_yieldExpirationPortIdMap[portId] = yieldExpiration;
	}
	//! Yield based on the input time
	void yield(std::string portId, uint64_t time)
	{
		std::lock_guard<std::mutex> lock(_mtx);
		uint64_t yieldExpiration = (getTimeMillis() + time);
		_yieldExpirationPortIdMap[portId] = yieldExpiration;
	}
	//! whether need be to yield
	bool isYield(std::string portId)
	{
		std::lock_guard<std::mutex> lock(_mtx);
		std::map<std::string, uint64_t>::iterator it = this->_yieldExpirationPortIdMap.find(portId);
		if (it != _yieldExpirationPortIdMap.end())
		{
			uint64_t yieldExpiration = it->second;
			return (yieldExpiration >= getTimeMillis());
		}
		else
		{
			return false;
		}
	}
	//! clear yield expiration
	void clearYield(std::string portId)
	{
		std::lock_guard<std::mutex> lock(_mtx);
		std::map<std::string, uint64_t>::iterator it = this->_yieldExpirationPortIdMap.find(portId);
		if (it != _yieldExpirationPortIdMap.end())
		{
			_yieldExpirationPortIdMap.erase(portId);
		}
	}
	//! setTimeOut
	void setTimeOut(uint64_t time)
	{
		_timeOut = time;
	}
	//! getTimeOut
	uint64_t getTimeOut()
	{
		return _timeOut;
	}
	int write(uint8_t value, CRC32 *crc = NULL)
	{
		return sendData(&value, 1, crc);
	}
	int write(char value, CRC32 *crc = NULL)
	{
		return sendData((uint8_t *)&value, 1, crc);
	}
	int write(uint32_t value, CRC32 *crc = NULL)
	{
		uint8_t temp[4];

		temp[0] = (value & 0xFF000000) >> 24;
		temp[1] = (value & 0x00FF0000) >> 16;
		temp[2] = (value & 0x0000FF00) >> 8;
		temp[3] = (value & 0x000000FF);
		return sendData(temp, 4, crc);
	}
	int write(uint16_t value, CRC32 *crc = NULL)
	{
		uint8_t temp[2];
		temp[0] = (value & 0xFF00) >> 8;
		temp[1] = (value & 0xFF);
		return sendData(temp, 2, crc);
	}
	int write(uint8_t *value, int len, CRC32 *crc = NULL)
	{
		return sendData(value, len, crc);
	}
	int write(uint64_t value, CRC32 *crc = NULL)
	{
		uint8_t temp[8];

		temp[0] = (value >> 56) & 0xFF;
		temp[1] = (value >> 48) & 0xFF;
		temp[2] = (value >> 40) & 0xFF;
		temp[3] = (value >> 32) & 0xFF;
		temp[4] = (value >> 24) & 0xFF;
		temp[5] = (value >> 16) & 0xFF;
		temp[6] = (value >>  8) & 0xFF;
		temp[7] = (value >>  0) & 0xFF;
		return sendData(temp, 8, crc);
	}
	int write(bool value, CRC32 *crc = NULL)
	{
		uint8_t temp = value;
		return write(temp, crc);
	}
	int writeUTF(std::string str, bool widen = false, CRC32 *crc = NULL);
	int read(uint8_t &value, CRC32 *crc = NULL)
	{
		uint8_t buf;

		int ret = readData(&buf, 1, crc);
		if (ret == 1)
			value = buf;
		return ret;
	}
	int read(uint16_t &value, CRC32 *crc = NULL)
	{
		uint8_t buf[2];

		int ret = readData(buf, 2, crc);
		if (ret == 2)
			value = (buf[0] << 8) | buf[1];
		return ret;
	}
	int read(char &value, CRC32 *crc = NULL)
	{
		uint8_t buf;

		int ret = readData(&buf, 1, crc);
		if (ret == 1)
			value = (char) buf;
		return ret;
	}
	int read(uint8_t *value, int len, CRC32 *crc = NULL)
	{
		return readData(value, len, crc);
	}
	int read(uint32_t &value, CRC32 *crc = NULL)
	{
		uint8_t buf[4];

		int ret = readData(buf, 4, crc);
		if (ret == 4)
			value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
		return ret;
	}
	int read(uint64_t &value, CRC32 *crc = NULL)
	{
		uint8_t buf[8];

		int ret = readData(buf, 8, crc);
		if (ret == 8)
		{
			value = ((uint64_t) buf[0] << 56) |
					((uint64_t) (buf[1] & 255) << 48) |
					((uint64_t) (buf[2] & 255) << 40) |
					((uint64_t) (buf[3] & 255) << 32) |
					((uint64_t) (buf[4] & 255) << 24) |
					((uint64_t) (buf[5] & 255) << 16) |
					((uint64_t) (buf[6] & 255) <<  8) |
					((uint64_t) (buf[7] & 255) <<  0);
		}
		return ret;
	}
	int readUTF(std::string &str, bool widen = false, CRC32 *crc = NULL);
	//! open connection to the peer
	bool Open();
	//! close connection to the peer
	void Close();
	//! Send Data via the socket, return -1 for failure
	int sendData(uint8_t *buf, int buflen, CRC32 *crc = NULL);
	//! Read length into buf, return -1 for failure and 0 for EOF
	int readData(uint8_t *buf, int buflen, CRC32 *crc = NULL);
	//! Select on the socket
	int Select(int msec);

protected:

private:
	//! Mutex for protection
	std::mutex _mtx;
	//! S2S server Name
	std::string _host;
	//! S2S server port
	uint16_t _port;
	//! socket to server
	int _socket;
	//! URL
	std::string _url;
	//! socket timeout;
	std::atomic<uint64_t> _timeOut;
	//! Logger
	Logger *_logger;
	//! Configure
	Configure *_configure;
	//! Yield Period in Milliseconds
	std::atomic<uint64_t> _yieldPeriodMsec;
	//! Yield Expiration
	std::atomic<uint64_t> _yieldExpiration;
	//! Yield Expiration per destination PortID
	std::map<std::string, uint64_t> _yieldExpirationPortIdMap;
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	Site2SitePeer(const Site2SitePeer &parent);
	Site2SitePeer &operator=(const Site2SitePeer &parent);
};

#endif
