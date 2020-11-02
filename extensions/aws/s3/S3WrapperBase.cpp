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
#include <utility>
#include <vector>

#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/RegexUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace s3 {

void GetObjectResult::setFilePaths(const std::string& key) {
  absolute_path = key;
  std::tie(path, filename) = minifi::utils::file::FileUtils::split_path(key, true /*force_posix*/);
}

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

Expiration S3WrapperBase::getExpiration(const std::string& expiration) {
  minifi::utils::Regex expr("expiry-date=\"(.*)\", rule-id=\"(.*)\"");
  const auto match = expr.match(expiration);
  const auto& results = expr.getResult();
  if (!match || results.size() < 3)
    return Expiration{};
  return Expiration{results[1], results[2]};
}

std::string S3WrapperBase::getEncryptionString(Aws::S3::Model::ServerSideEncryption encryption) {
  if (encryption == Aws::S3::Model::ServerSideEncryption::NOT_SET) {
    return "";
  }

  auto it = std::find_if(SERVER_SIDE_ENCRYPTION_MAP.begin(), SERVER_SIDE_ENCRYPTION_MAP.end(),
    [&](const std::pair<std::string, const Aws::S3::Model::ServerSideEncryption&> pair) {
      return pair.second == encryption;
    });
  if (it != SERVER_SIDE_ENCRYPTION_MAP.end()) {
    return it->first;
  }
  return "";
}

minifi::utils::optional<PutObjectResult> S3WrapperBase::putObject(const PutObjectRequestParameters& put_object_params, std::shared_ptr<Aws::IOStream> data_stream) {
  Aws::S3::Model::PutObjectRequest request;
  request.SetBucket(put_object_params.bucket);
  request.SetKey(put_object_params.object_key);
  request.SetStorageClass(STORAGE_CLASS_MAP.at(put_object_params.storage_class));
  request.SetServerSideEncryption(SERVER_SIDE_ENCRYPTION_MAP.at(put_object_params.server_side_encryption));
  request.SetContentType(put_object_params.content_type);
  request.SetMetadata(put_object_params.user_metadata_map);
  request.SetBody(data_stream);
  request.SetGrantFullControl(put_object_params.fullcontrol_user_list);
  request.SetGrantRead(put_object_params.read_permission_user_list);
  request.SetGrantReadACP(put_object_params.read_acl_user_list);
  request.SetGrantWriteACP(put_object_params.write_acl_user_list);
  setCannedAcl(request, put_object_params.canned_acl);

  auto aws_result = sendPutObjectRequest(request);
  if (!aws_result) {
    return minifi::utils::nullopt;
  }

  PutObjectResult result;
  // Etags are returned by AWS in quoted form that should be removed
  result.etag = minifi::utils::StringUtils::removeFramingCharacters(aws_result->GetETag(), '"');
  result.version = aws_result->GetVersionId();

  // GetExpiration returns a string pair with a date and a ruleid in 'expiry-date=\"<DATE>\", rule-id=\"<RULEID>\"' format
  // s3.expiration only needs the date member of this pair
  result.expiration_time = getExpiration(aws_result->GetExpiration()).expiration_time;
  result.ssealgorithm = getEncryptionString(aws_result->GetServerSideEncryption());
  return result;
}

bool S3WrapperBase::deleteObject(const std::string& bucket, const std::string& object_key, const std::string& version) {
  Aws::S3::Model::DeleteObjectRequest request;
  request.SetBucket(bucket);
  request.SetKey(object_key);
  if (!version.empty()) {
    request.SetVersionId(version);
  }
  return sendDeleteObjectRequest(request);
}

int64_t S3WrapperBase::writeFetchedBody(Aws::IOStream& source, const int64_t data_size, const std::shared_ptr<io::BaseStream>& output) {
  static const uint64_t BUFFER_SIZE = 4096;
  std::vector<uint8_t> buffer;
  buffer.reserve(BUFFER_SIZE);

  int64_t write_size = 0;
  while (write_size < data_size) {
    auto next_write_size = data_size - write_size < BUFFER_SIZE ? data_size - write_size : BUFFER_SIZE;
    if (!source.read(reinterpret_cast<char*>(buffer.data()), next_write_size)) {
      return -1;
    }
    auto ret = output->write(buffer.data(), next_write_size);
    if (ret < 0) {
      return ret;
    }
    write_size += next_write_size;
  }
  return write_size;
}

