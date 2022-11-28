# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/attributes_to_json.feature")
set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/core_functionality.feature")
set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/defragtextflowfiles.feature")
set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/file_system_operations.feature")
set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/hashcontent.feature")
set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/replace_text.feature")
set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/routetext.feature")
set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/s2s.feature")
set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/syslog_listener.feature")
set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/network_listener.feature")
set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/minifi_c2_server.feature")

if (ENABLE_AZURE)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/azure_storage.feature")
endif()

if (ENABLE_GCP)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/google_cloud_storage.feature")
endif()

if (NOT DISABLE_CIVET)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/http.feature")
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/https.feature")
endif()

if (ENABLE_LIBRDKAFKA)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/kafka.feature")
endif()

if (ENABLE_KUBERNETES)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/kubernetes_logging.feature")
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/kubernetes_metrics.feature")
endif()

if (ENABLE_MQTT)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/mqtt.feature")
endif()

if (ENABLE_OPC)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/opcua.feature")
endif()

if (ENABLE_SCRIPTING)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/python.feature")
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/script.feature")
endif()

if (ENABLE_AWS)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/s3.feature")
endif()

if (ENABLE_SPLUNK)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/splunk.feature")
endif()

if (ENABLE_SQL)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/sql.feature")
endif()

if (ENABLE_ELASTICSEARCH AND NOT CI_BUILD) # Elasticsearch/Opensearch containers require more RAM than what the CI environment has
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/elasticsearch.feature")
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/opensearch.feature")
endif()

if (ENABLE_PROMETHEUS)
    set(ENABLED_BEHAVE_TESTS "${ENABLED_BEHAVE_TESTS};${CMAKE_CURRENT_SOURCE_DIR}/prometheus.feature")
endif()

file(GLOB FEATURE_FILES  "*.feature")

set(ENABLED_BEHAVE_TESTS ${ENABLED_BEHAVE_TESTS} PARENT_SCOPE)
set(ALL_BEHAVE_TESTS ${FEATURE_FILES} PARENT_SCOPE)
