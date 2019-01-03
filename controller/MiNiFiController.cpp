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
#include <yaml-cpp/yaml.h>
#include <iostream>
#include "io/BaseStream.h"

#include "core/Core.h"

#include "core/FlowConfiguration.h"
#include "core/ConfigurationFactory.h"
#include "core/RepositoryFactory.h"
#include "FlowController.h"
#include "Main.h"
#include "properties/Configure.h"
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
      #ifdef WIN32
          _getcwd(cwd,PATH_MAX);
      #else
          getcwd(cwd, PATH_MAX);
      #endif
      minifiHome = cwd;
    }

  }

  if (!validHome(minifiHome)) {
    logger->log_error("No valid MINIFI_HOME could be inferred. "
                      "Please set MINIFI_HOME or run minifi from a valid location.");
    //return -1;
  }

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  configuration->setHome(minifiHome);
  configuration->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

  std::shared_ptr<logging::LoggerProperties> log_properties = std::make_shared<logging::LoggerProperties>();
  log_properties->setHome(minifiHome);
  log_properties->loadConfigureFile(DEFAULT_LOG_PROPERTIES_FILE);
  logging::LoggerConfiguration::getConfiguration().initialize(log_properties);

  std::string context_name;

  std::shared_ptr<minifi::controllers::SSLContextService> secure_context = nullptr;

  // if the user wishes to use a controller service we need to instantiate the flow
  if (configuration->get("controller.ssl.context.service", context_name)) {
    std::shared_ptr<core::controller::ControllerService> service = getControllerService(configuration, context_name);
    if (nullptr != service) {
      secure_context = std::static_pointer_cast<minifi::controllers::SSLContextService>(service);
    }
  }

  if (nullptr == secure_context) {
    std::string secureStr;
    bool is_secure = false;
    if (configuration->get(minifi::Configure::nifi_remote_input_secure, secureStr) && org::apache::nifi::minifi::utils::StringUtils::StringToBool(secureStr, is_secure)) {
      secure_context = std::make_shared<minifi::controllers::SSLContextService>("ControllerSocketProtocolSSL", configuration);
      secure_context->onEnable();
    }
  }

  std::string value;

  auto stream_factory_ = minifi::io::StreamFactory::getInstance(configuration);

  std::string host = "localhost", portStr, caCert;
  int port = -1;

  cxxopts::Options options("MiNiFiController", "MiNiFi local agent controller");
  options.positional_help("[optional args]").show_positional_help();

  options.add_options()  //NOLINT
  ("h,help", "Shows Help")  //NOLINT
  ("host", "Specifies connecting host name", cxxopts::value<std::string>())  //NOLINT
  ("port", "Specifies connecting host port", cxxopts::value<int>())  //NOLINT
  ("stop", "Shuts down the provided component", cxxopts::value<std::vector<std::string>>())  //NOLINT
  ("start", "Starts provided component", cxxopts::value<std::vector<std::string>>())  //NOLINT
  ("l,list", "Provides a list of connections or processors", cxxopts::value<std::string>())  //NOLINT
  ("c,clear", "Clears the associated connection queue", cxxopts::value<std::vector<std::string>>())  //NOLINT
  ("getsize", "Reports the size of the associated connection queue", cxxopts::value<std::vector<std::string>>())  //NOLINT
  ("updateflow", "Updates the flow of the agent using the provided flow file", cxxopts::value<std::string>())  //NOLINT
  ("getfull", "Reports a list of full connections")  //NOLINT
  ("jstack", "Returns backtraces from the agent")  //NOLINT
  ("manifest", "Generates a manifest for the current binary")  //NOLINT
  ("noheaders", "Removes headers from output streams");

  bool show_headers = true;

  try {
    auto result = options.parse(argc, argv);

    if (result.count("help")) {
      std::cout << options.help( { "", "Group" }) << std::endl;
      exit(0);
    }

    if (result.count("host")) {
      host = result["host"].as<std::string>();
    } else {
      configuration->get("controller.socket.host", host);
    }

    if (result.count("port")) {
      port = result["port"].as<int>();
    } else {
      if (port == -1 && configuration->get("controller.socket.port", portStr)) {
        port = std::stoi(portStr);
      }
    }

    if ((IsNullOrEmpty(host) && port == -1)) {
      std::cout << "MiNiFi Controller is disabled" << std::endl;
      exit(0);
    } else

    if (result.count("noheaders")) {
      show_headers = false;
    }

    if (result.count("stop") > 0) {
      auto& components = result["stop"].as<std::vector<std::string>>();
      for (const auto& component : components) {
        auto socket = secure_context != nullptr ? stream_factory_->createSecureSocket(host, port, secure_context) : stream_factory_->createSocket(host, port);
        if (stopComponent(std::move(socket), component))
          std::cout << component << " requested to stop" << std::endl;
        else
          std::cout << "Could not connect to remote host " << host << ":" << port << std::endl;
      }
    }

    if (result.count("start") > 0) {
      auto& components = result["start"].as<std::vector<std::string>>();
      for (const auto& component : components) {
        auto socket = secure_context != nullptr ? stream_factory_->createSecureSocket(host, port, secure_context) : stream_factory_->createSocket(host, port);
        if (startComponent(std::move(socket), component))
          std::cout << component << " requested to start" << std::endl;
        else
          std::cout << "Could not connect to remote host " << host << ":" << port << std::endl;
      }
    }

    if (result.count("c") > 0) {
      auto& components = result["c"].as<std::vector<std::string>>();
      for (const auto& connection : components) {
        auto socket = secure_context != nullptr ? stream_factory_->createSecureSocket(host, port, secure_context) : stream_factory_->createSocket(host, port);
        if (clearConnection(std::move(socket), connection)) {
          std::cout << "Sent clear command to " << connection << ". Size before clear operation sent: " << std::endl;
          socket = secure_context != nullptr ? stream_factory_->createSecureSocket(host, port, secure_context) : stream_factory_->createSocket(host, port);
          if (getConnectionSize(std::move(socket), std::cout, connection) < 0)
            std::cout << "Could not connect to remote host " << host << ":" << port << std::endl;
        } else
          std::cout << "Could not connect to remote host " << host << ":" << port << std::endl;
      }
    }

    if (result.count("getsize") > 0) {
      auto& components = result["getsize"].as<std::vector<std::string>>();
      for (const auto& component : components) {
        auto socket = secure_context != nullptr ? stream_factory_->createSecureSocket(host, port, secure_context) : stream_factory_->createSocket(host, port);
        if (getConnectionSize(std::move(socket), std::cout, component) < 0)
          std::cout << "Could not connect to remote host " << host << ":" << port << std::endl;
      }

    }

    if (result.count("l") > 0) {
      auto& option = result["l"].as<std::string>();
      auto socket = secure_context != nullptr ? stream_factory_->createSecureSocket(host, port, secure_context) : stream_factory_->createSocket(host, port);
      if (option == "components") {
        if (listComponents(std::move(socket), std::cout, show_headers) < 0)
          std::cout << "Could not connect to remote host " << host << ":" << port << std::endl;
      } else if (option == "connections") {
        if (listConnections(std::move(socket), std::cout, show_headers) < 0)
          std::cout << "Could not connect to remote host " << host << ":" << port << std::endl;
      }

    }

    if (result.count("getfull") > 0) {
      auto socket = secure_context != nullptr ? stream_factory_->createSecureSocket(host, port, secure_context) : stream_factory_->createSocket(host, port);
      if (getFullConnections(std::move(socket), std::cout) < 0)
        std::cout << "Could not connect to remote host " << host << ":" << port << std::endl;
    }

    if (result.count("jstack") > 0) {
      auto socket = secure_context != nullptr ? stream_factory_->createSecureSocket(host, port, secure_context) : stream_factory_->createSocket(host, port);
      if (getJstacks(std::move(socket), std::cout) < 0)
        std::cout << "Could not connect to remote host " << host << ":" << port << std::endl;
    }

    if (result.count("updateflow") > 0) {
      auto& flow_file = result["updateflow"].as<std::string>();
      auto socket = secure_context != nullptr ? stream_factory_->createSecureSocket(host, port, secure_context) : stream_factory_->createSocket(host, port);
      if (updateFlow(std::move(socket), std::cout, flow_file) < 0)
        std::cout << "Could not connect to remote host " << host << ":" << port << std::endl;
    }

    if (result.count("manifest") > 0) {
      printManifest(configuration);
    }
  } catch (const std::exception &exc) {
    // catch anything thrown within try block that derives from std::exception
    std::cerr << exc.what() << std::endl;
  } catch (...) {
    std::cout << options.help( { "", "Group" }) << std::endl;
    exit(0);
  }
  return 0;
}
