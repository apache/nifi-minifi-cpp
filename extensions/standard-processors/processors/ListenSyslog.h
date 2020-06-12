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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_LISTENSYSLOG_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_LISTENSYSLOG_H_

#include <stdio.h>
#include <sys/types.h>

#include <memory>
#include <queue>
#include <string>
#include <vector>

#ifndef WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>

#else
#include <WinSock2.h>

#endif
#include <errno.h>

#include <chrono>
#include <thread>

#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "FlowFileRecord.h"

#ifndef WIN32

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {


// SyslogEvent
typedef struct {
  char *payload;
  uint64_t len;
} SysLogEvent;

// ListenSyslog Class
class ListenSyslog : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  ListenSyslog(std::string name,  utils::Identifier uuid = utils::Identifier()) // NOLINT
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<ListenSyslog>::getLogger()) {
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
  // Destructor
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
      logger_->log_debug("ListenSysLog Server socket %d close", _serverSocket);
      close(_serverSocket);
      _serverSocket = 0;
    }
  }
  // Processor Name
  static constexpr char const *ProcessorName = "ListenSyslog";
  // Supported Properties
  static core::Property RecvBufSize;
  static core::Property MaxSocketBufSize;
  static core::Property MaxConnections;
  static core::Property MaxBatchSize;
  static core::Property MessageDelimiter;
  static core::Property ParseMessages;
  static core::Property Protocol;
  static core::Property Port;
  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship Invalid;
  // Nest Callback Class for write stream
  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(char *data, uint64_t size)
        : _data(reinterpret_cast<uint8_t*>(data)),
          _dataSize(size) {
    }
    uint8_t *_data;
    uint64_t _dataSize;
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      int64_t ret = 0;
      if (_data && _dataSize > 0)
        ret = stream->write(_data, _dataSize);
      return ret;
    }
  };

 public:
  // OnTrigger method, implemented by NiFi ListenSyslog
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
  // Initialize, over write by NiFi ListenSyslog
  virtual void initialize(void);

 private:
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // Run function for the thread
  static void run(ListenSyslog *process);
  // Run Thread
  void runThread();
  // Queue for store syslog event
  std::queue<SysLogEvent> _eventQueue;
  // Size of Event queue in bytes
  uint64_t _eventQueueByteSize;
  // Get event queue size
  uint64_t getEventQueueSize() {
    std::lock_guard<std::mutex> lock(mutex_);
    return _eventQueue.size();
  }
  // Get event queue byte size
  uint64_t getEventQueueByteSize() {
    std::lock_guard<std::mutex> lock(mutex_);
    return _eventQueueByteSize;
  }
  // Whether the event queue  is empty
  bool isEventQueueEmpty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return _eventQueue.empty();
  }
  // Put event into directory listing
  void putEvent(uint8_t *payload, uint64_t len) {
    std::lock_guard<std::mutex> lock(mutex_);
    SysLogEvent event;
    event.payload = reinterpret_cast<char*>(payload);
    event.len = len;
    _eventQueue.push(event);
    _eventQueueByteSize += len;
  }
  // Read \n terminated line from TCP socket
  int readline(int fd, char *bufptr, size_t len);
  // start server socket and handling client socket
  void startSocketThread();
  // Poll event
  void pollEvent(std::queue<SysLogEvent> &list, int maxSize) {
    std::lock_guard<std::mutex> lock(mutex_);

    while (!_eventQueue.empty() && (maxSize == 0 || list.size() < maxSize)) {
      SysLogEvent event = _eventQueue.front();
      _eventQueue.pop();
      _eventQueueByteSize -= event.len;
      list.push(event);
    }
    return;
  }
  // Mutex for protection of the directory listing
  std::mutex mutex_;
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
  // thread
  std::thread *_thread;
  // whether to reset the server socket
  bool _resetServerSocket; bool _serverTheadRunning;
  // buffer for read socket
  char _buffer[2048];
};

REGISTER_RESOURCE(ListenSyslog, "Listens for Syslog messages being sent to a given port over TCP or UDP. Incoming messages are checked against regular expressions for RFC5424 and RFC3164 formatted messages. " // NOLINT
                  "The format of each message is: (<PRIORITY>)(VERSION )(TIMESTAMP) (HOSTNAME) (BODY) where version is optional. The timestamp can be an RFC5424 timestamp with a format of "
                  "\"yyyy-MM-dd'T'HH:mm:ss.SZ\" or \"yyyy-MM-dd'T'HH:mm:ss.S+hh:mm\", or it can be an RFC3164 timestamp with a format of \"MMM d HH:mm:ss\". If an incoming messages matches "
                  "one of these patterns, the message will be parsed and the individual pieces will be placed in FlowFile attributes, with the original message in the content of the FlowFile. "
                  "If an incoming message does not match one of these patterns it will not be parsed and the syslog.valid attribute will be set to false with the original message in the content "
                  "of the FlowFile. Valid messages will be transferred on the success relationship, and invalid messages will be transferred on the invalid relationship.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_LISTENSYSLOG_H_
