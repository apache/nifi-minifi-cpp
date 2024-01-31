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
#include "agent/agent_version.h"

using org::apache::nifi::minifi::encrypt_config::EncryptConfig;

int main(int argc, char* argv[]) try {
  argparse::ArgumentParser argument_parser("Apache MiNiFi C++ Encrypt-Config", org::apache::nifi::minifi::AgentBuild::VERSION);
  argument_parser.add_argument("-m", "--minifi-home")
    .required()
    .metavar("MINIFI_HOME")
    .help("Specifies the home directory used by the minifi agent");
  argument_parser.add_argument("-p", "--sensitive-values-in-minifi-properties")
    .flag()
    .help("Encrypt sensitive values in minifi.properties");
  argument_parser.add_argument("-c", "--sensitive-values-in-flow-config")
    .flag()
    .help("Encrypt sensitive values in the flow configuration file (config.yml or as specified in minifi.properties)");
  argument_parser.add_argument("-e", "--encrypt-flow-config")
    .flag()
    .help("Encrypt the flow configuration file (config.yml or as specified in minifi.properties) as a blob");

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << "\n\n" << argument_parser;
    return 1;
  }

  bool sensitive_values_in_minifi_properties = argument_parser.get<bool>("--sensitive-values-in-minifi-properties");
  const bool sensitive_values_in_flow_config = argument_parser.get<bool>("--sensitive-values-in-flow-config");
  const bool flow_config_blob = argument_parser.get<bool>("--encrypt-flow-config");

  if (!(sensitive_values_in_minifi_properties || sensitive_values_in_flow_config || flow_config_blob)) {
    std::cout << "No options were selected, defaulting to --sensitive-values-in-minifi-properties.\n";
    sensitive_values_in_minifi_properties = true;
  }

  EncryptConfig encrypt_config{argument_parser.get("-m")};

  if (sensitive_values_in_minifi_properties) {
    encrypt_config.encryptSensitiveValuesInMinifiProperties();
  }
  if (sensitive_values_in_flow_config) {
    encrypt_config.encryptSensitiveValuesInFlowConfig();
  }
  if (flow_config_blob) {
    encrypt_config.encryptFlowConfigBlob();
  }

  if (encrypt_config.encryptionType() == EncryptConfig::EncryptionType::RE_ENCRYPT && !sensitive_values_in_minifi_properties) {
    std::cout << "WARNING: an .old key was provided but "
        << "--sensitive-values-in-minifi-properties was not requested. If sensitive values in minifi.properties are "
        << "currently encrypted with the old key and the old key is removed, you won't be able to recover these values!\n";
  }

  if (encrypt_config.encryptionType() == EncryptConfig::EncryptionType::RE_ENCRYPT && !flow_config_blob) {
    std::cout << "WARNING: an .old key was provided but "
        << "--encrypt-flow-config was not requested. If the flow config file is "
        << "currently encrypted with the old key and the old key is removed, you won't be able to recover these values!\n";
  }

  return 0;
} catch (const std::exception& ex) {
  std::cerr << ex.what() << "\n(" << typeid(ex).name() << ")\n";
  return 2;
} catch (...) {
  std::cerr << "Unknown error\n";
  return 3;
}
