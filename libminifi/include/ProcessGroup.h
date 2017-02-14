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
		name_ = name;
	}
	//! Get Process Name
	std::string getName(void) {
		return (name_);
	}
	//! Set URL
	void setURL(std::string url) {
		url_ = url;
	}
	//! Get URL
	std::string getURL(void) {
		return (url_);
	}
	//! SetTransmitting
	void setTransmitting(bool val)
	{
		transmitting_ = val;
	}
	//! Get Transmitting
	bool getTransmitting()
	{
		return transmitting_;
	}
	//! setTimeOut
	void setTimeOut(uint64_t time)
	{
		timeOut_ = time;
	}
	uint64_t getTimeOut()
	{
		return timeOut_;
	}
	//! Set Processor yield period in MilliSecond
	void setYieldPeriodMsec(uint64_t period) {
		yield_period_msec_ = period;
	}
	//! Get Processor yield period in MilliSecond
	uint64_t getYieldPeriodMsec(void) {
		return(yield_period_msec_);
	}
	//! Set UUID
	void setUUID(uuid_t uuid) {
		uuid_copy(uuid_, uuid);
	}
	//! Get UUID
	bool getUUID(uuid_t uuid) {
		if (uuid)
		{
			uuid_copy(uuid, uuid_);
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
		std::lock_guard<std::mutex> lock(mtx_);
		parent_process_group_ = parent;
	}
	//! get parent process group
	ProcessGroup *getParent(void) {
		std::lock_guard<std::mutex> lock(mtx_);
		return parent_process_group_;
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
	uuid_t uuid_;
	//! Processor Group Name
	std::string name_;
	//! Process Group Type
	ProcessGroupType type_;
	//! Processors (ProcessNode) inside this process group which include Input/Output Port, Remote Process Group input/Output port
	std::set<Processor *> processors_;
	std::set<ProcessGroup *> child_process_groups_;
	//! Connections between the processor inside the group;
	std::set<Connection *> connections_;
	//! Parent Process Group
	ProcessGroup* parent_process_group_;
	//! Yield Period in Milliseconds
	std::atomic<uint64_t> yield_period_msec_;
	std::atomic<uint64_t> timeOut_;
	//! URL
	std::string url_;
	//! Transmitting
	std::atomic<bool> transmitting_;

private:

	//! Mutex for protection
	std::mutex mtx_;
	//! Logger
	Logger *logger_;
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	ProcessGroup(const ProcessGroup &parent);
	ProcessGroup &operator=(const ProcessGroup &parent);
};

#endif
