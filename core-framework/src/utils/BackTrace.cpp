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
#include <cxxabi.h>
#include <execinfo.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#ifdef __linux__
#include <link.h>
#endif
#include <cstring>
#include <iostream>
#include <utility>
#include <memory>

#include "utils/Deleters.h"
#endif

#include "utils/OptionalUtils.h"

#ifdef HAS_EXECINFO
namespace {
  using std::optional;
  /**
   * Demangles a symbol name using the cxx abi.
   * @param symbol_name the mangled name of the symbol
   * @return the demangled name on success, empty string on failure
   */
  optional<std::string> demangle_symbol(const char* symbol_name) {
    int status = 0;
    std::unique_ptr<char, org::apache::nifi::minifi::utils::FreeDeleter> demangled(abi::__cxa_demangle(symbol_name, nullptr, nullptr, &status));
    if (status == 0) {
      std::string demangled_name = demangled.get();
      return { demangled_name };
    } else {
      return {};
    }
  }
}  // namespace
#endif

void pull_trace([[maybe_unused]] uint8_t frames_to_skip /* = 1 */) {
#ifdef HAS_EXECINFO
  void* stack_buffer[TRACE_BUFFER_SIZE + 1];  // NOLINT(cppcoreguidelines-avoid-c-arrays)

  /* Get the backtrace of the current thread */
  int trace_size = backtrace(stack_buffer, TRACE_BUFFER_SIZE);

  /* We can skip the signal handler, call to pull_trace, and the first entry for backtrace_symbols */
  for (int i = frames_to_skip; i < trace_size; i++) {
    const char* file_name = "???";
    uintptr_t symbol_offset = 0;

    /* Translate the address to symbolic information */
    Dl_info dl_info{};
#ifdef __linux__
    struct link_map* l_map = nullptr;
    int res = dladdr1(stack_buffer[i], &dl_info, reinterpret_cast<void**>(&l_map), RTLD_DL_LINKMAP);
#else  // __linux__
    int res = dladdr(stack_buffer[i], &dl_info);
#endif  // __linux__
    if (res == 0 || dl_info.dli_fname == nullptr || dl_info.dli_fname[0] == '\0') {
      /* We could not determine symbolic information for this address*/
      TraceResolver::getResolver().addTraceLine(file_name, nullptr, symbol_offset);
      continue;
    }

    /* Determine the filename of the shared object */
    if (dl_info.dli_fname != nullptr) {
      const char* last_slash = strrchr(dl_info.dli_fname, '/');
      /* If the shared object name is a full path, we still only want the filename component */
      if (last_slash != nullptr) {
        file_name = last_slash + 1;
      } else {
        file_name = dl_info.dli_fname;
      }
    }

    const std::string symbol_name = dl_info.dli_sname
        ? demangle_symbol(dl_info.dli_sname).value_or(file_name)
        : file_name;

    /* Determine our offset */
    uintptr_t base_address = 0;
    if (dl_info.dli_sname != nullptr) {
      /* If we could determine our symbol we will display our offset from the address of the symbol */
      base_address = reinterpret_cast<uintptr_t>(dl_info.dli_saddr);
    } else {
      /* Otherwise we will display our offset from base address of the shared object */
#ifdef __linux__
      /*
       * glibc uses l_addr from the link map instead of dli_fbase in backtrace_symbols for calculating this offset.
       * I could not find a difference between the two in my limited measurements, but we will use it too, just to be sure.
       */
      if (l_map != nullptr) {
        dl_info.dli_fbase = reinterpret_cast<void*>(l_map->l_addr);  // NOLINT(performance-no-int-to-ptr)
      }
#endif  // __linux__
      base_address = reinterpret_cast<uintptr_t>(dl_info.dli_fbase);
    }
    symbol_offset = reinterpret_cast<uintptr_t>(stack_buffer[i]) - base_address;

    TraceResolver::getResolver().addTraceLine(file_name, symbol_name.c_str(), symbol_offset);
  }
#endif  // HAS_EXECINFO
}

BackTrace TraceResolver::getBackTrace(std::string thread_name, [[maybe_unused]] std::thread::native_handle_type thread_handle) {
  // lock so that we only perform one backtrace at a time.
#ifdef HAS_EXECINFO
  std::lock_guard<std::mutex> lock(mutex_);
  trace_ = BackTrace(std::move(thread_name));

  if (0 == thread_handle || pthread_equal(pthread_self(), thread_handle)) {
    pull_trace();
  } else {
    emplace_handler();
    std::unique_lock<std::mutex> ulock(trace_mutex_);
    if (pthread_kill(thread_handle, SIGUSR2) != 0) {
      return std::move(trace_);
    }
    pull_traces_ = false;
    trace_condition_.wait(ulock, [this] { return pull_traces_; });
  }
#else
  // even if tracing is disabled, include thread name into the trace object
  trace_ = BackTrace(std::move(thread_name));
#endif
  return std::move(trace_);
}
#ifdef HAS_EXECINFO
void handler(int /*signr*/, siginfo_t* /*info*/, void* /*secret*/) {
  std::unique_lock<std::mutex> lock(TraceResolver::getResolver().lock());
  pull_trace();
  TraceResolver::getResolver().notifyPullTracesDone(lock);
}
#endif

void emplace_handler() {
#ifdef HAS_EXECINFO
  struct sigaction sa{};
  sigfillset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = handler;
  sigaction(SIGUSR2, &sa, nullptr);
#endif
}
