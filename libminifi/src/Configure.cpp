/**
 *
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
#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

const char *Configure::nifi_default_directory = "nifi.default.directory";
const char *Configure::nifi_c2_enable = "nifi.c2.enable";
const char *Configure::nifi_flow_configuration_file = "nifi.flow.configuration.file";
const char *Configure::nifi_flow_configuration_file_exit_failure = "nifi.flow.configuration.file.exit.onfailure";
const char *Configure::nifi_flow_configuration_file_backup_update = "nifi.flow.configuration.backup.on.update";
const char *Configure::nifi_flow_engine_threads = "nifi.flow.engine.threads";
const char *Configure::nifi_administrative_yield_duration = "nifi.administrative.yield.duration";
const char *Configure::nifi_bored_yield_duration = "nifi.bored.yield.duration";
const char *Configure::nifi_graceful_shutdown_seconds = "nifi.flowcontroller.graceful.shutdown.period";
const char *Configure::nifi_log_level = "nifi.log.level";
const char *Configure::nifi_server_name = "nifi.server.name";
const char *Configure::nifi_configuration_class_name = "nifi.flow.configuration.class.name";
const char *Configure::nifi_flow_repository_class_name = "nifi.flowfile.repository.class.name";
const char *Configure::nifi_content_repository_class_name = "nifi.content.repository.class.name";
const char *Configure::nifi_volatile_repository_options = "nifi.volatile.repository.options.";
const char *Configure::nifi_provenance_repository_class_name = "nifi.provenance.repository.class.name";
const char *Configure::nifi_server_port = "nifi.server.port";
const char *Configure::nifi_server_report_interval = "nifi.server.report.interval";
const char *Configure::nifi_provenance_repository_max_storage_size = "nifi.provenance.repository.max.storage.size";
const char *Configure::nifi_provenance_repository_max_storage_time = "nifi.provenance.repository.max.storage.time";
const char *Configure::nifi_provenance_repository_directory_default = "nifi.provenance.repository.directory.default";
const char *Configure::nifi_flowfile_repository_max_storage_size = "nifi.flowfile.repository.max.storage.size";
const char *Configure::nifi_flowfile_repository_max_storage_time = "nifi.flowfile.repository.max.storage.time";
const char *Configure::nifi_flowfile_repository_directory_default = "nifi.flowfile.repository.directory.default";
const char *Configure::nifi_dbcontent_repository_directory_default = "nifi.database.content.repository.directory.default";
const char *Configure::nifi_remote_input_secure = "nifi.remote.input.secure";
const char *Configure::nifi_remote_input_http = "nifi.remote.input.http.enabled";
const char *Configure::nifi_security_need_ClientAuth = "nifi.security.need.ClientAuth";
const char *Configure::nifi_security_client_certificate = "nifi.security.client.certificate";
const char *Configure::nifi_security_client_private_key = "nifi.security.client.private.key";
const char *Configure::nifi_security_client_pass_phrase = "nifi.security.client.pass.phrase";
const char *Configure::nifi_security_client_ca_certificate = "nifi.security.client.ca.certificate";
const char *Configure::nifi_rest_api_user_name = "nifi.rest.api.user.name";
const char *Configure::nifi_rest_api_password = "nifi.rest.api.password";
const char *Configure::nifi_c2_file_watch = "nifi.c2.file.watch";
const char *Configure::nifi_c2_flow_id = "nifi.c2.flow.id";
const char *Configure::nifi_c2_flow_url = "nifi.c2.flow.url";
const char *Configure::nifi_c2_flow_base_url = "nifi.c2.flow.base.url";

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
