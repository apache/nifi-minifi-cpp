/**
 * @file MergeContent.cpp
 * MergeContent class implementation
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
#include "MergeContent.h"
#include <stdio.h>
#include <memory>
#include <string>
#include <set>
#include <map>
#include <deque>
#include <utility>
#include <algorithm>
#include <numeric>
#include "utils/TimeUtil.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "serialization/PayloadSerializer.h"
#include "serialization/FlowFileV3Serializer.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property MergeContent::MergeStrategy(
  core::PropertyBuilder::createProperty("Merge Strategy")
  ->withDescription("Defragment or Bin-Packing Algorithm")
  ->withAllowableValues<std::string>({merge_content_options::MERGE_STRATEGY_DEFRAGMENT, merge_content_options::MERGE_STRATEGY_BIN_PACK})
  ->withDefaultValue(merge_content_options::MERGE_STRATEGY_DEFRAGMENT)->build());
core::Property MergeContent::MergeFormat(
  core::PropertyBuilder::createProperty("Merge Format")
  ->withDescription("Merge Format")
  ->withAllowableValues<std::string>({
      merge_content_options::MERGE_FORMAT_CONCAT_VALUE,
      merge_content_options::MERGE_FORMAT_TAR_VALUE,
      merge_content_options::MERGE_FORMAT_ZIP_VALUE,
      merge_content_options::MERGE_FORMAT_FLOWFILE_STREAM_V3_VALUE})
  ->withDefaultValue(merge_content_options::MERGE_FORMAT_CONCAT_VALUE)->build());
core::Property MergeContent::CorrelationAttributeName("Correlation Attribute Name", "Correlation Attribute Name", "");
core::Property MergeContent::DelimiterStrategy(
  core::PropertyBuilder::createProperty("Delimiter Strategy")
  ->withDescription("Determines if Header, Footer, and Demarcator should point to files")
  ->withAllowableValues<std::string>({merge_content_options::DELIMITER_STRATEGY_FILENAME, merge_content_options::DELIMITER_STRATEGY_TEXT})
  ->withDefaultValue(merge_content_options::DELIMITER_STRATEGY_FILENAME)->build());
core::Property MergeContent::Header("Header File", "Filename specifying the header to use", "");
core::Property MergeContent::Footer("Footer File", "Filename specifying the footer to use", "");
core::Property MergeContent::Demarcator("Demarcator File", "Filename specifying the demarcator to use", "");
core::Property MergeContent::KeepPath(
  core::PropertyBuilder::createProperty("Keep Path")
  ->withDescription("If using the Zip or Tar Merge Format, specifies whether or not the FlowFiles' paths should be included in their entry")
  ->withDefaultValue(false)->build());
core::Property MergeContent::AttributeStrategy(
  core::PropertyBuilder::createProperty("Attribute Strategy")
  ->withDescription("Determines which FlowFile attributes should be added to the bundle. If 'Keep All Unique Attributes' is selected, "
                    "any attribute on any FlowFile that gets bundled will be kept unless its value conflicts with the value from another FlowFile "
                    "(in which case neither, or none, of the conflicting attributes will be kept). If 'Keep Only Common Attributes' is selected, "
                    "only the attributes that exist on all FlowFiles in the bundle, with the same value, will be preserved.")
  ->withAllowableValues<std::string>({merge_content_options::ATTRIBUTE_STRATEGY_KEEP_COMMON, merge_content_options::ATTRIBUTE_STRATEGY_KEEP_ALL_UNIQUE})
  ->withDefaultValue(merge_content_options::ATTRIBUTE_STRATEGY_KEEP_COMMON)->build());
core::Relationship MergeContent::Merge("merged", "The FlowFile containing the merged content");

void MergeContent::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(MinSize);
  properties.insert(MaxSize);
  properties.insert(MinEntries);
  properties.insert(MaxEntries);
  properties.insert(MaxBinAge);
  properties.insert(MaxBinCount);
  properties.insert(BatchSize);
  properties.insert(MergeStrategy);
  properties.insert(MergeFormat);
  properties.insert(CorrelationAttributeName);
  properties.insert(DelimiterStrategy);
  properties.insert(Header);
  properties.insert(Footer);
  properties.insert(Demarcator);
  properties.insert(KeepPath);
  properties.insert(AttributeStrategy);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Original);
  relationships.insert(Failure);
  relationships.insert(Merge);
  setSupportedRelationships(relationships);
}

std::string MergeContent::readContent(std::string path) {
  std::string contents;
  std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
  if (in) {
    in.seekg(0, std::ios::end);
    contents.resize(gsl::narrow<size_t>(in.tellg()));
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
  }
  return (contents);
}

void MergeContent::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  BinFiles::onSchedule(context, sessionFactory);

  context->getProperty(MergeStrategy.getName(), mergeStrategy_);
  context->getProperty(MergeFormat.getName(), mergeFormat_);
  context->getProperty(CorrelationAttributeName.getName(), correlationAttributeName_);
  context->getProperty(DelimiterStrategy.getName(), delimiterStrategy_);
  context->getProperty(Header.getName(), header_);
  context->getProperty(Footer.getName(), footer_);
  context->getProperty(Demarcator.getName(), demarcator_);
  context->getProperty(KeepPath.getName(), keepPath_);
  context->getProperty(AttributeStrategy.getName(), attributeStrategy_);

  validatePropertyOptions();

  if (mergeStrategy_ == merge_content_options::MERGE_STRATEGY_DEFRAGMENT) {
    binManager_.setFileCount(FRAGMENT_COUNT_ATTRIBUTE);
  }
  logger_->log_debug("Merge Content: Strategy [%s] Format [%s] Correlation Attribute [%s] Delimiter [%s]", mergeStrategy_, mergeFormat_, correlationAttributeName_, delimiterStrategy_);
  logger_->log_debug("Merge Content: Footer [%s] Header [%s] Demarcator [%s] KeepPath [%d]", footer_, header_, demarcator_, keepPath_);

  if (mergeFormat_ != merge_content_options::MERGE_FORMAT_CONCAT_VALUE) {
    if (!header_.empty()) {
      logger_->log_warn("Header property only works with the Binary Concatenation format, value [%s] is ignored", header_);
    }
    if (!footer_.empty()) {
      logger_->log_warn("Footer property only works with the Binary Concatenation format, value [%s] is ignored", footer_);
    }
    if (!demarcator_.empty()) {
      logger_->log_warn("Demarcator property only works with the Binary Concatenation format, value [%s] is ignored", demarcator_);
    }
  }

  if (delimiterStrategy_ == merge_content_options::DELIMITER_STRATEGY_FILENAME) {
    if (!header_.empty()) {
      headerContent_ = readContent(header_);
    }
    if (!footer_.empty()) {
       footerContent_ = readContent(footer_);
    }
    if (!demarcator_.empty()) {
        demarcatorContent_ = readContent(demarcator_);
    }
  }
  if (delimiterStrategy_ == merge_content_options::DELIMITER_STRATEGY_TEXT) {
    headerContent_ = header_;
    footerContent_ = footer_;
    demarcatorContent_ = demarcator_;
  }
}

void MergeContent::validatePropertyOptions() {
  if (mergeStrategy_ != merge_content_options::MERGE_STRATEGY_DEFRAGMENT &&
      mergeStrategy_ != merge_content_options::MERGE_STRATEGY_BIN_PACK) {
    logger_->log_error("Merge strategy not supported %s", mergeStrategy_);
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Invalid merge strategy: " + attributeStrategy_);
  }

  if (mergeFormat_ != merge_content_options::MERGE_FORMAT_CONCAT_VALUE &&
      mergeFormat_ != merge_content_options::MERGE_FORMAT_TAR_VALUE &&
      mergeFormat_ != merge_content_options::MERGE_FORMAT_ZIP_VALUE &&
      mergeFormat_ != merge_content_options::MERGE_FORMAT_FLOWFILE_STREAM_V3_VALUE) {
    logger_->log_error("Merge format not supported %s", mergeFormat_);
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Invalid merge format: " + mergeFormat_);
  }

  if (delimiterStrategy_ != merge_content_options::DELIMITER_STRATEGY_FILENAME &&
      delimiterStrategy_ != merge_content_options::DELIMITER_STRATEGY_TEXT) {
    logger_->log_error("Delimiter strategy not supported %s", delimiterStrategy_);
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Invalid delimiter strategy: " + delimiterStrategy_);
  }

  if (attributeStrategy_ != merge_content_options::ATTRIBUTE_STRATEGY_KEEP_COMMON &&
      attributeStrategy_ != merge_content_options::ATTRIBUTE_STRATEGY_KEEP_ALL_UNIQUE) {
    logger_->log_error("Attribute strategy not supported %s", attributeStrategy_);
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Invalid attribute strategy: " + attributeStrategy_);
  }
}

std::string MergeContent::getGroupId(core::ProcessContext*, std::shared_ptr<core::FlowFile> flow) {
  std::string groupId = "";
  std::string value;
  if (!correlationAttributeName_.empty()) {
    if (flow->getAttribute(correlationAttributeName_, value))
      groupId = value;
  }
  if (groupId.empty() && mergeStrategy_ == merge_content_options::MERGE_STRATEGY_DEFRAGMENT) {
    if (flow->getAttribute(FRAGMENT_ID_ATTRIBUTE, value))
      groupId = value;
  }
  return groupId;
}

bool MergeContent::checkDefragment(std::unique_ptr<Bin> &bin) {
  std::deque<std::shared_ptr<core::FlowFile>> &flows = bin->getFlowFile();
  if (!flows.empty()) {
    std::shared_ptr<core::FlowFile> front = flows.front();
    std::string fragId;
    if (!front->getAttribute(BinFiles::FRAGMENT_ID_ATTRIBUTE, fragId))
      return false;
    std::string fragCount;
    if (!front->getAttribute(BinFiles::FRAGMENT_COUNT_ATTRIBUTE, fragCount))
      return false;
    int fragCountInt;
    try {
      fragCountInt = std::stoi(fragCount);
    }
    catch (...) {
      return false;
    }
    for (auto flow : flows) {
      std::string value;
      if (!flow->getAttribute(BinFiles::FRAGMENT_ID_ATTRIBUTE, value))
          return false;
      if (value != fragId)
        return false;
      if (!flow->getAttribute(BinFiles::FRAGMENT_COUNT_ATTRIBUTE, value))
        return false;
      if (value != fragCount)
        return false;
      if (!flow->getAttribute(BinFiles::FRAGMENT_INDEX_ATTRIBUTE, value))
        return false;
      try {
        int index = std::stoi(value);
        if (index < 0 || index >= fragCountInt)
          return false;
      }
      catch (...) {
        return false;
      }
    }
  } else {
    return false;
  }

  return true;
}

void MergeContent::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  BinFiles::onTrigger(context, session);
}

bool MergeContent::processBin(core::ProcessContext *context, core::ProcessSession *session, std::unique_ptr<Bin> &bin) {
  if (mergeStrategy_ != merge_content_options::MERGE_STRATEGY_DEFRAGMENT && mergeStrategy_ != merge_content_options::MERGE_STRATEGY_BIN_PACK)
    return false;

  if (mergeStrategy_ == merge_content_options::MERGE_STRATEGY_DEFRAGMENT) {
    // check the flowfile fragment values
    if (!checkDefragment(bin)) {
      logger_->log_error("Merge Content check defgrament failed");
      return false;
    }
    // sort the flowfile fragment index
    std::deque<std::shared_ptr<core::FlowFile>> &flows = bin->getFlowFile();
    std::sort(flows.begin(), flows.end(), [] (const std::shared_ptr<core::FlowFile> &first, const std::shared_ptr<core::FlowFile> &second)
        {std::string value;
         first->getAttribute(BinFiles::FRAGMENT_INDEX_ATTRIBUTE, value);
         int indexFirst = std::stoi(value);
         second->getAttribute(BinFiles::FRAGMENT_INDEX_ATTRIBUTE, value);
         int indexSecond = std::stoi(value);
         if (indexSecond > indexFirst)
           return true;
         else
           return false;
        });
  }

  std::shared_ptr<core::FlowFile> merge_flow = std::static_pointer_cast<FlowFileRecord>(session->create());
  if (attributeStrategy_ == merge_content_options::ATTRIBUTE_STRATEGY_KEEP_COMMON) {
    KeepOnlyCommonAttributesMerger(bin->getFlowFile()).mergeAttributes(session, merge_flow);
  } else if (attributeStrategy_ == merge_content_options::ATTRIBUTE_STRATEGY_KEEP_ALL_UNIQUE) {
    KeepAllUniqueAttributesMerger(bin->getFlowFile()).mergeAttributes(session, merge_flow);
  } else {
    logger_->log_error("Attribute strategy not supported %s", attributeStrategy_);
    return false;
  }

  auto flowFileReader = [&] (const std::shared_ptr<core::FlowFile>& ff, InputStreamCallback* cb) {
    return session->read(ff, cb);
  };

  const char* mimeType;
  std::unique_ptr<MergeBin> mergeBin;
  std::unique_ptr<minifi::FlowFileSerializer> serializer = std::make_unique<PayloadSerializer>(flowFileReader);
  if (mergeFormat_ == merge_content_options::MERGE_FORMAT_CONCAT_VALUE) {
    mergeBin = std::make_unique<BinaryConcatenationMerge>(headerContent_, footerContent_, demarcatorContent_);
    mimeType = "application/octet-stream";
  } else if (mergeFormat_ == merge_content_options::MERGE_FORMAT_FLOWFILE_STREAM_V3_VALUE) {
    // disregard header, demarcator, footer
    mergeBin = std::make_unique<BinaryConcatenationMerge>("", "", "");
    serializer = std::make_unique<FlowFileV3Serializer>(flowFileReader);
    mimeType = "application/flowfile-v3";
  } else if (mergeFormat_ == merge_content_options::MERGE_FORMAT_TAR_VALUE) {
    mergeBin = std::make_unique<TarMerge>();
    mimeType = "application/tar";
  } else if (mergeFormat_ == merge_content_options::MERGE_FORMAT_ZIP_VALUE) {
    mergeBin = std::make_unique<ZipMerge>();
    mimeType = "application/zip";
  } else {
    logger_->log_error("Merge format not supported %s", mergeFormat_);
    return false;
  }

  std::shared_ptr<core::FlowFile> mergeFlow;
  try {
    mergeBin->merge(context, session, bin->getFlowFile(), *serializer, merge_flow);
    session->putAttribute(merge_flow, core::SpecialFlowAttribute::MIME_TYPE, mimeType);
  } catch (...) {
    logger_->log_error("Merge Content merge catch exception");
    return false;
  }
  session->putAttribute(merge_flow, BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(bin->getSize()));

  // we successfully merge the flow
  session->transfer(merge_flow, Merge);
  std::deque<std::shared_ptr<core::FlowFile>> &flows = bin->getFlowFile();
  for (auto flow : flows) {
    session->transfer(flow, Original);
  }
  logger_->log_info("Merge FlowFile record UUID %s, payload length %d", merge_flow->getUUIDStr(), merge_flow->getSize());

  return true;
}

BinaryConcatenationMerge::BinaryConcatenationMerge(const std::string &header, const std::string& footer, const std::string &demarcator)
  : header_(header),
    footer_(footer),
    demarcator_(demarcator) {}

void BinaryConcatenationMerge::merge(core::ProcessContext* /*context*/, core::ProcessSession *session,
    std::deque<std::shared_ptr<core::FlowFile>> &flows, FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile>& merge_flow) {
  BinaryConcatenationMerge::WriteCallback callback(header_, footer_, demarcator_, flows, serializer);
  session->write(merge_flow, &callback);
  std::string fileName;
  if (flows.size() == 1) {
    flows.front()->getAttribute(core::SpecialFlowAttribute::FILENAME, fileName);
  } else {
    flows.front()->getAttribute(BinFiles::SEGMENT_ORIGINAL_FILENAME, fileName);
  }
  if (!fileName.empty())
    session->putAttribute(merge_flow, core::SpecialFlowAttribute::FILENAME, fileName);
}

