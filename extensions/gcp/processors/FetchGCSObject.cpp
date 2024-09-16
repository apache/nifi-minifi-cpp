/**
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

#include "FetchGCSObject.h"

#include <utility>

#include "core/Resource.h"
#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "../GCPAttributes.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {
namespace {
class FetchFromGCSCallback {
 public:
  FetchFromGCSCallback(gcs::Client& client, std::string bucket, std::string key)
      : bucket_(std::move(bucket)),
        key_(std::move(key)),
        client_(client) {
  }

  int64_t operator()(const std::shared_ptr<io::OutputStream>& stream) {
    auto reader = client_.ReadObject(bucket_, key_, encryption_key_, generation_, gcs::IfGenerationNotMatch(0));
    auto set_members = gsl::finally([&]{
      status_ = reader.status();
      result_generation_ = reader.generation();
      meta_generation_ = reader.metageneration();
      storage_class_ = reader.storage_class();
    });
    if (!reader)
      return 0;
    std::string contents{std::istreambuf_iterator<char>{reader}, {}};
    const auto write_ret = gsl::narrow<int64_t>(stream->write(gsl::make_span(contents).as_span<std::byte>()));
    reader.Close();
    return write_ret;
  }

  [[nodiscard]] auto getStatus() const noexcept { return status_; }
  [[nodiscard]] auto getGeneration() const noexcept { return result_generation_; }
  [[nodiscard]] auto getMetaGeneration() const noexcept { return meta_generation_; }
  [[nodiscard]] auto getStorageClass() const noexcept { return storage_class_; }


  void setEncryptionKey(const gcs::EncryptionKey& encryption_key) {
    encryption_key_ = encryption_key;
  }

  void setGeneration(const gcs::Generation generation) {
    generation_ = generation;
  }

 private:
  std::string bucket_;
  std::string key_;
  gcs::Client& client_;

  gcs::EncryptionKey encryption_key_;
  gcs::Generation generation_;

  google::cloud::Status status_;
  std::optional<std::int64_t> result_generation_;
  std::optional<std::int64_t> meta_generation_;
  std::optional<std::string> storage_class_;
};
}  // namespace


void FetchGCSObject::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchGCSObject::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  GCSProcessor::onSchedule(context, session_factory);
  if (auto encryption_key = context.getProperty(EncryptionKey)) {
    try {
      encryption_key_ = gcs::EncryptionKey::FromBase64Key(*encryption_key);
    } catch (const google::cloud::RuntimeStatusError&) {
      throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Could not decode the base64-encoded encryption key from property " + std::string(EncryptionKey.name));    }
  }
}

void FetchGCSObject::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(gcp_credentials_);

  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  auto bucket = context.getProperty(Bucket, flow_file.get());
  if (!bucket || bucket->empty()) {
    logger_->log_error("Missing bucket name");
    session.transfer(flow_file, Failure);
    return;
  }
  auto object_name = context.getProperty(Key, flow_file.get());
  if (!object_name || object_name->empty()) {
    logger_->log_error("Missing object name");
    session.transfer(flow_file, Failure);
    return;
  }

  gcs::Client client = getClient();
  FetchFromGCSCallback callback(client, *bucket, *object_name);
  callback.setEncryptionKey(encryption_key_);

  if (const auto object_generation_str =  context.getProperty(ObjectGeneration, flow_file.get()); object_generation_str && !object_generation_str->empty()) {
    if (const auto geni64 = parsing::parseIntegral<int64_t>(*object_generation_str)) {
      gcs::Generation generation = gcs::Generation{*geni64};
      callback.setGeneration(generation);

    } else {
      logger_->log_error("Invalid generation: {}", *object_generation_str);
      session.transfer(flow_file, Failure);
      return;
    }
  }


  session.write(flow_file, std::ref(callback));
  if (!callback.getStatus().ok()) {
    flow_file->setAttribute(GCS_STATUS_MESSAGE, callback.getStatus().message());
    flow_file->setAttribute(GCS_ERROR_REASON, callback.getStatus().error_info().reason());
    flow_file->setAttribute(GCS_ERROR_DOMAIN, callback.getStatus().error_info().domain());
    logger_->log_error("Failed to fetch from Google Cloud Storage {} {}", callback.getStatus().message(), callback.getStatus().error_info().reason());
    session.transfer(flow_file, Failure);
    return;
  }

  if (auto generation = callback.getGeneration())
    flow_file->setAttribute(GCS_GENERATION, std::to_string(*generation));
  if (auto meta_generation = callback.getMetaGeneration())
    flow_file->setAttribute(GCS_META_GENERATION, std::to_string(*meta_generation));
  if (auto storage_class = callback.getStorageClass())
    flow_file->setAttribute(GCS_STORAGE_CLASS, *storage_class);
  session.transfer(flow_file, Success);
}

REGISTER_RESOURCE(FetchGCSObject, Processor);

}  // namespace org::apache::nifi::minifi::extensions::gcp
