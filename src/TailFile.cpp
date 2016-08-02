/**
 * @file TailFile.cpp
 * TailFile class implementation
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
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>

#include "TimeUtil.h"
#include "TailFile.h"
#include "ProcessContext.h"
#include "ProcessSession.h"

const std::string TailFile::ProcessorName("TailFile");
Property TailFile::FileName("File to Tail", "Fully-qualified filename of the file that should be tailed", "");
Property TailFile::StateFile("State File",
		"Specifies the file that should be used for storing state about what data has been ingested so that upon restart NiFi can resume from where it left off", "");
Relationship TailFile::Success("success", "All files are routed to success");

void TailFile::initialize()
{
	//! Set the supported properties
	std::set<Property> properties;
	properties.insert(FileName);
	properties.insert(StateFile);
	setSupportedProperties(properties);
	//! Set the supported relationships
	std::set<Relationship> relationships;
	relationships.insert(Success);
	setSupportedRelationships(relationships);
}

std::string TailFile::trimLeft(const std::string& s)
{
	const char *WHITESPACE = " \n\r\t";
    size_t startpos = s.find_first_not_of(WHITESPACE);
    return (startpos == std::string::npos) ? "" : s.substr(startpos);
}

std::string TailFile::trimRight(const std::string& s)
{
	const char *WHITESPACE = " \n\r\t";
    size_t endpos = s.find_last_not_of(WHITESPACE);
    return (endpos == std::string::npos) ? "" : s.substr(0, endpos+1);
}

void TailFile::parseStateFileLine(char *buf)
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

    if (key == "FILENAME")
    	this->_currentTailFileName = value;
    if (key == "POSITION")
    	this->_currentTailFilePosition = std::stoi(value);

    return;
}

void TailFile::recoverState()
{
	std::ifstream file(_stateFile.c_str(), std::ifstream::in);
	if (!file.good())
	{
		_logger->log_error("load state file failed %s", _stateFile.c_str());
		return;
	}
	const unsigned int bufSize = 512;
	char buf[bufSize];
	for (file.getline(buf,bufSize); file.good(); file.getline(buf,bufSize))
	{
		parseStateFileLine(buf);
	}
}

void TailFile::storeState()
{
	std::ofstream file(_stateFile.c_str());
	if (!file.is_open())
	{
		_logger->log_error("store state file failed %s", _stateFile.c_str());
		return;
	}
	file << "FILENAME=" << this->_currentTailFileName << "\n";
	file << "POSITION=" << this->_currentTailFilePosition << "\n";
	file.close();
}

static bool sortTailMatchedFileItem(TailMatchedFileItem i, TailMatchedFileItem j)
{
	return (i.modifiedTime < j.modifiedTime);
}
void TailFile::checkRollOver()
{
	struct stat statbuf;
	std::vector<TailMatchedFileItem> matchedFiles;
	std::string fullPath = this->_fileLocation + "/" + _currentTailFileName;

	if (stat(fullPath.c_str(), &statbuf) == 0)
	{
		if (statbuf.st_size > this->_currentTailFilePosition)
			// there are new input for the current tail file
			return;

		uint64_t modifiedTimeCurrentTailFile = ((uint64_t) (statbuf.st_mtime) * 1000);
		std::string pattern = _fileName;
		std::size_t found = _fileName.find_last_of(".");
		if (found != std::string::npos)
			pattern = _fileName.substr(0,found);
		DIR *d;
		d = opendir(this->_fileLocation.c_str());
		if (!d)
			return;
		while (1)
		{
			struct dirent *entry;
			entry = readdir(d);
			if (!entry)
				break;
			std::string d_name = entry->d_name;
			if (!(entry->d_type & DT_DIR))
			{
				std::string fileName = d_name;
				std::string fileFullName = this->_fileLocation + "/" + d_name;
				if (fileFullName.find(pattern) != std::string::npos && stat(fileFullName.c_str(), &statbuf) == 0)
				{
					if (((uint64_t) (statbuf.st_mtime) * 1000) >= modifiedTimeCurrentTailFile)
					{
						TailMatchedFileItem item;
						item.fileName = fileName;
						item.modifiedTime = ((uint64_t) (statbuf.st_mtime) * 1000);
						matchedFiles.push_back(item);
					}
				}
			}
		}
		closedir(d);

		// Sort the list based on modified time
		std::sort(matchedFiles.begin(), matchedFiles.end(), sortTailMatchedFileItem);
		for (std::vector<TailMatchedFileItem>::iterator it = matchedFiles.begin(); it!=matchedFiles.end(); ++it)
		{
			TailMatchedFileItem item = *it;
			if (item.fileName == _currentTailFileName)
			{
				++it;
				if (it!=matchedFiles.end())
				{
					TailMatchedFileItem nextItem = *it;
					_logger->log_info("TailFile File Roll Over from %s to %s", _currentTailFileName.c_str(), nextItem.fileName.c_str());
					_currentTailFileName = nextItem.fileName;
					_currentTailFilePosition = 0;
					storeState();
				}
				break;
			}
		}
	}
	else
		return;
}


void TailFile::onTrigger(ProcessContext *context, ProcessSession *session)
{
	std::string value;
	if (context->getProperty(FileName.getName(), value))
	{
		std::size_t found = value.find_last_of("/\\");
		this->_fileLocation = value.substr(0,found);
		this->_fileName = value.substr(found+1);
	}
	if (context->getProperty(StateFile.getName(), value))
	{
		_stateFile = value;
	}
	if (!this->_stateRecovered)
	{
		_stateRecovered = true;
		this->_currentTailFileName = _fileName;
		this->_currentTailFilePosition = 0;
		// recover the state if we have not done so
		this->recoverState();
	}
	checkRollOver();
	std::string fullPath = this->_fileLocation + "/" + _currentTailFileName;
	struct stat statbuf;
	if (stat(fullPath.c_str(), &statbuf) == 0)
	{
		if (statbuf.st_size <= this->_currentTailFilePosition)
			// there are no new input for the current tail file
		{
			context->yield();
			return;
		}
		FlowFileRecord *flowFile = session->create();
		if (!flowFile)
			return;
		std::size_t found = _currentTailFileName.find_last_of(".");
		std::string baseName = _currentTailFileName.substr(0,found);
		std::string extension = _currentTailFileName.substr(found+1);
		flowFile->updateAttribute(PATH, _fileLocation);
		flowFile->addAttribute(ABSOLUTE_PATH, fullPath);
		session->import(fullPath, flowFile, true, this->_currentTailFilePosition);
		session->transfer(flowFile, Success);
		_logger->log_info("TailFile %s for %d bytes", _currentTailFileName.c_str(), flowFile->getSize());
		std::string logName = baseName + "." + std::to_string(_currentTailFilePosition) + "-" +
				std::to_string(_currentTailFilePosition + flowFile->getSize()) + "." + extension;
		flowFile->updateAttribute(FILENAME, logName);
		this->_currentTailFilePosition += flowFile->getSize();
		storeState();
	}
}

