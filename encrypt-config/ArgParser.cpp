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
#include "utils/CollectionUtils.h"
#include "CommandException.h"

namespace org::apache::nifi::minifi::encrypt_config {

const std::vector<Argument> Arguments::registered_args_{
    {std::set<std::string>{"--minifi-home", "-m"},
     true,
     "minifi home",
     "Specifies the home directory used by the minifi agent"}
};

const std::vector<Flag> Arguments::registered_flags_{
    {std::set<std::string>{"--help", "-h"},
     "Prints this help message"},
    {std::set<std::string>{"--encrypt-flow-config"},
     "If set, the flow configuration file (as specified in minifi.properties) is also encrypted."}
};

std::string Arguments::getHelp() {
  std::stringstream ss;
  ss << "Usage: " << "encrypt-config";
  for (const auto& arg : registered_args_) {
    ss << " ";
    if (!arg.required) {
      ss << "[";
    }
    ss << utils::StringUtils::join("|", arg.names)
       << " <" << arg.value_name << ">";
    if (!arg.required) {
      ss << "]";
    }
  }
  for (const auto& flag : registered_flags_) {
    ss << " [" << utils::StringUtils::join("|", flag.names) << "]";
  }
  ss << "\n";
  for (const auto& arg : registered_args_) {
    ss << "\t";
    ss << utils::StringUtils::join("|", arg.names) << " : ";
    if (arg.required) {
      ss << "(required)";
    } else {
      ss << "(optional)";
    }
    ss << " " << arg.description;
    ss << "\n";
  }
  for (const auto& flag : registered_flags_) {
    ss << "\t" << utils::StringUtils::join("|", flag.names) << " : "
        << flag.description << "\n";
  }
  return ss.str();
}

void Arguments::set(const std::string& key, const std::string& value) {
  if (get(key)) {
    throw CommandException("Key is specified more than once \"" + key + "\"");
  }
  args_[key] = value;
}

void Arguments::set(const std::string& flag) {
  if (isSet(flag)) {
    throw CommandException("Flag is specified more than once \"" + flag + "\"");
  }
  flags_.insert(flag);
}

std::optional<std::string> Arguments::get(const std::string &key) const {
  return getArg(key) | utils::flatMap([&] (const Argument& arg) {return get(arg);});
}

std::optional<std::string> Arguments::get(const Argument& arg) const {
  for (const auto& name : arg.names) {
    auto it = args_.find(name);
    if (it != args_.end()) {
      return it->second;
    }
  }
  return {};
}

bool Arguments::isSet(const std::string &flag) const {
  std::optional<Flag> opt_flag = getFlag(flag);
  if (!opt_flag) {
    return false;
  }
  return utils::haveCommonItem(opt_flag->names, flags_);
}

Arguments Arguments::parse(int argc, char* argv[]) {
  Arguments args;
  for (int argIdx = 1; argIdx < argc; ++argIdx) {
    std::string key{argv[argIdx]};
    if (getFlag(key)) {
      args.set(key);
      continue;
    }
    if (!getArg(key)) {
      throw CommandException("Unrecognized option: \"" + key + "\"");
    }
    if (argIdx == argc - 1) {
      throw CommandException("No value specified for key \"" + key + "\"");
    }
    ++argIdx;
    std::string value{argv[argIdx]};
    args.set(key, value);
  }
  if (args.isSet("-h")) {
    std::cout << getHelp();
    std::exit(0);
  }
  for (const auto& arg : registered_args_) {
    if (arg.required && !args.get(arg)) {
      throw CommandException("Missing required option " + utils::StringUtils::join("|", arg.names));
    }
  }
  return args;
}

std::optional<Flag> Arguments::getFlag(const std::string &name) {
  for (const auto& flag : registered_flags_) {
    if (flag.names.contains(name)) {
      return flag;
    }
  }
  return {};
}

std::optional<Argument> Arguments::getArg(const std::string &key) {
  for (const auto& arg : registered_args_) {
    if (arg.names.contains(key)) {
      return arg;
    }
  }
  return {};
}

}  // namespace org::apache::nifi::minifi::encrypt_config
