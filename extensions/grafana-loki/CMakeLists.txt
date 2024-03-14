#
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
#
if (NOT (ENABLE_ALL OR ENABLE_GRAFANA_LOKI))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

if (ENABLE_GRPC_FOR_LOKI)
    include(Abseil)
    include(Grpc)

    set(LOKI_PROTOBUF_GENERATED_DIR ${CMAKE_BINARY_DIR}/grafana-loki-protobuf-generated)
    file(MAKE_DIRECTORY ${LOKI_PROTOBUF_GENERATED_DIR})

    add_custom_command(
        OUTPUT  ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.grpc.pb.cc ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.grpc.pb.h ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.pb.h ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.pb.cc
        COMMAND ${PROTOBUF_COMPILER} --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN} -I=${CMAKE_CURRENT_SOURCE_DIR}/protos/ -I=${protobuf_SOURCE_DIR}/src --grpc_out=${LOKI_PROTOBUF_GENERATED_DIR} --cpp_out=${LOKI_PROTOBUF_GENERATED_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/protos/grafana-loki-push.proto
        DEPENDS protobuf::protoc grpc_cpp_plugin)

    add_custom_target(grafana-loki-protos ALL DEPENDS ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.grpc.pb.cc ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.grpc.pb.h ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.pb.h ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.pb.cc)

    file(GLOB SOURCES "*.cpp")
    list(APPEND SOURCES
        ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.grpc.pb.cc
        ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.pb.cc
    )

    if (NOT WIN32)
        set_source_files_properties(${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.grpc.pb.cc PROPERTIES COMPILE_FLAGS -Wno-error)
        set_source_files_properties(${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.pb.cc PROPERTIES COMPILE_FLAGS -Wno-error)
    endif()
else()
    set(SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/PushGrafanaLoki.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/PushGrafanaLokiREST.cpp
    )
endif()

add_minifi_library(minifi-grafana-loki SHARED ${SOURCES})
target_include_directories(minifi-grafana-loki PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/http-curl")
target_link_libraries(minifi-grafana-loki ${LIBMINIFI} minifi-http-curl)
add_dependencies(minifi-grafana-loki minifi-http-curl)

if (ENABLE_GRPC_FOR_LOKI)
    target_include_directories(minifi-grafana-loki SYSTEM PRIVATE BEFORE "${LOKI_PROTOBUF_GENERATED_DIR}" "${GRPC_INCLUDE_DIR}" "${PROTOBUF_INCLUDE_DIR}")
    target_link_libraries(minifi-grafana-loki grpc++ protobuf::libprotobuf)
    add_dependencies(minifi-grafana-loki grpc grafana-loki-protos)
endif()

register_extension(minifi-grafana-loki "GRAFANA LOKI EXTENSIONS" GRAFANA-LOKI-EXTENSIONS "This enables Grafana Loki support" "extensions/grafana-loki/tests")

register_extension_linter(minifi-grafana-loki-extensions-linter)
