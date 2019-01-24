/**
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "uthash.h"
#include "sitetosite/CRawSocketProtocol.h"
#include "sitetosite/CPeer.h"

#include "core/cstream.h"

#include "api/nanofi.h"
#include "core/log.h"

static const char *HandShakePropertyStr[MAX_HANDSHAKE_PROPERTY] = {
/**
 * Boolean value indicating whether or not the contents of a FlowFile should
 * be GZipped when transferred.
 */
"GZIP",
/**
 * The unique identifier of the port to communicate with
 */
"PORT_IDENTIFIER",
/**
 * Indicates the number of milliseconds after the request was made that the
 * client will wait for a response. If no response has been received by the
 * time this value expires, the server can move on without attempting to
 * service the request because the client will have already disconnected.
 */
"REQUEST_EXPIRATION_MILLIS",
/**
 * The preferred number of FlowFiles that the server should send to the
 * client when pulling data. This property was introduced in version 5 of
 * the protocol.
 */
"BATCH_COUNT",
/**
 * The preferred number of bytes that the server should send to the client
 * when pulling data. This property was introduced in version 5 of the
 * protocol.
 */
"BATCH_SIZE",
/**
 * The preferred amount of time that the server should send data to the
 * client when pulling data. This property was introduced in version 5 of
 * the protocol. Value is in milliseconds.
 */
"BATCH_DURATION" };

typedef struct {
  const char * name;
  char value[40];
  UT_hash_handle hh;
} PropertyValue;

int handShake(struct CRawSiteToSiteClient * client) {
  if (client->_peer_state != ESTABLISHED) {
    //client->logger_->log_error("Site2Site peer state is not established while handshake");
    return -1;
  }
  //client->logger_->log_debug("Site2Site Protocol Perform hand shake with destination port %s", client->_port_id_str);

  CIDGenerator gen;
  gen.implementation_ = CUUID_DEFAULT_IMPL;
  generate_uuid(&gen, client->_commsIdentifier);
  client->_commsIdentifier[36]='\0';

  int ret = writeUTF(client->_commsIdentifier, strlen(client->_commsIdentifier), False, client->_peer->_stream);

  if (ret <= 0) {
    return -1;
  }

  uint32_t prop_size;
  PropertyValue *current, *tmp, * properties = NULL;

  current = (PropertyValue *)malloc(sizeof(PropertyValue));

  current->name = HandShakePropertyStr[GZIP];
  strncpy(current->value, "false", strlen("false") +1);

  HASH_ADD_KEYPTR(hh, properties, current->name, strlen(current->name), current);

  current = (PropertyValue *)malloc(sizeof(PropertyValue));

  current->name = HandShakePropertyStr[PORT_IDENTIFIER];
  strncpy(current->value, client->_port_id_str, strlen(client->_port_id_str) +1);

  HASH_ADD_KEYPTR(hh, properties, current->name, strlen(current->name), current);

  current = (PropertyValue *)malloc(sizeof(PropertyValue));

  current->name = HandShakePropertyStr[REQUEST_EXPIRATION_MILLIS];
  sprintf(current->value, "%llu", client->_timeout);

  HASH_ADD_KEYPTR(hh, properties, current->name, strlen(current->name), current);

  prop_size = 3;

  if (client->_currentVersion >= 5) {
    if (client->_batch_count > 0) {
      current = (PropertyValue *)malloc(sizeof(PropertyValue));

      current->name = HandShakePropertyStr[BATCH_COUNT];
      sprintf(current->value, "%llu", client->_batch_count);

      HASH_ADD_KEYPTR(hh, properties, current->name, strlen(current->name), current);

      prop_size++;
    }
    if (client->_batch_size > 0) {
      current = (PropertyValue *)malloc(sizeof(PropertyValue));

      current->name = HandShakePropertyStr[BATCH_SIZE];
      sprintf(current->value, "%llu", client->_batch_size);

      HASH_ADD_KEYPTR(hh, properties, current->name, strlen(current->name), current);

      prop_size++;
    }
    if (client->_batch_duration > 0) {
      current = (PropertyValue *)malloc(sizeof(PropertyValue));

      current->name = HandShakePropertyStr[BATCH_DURATION];
      sprintf(current->value, "%llu", client->_batch_duration);

      HASH_ADD_KEYPTR(hh, properties, current->name, strlen(current->name), current);

      prop_size++;
    }
  }

  int ret_val = 0;

  if (client->_currentVersion >= 3) {

    //ret = client->_peer->writeUTF(client->_peer->getURL());
    const char * urlstr = getURL(client->_peer);
    ret = writeUTF(urlstr, strlen(urlstr), False, client->_peer->_stream);
    if (ret <= 0) {
      ret_val = -1;
    }
  }

  if(ret_val == 0) {
    ret = write_uint32_t(prop_size, client->_peer->_stream);
  }
  if (ret <= 0) {
    ret_val = -1;
  }

  HASH_ITER(hh, properties, current, tmp) {
    if(ret_val == 0 && writeUTF(current->name, strlen(current->name), False, client->_peer->_stream) <= 0) {
      ret_val = -1;
    }
    if(ret_val == 0 && writeUTF(current->value, strlen(current->value), False, client->_peer->_stream) <= 0) {
      ret_val = -1;
    }
    logc(debug, "Site2Site Protocol Send handshake properties %s %s", current->name, current->value);
    HASH_DEL(properties, current);
    free(current);
  }

  if(ret_val < 0) {
    logc(err, "%s", "Failed to transfer handshake properties");
    return -1;
  }

  RespondCode code;

  ret = readResponse(client, &code);

  if (ret <= 0) {
    return -1;
  }

  RespondCodeContext *resCode = getRespondCodeContext(code);

  if(resCode == NULL) {
    logc(err, "Received invalid respond code: %d", code);
    return -1;
  }

  if (resCode->hasDescription) {
    uint32_t utflen;
    ret = readUTFLen(&utflen, client->_peer->_stream);
    if (ret <= 0)
      return -1;

    memset(client->_description_buffer, 0, utflen+1);
    ret = readUTF(client->_description_buffer, utflen, client->_peer->_stream);
    if (ret <= 0)
      return -1;
  }

  const char * error = "";

  switch (code) {
    case PROPERTIES_OK:
      logc(debug, "%s", "Site2Site HandShake Completed");
      client->_peer_state = HANDSHAKED;
      return 0;
    case PORT_NOT_IN_VALID_STATE:
      error = "in invalid state";
      break;
    case UNKNOWN_PORT:
      error = "an unknown port";
      break;
    case PORTS_DESTINATION_FULL:
      error = "full";
      break;
    // Unknown error
    default:
      logc(err, "HandShake Failed because of unknown respond code %d", code);
      return -1;
  }

  // All known error cases handled here
  logc(err, "Site2Site HandShake Failed because destination port, %s, is %s", client->_port_id_str, error);
  return -2;
}


