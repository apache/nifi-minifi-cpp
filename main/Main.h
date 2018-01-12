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
#ifndef MAIN_MAIN_H_
#define MAIN_MAIN_H_


//! Main thread sleep interval 1 second
#define SLEEP_INTERVAL 1
//! Main thread stop wait time
#define STOP_WAIT_TIME_MS 30*1000
//! Default YAML location
#define DEFAULT_NIFI_CONFIG_YML "./conf/config.yml"
//! Default properties file paths
#define DEFAULT_NIFI_PROPERTIES_FILE "./conf/minifi.properties"
#define DEFAULT_LOG_PROPERTIES_FILE "./conf/minifi-log.properties"
#define DEFAULT_UID_PROPERTIES_FILE "./conf/minifi-uid.properties"
//! Define home environment variable
#define MINIFI_HOME_ENV_KEY "MINIFI_HOME"

/* Define Parser Values for Configuration YAML sections */
#define CONFIG_YAML_PROCESSORS_KEY "Processors"
#define CONFIG_YAML_FLOW_CONTROLLER_KEY "Flow Controller"
#define CONFIG_YAML_CONNECTIONS_KEY "Connections"
#define CONFIG_YAML_REMOTE_PROCESSING_GROUPS_KEY "Remote Processing Groups"


/**
 * Validates a MINIFI_HOME value.
 * @param home_path
 * @return true if home_path represents a valid MINIFI_HOME
 */
bool validHome(const std::string &home_path) {
  struct stat stat_result { };
  auto properties_file_path = home_path + "/" + DEFAULT_NIFI_PROPERTIES_FILE;
  return (stat(properties_file_path.c_str(), &stat_result) == 0);
}





#endif /* MAIN_MAIN_H_ */
