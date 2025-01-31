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
#include <string>
#include <string_view>
#include <filesystem>

#include "MainHelper.h"
#include "properties/Configure.h"
#include "Controller.h"
#include "c2/ControllerSocketProtocol.h"
#include "core/controller/ControllerService.h"
#include "core/extension/ExtensionManager.h"
#include "core/ConfigurationFactory.h"
#include "Exception.h"
#include "argparse/argparse.hpp"
#include "range/v3/algorithm/contains.hpp"
#include "agent/agent_version.h"

namespace minifi = org::apache::nifi::minifi;

std::shared_ptr<minifi::core::controller::ControllerService> getControllerService(const std::shared_ptr<minifi::Configure> &configuration,
    const std::string &service_name) {
  std::string nifi_configuration_class_name = "adaptiveconfiguration";

  minifi::core::extension::ExtensionManagerImpl::get().initialize(configuration);

  configuration->get(minifi::Configure::nifi_configuration_class_name, nifi_configuration_class_name);
  auto flow_configuration = minifi::core::createFlowConfiguration(
    minifi::core::ConfigurationContext{
      .flow_file_repo = nullptr,
      .content_repo = nullptr,
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
    if (configuration->get(minifi::Configure::nifi_remote_input_secure, secureStr) && minifi::utils::string::toBool(secureStr).value_or(false)) {
      secure_context = std::make_shared<minifi::controllers::SSLContextServiceImpl>("ControllerSocketProtocolSSL", configuration);
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

  const auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(minifi_home);
  configuration->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

  const auto log_properties = std::make_shared<minifi::core::logging::LoggerProperties>();
  log_properties->setHome(minifi_home);
  log_properties->loadConfigureFile(DEFAULT_LOG_PROPERTIES_FILE);
  minifi::core::logging::LoggerConfiguration::getConfiguration().initialize(log_properties);

  minifi::utils::net::SocketData socket_data;
  try {
    socket_data.ssl_context_service = getSSLContextService(configuration);
  } catch(const minifi::Exception& ex) {
    logger->log_error("{}", ex.what());
    std::exit(1);
  }

  argparse::ArgumentParser argument_parser("Apache MiNiFi C++ Controller", minifi::AgentBuild::VERSION);
  argument_parser.add_argument("--host").metavar("HOSTNAME")
    .help("Specifies connecting host name");
  argument_parser.add_argument("--port")
    .metavar("PORT")
    .help("Specifies connecting host port")
    .scan<'d', int>();
  argument_parser.add_argument("--stop")
    .metavar("COMPONENT")
    .nargs(argparse::nargs_pattern::at_least_one)
    .help("Shuts down the provided components");
  argument_parser.add_argument("--start")
    .metavar("COMPONENT")
    .nargs(argparse::nargs_pattern::at_least_one)
    .help("Starts provided components");
  argument_parser.add_argument("-l", "--list")
    .action([](const std::string& value) {
      if (ranges::contains(std::array{"components", "connections"}, value)) {
        return value;
      }
      throw std::runtime_error("List command only accepts the following parameters: [components, connections]");
    })
    .help("Provides a list of connections or components (processors). Accepted parameters: [components, components]");
  argument_parser.add_argument("-c", "--clear")
    .metavar("CONNECTION")
    .nargs(argparse::nargs_pattern::at_least_one)
    .help("Clears the associated connection queues");
  argument_parser.add_argument("--getsize")
    .metavar("CONNECTION")
    .nargs(argparse::nargs_pattern::at_least_one)
    .help("Reports the size of the associated connection queues");
  argument_parser.add_argument("--updateflow")
    .metavar("FLOW_CONFIG_PATH")
    .help("Updates the flow of the agent using the provided flow file");

  auto addFlagOption = [&](std::string_view name, const std::string& help) {
    argument_parser.add_argument(name)
      .default_value(false)
      .implicit_value(true)
      .help(help);
  };
  addFlagOption("--getfull", "Reports a list of full connections");
  addFlagOption("--jstack", "Returns backtraces from the agent");
  addFlagOption("--manifest", "Generates a manifest for the current binary");
  addFlagOption("--noheaders", "Removes headers from output streams");

  argument_parser.add_argument("-d", "--debug").metavar("BUNDLE_OUT_DIR")
    .help("Get debug bundle");

  bool show_headers = true;

  if (argc <= 1) {
    std::cerr << argument_parser;
    std::exit(1);
  }

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << argument_parser;
    std::exit(1);
  }

  try {
    if (const auto& host = argument_parser.present("--host")) {
      socket_data.host = *host;
    } else {
      configuration->get(minifi::Configure::controller_socket_host, socket_data.host);
    }

    std::string port_str;
    if (const auto& port = argument_parser.present<int>("--port")) {
      socket_data.port = *port;
    } else if (socket_data.port == -1 && configuration->get(minifi::Configure::controller_socket_port, port_str)) {
      socket_data.port = std::stoi(port_str);
    }

    if ((minifi::IsNullOrEmpty(socket_data.host) && socket_data.port == -1)) {
      std::cout << "MiNiFi Controller is disabled" << std::endl;
      std::exit(0);
    }
    if (argument_parser.get<bool>("--noheaders")) {
      show_headers = false;
    }

    if (const auto& components = argument_parser.present<std::vector<std::string>>("--stop")) {
      for (const auto& component : *components) {
        if (minifi::controller::stopComponent(socket_data, component))
          std::cout << component << " requested to stop" << std::endl;
        else
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
      }
    }

    if (const auto& components = argument_parser.present<std::vector<std::string>>("--start")) {
      for (const auto& component : *components) {
        if (minifi::controller::startComponent(socket_data, component))
          std::cout << component << " requested to start" << std::endl;
        else
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
      }
    }

    if (const auto& components = argument_parser.present<std::vector<std::string>>("--clear")) {
      for (const auto& connection : *components) {
        if (minifi::controller::clearConnection(socket_data, connection)) {
          std::cout << "Sent clear command to " << connection << "." << std::endl;
        } else {
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
        }
      }
    }

    if (const auto& components = argument_parser.present<std::vector<std::string>>("--getsize")) {
      for (const auto& component : *components) {
        if (!minifi::controller::getConnectionSize(socket_data, std::cout, component))
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
      }
    }

    if (const auto& option = argument_parser.present("--list")) {
      if (*option == "components") {
        if (!minifi::controller::listComponents(socket_data, std::cout, show_headers))
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
      } else if (*option == "connections") {
        if (!minifi::controller::listConnections(socket_data, std::cout, show_headers))
          std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
      }
    }

    if (argument_parser.get<bool>("--getfull")) {
      if (!minifi::controller::getFullConnections(socket_data, std::cout))
        std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
    }

    if (const auto& flow_file = argument_parser.present("--updateflow")) {
      if (!minifi::controller::updateFlow(socket_data, std::cout, *flow_file))
        std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
    }

    if (argument_parser.get<bool>("--manifest")) {
      if (!minifi::controller::printManifest(socket_data, std::cout))
        std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
    }

    if (argument_parser.get<bool>("--jstack")) {
      if (!minifi::controller::getJstacks(socket_data, std::cout))
        std::cout << "Could not connect to remote host " << socket_data.host << ":" << socket_data.port << std::endl;
    }

    if (const auto& debug_path = argument_parser.present("--debug")) {
      auto debug_res = minifi::controller::getDebugBundle(socket_data, std::filesystem::path(*debug_path));
      if (!debug_res)
        std::cout << debug_res.error() << std::endl;
      else
        std::cout << "Debug bundle written to " << std::filesystem::path(*debug_path) / "debug.tar.gz";
    }
  } catch (const std::exception &exc) {
    // catch anything thrown within try block that derives from std::exception
    std::cerr << exc.what() << std::endl;
    std::exit(1);
  } catch (...) {
    std::cerr << "Caught unknown exception" << std::endl;
    std::exit(1);
  }
  return 0;
}
