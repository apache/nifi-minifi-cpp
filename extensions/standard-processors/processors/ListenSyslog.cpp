/**
 * @file ListenSyslog.cpp
 * ListenSyslog class implementation
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
#include "ListenSyslog.h"
#include <stdio.h>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <queue>
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/TypedValues.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
#ifndef WIN32
core::Property ListenSyslog::RecvBufSize(
    core::PropertyBuilder::createProperty("Receive Buffer Size")->withDescription("The size of each buffer used to receive Syslog messages.")->
    withDefaultValue<core::DataSizeValue>("65507 B")->build());

core::Property ListenSyslog::MaxSocketBufSize(
    core::PropertyBuilder::createProperty("Max Size of Socket Buffer")->withDescription("The maximum size of the socket buffer that should be used.")->withDefaultValue<core::DataSizeValue>("1 MB")
        ->build());

core::Property ListenSyslog::MaxConnections(
    core::PropertyBuilder::createProperty("Max Number of TCP Connections")->withDescription("The maximum number of concurrent connections to accept Syslog messages in TCP mode.")
        ->withDefaultValue<int>(2)->build());

core::Property ListenSyslog::MaxBatchSize(
    core::PropertyBuilder::createProperty("Max Batch Size")->withDescription("The maximum number of Syslog events to add to a single FlowFile.")->withDefaultValue<int>(1)->build());

core::Property ListenSyslog::MessageDelimiter(
    core::PropertyBuilder::createProperty("Message Delimiter")->withDescription("Specifies the delimiter to place between Syslog messages when multiple "
                                                                                "messages are bundled together (see <Max Batch Size> core::Property).")->withDefaultValue("\n")->build());

core::Property ListenSyslog::ParseMessages(
    core::PropertyBuilder::createProperty("Parse Messages")->withDescription("Indicates if the processor should parse the Syslog messages. If set to false, each outgoing FlowFile will only.")
        ->withDefaultValue<bool>(false)->build());

core::Property ListenSyslog::Protocol(
    core::PropertyBuilder::createProperty("Protocol")->withDescription("The protocol for Syslog communication.")->withAllowableValue<std::string>("UDP")->withAllowableValue("TCP")->withDefaultValue(
        "UDP")->build());

core::Property ListenSyslog::Port(
    core::PropertyBuilder::createProperty("Port")->withDescription("The port for Syslog communication")->withDefaultValue<int64_t>(514, core::StandardValidators::get().PORT_VALIDATOR)->build());

core::Relationship ListenSyslog::Success("success", "All files are routed to success");
core::Relationship ListenSyslog::Invalid("invalid", "SysLog message format invalid");

void ListenSyslog::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(RecvBufSize);
  properties.insert(MaxSocketBufSize);
  properties.insert(MaxConnections);
  properties.insert(MaxBatchSize);
  properties.insert(MessageDelimiter);
  properties.insert(ParseMessages);
  properties.insert(Protocol);
  properties.insert(Port);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Invalid);
  setSupportedRelationships(relationships);
}

void ListenSyslog::startSocketThread() {
  if (_thread != NULL)
    return;

  logger_->log_trace("ListenSysLog Socket Thread Start");
  _serverTheadRunning = true;
  _thread = new std::thread(run, this);
  _thread->detach();
}

void ListenSyslog::run(ListenSyslog *process) {
  process->runThread();
}

void ListenSyslog::runThread() {
  while (_serverTheadRunning) {
    if (_resetServerSocket) {
      _resetServerSocket = false;
      // need to reset the socket
      std::vector<int>::iterator it;
      for (it = _clientSockets.begin(); it != _clientSockets.end(); ++it) {
        int clientSocket = *it;
        close(clientSocket);
      }
      _clientSockets.clear();
      if (_serverSocket > 0) {
        close(_serverSocket);
        _serverSocket = 0;
      }
    }

    if (_serverSocket <= 0) {
      uint16_t portno = _port;
      struct sockaddr_in serv_addr;
      int sockfd;
      if (_protocol == "TCP")
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
      else
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
      if (sockfd < 0) {
        logger_->log_error("ListenSysLog Server socket creation failed");
        break;
      }
      bzero(reinterpret_cast<char *>(&serv_addr), sizeof(serv_addr));
      serv_addr.sin_family = AF_INET;
      serv_addr.sin_addr.s_addr = INADDR_ANY;
      serv_addr.sin_port = htons(portno);
      if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        logger_->log_error("ListenSysLog Server socket bind failed");
        break;
      }
      if (_protocol == "TCP")
        listen(sockfd, 5);
      _serverSocket = sockfd;
      logger_->log_info("ListenSysLog Server socket %d bind OK to port %d", _serverSocket, portno);
    }
    FD_ZERO(&_readfds);
    FD_SET(_serverSocket, &_readfds);
    _maxFds = _serverSocket;
    std::vector<int>::iterator it;
    for (it = _clientSockets.begin(); it != _clientSockets.end(); ++it) {
      int clientSocket = *it;
      if (clientSocket >= _maxFds)
        _maxFds = clientSocket;
      FD_SET(clientSocket, &_readfds);
    }
    fd_set fds;
    struct timeval tv;
    int retval;
    fds = _readfds;
    tv.tv_sec = 0;
    // 100 msec
    tv.tv_usec = 100000;
    retval = select(_maxFds + 1, &fds, NULL, NULL, &tv);
    if (retval < 0)
      break;
    if (retval == 0)
      continue;
    if (FD_ISSET(_serverSocket, &fds)) {
      // server socket, either we have UDP datagram or TCP connection request
      if (_protocol == "TCP") {
        socklen_t clilen;
        struct sockaddr_in cli_addr;
        clilen = sizeof(cli_addr);
        int newsockfd = accept(_serverSocket, reinterpret_cast<struct sockaddr *>(&cli_addr), &clilen);
        if (newsockfd > 0) {
          if (_clientSockets.size() < (uint64_t) _maxConnections) {
            _clientSockets.push_back(newsockfd);
            logger_->log_info("ListenSysLog new client socket %d connection", newsockfd);
            continue;
          } else {
            close(newsockfd);
          }
        }
      } else {
        socklen_t clilen;
        struct sockaddr_in cli_addr;
        clilen = sizeof(cli_addr);
        int recvlen = recvfrom(_serverSocket, _buffer, sizeof(_buffer), 0, (struct sockaddr *) &cli_addr, &clilen);
        if (recvlen > 0 && (uint64_t) (recvlen + getEventQueueByteSize()) <= static_cast<uint64_t>(_recvBufSize)) {
          uint8_t *payload = new uint8_t[recvlen];
          memcpy(payload, _buffer, recvlen);
          putEvent(payload, recvlen);
        }
      }
    }
    it = _clientSockets.begin();
    while (it != _clientSockets.end()) {
      int clientSocket = *it;
      if (FD_ISSET(clientSocket, &fds)) {
        int recvlen = readline(clientSocket, _buffer, sizeof(_buffer));
        if (recvlen <= 0) {
          close(clientSocket);
          logger_->log_debug("ListenSysLog client socket %d close", clientSocket);
          it = _clientSockets.erase(it);
        } else {
          if ((uint64_t) (recvlen + getEventQueueByteSize()) <= static_cast<uint64_t>(_recvBufSize)) {
            uint8_t *payload = new uint8_t[recvlen];
            memcpy(payload, _buffer, recvlen);
            putEvent(payload, recvlen);
          }
          ++it;
        }
      }
    }
  }
  return;
}

int ListenSyslog::readline(int fd, char *bufptr, size_t len) {
  char *bufx = bufptr;
  static char *bp;
  static int cnt = 0;
  static char b[2048];
  char c;

  while (--len > 0) {
    if (--cnt <= 0) {
      cnt = recv(fd, b, sizeof(b), 0);
      if (cnt < 0) {
        if (errno == EINTR) {
          len++; /* the while will decrement */
          continue;
        }
        return -1;
      }
      if (cnt == 0)
        return 0;
      bp = b;
    }
    c = *bp++;
    *bufptr++ = c;
    if (c == '\n') {
      *bufptr = '\n';
      return bufptr - bufx + 1;
    }
  }
  return -1;
}

