// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "core/Core.h"
#include "core/ProcessorImpl.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/PropertyDefinition.h"
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
  using ProcessorImpl::ProcessorImpl;

  static std::vector<std::string> collectArguments(const std::shared_ptr<core::FlowFile>& flow_file);

  virtual void processOnSchedule(core::ProcessContext& context) = 0;
  virtual void processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) = 0;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  void notifyStop() override {
    connection_.reset();
  }

  std::shared_ptr<sql::controllers::DatabaseService> db_service_;
  std::unique_ptr<sql::Connection> connection_;
};

}  // namespace org::apache::nifi::minifi::processors

