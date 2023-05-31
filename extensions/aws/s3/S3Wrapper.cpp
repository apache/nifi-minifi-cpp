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
#include <utility>
#include <vector>

#include "S3ClientRequestSender.h"
#include "range/v3/algorithm/find.hpp"
#include "utils/ArrayUtils.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/gsl.h"
#include "utils/RegexUtils.h"
#include "aws/core/utils/HashingUtils.h"

namespace org::apache::nifi::minifi::aws::s3 {

void HeadObjectResult::setFilePaths(const std::string& key) {
  absolute_path = std::filesystem::path(key, std::filesystem::path::format::generic_format);
  path = absolute_path.parent_path();
  filename = absolute_path.filename();
}

S3Wrapper::S3Wrapper() : request_sender_(std::make_unique<S3ClientRequestSender>()) {
}

S3Wrapper::S3Wrapper(std::unique_ptr<S3RequestSender>&& request_sender) : request_sender_(std::move(request_sender)) {
}

Expiration S3Wrapper::getExpiration(const std::string& expiration) {
  minifi::utils::Regex expr("expiry-date=\"(.*)\", rule-id=\"(.*)\"");
  minifi::utils::SMatch matches;
  const bool matched = minifi::utils::regexMatch(expiration, matches, expr);
  if (!matched || matches.size() < 3)
    return Expiration{};
  return Expiration{matches[1], matches[2]};
}

std::string S3Wrapper::getEncryptionString(Aws::S3::Model::ServerSideEncryption encryption) {
  if (encryption == Aws::S3::Model::ServerSideEncryption::NOT_SET) {
    return "";
  }

  const auto it = ranges::find(SERVER_SIDE_ENCRYPTION_MAP, encryption, [](const auto& kv) { return kv.second; });
  if (it != SERVER_SIDE_ENCRYPTION_MAP.end()) {
    return std::string{it->first};
  }
  return "";
}

std::shared_ptr<Aws::StringStream> S3Wrapper::readFlowFileStream(const std::shared_ptr<io::InputStream>& stream, uint64_t read_limit, uint64_t& read_size_out) {
  std::vector<std::byte> buffer;
  buffer.resize(BUFFER_SIZE);
  auto data_stream = std::make_shared<Aws::StringStream>();
  uint64_t read_size = 0;
  while (read_size < read_limit) {
    const auto next_read_size = (std::min)(read_limit - read_size, BUFFER_SIZE);
    const auto read_ret = stream->read(std::span(buffer).subspan(0, next_read_size));
    if (io::isError(read_ret)) {
      throw StreamReadException("Reading flow file inputstream failed!");
    }
    if (read_ret > 0) {
      data_stream->write(reinterpret_cast<char*>(buffer.data()), gsl::narrow<std::streamsize>(next_read_size));
      read_size += read_ret;
    } else {
      break;
    }
  }
  read_size_out = read_size;
  return data_stream;
}

std::optional<PutObjectResult> S3Wrapper::putObject(const PutObjectRequestParameters& put_object_params, const std::shared_ptr<io::InputStream>& stream, uint64_t flow_size) {
  uint64_t read_size{};
  auto data_stream = readFlowFileStream(stream, flow_size, read_size);

  Aws::S3::Model::PutObjectRequest request;
  request.SetBucket(put_object_params.bucket);
  request.SetKey(put_object_params.object_key);
  request.SetBody(data_stream);
  request.SetStorageClass(minifi::utils::at(STORAGE_CLASS_MAP, put_object_params.storage_class));
  if (!put_object_params.server_side_encryption.empty() && put_object_params.server_side_encryption != "None") {
    request.SetServerSideEncryption(minifi::utils::at(SERVER_SIDE_ENCRYPTION_MAP, put_object_params.server_side_encryption));
  }
  if (!put_object_params.content_type.empty()) {
    request.SetContentType(put_object_params.content_type);
  }
  if (!put_object_params.user_metadata_map.empty()) {
    request.SetMetadata(put_object_params.user_metadata_map);
  }
  if (!put_object_params.fullcontrol_user_list.empty()) {
    request.SetGrantFullControl(put_object_params.fullcontrol_user_list);
  }
  if (!put_object_params.read_permission_user_list.empty()) {
    request.SetGrantRead(put_object_params.read_permission_user_list);
  }
  if (!put_object_params.read_acl_user_list.empty()) {
    request.SetGrantReadACP(put_object_params.read_acl_user_list);
  }
  if (!put_object_params.write_acl_user_list.empty()) {
    request.SetGrantWriteACP(put_object_params.write_acl_user_list);
  }
  setCannedAcl(request, put_object_params.canned_acl);

  auto aws_result = request_sender_->sendPutObjectRequest(request, put_object_params.credentials, put_object_params.client_config, put_object_params.use_virtual_addressing);
  if (!aws_result) {
    return std::nullopt;
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

std::optional<PutObjectResult> S3Wrapper::putObjectMultipart(const PutObjectRequestParameters& put_object_params, const std::shared_ptr<io::InputStream>& stream,
    uint64_t flow_size, uint64_t multipart_size) {
  Aws::S3::Model::CreateMultipartUploadRequest request;
  request.SetBucket(put_object_params.bucket);
  request.SetKey(put_object_params.object_key);
  request.SetStorageClass(STORAGE_CLASS_MAP.at(put_object_params.storage_class));
  request.SetServerSideEncryption(SERVER_SIDE_ENCRYPTION_MAP.at(put_object_params.server_side_encryption));
  request.SetContentType(put_object_params.content_type);
  request.SetMetadata(put_object_params.user_metadata_map);
  request.SetGrantFullControl(put_object_params.fullcontrol_user_list);
  request.SetGrantRead(put_object_params.read_permission_user_list);
  request.SetGrantReadACP(put_object_params.read_acl_user_list);
  request.SetGrantWriteACP(put_object_params.write_acl_user_list);
  setCannedAcl(request, put_object_params.canned_acl);
  auto create_multipart_result = request_sender_->sendCreateMultipartUploadRequest(request, put_object_params.credentials, put_object_params.client_config, put_object_params.use_virtual_addressing);
  if (!create_multipart_result) {
    return std::nullopt;
  }

  size_t part_count = flow_size % multipart_size == 0 ? flow_size / multipart_size : flow_size / multipart_size + 1;
  std::vector<std::string> part_etags;
  size_t total_read = 0;
  for (size_t i = 1; i <= part_count; ++i) {
    uint64_t read_size{};
    const auto remaining = flow_size - total_read;
    const auto next_read_size = remaining < multipart_size ? remaining : multipart_size;
    auto stream_ptr = readFlowFileStream(stream, next_read_size, read_size);
    total_read += read_size;

    Aws::S3::Model::UploadPartRequest upload_part_request;
    upload_part_request.SetBucket(put_object_params.bucket);
    upload_part_request.SetKey(put_object_params.object_key);
    upload_part_request.SetPartNumber(i);
    upload_part_request.SetUploadId(create_multipart_result->GetUploadId());
    upload_part_request.SetBody(stream_ptr);

    Aws::Utils::ByteBuffer part_md5(Aws::Utils::HashingUtils::CalculateMD5(*stream_ptr));
    upload_part_request.SetContentMD5(Aws::Utils::HashingUtils::Base64Encode(part_md5));

    auto upload_part_result = request_sender_->sendUploadPartRequest(upload_part_request, put_object_params.credentials, put_object_params.client_config, put_object_params.use_virtual_addressing);
    if (!upload_part_result) {
      return std::nullopt;
    }
    part_etags.push_back(upload_part_result->GetETag());
  }

  Aws::S3::Model::CompleteMultipartUploadRequest complete_multipart_upload_request;
  complete_multipart_upload_request.SetBucket(put_object_params.bucket);
  complete_multipart_upload_request.SetKey(create_multipart_result->GetKey());
  complete_multipart_upload_request.SetUploadId(create_multipart_result->GetUploadId());

  Aws::S3::Model::CompletedMultipartUpload completed_multipart_upload;
  for (size_t i = 0; i < part_count; ++i) {
    Aws::S3::Model::CompletedPart part;
    part.SetETag(part_etags[i]);
    part.SetPartNumber(i + 1);
    completed_multipart_upload.AddParts(part);
  }

  complete_multipart_upload_request.WithMultipartUpload(completed_multipart_upload);

  auto complete_multipart_upload_result =
    request_sender_->sendCompleteMultipartUploadRequest(complete_multipart_upload_request, put_object_params.credentials, put_object_params.client_config, put_object_params.use_virtual_addressing);

  if (!complete_multipart_upload_result) {
    return std::nullopt;
  }

  PutObjectResult result;
  // Etags are returned by AWS in quoted form that should be removed
  result.etag = minifi::utils::StringUtils::removeFramingCharacters(complete_multipart_upload_result->GetETag(), '"');
  result.version = complete_multipart_upload_result->GetVersionId();

  // GetExpiration returns a string pair with a date and a ruleid in 'expiry-date=\"<DATE>\", rule-id=\"<RULEID>\"' format
  // s3.expiration only needs the date member of this pair
  result.expiration = getExpiration(complete_multipart_upload_result->GetExpiration()).expiration_time;
  result.ssealgorithm = getEncryptionString(complete_multipart_upload_result->GetServerSideEncryption());
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

int64_t S3Wrapper::writeFetchedBody(Aws::IOStream& source, const int64_t data_size, io::OutputStream& output) {
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

std::optional<GetObjectResult> S3Wrapper::getObject(const GetObjectRequestParameters& get_object_params, io::OutputStream& out_body) {
  auto request = createFetchObjectRequest<Aws::S3::Model::GetObjectRequest>(get_object_params);
  auto aws_result = request_sender_->sendGetObjectRequest(request, get_object_params.credentials, get_object_params.client_config);
  if (!aws_result) {
    return std::nullopt;
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
    attributes.last_modified = version.GetLastModified().UnderlyingTimestamp();
    attributes.length = version.GetSize();
    attributes.store_class = minifi::utils::at(VERSION_STORAGE_CLASS_MAP, version.GetStorageClass());
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
    attributes.last_modified = object.GetLastModified().UnderlyingTimestamp();
    attributes.length = object.GetSize();
    attributes.store_class = minifi::utils::at(OBJECT_STORAGE_CLASS_MAP, object.GetStorageClass());
    listed_objects.push_back(attributes);
  }
}

std::optional<std::vector<ListedObjectAttributes>> S3Wrapper::listVersions(const ListRequestParameters& params) {
  auto request = createListRequest<Aws::S3::Model::ListObjectVersionsRequest>(params);
  std::vector<ListedObjectAttributes> attribute_list;
  std::optional<Aws::S3::Model::ListObjectVersionsResult> aws_result;
  do {
    aws_result = request_sender_->sendListVersionsRequest(request, params.credentials, params.client_config);
    if (!aws_result) {
      return std::nullopt;
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

std::optional<std::vector<ListedObjectAttributes>> S3Wrapper::listObjects(const ListRequestParameters& params) {
  auto request = createListRequest<Aws::S3::Model::ListObjectsV2Request>(params);
  std::vector<ListedObjectAttributes> attribute_list;
  std::optional<Aws::S3::Model::ListObjectsV2Result> aws_result;
  do {
    aws_result = request_sender_->sendListObjectsRequest(request, params.credentials, params.client_config);
    if (!aws_result) {
      return std::nullopt;
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

std::optional<std::vector<ListedObjectAttributes>> S3Wrapper::listBucket(const ListRequestParameters& params) {
  last_bucket_list_timestamp_ = gsl::narrow<uint64_t>(Aws::Utils::DateTime::CurrentTimeMillis());
  if (params.use_versions) {
    return listVersions(params);
  }
  return listObjects(params);
}

std::optional<std::map<std::string, std::string>> S3Wrapper::getObjectTags(const GetObjectTagsParameters& params) {
  Aws::S3::Model::GetObjectTaggingRequest request;
  request.SetBucket(params.bucket);
  request.SetKey(params.object_key);
  if (!params.version.empty()) {
    request.SetVersionId(params.version);
  }
  auto aws_result = request_sender_->sendGetObjectTaggingRequest(request, params.credentials, params.client_config);
  if (!aws_result) {
    return std::nullopt;
  }
  std::map<std::string, std::string> tags;
  for (const auto& tag : aws_result->GetTagSet()) {
    tags.emplace(tag.GetKey(), tag.GetValue());
  }
  return tags;
}

std::optional<HeadObjectResult> S3Wrapper::headObject(const HeadObjectRequestParameters& head_object_params) {
  auto request = createFetchObjectRequest<Aws::S3::Model::HeadObjectRequest>(head_object_params);
  auto aws_result = request_sender_->sendHeadObjectRequest(request, head_object_params.credentials, head_object_params.client_config);
  if (!aws_result) {
    return std::nullopt;
  }
  return fillFetchObjectResult<Aws::S3::Model::HeadObjectResult, HeadObjectResult>(head_object_params, aws_result.value());
}

template<typename ListRequest>
ListRequest S3Wrapper::createListRequest(const ListRequestParameters& params) {
  ListRequest request;
  request.SetBucket(params.bucket);
  if (!params.delimiter.empty()) {
    request.SetDelimiter(params.delimiter);
  }
  if (!params.prefix.empty()) {
    request.SetPrefix(params.prefix);
  }
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

void S3Wrapper::addListMultipartUploadResults(const Aws::Vector<Aws::S3::Model::MultipartUpload>& uploads, std::chrono::milliseconds max_upload_age, std::vector<MultipartUpload>& filtered_uploads) {
  const auto now = Aws::Utils::DateTime::Now();
  for (const auto& upload : uploads) {
    if (now - upload.GetInitiated() <= max_upload_age) {
      logger_->log_debug("Multipart upload with key '%s' and upload id '%s' did not age off yet", upload.GetKey(), upload.GetUploadId());
      continue;
    }

    logger_->log_info("Multipart upload with key '%s' and upload id '%s' older than age limit, marked for abortion", upload .GetKey(), upload.GetUploadId());
    MultipartUpload filtered_upload;
    filtered_upload.key = upload.GetKey();
    filtered_upload.upload_id = upload.GetUploadId();
    filtered_uploads.push_back(filtered_upload);
  }
}

std::optional<std::vector<MultipartUpload>> S3Wrapper::listAgedOffMultipartUploads(const ListMultipartUploadsRequestParameters& params) {
  std::vector<MultipartUpload> result;
  std::optional<Aws::S3::Model::ListMultipartUploadsResult> aws_result;
  Aws::S3::Model::ListMultipartUploadsRequest request;
  request.SetBucket(params.bucket);
  do {
    aws_result = request_sender_->sendListMultipartUploadsRequest(request, params.credentials, params.client_config, params.use_virtual_addressing);
    if (!aws_result) {
      return std::nullopt;
    }
    const auto& uploads = aws_result->GetUploads();
    logger_->log_debug("AWS S3 List operation returned %zu multipart uploads. This result is%s truncated.", uploads.size(), aws_result->GetIsTruncated() ? "" : " not");
    addListMultipartUploadResults(uploads, params.upload_max_age, result);
    if (aws_result->GetIsTruncated()) {
      request.SetKeyMarker(aws_result->GetNextKeyMarker());
    }
  } while (aws_result->GetIsTruncated());

  return result;
}

bool S3Wrapper::abortMultipartUpload(const AbortMultipartUploadRequestParameters& params) {
  Aws::S3::Model::AbortMultipartUploadRequest request;
  request.SetBucket(params.bucket);
  request.SetKey(params.key);
  request.SetUploadId(params.upload_id);
  return request_sender_->sendAbortMultipartUploadRequest(request, params.credentials, params.client_config, params.use_virtual_addressing);
}

}  // namespace org::apache::nifi::minifi::aws::s3
