/**
 * @file ListS3.h
 * ListS3 class declaration
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

#include "S3Processor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

class ListS3 : public S3Processor {
 public:
  static constexpr char const* ProcessorName = "ListS3";

  // Supported Properties
  static const core::Property Delimiter;
  static const core::Property Prefix;
  static const core::Property UseVersions;
  static const core::Property MinimumObjectAge;
  static const core::Property WriteObjectTags;

  // Supported Relationships
  static const core::Relationship Success;

  explicit ListS3(std::string name, minifi::utils::Identifier uuid = minifi::utils::Identifier())
    : S3Processor(name, uuid, logging::LoggerFactory<ListS3>::getLogger()) {
  }

  explicit ListS3(std::string name, minifi::utils::Identifier uuid, std::unique_ptr<aws::s3::S3WrapperBase> s3_wrapper)
    : S3Processor(std::move(name), uuid, logging::LoggerFactory<ListS3>::getLogger(), std::move(s3_wrapper)) {
  }

  ~ListS3() override = default;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  aws::s3::ListRequestParameters list_request_params_;
  bool write_object_tags_ = false;
};

REGISTER_RESOURCE(ListS3, "This Processor retrieves a listing of objects from an Amazon S3 bucket.");

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
