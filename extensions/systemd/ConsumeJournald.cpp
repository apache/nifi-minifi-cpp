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

#include "ConsumeJournald.h"

#include <algorithm>

#include "date/date.h"
#include "fmt/format.h"
#include "utils/GeneralUtils.h"
#include "utils/OptionalUtils.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::extensions::systemd {

namespace chr = std::chrono;

ConsumeJournald::ConsumeJournald(core::ProcessorMetadata metadata, std::unique_ptr<libwrapper::LibWrapper>&& libwrapper)
    : core::ProcessorImpl{std::move(metadata)}, libwrapper_{std::move(libwrapper)} {}

void ConsumeJournald::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);

  worker_ = std::make_unique<utils::FifoExecutor>();
}

void ConsumeJournald::notifyStop() {
  bool running = true;
  if (!running_.compare_exchange_strong(running, false, std::memory_order_acq_rel) || !journal_) return;
  worker_->enqueue([this] {
    journal_ = nullptr;
  }).get();
  worker_ = nullptr;
}

void ConsumeJournald::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  gsl_Expects(!running_ && worker_);

  batch_size_ = utils::parseU64Property(context, BatchSize);
  payload_format_ = utils::parseEnumProperty<systemd::PayloadFormat>(context, PayloadFormat);
  include_timestamp_ = utils::parseBoolProperty(context, IncludeTimestamp);
  const auto journal_type = utils::parseEnumProperty<systemd::JournalType>(context, JournalType);

  const auto process_old_messages = utils::parseBoolProperty(context, ProcessOldMessages);
  timestamp_format_ = [&context] {
    auto tf_prop = (context.getProperty(TimestampFormat)
        | utils::orElse([]{ throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "invalid timestamp format" }; }))
        .value();
    if (tf_prop == "ISO" || tf_prop == "ISO 8601" || tf_prop == "ISO8601") return std::string{"%FT%T%Ez"};
    return tf_prop;
  }();

  state_manager_ = context.getStateManager();
  // All journal-related API calls are thread-agnostic, meaning they need to be called from the same thread. In our environment,
  // where a processor can easily be scheduled on different threads, we ensure this by executing all library calls on a dedicated
  // worker thread. This is why all such operations are dispatched to a thread and immediately waited for in the initiating thread.
  journal_ = worker_->enqueue([this, journal_type]{ return libwrapper_->openJournal(journal_type); }).get();
  const auto seek_default = [process_old_messages](libwrapper::Journal& journal) {
    return process_old_messages ? journal.seekHead() : journal.seekTail();
  };
  worker_->enqueue([this, &seek_default] {
    const auto cursor = state_manager_->get() | utils::transform([](const std::unordered_map<std::string, std::string>& m) { return m.at(CURSOR_KEY); });
    if (!cursor) {
      seek_default(*journal_);
    } else {
      const auto ret = journal_->seekCursor(cursor->c_str());
      if (ret < 0) {
        const auto error_message = std::generic_category().default_error_condition(-ret).message();
        logger_->log_warn("Failed to seek to cursor: {}. Seeking to tail or head (depending on Process Old Messages property) instead. cursor=\"{}\"", error_message, *cursor);
        seek_default(*journal_);
      }
    }
  }).get();
  running_ = true;
}

void ConsumeJournald::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(state_manager_);
  if (!running_.load(std::memory_order_acquire)) return;
  auto cursor_and_messages = getCursorAndMessageBatch().get();
  auto messages = std::move(cursor_and_messages.second);
  if (messages.empty()) {
    context.yield();
    return;
  }

  for (auto& msg: messages) {
    const auto flow_file = session.create();
    if (payload_format_ == systemd::PayloadFormat::Syslog) session.writeBuffer(flow_file, gsl::make_span(formatSyslogMessage(msg)));
    for (auto& field: msg.fields) {
      if (field.name == "MESSAGE" && payload_format_ == systemd::PayloadFormat::Raw) {
        session.writeBuffer(flow_file, gsl::make_span(field.value));
      } else {
        flow_file->setAttribute(std::move(field.name), std::move(field.value));
      }
    }
    if (include_timestamp_) flow_file->setAttribute("timestamp", date::format(timestamp_format_, chr::floor<chr::microseconds>(msg.timestamp)));
    session.transfer(flow_file, Success);
  }
  state_manager_->set({{"cursor", std::move(cursor_and_messages.first)}});
}

