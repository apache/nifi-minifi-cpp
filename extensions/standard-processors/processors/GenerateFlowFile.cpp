/**
 * @file GenerateFlowFile.cpp
 * GenerateFlowFile class implementation
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

#include "GenerateFlowFile.h"

#include <limits>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "utils/gsl.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {
const char *GenerateFlowFile::DATA_FORMAT_TEXT = "Text";

constexpr const char * TEXT_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%^&*()-_=+/?.,';:\"?<>\n\t ";

void GenerateFlowFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void generateData(std::vector<char>& data, const bool text_data = false) {
  std::random_device rd;
  std::mt19937 eng(rd());
  if (text_data) {
    const int index_of_last_char = gsl::narrow<int>(strlen(TEXT_CHARS)) - 1;
    std::uniform_int_distribution<> distr(0, index_of_last_char);
    std::generate_n(data.begin(), data.size(), [&] { return TEXT_CHARS[static_cast<uint8_t>(distr(eng))]; });
  } else {
    std::uniform_int_distribution<> distr(std::numeric_limits<char>::min(), std::numeric_limits<char>::max());
    auto rand = [&distr, &eng] { return distr(eng); };
    std::generate_n(data.begin(), data.size(), rand);
  }
}

void GenerateFlowFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  if (context.getProperty(FileSize.name, file_size_)) {
    logger_->log_trace("File size is configured to be {}", file_size_);
  }

  if (context.getProperty(BatchSize.name, batch_size_)) {
    logger_->log_trace("Batch size is configured to be {}", batch_size_);
  }

  if (auto data_format = context.getProperty(DataFormat)) {
    textData_ = (*data_format == DATA_FORMAT_TEXT);
  }
  if (context.getProperty(UniqueFlowFiles.name, unique_flow_file_)) {
    logger_->log_trace("Unique Flow files is configured to be {}", unique_flow_file_);
  }

  std::string custom_text;
  context.getProperty(CustomText, custom_text, nullptr);
  if (!custom_text.empty()) {
    if (textData_ && !unique_flow_file_) {
      non_unique_data_.assign(custom_text.begin(), custom_text.end());
      return;
    }
    logger_->log_warn("Custom Text property is set, but not used!");
  }

  if (!unique_flow_file_) {
    non_unique_data_.resize(gsl::narrow<size_t>(file_size_));
    generateData(non_unique_data_, textData_);
  }
}

// If the Data Format is text and if Unique FlowFiles is false, the custom text has to be evaluated once per batch
void GenerateFlowFile::regenerateNonUniqueData(core::ProcessContext& context) {
  if (!textData_ || unique_flow_file_) return;

  std::string custom_text;
  context.getProperty(CustomText, custom_text, nullptr);
  non_unique_data_.assign(custom_text.begin(), custom_text.end());
}

void GenerateFlowFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  regenerateNonUniqueData(context);
  for (uint64_t i = 0; i < batch_size_; i++) {
    // For each batch
    std::shared_ptr<core::FlowFile> flow_file = session.create();
    if (!flow_file) {
      logger_->log_error("Failed to create flowfile!");
      return;
    }
    if (unique_flow_file_) {
      std::vector<char> data(gsl::narrow<size_t>(file_size_));
      if (file_size_ > 0) {
        generateData(data, textData_);
      }
      session.writeBuffer(flow_file, data);
    } else {
      session.writeBuffer(flow_file, non_unique_data_);
    }
    session.transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(GenerateFlowFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
