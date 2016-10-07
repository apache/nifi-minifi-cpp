/**
 * @file Configure.h
 * Configure class declaration
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
#ifndef __CONFIGURE_H__
#define __CONFIGURE_H__

#include <stdio.h>
#include <string>
#include <map>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include "Logger.h"

class Configure {
public:
	//! Get the singleton logger instance
	static Configure * getConfigure()
	{
		if (!_configure)
		{
			_configure = new Configure();
		}
		return _configure;
	}
	//! nifi.flow.configuration.file
	static const char *nifi_flow_configuration_file;
	static const char *nifi_administrative_yield_duration;
	static const char *nifi_bored_yield_duration;
	static const char *nifi_server_name;
	static const char *nifi_server_port;
	static const char *nifi_server_report_interval;

	//! Clear the load config
	void clear()
	{
		std::lock_guard<std::mutex> lock(_mtx);
		_properties.clear();
	}
	//! Set the config value
	void set(std::string key, std::string value)
	{
		std::lock_guard<std::mutex> lock(_mtx);
		_properties[key] = value;
	}
	//! Check whether the config value existed
	bool has(std::string key)
	{
		std::lock_guard<std::mutex> lock(_mtx);
		return (_properties.find(key) != _properties.end());
	}
	//! Get the config value
	bool get(std::string key, std::string &value);
	// Trim String utils
	std::string trim(const std::string& s);
	std::string trimLeft(const std::string& s);
	std::string trimRight(const std::string& s);
	//! Parse one line in configure file like key=value
	void parseConfigureFileLine(char *buf);
	//! Load Configure File
	void loadConfigureFile(const char *fileName);
    //! Set the determined MINIFI_HOME
    void setHome(std::string minifiHome)
    {
        _minifiHome = minifiHome;
    }

    //! Get the determined MINIFI_HOME
    std::string getHome()
    {
        return _minifiHome;
    }
    //! Parse Command Line
    void parseCommandLine(int argc, char **argv);

private:
	//! Mutex for protection
	std::mutex _mtx;
	//! Logger
	Logger *_logger;
	//! Home location for this executable
	std::string _minifiHome;

	Configure()
	{
		_logger = Logger::getLogger();
	}
	virtual ~Configure()
	{

	}
	static Configure *_configure;

protected:
	std::map<std::string,std::string> _properties;
};

#endif