/*bool CRawSiteToSiteClient::getPeerList(std::vector<CPeerStatus> &peers) {
  if (establish(this) == 0 && handShake()) {
    int status = writeRequestType(this, REQUEST_PEER_LIST);

    if (status <= 0) {
      tearDown(this);
      return false;
    }

    uint32_t number;
    status = _peer->read(number);

    if (status <= 0) {
      tearDown(this);
      return false;
    }

    for (uint32_t i = 0; i < number; i++) {
      std::string host;
      status = _peer->readUTF(host);
      if (status <= 0) {
        tearDown(this);
        return false;
      }
      uint32_t port;
      status = _peer->read(port);
      if (status <= 0) {
        tearDown(this);
        return false;
      }
      uint8_t secure;
      status = _peer->read(secure);
      if (status <= 0) {
        tearDown(this);
        return false;
      }
      uint32_t count;
      status = _peer->read(count);
      if (status <= 0) {
        tearDown(this);
        return false;
      }
      CPeerStatus status(std::make_shared<CPeer>(port_id_, host, port, secure), count, true);
      peers.push_back(std::move(status));
      logging::LOG_TRACE(logger_) << "Site2Site Peer host " << host << " port " << port << " Secure " << secure;
    }

    tearDown(this);
    return true;
  } else {
    tearDown(this);
    return false;
  }
}*/

int bootstrap(struct CRawSiteToSiteClient * client) {
  if (client->_peer_state == READY)
    return 0;

  tearDown(client);

  if (establish(client) ==0 && handShake(client) == 0 && negotiateCodec(client) == 0) {
    logc(debug, "%s", "Site to Site ready for data transaction");
    return 0;
  } else {
    tearDown(client);
    return -1;
  }
}

