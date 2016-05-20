/**
 * @file Connection.cpp
 * Connection class implementation
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
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <sys/time.h>
#include <time.h>
#include <chrono>
#include <thread>
#include <iostream>

#include "Connection.h"

Connection::Connection(std::string name, uuid_t uuid, uuid_t srcUUID, uuid_t destUUID)
: _name(name)
{
	if (!uuid)
		// Generate the global UUID for the flow record
		uuid_generate(_uuid);
	else
		uuid_copy(_uuid, uuid);

	if (srcUUID)
		uuid_copy(_srcUUID, srcUUID);
	if (destUUID)
		uuid_copy(_destUUID, destUUID);

	_srcProcessor = NULL;
	_destProcessor = NULL;
	_maxQueueSize = 0;
	_maxQueueDataSize = 0;
	_expiredDuration = 0;
	_queuedDataSize = 0;

	_logger = Logger::getLogger();

	_logger->log_info("Connection %s created", _name.c_str());
}

bool Connection::isEmpty()
{
	std::lock_guard<std::mutex> lock(_mtx);

	return _queue.empty();
}

bool Connection::isFull()
{
	std::lock_guard<std::mutex> lock(_mtx);

	if (_maxQueueSize <= 0 && _maxQueueDataSize <= 0)
		// No back pressure setting
		return false;

	if (_maxQueueSize > 0 && _queue.size() >= _maxQueueSize)
		return true;

	if (_maxQueueDataSize > 0 && _queuedDataSize >= _maxQueueDataSize)
		return true;

	return false;
}

void Connection::put(FlowFileRecord *flow)
{
	std::lock_guard<std::mutex> lock(_mtx);

	_queue.push(flow);

	_queuedDataSize += flow->getSize();

	_logger->log_debug("Enqueue flow file UUID %s to connection %s",
			flow->getUUIDStr().c_str(), _name.c_str());
}

FlowFileRecord* Connection::poll(std::set<FlowFileRecord *> &expiredFlowRecords)
{
	std::lock_guard<std::mutex> lock(_mtx);

	while (!_queue.empty())
	{
		FlowFileRecord *item = _queue.front();
		_queue.pop();
		_queuedDataSize -= item->getSize();

		if (_expiredDuration > 0)
		{
			// We need to check for flow expiration
			if (getTimeMillis() > (item->getEntryDate() + _expiredDuration))
			{
				// Flow record expired
				expiredFlowRecords.insert(item);
			}
			else
			{
				// Flow record not expired
				if (item->isPenalized())
				{
					// Flow record was penalized
					_queue.push(item);
					_queuedDataSize += item->getSize();
					break;
				}
				item->setOriginalConnection(this);
				_logger->log_debug("Dequeue flow file UUID %s from connection %s",
						item->getUUIDStr().c_str(), _name.c_str());
				return item;
			}
		}
		else
		{
			// Flow record not expired
			if (item->isPenalized())
			{
				// Flow record was penalized
				_queue.push(item);
				_queuedDataSize += item->getSize();
				break;
			}
			item->setOriginalConnection(this);
			_logger->log_debug("Dequeue flow file UUID %s from connection %s",
					item->getUUIDStr().c_str(), _name.c_str());
			return item;
		}
	}

	return NULL;
}
