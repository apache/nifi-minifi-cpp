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
#include <algorithm>

#include "properties/Configuration.h"
#include "core/PropertyBuilder.h"

namespace org::apache::nifi::minifi {

const std::vector<core::ConfigurationProperty> Configuration::CONFIGURATION_PROPERTIES{
  core::ConfigurationProperty{Configuration::nifi_default_directory},
  core::ConfigurationProperty{Configuration::nifi_flow_configuration_file},
  core::ConfigurationProperty{Configuration::nifi_flow_configuration_encrypt, gsl::make_not_null(core::StandardValidators::get().BOOLEAN_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_flow_configuration_file_backup_update, gsl::make_not_null(core::StandardValidators::get().BOOLEAN_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_flow_engine_threads, gsl::make_not_null(core::StandardValidators::get().UNSIGNED_INT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_flow_engine_alert_period, gsl::make_not_null(core::StandardValidators::get().UNSIGNED_INT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_flow_engine_event_driven_time_slice, gsl::make_not_null(core::StandardValidators::get().UNSIGNED_INT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_administrative_yield_duration, gsl::make_not_null(core::StandardValidators::get().TIME_PERIOD_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_bored_yield_duration, gsl::make_not_null(core::StandardValidators::get().TIME_PERIOD_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_graceful_shutdown_seconds, gsl::make_not_null(core::StandardValidators::get().UNSIGNED_INT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_flowcontroller_drain_timeout, gsl::make_not_null(core::StandardValidators::get().TIME_PERIOD_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_server_name},
  core::ConfigurationProperty{Configuration::nifi_configuration_class_name},
  core::ConfigurationProperty{Configuration::nifi_flow_repository_class_name},
  core::ConfigurationProperty{Configuration::nifi_content_repository_class_name},
  core::ConfigurationProperty{Configuration::nifi_provenance_repository_class_name},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_flowfile_max_count, gsl::make_not_null(core::StandardValidators::get().UNSIGNED_INT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_flowfile_max_bytes, gsl::make_not_null(core::StandardValidators::get().DATA_SIZE_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_provenance_max_count, gsl::make_not_null(core::StandardValidators::get().UNSIGNED_INT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_provenance_max_bytes, gsl::make_not_null(core::StandardValidators::get().DATA_SIZE_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_content_max_count, gsl::make_not_null(core::StandardValidators::get().UNSIGNED_INT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_content_max_bytes, gsl::make_not_null(core::StandardValidators::get().DATA_SIZE_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_content_minimal_locking, gsl::make_not_null(core::StandardValidators::get().BOOLEAN_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_server_port, gsl::make_not_null(core::StandardValidators::get().PORT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_server_report_interval, gsl::make_not_null(core::StandardValidators::get().TIME_PERIOD_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_provenance_repository_max_storage_size, gsl::make_not_null(core::StandardValidators::get().DATA_SIZE_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_provenance_repository_max_storage_time, gsl::make_not_null(core::StandardValidators::get().TIME_PERIOD_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_provenance_repository_directory_default},
  core::ConfigurationProperty{Configuration::nifi_flowfile_repository_directory_default},
  core::ConfigurationProperty{Configuration::nifi_dbcontent_repository_directory_default},
  core::ConfigurationProperty{Configuration::nifi_remote_input_secure, gsl::make_not_null(core::StandardValidators::get().BOOLEAN_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_security_need_ClientAuth, gsl::make_not_null(core::StandardValidators::get().BOOLEAN_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_sensitive_props_additional_keys},
  core::ConfigurationProperty{Configuration::nifi_python_processor_dir},
  core::ConfigurationProperty{Configuration::nifi_extension_path},
  core::ConfigurationProperty{Configuration::nifi_security_client_certificate},
  core::ConfigurationProperty{Configuration::nifi_security_client_private_key},
  core::ConfigurationProperty{Configuration::nifi_security_client_pass_phrase},
  core::ConfigurationProperty{Configuration::nifi_security_client_ca_certificate},
  core::ConfigurationProperty{Configuration::nifi_security_use_system_cert_store},
  core::ConfigurationProperty{Configuration::nifi_security_windows_cert_store_location},
  core::ConfigurationProperty{Configuration::nifi_security_windows_server_cert_store},
  core::ConfigurationProperty{Configuration::nifi_security_windows_client_cert_store},
  core::ConfigurationProperty{Configuration::nifi_security_windows_client_cert_cn},
  core::ConfigurationProperty{Configuration::nifi_security_windows_client_cert_key_usage},
  core::ConfigurationProperty{Configuration::nifi_rest_api_user_name},
  core::ConfigurationProperty{Configuration::nifi_rest_api_password},
  core::ConfigurationProperty{Configuration::nifi_c2_enable, gsl::make_not_null(core::StandardValidators::get().BOOLEAN_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_c2_file_watch},
  core::ConfigurationProperty{Configuration::nifi_c2_flow_id},
  core::ConfigurationProperty{Configuration::nifi_c2_flow_url},
  core::ConfigurationProperty{Configuration::nifi_c2_flow_base_url},
  core::ConfigurationProperty{Configuration::nifi_c2_full_heartbeat, gsl::make_not_null(core::StandardValidators::get().BOOLEAN_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_c2_coap_connector_service},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_heartbeat_period, gsl::make_not_null(core::StandardValidators::get().UNSIGNED_INT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_heartbeat_reporter_classes},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_class},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_coap_host},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_coap_port, gsl::make_not_null(core::StandardValidators::get().PORT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_protocol_class},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_identifier},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_identifier_fallback},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_trigger_classes},
  core::ConfigurationProperty{Configuration::nifi_c2_root_classes},
  core::ConfigurationProperty{Configuration::nifi_c2_root_class_definitions},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_listener_port, gsl::make_not_null(core::StandardValidators::get().LISTEN_PORT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_listener_cacert},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_url},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_url_ack},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_ssl_context_service},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_request_encoding},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_heartbeat_minimize_updates, gsl::make_not_null(core::StandardValidators::get().BOOLEAN_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_c2_mqtt_connector_service},
  core::ConfigurationProperty{Configuration::nifi_c2_mqtt_heartbeat_topic},
  core::ConfigurationProperty{Configuration::nifi_c2_mqtt_update_topic},
  core::ConfigurationProperty{Configuration::nifi_state_management_provider_local},
  core::ConfigurationProperty{Configuration::nifi_state_management_provider_local_class_name},
  core::ConfigurationProperty{Configuration::nifi_state_management_provider_local_always_persist, gsl::make_not_null(core::StandardValidators::get().BOOLEAN_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_state_management_provider_local_auto_persistence_interval, gsl::make_not_null(core::StandardValidators::get().TIME_PERIOD_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_state_management_provider_local_path},
  core::ConfigurationProperty{Configuration::minifi_disk_space_watchdog_enable, gsl::make_not_null(core::StandardValidators::get().BOOLEAN_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::minifi_disk_space_watchdog_interval, gsl::make_not_null(core::StandardValidators::get().TIME_PERIOD_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::minifi_disk_space_watchdog_stop_threshold, gsl::make_not_null(core::StandardValidators::get().UNSIGNED_LONG_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::minifi_disk_space_watchdog_restart_threshold, gsl::make_not_null(core::StandardValidators::get().UNSIGNED_LONG_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_framework_dir},
  core::ConfigurationProperty{Configuration::nifi_jvm_options},
  core::ConfigurationProperty{Configuration::nifi_nar_directory},
  core::ConfigurationProperty{Configuration::nifi_nar_deploy_directory},
  core::ConfigurationProperty{Configuration::nifi_log_spdlog_pattern},
  core::ConfigurationProperty{Configuration::nifi_log_spdlog_shorten_names, gsl::make_not_null(core::StandardValidators::get().BOOLEAN_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_log_appender_rolling},
  core::ConfigurationProperty{Configuration::nifi_log_appender_rolling_directory},
  core::ConfigurationProperty{Configuration::nifi_log_appender_rolling_file_name},
  core::ConfigurationProperty{Configuration::nifi_log_appender_rolling_max_files, gsl::make_not_null(core::StandardValidators::get().UNSIGNED_INT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_log_appender_rolling_max_file_size, gsl::make_not_null(core::StandardValidators::get().DATA_SIZE_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_log_appender_stdout},
  core::ConfigurationProperty{Configuration::nifi_log_appender_stderr},
  core::ConfigurationProperty{Configuration::nifi_log_appender_null},
  core::ConfigurationProperty{Configuration::nifi_log_appender_syslog},
  core::ConfigurationProperty{Configuration::nifi_log_logger_root},
  core::ConfigurationProperty{Configuration::nifi_log_compression_cached_log_max_size, gsl::make_not_null(core::StandardValidators::get().DATA_SIZE_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_log_compression_compressed_log_max_size, gsl::make_not_null(core::StandardValidators::get().DATA_SIZE_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_asset_directory},
  core::ConfigurationProperty{Configuration::nifi_metrics_publisher_class},
  core::ConfigurationProperty{Configuration::nifi_metrics_publisher_port, gsl::make_not_null(core::StandardValidators::get().PORT_VALIDATOR.get())},
  core::ConfigurationProperty{Configuration::nifi_metrics_publisher_metrics}
};

const std::array<const char*, 2> Configuration::DEFAULT_SENSITIVE_PROPERTIES = {Configuration::nifi_security_client_pass_phrase,
                                                                                Configuration::nifi_rest_api_password};

std::vector<std::string> Configuration::mergeProperties(std::vector<std::string> properties,
                                                        const std::vector<std::string>& additional_properties) {
  for (const auto& property_name : additional_properties) {
    std::string property_name_trimmed = utils::StringUtils::trim(property_name);
    if (!property_name_trimmed.empty()) {
      properties.push_back(std::move(property_name_trimmed));
    }
  }

  std::sort(properties.begin(), properties.end());
  auto new_end = std::unique(properties.begin(), properties.end());
  properties.erase(new_end, properties.end());
  return properties;
}

std::vector<std::string> Configuration::getSensitiveProperties(std::function<std::optional<std::string>(const std::string&)> reader) {
  std::vector<std::string> sensitive_properties(Configuration::DEFAULT_SENSITIVE_PROPERTIES.begin(), Configuration::DEFAULT_SENSITIVE_PROPERTIES.end());
  if (reader) {
    const auto additional_sensitive_props_list = reader(Configuration::nifi_sensitive_props_additional_keys);
    if (additional_sensitive_props_list) {
      std::vector<std::string> additional_sensitive_properties = utils::StringUtils::split(*additional_sensitive_props_list, ",");
      return Configuration::mergeProperties(sensitive_properties, additional_sensitive_properties);
    }
  }
  return sensitive_properties;
}

bool Configuration::validatePropertyValue(const std::string& property_name, const std::string& property_value) {
  for (const auto& config_property: Configuration::CONFIGURATION_PROPERTIES) {
    if (config_property.name == property_name) {
      return config_property.validator->validate(property_name, property_value).valid();
    }
  }
  return true;
}

}  // namespace org::apache::nifi::minifi