CTransaction* createTransaction(struct CRawSiteToSiteClient * client, TransferDirection direction) {
  int ret;
  int dataAvailable = 0;
  CTransaction* transaction = NULL;

  if (client->_peer_state != READY) {
    bootstrap(client);
  }

  if (client->_peer_state != READY) {
    return transaction;
  }

  if (direction == RECEIVE) {
    ret = writeRequestType(client, RECEIVE_FLOWFILES);

    if (ret <= 0) {
      return transaction;
    }

    RespondCode code;

    ret = readResponse(client, &code);

    if (ret <= 0) {
      return transaction;
    }

    RespondCodeContext *resCode = getRespondCodeContext(code);

    if(resCode == NULL) {
      logc(err, "Received invalid respond code: %d", code);
      return NULL;
    }

    if (resCode->hasDescription) {
      uint32_t utflen;
      ret = readUTFLen(&utflen, client->_peer->_stream);
      if (ret <= 0)
        return transaction;
      memset(client->_description_buffer, 0, utflen+1);
      ret = readUTF(client->_description_buffer, utflen, client->_peer->_stream);
      if (ret <= 0)
        return transaction;
    }
    
    switch (code) {
      case MORE_DATA:
        dataAvailable = 1;
        logc(trace, "%s", "Site2Site peer indicates that data is available");
        break;
      case NO_MORE_DATA:
        dataAvailable = 0;
        logc(trace, "%s", "Site2Site peer indicates that no data is available");
        break;
      default:
        logc(warn, "Site2Site got unexpected response %d when asking for data", code);
        return NULL;
    }
    transaction = (CTransaction*)malloc(1* sizeof(CTransaction));
    InitTransaction(transaction, direction, client->_peer->_stream);
    addTransaction(client, transaction);
    setDataAvailable(transaction, dataAvailable);
    logc(trace, "Site2Site create transaction %s", getUUIDStr(transaction));
    return transaction;
  } else {
    ret = writeRequestType(client, SEND_FLOWFILES);

    if (ret <= 0) {
      return NULL;
    } else {
      transaction = (CTransaction*)malloc(1* sizeof(CTransaction));
      InitTransaction(transaction, direction, client->_peer->_stream);
      addTransaction(client, transaction);
      logc(trace, "Site2Site create transaction %s", getUUIDStr(transaction));
      return transaction;
    }
  }
}

int transmitPayload(struct CRawSiteToSiteClient * client, const char * payload, const attribute_set * attributes) {
  CTransaction* transaction = NULL;

  if (payload == NULL && attributes == NULL) {
    return -1;
  }

  if (client->_peer_state != READY) {
    if (bootstrap(client) != 0) {
      return -1;
    }
  }

  if (client->_peer_state != READY) {
    tearDown(client);
  }

  // Create the transaction
  const char * transactionID;
  transaction = createTransaction(client, SEND);

  if (transaction == NULL) {
    tearDown(client);
    return -1;
  }

  transactionID = getUUIDStr(transaction);

  CDataPacket packet;

  initPacket(&packet, transaction, attributes, payload);

  int16_t resp = sendPacket(client, transactionID, &packet, NULL);
  if (resp != 0) {
    deleteTransaction(client, transactionID);
    tearDown(client);
    return resp;
  }
  logc(info, "Site2Site transaction %s sent bytes length %lu", transactionID, strlen(payload));


  int ret = confirm(client, transactionID);

  if(ret == 0) {
    ret = complete(client, transactionID);
  }

  deleteTransaction(client, transactionID);

  if (ret != 0) {
    tearDown(client);
  }

  return ret;
}

