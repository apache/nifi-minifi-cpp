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
#include <regex>
#include <string>
#include <utility>

#include <CivetServer.h>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/MinifiConcurrentQueue.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// ListenHTTP Class
class ListenHTTP : public core::Processor {
 public:
  using FlowFileBufferPair = std::pair<std::shared_ptr<FlowFileRecord>, std::unique_ptr<io::BufferStream>>;

  // Constructor
  /*!
   * Create a new processor
   */
  explicit ListenHTTP(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<ListenHTTP>::getLogger()),
        batch_size_(0) {
    callbacks_.log_message = &logMessage;
    callbacks_.log_access = &logAccess;
  }
  // Destructor
  ~ListenHTTP() override;
  // Processor Name
  EXTENSIONAPI static constexpr char const *ProcessorName = "ListenHTTP";
  // Supported Properties
  EXTENSIONAPI static core::Property BasePath;
  EXTENSIONAPI static core::Property Port;
  EXTENSIONAPI static core::Property AuthorizedDNPattern;
  EXTENSIONAPI static core::Property SSLCertificate;
  EXTENSIONAPI static core::Property SSLCertificateAuthority;
  EXTENSIONAPI static core::Property SSLVerifyPeer;
  EXTENSIONAPI static core::Property SSLMinimumVersion;
  EXTENSIONAPI static core::Property HeadersAsAttributesRegex;
  EXTENSIONAPI static core::Property BatchSize;
  EXTENSIONAPI static core::Property BufferSize;
  // Supported Relationships
  EXTENSIONAPI static core::Relationship Success;

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
            std::string &&authDNPattern,
            std::string &&headersAsAttributesPattern);
    bool handlePost(CivetServer *server, struct mg_connection *conn) override;
    bool handleGet(CivetServer *server, struct mg_connection *conn) override;
    bool handleHead(CivetServer *server, struct mg_connection *conn) override;

    /**
     * Sets a static response body string to be used for a given URI, with a number of seconds it will be kept in memory.
     * @param response
     */
    void setResponseBody(const ResponseBody& response);

    bool dequeueRequest(FlowFileBufferPair &flow_file_buffer_pair);

   private:
    void sendHttp500(struct mg_connection *conn);
    void sendHttp503(struct mg_connection *conn);
    bool authRequest(mg_connection *conn, const mg_request_info *req_info) const;
    void setHeaderAttributes(const mg_request_info *req_info, const std::shared_ptr<core::FlowFile> &flow_file) const;
    void writeBody(mg_connection *conn, const mg_request_info *req_info, bool include_payload = true);
    std::unique_ptr<io::BufferStream> createContentBuffer(struct mg_connection *conn, const struct mg_request_info *req_info);
    void enqueueRequest(mg_connection *conn, const mg_request_info *req_info, std::unique_ptr<io::BufferStream>);

    std::string base_uri_;
    std::regex auth_dn_regex_;
    std::regex headers_as_attrs_regex_;
    core::ProcessContext *process_context_;
    std::shared_ptr<logging::Logger> logger_;
    std::map<std::string, ResponseBody> response_uri_map_;
    std::mutex uri_map_mutex_;
    uint64_t buffer_size_;
    utils::ConcurrentQueue<FlowFileBufferPair> request_buffer_;
  };

  class ResponseBodyReadCallback : public InputStreamCallback {
   public:
    explicit ResponseBodyReadCallback(std::string *out_str)
        : out_str_(out_str) {
    }
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      out_str_->resize(stream->size());
      const auto num_read = stream->read(reinterpret_cast<uint8_t *>(&(*out_str_)[0]), stream->size());
      if (num_read != stream->size()) {
        throw std::runtime_error("GraphReadCallback failed to fully read flow file input stream");
      }

      return gsl::narrow<int64_t>(num_read);
    }

   private:
    std::string *out_str_;
  };

  // Write callback for transferring data from HTTP request to content repo
  class WriteCallback : public OutputStreamCallback {
   public:
    explicit WriteCallback(std::unique_ptr<io::BufferStream>);
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;

   private:
    std::unique_ptr<io::BufferStream> request_content_;
  };

  static int logMessage(const struct mg_connection *conn, const char *message) {
    try {
      struct mg_context* ctx = mg_get_context(conn);
      /* CivetServer stores 'this' as the userdata when calling mg_start */
      auto* const server = static_cast<CivetServer*>(mg_get_user_data(ctx));
      if (server == nullptr) {
        return 0;
      }
      auto* const logger = static_cast<std::shared_ptr<logging::Logger>*>(const_cast<void*>(server->getUserContext()));
      if (logger == nullptr) {
        return 0;
      }
      logging::LOG_ERROR((*logger)) << "CivetWeb error: " << message;
    } catch (...) {
    }
    return 0;
  }

  static int logAccess(const struct mg_connection *conn, const char *message) {
    try {
      struct mg_context* ctx = mg_get_context(conn);
      /* CivetServer stores 'this' as the userdata when calling mg_start */
      CivetServer* server = static_cast<CivetServer*>(mg_get_user_data(ctx));
      if (server == nullptr) {
        return 0;
      }
      std::shared_ptr<logging::Logger>* logger = static_cast<std::shared_ptr<logging::Logger>*>(const_cast<void*>(server->getUserContext()));
      if (logger == nullptr) {
        return 0;
      }
      logging::LOG_DEBUG((*logger)) << "CivetWeb access: " << message;
    } catch (...) {
    }
    return 0;
  }

 protected:
  void notifyStop() override;

 private:
  static const uint64_t DEFAULT_BUFFER_SIZE;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

  void processIncomingFlowFile(core::ProcessSession *session);
  void processRequestBuffer(core::ProcessSession *session);

  std::shared_ptr<logging::Logger> logger_;
  CivetCallbacks callbacks_;
  std::unique_ptr<CivetServer> server_;
  std::unique_ptr<Handler> handler_;
  std::string listeningPort;
  uint64_t batch_size_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
