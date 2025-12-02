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

#include "agent/JsonSchema.h"

#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

#include "RemoteProcessGroupPort.h"
#include "minifi-cpp/agent/agent_docs.h"
#include "minifi-cpp/agent/agent_version.h"
#include "minifi-cpp/utils/gsl.h"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/filter.hpp"
#include "range/v3/view/join.hpp"
#include "range/v3/view/transform.hpp"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

namespace org::apache::nifi::minifi::docs {

static std::string escape(std::string str) {
  utils::string::replaceAll(str, "\\", "\\\\");
  utils::string::replaceAll(str, "\"", "\\\"");
  utils::string::replaceAll(str, "\n", "\\n");
  return str;
}

static std::string prettifyJson(const std::string& str) {
  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.Parse(str.c_str(), str.length());
  gsl_Assert(res);

  rapidjson::StringBuffer buffer;

  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);

  return std::string{buffer.GetString(), buffer.GetSize()};
}

void writePropertySchema(const core::Property& prop, std::ostream& out) {
  out << "\"" << escape(prop.getName()) << "\" : {";
  out << R"("description": ")" << escape(prop.getDescription()) << "\"";
  if (const auto& values = prop.getAllowedValues(); !values.empty()) {
    out << R"(, "enum": [)"
        << (values
            | ranges::views::transform([] (auto& val) {return '"' + escape(val) + '"';})
            | ranges::views::join(',')
            | ranges::to<std::string>())
        << "]";
  }
  if (const auto validator_name = prop.getValidator().getEquivalentNifiStandardValidatorName()) {
    if (validator_name == core::StandardPropertyValidators::INTEGER_VALIDATOR.getEquivalentNifiStandardValidatorName() ||
        validator_name == core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
      out << R"(, "type": "integer")";
      if (const auto default_value = prop.getDefaultValue()) {
        out << R"(, "default": )" << *default_value;
      }
    } else if (validator_name == core::StandardPropertyValidators::BOOLEAN_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
      out << R"(, "type": "boolean")";
      if (const auto default_value = prop.getDefaultValue()) {
        out << R"(, "default": )" << *default_value;
      }
    } else {
      out << R"(, "type": "string")";
      if (const auto default_value = prop.getDefaultValue()) {
        out << R"(, "default": ")" << escape(*default_value) << '"';
      }
    }
  } else {
    out << R"(, "type": "string")";
    if (const auto default_value = prop.getDefaultValue()) {
      out << R"(, "default": ")" << escape(*default_value) << '"';
    }
  }


  out << "}";
}

template<typename PropertyContainer>
void writeProperties(const PropertyContainer& props, bool supports_dynamic, std::ostream& out) {
  out << R"("Properties": {)"
        << R"("type": "object",)"
        << R"("additionalProperties": )" << (supports_dynamic? "true" : "false") << ","
        << R"("required": [)"
        << (props
            | ranges::views::filter([] (auto& prop) {return prop.getRequired() && !prop.getDefaultValue().has_value();})
            | ranges::views::transform([] (auto& prop) {return '"' + escape(prop.getName()) + '"';})
            | ranges::views::join(',')
            | ranges::to<std::string>())
        << "]";

  out << R"(, "properties": {)";
  for (size_t prop_idx = 0; prop_idx < props.size(); ++prop_idx) {
    const auto& property = props[prop_idx];
    if (prop_idx != 0) out << ",";
    writePropertySchema(property, out);
  }
  out << "}";  // "properties"
  out << "}";  // "Properties"
}

