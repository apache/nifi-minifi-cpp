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

#include <memory>
#include <string>
#include <vector>
#include "grpcpp/grpcpp.h"
#include "grafana-loki-push.grpc.pb.h"
#include "google/protobuf/util/time_util.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki::test {

struct GrafanaLokiLineEntry {
  uint64_t timestamp = 0;
  std::string line;
  std::map<std::string, std::string> labels;
};

struct GrafanaLokiGrpcRequest {
  void reset() {
    stream_labels.clear();
    entries.clear();
  }

  std::string stream_labels;
  std::vector<GrafanaLokiLineEntry> entries;
};

class GrafanaLokiGrpcService final : public ::logproto::Pusher::Service {
 public:
  ::grpc::Status Push(::grpc::ServerContext* ctx, const ::logproto::PushRequest* request, ::logproto::PushResponse*) override {
    tenant_id_.clear();
    auto metadata = ctx->client_metadata();
    auto org_id = metadata.find("x-scope-orgid");
    if (org_id != metadata.end()) {
      tenant_id_ = std::string(org_id->second.data(), org_id->second.size());
    }
    last_request_received_.reset();
    auto& stream = request->streams(0);
    last_request_received_.stream_labels = stream.labels();
    for (int i = 0; i < stream.entries_size(); ++i) {
      auto& request_entry = stream.entries(i);
      GrafanaLokiLineEntry entry;
      entry.timestamp = google::protobuf::util::TimeUtil::TimestampToNanoseconds(request_entry.timestamp());;
      entry.line = request_entry.line();
      for (int j = 0; j < request_entry.nonindexedlabels_size(); ++j) {
        auto& label = request_entry.nonindexedlabels(j);
        entry.labels[label.name()] = label.value();
      }
      last_request_received_.entries.push_back(entry);
    }
    return ::grpc::Status::OK;
  }

  GrafanaLokiGrpcRequest getLastRequest() const {
    return last_request_received_;
  }

  std::string getLastTenantId() const {
    return tenant_id_;
  }

 private:
  GrafanaLokiGrpcRequest last_request_received_;
  std::string tenant_id_;
};

class MockGrafanaLokiGrpc {
 public:
  explicit MockGrafanaLokiGrpc(std::string port) {
    std::string server_address("0.0.0.0:" + port);
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, ::grpc::InsecureServerCredentials());
    builder.RegisterService(&loki_grpc_service_);

    grpc_server_ = builder.BuildAndStart();
  }

  GrafanaLokiGrpcRequest getLastRequest() const {
    return loki_grpc_service_.getLastRequest();
  }

  std::string getLastTenantId() const {
    return loki_grpc_service_.getLastTenantId();
  }

 private:
  GrafanaLokiGrpcService loki_grpc_service_;
  std::unique_ptr<::grpc::Server> grpc_server_;
};

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki::test
