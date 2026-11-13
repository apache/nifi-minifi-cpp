// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "ConvertRecord.h"

#include <expected>

#include "core/Resource.h"
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

  std::expected<core::RecordSet, std::error_code> record_set;
  session.read(flow_file, [this, &record_set](const std::shared_ptr<io::InputStream>& input_stream) -> io::IoResult {
    record_set = record_converter_->record_set_reader->read(*input_stream);
    return io::IoResult::from(input_stream->size());
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
