/**
 * @file Exception.h
 * Exception class declaration
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
#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <sstream>
#include <exception>
#include <stdexcept>
#include <errno.h>
#include <string.h>

//! ExceptionType 
enum ExceptionType 
{
	FILE_OPERATION_EXCEPTION = 0,
	FLOW_EXCEPTION,
	PROCESSOR_EXCEPTION,
	PROCESS_SESSION_EXCEPTION,
	PROCESS_SCHEDULE_EXCEPTION,
	GENERAL_EXCEPTION,
	MAX_EXCEPTION
};

//! Exception String 
static const char *ExceptionStr[MAX_EXCEPTION] =
{
		"File Operation",
		"Flow File Operation",
		"Processor Operation",
		"Process Session Operation",
		"Process Schedule Operation",
		"General Operation"
};

//! Exception Type to String 
inline const char *ExceptionTypeToString(ExceptionType type)
{
	if (type < MAX_EXCEPTION)
		return ExceptionStr[type];
	else
		return NULL;
}

//! Exception Class
class Exception : public std::exception
{
public:
	//! Constructor
	/*!
	 * Create a new flow record
	 */
	Exception(ExceptionType type, const char *errorMsg) : _type(type), _errorMsg(errorMsg) {
	}
	//! Destructor
	virtual ~Exception() throw () {}
	virtual const char * what() const throw () {

		_whatStr = ExceptionTypeToString(_type);

		_whatStr += ":" + _errorMsg;
		return _whatStr.c_str();
	}

protected:

private:
	//! Exception type
	ExceptionType _type;
	//! Exception detailed information
	std::string _errorMsg;
	//! Hold the what result
	mutable std::string _whatStr;

};

#endif
