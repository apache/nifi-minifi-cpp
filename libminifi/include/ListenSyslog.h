/**
 * @file ListenSyslog.h
 * ListenSyslog class declaration
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
#ifndef __LISTEN_SYSLOG_H__
#define __LISTEN_SYSLOG_H__

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <chrono>
#include <thread>
#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

//! SyslogEvent
typedef struct {
  uint8_t *payload;
  uint64_t len;
} SysLogEvent;

//! ListenSyslog Class
class ListenSyslog : public Processor {
 public:
  //! Constructor
  /*!
   * Create a new processor
   */
  ListenSyslog(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid) {
    logger_ = Logger::getLogger();
    _eventQueueByteSize = 0;
    _serverSocket = 0;
    _recvBufSize = 65507;
    _maxSocketBufSize = 1024 * 1024;
    _maxConnections = 2;
    _maxBatchSize = 1;
    _messageDelimiter = "\n";
    _protocol = "UDP";
    _port = 514;
    _parseMessages = false;
    _serverSocket = 0;
    _maxFds = 0;
    FD_ZERO(&_readfds);
    _thread = NULL;
    _resetServerSocket = false;
    _serverTheadRunning = false;
  }
  //! Destructor
  virtual ~ListenSyslog() {
    _serverTheadRunning = false;
    if (this->_thread)
      delete this->_thread;
    // need to reset the socket
    std::vector<int>::iterator it;
    for (it = _clientSockets.begin(); it != _clientSockets.end(); ++it) {
      int clientSocket = *it;
      close(clientSocket);
    }
    _clientSockets.clear();
    if (_serverSocket > 0) {
      logger_->log_info("ListenSysLog Server socket %d close", _serverSocket);
      close(_serverSocket);
      _serverSocket = 0;
    }
  }
  //! Processor Name
  static const std::string ProcessorName;
  //! Supported Properties
  static Property RecvBufSize;
  static Property MaxSocketBufSize;
  static Property MaxConnections;
  static Property MaxBatchSize;
  static Property MessageDelimiter;
  static Property ParseMessages;
  static Property Protocol;
  static Property Port;
  //! Supported Relationships
  static Relationship Success;
  static Relationship Invalid;
  //! Nest Callback Class for write stream
  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(char *data, uint64_t size)
        : _data(data),
          _dataSize(size) {
    }
    char *_data;
    uint64_t _dataSize;
    void process(std::ofstream *stream) {
      if (_data && _dataSize > 0)
        stream->write(_data, _dataSize);
    }
  };

 public:
  //! OnTrigger method, implemented by NiFi ListenSyslog
  virtual void onTrigger(ProcessContext *context, ProcessSession *session);
  //! Initialize, over write by NiFi ListenSyslog
  virtual void initialize(void);

 protected:

 private:
  //! Logger
  std::shared_ptr<Logger> logger_;
  //! Run function for the thread
  static void run(ListenSyslog *process);
  //! Run Thread
  void runThread();
  //! Queue for store syslog event
  std::queue<SysLogEvent> _eventQueue;
  //! Size of Event queue in bytes
  uint64_t _eventQueueByteSize;
  //! Get event queue size
  uint64_t getEventQueueSize() {
    std::lock_guard<std::mutex> lock(_mtx);
    return _eventQueue.size();
  }
  //! Get event queue byte size
  uint64_t getEventQueueByteSize() {
    std::lock_guard<std::mutex> lock(_mtx);
    return _eventQueueByteSize;
  }
  //! Whether the event queue  is empty
  bool isEventQueueEmpty() {
    std::lock_guard<std::mutex> lock(_mtx);
    return _eventQueue.empty();
  }
  //! Put event into directory listing
  void putEvent(uint8_t *payload, uint64_t len) {
    std::lock_guard<std::mutex> lock(_mtx);
    SysLogEvent event;
    event.payload = payload;
    event.len = len;
    _eventQueue.push(event);
    _eventQueueByteSize += len;
  }
  //! Read \n terminated line from TCP socket
  int readline(int fd, char *bufptr, size_t len);
  //! start server socket and handling client socket
  void startSocketThread();
  //! Poll event
  void pollEvent(std::queue<SysLogEvent> &list, int maxSize) {
    std::lock_guard<std::mutex> lock(_mtx);

    while (!_eventQueue.empty() && (maxSize == 0 || list.size() < maxSize)) {
      SysLogEvent event = _eventQueue.front();
      _eventQueue.pop();
      _eventQueueByteSize -= event.len;
      list.push(event);
    }
    return;
  }
  //! Mutex for protection of the directory listing
  std::mutex _mtx;
  int64_t _recvBufSize;
  int64_t _maxSocketBufSize;
  int64_t _maxConnections;
  int64_t _maxBatchSize;
  std::string _messageDelimiter;
  std::string _protocol;
  int64_t _port;
  bool _parseMessages;
  int _serverSocket;
  std::vector<int> _clientSockets;
  int _maxFds;
  fd_set _readfds;
  //! thread
  std::thread *_thread;
  //! whether to reset the server socket
  bool _resetServerSocket;
  bool _serverTheadRunning;
  //! buffer for read socket
  uint8_t _buffer[2048];
};

#endif
