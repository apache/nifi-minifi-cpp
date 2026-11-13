# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

set(LOKI_PROTOBUF_GENERATED_DIR ${CMAKE_BINARY_DIR}/grafana-loki-protobuf-generated)
file(MAKE_DIRECTORY ${LOKI_PROTOBUF_GENERATED_DIR})

if(MINIFI_GRPC_SOURCE STREQUAL "CONAN")
    find_package(protobuf CONFIG REQUIRED)
    find_package(gRPC CONFIG REQUIRED)

    find_program(LOKI_PROTOC_EXECUTABLE
        NAMES protoc
        PATHS "${protobuf_INCLUDE_DIR}/../bin"
        NO_DEFAULT_PATH
        REQUIRED)

    add_custom_command(
        OUTPUT
            ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.grpc.pb.cc
            ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.grpc.pb.h
            ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.pb.h
            ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.pb.cc
        COMMAND ${LOKI_PROTOC_EXECUTABLE}
        ARGS
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
            --proto_path=.
            --grpc_out=${LOKI_PROTOBUF_GENERATED_DIR}
            --cpp_out=${LOKI_PROTOBUF_GENERATED_DIR}
            grafana-loki-push.proto
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/protos
        DEPENDS
            protobuf::protoc
            gRPC::grpc_cpp_plugin
            ${CMAKE_CURRENT_SOURCE_DIR}/protos/grafana-loki-push.proto
    )
elseif(MINIFI_GRPC_SOURCE STREQUAL "BUILD")
    set(LOKI_PROTOBUF_GENERATED_DIR ${CMAKE_BINARY_DIR}/grafana-loki-protobuf-generated)
    file(MAKE_DIRECTORY ${LOKI_PROTOBUF_GENERATED_DIR})

    add_custom_command(
        OUTPUT
            ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.grpc.pb.cc
            ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.grpc.pb.h
            ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.pb.h
            ${LOKI_PROTOBUF_GENERATED_DIR}/grafana-loki-push.pb.cc
        COMMAND ${PROTOBUF_COMPILER}
        ARGS
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
            --proto_path=.
            --proto_path=${protobuf_SOURCE_DIR}/src
            --grpc_out=${LOKI_PROTOBUF_GENERATED_DIR}
            --cpp_out=${LOKI_PROTOBUF_GENERATED_DIR}
            grafana-loki-push.proto
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/protos
        DEPENDS
            protobuf::protoc
            grpc_cpp_plugin
            ${CMAKE_CURRENT_SOURCE_DIR}/protos/grafana-loki-push.proto
    )
endif()