void TarMerge::merge(core::ProcessContext* /*context*/, core::ProcessSession *session,
    std::deque<std::shared_ptr<core::FlowFile>> &flows, FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile>& merge_flow) {
  ArchiveMerge::WriteCallback callback(std::string(merge_content_options::MERGE_FORMAT_TAR_VALUE), flows, serializer);
  session->write(merge_flow, &callback);
  std::string fileName;
  merge_flow->getAttribute(core::SpecialFlowAttribute::FILENAME, fileName);
  if (flows.size() == 1) {
    flows.front()->getAttribute(core::SpecialFlowAttribute::FILENAME, fileName);
  } else {
    flows.front()->getAttribute(BinFiles::SEGMENT_ORIGINAL_FILENAME, fileName);
  }
  if (!fileName.empty()) {
    fileName += ".tar";
    session->putAttribute(merge_flow, core::SpecialFlowAttribute::FILENAME, fileName);
  }
}

void ZipMerge::merge(core::ProcessContext* /*context*/, core::ProcessSession *session,
    std::deque<std::shared_ptr<core::FlowFile>> &flows, FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile>& merge_flow) {
  ArchiveMerge::WriteCallback callback(std::string(merge_content_options::MERGE_FORMAT_ZIP_VALUE), flows, serializer);
  session->write(merge_flow, &callback);
  std::string fileName;
  merge_flow->getAttribute(core::SpecialFlowAttribute::FILENAME, fileName);
  if (flows.size() == 1) {
    flows.front()->getAttribute(core::SpecialFlowAttribute::FILENAME, fileName);
  } else {
    flows.front()->getAttribute(BinFiles::SEGMENT_ORIGINAL_FILENAME, fileName);
  }
  if (!fileName.empty()) {
    fileName += ".zip";
    session->putAttribute(merge_flow, core::SpecialFlowAttribute::FILENAME, fileName);
  }
}