// Complete the transaction
int complete(struct CRawSiteToSiteClient * client, const char * transactionID) {
  if (client->_peer_state != READY) {
    bootstrap(client);
  }

  if (client->_peer_state != READY) {
    return -1;
  }

  CTransaction* transaction = findTransaction(client, transactionID);

  if (!transaction) {
    return -1;
  }

  if (transaction->total_transfers_ > 0 && getState(transaction) != TRANSACTION_CONFIRMED) {
    return -1;
  }
  if (getDirection(transaction) == RECEIVE) {
    if (transaction->current_transfers_ == 0) {
      transaction->_state = TRANSACTION_COMPLETED;
      return 0;
    } else {
      logc(debug, "Site2Site transaction %s send finished", transactionID);
      if(writeResponse(client, TRANSACTION_FINISHED, "Finished") <= 0) {
        return -1;
      } else {
        transaction->_state = TRANSACTION_COMPLETED;
        return 0;
      }
    }
  } else {
    RespondCode code;

    if (readResponse(client, &code) <= 0) {
      return -1;
    }

    RespondCodeContext *resCode = getRespondCodeContext(code);

    if(resCode == NULL) {
      logc(err, "Received invalid respond code: %d", code);
      return -1;
    }

    if (resCode->hasDescription) {
      uint32_t utflen;
      int ret = readUTFLen(&utflen, client->_peer->_stream);
      if (ret <= 0)
        return -1;
      memset(client->_description_buffer, 0, utflen+1);
      ret = readUTF(client->_description_buffer, utflen, client->_peer->_stream);
      if (ret <= 0)
        return -1;
    }

    if (code == TRANSACTION_FINISHED) {
      logc(debug, "Site2Site transaction %s peer finished transaction", transactionID);
      transaction->_state = TRANSACTION_COMPLETED;
      return 0;
    } else {
      logc(warn, "Site2Site transaction %s peer unknown respond code %d", transactionID, code);
      return -1;
    }
  }
}

