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

#include <fstream>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "../agent/JsonSchema.h"
#include "nlohmann/json-schema.hpp"
#include "utils/RegexUtils.h"
#include "utils/StringUtils.h"

struct JsonError {
  std::string path;
  std::string error;
};

class ErrorHandler : public nlohmann::json_schema::error_handler {
 public:
  explicit ErrorHandler(std::function<void(const JsonError&)> handler)
    : handler_(std::move(handler)) {}
  void error(const nlohmann::json::json_pointer& ptr, const nlohmann::json& /*instance*/, const std::string& message) override {
    handler_(JsonError{ptr.to_string(), message});
  }

 private:
  std::function<void(const JsonError&)> handler_;
};

void extractExpectedErrors(nlohmann::json& node, const std::string& path, std::unordered_map<std::string, std::string>& errors) {
  if (node.is_object() && node.contains("$err")) {
    errors[path] = node["$err"].get<std::string>();
    node = node["$value"];
  }
  if (node.is_object() || node.is_array()) {
    for (auto& [key, val] : node.items()) {
      extractExpectedErrors(val, utils::string::join_pack(path, "/", key), errors);
    }
  }
}

TEST_CASE("The generated JSON schema matches a valid json flow") {
  const nlohmann::json config_schema = nlohmann::json::parse(minifi::docs::generateJsonSchema());

  nlohmann::json_schema::json_validator validator;
  validator.set_root_schema(config_schema);

  auto config_json = R"(
    {
      "Flow Controller": {"name": "Test"},
      "Processors": [
        {
          "id": "00000000-0000-0000-0000-000000000000",
          "class": "GenerateFlowFile",
          "name": "Proc1",
          "scheduling strategy": "TIMER_DRIVEN",
          "scheduling period": "1 min",
          "Properties": {}
        }
      ],
      "Connections": [
        {
          "id": "00000000-0000-0000-0000-000000000000",
          "name": "Conn1",
          "source id": "00000000-0000-0000-0000-000000000000",
          "source relationship names": ["success"],
          "destination id": "00000000-0000-0000-0000-000000000000"
        }
      ],
      "Input Ports": [
        {"id": "00000000-0000-0000-0000-000000000000", "name": "In1"}
      ],
      "Output Ports": [
        {"id": "00000000-0000-0000-0000-000000000000", "name": "Out1"}
      ],
      "Funnels": [
        {"id": "00000000-0000-0000-0000-000000000000", "name": "Fun1"}
      ],
      "Process Groups": [
        {
          "name": "Group1",
          "Processors": [],
          "Connections": [],
          "Process Groups": []
        }
      ],
      "Remote Process Groups": [
        {
          "id": "00000000-0000-0000-0000-000000000000",
          "name": "RPG1",
          "Input Ports": [{
            "id": "00000000-0000-0000-0000-000000000000",
            "name": "RIn1",
            "Properties": {
              "Host Name": "localhost"
            }
          }]
        }
      ],
      "Controller Services": [
        {
          "id": "00000000-0000-0000-0000-000000000000",
          "class": "SSLContextService",
          "name": "Service1",
          "Properties": {
            "Client Certificate": "",
            "Private Key": "",
            "Passphrase": "",
            "CA Certificate": ""
          }
        }
      ]
    }
  )"_json;

  validator.validate(config_json);
}

