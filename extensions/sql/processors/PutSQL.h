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

#include "core/Resource.h"
#include "core/ProcessSession.h"
#include "SQLProcessor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

//! PutSQL Class
class PutSQL: public SQLProcessor {
 public:
  explicit PutSQL(const std::string& name, utils::Identifier uuid = utils::Identifier());

  //! Processor Name
  static const std::string ProcessorName;

  void processOnSchedule(core::ProcessContext& context) override;
  void processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  
  void initialize() override;

  static const core::Property SQLStatement;

  static const core::Relationship Success;
};

REGISTER_RESOURCE(PutSQL, "PutSQL to execute SQL command via ODBC.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
