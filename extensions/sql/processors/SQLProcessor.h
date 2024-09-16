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
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/Enum.h"

#include "services/DatabaseService.h"

namespace org::apache::nifi::minifi::processors {

class SQLProcessor: public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr auto DBControllerService = core::PropertyDefinitionBuilder<>::createProperty("DB Controller Service")
      .withDescription("Database Controller Service.")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({DBControllerService});

 protected:
  SQLProcessor(std::string_view name, const utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger)
    : core::ProcessorImpl(name, uuid), logger_(std::move(logger)) {
  }

  static std::vector<std::string> collectArguments(const std::shared_ptr<core::FlowFile>& flow_file);

  virtual void processOnSchedule(core::ProcessContext& context) = 0;
  virtual void processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) = 0;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  void notifyStop() override {
    connection_.reset();
  }

  std::shared_ptr<core::logging::Logger> logger_;
  std::shared_ptr<sql::controllers::DatabaseService> db_service_;
  std::unique_ptr<sql::Connection> connection_;
};

}  // namespace org::apache::nifi::minifi::processors