minifi::utils::optional<GetObjectResult> S3WrapperBase::getObject(const GetObjectRequestParameters& get_object_params, const std::shared_ptr<io::BaseStream>& out_body) {
  Aws::S3::Model::GetObjectRequest request;
  request.SetBucket(get_object_params.bucket);
  request.SetKey(get_object_params.object_key);
  if (!get_object_params.version.empty()) {
    request.SetVersionId(get_object_params.version);
  }
  if (get_object_params.requester_pays) {
    request.SetRequestPayer(Aws::S3::Model::RequestPayer::requester);
  }
  auto aws_result = sendGetObjectRequest(request);
  if (!aws_result) {
    return minifi::utils::nullopt;
  }

  GetObjectResult result;
  result.setFilePaths(get_object_params.object_key);
  result.mime_type = aws_result->GetContentType();
  result.etag = minifi::utils::StringUtils::removeFramingCharacters(aws_result->GetETag(), '"');
  result.expiration = getExpiration(aws_result->GetExpiration());
  result.ssealgorithm = getEncryptionString(aws_result->GetServerSideEncryption());
  result.version = aws_result->GetVersionId();
  for (const auto& metadata : aws_result->GetMetadata()) {
    result.user_metadata_map.emplace(metadata.first, metadata.second);
  }
  if (out_body != nullptr) {
    result.write_size = writeFetchedBody(aws_result->GetBody(), aws_result->GetContentLength(), out_body);
  }

  return result;
}

void S3WrapperBase::addListResults(const Aws::Vector<Aws::S3::Model::ObjectVersion>& content, const uint64_t min_object_age, std::vector<ListedObjectAttributes>& listed_objects) {
  for (const auto& version : content) {
    if (last_bucket_list_timestamp - min_object_age < version.GetLastModified().Millis()) {
      continue;
    }

    ListedObjectAttributes attributes;
    attributes.etag = minifi::utils::StringUtils::removeFramingCharacters(version.GetETag(), '"');
    attributes.filename = version.GetKey();
    attributes.is_latest = version.GetIsLatest();
    attributes.last_modified = version.GetLastModified().Millis();
    attributes.length = version.GetSize();
    attributes.store_class = VERSION_STORAGE_CLASS_MAP.at(version.GetStorageClass());
    attributes.version = version.GetVersionId();
    listed_objects.push_back(attributes);
  }
}

void S3WrapperBase::addListResults(const Aws::Vector<Aws::S3::Model::Object>& content, const uint64_t min_object_age, std::vector<ListedObjectAttributes>& listed_objects) {
  for (const auto& object : content) {
    if (last_bucket_list_timestamp - min_object_age < object.GetLastModified().Millis()) {
      continue;
    }

    ListedObjectAttributes attributes;
    attributes.etag = minifi::utils::StringUtils::removeFramingCharacters(object.GetETag(), '"');
    attributes.filename = object.GetKey();
    attributes.is_latest = true;
    attributes.last_modified = object.GetLastModified().Millis();
    attributes.length = object.GetSize();
    attributes.store_class = OBJECT_STORAGE_CLASS_MAP.at(object.GetStorageClass());
    listed_objects.push_back(attributes);
  }
}

minifi::utils::optional<std::vector<ListedObjectAttributes>> S3WrapperBase::listVersions(const ListRequestParameters& params) {
  auto request = createListRequest<Aws::S3::Model::ListObjectVersionsRequest>(params);
  std::vector<ListedObjectAttributes> attribute_list;
  nonstd::optional_lite::optional<Aws::S3::Model::ListObjectVersionsResult> aws_result;
  do {
    aws_result = sendListVersionsRequest(request);
    if (!aws_result) {
      return minifi::utils::nullopt;
    }
    addListResults(aws_result->GetVersions(), params.min_object_age, attribute_list);
    request.SetKeyMarker(aws_result->GetNextKeyMarker());
    request.SetVersionIdMarker(aws_result->GetNextVersionIdMarker());
  } while(aws_result->GetIsTruncated());

  return attribute_list;
}

minifi::utils::optional<std::vector<ListedObjectAttributes>> S3WrapperBase::listObjects(const ListRequestParameters& params) {
  auto request = createListRequest<Aws::S3::Model::ListObjectsV2Request>(params);
  std::vector<ListedObjectAttributes> attribute_list;
  nonstd::optional_lite::optional<Aws::S3::Model::ListObjectsV2Result> aws_result;
  do {
    aws_result = sendListObjectsRequest(request);
    if (!aws_result) {
      return minifi::utils::nullopt;
    }
    addListResults(aws_result->GetContents(), params.min_object_age, attribute_list);
    request.SetContinuationToken(aws_result->GetContinuationToken());
  } while(aws_result->GetIsTruncated());

  return attribute_list;
}

minifi::utils::optional<std::vector<ListedObjectAttributes>> S3WrapperBase::listBucket(const ListRequestParameters& params) {
  last_bucket_list_timestamp = Aws::Utils::DateTime::CurrentTimeMillis();
  if (params.use_versions) {
    return listVersions(params);
  }
  return listObjects(params);
}

minifi::utils::optional<std::map<std::string, std::string>> S3WrapperBase::getObjectTags(const std::string& bucket, const std::string& object_key, const std::string& version) {
  Aws::S3::Model::GetObjectTaggingRequest request;
  request.SetBucket(bucket);
  request.SetKey(object_key);
  if (!version.empty()) {
    request.SetVersionId(version);
  }
  auto aws_result = sendGetObjectTaggingRequest(request);
  if (!aws_result) {
    return minifi::utils::nullopt;
  }
  std::map<std::string, std::string> tags;
  for (const auto& tag : aws_result->GetTagSet()) {
    tags.emplace(tag.GetKey(), tag.GetValue());
  }
  return tags;
}

}  // namespace s3
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
