/**
 * @file SplitText.cpp
 * SplitText class implementation
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
#include "SplitText.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "core/FlowFile.h"
#include "utils/gsl.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

namespace detail {

LineReader::LineReader(const std::shared_ptr<io::InputStream>& stream)
    : stream_(stream) {
  if (!stream_ || stream_->size() == 0) {
    state_ = StreamReadState::EndOfStream;
  }
}

uint8_t LineReader::getEndLineSize(size_t newline_index) {
  gsl_Expects(buffer_.size() > newline_index);
  if (buffer_[newline_index] != '\n') {
    return 0;
  }
  if (newline_index == 0 || buffer_[newline_index - 1] != '\r') {
    return 1;
  }
  return 2;
}

void LineReader::setLastLineInfoAttributes(uint8_t endline_size, const std::optional<std::string>& starts_with) {
  const uint64_t size_from_beginning_of_stream = (current_buffer_count_ - 1) * SPLIT_TEXT_BUFFER_SIZE + buffer_offset_;
  if (last_line_info_) {
    LineInfo previous_line_info = *last_line_info_;
    last_line_info_->offset = previous_line_info.offset + previous_line_info.size;
    last_line_info_->size = size_from_beginning_of_stream - previous_line_info.offset - previous_line_info.size;
    last_line_info_->endline_size = endline_size;
    last_line_info_->matches_starts_with = true;
  } else {
    last_line_info_ = LineInfo{.offset = 0, .size = read_size_ - last_read_size_ + buffer_offset_, .endline_size = endline_size, .matches_starts_with = true};
  }

  if (starts_with) {
    last_line_info_->matches_starts_with = last_line_info_->size >= starts_with->size() &&
      std::equal(starts_with->begin(), starts_with->end(), buffer_.begin() + last_line_info_->offset, buffer_.begin() + last_line_info_->offset + starts_with->size());
  }
}

bool LineReader::readNextBuffer() {
  buffer_offset_ = 0;
  last_read_size_ = (std::min)(gsl::narrow<size_t>(stream_->size() - read_size_), SPLIT_TEXT_BUFFER_SIZE);
  const auto read_ret = stream_->read(as_writable_bytes(std::span(buffer_).subspan(0, last_read_size_)));
  read_size_ += read_ret;
  if (io::isError(read_ret)) {
    state_ = StreamReadState::StreamReadError;
    return false;
  }
  ++current_buffer_count_;
  return true;
}

std::optional<LineReader::LineInfo> LineReader::finalizeLineInfo(uint8_t endline_size, const std::optional<std::string>& starts_with) {
  setLastLineInfoAttributes(endline_size, starts_with);
  if (last_line_info_->size == 0) {
    return std::nullopt;
  }
  return last_line_info_;
}

std::optional<LineReader::LineInfo> LineReader::readNextLine(const std::optional<std::string>& starts_with) {
  if (state_ != StreamReadState::Ok) {
    return std::nullopt;
  }

  const auto isLastReadProcessed = [this]() { return last_read_size_ <= buffer_offset_; };
  while (read_size_ < stream_->size() || !isLastReadProcessed()) {
    if (isLastReadProcessed() && !readNextBuffer()) {
      return std::nullopt;
    }

    for (auto i = buffer_offset_; i < last_read_size_; ++i) {
      if (buffer_[i] == '\n') {
        buffer_offset_ = i + 1;
        return finalizeLineInfo(getEndLineSize(i), starts_with);
      }
    }
    buffer_offset_ = last_read_size_;
  }

  state_ = StreamReadState::EndOfStream;
  return finalizeLineInfo(0, starts_with);
}

SplitTextFragmentGenerator::SplitTextFragmentGenerator(const std::shared_ptr<io::InputStream>& stream, const SplitTextConfiguration& split_text_config)
    : line_reader_(stream),
      split_text_config_(split_text_config) {
}

void SplitTextFragmentGenerator::finalizeFragmentOffset(Fragment& current_fragment) {
  current_fragment.fragment_offset = flow_file_offset_;
  flow_file_offset_ += current_fragment.fragment_size;
}

void SplitTextFragmentGenerator::addLineToFragment(Fragment& current_fragment, const LineReader::LineInfo& line) {
  if (line.endline_size == line.size) {  // if line consists only of endline characters, we need to append the fragment trim size
    current_fragment.endline_size += line.endline_size;
  } else {
    current_fragment.endline_size = line.endline_size;
  }
  current_fragment.text_line_count += line.endline_size == line.size ? 0 : 1;
  current_fragment.fragment_size += line.size;
}

bool SplitTextFragmentGenerator::lineSizeWouldExceedMaxFragmentSize(const LineReader::LineInfo& line, uint64_t fragment_size) const {
  return split_text_config_.maximum_fragment_size && fragment_size + line.size + header_fragment_size_ > split_text_config_.maximum_fragment_size.value();
}

nonstd::expected<SplitTextFragmentGenerator::Fragment, std::string> SplitTextFragmentGenerator::createHeaderFragmentUsingLineCount() {
  Fragment header_fragment;
  for (uint64_t i = 0; i < split_text_config_.header_line_count; ++i) {
    auto line = line_reader_.readNextLine();
    if (!line) {
      if (getState() == StreamReadState::EndOfStream) {
        return nonstd::make_unexpected("The flow file's line count is less than the specified header line count!");
      } else {
        return nonstd::make_unexpected("Error while reading flow file stream!");
      }
    }
    if (lineSizeWouldExceedMaxFragmentSize(*line, header_fragment.fragment_size)) {
      return nonstd::make_unexpected("Header line would exceed the maximum fragment size!");
    }

    addLineToFragment(header_fragment, *line);
  }

  flow_file_offset_ += header_fragment.fragment_size;
  header_fragment_size_ = header_fragment.fragment_size;
  return header_fragment;
}

nonstd::expected<SplitTextFragmentGenerator::Fragment, std::string> SplitTextFragmentGenerator::createHeaderFragmentUsingHeaderMarkerCharacters() {
  Fragment header_fragment;
  while (auto line = line_reader_.readNextLine(split_text_config_.header_line_marker_characters)) {
    if (line->size < split_text_config_.header_line_marker_characters->size() || !line->matches_starts_with) {
      buffered_line_info_ = line;
      break;
    }
    if (lineSizeWouldExceedMaxFragmentSize(*line, header_fragment.fragment_size)) {
      return nonstd::make_unexpected("Header line would exceed the maximum fragment size!");
    }

    addLineToFragment(header_fragment, *line);
  }

  flow_file_offset_ += header_fragment.fragment_size;
  header_fragment_size_ = header_fragment.fragment_size;
  return header_fragment;
}

nonstd::expected<SplitTextFragmentGenerator::Fragment, std::string> SplitTextFragmentGenerator::readHeaderFragment() {
  gsl_Expects(flow_file_offset_ == 0);
  if (split_text_config_.header_line_count == 0 && !split_text_config_.header_line_marker_characters) {
    return nonstd::make_unexpected("No header properties were set!");
  }

  if (split_text_config_.header_line_count > 0) {
    return createHeaderFragmentUsingLineCount();
  }

  return createHeaderFragmentUsingHeaderMarkerCharacters();
}

std::optional<SplitTextFragmentGenerator::Fragment> SplitTextFragmentGenerator::readNextFragment() {
  Fragment current_fragment;
  while (auto line = buffered_line_info_ ? buffered_line_info_ : line_reader_.readNextLine()) {
    buffered_line_info_.reset();
    if (lineSizeWouldExceedMaxFragmentSize(*line, current_fragment.fragment_size)) {
      if (current_fragment.processed_line_count == 0) {  // first fragment line would be bigger than maximum fragment size (we don't have any other line in the fragment yet)
        addLineToFragment(current_fragment, *line);
      } else {
        buffered_line_info_ = line;
      }

      finalizeFragmentOffset(current_fragment);
      return current_fragment;
    }

    ++current_fragment.processed_line_count;
    addLineToFragment(current_fragment, *line);
    if (split_text_config_.line_split_count == current_fragment.processed_line_count) {
      finalizeFragmentOffset(current_fragment);
      return current_fragment;
    }
  }

  if (current_fragment.fragment_size > 0) {
    finalizeFragmentOffset(current_fragment);
    return current_fragment;
  }
  return std::nullopt;
}

}  // namespace detail

void SplitText::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void SplitText::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  gsl_Expects(context);
  split_text_config_.line_split_count = utils::getRequiredPropertyOrThrow<uint64_t>(*context, LineSplitCount.name);
  logger_->log_debug("SplitText line split count: {}", split_text_config_.line_split_count);
  auto max_fragment_data_size_value = context->getProperty<core::DataSizeValue>(MaximumFragmentSize);
  if (max_fragment_data_size_value) {
    split_text_config_.maximum_fragment_size = max_fragment_data_size_value->getValue();
    logger_->log_debug("SplitText maximum fragment size: {}", split_text_config_.maximum_fragment_size.value());
  }
  if (split_text_config_.maximum_fragment_size && split_text_config_.maximum_fragment_size.value() == 0) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Maximum Fragment Size cannot be 0!");
  }
  if (split_text_config_.line_split_count == 0 && !split_text_config_.maximum_fragment_size) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Line Split Count is set to 0, but Maximum Fragment Size is not set!");
  }
  split_text_config_.header_line_count = utils::getRequiredPropertyOrThrow<uint64_t>(*context, HeaderLineCount.name);
  logger_->log_debug("SplitText header line count: {}", split_text_config_.header_line_count);
  split_text_config_.header_line_marker_characters = context->getProperty(HeaderLineMarkerCharacters);
  if (split_text_config_.header_line_marker_characters && split_text_config_.header_line_marker_characters->size() >= detail::SPLIT_TEXT_BUFFER_SIZE) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("SplitText header line marker characters length is larger than the maximum allowed: {}", detail::SPLIT_TEXT_BUFFER_SIZE));
  }
  if (split_text_config_.header_line_marker_characters) {
    logger_->log_debug("SplitText header line marker characters were set: {}", *split_text_config_.header_line_marker_characters);
  }
  split_text_config_.remove_trailing_new_lines = utils::getRequiredPropertyOrThrow<bool>(*context, RemoveTrailingNewlines.name);
  logger_->log_debug("SplitText should remove trailing new lines: {}", split_text_config_.remove_trailing_new_lines ? "true" : "false");
}

void SplitText::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  gsl_Expects(context && session);
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    return;
  }

  ReadCallback callback{flow_file, split_text_config_, session.get(), logger_};
  session->read(flow_file, std::ref(callback));
  if (callback.error) {
    logger_->log_error("Splitting flow file failed with error: {}", *callback.error);
    session->transfer(flow_file, Failure);
  } else {
    logger_->log_info("Splitting flow file '{}' (id: {}) resulted in {} fragments", flow_file->getName(), flow_file->getUUIDStr(), callback.results.size());
    for (const auto& res : callback.results) {
      res->setAttribute(SplitText::FragmentCountOutputAttribute.name, std::to_string(callback.results.size()));
      session->transfer(res, Splits);
    }
    session->transfer(flow_file, Original);
  }
}

void SplitText::ReadCallback::setAttributesOfDoneSegment(core::FlowFile& current_flow_file, uint64_t line_count) {
  const std::string original_filename_or_uuid = flow_file_->getAttribute(core::SpecialFlowAttribute::FILENAME).value_or(flow_file_->getUUIDStr());
  current_flow_file.setAttribute(core::SpecialFlowAttribute::FILENAME, original_filename_or_uuid + ".fragment." + fragment_identifier_ + "." + std::to_string(emitted_fragment_index_));
  current_flow_file.setAttribute(SplitText::TextLineCountOutputAttribute.name, std::to_string(line_count));
  current_flow_file.setAttribute(SplitText::FragmentSizeOutputAttribute.name, std::to_string(current_flow_file.getSize()));
  current_flow_file.setAttribute(SplitText::FragmentIdentifierOutputAttribute.name, fragment_identifier_);
  current_flow_file.setAttribute(SplitText::FragmentIndexOutputAttribute.name, std::to_string(emitted_fragment_index_));
  current_flow_file.setAttribute(SplitText::SegmentOriginalFilenameOutputAttribute.name, flow_file_->getAttribute(core::SpecialFlowAttribute::FILENAME).value_or(""));
  ++emitted_fragment_index_;
}

void SplitText::ReadCallback::createHeaderOnlyFragmentFlow(const detail::SplitTextFragmentGenerator::Fragment& header_fragment) {
  gsl_Expects(split_text_config_.remove_trailing_new_lines);  // This is only possible if the split fragment has no content and the endlines are trimmed
  auto header_only_flow = session_->clone(flow_file_, gsl::narrow<int64_t>(header_fragment.fragment_offset), gsl::narrow<int64_t>(header_fragment.fragment_size - header_fragment.endline_size));
  if (!header_only_flow) {
    logger_->log_error("Failed to clone header only fragment flow!");
    return;
  }
  logger_->log_debug("Creating a header only fragment with fragment index: {} fragment size: {}", emitted_fragment_index_, header_only_flow->getSize());
  setAttributesOfDoneSegment(*header_only_flow, 0);
  results.push_back(header_only_flow);
}

void SplitText::ReadCallback::mergeHeaderAndFragmentFlows(const std::shared_ptr<core::FlowFile>& header_flow, const detail::SplitTextFragmentGenerator::Fragment& fragment, size_t fragment_trim_size) {
  auto fragment_flow = session_->clone(flow_file_, gsl::narrow<int64_t>(fragment.fragment_offset), gsl::narrow<int64_t>(fragment.fragment_size - fragment_trim_size));
  if (!fragment_flow) {
    logger_->log_error("Failed to clone fragment flow!");
    return;
  }
  auto merged_flow = session_->clone(header_flow);  // clone header to copy attributes
  if (!merged_flow) {
    logger_->log_error("Failed to clone merged fragment flow!");
    return;
  }
  session_->write(merged_flow, [this, &fragment_flow, &header_flow](const std::shared_ptr<io::OutputStream>& output_stream) -> int64_t {
    session_->read(header_flow, [&output_stream](const std::shared_ptr<io::InputStream>& header_input_stream) -> int64_t {
      return internal::pipe(*header_input_stream, *output_stream);
    });
    return session_->read(fragment_flow, [&output_stream](const std::shared_ptr<io::InputStream>& fragment_input_stream) -> int64_t {
      return internal::pipe(*fragment_input_stream, *output_stream);
    });
  });
  logger_->log_debug("Creating fragment with header with fragment index: {} fragment size: {}", emitted_fragment_index_, merged_flow->getSize());
  setAttributesOfDoneSegment(*merged_flow, fragment.text_line_count);
  results.push_back(merged_flow);
  session_->remove(fragment_flow);
}

void SplitText::ReadCallback::createFragmentFlowWithoutHeader(const detail::SplitTextFragmentGenerator::Fragment& fragment, size_t fragment_trim_size) {
  auto fragment_flow = session_->clone(flow_file_, gsl::narrow<int64_t>(fragment.fragment_offset), gsl::narrow<int64_t>(fragment.fragment_size - fragment_trim_size));
  if (!fragment_flow) {
    logger_->log_error("Failed to clone fragment flow without header!");
    return;
  }
  logger_->log_debug("Creating fragment with header with fragment index: {} fragment size: {}", emitted_fragment_index_, fragment_flow->getSize());
  setAttributesOfDoneSegment(*fragment_flow, fragment.text_line_count);
  results.push_back(fragment_flow);
}

int64_t SplitText::ReadCallback::operator()(const std::shared_ptr<io::InputStream>& stream) {
  detail::SplitTextFragmentGenerator fragment_generator(stream, split_text_config_);
  nonstd::expected<detail::SplitTextFragmentGenerator::Fragment, std::string> header_fragment;
  std::shared_ptr<core::FlowFile> header_flow;  // cache header flow file to avoid cloning it for each fragment
  if (split_text_config_.header_line_count > 0 || split_text_config_.header_line_marker_characters) {
    header_fragment = fragment_generator.readHeaderFragment();
    if (!header_fragment) {
      error = header_fragment.error();
      return gsl::narrow<int64_t>(flow_file_->getSize());
    }
    header_flow = session_->clone(flow_file_, gsl::narrow<int64_t>(header_fragment->fragment_offset), gsl::narrow<int64_t>(header_fragment->fragment_size));
    if (!header_flow) {
      logger_->log_error("Failed to clone header flow!");
      return -1;
    }
  }

  while (auto fragment = fragment_generator.readNextFragment()) {
    size_t fragment_trim_size = split_text_config_.remove_trailing_new_lines ? fragment->endline_size : 0;
    if (header_flow) {
      if (fragment->fragment_size - fragment_trim_size == 0) {
        createHeaderOnlyFragmentFlow(*header_fragment);
      } else {
        mergeHeaderAndFragmentFlows(header_flow, *fragment, fragment_trim_size);
      }
    } else if (fragment->fragment_size - fragment_trim_size != 0) {
      createFragmentFlowWithoutHeader(*fragment, fragment_trim_size);
    }
  }
  if (header_flow) {
    session_->remove(header_flow);
  }
  return fragment_generator.getState() == detail::StreamReadState::EndOfStream ? gsl::narrow<int64_t>(flow_file_->getSize()) : -1;
}

SplitText::ReadCallback::ReadCallback(std::shared_ptr<core::FlowFile> flow_file, const SplitTextConfiguration& split_text_config,
  core::ProcessSession *session,  std::shared_ptr<core::logging::Logger> logger)
    : flow_file_(std::move(flow_file)),
      split_text_config_(split_text_config),
      session_(session),
      logger_(std::move(logger)) {
}

REGISTER_RESOURCE(SplitText, Processor);

}  // namespace org::apache::nifi::minifi::processors
