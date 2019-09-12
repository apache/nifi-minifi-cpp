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

#include <processors/c2_consumer.h>
#include <coap/c2agent.h>

task_state_t c2_consumer(void * args, void * state) {
    c2context_t * c2 = (c2context_t *)args;

    acquire_lock(&c2->c2_lock);
    if (c2->shuttingdown) {
    	c2->c2_consumer_stop = 1;
        condition_variable_broadcast(&c2->consumer_stop_notify);
        release_lock(&c2->c2_lock);
        return DONOT_RUN_AGAIN;
    }
    release_lock(&c2->c2_lock);

    c2_server_response_t * c2_serv_resp = dequeue_c2_serv_response(c2);
    if (c2_serv_resp && c2_serv_resp->ident) {
        handle_c2_server_response(c2, c2_serv_resp);
        c2_response_t * c2_resp = prepare_c2_response(c2_serv_resp->ident);
        enqueue_c2_resp(c2, c2_resp);
    }
    free_c2_server_responses(c2_serv_resp);
    return RUN_AGAIN;
}


