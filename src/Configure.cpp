/**
 * @file Configure.cpp
 * Configure class implementation
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
#include "Configure.h"

Configure *Configure::_configure(NULL);
const char *Configure::nifi_flow_configuration_file = "nifi.flow.configuration.file";
const char *Configure::nifi_administrative_yield_duration = "nifi.administrative.yield.duration";
const char *Configure::nifi_bored_yield_duration = "nifi.bored.yield.duration";
const char *Configure::nifi_server_name = "nifi.server.name";
const char *Configure::nifi_server_port = "nifi.server.port";
const char *Configure::nifi_server_report_interval= "nifi.server.report.interval";


//! Get the config value
bool Configure::get(std::string key, std::string &value)
{
	std::lock_guard<std::mutex> lock(_mtx);
	std::map<std::string,std::string>::iterator it = _properties.find(key);

	if (it != _properties.end())
	{
		value = it->second;
		return true;
	}
	else
	{
		return false;
	}
}

// Trim String utils
std::string Configure::trim(const std::string& s)
{
    return trimRight(trimLeft(s));
}

std::string Configure::trimLeft(const std::string& s)
{
	const char *WHITESPACE = " \n\r\t";
    size_t startpos = s.find_first_not_of(WHITESPACE);
    return (startpos == std::string::npos) ? "" : s.substr(startpos);
}

std::string Configure::trimRight(const std::string& s)
{
	const char *WHITESPACE = " \n\r\t";
    size_t endpos = s.find_last_not_of(WHITESPACE);
    return (endpos == std::string::npos) ? "" : s.substr(0, endpos+1);
}

//! Parse one line in configure file like key=value
void Configure::parseConfigureFileLine(char *buf)
{
	char *line = buf;

    while ((line[0] == ' ') || (line[0] =='\t'))
    	++line;

    char first = line[0];
    if ((first == '\0') || (first == '#')  || (first == '\r') || (first == '\n') || (first == '='))
    {
    	return;
    }

    char *equal = strchr(line, '=');
    if (equal == NULL)
    {
    	return;
    }

    equal[0] = '\0';
    std::string key = line;

    equal++;
    while ((equal[0] == ' ') || (equal[0] == '\t'))
    	++equal;

    first = equal[0];
    if ((first == '\0') || (first == '\r') || (first== '\n'))
    {
    	return;
    }

    std::string value = equal;
    key = trimRight(key);
    value = trimRight(value);
    set(key, value);
}

//! Load Configure File
void Configure::loadConfigureFile(const char *fileName)
{

    std::string adjustedFilename;
    if (fileName)
    {
        // perform a naive determination if this is a relative path
        if (fileName[0] != '/')
        {
            adjustedFilename = adjustedFilename + _configure->getHome() + "/" + fileName;
        }
        else
        {
            adjustedFilename += fileName;
        }
    }
    char *path = NULL;
    char full_path[PATH_MAX];
    path = realpath(adjustedFilename.c_str(), full_path);
    _logger->log_info("Using configuration file located at %s", path);

    std::ifstream file(path, std::ifstream::in);
    if (!file.good())
    {
        _logger->log_error("load configure file failed %s", path);
        return;
    }
    this->clear();
    const unsigned int bufSize = 512;
    char buf[bufSize];
    for (file.getline(buf, bufSize); file.good(); file.getline(buf, bufSize))
    {
        parseConfigureFileLine(buf);
    }
}

//! Parse Command Line
void Configure::parseCommandLine(int argc, char **argv)
{
	int i;
	bool keyFound = false;
	std::string key, value;

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-' && argv[i][1] != '\0')
		{
			keyFound = true;
			key = &argv[i][1];
			continue;
		}
		if (keyFound)
		{
			value = argv[i];
			set(key,value);
			keyFound = false;
		}
	}
	return;
}
