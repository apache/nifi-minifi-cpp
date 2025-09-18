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

#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ConfigurationUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::processors {
void SplitContent::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void SplitContent::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  buffer_size_ = utils::configuration::getBufferSize(*context.getConfiguration());
  auto byte_sequence_str = utils::parseProperty(context, ByteSequence);
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
  keep_byte_sequence = utils::parseBoolProperty(context, KeepByteSequence);
}

namespace {
class Splitter {
 public:
  explicit Splitter(core::ProcessSession& session, std::optional<std::string> original_filename, SplitContent::ByteSequenceMatcher& byte_sequence_matcher, const bool keep_byte_sequence,
      const SplitContent::ByteSequenceLocation byte_sequence_location, const size_t buffer_size)
      : session_(session),
        original_filename_(std::move(original_filename)),
        byte_sequence_matcher_(byte_sequence_matcher),
        keep_trailing_byte_sequence_(keep_byte_sequence && byte_sequence_location == SplitContent::ByteSequenceLocation::Trailing),
        keep_leading_byte_sequence_(keep_byte_sequence && byte_sequence_location == SplitContent::ByteSequenceLocation::Leading),
        buffer_size_(buffer_size) {
    data_before_byte_sequence_.reserve(buffer_size_);
  }

  Splitter(const Splitter&) = delete;
  Splitter& operator=(const Splitter&) = delete;
  Splitter(Splitter&&) = delete;
  Splitter& operator=(Splitter&&) = delete;

  ~Splitter() {
    flushRemainingData();
    updateSplitAttributesAndTransfer();
  }

  void digest(const std::byte b) {
    const auto prev_matching_bytes = matching_bytes_;
    matching_bytes_ = byte_sequence_matcher_.getNumberOfMatchingBytes(matching_bytes_, b);
    if (matchedByteSequence()) {
      appendDataBeforeByteSequenceToSplit();
      if (keep_trailing_byte_sequence_) { appendByteSequenceToSplit(); }
      closeCurrentSplit();

      // possible new split
      if (keep_leading_byte_sequence_) { appendByteSequenceToSplit(); }
      matching_bytes_ = 0;
      return;
    }
    // matching grew, no need to grow data_before_byte_sequence
    if (matching_bytes_ > prev_matching_bytes) { return; }

    if (matching_bytes_ > 0) {
      // last byte could be part of the byte_sequence
      std::copy_n(getByteSequence().begin(), prev_matching_bytes - matching_bytes_ + 1, std::back_inserter(data_before_byte_sequence_));
    } else {
      // last byte is not part of the byte_sequence
      std::copy_n(getByteSequence().begin(), prev_matching_bytes - matching_bytes_, std::back_inserter(data_before_byte_sequence_));
      data_before_byte_sequence_.push_back(b);
    }
  }

  void flushIfBufferTooLarge() {
    if (data_before_byte_sequence_.size() >= buffer_size_) { appendDataBeforeByteSequenceToSplit(); }
  }

 private:
  void closeCurrentSplit() {
    if (current_split_) {
      completed_splits_.push_back(current_split_);
      current_split_.reset();
    }
  }

  void appendDataBeforeByteSequenceToSplit() {
    if (data_before_byte_sequence_.empty()) { return; }
    ensureCurrentSplit();
    session_.appendBuffer(current_split_, std::span<const std::byte>(data_before_byte_sequence_.data(), data_before_byte_sequence_.size()));
    data_before_byte_sequence_.clear();
  }

  void appendByteSequenceToSplit() {
    ensureCurrentSplit();
    session_.appendBuffer(current_split_, getByteSequence());
  }

  [[nodiscard]] std::span<const std::byte> getByteSequence() const { return byte_sequence_matcher_.getByteSequence(); }

  void ensureCurrentSplit() {
    if (!current_split_) { current_split_ = session_.create(); }
  }

  [[nodiscard]] bool matchedByteSequence() const { return matching_bytes_ == byte_sequence_matcher_.getByteSequence().size(); }

  void flushRemainingData() {
    if (current_split_ || !data_before_byte_sequence_.empty() || matching_bytes_ > 0) {
      ensureCurrentSplit();
      session_.appendBuffer(current_split_, std::span<const std::byte>(data_before_byte_sequence_.data(), data_before_byte_sequence_.size()));
      session_.appendBuffer(current_split_, byte_sequence_matcher_.getByteSequence().subspan(0, matching_bytes_));
      completed_splits_.push_back(current_split_);
    }
  }

  void updateSplitAttributesAndTransfer() const {
    const std::string fragment_identifier_ = utils::IdGenerator::getIdGenerator()->generate().to_string();
    for (size_t split_i = 0; split_i < completed_splits_.size(); ++split_i) {
      const auto& split = completed_splits_[split_i];
      split->setAttribute(SplitContent::FragmentCountOutputAttribute.name, std::to_string(completed_splits_.size()));
      split->setAttribute(SplitContent::FragmentIndexOutputAttribute.name, std::to_string(split_i + 1));  // One based indexing
      split->setAttribute(SplitContent::FragmentIdentifierOutputAttribute.name, fragment_identifier_);
      split->setAttribute(SplitContent::SegmentOriginalFilenameOutputAttribute.name, original_filename_.value_or(""));
      session_.transfer(split, SplitContent::Splits);
    }
  }

  core::ProcessSession& session_;
  const std::optional<std::string> original_filename_;
  SplitContent::ByteSequenceMatcher& byte_sequence_matcher_;
  std::vector<std::byte> data_before_byte_sequence_;
  std::shared_ptr<core::FlowFile> current_split_ = nullptr;
  std::vector<std::shared_ptr<core::FlowFile>> completed_splits_;
  SplitContent::size_type matching_bytes_ = 0;
  const bool keep_trailing_byte_sequence_ = false;
  const bool keep_leading_byte_sequence_ = false;
  size_t buffer_size_{};
};
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

  Splitter splitter{session, original->getAttribute(core::SpecialFlowAttribute::FILENAME), *byte_sequence_matcher_, keep_byte_sequence, byte_sequence_location_, buffer_size_};

  while (auto latest_byte = ff_content_stream->readByte()) {
    splitter.digest(*latest_byte);
    splitter.flushIfBufferTooLarge();
  }

  session.transfer(original, Original);
}

REGISTER_RESOURCE(SplitContent, Processor);

}  // namespace org::apache::nifi::minifi::processors
