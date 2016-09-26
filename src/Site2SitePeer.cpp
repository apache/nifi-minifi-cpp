/**
 * @file Site2SitePeer.cpp
 * Site2SitePeer class implementation
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
#include <sys/time.h>
#include <stdio.h>
#include <time.h>
#include <chrono>
#include <thread>
#include <random>
#include <netinet/tcp.h>
#include <iostream>
#include "Site2SitePeer.h"

//! CRC tables
std::atomic<bool> CRC32::tableInit(false);
unsigned int CRC32::table[256];

bool Site2SitePeer::Open()
{
	in_addr_t addr;
	int sock = 0;
	struct hostent *h;
	const char *host;
	uint16_t port;

	host = this->_host.c_str();
	port = this->_port;

	if (strlen(host) == 0)
		return false;

#ifdef __MACH__
	h = gethostbyname(host);
#else
	char buf[1024];
	struct hostent he;
	int hh_errno;
	gethostbyname_r(host, &he, buf, sizeof(buf), &h, &hh_errno);
#endif
	memcpy((char *) &addr, h->h_addr_list[0], h->h_length);
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		_logger->log_error("Could not create socket to hostName %s", host);
		this->yield();
		return false;
	}

#ifndef __MACH__
	int opt = 1;
	bool nagle_off = true;

	if (nagle_off)
	{
		if (setsockopt(sock, SOL_TCP, TCP_NODELAY, (void *)&opt, sizeof(opt)) < 0)
		{
			_logger->log_error("setsockopt() TCP_NODELAY failed");
			close(sock);
			this->yield();
			return false;
		}
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
				(char *)&opt, sizeof(opt)) < 0)
		{
			_logger->log_error("setsockopt() SO_REUSEADDR failed");
			close(sock);
			this->yield();
			return false;
		}
	}

	int sndsize = 256*1024;
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sndsize, (int)sizeof(sndsize)) < 0)
	{
		_logger->log_error("setsockopt() SO_SNDBUF failed");
		close(sock);
		this->yield();
		return false;
	}
	int rcvsize = 256*1024;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcvsize, (int)sizeof(rcvsize)) < 0)
	{
		_logger->log_error("setsockopt() SO_RCVBUF failed");
		close(sock);
		this->yield();
		return false;
	}
#endif

	struct sockaddr_in sa;
	socklen_t socklen;
	int status;

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(0);
	socklen = sizeof(sa);
	if (bind(sock, (struct sockaddr *)&sa, socklen) < 0)
	{
		_logger->log_error("socket bind failed");
		close(sock);
		this->yield();
		return false;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = addr;
	sa.sin_port = htons(port);
	socklen = sizeof(sa);

	status = connect(sock, (struct sockaddr *)&sa, socklen);

	if (status < 0)
	{
		_logger->log_error("socket connect failed to %s %d", host, port);
		close(sock);
		this->yield();
		return false;
	}

	_logger->log_info("Site2Site Peer socket %d connect to server %s port %d success", sock, host, port);

	_socket = sock;

	status = sendData((uint8_t *) MAGIC_BYTES, sizeof(MAGIC_BYTES));

	if (status <= 0)
	{
		Close();
		return false;
	}

	return true;
}

void Site2SitePeer::Close()
{
	if (_socket)
	{
		_logger->log_info("Site2Site Peer socket %d close", _socket);
		close(_socket);
		_socket = 0;
	}
}

int Site2SitePeer::sendData(uint8_t *buf, int buflen, CRC32 *crc)
{
	int ret = 0, bytes = 0;

	if (_socket <= 0)
	{
		// this->yield();
		return -1;
	}

	while (bytes < buflen)
	{
		ret = send(_socket, buf+bytes, buflen-bytes, 0);
		//check for errors
		if (ret == -1)
		{
			_logger->log_error("Site2Site Peer socket %d send failed %s", _socket, strerror(errno));
			Close();
			// this->yield();
			return ret;
		}
		bytes+=ret;
	}

	if (crc)
		crc->update(buf, buflen);

	return bytes;
}

int Site2SitePeer::Select(int msec)
{
	fd_set fds;
	struct timeval tv;
    int retval;
    int fd = _socket;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    tv.tv_sec = msec/1000;
    tv.tv_usec = (msec % 1000) * 1000;

    if (msec > 0)
       retval = select(fd+1, &fds, NULL, NULL, &tv);
    else
       retval = select(fd+1, &fds, NULL, NULL, NULL);

    if (retval <= 0)
      return retval;
    if (FD_ISSET(fd, &fds))
      return retval;
    else
      return 0;
}

int Site2SitePeer::readData(uint8_t *buf, int buflen, CRC32 *crc)
{
	int sendSize = buflen;
	uint8_t *start = buf;

	if (_socket <= 0)
	{
		// this->yield();
		return -1;
	}

	while (buflen)
	{
		int status;
		status = Select((int) _timeOut);
		if (status <= 0)
		{
			Close();
			return status;
		}
		status = recv(_socket, buf, buflen, 0);
		if (status <= 0)
		{
			Close();
			// this->yield();
			return status;
		}
		buflen -= status;
		buf += status;
	}

	if (crc)
		crc->update(start, sendSize);

	return sendSize;
}

int Site2SitePeer::writeUTF(std::string str, bool widen, CRC32 *crc)
{
	int strlen = str.length();
	int utflen = 0;
	int c, count = 0;

	/* use charAt instead of copying String to char array */
	for (int i = 0; i < strlen; i++) {
		c = str.at(i);
		if ((c >= 0x0001) && (c <= 0x007F)) {
			utflen++;
		} else if (c > 0x07FF) {
			utflen += 3;
		} else {
			utflen += 2;
		}
	}

	if (utflen > 65535)
		return -1;

	uint8_t *bytearr = NULL;
	if (!widen)
	{
		bytearr = new uint8_t[utflen+2];
		bytearr[count++] = (uint8_t) ((utflen >> 8) & 0xFF);
		bytearr[count++] = (uint8_t) ((utflen >> 0) & 0xFF);
	}
	else
	{
		bytearr = new uint8_t[utflen+4];
		bytearr[count++] = (uint8_t) ((utflen >> 24) & 0xFF);
		bytearr[count++] = (uint8_t) ((utflen >> 16) & 0xFF);
		bytearr[count++] = (uint8_t) ((utflen >> 8) & 0xFF);
		bytearr[count++] = (uint8_t) ((utflen >> 0) & 0xFF);
	}

	int i=0;
	for (i=0; i<strlen; i++) {
		c = str.at(i);
		if (!((c >= 0x0001) && (c <= 0x007F))) break;
		bytearr[count++] = (uint8_t) c;
	}

	for (;i < strlen; i++){
		c = str.at(i);
		if ((c >= 0x0001) && (c <= 0x007F)) {
			bytearr[count++] = (uint8_t) c;
		} else if (c > 0x07FF) {
			bytearr[count++] = (uint8_t) (0xE0 | ((c >> 12) & 0x0F));
			bytearr[count++] = (uint8_t) (0x80 | ((c >>  6) & 0x3F));
			bytearr[count++] = (uint8_t) (0x80 | ((c >>  0) & 0x3F));
		} else {
			bytearr[count++] = (uint8_t) (0xC0 | ((c >>  6) & 0x1F));
			bytearr[count++] = (uint8_t) (0x80 | ((c >>  0) & 0x3F));
		}
	}
	int ret;
	if (!widen)
	{
		ret = sendData(bytearr, utflen+2, crc);
	}
	else
	{
		ret = sendData(bytearr, utflen+4, crc);
	}
	delete[] bytearr;
	return ret;
}

