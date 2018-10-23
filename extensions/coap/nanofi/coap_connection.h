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
#ifndef EXTENSIONS_COAP_NANOFI_COAP_CONNECTION_H_
#define EXTENSIONS_COAP_NANOFI_COAP_CONNECTION_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <coap2/coap.h>
#include <netdb.h>
#include "coap_message.h"
#include "coap_functions.h"



typedef struct {
  struct coap_context_t* ctx;
  struct coap_session_t* session;
  coap_address_t dst_addr;
  coap_address_t src_addr;
  coap_optlist_t *optlist;
} CoapPDU;


/**
 * Creates a connection to the server
 * @param type connection type
 * @param server server name
 * @param endpoint endpoint to connect to
 * @param port CoAP port
 * @param message message to send
 * @return CoAPPDU object.
 */
CoapPDU *create_connection(uint8_t type, const char * const server, const char * const endpoint, int port, const CoapMessage * const message);

/**
 * Sends the pdu
 * @param pdu to send
 * @return result 0 if success, failure otherwise
 */
int8_t send_pdu(const CoapPDU *const pdu);
/**
 * Frees the pdu
 * @param pdu to free
 * @return result 0 if success, failure otherwise
 */

int8_t free_pdu(CoapPDU *pdu);

#ifdef __cplusplus
}
#endif
#endif /* EXTENSIONS_COAP_NANOFI_COAP_CONNECTION_H_ */
