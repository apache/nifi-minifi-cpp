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
#include "core/state/LogMetricsPublisher.h"

#include "core/Resource.h"
#include "properties/Configuration.h"
#include "core/TypedValues.h"

namespace org::apache::nifi::minifi::state {

LogMetricsPublisher::~LogMetricsPublisher() {
  if (metrics_logger_thread_) {
    metrics_logger_thread_->stopAndJoin();
  }
}

void LogMetricsPublisher::initialize(const std::shared_ptr<Configure>& configuration, const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader) {
  state::MetricsPublisherImpl::initialize(configuration, response_node_loader);
  readLoggingInterval();
  readLogLevel();
}

void LogMetricsPublisher::logMetrics() {
  do {
    response::SerializedResponseNode parent_node;
    parent_node.name = "LogMetrics";
    {
      std::lock_guard<std::mutex> lock(response_nodes_mutex_);
      for (const auto& response_node : response_nodes_) {
        response::SerializedResponseNode metric_response_node;
        metric_response_node.name = response_node->getName();
        for (const auto& serialized_node : response_node->serialize()) {
          metric_response_node.children.push_back(serialized_node);
        }
        parent_node.children.push_back(metric_response_node);
      }
    }
    logger_->log_string(utils::LogUtils::mapToLogLevel(log_level_), parent_node.to_pretty_string());
  } while (!utils::StoppableThread::waitForStopRequest(logging_interval_));
}

void LogMetricsPublisher::readLoggingInterval() {
  gsl_Expects(configuration_);
  if (auto logging_interval_str = configuration_->get(Configure::nifi_metrics_publisher_log_metrics_logging_interval)) {
    if (auto logging_interval = minifi::core::TimePeriodValue::fromString(logging_interval_str.value())) {
      logging_interval_ = logging_interval->getMilliseconds();
      logger_->log_info("Metric logging interval is set to {}", logging_interval_);
      return;
    } else {
      logger_->log_error("Configured logging interval '{}' is invalid!", logging_interval_str.value());
    }
  }

  throw Exception(GENERAL_EXCEPTION, "Metrics logging interval not configured for log metrics publisher!");
}

void LogMetricsPublisher::readLogLevel() {
  gsl_Expects(configuration_);
  if (auto log_level_str = configuration_->get(Configure::nifi_metrics_publisher_log_metrics_log_level)) {
    log_level_ = magic_enum::enum_cast<utils::LogUtils::LogLevelOption>(*log_level_str, magic_enum::case_insensitive).value_or(utils::LogUtils::LogLevelOption::LOGGING_INFO);
    logger_->log_info("Metric log level is set to {}", magic_enum::enum_name(log_level_));
    return;
  }

  logger_->log_info("Metric log level is set to INFO by default");
}

void LogMetricsPublisher::clearMetricNodes() {
  {
    std::lock_guard<std::mutex> lock(response_nodes_mutex_);
    logger_->log_debug("Clearing all metric nodes.");
    response_nodes_.clear();
  }
  if (metrics_logger_thread_) {
    metrics_logger_thread_->stopAndJoin();
    metrics_logger_thread_.reset();
  }
}

void LogMetricsPublisher::loadMetricNodes() {
  gsl_Expects(response_node_loader_ && configuration_);
  auto metric_classes_str = configuration_->get(minifi::Configuration::nifi_metrics_publisher_log_metrics_publisher_metrics);
  if (!metric_classes_str || metric_classes_str->empty()) {
    metric_classes_str = configuration_->get(minifi::Configuration::nifi_metrics_publisher_metrics);
  }
  if (metric_classes_str && !metric_classes_str->empty()) {
    auto metric_classes = utils::string::split(*metric_classes_str, ",");
    std::lock_guard<std::mutex> lock(response_nodes_mutex_);
    for (const std::string& clazz : metric_classes) {
      auto loaded_response_nodes = response_node_loader_->loadResponseNodes(clazz);
      if (loaded_response_nodes.empty()) {
        logger_->log_warn("Metric class '{}' could not be loaded.", clazz);
        continue;
      }
      response_nodes_.insert(response_nodes_.end(), loaded_response_nodes.begin(), loaded_response_nodes.end());
    }
  }
  if (response_nodes_.empty()) {
    logger_->log_warn("LogMetricsPublisher is configured without any valid metrics!");
  }
  if (response_nodes_.empty() && metrics_logger_thread_) {
    metrics_logger_thread_->stopAndJoin();
    metrics_logger_thread_.reset();
  } else if (!response_nodes_.empty() && !metrics_logger_thread_) {
    metrics_logger_thread_ = std::make_unique<utils::StoppableThread>([this] { logMetrics(); });
  }
}

REGISTER_RESOURCE(LogMetricsPublisher, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state
