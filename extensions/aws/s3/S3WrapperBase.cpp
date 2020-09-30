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

#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace s3 {

void S3WrapperBase::setCredentials(const Aws::Auth::AWSCredentials& cred) {
  logger_->log_debug("Setting new AWS credentials");
  credentials_ = cred;
}

void S3WrapperBase::setRegion(const Aws::String& region) {
  logger_->log_debug("Setting new AWS region [%s]", region);
  client_config_.region = region;
}

void S3WrapperBase::setTimeout(uint64_t timeout) {
  logger_->log_debug("Setting AWS client connection timeout [%d]", timeout);
  client_config_.connectTimeoutMs = timeout;
}

void S3WrapperBase::setEndpointOverrideUrl(const Aws::String& url) {
  logger_->log_debug("Setting AWS endpoint url [%s]", url);
  client_config_.endpointOverride = url;
}

void S3WrapperBase::setProxy(const ProxyOptions& proxy) {
  logger_->log_debug("Setting AWS client proxy host [%s] port [%d]", proxy.host, proxy.port);
  client_config_.proxyHost = proxy.host;
  client_config_.proxyPort = proxy.port;
  client_config_.proxyUserName = proxy.username;
  client_config_.proxyPassword = proxy.password;
}

void S3WrapperBase::setCannedAcl(Aws::S3::Model::PutObjectRequest& request, const std::string& canned_acl) const {
  if (canned_acl.empty() || CANNED_ACL_MAP.find(canned_acl) == CANNED_ACL_MAP.end())
    return;

  logger_->log_debug("Setting AWS canned ACL [%s]", canned_acl);
  request.SetACL(CANNED_ACL_MAP.at(canned_acl));
}

minifi::utils::optional<PutObjectResult> S3WrapperBase::putObject(const PutObjectRequestParameters& params, std::shared_ptr<Aws::IOStream> data_stream) {
  Aws::S3::Model::PutObjectRequest request;
  request.SetBucket(params.bucket);
  request.SetKey(params.object_key);
  request.SetStorageClass(STORAGE_CLASS_MAP.at(params.storage_class));
  request.SetServerSideEncryption(SERVER_SIDE_ENCRYPTION_MAP.at(params.server_side_encryption));
  request.SetContentType(params.content_type);
  request.SetMetadata(params.user_metadata_map);
  request.SetBody(data_stream);
  request.SetGrantFullControl(params.fullcontrol_user_list);
  request.SetGrantRead(params.read_permission_user_list);
  request.SetGrantReadACP(params.read_acl_user_list);
  request.SetGrantWriteACP(params.write_acl_user_list);
  setCannedAcl(request, params.canned_acl);

  return putObject(request);
}

}  // namespace s3
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
