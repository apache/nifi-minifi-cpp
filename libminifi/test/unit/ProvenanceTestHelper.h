/**
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
#ifndef LIBMINIFI_TEST_UNIT_PROVENANCETESTHELPER_H_
#define LIBMINIFI_TEST_UNIT_PROVENANCETESTHELPER_H_

#include "Provenance.h"
#include "FlowController.h"
#include "FlowFileRepository.h"

/**
 * Test repository
 */
class FlowTestRepository : public FlowFileRepository
{
public:
	FlowTestRepository()
{
}
		//! initialize
		bool initialize()
		{
			return true;
		}

		//! Destructor
		virtual ~FlowTestRepository() {

		}

		bool Put(std::string key, uint8_t *buf, int bufLen)
		{
			repositoryResults.insert(std::pair<std::string,std::string>(key,std::string((const char*)buf,bufLen)));
			return true;
		}
		//! Delete
		bool Delete(std::string key)
		{
			repositoryResults.erase(key);
			return true;
		}
		//! Get
		bool Get(std::string key, std::string &value)
		{
			auto result = repositoryResults.find(key);
			if (result != repositoryResults.end())
			{
				value = result->second;
				return true;
			}
			else
			{
				return false;
			}
		}

		const std::map<std::string,std::string> &getRepoMap() const
		{
			return repositoryResults;
		}

protected:
		std::map<std::string,std::string> repositoryResults;
};

/**
 * Test repository
 */
class ProvenanceTestRepository : public ProvenanceRepository
{
public:
	ProvenanceTestRepository()
{
}
		//! initialize
		bool initialize()
		{
			return true;
		}

		//! Destructor
		virtual ~ProvenanceTestRepository() {

		}

		bool Put(std::string key, uint8_t *buf, int bufLen)
		{
			repositoryResults.insert(std::pair<std::string,std::string>(key,std::string((const char*)buf,bufLen)));
			return true;
		}
		//! Delete
		bool Delete(std::string key)
		{
			repositoryResults.erase(key);
			return true;
		}
		//! Get
		bool Get(std::string key, std::string &value)
		{
			auto result = repositoryResults.find(key);
			if (result != repositoryResults.end())
			{
				value = result->second;
				return true;
			}
			else
			{
				return false;
			}
		}

		const std::map<std::string,std::string> &getRepoMap() const
		{
			return repositoryResults;
		}

protected:
		std::map<std::string,std::string> repositoryResults;
};


class TestFlowController : public FlowController
{

public:
	TestFlowController(ProvenanceTestRepository &provenanceRepo, FlowTestRepository &flowRepo) : ::FlowController()
	{
		_provenanceRepo = dynamic_cast<ProvenanceRepository*>(&provenanceRepo);
		_flowfileRepo = dynamic_cast<FlowFileRepository*>(&flowRepo);
	}
	~TestFlowController()
	{

	}
	void load(){

	}

	bool start()
	{
		_running.store(true);
		return true;
	}

	void stop(bool force)
	{
		_running.store(false);
	}
	void waitUnload(const uint64_t timeToWaitMs)
	{
		stop(true);
	}

	void unload()
	{
		stop(true);
	}

	void reload(std::string file)
	{

	}

	bool isRunning()
	{
		return true;
	}


	Processor *createProcessor(std::string name, uuid_t uuid){ return 0;}

	ProcessGroup *createRootProcessGroup(std::string name, uuid_t uuid){ return 0;}

	ProcessGroup *createRemoteProcessGroup(std::string name, uuid_t uuid){ return 0; }

	Connection *createConnection(std::string name, uuid_t uuid){ return 0; }
};


#endif /* LIBMINIFI_TEST_UNIT_PROVENANCETESTHELPER_H_ */
