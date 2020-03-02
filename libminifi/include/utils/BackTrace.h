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

#ifdef HAS_EXECINFO
#include <execinfo.h>
#include <csignal>
#endif
#include <thread>
#include <utility>
#include <vector>
#include <mutex>
#include <iostream>
#include <sstream>

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

  BackTrace(std::string name)
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
   * Returns the thread handle reference in the native format.
   */
  std::thread::native_handle_type getThreadHandle() {
    return thread_handle_;
  }

  /**
   * Returns the caller handle reference in the native format.
   */
  std::thread::native_handle_type getCallerHandle() {
    return caller_handle_;
  }

 private:
  TraceResolver() = default;

  BackTrace trace_;
  std::thread::native_handle_type thread_handle_{0};
  std::thread::native_handle_type caller_handle_{0};
  std::mutex mutex_;
};

#endif /* LIBMINIFI_INCLUDE_UTILS_BACKTRACE_H_ */

