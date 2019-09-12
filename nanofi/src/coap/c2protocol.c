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

#ifdef __cplusplus
extern "C" {
#endif

#include "cbor.h"
#include <coap/c2protocol.h>
#include <coap/c2agent.h>
#include <coap/c2payload.h>
#include "utlist.h"

#include <string.h>

void free_agent_manifest(c2heartbeat_t * hb) {
    if (!hb) return;

    int i;
    for (i = 0; i < hb->ag_manifest.num_ecus; ++i) {
        free((void *)hb->ag_manifest.ecus[i].input);
        free((void *)hb->ag_manifest.ecus[i].output);
        free((void *)hb->ag_manifest.ecus[i].name);
        free_properties(hb->ag_manifest.ecus[i].ip_args);
        free_properties(hb->ag_manifest.ecus[i].op_args);
    }
    free(hb->ag_manifest.ecus);

    /*for (i = 0; i < hb->ag_manifest.io.num_ips; ++i) {
        free(hb->ag_manifest.io.input_params[i].name);
        int n;
        for (n = 0; n < hb->ag_manifest.io.input_params[i].num_params; ++n) {
            free(hb->ag_manifest.io.input_params[i].params[n]);
        }
        free(hb->ag_manifest.io.input_params[i].params);
    }
    free(hb->ag_manifest.io.input_params);

    for (i = 0; i < hb->ag_manifest.io.num_ops; ++i) {
        free(hb->ag_manifest.io.output_params[i].name);
        int n;
        for (n = 0; n < hb->ag_manifest.io.output_params[i].num_params; ++n) {
            free(hb->ag_manifest.io.output_params[i].params[n]);
        }
        free(hb->ag_manifest.io.output_params[i].params);
    }
    free(hb->ag_manifest.io.output_params);*/
}

void free_c2heartbeat(c2heartbeat_t * c2_heartbeat) {
    if (!c2_heartbeat) {
        return;
    }
    const char * machine_arch = c2_heartbeat->device_info.system_info.machine_arch;
    free((void *)machine_arch);
    c2_heartbeat->device_info.system_info.machine_arch = NULL;

    const char * ac = c2_heartbeat->agent_info.agent_class;
    free((void *)ac);
    c2_heartbeat->agent_info.agent_class = NULL;

    const char * id = c2_heartbeat->device_info.ident;
    free((void *)id);
    c2_heartbeat->device_info.ident = NULL;

    const char * devid = c2_heartbeat->device_info.network_info.device_id;
    free((void *)devid);
    c2_heartbeat->device_info.network_info.device_id = NULL;
    if (c2_heartbeat->has_ag_manifest) {
        free_agent_manifest(c2_heartbeat);
    }
}

void build_cbor_map(cbor_item_t * cbor_map, uint16_t key, value_t value);

void build_cbor_list(cbor_item_t * item, value_t value) {
    if (!item || !cbor_isa_array(item)) return;

    switch (value.val_type) {
    case UINT8_TYPE: {
        cbor_array_push(item, cbor_move(cbor_build_uint8(value.v_uint8)));
        break;
    }
    case UINT16_TYPE: {
        cbor_array_push(item, cbor_move(cbor_build_uint16(value.v_uint16)));
        break;
    }
    case UINT32_TYPE: {
        cbor_array_push(item, cbor_move(cbor_build_uint32(value.v_uint32)));
        break;
    }
    case UINT64_TYPE: {
        cbor_array_push(item, cbor_move(cbor_build_uint64(value.v_uint64)));
        break;
    }
    case STRING_TYPE: {
        cbor_array_push(item, cbor_move(cbor_build_string(value.v_str)));
        break;
    }
    case HASH_TYPE: {
        cbor_item_t * map = cbor_new_indefinite_map();
        struct c2_payload_map * el, *tmp;
        HASH_ITER(hh, value.v_map, el, tmp) {
            build_cbor_map(map, el->key, el->value);
        }
        cbor_array_push(item, cbor_move(map));
        break;
    }
    case LIST_TYPE: {
        cbor_item_t * list = cbor_new_indefinite_array();
        c2_payload_list_t * el;
        LL_FOREACH(value.v_maplist, el) {
            build_cbor_list(list, el->value);
        }
        cbor_array_push(item, cbor_move(list));
        break;
    }
    case PROP_TYPE: {
        cbor_item_t * pmap = cbor_new_indefinite_map();
        properties_t * el, *tmp;
        HASH_ITER(hh, value.v_props, el, tmp) {
            cbor_map_add(pmap,
                         (struct cbor_pair){
                            .key = cbor_move(cbor_build_string(el->key)),
                            .value = cbor_move(cbor_build_string(el->value))
                         });
        }
        cbor_array_push(item, pmap);
        break;
    }
    default:
        break;
    }
}

void build_cbor_map(cbor_item_t * cbor_map, uint16_t key, value_t value) {
    if (!cbor_map || !cbor_isa_map(cbor_map)) return;

    switch (value.val_type) {
    case UINT8_TYPE: {
        cbor_map_add(cbor_map,
                     (struct cbor_pair){
                         .key = cbor_move(cbor_build_uint16(key)),
                         .value = cbor_move(cbor_build_uint8(value.v_uint8))});
        break;
    }
    case UINT16_TYPE: {
        cbor_map_add(cbor_map,
                     (struct cbor_pair){
                         .key = cbor_move(cbor_build_uint16(key)),
                         .value = cbor_move(cbor_build_uint16(value.v_uint16))});
        break;
    }
    case UINT32_TYPE: {
        cbor_map_add(cbor_map,
                     (struct cbor_pair){
                         .key = cbor_move(cbor_build_uint16(key)),
                         .value = cbor_move(cbor_build_uint32(value.v_uint32))});
        break;
    }
    case UINT64_TYPE: {
        cbor_map_add(cbor_map,
                     (struct cbor_pair){
                         .key = cbor_move(cbor_build_uint16(key)),
                         .value = cbor_move(cbor_build_uint64(value.v_uint64))});
        break;
    }
    case STRING_TYPE: {
        cbor_map_add(cbor_map,
                     (struct cbor_pair){
                         .key = cbor_move(cbor_build_uint16(key)),
                         .value = cbor_move(cbor_build_string(value.v_str))});
        break;
    }
    case HASH_TYPE: {
        cbor_item_t * map_item = cbor_new_indefinite_map();
        struct c2_payload_map * el, *tmp;
        HASH_ITER(hh, value.v_map, el, tmp) {
            build_cbor_map(map_item, el->key, el->value);
        }
        cbor_map_add(cbor_map,
                     (struct cbor_pair){
                         .key = cbor_move(cbor_build_uint16(key)),
                         .value = cbor_move(map_item)
                     });
        break;
    }
    case LIST_TYPE: {
        cbor_item_t * list_item = cbor_new_indefinite_array();
        c2_payload_list_t * el;
        LL_FOREACH(value.v_maplist, el) {
            build_cbor_list(list_item, el->value);
        }
        cbor_map_add(cbor_map,
                     (struct cbor_pair){
                         .key = cbor_move(cbor_build_uint16(key)),
                         .value = cbor_move(list_item)});
        break;
    }
    case PROP_TYPE: {
        cbor_item_t * map_item = cbor_new_indefinite_map();
        properties_t * el, *tmp;
        HASH_ITER(hh, value.v_props, el, tmp) {
            cbor_map_add(map_item,
                         (struct cbor_pair){
                            .key = cbor_move(cbor_build_string(el->key)),
                            .value = cbor_move(cbor_build_string(el->value))});
        }
        cbor_map_add(cbor_map,
                     (struct cbor_pair){
                         .key = cbor_move(cbor_build_uint16(key)),
                         .value = cbor_move(map_item)});
        break;
    }
    default:
        break;
    }
}

cbor_item_t * cborize_c2_payload(const c2_payload_map_t * c2payload) {
    if (!c2payload) {
        return NULL;
    }
    c2_payload_map_t * el, *tmp;
    cbor_item_t * c2_cbor = cbor_new_indefinite_map();
    HASH_ITER(hh, c2payload, el, tmp) {
        build_cbor_map(c2_cbor, el->key, el->value);
    }
    return c2_cbor;
}

size_t serialize_payload(const c2_payload_map_t * c2payload, char ** buff, size_t * length) {
    cbor_item_t * head = NULL;
    if (!c2payload || (head = cborize_c2_payload(c2payload)) == NULL) {
        *buff = NULL;
        *length = 0;
        return 0;
    }
    *length = cbor_serialize_alloc(head, buff, length);
    cbor_decref(&head);
    return *length;
}

properties_t * extract_properties(const struct cbor_pair * kvps, size_t sz) {
    properties_t * props = NULL;
    int i;
    for (i = 0; i < sz; ++i) {
        unsigned char * key_item = cbor_string_handle(kvps[i].key);
        size_t key_len = cbor_string_length(kvps[i].key);
        char * key = (char *)malloc(key_len + 1);
        memset(key, 0, key_len + 1);
        copynstr(key_item, key_len, key);

        unsigned char * val_item = cbor_string_handle(kvps[i].value);
        size_t val_len = cbor_string_length(kvps[i].value);
        char * val = (char *)malloc(val_len + 1);
        copynstr(val_item, val_len, val);
        properties_t * prop = (properties_t *)malloc(sizeof(properties_t));
        prop->key = key;
        prop->value = val;
        HASH_ADD_KEYPTR(hh, props, key, key_len, prop);
    }
    return props;
}

value_t load_cbor_items(cbor_item_t * item) {
    assert(item);
    if (cbor_isa_uint(item)) {
        switch (cbor_int_get_width(item)) {
        case CBOR_INT_8:
            return value_uint8(cbor_get_uint8(item));
        case CBOR_INT_16:
            return value_uint16(cbor_get_uint16(item));
        case CBOR_INT_32:
            return value_uint32(cbor_get_uint32(item));
        case CBOR_INT_64:
            return value_uint64(cbor_get_uint64(item));
        default:
            return value_none();
        }
    }

    if (cbor_isa_string(item)) {
         unsigned char * str = cbor_string_handle(item);
         size_t sz = cbor_string_length(item);
         return value_nstring(str, sz);
    }

    if (cbor_isa_map(item)) {
        c2_payload_map_t * map = NULL;
        if (cbor_map_size(item) == 0) {
            return value_map(map);
        }
        struct cbor_pair * kvps = cbor_map_handle(item);
        size_t sz = cbor_map_size(item);
        //check if we are parsing properties
        if (cbor_isa_string(kvps[0].key) && cbor_isa_string(kvps[0].value)) {
            properties_t * props = extract_properties(kvps, sz);
            value_t prop_vals = value_property(props);
            free_properties(props);
            return prop_vals;
        }

        int i;
        for (i = 0; i < sz; ++i) {
            uint16_t key = cbor_get_uint16(kvps[i].key);
            cbor_item_t * value_item = kvps[i].value;
            value_t value = load_cbor_items(value_item);
            add_kvp(&map, key, value);
        }
        return value_map(map);
    }

    if (cbor_isa_array(item)) {
        c2_payload_list_t * list = NULL;
        size_t sz =  cbor_array_size(item);
        cbor_item_t ** items = cbor_array_handle(item);
        int i;
        for (i = 0; i < sz; ++i) {
            value_t value = load_cbor_items(items[i]);
            add_list(&list, value);
        }
        return value_list(list);
    }
    return value_none();
}

c2_payload_map_t * prepare_c2_payload(cbor_item_t * item) {
    if (!cbor_isa_map(item)) {
        return NULL;
    }
    value_t val = load_cbor_items(item);
    return is_value_none(val) ? NULL : val.v_map;
}

c2_payload_map_t * deserialize_payload(const unsigned char * payload, size_t length) {
    if (!payload || !length) {
        return NULL;
    }
    struct cbor_load_result result;
    cbor_item_t * root = cbor_load(payload, length, &result);
    if (result.error.code != CBOR_ERR_NONE || !cbor_map_is_indefinite(root)) {
        cbor_decref(&root);
        return NULL;
    }
    c2_payload_map_t * ret = prepare_c2_payload(root);
    cbor_decref(&root);
    return ret;
}

uint16_t endian_check_uint16(uint16_t value, int is_little_endian) {
    unsigned char buf[2];
    memcpy(buf, &value, 2);
    if (is_little_endian) {
        return (buf[0] << 8) | buf[1];
    }
    return buf[0] | (buf[1] << 8);
}

uint32_t endian_check_uint32(uint32_t value, int is_little_endian) {
    unsigned char buf[4];
    memcpy(buf, &value, 4);
    if (is_little_endian) {
        return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    }
    return buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
}

uint64_t endian_check_uint64(uint64_t value, int is_little_endian) {
    unsigned char buf[8];
    memcpy(buf, &value, 8);
    if (is_little_endian) {
        return ((uint64_t) buf[0] << 56) | ((uint64_t) (buf[1] & 255) << 48) | ((uint64_t) (buf[2] & 255) << 40) | ((uint64_t) (buf[3] & 255) << 32) | ((uint64_t) (buf[4] & 255) << 24)
                | ((uint64_t) (buf[5] & 255) << 16) | ((uint64_t) (buf[6] & 255) << 8) | ((uint64_t) (buf[7] & 255) << 0);
    }
    return ((uint64_t) buf[0] << 0) | ((uint64_t) (buf[1] & 255) << 8) | ((uint64_t) (buf[2] & 255) << 16) | ((uint64_t) (buf[3] & 255) << 24) | ((uint64_t) (buf[4] & 255) << 32)
           | ((uint64_t) (buf[5] & 255) << 40) | ((uint64_t) (buf[6] & 255) << 48) | ((uint64_t) (buf[7] & 255) << 56);
}

c2operation get_operation(uint8_t type) {
    switch (type) {
        case 0:
            return ACKNOWLEDGE;
        case 1:
            return HEARTBEAT;
        case 2:
            return CLEAR;
        case 3:
            return DESCRIBE;
        case 4:
            return RESTART;
        case 5:
            return START;
        case 6:
            return UPDATE;
        case 7:
            return STOP;
    }
    return ACKNOWLEDGE;
}

c2_server_response_t * decode_c2_server_response(const struct coap_message * msg, int is_little_endian) {
    if (!msg) {
        return NULL;
    }
    const unsigned char * payload = msg->data;
    size_t length = msg->length;

    c2_payload_map_t * c2_payload = deserialize_payload(payload, length);
    c2_server_response_t *  c2_serv_response = extract_c2_server_response(c2_payload);
    free_c2_payload(c2_payload);
    return c2_serv_response;
}

#ifdef __cplusplus
}
#endif
