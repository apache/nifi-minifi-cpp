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
#include "ConvertRecord.h"

#include "core/Resource.h"
#include "nonstd/expected.hpp"
#include "utils/GeneralUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

void ConvertRecord::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  record_converter_ = core::RecordConverter{
    .record_set_reader = utils::parseControllerService<core::RecordSetReader>(context, RecordReader, getUUID()),
    .record_set_writer = utils::parseControllerService<core::RecordSetWriter>(context, RecordWriter, getUUID())
  };
  include_zero_record_flow_files_ = utils::parseBoolProperty(context, IncludeZeroRecordFlowFiles);
}

void ConvertRecord::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(record_converter_);
  const auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  nonstd::expected<core::RecordSet, std::error_code> record_set;
  session.read(flow_file, [this, &record_set](const std::shared_ptr<io::InputStream>& input_stream) {
    record_set = record_converter_->record_set_reader->read(*input_stream);
    return gsl::narrow<int64_t>(input_stream->size());
  });
  if (!record_set) {
    logger_->log_error("Failed to read record set from flow file: {}", record_set.error().message());
    flow_file->setAttribute(processors::ConvertRecord::RecordErrorMessageOutputAttribute.name, record_set.error().message());
    session.transfer(flow_file, Failure);
    return;
  }

  if (!include_zero_record_flow_files_ && record_set->empty()) {
    logger_->log_info("No records found in flow file, removing flow file");
    session.remove(flow_file);
    return;
  }

  record_converter_->record_set_writer->write(*record_set, flow_file, session);
  flow_file->setAttribute(processors::ConvertRecord::RecordCountOutputAttribute.name, std::to_string(record_set->size()));
  session.transfer(flow_file, Success);
}

REGISTER_RESOURCE(ConvertRecord, Processor);

}  // namespace org::apache::nifi::minifi::processors
