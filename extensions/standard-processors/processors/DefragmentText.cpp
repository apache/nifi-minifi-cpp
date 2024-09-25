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

#include "DefragmentText.h"

#include <vector>
#include <utility>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "serialization/PayloadSerializer.h"
#include "TextFragmentUtils.h"
#include "utils/gsl.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

const core::Relationship DefragmentText::Self("__self__", "Marks the FlowFile to be owned by this processor");

void DefragmentText::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void DefragmentText::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  if (auto max_buffer_age = context.getProperty(MaxBufferAge) | utils::andThen(&core::TimePeriodValue::fromString)) {
    max_age_ = max_buffer_age->getMilliseconds();
    setTriggerWhenEmpty(true);
    logger_->log_trace("The Buffer maximum age is configured to be {}", max_buffer_age->getMilliseconds());
  }

  auto max_buffer_size = context.getProperty<core::DataSizeValue>(MaxBufferSize);
  if (max_buffer_size.has_value() && max_buffer_size->getValue() > 0) {
    max_size_ = max_buffer_size->getValue();
    logger_->log_trace("The Buffer maximum size is configured to be {} B", max_buffer_size->getValue());
  }

  pattern_location_ = utils::parseEnumProperty<defragment_text::PatternLocation>(context, PatternLoc);

  std::string pattern_str;
  if (context.getProperty(Pattern, pattern_str) && !pattern_str.empty()) {
    pattern_ = utils::Regex(pattern_str);
    logger_->log_trace("The Pattern is configured to be {}", pattern_str);
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Pattern property missing or invalid");
  }
}

void DefragmentText::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  auto flowFiles = flow_file_store_.getNewFlowFiles();
  for (auto& file : flowFiles) {
    if (file)
      processNextFragment(session, gsl::not_null(file));
  }
  {
    std::shared_ptr<core::FlowFile> original_flow_file = session.get();
    if (original_flow_file)
      processNextFragment(session, gsl::not_null(std::move(original_flow_file)));
  }
  for (auto& [fragment_source_id, fragment_source] : fragment_sources_) {
    if (fragment_source.buffer.maxSizeReached(max_size_)) {
      fragment_source.buffer.flushAndReplace(session, Failure, nullptr);
    } else if (fragment_source.buffer.maxAgeReached(max_age_)) {
      fragment_source.buffer.flushAndReplace(session, pattern_location_ == defragment_text::PatternLocation::START_OF_MESSAGE ? Success : Failure, nullptr);
    }
  }
}

namespace {
std::optional<size_t> getFragmentOffset(const core::FlowFile& flow_file) {
  if (auto offset_attribute = flow_file.getAttribute(textfragmentutils::OFFSET_ATTRIBUTE)) {
    return std::stoi(*offset_attribute);
  }
  return std::nullopt;
}
}  // namespace

void DefragmentText::processNextFragment(core::ProcessSession& session, const gsl::not_null<std::shared_ptr<core::FlowFile>>& next_fragment) {
  auto fragment_source_id = FragmentSource::Id(*next_fragment);
  auto& fragment_source = fragment_sources_[fragment_source_id];
  auto& buffer = fragment_source.buffer;
  if (!buffer.empty() && buffer.getNextFragmentOffset() != getFragmentOffset(*next_fragment)) {
    buffer.flushAndReplace(session, Failure, nullptr);
    session.transfer(next_fragment, Failure);
    return;
  }
  std::shared_ptr<core::FlowFile> split_before_last_pattern;
  std::shared_ptr<core::FlowFile> split_after_last_pattern;
  bool found_pattern = splitFlowFileAtLastPattern(session, next_fragment, split_before_last_pattern, split_after_last_pattern);
  if (split_before_last_pattern)
    buffer.append(session, gsl::not_null(std::move(split_before_last_pattern)));
  if (found_pattern) {
    buffer.flushAndReplace(session, Success, split_after_last_pattern);
  }
  session.remove(next_fragment);
}


void DefragmentText::updateAttributesForSplitFiles(const core::FlowFile& original_flow_file,
                                                   const std::shared_ptr<core::FlowFile>& split_before_last_pattern,
                                                   const std::shared_ptr<core::FlowFile>& split_after_last_pattern,
                                                   const size_t split_position) {
  std::string base_name;
  std::string post_name;
  std::string offset_str;
  if (!original_flow_file.getAttribute(textfragmentutils::BASE_NAME_ATTRIBUTE, base_name))
    return;
  if (!original_flow_file.getAttribute(textfragmentutils::POST_NAME_ATTRIBUTE, post_name))
    return;
  if (!original_flow_file.getAttribute(textfragmentutils::OFFSET_ATTRIBUTE, offset_str))
    return;

  size_t fragment_offset = std::stoi(offset_str);

  if (split_before_last_pattern) {
    std::string first_part_name = textfragmentutils::createFileName(base_name, post_name, fragment_offset, split_before_last_pattern->getSize());
    split_before_last_pattern->setAttribute(core::SpecialFlowAttribute::FILENAME, first_part_name);
  }
  if (split_after_last_pattern) {
    std::string second_part_name = textfragmentutils::createFileName(base_name, post_name, fragment_offset + split_position, split_after_last_pattern->getSize());
    split_after_last_pattern->setAttribute(core::SpecialFlowAttribute::FILENAME, second_part_name);
    split_after_last_pattern->setAttribute(textfragmentutils::OFFSET_ATTRIBUTE, std::to_string(fragment_offset + split_position));
  }
}

