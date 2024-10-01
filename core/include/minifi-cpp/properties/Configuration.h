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

#include <vector>
#include <unordered_map>
#include <string>
#include <optional>
#include <functional>

#include "Properties.h"
#include "minifi-cpp/utils/Export.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi {

namespace core {
class PropertyValidator;
}

class Configuration : public virtual Properties {
 public:
  static constexpr const char *nifi_volatile_repository_options = "nifi.volatile.repository.options.";
  static constexpr const char *nifi_global_rocksdb_options = "nifi.global.rocksdb.options.";
  static constexpr const char *nifi_flowfile_repository_rocksdb_options = "nifi.flowfile.repository.rocksdb.options.";
  static constexpr const char *nifi_content_repository_rocksdb_options = "nifi.content.repository.rocksdb.options.";
  static constexpr const char *nifi_provenance_repository_rocksdb_options = "nifi.provenance.repository.rocksdb.options.";
  static constexpr const char *nifi_state_storage_rocksdb_options = "nifi.state.storage.rocksdb.options.";

  // nifi.flow.configuration.file
  static constexpr const char *nifi_default_directory = "nifi.default.directory";
  static constexpr const char *nifi_flow_configuration_file = "nifi.flow.configuration.file";
  static constexpr const char *nifi_flow_configuration_encrypt = "nifi.flow.configuration.encrypt";
  static constexpr const char *nifi_flow_configuration_file_backup_update = "nifi.flow.configuration.backup.on.update";
  static constexpr const char *nifi_flow_engine_threads = "nifi.flow.engine.threads";
  static constexpr const char *nifi_flow_engine_alert_period = "nifi.flow.engine.alert.period";
  static constexpr const char *nifi_flow_engine_event_driven_time_slice = "nifi.flow.engine.event.driven.time.slice";
  static constexpr const char *nifi_administrative_yield_duration = "nifi.administrative.yield.duration";
  static constexpr const char *nifi_bored_yield_duration = "nifi.bored.yield.duration";
  static constexpr const char *nifi_graceful_shutdown_seconds = "nifi.flowcontroller.graceful.shutdown.period";
  static constexpr const char *nifi_flowcontroller_drain_timeout = "nifi.flowcontroller.drain.timeout";
  static constexpr const char *nifi_configuration_class_name = "nifi.flow.configuration.class.name";
  static constexpr const char *nifi_flow_repository_class_name = "nifi.flowfile.repository.class.name";
  static constexpr const char *nifi_flow_repository_rocksdb_compression = "nifi.flowfile.repository.rocksdb.compression";
  static constexpr const char *nifi_content_repository_class_name = "nifi.content.repository.class.name";
  static constexpr const char *nifi_content_repository_rocksdb_compression = "nifi.content.repository.rocksdb.compression";
  static constexpr const char *nifi_provenance_repository_class_name = "nifi.provenance.repository.class.name";
  static constexpr const char *nifi_volatile_repository_options_flowfile_max_count = "nifi.volatile.repository.options.flowfile.max.count";
  static constexpr const char *nifi_volatile_repository_options_flowfile_max_bytes = "nifi.volatile.repository.options.flowfile.max.bytes";
  static constexpr const char *nifi_volatile_repository_options_provenance_max_count = "nifi.volatile.repository.options.provenance.max.count";
  static constexpr const char *nifi_volatile_repository_options_provenance_max_bytes = "nifi.volatile.repository.options.provenance.max.bytes";
  static constexpr const char *nifi_volatile_repository_options_content_max_count = "nifi.volatile.repository.options.content.max.count";
  static constexpr const char *nifi_volatile_repository_options_content_max_bytes = "nifi.volatile.repository.options.content.max.bytes";
  static constexpr const char *nifi_volatile_repository_options_content_minimal_locking = "nifi.volatile.repository.options.content.minimal.locking";
  static constexpr const char *nifi_provenance_repository_max_storage_size = "nifi.provenance.repository.max.storage.size";
  static constexpr const char *nifi_provenance_repository_max_storage_time = "nifi.provenance.repository.max.storage.time";
  static constexpr const char *nifi_provenance_repository_directory_default = "nifi.provenance.repository.directory.default";
  static constexpr const char *nifi_flowfile_repository_directory_default = "nifi.flowfile.repository.directory.default";
  static constexpr const char *nifi_dbcontent_repository_directory_default = "nifi.database.content.repository.directory.default";

  // these are internal properties related to the rocksdb backend
  static constexpr const char *nifi_flowfile_repository_rocksdb_compaction_period = "nifi.flowfile.repository.rocksdb.compaction.period";
  static constexpr const char *nifi_dbcontent_repository_rocksdb_compaction_period = "nifi.database.content.repository.rocksdb.compaction.period";
  static constexpr const char *nifi_dbcontent_repository_purge_period = "nifi.database.content.repository.purge.period";
  static constexpr const char *nifi_content_repository_rocksdb_use_synchronous_writes = "nifi.content.repository.rocksdb.use.synchronous.writes";

