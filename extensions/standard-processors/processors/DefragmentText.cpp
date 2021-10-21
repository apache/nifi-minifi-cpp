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

#include "core/Resource.h"
#include "serialization/PayloadSerializer.h"
#include "TextFragmentUtils.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

const core::Relationship DefragmentText::Success("success", "Flowfiles that have no fragmented messages in them");
const core::Relationship DefragmentText::Failure("failure", "Flowfiles that failed the defragmentation process");
const core::Relationship DefragmentText::Self("__self__", "Marks the FlowFile to be owned by this processor");

const core::Property DefragmentText::Pattern(
    core::PropertyBuilder::createProperty("Pattern")
        ->withDescription("A regular expression to match at the start or end of messages.")
        ->isRequired(true)->build());

const core::Property DefragmentText::PatternLoc(
    core::PropertyBuilder::createProperty("Pattern Location")->withDescription("Whether the pattern is located at the start or at the end of the messages.")
        ->withAllowableValues(PatternLocation::values())
        ->withDefaultValue(toString(PatternLocation::START_OF_MESSAGE))->build());


const core::Property DefragmentText::MaxBufferSize(
    core::PropertyBuilder::createProperty("Max Buffer Size")
        ->withDescription("The maximum buffer size, if the buffer exceeds this, it will be transferred to failure. Expected format is <size> <data unit>")
        ->withType(core::StandardValidators::get().DATA_SIZE_VALIDATOR)->build());

const core::Property DefragmentText::MaxBufferAge(
    core::PropertyBuilder::createProperty("Max Buffer Age")->
        withDescription("The maximum age of a buffer after which the buffer will be transferred to failure. Expected format is <duration> <time unit>")->build());

void DefragmentText::initialize() {
  setSupportedRelationships({Success, Failure});
  setSupportedProperties({Pattern, PatternLoc, MaxBufferAge, MaxBufferSize});
}

void DefragmentText::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory*) {
  gsl_Expects(context);

  std::string max_buffer_age_str;
  if (context->getProperty(MaxBufferAge.getName(), max_buffer_age_str)) {
    core::TimeUnit unit;
    uint64_t max_buffer_age;
    if (core::Property::StringToTime(max_buffer_age_str, max_buffer_age, unit) && core::Property::ConvertTimeUnitToMS(max_buffer_age, unit, max_buffer_age)) {
      buffer_.setMaxAge(max_buffer_age);
      logger_->log_trace("The Buffer maximum age is configured to be %" PRIu64 " ms", max_buffer_age);
    }
  }

  std::string max_buffer_size_str;
  if (context->getProperty(MaxBufferSize.getName(), max_buffer_size_str)) {
    uint64_t max_buffer_size = core::DataSizeValue(max_buffer_size_str).getValue();
    if (max_buffer_size > 0) {
      buffer_.setMaxSize(max_buffer_size);
      logger_->log_trace("The Buffer maximum size is configured to be %" PRIu64 " B", max_buffer_size);
    }
  }

  context->getProperty(PatternLoc.getName(), pattern_location_);

  std::string pattern_str;
  if (context->getProperty(Pattern.getName(), pattern_str) && pattern_str != "") {
    pattern_ = std::regex(pattern_str);
    logger_->log_trace("The Pattern is configured to be %s", pattern_str);
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Pattern property missing or invalid");
  }
}

void DefragmentText::onTrigger(core::ProcessContext*, core::ProcessSession* session) {
  gsl_Expects(session);
  std::lock_guard<std::mutex> defrag_lock(defrag_mutex_);
  auto flowFiles = flow_file_store_.getNewFlowFiles();
  for (auto& file : flowFiles) {
    processNextFragment(session, file);
  }
  std::shared_ptr<core::FlowFile> original_flow_file = session->get();
  processNextFragment(session, original_flow_file);
  if (buffer_.maxAgeReached() || buffer_.maxSizeReached()) {
    buffer_.flushAndReplace(session, Failure, nullptr);
  }
}

void DefragmentText::processNextFragment(core::ProcessSession *session, const std::shared_ptr<core::FlowFile>& next_fragment) {
  if (!next_fragment)
    return;
  if (!buffer_.isCompatible(next_fragment)) {
    buffer_.flushAndReplace(session, Failure, nullptr);
    session->transfer(next_fragment, Failure);
    return;
  }
  std::shared_ptr<core::FlowFile> split_before_last_pattern;
  std::shared_ptr<core::FlowFile> split_after_last_pattern;
  bool found_pattern = splitFlowFileAtLastPattern(session, next_fragment, split_before_last_pattern,
                                                  split_after_last_pattern);
  buffer_.append(session, split_before_last_pattern);
  if (found_pattern) {
    buffer_.flushAndReplace(session, Success, split_after_last_pattern);
  }
  session->remove(next_fragment);
}


