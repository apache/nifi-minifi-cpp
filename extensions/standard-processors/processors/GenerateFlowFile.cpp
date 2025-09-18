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
#include <vector>

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/OptionalUtils.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/ProcessorConfigUtils.h"
#include "minifi-cpp/core/ProcessorDescriptor.h"

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

GenerateFlowFile::Mode GenerateFlowFile::getMode(bool is_unique, bool is_text, bool has_custom_text, uint64_t file_size) {
  if (is_text && !is_unique && has_custom_text)
    return Mode::CustomText;

  if (file_size == 0)
    return Mode::Empty;

  if (is_unique) {
    if (is_text)
      return Mode::UniqueText;
    else
      return Mode::UniqueByte;
  } else {
    if (is_text)
      return Mode::NotUniqueText;
    else
      return Mode::NotUniqueByte;
  }
}

void GenerateFlowFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  bool is_text = context.getProperty(DataFormat)
      | utils::transform([](const std::string& data_format) { return data_format == DATA_FORMAT_TEXT;})
      | utils::valueOrElse([]() {return false;});
  bool is_unique = utils::parseOptionalBoolProperty(context, UniqueFlowFiles).value_or(true);

  const bool has_custom_text = context.hasNonEmptyProperty(CustomText.name);

  file_size_ = utils::parseDataSizeProperty(context, FileSize);
  batch_size_ = utils::parseU64Property(context, BatchSize);

  mode_ = getMode(is_unique, is_text, has_custom_text, file_size_);

  if (!isUnique(mode_)) {
    non_unique_data_.resize(gsl::narrow<size_t>(file_size_));
    generateData(non_unique_data_, isText(mode_));
  }

  logger_->log_trace("GenerateFlowFile is configured in {} mode", magic_enum::enum_name(mode_));
  if (mode_ != Mode::CustomText && has_custom_text)
    logger_->log_warn("Custom Text property is set but not used. "
      "For Custom Text to be used, Data Format needs to be Text, and Unique FlowFiles needs to be false.");
}

// The custom text has to be reevaluated once per batch
void GenerateFlowFile::refreshNonUniqueData(core::ProcessContext& context) {
  if (mode_ != Mode::CustomText)
    return;
  std::string custom_text = context.getProperty(CustomText).value_or("");
  non_unique_data_.assign(custom_text.begin(), custom_text.end());
}

void GenerateFlowFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  refreshNonUniqueData(context);
  for (uint64_t i = 0; i < batch_size_; i++) {
    std::shared_ptr<core::FlowFile> flow_file = session.create();
    if (!flow_file) {
      logger_->log_error("Failed to create flowfile!");
      return;
    }
    if (mode_ == Mode::Empty) {
      // the newly created flowfile is empty by default
    } else if (isUnique(mode_)) {
      std::vector<char> unique_data(gsl::narrow<size_t>(file_size_));
      generateData(unique_data, isText(mode_));
      session.writeBuffer(flow_file, unique_data);
    } else {
      session.writeBuffer(flow_file, non_unique_data_);
    }
    session.transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(GenerateFlowFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
