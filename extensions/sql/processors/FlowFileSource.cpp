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
