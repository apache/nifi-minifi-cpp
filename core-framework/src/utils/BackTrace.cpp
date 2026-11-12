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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "utils/BackTrace.h"

#ifndef HAS_EXECINFO
namespace org::apache::nifi::minifi::utils::trace_resolver {
BackTrace getBackTrace(std::string_view thread_name, std::thread::native_handle_type) {
  return BackTrace{.name = std::string{thread_name}, .trace = {}};
}
BackTrace getBackTrace(std::string_view thread_name) {
  return BackTrace{.name = std::string{thread_name}, .trace = {}};
}
}  // namespace org::apache::nifi::minifi::utils::trace_resolver
#else

#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#ifdef __linux__
#include <link.h>
#endif

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include "core/logging/LoggerFactory.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/Deleters.h"

using namespace std::literals::chrono_literals;

namespace {
std::optional<std::string> demangle_symbol(const char* symbol_name) {
  int status = 0;
  std::unique_ptr<char, org::apache::nifi::minifi::utils::FreeDeleter> demangled(abi::__cxa_demangle(symbol_name, nullptr, nullptr, &status));
  if (status == 0) {
    std::string demangled_name = demangled.get();
    return { demangled_name };
  } else {
    return {};
  }
}

constexpr size_t TRACE_BUFFER_SIZE = 128;

struct Capture {
  pthread_t target{};
  std::array<void*, TRACE_BUFFER_SIZE> stack_buffer{};
  int trace_size{0};
  std::atomic<bool> done{false};
};

std::vector<std::string> resolveRawTrace(const Capture& capture, const uint8_t frames_to_skip) {
  std::vector<std::string> trace;

  // Skip the innermost frame(s) common to all stack traces
  for (int i = frames_to_skip; i < capture.trace_size; i++) {
    const char* file_name = "???";
    uintptr_t symbol_offset = 0;

    /* Translate the address to symbolic information */
    Dl_info dl_info{};
#ifdef __linux__
    struct link_map* l_map = nullptr;
    int res = dladdr1(capture.stack_buffer[i], &dl_info, reinterpret_cast<void**>(&l_map), RTLD_DL_LINKMAP);
#else  // __linux__
    int res = dladdr(capture.stack_buffer[i], &dl_info);
#endif  // __linux__
    if (res == 0 || dl_info.dli_fname == nullptr || dl_info.dli_fname[0] == '\0') {
      /* We could not determine symbolic information for this address*/
      trace.emplace_back(file_name);
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
        ? demangle_symbol(dl_info.dli_sname).value_or(dl_info.dli_sname)
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
    symbol_offset = reinterpret_cast<uintptr_t>(capture.stack_buffer[i]) - base_address;

    trace.push_back(fmt::format("{} @ {} + {}", file_name, symbol_name, symbol_offset));
  }

  return trace;
}

std::atomic<Capture*> in_flight_capture{nullptr};

void handler(int /*signr*/, siginfo_t* /*info*/, void* /*secret*/) {
  const int saved_errno = errno;
  Capture* capture = in_flight_capture.load(std::memory_order_acquire);
  if (capture != nullptr && pthread_equal(pthread_self(), capture->target)
      && in_flight_capture.compare_exchange_strong(capture, nullptr, std::memory_order_acq_rel)) {
    capture->trace_size = backtrace(capture->stack_buffer.data(), static_cast<int>(capture->stack_buffer.size()));
    capture->done.store(true, std::memory_order_release);
  }
  errno = saved_errno;
}

void emplace_handler() {
  struct sigaction sa{};
  sigfillset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sa.sa_sigaction = handler;
  sigaction(SIGUSR2, &sa, nullptr);
}

bool wait_for_capture_done(const Capture& capture, const std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!capture.done.load(std::memory_order_acquire)) {
    if (std::chrono::steady_clock::now() >= deadline) { return false; }
    std::this_thread::sleep_for(100us);
  }
  return true;
}
}  // namespace

namespace org::apache::nifi::minifi::utils::trace_resolver {

BackTrace getBackTrace(std::string_view thread_name, std::thread::native_handle_type thread_handle) {
  static std::mutex mutex;
  std::scoped_lock guard(mutex);

  static const auto logger = core::logging::LoggerFactory<BackTrace>::getLogger();
  logger->log_trace("getting BackTrace for thread '{}'", thread_name);

  if (0 == thread_handle || pthread_equal(pthread_self(), thread_handle)) {
    Capture capture;
    capture.trace_size = backtrace(capture.stack_buffer.data(), static_cast<int>(capture.stack_buffer.size()));
    auto trace = resolveRawTrace(capture, 1);
    logger->log_debug("returning BackTrace for thread '{}' (current thread): {}", thread_name, trace);
    return BackTrace{.name = std::string{thread_name}, .trace = std::move(trace)};
  }

  [[maybe_unused]] static const bool warm_up_backtrace = [] {
    // Warm up backtrace()/libgcc here, where we are allowed to allocate, so that its one-time lazy dlopen never runs inside the signal handler.
    std::array<void*, 1> buffer{};
    backtrace(buffer.data(), static_cast<int>(buffer.size()));
    return true;
  }();

  [[maybe_unused]] static const bool signal_handler_installed = [] {
    emplace_handler();
    return true;
  }();

  auto capture = std::make_unique<Capture>();
  capture->target = thread_handle;

  in_flight_capture.store(capture.get(), std::memory_order_release);
  const auto zero_out_in_flight_capture = gsl::finally([&] {
    in_flight_capture.store(nullptr, std::memory_order_release);
  });

  if (pthread_kill(thread_handle, SIGUSR2) != 0) {
    logger->log_debug("SIGUSR2 failed, returning empty BackTrace for thread '{}'", thread_name);
    return BackTrace{.name = std::string{thread_name}, .trace = {}};
  }

  if (!wait_for_capture_done(*capture, 1s)) {
    logger->log_warn("timed out waiting for the stack trace of thread '{}', returning an empty BackTrace", thread_name);
    std::ignore = capture.release();  // leaked intentionally, so capture won't be deleted while handler() may still be running
    return BackTrace{.name = std::string{thread_name}, .trace = {}};
  }

  auto trace = resolveRawTrace(*capture, 1);
  logger->log_debug("returning BackTrace for thread '{}': {}", thread_name, trace);
  return BackTrace{.name = std::string{thread_name}, .trace = std::move(trace)};
}

BackTrace getBackTrace(std::string_view thread_name) {
  return getBackTrace(thread_name, pthread_self());
}

}  // namespace org::apache::nifi::minifi::utils::trace_resolver

#endif  // HAS_EXECINFO
