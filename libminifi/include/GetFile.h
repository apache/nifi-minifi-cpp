/**
 * @file GetFile.h
 * GetFile class declaration
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
#ifndef __GET_FILE_H__
#define __GET_FILE_H__

#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

//! GetFile Class
class GetFile : public Processor
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	GetFile(std::string name, uuid_t uuid = NULL)
	: Processor(name, uuid)
	{
		logger_ = Logger::getLogger();
		_directory = ".";
		_recursive = true;
		_keepSourceFile = false;
		_minAge = 0;
		_maxAge = 0;
		_minSize = 0;
		_maxSize = 0;
		_ignoreHiddenFile = true;
		_pollInterval = 0;
		_batchSize = 10;
		_lastDirectoryListingTime = getTimeMillis();
		_fileFilter = "[^\\.].*";
	}
	//! Destructor
	virtual ~GetFile()
	{
	}
	//! Processor Name
	static const std::string ProcessorName;
	//! Supported Properties
	static Property Directory;
	static Property Recurse;
	static Property KeepSourceFile;
	static Property MinAge;
	static Property MaxAge;
	static Property MinSize;
	static Property MaxSize;
	static Property IgnoreHiddenFile;
	static Property PollInterval;
	static Property BatchSize;
	static Property FileFilter;
	//! Supported Relationships
	static Relationship Success;

public:
	//! OnTrigger method, implemented by NiFi GetFile
	virtual void onTrigger(ProcessContext *context, ProcessSession *session);
	//! Initialize, over write by NiFi GetFile
	virtual void initialize(void);
	//! perform directory listing
	void performListing(std::string dir);

protected:

private:
	//! Logger
	std::shared_ptr<Logger> logger_;
	//! Queue for store directory list
	std::queue<std::string> _dirList;
	//! Get Listing size
	uint64_t getListingSize() {
		std::lock_guard<std::mutex> lock(_mtx);
		return _dirList.size();
	}
	//! Whether the directory listing is empty
	bool isListingEmpty();
	//! Put full path file name into directory listing
	void putListing(std::string fileName);
	//! Poll directory listing for files
	void pollListing(std::queue<std::string> &list, int maxSize);
	//! Check whether file can be added to the directory listing
	bool acceptFile(std::string fullName, std::string name);
	//! Mutex for protection of the directory listing
	std::mutex _mtx;
	std::string _directory;
	bool _recursive;
	bool _keepSourceFile;
	int64_t _minAge;
	int64_t _maxAge;
	int64_t _minSize;
	int64_t _maxSize;
	bool _ignoreHiddenFile;
	int64_t _pollInterval;
	int64_t _batchSize;
	uint64_t _lastDirectoryListingTime;
	std::string _fileFilter;
};

#endif
