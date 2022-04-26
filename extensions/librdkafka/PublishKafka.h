/**
 * @file PublishKafka.h
 * PublishKafka class declaration
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
#ifndef EXTENSIONS_LIBRDKAFKA_PUBLISHKAFKA_H_
#define EXTENSIONS_LIBRDKAFKA_PUBLISHKAFKA_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <condition_variable>
#include <utility>
#include <vector>

#include "KafkaProcessorBase.h"
#include "utils/GeneralUtils.h"
#include "FlowFileRecord.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/logging/Logger.h"
#include "controllers/SSLContextService.h"
#include "rdkafka.h"
#include "KafkaConnection.h"
#include "utils/RegexUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// PublishKafka Class
class PublishKafka : public KafkaProcessorBase {
 public:
  static constexpr char const* ProcessorName = "PublishKafka";

  // Supported Properties
  EXTENSIONAPI static const core::Property SeedBrokers;
  EXTENSIONAPI static const core::Property Topic;
  EXTENSIONAPI static const core::Property DeliveryGuarantee;
  EXTENSIONAPI static const core::Property MaxMessageSize;
  EXTENSIONAPI static const core::Property RequestTimeOut;
  EXTENSIONAPI static const core::Property MessageTimeOut;
  EXTENSIONAPI static const core::Property ClientName;
  EXTENSIONAPI static const core::Property BatchSize;
  EXTENSIONAPI static const core::Property TargetBatchPayloadSize;
  EXTENSIONAPI static const core::Property AttributeNameRegex;
  EXTENSIONAPI static const core::Property QueueBufferMaxTime;
  EXTENSIONAPI static const core::Property QueueBufferMaxSize;
  EXTENSIONAPI static const core::Property QueueBufferMaxMessage;
  EXTENSIONAPI static const core::Property CompressCodec;
  EXTENSIONAPI static const core::Property MaxFlowSegSize;
  EXTENSIONAPI static const core::Property SecurityCA;
  EXTENSIONAPI static const core::Property SecurityCert;
  EXTENSIONAPI static const core::Property SecurityPrivateKey;
  EXTENSIONAPI static const core::Property SecurityPrivateKeyPassWord;
  EXTENSIONAPI static const core::Property KafkaKey;
  EXTENSIONAPI static const core::Property MessageKeyField;
  EXTENSIONAPI static const core::Property DebugContexts;
  EXTENSIONAPI static const core::Property FailEmptyFlowFiles;

  // Supported Relationships
  EXTENSIONAPI static const core::Relationship Failure;
  EXTENSIONAPI static const core::Relationship Success;

  static constexpr const char* COMPRESSION_CODEC_NONE = "none";
  static constexpr const char* COMPRESSION_CODEC_GZIP = "gzip";
  static constexpr const char* COMPRESSION_CODEC_SNAPPY = "snappy";
  static constexpr const char* ROUND_ROBIN_PARTITIONING = "Round Robin";
  static constexpr const char* RANDOM_PARTITIONING = "Random Robin";
  static constexpr const char* USER_DEFINED_PARTITIONING = "User-Defined";
  static constexpr const char* DELIVERY_REPLICATED = "all";
  static constexpr const char* DELIVERY_ONE_NODE = "1";
  static constexpr const char* DELIVERY_BEST_EFFORT = "0";
  static constexpr const char* KAFKA_KEY_ATTRIBUTE = "kafka.key";

  explicit PublishKafka(const std::string& name, const utils::Identifier& uuid = {})
      : KafkaProcessorBase(name, uuid, core::logging::LoggerFactory<PublishKafka>::getLogger()) {
  }

  ~PublishKafka() override = default;

  bool supportsDynamicProperties() override { return true; }

  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void notifyStop() override;

  class Messages;

 protected:
  bool configureNewConnection(const std::shared_ptr<core::ProcessContext> &context);
  bool createNewTopic(const std::shared_ptr<core::ProcessContext> &context, const std::string& topic_name, const std::shared_ptr<core::FlowFile>& flow_file);
  std::optional<utils::SSL_data> getSslData(core::ProcessContext& context) const override;

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  KafkaConnectionKey key_;
  std::unique_ptr<KafkaConnection> conn_;
  std::mutex connection_mutex_;

  uint32_t batch_size_{};
  uint64_t target_batch_payload_size_{};
  uint64_t max_flow_seg_size_{};
  utils::Regex attributeNameRegex_;

  std::atomic<bool> interrupted_{false};
  std::mutex messages_mutex_;  // If both connection_mutex_ and messages_mutex_ are needed, always take connection_mutex_ first to avoid deadlock
  std::set<std::shared_ptr<Messages>> messages_set_;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_LIBRDKAFKA_PUBLISHKAFKA_H_
