/**
 * @file ExecuteProcess.h
 * ExecuteProcess class declaration
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
#ifndef __EXECUTE_PROCESS_H__
#define __EXECUTE_PROCESS_H__

#include <stdio.h>
#include <unistd.h>
#include <string>
#include <errno.h>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <sys/types.h>
#include <signal.h>
#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

//! ExecuteProcess Class
class ExecuteProcess : public Processor
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	ExecuteProcess(std::string name, uuid_t uuid = NULL)
	: Processor(name, uuid)
	{
		_logger = Logger::getLogger();
		_redirectErrorStream = false;
		_batchDuration = 0;
		_workingDir = ".";
		_processRunning = false;
		_pid = 0;
	}
	//! Destructor
	virtual ~ExecuteProcess()
	{
		if (_processRunning && _pid > 0)
			kill(_pid, SIGTERM);
	}
	//! Processor Name
	static const std::string ProcessorName;
	//! Supported Properties
	static Property Command;
	static Property CommandArguments;
	static Property WorkingDir;
	static Property BatchDuration;
	static Property RedirectErrorStream;
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
	//! OnTrigger method, implemented by NiFi ExecuteProcess
	virtual void onTrigger(ProcessContext *context, ProcessSession *session);
	//! Initialize, over write by NiFi ExecuteProcess
	virtual void initialize(void);

protected:

private:
	//! Logger
	Logger *_logger;
	//! Property
	std::string _command;
	std::string _commandArgument;
	std::string _workingDir;
	int64_t _batchDuration;
	bool _redirectErrorStream;
	//! Full command
	std::string _fullCommand;
	//! whether the process is running
	bool _processRunning;
	int _pipefd[2];
	pid_t _pid;
};

#endif