void DefragmentText::updateAttributesForSplitFiles(const std::shared_ptr<const core::FlowFile> &original_flow_file,
                                                   const std::shared_ptr<core::FlowFile> &split_before_last_pattern,
                                                   const std::shared_ptr<core::FlowFile> &split_after_last_pattern,
                                                   const size_t split_position) const {
  std::string base_name, post_name, offset_str;
  if (!original_flow_file->getAttribute(textfragmentutils::BASE_NAME_ATTRIBUTE, base_name))
    return;
  if (!original_flow_file->getAttribute(textfragmentutils::POST_NAME_ATTRIBUTE, post_name))
    return;
  if (!original_flow_file->getAttribute(textfragmentutils::OFFSET_ATTRIBUTE, offset_str))
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
class AppendFlowFileToFlowFile : public OutputStreamCallback {
 public:
  explicit AppendFlowFileToFlowFile(const std::shared_ptr<core::FlowFile>& flow_file_to_append, PayloadSerializer& serializer)
      : flow_file_to_append_(flow_file_to_append), serializer_(serializer) {}

  int64_t process(const std::shared_ptr<io::BaseStream> &stream) override {
    return serializer_.serialize(flow_file_to_append_, stream);
  }
 private:
  const std::shared_ptr<core::FlowFile>& flow_file_to_append_;
  PayloadSerializer& serializer_;
};

void updateAppendedAttributes(const std::shared_ptr<core::FlowFile>& buffered_ff) {
  std::string base_name, post_name, offset_str;
  if (!buffered_ff->getAttribute(textfragmentutils::BASE_NAME_ATTRIBUTE, base_name))
    return;
  if (!buffered_ff->getAttribute(textfragmentutils::POST_NAME_ATTRIBUTE, post_name))
    return;
  if (!buffered_ff->getAttribute(textfragmentutils::OFFSET_ATTRIBUTE, offset_str))
    return;
  size_t fragment_offset = std::stoi(offset_str);

  std::string buffer_new_name = textfragmentutils::createFileName(base_name, post_name, fragment_offset, buffered_ff->getSize());
  buffered_ff->setAttribute(core::SpecialFlowAttribute::FILENAME, buffer_new_name);
}

class LastPatternFinder : public InputStreamCallback {
 public:
  LastPatternFinder(const std::regex &pattern, DefragmentText::PatternLocation pattern_location) : pattern_(pattern), pattern_location_(pattern_location) {}

  ~LastPatternFinder() override = default;

  int64_t process(const std::shared_ptr<io::BaseStream> &stream) override {
    if (nullptr == stream)
      return 0;
    std::string content;
    content.resize(stream->size());
    const auto ret = stream->read(reinterpret_cast<uint8_t *>(content.data()), stream->size());
    if (io::isError(ret))
      return -1;
    searchContent(content);

    return 0;
  }

  const std::optional<size_t> &getLastPatternPosition() const { return last_pattern_location; }

 protected:
  void searchContent(const std::string &content) {
    auto matches_begin = std::sregex_iterator(content.begin(), content.end(), pattern_);
    auto number_of_matches = std::distance(matches_begin, std::sregex_iterator());
    if (number_of_matches > 0) {
      auto last_match = std::next(matches_begin, number_of_matches - 1);
      last_pattern_location = last_match->position(0);
      if (pattern_location_ == DefragmentText::PatternLocation::END_OF_MESSAGE)
        last_pattern_location.value() += last_match->length(0);
    } else {
      last_pattern_location = std::nullopt;
    }
  }

  const std::regex &pattern_;
  DefragmentText::PatternLocation pattern_location_;
  std::optional<size_t> last_pattern_location;
};
}  // namespace

bool DefragmentText::splitFlowFileAtLastPattern(core::ProcessSession *session,
                                                const std::shared_ptr<core::FlowFile> &original_flow_file,
                                                std::shared_ptr<core::FlowFile> &split_before_last_pattern,
                                                std::shared_ptr<core::FlowFile> &split_after_last_pattern) const {
  LastPatternFinder find_last_pattern(pattern_, pattern_location_);
  session->read(original_flow_file, &find_last_pattern);
  if (auto split_position = find_last_pattern.getLastPatternPosition()) {
    if (*split_position != 0) {
      split_before_last_pattern = session->clone(original_flow_file, 0, *split_position);
    }
    if (*split_position != original_flow_file->getSize()) {
      split_after_last_pattern = session->clone(original_flow_file, *split_position, original_flow_file->getSize() - *split_position);
    }
    updateAttributesForSplitFiles(original_flow_file, split_before_last_pattern, split_after_last_pattern, *split_position);
    return true;
  } else {
    split_before_last_pattern = session->clone(original_flow_file);
    split_after_last_pattern = nullptr;
    return false;
  }
}

