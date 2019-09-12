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

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <core/synchutils.h>
#include <core/threadutils.h>
#include <stdint.h>
#include "utlist.h"

typedef enum task_state {
    RUN_AGAIN,
    DONOT_RUN_AGAIN
} task_state_t;

typedef task_state_t (*function_t)(void * args, void * state);

typedef struct task {
    function_t function;
    void * args;
    void * state;
    int64_t start_time_ms;
    uint64_t interval_ms;
} task_t;

typedef struct task_node {
    task_t task;
    struct task_node * next;
} task_node_t;

typedef struct threadpool {
    task_node_t * task_queue;
	lock_t task_queue_lock;
    conditionvariable_t task_queue_cond;
    int num_threads;
    thread_handle_t * threads;
    int shuttingdown;
    int num_tasks;
    int started;
} threadpool_t;

task_node_t * create_repeatable_task(function_t function, void * args, void* state, uint64_t interval_seconds);
task_node_t * create_oneshot_task(function_t function, void * args, void * state);
int is_task_repeatable(task_t * task);
uint64_t get_task_repeat_interval(task_t * task);
uint64_t get_num_tasks(threadpool_t * pool);

void threadpool_add(threadpool_t * pool, task_node_t * task);
int threadpool_start(threadpool_t * pool);
void threadpool_shutdown(threadpool_t * pool);
threadpool_t * threadpool_create(uint64_t num_threads);

#ifndef WIN32
void * threadpool_thread_function(void * pool);
#else
unsigned __stdcall threadpool_thread_function(PVOID p);
#endif

#ifdef __cplusplus
}
#endif


#endif /* THREADPOOL_H_ */
