/**
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
#ifndef LIBMINIFI_INCLUDE_UTILS_BACKTRACE_H_
#define LIBMINIFI_INCLUDE_UTILS_BACKTRACE_H_

#include <string>

#ifdef HAS_EXECINFO
#include <execinfo.h>
#include <csignal>
#endif
#include <thread>
#include <utility>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <sstream>
#include <memory>

#define TRACE_BUFFER_SIZE 128

/**
 * Forward declaration allows us to tightly couple TraceResolver
 * with BackTrace.
 */
class TraceResolver;

/**
 * Purpose: Backtrace is a vector of trace lines.
 *
 */
class BackTrace {
 public:
  BackTrace() = default;

  BackTrace(std::string name) // NOLINT
      : name_(std::move(name)) {
  }

  std::vector<std::string> getTraces() const {
    return trace_;
  }

  /**
   * Return thread name of f this caller
   * @returns name ;
   */
  std::string getName() const {
    return name_;
  }

 protected:
  void addLine(std::string symbol_line) {
    trace_.emplace_back(std::move(symbol_line));
  }

 private:
  std::string name_;
  std::vector<std::string> trace_;
  friend class TraceResolver;
};

/**
 * Pulls the trace and places it onto the TraceResolver instance.
 */
void pull_trace(uint8_t frames_to_skip = 1);

/**
 * Emplaces a signal handler for SIGUSR2
 */
void emplace_handler();

/**
 * Purpose: Provides a singular instance to grab the call stack for thread(s).
 * Design: is a singleton to avoid multiple signal handlers.
 */
class TraceResolver {
 public:
  /**
   * Retrieves the backtrace for the provided thread reference
   * @return BackTrace instance
   */
  BackTrace getBackTrace(std::string thread_name, std::thread::native_handle_type thread);

  /**
   * Retrieves the backtrace for the calling thread
   * @returns BackTrace instance
   */
  BackTrace getBackTrace(std::string thread_name) {
#ifdef WIN32
    // currrently not supported in windows
    return BackTrace(std::move(thread_name));
#else
    return getBackTrace(std::move(thread_name), pthread_self());
#endif
  }

  /**
   * Returns a static instance of the thread resolver.
   */
  static TraceResolver &getResolver() {
    static TraceResolver resolver;
    return resolver;
  }

  /**
   * Adds a trace line with an optional function
   * @param symbol_line symbol line that was produced
   * @param func function name
   */
  void addTraceLine(const char *symbol_line, const char *func = nullptr) {
    std::stringstream line;
    line << symbol_line;
    if (nullptr != func) {
      line << " @" << func;
    }
    trace_.addLine(line.str());
  }

  /**
   * Adds a trace line in the format "<file_name> @ <symbol_name> + <symbol_offset>", if symbol_name is not nullptr,
   * "<file_name>" otherwise.
   * @param file_name the filename of the shared object
   * @param symbol_name the name of the symbol (or of the shared object, if the offset is supplied from that)
   * @param symbol_offset offset from the base address of the symbol (or the shared object)
   */
  void addTraceLine(const char* file_name, const char* symbol_name, uintptr_t symbol_offset) {
    std::stringstream line;
    line << file_name;
    if (symbol_name != nullptr) {
      line << " @ " << symbol_name << " + " << symbol_offset;
    }
    trace_.addLine(line.str());
  }

  std::unique_lock<std::mutex> lock() {
    return std::unique_lock<std::mutex>(trace_mutex_);
  }

  void notifyPullTracesDone(std::unique_lock<std::mutex>& lock) {
    std::unique_lock<std::mutex> tlock(std::move(lock));
    pull_traces_ = true;
    trace_condition_.notify_one();
  }

 private:
  TraceResolver() = default;

  BackTrace trace_;
  mutable std::mutex mutex_;

  bool pull_traces_{false};
  mutable std::mutex trace_mutex_;
  std::condition_variable trace_condition_;
};

#endif  // LIBMINIFI_INCLUDE_UTILS_BACKTRACE_H_

