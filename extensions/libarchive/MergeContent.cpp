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
#include <vector>
#include <set>
#include <queue>
#include <map>
#include <deque>
#include <utility>
#include <algorithm>
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property MergeContent::MergeStrategy("Merge Strategy", "Defragment or Bin-Packing Algorithm", MERGE_STRATEGY_DEFRAGMENT);
core::Property MergeContent::MergeFormat("Merge Format", "Merge Format", MERGE_FORMAT_CONCAT_VALUE);
core::Property MergeContent::CorrelationAttributeName("Correlation Attribute Name", "Correlation Attribute Name", "");
core::Property MergeContent::DelimiterStratgey("Delimiter Strategy", "Determines if Header, Footer, and Demarcator should point to files", DELIMITER_STRATEGY_FILENAME);
core::Property MergeContent::Header("Header File", "Filename specifying the header to use", "");
core::Property MergeContent::Footer("Footer File", "Filename specifying the footer to use", "");
core::Property MergeContent::Demarcator("Demarcator File", "Filename specifying the demarcator to use", "");
core::Property MergeContent::KeepPath("Keep Path", "If using the Zip or Tar Merge Format, specifies whether or not the FlowFiles' paths should be included in their entry", "false");
core::Relationship MergeContent::Merge("merged", "The FlowFile containing the merged content");
const char *BinaryConcatenationMerge::mimeType = "application/octet-stream";
const char *TarMerge::mimeType = "application/tar";
const char *ZipMerge::mimeType = "application/zip";

void MergeContent::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(MinSize);
  properties.insert(MaxSize);
  properties.insert(MinEntries);
  properties.insert(MaxEntries);
  properties.insert(MaxBinAge);
  properties.insert(MaxBinCount);
  properties.insert(MergeStrategy);
  properties.insert(MergeFormat);
  properties.insert(CorrelationAttributeName);
  properties.insert(DelimiterStratgey);
  properties.insert(Header);
  properties.insert(Footer);
  properties.insert(Demarcator);
  properties.insert(KeepPath);
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
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
  }
  return (contents);
}

void MergeContent::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  std::string value;
  BinFiles::onSchedule(context, sessionFactory);
  if (context->getProperty(MergeStrategy.getName(), value) && !value.empty()) {
    this->mergeStratgey_ = value;
  }
  value = "";
  if (context->getProperty(MergeFormat.getName(), value) && !value.empty()) {
    this->mergeFormat_ = value;
  }
  value = "";
  if (context->getProperty(CorrelationAttributeName.getName(), value) && !value.empty()) {
      this->correlationAttributeName_ = value;
  }
  value = "";
  if (context->getProperty(DelimiterStratgey.getName(), value) && !value.empty()) {
      this->delimiterStratgey_ = value;
  }
  value = "";
  if (context->getProperty(Header.getName(), value) && !value.empty()) {
      this->header_ = value;
  }
  value = "";
  if (context->getProperty(Footer.getName(), value) && !value.empty()) {
      this->footer_ = value;
  }
  value = "";
  if (context->getProperty(Demarcator.getName(), value) && !value.empty()) {
      this->demarcator_ = value;
  }
  value = "";
  if (context->getProperty(KeepPath.getName(), value) && !value.empty()) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, keepPath_);
  }
  if (mergeStratgey_ == MERGE_STRATEGY_DEFRAGMENT) {
    binManager_.setFileCount(FRAGMENT_COUNT_ATTRIBUTE);
  }
  logger_->log_debug("Merge Content: Strategy [%s] Format [%s] Correlation Attribute [%s] Delimiter [%s]", mergeStratgey_, mergeFormat_, correlationAttributeName_, delimiterStratgey_);
  logger_->log_debug("Merge Content: Footer [%s] Header [%s] Demarcator [%s] KeepPath [%d]", footer_, header_, demarcator_, keepPath_);
  if (delimiterStratgey_ == DELIMITER_STRATEGY_FILENAME) {
    if (!header_.empty()) {
      this->headerContent_ = readContent(header_);
    }
    if (!footer_.empty()) {
       this->footerContent_ = readContent(footer_);
    }
    if (!demarcator_.empty()) {
        this->demarcatorContent_ = readContent(demarcator_);
    }
  }
  if (delimiterStratgey_ == DELIMITER_STRATEGY_TEXT) {
    this->headerContent_ = header_;
    this->footerContent_ = footer_;
    this->demarcatorContent_ = demarcator_;
  }
}

