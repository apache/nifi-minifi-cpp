// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "MetricsFilter.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::kubernetes::metrics {

std::expected<std::string, std::string> filter(const std::string& metrics_json, const std::function<bool(const kubernetes::ContainerInfo&)>& filter_function) {
  rapidjson::Document document;
  rapidjson::ParseResult parse_result = document.Parse<rapidjson::kParseStopWhenDoneFlag>(metrics_json.data());
  if (parse_result.IsError()) {
    return std::unexpected{utils::string::join_pack("Error parsing the metrics received from the Kubernetes API at offset ",
        std::to_string(parse_result.Offset()), ": ", rapidjson::GetParseError_En(parse_result.Code()))};
  }

  if (!document.HasMember("items") || !document["items"].IsArray()) {
    return std::unexpected{"Unexpected JSON received from the Kubernetes API: missing list of 'items'"};
  }

  rapidjson::Value& pods = document["items"];
  for (rapidjson::Value::ValueIterator pod_it = pods.Begin(); pod_it != pods.End(); /* don't increment here, as we may remove elements */) {
    rapidjson::Value& pod_metrics = *pod_it;

    if (!pod_metrics.HasMember("metadata") || !pod_metrics["metadata"].IsObject()) { ++pod_it; continue; }
    const rapidjson::Value& metadata = pod_metrics["metadata"];

    if (!metadata.HasMember("namespace") || !metadata["namespace"].IsString()) { ++pod_it; continue; }
    const std::string name_space = metadata["namespace"].GetString();

    if (!metadata.HasMember("name") || !metadata["name"].IsString()) { ++pod_it; continue; }
    const std::string pod_name = metadata["name"].GetString();

    if (!pod_metrics.HasMember("containers") || !pod_metrics["containers"].IsArray()) { ++pod_it; continue; }
    rapidjson::Value& containers = pod_metrics["containers"];

    for (rapidjson::Value::ValueIterator container_it = containers.Begin(); container_it != containers.End(); /* don't increment here, as we may remove elements */) {
      const rapidjson::Value& container_metrics = *container_it;

      if (!container_metrics.HasMember("name") || !container_metrics["name"].IsString()) { ++container_it; continue; }
      const std::string container_name = container_metrics["name"].GetString();

      if (!filter_function(kubernetes::ContainerInfo{.name_space = name_space, .pod_name = pod_name, .container_name = container_name})) {
        container_it = containers.Erase(container_it);
      } else {
        ++container_it;
      }
    }

    if (containers.Empty()) {
      pod_it = pods.Erase(pod_it);
    } else {
      ++pod_it;
    }
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  document.Accept(writer);
  return buffer.GetString();
}

}  // namespace org::apache::nifi::minifi::kubernetes::metrics
