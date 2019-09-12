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

#ifndef NIFI_MINIFI_CPP_COAPPROTOCOL_H
#define NIFI_MINIFI_CPP_COAPPROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <c2_api/c2api.h>
#include "nanofi/coap_message.h"

typedef struct coap_message {
    uint8_t * data;
    size_t length;
    uint8_t code;
} coap_message;

typedef struct coap_messages {
    void * ctx; //key
    coap_message coap_msg;
    UT_hash_handle hh;
} coap_messages;

void initialize_coap(struct c2context * c2);
void insert_coap_message(struct c2context * c2, const struct coap_context_t * ctx, const struct coap_message * message);
struct coap_message * get_coap_message(struct c2context * c2, const struct coap_context_t * ctx);
struct coap_message * send_payload(struct c2context * c2, const char * endpoint, const CoapMessage * const message);

#ifdef __cplusplus
}
#endif
#endif //NIFI_MINIFI_CPP_COAPPROTOCOL_H
