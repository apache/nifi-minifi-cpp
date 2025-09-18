/**
 * @file SiteToSiteProvenanceReportingTask.cpp
 * SiteToSiteProvenanceReportingTask class implementation
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
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <sstream>
#include <functional>
#include <iostream>
#include <utility>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

#include "minifi-cpp/core/Repository.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"
#include "utils/TimeUtil.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/provenance/Provenance.h"
#include "FlowController.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::core::reporting {

const char *SiteToSiteProvenanceReportingTask::ProvenanceAppStr = "MiNiFi Flow";

void SiteToSiteProvenanceReportingTask::initialize() {
  RemoteProcessorGroupPort::initialize();
}

void setJsonStr(const std::string& key, const std::string& value, rapidjson::Value& parent, rapidjson::Document::AllocatorType& alloc) { // NOLINT
  rapidjson::Value keyVal;
  rapidjson::Value valueVal;
  const char* c_key = key.c_str();
  const char* c_val = value.c_str();

  keyVal.SetString(c_key, gsl::narrow<rapidjson::SizeType>(key.length()), alloc);
  valueVal.SetString(c_val, gsl::narrow<rapidjson::SizeType>(value.length()), alloc);

  parent.AddMember(keyVal, valueVal, alloc);
}

rapidjson::Value getStringValue(const std::string& value, rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value Val;
  Val.SetString(value.c_str(), gsl::narrow<rapidjson::SizeType>(value.length()), alloc);
  return Val;
}

template<size_t N>
rapidjson::Value getStringValue(const utils::SmallString<N>& value, rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value Val;
  Val.SetString(value.c_str(), gsl::narrow<rapidjson::SizeType>(value.length()), alloc);
  return Val;
}

void appendJsonStr(const std::string& value, rapidjson::Value& parent, rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value valueVal;
  valueVal.SetString(value.c_str(), gsl::narrow<rapidjson::SizeType>(value.length()), alloc);
  parent.PushBack(valueVal, alloc);
}

template<size_t N>
void appendJsonStr(const utils::SmallString<N>& value, rapidjson::Value& parent, rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value valueVal;
  valueVal.SetString(value.c_str(), gsl::narrow<rapidjson::SizeType>(value.length()), alloc);
  parent.PushBack(valueVal, alloc);
}

void SiteToSiteProvenanceReportingTask::getJsonReport(core::ProcessContext&, core::ProcessSession&,
                                                      std::vector<std::shared_ptr<core::SerializableComponent>> &records, std::string &report) {
  rapidjson::Document array(rapidjson::kArrayType);
  rapidjson::Document::AllocatorType &alloc = array.GetAllocator();

  for (const auto& sercomp : records) {
    std::shared_ptr<provenance::ProvenanceEventRecord> record = std::dynamic_pointer_cast<provenance::ProvenanceEventRecord>(sercomp);
    if (nullptr == record) {
      break;
    }

    rapidjson::Value recordJson(rapidjson::kObjectType);
    rapidjson::Value updatedAttributesJson(rapidjson::kObjectType);
    rapidjson::Value parentUuidJson(rapidjson::kArrayType);
    rapidjson::Value childUuidJson(rapidjson::kArrayType);

    recordJson.AddMember("timestampMillis", int64_t{std::chrono::duration_cast<std::chrono::milliseconds>(record->getEventTime().time_since_epoch()).count()}, alloc);
    recordJson.AddMember("durationMillis", int64_t{record->getEventDuration().count()}, alloc);
    recordJson.AddMember("lineageStart", int64_t{std::chrono::duration_cast<std::chrono::milliseconds>(record->getlineageStartDate().time_since_epoch()).count()}, alloc);
    recordJson.AddMember("entitySize", record->getFileSize(), alloc);
    recordJson.AddMember("entityOffset", record->getFileOffset(), alloc);

    recordJson.AddMember("entityType", "org.apache.nifi.flowfile.FlowFile", alloc);

    recordJson.AddMember("eventId", getStringValue(record->getEventId().to_string(), alloc), alloc);
    recordJson.AddMember("eventType", getStringValue(provenance::ProvenanceEventRecord::ProvenanceEventTypeStr[record->getEventType()], alloc), alloc);
    recordJson.AddMember("details", getStringValue(record->getDetails(), alloc), alloc);
    recordJson.AddMember("componentId", getStringValue(record->getComponentId(), alloc), alloc);
    recordJson.AddMember("componentType", getStringValue(record->getComponentType(), alloc), alloc);
    recordJson.AddMember("entityId", getStringValue(record->getFlowFileUuid().to_string(), alloc), alloc);
    recordJson.AddMember("transitUri", getStringValue(record->getTransitUri(), alloc), alloc);
    recordJson.AddMember("remoteIdentifier", getStringValue(record->getSourceSystemFlowFileIdentifier(), alloc), alloc);
    recordJson.AddMember("alternateIdentifier", getStringValue(record->getAlternateIdentifierUri(), alloc), alloc);

    for (const auto& attr : record->getAttributes()) {
      setJsonStr(attr.first, attr.second, updatedAttributesJson, alloc);
    }
    recordJson.AddMember("updatedAttributes", updatedAttributesJson, alloc);

    for (auto parentUUID : record->getParentUuids()) {
      appendJsonStr(parentUUID.to_string(), parentUuidJson, alloc);
    }
    recordJson.AddMember("parentIds", parentUuidJson, alloc);

    for (auto childUUID : record->getChildrenUuids()) {
      appendJsonStr(childUUID.to_string(), childUuidJson, alloc);
    }
    recordJson.AddMember("childIds", childUuidJson, alloc);

    rapidjson::Value applicationVal;
    applicationVal.SetString(ProvenanceAppStr, gsl::narrow<rapidjson::SizeType>(std::strlen(ProvenanceAppStr)));
    recordJson.AddMember("application", applicationVal, alloc);

    array.PushBack(recordJson, alloc);
  }

  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  array.Accept(writer);

  report = buffer.GetString();
}

void SiteToSiteProvenanceReportingTask::onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) {
}

void SiteToSiteProvenanceReportingTask::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_debug("SiteToSiteProvenanceReportingTask -- onTrigger");
  std::vector<std::shared_ptr<core::SerializableComponent>> records;
  logger_->log_debug("batch size {} records", batch_size_);
  size_t deserialized = batch_size_;
  std::shared_ptr<core::Repository> repo = context.getProvenanceRepository();
  if (!repo->getElements(records, deserialized) && deserialized == 0) {
    return;
  }
  logger_->log_debug("Captured {} records", deserialized);
  std::string jsonStr;
  this->getJsonReport(context, session, records, jsonStr);
  if (jsonStr.empty()) {
    return;
  }

  auto protocol_ = getNextProtocol(true);

  if (!protocol_) {
    context.yield();
    return;
  }

  try {
    std::map<std::string, std::string> attributes;
    if (!protocol_->transmitPayload(context, session, jsonStr, attributes)) {
      context.yield();
    }
  } catch (...) {
    // if transfer bytes failed, return instead of purge the provenance records
    return;
  }

  // we transfer the record, purge the record from DB
  repo->Delete(records);
  returnProtocol(context, std::move(protocol_));
}

}  // namespace org::apache::nifi::minifi::core::reporting
