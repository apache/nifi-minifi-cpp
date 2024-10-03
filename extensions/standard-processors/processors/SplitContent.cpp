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

constexpr size_t BUFFER_TARGET_SIZE = 1024;

void SplitContent::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void SplitContent::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  auto byte_sequence_str = utils::getRequiredPropertyOrThrow<std::string>(context, ByteSequence.name);
  const auto byte_sequence_format = utils::parseEnumProperty<ByteSequenceFormat>(context, ByteSequenceFormatProperty);
  if (byte_sequence_format == ByteSequenceFormat::Hexadecimal) {
    byte_sequence_ = utils::string::from_hex(byte_sequence_str);
  } else {
    byte_sequence_.resize(byte_sequence_str.size());
    std::ranges::transform(byte_sequence_str, byte_sequence_.begin(), [](char c) { return static_cast<std::byte>(c); });
  }
  byte_sequence_location_ = utils::parseEnumProperty<ByteSequenceLocation>(context, ByteSequenceLocationProperty);
  keep_byte_sequence = utils::getRequiredPropertyOrThrow<bool>(context, KeepByteSequence.name);
}

std::shared_ptr<core::FlowFile> SplitContent::createNewSplit(core::ProcessSession& session) const {
  auto next_split = session.create();
  if (!next_split) {
    throw Exception(PROCESSOR_EXCEPTION, "Couldn't create FlowFile");
  }
  if (keep_byte_sequence && byte_sequence_location_ == ByteSequenceLocation::Leading) {
    session.appendBuffer(next_split, byte_sequence_);
  }
  return next_split;
}

void SplitContent::finalizeLatestSplitContent(core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& latest_split, const std::vector<std::byte>& buffer) const {
  const std::span<const std::byte> data_without_byte_sequence{buffer.data(), buffer.size() - byte_sequence_.size()};
  session.appendBuffer(latest_split, data_without_byte_sequence);
  if (keep_byte_sequence && byte_sequence_location_ == ByteSequenceLocation::Trailing) {
    session.appendBuffer(latest_split, byte_sequence_);
  }
}

void SplitContent::finalizeLastSplitContent(core::ProcessSession& session, std::vector<std::shared_ptr<core::FlowFile>>& splits, const std::vector<std::byte>& buffer,
    const bool ended_with_byte_sequence) const {
  if (ended_with_byte_sequence && splits.back()->getSize() != 0) {
    if (keep_byte_sequence && byte_sequence_location_ == ByteSequenceLocation::Leading) {
      const auto last_split = session.create();
      if (!last_split) {
        throw Exception(PROCESSOR_EXCEPTION, "Couldn't create FlowFile");
      }
      splits.push_back(last_split);
      session.appendBuffer(splits.back(), byte_sequence_);
    }
  } else {
    session.appendBuffer(splits.back(), buffer);
  }
}

namespace {
std::shared_ptr<core::FlowFile> createFirstSplit(core::ProcessSession& session) {
  auto first_split = session.create();
  if (!first_split) {
    throw Exception(PROCESSOR_EXCEPTION, "Couldn't create FlowFile");
  }
  return first_split;
}

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

bool lastSplitIsEmpty(const std::vector<std::shared_ptr<core::FlowFile>>& splits) {
  return splits.back()->getSize() != 0;
}
}  // namespace

void SplitContent::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Assert(!byte_sequence_.empty());
  const auto original = session.get();
  if (!original) {
    context.yield();
    return;
  }

  const auto ff_content_stream = session.getFlowFileContentStream(*original);
  if (!ff_content_stream) {
    throw Exception(PROCESSOR_EXCEPTION, fmt::format("Couldn't access the ContentStream of {}", original->getUUID().to_string()));
  }
  std::vector<std::byte> buffer{};
  buffer.reserve(BUFFER_TARGET_SIZE + byte_sequence_.size());
  size_t matching_bytes = 0;
  bool ended_with_byte_sequence = false;
  std::vector<std::shared_ptr<core::FlowFile>> splits{};
  splits.push_back(createFirstSplit(session));

  while (auto latest_byte = ff_content_stream->readByte()) {
    buffer.push_back(*latest_byte);
    if (ended_with_byte_sequence) {
      ended_with_byte_sequence = false;
      if (lastSplitIsEmpty(splits)) {
        splits.push_back(createNewSplit(session));
      }
    }
    if (latest_byte == byte_sequence_[matching_bytes]) {
      matching_bytes++;
      if (matching_bytes == byte_sequence_.size()) {
        // Found the Byte Sequence
        finalizeLatestSplitContent(session, splits.back(), buffer);
        ended_with_byte_sequence = true;
        matching_bytes = 0;
        buffer.clear();
      }
    } else {
      matching_bytes = 0;
      if (buffer.size() >= BUFFER_TARGET_SIZE) {
        session.appendBuffer(splits.back(), buffer);
        buffer.clear();
      }
    }
  }

  finalizeLastSplitContent(session, splits, buffer, ended_with_byte_sequence);

  updateSplitAttributesAndTransfer(session, splits, *original);
  session.transfer(original, Original);
}

REGISTER_RESOURCE(SplitContent, Processor);

}  // namespace org::apache::nifi::minifi::processors
