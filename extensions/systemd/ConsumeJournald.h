#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <systemd/sd-journal.h>
#include <type_traits>
#include "core/CoreComponentState.h"
#include "core/Processor.h"
#include "core/Resource.h"
#include "core/logging/Logger.h"
#include "JournalHandle.h"
#include "utils/Deleters.h"
#include "utils/gsl.h"
#include "utils/OptionalUtils.h"
#include "WorkerThread.h"

namespace org { namespace apache { namespace nifi { namespace minifi { namespace extensions { namespace systemd {

class ConsumeJournald final : public core::Processor {
 public:
  static constexpr const char* CURSOR_KEY = "cursor";
  static const core::Relationship Success;

  explicit ConsumeJournald(const std::string& name, const utils::Identifier& id = {})
      :core::Processor{name, id}
  {}

  ~ConsumeJournald() final {
    notifyStop();
  }

  void initialize() final {
    setSupportedProperties({});
    // Set the supported relationships
    setSupportedRelationships({Success});
  }

  void notifyStop() final {
    if (!journal_handle_) { return; }
    worker_.enqueue([this] {
      journal_handle_.reset();
    }).get();
  }

  void onSchedule(core::ProcessContext* const context, core::ProcessSessionFactory* const sessionFactory) final {
    gsl_Expects(context && sessionFactory);
    state_manager_ = context->getStateManager();
    journal_handle_ = utils::make_optional(worker_.enqueue([]{ return JournalHandle{}; }).get());
    worker_.enqueue([this] {
      journal_handle_->visit([this](sd_journal* const journal) {
        const auto cursor = state_manager_->get() | utils::map([](std::unordered_map<std::string, std::string>&& m) { return m.at(CURSOR_KEY); });
        if (cursor) {
          sd_journal_seek_cursor(journal, cursor->c_str());
        } else {
          sd_journal_seek_head(journal);
        }
      });
    }).get();
  }

  void onTrigger(core::ProcessContext* const context, core::ProcessSession* const session) final {
    gsl_Expects(context && session);
    auto cursor_and_messages = getCursorAndMessageBatch().get();
    state_manager_->set({{"cursor", std::move(cursor_and_messages.first)}});
    auto messages = std::move(cursor_and_messages.second);

    if (messages.empty()) {
      yield();
      return;
    }

    for (auto& msg: messages) {
      const auto flow_file = session->create();
      for (auto& field: msg) {
        if (field.name == "MESSAGE") {
          session->writeBuffer(flow_file, gsl::make_span(field.value));
        } else {
          flow_file->setAttribute(std::move(field.name), std::move(field.value));
        }
      }
      session->transfer(flow_file, Success);
    }
    session->commit();
  }

 private:
  struct journal_field {
    std::string name;
    std::string value;
  };

  static utils::optional<gsl::span<const char>> enumerateJournalEntry(sd_journal* const journal) {
    gsl_Expects(journal);
    const void* data_ptr{};
    size_t data_length{};
    const auto status_code = sd_journal_enumerate_data(journal, &data_ptr, &data_length);
    if (status_code == 0) return {};
    if (status_code < 0) throw SystemErrorException{ "sd_journal_enumerate_data", std::generic_category().default_error_condition(-status_code) };
    gsl_Ensures(data_ptr && "if sd_journal_enumerate_data was successful, then data_ptr must be set");
    gsl_Ensures(data_length > 0 && "if sd_journal_enumerate_data was successful, then data_length must be greater than zero");
    const char* const data_str_ptr = reinterpret_cast<const char*>(data_ptr);
    return gsl::make_span(data_str_ptr, data_length);
  }

  static utils::optional<journal_field> getNextField(sd_journal* const journal) {
    gsl_Expects(journal);
    return enumerateJournalEntry(journal) | utils::map([](gsl::span<const char> field) {
      const auto eq_pos = std::find(std::begin(field), std::end(field), '=');
      gsl_Ensures(eq_pos != std::end(field) && "field string must contain an equals sign");
      const auto eq_idx = eq_pos - std::begin(field);
      return journal_field{
          utils::span_to<std::string>(field.subspan(0, eq_idx)),
          utils::span_to<std::string>(field.subspan(eq_idx + 1))
      };
    });
  }

  std::future<std::pair<std::string, std::vector<std::vector<journal_field>>>> getCursorAndMessageBatch() {
    return worker_.enqueue([this] {
      std::vector<std::vector<journal_field>> messages;
      messages.reserve(batch_size_);
      std::unique_ptr<char, utils::FreeDeleter> cursor;
      journal_handle_->visit([this, &messages, &cursor](sd_journal* const journal) {
        for (size_t i = 0; i < batch_size_ && sd_journal_next(journal) > 0; ++i) {
          std::vector<journal_field> message;
          utils::optional<journal_field> field;
          while ((field = getNextField(journal)).has_value()) {
            message.push_back(std::move(*field));
          }
          messages.push_back(std::move(message));
        }

        char* cursor_out;
        const auto err_code = sd_journal_get_cursor(journal, &cursor_out);
        if (err_code < 0) throw SystemErrorException{"sd_journal_get_cursor", std::generic_category().default_error_condition(-err_code)};
        gsl_Ensures(cursor_out);
        cursor.reset(cursor_out);
      });
      return std::make_pair(std::string{cursor.get()}, messages);
    });
  }

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ConsumeJournald>::getLogger();
  std::shared_ptr<core::CoreComponentStateManager> state_manager_;
  Worker worker_;
  utils::optional<JournalHandle> journal_handle_;

  std::size_t batch_size_ = 10;
};

REGISTER_RESOURCE(ConsumeJournald, "Consume systemd-journald journal messages")

}}}}}}  // namespace org::apache::nifi::minifi::extensions::systemd
