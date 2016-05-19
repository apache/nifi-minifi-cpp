/**
 * @file MiNiFiMain.cpp 
 * MiNiFiMain implementation 
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
#include <unistd.h>

#include "Logger.h"
#include "Configure.h"
#include "FlowController.h"

//! Main thread sleep interval 1 second
#define SLEEP_INTERVAL 1
//! Default nifi properties file path
#define DEFAULT_NIFI_PROPERTIES_FILE "./conf/nifi.properties"

int main(int argc, char **argv)
{
	Logger *logger = Logger::getLogger();
	logger->setLogLevel(trace);
	logger->log_info("MiNiFi started");

	Configure *configure = Configure::getConfigure();
	configure->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

	FlowController controller;

	// Load flow XML
	controller.load();
	// Start Processing the flow
	controller.start();

	// main loop
	while (true)
	{
		sleep(SLEEP_INTERVAL);
	}

	return 0;
}
