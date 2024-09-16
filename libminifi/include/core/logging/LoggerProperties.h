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

#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>

#include "spdlog/sinks/sink.h"

#include "properties/Properties.h"

namespace org::apache::nifi::minifi::core::logging {

class LoggerProperties : public PropertiesImpl {
 public:
  LoggerProperties()
      : PropertiesImpl("Logger properties") {
  }
  /**
   * Gets all keys that start with the given prefix and do not have a "." after the prefix and "." separator.
   *
   * Ex: with type argument "appender"
   * you would get back a property of "appender.rolling" but not "appender.rolling.file_name"
   */
  std::vector<std::string> get_keys_of_type(const std::string &type) const;

  /**
   * Registers a sink witht the given name. This allows for programmatic definition of sinks.
   */
  void add_sink(const std::string &name, const std::shared_ptr<spdlog::sinks::sink>& sink) {
    sinks_[name] = sink;
  }
  std::map<std::string, std::shared_ptr<spdlog::sinks::sink>> initial_sinks() {
    return sinks_;
  }

 private:
  std::map<std::string, std::shared_ptr<spdlog::sinks::sink>> sinks_;
};

}  // namespace org::apache::nifi::minifi::core::logging





