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

#include "LibsystemdWrapper.h"

#include "Exception.h"
#include <systemd/sd-journal.h>
#include <thread>
#include "utils/GeneralUtils.h"
#include "utils/gsl.h"

namespace org { namespace apache { namespace nifi { namespace minifi { namespace extensions { namespace systemd { namespace libwrapper {

struct LibsystemdJournal : public Journal {
  explicit LibsystemdJournal(const gsl::owner<sd_journal*> journal)
      : j_{journal}
  {}
  LibsystemdJournal(const LibsystemdJournal&) = delete;
  LibsystemdJournal(LibsystemdJournal&&) = delete;
  LibsystemdJournal& operator=(const LibsystemdJournal&) = delete;
  LibsystemdJournal& operator=(LibsystemdJournal&&) = delete;

  int seekHead() noexcept override { return sd_journal_seek_head(j_); }
  int seekTail() noexcept override { return sd_journal_seek_tail(j_); }
  int seekCursor(const char* const cursor) noexcept override { return sd_journal_seek_cursor(j_, cursor); }

  int getCursor(char** const cursor_out) noexcept override { return sd_journal_get_cursor(j_, cursor_out); }

  int next() noexcept override { return sd_journal_next(j_); }
  int enumerateData(const void** const data_out, size_t* const size_out) noexcept override { return sd_journal_enumerate_data(j_, data_out, size_out); }

  int getRealtimeUsec(uint64_t* const usec_out) noexcept override { return sd_journal_get_realtime_usec(j_, usec_out); }

  ~LibsystemdJournal() override { sd_journal_close(j_); }
 private:
  gsl::owner<sd_journal*> j_;
};


std::unique_ptr<Journal> LibsystemdWrapper::openJournal(JournalType type) {
  const int flags{(type == JournalType::User ? SD_JOURNAL_CURRENT_USER : 0)
                      | (type == JournalType::System ? SD_JOURNAL_SYSTEM : 0)
                      | SD_JOURNAL_LOCAL_ONLY};
  gsl::owner<sd_journal*> journal = nullptr;
  const int error_code = sd_journal_open(&journal, flags);
  if (error_code < 0) {
    // sd_journal_open returns negative errno values
    const auto err_cond = std::generic_category().default_error_condition(-error_code);
    throw SystemErrorException{"sd_journal_open", err_cond};
  }
  return utils::make_unique<LibsystemdJournal>(journal);
}

}}}}}}}  // namespace org::apache::nifi::minifi::extensions::systemd::libwrapper
