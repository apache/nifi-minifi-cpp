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
#include "minifi-cpp/core/PropertyValidator.h"

namespace org::apache::nifi::minifi {

const std::unordered_map<std::string_view, gsl::not_null<const core::PropertyValidator*>> Configuration::CONFIGURATION_PROPERTIES{
  {Configuration::nifi_default_directory, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_flow_configuration_file, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_flow_configuration_encrypt, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_flow_configuration_file_backup_update, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_flow_engine_threads, gsl::make_not_null(&core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)},
  {Configuration::nifi_flow_engine_alert_period, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_flow_engine_event_driven_time_slice, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_administrative_yield_duration, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_bored_yield_duration, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_graceful_shutdown_seconds, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_flowcontroller_drain_timeout, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_configuration_class_name, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_flow_repository_class_name, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_flow_repository_rocksdb_compression, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_content_repository_class_name, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_content_repository_rocksdb_compression, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_provenance_repository_class_name, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_volatile_repository_options_provenance_max_count, gsl::make_not_null(&core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)},
  {Configuration::nifi_volatile_repository_options_provenance_max_bytes, gsl::make_not_null(&core::StandardPropertyValidators::DATA_SIZE_VALIDATOR)},
  {Configuration::nifi_provenance_repository_max_storage_size, gsl::make_not_null(&core::StandardPropertyValidators::DATA_SIZE_VALIDATOR)},
  {Configuration::nifi_provenance_repository_max_storage_time, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_provenance_repository_directory_default, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_flowfile_repository_directory_default, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_dbcontent_repository_directory_default, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_default_internal_buffer_size, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_flowfile_repository_rocksdb_compaction_period, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_dbcontent_repository_rocksdb_compaction_period, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_content_repository_rocksdb_use_synchronous_writes, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_content_repository_rocksdb_read_verify_checksums, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_flowfile_repository_rocksdb_read_verify_checksums, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_provenance_repository_rocksdb_read_verify_checksums, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_rocksdb_state_storage_read_verify_checksums, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_dbcontent_repository_purge_period, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_remote_input_secure, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_security_need_ClientAuth, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_sensitive_props_additional_keys, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_python_processor_dir, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_extension_path, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_security_client_certificate, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_security_client_private_key, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_security_client_pass_phrase, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_security_client_ca_certificate, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_security_use_system_cert_store, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_security_windows_cert_store_location, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_security_windows_server_cert_store, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_security_windows_client_cert_store, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_security_windows_client_cert_cn, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_security_windows_client_cert_key_usage, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_rest_api_user_name, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_rest_api_password, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_enable, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_c2_file_watch, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_flow_id, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_flow_url, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_flow_base_url, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_full_heartbeat, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_c2_agent_heartbeat_period, gsl::make_not_null(&core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)},
  {Configuration::nifi_c2_agent_heartbeat_reporter_classes, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_agent_class, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_agent_identifier, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_agent_identifier_fallback, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_agent_trigger_classes, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_root_classes, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_root_class_definitions, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_rest_listener_port, gsl::make_not_null(&core::StandardPropertyValidators::PORT_VALIDATOR)},
  {Configuration::nifi_c2_rest_listener_cacert, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_rest_path_base, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_rest_url, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_rest_url_ack, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_rest_request_encoding, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_c2_rest_heartbeat_minimize_updates, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_c2_flow_info_processor_bulletin_limit, gsl::make_not_null(&core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)},
  {Configuration::nifi_state_storage_local, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_state_storage_local_old, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_state_storage_local_class_name, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_state_storage_local_class_name_old, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_state_storage_local_always_persist, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_state_storage_local_always_persist_old, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_state_storage_local_auto_persistence_interval, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_state_storage_local_auto_persistence_interval_old, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_state_storage_local_path, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_state_storage_local_path_old, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::minifi_disk_space_watchdog_enable, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::minifi_disk_space_watchdog_interval, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::minifi_disk_space_watchdog_stop_threshold, gsl::make_not_null(&core::StandardPropertyValidators::INTEGER_VALIDATOR)},
  {Configuration::minifi_disk_space_watchdog_restart_threshold, gsl::make_not_null(&core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)},
  {Configuration::nifi_log_spdlog_pattern, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_spdlog_shorten_names, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_log_appender_rolling, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_appender_rolling_directory, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_appender_rolling_file_name, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_appender_rolling_max_files, gsl::make_not_null(&core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)},
  {Configuration::nifi_log_appender_rolling_max_file_size, gsl::make_not_null(&core::StandardPropertyValidators::DATA_SIZE_VALIDATOR)},
  {Configuration::nifi_log_appender_stdout, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_appender_stderr, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_appender_null, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_appender_syslog, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_logger_root, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_compression_cached_log_max_size, gsl::make_not_null(&core::StandardPropertyValidators::DATA_SIZE_VALIDATOR)},
  {Configuration::nifi_log_compression_compressed_log_max_size, gsl::make_not_null(&core::StandardPropertyValidators::DATA_SIZE_VALIDATOR)},
  {Configuration::nifi_log_max_log_entry_length, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_alert_url, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_alert_batch_size, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_alert_flush_period, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_log_alert_filter, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_alert_rate_limit, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_alert_buffer_limit, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_log_alert_level, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_asset_directory, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_metrics_publisher_agent_identifier, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_metrics_publisher_class, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_metrics_publisher_prometheus_metrics_publisher_port, gsl::make_not_null(&core::StandardPropertyValidators::PORT_VALIDATOR)},
  {Configuration::nifi_metrics_publisher_prometheus_metrics_publisher_metrics, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_metrics_publisher_log_metrics_publisher_metrics, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_metrics_publisher_log_metrics_logging_interval, gsl::make_not_null(&core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)},
  {Configuration::nifi_metrics_publisher_log_metrics_log_level, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_metrics_publisher_metrics, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_metrics_publisher_prometheus_metrics_publisher_certificate, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_metrics_publisher_prometheus_metrics_publisher_ca_certificate, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::controller_socket_enable, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::controller_socket_local_any_interface, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::controller_socket_host, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::controller_socket_port, gsl::make_not_null(&core::StandardPropertyValidators::PORT_VALIDATOR)},
  {Configuration::nifi_flow_file_repository_check_health, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_python_virtualenv_directory, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_python_env_setup_binary, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::nifi_python_install_packages_automatically, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::nifi_openssl_fips_support_enable, gsl::make_not_null(&core::StandardPropertyValidators::BOOLEAN_VALIDATOR)},
  {Configuration::minifi_variable_registry_whitelist, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)},
  {Configuration::minifi_variable_registry_blacklist, gsl::make_not_null(&core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR)}
};

