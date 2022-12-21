/**
 * @file PutSQL.h
 * PutSQL class declaration
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

#include <string>

#include "core/ProcessSession.h"
#include "SQLProcessor.h"
#include "utils/ArrayUtils.h"
#include "core/logging/Logger.h"

namespace org::apache::nifi::minifi::processors {

class PutSQL : public SQLProcessor {
 public:
  explicit PutSQL(std::string name, const utils::Identifier& uuid = {});

  EXTENSIONAPI static constexpr const char* Description =
      "Executes a SQL UPDATE or INSERT command. The content of an incoming FlowFile is expected to be the SQL command to execute. "
      "The SQL command may use the ? character to bind parameters. In this case, the parameters to use must exist as FlowFile attributes with the naming convention "
      "sql.args.N.type and sql.args.N.value, where N is a positive integer. The content of the FlowFile is expected to be in UTF-8 format.";

  EXTENSIONAPI static const core::Property SQLStatement;
  static auto properties() {
    return utils::array_cat(SQLProcessor::properties(), std::array{SQLStatement});
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void processOnSchedule(core::ProcessContext& context) override;
  void processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  void initialize() override;
};

}  // namespace org::apache::nifi::minifi::processors
