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
#include "ArgParser.h"
#include "CommandException.h"

using org::apache::nifi::minifi::encrypt_config::Arguments;
using org::apache::nifi::minifi::encrypt_config::EncryptConfig;
using org::apache::nifi::minifi::encrypt_config::CommandException;

int main(int argc, char* argv[]) try {
  Arguments args = Arguments::parse(argc, argv);
  EncryptConfig encrypt_config{args.get("-m").value()};
  EncryptConfig::EncryptionType type = encrypt_config.encryptSensitiveProperties();
  if (args.isSet("--encrypt-flow-config")) {
    encrypt_config.encryptFlowConfig();
  } else if (type == EncryptConfig::EncryptionType::RE_ENCRYPT) {
    std::cout << "WARNING: you did not request the flow config to be updated, "
              << "if it is currently encrypted and the old key is removed, "
              << "you won't be able to recover the flow config.\n";
  }
  return 0;
} catch (const CommandException& c_ex) {
  std::cerr << c_ex.what() << "\n";
  std::cerr << Arguments::getHelp() << "\n";
  return 1;
} catch (const std::exception& ex) {
  std::cerr << ex.what() << "\n(" << typeid(ex).name() << ")\n";
  return 2;
} catch (...) {
  std::cerr << "Unknown error\n";
  return 3;
}
