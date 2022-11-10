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
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/CoreComponentState.h"
#include "core/Processor.h"
#include "core/logging/LoggerConfiguration.h"
#include "libwrapper/LibWrapper.h"
#include "utils/Deleters.h"
#include "utils/gsl.h"
#include "utils/FifoExecutor.h"

namespace org::apache::nifi::minifi::extensions::systemd {

enum class PayloadFormat { Raw, Syslog };

class ConsumeJournald final : public core::Processor {
 public:
  static constexpr const char* CURSOR_KEY = "cursor";
  static constexpr const char* PAYLOAD_FORMAT_RAW = "Raw";
  static constexpr const char* PAYLOAD_FORMAT_SYSLOG = "Syslog";
  static constexpr const char* JOURNAL_TYPE_USER = "User";
  static constexpr const char* JOURNAL_TYPE_SYSTEM = "System";
  static constexpr const char* JOURNAL_TYPE_BOTH = "Both";

  EXTENSIONAPI static constexpr const char* Description = "Consume systemd-journald journal messages. Creates one flow file per message."
      "Fields are mapped to attributes. Realtime timestamp is mapped to the 'timestamp' attribute.";

  EXTENSIONAPI static const core::Property BatchSize;
  EXTENSIONAPI static const core::Property PayloadFormat;
  EXTENSIONAPI static const core::Property IncludeTimestamp;
  EXTENSIONAPI static const core::Property JournalType;
  EXTENSIONAPI static const core::Property ProcessOldMessages;
  EXTENSIONAPI static const core::Property TimestampFormat;
  static auto properties() {
    return std::array{
      BatchSize,
      PayloadFormat,
      IncludeTimestamp,
      JournalType,
      ProcessOldMessages,
      TimestampFormat
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit ConsumeJournald(std::string name, const utils::Identifier& id = {}, std::unique_ptr<libwrapper::LibWrapper>&& = libwrapper::createLibWrapper());
  ConsumeJournald(const ConsumeJournald&) = delete;
  ConsumeJournald(ConsumeJournald&&) = delete;
  ConsumeJournald& operator=(const ConsumeJournald&) = delete;
  ConsumeJournald& operator=(ConsumeJournald&&) = delete;
  ~ConsumeJournald() final { notifyStop(); }

  void initialize() final;
  void notifyStop() final;
  void onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* sessionFactory) final;
  void onTrigger(core::ProcessContext* context, core::ProcessSession* session) final;

  friend struct ConsumeJournaldTestAccessor;
 private:
  struct journal_field {
    std::string name;
    std::string value;
  };

  struct journal_message {
    std::vector<journal_field> fields;
    std::chrono::system_clock::time_point timestamp;
  };

  static std::optional<gsl::span<const char>> enumerateJournalEntry(libwrapper::Journal&);
  static std::optional<journal_field> getNextField(libwrapper::Journal&);
  std::future<std::pair<std::string, std::vector<journal_message>>> getCursorAndMessageBatch();
  std::string formatSyslogMessage(const journal_message&) const;
  std::string getCursor() const;

  std::atomic<bool> running_{false};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ConsumeJournald>::getLogger();
  core::CoreComponentStateManager* state_manager_;
  std::unique_ptr<libwrapper::LibWrapper> libwrapper_;
  std::unique_ptr<utils::FifoExecutor> worker_;
  std::unique_ptr<libwrapper::Journal> journal_;

  std::size_t batch_size_ = 1000;
  systemd::PayloadFormat payload_format_ = systemd::PayloadFormat::Syslog;
  bool include_timestamp_ = true;
  std::string timestamp_format_ = "%x %X %Z";
};

}  // namespace org::apache::nifi::minifi::extensions::systemd
