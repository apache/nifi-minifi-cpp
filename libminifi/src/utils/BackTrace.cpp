/* Licensed to the Apache Software Foundation (ASF) under one or more
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
#include "utils/BackTrace.h"
#ifdef HAS_EXECINFO
#include <execinfo.h>
#include <iostream>
#include <cxxabi.h>
#endif
#define NAME_SIZE 256

void pull_trace(const uint8_t frames_to_skip) {
#ifdef HAS_EXECINFO
  void *stackBuffer[TRACE_BUFFER_SIZE + 1];

  // retrieve current stack addresses
  int trace_size = backtrace(stackBuffer, TRACE_BUFFER_SIZE);

  char **symboltable = backtrace_symbols(stackBuffer, trace_size);
  /**
   * we can skip the signal handler, call to pull_trace, and the first entry for backtrace_symbols
   */
  for (int i = frames_to_skip; i < trace_size; i++) {
    char *start_parenthetical = 0;
    char *functor = 0;
    char *stop_parenthetical = 0;

    for (char *p = symboltable[i]; *p; ++p) {
      if (*p == '(') {
        start_parenthetical = p;
      } else if (*p == '+') {
        functor = p;
      } else if (*p == ')' && functor) {
        stop_parenthetical = p;
        break;
      }
    }
    bool hasFunc = start_parenthetical && functor && stop_parenthetical;
    if (hasFunc && start_parenthetical < functor) {
      *start_parenthetical++ = '\0';
      *functor++ = '\0';
      *stop_parenthetical = '\0';

      /**
       * Demangle the names -- this requires calling cxx api to demangle the function name.
       * not sending an allocated buffer, so we'll deallocate if status is zero.
       */

      int status;

      auto demangled = abi::__cxa_demangle(start_parenthetical, nullptr, nullptr, &status);
      if (status == 0) {
        TraceResolver::getResolver().addTraceLine(symboltable[i], demangled);
        free(demangled);
      } else {
        TraceResolver::getResolver().addTraceLine(symboltable[i], start_parenthetical);
      }
    } else {
      TraceResolver::getResolver().addTraceLine(symboltable[i], "");
    }
  }

  free(symboltable);
#endif
}

BackTrace &&TraceResolver::getBackTrace(const std::string &thread_name, std::thread::native_handle_type thread_handle) {
  // lock so that we only perform one backtrace at a time.
#ifdef HAS_EXECINFO
  std::lock_guard<std::mutex> lock(mutex_);

  caller_handle_ = pthread_self();
  thread_handle_ = thread_handle;
  trace_ = BackTrace(thread_name);

  if (0 == thread_handle_ || pthread_equal(caller_handle_, thread_handle)) {
    pull_trace();
  } else {
    if (thread_handle_ == 0) {
      return std::move(trace_);
    }
    emplace_handler();
    if (pthread_kill(thread_handle_, SIGUSR2) != 0) {
      return std::move(trace_);
    }
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGUSR2);
    sigsuspend(&mask);
  }
#else
  // even if tracing is disabled, include thread name into the trace object
  trace_ = BackTrace(thread_name);
#endif
  return std::move(trace_);
}
#ifdef HAS_EXECINFO
void handler(int signr, siginfo_t *info, void *secret) {
  auto curThread = pthread_self();

  // not the intended thread
  if (!pthread_equal(curThread, TraceResolver::getResolver().getThreadHandle())) {
    return;
  }

  pull_trace();

  pthread_kill(TraceResolver::getResolver().getCallerHandle(), SIGUSR2);
}
#endif

void emplace_handler() {
#ifdef HAS_EXECINFO
  struct sigaction sa;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = handler;
  sigaction(SIGUSR2, &sa, NULL);
#endif
}
