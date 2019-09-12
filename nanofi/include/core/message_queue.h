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

#ifndef NANOFI_INCLUDE_CORE_MESSAGE_QUEUE_H_
#define NANOFI_INCLUDE_CORE_MESSAGE_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "utlist.h"
#include <stdint.h>
#include <core/ring_buffer.h>
#include <core/synchutils.h>
#include <core/cstructs.h>

typedef struct message_attrs {
    size_t length; //this is the length of the message corresponding to the payload in the ring buffer
    attribute_set as;
    struct message_attrs * next;
} message_attrs_t;

typedef message_attrs_t*(*attribute_set_cb_t)(message_t * msg, size_t len1, size_t len2);

typedef struct message_queue {
    ring_buffer_t * ring_buff;
    message_attrs_t * attrs;
    attribute_set_cb_t attr_cb;
    lock_t queue_lock;
    conditionvariable_t write_notify;
    conditionvariable_attr_t wrt_notify_attr;
    int stop;
} message_queue_t;

message_queue_t * create_msg_queue(uint64_t capacity_bytes);
void free_queue(message_queue_t * mq);
size_t enqueue_message(message_queue_t * mq, message_t * msg);
message_t * dequeue_message(message_queue_t * mq);
void set_attribute_update_cb(message_queue_t * mq, attribute_set_cb_t cb);
attribute * find_attribute(attribute_set as, const char * key);
void update_attribute_value(attribute * attr, const char * value);

void stop_message_queue(message_queue_t * queue);
void start_message_queue(message_queue_t * queue);
message_t * dequeue_message_nolock(message_queue_t * mq);

attribute_set prepare_attributes(properties_t * props);
const message_t * prepare_message(const char * payload, size_t len, attribute_set as);
void free_attributes(attribute_set as);
attribute_set copy_attributes(attribute_set as);
void free_message(message_t * msg);

#ifdef __cplusplus
}
#endif
#endif /* NANOFI_INCLUDE_CORE_MESSAGE_QUEUE_H_ */
