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

#ifndef LIBMINIFI_TEST_TESTBASE_H_
#define LIBMINIFI_TEST_TESTBASE_H_
#include <cstdio>
#include <cstdlib>
#include "ResourceClaim.h"
#include "catch.hpp"
#include "Logger.h"
#include <vector>


class LogTestController {
public:
	LogTestController(const std::string level = "debug") {
		Logger::getLogger()->setLogLevel(level);
	}


	void enableDebug()
	{
		Logger::getLogger()->setLogLevel("debug");
	}

	~LogTestController() {
		Logger::getLogger()->setLogLevel(LOG_LEVEL_E::info);
	}
};

class TestController{
public:



	TestController() : log("info")
	{
		ResourceClaim::default_directory_path = "./";
	}

	~TestController()
	{
		for(auto dir : directories)
		{
			rmdir(dir);
		}
	}

	void enableDebug() {
		log.enableDebug();
	}

	char *createTempDirectory(char *format)
	{
		char *dir = mkdtemp(format);
		return dir;
	}

protected:
	LogTestController log;
	std::vector<char*> directories;


};





#endif /* LIBMINIFI_TEST_TESTBASE_H_ */
