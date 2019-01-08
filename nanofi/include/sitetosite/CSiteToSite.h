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
#ifndef LIBMINIFI_INCLUDE_CORE_CSITETOSITE_CSITETOSITE_H_
#define LIBMINIFI_INCLUDE_CORE_CSITETOSITE_CSITETOSITE_H_

#include <zlib.h>
#include <string.h>
#include "uthash.h"
#include "CPeer.h"
#include "core/cstream.h"
#include "core/cuuid.h"

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define htonll_r(x) ((((uint64_t)htonl(x)) << 32) + htonl((x) >> 32))

// Resource Negotiated Status Code
#define RESOURCE_OK 20
#define DIFFERENT_RESOURCE_VERSION 21
#define NEGOTIATED_ABORT 255
// ! Max attributes
#define MAX_NUM_ATTRIBUTES 25000

// Respond Code Sequence Pattern
static const uint8_t CODE_SEQUENCE_VALUE_1 = (uint8_t) 'R';
static const uint8_t CODE_SEQUENCE_VALUE_2 = (uint8_t) 'C';

/**
 * Enumeration of Properties that can be used for the Site-to-Site Socket
 * Protocol.
 */
typedef enum {
  /**
   * Boolean value indicating whether or not the contents of a FlowFile should
   * be GZipped when transferred.
   */
  GZIP,
  /**
   * The unique identifier of the port to communicate with
   */
  PORT_IDENTIFIER,
  /**
   * Indicates the number of milliseconds after the request was made that the
   * client will wait for a response. If no response has been received by the
   * time this value expires, the server can move on without attempting to
   * service the request because the client will have already disconnected.
   */
  REQUEST_EXPIRATION_MILLIS,
  /**
   * The preferred number of FlowFiles that the server should send to the
   * client when pulling data. This property was introduced in version 5 of
   * the protocol.
   */
  BATCH_COUNT,
  /**
   * The preferred number of bytes that the server should send to the client
   * when pulling data. This property was introduced in version 5 of the
   * protocol.
   */
  BATCH_SIZE,
  /**
   * The preferred amount of time that the server should send data to the
   * client when pulling data. This property was introduced in version 5 of
   * the protocol. Value is in milliseconds.
   */
  BATCH_DURATION,
  MAX_HANDSHAKE_PROPERTY
} HandshakeProperty;

typedef enum {
  RAW,
  HTTP
} CLIENT_TYPE;

/**
 * An enumeration for specifying the direction in which data should be
 * transferred between a client and a remote NiFi instance.
 */
typedef enum {
  /**
   * * The client is to send data to the remote instance.
   * */
  SEND,
  /**
   * * The client is to receive data from the remote instance.
   * */
  RECEIVE
} TransferDirection;

// Peer State
typedef enum {
  /**
   * * IDLE
   * */
  IDLE = 0,
  /**
   * * Socket Established
   * */
  ESTABLISHED,
  /**
   * * HandShake Done
   * */
  HANDSHAKED,
  /**
   * * After CodeDec Completion
   * */
  READY
} PeerState;

// Transaction State
typedef enum {
  /**
   * * Transaction has been started but no data has been sent or received.
   * */
  TRANSACTION_STARTED,
  /**
   * * Transaction has been started and data has been sent or received.
   * */
  DATA_EXCHANGED,
  /**
   * * Data that has been transferred has been confirmed via its CRC.
   * * Transaction is ready to be completed.
   * */
  TRANSACTION_CONFIRMED,
  /**
   * * Transaction has been successfully completed.
   * */
  TRANSACTION_COMPLETED,
  /**
   * * The Transaction has been canceled.
   * */
  TRANSACTION_CANCELED,

  /**
   * * Transaction has been successfully closed.
   * */
  TRANSACTION_CLOSED,
  /**
   * * The Transaction ended in an error.
   * */
  TRANSACTION_ERROR
} TransactionState;

// Request Type
typedef enum {
  NEGOTIATE_FLOWFILE_CODEC = 0,
  REQUEST_PEER_LIST,
  SEND_FLOWFILES,
  RECEIVE_FLOWFILES,
  SHUTDOWN,
  MAX_REQUEST_TYPE
} RequestType;


