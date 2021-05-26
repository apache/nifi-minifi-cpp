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
#include "S3Wrapper.h"

#include <memory>
#include <regex>
#include <utility>
#include <vector>

#include "S3ClientRequestSender.h"
#include "utils/GeneralUtils.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/RegexUtils.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace s3 {

void HeadObjectResult::setFilePaths(const std::string& key) {
  absolute_path = key;
  std::tie(path, filename) = minifi::utils::file::FileUtils::split_path(key, true /*force_posix*/);
}

S3Wrapper::S3Wrapper() : request_sender_(minifi::utils::make_unique<S3ClientRequestSender>()) {
}

S3Wrapper::S3Wrapper(std::unique_ptr<S3RequestSender>&& request_sender) : request_sender_(std::move(request_sender)) {
}

void S3Wrapper::setCannedAcl(Aws::S3::Model::PutObjectRequest& request, const std::string& canned_acl) const {
  if (canned_acl.empty() || CANNED_ACL_MAP.find(canned_acl) == CANNED_ACL_MAP.end())
    return;

  logger_->log_debug("Setting AWS canned ACL [%s]", canned_acl);
  request.SetACL(CANNED_ACL_MAP.at(canned_acl));
}

Expiration S3Wrapper::getExpiration(const std::string& expiration) {
  minifi::utils::Regex expr("expiry-date=\"(.*)\", rule-id=\"(.*)\"");
  const auto match = expr.match(expiration);
  const auto& results = expr.getResult();
  if (!match || results.size() < 3)
    return Expiration{};
  return Expiration{results[1], results[2]};
}

std::string S3Wrapper::getEncryptionString(Aws::S3::Model::ServerSideEncryption encryption) {
  if (encryption == Aws::S3::Model::ServerSideEncryption::NOT_SET) {
    return "";
  }

  auto it = std::find_if(SERVER_SIDE_ENCRYPTION_MAP.begin(), SERVER_SIDE_ENCRYPTION_MAP.end(),
    [&](const std::pair<std::string, const Aws::S3::Model::ServerSideEncryption>& pair) {
      return pair.second == encryption;
    });
  if (it != SERVER_SIDE_ENCRYPTION_MAP.end()) {
    return it->first;
  }
  return "";
}

minifi::utils::optional<PutObjectResult> S3Wrapper::putObject(const PutObjectRequestParameters& put_object_params, std::shared_ptr<Aws::IOStream> data_stream) {
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

  auto aws_result = request_sender_->sendPutObjectRequest(request, put_object_params.credentials, put_object_params.client_config);
  if (!aws_result) {
    return minifi::utils::nullopt;
  }

  PutObjectResult result;
  // Etags are returned by AWS in quoted form that should be removed
  result.etag = minifi::utils::StringUtils::removeFramingCharacters(aws_result->GetETag(), '"');
  result.version = aws_result->GetVersionId();

  // GetExpiration returns a string pair with a date and a ruleid in 'expiry-date=\"<DATE>\", rule-id=\"<RULEID>\"' format
  // s3.expiration only needs the date member of this pair
  result.expiration = getExpiration(aws_result->GetExpiration()).expiration_time;
  result.ssealgorithm = getEncryptionString(aws_result->GetServerSideEncryption());
  return result;
}

bool S3Wrapper::deleteObject(const DeleteObjectRequestParameters& params) {
  Aws::S3::Model::DeleteObjectRequest request;
  request.SetBucket(params.bucket);
  request.SetKey(params.object_key);
  if (!params.version.empty()) {
    request.SetVersionId(params.version);
  }
  return request_sender_->sendDeleteObjectRequest(request, params.credentials, params.client_config);
}

