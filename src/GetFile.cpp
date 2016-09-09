/**
 * @file GetFile.cpp
 * GetFile class implementation
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
#include <regex>

#include "TimeUtil.h"
#include "GetFile.h"
#include "ProcessContext.h"
#include "ProcessSession.h"

const std::string GetFile::ProcessorName("GetFile");
Property GetFile::BatchSize("Batch Size", "The maximum number of files to pull in each iteration", "10");
Property GetFile::Directory("Input Directory", "The input directory from which to pull files", ".");
Property GetFile::IgnoreHiddenFile("Ignore Hidden Files", "Indicates whether or not hidden files should be ignored", "true");
Property GetFile::KeepSourceFile("Keep Source File",
		"If true, the file is not deleted after it has been copied to the Content Repository", "false");
Property GetFile::MaxAge("Maximum File Age",
		"The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored", "0 sec");
Property GetFile::MinAge("Minimum File Age",
		"The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored", "0 sec");
Property GetFile::MaxSize("Maximum File Size", "The maximum size that a file can be in order to be pulled", "0 B");
Property GetFile::MinSize("Minimum File Size", "The minimum size that a file must be in order to be pulled", "0 B");
Property GetFile::PollInterval("Polling Interval", "Indicates how long to wait before performing a directory listing", "0 sec");
Property GetFile::Recurse("Recurse Subdirectories", "Indicates whether or not to pull files from subdirectories", "true");
Property GetFile::FileFilter("File Filter", "Only files whose names match the given regular expression will be picked up", "[^\\.].*");
Relationship GetFile::Success("success", "All files are routed to success");

void GetFile::initialize()
{
	//! Set the supported properties
	std::set<Property> properties;
	properties.insert(BatchSize);
	properties.insert(Directory);
	properties.insert(IgnoreHiddenFile);
	properties.insert(KeepSourceFile);
	properties.insert(MaxAge);
	properties.insert(MinAge);
	properties.insert(MaxSize);
	properties.insert(MinSize);
	properties.insert(PollInterval);
	properties.insert(Recurse);
	properties.insert(FileFilter);
	setSupportedProperties(properties);
	//! Set the supported relationships
	std::set<Relationship> relationships;
	relationships.insert(Success);
	setSupportedRelationships(relationships);
}

void GetFile::onTrigger(ProcessContext *context, ProcessSession *session)
{
	std::string value;
	if (context->getProperty(Directory.getName(), value))
	{
		_directory = value;
	}
	if (context->getProperty(BatchSize.getName(), value))
	{
		Property::StringToInt(value, _batchSize);
	}
	if (context->getProperty(IgnoreHiddenFile.getName(), value))
	{
		Property::StringToBool(value, _ignoreHiddenFile);
	}
	if (context->getProperty(KeepSourceFile.getName(), value))
	{
		Property::StringToBool(value, _keepSourceFile);
	}
	if (context->getProperty(MaxAge.getName(), value))
	{
		TimeUnit unit;
		if (Property::StringToTime(value, _maxAge, unit) &&
			Property::ConvertTimeUnitToMS(_maxAge, unit, _maxAge))
		{

		}
	}
	if (context->getProperty(MinAge.getName(), value))
	{
		TimeUnit unit;
		if (Property::StringToTime(value, _minAge, unit) &&
			Property::ConvertTimeUnitToMS(_minAge, unit, _minAge))
		{

		}
	}
	if (context->getProperty(MaxSize.getName(), value))
	{
		Property::StringToInt(value, _maxSize);
	}
	if (context->getProperty(MinSize.getName(), value))
	{
		Property::StringToInt(value, _minSize);
	}
	if (context->getProperty(PollInterval.getName(), value))
	{
		TimeUnit unit;
		if (Property::StringToTime(value, _pollInterval, unit) &&
			Property::ConvertTimeUnitToMS(_pollInterval, unit, _pollInterval))
		{

		}
	}
	if (context->getProperty(Recurse.getName(), value))
	{
		Property::StringToBool(value, _recursive);
	}

	if (context->getProperty(FileFilter.getName(), value))
	{
		_fileFilter = value;
	}

	// Perform directory list
	if (isListingEmpty())
	{
		if (_pollInterval == 0 || (getTimeMillis() - _lastDirectoryListingTime) > _pollInterval)
		{
			performListing(_directory);
		}
	}

	if (!isListingEmpty())
	{
		try
		{
			std::queue<std::string> list;
			pollListing(list, _batchSize);
			while (!list.empty())
			{
				std::string fileName = list.front();
				list.pop();
				_logger->log_info("GetFile process %s", fileName.c_str());
				FlowFileRecord *flowFile = session->create();
				if (!flowFile)
					return;
				std::size_t found = fileName.find_last_of("/\\");
				std::string path = fileName.substr(0,found);
				std::string name = fileName.substr(found+1);
				flowFile->updateAttribute(FILENAME, name);
				flowFile->updateAttribute(PATH, path);
				flowFile->addAttribute(ABSOLUTE_PATH, fileName);
				session->import(fileName, flowFile, _keepSourceFile);
				session->transfer(flowFile, Success);
			}
		}
		catch (std::exception &exception)
		{
			_logger->log_debug("GetFile Caught Exception %s", exception.what());
			throw;
		}
		catch (...)
		{
			throw;
		}
	}
}

bool GetFile::isListingEmpty()
{
	std::lock_guard<std::mutex> lock(_mtx);

	return _dirList.empty();
}

void GetFile::putListing(std::string fileName)
{
	std::lock_guard<std::mutex> lock(_mtx);

	_dirList.push(fileName);
}

void GetFile::pollListing(std::queue<std::string> &list, int maxSize)
{
	std::lock_guard<std::mutex> lock(_mtx);

	while (!_dirList.empty() && (maxSize == 0 || list.size() < maxSize))
	{
		std::string fileName = _dirList.front();
		_dirList.pop();
		list.push(fileName);
	}

	return;
}

bool GetFile::acceptFile(std::string fileName)
{
	struct stat statbuf;

	if (stat(fileName.c_str(), &statbuf) == 0)
	{
		if (_minSize > 0 && statbuf.st_size <_minSize)
			return false;

		if (_maxSize > 0 && statbuf.st_size > _maxSize)
			return false;

		uint64_t modifiedTime = ((uint64_t) (statbuf.st_mtime) * 1000);
		uint64_t fileAge = getTimeMillis() - modifiedTime;
		if (_minAge > 0 && fileAge < _minAge)
			return false;
		if (_maxAge > 0 && fileAge > _maxAge)
			return false;

		if (_ignoreHiddenFile && fileName.c_str()[0] == '.')
			return false;

		if (access(fileName.c_str(), R_OK) != 0)
			return false;

		if (_keepSourceFile == false && access(fileName.c_str(), W_OK) != 0)
			return false;

		try {
			std::regex re(_fileFilter);
			if (!std::regex_match(fileName, re)) {
				return false;
	   		}
		} catch (std::regex_error e) {
			_logger->log_error("Invalid File Filter regex: %s.", e.what());
			return false;
		}

		return true;
	}

	return false;
}

void GetFile::performListing(std::string dir)
{
	DIR *d;
	d = opendir(dir.c_str());
	if (!d)
		return;
	while (1)
	{
		struct dirent *entry;
		entry = readdir(d);
		if (!entry)
			break;
		std::string d_name = entry->d_name;
		if ((entry->d_type & DT_DIR))
		{
			// if this is a directory
			if (_recursive && strcmp(d_name.c_str(), "..") != 0 && strcmp(d_name.c_str(), ".") != 0)
			{
				std::string path = dir + "/" + d_name;
				performListing(path);
			}
		}
		else
		{
			std::string fileName = dir + "/" + d_name;
			if (acceptFile(fileName))
			{
				// check whether we can take this file
				putListing(fileName);
			}
		}
	}
	closedir(d);
}
