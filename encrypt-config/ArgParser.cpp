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

#include <string>
#include <set>
#include <iostream>
#include <algorithm>
#include "ArgParser.h"
#include "utils/OptionalUtils.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace encrypt_config {

const std::vector<Argument> Arguments::simple_arguments_{
    {std::vector<std::string>{"--minifi-home", "-m"},
     true,
     "minifi home",
     "Specifies the home directory used by the minifi agent"}
};

const std::vector<FlagArgument> Arguments::flag_arguments_{
    {std::vector<std::string>{"--help", "-h"},
     "Prints this help message"},
    {std::vector<std::string>{"--encrypt-flow-config"},
     "If set, the flow configuration file (as specified in minifi.properties) is also encrypted."}
};

std::string Arguments::getHelp() {
  std::stringstream ss;
  ss << "Usage: " << "encrypt-config";
  for (const auto& simple_arg : simple_arguments_) {
    ss << " ";
    if (!simple_arg.required) {
      ss << "[";
    }
    ss << utils::StringUtils::join("|", simple_arg.names)
        << " <" << simple_arg.value_name << ">";
    if (!simple_arg.required) {
      ss << "]";
    }
  }
  for (const auto& flag : flag_arguments_) {
    ss << " [" << utils::StringUtils::join("|", flag.names) << "]";
  }
  ss << std::endl;
  for (const auto& simple_arg : simple_arguments_) {
    ss << "\t";
    ss << utils::StringUtils::join("|", simple_arg.names) << " : ";
    if (simple_arg.required) {
      ss << "(required)";
    } else {
      ss << "(optional)";
    }
    ss << " " << simple_arg.description;
    ss << std::endl;
  }
  for (const auto& flag : flag_arguments_) {
    ss << "\t" << utils::StringUtils::join("|", flag.names) << " : "
        << flag.description << std::endl;
  }
  return ss.str();
}

void Arguments::set(const std::string& key, const std::string& value) {
  if (get(key)) {
    std::cerr << "Key is specified more than once \"" << key << "\"" << std::endl;
    std::cerr << getHelp();
    std::exit(1);
  }
  simple_args_[key] = value;
}

void Arguments::set(const std::string& flag) {
  if (isSet(flag)) {
    std::cerr << "Flag is specified more than once \"" << flag << "\"" << std::endl;
    std::cerr << getHelp();
    std::exit(1);
  }
  flag_args_.insert(flag);
}

utils::optional<std::string> Arguments::get(const std::string &key) const {
  utils::optional<Argument> opt_arg = getSimpleArg(key);
  if (!opt_arg) {
    return {};
  }
  for (const auto& name : opt_arg->names) {
    auto it = simple_args_.find(name);
    if (it != simple_args_.end()) {
      return it->second;
    }
  }
  return {};
}

bool Arguments::isSet(const std::string &flag) const {
  utils::optional<FlagArgument> opt_flag = getFlag(flag);
  if (!opt_flag) {
    return false;
  }
  return std::any_of(opt_flag->names.begin(), opt_flag->names.end(), [&] (const std::string& name) {
    return flag_args_.find(name) != flag_args_.end();
  });
}

Arguments Arguments::parse(int argc, char* argv[]) {
  Arguments args;
  for (int argIdx = 1; argIdx < argc; ++argIdx) {
    std::string key{argv[argIdx]};
    if (getFlag(key)) {
      args.set(key);
      continue;
    }
    if (!getSimpleArg(key)) {
      std::cerr << "Unrecognized option: \"" << key << "\"" << std::endl;
      std::cerr << getHelp();
      std::exit(1);
    }
    if (argIdx == argc - 1) {
      std::cerr << "No value specified for key \"" << key << "\"" << std::endl;
      std::cerr << getHelp();
      std::exit(1);
    }
    ++argIdx;
    std::string value{argv[argIdx]};
    args.set(key, value);
  }
  if (args.isSet("-h")) {
    std::cout << getHelp();
    std::exit(0);
  }
  for (const auto& simple_arg : simple_arguments_) {
    if (simple_arg.required) {
      if (std::none_of(simple_arg.names.begin(), simple_arg.names.end(), [&] (const std::string& name) {
        return static_cast<bool>(args.get(name));
      })) {
        std::cerr << "Missing required option " << utils::StringUtils::join("|", simple_arg.names) << std::endl;
        std::cerr << getHelp();
        std::exit(1);
      }
    }
  }
  return args;
}

utils::optional<FlagArgument> Arguments::getFlag(const std::string &name) {
  for (const auto& flag : flag_arguments_) {
    if (std::find(flag.names.begin(), flag.names.end(), name) != flag.names.end()) {
      return flag;
    }
  }
  return {};
}

utils::optional<Argument> Arguments::getSimpleArg(const std::string &key) {
  for (const auto& arg : simple_arguments_) {
    if (std::find(arg.names.begin(), arg.names.end(), key) != arg.names.end()) {
      return arg;
    }
  }
  return {};
}

}  // namespace encrypt_config
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

