/**
 * @file Processor.h
 * Processor class declaration
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
#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <set>

#include "TimeUtil.h"
#include "Property.h"
#include "Relationship.h"
#include "Connection.h"

//! Forwarder declaration
class ProcessContext;
class ProcessSession;

//! Minimum scheduling period in Nano Second
#define MINIMUM_SCHEDULING_NANOS 30000

//! Default yield period in second
#define DEFAULT_YIELD_PERIOD_SECONDS 1

//! Default penalization period in second
#define DEFAULT_PENALIZATION_PERIOD_SECONDS 30

/*!
 * Indicates the valid values for the state of a entity
 * with respect to scheduling the entity to run.
 */
enum ScheduledState {

    /**
     * Entity cannot be scheduled to run
     */
    DISABLED,
    /**
     * Entity can be scheduled to run but currently is not
     */
    STOPPED,
    /**
     * Entity is currently scheduled to run
     */
    RUNNING
};

/*!
 * Scheduling Strategy
 */
enum SchedulingStrategy {
	//! Event driven
	EVENT_DRIVEN,
	//! Timer driven
	TIMER_DRIVEN,
	//! Cron Driven
	CRON_DRIVEN
};

//! Processor Class
class Processor
{
	friend class ProcessContext;
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	Processor(std::string name, uuid_t uuid = NULL);
	//! Destructor
	virtual ~Processor();
	//! Set Processor Name
	void setName(std::string name) {
		_name = name;
	}
	//! Get Process Name
	std::string getName(void) {
		return (_name);
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
	//! Set the supported processor properties while the process is not running
	bool setSupportedProperties(std::set<Property> properties);
	//! Set the supported relationships while the process is not running
	bool setSupportedRelationships(std::set<Relationship> relationships);
	//! Get the supported property value by name
	bool getProperty(std::string name, std::string &value);
	//! Set the supported property value by name wile the process is not running
	bool setProperty(std::string name, std::string value);
	//! Whether the relationship is supported
	bool isSupportedRelationship(Relationship relationship);
	//! Set the auto terminated relationships while the process is not running
	bool setAutoTerminatedRelationships(std::set<Relationship> relationships);
	//! Check whether the relationship is auto terminated
	bool isAutoTerminated(Relationship relationship);
	//! Check whether the processor is running
	bool isRunning();
	//! Set Processor Scheduled State
	void setScheduledState(ScheduledState state) {
		_state = state;
	}
	//! Get Processor Scheduled State
	ScheduledState getScheduledState(void) {
		return _state;
	}
	//! Set Processor Scheduling Strategy
	void setSchedulingStrategy(SchedulingStrategy strategy) {
		_strategy = strategy;
	}
	//! Get Processor Scheduling Strategy
	SchedulingStrategy getSchedulingStrategy(void) {
		return _strategy;
	}
	//! Set Processor Loss Tolerant
	void setlossTolerant(bool lossTolerant) {
		_lossTolerant = lossTolerant;
	}
	//! Get Processor Loss Tolerant
	bool getlossTolerant(void) {
		return _lossTolerant;
	}
	//! Set Processor Scheduling Period in Nano Second
	void setSchedulingPeriodNano(uint64_t period) {
		uint64_t minPeriod = MINIMUM_SCHEDULING_NANOS;
		_schedulingPeriodNano = std::max(period, minPeriod);
	}
	//! Get Processor Scheduling Period in Nano Second
	uint64_t getSchedulingPeriodNano(void) {
		return _schedulingPeriodNano;
	}
	//! Set Processor Run Duration in Nano Second
	void setRunDurationNano(uint64_t period) {
		_runDurantionNano = period;
	}
	//! Get Processor Run Duration in Nano Second
	uint64_t getRunDurationNano(void) {
		return(_runDurantionNano);
	}
	//! Set Processor yield period in MilliSecond
	void setYieldPeriodMsec(uint64_t period) {
		_yieldPeriodMsec = period;
	}
	//! Get Processor yield period in MilliSecond
	uint64_t getYieldPeriodMsec(void) {
		return(_yieldPeriodMsec);
	}
	//! Set Processor penalization period in MilliSecond
	void setPenalizationPeriodMsec(uint64_t period) {
		_penalizationPeriodMsec = period;
	}
	//! Get Processor penalization period in MilliSecond
	uint64_t getPenalizationPeriodMsec(void) {
		return(_penalizationPeriodMsec);
	}
	//! Set Processor Maximum Concurrent Tasks
	void setMaxConcurrentTasks(uint8_t tasks) {
		_maxConcurrentTasks = tasks;
	}
	//! Get Processor Maximum Concurrent Tasks
	uint8_t getMaxConcurrentTasks(void) {
		return(_maxConcurrentTasks);
	}
	//! Set Trigger when empty
	void setTriggerWhenEmpty(bool value) {
		_triggerWhenEmpty = value;
	}
	//! Get Trigger when empty
	bool getTriggerWhenEmpty(void) {
		return(_triggerWhenEmpty);
	}
	//! Get Active Task Counts
	uint8_t getActiveTasks(void) {
		return(_activeTasks);
	}
	//! Yield based on the yield period
	void yield();
	//! Get incoming connections
	std::set<Connection *> getIncomingConnections() {
		return _incomingConnections;
	}
	//! Get outgoing connections based on relationship name
	std::set<Connection *> getOutGoingConnections(std::string relationship);
	//! Add connection
	bool addConnection(Connection *connection);
	//! Remove connection
	void removeConnection(Connection *connection);

public:
	//! OnTrigger method, implemented by NiFi Processor Designer
	virtual void onTrigger(ProcessContext *context, ProcessSession *session) = 0;
	//! Initialize, over write by NiFi Process Designer
	virtual void initialize(void) {
		return;
	}

protected:

	//! A global unique identifier
	uuid_t _uuid;
	//! Processor Name
	std::string _name;
	//! Supported properties
	std::map<std::string, Property> _properties;
	//! Supported relationships
	std::map<std::string, Relationship> _relationships;
	//! Autoterminated relationships
	std::map<std::string, Relationship> _autoTerminatedRelationships;
	//! Processor state
	std::atomic<ScheduledState> _state;
	//! Scheduling Strategy
	std::atomic<SchedulingStrategy> _strategy;
	//! lossTolerant
	std::atomic<bool> _lossTolerant;
	//! SchedulePeriod in Nano Seconds
	std::atomic<uint64_t> _schedulingPeriodNano;
	//! Run Duration in Nano Seconds
	std::atomic<uint64_t> _runDurantionNano;
	//! Yield Period in Milliseconds
	std::atomic<uint64_t> _yieldPeriodMsec;
	//! Penalization Period in MilliSecond
	std::atomic<uint64_t> _penalizationPeriodMsec;
	//! Maximum Concurrent Tasks
	std::atomic<uint8_t> _maxConcurrentTasks;
	//! Active Tasks
	std::atomic<uint8_t> _activeTasks;
	//! Trigger the Processor even if the incoming connection is empty
	std::atomic<bool> _triggerWhenEmpty;
	//! Incoming connections
	std::set<Connection *> _incomingConnections;
	//! Outgoing connections map based on Relationship name
	std::map<std::string, std::set<Connection *>> _outGoingConnections;

private:

	//! Mutex for protection
	std::mutex _mtx;
	//! Yield Expiration
	std::atomic<uint64_t> _yieldExpiration;
	//! Logger
	Logger *_logger;
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	Processor(const Processor &parent);
	Processor &operator=(const Processor &parent);

};

#endif
