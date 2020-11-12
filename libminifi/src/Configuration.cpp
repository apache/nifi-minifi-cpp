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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

constexpr const char *Configuration::nifi_default_directory;
constexpr const char *Configuration::nifi_c2_enable;
constexpr const char *Configuration::nifi_flow_configuration_file;
constexpr const char *Configuration::nifi_flow_configuration_file_exit_failure;
constexpr const char *Configuration::nifi_flow_configuration_file_backup_update;
constexpr const char *Configuration::nifi_flow_engine_threads;
constexpr const char *Configuration::nifi_flow_engine_alert_period;
constexpr const char *Configuration::nifi_flow_engine_event_driven_time_slice;
constexpr const char *Configuration::nifi_administrative_yield_duration;
constexpr const char *Configuration::nifi_bored_yield_duration;
constexpr const char *Configuration::nifi_graceful_shutdown_seconds;
constexpr const char *Configuration::nifi_flowcontroller_drain_timeout;
constexpr const char *Configuration::nifi_log_level;
constexpr const char *Configuration::nifi_server_name;
constexpr const char *Configuration::nifi_configuration_class_name;
constexpr const char *Configuration::nifi_flow_repository_class_name;
constexpr const char *Configuration::nifi_content_repository_class_name;
constexpr const char *Configuration::nifi_volatile_repository_options;
constexpr const char *Configuration::nifi_provenance_repository_class_name;
constexpr const char *Configuration::nifi_server_port;
constexpr const char *Configuration::nifi_server_report_interval;
constexpr const char *Configuration::nifi_provenance_repository_max_storage_size;
constexpr const char *Configuration::nifi_provenance_repository_max_storage_time;
constexpr const char *Configuration::nifi_provenance_repository_directory_default;
constexpr const char *Configuration::nifi_flowfile_repository_max_storage_size;
constexpr const char *Configuration::nifi_flowfile_repository_max_storage_time;
constexpr const char *Configuration::nifi_flowfile_repository_directory_default;
constexpr const char *Configuration::nifi_dbcontent_repository_directory_default;
constexpr const char *Configuration::nifi_remote_input_secure;
constexpr const char *Configuration::nifi_remote_input_http;
constexpr const char *Configuration::nifi_security_need_ClientAuth;
constexpr const char *Configuration::nifi_security_client_certificate;
constexpr const char *Configuration::nifi_security_client_private_key;
constexpr const char *Configuration::nifi_security_client_pass_phrase;
constexpr const char *Configuration::nifi_security_client_ca_certificate;
constexpr const char *Configuration::nifi_security_use_system_cert_store;
constexpr const char *Configuration::nifi_security_windows_cert_store_location;
constexpr const char *Configuration::nifi_security_windows_server_cert_store;
constexpr const char *Configuration::nifi_security_windows_client_cert_store;
constexpr const char *Configuration::nifi_security_windows_client_cert_cn;
constexpr const char *Configuration::nifi_security_windows_client_cert_key_usage;
constexpr const char *Configuration::nifi_rest_api_user_name;
constexpr const char *Configuration::nifi_rest_api_password;
constexpr const char *Configuration::nifi_c2_file_watch;
constexpr const char *Configuration::nifi_c2_flow_id;
constexpr const char *Configuration::nifi_c2_flow_url;
constexpr const char *Configuration::nifi_c2_flow_base_url;
constexpr const char *Configuration::nifi_c2_full_heartbeat;
constexpr const char *Configuration::nifi_state_management_provider_local;
constexpr const char *Configuration::nifi_state_management_provider_local_always_persist;
constexpr const char *Configuration::nifi_state_management_provider_local_auto_persistence_interval;
constexpr const char *Configuration::minifi_disk_space_watchdog_enable;
constexpr const char *Configuration::minifi_disk_space_watchdog_interval;
constexpr const char *Configuration::minifi_disk_space_watchdog_stop_threshold;
constexpr const char *Configuration::minifi_disk_space_watchdog_restart_threshold;

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
