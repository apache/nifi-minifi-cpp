/**
 * @file DeleteS3Object.h
 * DeleteS3Object class declaration
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

#include <sstream>
#include <utility>
#include <memory>
#include <string>

#include "S3Processor.h"
#include "utils/GeneralUtils.h"

template<typename T>
class S3TestsFixture;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

class DeleteS3Object : public S3Processor {
 public:
  static constexpr char const* ProcessorName = "DeleteS3Object";

  // Supported Properties
  static const core::Property ObjectKey;
  static const core::Property Version;

  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  explicit DeleteS3Object(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : S3Processor(name, uuid, logging::LoggerFactory<DeleteS3Object>::getLogger()) {
  }

  ~DeleteS3Object() override = default;

  void initialize() override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::S3TestsFixture<DeleteS3Object>;

  explicit DeleteS3Object(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<aws::s3::S3RequestSender> s3_request_sender)
    : S3Processor(name, uuid, logging::LoggerFactory<DeleteS3Object>::getLogger(), std::move(s3_request_sender)) {
  }
};

REGISTER_RESOURCE(DeleteS3Object, "This Processor deletes FlowFiles on an Amazon S3 Bucket.");

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
