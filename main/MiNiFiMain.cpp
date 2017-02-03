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
#include <iostream>
#include "spdlog/spdlog.h"

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
//! Define home environment variable
#define MINIFI_HOME_ENV_KEY "MINIFI_HOME"

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

int main(int argc, char **argv)
{
	Logger *logger = Logger::getLogger();
	logger->setLogLevel(info);

    // assumes POSIX compliant environment
    std::string minifiHome;
    if (const char* env_p = std::getenv(MINIFI_HOME_ENV_KEY))
    {
        minifiHome = env_p;
    }
    else
    {
        logger->log_info("MINIFI_HOME was not found, determining based on executable path.");
        char *path = NULL;
        char full_path[PATH_MAX];
        path = realpath(argv[0], full_path);
        std::string minifiHomePath(path);
        minifiHomePath = minifiHomePath.substr(0, minifiHomePath.find_last_of("/\\"));	//Remove /minifi from path
        minifiHome = minifiHomePath.substr(0, minifiHomePath.find_last_of("/\\"));	//Remove /bin from path
    }

	if (signal(SIGINT, sigHandler) == SIG_ERR || signal(SIGTERM, sigHandler) == SIG_ERR || signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		logger->log_error("Can not install signal handler");
		return -1;
	}

    Configure *configure = Configure::getConfigure();
    configure->setHome(minifiHome);
    configure->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

	controller = FlowController::getFlowController();

	// Load flow from specified configuration file
	controller->load();
	// Start Processing the flow
	controller->start();
	running = true;

	logger->log_info("MiNiFi started");

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
