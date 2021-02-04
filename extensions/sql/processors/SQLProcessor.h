/**
 * @file SQLProcessor.h
 * SQLProcessor class declaration
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

#include "core/Core.h"
#include "FlowFileRecord.h"
#include "concurrentqueue.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "utils/Enum.h"

#include "services/DatabaseService.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class SQLProcessor: public core::Processor {
 public:
  static const core::Property DBControllerService;

 protected:
  SQLProcessor(const std::string& name, utils::Identifier uuid, std::shared_ptr<logging::Logger> logger)
    : core::Processor(name, uuid), logger_(std::move(logger)) {
  }

  static std::vector<std::string> collectArguments(const std::shared_ptr<core::FlowFile>& flow_file);

  virtual void processOnSchedule(core::ProcessContext& context) = 0;
  virtual void processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) = 0;

  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;

  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;

  void notifyStop() override {
    connection_.reset();
  }

 protected:
   std::shared_ptr<logging::Logger> logger_;
   std::shared_ptr<sql::controllers::DatabaseService> dbService_;
   std::unique_ptr<sql::Connection> connection_;
   std::mutex onTriggerMutex_;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