int confirm(struct CRawSiteToSiteClient * client, const char * transactionID) {

  if (client->_peer_state != READY) {
    bootstrap(client);
  }

  if (client->_peer_state != READY) {
    return -1;
  }

  CTransaction* transaction = findTransaction(client, transactionID);

  if (!transaction) {
    return -1;
  }

  if (getState(transaction) == TRANSACTION_STARTED && isDataAvailable(transaction) == 0 && getDirection(transaction) == RECEIVE) {
    transaction->_state = TRANSACTION_CONFIRMED;
    return 0;
  }

  if (getState(transaction) != DATA_EXCHANGED)
    return -1;

  if (getDirection(transaction) == RECEIVE) {
    if (isDataAvailable(transaction) != 0)
      return -1;

    // we received a FINISH_TRANSACTION indicator. Send back a CONFIRM_TRANSACTION message
    // to peer so that we can verify that the connection is still open. This is a two-phase commit,
    // which helps to prevent the chances of data duplication. Without doing this, we may commit the
    // session and then when we send the response back to the peer, the peer may have timed out and may not
    // be listening. As a result, it will re-send the data. By doing this two-phase commit, we narrow the
    // Critical Section involved in this transaction so that rather than the Critical Section being the
    // time window involved in the entire transaction, it is reduced to a simple round-trip conversation.
    int64_t crcValue = getCRC(transaction);
    char crc[40];
    sprintf(crc, "%lld", crcValue);

    logc(debug, "Site2Site Send confirm with CRC %lld to transaction %s", crcValue, transactionID);
    if (writeResponse(client, CONFIRM_TRANSACTION, crc) <= 0) {
      return -1;
    }

    RespondCode code;
    if (readResponse(client, &code) <= 0) {
      return -1;
    }

    RespondCodeContext *resCode = getRespondCodeContext(code);

    if(resCode == NULL) {
      logc(err, "Received invalid respond code: %d", code);
      return -1;
    }

    if (resCode->hasDescription) {
      uint32_t utflen;
      int ret = readUTFLen(&utflen, client->_peer->_stream);
      if (ret <= 0)
        return -1;
      memset(client->_description_buffer, 0, utflen+1);
      ret = readUTF(client->_description_buffer, utflen, client->_peer->_stream);
      if (ret <= 0)
        return -1;
    }

    if (code == CONFIRM_TRANSACTION) {
      logc(debug, "Site2Site transaction %s peer confirm transaction", transactionID);
      transaction->_state = TRANSACTION_CONFIRMED;
      return 0;
    } else if (code == BAD_CHECKSUM) {
      logc(debug, "Site2Site transaction %s peer indicate bad checksum", transactionID);
      return -1;
    } else {
      logc(debug, "Site2Site transaction %s peer unknown response code %d", transactionID, code);
      return -1;
    }
  } else {
    logc(debug, "Site2Site Send FINISH TRANSACTION for transaction %s", transactionID);
    if (writeResponse(client, FINISH_TRANSACTION, "FINISH_TRANSACTION") <= 0) {
      return -1;
    }

    RespondCode code;
    if(readResponse(client, &code) <= 0) {
      return -1;
    }

    RespondCodeContext *resCode = getRespondCodeContext(code);

    if(resCode == NULL) {
      logc(err, "Received invalid respond code: %d", code);
      return -1;
    }

    if (resCode->hasDescription) {
      uint32_t utflen;
      int ret = readUTFLen(&utflen, client->_peer->_stream);
      if (ret <= 0)
        return -1;
      memset(client->_description_buffer, 0, utflen+1);
      ret = readUTF(client->_description_buffer, utflen, client->_peer->_stream);
      if (ret <= 0)
        return -1;
    }

    // we've sent a FINISH_TRANSACTION. Now we'll wait for the peer to send a 'Confirm Transaction' response
    if (code == CONFIRM_TRANSACTION) {
      logc(debug, "Site2Site transaction %s peer confirm transaction with CRC %s", transactionID, client->_description_buffer);

      if (client->_currentVersion > 3) {
        int64_t crcValue = getCRC(transaction);
        char crc[40];
        memset(crc, 0, 40);
        sprintf(crc, "%lld", crcValue);

        if (strcmp(client->_description_buffer, crc) == 0) {
          logc(debug, "Site2Site transaction %s CRC matched", transactionID);
          if(writeResponse(client, CONFIRM_TRANSACTION, "CONFIRM_TRANSACTION") <= 0) {
            return -1;
          }
          transaction->_state = TRANSACTION_CONFIRMED;
          return 0;
        } else {
          logc(warn, "Site2Site transaction %s CRC not matched %s", transactionID, crc);
          writeResponse(client, BAD_CHECKSUM, "BAD_CHECKSUM");
          return -1;
        }
      }
      if (writeResponse(client, CONFIRM_TRANSACTION, "CONFIRM_TRANSACTION") <= 0) {
        return -1;
      }
      transaction->_state = TRANSACTION_CONFIRMED;
      return 0;
    } else {
      logc(debug, "Site2Site transaction %s peer unknown respond code %d", transactionID, code);
      return -1;
    }
  }
}

  int16_t sendPacket(struct CRawSiteToSiteClient * client, const char * transactionID, CDataPacket *packet, flow_file_record * ff) {

    if (client->_peer_state != READY) {
      bootstrap(client);
    }

    if (client->_peer_state != READY) {
      return -1;
    }
    CTransaction* transaction = findTransaction(client, transactionID);

    if (!transaction) {
      return -1;
    }

    if (getState(transaction) != TRANSACTION_STARTED && getState(transaction) != DATA_EXCHANGED) {
      logc(warn, "Site2Site transaction %s is not at started or exchanged state", transactionID);
      return -1;
    }

    if (getDirection(transaction) != SEND) {
      logc(warn, "Site2Site transaction %s direction is wrong", transactionID);
      return -1;
    }

    int ret;

    if (transaction->current_transfers_ > 0) {
      ret = writeResponse(client, CONTINUE_TRANSACTION, "CONTINUE_TRANSACTION");
      if (ret <= 0) {
        return -1;
      }
    }
    // start to read the packet
    uint32_t numAttributes = packet->_attributes->size;
    ret = write_uint32t(transaction, numAttributes);
    if (ret != 4) {
      return -1;
    }

    int i;
    for (i = 0; i < packet->_attributes->size; ++i) {
      const char *key = packet->_attributes->attributes[i].key;

      ret = write_UTF(transaction, key, True);

      if (ret <= 0) {
        return -1;
      }

      const char *value = (const char *) packet->_attributes->attributes[i].value;

      ret = write_UTF_len(transaction, value, packet->_attributes->attributes[i].value_size, True);
      if (ret <= 0) {
        return -1;
      }
    }

    uint64_t len = 0;

    uint64_t content_size = 0;

    if(ff != NULL) {
      content_size = ff->size;

      uint8_t * content_buf = NULL;

      if(content_size > 0 && ff->crp != NULL) {
        content_buf = (uint8_t*)malloc(content_size*sizeof(uint8_t));
        if(get_content(ff, content_buf, content_size) <= 0) {
          return -2;
        }
        ret = write_uint64t(transaction, len);
        if (ret != 8) {
          logc(debug, "ret != 8");
          return -1;
        }
        writeData(transaction, content_buf, len);
      }

    } else if (strlen(packet->payload_) > 0) {
      len = strlen(packet->payload_);

      ret = write_uint64t(transaction, len);
      if (ret != 8) {
        return -1;
      }

      ret = writeData(transaction, (uint8_t *)(packet->payload_), len);
      if (ret != (int64_t)len) {
        logc(debug, "ret != len");
        return -1;
      }
    }

    transaction->current_transfers_++;
    transaction->total_transfers_++;
    transaction->_state = DATA_EXCHANGED;
    transaction->_bytes += len;

    logc(info, "Site to Site transaction %s sent flow %d flow records, with total size %llu", transactionID,
        transaction->total_transfers_, transaction->_bytes);

    return 0;
  }

