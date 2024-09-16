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
