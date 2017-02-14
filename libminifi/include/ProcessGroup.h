/**
 * @file ProcessGroup.h
 * ProcessGroup class declaration
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
#ifndef __PROCESS_GROUP_H__
#define __PROCESS_GROUP_H__

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
#include "Exception.h"
#include "TimerDrivenSchedulingAgent.h"
#include "EventDrivenSchedulingAgent.h"

//! Process Group Type
enum ProcessGroupType
{
	ROOT_PROCESS_GROUP = 0,
	REMOTE_PROCESS_GROUP,
	MAX_PROCESS_GROUP_TYPE
};

//! ProcessGroup Class
class ProcessGroup
{
public:
	//! Constructor
	/*!
	 * Create a new process group
	 */
	ProcessGroup(ProcessGroupType type, std::string name, uuid_t uuid = NULL, ProcessGroup *parent = NULL);
	//! Destructor
	virtual ~ProcessGroup();
	//! Set Processor Name
	void setName(std::string name) {
		_name = name;
	}
	//! Get Process Name
	std::string getName(void) {
		return (_name);
	}
	//! Set URL
	void setURL(std::string url) {
		_url = url;
	}
	//! Get URL
	std::string getURL(void) {
		return (_url);
	}
	//! SetTransmitting
	void setTransmitting(bool val)
	{
		_transmitting = val;
	}
	//! Get Transmitting
	bool getTransmitting()
	{
		return _transmitting;
	}
	//! setTimeOut
	void setTimeOut(uint64_t time)
	{
		_timeOut = time;
	}
	uint64_t getTimeOut()
	{
		return _timeOut;
	}
	//! Set Processor yield period in MilliSecond
	void setYieldPeriodMsec(uint64_t period) {
		_yieldPeriodMsec = period;
	}
	//! Get Processor yield period in MilliSecond
	uint64_t getYieldPeriodMsec(void) {
		return(_yieldPeriodMsec);
	}
	//! Set UUID
	void setUUID(uuid_t uuid) {
		uuid_copy(_uuid, uuid);
	}
	//! Get UUID
	bool getUUID(uuid_t uuid) {
		if (uuid)
		{
			uuid_copy(uuid, _uuid);
			return true;
		}
		else
			return false;
	}
	//! Start Processing
	void startProcessing(TimerDrivenSchedulingAgent *timeScheduler,
			EventDrivenSchedulingAgent *eventScheduler);
	//! Stop Processing
	void stopProcessing(TimerDrivenSchedulingAgent *timeScheduler,
			EventDrivenSchedulingAgent *eventScheduler);
	//! Whether it is root process group
	bool isRootProcessGroup();
	//! set parent process group
	void setParent(ProcessGroup *parent) {
		std::lock_guard<std::mutex> lock(_mtx);
		_parentProcessGroup = parent;
	}
	//! get parent process group
	ProcessGroup *getParent(void) {
		std::lock_guard<std::mutex> lock(_mtx);
		return _parentProcessGroup;
	}
	//! Add processor
	void addProcessor(Processor *processor);
	//! Remove processor
	void removeProcessor(Processor *processor);
	//! Add child processor group
	void addProcessGroup(ProcessGroup *child);
	//! Remove child processor group
	void removeProcessGroup(ProcessGroup *child);
	// ! Add connections
	void addConnection(Connection *connection);
	//! findProcessor based on UUID
	Processor *findProcessor(uuid_t uuid);
	//! findProcessor based on name
	Processor *findProcessor(std::string processorName);
	//! removeConnection
	void removeConnection(Connection *connection);
	//! update property value
	void updatePropertyValue(std::string processorName, std::string propertyName, std::string propertyValue);

protected:
	//! A global unique identifier
	uuid_t _uuid;
	//! Processor Group Name
	std::string _name;
	//! Process Group Type
	ProcessGroupType _type;
	//! Processors (ProcessNode) inside this process group which include Input/Output Port, Remote Process Group input/Output port
	std::set<Processor *> _processors;
	std::set<ProcessGroup *> _childProcessGroups;
	//! Connections between the processor inside the group;
	std::set<Connection *> _connections;
	//! Parent Process Group
	ProcessGroup* _parentProcessGroup;
	//! Yield Period in Milliseconds
	std::atomic<uint64_t> _yieldPeriodMsec;
	std::atomic<uint64_t> _timeOut;
	//! URL
	std::string _url;
	//! Transmitting
	std::atomic<bool> _transmitting;

private:

	//! Mutex for protection
	std::mutex _mtx;
	//! Logger
	Logger *_logger;
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	ProcessGroup(const ProcessGroup &parent);
	ProcessGroup &operator=(const ProcessGroup &parent);
};

#endif
