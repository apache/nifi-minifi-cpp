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
#include <vector>
#include <iostream>

#include "MainHelper.h"
#include "properties/Configure.h"
#include "Controller.h"
#include "c2/ControllerSocketProtocol.h"
#include "core/controller/ControllerService.h"
#include "core/extension/ExtensionManager.h"
#include "io/StreamFactory.h"
#include "core/ConfigurationFactory.h"
#include "Exception.h"

#include "cxxopts.hpp"

namespace minifi = org::apache::nifi::minifi;

std::shared_ptr<minifi::core::controller::ControllerService> getControllerService(const std::shared_ptr<minifi::Configure> &configuration,
    const std::string &service_name) {
  std::string nifi_configuration_class_name = "adaptiveconfiguration";

  minifi::core::extension::ExtensionManager::get().initialize(configuration);

  configuration->get(minifi::Configure::nifi_configuration_class_name, nifi_configuration_class_name);
  const auto stream_factory = minifi::io::StreamFactory::getInstance(configuration);
  auto flow_configuration = minifi::core::createFlowConfiguration(
    minifi::core::ConfigurationContext{
      .flow_file_repo = nullptr,
      .content_repo = nullptr,
      .stream_factory = stream_factory,
      .configuration = configuration,
      .path = configuration->get(minifi::Configure::nifi_flow_configuration_file)},
    nifi_configuration_class_name);

  auto root = flow_configuration->getRoot();
  if (!root) {
    return nullptr;
  }
  auto controller = root->findControllerService(service_name);
  if (!controller) {
    return nullptr;
  }
  return controller->getControllerServiceImplementation();
}

std::shared_ptr<minifi::controllers::SSLContextService> getSSLContextService(const std::shared_ptr<minifi::Configure>& configuration) {
  std::shared_ptr<minifi::controllers::SSLContextService> secure_context;
  std::string context_name;
  // if the user wishes to use a controller service we need to instantiate the flow
  if (configuration->get(minifi::Configure::controller_ssl_context_service, context_name) && !context_name.empty()) {
    const auto service = getControllerService(configuration, context_name);
    if (nullptr != service) {
      secure_context = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(service);
    }
    if (secure_context == nullptr) {
      throw minifi::Exception(minifi::GENERAL_EXCEPTION, "SSL Context was set, but the context name '" + context_name + "' could not be found");
    }
  }

  if (nullptr == secure_context) {
    std::string secureStr;
    if (configuration->get(minifi::Configure::nifi_remote_input_secure, secureStr) && minifi::utils::StringUtils::toBool(secureStr).value_or(false)) {
      secure_context = std::make_shared<minifi::controllers::SSLContextService>("ControllerSocketProtocolSSL", configuration);
      secure_context->onEnable();
    }
  } else {
    secure_context->onEnable();
  }
  return secure_context;
}

