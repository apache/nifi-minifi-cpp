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
#include "SparkplugBWriter.h"

#include <vector>
#include <optional>
#include <concepts>

#include "sparkplug_b.pb.h"
#include "minifi-cpp/core/Record.h"
#include "core/Resource.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::controllers {

namespace {

template<std::integral T>
T getIntegral(const std::variant<std::string, int64_t, uint64_t, double, bool, std::chrono::system_clock::time_point, core::RecordArray, core::RecordObject>& variant) {
  if (std::holds_alternative<int64_t>(variant)) {
    return gsl::narrow<T>(std::get<int64_t>(variant));
  } else if (std::holds_alternative<uint64_t>(variant)) {
    return gsl::narrow<T>(std::get<uint64_t>(variant));
  } else {
    throw std::invalid_argument("Invalid type for integral conversion");
  }
}

void writeProperties(const core::RecordObject& properties, org::eclipse::tahu::protobuf::Payload_PropertySet* properties_proto);

template<typename T>
void writeMetric(const core::RecordObject& metric, T* metric_proto);

void writePropertyValue(const core::RecordObject& value, org::eclipse::tahu::protobuf::Payload_PropertyValue* value_proto) {
  if (value.contains("type")) {
    value_proto->set_type(getIntegral<uint32_t>(value.at("type").value_));
  }

  if (value.contains("is_null")) {
    value_proto->set_is_null(std::get<bool>(value.at("is_null").value_));
  }

  if (value.contains("int_value")) {
    value_proto->set_int_value(getIntegral<uint32_t>(value.at("int_value").value_));
  } else if (value.contains("long_value")) {
    value_proto->set_long_value(getIntegral<uint64_t>(value.at("long_value").value_));
  } else if (value.contains("float_value")) {
    value_proto->set_float_value(gsl::narrow<float>(std::get<double>(value.at("float_value").value_)));
  } else if (value.contains("double_value")) {
    value_proto->set_double_value(std::get<double>(value.at("double_value").value_));
  } else if (value.contains("boolean_value")) {
    value_proto->set_boolean_value(std::get<bool>(value.at("boolean_value").value_));
  } else if (value.contains("string_value")) {
    value_proto->set_string_value(std::get<std::string>(value.at("string_value").value_));
  } else if (value.contains("propertyset_value")) {
    const auto& propertyset = std::get<core::RecordObject>(value.at("propertyset_value").value_);
    auto* propertyset_proto = value_proto->mutable_propertyset_value();
    writeProperties(propertyset, propertyset_proto);
  } else if (value.contains("propertysets_value")) {
    const auto& propertysets = std::get<core::RecordArray>(value.at("propertysets_value").value_);
    auto* propertysets_proto = value_proto->mutable_propertysets_value();
    for (const auto& propertyset : propertysets) {
      auto* propertyset_proto = propertysets_proto->add_propertyset();
      writeProperties(std::get<core::RecordObject>(propertyset.value_), propertyset_proto);
    }
  }
}

void writeProperties(const core::RecordObject& properties, org::eclipse::tahu::protobuf::Payload_PropertySet* properties_proto) {
  if (properties.contains("keys")) {
    const auto& keys = std::get<core::RecordArray>(properties.at("keys").value_);
    for (const auto& key : keys) {
      properties_proto->add_keys(std::get<std::string>(key.value_));
    }
  }

  if (properties.contains("values")) {
    const auto& values = std::get<core::RecordArray>(properties.at("values").value_);
    for (const auto& value : values) {
      auto* value_proto = properties_proto->add_values();
      writePropertyValue(std::get<core::RecordObject>(value.value_), value_proto);
    }
  }
}

void writeMetadataToMetric(const core::RecordObject& metadata, org::eclipse::tahu::protobuf::Payload_MetaData* metadata_proto) {
  if (metadata.contains("is_multi_part")) {
    metadata_proto->set_is_multi_part(std::get<bool>(metadata.at("is_multi_part").value_));
  }
  if (metadata.contains("content_type")) {
    metadata_proto->set_content_type(std::get<std::string>(metadata.at("content_type").value_));
  }
  if (metadata.contains("size")) {
    metadata_proto->set_size(getIntegral<uint64_t>(metadata.at("size").value_));
  }
  if (metadata.contains("seq")) {
    metadata_proto->set_seq(getIntegral<uint64_t>(metadata.at("seq").value_));
  }
  if (metadata.contains("file_name")) {
    metadata_proto->set_file_name(std::get<std::string>(metadata.at("file_name").value_));
  }
  if (metadata.contains("file_type")) {
    metadata_proto->set_file_type(std::get<std::string>(metadata.at("file_type").value_));
  }
  if (metadata.contains("md5")) {
    metadata_proto->set_md5(std::get<std::string>(metadata.at("md5").value_));
  }
  if (metadata.contains("description")) {
    metadata_proto->set_description(std::get<std::string>(metadata.at("description").value_));
  }
}

void writeDataSetValue(const core::RecordObject& dataset_value, org::eclipse::tahu::protobuf::Payload_DataSet_DataSetValue* dataset_value_proto) {
  if (dataset_value.contains("int_value")) {
    dataset_value_proto->set_int_value(getIntegral<uint32_t>(dataset_value.at("int_value").value_));
  } else if (dataset_value.contains("long_value")) {
    dataset_value_proto->set_long_value(getIntegral<uint64_t>(dataset_value.at("long_value").value_));
  } else if (dataset_value.contains("float_value")) {
    dataset_value_proto->set_float_value(gsl::narrow<float>(std::get<double>(dataset_value.at("float_value").value_)));
  } else if (dataset_value.contains("double_value")) {
    dataset_value_proto->set_double_value(std::get<double>(dataset_value.at("double_value").value_));
  } else if (dataset_value.contains("boolean_value")) {
    dataset_value_proto->set_boolean_value(std::get<bool>(dataset_value.at("boolean_value").value_));
  } else if (dataset_value.contains("string_value")) {
    dataset_value_proto->set_string_value(std::get<std::string>(dataset_value.at("string_value").value_));
  }
}

void writeRow(const core::RecordObject& row, org::eclipse::tahu::protobuf::Payload_DataSet_Row* row_proto) {
  if (row.contains("elements")) {
    const auto& datasetvalue_elements = std::get<core::RecordArray>(row.at("elements").value_);
    for (const auto& dataset_value : datasetvalue_elements) {
      auto* dataset_value_proto = row_proto->add_elements();
      writeDataSetValue(std::get<core::RecordObject>(dataset_value.value_), dataset_value_proto);
    }
  }
}

void writeDataSet(const core::RecordObject& dataset, org::eclipse::tahu::protobuf::Payload_DataSet* dataset_proto) {
  if (dataset.contains("num_of_columns")) {
    dataset_proto->set_num_of_columns(getIntegral<uint64_t>(dataset.at("num_of_columns").value_));
  }
  if (dataset.contains("columns")) {
    const auto& columns = std::get<core::RecordArray>(dataset.at("columns").value_);
    for (const auto& column : columns) {
      dataset_proto->add_columns(std::get<std::string>(column.value_));
    }
  }
  if (dataset.contains("types")) {
    const auto& types = std::get<core::RecordArray>(dataset.at("types").value_);
    for (const auto& type : types) {
      dataset_proto->add_types(getIntegral<uint32_t>(type.value_));
    }
  }
  if (dataset.contains("rows")) {
    const auto& rows = std::get<core::RecordArray>(dataset.at("rows").value_);
    for (const auto& row : rows) {
      auto* row_proto = dataset_proto->add_rows();
      writeRow(std::get<core::RecordObject>(row.value_), row_proto);
    }
  }
}

void writeParameter(const core::RecordObject& parameter, org::eclipse::tahu::protobuf::Payload_Template_Parameter* parameter_proto) {
  if (parameter.contains("name")) {
    parameter_proto->set_name(std::get<std::string>(parameter.at("name").value_));
  }
  if (parameter.contains("type")) {
    parameter_proto->set_type(getIntegral<uint32_t>(parameter.at("type").value_));
  }

  if (parameter.contains("int_value")) {
    parameter_proto->set_int_value(getIntegral<uint32_t>(parameter.at("int_value").value_));
  } else if (parameter.contains("long_value")) {
    parameter_proto->set_long_value(getIntegral<uint64_t>(parameter.at("long_value").value_));
  } else if (parameter.contains("float_value")) {
    parameter_proto->set_float_value(gsl::narrow<float>(std::get<double>(parameter.at("float_value").value_)));
  } else if (parameter.contains("double_value")) {
    parameter_proto->set_double_value(std::get<double>(parameter.at("double_value").value_));
  } else if (parameter.contains("boolean_value")) {
    parameter_proto->set_boolean_value(std::get<bool>(parameter.at("boolean_value").value_));
  } else if (parameter.contains("string_value")) {
    parameter_proto->set_string_value(std::get<std::string>(parameter.at("string_value").value_));
  }
}

void writeTemplate(const core::RecordObject& template_value, org::eclipse::tahu::protobuf::Payload_Template* template_proto) {
  if (template_value.contains("version")) {
    template_proto->set_version(std::get<std::string>(template_value.at("version").value_));
  }
  if (template_value.contains("metrics")) {
    const auto& metrics = std::get<core::RecordArray>(template_value.at("metrics").value_);
    for (const auto& metric : metrics) {
      auto* metric_proto = template_proto->add_metrics();
      writeMetric(std::get<core::RecordObject>(metric.value_), metric_proto);
    }
  }
  if (template_value.contains("parameters")) {
    const auto& parameters = std::get<core::RecordArray>(template_value.at("parameters").value_);
    for (const auto& parameter : parameters) {
      auto* parameter_proto = template_proto->add_parameters();
      writeParameter(std::get<core::RecordObject>(parameter.value_), parameter_proto);
    }
  }
  if (template_value.contains("template_ref")) {
    template_proto->set_template_ref(std::get<std::string>(template_value.at("template_ref").value_));
  }
  if (template_value.contains("is_definition")) {
    template_proto->set_is_definition(std::get<bool>(template_value.at("is_definition").value_));
  }
}

template<typename T>
void writeMetric(const core::RecordObject& metric, T* metric_proto) {
  if (metric.contains("name")) {
    metric_proto->set_name(std::get<std::string>(metric.at("name").value_));
  }
  if (metric.contains("alias")) {
    metric_proto->set_alias(getIntegral<uint64_t>(metric.at("alias").value_));
  }
  if (metric.contains("timestamp")) {
    metric_proto->set_timestamp(getIntegral<uint64_t>(metric.at("timestamp").value_));
  }
  if (metric.contains("datatype")) {
    metric_proto->set_datatype(getIntegral<uint32_t>(metric.at("datatype").value_));
  }
  if (metric.contains("is_historical")) {
    metric_proto->set_is_historical(std::get<bool>(metric.at("is_historical").value_));
  }
  if (metric.contains("is_transient")) {
    metric_proto->set_is_transient(std::get<bool>(metric.at("is_transient").value_));
  }
  if (metric.contains("is_null")) {
    metric_proto->set_is_null(std::get<bool>(metric.at("is_null").value_));
  }
  if (metric.contains("metadata")) {
    const auto& metadata = std::get<core::RecordObject>(metric.at("metadata").value_);
    auto* metadata_proto = metric_proto->mutable_metadata();
    writeMetadataToMetric(metadata, metadata_proto);
  }
  if (metric.contains("properties")) {
    const auto& properties = std::get<core::RecordObject>(metric.at("properties").value_);
    auto* properties_proto = metric_proto->mutable_properties();
    writeProperties(properties, properties_proto);
  }

  if (metric.contains("int_value")) {
    metric_proto->set_int_value(getIntegral<uint32_t>(metric.at("int_value").value_));
  } else if (metric.contains("long_value")) {
    metric_proto->set_long_value(getIntegral<uint64_t>(metric.at("long_value").value_));
  } else if (metric.contains("float_value")) {
    metric_proto->set_float_value(gsl::narrow<float>(std::get<double>(metric.at("float_value").value_)));
  } else if (metric.contains("double_value")) {
    metric_proto->set_double_value(std::get<double>(metric.at("double_value").value_));
  } else if (metric.contains("boolean_value")) {
    metric_proto->set_boolean_value(std::get<bool>(metric.at("boolean_value").value_));
  } else if (metric.contains("string_value")) {
    metric_proto->set_string_value(std::get<std::string>(metric.at("string_value").value_));
  } else if (metric.contains("bytes_value")) {
    metric_proto->set_bytes_value(std::get<std::string>(metric.at("bytes_value").value_));
  } else if (metric.contains("dataset_value")) {
    const auto& dataset = std::get<core::RecordObject>(metric.at("dataset_value").value_);
    auto* dataset_proto = metric_proto->mutable_dataset_value();
    writeDataSet(dataset, dataset_proto);
  } else if (metric.contains("template_value")) {
    const auto& template_value = std::get<core::RecordObject>(metric.at("template_value").value_);
    auto* template_proto = metric_proto->mutable_template_value();
    writeTemplate(template_value, template_proto);
  }
}

void writeRecordToPayload(const core::Record& record, org::eclipse::tahu::protobuf::Payload& payload) {
  if (record.contains("timestamp")) {
    payload.set_timestamp(getIntegral<uint64_t>(record.at("timestamp").value_));
  }
  if (record.contains("seq")) {
    payload.set_seq(getIntegral<uint64_t>(record.at("seq").value_));
  }
  if (record.contains("uuid")) {
    payload.set_uuid(std::get<std::string>(record.at("uuid").value_));
  }
  if (record.contains("body")) {
    payload.set_body(std::get<std::string>(record.at("body").value_));
  }
  if (record.contains("metrics")) {
    const auto& metrics = std::get<core::RecordArray>(record.at("metrics").value_);
    for (const auto& metric : metrics) {
      auto* metric_proto = payload.add_metrics();
      writeMetric(std::get<core::RecordObject>(metric.value_), metric_proto);
    }
  }
}

}  // namespace

