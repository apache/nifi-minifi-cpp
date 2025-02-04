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

#include "PutKinesisStream.h"

#include <memory>
#include <random>
#include <unordered_map>

#include "aws/kinesis/KinesisClient.h"
#include "aws/kinesis/model/PutRecordsRequest.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

void PutKinesisStream::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutKinesisStream::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  AwsProcessor::onSchedule(context, session_factory);

  batch_size_ = parseU64Property(context, MessageBatchSize);
  if (batch_size_ == 0 || batch_size_ > 500) {
    logger_->log_warn("PutKinesisStream::MessageBatchSize is invalid. Setting it to the maximum 500 value.");
    batch_size_ = 500;
  }
  batch_data_size_soft_cap_ = parseDataSizeProperty(context, MaxBatchDataSize);
  if (batch_data_size_soft_cap_ > 4_MB) {
    logger_->log_warn("PutKinesisStream::MaxMessageBufferSize is invalid. Setting it to the maximum 4 MB value.");
    batch_data_size_soft_cap_ = 4_MB;
  }

  endpoint_override_url_ = context.getProperty(EndpointOverrideURL.name) | minifi::utils::toOptional();
}

struct StreamBatch {
  uint64_t batch_size = 0;
  std::vector<std::shared_ptr<core::FlowFile>> flow_files;
};

void PutKinesisStream::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("PutKinesisStream onTrigger");

  constexpr uint64_t SINGLE_RECORD_MAX_SIZE = 1_MB;
  std::unordered_map<std::string, StreamBatch> stream_batches;
  auto credentials = getAWSCredentials(context, nullptr);

  if (!credentials) {
    logger_->log_error("Failed to get credentials for PutKinesisStream");
    context.yield();
    return;
  }

  for (uint64_t i = 0; i < batch_size_; i++) {
    std::shared_ptr<core::FlowFile> flow_file = session.get();
    if (!flow_file) { break; }
    const auto flow_file_size = flow_file->getSize();
    if (flow_file_size > SINGLE_RECORD_MAX_SIZE) {
      flow_file->setAttribute(AwsKinesisErrorMessage.name, fmt::format("record too big {}, max allowed {}", flow_file_size, SINGLE_RECORD_MAX_SIZE));
      session.transfer(flow_file, Failure);
      logger_->log_error("Failed to publish to kinesis record {} because the size was greater than {} bytes", flow_file->getUUID().to_string(), SINGLE_RECORD_MAX_SIZE);
      continue;
    }

    auto stream_name = context.getProperty(AmazonKinesisStreamName.name, flow_file.get());
    if (!stream_name) {
      logger_->log_error("Stream name is invalid due to {}", stream_name.error().message());
      session.transfer(flow_file, Failure);
      continue;
    }
    auto partition_key = context.getProperty(AmazonKinesisStreamPartitionKey.name, flow_file.get())
        | minifi::utils::valueOrElse([&flow_file]() -> std::string { return flow_file->getUUID().to_string(); });

    stream_batches[*stream_name].flow_files.push_back(std::move(flow_file));
    stream_batches[*stream_name].batch_size += flow_file_size;

    if (stream_batches[*stream_name].batch_size > batch_data_size_soft_cap_) {
      break;
    }
  }

  std::unique_ptr<Aws::Kinesis::KinesisClient> kinesis_client = getClient(*credentials);

  for (const auto& [stream_name, stream_batch]: stream_batches) {
    Aws::Kinesis::Model::PutRecordsRequest request;
    request.SetStreamName(stream_name);
    Aws::Vector<Aws::Kinesis::Model::PutRecordsRequestEntry> records;
    for (const auto& flow_file : stream_batch.flow_files) {
      Aws::Kinesis::Model::PutRecordsRequestEntry entry;
      const auto partition_key = context.getProperty(AmazonKinesisStreamPartitionKey.name, flow_file.get()) | minifi::utils::valueOrElse([&flow_file] { return flow_file->getUUID().to_string(); });
      entry.SetPartitionKey(partition_key);
      const auto [status, buffer] = session.readBuffer(flow_file);
      Aws::Utils::ByteBuffer aws_buffer(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size());
      entry.SetData(aws_buffer);
      records.push_back(entry);
    }
    request.SetRecords(records);

    const auto outcome = kinesis_client->PutRecords(request);

    if (!outcome.IsSuccess()) {
      for (const auto& flow_file : stream_batch.flow_files) {
        flow_file->addAttribute(AwsKinesisErrorMessage.name, outcome.GetError().GetMessage());
        flow_file->addAttribute(AwsKinesisErrorCode.name, std::to_string(static_cast<int>(outcome.GetError().GetErrorType())));
        session.transfer(flow_file, Failure);
      }
    } else {
      const auto result_records = outcome.GetResult().GetRecords();
      if (result_records.size() != stream_batch.flow_files.size()) {
        logger_->log_critical("PutKinesisStream record size mismatch cannot tell which record succeeded and which didnt");
        for (const auto& flow_file : stream_batch.flow_files) {
          flow_file->addAttribute(AwsKinesisErrorMessage.name, "Record size mismatch");
          session.transfer(flow_file, Failure);
        }
        continue;
      }
      for (uint64_t i = 0; i < stream_batch.flow_files.size(); i++) {
        const auto& flow_file = stream_batch.flow_files[i];
        const auto& result_record = result_records[i];
        if (result_record.GetErrorCode().empty()) {
          flow_file->addAttribute(AwsKinesisShardId.name, result_record.GetShardId());
          flow_file->addAttribute(AwsKinesisSequenceNumber.name, result_record.GetSequenceNumber());
          session.transfer(flow_file, Success);
        } else {
          flow_file->addAttribute(AwsKinesisErrorMessage.name, result_record.GetErrorMessage());
          flow_file->addAttribute(AwsKinesisErrorCode.name, result_record.GetErrorCode());
          session.transfer(flow_file, Failure);
        }
      }
    }
  }
}

std::unique_ptr<Aws::Kinesis::KinesisClient> PutKinesisStream::getClient(const Aws::Auth::AWSCredentials& credentials) {
  gsl_Expects(client_config_);
  auto client = std::make_unique<Aws::Kinesis::KinesisClient>(credentials, *client_config_);
  if (endpoint_override_url_) {
    client->OverrideEndpoint(*endpoint_override_url_);
  }
  return client;
}

REGISTER_RESOURCE(PutKinesisStream, Processor);

}  // namespace org::apache::nifi::minifi::aws::processors
