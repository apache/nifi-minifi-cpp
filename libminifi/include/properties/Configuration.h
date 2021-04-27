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

#pragma once

#include <string>
#include <mutex>
#include "properties/Properties.h"
#include "utils/OptionalUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

// TODO(adebreceni): eliminate this class in a separate PR
class Configuration : public Properties {
 public:
  Configuration() : Properties("MiNiFi configuration") {}

  // nifi.flow.configuration.file
  static constexpr const char *nifi_default_directory = "nifi.default.directory";
  static constexpr const char *nifi_flow_configuration_file = "nifi.flow.configuration.file";
  static constexpr const char *nifi_flow_configuration_encrypt = "nifi.flow.configuration.encrypt";
  static constexpr const char *nifi_flow_configuration_file_exit_failure = "nifi.flow.configuration.file.exit.onfailure";
  static constexpr const char *nifi_flow_configuration_file_backup_update = "nifi.flow.configuration.backup.on.update";
  static constexpr const char *nifi_flow_engine_threads = "nifi.flow.engine.threads";
  static constexpr const char *nifi_flow_engine_alert_period = "nifi.flow.engine.alert.period";
  static constexpr const char *nifi_flow_engine_event_driven_time_slice = "nifi.flow.engine.event.driven.time.slice";
  static constexpr const char *nifi_administrative_yield_duration = "nifi.administrative.yield.duration";
  static constexpr const char *nifi_bored_yield_duration = "nifi.bored.yield.duration";
  static constexpr const char *nifi_graceful_shutdown_seconds = "nifi.flowcontroller.graceful.shutdown.period";
  static constexpr const char *nifi_flowcontroller_drain_timeout = "nifi.flowcontroller.drain.timeout";
  static constexpr const char *nifi_log_level = "nifi.log.level";
  static constexpr const char *nifi_server_name = "nifi.server.name";
  static constexpr const char *nifi_configuration_class_name = "nifi.flow.configuration.class.name";
  static constexpr const char *nifi_flow_repository_class_name = "nifi.flowfile.repository.class.name";
  static constexpr const char *nifi_content_repository_class_name = "nifi.content.repository.class.name";
  static constexpr const char *nifi_volatile_repository_options = "nifi.volatile.repository.options.";
  static constexpr const char *nifi_provenance_repository_class_name = "nifi.provenance.repository.class.name";
  static constexpr const char *nifi_server_port = "nifi.server.port";
  static constexpr const char *nifi_server_report_interval = "nifi.server.report.interval";
  static constexpr const char *nifi_provenance_repository_max_storage_size = "nifi.provenance.repository.max.storage.size";
  static constexpr const char *nifi_provenance_repository_max_storage_time = "nifi.provenance.repository.max.storage.time";
  static constexpr const char *nifi_provenance_repository_directory_default = "nifi.provenance.repository.directory.default";
  static constexpr const char *nifi_flowfile_repository_max_storage_size = "nifi.flowfile.repository.max.storage.size";
  static constexpr const char *nifi_flowfile_repository_max_storage_time = "nifi.flowfile.repository.max.storage.time";
  static constexpr const char *nifi_flowfile_repository_directory_default = "nifi.flowfile.repository.directory.default";
  static constexpr const char *nifi_dbcontent_repository_directory_default = "nifi.database.content.repository.directory.default";
  static constexpr const char *nifi_remote_input_secure = "nifi.remote.input.secure";
  static constexpr const char *nifi_remote_input_http = "nifi.remote.input.http.enabled";
  static constexpr const char *nifi_security_need_ClientAuth = "nifi.security.need.ClientAuth";
  // site2site security config
  static constexpr const char *nifi_security_client_certificate = "nifi.security.client.certificate";
  static constexpr const char *nifi_security_client_private_key = "nifi.security.client.private.key";
  static constexpr const char *nifi_security_client_pass_phrase = "nifi.security.client.pass.phrase";
  static constexpr const char *nifi_security_client_ca_certificate = "nifi.security.client.ca.certificate";
  static constexpr const char *nifi_security_use_system_cert_store = "nifi.security.use.system.cert.store";
  static constexpr const char *nifi_security_windows_cert_store_location = "nifi.security.windows.cert.store.location";
  static constexpr const char *nifi_security_windows_server_cert_store = "nifi.security.windows.server.cert.store";
  static constexpr const char *nifi_security_windows_client_cert_store = "nifi.security.windows.client.cert.store";
  static constexpr const char *nifi_security_windows_client_cert_cn = "nifi.security.windows.client.cert.cn";
  static constexpr const char *nifi_security_windows_client_cert_key_usage = "nifi.security.windows.client.cert.key.usage";

  // nifi rest api user name and password
  static constexpr const char *nifi_rest_api_user_name = "nifi.rest.api.user.name";
  static constexpr const char *nifi_rest_api_password = "nifi.rest.api.password";
  // c2 options
  static constexpr const char *nifi_c2_enable = "nifi.c2.enable";
  static constexpr const char *nifi_c2_file_watch = "nifi.c2.file.watch";
  static constexpr const char *nifi_c2_flow_id = "nifi.c2.flow.id";
  static constexpr const char *nifi_c2_flow_url = "nifi.c2.flow.url";
  static constexpr const char *nifi_c2_flow_base_url = "nifi.c2.flow.base.url";
  static constexpr const char *nifi_c2_full_heartbeat = "nifi.c2.full.heartbeat";

  // state management options
  static constexpr const char *nifi_state_management_provider_local = "nifi.state.management.provider.local";
  static constexpr const char *nifi_state_management_provider_local_class_name = "nifi.state.management.provider.local.class.name";
  static constexpr const char *nifi_state_management_provider_local_always_persist = "nifi.state.management.provider.local.always.persist";
  static constexpr const char *nifi_state_management_provider_local_auto_persistence_interval = "nifi.state.management.provider.local.auto.persistence.interval";
  static constexpr const char *nifi_state_management_provider_local_path = "nifi.state.management.provider.local.path";

  // disk space watchdog options
  static constexpr const char *minifi_disk_space_watchdog_enable = "minifi.disk.space.watchdog.enable";
  static constexpr const char *minifi_disk_space_watchdog_interval = "minifi.disk.space.watchdog.interval";
  static constexpr const char *minifi_disk_space_watchdog_stop_threshold = "minifi.disk.space.watchdog.stop.threshold";
  static constexpr const char *minifi_disk_space_watchdog_restart_threshold = "minifi.disk.space.watchdog.restart.threshold";
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
