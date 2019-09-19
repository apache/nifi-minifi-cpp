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


	enum METADATA {
		LOG_NAME,
		SOURCE,
		TIME_CREATED,
		EVENTID,
		EVENT_RECORDID,
		TASK_CATEGORY,
		LEVEL,
		KEYWORDS,
		USER,
		COMPUTER,
		UNKNOWN
	};


// this is a continuous enum so we can rely on the array

typedef std::map<METADATA, std::string> METADATA_NAMES;

class WindowsEventLogHandler
{
public:
	WindowsEventLogHandler() : metadata_provider_(nullptr){
	}
	
	explicit WindowsEventLogHandler(EVT_HANDLE metadataProvider) : metadata_provider_(metadataProvider) {
	}
	
	std::string getEventMessage(EVT_HANDLE eventHandle) const;


	EVT_HANDLE getMetadata() const;

private:
	EVT_HANDLE metadata_provider_;
};

class WindowsEventLogMetadata {
public:
	WindowsEventLogMetadata(EVT_HANDLE metadataProvider, const std::string &log_name) : metadata_ptr_(metadataProvider), log_name_(log_name) {
	}

	virtual std::map<std::string, std::string> getFieldValues() const = 0;

	virtual std::map<std::string, std::string> getIdentifiers() const = 0;

	virtual std::string getMetadata(METADATA metadata) const = 0;

	

	static std::string getMetadataString(METADATA val) {
		static std::map< METADATA, std::string> map = {
			{LOG_NAME,	"LOG_NAME" },
		{SOURCE,"SOURCE"},
		{TIME_CREATED,"TIME_CREATED" },
		{EVENTID,"EVENTID"},
		{EVENT_RECORDID,"EVENT_RECORDID"},
		{TASK_CATEGORY, "TASK_CATEGORY"},
		{LEVEL,"LEVEL"},
		{KEYWORDS,"KEYWORDS"},
		{USER,"USER"},
		{COMPUTER,"COMPUTER"}
		};

		return map[val];
	}


	static METADATA getMetadataFromString(const std::string &val) {
		static std::map< std::string, METADATA> map = {
			{"LOG_NAME",LOG_NAME},
			{"SOURCE",SOURCE},
			{"TIME_CREATED",TIME_CREATED },
			{"EVENTID",EVENTID},
			{"EVENT_RECORDID",EVENT_RECORDID},
			{"TASK_CATEGORY", TASK_CATEGORY},
			{"LEVEL",LEVEL},
			{"KEYWORDS",KEYWORDS},
			{"USER",USER},
			{"COMPUTER",COMPUTER}
		};

		auto enumVal = map.find(val);
		if (enumVal != std::end(map)) {
			return enumVal->second;
		}
		else {
			return METADATA::UNKNOWN;
		}
	}

	static std::string getComputerName() {
		static std::string computer_name;
		if (computer_name.empty()) {
			char buff[MAX_COMPUTERNAME_LENGTH + 1];
			DWORD size = sizeof(buff);
			if (GetComputerName(buff, &size)) {
				computer_name = buff;
			}
			else {
				computer_name = "N/A";
			}
		}
		return computer_name;
	}

protected:
	std::string log_name_;
	EVT_HANDLE metadata_ptr_;
};


class WindowsEventLogHeader {
public:
	explicit WindowsEventLogHeader(METADATA_NAMES header_names) : header_names_(header_names){

	}

	void setDelimiter(const std::string &delim);

	std::string getEventHeader(const WindowsEventLogMetadata * const metadata) const;

	

private:

	inline std::string createDefaultDelimiter(size_t max, size_t length) const;

	std::string delimiter_;
	METADATA_NAMES header_names_;
};

} /* namespace wel */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

