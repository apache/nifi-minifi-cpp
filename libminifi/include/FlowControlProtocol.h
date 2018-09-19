/**
 * @file FlowControlProtocol.h
 * FlowControlProtocol class declaration
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
#ifndef __FLOW_CONTROL_PROTOCOL_H__
#define __FLOW_CONTROL_PROTOCOL_H__

#include <stdio.h>
#include <sys/types.h>

#include <fcntl.h>
#include <string>
#include <errno.h>
#include <chrono>
#include <thread>

#include "core/Property.h"
#include "properties/Configure.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
// Forwarder declaration
class FlowController;

#define DEFAULT_NIFI_SERVER_PORT 9000
#define DEFAULT_REPORT_INTERVAL 1000 // 1 sec
#define MAX_READ_TIMEOUT 30000 // 30 seconds

// FlowControl Protocol Msg Type
typedef enum {
  REGISTER_REQ,  // Device Register Request from device to server which contain device serial number, current running flow YAML version
  REGISTER_RESP,  // Device Register Respond from server to device, may contain new flow.YAML from server ask device to apply and also device report interval
  REPORT_REQ,  // Period Device Report from device to server which contain device serial number, current running flow YAML name/version and other period report info
  REPORT_RESP,  // Report Respond from server to device, may ask device to update flow YAML or processor property
  MAX_FLOW_CONTROL_MSG_TYPE
} FlowControlMsgType;

// FlowControl Protocol Msg Type String
static const char *FlowControlMsgTypeStr[MAX_FLOW_CONTROL_MSG_TYPE] = { "REGISTER_REQ", "REGISTER_RESP", "REPORT_REQ", "REPORT_RESP" };

// Flow Control Msg Type to String
inline const char *FlowControlMsgTypeToStr(FlowControlMsgType type) {
  if (type < MAX_FLOW_CONTROL_MSG_TYPE)
    return FlowControlMsgTypeStr[type];
  else
    return NULL;
}

// FlowControll Protocol Msg ID (Some Messages are fix length, Some are variable length (TLV)
typedef enum {
  //Fix length 8 bytes: client to server in register request, required field
  FLOW_SERIAL_NUMBER,
  // Flow YAML name TLV: client to server in register request and report request, required field
  FLOW_YML_NAME,
  // Flow YAML content, TLV: server to client in register respond, option field in case server want to ask client to load YAML from server
  FLOW_YML_CONTENT,
  // Fix length, 4 bytes Report interval in msec: server to client in register respond, option field
  REPORT_INTERVAL,
  // Processor Name TLV:  server to client in report respond, option field in case server want to ask client to update processor property
  PROCESSOR_NAME,
  // Processor Property Name TLV: server to client in report respond, option field in case server want to ask client to update processor property
  PROPERTY_NAME,
  // Processor Property Value TLV: server to client in report respond, option field in case server want to ask client to update processor property
  PROPERTY_VALUE,
  // Report Blob TLV: client to server in report request, option field in case client want to pickyback the report blob in report request to server
  REPORT_BLOB,
  MAX_FLOW_MSG_ID
} FlowControlMsgID;

// FlowControl Protocol Msg ID String
static const char *FlowControlMsgIDStr[MAX_FLOW_MSG_ID] = { "FLOW_SERIAL_NUMBER", "FLOW_YAML_NAME", "FLOW_YAML_CONTENT", "REPORT_INTERVAL", "PROCESSOR_NAME"
    "PROPERTY_NAME", "PROPERTY_VALUE", "REPORT_BLOB" };

#define TYPE_HDR_LEN 4 // Fix Hdr Type
#define TLV_HDR_LEN 8 // Type 4 bytes and Len 4 bytes

// FlowControl Protocol Msg Len
inline int FlowControlMsgIDEncodingLen(FlowControlMsgID id, int payLoadLen) {
  if (id == FLOW_SERIAL_NUMBER)
    return (TYPE_HDR_LEN + 8);
  else if (id == REPORT_INTERVAL)
    return (TYPE_HDR_LEN + 4);
  else if (id < MAX_FLOW_MSG_ID)
    return (TLV_HDR_LEN + payLoadLen);
  else
    return -1;
}

// Flow Control Msg Id to String
inline const char *FlowControlMsgIdToStr(FlowControlMsgID id) {
  if (id < MAX_FLOW_MSG_ID)
    return FlowControlMsgIDStr[id];
  else
    return NULL;
}

// Flow Control Respond status code
typedef enum {
  RESP_SUCCESS,
  RESP_TRIGGER_REGISTER,  // Server respond to client report to re trigger register
  RESP_START_FLOW_CONTROLLER,  // Server respond to client to start flow controller
  RESP_STOP_FLOW_CONTROLLER,  // Server respond to client to stop flow controller
  RESP_FAILURE,
  MAX_RESP_CODE
} FlowControlRespCode;

// FlowControl Resp Code str
static const char *FlowControlRespCodeStr[MAX_RESP_CODE] = { "RESP_SUCCESS", "RESP_TRIGGER_REGISTER", "RESP_START_FLOW_CONTROLLER", "RESP_STOP_FLOW_CONTROLLER", "RESP_FAILURE" };

// Flow Control Resp Code to String
inline const char *FlowControlRespCodeToStr(FlowControlRespCode code) {
  if (code < MAX_RESP_CODE)
    return FlowControlRespCodeStr[code];
  else
    return NULL;
}

// Common FlowControlProtocol Header
typedef struct {
  uint32_t msgType;  // Msg Type
  uint32_t seqNumber;  // Seq Number to match Req with Resp
  uint32_t status;  // Resp Code, see FlowControlRespCode
  uint32_t payloadLen;  // Msg Payload length
} FlowControlProtocolHeader;

// FlowControlProtocol Class
class FlowControlProtocol {
 public:
  // Constructor
  /*!
   * Create a new control protocol
   */
  FlowControlProtocol(FlowController *controller, const std::shared_ptr<Configure> &configure)
      : logger_(logging::LoggerFactory<FlowControlProtocol>::getLogger()) {
    _controller = controller;
    _socket = 0;
    _serverName = "localhost";
    _serverPort = DEFAULT_NIFI_SERVER_PORT;
    _registered = false;
    _seqNumber = 0;
    _reportBlob = NULL;
    _reportBlobLen = 0;
    _reportInterval = DEFAULT_REPORT_INTERVAL;
    running_ = false;

    std::string value;

    if (configure->get(Configure::nifi_server_name, value)) {
      _serverName = value;
      logger_->log_info("NiFi Server Name %s", _serverName);
    }
    if (configure->get(Configure::nifi_server_port, value) && core::Property::StringToInt(value, _serverPort)) {
      logger_->log_info("NiFi Server Port: [%ll]", _serverPort);
    }
    if (configure->get(Configure::nifi_server_report_interval, value)) {
      core::TimeUnit unit;
      if (core::Property::StringToTime(value, _reportInterval, unit) && core::Property::ConvertTimeUnitToMS(_reportInterval, unit, _reportInterval)) {
        logger_->log_info("NiFi server report interval: [%ll] ms", _reportInterval);
      }
    } else
      _reportInterval = 0;
  }
  // Destructor
  virtual ~FlowControlProtocol() {
    stop();
    if (_socket)
      close(_socket);
    if (_reportBlob)
      delete[] _reportBlob;
    if (this->_thread)
      delete this->_thread;
  }

 public:

  // SendRegisterRequest and Process Register Respond, return 0 for success
  int sendRegisterReq();
  // SendReportReq and Process Report Respond, return 0 for success
  int sendReportReq();
  // Start the flow control protocol
  void start();
  // Stop the flow control protocol
  void stop();
  // Set Report BLOB for periodically report
  void setReportBlob(char *blob, int len) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (_reportBlob && _reportBlobLen >= len) {
      memcpy(_reportBlob, blob, len);
      _reportBlobLen = len;
    } else {
      if (_reportBlob)
        delete[] _reportBlob;
      _reportBlob = new char[len];
      _reportBlobLen = len;
    }
  }
  // Run function for the thread
  static void run(FlowControlProtocol *protocol);

 protected:

 private:
  // Connect to the socket, return sock descriptor if success, 0 for failure
  int connectServer(const char *host, uint16_t port);
  // Send Data via the socket, return -1 for failure
  int sendData(uint8_t *buf, int buflen);
  // Read length into buf, return -1 for failure and 0 for EOF
  int readData(uint8_t *buf, int buflen);
  // Select on the socket
  int selectClient(int msec);
  // Read the header
  int readHdr(FlowControlProtocolHeader *hdr);
  // encode uint32_t
  uint8_t *encode(uint8_t *buf, uint32_t value) {
    *buf++ = (value & 0xFF000000) >> 24;
    *buf++ = (value & 0x00FF0000) >> 16;
    *buf++ = (value & 0x0000FF00) >> 8;
    *buf++ = (value & 0x000000FF);
    return buf;
  }
  // encode uint32_t
  uint8_t *decode(uint8_t *buf, uint32_t &value) {
    value = ((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]));
    return (buf + 4);
  }
  // encode byte array
  uint8_t *encode(uint8_t *buf, uint8_t *bufArray, int size) {
    memcpy(buf, bufArray, size);
    buf += size;
    return buf;
  }
  // encode std::string
  uint8_t *encode(uint8_t *buf, std::string value) {
    // add the \0 for size
    buf = encode(buf, value.size() + 1);
    buf = encode(buf, (uint8_t *) value.c_str(), value.size() + 1);
    return buf;
  }
  // Mutex for protection
  std::mutex mutex_;
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // NiFi server Name
  std::string _serverName;
  // NiFi server port
  int64_t _serverPort;
  // Serial Number
  uint8_t _serialNumber[8];
  // socket to server
  int _socket;
  // report interal in msec
  int64_t _reportInterval;
  // whether it was registered to the NiFi server
  bool _registered;
  // seq number
  uint32_t _seqNumber;
  // FlowController
  FlowController *_controller = NULL;
  // report Blob
  char *_reportBlob;
  // report Blob len;
  int _reportBlobLen;
  // thread
  std::thread *_thread = NULL;
  // whether it is running
  bool running_;
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  FlowControlProtocol(const FlowControlProtocol &parent);
  FlowControlProtocol &operator=(const FlowControlProtocol &parent);

};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