void DefragmentText::restore(const std::shared_ptr<core::FlowFile>& flowFile) {
  if (!flowFile)
    return;
  flow_file_store_.put(flowFile);
}

std::set<std::shared_ptr<core::Connectable>> DefragmentText::getOutGoingConnections(const std::string &relationship) const {
  auto result = core::Connectable::getOutGoingConnections(relationship);
  if (relationship == Self.getName()) {
    result.insert(std::static_pointer_cast<core::Connectable>(std::const_pointer_cast<core::Processor>(shared_from_this())));
  }
  return result;
}

void DefragmentText::Buffer::append(core::ProcessSession* session, const std::shared_ptr<core::FlowFile>& flow_file_to_append) {
  if (!flow_file_to_append)
    return;
  if (empty()) {
    store(session, flow_file_to_append);
    return;
  }
  auto flowFileReader = [&] (const std::shared_ptr<core::FlowFile>& ff, InputStreamCallback* cb) {
    return session->read(ff, cb);
  };
  PayloadSerializer serializer(flowFileReader);
  AppendFlowFileToFlowFile append_flow_file_to_flow_file(flow_file_to_append, serializer);
  session->add(buffered_flow_file_);
  session->append(buffered_flow_file_, &append_flow_file_to_flow_file);
  updateAppendedAttributes(buffered_flow_file_);
  session->transfer(buffered_flow_file_, Self);

  session->remove(flow_file_to_append);
}

bool DefragmentText::Buffer::maxSizeReached() const {
  return !empty()
      && max_size_.has_value()
      && (max_size_.value() < buffered_flow_file_->getSize());
}

bool DefragmentText::Buffer::maxAgeReached() const {
  return !empty()
      && max_age_.has_value()
      && (creation_time_ + max_age_.value() < std::chrono::system_clock::now());
}

void DefragmentText::Buffer::setMaxAge(uint64_t max_age) {
  max_age_ = std::chrono::milliseconds(max_age);
}

void DefragmentText::Buffer::setMaxSize(size_t max_size) {
  max_size_ = max_size;
}

void DefragmentText::Buffer::flushAndReplace(core::ProcessSession* session, const core::Relationship& relationship,
                                             const std::shared_ptr<core::FlowFile>& new_buffered_flow_file) {
  if (!empty()) {
    session->add(buffered_flow_file_);
    session->transfer(buffered_flow_file_, relationship);
  }
  store(session, new_buffered_flow_file);
}

void DefragmentText::Buffer::store(core::ProcessSession* session, const std::shared_ptr<core::FlowFile>& new_buffered_flow_file) {
  buffered_flow_file_ = new_buffered_flow_file;
  creation_time_ = std::chrono::system_clock::now();
  if (!empty()) {
    session->add(buffered_flow_file_);
    session->transfer(buffered_flow_file_, Self);
  }
}

bool DefragmentText::Buffer::isCompatible(const std::shared_ptr<core::FlowFile> &flow_file_to_append) const {
  if (empty() || flow_file_to_append == nullptr)
    return true;
  if (buffered_flow_file_->getAttribute(textfragmentutils::BASE_NAME_ATTRIBUTE)
      != flow_file_to_append->getAttribute(textfragmentutils::BASE_NAME_ATTRIBUTE)) {
    return false;
  }
  if (buffered_flow_file_->getAttribute(textfragmentutils::POST_NAME_ATTRIBUTE)
      != flow_file_to_append->getAttribute(textfragmentutils::POST_NAME_ATTRIBUTE)) {
    return false;
  }
  std::string current_offset_str, append_offset_str;
  if (buffered_flow_file_->getAttribute(textfragmentutils::OFFSET_ATTRIBUTE, current_offset_str)
      != flow_file_to_append->getAttribute(textfragmentutils::OFFSET_ATTRIBUTE, append_offset_str)) {
    return false;
  }
  if (current_offset_str != "" && append_offset_str != "") {
    size_t current_offset = std::stoi(current_offset_str);
    size_t append_offset = std::stoi(append_offset_str);
    if (current_offset + buffered_flow_file_->getSize() != append_offset)
      return false;
  }
  return true;
}

REGISTER_RESOURCE(DefragmentText, "DefragmentText splits and merges incoming flowfiles so cohesive messages are not split between them");


}  // namespace org::apache::nifi::minifi::processors
