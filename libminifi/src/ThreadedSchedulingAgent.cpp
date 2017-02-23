/**
 * @file ThreadedSchedulingAgent.cpp
 * ThreadedSchedulingAgent class implementation
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
#include <thread>
#include <iostream>

#include "ThreadedSchedulingAgent.h"

#include "ProcessContext.h"
#include "ProcessSession.h"
#include "ProcessSessionFactory.h"

void ThreadedSchedulingAgent::schedule(Processor *processor)
{
	std::lock_guard<std::mutex> lock(_mtx);

	_administrativeYieldDuration = 0;
	std::string yieldValue;

	if (configure_->get(Configure::nifi_administrative_yield_duration, yieldValue))
	{
		TimeUnit unit;
		if (Property::StringToTime(yieldValue, _administrativeYieldDuration, unit) &&
					Property::ConvertTimeUnitToMS(_administrativeYieldDuration, unit, _administrativeYieldDuration))
		{
			logger_->log_debug("nifi_administrative_yield_duration: [%d] ms", _administrativeYieldDuration);
		}
	}

	_boredYieldDuration = 0;
	if (configure_->get(Configure::nifi_bored_yield_duration, yieldValue))
	{
		TimeUnit unit;
		if (Property::StringToTime(yieldValue, _boredYieldDuration, unit) &&
					Property::ConvertTimeUnitToMS(_boredYieldDuration, unit, _boredYieldDuration))
		{
			logger_->log_debug("nifi_bored_yield_duration: [%d] ms", _boredYieldDuration);
		}
	}

	if (processor->getScheduledState() != RUNNING)
	{
		logger_->log_info("Can not schedule threads for processor %s because it is not running", processor->getName().c_str());
		return;
	}

	std::map<std::string, std::vector<std::thread *>>::iterator it =
			_threads.find(processor->getUUIDStr());
	if (it != _threads.end())
	{
		logger_->log_info("Can not schedule threads for processor %s because there are existing threads running");
		return;
	}

	auto processContext = std::make_shared<ProcessContext>(processor);
	auto sessionFactory = std::make_shared<ProcessSessionFactory>(processContext.get());

	processor->onSchedule(processContext.get(), sessionFactory.get());

	std::vector<std::thread *> threads;
	for (int i = 0; i < processor->getMaxConcurrentTasks(); i++)
	{
	    ThreadedSchedulingAgent *agent = this;
		std::thread *thread = new std::thread([agent, processor, processContext, sessionFactory] () {
			agent->run(processor, processContext.get(), sessionFactory.get());
		});
		thread->detach();
		threads.push_back(thread);
		logger_->log_info("Scheduled thread %d running for process %s", thread->get_id(),
				processor->getName().c_str());
	}
	_threads[processor->getUUIDStr().c_str()] = threads;

	return;
}

void ThreadedSchedulingAgent::unschedule(Processor *processor)
{
	std::lock_guard<std::mutex> lock(_mtx);
	
	logger_->log_info("Shutting down threads for processor %s/%s",
			processor->getName().c_str(),
			processor->getUUIDStr().c_str());

	if (processor->getScheduledState() != RUNNING)
	{
		logger_->log_info("Cannot unschedule threads for processor %s because it is not running", processor->getName().c_str());
		return;
	}

	std::map<std::string, std::vector<std::thread *>>::iterator it =
			_threads.find(processor->getUUIDStr());

	if (it == _threads.end())
	{
		logger_->log_info("Cannot unschedule threads for processor %s because there are no existing threads running", processor->getName().c_str());
		return;
	}
	for (std::vector<std::thread *>::iterator itThread = it->second.begin(); itThread != it->second.end(); ++itThread)
	{
		std::thread *thread = *itThread;
		logger_->log_info("Scheduled thread %d deleted for process %s", thread->get_id(),
				processor->getName().c_str());
		delete thread;
	}
	_threads.erase(processor->getUUIDStr());
	processor->clearActiveTask();
}
