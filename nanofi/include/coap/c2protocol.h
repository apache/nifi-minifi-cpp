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

#ifndef NIFI_MINIFI_CPP_C2PROTOCOL_H
#define NIFI_MINIFI_CPP_C2PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "coap/c2structs.h"
#include "coap/coapprotocol.h"
#include "coap/c2payload.h"

#include <stdint.h>

size_t serialize_payload(const c2_payload_map_t * c2payload, char ** buff, size_t * length);
c2_payload_map_t * deserialize_payload(const unsigned char * payload, size_t length);

void free_c2heartbeat(c2heartbeat_t * c2_heartbeat);
c2_server_response_t * decode_c2_server_response(const struct coap_message * msg, int is_little_endian);

#ifdef __cplusplus
}
#endif
#endif //NIFI_MINIFI_CPP_C2PROTOCOL_H
