/**
 * @file Configure.h
 * Configure class declaration
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
#ifndef __CONFIGURE_H__
#define __CONFIGURE_H__

#include "properties/Properties.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class Configure : public Properties {
 public:
  // nifi.flow.configuration.file
  static const char *nifi_default_directory;
  static const char *nifi_flow_configuration_file;
  static const char *nifi_flow_engine_threads;
  static const char *nifi_administrative_yield_duration;
  static const char *nifi_bored_yield_duration;
  static const char *nifi_graceful_shutdown_seconds;
  static const char *nifi_log_level;
  static const char *nifi_server_name;
  static const char *nifi_configuration_class_name;
  static const char *nifi_flow_repository_class_name;
  static const char *nifi_volatile_repository_options;
  static const char *nifi_provenance_repository_class_name;
  static const char *nifi_server_port;
  static const char *nifi_server_report_interval;
  static const char *nifi_provenance_repository_max_storage_time;
  static const char *nifi_provenance_repository_max_storage_size;
  static const char *nifi_provenance_repository_directory_default;
  static const char *nifi_provenance_repository_enable;
  static const char *nifi_flowfile_repository_max_storage_time;
  static const char *nifi_flowfile_repository_max_storage_size;
  static const char *nifi_flowfile_repository_directory_default;
  static const char *nifi_flowfile_repository_enable;
  static const char *nifi_remote_input_secure;
  static const char *nifi_security_need_ClientAuth;
  static const char *nifi_security_client_certificate;
  static const char *nifi_security_client_private_key;
  static const char *nifi_security_client_pass_phrase;
  static const char *nifi_security_client_ca_certificate;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
