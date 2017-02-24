/**
 * @file SchedulingAgent.h
 * SchedulingAgent class declaration
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
#ifndef __SCHEDULING_AGENT_H__
#define __SCHEDULING_AGENT_H__

#include <uuid/uuid.h>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <thread>
#include "utils/TimeUtil.h"
#include "Logger.h"
#include "Configure.h"
#include "FlowFileRecord.h"
#include "Logger.h"
#include "Processor.h"
#include "ProcessContext.h"

//! SchedulingAgent Class
class SchedulingAgent
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	SchedulingAgent() {
		configure_ = Configure::getConfigure();
		logger_ = Logger::getLogger();
		_running = false;
	}
	//! Destructor
	virtual ~SchedulingAgent()
	{

	}
	//! onTrigger, return whether the yield is need
	bool onTrigger(Processor *processor, ProcessContext *processContext, ProcessSessionFactory *sessionFactory);
	//! Whether agent has work to do
	bool hasWorkToDo(Processor *processor);
	//! Whether the outgoing need to be backpressure
	bool hasTooMuchOutGoing(Processor *processor);
	//! start
	void start() {
		_running = true;
	}
	//! stop
	void stop() {
		_running = false;
	}

public:
	//! schedule, overwritten by different DrivenSchedulingAgent
	virtual void schedule(Processor *processor) = 0;
	//! unschedule, overwritten by different DrivenSchedulingAgent
	virtual void unschedule(Processor *processor) = 0;

protected:
	//! Logger
	std::shared_ptr<Logger> logger_;
	//! Configure
	Configure *configure_;
	//! Mutex for protection
	std::mutex _mtx;
	//! Whether it is running
	std::atomic<bool> _running;
	//! AdministrativeYieldDuration
	int64_t _administrativeYieldDuration;
	//! BoredYieldDuration
	int64_t _boredYieldDuration;

private:
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	SchedulingAgent(const SchedulingAgent &parent);
	SchedulingAgent &operator=(const SchedulingAgent &parent);

};

#endif
