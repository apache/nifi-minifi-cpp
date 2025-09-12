/**
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

#include <iostream>
#include <typeinfo>

#include "EncryptConfig.h"
#include "argparse/argparse.hpp"
#include "minifi-cpp/agent/agent_version.h"
#include "utils/StringUtils.h"

namespace minifi = org::apache::nifi::minifi;
using minifi::encrypt_config::EncryptConfig;

constexpr std::string_view OPERATION_MINIFI_PROPERTIES = "minifi-properties";
constexpr std::string_view OPERATION_FLOW_CONFIG = "flow-config";
constexpr std::string_view OPERATION_WHOLE_FLOW_CONFIG_FILE = "whole-flow-config-file";

int main(int argc, char* argv[]) try {
  argparse::ArgumentParser argument_parser("Apache MiNiFi C++ Encrypt-Config", org::apache::nifi::minifi::AgentBuild::VERSION);
  argument_parser.add_argument("operation")
      .default_value("minifi-properties")
      .help(minifi::utils::string::join_pack("what to encrypt: ", OPERATION_MINIFI_PROPERTIES, " | ", OPERATION_FLOW_CONFIG, " | ", OPERATION_WHOLE_FLOW_CONFIG_FILE));
  argument_parser.add_argument("-m", "--minifi-home")
      .required()
      .metavar("MINIFI_HOME")
      .help("Specifies the home directory used by the minifi agent");
  argument_parser.add_argument("--component-id")
      .metavar("ID")
      .help(minifi::utils::string::join_pack("Processor or controller service id (", OPERATION_FLOW_CONFIG, " only)"));
  argument_parser.add_argument("--property-name")
      .metavar("NAME")
      .help(minifi::utils::string::join_pack("The name of the sensitive property (", OPERATION_FLOW_CONFIG, " only)"));
  argument_parser.add_argument("--property-value")
      .metavar("VALUE")
      .help(minifi::utils::string::join_pack("The new value of the sensitive property (", OPERATION_FLOW_CONFIG, " only)"));
  argument_parser.add_argument("--parameter-context-id")
      .metavar("ID")
      .help(minifi::utils::string::join_pack("Parameter context id (", OPERATION_FLOW_CONFIG, " only)"));
  argument_parser.add_argument("--parameter-name")
      .metavar("NAME")
      .help(minifi::utils::string::join_pack("The name of the sensitive parameter (", OPERATION_FLOW_CONFIG, " only)"));
  argument_parser.add_argument("--parameter-value")
      .metavar("VALUE")
      .help(minifi::utils::string::join_pack("The new value of the sensitive parameter (", OPERATION_FLOW_CONFIG, " only)"));
  argument_parser.add_argument("--re-encrypt")
      .flag()
      .help(minifi::utils::string::join_pack("Decrypt all properties with the old key and re-encrypt them with a new key (", OPERATION_FLOW_CONFIG, " only)"));

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << "\n\n" << argument_parser;
    return 1;
  }

  EncryptConfig encrypt_config{argument_parser.get("-m")};

  std::string operation = argument_parser.get("operation");
  const auto re_encrypt = argument_parser.get<bool>("--re-encrypt");
  const auto component_id = argument_parser.present("--component-id");
  const auto property_name = argument_parser.present("--property-name");
  const auto property_value = argument_parser.present("--property-value");
  const auto parameter_context_id = argument_parser.present("--parameter-context-id");
  const auto parameter_name = argument_parser.present("--parameter-name");
  const auto parameter_value = argument_parser.present("--parameter-value");

  if (operation != OPERATION_FLOW_CONFIG && (re_encrypt || component_id || property_name || property_value || parameter_context_id || parameter_name || parameter_value)) {
    std::cerr << "Unsupported option for operation '" << operation << "'\n\n" << argument_parser;
    return 5;
  }

  if ((component_id || property_name || property_value) && (parameter_context_id || parameter_name || parameter_value)) {
    std::cerr << "Property and parameter options cannot be used together\n\n" << argument_parser;
    return 6;
  }

  if (operation == OPERATION_MINIFI_PROPERTIES) {
    encrypt_config.encryptSensitiveValuesInMinifiProperties();
  } else if (operation == OPERATION_FLOW_CONFIG) {
    if (property_name) {
      encrypt_config.encryptSensitiveValuesInFlowConfig(re_encrypt, component_id, property_name, property_value);
    } else {
      encrypt_config.encryptSensitiveValuesInFlowConfig(re_encrypt, parameter_context_id, parameter_name, parameter_value);
    }
  } else if (operation == OPERATION_WHOLE_FLOW_CONFIG_FILE) {
    encrypt_config.encryptWholeFlowConfigFile();
  } else {
    std::cerr << "Unknown operation: " << operation << "\n\n" << argument_parser;
    return 4;
  }

  if ((operation == OPERATION_MINIFI_PROPERTIES || operation == OPERATION_WHOLE_FLOW_CONFIG_FILE) && encrypt_config.isReEncrypting()) {
    std::cout << "WARNING: an .old key was provided, which is used for both " << OPERATION_MINIFI_PROPERTIES << " and " << OPERATION_WHOLE_FLOW_CONFIG_FILE << ".\n"
        << "If both are currently encrypted, make sure to run " << argv[0] << " to re-encrypt both before removing the .old key,\n"
        << "otherwise you won't be able to recover the encrypted data!\n";
  }

  return 0;
} catch (const std::exception& ex) {
  std::cerr << ex.what() << "\n(" << typeid(ex).name() << ")\n";
  return 2;
} catch (...) {
  std::cerr << "Unknown error\n";
  return 3;
}
