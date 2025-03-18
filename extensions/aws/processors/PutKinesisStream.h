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


#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "S3Processor.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/ArrayUtils.h"
#include "aws/kinesis/KinesisClient.h"


namespace org::apache::nifi::minifi::aws::processors {

class PutKinesisStream : public AwsProcessor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Sends the contents to a specified Amazon Kinesis. In order to send data to Kinesis, the stream name has to be specified.";

  EXTENSIONAPI static constexpr auto AmazonKinesisStreamName = core::PropertyDefinitionBuilder<>::createProperty("Amazon Kinesis Stream Name")
      .withDescription("The name of Kinesis Stream")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto AmazonKinesisStreamPartitionKey = core::PropertyDefinitionBuilder<>::createProperty("Amazon Kinesis Stream Partition Key")
      .withDescription("The partition key attribute. If it is not set, a random value is used")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto MessageBatchSize = core::PropertyDefinitionBuilder<>::createProperty("Batch Size")
      .withDescription("Batch size for messages. [1-500]")
      .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
      .withDefaultValue("250")
      .build();
  EXTENSIONAPI static constexpr auto MaxBatchDataSize = core::PropertyDefinitionBuilder<>::createProperty("Max Batch Data Size")
      .withDescription("Soft cap on the data size of the batch to a single stream. (max 4MB)")
      .withValidator(core::StandardPropertyValidators::DATA_SIZE_VALIDATOR)
      .withDefaultValue("1 MB")
      .build();

  EXTENSIONAPI static constexpr auto Properties = minifi::utils::array_cat(AwsProcessor::Properties, std::to_array<core::PropertyReference>({
    AmazonKinesisStreamName, AmazonKinesisStreamPartitionKey, MessageBatchSize, MaxBatchDataSize
  }));

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles are routed to success relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles are routed to failure relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr auto AwsKinesisErrorMessage = core::OutputAttributeDefinition<>{"aws.kinesis.error.message", { Failure },
    "Error message on posting message to AWS Kinesis"};
  EXTENSIONAPI static constexpr auto AwsKinesisErrorCode = core::OutputAttributeDefinition<>{"aws.kinesis.error.code", { Failure },
    "Error code for the message when posting to AWS Kinesis"};
  EXTENSIONAPI static constexpr auto AwsKinesisSequenceNumber = core::OutputAttributeDefinition<>{"aws.kinesis.sequence.number", { Success },
    "Sequence number for the message when posting to AWS Kinesis"};
  EXTENSIONAPI static constexpr auto AwsKinesisShardId = core::OutputAttributeDefinition<>{"aws.kinesis.shard.id", { Success },
    "Shard id of the message posted to AWS Kinesis"};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 4>{AwsKinesisErrorMessage, AwsKinesisErrorCode, AwsKinesisSequenceNumber, AwsKinesisShardId};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr auto InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutKinesisStream(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : AwsProcessor(name, uuid, core::logging::LoggerFactory<PutKinesisStream>::getLogger(uuid)) {
  }

  ~PutKinesisStream() override = default;

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 protected:
  virtual std::unique_ptr<Aws::Kinesis::KinesisClient> getClient(const Aws::Auth::AWSCredentials& credentials);

 private:
  uint64_t batch_size_ = 250;
  uint64_t batch_data_size_soft_cap_ = 1_MB;
  const utils::AWSInitializer& AWS_INITIALIZER = utils::AWSInitializer::get();
  std::optional<std::string> endpoint_override_url_;
};

}  // namespace org::apache::nifi::minifi::aws::processors
