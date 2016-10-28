/**
 * @file ProcessSession.h
 * ProcessSession class declaration
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
#ifndef __PROCESS_SESSION_H__
#define __PROCESS_SESSION_H__

#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <set>

#include "Logger.h"
#include "Processor.h"
#include "ProcessContext.h"
#include "FlowFileRecord.h"
#include "Exception.h"
#include "Provenance.h"

//! ProcessSession Class
class ProcessSession
{
public:
	//! Constructor
	/*!
	 * Create a new process session
	 */
	ProcessSession(ProcessContext *processContext = NULL) : _processContext(processContext) {
		_logger = Logger::getLogger();
		_logger->log_trace("ProcessSession created for %s", _processContext->getProcessor()->getName().c_str());
		_provenanceReport = new ProvenanceReporter(_processContext->getProcessor()->getUUIDStr(),
				_processContext->getProcessor()->getName());
	}
	//! Destructor
	virtual ~ProcessSession() {
		if (_provenanceReport)
			delete _provenanceReport;
	}
	//! Commit the session
	void commit();
	//! Roll Back the session
	void rollback();
	//! Get Provenance Report
	ProvenanceReporter *getProvenanceReporter()
	{
		return _provenanceReport;
	}
	//!
	//! Get the FlowFile from the highest priority queue
	FlowFileRecord *get();
	//! Create a new UUID FlowFile with no content resource claim and without parent
	FlowFileRecord *create();
	//! Create a new UUID FlowFile with no content resource claim and inherit all attributes from parent
	FlowFileRecord *create(FlowFileRecord *parent);
	//! Clone a new UUID FlowFile from parent both for content resource claim and attributes
	FlowFileRecord *clone(FlowFileRecord *parent);
	//! Clone a new UUID FlowFile from parent for attributes and sub set of parent content resource claim
	FlowFileRecord *clone(FlowFileRecord *parent, long offset, long size);
	//! Duplicate a FlowFile with the same UUID and all attributes and content resource claim for the roll back of the session
	FlowFileRecord *duplicate(FlowFileRecord *orignal);
	//! Transfer the FlowFile to the relationship
	void transfer(FlowFileRecord *flow, Relationship relationship);
	//! Put Attribute
	void putAttribute(FlowFileRecord *flow, std::string key, std::string value);
	//! Remove Attribute
	void removeAttribute(FlowFileRecord *flow, std::string key);
	//! Remove Flow File
	void remove(FlowFileRecord *flow);
	//! Execute the given read callback against the content
	void read(FlowFileRecord *flow, InputStreamCallback *callback);
	//! Execute the given write callback against the content
	void write(FlowFileRecord *flow, OutputStreamCallback *callback);
	//! Execute the given write/append callback against the content
	void append(FlowFileRecord *flow, OutputStreamCallback *callback);
	//! Penalize the flow
	void penalize(FlowFileRecord *flow);
	//! Import the existed file into the flow
	void import(std::string source, FlowFileRecord *flow, bool keepSource = true, uint64_t offset = 0);

protected:
	//! FlowFiles being modified by current process session
	std::map<std::string, FlowFileRecord *> _updatedFlowFiles;
	//! Copy of the original FlowFiles being modified by current process session as above
	std::map<std::string, FlowFileRecord *> _originalFlowFiles;
	//! FlowFiles being added by current process session
	std::map<std::string, FlowFileRecord *> _addedFlowFiles;
	//! FlowFiles being deleted by current process session
	std::map<std::string, FlowFileRecord *> _deletedFlowFiles;
	//! FlowFiles being transfered to the relationship
	std::map<std::string, Relationship> _transferRelationship;
	//! FlowFiles being cloned for multiple connections per relationship
	std::map<std::string, FlowFileRecord *> _clonedFlowFiles;

private:
	// Clone the flow file during transfer to multiple connections for a relationship
	FlowFileRecord* cloneDuringTransfer(FlowFileRecord *parent);
	//! ProcessContext
	ProcessContext *_processContext;
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	ProcessSession(const ProcessSession &parent);
	ProcessSession &operator=(const ProcessSession &parent);
	//! Logger
	Logger *_logger;
	//! Provenance Report
	ProvenanceReporter *_provenanceReport;

};

#endif
