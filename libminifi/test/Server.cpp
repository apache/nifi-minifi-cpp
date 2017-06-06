/**
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

/* A simple server in the internet domain using TCP
 The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <string>
#include <errno.h>
#include <chrono>
#include <thread>
#include <fstream>      // std::ifstream
#include <signal.h>

#define DEFAULT_NIFI_SERVER_PORT 9000
#define DEFAULT_REPORT_INTERVAL 1000 // 1 sec
#define MAX_READ_TIMEOUT 30000 // 30 seconds

// FlowControl Protocol Msg Type
typedef enum {
  REGISTER_REQ,  // Device Register Request from device to server which contain device serial number, current running flow YAML version
  REGISTER_RESP,  // Device Register Respond from server to device, may contain new flow.yml from server ask device to apply and also device report interval
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
  FLOW_YAML_NAME,
  // Flow YAML content, TLV: server to client in register respond, option field in case server want to ask client to load YAML from server
  FLOW_YAML_CONTENT,
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

int sendData(int socket, uint8_t *buf, int buflen) {
  int ret = 0, bytes = 0;

  while (bytes < buflen) {
    ret = send(socket, buf + bytes, buflen - bytes, 0);
    //check for errors
    if (ret == -1) {
      return ret;
    }
    bytes += ret;
  }

  return bytes;
}

void error(const char *msg) {
  perror(msg);
  exit(1);
}

/* readline - read a '\n' terminated line from socket fd 
 into buffer bufptr of size len. The line in the
 buffer is terminated with '\0'.
 It returns -1 in case of error or if
 the capacity of the buffer is exceeded.
 It returns 0 if EOF is encountered before reading '\n'.
 */
