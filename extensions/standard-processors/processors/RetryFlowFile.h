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
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "core/state/nodes/MetricsBase.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/OptionalUtils.h"

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
  static constexpr char const* ProcessorName = "RetryFlowFile";
  // Supported Properties
  static core::Property RetryAttribute;
  static core::Property MaximumRetries;
  static core::Property PenalizeRetries;
  static core::Property FailOnNonNumericalOverwrite;
  static core::Property ReuseMode;
  // Supported Relationships
  static core::Relationship Retry;
  static core::Relationship RetriesExceeded;
  static core::Relationship Failure;
  // ReuseMode allowable values
  static constexpr char const* FAIL_ON_REUSE = "Fail on Reuse";
  static constexpr char const* WARN_ON_REUSE = "Warn on Reuse";
  static constexpr char const*   RESET_REUSE = "Reset Reuse";

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
  utils::optional<uint64_t> getRetryPropertyValue(const std::shared_ptr<core::FlowFile>& flow_file) const;
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

REGISTER_RESOURCE(RetryFlowFile,
    "FlowFiles passed to this Processor have a 'Retry Attribute' value checked against a configured 'Maximum Retries' value. "
    "If the current attribute value is below the configured maximum, the FlowFile is passed to a retry relationship. "
    "The FlowFile may or may not be penalized in that condition. If the FlowFile's attribute value exceeds the configured maximum, "
    "the FlowFile will be passed to a 'retries_exceeded' relationship. "
    "WARNING: If the incoming FlowFile has a non-numeric value in the configured 'Retry Attribute' attribute, it will be reset to '1'. "
    "You may choose to fail the FlowFile instead of performing the reset. Additional dynamic properties can be defined for any attributes "
    "you wish to add to the FlowFiles transferred to 'retries_exceeded'. These attributes support attribute expression language.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_RETRYFLOWFILE_H_
