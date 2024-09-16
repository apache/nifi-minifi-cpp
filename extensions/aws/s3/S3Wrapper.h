/**
 * @file S3Wrapper.h
 * S3Wrapper class declaration
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

#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "aws/s3/model/StorageClass.h"
#include "aws/s3/model/ServerSideEncryption.h"
#include "aws/s3/model/ObjectCannedACL.h"

#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "utils/AWSInitializer.h"
#include "utils/OptionalUtils.h"
#include "utils/StringUtils.h"
#include "utils/ListingStateManager.h"
#include "utils/gsl.h"
#include "S3RequestSender.h"
#include "Exception.h"
#include "MultipartUploadStateStorage.h"
#include "range/v3/algorithm/find.hpp"
#include "utils/Literals.h"
#include "io/InputStream.h"
#include "io/OutputStream.h"

namespace org::apache::nifi::minifi::aws::s3 {

inline constexpr std::array<std::pair<std::string_view, Aws::S3::Model::StorageClass>, 7> STORAGE_CLASS_MAP {{
  {"Standard", Aws::S3::Model::StorageClass::STANDARD},
  {"ReducedRedundancy", Aws::S3::Model::StorageClass::REDUCED_REDUNDANCY},
  {"StandardIA", Aws::S3::Model::StorageClass::STANDARD_IA},
  {"OnezoneIA", Aws::S3::Model::StorageClass::ONEZONE_IA},
  {"IntelligentTiering", Aws::S3::Model::StorageClass::INTELLIGENT_TIERING},
  {"Glacier", Aws::S3::Model::StorageClass::GLACIER},
  {"DeepArchive", Aws::S3::Model::StorageClass::DEEP_ARCHIVE}
}};

inline constexpr std::array<std::pair<Aws::S3::Model::ObjectStorageClass, std::string_view>, 7> OBJECT_STORAGE_CLASS_MAP {{
  {Aws::S3::Model::ObjectStorageClass::STANDARD, "Standard"},
  {Aws::S3::Model::ObjectStorageClass::REDUCED_REDUNDANCY, "ReducedRedundancy"},
  {Aws::S3::Model::ObjectStorageClass::STANDARD_IA, "StandardIA"},
  {Aws::S3::Model::ObjectStorageClass::ONEZONE_IA, "OnezoneIA"},
  {Aws::S3::Model::ObjectStorageClass::INTELLIGENT_TIERING, "IntelligentTiering"},
  {Aws::S3::Model::ObjectStorageClass::GLACIER, "Glacier"},
  {Aws::S3::Model::ObjectStorageClass::DEEP_ARCHIVE, "DeepArchive"}
}};

inline constexpr std::array<std::pair<Aws::S3::Model::ObjectVersionStorageClass, std::string_view>, 1> VERSION_STORAGE_CLASS_MAP {{
  {Aws::S3::Model::ObjectVersionStorageClass::STANDARD, "Standard"}
}};

inline constexpr std::array<std::pair<std::string_view, Aws::S3::Model::ServerSideEncryption>, 3> SERVER_SIDE_ENCRYPTION_MAP {{
  {"None", Aws::S3::Model::ServerSideEncryption::NOT_SET},
  {"AES256", Aws::S3::Model::ServerSideEncryption::AES256},
  {"aws_kms", Aws::S3::Model::ServerSideEncryption::aws_kms},
}};

inline constexpr std::array<std::pair<std::string_view, Aws::S3::Model::ObjectCannedACL>, 7> CANNED_ACL_MAP {{
  {"BucketOwnerFullControl", Aws::S3::Model::ObjectCannedACL::bucket_owner_full_control},
  {"BucketOwnerRead", Aws::S3::Model::ObjectCannedACL::bucket_owner_read},
  {"AuthenticatedRead", Aws::S3::Model::ObjectCannedACL::authenticated_read},
  {"PublicReadWrite", Aws::S3::Model::ObjectCannedACL::public_read_write},
  {"PublicRead", Aws::S3::Model::ObjectCannedACL::public_read},
  {"Private", Aws::S3::Model::ObjectCannedACL::private_},
  {"AwsExecRead", Aws::S3::Model::ObjectCannedACL::aws_exec_read},
}};

struct Expiration {
  std::string expiration_time;
  std::string expiration_time_rule_id;
};

struct PutObjectResult {
  std::string version;
  std::string etag;
  std::string expiration;
  std::string ssealgorithm;
};

struct RequestParameters {
  RequestParameters(Aws::Auth::AWSCredentials creds, Aws::Client::ClientConfiguration config)
    : credentials(std::move(creds)),
      client_config(std::move(config)) {}
  Aws::Auth::AWSCredentials credentials;
  Aws::Client::ClientConfiguration client_config;

  void setClientConfig(const aws::s3::ProxyOptions& proxy, const std::string& endpoint_override_url) {
    client_config.proxyHost = proxy.host;
    client_config.proxyPort = proxy.port;
    client_config.proxyUserName = proxy.username;
    client_config.proxyPassword = proxy.password;
    client_config.endpointOverride = endpoint_override_url;
  }
};

struct PutObjectRequestParameters : public RequestParameters {
  PutObjectRequestParameters(const Aws::Auth::AWSCredentials& creds, const Aws::Client::ClientConfiguration& config)
    : RequestParameters(creds, config) {}
  std::string bucket;
  std::string object_key;
  std::string storage_class;
  std::string server_side_encryption;
  std::string content_type;
  std::map<std::string, std::string> user_metadata_map;
  std::string fullcontrol_user_list;
  std::string read_permission_user_list;
  std::string read_acl_user_list;
  std::string write_acl_user_list;
  std::string canned_acl;
  bool use_virtual_addressing = true;
};

struct DeleteObjectRequestParameters : public RequestParameters {
  DeleteObjectRequestParameters(const Aws::Auth::AWSCredentials& creds, const Aws::Client::ClientConfiguration& config)
    : RequestParameters(creds, config) {}
  std::string bucket;
  std::string object_key;
  std::string version;
};

struct GetObjectRequestParameters : public RequestParameters {
  GetObjectRequestParameters(const Aws::Auth::AWSCredentials& creds, const Aws::Client::ClientConfiguration& config)
    : RequestParameters(creds, config) {}
  std::string bucket;
  std::string object_key;
  std::string version;
  bool requester_pays = false;
};

struct HeadObjectResult {
  std::filesystem::path path;
  std::filesystem::path absolute_path;
  std::filesystem::path filename;
  std::string mime_type;
  std::string etag;
  Expiration expiration;
  std::string ssealgorithm;
  std::string version;
  std::map<std::string, std::string> user_metadata_map;

  void setFilePaths(const std::string& key);
};

struct GetObjectResult : public HeadObjectResult {
  int64_t write_size = 0;
};

struct ListRequestParameters : public RequestParameters {
  ListRequestParameters(const Aws::Auth::AWSCredentials& creds, const Aws::Client::ClientConfiguration& config)
    : RequestParameters(creds, config) {}
  std::string bucket;
  std::string delimiter;
  std::string prefix;
  bool use_versions = false;
  uint64_t min_object_age = 0;
};

struct ListedObjectAttributes : public minifi::utils::ListedObject {
  [[nodiscard]] std::chrono::time_point<std::chrono::system_clock> getLastModified() const override {
    return last_modified;
  }

  [[nodiscard]] std::string getKey() const override {
    return filename;
  }

  std::string filename;
  std::string etag;
  bool is_latest = false;
  std::chrono::time_point<std::chrono::system_clock> last_modified;
  int64_t length = 0;
  std::string store_class;
  std::string version;
};

using HeadObjectRequestParameters = GetObjectRequestParameters;
using GetObjectTagsParameters = DeleteObjectRequestParameters;

struct ListMultipartUploadsRequestParameters : public RequestParameters {
  ListMultipartUploadsRequestParameters(const Aws::Auth::AWSCredentials& creds, const Aws::Client::ClientConfiguration& config)
    : RequestParameters(creds, config) {}
  std::string bucket;
  std::optional<std::chrono::milliseconds> age_off_limit;  // if set, only list the aged off uploads
  bool use_virtual_addressing = true;
};

struct MultipartUpload {
  std::string key;
  std::string upload_id;
};

struct AbortMultipartUploadRequestParameters : public RequestParameters {
  AbortMultipartUploadRequestParameters(const Aws::Auth::AWSCredentials& creds, const Aws::Client::ClientConfiguration& config)
    : RequestParameters(creds, config) {}
  std::string bucket;
  std::string key;
  std::string upload_id;
  bool use_virtual_addressing = true;
};

class StreamReadException : public Exception {
 public:
  explicit StreamReadException(const std::string& error) : Exception(GENERAL_EXCEPTION, error) {}
};

class S3Wrapper {
 public:
  static constexpr uint64_t BUFFER_SIZE = 4_KiB;

  S3Wrapper();
  explicit S3Wrapper(std::unique_ptr<S3RequestSender>&& request_sender);

  std::optional<PutObjectResult> putObject(const PutObjectRequestParameters& put_object_params, const std::shared_ptr<io::InputStream>& stream, uint64_t flow_size);
  std::optional<PutObjectResult> putObjectMultipart(const PutObjectRequestParameters& put_object_params, const std::shared_ptr<io::InputStream>& stream, uint64_t flow_size, uint64_t multipart_size);
  bool deleteObject(const DeleteObjectRequestParameters& params);
  std::optional<GetObjectResult> getObject(const GetObjectRequestParameters& get_object_params, io::OutputStream& out_body);
  std::optional<std::vector<ListedObjectAttributes>> listBucket(const ListRequestParameters& params);
  std::optional<std::map<std::string, std::string>> getObjectTags(const GetObjectTagsParameters& params);
  std::optional<HeadObjectResult> headObject(const HeadObjectRequestParameters& head_object_params);
  std::optional<std::vector<MultipartUpload>> listMultipartUploads(const ListMultipartUploadsRequestParameters& params);
  bool abortMultipartUpload(const AbortMultipartUploadRequestParameters& params);
  void ageOffLocalS3MultipartUploadStates(std::chrono::milliseconds multipart_upload_max_age_threshold);
  void initializeMultipartUploadStateStorage(gsl::not_null<minifi::core::StateManager*> state_manager);

  virtual ~S3Wrapper() = default;

 private:
  struct UploadPartsResult {
    std::string upload_id;
    std::vector<std::string> part_etags;
  };

  static Expiration getExpiration(const std::string& expiration);

  template<typename RequestType>
  void setCannedAcl(RequestType& request, const std::string& canned_acl) const {
    if (canned_acl.empty()) return;

    const auto it = ranges::find(CANNED_ACL_MAP, canned_acl, [](const auto& kv) { return kv.first; });
    if (it == CANNED_ACL_MAP.end()) return;

    logger_->log_debug("Setting AWS canned ACL [{}]", canned_acl);
    request.SetACL(it->second);
  }

  template<typename RequestType>
  RequestType createPutObjectRequest(const PutObjectRequestParameters& put_object_params) {
    auto request = RequestType{}
      .WithBucket(put_object_params.bucket)
      .WithKey(put_object_params.object_key)
      .WithStorageClass(minifi::utils::at(STORAGE_CLASS_MAP, put_object_params.storage_class));
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
    setCannedAcl<RequestType>(request, put_object_params.canned_acl);
    return request;
  }

  template<typename ResultType>
  PutObjectResult createPutObjectResult(const ResultType& upload_result) {
    return {
      .version = upload_result.GetVersionId(),
      // Etags are returned by AWS in quoted form that should be removed
      .etag = minifi::utils::string::removeFramingCharacters(upload_result.GetETag(), '"'),
      // GetExpiration returns a string pair with a date and a ruleid in 'expiry-date=\"<DATE>\", rule-id=\"<RULEID>\"' format
      // s3.expiration only needs the date member of this pair
      .expiration = getExpiration(upload_result.GetExpiration()).expiration_time,
      .ssealgorithm = getEncryptionString(upload_result.GetServerSideEncryption())
    };
  }

  static int64_t writeFetchedBody(Aws::IOStream& source, int64_t data_size, io::OutputStream& output);
  static std::string getEncryptionString(Aws::S3::Model::ServerSideEncryption encryption);
  static std::shared_ptr<Aws::StringStream> readFlowFileStream(const std::shared_ptr<io::InputStream>& stream, uint64_t read_limit, uint64_t& read_size_out);

  std::optional<std::vector<ListedObjectAttributes>> listVersions(const ListRequestParameters& params);
  std::optional<std::vector<ListedObjectAttributes>> listObjects(const ListRequestParameters& params);
  void addListResults(const Aws::Vector<Aws::S3::Model::ObjectVersion>& content, uint64_t min_object_age, std::vector<ListedObjectAttributes>& listed_objects);
  void addListResults(const Aws::Vector<Aws::S3::Model::Object>& content, uint64_t min_object_age, std::vector<ListedObjectAttributes>& listed_objects);
  void addListMultipartUploadResults(const Aws::Vector<Aws::S3::Model::MultipartUpload>& uploads, std::optional<std::chrono::milliseconds> age_off_limit,
    std::vector<MultipartUpload>& filtered_uploads);
  std::optional<UploadPartsResult> uploadParts(const PutObjectRequestParameters& put_object_params, const std::shared_ptr<io::InputStream>& stream, MultipartUploadState upload_state);
  std::optional<Aws::S3::Model::CompleteMultipartUploadResult> completeMultipartUpload(const PutObjectRequestParameters& put_object_params, const UploadPartsResult& upload_parts_result);
  bool multipartUploadExistsInS3(const PutObjectRequestParameters& put_object_params);
  std::optional<MultipartUploadState> getMultipartUploadState(const PutObjectRequestParameters& put_object_params);

  template<typename ListRequest>
  ListRequest createListRequest(const ListRequestParameters& params);

  template<typename FetchObjectRequest>
  FetchObjectRequest createFetchObjectRequest(const GetObjectRequestParameters& get_object_params);

  template<typename AwsResult, typename FetchObjectResult>
  FetchObjectResult fillFetchObjectResult(const GetObjectRequestParameters& get_object_params, const AwsResult& fetch_object_result);

  const utils::AWSInitializer& AWS_INITIALIZER = utils::AWSInitializer::get();
  std::shared_ptr<minifi::core::logging::Logger> logger_{minifi::core::logging::LoggerFactory<S3Wrapper>::getLogger()};
  std::unique_ptr<S3RequestSender> request_sender_;
  uint64_t last_bucket_list_timestamp_ = 0;
  std::unique_ptr<MultipartUploadStateStorage> multipart_upload_storage_;
};

}  // namespace org::apache::nifi::minifi::aws::s3