int readline(int fd, char *bufptr, size_t len) {
  /* Note that this function is very tricky.  It uses the
   static variables bp, cnt, and b to establish a local buffer.
   The recv call requests large chunks of data (the size of the buffer).
   Then if the recv call reads more than one line, the overflow
   remains in the buffer and it is made available to the next call
   to readline. 
   Notice also that this routine reads up to '\n' and overwrites
   it with '\0'. Thus if the line is really terminated with
   "\r\n", the '\r' will remain unchanged.
   */
  char *bufx = bufptr;
  static char *bp;
  static int cnt = 0;
  static char b[4096];
  char c;

  while (--len > 0) {
    if (--cnt <= 0) {
      cnt = recv(fd, b, sizeof(b), 0);
      if (cnt < 0) {
        if ( errno == EINTR) {
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
      *bufptr = '\0';
      return bufptr - bufx;
    }
  }
  return -1;
}

int readData(int socket, uint8_t *buf, int buflen) {
  int sendSize = buflen;
  int status;

  while (buflen) {
#ifndef __MACH__
    status = read(socket, buf, buflen);
#else
    status = recv(socket, buf, buflen, 0);
#endif
    if (status <= 0) {
      return status;
    }
    buflen -= status;
    buf += status;
  }

  return sendSize;
}

int readHdr(int socket, FlowControlProtocolHeader *hdr) {
  uint8_t buffer[sizeof(FlowControlProtocolHeader)];

  uint8_t *data = buffer;

  int status = readData(socket, buffer, sizeof(FlowControlProtocolHeader));
  if (status <= 0)
    return status;

  uint32_t value;
  data = decode(data, value);
  hdr->msgType = value;

  data = decode(data, value);
  hdr->seqNumber = value;

  data = decode(data, value);
  hdr->status = value;

  data = decode(data, value);
  hdr->payloadLen = value;

  return sizeof(FlowControlProtocolHeader);
}

int readYAML(char **ymlContent) {
  std::ifstream is("conf/flowServer.yml", std::ifstream::binary);
  if (is) {
    // get length of file:
    is.seekg(0, is.end);
    int length = is.tellg();
    is.seekg(0, is.beg);

    char * buffer = new char[length];

    printf("Reading %s len %d\n", "conf/flowServer.yml", length);
    // read data as a block:
    is.read(buffer, length);

    is.close();

    // ...buffer contains the entire file...
    *ymlContent = buffer;

    return length;
  }
  return 0;
}

static int sockfd = 0, newsockfd = 0;
void sigHandler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    close(newsockfd);
    close(sockfd);
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  int portno;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  char buffer[4096];
  int flag = 0;
  int number = 0;

  int n;
  if (argc < 2) {
    fprintf(stderr, "ERROR, no port provided\n");
    exit(1);
  }

  if (signal(SIGINT, sigHandler) == SIG_ERR || signal(SIGTERM, sigHandler) == SIG_ERR) {

    return -1;
  }
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");
  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding");
  listen(sockfd, 5);
  if (portno == DEFAULT_NIFI_SERVER_PORT) {
    while (true) {
      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
      if (newsockfd < 0) {
        error("ERROR on accept");
        break;
      }
      // process request
      FlowControlProtocolHeader hdr;
      int status = readHdr(newsockfd, &hdr);
      if (status > 0) {
        printf("Flow Control Protocol receive MsgType %s\n", FlowControlMsgTypeToStr((FlowControlMsgType) hdr.msgType));
        printf("Flow Control Protocol receive Seq Num %d\n", hdr.seqNumber);
        printf("Flow Control Protocol receive Resp Code %s\n", FlowControlRespCodeToStr((FlowControlRespCode) hdr.status));
        printf("Flow Control Protocol receive Payload len %d\n", hdr.payloadLen);
        if (((FlowControlMsgType) hdr.msgType) == REGISTER_REQ) {
          printf("Flow Control Protocol Register Req receive\n");
          uint8_t *payload = new uint8_t[hdr.payloadLen];
          uint8_t *payloadPtr = payload;
          status = readData(newsockfd, payload, hdr.payloadLen);
          while (status > 0 && payloadPtr < (payload + hdr.payloadLen)) {
            uint32_t msgID = 0xFFFFFFFF;
            payloadPtr = decode(payloadPtr, msgID);
            if (((FlowControlMsgID) msgID) == FLOW_SERIAL_NUMBER) {
              // Fixed 8 bytes
              uint8_t seqNum[8];
              memcpy(seqNum, payloadPtr, 8);
              printf("Flow Control Protocol Register Req receive serial num\n");
              payloadPtr += 8;
            } else if (((FlowControlMsgID) msgID) == FLOW_YAML_NAME) {
              uint32_t len;
              payloadPtr = decode(payloadPtr, len);
              printf("Flow Control Protocol receive YAML name length %d\n", len);
              std::string flowName = (const char *) payloadPtr;
              payloadPtr += len;
              printf("Flow Control Protocol receive YAML name %s\n", flowName.c_str());
            } else {
              break;
            }
          }
          delete[] payload;
          // Send Register Respond
          // Calculate the total payload msg size
          char *ymlContent;
          uint32_t yamlLen = readYAML(&ymlContent);
          uint32_t payloadSize = FlowControlMsgIDEncodingLen(REPORT_INTERVAL, 0);
          if (yamlLen > 0)
            payloadSize += FlowControlMsgIDEncodingLen(FLOW_YAML_CONTENT, yamlLen);

          uint32_t size = sizeof(FlowControlProtocolHeader) + payloadSize;
          uint8_t *data = new uint8_t[size];
          uint8_t *start = data;

          // encode the HDR
          hdr.msgType = REGISTER_RESP;
          hdr.payloadLen = payloadSize;
          hdr.status = RESP_SUCCESS;
          data = encode(data, hdr.msgType);
          data = encode(data, hdr.seqNumber);
          data = encode(data, hdr.status);
          data = encode(data, hdr.payloadLen);

          // encode the report interval
          data = encode(data, REPORT_INTERVAL);
          data = encode(data, DEFAULT_REPORT_INTERVAL);

          // encode the YAML content
          if (yamlLen > 0) {
            data = encode(data, FLOW_YAML_CONTENT);
            data = encode(data, yamlLen);
            data = encode(data, (uint8_t *) ymlContent, yamlLen);
            delete[] ymlContent;
          }

          // send it
          status = sendData(newsockfd, start, size);
          delete[] start;
        } else if (((FlowControlMsgType) hdr.msgType) == REPORT_REQ) {
          printf("Flow Control Protocol Report Req receive\n");
          uint8_t *payload = new uint8_t[hdr.payloadLen];
          uint8_t *payloadPtr = payload;
          status = readData(newsockfd, payload, hdr.payloadLen);
          while (status > 0 && payloadPtr < (payload + hdr.payloadLen)) {
            uint32_t msgID = 0xFFFFFFFF;
            payloadPtr = decode(payloadPtr, msgID);
            if (((FlowControlMsgID) msgID) == FLOW_YAML_NAME) {
              uint32_t len;
              payloadPtr = decode(payloadPtr, len);
              printf("Flow Control Protocol receive YAML name length %d\n", len);
              std::string flowName = (const char *) payloadPtr;
              payloadPtr += len;
              printf("Flow Control Protocol receive YAML name %s\n", flowName.c_str());
            } else {
              break;
            }
          }
          delete[] payload;
          // Send Register Respond
          // Calculate the total payload msg size
          std::string processor = "RealTimeDataCollector";
          std::string propertyName1 = "real Time Message ID";
          std::string propertyValue1 = "41";
          std::string propertyName2 = "Batch Message ID";
          std::string propertyValue2 = "172,30,48";
          if (flag == 0) {
            propertyName1 = "Real Time Message ID";
            propertyValue1 = "41";
            propertyName2 = "Batch Message ID";
            propertyValue2 = "172,48";
            flag = 1;
          } else if (flag == 1) {
            propertyName1 = "Real Time Message ID";
            propertyValue1 = "172,48";
            propertyName2 = "Batch Message ID";
            propertyValue2 = "41";
            flag = 0;
          }
          uint32_t payloadSize = FlowControlMsgIDEncodingLen(PROCESSOR_NAME, processor.size() + 1);
          payloadSize += FlowControlMsgIDEncodingLen(PROPERTY_NAME, propertyName1.size() + 1);
          payloadSize += FlowControlMsgIDEncodingLen(PROPERTY_VALUE, propertyValue1.size() + 1);
          payloadSize += FlowControlMsgIDEncodingLen(PROPERTY_NAME, propertyName2.size() + 1);
          payloadSize += FlowControlMsgIDEncodingLen(PROPERTY_VALUE, propertyValue2.size() + 1);

          uint32_t size = sizeof(FlowControlProtocolHeader) + payloadSize;
          uint8_t *data = new uint8_t[size];
          uint8_t *start = data;

          // encode the HDR
          hdr.msgType = REPORT_RESP;
          hdr.payloadLen = payloadSize;
          hdr.status = RESP_SUCCESS;

          if (number >= 10 && number < 20) {
            // After 10 second report, stop the flow controller for 10 second
            hdr.status = RESP_STOP_FLOW_CONTROLLER;
          } else if (number == 20) {
            // restart the flow controller after 10 second
            hdr.status = RESP_START_FLOW_CONTROLLER;
          } else if (number == 30) {
            // retrigger register
            hdr.status = RESP_TRIGGER_REGISTER;
            number = 0;
          }

          number++;

          data = encode(data, hdr.msgType);
          data = encode(data, hdr.seqNumber);
          data = encode(data, hdr.status);
          data = encode(data, hdr.payloadLen);

          // encode the processorName
          data = encode(data, PROCESSOR_NAME);
          data = encode(data, processor);

          // encode the propertyName and value TLV
          data = encode(data, PROPERTY_NAME);
          data = encode(data, propertyName1);
          data = encode(data, PROPERTY_VALUE);
          data = encode(data, propertyValue1);
          data = encode(data, PROPERTY_NAME);
          data = encode(data, propertyName2);
          data = encode(data, PROPERTY_VALUE);
          data = encode(data, propertyValue2);
          // send it
          status = sendData(newsockfd, start, size);
          delete[] start;
        }
      }
      close(newsockfd);
    }
    close(sockfd);
  } else {
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0)
      error("ERROR on accept");
    while (1) {
      bzero(buffer, 4096);
      n = readline(newsockfd, buffer, 4095);
      if (n <= 0) {
        close(newsockfd);
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        continue;
      }
      printf("%s", buffer);
    }
    close(newsockfd);
    close(sockfd);
  }
  return 0;
}