namespace {
void updateAppendedAttributes(core::FlowFile& buffered_ff) {
  std::string base_name;
  std::string post_name;
  std::string offset_str;
  if (!buffered_ff.getAttribute(textfragmentutils::BASE_NAME_ATTRIBUTE, base_name))
    return;
  if (!buffered_ff.getAttribute(textfragmentutils::POST_NAME_ATTRIBUTE, post_name))
    return;
  if (!buffered_ff.getAttribute(textfragmentutils::OFFSET_ATTRIBUTE, offset_str))
    return;
  size_t fragment_offset = std::stoi(offset_str);

  std::string buffer_new_name = textfragmentutils::createFileName(base_name, post_name, fragment_offset, buffered_ff.getSize());
  buffered_ff.setAttribute(core::SpecialFlowAttribute::FILENAME, buffer_new_name);
}

size_t getSplitPosition(const utils::SMatch& last_match, defragment_text::PatternLocation pattern_location) {
  size_t split_position = last_match.position(0);
  if (pattern_location == defragment_text::PatternLocation::END_OF_MESSAGE) {
    split_position += last_match.length(0);
  }
  return split_position;
}

}  // namespace

bool DefragmentText::splitFlowFileAtLastPattern(core::ProcessSession& session,
                                                const gsl::not_null<std::shared_ptr<core::FlowFile>> &original_flow_file,
                                                std::shared_ptr<core::FlowFile> &split_before_last_pattern,
                                                std::shared_ptr<core::FlowFile> &split_after_last_pattern) const {
  const auto read_result = session.readBuffer(original_flow_file);
  auto last_regex_match = utils::getLastRegexMatch(to_string(read_result), pattern_);
  if (!last_regex_match.ready()) {
    split_before_last_pattern = session.clone(*original_flow_file);
    split_after_last_pattern = nullptr;
    return false;
  }
  auto split_position = getSplitPosition(last_regex_match, pattern_location_);
  if (split_position != 0) {
    split_before_last_pattern = session.clone(*original_flow_file, 0, split_position);
  }
  if (split_position != original_flow_file->getSize()) {
    split_after_last_pattern = session.clone(*original_flow_file, split_position, original_flow_file->getSize() - split_position);
  }
  updateAttributesForSplitFiles(*original_flow_file, split_before_last_pattern, split_after_last_pattern, split_position);
  return true;
}

void DefragmentText::restore(const std::shared_ptr<core::FlowFile>& flowFile) {
  if (!flowFile)
    return;
  flow_file_store_.put(flowFile);
}

std::set<core::Connectable*> DefragmentText::getOutGoingConnections(const std::string &relationship) {
  auto result = core::ConnectableImpl::getOutGoingConnections(relationship);
  if (relationship == Self.getName()) {
    result.insert(this);
  }
  return result;
}

void DefragmentText::Buffer::append(core::ProcessSession& session, const gsl::not_null<std::shared_ptr<core::FlowFile>>& flow_file_to_append) {
  if (empty()) {
    store(session, flow_file_to_append);
    return;
  }
  auto flowFileReader = [&] (const std::shared_ptr<core::FlowFile>& ff, const io::InputStreamCallback& cb) {
    return session.read(ff, cb);
  };
  PayloadSerializer serializer(flowFileReader);
  session.add(buffered_flow_file_);
  session.append(buffered_flow_file_, [&serializer, &flow_file_to_append](const auto& output_stream) -> int64_t {
    return serializer.serialize(flow_file_to_append, output_stream);
  });
  updateAppendedAttributes(*buffered_flow_file_);
  session.transfer(buffered_flow_file_, Self);

  session.remove(flow_file_to_append);
}

bool DefragmentText::Buffer::maxSizeReached(const std::optional<size_t> max_size) const {
  return !empty()
      && max_size.has_value()
      && (max_size.value() < buffered_flow_file_->getSize());
}

bool DefragmentText::Buffer::maxAgeReached(const std::optional<std::chrono::milliseconds> max_age) const {
  return !empty()
      && max_age.has_value()
      && (creation_time_ + max_age.value() < std::chrono::steady_clock::now());
}

void DefragmentText::Buffer::flushAndReplace(core::ProcessSession& session, const core::Relationship& relationship,
                                             const std::shared_ptr<core::FlowFile>& new_buffered_flow_file) {
  if (!empty()) {
    session.add(buffered_flow_file_);
    session.transfer(buffered_flow_file_, relationship);
  }
  store(session, new_buffered_flow_file);
}

void DefragmentText::Buffer::store(core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& new_buffered_flow_file) {
  buffered_flow_file_ = new_buffered_flow_file;
  creation_time_ = std::chrono::steady_clock::now();
  if (!empty()) {
    session.add(buffered_flow_file_);
    session.transfer(buffered_flow_file_, Self);
  }
}

std::optional<size_t> DefragmentText::Buffer::getNextFragmentOffset() const {
  if (empty())
    return std::nullopt;
  if (auto offset_attribute = buffered_flow_file_->getAttribute(textfragmentutils::OFFSET_ATTRIBUTE))
    return std::stoi(*offset_attribute) + buffered_flow_file_->getSize();
  return std::nullopt;
}

DefragmentText::FragmentSource::Id::Id(const core::FlowFile& flow_file) {
  if (auto absolute_path = flow_file.getAttribute(core::SpecialFlowAttribute::ABSOLUTE_PATH))
    absolute_path_ = *absolute_path;
}

size_t DefragmentText::FragmentSource::Id::hash::operator() (const Id& fragment_id) const {
  return std::hash<std::optional<std::string>>{}(fragment_id.absolute_path_);
}

REGISTER_RESOURCE(DefragmentText, Processor);


}  // namespace org::apache::nifi::minifi::processors
