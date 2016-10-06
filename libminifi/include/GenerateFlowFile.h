/**
 * @file GenerateFlowFile.h
 * GenerateFlowFile class declaration
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
#ifndef __GENERATE_FLOW_FILE_H__
#define __GENERATE_FLOW_FILE_H__

#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

//! GenerateFlowFile Class
class GenerateFlowFile : public Processor
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	GenerateFlowFile(std::string name, uuid_t uuid = NULL)
	: Processor(name, uuid)
	{
		_data = NULL;
		_dataSize = 0;
	}
	//! Destructor
	virtual ~GenerateFlowFile()
	{
		if (_data)
			delete[] _data;
	}
	//! Processor Name
	static const std::string ProcessorName;
	//! Supported Properties
	static Property FileSize;
	static Property BatchSize;
	static Property DataFormat;
	static Property UniqueFlowFiles;
	static const char *DATA_FORMAT_BINARY;
	static const char *DATA_FORMAT_TEXT;
	//! Supported Relationships
	static Relationship Success;
	//! Nest Callback Class for write stream
	class WriteCallback : public OutputStreamCallback
	{
		public:
		WriteCallback(char *data, uint64_t size)
		: _data(data), _dataSize(size) {}
		char *_data;
		uint64_t _dataSize;
		void process(std::ofstream *stream) {
			if (_data && _dataSize > 0)
				stream->write(_data, _dataSize);
		}
	};

public:
	//! OnTrigger method, implemented by NiFi GenerateFlowFile
	virtual void onTrigger(ProcessContext *context, ProcessSession *session);
	//! Initialize, over write by NiFi GenerateFlowFile
	virtual void initialize(void);

protected:

private:
	//! Generated data
	char * _data;
	//! Size of the generate data
	uint64_t _dataSize;
};

#endif
