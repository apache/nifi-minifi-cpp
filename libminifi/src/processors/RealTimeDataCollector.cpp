/**
 * @file RealTimeDataCollector.cpp
 * RealTimeDataCollector class implementation
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
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <sys/time.h>
#include <time.h>
#include <chrono>
#include <thread>
#include <memory>
#include <random>
#include <netinet/tcp.h>

#include "utils/StringUtils.h"
#include "processors/RealTimeDataCollector.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
const std::string RealTimeDataCollector::ProcessorName("RealTimeDataCollector");
core::Property RealTimeDataCollector::FILENAME(
    "File Name", "File Name for the real time processor to process",
    "data.osp");
core::Property RealTimeDataCollector::REALTIMESERVERNAME(
    "Real Time Server Name", "Real Time Server Name", "localhost");
core::Property RealTimeDataCollector::REALTIMESERVERPORT(
    "Real Time Server Port", "Real Time Server Port", "10000");
core::Property RealTimeDataCollector::BATCHSERVERNAME(
    "Batch Server Name", "Batch Server Name", "localhost");
core::Property RealTimeDataCollector::BATCHSERVERPORT(
    "Batch Server Port", "Batch Server Port", "10001");
core::Property RealTimeDataCollector::ITERATION(
    "Iteration", "If true, sample osp file will be iterated", "true");
core::Property RealTimeDataCollector::REALTIMEMSGID(
    "Real Time Message ID", "Real Time Message ID", "41");
core::Property RealTimeDataCollector::BATCHMSGID(
    "Batch Message ID", "Batch Message ID", "172, 30, 48");
core::Property RealTimeDataCollector::REALTIMEINTERVAL(
    "Real Time Interval", "Real Time Data Collection Interval in msec",
    "10 ms");
core::Property RealTimeDataCollector::BATCHINTERVAL(
    "Batch Time Interval", "Batch Processing Interval in msec", "100 ms");
core::Property RealTimeDataCollector::BATCHMAXBUFFERSIZE(
    "Batch Max Buffer Size", "Batch Buffer Maximum size in bytes", "262144");
core::Relationship RealTimeDataCollector::Success(
    "success", "success operational on the flow record");

void RealTimeDataCollector::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(FILENAME);
  properties.insert(REALTIMESERVERNAME);
  properties.insert(REALTIMESERVERPORT);
  properties.insert(BATCHSERVERNAME);
  properties.insert(BATCHSERVERPORT);
  properties.insert(ITERATION);
  properties.insert(REALTIMEMSGID);
  properties.insert(BATCHMSGID);
  properties.insert(REALTIMEINTERVAL);
  properties.insert(BATCHINTERVAL);
  properties.insert(BATCHMAXBUFFERSIZE);

  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);

}

int RealTimeDataCollector::connectServer(const char *host, uint16_t port) {
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
  if (sock < 0) {
    logger_->log_error("Could not create socket to hostName %s", host);
    return 0;
  }

#ifndef __MACH__
  int opt = 1;
  bool nagle_off = true;

  if (nagle_off)
  {
    if (setsockopt(sock, SOL_TCP, TCP_NODELAY, (void *)&opt, sizeof(opt)) < 0)
    {
      logger_->log_error("setsockopt() TCP_NODELAY failed");
      close(sock);
      return 0;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
            (char *)&opt, sizeof(opt)) < 0)
    {
      logger_->log_error("setsockopt() SO_REUSEADDR failed");
      close(sock);
      return 0;
    }
  }

  int sndsize = 256*1024;
  if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sndsize, (int)sizeof(sndsize)) < 0)
  {
    logger_->log_error("setsockopt() SO_SNDBUF failed");
    close(sock);
    return 0;
  }
#endif

  struct sockaddr_in sa;
  socklen_t socklen;
  int status;

  //TODO bind socket to the interface
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  sa.sin_port = htons(0);
  socklen = sizeof(sa);
  if (bind(sock, (struct sockaddr *) &sa, socklen) < 0) {
    logger_->log_error("socket bind failed");
    close(sock);
    return 0;
  }

  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = addr;
  sa.sin_port = htons(port);
  socklen = sizeof(sa);

  status = connect(sock, (struct sockaddr *) &sa, socklen);

  if (status < 0) {
    logger_->log_error("socket connect failed to %s %d", host, port);
    close(sock);
    return 0;
  }

  logger_->log_info("socket %d connect to server %s port %d success", sock,
                    host, port);

  return sock;
}

int RealTimeDataCollector::sendData(int socket, const char *buf, int buflen) {
  int ret = 0, bytes = 0;

  while (bytes < buflen) {
    ret = send(socket, buf + bytes, buflen - bytes, 0);
    //check for errors
    if (ret == -1) {
      return ret;
    }
    bytes += ret;
  }

  if (ret)
    logger_->log_debug("Send data size %d over socket %d", buflen, socket);

  return ret;
}

void RealTimeDataCollector::onTriggerRealTime(
    core::ProcessContext *context,
    core::ProcessSession *session) {
  if (_realTimeAccumulated >= this->_realTimeInterval) {
    std::string value;
    if (this->getProperty(REALTIMEMSGID.getName(), value)) {
      this->_realTimeMsgID.clear();
      this->logger_->log_info("Real Time Msg IDs %s", value.c_str());
      std::stringstream lineStream(value);
      std::string cell;

      while (std::getline(lineStream, cell, ',')) {
        this->_realTimeMsgID.push_back(cell);
        // this->logger_->log_debug("Real Time Msg ID %s", cell.c_str());
      }
    }
    if (this->getProperty(BATCHMSGID.getName(), value)) {
      this->_batchMsgID.clear();
      this->logger_->log_info("Batch Msg IDs %s", value.c_str());
      std::stringstream lineStream(value);
      std::string cell;

      while (std::getline(lineStream, cell, ',')) {
        cell = org::apache::nifi::minifi::utils::StringUtils::trim(cell);
        this->_batchMsgID.push_back(cell);
      }
    }
    // Open the file
    if (!this->_fileStream.is_open()) {
      _fileStream.open(this->_fileName.c_str(), std::ifstream::in);
      if (this->_fileStream.is_open())
        logger_->log_debug("open %s", _fileName.c_str());
    }
    if (!_fileStream.good()) {
      logger_->log_error("load data file failed %s", _fileName.c_str());
      return;
    }
    if (this->_fileStream.is_open()) {
      std::string line;

      while (std::getline(_fileStream, line)) {
        line += "\n";
        std::stringstream lineStream(line);
        std::string cell;
        if (std::getline(lineStream, cell, ',')) {
          cell = org::apache::nifi::minifi::utils::StringUtils::trim(cell);
          // Check whether it match to the batch traffic
          for (std::vector<std::string>::iterator it = _batchMsgID.begin();
              it != _batchMsgID.end(); ++it) {
            if (cell == *it) {
              // push the batch data to the queue
              std::lock_guard<std::mutex> lock(mutex_);
              while ((_queuedDataSize + line.size()) > _batchMaxBufferSize) {
                std::string item = _queue.front();
                _queuedDataSize -= item.size();
                logger_->log_debug(
                    "Pop item size %d from batch queue, queue buffer size %d",
                    item.size(), _queuedDataSize);
                _queue.pop();
              }
              _queue.push(line);
              _queuedDataSize += line.size();
              logger_->log_debug(
                  "Push batch msg ID %s into batch queue, queue buffer size %d",
                  cell.c_str(), _queuedDataSize);
            }
          }
          bool findRealTime = false;
          // Check whether it match to the real time traffic
          for (std::vector<std::string>::iterator it = _realTimeMsgID.begin();
              it != _realTimeMsgID.end(); ++it) {
            if (cell == *it) {
              int status = 0;
              if (this->_realTimeSocket <= 0) {
                // Connect the LTE socket
                uint16_t port = _realTimeServerPort;
                this->_realTimeSocket = connectServer(
                    _realTimeServerName.c_str(), port);
              }
              if (this->_realTimeSocket) {
                // try to send the data
                status = sendData(_realTimeSocket, line.data(), line.size());
                if (status < 0) {
                  close(_realTimeSocket);
                  _realTimeSocket = 0;
                }
              }
              if (this->_realTimeSocket <= 0 || status < 0) {
                // push the batch data to the queue
                std::lock_guard<std::mutex> lock(mutex_);
                while ((_queuedDataSize + line.size()) > _batchMaxBufferSize) {
                  std::string item = _queue.front();
                  _queuedDataSize -= item.size();
                  logger_->log_debug(
                      "Pop item size %d from batch queue, queue buffer size %d",
                      item.size(), _queuedDataSize);
                  _queue.pop();
                }
                _queue.push(line);
                _queuedDataSize += line.size();
                logger_->log_debug(
                    "Push real time msg ID %s into batch queue, queue buffer size %d",
                    cell.c_str(), _queuedDataSize);
              }
              // find real time
              findRealTime = true;
            }  // cell
          }  // for real time pattern
          if (findRealTime)
            // we break the while once we find the first real time
            break;
        }  // if get line
      }  // while
      if (_fileStream.eof()) {
        _fileStream.close();
      }
    }  // if open
    _realTimeAccumulated = 0;
  }
  std::shared_ptr<Processor> processor = std::static_pointer_cast<Processor>(
      context->getProcessorNode().getProcessor());
  _realTimeAccumulated += processor->getSchedulingPeriodNano();
}

void RealTimeDataCollector::onTriggerBatch(
    core::ProcessContext *context,
    core::ProcessSession *session) {
  if (_batchAcccumulated >= this->_batchInterval) {
    // logger_->log_info("onTriggerBatch");
    // dequeue the batch and send over WIFI
    int status = 0;
    if (this->_batchSocket <= 0) {
      // Connect the WIFI socket
      uint16_t port = _batchServerPort;
      this->_batchSocket = connectServer(_batchServerName.c_str(), port);
    }
    if (this->_batchSocket) {
      std::lock_guard<std::mutex> lock(mutex_);

      while (!_queue.empty()) {
        std::string line = _queue.front();
        status = sendData(_batchSocket, line.data(), line.size());
        _queue.pop();
        _queuedDataSize -= line.size();
        if (status < 0) {
          close(_batchSocket);
          _batchSocket = 0;
          break;
        }
      }
    }
    _batchAcccumulated = 0;
  }
  std::shared_ptr<Processor> processor = std::static_pointer_cast<Processor>(
      context->getProcessorNode().getProcessor());
  _batchAcccumulated += processor->getSchedulingPeriodNano();
}

void RealTimeDataCollector::onTrigger(
    core::ProcessContext *context,
    core::ProcessSession *session) {
  std::thread::id id = std::this_thread::get_id();

  if (id == _realTimeThreadId)
    return onTriggerRealTime(context, session);
  else if (id == _batchThreadId)
    return onTriggerBatch(context, session);
  else {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!this->_firstInvoking) {
      this->_fileName = "data.osp";
      std::string value;
      if (this->getProperty(FILENAME.getName(), value)) {
        this->_fileName = value;
        this->logger_->log_info("Data Collector File Name %s",
                                _fileName.c_str());
      }
      this->_realTimeServerName = "localhost";
      if (this->getProperty(REALTIMESERVERNAME.getName(), value)) {
        this->_realTimeServerName = value;
        this->logger_->log_info("Real Time Server Name %s",
                                this->_realTimeServerName.c_str());
      }
      this->_realTimeServerPort = 10000;
      if (this->getProperty(REALTIMESERVERPORT.getName(), value)) {
        core::Property::StringToInt(
            value, _realTimeServerPort);
        this->logger_->log_info("Real Time Server Port %d",
                                _realTimeServerPort);
      }
      if (this->getProperty(BATCHSERVERNAME.getName(), value)) {
        this->_batchServerName = value;
        this->logger_->log_info("Batch Server Name %s",
                                this->_batchServerName.c_str());
      }
      this->_batchServerPort = 10001;
      if (this->getProperty(BATCHSERVERPORT.getName(), value)) {
        core::Property::StringToInt(
            value, _batchServerPort);
        this->logger_->log_info("Batch Server Port %d", _batchServerPort);
      }
      if (this->getProperty(ITERATION.getName(), value)) {
        org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, this->_iteration);
        logger_->log_info("Iteration %d", _iteration);
      }
      this->_realTimeInterval = 10000000;  //10 msec
      if (this->getProperty(REALTIMEINTERVAL.getName(), value)) {
        core::TimeUnit unit;
        if (core::Property::StringToTime(
            value, _realTimeInterval, unit)
            && core::Property::ConvertTimeUnitToNS(
                _realTimeInterval, unit, _realTimeInterval)) {
          logger_->log_info("Real Time Interval: [%d] ns", _realTimeInterval);
        }
      }
      this->_batchInterval = 100000000;  //100 msec
      if (this->getProperty(BATCHINTERVAL.getName(), value)) {
        core::TimeUnit unit;
        if (core::Property::StringToTime(
            value, _batchInterval, unit)
            && core::Property::ConvertTimeUnitToNS(
                _batchInterval, unit, _batchInterval)) {
          logger_->log_info("Batch Time Interval: [%d] ns", _batchInterval);
        }
      }
      this->_batchMaxBufferSize = 256 * 1024;
      if (this->getProperty(BATCHMAXBUFFERSIZE.getName(), value)) {
        core::Property::StringToInt(
            value, _batchMaxBufferSize);
        this->logger_->log_info("Batch Max Buffer Size %d",
                                _batchMaxBufferSize);
      }
      if (this->getProperty(REALTIMEMSGID.getName(), value)) {
        this->logger_->log_info("Real Time Msg IDs %s", value.c_str());
        std::stringstream lineStream(value);
        std::string cell;

        while (std::getline(lineStream, cell, ',')) {
          this->_realTimeMsgID.push_back(cell);
          this->logger_->log_info("Real Time Msg ID %s", cell.c_str());
        }
      }
      if (this->getProperty(BATCHMSGID.getName(), value)) {
        this->logger_->log_info("Batch Msg IDs %s", value.c_str());
        std::stringstream lineStream(value);
        std::string cell;

        while (std::getline(lineStream, cell, ',')) {
          cell = org::apache::nifi::minifi::utils::StringUtils::trim(cell);
          this->_batchMsgID.push_back(cell);
          this->logger_->log_info("Batch Msg ID %s", cell.c_str());
        }
      }
      // Connect the LTE socket
      uint16_t port = _realTimeServerPort;

      this->_realTimeSocket = connectServer(_realTimeServerName.c_str(), port);

      // Connect the WIFI socket
      port = _batchServerPort;

      this->_batchSocket = connectServer(_batchServerName.c_str(), port);

      // Open the file
      _fileStream.open(this->_fileName.c_str(), std::ifstream::in);
      if (!_fileStream.good()) {
        logger_->log_error("load data file failed %s", _fileName.c_str());
        return;
      } else {
        logger_->log_debug("open %s", _fileName.c_str());
      }
      _realTimeThreadId = id;
      this->_firstInvoking = true;
    } else {
      if (id != _realTimeThreadId)
        _batchThreadId = id;
      this->_firstInvoking = false;
    }
  }
}
} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
