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

#include "../TestBase.h"
#include "SQLiteConnection.h"

class SQLTestPlan {
 public:
  SQLTestPlan(TestController& controller, const utils::Path& database, const std::string& sql_processor, std::initializer_list<core::Relationship> output_rels) {
    plan_ = controller.createPlan();
    processor_ = plan_->addProcessor(sql_processor, sql_processor);
    plan_->setProperty(processor_, "DB Controller Service", "ODBCService");
    input_ = plan_->addConnection({}, {"success", "d"}, processor_);
    for (const auto& output_rel : output_rels) {
      outputs_[output_rel] = plan_->addConnection(processor_, output_rel, {});
    }

    // initialize database service
    auto service = plan_->addController("ODBCService", "ODBCService");
    plan_->setProperty(service,
         minifi::sql::controllers::DatabaseService::ConnectionString.getName(),
         "Driver=libsqlite3odbc.so;Database=" + database.str());
  }

  std::string getContent(const std::shared_ptr<core::FlowFile>& flow_file) {
    return plan_->getContent(flow_file);
  }

  std::shared_ptr<core::FlowFile> addInput(std::initializer_list<std::pair<std::string, std::string>> attributes = {}, const utils::optional<std::string>& content = {}) {
    auto flow_file = std::make_shared<minifi::FlowFileRecord>();
    for (const auto& attr : attributes) {
      flow_file->setAttribute(attr.first, attr.second);
    }
    if (content) {
      auto claim = std::make_shared<minifi::ResourceClaim>(plan_->getContentRepo());
      auto content_stream = plan_->getContentRepo()->write(*claim);
      int ret = content_stream->write(reinterpret_cast<uint8_t*>(const_cast<char*>(content->c_str())), content->length());
      REQUIRE(ret == content->length());
      flow_file->setOffset(0);
      flow_file->setSize(content->length());
      flow_file->setResourceClaim(claim);
    }
    input_->put(flow_file);
    return flow_file;
  }

  std::shared_ptr<core::Processor> getSQLProcessor() {
    return processor_;
  }

  void run(bool reschedule = false) {
    if (reschedule) {
      plan_->reset(reschedule);
    }
    plan_->runProcessor(0);  // run the one and only sql processor
  }

  std::map<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>> getAllOutputs() {
    std::map<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>> flow_file_map;
    for (const auto& output : outputs_) {
      flow_file_map[output.first] = getOutputs(output.first);
    }
    return flow_file_map;
  }

  std::vector<std::shared_ptr<core::FlowFile>> getOutputs(const core::Relationship& relationship) {
    auto conn = outputs_[relationship];
    REQUIRE(conn);
    std::vector<std::shared_ptr<core::FlowFile>> flow_files;
    std::set<std::shared_ptr<core::FlowFile>> expired;
    while (auto flow_file = conn->poll(expired)) {
      REQUIRE(expired.empty());
      flow_files.push_back(std::move(flow_file));
    }
    REQUIRE(expired.empty());
    return flow_files;
  }

 private:
  std::shared_ptr<TestPlan> plan_;
  std::shared_ptr<core::Processor> processor_;
  std::shared_ptr<minifi::Connection> input_;
  std::map<core::Relationship, std::shared_ptr<minifi::Connection>> outputs_;
};