std::optional<std::span<const char>> ConsumeJournald::enumerateJournalEntry(libwrapper::Journal& journal) {
  const void* data_ptr{};
  size_t data_length{};
  const auto status_code = journal.enumerateData(&data_ptr, &data_length);
  if (status_code == 0) return std::nullopt;
  if (status_code < 0) throw SystemErrorException{ "sd_journal_enumerate_data", std::generic_category().default_error_condition(-status_code) };
  gsl_Ensures(data_ptr && "if sd_journal_enumerate_data was successful, then data_ptr must be set");
  gsl_Ensures(data_length > 0 && "if sd_journal_enumerate_data was successful, then data_length must be greater than zero");
  const char* const data_str_ptr = reinterpret_cast<const char*>(data_ptr);
  return gsl::make_span(data_str_ptr, data_length);
}

std::optional<ConsumeJournald::journal_field> ConsumeJournald::getNextField(libwrapper::Journal& journal) {
  return enumerateJournalEntry(journal) | utils::transform([](std::span<const char> field) {
    const auto eq_pos = std::find(std::begin(field), std::end(field), '=');
    gsl_Ensures(eq_pos != std::end(field) && "field string must contain an equals sign");
    const auto eq_idx = gsl::narrow<size_t>(eq_pos - std::begin(field));
    return journal_field{
        utils::span_to<std::string>(field.subspan(0, eq_idx)),
        utils::span_to<std::string>(field.subspan(eq_idx + 1))
    };
  });
}

std::future<std::pair<std::string, std::vector<ConsumeJournald::journal_message>>> ConsumeJournald::getCursorAndMessageBatch() {
  gsl_Expects(worker_);
  return worker_->enqueue([this] {
    std::vector<journal_message> messages;
    messages.reserve(batch_size_);
    for (size_t i = 0; i < batch_size_ && journal_->next() > 0; ++i) {
      journal_message message;
      std::optional<journal_field> field;
      while ((field = getNextField(*journal_)).has_value()) {
        message.fields.push_back(std::move(*field));
      }
      if (include_timestamp_ || payload_format_ == systemd::PayloadFormat::Syslog) {
        message.timestamp = [this] {
          uint64_t journal_timestamp_usec_since_epoch{};
          journal_->getRealtimeUsec(&journal_timestamp_usec_since_epoch);
          return std::chrono::system_clock::time_point{std::chrono::microseconds{journal_timestamp_usec_since_epoch}};
        }();
      }
      messages.push_back(std::move(message));
    }

    return std::make_pair(getCursor(), messages);
  });
}

std::string ConsumeJournald::formatSyslogMessage(const journal_message& msg) const {
  gsl_Expects(msg.timestamp != decltype(msg.timestamp){});

  const std::string* systemd_hostname = nullptr;
  const std::string* syslog_pid = nullptr;
  const std::string* systemd_pid = nullptr;
  const std::string* syslog_identifier = nullptr;
  const std::string* message = nullptr;

  for (const auto& field: msg.fields) {
    if (field.name == "_HOSTNAME") systemd_hostname = &field.value;
    else if (field.name == "SYSLOG_PID") syslog_pid = &field.value;
    else if (field.name == "_PID") systemd_pid = &field.value;
    else if (field.name == "SYSLOG_IDENTIFIER") syslog_identifier = &field.value;
    else if (field.name == "MESSAGE") message = &field.value;
    else if (systemd_hostname && (syslog_pid || systemd_pid) && syslog_identifier && message) break;
  }

  gsl_Ensures(message && "MESSAGE is guaranteed to be present");

  const auto pid_string = utils::optional_from_ptr(syslog_pid)
      | utils::orElse([&] { return utils::optional_from_ptr(systemd_pid); })
      | utils::transform([](const std::string* const pid) { return fmt::format("[{}]", *pid); });

  return fmt::format("{} {} {}{}: {}",
      date::format(timestamp_format_, chr::floor<chr::microseconds>(msg.timestamp)),
      (utils::optional_from_ptr(systemd_hostname) | utils::transform(utils::dereference)).value_or("unknown_host"),
      (utils::optional_from_ptr(syslog_identifier) | utils::transform(utils::dereference)).value_or("unknown_process"),
      pid_string.value_or(std::string{}),
      *message);
}

std::string ConsumeJournald::getCursor() const {
  const auto cursor = [this] {
    gsl::owner<char*> cursor_out = nullptr;
    const auto err_code = journal_->getCursor(&cursor_out);
    if (err_code < 0) throw SystemErrorException{"sd_journal_get_cursor", std::generic_category().default_error_condition(-err_code)};
    gsl_Ensures(cursor_out);
    return std::unique_ptr<char, utils::FreeDeleter>{cursor_out};
  }();
  return std::string{cursor.get()};
}

REGISTER_RESOURCE(ConsumeJournald, Processor);

}  // namespace org::apache::nifi::minifi::extensions::systemd
