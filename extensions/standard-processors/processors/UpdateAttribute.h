/**
 * @file UpdateAttribute.h
 * UpdateAttribute class declaration
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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_UPDATEATTRIBUTE_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_UPDATEATTRIBUTE_H_

#include <memory>
#include <string>
#include <vector>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class UpdateAttribute : public core::Processor {
 public:
  UpdateAttribute(const std::string& name,  const utils::Identifier& uuid = {}) // NOLINT
      : core::Processor(name, uuid),
        logger_(logging::LoggerFactory<UpdateAttribute>::getLogger()) {
  }

  /**
   * Relationships
   */

  static core::Relationship Success;
  static core::Relationship Failure;

  /**
   * NiFi API implementation
   */

  bool supportsDynamicProperties() override {
    return true;
  }

  void onSchedule(core::ProcessContext *context,
                          core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context,
                         core::ProcessSession *session) override;
  void initialize() override;

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  std::shared_ptr<logging::Logger> logger_;
  std::vector<core::Property> attributes_;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_UPDATEATTRIBUTE_H_