int main(int argc, char **argv) {
  const auto logger = minifi::core::logging::LoggerConfiguration::getConfiguration().getLogger("controller");

  const auto minifi_home = determineMinifiHome(logger);
  if (minifi_home.empty()) {
    // determineMinifiHome already logged everything we need
    return -1;
  }

  const auto configuration = std::make_shared<minifi::Configure>();
  configuration->setHome(minifi_home);
  configuration->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

  const auto log_properties = std::make_shared<minifi::core::logging::LoggerProperties>();
  log_properties->setHome(minifi_home);
  log_properties->loadConfigureFile(DEFAULT_LOG_PROPERTIES_FILE);
  minifi::core::logging::LoggerConfiguration::getConfiguration().initialize(log_properties);

  minifi::controller::ControllerSocketData socket_data;
  try {
    socket_data.ssl_context_service = getSSLContextService(configuration);
  } catch(const minifi::Exception& ex) {
    logger->log_error(ex.what());
    exit(1);
  }
  auto stream_factory_ = minifi::io::StreamFactory::getInstance(configuration);


  std::string port_str;
  std::string ca_cert;

  cxxopts::Options options("MiNiFiController", "MiNiFi local agent controller");
  options.positional_help("[optional args]").show_positional_help();

  options.add_options()
      ("h,help", "Shows Help")
      ("host", "Specifies connecting host name", cxxopts::value<std::string>())
      ("port", "Specifies connecting host port", cxxopts::value<int>())
      ("stop", "Shuts down the provided component", cxxopts::value<std::vector<std::string>>())
      ("start", "Starts provided component", cxxopts::value<std::vector<std::string>>())
      ("l,list", "Provides a list of connections or processors", cxxopts::value<std::string>())
      ("c,clear", "Clears the associated connection queue", cxxopts::value<std::vector<std::string>>())
      ("getsize", "Reports the size of the associated connection queue", cxxopts::value<std::vector<std::string>>())
      ("updateflow", "Updates the flow of the agent using the provided flow file", cxxopts::value<std::string>())
      ("getfull", "Reports a list of full connections")
      ("jstack", "Returns backtraces from the agent")
      ("manifest", "Generates a manifest for the current binary")
      ("noheaders", "Removes headers from output streams");

  bool show_headers = true;

  try {
    auto result = options.parse(argc, argv);

    if (result.count("help")) {
      std::cout << options.help({ "", "Group" }) << std::endl;
      exit(0);
    }

    if (result.count("host")) {
      socket_data.host = result["host"].as<std::string>();
    } else {
      configuration->get(minifi::Configure::controller_socket_host, socket_data.host);
    }

    if (result.count("port")) {
      socket_data.port = result["port"].as<int>();
    } else if (socket_data.port == -1 && configuration->get(minifi::Configure::controller_socket_port, port_str)) {
      socket_data.port = std::stoi(port_str);
    }

    if ((minifi::IsNullOrEmpty(socket_data.host) && socket_data.port == -1)) {
      std::cout << "MiNiFi Controller is disabled" << std::endl;
      exit(0);
    }
    if (result.count("noheaders")) {
      show_headers = false;
    }

    if (result.count("stop") > 0) {
      auto& components = result["stop"].as<std::vector<std::string>>();
      for (const auto& component : components) {
        if (minifi::controller::stopComponent(socket_data, component))
          std::cout << component << " requested to stop" << std::endl;
        else
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
      }
    }

    if (result.count("start") > 0) {
      auto& components = result["start"].as<std::vector<std::string>>();
      for (const auto& component : components) {
        if (minifi::controller::startComponent(socket_data, component))
          std::cout << component << " requested to start" << std::endl;
        else
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
      }
    }

    if (result.count("c") > 0) {
      auto& components = result["c"].as<std::vector<std::string>>();
      for (const auto& connection : components) {
        if (minifi::controller::clearConnection(socket_data, connection)) {
          std::cout << "Sent clear command to " << connection << ". Size before clear operation sent: " << std::endl;
          if (!minifi::controller::getConnectionSize(socket_data, std::cout, connection))
            std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
        } else {
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
        }
      }
    }

    if (result.count("getsize") > 0) {
      auto& components = result["getsize"].as<std::vector<std::string>>();
      for (const auto& component : components) {
        if (!minifi::controller::getConnectionSize(socket_data, std::cout, component))
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
      }
    }
    if (result.count("l") > 0) {
      auto& option = result["l"].as<std::string>();
      if (option == "components") {
        if (!minifi::controller::listComponents(socket_data, std::cout, show_headers))
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
      } else if (option == "connections") {
        if (!minifi::controller::listConnections(socket_data, std::cout, show_headers))
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
      }
    }
    if (result.count("getfull") > 0) {
      if (!minifi::controller::getFullConnections(socket_data, std::cout))
        std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
    }

    if (result.count("updateflow") > 0) {
      auto& flow_file = result["updateflow"].as<std::string>();
      if (!minifi::controller::updateFlow(socket_data, std::cout, flow_file))
        std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
    }

    if (result.count("manifest") > 0) {
      if (!minifi::controller::printManifest(socket_data, std::cout))
        std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
    }

    if (result.count("jstack") > 0) {
      if (!minifi::controller::getJstacks(socket_data, std::cout))
        std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
    }
  } catch (const std::exception &exc) {
    // catch anything thrown within try block that derives from std::exception
    std::cerr << exc.what() << std::endl;
  } catch (...) {
    std::cout << options.help({ "", "Group" }) << std::endl;
    exit(0);
  }
  return 0;
}
