/**
 * @file S3Wrapper.cpp
 * S3Wrapper class implementation
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
#include "S3WrapperBase.h"


namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace s3 {

void S3WrapperBase::setCredentials(const Aws::Auth::AWSCredentials& cred) {
  credentials_ = cred;
}

void S3WrapperBase::setRegion(const Aws::String& region) {
  client_config_.region = region;
}

void S3WrapperBase::setTimeout(uint64_t timeout) {
  client_config_.connectTimeoutMs = timeout;
}

void S3WrapperBase::setEndpointOverrideUrl(const Aws::String& url) {
  client_config_.endpointOverride = url;
}

minifi::utils::optional<PutObjectResult> S3WrapperBase::putObject(const PutS3RequestParameters& params, std::shared_ptr<Aws::IOStream> data_stream) {
  Aws::S3::Model::PutObjectRequest request;
  request.SetBucket(params.bucket);
  request.SetKey(params.object_key);
  request.SetStorageClass(storage_class_map.at(params.storage_class));
  request.SetServerSideEncryption(server_side_encryption_map.at(params.server_side_encryption));
  request.SetContentType(params.content_type);
  request.SetMetadata(params.user_metadata_map);
  request.SetBody(data_stream);

  return putObject(request);
}

} /* namespace s3 */
} /* namespace aws */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
