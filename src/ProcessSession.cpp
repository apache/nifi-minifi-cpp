/**
 * @file ProcessSession.cpp
 * ProcessSession class implementation
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

#include "ProcessSession.h"

FlowFileRecord* ProcessSession::create()
{
	std::map<std::string, std::string> empty;
	FlowFileRecord *record = new FlowFileRecord(empty);

	if (record)
	{
		_addedFlowFiles[record->getUUIDStr()] = record;
		_logger->log_debug("Create FlowFile with UUID %s", record->getUUIDStr().c_str());
	}

	return record;
}

FlowFileRecord* ProcessSession::create(FlowFileRecord *parent)
{
	FlowFileRecord *record = this->create();
	if (record)
	{
		// Copy attributes
		std::map<std::string, std::string> parentAttributes = parent->getAttributes();
	    std::map<std::string, std::string>::iterator it;
	    for (it = parentAttributes.begin(); it!= parentAttributes.end(); it++)
	    {
	    	if (it->first == FlowAttributeKey(ALTERNATE_IDENTIFIER) ||
	    			it->first == FlowAttributeKey(DISCARD_REASON) ||
					it->first == FlowAttributeKey(UUID))
	    		// Do not copy special attributes from parent
	    		continue;
	    	record->setAttribute(it->first, it->second);
	    }
	    record->_lineageStartDate = parent->_lineageStartDate;
	    record->_lineageIdentifiers = parent->_lineageIdentifiers;
	    record->_lineageIdentifiers.insert(parent->_uuidStr);

	}
	return record;
}

FlowFileRecord* ProcessSession::clone(FlowFileRecord *parent)
{
	FlowFileRecord *record = this->create(parent);
	if (record)
	{
		// Copy Resource Claim
		record->_claim = parent->_claim;
		if (record->_claim)
		{
			record->_offset = parent->_offset;
			record->_size = parent->_size;
			record->_claim->increaseFlowFileRecordOwnedCount();
		}
	}
	return record;
}

FlowFileRecord* ProcessSession::clone(FlowFileRecord *parent, long offset, long size)
{
	FlowFileRecord *record = this->create(parent);
	if (record)
	{
		if (parent->_claim)
		{
			if ((offset + size) > parent->_size)
			{
				// Set offset and size
				_logger->log_error("clone offset %d and size %d exceed parent size %d",
						offset, size, parent->_size);
				// Remove the Add FlowFile for the session
				std::map<std::string, FlowFileRecord *>::iterator it =
						this->_addedFlowFiles.find(record->getUUIDStr());
				if (it != this->_addedFlowFiles.end())
					this->_addedFlowFiles.erase(record->getUUIDStr());
				delete record;
				return NULL;
			}
			record->_offset = parent->_offset + parent->_offset;
			record->_size = size;
			// Copy Resource Claim
			record->_claim = parent->_claim;
			record->_claim->increaseFlowFileRecordOwnedCount();
		}
	}
	return record;
}

void ProcessSession::remove(FlowFileRecord *flow)
{
	flow->_markedDelete = true;
	_deletedFlowFiles[flow->getUUIDStr()] = flow;
}

void ProcessSession::putAttribute(FlowFileRecord *flow, std::string key, std::string value)
{
	flow->setAttribute(key, value);
}

void ProcessSession::removeAttribute(FlowFileRecord *flow, std::string key)
{
	flow->removeAttribute(key);
}

void ProcessSession::penalize(FlowFileRecord *flow)
{
	flow->_penaltyExpirationMs = getTimeMillis() + this->_processContext->getProcessor()->getPenalizationPeriodMsec();
}

void ProcessSession::transfer(FlowFileRecord *flow, Relationship relationship)
{
	_transferRelationship[flow->getUUIDStr()] = relationship;
}

void ProcessSession::write(FlowFileRecord *flow, OutputStreamCallback callback)
{
	ResourceClaim *claim = NULL;

	claim = new ResourceClaim(DEFAULT_CONTENT_DIRECTORY);

	try
	{
		std::ofstream fs;
		fs.open(claim->getContentFullPath().c_str(), std::fstream::out | std::fstream::binary | std::fstream::trunc);
		if (fs.is_open())
		{
			// Call the callback to write the content
			callback(&fs);
			if (fs.good() && fs.tellp() >= 0)
			{
				flow->_size = fs.tellp();
				flow->_offset = 0;
				if (flow->_claim)
				{
					// Remove the old claim
					flow->_claim->decreaseFlowFileRecordOwnedCount();
					flow->_claim = NULL;
				}
				flow->_claim = claim;
				_logger->log_debug("Write offset %d length %d into content %s for FlowFile UUID %s",
						flow->_offset, flow->_size, flow->_claim->getContentFullPath().c_str(), flow->getUUIDStr().c_str());
				fs.close();
			}
			else
			{
				fs.close();
				throw Exception(FILE_OPERATION_EXCEPTION, "File Write Error");
			}
		}
		else
		{
			throw Exception(FILE_OPERATION_EXCEPTION, "File Open Error");
		}
	}
	catch (std::exception &exception)
	{
		if (claim)
			delete claim;
		_logger->log_debug("Caught Exception %s", exception.what());
		throw;
	}
	catch (...)
	{
		if (claim)
			delete claim;
		_logger->log_debug("Caught Exception during process session write");
		throw;
	}
}

void ProcessSession::append(FlowFileRecord *flow, OutputStreamCallback callback)
{
	ResourceClaim *claim = NULL;

	if (flow->_claim == NULL)
	{
		// No existed claim for append, we need to create new claim
		return write(flow, callback);
	}

	claim = flow->_claim;

	try
	{
		std::ofstream fs;
		fs.open(claim->getContentFullPath().c_str(), std::fstream::out | std::fstream::binary | std::fstream::app);
		if (fs.is_open())
		{
			// Call the callback to write the content
			std::streampos oldPos = fs.tellp();
			callback(&fs);
			if (fs.good() && fs.tellp() >= 0)
			{
				uint64_t appendSize = fs.tellp() - oldPos;
				flow->_size += appendSize;
				_logger->log_debug("Append offset %d extra length %d to new size %d into content %s for FlowFile UUID %s",
						flow->_offset, appendSize, flow->_size, claim->getContentFullPath().c_str(), flow->getUUIDStr().c_str());
				fs.close();
			}
			else
			{
				fs.close();
				throw Exception(FILE_OPERATION_EXCEPTION, "File Write Error");
			}
		}
		else
		{
			throw Exception(FILE_OPERATION_EXCEPTION, "File Open Error");
		}
	}
	catch (std::exception &exception)
	{
		if (claim)
			delete claim;
		_logger->log_debug("Caught Exception %s", exception.what());
		throw;
	}
	catch (...)
	{
		if (claim)
			delete claim;
		_logger->log_debug("Caught Exception during process session append");
		throw;
	}
}

void ProcessSession::read(FlowFileRecord *flow, InputStreamCallback callback)
{
	try
	{
		ResourceClaim *claim = NULL;
		if (flow->_claim == NULL)
		{
			// No existed claim for read, we throw exception
			throw Exception(FILE_OPERATION_EXCEPTION, "No Content Claim existed for read");
		}

		claim = flow->_claim;
		std::ifstream fs;
		fs.open(claim->getContentFullPath().c_str(), std::fstream::in | std::fstream::binary);
		if (fs.is_open())
		{
			fs.seekg(flow->_offset, fs.beg);

			if (fs.good())
			{
				callback(&fs);
				_logger->log_debug("Read offset %d size %d content %s for FlowFile UUID %s",
						flow->_offset, flow->_size, claim->getContentFullPath().c_str(), flow->getUUIDStr().c_str());
				fs.close();
			}
			else
			{
				fs.close();
				throw Exception(FILE_OPERATION_EXCEPTION, "File Read Error");
			}
		}
		else
		{
			throw Exception(FILE_OPERATION_EXCEPTION, "File Open Error");
		}
	}
	catch (std::exception &exception)
	{
		_logger->log_debug("Caught Exception %s", exception.what());
		throw;
	}
	catch (...)
	{
		_logger->log_debug("Caught Exception during process session read");
		throw;
	}
}

void ProcessSession::commit()
{
	try
	{
		// First we clone the flow record based on the transfered relationship for updated flow record
		std::map<std::string, FlowFileRecord *>::iterator it;
		for (it = _updatedFlowFiles.begin(); it!= _updatedFlowFiles.end(); it++)
		{
			FlowFileRecord *record = it->second;
			if (record->_markedDelete)
				continue;
			std::map<std::string, Relationship>::iterator itRelationship =
					this->_transferRelationship.find(record->getUUIDStr());
			if (itRelationship != _transferRelationship.end())
			{
				Relationship relationship = itRelationship->second;
				// Find the relationship, we need to find the connections for that relationship
				std::set<Connection *> connections =
						_processContext->getProcessor()->getOutGoingConnections(relationship.getName());
				if (connections.empty())
				{
					// No connection
					if (!_processContext->getProcessor()->isAutoTerminated(relationship))
					{
						// Not autoterminate, we should have the connect
						throw Exception(PROCESS_SESSION_EXCEPTION, "Connect empty for non auto terminated relationship");
					}
					else
					{
						// Autoterminated
						record->_markedDelete = true;
					}
				}
				else
				{
					// We connections, clone the flow and assign the connection accordingly
					for (std::set<Connection *>::iterator itConnection = connections.begin(); itConnection != connections.end(); ++itConnection)
					{
						Connection *connection(*itConnection);
						if (itConnection == connections.begin())
						{
							// First connection which the flow need be routed to
							record->_connection = connection;
						}
						else
						{
							// Clone the flow file and route to the connection
							FlowFileRecord *cloneFlow = clone(record);
							cloneFlow->_connection = connection;
							this->_clonedFlowFiles[cloneFlow->getUUIDStr()] = cloneFlow;
						}
					}
				}
			}
			else
			{
				// Can not find relationship for the flow
				throw Exception(PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the flow");
			}
		}

		// Do the samething for added flow file
		for (it = _addedFlowFiles.begin(); it!= _addedFlowFiles.end(); it++)
		{
			FlowFileRecord *record = it->second;
			if (record->_markedDelete)
				continue;
			std::map<std::string, Relationship>::iterator itRelationship =
					this->_transferRelationship.find(record->getUUIDStr());
			if (itRelationship != _transferRelationship.end())
			{
				Relationship relationship = itRelationship->second;
				// Find the relationship, we need to find the connections for that relationship
				std::set<Connection *> connections =
						_processContext->getProcessor()->getOutGoingConnections(relationship.getName());
				if (connections.empty())
				{
					// No connection
					if (!_processContext->getProcessor()->isAutoTerminated(relationship))
					{
						// Not autoterminate, we should have the connect
						throw Exception(PROCESS_SESSION_EXCEPTION, "Connect empty for non auto terminated relationship");
					}
					else
					{
						// Autoterminated
						record->_markedDelete = true;
					}
				}
				else
				{
					// We connections, clone the flow and assign the connection accordingly
					for (std::set<Connection *>::iterator itConnection = connections.begin(); itConnection != connections.end(); ++itConnection)
					{
						Connection *connection(*itConnection);
						if (itConnection == connections.begin())
						{
							// First connection which the flow need be routed to
							record->_connection = connection;
						}
						else
						{
							// Clone the flow file and route to the connection
							FlowFileRecord *cloneFlow = clone(record);
							cloneFlow->_connection = connection;
							this->_clonedFlowFiles[cloneFlow->getUUIDStr()] = cloneFlow;
						}
					}
				}
			}
			else
			{
				// Can not find relationship for the flow
				throw Exception(PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the flow");
			}
		}

		// Complete process the added and update flow files for the session, send the flow file to its queue
		for (it = _updatedFlowFiles.begin(); it!= _updatedFlowFiles.end(); it++)
		{
			FlowFileRecord *record = it->second;
			if (record->_markedDelete)
			{
				delete record;
				continue;
			}
			if (record->_connection)
				record->_connection->put(record);
		}
		for (it = _addedFlowFiles.begin(); it!= _addedFlowFiles.end(); it++)
		{
			FlowFileRecord *record = it->second;
			if (record->_markedDelete)
			{
				delete record;
				continue;
			}
			if (record->_connection)
				record->_connection->put(record);
		}
		// Process the clone flow files
		for (it = _clonedFlowFiles.begin(); it!= _clonedFlowFiles.end(); it++)
		{
			FlowFileRecord *record = it->second;
			if (record->_markedDelete)
			{
				delete record;
				continue;
			}
			if (record->_connection)
				record->_connection->put(record);
		}
		// Delete the deleted flow files
		for (it = _deletedFlowFiles.begin(); it!= _deletedFlowFiles.end(); it++)
		{
			FlowFileRecord *record = it->second;
			delete record;
		}
		// Delete the snapshot
		for (it = _originalFlowFiles.begin(); it!= _originalFlowFiles.end(); it++)
		{
			FlowFileRecord *record = it->second;
			delete record;
		}
		_logger->log_trace("ProcessSession committed for %s", _processContext->getProcessor()->getName().c_str());
	}
	catch (std::exception &exception)
	{
		_logger->log_debug("Caught Exception %s", exception.what());
		throw;
	}
	catch (...)
	{
		_logger->log_debug("Caught Exception during process session commit");
		throw;
	}
}


void ProcessSession::rollback()
{
	try
	{
		std::map<std::string, FlowFileRecord *>::iterator it;
		// Requeue the snapshot of the flowfile back
		for (it = _originalFlowFiles.begin(); it!= _originalFlowFiles.end(); it++)
		{
			FlowFileRecord *record = it->second;
			if (record->_orginalConnection)
				record->_orginalConnection->put(record);
			else
				delete record;
		}
		// Process the clone flow files
		for (it = _clonedFlowFiles.begin(); it!= _clonedFlowFiles.end(); it++)
		{
			FlowFileRecord *record = it->second;
			delete record;
		}
		for (it = _addedFlowFiles.begin(); it!= _addedFlowFiles.end(); it++)
		{
			FlowFileRecord *record = it->second;
			delete record;
		}
		for (it = _updatedFlowFiles.begin(); it!= _updatedFlowFiles.end(); it++)
		{
			FlowFileRecord *record = it->second;
			delete record;
		}
		_logger->log_trace("ProcessSession rollback for %s", _processContext->getProcessor()->getName().c_str());
	}
	catch (std::exception &exception)
	{
		_logger->log_debug("Caught Exception %s", exception.what());
		throw;
	}
	catch (...)
	{
		_logger->log_debug("Caught Exception during process session roll back");
		throw;
	}
}

