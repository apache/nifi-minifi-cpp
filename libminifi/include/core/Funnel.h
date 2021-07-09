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

#include "Processor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class Funnel : public Processor {
 public:
  // Supported Relationships
  static const core::Relationship Success;

  Funnel(const std::string& name, const utils::Identifier& uuid) : Processor(name, uuid), logger_(logging::LoggerFactory<Funnel>::getLogger()) {}
  explicit Funnel(const std::string& name) : Processor(name), logger_(logging::LoggerFactory<Funnel>::getLogger()) {}

  void initialize() override;

  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;

  annotation::Input getInputRequirement() const override {
    return annotation::Input::INPUT_REQUIRED;
  }

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
