/**
 * @file LogAttribute.h
 * LogAttribute class declaration
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
#ifndef __LOG_ATTRIBUTE_H__
#define __LOG_ATTRIBUTE_H__

#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

//! LogAttribute Class
class LogAttribute : public Processor
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	LogAttribute(std::string name, uuid_t uuid = NULL)
	: Processor(name, uuid)
	{
		_logger = Logger::getLogger();
	}
	//! Destructor
	virtual ~LogAttribute()
	{
	}
	//! Processor Name
	static const std::string ProcessorName;
	//! Supported Properties
	static Property LogLevel;
	static Property AttributesToLog;
	static Property AttributesToIgnore;
	static Property LogPayload;
	static Property LogPrefix;
	//! Supported Relationships
	static Relationship Success;
	enum LogAttrLevel {
        LogAttrLevelTrace, LogAttrLevelDebug, LogAttrLevelInfo, LogAttrLevelWarn, LogAttrLevelError
    };
	//! Convert log level from string to enum
	bool logLevelStringToEnum(std::string logStr, LogAttrLevel &level)
	{
		if (logStr == "trace")
		{
			level = LogAttrLevelTrace;
			return true;
		}
		else if (logStr == "debug")
		{
			level = LogAttrLevelDebug;
			return true;
		}
		else if (logStr == "info")
		{
			level = LogAttrLevelInfo;
			return true;
		}
		else if (logStr == "warn")
		{
			level = LogAttrLevelWarn;
			return true;
		}
		else if (logStr == "error")
		{
			level = LogAttrLevelError;
			return true;
		}
		else
			return false;
	}

public:
	//! OnTrigger method, implemented by NiFi LogAttribute
	virtual void onTrigger(ProcessContext *context, ProcessSession *session);
	//! Initialize, over write by NiFi LogAttribute
	virtual void initialize(void);

protected:

private:
	//! Logger
	Logger *_logger;
};

#endif
