/**
 * @file S3RequestSender.cpp
 * S3RequestSender class implementation
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
#include "S3RequestSender.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace s3 {


void S3RequestSender::setCredentials(const Aws::Auth::AWSCredentials& cred) {
  logger_->log_debug("Setting new AWS credentials");
  credentials_ = cred;
}

void S3RequestSender::setRegion(const Aws::String& region) {
  logger_->log_debug("Setting new AWS region [%s]", region);
  client_config_.region = region;
}

void S3RequestSender::setTimeout(uint64_t timeout) {
  logger_->log_debug("Setting AWS client connection timeout [%llu]", timeout);
  client_config_.connectTimeoutMs = timeout;
}

void S3RequestSender::setEndpointOverrideUrl(const Aws::String& url) {
  logger_->log_debug("Setting AWS endpoint url [%s]", url);
  client_config_.endpointOverride = url;
}

void S3RequestSender::setProxy(const ProxyOptions& proxy) {
  logger_->log_debug("Setting AWS client proxy host [%s] port [%lu]", proxy.host, proxy.port);
  client_config_.proxyHost = proxy.host;
  client_config_.proxyPort = proxy.port;
  client_config_.proxyUserName = proxy.username;
  client_config_.proxyPassword = proxy.password;
}

}  // namespace s3
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