// Respond Code
typedef enum {
  RESERVED = 0,
  // ResponseCode, so that we can indicate a 0 followed by some other bytes

  // handshaking properties
  PROPERTIES_OK = 1,
  UNKNOWN_PROPERTY_NAME = 230,
  ILLEGAL_PROPERTY_VALUE = 231,
  MISSING_PROPERTY = 232,
  // transaction indicators
  CONTINUE_TRANSACTION = 10,
  FINISH_TRANSACTION = 11,
  CONFIRM_TRANSACTION = 12,// "Explanation" of this code is the checksum
  TRANSACTION_FINISHED = 13,
  TRANSACTION_FINISHED_BUT_DESTINATION_FULL = 14,
  CANCEL_TRANSACTION = 15,
  BAD_CHECKSUM = 19,
  // data availability indicators
  MORE_DATA = 20,
  NO_MORE_DATA = 21,
  // port state indicators
  UNKNOWN_PORT = 200,
  PORT_NOT_IN_VALID_STATE = 201,
  PORTS_DESTINATION_FULL = 202,
  // authorization
  UNAUTHORIZED = 240,
  // error indicators
  ABORT = 250,
  UNRECOGNIZED_RESPONSE_CODE = 254,
  END_OF_STREAM = 255
}RespondCode;

// Respond Code Class
typedef struct {
  RespondCode code;
  const char *description; int hasDescription;
} RespondCodeContext;



// Request Type Str
static const char *RequestTypeStr[MAX_REQUEST_TYPE] = { "NEGOTIATE_FLOWFILE_CODEC", "REQUEST_PEER_LIST", "SEND_FLOWFILES", "RECEIVE_FLOWFILES", "SHUTDOWN" };
static RespondCodeContext respondCodeContext[21] = { //NOLINT
  { RESERVED, "Reserved for Future Use", 0 },  //NOLINT
  { PROPERTIES_OK, "Properties OK", 0 },  //NOLINT
  { UNKNOWN_PROPERTY_NAME, "Unknown Property Name", 1 },  //NOLINT
  { ILLEGAL_PROPERTY_VALUE, "Illegal Property Value", 1 },  //NOLINT
  { MISSING_PROPERTY, "Missing Property", 1 },  //NOLINT
  { CONTINUE_TRANSACTION, "Continue Transaction", 0 },  //NOLINT
  { FINISH_TRANSACTION, "Finish Transaction", 0 },  //NOLINT
  { CONFIRM_TRANSACTION, "Confirm Transaction", 1 },  //NOLINT
  { TRANSACTION_FINISHED, "Transaction Finished", 0 },  //NOLINT
  { TRANSACTION_FINISHED_BUT_DESTINATION_FULL, "Transaction Finished But Destination is Full", 0 },  //NOLINT
  { CANCEL_TRANSACTION, "Cancel Transaction", 1 },  //NOLINT
  { BAD_CHECKSUM, "Bad Checksum", 0 },  //NOLINT
  { MORE_DATA, "More Data Exists", 0 },  //NOLINT
  { NO_MORE_DATA, "No More Data Exists", 0 },  //NOLINT
  { UNKNOWN_PORT, "Unknown Port", 0 },  //NOLINT
  { PORT_NOT_IN_VALID_STATE, "Port Not in a Valid State", 1 },  //NOLINT
  { PORTS_DESTINATION_FULL, "Port's Destination is Full", 0 },  //NOLINT
  { UNAUTHORIZED, "User Not Authorized", 1 },  //NOLINT
  { ABORT, "Abort", 1 },  //NOLINT
  { UNRECOGNIZED_RESPONSE_CODE, "Unrecognized Response Code", 0 },  //NOLINT
  { END_OF_STREAM, "End of Stream", 0 }
};



// Transaction Class
typedef struct {

  // Number of current transfers
  int current_transfers_;
  // number of total seen transfers
  int total_transfers_;

  // Number of content bytes
  uint64_t _bytes;

  // Transaction State
  TransactionState _state;

  // Whether received data is available
  int _dataAvailable;

  //org::apache::nifi::minifi::io::BaseStream* _stream;
  cstream * _stream;

  uint64_t _crc;

  char _uuid_str[37];

  TransferDirection _direction;

  UT_hash_handle hh;
} CTransaction;

