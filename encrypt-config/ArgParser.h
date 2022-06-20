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
#pragma once

#include <string>
#include <set>
#include <vector>
#include <map>
#include <optional>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace encrypt_config {

struct Argument {
  std::set<std::string> names;
  bool required;
  std::string value_name;
  std::string description;
};

struct Flag {
  std::set<std::string> names;
  std::string description;
};

class Arguments {
  static const std::vector<Argument> registered_args_;
  static const std::vector<Flag> registered_flags_;

  void set(const std::string& key, const std::string& value);

  void set(const std::string& flag);

  static std::optional<Argument> getArg(const std::string& key);
  static std::optional<Flag> getFlag(const std::string& name);

 public:
  static Arguments parse(int argc, char* argv[]);

  static std::string getHelp();

  [[nodiscard]] std::optional<std::string> get(const std::string& key) const;

  [[nodiscard]] bool isSet(const std::string& flag) const;

 private:
  [[nodiscard]] std::optional<std::string> get(const Argument& arg) const;

  std::map<std::string, std::string> args_;
  std::set<std::string> flags_;
};

}  // namespace encrypt_config
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
