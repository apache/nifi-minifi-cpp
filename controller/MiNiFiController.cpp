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
#include <fcntl.h>
#include <stdio.h>
#include <semaphore.h>
#include <signal.h>
#include <vector>
#include <queue>
#include <map>
#include <unistd.h>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include "io/BaseStream.h"

#include "core/Core.h"

#include "core/FlowConfiguration.h"
#include "core/ConfigurationFactory.h"
#include "core/RepositoryFactory.h"
#include "FlowController.h"
#include "Main.h"

#include "Controller.h"
#include "c2/ControllerSocketProtocol.h"

#include "cxxopts.hpp"

int main(int argc, char **argv) {

  std::shared_ptr<logging::Logger> logger = logging::LoggerConfiguration::getConfiguration().getLogger("controller");

  // assumes POSIX compliant environment
  std::string minifiHome;
  if (const char *env_p = std::getenv(MINIFI_HOME_ENV_KEY)) {
    minifiHome = env_p;
    logger->log_info("Using MINIFI_HOME=%s from environment.", minifiHome);
  } else {
    logger->log_info("MINIFI_HOME is not set; determining based on environment.");
    char *path = nullptr;
    char full_path[PATH_MAX];
    path = realpath(argv[0], full_path);

    if (path != nullptr) {
      std::string minifiHomePath(path);
      if (minifiHomePath.find_last_of("/\\") != std::string::npos) {
        minifiHomePath = minifiHomePath.substr(0, minifiHomePath.find_last_of("/\\"));  //Remove /minifi from path
        minifiHome = minifiHomePath.substr(0, minifiHomePath.find_last_of("/\\"));    //Remove /bin from path
      }
    }

    // attempt to use cwd as MINIFI_HOME
    if (minifiHome.empty() || !validHome(minifiHome)) {
      char cwd[PATH_MAX];
      getcwd(cwd, PATH_MAX);
      minifiHome = cwd;
    }

  }

  if (!validHome(minifiHome)) {
    logger->log_error("No valid MINIFI_HOME could be inferred. "
                      "Please set MINIFI_HOME or run minifi from a valid location.");
    return -1;
  }

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  configuration->setHome(minifiHome);
  configuration->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

  std::shared_ptr<logging::LoggerProperties> log_properties = std::make_shared<logging::LoggerProperties>();
  log_properties->setHome(minifiHome);
  log_properties->loadConfigureFile(DEFAULT_LOG_PROPERTIES_FILE);
  logging::LoggerConfiguration::getConfiguration().initialize(log_properties);

  auto stream_factory_ = std::make_shared<minifi::io::StreamFactory>(configuration);

  std::string host, port, caCert;

  if (!configuration->get("controller.socket.host", host) || !configuration->get("controller.socket.port", port)) {
    std::cout << "MiNiFi Controller is disabled" << std::endl;
    exit(0);
  }

  cxxopts::Options options("MiNiFiController", "MiNiFi local agent controller");
  options.positional_help("[optional args]").show_positional_help();

  options.add_options()  //NOLINT
  ("help", "Shows Help")  //NOLINT
  ("stop", "Shuts down the provided component", cxxopts::value<std::vector<std::string>>())  //NOLINT
  ("start", "Starts provided component", cxxopts::value<std::vector<std::string>>())  //NOLINT
  ("l,list", "Provides a list of connections or processors", cxxopts::value<std::string>())  //NOLINT
  ("c,clear", "Clears the associated connection queue", cxxopts::value<std::vector<std::string>>())  //NOLINT
  ("getsize", "Reports the size of the associated connection queue", cxxopts::value<std::vector<std::string>>())  //NOLINT
  ("updateflow", "Updates the flow of the agent using the provided flow file", cxxopts::value<std::string>())  //NOLINT
  ("getfull", "Reports a list of full connections");

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help( { "", "Group" }) << std::endl;
    exit(0);
  }

  if (result.count("stop") > 0) {
    auto& components = result["stop"].as<std::vector<std::string>>();
    for (const auto& component : components) {
      auto socket = stream_factory_->createSocket(host, std::stoi(port));
      stopComponent(std::move(socket), component);
      std::cout << component << " requested to stop" << std::endl;
    }
  }

  if (result.count("start") > 0) {
    auto& components = result["start"].as<std::vector<std::string>>();
    for (const auto& component : components) {
      auto socket = stream_factory_->createSocket(host, std::stoi(port));
      startComponent(std::move(socket), component);
      std::cout << component << " requested to start" << std::endl;
    }
  }

  if (result.count("c") > 0) {
    auto& components = result["c"].as<std::vector<std::string>>();
    for (const auto& connection : components) {
      auto socket = stream_factory_->createSocket(host, std::stoi(port));
      clearConnection(std::move(socket), connection);
      std::cout << "Cleared " << connection << std::endl;
    }
  }

  if (result.count("getsize") > 0) {
    auto& components = result["getsize"].as<std::vector<std::string>>();
    for (const auto& component : components) {
      auto socket = stream_factory_->createSocket(host, std::stoi(port));
      getConnectionSize(std::move(socket), std::cout, component);
    }

  }

  if (result.count("l") > 0) {
    auto& option = result["l"].as<std::string>();
    if (option == "processors" || option == "connections") {

      auto socket = stream_factory_->createSocket(host, std::stoi(port));
      socket->initialize();

      uint8_t op = minifi::c2::Operation::DESCRIBE;
      minifi::io::BaseStream stream;
      stream.writeData(&op, 1);
      stream.writeUTF(option);
      socket->writeData(const_cast<uint8_t*>(stream.getBuffer()), stream.getSize());
      uint16_t responses = 0;
      socket->readData(&op, 1);
      socket->read(responses);
      if (option == "processors")
        std::cout << "Processors:" << std::endl;
      else {
        std::cout << "Connection Names:" << std::endl;
      }
      for (int i = 0; i < responses; i++) {
        std::string name;
        socket->readUTF(name, false);
        std::cout << name << std::endl;
      }
    }
  }

  if (result.count("getfull") > 0) {
    auto socket = stream_factory_->createSocket(host, std::stoi(port));
    getFullConnections(std::move(socket), std::cout);
  }

  if (result.count("updateflow") > 0) {
    auto& flow_file = result["updateflow"].as<std::string>();
    auto socket = stream_factory_->createSocket(host, std::stoi(port));
    updateFlow(std::move(socket), std::cout, flow_file);

  }

  return 0;
}
