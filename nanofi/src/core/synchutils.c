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

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <core/synchutils.h>

void initialize_lock(lock_t * lock) {
#ifndef WIN32
    pthread_mutex_init(&lock->mutex, NULL);
#else
    InitializeCriticalSection(&lock->cs);
#endif
}

#ifndef WIN32
void initialize_cvattr(conditionvariable_attr_t * cv_attr) {
    assert(cv_attr != NULL);
    pthread_condattr_init(&cv_attr->cv_attr);
    cv_attr->initialized = 1;
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
void condition_attr_set_clock(conditionvariable_attr_t * cv_attr, clockid_t clock) {
    assert(cv_attr != NULL);
    pthread_condattr_setclock(&cv_attr->cv_attr, clock);
}
#endif

void initialize_cv(conditionvariable_t * cv, conditionvariable_attr_t * cv_attr) {
    assert(cv != NULL);
#ifndef WIN32
    if (cv_attr && cv_attr->initialized) {
        pthread_cond_init(&cv->cv, &cv_attr->cv_attr);
    }
    else {
        pthread_cond_init(&cv->cv, NULL);
    }
#else
    InitializeConditionVariable(&cv->cv);
#endif
}

void acquire_lock(lock_t * lock) {
    assert(lock != NULL);
#ifndef WIN32
    pthread_mutex_lock(&lock->mutex);
#else
    EnterCriticalSection(&lock->cs);
#endif
}

void release_lock(lock_t * lock) {
    assert(lock != NULL);
#ifndef WIN32
    pthread_mutex_unlock(&lock->mutex);
#else
    LeaveCriticalSection(&lock->cs);
#endif
}

#ifndef WIN32
uint64_t get_time_millis(struct timespec ts) {
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec = ts.tv_nsec % 1000000000L;

    uint64_t ms = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000L);
    ts.tv_nsec = ts.tv_nsec % 1000000L;

    ms += lround((double)((double)ts.tv_nsec / 1000000L));
    return ms;
}

struct timespec get_timespec_millis_from_now(uint64_t millis) {
    struct timespec ts;
    memset(&ts, 0, sizeof(ts));
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_nsec += millis * 1000000;
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec = ts.tv_nsec % 1000000000L;
    return ts;
}
#endif

uint64_t get_now_ms() {
#ifndef WIN32
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return get_time_millis(ts);
#else
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER counts;
    QueryPerformanceCounter(&counts);
    LARGE_INTEGER milli_seconds;
    milli_seconds.QuadPart = counts.QuadPart * 1000;
    milli_seconds.QuadPart = milli_seconds.QuadPart / frequency.QuadPart;
    return (uint64_t)milli_seconds.QuadPart;
#endif
}

int condition_variable_timedwait(conditionvariable_t * cv, lock_t * lock, size_t millis) {
#if defined(__APPLE__)
    struct timespec ts;
    ts.tv_sec = millis / 1000L;
    ts.tv_nsec = (millis % 1000L) * 1000000L;
    return pthread_cond_timedwait_relative_np(&cv->cv, &lock->mutex, &ts);
#elif !defined(WIN32)
    struct timespec millis_from_now = get_timespec_millis_from_now(millis);
    return pthread_cond_timedwait(&cv->cv, &lock->mutex, &millis_from_now);
#else
    SetLastError(0);
    if (!SleepConditionVariableCS(&cv->cv, &lock->cs, millis)) {
        DWORD error = GetLastError();
        if (error == ERROR_TIMEOUT) {
            return ETIMEDOUT;
        }
    }
    return 0;
#endif
}

void condition_variable_wait(conditionvariable_t * cv, lock_t * lock) {
#ifndef WIN32
    pthread_cond_wait(&cv->cv, &lock->mutex);
#else
    SleepConditionVariableCS(&cv->cv, &lock->cs, INFINITE);
#endif
}

void condition_variable_signal(conditionvariable_t * cv) {
    assert(cv != NULL);
#ifndef WIN32
    pthread_cond_signal(&cv->cv);
#else
    WakeConditionVariable(&cv->cv);
#endif
}

void condition_variable_broadcast(conditionvariable_t * cv) {
    assert(cv != NULL);
#ifndef WIN32
    pthread_cond_broadcast(&cv->cv);
#else
    WakeAllConditionVariable(&cv->cv);
#endif
}

void destroy_cv(conditionvariable_t * cv) {
#ifndef WIN32
    assert(cv != NULL);
    pthread_cond_destroy(&cv->cv);
#endif
}

void destroy_cvattr(conditionvariable_attr_t * cv_attr) {
#ifndef WIN32
    assert(cv_attr != NULL);
    if (cv_attr->initialized) {
        pthread_condattr_destroy(&cv_attr->cv_attr);
    }
#endif
}

void destroy_lock(lock_t * lock) {
    assert(lock != NULL);
#ifndef WIN32
    pthread_mutex_destroy(&lock->mutex);
#else
    DeleteCriticalSection(&lock->cs);
#endif
}
