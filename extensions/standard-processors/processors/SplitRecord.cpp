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
#include "SplitRecord.h"

#include "core/Resource.h"
#include "nonstd/expected.hpp"
#include "utils/GeneralUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

void SplitRecord::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  record_converter_ = core::RecordConverter{
    .record_set_reader = utils::parseControllerService<core::RecordSetReader>(context, RecordReader, getUUID()),
    .record_set_writer = utils::parseControllerService<core::RecordSetWriter>(context, RecordWriter, getUUID())
  };
}

nonstd::expected<std::size_t, std::string> SplitRecord::readRecordsPerSplit(core::ProcessContext& context, const core::FlowFile& original_flow_file) {
  return context.getProperty(RecordsPerSplit, &original_flow_file)
      | utils::andThen([](const auto& records_per_split_str) {
            return parsing::parseIntegralMinMax<std::size_t>(records_per_split_str, 1, std::numeric_limits<std::size_t>::max());
          })
      | utils::transformError([](std::error_code) -> std::string { return std::string{"Records Per Split should be set to a number larger than 0"}; });
}

void SplitRecord::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(record_converter_);
  const auto original_flow_file = session.get();
  if (!original_flow_file) {
    context.yield();
    return;
  }

  auto records_per_split = readRecordsPerSplit(context, *original_flow_file);
  if (!records_per_split) {
    logger_->log_error("Failed to read Records Per Split property: {}", records_per_split.error());
    session.transfer(original_flow_file, Failure);
    return;
  }

  nonstd::expected<core::RecordSet, std::error_code> record_set;
  session.read(original_flow_file, [this, &record_set](const std::shared_ptr<io::InputStream>& input_stream) {
    record_set = record_converter_->record_set_reader->read(*input_stream);
    return gsl::narrow<int64_t>(input_stream->size());
  });
  if (!record_set) {
    logger_->log_error("Failed to read record set from flow file: {}", record_set.error().message());
    session.transfer(original_flow_file, Failure);
    return;
  }

  std::size_t current_index = 0;
  const auto fragment_identifier = original_flow_file->getAttribute(core::SpecialFlowAttribute::UUID).value_or(utils::IdGenerator::getIdGenerator()->generate().to_string());
  std::size_t fragment_index = 0;
  const auto fragment_count = utils::intdiv_ceil(record_set->size(), records_per_split.value());
  while (current_index < record_set->size()) {
    auto split_flow_file = session.create(original_flow_file.get());
    if (!split_flow_file) {
      logger_->log_error("Failed to create a new flow file for record set");
      session.transfer(original_flow_file, Failure);
      return;
    }

    core::RecordSet slice_record_set;
    slice_record_set.reserve(*records_per_split);
    for (std::size_t i = 0; i < records_per_split.value() && current_index < record_set->size(); ++i, ++current_index) {
      slice_record_set.push_back(std::move(record_set->at(current_index)));
    }

    split_flow_file->setAttribute("record.count", std::to_string(slice_record_set.size()));
    split_flow_file->setAttribute("fragment.identifier", fragment_identifier);
    split_flow_file->setAttribute("fragment.index", std::to_string(fragment_index));
    split_flow_file->setAttribute("fragment.count", std::to_string(fragment_count));
    split_flow_file->setAttribute("segment.original.filename", original_flow_file->getAttribute("filename").value_or(""));

    record_converter_->record_set_writer->write(slice_record_set, split_flow_file, session);
    session.transfer(split_flow_file, Splits);
    ++fragment_index;
  }

  session.transfer(original_flow_file, Original);
}

REGISTER_RESOURCE(SplitRecord, Processor);

}  // namespace org::apache::nifi::minifi::processors
