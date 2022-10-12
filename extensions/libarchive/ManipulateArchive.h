/**
 * @file ManipulateArchive.h
 * ManipulateArchive class declaration
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
#include <string>
#include <utility>

#include "FlowFileRecord.h"
#include "ArchiveMetadata.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"

#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

using core::logging::Logger;

class ManipulateArchive : public core::Processor {
 public:
  explicit ManipulateArchive(std::string name, const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid) {
  }
  ~ManipulateArchive() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Performs an operation which manipulates an archive without needing to split the archive into multiple FlowFiles.";

  EXTENSIONAPI static const core::Property Operation;
  EXTENSIONAPI static const core::Property Target;
  EXTENSIONAPI static const core::Property Destination;
  EXTENSIONAPI static const core::Property Before;
  EXTENSIONAPI static const core::Property After;
  static auto properties() {
    return std::array{
      Operation,
      Target,
      Destination,
      Before,
      After
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  // Supported operations
  EXTENSIONAPI static char const* OPERATION_REMOVE;
  EXTENSIONAPI static char const* OPERATION_COPY;
  EXTENSIONAPI static char const* OPERATION_MOVE;
  EXTENSIONAPI static char const* OPERATION_TOUCH;

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void initialize() override;

 private:
  std::shared_ptr<Logger> logger_ = core::logging::LoggerFactory<ManipulateArchive>::getLogger();
  std::string before_, after_, operation_, destination_, targetEntry_;
};

}  // namespace org::apache::nifi::minifi::processors
