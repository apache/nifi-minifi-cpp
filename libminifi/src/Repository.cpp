/**
 * @file Repository.cpp
 * Repository implemenatation 
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
#include <cstdint>
#include <vector>
#include <arpa/inet.h>
#include "io/DataStream.h"
#include "io/Serializable.h"
#include "Relationship.h"
#include "Logger.h"
#include "FlowController.h"
#include "Repository.h"
#include "Provenance.h"
#include "FlowFileRepository.h"

const char *Repository::RepositoryTypeStr[MAX_REPO_TYPE] = {"Provenace Repository", "FlowFile Repository"};
uint64_t Repository::_repoSize[MAX_REPO_TYPE] = {0, 0}; 

void Repository::start() {
	if (!_enable)
		return;
	if (this->_purgePeriod <= 0)
		return;
	if (_running)
		return;
	_running = true;
	logger_->log_info("%s Repository Monitor Thread Start", RepositoryTypeStr[_type]);
	_thread = new std::thread(run, this);
	_thread->detach();
}

void Repository::stop() {
	if (!_running)
		return;
	_running = false;
	logger_->log_info("%s Repository Monitor Thread Stop", RepositoryTypeStr[_type]);
}

void Repository::run(Repository *repo) {
#ifdef LEVELDB_SUPPORT
	// threshold for purge
	uint64_t purgeThreshold = repo->_maxPartitionBytes * 3 / 4;
	while (repo->_running) {
		std::this_thread::sleep_for(
				std::chrono::milliseconds(repo->_purgePeriod));
		uint64_t curTime = getTimeMillis();
		uint64_t size = repo->repoSize();
		if (size >= purgeThreshold) {
			std::vector<std::string> purgeList;
			leveldb::Iterator* it = repo->_db->NewIterator(
					leveldb::ReadOptions());
			if (repo->_type == PROVENANCE)
			{
				for (it->SeekToFirst(); it->Valid(); it->Next()) {
					ProvenanceEventRecord eventRead;
					std::string key = it->key().ToString();
					if (eventRead.DeSerialize((uint8_t *) it->value().data(),
						(int) it->value().size())) {
						if ((curTime - eventRead.getEventTime())
							> repo->_maxPartitionMillis)
							purgeList.push_back(key);
					} else {
						repo->logger_->log_debug(
							"NiFi %s retrieve event %s fail",
							RepositoryTypeStr[repo->_type],
							key.c_str());
						purgeList.push_back(key);
					}
				}
			}
			if (repo->_type == FLOWFILE)
			{
				for (it->SeekToFirst(); it->Valid(); it->Next()) {
					FlowFileEventRecord eventRead;
					std::string key = it->key().ToString();
					if (eventRead.DeSerialize((uint8_t *) it->value().data(),
						(int) it->value().size())) {
						if ((curTime - eventRead.getEventTime())
							> repo->_maxPartitionMillis)
							purgeList.push_back(key);
					} else {
						repo->logger_->log_debug(
							"NiFi %s retrieve event %s fail",
							RepositoryTypeStr[repo->_type],
							key.c_str());
						purgeList.push_back(key);
					}
				}
			}
			delete it;
			for (auto eventId : purgeList)
			{
				repo->logger_->log_info("Repository Repo %s Purge %s",
						RepositoryTypeStr[repo->_type],
						eventId.c_str());
				repo->Delete(eventId);
			}
		}
		if (size > repo->_maxPartitionBytes)
			repo->_repoFull = true;
		else
			repo->_repoFull = false;
	}
#endif
	return;
}

//! repoSize
uint64_t Repository::repoSize()
{
	_repoSize[_type] = 0;
	if (_type == PROVENANCE)
        {
		if (ftw(_directory.c_str(), repoSumProvenance, 1) != 0)
			_repoSize[_type] = 0;
	}
	if (_type == FLOWFILE)
        {
		if (ftw(_directory.c_str(), repoSumFlowFile, 1) != 0)
			_repoSize[_type] = 0;
	}
	return _repoSize[_type];
}

