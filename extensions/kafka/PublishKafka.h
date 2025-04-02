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
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "KafkaConnection.h"
#include "KafkaProcessorBase.h"
#include "controllers/SSLContextService.h"
#include "core/Core.h"
#include "core/FlowFile.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "core/RelationshipDefinition.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "rdkafka.h"
#include "utils/ArrayUtils.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

class PublishKafka final : public KafkaProcessorBase {
 public:
  enum class CompressionCodecEnum { none, gzip, snappy, lz4, zstd };

  EXTENSIONAPI static constexpr const char* Description =
      "This Processor puts the contents of a FlowFile to a Topic in Apache "
      "Kafka. "
      "The content of a FlowFile becomes the contents of a Kafka message. "
      "This message is optionally assigned a key by using the <Kafka Key> "
      "Property.";

  EXTENSIONAPI static constexpr auto SeedBrokers =
      core::PropertyDefinitionBuilder<>::createProperty("Known Brokers")
          .withDescription(
              "A comma-separated list of known Kafka Brokers in the format "
              "<host>:<port>")
          .isRequired(true)
          .supportsExpressionLanguage(true)
          .build();
  EXTENSIONAPI static constexpr auto Topic =
      core::PropertyDefinitionBuilder<>::createProperty("Topic Name")
          .withDescription("The Kafka Topic of interest")
          .isRequired(true)
          .supportsExpressionLanguage(true)
          .build();
  EXTENSIONAPI static constexpr auto DeliveryGuarantee =
      core::PropertyDefinitionBuilder<>::createProperty("Delivery Guarantee")
          .withDescription(
              "Specifies the requirement for guaranteeing that a message is "
              "sent to Kafka. "
              "Valid values are 0 (do not wait for acks), "
              "-1 or all (block until message is committed by all in sync "
              "replicas) "
              "or any concrete number of nodes.")
          .isRequired(false)
          .supportsExpressionLanguage(true)
          .withDefaultValue("1")
          .build();
  EXTENSIONAPI static constexpr auto MaxMessageSize =
      core::PropertyDefinitionBuilder<>::createProperty("Max Request Size")
          .withDescription("Maximum Kafka protocol request message size")
          .isRequired(false)
          .build();
  EXTENSIONAPI static constexpr auto RequestTimeOut =
      core::PropertyDefinitionBuilder<>::createProperty("Request Timeout")
          .withDescription("The ack timeout of the producer request")
          .isRequired(false)
          .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
          .withDefaultValue("10 sec")
          .build();
  EXTENSIONAPI static constexpr auto MessageTimeOut =
      core::PropertyDefinitionBuilder<>::createProperty("Message Timeout")
          .withDescription("The total time sending a message could take")
          .isRequired(false)
          .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
          .withDefaultValue("30 sec")
          .build();
  EXTENSIONAPI static constexpr auto ClientName =
      core::PropertyDefinitionBuilder<>::createProperty("Client Name")
          .withDescription("Client Name to use when communicating with Kafka")
          .isRequired(true)
          .supportsExpressionLanguage(true)
          .build();
  EXTENSIONAPI static constexpr auto BatchSize =
      core::PropertyDefinitionBuilder<>::createProperty("Batch Size")
          .withDescription("Maximum number of messages batched in one MessageSet")
          .isRequired(false)
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .withDefaultValue("10")
          .build();
  EXTENSIONAPI static constexpr auto TargetBatchPayloadSize =
      core::PropertyDefinitionBuilder<>::createProperty("Target Batch Payload Size")
          .withDescription(
              "The target total payload size for a batch. 0 B means unlimited "
              "(Batch Size is still applied).")
          .isRequired(false)
          .withValidator(core::StandardPropertyValidators::DATA_SIZE_VALIDATOR)
          .withDefaultValue("512 KB")
          .build();
  EXTENSIONAPI static constexpr auto AttributeNameRegex =
      core::PropertyDefinitionBuilder<>::createProperty("Attributes to Send as Headers")
          .withDescription(
              "Any attribute whose name matches the regex will be added to the "
              "Kafka messages as a Header")
          .build();
  EXTENSIONAPI static constexpr auto QueueBufferMaxTime =
      core::PropertyDefinitionBuilder<>::createProperty("Queue Buffering Max Time")
          .withDescription(
              "Delay to wait for messages in the producer queue to accumulate "
              "before constructing message batches")
          .isRequired(false)
          .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
          .withDefaultValue("5 millis")
          .build();
  EXTENSIONAPI static constexpr auto QueueBufferMaxSize =
      core::PropertyDefinitionBuilder<>::createProperty("Queue Max Buffer Size")
          .withDescription("Maximum total message size sum allowed on the producer queue")
          .isRequired(false)
          .withValidator(core::StandardPropertyValidators::DATA_SIZE_VALIDATOR)
          .withDefaultValue("1 MB")
          .build();
  EXTENSIONAPI static constexpr auto QueueBufferMaxMessage =
      core::PropertyDefinitionBuilder<>::createProperty("Queue Max Message")
          .withDescription("Maximum number of messages allowed on the producer queue")
          .isRequired(false)
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .withDefaultValue("1000")
          .build();
  EXTENSIONAPI static constexpr auto CompressCodec =
      core::PropertyDefinitionBuilder<magic_enum::enum_count<CompressionCodecEnum>()>::createProperty("Compress Codec")
          .withDescription("compression codec to use for compressing message sets")
          .isRequired(false)
          .withDefaultValue(magic_enum::enum_name(CompressionCodecEnum::none))
          .withAllowedValues(magic_enum::enum_names<CompressionCodecEnum>())
          .build();
  EXTENSIONAPI static constexpr auto MaxFlowSegSize =
      core::PropertyDefinitionBuilder<>::createProperty("Max Flow Segment Size")
          .withDescription(
              "Maximum flow content payload segment size for the kafka record. "
              "0 B means unlimited.")
          .isRequired(false)
          .withValidator(core::StandardPropertyValidators::DATA_SIZE_VALIDATOR)
          .withDefaultValue("0 B")
          .build();
  EXTENSIONAPI static constexpr auto SecurityCA =
      core::PropertyDefinitionBuilder<>::createProperty("Security CA")
          .withDescription(
              "DEPRECATED in favor of SSL Context Service. File or directory "
              "path to CA certificate(s) for verifying the broker's key")
          .build();
  EXTENSIONAPI static constexpr auto SecurityCert =
      core::PropertyDefinitionBuilder<>::createProperty("Security Cert")
          .withDescription(
              "DEPRECATED in favor of SSL Context Service.Path to client's "
              "public key (PEM) used for authentication")
          .build();
  EXTENSIONAPI static constexpr auto SecurityPrivateKey =
      core::PropertyDefinitionBuilder<>::createProperty("Security Private Key")
          .withDescription(
              "DEPRECATED in favor of SSL Context Service.Path to client's "
              "private key (PEM) used for authentication")
          .build();
  EXTENSIONAPI static constexpr auto SecurityPrivateKeyPassWord =
      core::PropertyDefinitionBuilder<>::createProperty("Security Pass Phrase")
          .withDescription(
              "DEPRECATED in favor of SSL Context Service.Private key "
              "passphrase")
          .isSensitive(true)
          .build();
  EXTENSIONAPI static constexpr auto KafkaKey =
      core::PropertyDefinitionBuilder<>::createProperty("Kafka Key")
          .withDescription(
              "The key to use for the message. If not specified, the UUID of "
              "the flow file is used as the message key.")
          .supportsExpressionLanguage(true)
          .build();
  EXTENSIONAPI static constexpr auto MessageKeyField =
      core::PropertyDefinitionBuilder<>::createProperty("Message Key Field")
          .withDescription("DEPRECATED, does not work -- use Kafka Key instead")
          .build();
  EXTENSIONAPI static constexpr auto DebugContexts =
      core::PropertyDefinitionBuilder<>::createProperty("Debug contexts")
          .withDescription(
              "A comma-separated list of debug contexts to enable."
              "Including: generic, broker, topic, metadata, feature, queue, "
              "msg, protocol, cgrp, security, fetch, interceptor, plugin, "
              "consumer, "
              "admin, eos, all")
          .build();
  EXTENSIONAPI static constexpr auto FailEmptyFlowFiles =
      core::PropertyDefinitionBuilder<>::createProperty("Fail empty flow files")
          .withDescription(
              "Keep backwards compatibility with <=0.7.0 bug which caused flow "
              "files with empty content to not be published to Kafka and "
              "forwarded "
              "to failure. The old behavior is "
              "deprecated. Use connections to drop empty flow files!")
          .isRequired(false)
          .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
          .withDefaultValue("true")
          .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(KafkaProcessorBase::Properties,
      std::to_array<core::PropertyReference>({SeedBrokers, Topic, DeliveryGuarantee, MaxMessageSize, RequestTimeOut,
          MessageTimeOut, ClientName, BatchSize, TargetBatchPayloadSize, AttributeNameRegex, QueueBufferMaxTime,
          QueueBufferMaxSize, QueueBufferMaxMessage, CompressCodec, MaxFlowSegSize, SecurityCA, SecurityCert,
          SecurityPrivateKey, SecurityPrivateKeyPassWord, KafkaKey, MessageKeyField, DebugContexts, FailEmptyFlowFiles}));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success",
      "Any FlowFile that is successfully sent to Kafka will be routed to "
      "this Relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure",
      "Any FlowFile that cannot be sent to Kafka will be routed to this "
      "Relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr auto InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PublishKafka(const std::string_view name, const utils::Identifier& uuid = {})
      : KafkaProcessorBase(name, uuid, core::logging::LoggerFactory<PublishKafka>::getLogger(uuid)) {}

