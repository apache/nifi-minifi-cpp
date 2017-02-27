/**
 * @file RealTimeDataCollector.h
 * RealTimeDataCollector class declaration
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
#ifndef __REAL_TIME_DATA_COLLECTOR_H__
#define __REAL_TIME_DATA_COLLECTOR_H__

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
#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

//! RealTimeDataCollector Class
class RealTimeDataCollector : public Processor {
 public:
  //! Constructor
  /*!
   * Create a new processor
   */
  RealTimeDataCollector(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid) {
    _realTimeSocket = 0;
    _batchSocket = 0;
    logger_ = Logger::getLogger();
    _firstInvoking = false;
    _realTimeAccumulated = 0;
    _batchAcccumulated = 0;
    _queuedDataSize = 0;
  }
  //! Destructor
  virtual ~RealTimeDataCollector() {
    if (_realTimeSocket)
      close(_realTimeSocket);
    if (_batchSocket)
      close(_batchSocket);
    if (_fileStream.is_open())
      _fileStream.close();
  }
  //! Processor Name
  static const std::string ProcessorName;
  //! Supported Properties
  static Property REALTIMESERVERNAME;
  static Property REALTIMESERVERPORT;
  static Property BATCHSERVERNAME;
  static Property BATCHSERVERPORT;
  static Property FILENAME;
  static Property ITERATION;
  static Property REALTIMEMSGID;
  static Property BATCHMSGID;
  static Property REALTIMEINTERVAL;
  static Property BATCHINTERVAL;
  static Property BATCHMAXBUFFERSIZE;
  //! Supported Relationships
  static Relationship Success;
  //! Connect to the socket
  int connectServer(const char *host, uint16_t port);
  int sendData(int socket, const char *buf, int buflen);
  void onTriggerRealTime(ProcessContext *context, ProcessSession *session);
  void onTriggerBatch(ProcessContext *context, ProcessSession *session);

 public:
  //! OnTrigger method, implemented by NiFi RealTimeDataCollector
  virtual void onTrigger(ProcessContext *context, ProcessSession *session);
  //! Initialize, over write by NiFi RealTimeDataCollector
  virtual void initialize(void);

 protected:

 private:
  //! realtime server Name
  std::string _realTimeServerName;
  int64_t _realTimeServerPort;
  std::string _batchServerName;
  int64_t _batchServerPort;
  int64_t _realTimeInterval;
  int64_t _batchInterval;
  int64_t _batchMaxBufferSize;
  //! Match pattern for Real time Message ID
  std::vector<std::string> _realTimeMsgID;
  //! Match pattern for Batch Message ID
  std::vector<std::string> _batchMsgID;
  //! file for which the realTime collector will tail
  std::string _fileName;
  //! Whether we need to iterate from the beginning for demo
  bool _iteration;
  int _realTimeSocket;
  int _batchSocket;
  //! Logger
  std::shared_ptr<Logger> logger_;
  //! Mutex for protection
  std::mutex _mtx;
  //! Queued data size
  uint64_t _queuedDataSize;
  //! Queue for the batch process
  std::queue<std::string> _queue;
  std::thread::id _realTimeThreadId;
  std::thread::id _batchThreadId;
  std::atomic<bool> _firstInvoking;
  int64_t _realTimeAccumulated;
  int64_t _batchAcccumulated;
  std::ifstream _fileStream;
};

#endif
