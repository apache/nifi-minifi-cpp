/**
 * @file RemoteProcessorGroupPort.h
 * RemoteProcessorGroupPort class declaration
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
#ifndef __REMOTE_PROCESSOR_GROUP_PORT_H__
#define __REMOTE_PROCESSOR_GROUP_PORT_H__

#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"
#include "Site2SiteClientProtocol.h"

//! RemoteProcessorGroupPort Class
class RemoteProcessorGroupPort : public Processor
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	RemoteProcessorGroupPort(std::string name, uuid_t uuid = NULL)
	: Processor(name, uuid)
	{
		_logger = Logger::getLogger();
		_peer = new Site2SitePeer("", 9999);
		_protocol = new Site2SiteClientProtocol(_peer);
		_protocol->setPortId(uuid);
	}
	//! Destructor
	virtual ~RemoteProcessorGroupPort()
	{
		delete _protocol;
		delete _peer;
	}
	//! Processor Name
	static const std::string ProcessorName;
	//! Supported Properties
	static Property hostName;
	static Property port;
	//! Supported Relationships
	static Relationship relation;
public:
	//! OnTrigger method, implemented by NiFi RemoteProcessorGroupPort
	virtual void onTrigger(ProcessContext *context, ProcessSession *session);
	//! Initialize, over write by NiFi RemoteProcessorGroupPort
	virtual void initialize(void);
	//! Set Direction
	void setDirection(TransferDirection direction)
	{
		_direction = direction;
		if (_direction == RECEIVE)
			this->setTriggerWhenEmpty(true);
	}
	//! Set Timeout
	void setTimeOut(uint64_t timeout)
	{
		_protocol->setTimeOut(timeout);
	}
	//! SetTransmitting
	void setTransmitting(bool val)
	{
		_transmitting = val;
	}

protected:

private:
	//! Logger
	Logger *_logger;
	//! Peer Connection
	Site2SitePeer *_peer;
	//! Peer Protocol
	Site2SiteClientProtocol *_protocol;
	//! Transaction Direction
	TransferDirection _direction;
	//! Transmitting
	bool _transmitting;

};

#endif
