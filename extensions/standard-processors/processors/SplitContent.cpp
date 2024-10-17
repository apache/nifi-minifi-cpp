/**
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

#include "SplitContent.h"

#include <range/v3/view/split.hpp>

#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {
void SplitContent::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void SplitContent::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  auto byte_sequence_str = utils::getRequiredPropertyOrThrow<std::string>(context, ByteSequence.name);
  const auto byte_sequence_format = utils::parseEnumProperty<ByteSequenceFormat>(context, ByteSequenceFormatProperty);
  std::vector<std::byte> byte_sequence{};
  if (byte_sequence_format == ByteSequenceFormat::Hexadecimal) {
    byte_sequence = utils::string::from_hex(byte_sequence_str);
  } else {
    byte_sequence.resize(byte_sequence_str.size());
    std::ranges::transform(byte_sequence_str, byte_sequence.begin(), [](char c) { return static_cast<std::byte>(c); });
  }
  if (byte_sequence.empty()) { throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Cannot operate without byte sequence"); }
  byte_sequence_matcher_.emplace(ByteSequenceMatcher(std::move(byte_sequence)));
  byte_sequence_location_ = utils::parseEnumProperty<ByteSequenceLocation>(context, ByteSequenceLocationProperty);
  keep_byte_sequence = utils::getRequiredPropertyOrThrow<bool>(context, KeepByteSequence.name);
}

std::span<const std::byte> SplitContent::getByteSequence() const {
  gsl_Assert(byte_sequence_matcher_);
  return byte_sequence_matcher_->getByteSequence();
}

SplitContent::size_type SplitContent::getByteSequenceSize() const {
  return getByteSequence().size();
}

namespace {
void updateSplitAttributesAndTransfer(core::ProcessSession& session, const std::vector<std::shared_ptr<core::FlowFile>>& splits, const core::FlowFile& original) {
  const std::string fragment_identifier_ = utils::IdGenerator::getIdGenerator()->generate().to_string();
  for (size_t split_i = 0; split_i < splits.size(); ++split_i) {
    const auto& split = splits[split_i];
    split->setAttribute(SplitContent::FragmentCountOutputAttribute.name, std::to_string(splits.size()));
    split->setAttribute(SplitContent::FragmentIndexOutputAttribute.name, std::to_string(split_i + 1));  // One based indexing
    split->setAttribute(SplitContent::FragmentIdentifierOutputAttribute.name, fragment_identifier_);
    split->setAttribute(SplitContent::SegmentOriginalFilenameOutputAttribute.name, original.getAttribute(core::SpecialFlowAttribute::FILENAME).value_or(""));
    session.transfer(split, SplitContent::Splits);
  }
}

void ensureFlowFile(std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) {
  if (!flow_file) {
    flow_file = session.create();
  }
}
}  // namespace

SplitContent::ByteSequenceMatcher::ByteSequenceMatcher(std::vector<std::byte> byte_sequence) : byte_sequence_(std::move(byte_sequence)) {
  byte_sequence_nodes_.push_back(node{.byte = {}, .cache = {}, .previous_max_match = {}});
  for (const auto& byte: byte_sequence_) { byte_sequence_nodes_.push_back(node{.byte = byte, .cache = {}, .previous_max_match = {}}); }
}

SplitContent::size_type SplitContent::ByteSequenceMatcher::getNumberOfMatchingBytes(const size_type number_of_currently_matching_bytes, const std::byte next_byte) {
  gsl_Assert(number_of_currently_matching_bytes <= byte_sequence_nodes_.size());
  auto& curr_go = byte_sequence_nodes_[number_of_currently_matching_bytes].cache;
  if (curr_go.contains(next_byte)) { return curr_go.at(next_byte); }
  if (next_byte == byte_sequence_nodes_[number_of_currently_matching_bytes + 1].byte) {
    curr_go[next_byte] = number_of_currently_matching_bytes + 1;
    return number_of_currently_matching_bytes + 1;
  }
  if (number_of_currently_matching_bytes == 0) {
    curr_go[next_byte] = 0;
    return 0;
  }

  curr_go[next_byte] = getNumberOfMatchingBytes(getPreviousMaxMatch(number_of_currently_matching_bytes), next_byte);
  return curr_go.at(next_byte);
}

SplitContent::size_type SplitContent::ByteSequenceMatcher::getPreviousMaxMatch(const size_type number_of_currently_matching_bytes) {
  gsl_Assert(number_of_currently_matching_bytes <= byte_sequence_nodes_.size());
  auto& prev_max_match = byte_sequence_nodes_[number_of_currently_matching_bytes].previous_max_match;
  if (prev_max_match) { return *prev_max_match; }
  if (number_of_currently_matching_bytes <= 1) {
    prev_max_match = 0;
    return 0;
  }
  prev_max_match = getNumberOfMatchingBytes(getPreviousMaxMatch(number_of_currently_matching_bytes - 1), byte_sequence_nodes_[number_of_currently_matching_bytes].byte);
  return *prev_max_match;
}

void SplitContent::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Assert(byte_sequence_matcher_);
  const auto original = session.get();
  if (!original) {
    context.yield();
    return;
  }

  const auto ff_content_stream = session.getFlowFileContentStream(*original);
  if (!ff_content_stream) { throw Exception(PROCESSOR_EXCEPTION, fmt::format("Couldn't access the ContentStream of {}", original->getUUID().to_string())); }
  std::vector<std::byte> data_before_byte_sequence;
  data_before_byte_sequence.reserve(BUFFER_TARGET_SIZE);
  size_type matching_bytes = 0;

  std::vector<std::shared_ptr<core::FlowFile>> splits{};
  std::shared_ptr<core::FlowFile> current_split = nullptr;

  while (auto latest_byte = ff_content_stream->readByte()) {
    const size_type prev_matching_bytes = matching_bytes;
    matching_bytes = byte_sequence_matcher_->getNumberOfMatchingBytes(matching_bytes, *latest_byte);
    if (matching_bytes == getByteSequenceSize()) {
      if (!data_before_byte_sequence.empty()) {
        ensureFlowFile(current_split, session);
        session.appendBuffer(current_split, std::span<const std::byte>(data_before_byte_sequence.data(), data_before_byte_sequence.size()));
      }
      if (keepTrailingByteSequence()) {
        ensureFlowFile(current_split, session);
        session.appendBuffer(current_split, getByteSequence());
      }
      if (current_split && current_split->getSize() > 0) {
        splits.push_back(current_split);
        current_split.reset();
      }
      data_before_byte_sequence.clear();
      matching_bytes = 0;
      if (keepLeadingByteSequence()) {
        ensureFlowFile(current_split, session);
        session.appendBuffer(current_split, getByteSequence());
      }
      continue;
    }
    if (matching_bytes > prev_matching_bytes) {
      // matching grew, no need to grow data_before_byte_sequence
      continue;
    }
    if (matching_bytes > 0) {
      // last byte could be part of the byte_sequence
      std::copy_n(getByteSequence().begin(), prev_matching_bytes - matching_bytes + 1, std::back_inserter(data_before_byte_sequence));
    } else {
      // last byte is not part of the byte_sequence
      std::copy_n(getByteSequence().begin(), prev_matching_bytes - matching_bytes, std::back_inserter(data_before_byte_sequence));
      data_before_byte_sequence.push_back(*latest_byte);
    }

    if (data_before_byte_sequence.size() >= BUFFER_TARGET_SIZE) {
      ensureFlowFile(current_split, session);
      session.appendBuffer(current_split, std::span<const std::byte>(data_before_byte_sequence.data(), data_before_byte_sequence.size()));
      data_before_byte_sequence.clear();
    }
  }

  // no more data in original, we need to flush the remainder to the last split
  if (current_split || !data_before_byte_sequence.empty() || matching_bytes > 0) {
    ensureFlowFile(current_split, session);
    session.appendBuffer(current_split, std::span<const std::byte>(data_before_byte_sequence.data(), data_before_byte_sequence.size()));
    session.appendBuffer(current_split, byte_sequence_matcher_->getByteSequence().subspan(0, matching_bytes));
    splits.push_back(current_split);
  }

  updateSplitAttributesAndTransfer(session, splits, *original);
  session.transfer(original, Original);
}

REGISTER_RESOURCE(SplitContent, Processor);

}  // namespace org::apache::nifi::minifi::processors
