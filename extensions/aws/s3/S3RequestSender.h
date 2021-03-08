/**
 * @file S3RequestSender.h
 * S3RequestSender class declaration
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
#include <memory>

#include "aws/core/auth/AWSCredentials.h"
#include "aws/core/client/ClientConfiguration.h"
#include "aws/core/utils/xml/XmlSerializer.h"
#include "aws/s3/model/PutObjectRequest.h"
#include "aws/s3/model/PutObjectResult.h"
#include "aws/s3/model/DeleteObjectRequest.h"
#include "aws/s3/model/GetObjectRequest.h"
#include "aws/s3/model/GetObjectResult.h"
#include "aws/s3/model/ListObjectsV2Request.h"
#include "aws/s3/model/ListObjectsV2Result.h"
#include "aws/s3/model/ListObjectVersionsRequest.h"
#include "aws/s3/model/ListObjectVersionsResult.h"
#include "aws/s3/model/GetObjectTaggingRequest.h"
#include "aws/s3/model/GetObjectTaggingResult.h"
#include "aws/s3/model/HeadObjectRequest.h"
#include "aws/s3/model/HeadObjectResult.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/OptionalUtils.h"
#include "utils/AWSInitializer.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace s3 {

struct ProxyOptions {
  std::string host;
  uint32_t port = 0;
  std::string username;
  std::string password;
};

class S3RequestSender {
 public:
  virtual minifi::utils::optional<Aws::S3::Model::PutObjectResult> sendPutObjectRequest(const Aws::S3::Model::PutObjectRequest& request) = 0;
  virtual bool sendDeleteObjectRequest(const Aws::S3::Model::DeleteObjectRequest& request) = 0;
  virtual minifi::utils::optional<Aws::S3::Model::GetObjectResult> sendGetObjectRequest(const Aws::S3::Model::GetObjectRequest& request) = 0;
  virtual minifi::utils::optional<Aws::S3::Model::ListObjectsV2Result> sendListObjectsRequest(const Aws::S3::Model::ListObjectsV2Request& request) = 0;
  virtual minifi::utils::optional<Aws::S3::Model::ListObjectVersionsResult> sendListVersionsRequest(const Aws::S3::Model::ListObjectVersionsRequest& request) = 0;
  virtual minifi::utils::optional<Aws::S3::Model::GetObjectTaggingResult> sendGetObjectTaggingRequest(const Aws::S3::Model::GetObjectTaggingRequest& request) = 0;
  virtual minifi::utils::optional<Aws::S3::Model::HeadObjectResult> sendHeadObjectRequest(const Aws::S3::Model::HeadObjectRequest& request) = 0;
  virtual ~S3RequestSender() = default;

  void setCredentials(const Aws::Auth::AWSCredentials& cred);
  void setRegion(const Aws::String& region);
  void setTimeout(uint64_t timeout);
  void setEndpointOverrideUrl(const Aws::String& url);
  void setProxy(const ProxyOptions& proxy);

 protected:
  const utils::AWSInitializer& AWS_INITIALIZER = utils::AWSInitializer::get();
  Aws::Client::ClientConfiguration client_config_;
  Aws::Auth::AWSCredentials credentials_;
  std::shared_ptr<minifi::core::logging::Logger> logger_{minifi::core::logging::LoggerFactory<S3RequestSender>::getLogger()};
};

}  // namespace s3
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
