/*
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

#include <processors/c2_heartbeat.h>
#include <coap/c2protocol.h>
#include <coap/c2payload.h>
#include <c2_api/c2api.h>

c2_server_response_t * send_heartbeat(c2context_t * c2_ctx) {
    c2heartbeat_t hb;
    memset(&hb, 0, sizeof(c2heartbeat_t));

    hb = prepare_c2_heartbeat(c2_ctx->agent_uuid, c2_ctx->start_time);
    if (c2_ctx->registration_required) {
        prepare_agent_manifest(c2_ctx, &hb);
        c2_ctx->registration_required = 0;
    }
    c2_payload_map_t * c2payload = c2_payload_heartbeat(hb);
    char * payload;
    size_t length = 0;

    serialize_payload(c2payload, &payload, &length);
    free_c2_payload(c2payload);
    free_c2heartbeat(&hb);

    if (length > 0) {
        CoapMessage message;
        message.data_ = (uint8_t *)payload;
        message.size_ = length;
        struct coap_message * coap_response = send_payload(c2_ctx, c2_ctx->heartbeat_uri, &message);
        free(payload);
        if (coap_response
            && coap_response->length == 8
            && coap_response->code == COAP_RESPONSE_400
            && memcmp(coap_response->data, "register", coap_response->length) == 0) {

            c2_ctx->registration_required = 1;
            return NULL;
        }
        if (coap_response) {
            c2_server_response_t * response = decode_c2_server_response(coap_response, c2_ctx->is_little_endian);
            free(coap_response->data);
            free(coap_response);
            return response;
        }
    }
    return NULL;
}

void send_acknowledge(c2context_t * c2, c2_response_t * resp) {
    char * payload;
    size_t len;
    c2_payload_map_t * c2_payload = c2_payload_agent_response(resp);
    serialize_payload(c2_payload, &payload, &len);
    free_c2_payload(c2_payload);
    if (len > 0) {
        CoapMessage message;
        message.data_ = (uint8_t *)payload;
        message.size_ = len;
        send_payload(c2, c2->acknowledge_uri, &message);
        free(payload);
    }
}

task_state_t c2_heartbeat_sender(void * args, void * state) {
    c2context_t  * c2 = (c2context_t *)args;

    acquire_lock(&c2->c2_lock);
    if (c2->shuttingdown) {
        c2->hb_stop = 1;
        condition_variable_broadcast(&c2->hb_stop_notify);
        release_lock(&c2->c2_lock);
        return DONOT_RUN_AGAIN;
    }
    release_lock(&c2->c2_lock);

    c2_response_t * c2_resp = NULL;
    while ((c2_resp = dequeue_c2_resp(c2))) {
        send_acknowledge(c2, c2_resp);
        free_c2_responses(c2_resp);
    }
    c2_server_response_t * response = send_heartbeat(c2);
    if (response) {
        enqueue_c2_serv_response(c2, response);
    }
    return RUN_AGAIN;
}
