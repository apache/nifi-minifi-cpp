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
  argument_parser.add_argument("-e", "--encrypt-flow-config")
    .default_value(false)
    .implicit_value(true)
    .help("If set, the flow configuration file (as specified in minifi.properties) is also encrypted.");

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << argument_parser;
    std::exit(1);
  }

  EncryptConfig encrypt_config{argument_parser.get("-m")};
  EncryptConfig::EncryptionType type = encrypt_config.encryptSensitiveProperties();
  if (argument_parser.get<bool>("--encrypt-flow-config")) {
    encrypt_config.encryptFlowConfig();
  } else if (type == EncryptConfig::EncryptionType::RE_ENCRYPT) {
    std::cout << "WARNING: you did not request the flow config to be updated, "
              << "if it is currently encrypted and the old key is removed, "
              << "you won't be able to recover the flow config.\n";
  }
  return 0;
} catch (const std::exception& ex) {
  std::cerr << ex.what() << "\n(" << typeid(ex).name() << ")\n";
  return 2;
} catch (...) {
  std::cerr << "Unknown error\n";
  return 3;
}
