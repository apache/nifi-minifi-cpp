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
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/MinifiConcurrentQueue.h"
#include "utils/gsl.h"
#include "utils/Export.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

class ListenHTTP : public core::Processor {
 public:
  using FlowFileBufferPair = std::pair<std::shared_ptr<FlowFileRecord>, std::unique_ptr<io::BufferStream>>;

  explicit ListenHTTP(std::string name, const utils::Identifier& uuid = {})
      : Processor(std::move(name), uuid),
        batch_size_(0) {
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

  EXTENSIONAPI static const core::Property BasePath;
  EXTENSIONAPI static const core::Property Port;
  EXTENSIONAPI static const core::Property AuthorizedDNPattern;
  EXTENSIONAPI static const core::Property SSLCertificate;
  EXTENSIONAPI static const core::Property SSLCertificateAuthority;
  EXTENSIONAPI static const core::Property SSLVerifyPeer;
  EXTENSIONAPI static const core::Property SSLMinimumVersion;
  EXTENSIONAPI static const core::Property HeadersAsAttributesRegex;
  EXTENSIONAPI static const core::Property BatchSize;
  EXTENSIONAPI static const core::Property BufferSize;
  static auto properties() {
    return std::array{
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
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  std::string getPort() const;
  bool isSecure() const;

  struct ResponseBody {
    std::string uri;
    std::string mime_type;
    std::string body;
  };

  // HTTP request handler
  class Handler : public CivetHandler {
   public:
    Handler(std::string base_uri,
            core::ProcessContext *context,
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
    void setResponseBody(const ResponseBody& response);

    bool dequeueRequest(FlowFileBufferPair &flow_file_buffer_pair);

   private:
    static void sendHttp500(struct mg_connection *conn);
    static void sendHttp503(struct mg_connection *conn);
    bool authRequest(mg_connection *conn, const mg_request_info *req_info) const;
    void setHeaderAttributes(const mg_request_info *req_info, const std::shared_ptr<core::FlowFile> &flow_file) const;
    void writeBody(mg_connection *conn, const mg_request_info *req_info, bool include_payload = true);
    static std::unique_ptr<io::BufferStream> createContentBuffer(struct mg_connection *conn, const struct mg_request_info *req_info);
    void enqueueRequest(mg_connection *conn, const mg_request_info *req_info, std::unique_ptr<io::BufferStream>);

    std::string base_uri_;
    utils::Regex auth_dn_regex_;
    std::optional<utils::Regex> headers_as_attrs_regex_;
    core::ProcessContext *process_context_;
    std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListenHTTP>::getLogger();
    std::map<std::string, ResponseBody> response_uri_map_;
    std::mutex uri_map_mutex_;
    uint64_t buffer_size_;
    utils::ConcurrentQueue<FlowFileBufferPair> request_buffer_;
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
      core::logging::LOG_ERROR((*logger)) << "CivetWeb error: " << message;
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
      core::logging::LOG_DEBUG((*logger)) << "CivetWeb access: " << message;
    } catch (...) {
    }
    return 0;
  }

 protected:
  void notifyStop() override;

 private:
  static const uint64_t DEFAULT_BUFFER_SIZE;

  bool processIncomingFlowFile(core::ProcessSession &session);
  bool processRequestBuffer(core::ProcessSession &session);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListenHTTP>::getLogger();
  CivetCallbacks callbacks_;
  std::unique_ptr<CivetServer> server_;
  std::unique_ptr<Handler> handler_;
  std::string listeningPort;
  uint64_t batch_size_;
};

}  // namespace org::apache::nifi::minifi::processors
