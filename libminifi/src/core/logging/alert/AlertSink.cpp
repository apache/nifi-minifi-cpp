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

#include "core/logging/alert/AlertSink.h"
#include "core/TypedValues.h"
#include "core/ClassLoader.h"
#include "utils/HTTPClient.h"
#include "utils/Hash.h"
#include "core/logging/Utils.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using std::chrono_literals::operator""s;
using std::chrono_literals::operator""min;

namespace org::apache::nifi::minifi::core::logging {

AlertSink::AlertSink(Config config, std::shared_ptr<Logger> logger)
    : config_(std::move(config)),
      buffer_(config_.buffer_limit, config_.batch_size),
      logger_(std::move(logger)) {
  set_level(config_.level);
  live_logs_.setLifetime(config_.rate_limit);
  auto task_id = utils::IdGenerator::getIdGenerator()->generate();
  utils::Worker<utils::TaskRescheduleInfo> functor(std::bind(&AlertSink::run, this), task_id.to_string(), std::make_unique<utils::ComplexMonitor>());
  std::future<utils::TaskRescheduleInfo> future;
  thread_pool_.execute(std::move(functor), future);
}

std::shared_ptr<AlertSink> AlertSink::create(const std::string& prop_name_prefix, const std::shared_ptr<LoggerProperties>& logger_properties, std::shared_ptr<Logger> logger) {
  Config config;

  if (auto url = logger_properties->getString(prop_name_prefix + ".url")) {
    config.url = url.value();
  } else {
    logger->log_info("Missing '%s.url' value, network logging won't be available", prop_name_prefix);
    return {};
  }

  if (auto filter_str = logger_properties->getString(prop_name_prefix + ".filter")) {
    try {
      config.filter = filter_str.value();
    } catch (const std::regex_error& err) {
      logger->log_error("Invalid '%s.filter' value, network logging won't be available: %s", prop_name_prefix, err.what());
      return {};
    }
  } else {
    logger->log_error("Missing '%s.filter' value, network logging won't be available", prop_name_prefix);
    return {};
  }

  auto readPropertyOr = [&] (auto suffix, auto parser, auto fallback) {
    if (auto prop_str = logger_properties->getString(prop_name_prefix + suffix)) {
      if (auto prop_val = parser(prop_str.value())) {
        return prop_val.value();
      }
      logger->log_error("Invalid '%s' value, using default '%s'", prop_name_prefix + suffix, fallback);
    } else {
      logger->log_info("Missing '%s' value, using default '%s'", prop_name_prefix + suffix, fallback);
    }
    return parser(fallback).value();
  };

  auto datasize_parser = [] (const std::string& str) -> std::optional<int> {
    int val;
    if (DataSizeValue::StringToInt(str, val)) {
      return val;
    }
    return {};
  };

  config.batch_size = readPropertyOr(".batch.size", datasize_parser, "100 KB");
  config.flush_period = readPropertyOr(".flush.period", TimePeriodValue::fromString, "5 s").getMilliseconds();
  config.rate_limit = readPropertyOr(".rate.limit", TimePeriodValue::fromString, "10 min").getMilliseconds();
  config.buffer_limit = readPropertyOr(".buffer.limit", datasize_parser, "1 MB");
  config.level = readPropertyOr(".level", utils::parse_log_level, "trace");
  config.ssl_service_name = logger_properties->getString(prop_name_prefix + ".ssl.context.service");

  return std::make_shared<AlertSink>(std::move(config), std::move(logger));
}

void AlertSink::initialize(core::controller::ControllerServiceProvider* controller, std::shared_ptr<AgentIdentificationProvider> agent_id) {
  if (thread_pool_.isRunning()) {
    return;
  }

  agent_id_ = std::move(agent_id);

  if (config_.ssl_service_name) {
    if (!controller) {
      logger_->log_error("Could not find service '%s': no service provider", config_.ssl_service_name.value());
      return;
    }
    if (auto service = controller->getControllerService(config_.ssl_service_name.value())) {
      if (auto ssl_service = std::dynamic_pointer_cast<controllers::SSLContextService>(service)) {
        ssl_service_ = ssl_service;
      } else {
        logger_->log_error("Service '%s' is not an SSLContextService", config_.ssl_service_name.value());
        return;
      }
    } else {
      logger_->log_error("Could not find service '%s'", config_.ssl_service_name.value());
      return;
    }
  }


  thread_pool_.start();
}

void AlertSink::sink_it_(const spdlog::details::log_msg& msg) {
  // this method is protected upstream in base_sink by a mutex

  std::match_results<std::string_view::const_iterator> match;
  std::string_view payload(msg.payload.data(), msg.payload.size());
  if (!std::regex_match(payload.begin(), payload.end(), match, config_.filter)) {
    return;
  }
  size_t hash = 0;
  for (size_t idx = 1; idx < match.size(); ++idx) {
    std::string_view submatch;
    if (match[idx].first != match[idx].second) {
      submatch = std::string_view(std::to_address(match[idx].first), std::distance(match[idx].first, match[idx].second));
    }
    hash = utils::hash_combine(hash, std::hash<std::string_view>{}(submatch));
  }
  if (!live_logs_.tryAdd(hash)) {
    return;
  }

  spdlog::memory_buf_t formatted;
  formatter_->format(msg, formatted);

  buffer_.modify([&] (LogBuffer& log_buf) {
    log_buf.size_ += formatted.size();
    log_buf.data_.emplace_back(std::string{formatted.data(), formatted.size()}, hash);
  });
}

void AlertSink::flush_() {}

utils::TaskRescheduleInfo AlertSink::run() {
  LogBuffer logs;
  buffer_.commit();
  if (!buffer_.tryDequeue(logs)) {
    return utils::TaskRescheduleInfo::RetryIn(config_.flush_period);
  }

  auto client = core::ClassLoader::getDefaultClassLoader().instantiate<utils::BaseHTTPClient>("HTTPClient", "HTTPClient");
  if (!client) {
    logger_->log_error("Could not instantiate a HTTPClient object");
    return utils::TaskRescheduleInfo::RetryIn(config_.flush_period);
  }
  client->initialize("PUT", config_.url, ssl_service_);

  rapidjson::Document doc(rapidjson::kObjectType);
  std::string agent_id = agent_id_->getAgentIdentifier();
  doc.AddMember("agentId", rapidjson::Value(agent_id.data(), agent_id.length()), doc.GetAllocator());
  doc.AddMember("alerts", rapidjson::Value(rapidjson::kArrayType), doc.GetAllocator());
  for (const auto& [log, hash] : logs.data_) {
    doc["alerts"].PushBack(rapidjson::Value(log.data(), log.size()), doc.GetAllocator());
  }
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);

