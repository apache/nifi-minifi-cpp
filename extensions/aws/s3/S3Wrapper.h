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

#include <string>
#include <map>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "aws/s3/model/StorageClass.h"
#include "aws/s3/model/ServerSideEncryption.h"
#include "aws/s3/model/ObjectCannedACL.h"

#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/AWSInitializer.h"
#include "utils/OptionalUtils.h"
#include "utils/StringUtils.h"
#include "io/BaseStream.h"
#include "S3RequestSender.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace s3 {

static const std::unordered_map<std::string, Aws::S3::Model::StorageClass> STORAGE_CLASS_MAP {
  {"Standard", Aws::S3::Model::StorageClass::STANDARD},
  {"ReducedRedundancy", Aws::S3::Model::StorageClass::REDUCED_REDUNDANCY},
  {"StandardIA", Aws::S3::Model::StorageClass::STANDARD_IA},
  {"OnezoneIA", Aws::S3::Model::StorageClass::ONEZONE_IA},
  {"IntelligentTiering", Aws::S3::Model::StorageClass::INTELLIGENT_TIERING},
  {"Glacier", Aws::S3::Model::StorageClass::GLACIER},
  {"DeepArchive", Aws::S3::Model::StorageClass::DEEP_ARCHIVE}
};

static const std::map<Aws::S3::Model::ObjectStorageClass, std::string> OBJECT_STORAGE_CLASS_MAP {
  {Aws::S3::Model::ObjectStorageClass::STANDARD, "Standard"},
  {Aws::S3::Model::ObjectStorageClass::REDUCED_REDUNDANCY, "ReducedRedundancy"},
  {Aws::S3::Model::ObjectStorageClass::STANDARD_IA, "StandardIA"},
  {Aws::S3::Model::ObjectStorageClass::ONEZONE_IA, "OnezoneIA"},
  {Aws::S3::Model::ObjectStorageClass::INTELLIGENT_TIERING, "IntelligentTiering"},
  {Aws::S3::Model::ObjectStorageClass::GLACIER, "Glacier"},
  {Aws::S3::Model::ObjectStorageClass::DEEP_ARCHIVE, "DeepArchive"}
};

static const std::map<Aws::S3::Model::ObjectVersionStorageClass, std::string> VERSION_STORAGE_CLASS_MAP {
  {Aws::S3::Model::ObjectVersionStorageClass::STANDARD, "Standard"}
};

static const std::unordered_map<std::string, Aws::S3::Model::ServerSideEncryption> SERVER_SIDE_ENCRYPTION_MAP {
  {"None", Aws::S3::Model::ServerSideEncryption::NOT_SET},
  {"AES256", Aws::S3::Model::ServerSideEncryption::AES256},
  {"aws_kms", Aws::S3::Model::ServerSideEncryption::aws_kms},
};

static const std::unordered_map<std::string, Aws::S3::Model::ObjectCannedACL> CANNED_ACL_MAP {
  {"BucketOwnerFullControl", Aws::S3::Model::ObjectCannedACL::bucket_owner_full_control},
  {"BucketOwnerRead", Aws::S3::Model::ObjectCannedACL::bucket_owner_read},
  {"AuthenticatedRead", Aws::S3::Model::ObjectCannedACL::authenticated_read},
  {"PublicReadWrite", Aws::S3::Model::ObjectCannedACL::public_read_write},
  {"PublicRead", Aws::S3::Model::ObjectCannedACL::public_read},
  {"Private", Aws::S3::Model::ObjectCannedACL::private_},
  {"AwsExecRead", Aws::S3::Model::ObjectCannedACL::aws_exec_read},
};

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

struct PutObjectRequestParameters {
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
};

struct GetObjectRequestParameters {
  std::string bucket;
  std::string object_key;
  std::string version;
  bool requester_pays = false;
};

struct HeadObjectResult {
  std::string path;
  std::string absolute_path;
  std::string filename;
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

struct ListRequestParameters {
  std::string bucket;
  std::string delimiter;
  std::string prefix;
  bool use_versions = false;
  uint64_t min_object_age = 0;
};

struct ListedObjectAttributes {
  std::string filename;
  std::string etag;
  bool is_latest = false;
  int64_t last_modified = 0;
  int64_t length = 0;
  std::string store_class;
  std::string version;
};

using HeadObjectRequestParameters = GetObjectRequestParameters;

class S3Wrapper {
 public:
  S3Wrapper();
  explicit S3Wrapper(std::unique_ptr<S3RequestSender>&& request_sender);

  void setCredentials(const Aws::Auth::AWSCredentials& cred);
  void setRegion(const Aws::String& region);
  void setTimeout(uint64_t timeout);
  void setEndpointOverrideUrl(const Aws::String& url);
  void setProxy(const ProxyOptions& proxy);

  minifi::utils::optional<PutObjectResult> putObject(const PutObjectRequestParameters& options, std::shared_ptr<Aws::IOStream> data_stream);
  bool deleteObject(const std::string& bucket, const std::string& object_key, const std::string& version = "");
  minifi::utils::optional<GetObjectResult> getObject(const GetObjectRequestParameters& get_object_params, io::BaseStream& fetched_body);
  minifi::utils::optional<std::vector<ListedObjectAttributes>> listBucket(const ListRequestParameters& params);
  minifi::utils::optional<std::map<std::string, std::string>> getObjectTags(const std::string& bucket, const std::string& object_key, const std::string& version = "");
  minifi::utils::optional<HeadObjectResult> headObject(const HeadObjectRequestParameters& head_object_params);

  virtual ~S3Wrapper() = default;

 private:
  static Expiration getExpiration(const std::string& expiration);

  void setCannedAcl(Aws::S3::Model::PutObjectRequest& request, const std::string& canned_acl) const;
  static int64_t writeFetchedBody(Aws::IOStream& source, const int64_t data_size, io::BaseStream& output);
  static std::string getEncryptionString(Aws::S3::Model::ServerSideEncryption encryption);

  minifi::utils::optional<std::vector<ListedObjectAttributes>> listVersions(const ListRequestParameters& params);
  minifi::utils::optional<std::vector<ListedObjectAttributes>> listObjects(const ListRequestParameters& params);
  void addListResults(const Aws::Vector<Aws::S3::Model::ObjectVersion>& content, uint64_t min_object_age, std::vector<ListedObjectAttributes>& listed_objects);
  void addListResults(const Aws::Vector<Aws::S3::Model::Object>& content, uint64_t min_object_age, std::vector<ListedObjectAttributes>& listed_objects);

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
};

}  // namespace s3
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
