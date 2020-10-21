#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_map>
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
    worker_.enqueue([this] {
      journal_handle_.reset();
    }).get();
  }

  void initialize() final {
    setSupportedProperties({});
    // Set the supported relationships
    setSupportedRelationships({Success});
  }

  void onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* sessionFactory) final {
    gsl_Expects(context && sessionFactory);
    state_manager_ = context->getStateManager();
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

  void onTrigger(core::ProcessContext* context, core::ProcessSession* session) final {
    gsl_Expects(context && session);
    auto cursor_and_messages = worker_.enqueue([this] {
      std::vector<std::vector<journal_field>> messages;
      messages.reserve(batch_size_);
      std::unique_ptr<char, utils::FreeDeleter> cursor;
      journal_handle_->visit([this, &messages](sd_journal* const journal) {
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
        if (err_code < 0) throw std::system_error{ -err_code, std::generic_category() };
      });
      return std::make_pair(std::string{cursor.get()}, messages);
    }).get();
    state_manager_->set({{"cursor", std::move(cursor_and_messages.first)}});
    auto messages = std::move(cursor_and_messages.second);

    if (messages.empty()) {
      yield();
      return;
    }

    for(auto& msg: messages) {
      auto flow_file = session->create();
      for(auto& field: msg) {
        if (field.name != "MESSAGE") {
          flow_file->setAttribute(field.name, field.value);
        } else {
          struct StringOutputStreamCallback : OutputStreamCallback {
            explicit StringOutputStreamCallback(std::string content) :content{std::move(content)} {}

            int64_t process(std::shared_ptr<io::BaseStream> stream) final {
              const auto rv = stream->write(reinterpret_cast<const uint8_t*>(content.data()), content.size());
              content.clear();
              return rv;
            }

            std::string content;
          };
          StringOutputStreamCallback cb{ std::move(field.value) };
          session->write(flow_file, &cb);
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

  static utils::optional<journal_field> getNextField(sd_journal* const journal) {
    const void* data_ptr{};
    size_t data_length{};
    const auto status_code = sd_journal_enumerate_data(journal, &data_ptr, &data_length);
    if (status_code == 0) return {};
    if (status_code < 0) throw std::system_error{ -status_code, std::generic_category() };
    assert(data_ptr && "if sd_journal_enumerate_data was successful, then data_ptr must be set");
    assert(data_length > 0 && "if sd_journal_enumerate_data was successful, then data_length must be greater than zero");
    const char* const data_str_ptr = reinterpret_cast<const char*>(data_ptr);
    const char* const data_str_end_ptr = data_str_ptr + std::strlen(data_str_ptr);
    const auto eq_pos = std::find(data_str_ptr, data_str_end_ptr, '=');
    assert(eq_pos != data_str_end_ptr && "field string must contain an equals sign");
    return journal_field{std::string(data_str_ptr, eq_pos - data_str_ptr), std::string{eq_pos}};
  }

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ConsumeJournald>::getLogger();
  std::shared_ptr<core::CoreComponentStateManager> state_manager_;
  Worker worker_;
  utils::optional<JournalHandle> journal_handle_ = utils::make_optional(worker_.enqueue([]{ return JournalHandle{}; }).get());

  std::size_t batch_size_ = 10;
};

REGISTER_RESOURCE(ConsumeJournald, "Consume systemd-journald journal messages")

}}}}}}  // namespace org::apache::nifi::minifi::extensions::systemd
