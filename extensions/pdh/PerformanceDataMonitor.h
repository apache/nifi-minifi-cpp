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

#pragma once

#include <pdh.h>
#include <string>
#include <vector>
#include <memory>
#include <utility>

#include "core/Processor.h"

#include "rapidjson/stream.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

#include "PerformanceDataCounter.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// PerformanceDataMonitor Class
class PerformanceDataMonitor : public core::Processor {
 public:
  static constexpr char const* JSON_FORMAT_STR = "JSON";
  static constexpr char const* OPEN_TELEMETRY_FORMAT_STR = "OpenTelemetry";

  explicit PerformanceDataMonitor(const std::string& name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid), output_format_(OutputFormat::kJSON),
      logger_(logging::LoggerFactory<PerformanceDataMonitor>::getLogger()),
      pdh_query_(nullptr), resource_consumption_counters_() {
  }
  ~PerformanceDataMonitor() override;
  static constexpr char const* ProcessorName = "PerformanceDataMonitor";
  // Supported Properties
  static core::Property PredefinedGroups;
  static core::Property CustomPDHCounters;
  static core::Property OutputFormatProperty;
  // Supported Relationships
  static core::Relationship Success;
  // Nest Callback Class for write stream
  class WriteCallback : public OutputStreamCallback {
   public:
    explicit WriteCallback(rapidjson::Document&& root) : root_(std::move(root)) {
    }
    rapidjson::Document root_;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) {
      rapidjson::StringBuffer buffer;
      rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
      root_.Accept(writer);
      return stream->write(reinterpret_cast<const uint8_t*>(buffer.GetString()), gsl::narrow<int>(buffer.GetSize()));
    }
  };

 public:
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize(void) override;

 protected:
  enum class OutputFormat {
    kJSON, kOpenTelemetry
  };

  rapidjson::Value& prepareJSONBody(rapidjson::Document& root);

  void setupMembersFromProperties(const std::shared_ptr<core::ProcessContext>& context);
  void addCountersFromPredefinedGroupsProperty(const std::string& custom_pdh_counters);
  void addCustomPDHCountersFromProperty(const std::string& custom_pdh_counters);

  OutputFormat output_format_;

  std::shared_ptr<logging::Logger> logger_;
  PDH_HQUERY pdh_query_;
  std::vector<PerformanceDataCounter*> resource_consumption_counters_;
};

REGISTER_RESOURCE(PerformanceDataMonitor, "This processor can create FlowFiles with various performance data through Performance Data Helper. (Windows only)");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