void ListenSyslog::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  std::string value;
  bool needResetServerSocket = false;
  if (context->getProperty(Protocol.getName(), value)) {
    if (_protocol != value)
      needResetServerSocket = true;
    _protocol = value;
  }
  if (context->getProperty(RecvBufSize.getName(), value)) {
    core::Property::StringToInt(value, _recvBufSize);
  }
  if (context->getProperty(MaxSocketBufSize.getName(), value)) {
    core::Property::StringToInt(value, _maxSocketBufSize);
  }
  if (context->getProperty(MaxConnections.getName(), value)) {
    core::Property::StringToInt(value, _maxConnections);
  }
  if (context->getProperty(MessageDelimiter.getName(), value)) {
    _messageDelimiter = value;
  }
  if (context->getProperty(ParseMessages.getName(), value)) {
    _parseMessages = org::apache::nifi::minifi::utils::StringUtils::toBool(value).value_or(false);
  }
  if (context->getProperty(Port.getName(), value)) {
    int64_t oldPort = _port;
    core::Property::StringToInt(value, _port);
    if (_port != oldPort)
      needResetServerSocket = true;
  }
  if (context->getProperty(MaxBatchSize.getName(), value)) {
    core::Property::StringToInt(value, _maxBatchSize);
  }

  if (needResetServerSocket)
    _resetServerSocket = true;

  startSocketThread();

  // read from the event queue
  if (isEventQueueEmpty()) {
    context->yield();
    return;
  }

  std::queue<SysLogEvent> eventQueue;
  pollEvent(eventQueue, _maxBatchSize);
  bool firstEvent = true;
  std::shared_ptr<core::FlowFile> flowFile = NULL;
  while (!eventQueue.empty()) {
    SysLogEvent event = eventQueue.front();
    eventQueue.pop();
    if (firstEvent) {
      flowFile = session->create();
      if (!flowFile)
        return;
      ListenSyslog::WriteCallback callback(event.payload, event.len);
      session->write(flowFile, &callback);
      delete[] event.payload;
      firstEvent = false;
    } else {
      ListenSyslog::WriteCallback callback(event.payload, event.len);
      session->append(flowFile, &callback);
      delete[] event.payload;
    }
  }
  flowFile->addAttribute("syslog.protocol", _protocol);
  flowFile->addAttribute("syslog.port", std::to_string(_port));
  session->transfer(flowFile, Success);
}

REGISTER_RESOURCE(ListenSyslog, "Listens for Syslog messages being sent to a given port over TCP or UDP. Incoming messages are checked against regular expressions for RFC5424 and RFC3164 formatted messages. " // NOLINT
                  "The format of each message is: (<PRIORITY>)(VERSION )(TIMESTAMP) (HOSTNAME) (BODY) where version is optional. The timestamp can be an RFC5424 timestamp with a format of "
                  "\"yyyy-MM-dd'T'HH:mm:ss.SZ\" or \"yyyy-MM-dd'T'HH:mm:ss.S+hh:mm\", or it can be an RFC3164 timestamp with a format of \"MMM d HH:mm:ss\". If an incoming messages matches "
                  "one of these patterns, the message will be parsed and the individual pieces will be placed in FlowFile attributes, with the original message in the content of the FlowFile. "
                  "If an incoming message does not match one of these patterns it will not be parsed and the syslog.valid attribute will be set to false with the original message in the content "
                  "of the FlowFile. Valid messages will be transferred on the success relationship, and invalid messages will be transferred on the invalid relationship.");

#endif
} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
