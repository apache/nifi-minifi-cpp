/**
 * @file AppendHostInfo.h
 * AppendHostInfo class declaration
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
#ifndef __APPEND_HOSTINFO_H__
#define __APPEND_HOSTINFO_H__

#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

//! AppendHostInfo Class
class AppendHostInfo : public Processor
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	AppendHostInfo(std::string name, uuid_t uuid = NULL)
	: Processor(name, uuid)
	{
		_logger = Logger::getLogger();
	}
	//! Destructor
	virtual ~AppendHostInfo()
	{
	}
	//! Processor Name
	static const std::string ProcessorName;
	//! Supported Properties
	static Property InterfaceName;
	static Property HostAttribute;
	static Property IPAttribute;

	//! Supported Relationships
	static Relationship Success;

public:
	//! OnTrigger method, implemented by NiFi AppendHostInfo
	virtual void onTrigger(ProcessContext *context, ProcessSession *session);
	//! Initialize, over write by NiFi AppendHostInfo
	virtual void initialize(void);

protected:

private:
	//! Logger
	Logger *_logger;
};

#endif
