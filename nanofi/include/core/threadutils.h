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
#include <synchapi.h>
#else
#include <pthread.h>
#include <time.h>
#endif
#include <stdint.h>

typedef struct lock {
#ifdef WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mutex;
#endif
} lock_t ;

typedef struct conditionvariable {
#ifdef WIN32
    CONDITION_VARIABLE cv;
#else
    pthread_cond_t cv;
#endif
} conditionvariable_t;

typedef struct conditionvariable_attr {
#ifndef WIN32
    pthread_condattr_t cv_attr;
#endif
    unsigned int initialized : 1;
} conditionvariable_attr_t;

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

#ifndef WIN32
void initialize_cvattr(conditionvariable_attr_t * cv_attr);
void condition_attr_set_clock(conditionvariable_attr_t * cv_attr, clockid_t clock);
#endif

void initialize_lock(lock_t * lock);
void initialize_cv(conditionvariable_t * cv, conditionvariable_attr_t * cv_attr);
void acquire_lock(lock_t * lock);
void release_lock(lock_t * lock);
int condition_variable_timedwait(conditionvariable_t * cv, lock_t * lock, size_t millis);
void condition_variable_wait(conditionvariable_t * cv, lock_t * lock);
void condition_variable_signal(conditionvariable_t * cv);
void condition_variable_broadcast(conditionvariable_t * cv);
void destroy_lock(lock_t * lock);
void destroy_cv(conditionvariable_t * cv);
void destroy_cvattr(conditionvariable_attr_t * cv_attr);

uint64_t get_now_ms();

#ifdef __cplusplus
}
#endif
#endif /* THREADUTILS_H_ */