  static constexpr const char *nifi_remote_input_secure = "nifi.remote.input.secure";
  static constexpr const char *nifi_security_need_ClientAuth = "nifi.security.need.ClientAuth";
  static constexpr const char *nifi_sensitive_props_additional_keys = "nifi.sensitive.props.additional.keys";
  static constexpr const char *nifi_python_processor_dir = "nifi.python.processor.dir";
  static constexpr const char *nifi_extension_path = "nifi.extension.path";

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
  static constexpr const char *nifi_c2_agent_heartbeat_period = "nifi.c2.agent.heartbeat.period";
  static constexpr const char *nifi_c2_agent_class = "nifi.c2.agent.class";
  static constexpr const char *nifi_c2_agent_heartbeat_reporter_classes = "nifi.c2.agent.heartbeat.reporter.classes";
  static constexpr const char *nifi_c2_agent_identifier = "nifi.c2.agent.identifier";
  static constexpr const char *nifi_c2_agent_identifier_fallback = "nifi.c2.agent.identifier.fallback";
  static constexpr const char *nifi_c2_agent_trigger_classes = "nifi.c2.agent.trigger.classes";
  static constexpr const char *nifi_c2_root_classes = "nifi.c2.root.classes";
  static constexpr const char *nifi_c2_root_class_definitions = "nifi.c2.root.class.definitions";
  static constexpr const char *nifi_c2_rest_listener_port = "nifi.c2.rest.listener.port";
  static constexpr const char *nifi_c2_rest_listener_cacert = "nifi.c2.rest.listener.cacert";
  static constexpr const char *nifi_c2_rest_path_base = "nifi.c2.rest.path.base";
  static constexpr const char *nifi_c2_rest_url = "nifi.c2.rest.url";
  static constexpr const char *nifi_c2_rest_url_ack = "nifi.c2.rest.url.ack";
  static constexpr const char *nifi_c2_rest_ssl_context_service = "nifi.c2.rest.ssl.context.service";
  static constexpr const char *nifi_c2_rest_heartbeat_minimize_updates = "nifi.c2.rest.heartbeat.minimize.updates";
  static constexpr const char *nifi_c2_rest_request_encoding = "nifi.c2.rest.request.encoding";

  // state management options
  static constexpr const char *nifi_state_storage_local = "nifi.state.storage.local";
  static constexpr const char *nifi_state_storage_local_old = "nifi.state.management.provider.local";
  static constexpr const char *nifi_state_storage_local_class_name = "nifi.state.storage.local.class.name";
  static constexpr const char *nifi_state_storage_local_class_name_old = "nifi.state.management.provider.local.class.name";
  static constexpr const char *nifi_state_storage_local_always_persist = "nifi.state.storage.local.always.persist";
  static constexpr const char *nifi_state_storage_local_always_persist_old = "nifi.state.management.provider.local.always.persist";
  static constexpr const char *nifi_state_storage_local_auto_persistence_interval = "nifi.state.storage.local.auto.persistence.interval";
  static constexpr const char *nifi_state_storage_local_auto_persistence_interval_old = "nifi.state.management.provider.local.auto.persistence.interval";
  static constexpr const char *nifi_state_storage_local_path = "nifi.state.storage.local.path";
  static constexpr const char *nifi_state_storage_local_path_old = "nifi.state.management.provider.local.path";

  // disk space watchdog options
  static constexpr const char *minifi_disk_space_watchdog_enable = "minifi.disk.space.watchdog.enable";
  static constexpr const char *minifi_disk_space_watchdog_interval = "minifi.disk.space.watchdog.interval";
  static constexpr const char *minifi_disk_space_watchdog_stop_threshold = "minifi.disk.space.watchdog.stop.threshold";
  static constexpr const char *minifi_disk_space_watchdog_restart_threshold = "minifi.disk.space.watchdog.restart.threshold";