int64_t S3Wrapper::writeFetchedBody(Aws::IOStream& source, const int64_t data_size, io::BaseStream& output) {
  std::vector<uint8_t> buffer(4096);
  size_t write_size = 0;
  if (data_size < 0) return 0;
  while (write_size < gsl::narrow<uint64_t>(data_size)) {
    const auto next_write_size = (std::min)(gsl::narrow<size_t>(data_size) - write_size, size_t{4096});
    if (!source.read(reinterpret_cast<char*>(buffer.data()), gsl::narrow<std::streamsize>(next_write_size))) {
      return -1;
    }
    const auto ret = output.write(buffer.data(), next_write_size);
    if (io::isError(ret)) {
      return -1;
    }
    write_size += next_write_size;
  }
  return gsl::narrow<int64_t>(write_size);
}

minifi::utils::optional<GetObjectResult> S3Wrapper::getObject(const GetObjectRequestParameters& get_object_params, io::BaseStream& out_body) {
  auto request = createFetchObjectRequest<Aws::S3::Model::GetObjectRequest>(get_object_params);
  auto aws_result = request_sender_->sendGetObjectRequest(request, get_object_params.credentials, get_object_params.client_config);
  if (!aws_result) {
    return minifi::utils::nullopt;
  }
  auto result = fillFetchObjectResult<Aws::S3::Model::GetObjectResult, GetObjectResult>(get_object_params, *aws_result);
  result.write_size = writeFetchedBody(aws_result->GetBody(), aws_result->GetContentLength(), out_body);
  return result;
}

