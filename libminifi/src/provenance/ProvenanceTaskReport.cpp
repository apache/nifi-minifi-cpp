/**
 * @file ProvenanceTaskReport.cpp
 * ProvenanceTaskReport class implementation
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
#include <iostream>

#include "provenance/ProvenanceTaskReport.h"
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
namespace provenance {

core::Property ProvenanceTaskReport::hostName("Host Name", "Remote Host Name.",
    "localhost");
core::Property ProvenanceTaskReport::port("Port", "Remote Port", "9999");
core::Property ProvenanceTaskReport::batchSize("Batch Size",
    "Specifies how many records to send in a single batch, at most.", "100");
core::Property ProvenanceTaskReport::portUUID("Port UUID",
    "Specifies remote NiFi Port UUID.", "");
core::Relationship ProvenanceTaskReport::relation;
const char *ProvenanceTaskReport::ProvenanceAppStr = "MiNiFi Flow";

void ProvenanceTaskReport::initialize() {
  //! Set the supported properties
  std::set<core::Property> properties;
  properties.insert(hostName);
  properties.insert(port);
  properties.insert(batchSize);
  properties.insert(portUUID);
  setSupportedProperties(properties);
  //! Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(relation);
  setSupportedRelationships(relationships);
}

void ProvenanceTaskReport::onTrigger(core::ProcessContext *context,
    core::ProcessSession *session) {
  std::string value;
  int64_t lvalue;
  std::string host = "";
  uint16_t sport = 0;

  if (context->getProperty(hostName.getName(), value)) {
    host = value;
  }
  if (context->getProperty(port.getName(), value)
      && core::Property::StringToInt(value, lvalue)) {
    sport = (uint16_t) lvalue;
  }
  if (context->getProperty(portUUID.getName(), value)) {
    uuid_parse(value.c_str(), protocol_uuid_);
  }

  std::shared_ptr<Site2SiteClientProtocol> protocol_ =
      this->obtainSite2SiteProtocol(host, sport, protocol_uuid_);

  if (!protocol_) {
    context->yield();
    return;
  }

  if (!protocol_->bootstrap()) {
    // bootstrap the client protocol if needeed
    context->yield();
    std::shared_ptr<Processor> processor = std::static_pointer_cast < Processor
        > (context->getProcessorNode().getProcessor());
    logger_->log_error("Site2Site bootstrap failed yield period %d peer ",
        processor->getYieldPeriodMsec());
    returnSite2SiteProtocol(protocol_);
    return;
  }

  int64_t batch = 100;

  if (context->getProperty(batchSize.getName(), value)
      && core::Property::StringToInt(value, lvalue)) {
    batch = lvalue;
  }

  std::vector < std::shared_ptr < ProvenanceEventRecord >> records;
  std::shared_ptr<ProvenanceRepository> repo = std::static_pointer_cast
      < ProvenanceRepository > (context->getProvenanceRepository());

  repo->getProvenanceRecord(records, batch);

  if (records.size() <= 0) {
    returnSite2SiteProtocol(protocol_);
    return;
  }

  Json::Value array;
  for (auto record : records) {
    Json::Value recordJson;
    Json::Value updatedAttributesJson;
    Json::Value parentUuidJson;
    Json::Value childUuidJson;
    recordJson["eventId"] = record->getEventId().c_str();
    recordJson["eventType"] =
        ProvenanceEventRecord::ProvenanceEventTypeStr[record->getEventType()];
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
    recordJson["remoteIdentifier"] =
        record->getSourceSystemFlowFileIdentifier().c_str();
    recordJson["alternateIdentifier"] =
        record->getAlternateIdentifierUri().c_str();
    recordJson["application"] = ProvenanceAppStr;
    array.append(recordJson);
  }

  Json::StyledWriter writer;
  std::string jsonStr = writer.write(array);

  try {
    std::map < std::string, std::string > attributes;
    protocol_->transferString(context, session, jsonStr, attributes);
  } catch (...) {
    // if transfer bytes failed, return instead of purge the provenance records
    return;
  }

  // we transfer the record, purge the record from DB
  repo->purgeProvenanceRecord(records);

  returnSite2SiteProtocol(protocol_);
}

} /* namespace provenance */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
