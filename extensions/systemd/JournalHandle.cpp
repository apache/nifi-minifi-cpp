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

#include "JournalHandle.h"

#include "Exception.h"
#include "utils/gsl.h"

#include <systemd/sd-journal.h>

namespace org { namespace apache { namespace nifi { namespace minifi { namespace extensions { namespace systemd {

namespace {
std::unique_ptr<sd_journal, detail::JournalHandleDeleter> journal_open(const JournalHandle::JournalType type) {
  const int flags = (type == JournalHandle::JournalType::USER ? SD_JOURNAL_CURRENT_USER : 0)
      | (type == JournalHandle::JournalType::SYSTEM ? SD_JOURNAL_SYSTEM : 0)
      | SD_JOURNAL_LOCAL_ONLY;
  sd_journal* journal = nullptr;
  const int error_code = sd_journal_open(&journal, flags);
  if (error_code < 0) {
    // sd_journal_open returns negative errno values
    const auto err_cond = std::generic_category().default_error_condition(-error_code);
    throw SystemErrorException{"sd_journal_open", err_cond};
  }
  // sd_journal handles can only be used from the same thread, see "man 8 sd_journal_open"
  const auto thread_id = std::this_thread::get_id();
  return std::unique_ptr<sd_journal, detail::JournalHandleDeleter>{journal, detail::JournalHandleDeleter{thread_id}};
}
}  // namespace

JournalHandle::JournalHandle(JournalType type)
    :owner_thread_id_{std::this_thread::get_id()},
    handle_{journal_open(type)}
{ }

void detail::JournalHandleDeleter::operator()(sd_journal *const j) const {
  gsl_Expects(std::this_thread::get_id() == owner_thread_id_);
  sd_journal_close(j);
}

}}}}}}  // namespace org::apache::nifi::minifi::extensions::systemd