void S3Wrapper::addListResults(const Aws::Vector<Aws::S3::Model::ObjectVersion>& content, const uint64_t min_object_age, std::vector<ListedObjectAttributes>& listed_objects) {
  for (const auto& version : content) {
    if (last_bucket_list_timestamp_ - min_object_age < gsl::narrow<uint64_t>(version.GetLastModified().Millis())) {
      logger_->log_debug("Object version '%s' of key '%s' skipped due to minimum object age filter", version.GetVersionId(), version.GetKey());
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

void S3Wrapper::addListResults(const Aws::Vector<Aws::S3::Model::Object>& content, const uint64_t min_object_age, std::vector<ListedObjectAttributes>& listed_objects) {
  for (const auto& object : content) {
    if (last_bucket_list_timestamp_ - min_object_age < gsl::narrow<uint64_t>(object.GetLastModified().Millis())) {
      logger_->log_debug("Object with key '%s' skipped due to minimum object age filter", object.GetKey());
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

minifi::utils::optional<std::vector<ListedObjectAttributes>> S3Wrapper::listVersions(const ListRequestParameters& params) {
  auto request = createListRequest<Aws::S3::Model::ListObjectVersionsRequest>(params);
  std::vector<ListedObjectAttributes> attribute_list;
  nonstd::optional_lite::optional<Aws::S3::Model::ListObjectVersionsResult> aws_result;
  do {
    aws_result = request_sender_->sendListVersionsRequest(request, params.credentials, params.client_config);
    if (!aws_result) {
      return minifi::utils::nullopt;
    }
    const auto& versions = aws_result->GetVersions();
    logger_->log_debug("AWS S3 List operation returned %zu versions. This result is%s truncated.", versions.size(), aws_result->GetIsTruncated() ? "" : " not");
    addListResults(versions, params.min_object_age, attribute_list);
    if (aws_result->GetIsTruncated()) {
      request.SetKeyMarker(aws_result->GetNextKeyMarker());
      request.SetVersionIdMarker(aws_result->GetNextVersionIdMarker());
    }
  } while (aws_result->GetIsTruncated());

  return attribute_list;
}

minifi::utils::optional<std::vector<ListedObjectAttributes>> S3Wrapper::listObjects(const ListRequestParameters& params) {
  auto request = createListRequest<Aws::S3::Model::ListObjectsV2Request>(params);
  std::vector<ListedObjectAttributes> attribute_list;
  nonstd::optional_lite::optional<Aws::S3::Model::ListObjectsV2Result> aws_result;
  do {
    aws_result = request_sender_->sendListObjectsRequest(request, params.credentials, params.client_config);
    if (!aws_result) {
      return minifi::utils::nullopt;
    }
    const auto& objects = aws_result->GetContents();
    logger_->log_debug("AWS S3 List operation returned %zu objects. This result is%s truncated.", objects.size(), aws_result->GetIsTruncated() ? "" : "not");
    addListResults(objects, params.min_object_age, attribute_list);
    if (aws_result->GetIsTruncated()) {
      request.SetContinuationToken(aws_result->GetNextContinuationToken());
    }
  } while (aws_result->GetIsTruncated());

  return attribute_list;
}

minifi::utils::optional<std::vector<ListedObjectAttributes>> S3Wrapper::listBucket(const ListRequestParameters& params) {
  last_bucket_list_timestamp_ = Aws::Utils::DateTime::CurrentTimeMillis();
  if (params.use_versions) {
    return listVersions(params);
  }
  return listObjects(params);
}

minifi::utils::optional<std::map<std::string, std::string>> S3Wrapper::getObjectTags(const GetObjectTagsParameters& params) {
  Aws::S3::Model::GetObjectTaggingRequest request;
  request.SetBucket(params.bucket);
  request.SetKey(params.object_key);
  if (!params.version.empty()) {
    request.SetVersionId(params.version);
  }
  auto aws_result = request_sender_->sendGetObjectTaggingRequest(request, params.credentials, params.client_config);
  if (!aws_result) {
    return minifi::utils::nullopt;
  }
  std::map<std::string, std::string> tags;
  for (const auto& tag : aws_result->GetTagSet()) {
    tags.emplace(tag.GetKey(), tag.GetValue());
  }
  return tags;
}

minifi::utils::optional<HeadObjectResult> S3Wrapper::headObject(const HeadObjectRequestParameters& head_object_params) {
  auto request = createFetchObjectRequest<Aws::S3::Model::HeadObjectRequest>(head_object_params);
  auto aws_result = request_sender_->sendHeadObjectRequest(request, head_object_params.credentials, head_object_params.client_config);
  if (!aws_result) {
    return minifi::utils::nullopt;
  }
  return fillFetchObjectResult<Aws::S3::Model::HeadObjectResult, HeadObjectResult>(head_object_params, aws_result.value());
}

template<typename ListRequest>
ListRequest S3Wrapper::createListRequest(const ListRequestParameters& params) {
  ListRequest request;
  request.SetBucket(params.bucket);
  request.SetDelimiter(params.delimiter);
  request.SetPrefix(params.prefix);
  return request;
}

template<typename FetchObjectRequest>
FetchObjectRequest S3Wrapper::createFetchObjectRequest(const GetObjectRequestParameters& get_object_params) {
  FetchObjectRequest request;
  request.SetBucket(get_object_params.bucket);
  request.SetKey(get_object_params.object_key);
  if (!get_object_params.version.empty()) {
    request.SetVersionId(get_object_params.version);
  }
  if (get_object_params.requester_pays) {
    request.SetRequestPayer(Aws::S3::Model::RequestPayer::requester);
  }
  return request;
}

template<typename AwsResult, typename FetchObjectResult>
FetchObjectResult S3Wrapper::fillFetchObjectResult(const GetObjectRequestParameters& get_object_params, const AwsResult& fetch_object_result) {
  FetchObjectResult result;
  result.setFilePaths(get_object_params.object_key);
  result.mime_type = fetch_object_result.GetContentType();
  result.etag = minifi::utils::StringUtils::removeFramingCharacters(fetch_object_result.GetETag(), '"');
  result.expiration = getExpiration(fetch_object_result.GetExpiration());
  result.ssealgorithm = getEncryptionString(fetch_object_result.GetServerSideEncryption());
  result.version = fetch_object_result.GetVersionId();
  for (const auto& metadata : fetch_object_result.GetMetadata()) {
    result.user_metadata_map.emplace(metadata.first, metadata.second);
  }
  return result;
}

}  // namespace s3
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
