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
#include "properties/Configuration.h"
#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

const std::vector<core::ConfigurationProperty> Configuration::CONFIGURATION_PROPERTIES{
  core::ConfigurationProperty{Configuration::nifi_version},
  core::ConfigurationProperty{Configuration::nifi_default_directory},
  core::ConfigurationProperty{Configuration::nifi_c2_enable, core::StandardValidators::get().BOOLEAN_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_flow_configuration_file},
  core::ConfigurationProperty{Configuration::nifi_flow_configuration_encrypt, core::StandardValidators::get().BOOLEAN_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_flow_configuration_file_exit_failure, core::StandardValidators::get().BOOLEAN_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_flow_configuration_file_backup_update, core::StandardValidators::get().BOOLEAN_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_flow_engine_threads, core::StandardValidators::get().UNSIGNED_INT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_flow_engine_alert_period, core::StandardValidators::get().UNSIGNED_INT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_flow_engine_event_driven_time_slice, core::StandardValidators::get().UNSIGNED_INT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_administrative_yield_duration, core::StandardValidators::get().TIME_PERIOD_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_bored_yield_duration, core::StandardValidators::get().TIME_PERIOD_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_graceful_shutdown_seconds, core::StandardValidators::get().UNSIGNED_INT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_flowcontroller_drain_timeout, core::StandardValidators::get().TIME_PERIOD_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_log_level},
  core::ConfigurationProperty{Configuration::nifi_server_name},
  core::ConfigurationProperty{Configuration::nifi_configuration_class_name},
  core::ConfigurationProperty{Configuration::nifi_flow_repository_class_name},
  core::ConfigurationProperty{Configuration::nifi_content_repository_class_name},
  core::ConfigurationProperty{Configuration::nifi_provenance_repository_class_name},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_flowfile_max_count, core::StandardValidators::get().UNSIGNED_INT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_flowfile_max_bytes, core::StandardValidators::get().DATA_SIZE_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_provenance_max_count, core::StandardValidators::get().UNSIGNED_INT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_provenance_max_bytes, core::StandardValidators::get().DATA_SIZE_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_content_max_count, core::StandardValidators::get().UNSIGNED_INT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_content_max_bytes, core::StandardValidators::get().DATA_SIZE_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_volatile_repository_options_content_minimal_locking, core::StandardValidators::get().BOOLEAN_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_server_port, core::StandardValidators::get().PORT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_server_report_interval, core::StandardValidators::get().TIME_PERIOD_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_provenance_repository_max_storage_size, core::StandardValidators::get().DATA_SIZE_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_provenance_repository_max_storage_time, core::StandardValidators::get().TIME_PERIOD_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_provenance_repository_directory_default},
  core::ConfigurationProperty{Configuration::nifi_flowfile_repository_max_storage_size, core::StandardValidators::get().DATA_SIZE_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_flowfile_repository_max_storage_time, core::StandardValidators::get().TIME_PERIOD_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_flowfile_repository_directory_default},
  core::ConfigurationProperty{Configuration::nifi_dbcontent_repository_directory_default},
  core::ConfigurationProperty{Configuration::nifi_remote_input_secure, core::StandardValidators::get().BOOLEAN_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_remote_input_http, core::StandardValidators::get().BOOLEAN_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_remote_input_socket_port, core::StandardValidators::get().PORT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_security_need_ClientAuth, core::StandardValidators::get().BOOLEAN_VALIDATOR},
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
  core::ConfigurationProperty{Configuration::nifi_c2_file_watch},
  core::ConfigurationProperty{Configuration::nifi_c2_flow_id},
  core::ConfigurationProperty{Configuration::nifi_c2_flow_url},
  core::ConfigurationProperty{Configuration::nifi_c2_flow_base_url},
  core::ConfigurationProperty{Configuration::nifi_c2_full_heartbeat, core::StandardValidators::get().BOOLEAN_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_heartbeat_period, core::StandardValidators::get().UNSIGNED_INT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_c2_root_classes},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_class},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_heartbeat_reporter_classes},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_listener_port, core::StandardValidators::get().LISTEN_PORT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_listener_cacert},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_coap_host},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_coap_port, core::StandardValidators::get().PORT_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_c2_coap_connector_service},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_protocol_class},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_url},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_url_ack},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_ssl_context_service},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_listener_heartbeat_rooturi},
  core::ConfigurationProperty{Configuration::nifi_c2_rest_heartbeat_minimize_updates, core::StandardValidators::get().BOOLEAN_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_c2_mqtt_connector_service},
  core::ConfigurationProperty{Configuration::nifi_c2_mqtt_heartbeat_topic},
  core::ConfigurationProperty{Configuration::nifi_c2_mqtt_update_topic},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_identifier},
  core::ConfigurationProperty{Configuration::nifi_c2_agent_trigger_classes},
  core::ConfigurationProperty{Configuration::nifi_c2_root_class_definitions},
  core::ConfigurationProperty{Configuration::nifi_state_management_provider_local},
  core::ConfigurationProperty{Configuration::nifi_state_management_provider_local_class_name},
  core::ConfigurationProperty{Configuration::nifi_state_management_provider_local_always_persist, core::StandardValidators::get().BOOLEAN_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_state_management_provider_local_auto_persistence_interval, core::StandardValidators::get().TIME_PERIOD_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_state_management_provider_local_path},
  core::ConfigurationProperty{Configuration::minifi_disk_space_watchdog_enable, core::StandardValidators::get().BOOLEAN_VALIDATOR},
  core::ConfigurationProperty{Configuration::minifi_disk_space_watchdog_interval, core::StandardValidators::get().TIME_PERIOD_VALIDATOR},
  core::ConfigurationProperty{Configuration::minifi_disk_space_watchdog_stop_threshold, core::StandardValidators::get().UNSIGNED_LONG_VALIDATOR},
  core::ConfigurationProperty{Configuration::minifi_disk_space_watchdog_restart_threshold, core::StandardValidators::get().UNSIGNED_LONG_VALIDATOR},
  core::ConfigurationProperty{Configuration::nifi_framework_dir},
  core::ConfigurationProperty{Configuration::nifi_jvm_options},
  core::ConfigurationProperty{Configuration::nifi_nar_directory},
  core::ConfigurationProperty{Configuration::nifi_nar_deploy_directory}
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
