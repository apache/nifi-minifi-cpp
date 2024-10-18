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
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

constexpr size_t BUFFER_TARGET_SIZE = 1024;

void SegmentContent::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void SegmentContent::onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) {}

namespace {
void updateSplitAttributesAndTransfer(core::ProcessSession& session, const std::vector<std::shared_ptr<core::FlowFile>>& splits, const core::FlowFile& original) {
  const std::string fragment_identifier_ = original.getAttribute(core::SpecialFlowAttribute::UUID).value_or(utils::IdGenerator::getIdGenerator()->generate().to_string());
  const auto original_filename_ = original.getAttribute(core::SpecialFlowAttribute::FILENAME).value_or("");
  for (size_t split_i = 0; split_i < splits.size(); ++split_i) {
    const auto& split = splits[split_i];
    split->setAttribute(SegmentContent::FragmentCountOutputAttribute.name, std::to_string(splits.size()));
    split->setAttribute(SegmentContent::FragmentIndexOutputAttribute.name, std::to_string(split_i + 1));  // One based indexing
    split->setAttribute(SegmentContent::FragmentIdentifierOutputAttribute.name, fragment_identifier_);
    split->setAttribute(SegmentContent::SegmentOriginalFilenameOutputAttribute.name, original_filename_);
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
  if (!segment_size_str || !core::DataSizeValue::StringToInt(*segment_size_str, max_segment_size) || max_segment_size == 0) {
    throw Exception(PROCESSOR_EXCEPTION, fmt::format("Invalid Segment Size: '{}'", segment_size_str.value_or("")));
  }

  const auto ff_content_stream = session.getFlowFileContentStream(*original);
  if (!ff_content_stream) {
    throw Exception(PROCESSOR_EXCEPTION, fmt::format("Couldn't access the ContentStream of {}", original->getUUID().to_string()));
  }

  std::vector<std::byte> buffer;
  std::vector<std::shared_ptr<core::FlowFile>> segments{};

  size_t current_segment_size = 0;
  size_t num_bytes_read{};
  bool needs_new_segment = true;
  while (true) {
    const size_t segment_remaining_size = max_segment_size - current_segment_size;
    const size_t buffer_size = std::min(BUFFER_TARGET_SIZE, segment_remaining_size);
    buffer.resize(buffer_size);
    num_bytes_read = ff_content_stream->read(buffer);
    if (io::isError(num_bytes_read)) {
      logger_->log_error("Error while reading from {}", original->getUUID().to_string());
      break;
    }
    if (num_bytes_read == 0) {  // No more data
      break;
    }
    if (needs_new_segment) {
      segments.push_back(session.create());
      needs_new_segment = false;
    }
    buffer.resize(num_bytes_read);
    session.appendBuffer(segments.back(), buffer);
    current_segment_size += num_bytes_read;
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
