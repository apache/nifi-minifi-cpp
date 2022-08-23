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

#include "JsonSchema.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "agent/agent_version.h"
#include "agent/build_description.h"

namespace org::apache::nifi::minifi::docs {

static std::string escape(std::string str) {
  utils::StringUtils::replaceAll(str, "\"", "\\\"");
  utils::StringUtils::replaceAll(str, "\n", "");
  return str;
}

static std::string buildSchema(const std::unordered_map<std::string, std::string>& relationships, const std::string& processors) {
  std::stringstream conn_source_rels;
  std::stringstream all_rels;
  for (const auto& [name, rels] : relationships) {
    all_rels << "\"relationships-" << escape(name) << "\": " << rels << ", ";
    conn_source_rels
      << "{\"if\": {\"properties\": {\"name\": {\"pattern\": \"^" << escape(name) << "\"}}},"
      << "\"then\": {\"properties\": {\"source relationship names\": {\"items\": {\"$ref\": \"#/$defs/relationships-" << escape(name) << "\"}}}}},\n";
  }
  return R"(
{
  "$schema": "http://json-schema.org/draft-07/schema",
  "$defs": {)" + std::move(all_rels).str() + R"(
    "datasize": {
      "type": "string",
      "pattern": "^\\s*[0-9]+\\s*(B|K|M|G|T|P|KB|MB|GB|TB|PT)\\s*$"
    },
    "uuid": {
      "type": "string",
      "pattern": "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$",
      "default": "00000000-0000-0000-000000000000"
    },
    "cron_pattern": {
      "type": "string",
      "pattern": ""
    },
    "time": {
      "type": "string",
      "pattern": "^\\s*[0-9]+\\s*(ns|nano|nanos|nanoseconds|nanosecond|us|micro|micros|microseconds|microsecond|msec|ms|millisecond|milliseconds|msecs|millis|milli|sec|s|second|seconds|secs|min|m|mins|minute|minutes|h|hr|hour|hrs|hours|d|day|days)\\s*$"
    },
    "processor": {"allOf": [{
      "type": "object",
      "required": ["name", "id", "class", "scheduling strategy"],
      "additionalProperties": false,
      "properties": {
        "name": {"type": "string"},
        "id": {"$ref": "#/$defs/uuid"},
        "class": {"type": "string"},
        "max concurrent tasks": {"type": "integer", "default": 1},
        "penalization period": {"$ref": "#/$defs/time"},
        "yield period": {"$ref": "#/$defs/time"},
        "run duration nanos": {"$ref": "#/$defs/time"},
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
        "then": {"required": ["scheduling period"], "properties": {"scheduling period": {"$ref": "#/$defs/time"}}}
      }, {
        "if": {"properties": {"scheduling strategy": {"const": "CRON_DRIVEN"}}},
        "then": {"required": ["scheduling period"], "properties": {"scheduling period": {"$ref": "#/$defs/cron_pattern"}}}
      }]
    }
  },
  "type": "object",
  "additionalProperties": false,
  "properties": {
    "$schema": {"type": "string"},
    "Flow Controller": {
      "type": "object"
    },
    "Processors": {
      "type": "array",
      "items": {
        "allOf": [{"$ref": "#/$defs/processor"}, )" + processors + R"(]
      }
    },
    "Connections": {
      "type": "array",
      "items": {"allOf": [)" + std::move(conn_source_rels).str() + R"({
        "type": "object",
        "additionalProperties": false,
        "required": ["name", "id", "source name", "source id", "source relationship names", "destination name", "destination id"],
        "properties": {
          "name": {"type": "string"},
          "id": {"$ref": "#/$defs/uuid"},
          "source name": {"type": "string"},
          "source id": {"$ref": "#/$defs/uuid"},
          "source relationship names": {
            "type": "array",
            "items": {"type": "string"}
          },
          "destination name": {"type": "string"},
          "destination id": {"$ref": "#/$defs/uuid"},
          "max work queue size": {"type": "integer", "default": 10000},
          "max work queue data size": {"$ref": "#/$defs/datasize", "default": "10 MB"},
          "flowfile expiration": {"$ref": "#/$defs/time", "default": "0 ms"}
        }
      }]}
    },
    "Remote Processing Groups": {
      "type": "array",
      "items": {

      }
    }
  }
}
)";
}

std::string generateJsonSchema() {
  std::unordered_map<std::string, std::string> relationships;
  std::vector<std::string> proc_schemas;
  auto putProcSchema = [&] (const ClassDescription& proc) {
    std::stringstream schema;
    schema
        << "{\n"
        << "\"if\": {\"properties\": {\"class\": {\"const\": \"" << escape(proc.short_name_) << "\"}}},\n"
        << "\"then\": {\n"
        << "\"required\": [\"Properties\"],\n"
        << "\"properties\": {\n";

    if (proc.isSingleThreaded_) {
      schema << "\"max concurrent tasks\": {\"const\": 1},\n";
    }

    schema << "\"auto-terminated relationships list\": {\"items\": {\"$ref\": \"#/$defs/relationships-" << escape(proc.short_name_) << "\"}},\n";
    {
      std::stringstream rel_schema;
      rel_schema << "{\"anyOf\": [";
      if (proc.dynamic_relationships_) {
        rel_schema << "{\"type\": \"string\"}\n";
      }
      for (size_t rel_idx = 0; rel_idx < proc.class_relationships_.size(); ++rel_idx) {
        if (rel_idx != 0 || proc.dynamic_relationships_) rel_schema << ", ";
        rel_schema << "{\"const\": \"" << escape(proc.class_relationships_[rel_idx].getName()) << "\"}\n";
      }
      rel_schema << "]}\n";
      relationships[proc.short_name_] = std::move(rel_schema).str();
    }

    schema << "\"Properties\": {\n"
        << "\"type\": \"object\",\n"
        << "\"additionalProperties\": " << (proc.dynamic_properties_ ? "true" : "false") << ",\n";

    {
      schema << "\"required\": [";
      bool first = true;
      for (const auto& prop : proc.class_properties_) {
        if (!prop.getRequired()) continue;
        if (!first) schema << ", ";
        first = false;
        schema << "\"" << escape(prop.getName()) << "\"";
      }
      schema << "]\n";
    }

    schema << ", \"properties\": {\n";
    for (size_t prop_idx = 0; prop_idx < proc.class_properties_.size(); ++prop_idx) {
      const auto& property = proc.class_properties_[prop_idx];
      if (prop_idx != 0) schema << ",";
      schema << "\"" << escape(property.getName()) << "\" : {";
      schema << "\"description\": \"" << escape(property.getDescription()) << "\"\n";
      if (const auto& values = property.getAllowedValues(); !values.empty()) {
        schema << ", \"enum\": [";
        for (size_t val_idx = 0; val_idx < values.size(); ++val_idx) {
          if (val_idx != 0) schema << ", ";
          schema << "\"" << escape(values[val_idx].to_string()) << "\"";
        }
        schema << "]\n";
      }
      if (const auto& def_value = property.getDefaultValue(); !def_value.empty()) {
        const auto& type = def_value.getTypeInfo();
        if (type == state::response::Value::INT_TYPE
            || type == state::response::Value::INT64_TYPE
            || type == state::response::Value::UINT32_TYPE
            || type == state::response::Value::UINT64_TYPE) {
          schema << ", \"type\": \"integer\", \"default\": " << static_cast<int64_t>(def_value);
        } else if (type == state::response::Value::DOUBLE_TYPE) {
          schema << ", \"type\": \"number\", \"default\": " << static_cast<double>(def_value);
        } else if (type == state::response::Value::BOOL_TYPE) {
          schema << ", \"type\": \"boolean\", \"default\": " << static_cast<bool>(def_value);
        } else {
          schema << ", \"type\": \"string\", \"default\": \"" << escape(def_value.to_string()) << "\"";
        }
      }
      schema << "\n}\n";  // property.getName()
    }
    schema << "}\n";  // "properties"
    schema << "}\n";  // "Properties"
    schema << "}\n";  // "properties"
    schema << "}\n";  // "then"
    schema << "}\n";  // if-block

    proc_schemas.push_back(std::move(schema).str());
  };

  const auto& descriptions = AgentDocs::getClassDescriptions();
  for (const std::string& group : AgentBuild::getExtensions()) {
    auto it = descriptions.find(group);
    if (it == descriptions.end()) {
      continue;
    }
    for (const auto& proc : it->second.processors_) {
      putProcSchema(proc);
    }
  }

  for (const auto& bundle : ExternalBuildDescription::getExternalGroups()) {
    for (const auto& proc : ExternalBuildDescription::getClassDescriptions(bundle.artifact).processors_) {
      putProcSchema(proc);
    }
  }



  return buildSchema(relationships, utils::StringUtils::join(", ", proc_schemas));
}

}  // namespace org::apache::nifi::minifi::docs
