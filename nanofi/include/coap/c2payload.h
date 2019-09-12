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

#ifndef NANOFI_INCLUDE_COAP_C2PAYLOAD_H_
#define NANOFI_INCLUDE_COAP_C2PAYLOAD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "uthash.h"
#include "utlist.h"
#include <coap/c2structs.h>

struct c2_payload_map;
struct c2_payload_list;

typedef enum value_type {
    NONE_TYPE,
    UINT8_TYPE,
    UINT16_TYPE,
    UINT32_TYPE,
    UINT64_TYPE,
    STRING_TYPE,
    HASH_TYPE,
    LIST_TYPE,
    PROP_TYPE
} value_type_t;

typedef struct value {
    uint8_t v_uint8;
    uint16_t v_uint16;
    uint32_t v_uint32;
    uint64_t v_uint64;
    char * v_str;
    struct c2_payload_map * v_map;
    struct properties * v_props;
    struct c2_payload_list * v_maplist;
    value_type_t val_type;
} value_t;

//list of values
typedef struct c2_payload_list {
    value_t value;
    struct c2_payload_list * next;
} c2_payload_list_t;

typedef struct c2_payload_map {
    uint16_t key;
    value_t value;
    UT_hash_handle hh;
} c2_payload_map_t;

c2_payload_map_t * c2_payload_heartbeat(c2heartbeat_t hb);
c2_payload_map_t * c2_payload_server_response(c2_server_response_t * sp);
c2_payload_map_t * c2_payload_agent_response(c2_response_t * ap);
void free_c2_payload(c2_payload_map_t * c2payload);

c2heartbeat_t * extract_c2_heartbeat(const c2_payload_map_t * c2_payload);
c2_server_response_t * extract_c2_server_response(const c2_payload_map_t * c2_payload);

void print_c2_payload(const c2_payload_map_t * c2_payload);

value_t value_uint8(uint8_t value);
value_t value_uint16(uint16_t value);
value_t value_uint32(uint32_t value);
value_t value_uint64(uint64_t value);
value_t value_string(const char * value);
value_t value_nstring(const unsigned char * value, size_t len);
value_t value_map(c2_payload_map_t * value);
value_t value_list(c2_payload_list_t * value);
value_t value_property(properties_t * props);
value_t value_none();
int is_value_none(value_t val);

#ifdef __cplusplus
}
#endif
#endif /* NANOFI_INCLUDE_COAP_C2PAYLOAD_H_ */