const std::array<const char*, 2> Configuration::DEFAULT_SENSITIVE_PROPERTIES = {Configuration::nifi_security_client_pass_phrase,
                                                                                Configuration::nifi_rest_api_password};

std::vector<std::string> Configuration::mergeProperties(std::vector<std::string> properties,
                                                        const std::vector<std::string>& additional_properties) {
  for (const auto& property_name : additional_properties) {
    std::string property_name_trimmed = utils::string::trim(property_name);
    if (!property_name_trimmed.empty()) {
      properties.push_back(std::move(property_name_trimmed));
    }
  }

  std::sort(properties.begin(), properties.end());
  auto new_end = std::unique(properties.begin(), properties.end());
  properties.erase(new_end, properties.end());
  return properties;
}

std::vector<std::string> Configuration::getSensitiveProperties(const std::function<std::optional<std::string>(const std::string&)>& reader) {
  std::vector<std::string> sensitive_properties(Configuration::DEFAULT_SENSITIVE_PROPERTIES.begin(), Configuration::DEFAULT_SENSITIVE_PROPERTIES.end());
  if (reader) {
    const auto additional_sensitive_props_list = reader(Configuration::nifi_sensitive_props_additional_keys);
    if (additional_sensitive_props_list) {
      std::vector<std::string> additional_sensitive_properties = utils::string::split(*additional_sensitive_props_list, ",");
      return Configuration::mergeProperties(sensitive_properties, additional_sensitive_properties);
    }
  }
  return sensitive_properties;
}

bool Configuration::validatePropertyValue(const std::string& property_name, const std::string& property_value) {
  const auto validator = Configuration::CONFIGURATION_PROPERTIES.find(property_name);
  if (validator == std::end(Configuration::CONFIGURATION_PROPERTIES))
    return true;

  return validator->second->validate(property_value);
}

}  // namespace org::apache::nifi::minifi
