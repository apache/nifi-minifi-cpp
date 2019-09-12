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

#ifndef NIFI_MINIFI_CPP_C2AGENT_H
#define NIFI_MINIFI_CPP_C2AGENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "c2structs.h"
#include "c2protocol.h"
#include <c2_api/c2api.h>

c2heartbeat_t prepare_c2_heartbeat(const char * agent_id, uint64_t start_time);
void prepare_agent_manifest(c2context_t * c2_ctx, c2heartbeat_t * hb);
c2_response_t * prepare_c2_response(const char * operation_id);

c2_message_ctx_t * create_c2_message_context();
void free_c2_message_context(c2_message_ctx_t * ctx);
void free_c2_coap_messages(coap_messages * msgs);
void enqueue_c2_serv_response(c2context_t * c2, c2_server_response_t * serv_resp);
c2_server_response_t * dequeue_c2_serv_response(c2context_t * c2);
void enqueue_c2_resp(c2context_t * c2, c2_response_t * resp);
c2_response_t * dequeue_c2_resp(c2context_t * c2);

void handle_c2_server_response(c2context_t * c2, c2_server_response_t * resp);

void free_c2_responses(c2_response_t * resps);
void free_c2_server_responses(c2_server_response_t * resps);

#ifdef __cplusplus
}
#endif
#endif //NIFI_MINIFI_CPP_C2AGENT_H