void AttributeMerger::mergeAttributes(core::ProcessSession *session, const std::shared_ptr<core::FlowFile> &merge_flow) {
  for (const auto& pair : getMergedAttributes()) {
    session->putAttribute(merge_flow, pair.first, pair.second);
  }
}

std::map<std::string, std::string> AttributeMerger::getMergedAttributes() {
  if (flows_.empty()) return {};
  std::map<std::string, std::string> sum{ flows_.front()->getAttributes() };
  const auto merge_attributes = [this](std::map<std::string, std::string>* const merged_attributes, const std::shared_ptr<core::FlowFile>& flow) {
    processFlowFile(flow, *merged_attributes);
    return merged_attributes;
  };
  return *std::accumulate(std::next(flows_.cbegin()), flows_.cend(), &sum, merge_attributes);
}

void KeepOnlyCommonAttributesMerger::processFlowFile(const std::shared_ptr<core::FlowFile> &flow_file, std::map<std::string, std::string> &merged_attributes) {
  auto flow_attributes = flow_file->getAttributes();
  std::map<std::string, std::string> tmp_merged;
  std::set_intersection(std::make_move_iterator(merged_attributes.begin()), std::make_move_iterator(merged_attributes.end()),
    std::make_move_iterator(flow_attributes.begin()), std::make_move_iterator(flow_attributes.end()), std::inserter(tmp_merged, tmp_merged.begin()));
  merged_attributes = std::move(tmp_merged);
}

void KeepAllUniqueAttributesMerger::processFlowFile(const std::shared_ptr<core::FlowFile> &flow_file, std::map<std::string, std::string> &merged_attributes) {
  auto flow_attributes = flow_file->getAttributes();
  for (auto&& attr : flow_attributes) {
    if (std::find(removed_attributes_.cbegin(), removed_attributes_.cend(), attr.first) != removed_attributes_.cend()) {
      continue;
    }
    std::map<std::string, std::string>::iterator insertion_res;
    bool insertion_happened;
    std::tie(insertion_res, insertion_happened) = merged_attributes.insert(attr);
    if (!insertion_happened && insertion_res->second != attr.second) {
      merged_attributes.erase(insertion_res);
      removed_attributes_.push_back(attr.first);
    }
  }
}

REGISTER_RESOURCE(MergeContent, "Merges a Group of FlowFiles together based on a user-defined strategy and packages them into a single FlowFile. "
    "MergeContent should be configured with only one incoming connection as it won't create grouped Flow Files."
    "This processor updates the mime.type attribute as appropriate.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
