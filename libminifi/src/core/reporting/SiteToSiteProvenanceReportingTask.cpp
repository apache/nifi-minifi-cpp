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
#include "core/Repository.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"
#include "../include/io/StreamFactory.h"
#include "io/ClientSocket.h"
#include "utils/TimeUtil.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "provenance/Provenance.h"
#include "FlowController.h"

#include "json/json.h"
#include "json/writer.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace reporting {

const char *SiteToSiteProvenanceReportingTask::ProvenanceAppStr = "MiNiFi Flow";

void SiteToSiteProvenanceReportingTask::initialize() {
  RemoteProcessorGroupPort::initialize();
}

void SiteToSiteProvenanceReportingTask::getJsonReport(core::ProcessContext *context, core::ProcessSession *session, std::vector<std::shared_ptr<core::SerializableComponent>> &records,
                                                      std::string &report) {
  Json::Value array;
  for (auto sercomp : records) {
    std::shared_ptr<provenance::ProvenanceEventRecord> record = std::dynamic_pointer_cast < provenance::ProvenanceEventRecord > (sercomp);
    if (nullptr == record) {
      break;
    }
    Json::Value recordJson;
    Json::Value updatedAttributesJson;
    Json::Value parentUuidJson;
    Json::Value childUuidJson;
    recordJson["eventId"] = record->getEventId().c_str();
    recordJson["eventType"] = provenance::ProvenanceEventRecord::ProvenanceEventTypeStr[record->getEventType()];
    recordJson["timestampMillis"] = record->getEventTime();
    recordJson["durationMillis"] = record->getEventDuration();
    recordJson["lineageStart"] = record->getlineageStartDate();
    recordJson["details"] = record->getDetails().c_str();
    recordJson["componentId"] = record->getComponentId().c_str();
    recordJson["componentType"] = record->getComponentType().c_str();
    recordJson["entityId"] = record->getFlowFileUuid().c_str();
    recordJson["entityType"] = "org.apache.nifi.flowfile.FlowFile";
    recordJson["entitySize"] = record->getFileSize();
    recordJson["entityOffset"] = record->getFileOffset();

    for (auto attr : record->getAttributes()) {
      updatedAttributesJson[attr.first] = attr.second;
    }
    recordJson["updatedAttributes"] = updatedAttributesJson;

    for (auto parentUUID : record->getParentUuids()) {
      parentUuidJson.append(parentUUID.c_str());
    }
    recordJson["parentIds"] = parentUuidJson;

    for (auto childUUID : record->getChildrenUuids()) {
      childUuidJson.append(childUUID.c_str());
    }
    recordJson["childIds"] = childUuidJson;
    recordJson["transitUri"] = record->getTransitUri().c_str();
    recordJson["remoteIdentifier"] = record->getSourceSystemFlowFileIdentifier().c_str();
    recordJson["alternateIdentifier"] = record->getAlternateIdentifierUri().c_str();
    recordJson["application"] = ProvenanceAppStr;
    array.append(recordJson);
  }

  Json::StyledWriter writer;
  report = writer.write(array);
}

void SiteToSiteProvenanceReportingTask::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
}

void SiteToSiteProvenanceReportingTask::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  std::unique_ptr<Site2SiteClientProtocol> protocol_ = getNextProtocol(true);

  if (!protocol_) {
    context->yield();
    return;
  }

  logger_->log_debug("SiteToSiteProvenanceReportingTask -- onTrigger");

  if (!protocol_->bootstrap()) {
    // bootstrap the client protocol if needeed
    context->yield();
    std::shared_ptr<Processor> processor = std::static_pointer_cast < Processor > (context->getProcessorNode()->getProcessor());
    logger_->log_error("Site2Site bootstrap failed yield period %d peer ", processor->getYieldPeriodMsec());
    returnProtocol(std::move(protocol_));
    return;
  }

  std::vector<std::shared_ptr<core::SerializableComponent>> records;

  logger_->log_debug("batch size %d records", batch_size_);
  size_t deserialized = batch_size_;
  std::shared_ptr<core::Repository> repo = context->getProvenanceRepository();
  std::function < std::shared_ptr<core::SerializableComponent>() > constructor = []() {return std::make_shared<provenance::ProvenanceEventRecord>();};
  if (!repo->DeSerialize(records, deserialized, constructor) && deserialized == 0) {
    logger_->log_debug("Not sending because deserialized is %d", deserialized);
    returnProtocol(std::move(protocol_));
    return;
  }

  logger_->log_debug("batch size %d records", batch_size_, deserialized);

  logger_->log_debug("Captured %d records", deserialized);
  std::string jsonStr;
  this->getJsonReport(context, session, records, jsonStr);
  if (jsonStr.length() <= 0) {
    returnProtocol(std::move(protocol_));
    return;
  }

  try {
    std::map<std::string, std::string> attributes;
    protocol_->transferString(context, session, jsonStr, attributes);
  } catch (...) {
    // if transfer bytes failed, return instead of purge the provenance records
    return;
  }

  // we transfer the record, purge the record from DB
  repo->Delete(records);
  returnProtocol(std::move(protocol_));
}

} /* namespace reporting */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
