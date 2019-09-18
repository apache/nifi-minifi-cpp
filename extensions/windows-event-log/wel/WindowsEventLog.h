/**
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

#pragma once

#include "MetadataWalker.h"
#include "core/Core.h"
#include "FlowFileRecord.h"
#include "concurrentqueue.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include <pugixml.hpp>
#include <winevt.h>
#include <sstream>
#include <regex>
#include <codecvt>
#include "utils/OsUtils.h"



namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace wel {

class WindowsEventLogHandler
{
public:
	WindowsEventLogHandler() : metadata_provider_(nullptr){
	}

	explicit WindowsEventLogHandler(EVT_HANDLE metadataProvider) : metadata_provider_(metadataProvider) {
	}
	
	std::string getEventMessage(EVT_HANDLE eventHandle) const;

	std::string getEventHeader(const std::string &log_name, MetadataWalker &walker) const;

	EVT_HANDLE getMetadata() const;

private:
	EVT_HANDLE metadata_provider_;
};

} /* namespace wel */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

