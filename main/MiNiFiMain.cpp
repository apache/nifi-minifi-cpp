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
#include <stdio.h>
#include <signal.h>
#include <vector>
#include <queue>
#include <map>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include "Logger.h"
#include "Configure.h"
#include "FlowController.h"

//! Main thread sleep interval 1 second
#define SLEEP_INTERVAL 1
//! Main thread stop wait time
#define STOP_WAIT_TIME 2
//! Default YAML location
#define DEFAULT_NIFI_CONFIG_YML "./conf/config.yml"
//! Default nifi properties file path
#define DEFAULT_NIFI_PROPERTIES_FILE "./conf/minifi.properties"

/* Define Parser Values for Configuration YAML sections */
#define CONFIG_YAML_FLOW_CONTROLLER_KEY "Flow Controller"
#define CONFIG_YAML_PROCESSORS_KEY "Processors"
#define CONFIG_YAML_CONNECTIONS_KEY "Connections"
#define CONFIG_YAML_REMOTE_PROCESSING_GROUPS_KEY "Remote Processing Groups"

//! Whether it is running
static bool running = false;
//! Flow Controller
static FlowController *controller = NULL;

void sigHandler(int signal)
{
	if (signal == SIGINT || signal == SIGTERM)
	{
		controller->stop(true);
		sleep(STOP_WAIT_TIME);
		running = false;
	}
}

int loadYaml() {
	YAML::Node flow = YAML::LoadFile("./conf/flow.yml");

	YAML::Node flowControllerNode = flow[CONFIG_YAML_FLOW_CONTROLLER_KEY];
	YAML::Node processorsNode = flow[CONFIG_YAML_PROCESSORS_KEY];
	YAML::Node connectionsNode = flow[CONFIG_YAML_CONNECTIONS_KEY];
	YAML::Node remoteProcessingGroupNode = flow[CONFIG_YAML_REMOTE_PROCESSING_GROUPS_KEY];

	if (processorsNode) {
		int numProcessors = processorsNode.size();
		if (numProcessors < 1) {
			throw new std::invalid_argument("There must be at least one processor configured.");
		}

		std::vector<ProcessorConfig> processorConfigs;

		if (processorsNode.IsSequence()) {
			for (YAML::const_iterator iter = processorsNode.begin(); iter != processorsNode.end(); ++iter) {
				ProcessorConfig procCfg;
				YAML::Node procNode = iter->as<YAML::Node>();

				procCfg.name = procNode["name"].as<std::string>();
				procCfg.javaClass = procNode["class"].as<std::string>();

				processorConfigs.push_back(procCfg);
			}
		}

		Logger::getLogger()->log_info("Added %d processor configs.", processorConfigs.size());
	} else {
		throw new std::invalid_argument(
				"Cannot instantiate a MiNiFi instance without a defined Processors configuration node.");
	}

	return 0;
}

int main(int argc, char **argv) {

	Logger *logger = Logger::getLogger();
	logger->setLogLevel(info);
	logger->log_info("MiNiFi started");

	try {
		logger->log_info("Performing parsing of specified config.yml");
		loadYaml();
	} catch (...) {
		std::cout << "Could not load YAML due to improper configuration.";
		return 1;
	}

	if (signal(SIGINT, sigHandler) == SIG_ERR || signal(SIGTERM, sigHandler) == SIG_ERR || signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		logger->log_error("Can not install signal handler");
		return -1;
	}

	Configure *configure = Configure::getConfigure();
	configure->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

	controller = new FlowController();

	// Load flow from specified configuration file
	controller->load(ConfigFormat::YAML);
	// Start Processing the flow
	controller->start();
	running = true;

	// main loop
	while (running)
	{
		sleep(SLEEP_INTERVAL);
	}

	controller->unload();
	delete controller;
	logger->log_info("MiNiFi exit");

	return 0;
}
