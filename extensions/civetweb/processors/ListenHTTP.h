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
#ifndef __LISTEN_HTTP_H__
#define __LISTEN_HTTP_H__

#include <memory>
#include <regex>

#include <CivetServer.h>
#include <concurrentqueue.h>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// ListenHTTP Class
class ListenHTTP : public core::Processor {
 public:

  // Constructor
  /*!
   * Create a new processor
   */
  ListenHTTP(std::string name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<ListenHTTP>::getLogger()) {
  }
  // Destructor
  virtual ~ListenHTTP();
  // Processor Name
  static constexpr char const *ProcessorName = "ListenHTTP";
  // Supported Properties
  static core::Property BasePath;
  static core::Property Port;
  static core::Property AuthorizedDNPattern;
  static core::Property SSLCertificate;
  static core::Property SSLCertificateAuthority;
  static core::Property SSLVerifyPeer;
  static core::Property SSLMinimumVersion;
  static core::Property HeadersAsAttributesRegex;
  // Supported Relationships
  static core::Relationship Success;

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
  void initialize();
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);

  struct response_body {
    std::string uri;
    std::string mime_type;
    std::string body;
  };

  // HTTP request handler
  class Handler : public CivetHandler {
   public:
    Handler(std::string base_uri,
            core::ProcessContext *context,
            core::ProcessSessionFactory *sessionFactory,
            std::string &&authDNPattern,
            std::string &&headersAsAttributesPattern);
    bool handlePost(CivetServer *server, struct mg_connection *conn);
    bool handleGet(CivetServer *server, struct mg_connection *conn);

    /**
     * Sets a static response body string to be used for a given URI, with a number of seconds it will be kept in memory.
     * @param response
     */
    void set_response_body(struct response_body response) {
      std::lock_guard<std::mutex> guard(uri_map_mutex_);

      if (response.body.empty()) {
        logger_->log_info("Unregistering response body for URI '%s'",
                          response.uri);
        response_uri_map_.erase(response.uri);
      } else {
        logger_->log_info("Registering response body for URI '%s' of length %lu",
                          response.uri,
                          response.body.size());
        response_uri_map_[response.uri] = std::move(response);
      }
    }

   private:
    // Send HTTP 500 error response to client
    void send_error_response(struct mg_connection *conn);
    bool auth_request(mg_connection *conn, const mg_request_info *req_info) const;
    void set_header_attributes(const mg_request_info *req_info, const std::shared_ptr<FlowFileRecord> &flow_file) const;
    void write_body(mg_connection *conn, const mg_request_info *req_info);

    std::string base_uri_;
    std::regex auth_dn_regex_;
    std::regex headers_as_attrs_regex_;
    core::ProcessContext *process_context_;
    core::ProcessSessionFactory *session_factory_;

    // Logger
    std::shared_ptr<logging::Logger> logger_;
    std::map<std::string, response_body> response_uri_map_;
    std::mutex uri_map_mutex_;
  };

  class ResponseBodyReadCallback : public InputStreamCallback {
   public:
    explicit ResponseBodyReadCallback(std::string *out_str)
        : out_str_(out_str) {
    }
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      out_str_->resize(stream->getSize());
      uint64_t num_read = stream->readData(reinterpret_cast<uint8_t *>(&(*out_str_)[0]),
                                           static_cast<int>(stream->getSize()));

      if (num_read != stream->getSize()) {
        throw std::runtime_error("GraphReadCallback failed to fully read flow file input stream");
      }

      return num_read;
    }

   private:
    std::string *out_str_;
  };

  // Write callback for transferring data from HTTP request to content repo
  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(struct mg_connection *conn, const struct mg_request_info *reqInfo);
    int64_t process(std::shared_ptr<io::BaseStream> stream);

   private:
    // Logger
    std::shared_ptr<logging::Logger> logger_;

    struct mg_connection *conn_;
    const struct mg_request_info *req_info_;
  };

 private:
  // Logger
  std::shared_ptr<logging::Logger> logger_;

  std::unique_ptr<CivetServer> server_;
  std::unique_ptr<Handler> handler_;
};

REGISTER_RESOURCE(ListenHTTP, "Starts an HTTP Server and listens on a given base path to transform incoming requests into FlowFiles. The default URI of the Service will be "
    "http://{hostname}:{port}/contentListener. Only HEAD, POST, and GET requests are supported. PUT, and DELETE will result in an error and the HTTP response status code 405."
    " The response body text for all requests, by default, is empty (length of 0). A static response body can be set for a given URI by sending input files to ListenHTTP with "
    "the http.type attribute set to response_body. The response body FlowFile filename attribute is appended to the Base Path property (separated by a /) when mapped to incoming requests. "
    "The mime.type attribute of the response body FlowFile is used for the Content-type header in responses. Response body content can be cleared by sending an empty (size 0) "
    "FlowFile for a given URI mapping.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