void SparkplugBWriter::write(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) {
  if (!flow_file) {
    logger_->log_error("FlowFile is null, cannot write Sparkplug B message");
    return;
  }

  if (record_set.empty()) {
    logger_->log_info("No records to write to Sparkplug B message");
    return;
  }

  bool is_first = true;
  for (const auto& record : record_set) {
    org::eclipse::tahu::protobuf::Payload payload;
    try {
      writeRecordToPayload(record, payload);
    } catch (const std::exception& e) {
      logger_->log_error("Failed to write record to Sparkplug B payload: {}", e.what());
      continue;
    }

    auto size = payload.ByteSizeLong();
    std::vector<uint8_t> buffer(size);

    if (!payload.SerializeToArray(buffer.data(), gsl::narrow<int>(size))) {
      logger_->log_error("Failed to serialize Sparkplug B payload");
      continue;
    }

    auto write_callback = [&buffer](const std::shared_ptr<io::OutputStream>& output_stream) -> int64_t {
      const auto ret = output_stream->write(buffer.data(), buffer.size());
      return io::isError(ret) ? -1 : gsl::narrow<int64_t>(ret);
    };
    if (is_first) {
      session.write(flow_file, write_callback);
      is_first = false;
    } else {
      session.append(flow_file, write_callback);
    }
  }
}

REGISTER_RESOURCE(SparkplugBWriter, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
