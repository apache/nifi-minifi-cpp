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
#include <sys/time.h>
#include <time.h>
#include <sstream>
#include <string.h>
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
namespace provenance{

const std::string ProvenanceTaskReport::ProcessorName("ProvenanceTaskReport");
core::Property ProvenanceTaskReport::hostName("Host Name", "Remote Host Name.", "localhost");
core::Property ProvenanceTaskReport::port("Port", "Remote Port", "9999");
core::Property ProvenanceTaskReport::batchSize("Batch Size", "Specifies how many records to send in a single batch, at most.", "100");
core::Relationship ProvenanceTaskReport::relation;

void ProvenanceTaskReport::initialize()
{
	//! Set the supported properties
	std::set<core::Property> properties;
	properties.insert(hostName);
	properties.insert(port);
	properties.insert(batchSize);
	setSupportedProperties(properties);
	//! Set the supported relationships
	std::set<core::Relationship> relationships;
	relationships.insert(relation);
	setSupportedRelationships(relationships);
}

std::unique_ptr<Site2SiteClientProtocol> ProvenanceTaskReport::getNextProtocol()
{
	std::lock_guard<std::mutex> protocol_lock_(protocol_mutex_);
	if (available_protocols_.empty())
		return nullptr;
	std::unique_ptr<Site2SiteClientProtocol> return_pointer = std::move(available_protocols_.top());
	available_protocols_.pop();
	return std::move(return_pointer);
}

void ProvenanceTaskReport::returnProtocol(
  std::unique_ptr<Site2SiteClientProtocol> return_protocol)
{
	std::lock_guard<std::mutex> protocol_lock_(protocol_mutex_);
	available_protocols_.push(std::move(return_protocol));
}

void ProvenanceTaskReport::onTrigger(core::ProcessContext *context, core::ProcessSession *session)
{
	std::string value;
	int64_t lvalue;
	
	std::unique_ptr<Site2SiteClientProtocol> protocol_ = getNextProtocol();

	if (protocol_ == nullptr)
	{
		protocol_ = std::unique_ptr<Site2SiteClientProtocol>(
	        new Site2SiteClientProtocol(0));
	    protocol_->setPortId(protocol_uuid_);

	    std::string host = "";
	    uint16_t sport = 0;

	    if (context->getProperty(hostName.getName(), value)) {
	      host = value;
	    }
	    if (context->getProperty(port.getName(), value)
	        && core::Property::StringToInt(value, lvalue)) {
	      sport = (uint16_t) lvalue;
	    }
	    std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str =
	        std::unique_ptr<org::apache::nifi::minifi::io::DataStream>(
	            org::apache::nifi::minifi::io::StreamFactory::getInstance()
	                ->createSocket(host, sport));

	    std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr<Site2SitePeer>(
	        new Site2SitePeer(std::move(str), host, sport));

	    protocol_->setPeer(std::move(peer_));
	}

	if (!protocol_->bootstrap())
	{
	    // bootstrap the client protocol if needeed
	    context->yield();
	    std::shared_ptr<Processor> processor = std::static_pointer_cast<Processor>(
	        context->getProcessorNode().getProcessor());
	    logger_->log_error("Site2Site bootstrap failed yield period %d peer ",
	                       processor->getYieldPeriodMsec());
	    return;
	}

	int batch = 100;

	if (context->getProperty(batchSize.getName(), value) && core::Property::StringToInt(value, lvalue))
	{
		batch = (int) lvalue;
	}
	
	std::vector<std::shared_ptr<ProvenanceEventRecord>> records;
	std::shared_ptr<ProvenanceRepository> repo = std::static_pointer_cast<ProvenanceRepository> (context->getProvenanceRepository());

	repo->getProvenanceRecord(records, batch);

	if (records.size() <= 0)
	{
		returnProtocol(std::move(protocol_));
		return;
	}

	Json::Value array;
	for (auto record : records)
	{
		Json::Value recordJson;
		Json::Value updatedAttributesJson;
		Json::Value parentUuidJson;
		Json::Value childUuidJson;
		recordJson["eventId"] = record->getEventId().c_str();
		recordJson["eventType"] = ProvenanceEventRecord::ProvenanceEventTypeStr[record->getEventType()];
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

		for (auto attr : record->getAttributes())
		{
			updatedAttributesJson[attr.first] = attr.second;
		}
		recordJson["updatedAttributes"] = updatedAttributesJson;

		for (auto parentUUID : record->getParentUuids())
		{
			parentUuidJson.append(parentUUID.c_str());
		}
		recordJson["parentIds"] = parentUuidJson;

		for (auto childUUID : record->getChildrenUuids())
		{
			childUuidJson.append(childUUID.c_str());
		}
		recordJson["childIds"] = childUuidJson;
		recordJson["transitUri"] = record->getTransitUri().c_str();
		recordJson["remoteIdentifier"] = record->getSourceSystemFlowFileIdentifier().c_str();
		recordJson["alternateIdentifier"] = record->getAlternateIdentifierUri().c_str();
		recordJson["application"] = "MiNiFi Flow";
		array.append(recordJson);
	}

	Json::StyledWriter writer;
	std::string jsonStr = writer.write(array);
	uint8_t *payload = (uint8_t *) jsonStr.c_str();
	int length = jsonStr.length();

	try
	{
		std::map<std::string, std::string> attributes;
		protocol_->transferBytes(context, session, payload, length, attributes);
	}
	catch (...)
	{
		// if transfer bytes failed, return instead of purge the provenance records
		returnProtocol(std::move(protocol_));
		return;
	}

	// we transfer the record, purge the record from DB
	repo->purgeProvenanceRecord(records);

	returnProtocol(std::move(protocol_));

	return;
}

} /* namespace provenance */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