  auto data_input = std::make_unique<utils::ByteInputCallback>();
  auto data_cb = std::make_unique<utils::HTTPUploadCallback>();
  data_input->write(std::string(buffer.GetString(), buffer.GetSize()));
  data_cb->ptr = data_input.get();
  client->setUploadCallback(data_cb.get());
  client->setContentType("application/json");

  bool req_success = client->submit();

  int64_t resp_code = client->getResponseCode();
  const bool client_err = 400 <= resp_code && resp_code < 500;
  const bool server_err = 500 <= resp_code && resp_code < 600;
  if (client_err || server_err) {
    logger_->log_error("Error response code '" "%" PRId64 "' from '%s'", resp_code, config_.url);
  } else {
    logger_->log_debug("Response code '" "%" PRId64 "' from '%s'", resp_code, config_.url);
  }

  if (!req_success) {
    logger_->log_error("Failed to send alert request");
  }

  return utils::TaskRescheduleInfo::RetryIn(config_.flush_period);
}

AlertSink::LogBuffer AlertSink::LogBuffer::allocate(size_t /*size*/) {
  return {};
}

AlertSink::LogBuffer AlertSink::LogBuffer::commit() {
  return std::move(*this);
}

size_t AlertSink::LogBuffer::size() const {
  return size_;
}

void AlertSink::LiveLogSet::setLifetime(std::chrono::milliseconds lifetime) {
  lifetime_ = lifetime;
}

bool AlertSink::LiveLogSet::tryAdd(size_t hash) {
  auto now = std::chrono::steady_clock::now();
  auto limit = now - lifetime_;
  while (!ordered_.empty() && ordered_.front().first < limit) {
    ignored_.erase(ordered_.front().second);
    ordered_.pop_front();
  }

  if (!ignored_.insert(hash).second) {
    return false;
  }

  ordered_.emplace_back(now, hash);
  return true;
}

}  // namespace org::apache::nifi::minifi::core::logging
