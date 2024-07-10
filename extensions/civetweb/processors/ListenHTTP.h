/**
 * @file ListenHTTP.h
 * ListenHTTP class declaration
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
#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>

#include <CivetServer.h>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/MinifiConcurrentQueue.h"
#include "utils/gsl.h"
#include "utils/Export.h"
#include "utils/RegexUtils.h"
#include "core/FlowFileStore.h"

namespace org::apache::nifi::minifi::test {
struct ListenHTTPTestAccessor;
}  // namespace org::apache::nifi::minifi::test

namespace org::apache::nifi::minifi::processors {

class ListenHTTP : public core::Processor {
 private:
  static constexpr std::string_view DEFAULT_BUFFER_SIZE_STR = "5";
  static const core::Relationship Self;

 public:
  friend struct ::org::apache::nifi::minifi::test::ListenHTTPTestAccessor;

  using FlowFileBufferPair = std::pair<std::shared_ptr<FlowFileRecord>, std::unique_ptr<io::BufferStream>>;

  explicit ListenHTTP(std::string_view name, const utils::Identifier& uuid = {})
      : Processor(name, uuid) {
    callbacks_.log_message = &logMessage;
    callbacks_.log_access = &logAccess;
  }
  ~ListenHTTP() override;

  EXTENSIONAPI static constexpr const char* Description = "Starts an HTTP Server and listens on a given base path to transform incoming requests into FlowFiles. The default URI of the Service "
      "will be http://{hostname}:{port}/contentListener. Only HEAD, POST, and GET requests are supported. PUT, and DELETE will result in an error and the HTTP response status code 405. "
      "The response body text for all requests, by default, is empty (length of 0). A static response body can be set for a given URI by sending input files to ListenHTTP with "
      "the http.type attribute set to response_body. The response body FlowFile filename attribute is appended to the Base Path property (separated by a /) when mapped to incoming requests. "
      "The mime.type attribute of the response body FlowFile is used for the Content-type header in responses. Response body content can be cleared by sending an empty (size 0) "
      "FlowFile for a given URI mapping.";


  EXTENSIONAPI static constexpr auto BasePath = core::PropertyDefinitionBuilder<>::createProperty("Base Path")
      .withDescription("Base path for incoming connections")
      .isRequired(false)
      .withDefaultValue("contentListener")
      .build();
  EXTENSIONAPI static constexpr auto Port = core::PropertyDefinitionBuilder<>::createProperty("Listening Port")
      .withDescription("The Port to listen on for incoming connections. 0 means port is going to be selected randomly.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::LISTEN_PORT_TYPE)
      .withDefaultValue("80")
      .build();
  EXTENSIONAPI static constexpr auto AuthorizedDNPattern = core::PropertyDefinitionBuilder<>::createProperty("Authorized DN Pattern")
      .withDescription("A Regular Expression to apply against the Distinguished Name of incoming"
          " connections. If the Pattern does not match the DN, the connection will be refused.")
      .withDefaultValue(".*")
      .build();
  EXTENSIONAPI static constexpr auto SSLCertificate = core::PropertyDefinitionBuilder<>::createProperty("SSL Certificate")
      .withDescription("File containing PEM-formatted file including TLS/SSL certificate and key")
      .build();
  EXTENSIONAPI static constexpr auto SSLCertificateAuthority = core::PropertyDefinitionBuilder<>::createProperty("SSL Certificate Authority")
      .withDescription("File containing trusted PEM-formatted certificates")
      .build();
  EXTENSIONAPI static constexpr auto SSLVerifyPeer = core::PropertyDefinitionBuilder<2>::createProperty("SSL Verify Peer")
      .withDescription("Whether or not to verify the client's certificate (yes/no)")
      .isRequired(false)
      .withAllowedValues({"yes", "no"})
      .withDefaultValue("no")
      .build();
  EXTENSIONAPI static constexpr auto SSLMinimumVersion = core::PropertyDefinitionBuilder<1>::createProperty("SSL Minimum Version")
      .withDescription("Minimum TLS/SSL version allowed (TLS1.2)")
      .isRequired(false)
      .withAllowedValues({"TLS1.2"})
      .withDefaultValue("TLS1.2")
      .build();
  EXTENSIONAPI static constexpr auto HeadersAsAttributesRegex = core::PropertyDefinitionBuilder<>::createProperty("HTTP Headers to receive as Attributes (Regex)")
      .withDescription("Specifies the Regular Expression that determines the names of HTTP Headers that should be passed along as FlowFile attributes")
      .build();
  EXTENSIONAPI static constexpr auto BatchSize = core::PropertyDefinitionBuilder<>::createProperty("Batch Size")
        .withDescription("Maximum number of buffered requests to be processed in a single batch. If set to zero all buffered requests are processed.")
        .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
        .withDefaultValue(ListenHTTP::DEFAULT_BUFFER_SIZE_STR)
        .build();
  EXTENSIONAPI static constexpr auto BufferSize = core::PropertyDefinitionBuilder<>::createProperty("Buffer Size")
        .withDescription("Maximum number of HTTP Requests allowed to be buffered before processing them when the processor is triggered. "
            "If the buffer full, the request is refused. If set to zero the buffer is unlimited.")
        .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
        .withDefaultValue(ListenHTTP::DEFAULT_BUFFER_SIZE_STR)
        .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      BasePath,
      Port,
      AuthorizedDNPattern,
      SSLCertificate,
      SSLCertificateAuthority,
      SSLVerifyPeer,
      SSLMinimumVersion,
      HeadersAsAttributesRegex,
      BatchSize,
      BufferSize
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  std::string getPort() const;
  bool isSecure() const;
  void restore(const std::shared_ptr<core::FlowFile>& flowFile) override;

  bool isWorkAvailable() override {
    return handler_ ? !handler_->empty() : false;
  }

  std::set<core::Connectable*> getOutGoingConnections(const std::string &relationship) override;

  struct ResponseBody {
    std::string uri;
    std::string mime_type;
    std::shared_ptr<core::FlowFile> flow_file;
  };

  // HTTP request handler
  class Handler : public CivetHandler {
   public:
    enum class FailureReason {
      PROCESSOR_SHUTDOWN
    };
    using RequestValue = std::pair<std::reference_wrapper<core::ProcessSession>, std::promise<void>>;
    using FailureValue = std::pair<FailureReason, std::promise<void>>;
    using Request = std::promise<nonstd::expected<RequestValue, FailureValue>>;

    Handler(std::string base_uri,
            std::optional<std::string> flow_id,
            uint64_t buffer_size,
            std::string &&auth_dn_regex,
            std::optional<utils::Regex> &&headers_as_attrs_regex);
    bool handlePost(CivetServer *server, struct mg_connection *conn) override;
    bool handleGet(CivetServer *server, struct mg_connection *conn) override;
    bool handleHead(CivetServer *server, struct mg_connection *conn) override;
    bool handlePut(CivetServer *server, struct mg_connection *conn) override;
    bool handleDelete(CivetServer *server, struct mg_connection *conn) override;

    /**
     * Sets a static response body string to be used for a given URI, with a number of seconds it will be kept in memory.
     * @param response
     */
    bool setResponseBody(const ResponseBody& response);

    bool dequeueRequest(Request& req);

    size_t requestCount() const {
      return request_buffer_.size();
    }

    bool empty() const {
      return request_buffer_.empty();
    }

    void stop() {
      request_buffer_.stop();
      Request req;
      while (dequeueRequest(req)) {
        std::promise<void> req_done_promise;
        auto req_done = req_done_promise.get_future();
        req.set_value(nonstd::make_unexpected(FailureValue{Handler::FailureReason::PROCESSOR_SHUTDOWN, std::move(req_done_promise)}));
        req_done.wait();
      }
    }

   private:
    static void sendHttp500(struct mg_connection *conn);
    static void sendHttp503(struct mg_connection *conn);
    bool authRequest(mg_connection *conn, const mg_request_info *req_info) const;
    void setHeaderAttributes(const mg_request_info *req_info, core::FlowFile& flow_file) const;
    void writeBody(core::ProcessSession* payload_reader, mg_connection *conn, const mg_request_info *req_info);
    void enqueueRequest(mg_connection *conn, const mg_request_info *req_info, bool write_body);

    class RequestBuffer : public utils::ConcurrentQueue<Request> {
     public:
      void stop() {
        std::lock_guard lock{mtx_};
        running_ = false;
      }

     protected:
      void enqueueImpl(Request req) override {
        if (!running_) {
          req.set_value(nonstd::make_unexpected(FailureValue{FailureReason::PROCESSOR_SHUTDOWN, std::promise<void>{}}));
        } else {
          utils::ConcurrentQueue<Request>::enqueueImpl(std::move(req));
        }
      }

     private:
      bool running_{true};
    };

    std::string base_uri_;
    std::optional<std::string> flow_id_;
    utils::Regex auth_dn_regex_;
    std::optional<utils::Regex> headers_as_attrs_regex_;
    std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListenHTTP>::getLogger();
    std::map<std::string, ResponseBody> response_uri_map_;
    std::mutex uri_map_mutex_;
    uint64_t buffer_size_{0};
    RequestBuffer request_buffer_;
  };

  static int logMessage(const struct mg_connection *conn, const char *message) {
    try {
      struct mg_context* ctx = mg_get_context(conn);
      /* CivetServer stores 'this' as the userdata when calling mg_start */
      auto* const server = static_cast<CivetServer*>(mg_get_user_data(ctx));
      if (server == nullptr) {
        return 0;
      }
      auto* const logger = static_cast<std::shared_ptr<core::logging::Logger>*>(const_cast<void*>(server->getUserContext()));
      if (logger == nullptr) {
        return 0;
      }
      (*logger)->log_error("CivetWeb error: {}", message);
    } catch (...) {
    }
    return 0;
  }

  static int logAccess(const struct mg_connection *conn, const char *message) {
    try {
      struct mg_context* ctx = mg_get_context(conn);
      /* CivetServer stores 'this' as the userdata when calling mg_start */
      auto* server = static_cast<CivetServer*>(mg_get_user_data(ctx));
      if (server == nullptr) {
        return 0;
      }
      auto* logger = static_cast<std::shared_ptr<core::logging::Logger>*>(const_cast<void*>(server->getUserContext()));
      if (logger == nullptr) {
        return 0;
      }
      (*logger)->log_debug("CivetWeb access: {}", message);
    } catch (...) {
    }
    return 0;
  }

 protected:
  void notifyStop() override;

 private:
  bool processIncomingFlowFile(core::ProcessSession &session);
  bool processFlowFile(const std::shared_ptr<core::FlowFile>& flow_file);
  bool processRequestBuffer(core::ProcessSession &session);
  size_t pendingRequestCount() {
    return handler_ ? handler_->requestCount() : 0;
  }

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListenHTTP>::getLogger(uuid_);
  CivetCallbacks callbacks_;
  std::unique_ptr<CivetServer> server_;
  std::unique_ptr<Handler> handler_;
  std::string listeningPort;
  uint64_t batch_size_{0};
  core::FlowFileStore file_store_;
};

}  // namespace org::apache::nifi::minifi::processors