  PublishKafka(const PublishKafka&) = delete;
  PublishKafka(PublishKafka&&) = delete;
  PublishKafka& operator=(const PublishKafka&) = delete;
  PublishKafka& operator=(PublishKafka&&) = delete;
  ~PublishKafka() override = default;

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& sessionFactory) override;
  void notifyStop() override;

  class Messages;

 protected:
  bool configureNewConnection(core::ProcessContext& context);
  bool createNewTopic(
      core::ProcessContext& context, const std::string& topic_name, const std::shared_ptr<core::FlowFile>& flow_file) const;
  std::optional<utils::net::SslData> getSslData(core::ProcessContext& context) const override;

 private:
  KafkaConnectionKey key_;
  std::unique_ptr<KafkaConnection> conn_;
  std::mutex connection_mutex_;

  uint64_t batch_size_{};
  uint64_t target_batch_payload_size_{};
  uint64_t max_flow_seg_size_{};
  std::optional<utils::Regex> attributeNameRegex_;

  std::atomic<bool> interrupted_{false};
  std::mutex messages_mutex_;  // If both connection_mutex_ and messages_mutex_ are needed, always take connection_mutex_
                               // first to avoid deadlock
  std::set<std::shared_ptr<Messages>> messages_set_;
};

}  // namespace org::apache::nifi::minifi::processors
