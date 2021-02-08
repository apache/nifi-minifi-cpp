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

#include <string>
#include <vector>
#include <memory>

#include "core/Property.h"
#include "utils/Enum.h"
#include "data/SQLRowsetProcessor.h"
#include "ProcessSession.h"
#include "data/JSONSQLWriter.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class FlowFileSource {
 public:
  static const std::string FRAGMENT_IDENTIFIER;
  static const std::string FRAGMENT_COUNT;
  static const std::string FRAGMENT_INDEX;

  static const core::Property OutputFormat;
  static const core::Property MaxRowsPerFlowFile;

  SMART_ENUM(OutputType,
    (JSON, "JSON"),
    (JSONPretty, "JSON-Pretty")
  )

 protected:
  class FlowFileGenerator : public sql::SQLRowSubscriber {
   public:
    FlowFileGenerator(core::ProcessSession& session, sql::JSONSQLWriter& json_writer)
      : session_(session),
        json_writer_(json_writer) {}

    void beginProcessBatch() override {
      current_batch_size_ = 0;
    }
    void endProcessBatch(State state) override;
    void beginProcessRow() override {}
    void endProcessRow() override {
      ++current_batch_size_;
    }
    void processColumnNames(const std::vector<std::string>& names) override {}
    void processColumn(const std::string& name, const std::string& value) override {}
    void processColumn(const std::string& name, double value) override {}
    void processColumn(const std::string& name, int value) override {}
    void processColumn(const std::string& name, long long value) override {}
    void processColumn(const std::string& name, unsigned long long value) override {}
    void processColumn(const std::string& name, const char* value) override {}

    std::shared_ptr<core::FlowFile> getLastFlowFile() {
      if (!flow_files_.empty()) {
        return flow_files_.back();
      }
      return {};
    }

    std::vector<std::shared_ptr<core::FlowFile>>& getFlowFiles() {
      return flow_files_;
    }

   private:
    core::ProcessSession& session_;
    sql::JSONSQLWriter& json_writer_;
    const utils::Identifier batch_id_{utils::IdGenerator::getIdGenerator()->generate()};
    size_t current_batch_size_{0};
    std::vector<std::shared_ptr<core::FlowFile>> flow_files_;
  };

  OutputType output_format_;
  size_t max_rows_{0};
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
