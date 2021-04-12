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

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/CoreComponentState.h"
#include "core/Processor.h"
#include "core/Resource.h"
#include "core/logging/Logger.h"
#include <date/date.h>
#include "JournalHandle.h"
#include "utils/Deleters.h"
#include "utils/gsl.h"
#include "utils/OptionalUtils.h"
#include "WorkerThread.h"

namespace org { namespace apache { namespace nifi { namespace minifi { namespace extensions { namespace systemd {

enum class PayloadFormat { Raw, Syslog };

class ConsumeJournald final : public core::Processor {
 public:
  static constexpr const char* CURSOR_KEY = "cursor";
  static constexpr const char* PAYLOAD_FORMAT_RAW = "Raw";
  static constexpr const char* PAYLOAD_FORMAT_SYSLOG = "Syslog";
  static constexpr const char* JOURNAL_TYPE_USER = "User";
  static constexpr const char* JOURNAL_TYPE_SYSTEM = "System";
  static constexpr const char* JOURNAL_TYPE_BOTH = "Both";

  static const core::Relationship Success;

  static const core::Property BatchSize;
  static const core::Property PayloadFormat;
  static const core::Property IncludeTimestamp;
  static const core::Property JournalType;

  explicit ConsumeJournald(const std::string& name, const utils::Identifier& id = {}, std::unique_ptr<libwrapper::LibWrapper>&& = libwrapper::createLibWrapper());
  ConsumeJournald(const ConsumeJournald&) = delete;
  ConsumeJournald(ConsumeJournald&&) = delete;
  ConsumeJournald& operator=(const ConsumeJournald&) = delete;
  ConsumeJournald& operator=(ConsumeJournald&&) = delete;
  ~ConsumeJournald() final { notifyStop(); }

  void initialize() final;
  void notifyStop() final;
  void onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* sessionFactory) final;
  void onTrigger(core::ProcessContext* context, core::ProcessSession* session) final;

 private:
  struct journal_field {
    std::string name;
    std::string value;
  };

  struct journal_message {
    std::vector<journal_field> fields;
    std::chrono::system_clock::time_point timestamp;
  };

  static utils::optional<gsl::span<const char>> enumerateJournalEntry(libwrapper::Journal&);
  static utils::optional<journal_field> getNextField(libwrapper::Journal&);
  std::future<std::pair<std::string, std::vector<journal_message>>> getCursorAndMessageBatch();
  static std::string formatSyslogMessage(const journal_message&);

 private:
  std::atomic<bool> running_{false};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ConsumeJournald>::getLogger();
  std::shared_ptr<core::CoreComponentStateManager> state_manager_;
  std::unique_ptr<libwrapper::LibWrapper> libwrapper_;
  std::unique_ptr<Worker> worker_;
  utils::optional<JournalHandle> journal_handle_;

  std::size_t batch_size_ = 10;
  systemd::PayloadFormat payload_format_ = systemd::PayloadFormat::Syslog;
  bool include_timestamp_ = true;
};

REGISTER_RESOURCE(ConsumeJournald, "Consume systemd-journald journal messages");

}}}}}}  // namespace org::apache::nifi::minifi::extensions::systemd
