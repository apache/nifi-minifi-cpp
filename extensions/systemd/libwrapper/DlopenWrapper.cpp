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
#include "DlopenWrapper.h"

#include "utils/gsl.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::extensions::systemd::libwrapper {

namespace detail {
void dlclose_deleter::operator()(void* const libhandle) { if (libhandle) dlclose(libhandle); }
}  // namespace detail

class DlopenJournal : public Journal {
 public:
  DlopenJournal(DlopenWrapper& libhandle, const JournalType type)
      :libhandle_{&libhandle} {
    constexpr auto SD_JOURNAL_LOCAL_ONLY = 1 << 0;
    constexpr auto SD_JOURNAL_SYSTEM = 1 << 2;
    constexpr auto SD_JOURNAL_CURRENT_USER = 1 << 3;
    const int flags{(type == JournalType::User ? SD_JOURNAL_CURRENT_USER : 0)
        | (type == JournalType::System ? SD_JOURNAL_SYSTEM : 0)
        | SD_JOURNAL_LOCAL_ONLY};
    const int error_code = (*libhandle_->open_)(&j_, flags);
    if (error_code < 0) {
      // sd_journal_open returns negative errno values
      const auto err_cond = std::generic_category().default_error_condition(-error_code);
      throw SystemErrorException{"sd_journal_open", err_cond};
    }
  }
  DlopenJournal(const DlopenJournal&) = delete;
  DlopenJournal(DlopenJournal&&) = delete;
  DlopenJournal& operator=(const DlopenJournal&) = delete;
  DlopenJournal& operator=(DlopenJournal&&) = delete;

  ~DlopenJournal() override {
    if (j_) (*libhandle_->close_)(j_);
  }

  int seekHead() noexcept override { return (*libhandle_->seek_head_)(j_); }
  int seekTail() noexcept override { return (*libhandle_->seek_tail_)(j_); }
  int seekCursor(const char* const cursor) noexcept override { return (*libhandle_->seek_cursor_)(j_, cursor); }

  int getCursor(gsl::owner<char*>* const cursor_out) noexcept override { return (*libhandle_->get_cursor_)(j_, cursor_out); }

  int next() noexcept override { return (*libhandle_->next_)(j_); }
  int enumerateData(const void** const data_out, size_t* const size_out) noexcept override { return (*libhandle_->enumerate_data_)(j_, data_out, size_out); }

  int getRealtimeUsec(uint64_t* const usec_out) noexcept override { return (*libhandle_->get_realtime_usec_)(j_, usec_out); }

 private:
  gsl::not_null<DlopenWrapper*> libhandle_;
  gsl::owner<sd_journal*> j_ = nullptr;
};

std::unique_ptr<Journal> DlopenWrapper::openJournal(const JournalType type) {
  return std::make_unique<DlopenJournal>(*this, type);
}

}  // namespace org::apache::nifi::minifi::extensions::systemd::libwrapper
