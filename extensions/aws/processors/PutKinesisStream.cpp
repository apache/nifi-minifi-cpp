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
#include <ranges>
#include <unordered_map>

#include "aws/kinesis/KinesisClient.h"
#include "aws/kinesis/model/PutRecordsRequest.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "range/v3/algorithm/for_each.hpp"
#include "range/v3/view.hpp"
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
    logger_->log_warn("{} is invalid. Setting it to the maximum 500 value.", MessageBatchSize.name);
    batch_size_ = 500;
  }
  batch_data_size_soft_cap_ = parseDataSizeProperty(context, MaxBatchDataSize);
  if (batch_data_size_soft_cap_ > 4_MB) {
    logger_->log_warn("{} is invalid. Setting it to the maximum 4 MB value.", MaxBatchDataSize.name);
    batch_data_size_soft_cap_ = 4_MB;
  }

  endpoint_override_url_ = context.getProperty(EndpointOverrideURL.name) | minifi::utils::toOptional();
}

nonstd::expected<Aws::Kinesis::Model::PutRecordsRequestEntry, PutKinesisStream::BatchItemError> PutKinesisStream::createEntryFromFlowFile(const core::ProcessContext& context,
    core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file) const {
  Aws::Kinesis::Model::PutRecordsRequestEntry entry;
  const auto partition_key = context.getProperty(AmazonKinesisStreamPartitionKey.name, flow_file.get()) | minifi::utils::valueOrElse([&flow_file] { return flow_file->getUUID().to_string(); });
  entry.SetPartitionKey(partition_key);
  const auto [status, buffer] = session.readBuffer(flow_file);
  if (io::isError(status)) {
    logger_->log_error("Couldn't read content from {}", flow_file->getUUIDStr());
    return nonstd::make_unexpected(BatchItemError{.error_message = "Failed to read content", .error_code = std::nullopt});
  }
  Aws::Utils::ByteBuffer aws_buffer(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size());
  entry.SetData(aws_buffer);
  return entry;
}

std::unordered_map<std::string, PutKinesisStream::StreamBatch> PutKinesisStream::createStreamBatches(const core::ProcessContext& context, core::ProcessSession& session) const {
  static constexpr uint64_t SINGLE_RECORD_MAX_SIZE = 1_MB;
  std::unordered_map<std::string, StreamBatch> stream_batches;
  uint64_t ff_count_in_batches = 0;
  while (ff_count_in_batches < batch_size_) {
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
      flow_file->setAttribute(AwsKinesisErrorMessage.name, fmt::format("Stream name is invalid due to {}", stream_name.error().message()));
      session.transfer(flow_file, Failure);
      continue;
    }

    auto entry = createEntryFromFlowFile(context, session, flow_file);
    if (!entry) {
      flow_file->addAttribute(AwsKinesisErrorMessage.name, entry.error().error_message);
      if (entry.error().error_code) {
        flow_file->addAttribute(AwsKinesisErrorCode.name, *entry.error().error_code);
      }
      session.transfer(flow_file, Failure);
      continue;
    }

    auto [stream_batch, newly_created] = stream_batches.emplace(*stream_name, StreamBatch{});
    if (newly_created) {
      stream_batch->second.request.SetStreamName(*stream_name);
    }
    stream_batch->second.request.AddRecords(*entry);
    stream_batch->second.items.push_back(BatchItem{.flow_file = std::move(flow_file), .result = BatchItemResult{}});
    stream_batches[*stream_name].batch_size += flow_file_size;
    ++ff_count_in_batches;

    if (stream_batches[*stream_name].batch_size > batch_data_size_soft_cap_) {
      break;
    }
  }
  return stream_batches;
}

void PutKinesisStream::processBatch(StreamBatch& stream_batch, const Aws::Kinesis::KinesisClient& client) const {
  const auto put_record_result = client.PutRecords(stream_batch.request);
  if (!put_record_result.IsSuccess()) {
    ranges::for_each(stream_batch.items, [&](auto& item) {
      item.result = nonstd::make_unexpected(BatchItemError{
        .error_message = put_record_result.GetError().GetMessage(),
        .error_code = std::to_string(static_cast<int>(put_record_result.GetError().GetResponseCode()))});
    });
    return;
  }

  const auto result_records = put_record_result.GetResult().GetRecords();
  if (result_records.size() != stream_batch.items.size()) {
    logger_->log_critical("PutKinesisStream record size ({}) and result size ({}) mismatch in {} cannot tell which record succeeded and which didnt",
        stream_batch.items.size(), result_records.size(), stream_batch.request.GetStreamName());
    ranges::for_each(stream_batch.items, [&](auto& item) {
      item.result = nonstd::make_unexpected(BatchItemError{
        .error_message = "Record size mismatch",
        .error_code = std::nullopt});
    });
    return;
  }

  for (uint64_t i = 0; i < stream_batch.items.size(); i++) {
    auto& [flow_file, result] = stream_batch.items[i];
    const auto& result_record = result_records[i];
    if (!result_record.GetErrorCode().empty()) {
      result = nonstd::make_unexpected(BatchItemError{.error_message = result_record.GetErrorMessage(), .error_code = result_record.GetErrorCode()});
    } else {
      result = BatchItemResult{.sequence_number = result_record.GetSequenceNumber(), .shard_id = result_record.GetShardId()};
    }
  }
}

void PutKinesisStream::transferFlowFiles(core::ProcessSession& session, const StreamBatch& stream_batch) {
  for (const auto& batch_item : stream_batch.items) {
    if (batch_item.result) {
      batch_item.flow_file->setAttribute(AwsKinesisSequenceNumber.name, batch_item.result->sequence_number);
      batch_item.flow_file->setAttribute(AwsKinesisShardId.name, batch_item.result->shard_id);
      session.transfer(batch_item.flow_file, Success);
    } else {
      batch_item.flow_file->setAttribute(AwsKinesisErrorMessage.name, batch_item.result.error().error_message);
      if (batch_item.result.error().error_code) {
        batch_item.flow_file->setAttribute(AwsKinesisErrorCode.name, *batch_item.result.error().error_code);
      }
      session.transfer(batch_item.flow_file, Failure);
    }
  }
}


void PutKinesisStream::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("PutKinesisStream onTrigger");

  const auto credentials = getAWSCredentials(context, nullptr);
  if (!credentials) {
    logger_->log_error("Failed to get credentials for PutKinesisStream");
    context.yield();
    return;
  }

  auto stream_batches = createStreamBatches(context, session);
  if (stream_batches.empty()) {
    context.yield();
    return;
  }
  const auto kinesis_client = getClient(*credentials);

  for (auto& stream_batch: stream_batches | std::views::values) {
    processBatch(stream_batch, *kinesis_client);
    transferFlowFiles(session, stream_batch);
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