  // Log options
  static constexpr const char *nifi_log_spdlog_pattern = "nifi.log.spdlog.pattern";
  static constexpr const char *nifi_log_spdlog_shorten_names = "nifi.log.spdlog.shorten_names";
  static constexpr const char *nifi_log_appender_rolling = "nifi.log.appender.rolling";
  static constexpr const char *nifi_log_appender_rolling_directory = "nifi.log.appender.rolling.directory";
  static constexpr const char *nifi_log_appender_rolling_file_name = "nifi.log.appender.rolling.file_name";
  static constexpr const char *nifi_log_appender_rolling_max_files = "nifi.log.appender.rolling.max_files";
  static constexpr const char *nifi_log_appender_rolling_max_file_size = "nifi.log.appender.rolling.max_file_size";
  static constexpr const char *nifi_log_appender_stdout = "nifi.log.appender.stdout";
  static constexpr const char *nifi_log_appender_stderr = "nifi.log.appender.stderr";
  static constexpr const char *nifi_log_appender_null = "nifi.log.appender.null";
  static constexpr const char *nifi_log_appender_syslog = "nifi.log.appender.syslog";
  static constexpr const char *nifi_log_logger_root = "nifi.log.logger.root";
  static constexpr const char *nifi_log_compression_cached_log_max_size = "nifi.log.compression.cached.log.max.size";
  static constexpr const char *nifi_log_compression_compressed_log_max_size = "nifi.log.compression.compressed.log.max.size";
  static constexpr const char *nifi_log_max_log_entry_length = "nifi.log.max.log.entry.length";

  // alert options
  static constexpr const char *nifi_log_alert_url = "nifi.log.alert.url";
  static constexpr const char *nifi_log_alert_ssl_context_service = "nifi.log.alert.ssl.context.service";
  static constexpr const char *nifi_log_alert_batch_size = "nifi.log.alert.batch.size";
  static constexpr const char *nifi_log_alert_flush_period = "nifi.log.alert.flush.period";
  static constexpr const char *nifi_log_alert_filter = "nifi.log.alert.filter";
  static constexpr const char *nifi_log_alert_rate_limit = "nifi.log.alert.rate.limit";
  static constexpr const char *nifi_log_alert_buffer_limit = "nifi.log.alert.buffer.limit";
  static constexpr const char *nifi_log_alert_level = "nifi.log.alert.level";

  static constexpr const char *nifi_asset_directory = "nifi.asset.directory";

  // Metrics publisher options
  static constexpr const char *nifi_metrics_publisher_agent_identifier = "nifi.metrics.publisher.agent.identifier";
  static constexpr const char *nifi_metrics_publisher_class = "nifi.metrics.publisher.class";
  static constexpr const char *nifi_metrics_publisher_prometheus_metrics_publisher_port = "nifi.metrics.publisher.PrometheusMetricsPublisher.port";
  static constexpr const char *nifi_metrics_publisher_prometheus_metrics_publisher_metrics = "nifi.metrics.publisher.PrometheusMetricsPublisher.metrics";
  static constexpr const char *nifi_metrics_publisher_log_metrics_publisher_metrics = "nifi.metrics.publisher.LogMetricsPublisher.metrics";
  static constexpr const char *nifi_metrics_publisher_log_metrics_logging_interval = "nifi.metrics.publisher.LogMetricsPublisher.logging.interval";
  static constexpr const char *nifi_metrics_publisher_log_metrics_log_level = "nifi.metrics.publisher.LogMetricsPublisher.log.level";
  static constexpr const char *nifi_metrics_publisher_metrics = "nifi.metrics.publisher.metrics";
  static constexpr const char *nifi_metrics_publisher_prometheus_metrics_publisher_certificate = "nifi.metrics.publisher.PrometheusMetricsPublisher.certificate";
  static constexpr const char *nifi_metrics_publisher_prometheus_metrics_publisher_ca_certificate = "nifi.metrics.publisher.PrometheusMetricsPublisher.ca.certificate";

  // Controller socket options
  static constexpr const char *controller_socket_enable = "controller.socket.enable";
  static constexpr const char *controller_socket_local_any_interface = "controller.socket.local.any.interface";
  static constexpr const char *controller_socket_host = "controller.socket.host";
  static constexpr const char *controller_socket_port = "controller.socket.port";
  static constexpr const char *controller_ssl_context_service = "controller.ssl.context.service";

  static constexpr const char *nifi_flow_file_repository_check_health = "nifi.flowfile.repository.check.health";
  static constexpr const char *nifi_python_virtualenv_directory = "nifi.python.virtualenv.directory";
  static constexpr const char *nifi_python_env_setup_binary = "nifi.python.env.setup.binary";
  static constexpr const char *nifi_python_install_packages_automatically = "nifi.python.install.packages.automatically";

  MINIFIAPI static const std::unordered_map<std::string_view, gsl::not_null<const core::PropertyValidator*>> CONFIGURATION_PROPERTIES;
  MINIFIAPI static const std::array<const char*, 2> DEFAULT_SENSITIVE_PROPERTIES;

  static std::vector<std::string> mergeProperties(std::vector<std::string> properties,
                                                  const std::vector<std::string>& additional_properties);
  static std::vector<std::string> getSensitiveProperties(const std::function<std::optional<std::string>(const std::string&)>& reader);
  static bool validatePropertyValue(const std::string& property_name, const std::string& property_value);
};

}  // namespace org::apache::nifi::minifi
