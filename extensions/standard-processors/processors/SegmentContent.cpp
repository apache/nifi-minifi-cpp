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

#include "SegmentContent.h"

#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "range/v3/view/split.hpp"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

constexpr size_t BUFFER_TARGET_SIZE = 1024;

void SegmentContent::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void SegmentContent::onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) {}

namespace {
std::shared_ptr<core::FlowFile> createSegment(core::ProcessSession& session) {
  auto first_split = session.create();
  if (!first_split) { throw Exception(PROCESSOR_EXCEPTION, "Couldn't create FlowFile"); }
  return first_split;
}

void updateSplitAttributesAndTransfer(core::ProcessSession& session, const std::vector<std::shared_ptr<core::FlowFile>>& splits, const core::FlowFile& original) {
  const std::string fragment_identifier_ = utils::IdGenerator::getIdGenerator()->generate().to_string();
  for (size_t split_i = 0; split_i < splits.size(); ++split_i) {
    const auto& split = splits[split_i];
    split->setAttribute(SegmentContent::FragmentCountOutputAttribute.name, std::to_string(splits.size()));
    split->setAttribute(SegmentContent::FragmentIndexOutputAttribute.name, std::to_string(split_i + 1));  // One based indexing
    split->setAttribute(SegmentContent::FragmentIdentifierOutputAttribute.name, fragment_identifier_);
    split->setAttribute(SegmentContent::SegmentOriginalFilenameOutputAttribute.name, original.getAttribute(core::SpecialFlowAttribute::FILENAME).value_or(""));
    session.transfer(split, SegmentContent::Segments);
  }
}
}  // namespace

void SegmentContent::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  const auto original = session.get();
  if (!original) {
    context.yield();
    return;
  }

  size_t max_segment_size{};
  const auto segment_size_str = context.getProperty(SegmentSize, original.get());
  if (!segment_size_str || !core::DataSizeValue::StringToInt(*segment_size_str, max_segment_size)) {
    throw Exception(PROCESSOR_EXCEPTION, fmt::format("Invalid Segment Size {}", segment_size_str));
  }

  const auto ff_content_stream = session.getFlowFileContentStream(*original);
  if (!ff_content_stream) {
    throw Exception(PROCESSOR_EXCEPTION, fmt::format("Couldn't access the ContentStream of {}", original->getUUID().to_string()));
  }

  std::vector<std::byte> buffer;
  std::vector<std::shared_ptr<core::FlowFile>> segments{};

  size_t current_segment_size = 0;
  size_t ret{};
  bool needs_new_segment = true;
  while (true) {
    const size_t segment_remaining_size = max_segment_size - current_segment_size;
    const size_t buffer_size = std::min(BUFFER_TARGET_SIZE, segment_remaining_size);
    buffer.resize(buffer_size);
    ret = ff_content_stream->read(buffer);
    if (io::isError(ret)) {
      logger_->log_error("Error while reading from {}", original->getUUID().to_string());
      break;
    }
    if (ret == 0) {  // No more data
      break;
    }
    if (needs_new_segment) {
      segments.push_back(createSegment(session));
      needs_new_segment = false;
    }
    buffer.resize(ret);
    session.appendBuffer(segments.back(), buffer);
    current_segment_size += ret;
    if (current_segment_size >= max_segment_size) {  // Defensive >= (read shouldn't read larger than requested size)
      needs_new_segment = true;
      current_segment_size = 0;
    }
  };

  updateSplitAttributesAndTransfer(session, segments, *original);
  session.transfer(original, Original);
}

REGISTER_RESOURCE(SegmentContent, Processor);

}  // namespace org::apache::nifi::minifi::processors