int readResponse(struct CRawSiteToSiteClient* client, RespondCode *code) {
  uint8_t firstByte;

  int ret = read_uint8_t(&firstByte, client->_peer->_stream);

  if (ret <= 0 || firstByte != CODE_SEQUENCE_VALUE_1)
    return -1;

  uint8_t secondByte;

  ret = read_uint8_t(&secondByte, client->_peer->_stream);

  if (ret <= 0 || secondByte != CODE_SEQUENCE_VALUE_2)
    return -1;

  uint8_t thirdByte;

  ret = read_uint8_t(&thirdByte, client->_peer->_stream);

  if (ret <= 0)
    return ret;

  *code = (RespondCode) thirdByte;

  RespondCodeContext *resCode = getRespondCodeContext(*code);

  if (resCode == NULL) {
    logc(err, "Received invalid response code: %u", thirdByte);
    return -1;
  }
  return 3;
}


int writeResponse(struct CRawSiteToSiteClient* client, RespondCode code, const char * message) {
  RespondCodeContext *resCode = getRespondCodeContext(code);

  if (resCode == NULL) {
    logc(err, "Received invalid respond code: %d", code);
    // Not a valid respond code
    return -1;
  }

  uint8_t codeSeq[3];
  codeSeq[0] = CODE_SEQUENCE_VALUE_1;
  codeSeq[1] = CODE_SEQUENCE_VALUE_2;
  codeSeq[2] = (uint8_t) code;

  int ret = write_buffer(codeSeq, 3, client->_peer->_stream);

  if (ret != 3)
    return -1;

  if (resCode->hasDescription) {
    ret = writeUTF(message, strlen(message), False, client->_peer->_stream);
    if (ret > 0) {
      return (3 + ret);
    } else {
      return ret;
    }
  } else {
    return 3;
  }
}

int writeRequestType(struct CRawSiteToSiteClient* client, RequestType type) {
  if (type >= MAX_REQUEST_TYPE)
    return -1;

  const char * typestr = RequestTypeStr[type];

  return writeUTF(typestr, strlen(typestr), False, client->_peer->_stream);
}

int readRequestType(struct CRawSiteToSiteClient* client, RequestType *type) {
  char requestTypeStr[128];

  uint32_t utflen;

  int ret = readUTFLen(&utflen, client->_peer->_stream);

  if (ret <= 0)
    return ret;

  memset(requestTypeStr, 0, 128);
  ret = readUTF(requestTypeStr, utflen, client->_peer->_stream);

  if (ret <= 0)
    return ret;

  requestTypeStr[utflen] = '\0';

  int i;
  for (i = NEGOTIATE_FLOWFILE_CODEC; i <= SHUTDOWN; i++) {
    if (strcmp(RequestTypeStr[i], requestTypeStr) == 0) {
      *type = (RequestType) i;
      return ret;
    }
  }

  return -1;
}

void tearDown(struct CRawSiteToSiteClient* client) {
  if (client->_peer_state >= ESTABLISHED) {
    // need to write shutdown request
    writeRequestType(client, SHUTDOWN);
  }

  clearTransactions(client);
  closePeer(client->_peer);
  client->_peer_state = IDLE;
}

int initiateResourceNegotiation(struct CRawSiteToSiteClient* client) {
  // Negotiate the version
  if (client->_peer_state != IDLE) {
    return -1;
  }

  int ret = writeUTF(getResourceName(client), strlen(getResourceName(client)), False, client->_peer->_stream);

  if (ret <= 0) {
    return -1;
  }

  ret = write_uint32_t(client->_currentVersion, client->_peer->_stream);

  if (ret <= 0) {
    return -1;
  }

  uint8_t statusCode;

  ret = read_uint8_t(&statusCode, client->_peer->_stream);


  if (ret <= 0) {
    return -1;
  }

  uint32_t serverVersion;

  switch (statusCode) {
    case RESOURCE_OK:
      logc(info, "Resource negotiation completed successfully. Using version: %u", client->_currentVersion);
      return 0;
    case DIFFERENT_RESOURCE_VERSION:
      ret = read_uint32_t(&serverVersion, client->_peer->_stream);
      if (ret <= 0) {
        return -1;
      }

      unsigned int i;
      for (i = (client->_currentVersionIndex + 1); i < sizeof(client->_supportedVersion) / sizeof(uint32_t); i++) {
        if (serverVersion >= client->_supportedVersion[i]) {
          client->_currentVersion = client->_supportedVersion[i];
          client->_currentVersionIndex = i;
          return initiateResourceNegotiation(client);
        }
      }
      logc(err, "Server version %u not supported", serverVersion);
      return -2;
    case NEGOTIATED_ABORT:
      logc(err, "%s", "Server aborted negotiation");
      return -2;
    default:
      logc(err, "Received invalid status code: %u", statusCode);
      return -1;
  }
}

