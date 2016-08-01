/**
 * @file FlowControlProtocol.cpp
 * FlowControlProtocol class implementation
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
#include "FlowController.h"
#include "FlowControlProtocol.h"

int FlowControlProtocol::connectServer(const char *host, uint16_t port)
{
	in_addr_t addr;
	int sock = 0;
	struct hostent *h;
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
		return 0;
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
			return 0;
		}
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
				(char *)&opt, sizeof(opt)) < 0)
		{
			_logger->log_error("setsockopt() SO_REUSEADDR failed");
			close(sock);
			return 0;
		}
	}

	int sndsize = 256*1024;
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sndsize, (int)sizeof(sndsize)) < 0)
	{
		_logger->log_error("setsockopt() SO_SNDBUF failed");
		close(sock);
		return 0;
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
		return 0;
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
		return 0;
	}

	_logger->log_info("Flow Control Protocol socket %d connect to server %s port %d success", sock, host, port);

	return sock;
}

int FlowControlProtocol::sendData(uint8_t *buf, int buflen)
{
	int ret = 0, bytes = 0;

	while (bytes < buflen)
	{
		ret = send(_socket, buf+bytes, buflen-bytes, 0);
		//check for errors
		if (ret == -1)
		{
			return ret;
		}
		bytes+=ret;
	}

	return bytes;
}

int FlowControlProtocol::selectClient(int msec)
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

int FlowControlProtocol::readData(uint8_t *buf, int buflen)
{
	int sendSize = buflen;

	while (buflen)
	{
		int status;
		status = selectClient(MAX_READ_TIMEOUT);
		if (status <= 0)
		{
			return status;
		}
#ifndef __MACH__
		status = read(_socket, buf, buflen);
#else
		status = recv(_socket, buf, buflen, 0);
#endif
		if (status <= 0)
		{
			return status;
		}
		buflen -= status;
		buf += status;
	}

	return sendSize;
}

int FlowControlProtocol::readHdr(FlowControlProtocolHeader *hdr)
{
	uint8_t buffer[sizeof(FlowControlProtocolHeader)];

	uint8_t *data = buffer;

	int status = readData(buffer, sizeof(FlowControlProtocolHeader));
	if (status <= 0)
		return status;

	uint32_t value;
	data = this->decode(data, value);
	hdr->msgType = value;

	data = this->decode(data, value);
	hdr->seqNumber = value;

	data = this->decode(data, value);
	hdr->status = value;

	data = this->decode(data, value);
	hdr->payloadLen = value;

	return sizeof(FlowControlProtocolHeader);
}

void FlowControlProtocol::start()
{
	if (_reportInterval <= 0)
		return;
	if (_running)
		return;
	_running = true;
	_logger->log_info("FlowControl Protocol Start");
	_thread = new std::thread(run, this);
	_thread->detach();
}

void FlowControlProtocol::stop()
{
	if (!_running)
		return;
	_running = false;
	_logger->log_info("FlowControl Protocol Stop");
	delete _thread;
}

void FlowControlProtocol::run(FlowControlProtocol *protocol)
{
	while (protocol->_running)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(protocol->_reportInterval));
		if (!protocol->_registered)
		{
			// if it is not register yet
			protocol->sendRegisterReq();
			// protocol->_controller->reload("flow.xml");
		}
		else
			protocol->sendReportReq();
	}
	return;
}

int FlowControlProtocol::sendRegisterReq()
{
	if (_registered)
	{
		_logger->log_info("Already registered");
		return -1;
	}

	uint16_t port = this->_serverPort;

	if (this->_socket <= 0)
		this->_socket = connectServer(_serverName.c_str(), port);

	if (this->_socket <= 0)
		return -1;

	// Calculate the total payload msg size
	uint32_t payloadSize = FlowControlMsgIDEncodingLen(FLOW_SERIAL_NUMBER, 0) +
			FlowControlMsgIDEncodingLen(FLOW_XML_NAME, this->_controller->getName().size()+1);
	uint32_t size = sizeof(FlowControlProtocolHeader) + payloadSize;

	uint8_t *data = new uint8_t[size];
	uint8_t *start = data;

	// encode the HDR
	FlowControlProtocolHeader hdr;
	hdr.msgType = REGISTER_REQ;
	hdr.payloadLen = payloadSize;
	hdr.seqNumber  = this->_seqNumber;
	hdr.status = RESP_SUCCESS;
	data = this->encode(data, hdr.msgType);
	data = this->encode(data, hdr.seqNumber);
	data = this->encode(data, hdr.status);
	data = this->encode(data, hdr.payloadLen);

	// encode the serial number
	data = this->encode(data, FLOW_SERIAL_NUMBER);
	data = this->encode(data, this->_serialNumber, 8);

	// encode the XML name
	data = this->encode(data, FLOW_XML_NAME);
	data = this->encode(data, this->_controller->getName());

	// send it
	int status = sendData(start, size);
	delete[] start;
	if (status <= 0)
	{
		close(_socket);
		_socket = 0;
		_logger->log_error("Flow Control Protocol Send Register Req failed");
		return -1;
	}

	// Looking for register respond
	status = readHdr(&hdr);

	if (status <= 0)
	{
		close(_socket);
		_socket = 0;
		_logger->log_error("Flow Control Protocol Read Register Resp header failed");
		return -1;
	}
	_logger->log_info("Flow Control Protocol receive MsgType %s", FlowControlMsgTypeToStr((FlowControlMsgType) hdr.msgType));
	_logger->log_info("Flow Control Protocol receive Seq Num %d", hdr.seqNumber);
	_logger->log_info("Flow Control Protocol receive Resp Code %s", FlowControlRespCodeToStr((FlowControlRespCode) hdr.status));
	_logger->log_info("Flow Control Protocol receive Payload len %d", hdr.payloadLen);

	if (hdr.status == RESP_SUCCESS && hdr.seqNumber == this->_seqNumber)
	{
		this->_registered = true;
		this->_seqNumber++;
		_logger->log_info("Flow Control Protocol Register success");
		uint8_t *payload = new uint8_t[hdr.payloadLen];
		uint8_t *payloadPtr = payload;
		status = readData(payload, hdr.payloadLen);
		if (status <= 0)
		{
			delete[] payload;
			_logger->log_info("Flow Control Protocol Register Read Payload fail");
			close(_socket);
			_socket = 0;
			return -1;
		}
		while (payloadPtr < (payload + hdr.payloadLen))
		{
			uint32_t msgID;
			payloadPtr = this->decode(payloadPtr, msgID);
			if (((FlowControlMsgID) msgID) == REPORT_INTERVAL)
			{
				// Fixed 4 bytes
				uint32_t reportInterval;
				payloadPtr = this->decode(payloadPtr, reportInterval);
				_logger->log_info("Flow Control Protocol receive report interval %d ms", reportInterval);
				this->_reportInterval = reportInterval;
			}
			else if (((FlowControlMsgID) msgID) == FLOW_XML_CONTENT)
			{
				uint32_t xmlLen;
				payloadPtr = this->decode(payloadPtr, xmlLen);
				_logger->log_info("Flow Control Protocol receive XML content length %d", xmlLen);
				time_t rawtime;
				struct tm *timeinfo;
				time(&rawtime);
				timeinfo = localtime(&rawtime);
				std::string xmlFileName = "flow.";
				xmlFileName += asctime(timeinfo);
				xmlFileName += ".xml";
				std::ofstream fs;
				fs.open(xmlFileName.c_str(), std::fstream::out | std::fstream::binary | std::fstream::trunc);
				if (fs.is_open())
				{
					fs.write((const char *)payloadPtr, xmlLen);
					fs.close();
					this->_controller->reload(xmlFileName.c_str());
				}
			}
			else
			{
				break;
			}
		}
		delete[] payload;
		close(_socket);
		_socket = 0;
		return 0;
	}
	else
	{
		_logger->log_info("Flow Control Protocol Register fail");
		close(_socket);
		_socket = 0;
		return -1;
	}
}


int FlowControlProtocol::sendReportReq()
{
	uint16_t port = this->_serverPort;

	if (this->_socket <= 0)
		this->_socket = connectServer(_serverName.c_str(), port);

	if (this->_socket <= 0)
		return -1;

	// Calculate the total payload msg size
	uint32_t payloadSize =
			FlowControlMsgIDEncodingLen(FLOW_XML_NAME, this->_controller->getName().size()+1);
	uint32_t size = sizeof(FlowControlProtocolHeader) + payloadSize;

	uint8_t *data = new uint8_t[size];
	uint8_t *start = data;

	// encode the HDR
	FlowControlProtocolHeader hdr;
	hdr.msgType = REPORT_REQ;
	hdr.payloadLen = payloadSize;
	hdr.seqNumber  = this->_seqNumber;
	hdr.status = RESP_SUCCESS;
	data = this->encode(data, hdr.msgType);
	data = this->encode(data, hdr.seqNumber);
	data = this->encode(data, hdr.status);
	data = this->encode(data, hdr.payloadLen);

	// encode the XML name
	data = this->encode(data, FLOW_XML_NAME);
	data = this->encode(data, this->_controller->getName());

	// send it
	int status = sendData(start, size);
	delete[] start;
	if (status <= 0)
	{
		close(_socket);
		_socket = 0;
		_logger->log_error("Flow Control Protocol Send Report Req failed");
		return -1;
	}

	// Looking for report respond
	status = readHdr(&hdr);

	if (status <= 0)
	{
		close(_socket);
		_socket = 0;
		_logger->log_error("Flow Control Protocol Read Report Resp header failed");
		return -1;
	}
	_logger->log_info("Flow Control Protocol receive MsgType %s", FlowControlMsgTypeToStr((FlowControlMsgType) hdr.msgType));
	_logger->log_info("Flow Control Protocol receive Seq Num %d", hdr.seqNumber);
	_logger->log_info("Flow Control Protocol receive Resp Code %s", FlowControlRespCodeToStr((FlowControlRespCode) hdr.status));
	_logger->log_info("Flow Control Protocol receive Payload len %d", hdr.payloadLen);

	if (hdr.status == RESP_SUCCESS && hdr.seqNumber == this->_seqNumber)
	{
		this->_seqNumber++;
		uint8_t *payload = new uint8_t[hdr.payloadLen];
		uint8_t *payloadPtr = payload;
		status = readData(payload, hdr.payloadLen);
		if (status <= 0)
		{
			delete[] payload;
			_logger->log_info("Flow Control Protocol Report Resp Read Payload fail");
			close(_socket);
			_socket = 0;
			return -1;
		}
		std::string processor;
		std::string propertyName;
		std::string propertyValue;
		while (payloadPtr < (payload + hdr.payloadLen))
		{
			uint32_t msgID;
			payloadPtr = this->decode(payloadPtr, msgID);
			if (((FlowControlMsgID) msgID) == PROCESSOR_NAME)
			{
				uint32_t len;
				payloadPtr = this->decode(payloadPtr, len);
				processor = (const char *) payloadPtr;
				payloadPtr += len;
				_logger->log_info("Flow Control Protocol receive report resp processor %s", processor.c_str());
			}
			else if (((FlowControlMsgID) msgID) == PROPERTY_NAME)
			{
				uint32_t len;
				payloadPtr = this->decode(payloadPtr, len);
				propertyName = (const char *) payloadPtr;
				payloadPtr += len;
				_logger->log_info("Flow Control Protocol receive report resp property name %s", propertyName.c_str());
			}
			else if (((FlowControlMsgID) msgID) == PROPERTY_VALUE)
			{
				uint32_t len;
				payloadPtr = this->decode(payloadPtr, len);
				propertyValue = (const char *) payloadPtr;
				payloadPtr += len;
				_logger->log_info("Flow Control Protocol receive report resp property value %s", propertyValue.c_str());
				this->_controller->updatePropertyValue(processor, propertyName, propertyValue);
			}
			else
			{
				break;
			}
		}
		delete[] payload;
		close(_socket);
		_socket = 0;
		return 0;
	}
	else if (hdr.status == RESP_TRIGGER_REGISTER && hdr.seqNumber == this->_seqNumber)
	{
		_logger->log_info("Flow Control Protocol trigger reregister");
		this->_registered = false;
		this->_seqNumber++;
		close(_socket);
		_socket = 0;
		return 0;
	}
	else if (hdr.status == RESP_STOP_FLOW_CONTROLLER && hdr.seqNumber == this->_seqNumber)
	{
		_logger->log_info("Flow Control Protocol stop flow controller");
		this->_controller->stop(true);
		this->_seqNumber++;
		close(_socket);
		_socket = 0;
		return 0;
	}
	else if (hdr.status == RESP_START_FLOW_CONTROLLER && hdr.seqNumber == this->_seqNumber)
	{
		_logger->log_info("Flow Control Protocol start flow controller");
		this->_controller->start();
		this->_seqNumber++;
		close(_socket);
		_socket = 0;
		return 0;
	}
	else
	{
		_logger->log_info("Flow Control Protocol Report fail");
		close(_socket);
		_socket = 0;
		return -1;
	}
}

