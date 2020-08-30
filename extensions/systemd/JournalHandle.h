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

#pragma once

#include "WorkerThread.h"

#include <memory>
#include <thread>

struct sd_journal;

namespace org { namespace apache { namespace nifi { namespace minifi { namespace extensions { namespace systemd {

namespace detail {
class JournalHandleDeleter final {
 public:
  explicit JournalHandleDeleter(const std::thread::id owner_thread_id)
      :owner_thread_id_{owner_thread_id}
  {}

  void operator()(sd_journal* j) const;
 private:
  std::thread::id owner_thread_id_;
};
}  // namespace detail

/**
 * A handle to the log journal for reading. Has to be used from the same thread throughout its lifetime.
 * @see man 8 sd_journal_open
 */
class JournalHandle final {
 public:
  enum class JournalType { USER, SYSTEM, BOTH };
  explicit JournalHandle(JournalType = JournalType::BOTH);
 private:
  std::thread::id owner_thread_id_;
  std::unique_ptr<sd_journal, detail::JournalHandleDeleter> handle_;
};

}}}}}}  // namespace org::apache::nifi::minifi::extensions::systemd
