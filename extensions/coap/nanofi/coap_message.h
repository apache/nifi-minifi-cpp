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
#ifndef EXTENSIONS_COAP_NANOFI_COAP_MESSAGE_H
#define EXTENSIONS_COAP_NANOFI_COAP_MESSAGE_H


#ifdef __cplusplus
extern "C" {
#endif

#include <coap2/coap.h>
#include <stdint.h>
#include <stdio.h>



/**
 * CoAP-2 in libcoap uses uint8_t *  while the first version uses a different type, so we will have to cast
 * the data. We have to keep this in mind with the API that we use.
 */
typedef struct {
  uint32_t code_;
  size_t size_;
  uint8_t *data_;
} CoapMessage;

/**
 * Create a new CoAMessage, taking ownership of the aforementioned buffers
 */
CoapMessage * const create_coap_message(const coap_pdu_t * const pdu);
/**
 * FRee the CoAP messages that are provided.
 */
void free_coap_message(CoapMessage *msg);

#ifdef __cplusplus
}
#endif

#endif /* EXTENSIONS_COAP_NANOFI_COAP_CONNECTION_H_ */