int Site2SitePeer::readUTF(std::string &str, bool widen, CRC32 *crc)
{
    uint16_t utflen;
    int ret;

    if (!widen)
    {
    	ret = read(utflen, crc);
    	if (ret <= 0)
    		return ret;
    }
    else
    {
    	uint32_t len;
       	ret = read(len, crc);
        if (ret <= 0)
        	return ret;
        utflen = len;
    }

    uint8_t *bytearr = NULL;
    char *chararr = NULL;
    bytearr = new uint8_t[utflen];
    chararr = new char[utflen];
    memset(chararr, 0, utflen);

    int c, char2, char3;
    int count = 0;
    int chararr_count=0;

    ret = read(bytearr, utflen, crc);
    if (ret <= 0)
    {
    	delete[] bytearr;
    	delete[] chararr;
    	return ret;
    }

    while (count < utflen) {
        c = (int) bytearr[count] & 0xff;
        if (c > 127) break;
        count++;
        chararr[chararr_count++]=(char)c;
    }

    while (count < utflen) {
        c = (int) bytearr[count] & 0xff;
        switch (c >> 4) {
            case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
                /* 0xxxxxxx*/
                count++;
                chararr[chararr_count++]=(char)c;
                break;
            case 12: case 13:
                /* 110x xxxx   10xx xxxx*/
                count += 2;
                if (count > utflen)
                {
                	delete[] bytearr;
                	delete[] chararr;
                	return -1;
                }
                char2 = (int) bytearr[count-1];
                if ((char2 & 0xC0) != 0x80)
                {
                	delete[] bytearr;
                	delete[] chararr;
                	return -1;
                }
                chararr[chararr_count++]=(char)(((c & 0x1F) << 6) |
                                                (char2 & 0x3F));
                break;
            case 14:
                /* 1110 xxxx  10xx xxxx  10xx xxxx */
                count += 3;
                if (count > utflen)
                {
                	delete[] bytearr;
                	delete[] chararr;
                	return -1;
                }
                char2 = (int) bytearr[count-2];
                char3 = (int) bytearr[count-1];
                if (((char2 & 0xC0) != 0x80) || ((char3 & 0xC0) != 0x80))
                {
                	delete[] bytearr;
                	delete[] chararr;
                	return -1;
                }
                chararr[chararr_count++]=(char)(((c     & 0x0F) << 12) |
                                                ((char2 & 0x3F) << 6)  |
                                                ((char3 & 0x3F) << 0));
                break;
            default:
            	delete[] bytearr;
            	delete[] chararr;
            	return -1;
        }
    }
    // The number of chars produced may be less than utflen
    std::string value(chararr, chararr_count);
    str = value;
    delete[] bytearr;
    delete[] chararr;
    if (!widen)
    	return (2 + utflen);
    else
    	return (4 + utflen);
}
