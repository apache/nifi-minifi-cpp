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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_RETRYFLOWFILE_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_RETRYFLOWFILE_H_

#include <atomic>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "core/state/nodes/MetricsBase.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/OptionalUtils.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class RetryFlowFile : public core::Processor {
 public:
  explicit RetryFlowFile(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<RetryFlowFile>::getLogger()) {}
  // Destructor
  virtual ~RetryFlowFile() = default;
  // Processor Name
  EXTENSIONAPI static constexpr char const* ProcessorName = "RetryFlowFile";
  // Supported Properties
  EXTENSIONAPI static core::Property RetryAttribute;
  EXTENSIONAPI static core::Property MaximumRetries;
  EXTENSIONAPI static core::Property PenalizeRetries;
  EXTENSIONAPI static core::Property FailOnNonNumericalOverwrite;
  EXTENSIONAPI static core::Property ReuseMode;
  // Supported Relationships
  EXTENSIONAPI static core::Relationship Retry;
  EXTENSIONAPI static core::Relationship RetriesExceeded;
  EXTENSIONAPI static core::Relationship Failure;
  // ReuseMode allowable values
  EXTENSIONAPI static constexpr char const* FAIL_ON_REUSE = "Fail on Reuse";
  EXTENSIONAPI static constexpr char const* WARN_ON_REUSE = "Warn on Reuse";
  EXTENSIONAPI static constexpr char const*   RESET_REUSE = "Reset Reuse";

 public:
  bool supportsDynamicProperties() override {
    return true;
  }
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /* sessionFactory */) override;
  /**
   * Execution trigger for the RetryFlowFile Processor
   * @param context processor context
   * @param session processor session reference.
   */
  void onTrigger(core::ProcessContext* context, core::ProcessSession* session) override;

  // Initialize, overwrite by NiFi RetryFlowFile
  void initialize() override;


 private:
  void readDynamicPropertyKeys(core::ProcessContext* context);
  std::optional<uint64_t> getRetryPropertyValue(const std::shared_ptr<core::FlowFile>& flow_file) const;
  void setRetriesExceededAttributesOnFlowFile(core::ProcessContext* context, const std::shared_ptr<core::FlowFile>& flow_file) const;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  std::string retry_attribute_;
  uint64_t maximum_retries_ = 3;  // The real default value is set by the default on the MaximumRetries property
  bool penalize_retries_ =  true;  // The real default value is set by the default on the PenalizeRetries property
  bool fail_on_non_numerical_overwrite_ = false;  // The real default value is set by the default on the FailOnNonNumericalOverwrite property
  std::string reuse_mode_;
  std::vector<core::Property> exceeded_flowfile_attribute_keys_;

  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_RETRYFLOWFILE_H_
