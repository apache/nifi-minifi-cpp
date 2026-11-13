// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>

#include "unit/TestBase.h"
#include "core/FlowConfiguration.h"
#include "core/RepositoryFactory.h"
#include "core/yaml/YamlConfiguration.h"
#include "core/flow/AdaptiveConfiguration.h"

class ConfigurationTestController : public TestController {
 public:
  ConfigurationTestController() {
    flow_file_repo_ = core::createRepository("flowfilerepository");
    configuration_ = std::make_shared<minifi::ConfigureImpl>();
    content_repo_ = std::make_shared<core::repository::VolatileContentRepository>();

    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setTrace<core::YamlConfiguration>();
    LogTestController::getInstance().setDebug<core::Processor>();
    LogTestController::getInstance().setTrace<core::flow::AdaptiveConfiguration>();
  }

  [[nodiscard]] core::ConfigurationContext getContext() const {
    return core::ConfigurationContext{
        .flow_file_repo = flow_file_repo_,
        .content_repo = content_repo_,
        .configuration = configuration_,
        .path = "",
        .filesystem = filesystem_,
        .sensitive_values_encryptor = sensitive_values_encryptor_
    };
  }

  std::shared_ptr<core::Repository> flow_file_repo_;
  std::shared_ptr<minifi::Configure> configuration_;
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::shared_ptr<utils::file::FileSystem> filesystem_{std::make_shared<utils::file::FileSystem>()};
  utils::crypto::EncryptionProvider sensitive_values_encryptor_ = utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{utils::crypto::XSalsa20Cipher::generateKey()}};
};
