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

#include "uthash.h"
#include <string.h>
#include <errno.h>
#include <core/cuuid.h>
#include <core/log.h>
#include <core/string_utils.h>
#include <processors/c2_heartbeat.h>
#include <processors/c2_consumer.h>
#include <coap/coapprotocol.h>
#include <c2_api/c2api.h>

int is_little_endian() {
    const uint16_t x = 1;
    uint8_t * y = (uint8_t *)&x;
    return *y == 1;
}

c2context_t * create_c2_agent(const char * c2host, const char * c2port) {
    c2context_t * c2_ctx = (c2context_t *)malloc(sizeof(c2context_t));
    memset(c2_ctx, 0, sizeof(c2context_t));

    copystr(c2host, &c2_ctx->c2_host);
    copystr(c2port, &c2_ctx->c2_port);
    printf("host = %s, port = %s\n", c2_ctx->c2_host, c2_ctx->c2_port);

    const char * ack = "acknowledge";
    const char * hb = "heartbeat";

    copystr(ack, &c2_ctx->acknowledge_uri);
    copystr(hb, &c2_ctx->heartbeat_uri);

    initialize_lock(&c2_ctx->ecus_lock);
    c2_ctx->c2_msg_ctx = create_c2_message_context();

	initialize_lock(&c2_ctx->c2_lock);
    initialize_cv(&c2_ctx->consumer_stop_notify, NULL);
	initialize_cv(&c2_ctx->hb_stop_notify, NULL);
    c2_ctx->is_little_endian = is_little_endian();
    initialize_coap(c2_ctx);
    return c2_ctx;
}

void register_ecu(ecu_context_t * ecu, c2context_t * c2) {
    if (!c2 || !ecu) return;

    acquire_lock(&c2->ecus_lock);
    ecu_entry_t * el, *tmp;
    HASH_ITER(hh, c2->ecus, el, tmp) {
        HASH_DEL(c2->ecus, el);
        free(el);
    }
    ecu_entry_t * entry = (ecu_entry_t *)malloc(sizeof(ecu_entry_t));
    strcpy(entry->uuid, ecu->uuid);
    entry->ecu = ecu;
    HASH_ADD_STR(c2->ecus, uuid, entry);
    release_lock(&c2->ecus_lock);
}

void unregister_ecu(ecu_context_t * ecu, c2context_t * c2) {
    if (!c2 || !ecu) return;

    acquire_lock(&c2->ecus_lock);
    ecu_entry_t * out;
    HASH_FIND_STR(c2->ecus, ecu->uuid, out);
    if (out) {
        HASH_DEL(c2->ecus, out);
        free(out);
    }
    release_lock(&c2->ecus_lock);
}

int start_c2_agent(c2context_t * c2) {
    acquire_lock(&c2->c2_lock);
    if (c2->started) {
        logc(info, "C2 agent is already started");
        release_lock(&c2->c2_lock);
        return 0;
    }

    if (c2->shuttingdown) {
        logc(warn, "Could not start. C2 agent is shutting down");
        release_lock(&c2->c2_lock);
        return -1;
    }

    if (!c2->thread_pool) {
        threadpool_t * pool = threadpool_create(2);
        if (!pool) {
            release_lock(&c2->c2_lock);
            return -1;
        }
        c2->thread_pool = pool;
    }

    CIDGenerator gen;
    gen.implementation_ = CUUID_DEFAULT_IMPL;
    generate_uuid(&gen, c2->agent_uuid);
    c2->agent_uuid[36] = '\0';

    task_node_t * heartbeat_task = create_repeatable_task(&c2_heartbeat_sender, c2, NULL, 500);
    task_node_t * c2handler_task = create_repeatable_task(&c2_consumer, c2, NULL, 200);
    threadpool_add(c2->thread_pool, heartbeat_task);
    threadpool_add(c2->thread_pool, c2handler_task);
    if (threadpool_start(c2->thread_pool) < 0) {
        logc(err, "Could not start c2 agent. Threadpool failed to start {uuid: %s}", c2->agent_uuid);
        release_lock(&c2->c2_lock);
        return -1;
    }
    c2->started = 1;
    c2->shuttingdown = 0;
    c2->hb_stop = 0;
    c2->c2_consumer_stop = 0;
    c2->start_time = time(NULL);
    release_lock(&c2->c2_lock);
    logc(info, "C2 agent started {uuid: %s}", c2->agent_uuid);
    return 0;
}

void wait_tasks_complete(c2context_t * c2) {
    acquire_lock(&c2->c2_lock);

    while (!c2->hb_stop) {
        condition_variable_wait(&c2->hb_stop_notify, &c2->c2_lock);
    }

    while (!c2->c2_consumer_stop) {
        condition_variable_wait(&c2->consumer_stop_notify, &c2->c2_lock);
    }
    release_lock(&c2->c2_lock);
}

void stop_c2_agent(c2context_t * c2) {
    acquire_lock(&c2->c2_lock);
    if (!c2->started) {
        release_lock(&c2->c2_lock);
        return;
    }
    c2->started = 0;
    c2->shuttingdown = 1;
    release_lock(&c2->c2_lock);

    wait_tasks_complete(c2);
    threadpool_shutdown(c2->thread_pool);
    threadpool_t * pool = c2->thread_pool;
    free(pool);
    c2->thread_pool = NULL;
    logc(info, "C2 agent stopped, {uuid: %s}", c2->agent_uuid);
}

void set_start_callback(c2context_t * ctx, on_start_callback_t cb) {
    ctx->on_start = cb;
}

void set_stop_callback(c2context_t * ctx, on_stop_callback_t cb) {
    ctx->on_stop= cb;
}

void set_update_callback(c2context_t * ctx, on_start_callback_t cb) {
    ctx->on_update = cb;
}

void set_clear_callback(c2context_t * ctx, on_stop_callback_t cb) {
    ctx->on_clear = cb;
}

void destroy_c2_context(c2context_t * c2) {
    stop_c2_agent(c2);
    free((void *)c2->acknowledge_uri);
    free((void *)c2->heartbeat_uri);
    free((void *)c2->c2_host);
    free((void *)c2->c2_port);
    free_c2_coap_messages(c2->messages);
    free_c2_message_context(c2->c2_msg_ctx);

    ecu_entry_t * el, *tmp;
    HASH_ITER(hh, c2->ecus, el, tmp) {
        HASH_DEL(c2->ecus, el);
        free(el);
    }
    acquire_lock(&c2->ecus_lock);
    destroy_lock(&c2->ecus_lock);
    acquire_lock(&c2->c2_lock);
    destroy_cv(&c2->hb_stop_notify);
    destroy_cv(&c2->consumer_stop_notify);
    destroy_lock(&c2->c2_lock);
    free(c2);
}
