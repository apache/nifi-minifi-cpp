/**
 * @file ProvenanceTaskReport.h
 * ProvenanceTaskReport class declaration
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
#ifndef __PROVENANCE_TASK_REPORT_H__
#define __PROVENANCE_TASK_REPORT_H__

#include <mutex>
#include <memory>
#include <stack>
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "Site2SiteClientProtocol.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace provenance {

//! ProvenanceTaskReport Class
class ProvenanceTaskReport: public core::Processor {
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	ProvenanceTaskReport(std::string name, uuid_t uuid = NULL) :
			core::Processor(name, uuid) {
		logger_ = logging::Logger::getLogger();
		uuid_copy(protocol_uuid_,uuid);
		this->setTriggerWhenEmpty(true);
	}
	//! Destructor
	virtual ~ProvenanceTaskReport() {

	}
	//! Processor Name
	static const std::string ProcessorName;
	//! Supported Properties
	static core::Property hostName;
	static core::Property port;
	static core::Property batchSize;
	static core::Property portUUID;
	//! Supported Relationships
	static core::Relationship relation;
	static const char *ProvenanceAppStr;
public:
	//! OnTrigger method, implemented by NiFi ProvenanceTaskReport
	virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
	//! Initialize, over write by NiFi ProvenanceTaskReport
	virtual void initialize(void);

protected:

private:
	uuid_t protocol_uuid_;
	//! Logger
	std::shared_ptr<logging::Logger> logger_;
};

// Provenance Task Report

} /* namespace provenance */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
