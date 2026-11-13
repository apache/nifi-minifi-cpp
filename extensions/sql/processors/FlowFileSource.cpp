// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "FlowFileSource.h"

namespace org::apache::nifi::minifi::processors {

void FlowFileSource::FlowFileGenerator::endProcessBatch() {
  if (current_batch_size_ == 0) {
    // do not create flow files with no rows
    return;
  }

  auto new_flow = session_.create();
  new_flow->addAttribute(std::string{FRAGMENT_INDEX}, std::to_string(flow_files_.size()));
  new_flow->addAttribute(std::string{FRAGMENT_IDENTIFIER}, batch_id_.to_string());
  session_.writeBuffer(new_flow, json_writer_.toString());
  flow_files_.push_back(std::move(new_flow));
}

void FlowFileSource::FlowFileGenerator::finishProcessing() {
  // annotate the flow files with the fragment.count
  std::string fragment_count = std::to_string(flow_files_.size());
  for (const auto& flow_file : flow_files_) {
    flow_file->addAttribute(std::string{FRAGMENT_COUNT}, fragment_count);
  }
}

}  // namespace org::apache::nifi::minifi::processors
