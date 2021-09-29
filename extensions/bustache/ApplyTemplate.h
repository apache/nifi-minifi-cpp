/**
 * @file ApplyTemplate.h
 * ApplyTemplate class declaration
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

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/FlowFile.h"

namespace org::apache::nifi::minifi::processors {

/**
 * Applies a mustache template using incoming attributes as template parameters.
 */
class ApplyTemplate : public core::Processor {
 public:
  explicit ApplyTemplate(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid) {}
  static constexpr char const *ProcessorName = "ApplyTemplate";

  static const core::Property Template;

  static const core::Relationship Success;

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ApplyTemplate>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