static std::string buildSchema(const std::unordered_map<std::string, std::string>& relationships, const std::string& processors, const std::string& controller_services) {
  std::stringstream all_rels;
  for (const auto& [name, rels] : relationships) {
    all_rels << "\"relationships-" << escape(name) << "\": " << rels << ", ";
  }

  const auto rpg_property_refs = minifi::RemoteProcessGroupPort::Properties;
  std::vector<core::Property> rpg_properties(rpg_property_refs.begin(), rpg_property_refs.end());

  std::stringstream remote_port_props;
  writeProperties(rpg_properties, minifi::RemoteProcessGroupPort::SupportsDynamicProperties, remote_port_props);

  std::string process_group_properties = R"(
    "Processors": {
      "type": "array",
      "items": {"$ref": "#/definitions/processor"}
    },
    "Connections": {
      "type": "array",
      "items": {"$ref": "#/definitions/connection"}
    },
    "Controller Services": {
      "type": "array",
      "items": {"$ref": "#/definitions/controller_service"}
    },
    "Remote Process Groups": {
      "type": "array",
      "items": {"$ref": "#/definitions/remote_process_group"}
    },
    "Process Groups": {
      "type": "array",
      "items": {"$ref": "#/definitions/simple_process_group"}
    },
    "Funnels": {
      "type": "array",
      "items": {"$ref": "#/definitions/funnel"}
    },
    "Input Ports": {
      "type": "array",
      "items": {"$ref": "#/definitions/port"}
    },
    "Output Ports": {
      "type": "array",
      "items": {"$ref": "#/definitions/port"}
    }
  )";

  std::stringstream cron_pattern;
  {
    const char* all = "\\\\*";
    const char* any = "\\\\?";
    const char* increment = "(-?[0-9]+)";
    const char* secs = "([0-5]?[0-9])";
    const char* mins = "([0-5]?[0-9])";
    const char* hours = "(1?[0-9]|2[0-3])";
    const char* days = "([1-2]?[0-9]|3[0-1])";
    const char* months = "([0-9]|1[0-2]|jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec)";
    const char* weekdays = "([0-7]|sun|mon|tue|wed|thu|fri|sat)";
    const char* years = "([0-9]+)";

    auto makeCommon = [&] (const char* pattern) {
      std::stringstream common;
      common << all << "|" << any
        << "|" << pattern << "(," << pattern << ")*"
        << "|" << pattern << "-" << pattern
        << "|" << "(" << all << "|" << pattern << ")" << "/" << increment;
      return std::move(common).str();
    };

    cron_pattern << "^"
      << "(" << makeCommon(secs) << ")"
      << " (" << makeCommon(mins) << ")"
      << " (" << makeCommon(hours) << ")"
      << " (" << makeCommon(days) << "|LW|L|L-" << days << "|" << days << "W" << ")"
      << " (" << makeCommon(months) << ")"
      << " (" << makeCommon(weekdays) << "|" << weekdays << "?L|" << weekdays << "#" << "[1-5]" << ")"
      << "( (" << makeCommon(years) << "))?"
      << "$";
  }

  // the schema specification does not allow case-insensitive regex
  std::stringstream cron_pattern_case_insensitive;
  for (char ch : cron_pattern.str()) {
    if (std::isalpha(static_cast<unsigned char>(ch))) {
      cron_pattern_case_insensitive << "["
          << static_cast<char>(std::tolower(static_cast<unsigned char>(ch)))
          << static_cast<char>(std::toupper(static_cast<unsigned char>(ch)))
          << "]";
    } else {
      cron_pattern_case_insensitive << ch;
    }
  }

  return prettifyJson(R"(
{
  "$schema": "http://json-schema.org/draft-07/schema",
  "definitions": {)" + std::move(all_rels).str() + R"(
    "datasize": {
      "type": "string",
      "pattern": "^\\s*[0-9]+\\s*(B|K|M|G|T|P|KB|MB|GB|TB|PB)\\s*$"
    },
    "uuid": {
      "type": "string",
      "pattern": "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$",
      "default": "00000000-0000-0000-0000-000000000000"
    },
    "cron_pattern": {
      "type": "string",
      "pattern": ")" + std::move(cron_pattern_case_insensitive).str() + R"("
    },
    "remote_port": {
      "type": "object",
      "required": ["name", "id", "Properties"],
      "properties": {
        "name": {"type": "string"},
        "id": {"$ref": "#/definitions/uuid"},
        "max concurrent tasks": {"type": "integer"},
        )" + std::move(remote_port_props).str() +  R"(
      }
    },
    "port": {
      "type": "object",
      "required": ["name", "id"],
      "properties": {
        "name": {"type": "string"},
        "id": {"$ref": "#/definitions/uuid"}
      }
    },
    "time": {
      "type": "string",
      "pattern": "^\\s*[0-9]+\\s*(ns|nano|nanos|nanoseconds|nanosecond|us|micro|micros|microseconds|microsecond|msec|ms|millisecond|milliseconds|msecs|millis|milli|sec|s|second|seconds|secs|min|m|mins|minute|minutes|h|hr|hour|hrs|hours|d|day|days)\\s*$"
    },
    "controller_service": {"allOf": [{
      "type": "object",
      "required": ["name", "id", "class"],
      "properties": {
        "name": {"type": "string"},
        "class": {"type": "string"},
        "id": {"$ref": "#/definitions/uuid"}
      }
    }, )" + controller_services + R"(]},
    "processor": {"allOf": [{
      "type": "object",
      "required": ["name", "id", "class", "scheduling strategy"],
      "additionalProperties": false,
      "properties": {
        "name": {"type": "string"},
        "id": {"$ref": "#/definitions/uuid"},
        "class": {"type": "string"},
        "max concurrent tasks": {"type": "integer", "default": 1},
        "penalization period": {"$ref": "#/definitions/time"},
        "yield period": {"$ref": "#/definitions/time"},
        "run duration nanos": {"$ref": "#/definitions/time"},
        "Properties": {},
        "scheduling strategy": {"enum": ["EVENT_DRIVEN", "TIMER_DRIVEN", "CRON_DRIVEN"]},
        "scheduling period": {},
        "auto-terminated relationships list": {
          "type": "array",
          "items": {
            "type": "string"
          },
          "uniqueItems": true
        }
      }}, {
        "if": {"properties": {"scheduling strategy": {"const": "EVENT_DRIVEN"}}},
        "then": {"properties": {"scheduling period": false}}
      }, {
        "if": {"properties": {"scheduling strategy": {"const": "TIMER_DRIVEN"}}},
        "then": {"required": ["scheduling period"], "properties": {"scheduling period": {"$ref": "#/definitions/time"}}}
      }, {
        "if": {"properties": {"scheduling strategy": {"const": "CRON_DRIVEN"}}},
        "then": {"required": ["scheduling period"], "properties": {"scheduling period": {"$ref": "#/definitions/cron_pattern"}}}
      })" + (!processors.empty() ? ", " : "") + processors + R"(]
    },
    "remote_process_group": {"allOf": [{
      "type": "object",
      "required": ["name", "id", "Input Ports"],
      "properties": {
        "name": {"type": "string"},
        "id": {"$ref": "#/definitions/uuid"},
        "url": {"type": "string"},
        "yield period": {"$ref": "#/definitions/time"},
        "timeout": {"$ref": "#/definitions/time"},
        "local network interface": {"type": "string"},
        "transport protocol": {"enum": ["HTTP", "RAW"]},
        "Input Ports": {
          "type": "array",
          "items": {"$ref": "#/definitions/remote_port"}
        },
        "Output Ports": {
          "type": "array",
          "items": {"$ref": "#/definitions/remote_port"}
        }
      }
    }, {
      "if": {"properties": {"transport protocol": {"const": "HTTP"}}},
      "then": {"properties": {
        "proxy host": {"type": "string"},
        "proxy user": {"type": "string"},
        "proxy password": {"type": "string"},
        "proxy port": {"type": "integer"}
      }}
    }]},
    "connection": {
      "type": "object",
      "additionalProperties": false,
      "required": ["name", "id", "source id", "source relationship names", "destination id"],
      "properties": {
        "name": {"type": "string"},
        "id": {"$ref": "#/definitions/uuid"},
        "source name": {"type": "string"},
        "source id": {"$ref": "#/definitions/uuid"},
        "source relationship names": {
          "type": "array",
          "items": {"type": "string"}
        },
        "destination name": {"type": "string"},
        "destination id": {"$ref": "#/definitions/uuid"},
        "max work queue size": {"type": "integer", "default": 10000},
        "max work queue data size": {"$ref": "#/definitions/datasize", "default": "10 MB"},
        "flowfile expiration": {"$ref": "#/definitions/time", "default": "0 ms"}
      }
    },
    "funnel": {
      "type": "object",
      "required": ["id"],
      "properties": {
        "id": {"$ref": "#/definitions/uuid"},
        "name": {"type": "string"}
      }
    },
    "simple_process_group": {
      "type": "object",
      "required": ["name"],
      "additionalProperties": false,
      "properties": {
        "name": {"type": "string"},
        "version": {"type": "integer"},
        )" + process_group_properties + R"(
      }
    },
    "root_process_group": {
      "type": "object",
      "required": ["Flow Controller"],
      "additionalProperties": false,
      "properties": {
        "$schema": {"type": "string"},
        "Flow Controller": {
          "type": "object",
          "required": ["name"],
          "properties": {
            "name": {"type": "string"},
            "version": {"type": "integer"}
          }
        },
        )" + process_group_properties + R"(
      }
    }
  },
  "$ref": "#/definitions/root_process_group"
}
)");
}

