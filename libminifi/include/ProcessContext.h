/**
 * @file ProcessContext.h
 * ProcessContext class declaration
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
#ifndef __PROCESS_CONTEXT_H__
#define __PROCESS_CONTEXT_H__

#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>

#include "Logger.h"
#include "Processor.h"

//! ProcessContext Class
class ProcessContext
{
public:
	//! Constructor
	/*!
	 * Create a new process context associated with the processor/controller service/state manager
	 */
	ProcessContext(Processor *processor = NULL) : _processor(processor) {
		_logger = Logger::getLogger();
	}
	//! Destructor
	virtual ~ProcessContext() {}
	//! Get Processor associated with the Process Context
	Processor *getProcessor() {
		return _processor;
	}
	bool getProperty(std::string name, std::string &value) {
		if (_processor)
			return _processor->getProperty(name, value);
		else
			return false;
	}
	//! Whether the relationship is supported
	bool isSupportedRelationship(Relationship relationship) {
		if (_processor)
			return _processor->isSupportedRelationship(relationship);
		else
			return false;
	}
	//! Check whether the relationship is auto terminated
	bool isAutoTerminated(Relationship relationship) {
		if (_processor)
			return _processor->isAutoTerminated(relationship);
		else
			return false;
	}
	//! Get ProcessContext Maximum Concurrent Tasks
	uint8_t getMaxConcurrentTasks(void) {
		if (_processor)
			return _processor->getMaxConcurrentTasks();
		else
			return 0;
	}
	//! Yield based on the yield period
	void yield() {
		if (_processor)
			_processor->yield();
	}

protected:

private:

	//! Processor
	Processor *_processor;
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	ProcessContext(const ProcessContext &parent);
	ProcessContext &operator=(const ProcessContext &parent);
	//! Logger
	Logger *_logger;

};

#endif
