/**
 * @file TimerDrivenSchedulingAgent.cpp
 * TimerDrivenSchedulingAgent class implementation
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
#include <chrono>
#include <thread>
#include <iostream>
#include "Property.h"
#include "TimerDrivenSchedulingAgent.h"

void TimerDrivenSchedulingAgent::schedule(Processor *processor)
{
	std::lock_guard<std::mutex> lock(_mtx);

	_administrativeYieldDuration = 0;
	std::string yieldValue;

	if (_configure->get(Configure::nifi_administrative_yield_duration, yieldValue))
	{
		TimeUnit unit;
		if (Property::StringToTime(yieldValue, _administrativeYieldDuration, unit) &&
					Property::ConvertTimeUnitToMS(_administrativeYieldDuration, unit, _administrativeYieldDuration))
		{
			_logger->log_debug("nifi_administrative_yield_duration: [%d] ms", _administrativeYieldDuration);
		}
	}

	_boredYieldDuration = 0;
	if (_configure->get(Configure::nifi_bored_yield_duration, yieldValue))
	{
		TimeUnit unit;
		if (Property::StringToTime(yieldValue, _boredYieldDuration, unit) &&
					Property::ConvertTimeUnitToMS(_boredYieldDuration, unit, _boredYieldDuration))
		{
			_logger->log_debug("nifi_bored_yield_duration: [%d] ms", _boredYieldDuration);
		}
	}

	if (processor->getScheduledState() != RUNNING)
	{
		_logger->log_info("Can not schedule threads for processor %s because it is not running", processor->getName().c_str());
		return;
	}

	std::map<std::string, std::vector<std::thread *>>::iterator it =
			_threads.find(processor->getUUIDStr());
	if (it != _threads.end())
	{
		_logger->log_info("Can not schedule threads for processor %s because there are existed thread running");
		return;
	}

	std::vector<std::thread *> threads;
	for (int i = 0; i < processor->getMaxConcurrentTasks(); i++)
	{
		std::thread *thread = new std::thread(run, this, processor);
		thread->detach();
		threads.push_back(thread);
		_logger->log_info("Scheduled Time Driven thread %d running for process %s", thread->get_id(),
				processor->getName().c_str());
	}
	_threads[processor->getUUIDStr().c_str()] = threads;

	return;
}

void TimerDrivenSchedulingAgent::unschedule(Processor *processor)
{
	std::lock_guard<std::mutex> lock(_mtx);

	if (processor->getScheduledState() != RUNNING)
	{
		_logger->log_info("Can not unschedule threads for processor %s because it is not running", processor->getName().c_str());
		return;
	}

	std::map<std::string, std::vector<std::thread *>>::iterator it =
			_threads.find(processor->getUUIDStr());

	if (it == _threads.end())
	{
		_logger->log_info("Can not unschedule threads for processor %s because there are no existed thread running");
		return;
	}
	for (std::vector<std::thread *>::iterator itThread = it->second.begin(); itThread != it->second.end(); ++itThread)
	{
		std::thread *thread = *itThread;
		_logger->log_info("Scheduled Time Driven thread %d deleted for process %s", thread->get_id(),
				processor->getName().c_str());
		delete thread;
	}
	_threads.erase(processor->getUUIDStr());
	processor->clearActiveTask();

	return;
}

void TimerDrivenSchedulingAgent::run(TimerDrivenSchedulingAgent *agent, Processor *processor)
{
	while (agent->_running)
	{
		bool shouldYield = agent->onTrigger(processor);

		if (processor->isYield())
		{
			// Honor the yield
			std::this_thread::sleep_for(std::chrono::milliseconds(processor->getYieldTime()));
		}
		else if (shouldYield && agent->_boredYieldDuration > 0)
		{
			// No work to do or need to apply back pressure
			std::this_thread::sleep_for(std::chrono::milliseconds(agent->_boredYieldDuration));
		}
		std::this_thread::sleep_for(std::chrono::nanoseconds(processor->getSchedulingPeriodNano()));
	}
	return;
}