TEST_CASE("The JSON schema detects invalid values in the json flow") {
  std::ofstream{"/Users/adebreceni/work/minifi-homes/json-schema-test/schema.json"} << minifi::docs::generateJsonSchema();
  const nlohmann::json config_schema = nlohmann::json::parse(minifi::docs::generateJsonSchema());

  nlohmann::json_schema::json_validator validator;
  validator.set_root_schema(config_schema);

  // the objects of type {"$err": <error>, "$value": <value>} are special
  // in the sense that they are preprocessed, replaced by <value> and we expect
  // a validation error at the position of this object that matches the
  // regex <error>
  auto config_json = R"(
    {
      "Flow Controller": {"name": "Test"},
      "Processors": [
        {"$err": "property 'scheduling period' not found", "$value": {
          "id": "00000000-0000-0000-0000-000000000000",
          "class": "GenerateFlowFile",
          "name": "Proc1",
          "scheduling strategy": "TIMER_DRIVEN",
          "Properties": {}
        }},
        {
          "id": "00000000-0000-0000-0000-000000000000",
          "class": "GenerateFlowFile",
          "name": "Proc1",
          "scheduling strategy": "TIMER_DRIVEN",
          "scheduling period": "1 min",
          "Properties": {
            "Batch Size": {"$value": "not a number", "$err": "unexpected instance type"}
          }
        },
        {
          "id": "00000000-0000-0000-0000-000000000000",
          "class": "GenerateFlowFile",
          "name": "Proc1",
          "scheduling strategy": "TIMER_DRIVEN",
          "scheduling period": "1 min",
          "Properties": {"$err": "", "$value": {
            "No such property": 5
          }}
        }
      ],
      "Connections": [
        {"$err": "property 'name' not found", "$value": {
          "id": {"$value": "00000000-0000-0000-0000-00000000000", "$err": ""},
          "source id": "00000000-0000-0000-0000-000000000000",
          "source relationship names": ["success"],
          "destination id": "00000000-0000-0000-0000-000000000000"
        }}
      ],
      "Input Ports": [
        {"id": "00000000-0000-0000-0000-000000000000", "name": {"$value": 5, "$err": "unexpected instance type"}}
      ],
      "Output Ports": [
        {"id": "00000000-0000-0000-0000-000000000000", "name": "Out1"}
      ],
      "Funnels": [
        {"id": {"$value": 5, "$err": "unexpected instance type"}}
      ],
      "Process Groups": [
        {"$err": "", "$value": {
          "name": "Group1",
          "no such property": []
        }}
      ],
      "Remote Process Groups": [
        {"$err": "property 'Input Ports' not found", "$value": {
          "id": "00000000-0000-0000-0000-000000000000",
          "name": "RPG1"
        }}
      ],
      "Controller Services": [
        {
          "id": {"$value": "00000000-0000-0000-0000-00000000000", "$err": ""},
          "class": "SSLContextService",
          "name": "Service1",
          "Properties": {
            "Client Certificate": 6,
            "Private Key": "",
            "Passphrase": "",
            "CA Certificate": ""
          }
        },
        {
          "id": "00000000-0000-0000-0000-000000000001",
          "class": "SSLContextService",
          "name": "Service1",
          "Properties": {
            "Client Certificate": {"$value": 6, "$err": "unexpected instance type"},
            "Private Key": "",
            "Passphrase": "",
            "CA Certificate": ""
          }
        },
        {"$value": "kenyer", "$err": "unexpected instance type"}
      ]
    }
  )"_json;

  std::unordered_map<std::string, std::string> errors;
  extractExpectedErrors(config_json, "", errors);

  ErrorHandler err_handler{[&] (auto err) {
    auto it = errors.find(err.path);
    if (it == errors.end()) {
      throw std::logic_error("Unexpected error in json flow at " + err.path + ": " + err.error);
    }
    if (!it->second.empty()) {
      minifi::utils::Regex re(it->second);
      if (!minifi::utils::regexSearch(err.error, re)) {
        throw std::logic_error("Error in json flow at " + err.path + " does not match expected pattern, expected: '" + it->second + "', actual: " + err.error);
      }
    }
    errors.erase(it);
  }};
  validator.validate(config_json, err_handler);

  // all expected errors should have been processed
  if (!errors.empty()) {
    for (const auto& [path, err] : errors) {
      std::cerr << "Expected error at " << path << ": " << err << std::endl;
    }
    throw std::logic_error("There were some expected errors that did not occur");
  }
}