static void InitTransaction(CTransaction * transaction, TransferDirection direction, cstream * stream) {
  transaction->_stream = stream;
  transaction->_state = TRANSACTION_STARTED;
  transaction->_direction = direction;
  transaction->_dataAvailable = 0;
  transaction->current_transfers_ = 0;
  transaction->total_transfers_ = 0;
  transaction->_bytes = 0;
  transaction->_crc = 0;

  CIDGenerator gen;
  gen.implementation_ = CUUID_DEFAULT_IMPL;
  generate_uuid(&gen, transaction->_uuid_str);
  transaction->_uuid_str[36]='\0';
}

static TransferDirection getDirection(const CTransaction * transaction) {
  return transaction->_direction;
}

static TransactionState getState(const CTransaction * transaction) {
  return transaction->_state;
}

static int isDataAvailable(const CTransaction * transaction) {
  return transaction->_dataAvailable;
}

static void setDataAvailable(CTransaction * transaction, int available) {
  transaction->_dataAvailable = available;
}

static uint64_t getCRC(const CTransaction * transaction) {
  return transaction->_crc;
}

static const char * getUUIDStr(const CTransaction * transation) {
  return transation->_uuid_str;
}

static void updateCRC(CTransaction * transaction, const uint8_t *buffer, uint32_t length) {
  transaction->_crc = crc32(transaction->_crc, buffer, length);
}

static int writeData(CTransaction * transaction, const uint8_t *value, int size) {
  int ret = write_buffer(value, size, transaction->_stream);
  transaction->_crc = crc32(transaction->_crc, value, size);
  return ret;
}

static int readData(CTransaction * transaction, uint8_t *buf, int buflen) {
  //int ret = transaction->_stream->read(buf, buflen);
  int ret = read_buffer(buf, buflen, transaction->_stream);
  transaction->_crc = crc32(transaction->_crc, buf, ret);
  return ret;
}

static int is_little_endian() {
  static unsigned int x = 1;
  static char *c = (char*) &x;
  return  (*c == 1) ? 1 : 0;
}

static int write_uint64t(CTransaction * transaction, uint64_t base_value) {
  const uint64_t value = is_little_endian() == 1 ? htonll_r(base_value) : base_value;
  const uint8_t * buf = (uint8_t*)(&value);

  return writeData(transaction, buf, sizeof(uint64_t));
}

static int write_uint32t(CTransaction * transaction, uint32_t base_value) {
  const uint32_t value = is_little_endian() == 1 ? htonl(base_value) : base_value;
  const uint8_t * buf = (uint8_t*)(&value);

  return writeData(transaction, buf, sizeof(uint32_t));
}

static int write_uint16t(CTransaction * transaction, uint16_t base_value) {
  const uint16_t value = is_little_endian() == 1 ? htons(base_value) : base_value;
  const uint8_t *buf = (uint8_t *) (&value);

  return writeData(transaction, buf, sizeof(uint16_t));
}

static int write_UTF_len(CTransaction * transaction, const char * str, size_t len, enum Bool widen) {
  if (len > 65535) {
    return -1;
  }

  int ret;
  if (!widen) {
    uint16_t shortlen = len;
    ret = write_uint16t(transaction, shortlen);
  } else {
    ret = write_uint32t(transaction, len);
  }

  if(len == 0 || ret < 0) {
    return ret;
  }

  const uint8_t *underlyingPtr = (const uint8_t *)str;

  if (!widen) {
    uint16_t short_length = len;
    ret = writeData(transaction, underlyingPtr, short_length);
  } else {
    ret = writeData(transaction, underlyingPtr, len);
  }
  return ret;
}

static int write_UTF(CTransaction * transaction, const char * str, enum Bool widen) {
  //return transaction->_stream->writeUTF(str, widen);
  return write_UTF_len(transaction, str, strlen(str), widen);
}

/**
* Represents a piece of data that is to be sent to or that was received from a
* NiFi instance.
*/
typedef struct {
  const attribute_set * _attributes;
  CTransaction* transaction_;
  const char * payload_;
} CDataPacket;

static void initPacket(CDataPacket * packet, CTransaction* transaction, const attribute_set * attributes, const char * payload) {
  packet->payload_ = payload;
  packet->transaction_ = transaction;
  packet->_attributes = attributes;
}


#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* LIBMINIFI_INCLUDE_CORE_CSITETOSITE_CSITETOSITE_H_ */