int initiateCodecResourceNegotiation(struct CRawSiteToSiteClient* client) {
  // Negotiate the version
  if (client->_peer_state != HANDSHAKED) {
    return -1;
  }

  const char * coderresource = getCodecResourceName(client);

  int ret = writeUTF(coderresource, strlen(coderresource), False, client->_peer->_stream);

  if (ret <= 0) {
    return -1;
  }

  ret = write_uint32_t(client->_currentCodecVersion, client->_peer->_stream);


  if (ret <= 0) {
    return -1;
  }

  uint8_t statusCode;
  ret = read_uint8_t(&statusCode, client->_peer->_stream);

  if (ret <= 0) {
    return -1;
  }

  uint32_t serverVersion;
  switch (statusCode) {
    case RESOURCE_OK:
      logc(info, "Resource codec negotiation completed successfully. Using version: %u", client->_currentCodecVersion);
      return 0;
    case DIFFERENT_RESOURCE_VERSION:
      ret = read_uint32_t(&serverVersion, client->_peer->_stream);
      if (ret <= 0) {
        return -1;
      }

      unsigned int i;
      for (i = (client->_currentCodecVersionIndex + 1);
           i < sizeof(client->_supportedCodecVersion) / sizeof(uint32_t); i++) {
        if (serverVersion >= client->_supportedCodecVersion[i]) {
          client->_currentCodecVersion = client->_supportedCodecVersion[i];
          client->_currentCodecVersionIndex = i;
          return initiateCodecResourceNegotiation(client);
        }
      }
      logc(err, "Server codec version %u not supported", serverVersion);
      return -1;
    case NEGOTIATED_ABORT:
      logc(err, "%s", "Server aborted codec negotiation");
      return -2;
    default:
      logc(err, "Received invalid status code: %u", statusCode);
      return -1;
  }
}

int negotiateCodec(struct CRawSiteToSiteClient* client) {
  if (client->_peer_state != HANDSHAKED) {
    return -1;
  }
  int status = writeRequestType(client, NEGOTIATE_FLOWFILE_CODEC);

  if (status <= 0) {
    return -1;
  }

  if (initiateCodecResourceNegotiation(client) != 0) {
    return -2;
  }

  client->_peer_state = READY;
  return 0;
}

int establish(struct CRawSiteToSiteClient* client) {
  if (client->_peer_state != IDLE) {
    return -1;
  }

  if(openPeer(client->_peer) != 0) {
    return -1;
  }

  // Negotiate the version
  if(initiateResourceNegotiation(client) != 0) {
    return -1;
  }

  client->_peer_state = ESTABLISHED;

  return 0;
}

void addTransaction(struct CRawSiteToSiteClient * client, CTransaction * transaction) {
  HASH_ADD_STR(client->_known_transactions, _uuid_str, transaction);
}

CTransaction * findTransaction(const struct CRawSiteToSiteClient * client, const char * id) {
  CTransaction * transaction = NULL;
  HASH_FIND_STR(client->_known_transactions, id, transaction);
  return transaction;
}

void deleteTransaction(struct CRawSiteToSiteClient * client, const char * id) {
  CTransaction * transaction = findTransaction(client, id);
  if(transaction) {
    HASH_DEL(client->_known_transactions, transaction);
    free(transaction);
  }
}

void clearTransactions(struct CRawSiteToSiteClient * client) {
  if(client->_known_transactions == NULL) {
    return;
  }

  CTransaction *transaction, *tmp = NULL;

  HASH_ITER(hh, client->_known_transactions, transaction, tmp) {
    HASH_DEL(client->_known_transactions, transaction);
    free(transaction);
  }
}