std::string MergeContent::getGroupId(core::ProcessContext *context, std::shared_ptr<core::FlowFile> flow) {
  std::string groupId = "";
  std::string value;
  if (!correlationAttributeName_.empty()) {
    if (flow->getAttribute(correlationAttributeName_, value))
      groupId = value;
  }
  if (groupId.empty() && mergeStratgey_ == MERGE_STRATEGY_DEFRAGMENT) {
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
  if (mergeStratgey_ != MERGE_STRATEGY_DEFRAGMENT && mergeStratgey_ != MERGE_STRATEGY_BIN_PACK)
    return false;

  if (mergeStratgey_ == MERGE_STRATEGY_DEFRAGMENT) {
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

  std::unique_ptr<MergeBin> mergeBin;
  if (mergeFormat_ == MERGE_FORMAT_CONCAT_VALUE || mergeFormat_ == MERGE_FORMAT_TAR_VALUE
      || mergeFormat_ == MERGE_FORMAT_ZIP_VALUE) {
    if (mergeFormat_ == MERGE_FORMAT_CONCAT_VALUE)
      mergeBin = std::unique_ptr < MergeBin > (new BinaryConcatenationMerge());
    else if (mergeFormat_ == MERGE_FORMAT_TAR_VALUE)
      mergeBin = std::unique_ptr < MergeBin > (new TarMerge());
    else if (mergeFormat_ == MERGE_FORMAT_ZIP_VALUE)
          mergeBin = std::unique_ptr < MergeBin > (new ZipMerge());
    else
      return false;

    std::shared_ptr<core::FlowFile> mergeFlow;
    try {
      mergeFlow = mergeBin->merge(context, session, bin->getFlowFile(), this->headerContent_, this->footerContent_, this->demarcatorContent_);
    } catch (...) {
      logger_->log_error("Merge Content merge catch exception");
      return false;
    }
    session->putAttribute(mergeFlow, BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(bin->getSize()));
    // we successfully merge the flow
    session->transfer(mergeFlow, Merge);
    std::deque<std::shared_ptr<core::FlowFile>> &flows = bin->getFlowFile();
    for (auto flow : flows) {
      session->transfer(flow, Original);
    }
    logger_->log_info("Merge FlowFile record UUID %s, payload length %d", mergeFlow->getUUIDStr(), mergeFlow->getSize());
  } else {
    logger_->log_error("Merge format not supported %s", mergeFormat_);
    return false;
  }
  return true;
}

std::shared_ptr<core::FlowFile> BinaryConcatenationMerge::merge(core::ProcessContext *context, core::ProcessSession *session,
        std::deque<std::shared_ptr<core::FlowFile>> &flows, std::string &header, std::string &footer, std::string &demarcator) {
  std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast < FlowFileRecord > (session->create());
  BinaryConcatenationMerge::WriteCallback callback(header, footer, demarcator, flows, session);
  session->write(flowFile, &callback);
  session->putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), this->getMergedContentType());
  std::string fileName;
  if (flows.size() == 1) {
    flows.front()->getAttribute(FlowAttributeKey(FILENAME), fileName);
  } else {
    flows.front()->getAttribute(BinFiles::SEGMENT_ORIGINAL_FILENAME, fileName);
  }
  if (!fileName.empty())
    session->putAttribute(flowFile, FlowAttributeKey(FILENAME), fileName);
  return flowFile;
}

std::shared_ptr<core::FlowFile> TarMerge::merge(core::ProcessContext *context, core::ProcessSession *session, std::deque<std::shared_ptr<core::FlowFile>> &flows, std::string &header,
    std::string &footer, std::string &demarcator) {
  std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast < FlowFileRecord > (session->create());
  ArchiveMerge::WriteCallback callback(std::string(MERGE_FORMAT_TAR_VALUE), flows, session);
  session->write(flowFile, &callback);
  session->putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), this->getMergedContentType());
  std::string fileName;
  flowFile->getAttribute(FlowAttributeKey(FILENAME), fileName);
  if (flows.size() == 1) {
    flows.front()->getAttribute(FlowAttributeKey(FILENAME), fileName);
  } else {
    flows.front()->getAttribute(BinFiles::SEGMENT_ORIGINAL_FILENAME, fileName);
  }
  if (!fileName.empty()) {
    fileName += ".tar";
    session->putAttribute(flowFile, FlowAttributeKey(FILENAME), fileName);
  }
  return flowFile;
}

std::shared_ptr<core::FlowFile> ZipMerge::merge(core::ProcessContext *context, core::ProcessSession *session, std::deque<std::shared_ptr<core::FlowFile>> &flows, std::string &header,
    std::string &footer, std::string &demarcator) {
  std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast < FlowFileRecord > (session->create());
  ArchiveMerge::WriteCallback callback(std::string(MERGE_FORMAT_ZIP_VALUE), flows, session);
  session->write(flowFile, &callback);
  session->putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), this->getMergedContentType());
  std::string fileName;
  flowFile->getAttribute(FlowAttributeKey(FILENAME), fileName);
  if (flows.size() == 1) {
    flows.front()->getAttribute(FlowAttributeKey(FILENAME), fileName);
  } else {
    flows.front()->getAttribute(BinFiles::SEGMENT_ORIGINAL_FILENAME, fileName);
  }
  if (!fileName.empty()) {
    fileName += ".zip";
    session->putAttribute(flowFile, FlowAttributeKey(FILENAME), fileName);
  }
  return flowFile;
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