std::string generateJsonSchema() {
  std::unordered_map<std::string, std::string> relationships;
  std::vector<std::string> proc_schemas;
  auto putProcSchema = [&] (const ClassDescription& proc) {
    std::stringstream schema;
    schema
        << "{"
        << R"("if": {"properties": {"class": {"const": ")" << escape(proc.short_name_) << "\"}}},"
        << R"("then": {)"
        << R"("required": ["Properties"],)"
        << R"("properties": {)";

    if (proc.isSingleThreaded_) {
      schema << R"("max concurrent tasks": {"const": 1},)";
    }

    schema << R"("auto-terminated relationships list": {"items": {"$ref": "#/definitions/relationships-)" << escape(proc.short_name_) << "\"}},";
    {
      std::stringstream rel_schema;
      rel_schema << R"({"anyOf": [)";
      if (proc.supports_dynamic_relationships_) {
        rel_schema << R"({"type": "string"})";
      }
      for (size_t rel_idx = 0; rel_idx < proc.class_relationships_.size(); ++rel_idx) {
        if (rel_idx != 0 || proc.supports_dynamic_relationships_) rel_schema << ", ";
        rel_schema << R"({"const": ")" << escape(proc.class_relationships_[rel_idx].getName()) << "\"}";
      }
      rel_schema << "]}";
      relationships[proc.short_name_] = std::move(rel_schema).str();
    }

    writeProperties(proc.class_properties_, proc.supports_dynamic_properties_, schema);

    schema << "}";  // "properties"
    schema << "}";  // "then"
    schema << "}";  // if-block

    proc_schemas.push_back(std::move(schema).str());
  };

  std::vector<std::string> controller_services;
  auto putControllerService = [&] (const ClassDescription& service) {
    std::stringstream schema;
    schema
        << "{"
        << R"("if": {"properties": {"class": {"const": ")" << escape(service.short_name_) << "\"}}},"
        << R"("then": {)"
        << R"("required": ["Properties"],)"
        << R"("properties": {)";

    writeProperties(service.class_properties_, service.supports_dynamic_properties_, schema);

    schema << "}";  // "properties"
    schema << "}";  // "then"
    schema << "}";  // if-block

    controller_services.push_back(std::move(schema).str());
  };

  for (const auto& [bundle_id, components]: ClassDescriptionRegistry::getClassDescriptions()) {
    for (const auto& proc : components.processors) {
      putProcSchema(proc);
    }
    for (const auto& service : components.controller_services) {
      putControllerService(service);
    }
  }

  return buildSchema(relationships, utils::string::join(", ", proc_schemas), utils::string::join(", ", controller_services));
}

}  // namespace org::apache::nifi::minifi::docs
