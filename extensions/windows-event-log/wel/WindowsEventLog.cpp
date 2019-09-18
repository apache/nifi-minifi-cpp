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
#include "WindowsEventLog.h"
#include "utils/Deleters.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace wel {

std::string WindowsEventLogHandler::getEventMessage(EVT_HANDLE eventHandle) const
{
	std::string returnValue;
	std::unique_ptr<WCHAR, utils::FreeDeleter> pBuffer;
	DWORD dwBufferSize = 0;
	DWORD dwBufferUsed = 0;
	DWORD status = 0;

	//EvtFormatMessage(metadataProvider, eventHandle, 0, 0, NULL, EvtFormatMessageEvent, dwBufferSize, pBuffer.get(), &dwBufferUsed);
	EvtFormatMessage(metadata_provider_, eventHandle, 0, 0, NULL, EvtFormatMessageEvent, dwBufferSize, pBuffer.get(), &dwBufferUsed);

	//  we need to get the size of the buffer
	status = GetLastError();
	if (ERROR_INSUFFICIENT_BUFFER == status) {
		dwBufferSize = dwBufferUsed;

		/* All C++ examples use malloc and even HeapAlloc in some cases. To avoid any problems ( with EvtFormatMessage calling
			free for example ) we will continue to use malloc and use a custom deleter with unique_ptr.
		'*/
		pBuffer = std::unique_ptr<WCHAR, utils::FreeDeleter>(static_cast<LPWSTR>(malloc(dwBufferSize * sizeof(WCHAR))));


		if (pBuffer) {
			EvtFormatMessage(metadata_provider_, eventHandle, 0, 0, NULL, EvtFormatMessageEvent, dwBufferSize, pBuffer.get(), &dwBufferUsed);
		}
		else {
			return returnValue;
		}
	}
	else if (ERROR_EVT_MESSAGE_NOT_FOUND == status || ERROR_EVT_MESSAGE_ID_NOT_FOUND == status) {
		return returnValue;
	}
	else {
		return returnValue;
	}

	// convert wstring to std::string
	std::wstring message(pBuffer.get());
	returnValue = std::string(message.begin(), message.end());
	return returnValue;

}

std::string WindowsEventLogHandler::getEventHeader(const std::string &logName,MetadataWalker &walker) const {
	std::stringstream eventHeader;

	eventHeader << "Log Name:      " << utils::StringUtils::trim(logName) << std::endl;
	eventHeader << "Source:        " << utils::StringUtils::trim(walker.getMetadata(METADATA::SOURCE)) << std::endl;
	eventHeader << "Date:          " << utils::StringUtils::trim(walker.getMetadata(METADATA::TIME_CREATED)) << std::endl;
	eventHeader << "Event ID:      " << utils::StringUtils::trim(walker.getMetadata(METADATA::EVENTID)) << std::endl;
	eventHeader << "Task Category: " << utils::StringUtils::trim(walker.getMetadata(METADATA::TASK_CATEGORY)) << std::endl;
	eventHeader << "Level:         " << utils::StringUtils::trim(walker.getMetadata(METADATA::LEVEL)) << std::endl;
	eventHeader << "Keywords:      " << utils::StringUtils::trim(walker.getMetadata(METADATA::KEYWORDS)) << std::endl;
	eventHeader << "User:          " << utils::StringUtils::trim(walker.getMetadata(METADATA::USER)) << std::endl;
	eventHeader << "Computer:      " << utils::StringUtils::trim(walker.getMetadata(METADATA::COMPUTER)) << std::endl;
	eventHeader << "Description: " << std::endl;

	return eventHeader.str();
}

EVT_HANDLE WindowsEventLogHandler::getMetadata() const {
	return metadata_provider_;
}

} /* namespace wel */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

