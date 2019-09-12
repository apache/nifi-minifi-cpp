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

#ifndef NIFI_MINIFI_CPP_C2API_H
#define NIFI_MINIFI_CPP_C2API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <core/threadpool.h>
#include <core/cstructs.h>
#include <ecu_api/ecuapi.h>

typedef struct ecu_entry {
    char uuid[37];
    ecu_context_t * ecu;
    UT_hash_handle hh;
} ecu_entry_t;

typedef struct c2context {
    char * c2_host;
    char * c2_port;
    char * heartbeat_uri;
    char * acknowledge_uri;
    char agent_uuid[37];
    uint64_t start_time;

    ecu_entry_t * ecus;
    lock_t ecus_lock;

    c2_message_ctx_t * c2_msg_ctx;
    threadpool_t * thread_pool;

    unsigned started:1;
    unsigned shuttingdown:1;
    lock_t c2_lock;

    unsigned hb_stop:1;
    conditionvariable_t hb_stop_notify;

    uint8_t c2_consumer_stop;
	conditionvariable_t consumer_stop_notify;

    struct coap_messages * messages;
    lock_t coap_msgs_lock;

    uint8_t registration_required;

    on_start_callback_t on_start;
    on_stop_callback_t on_stop;
    on_start_callback_t on_update;
    on_stop_callback_t on_clear;

    int is_little_endian;
} c2context_t;

c2context_t * create_c2_agent(const char * c2host, const char * c2port);
void register_ecu(ecu_context_t * ecu, c2context_t * c2);
void unregister_ecu(ecu_context_t * ecu, c2context_t * c2);
int start_c2_agent(c2context_t * c2);
void stop_c2_agent(c2context_t * c2);
void destroy_c2_context(c2context_t * c2);

void set_start_callback(c2context_t * ctx, on_start_callback_t cb);
void set_stop_callback(c2context_t * ctx, on_stop_callback_t cb);
void set_update_callback(c2context_t * ctx, on_start_callback_t cb);

int is_little_endian();

#ifdef __cplusplus
}
#endif
#endif //NIFI_MINIFI_CPP_C2API_H
