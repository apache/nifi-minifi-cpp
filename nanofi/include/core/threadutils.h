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

#ifndef THREADUTILS_H_
#define THREADUTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <time.h>
#endif
#include <stdint.h>

typedef struct thread_handle {
#ifdef WIN32
	uintptr_t thread;
#else
	pthread_t thread;
#endif
} thread_handle_t;

#ifndef WIN32
typedef void*(*pthread_proc_type)(void *);
#endif

typedef struct thread_proc {
#ifdef WIN32
	_beginthreadex_proc_type threadfunc;
#else
	pthread_proc_type threadfunc;
#endif
} thread_proc_t;

int create_thread(thread_handle_t * hnd, thread_proc_t tproc, void * args);
void wait_thread_complete(thread_handle_t * hnd);
void thread_sleep_ms(uint64_t millis);

#ifdef __cplusplus
}
#endif
#endif /* THREADUTILS_H_ */
