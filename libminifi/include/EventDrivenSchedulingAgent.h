/**
 * @file EventDrivenSchedulingAgent.h
 * EventDrivenSchedulingAgent class declaration
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
#ifndef __EVENT_DRIVEN_SCHEDULING_AGENT_H__
#define __EVENT_DRIVEN_SCHEDULING_AGENT_H__

#include "Logger.h"
#include "Processor.h"
#include "ProcessContext.h"
#include "ThreadedSchedulingAgent.h"

//! EventDrivenSchedulingAgent Class
class EventDrivenSchedulingAgent : public ThreadedSchedulingAgent
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	EventDrivenSchedulingAgent()
	: ThreadedSchedulingAgent()
	{
	}
	//! Destructor
	virtual ~EventDrivenSchedulingAgent()
	{
	}
	//! Run function for the thread
	void run(Processor *processor);

private:
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	EventDrivenSchedulingAgent(const EventDrivenSchedulingAgent &parent);
	EventDrivenSchedulingAgent &operator=(const EventDrivenSchedulingAgent &parent);

};

#endif
