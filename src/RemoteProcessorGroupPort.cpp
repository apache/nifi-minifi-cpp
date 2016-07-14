/**
 * @file RemoteProcessorGroupPort.cpp
 * RemoteProcessorGroupPort class implementation
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

#include "TimeUtil.h"
#include "RemoteProcessorGroupPort.h"
#include "ProcessContext.h"
#include "ProcessSession.h"

const std::string RemoteProcessorGroupPort::ProcessorName("RemoteProcessorGroupPort");
Property RemoteProcessorGroupPort::hostName("Host Name", "Remote Host Name.", "localhost");
Property RemoteProcessorGroupPort::port("Port", "Remote Port", "9999");
Relationship RemoteProcessorGroupPort::relation;

void RemoteProcessorGroupPort::initialize()
{
	//! Set the supported properties
	std::set<Property> properties;
	properties.insert(hostName);
	properties.insert(port);
	setSupportedProperties(properties);
	//! Set the supported relationships
	std::set<Relationship> relationships;
	relationships.insert(relation);
	setSupportedRelationships(relationships);
}

void RemoteProcessorGroupPort::onTrigger(ProcessContext *context, ProcessSession *session)
{
	std::string value;

	if (!_transmitting)
		return;

	std::string host = _peer->getHostName();
	uint16_t sport = _peer->getPort();
	int64_t lvalue;
	bool needReset = false;

	if (context->getProperty(hostName.getName(), value))
	{
		host = value;
	}
	if (context->getProperty(port.getName(), value) && Property::StringToInt(value, lvalue))
	{
		sport = (uint16_t) lvalue;
	}
	if (host != _peer->getHostName())
	{
		_peer->setHostName(host);
		needReset= true;
	}
	if (sport != _peer->getPort())
	{
		_peer->setPort(sport);
		needReset = true;
	}
	if (needReset)
		_protocol->tearDown();

	if (!_protocol->bootstrap())
	{
		// bootstrap the client protocol if needeed
		context->yield();
		return;
	}

	if (_direction == RECEIVE)
		_protocol->receiveFlowFiles(context, session);
	else
		_protocol->transferFlowFiles(context, session);

	return;
}
