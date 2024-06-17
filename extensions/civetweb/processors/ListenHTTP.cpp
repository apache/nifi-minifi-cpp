/**
 * @file ListenHTTP.cpp

 * ListenHTTP class implementation
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
#include "ListenHTTP.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/Resource.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

const core::Relationship ListenHTTP::Self("__self__", "Marks the FlowFile to be owned by this processor");

void ListenHTTP::initialize() {
  logger_->log_trace("Initializing ListenHTTP");

  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ListenHTTP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  std::string basePath;

  if (!context.getProperty(BasePath, basePath)) {
    static_assert(BasePath.default_value);
    logger_->log_info("{} attribute is missing, so default value of {} will be used", BasePath.name, *BasePath.default_value);
    basePath = *BasePath.default_value;
  }

  basePath.insert(0, "/");

  if (!context.getProperty(Port, listeningPort)) {
    logger_->log_error("{} attribute is missing or invalid", Port.name);
    return;
  }

  bool randomPort = listeningPort == "0";

  std::string authDNPattern;
  if (context.getProperty(AuthorizedDNPattern, authDNPattern) && !authDNPattern.empty()) {
    logger_->log_debug("ListenHTTP using {}: {}", AuthorizedDNPattern.name, authDNPattern);
  } else {
    authDNPattern = ".*";
    logger_->log_debug("Authorized DN Pattern not set or invalid, using default '{}' pattern", authDNPattern);
  }

  std::string sslCertFile;

  if (context.getProperty(SSLCertificate, sslCertFile) && !sslCertFile.empty()) {
    logger_->log_debug("ListenHTTP using {}: {}", SSLCertificate.name, sslCertFile);
  }

  // Read further TLS/SSL options only if TLS/SSL usage is implied by virtue of certificate value being set
  std::string sslCertAuthorityFile;
  std::string sslVerifyPeer;
  std::string sslMinVer;

  if (!sslCertFile.empty()) {
    if (context.getProperty(SSLCertificateAuthority, sslCertAuthorityFile) && !sslCertAuthorityFile.empty()) {
      logger_->log_debug("ListenHTTP using {}: {}", SSLCertificateAuthority.name, sslCertAuthorityFile);
    }

    if (context.getProperty(SSLVerifyPeer, sslVerifyPeer)) {
      if (sslVerifyPeer.empty() || sslVerifyPeer == "no") {
        logger_->log_debug("ListenHTTP will not verify peers");
      } else {
        logger_->log_debug("ListenHTTP will verify peers");
      }
    } else {
      logger_->log_debug("ListenHTTP will not verify peers");
    }

    if (context.getProperty(SSLMinimumVersion, sslMinVer)) {
      logger_->log_debug("ListenHTTP using {}: {}", SSLMinimumVersion.name, sslMinVer);
    }
  }

  std::string headersAsAttributesPattern;

  if (context.getProperty(HeadersAsAttributesRegex, headersAsAttributesPattern) && !headersAsAttributesPattern.empty()) {
    logger_->log_debug("ListenHTTP using {}: {}", HeadersAsAttributesRegex.name, headersAsAttributesPattern);
  }

  auto numThreads = getMaxConcurrentTasks();

  logger_->log_info("ListenHTTP starting HTTP server on port {} and path {} with {} threads", randomPort ? "random" : listeningPort, basePath, numThreads);

  // Initialize web server
  std::vector<std::string> options;
  options.emplace_back("enable_keep_alive");
  options.emplace_back("yes");
  options.emplace_back("keep_alive_timeout_ms");
  options.emplace_back("15000");
  options.emplace_back("num_threads");
  options.emplace_back(std::to_string(numThreads));

  if (sslCertFile.empty()) {
    options.emplace_back("listening_ports");
    options.emplace_back(listeningPort);
  } else {
    listeningPort += "s";
    options.emplace_back("listening_ports");
    options.emplace_back(listeningPort);

    options.emplace_back("ssl_certificate");
    options.emplace_back(sslCertFile);

    if (!sslCertAuthorityFile.empty()) {
      options.emplace_back("ssl_ca_file");
      options.emplace_back(sslCertAuthorityFile);
    }

    if (sslVerifyPeer.empty() || sslVerifyPeer == "no") {
      options.emplace_back("ssl_verify_peer");
      options.emplace_back("no");
    } else {
      options.emplace_back("ssl_verify_peer");
      options.emplace_back("yes");
    }

    if (sslMinVer == "TLS1.2") {
      options.emplace_back("ssl_protocol_version");
      options.emplace_back(std::to_string(4));
    } else {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Invalid SSL Minimum Version specified!");
    }
  }

  server_ = std::make_unique<CivetServer>(options, &callbacks_, &logger_);

  context.getProperty(BatchSize, batch_size_);
  logger_->log_debug("ListenHTTP using {}: {}", BatchSize.name, batch_size_);

  std::optional<std::string> flow_id;
  if (auto flow_version = context.getProcessorNode()->getFlowIdentifier()) {
    flow_id = flow_version->getFlowId();
  }

  handler_ = std::make_unique<Handler>(basePath, flow_id, context.getProperty<uint64_t>(BufferSize).value_or(0), std::move(authDNPattern),
    headersAsAttributesPattern.empty() ? std::nullopt : std::make_optional<utils::Regex>(headersAsAttributesPattern));
  server_->addHandler(basePath, handler_.get());

  if (randomPort) {
    const auto& vec = server_->getListeningPorts();
    if (vec.size() != 1) {
      logger_->log_error("Random port is set, but there is no listening port! Server most probably failed to start!");
    } else {
      bool is_secure = isSecure();
      listeningPort = std::to_string(vec[0]);
      if (is_secure) {
        listeningPort += "s";
      }
      logger_->log_info("Listening on port {}", listeningPort);
    }
  }
}

ListenHTTP::~ListenHTTP() = default;

void ListenHTTP::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  logger_->log_trace("OnTrigger ListenHTTP");
  bool restored_processed = false;
  for (auto& ff : file_store_.getNewFlowFiles()) {
    restored_processed = true;
    if (!processFlowFile(ff)) {
      session.remove(ff);
    }
  }
  const bool incoming_processed = processIncomingFlowFile(session);
  const bool request_processed = processRequestBuffer(session);
  if (!restored_processed && !incoming_processed && !request_processed) {
    yield();
  }
}

void ListenHTTP::restore(const std::shared_ptr<core::FlowFile>& flowFile) {
  if (!flowFile) return;
  file_store_.put(flowFile);
}

/// @return Whether there was a flow file processed.
bool ListenHTTP::processIncomingFlowFile(core::ProcessSession &session) {
  std::shared_ptr<core::FlowFile> flow_file = session.get();
  if (!flow_file) {
    return false;
  }

  std::string type;
  flow_file->getAttribute("http.type", type);

  if (type == "response_body" && handler_ && processFlowFile(flow_file)) {
    session.transfer(flow_file, Self);
  } else {
    session.remove(flow_file);
  }

  return true;
}

bool ListenHTTP::processFlowFile(std::shared_ptr<core::FlowFile> flow_file) {
  ResponseBody response;
  flow_file->getAttribute("filename", response.uri);
  flow_file->getAttribute("mime.type", response.mime_type);
  if (response.mime_type.empty()) {
    logger_->log_warn("Using default mime type of application/octet-stream for response body file: {}", response.uri);
    response.mime_type = "application/octet-stream";
  }

  response.flow_file = flow_file;
  return handler_->setResponseBody(response);
}

/// @return Whether there was a request processed
bool ListenHTTP::processRequestBuffer(core::ProcessSession& session) {
  gsl_Expects(handler_);
  std::size_t flow_file_count = 0;
  for (; batch_size_ == 0 || batch_size_ > flow_file_count; ++flow_file_count) {
    Handler::Request req;
    if (!handler_->dequeueRequest(req)) {
      break;
    }

    [&] {
      std::promise<void> req_done_promise;
      auto res = req_done_promise.get_future();
      req.set_value(Handler::RequestValue{std::ref(session), std::move(req_done_promise)});
      return res;
    }().wait();
  }

  logger_->log_debug("ListenHTTP transferred {} flow files from HTTP request buffer", flow_file_count);
  return flow_file_count > 0;
}

namespace {

class MgConnectionInputStream : public io::InputStream {
 public:
  MgConnectionInputStream(struct mg_connection* conn, std::optional<size_t> size): conn_(conn), size_(size) {}

  size_t read(std::span<std::byte> out_buffer) override {
    const auto mg_read_return = mg_read(conn_, out_buffer.data(), std::min(out_buffer.size(), size_.value_or(std::numeric_limits<size_t>::max()) - offset_));
    if (mg_read_return <= 0) {
      return 0;
    }
    offset_ += gsl::narrow<size_t>(mg_read_return);
    return gsl::narrow<size_t>(mg_read_return);
  }

 private:
  struct mg_connection* conn_;
  size_t offset_{0};
  std::optional<size_t> size_;
};

class MgConnectionOutputStream : public io::OutputStream {
 public:
  MgConnectionOutputStream(struct mg_connection* conn): conn_(conn) {}

  size_t write(const uint8_t *value, size_t len) override {
    const auto mg_write_return = mg_write(conn_, reinterpret_cast<const void*>(value), len);
    if (mg_write_return <= 0) {
      return io::STREAM_ERROR;
    }
    return gsl::narrow<size_t>(mg_write_return);
  }

 private:
  struct mg_connection* conn_;
};

}  // namespace

ListenHTTP::Handler::Handler(std::string base_uri, std::optional<std::string> flow_id, uint64_t buffer_size, std::string &&auth_dn_regex, std::optional<utils::Regex> &&headers_as_attrs_regex)
    : base_uri_(std::move(base_uri)),
      flow_id_(std::move(flow_id)),
      auth_dn_regex_(std::move(auth_dn_regex)),
      headers_as_attrs_regex_(std::move(headers_as_attrs_regex)),
      buffer_size_(buffer_size) {
  logger_->log_debug("ListenHTTP using {}: {}", BufferSize.name, buffer_size_);
}

void ListenHTTP::Handler::sendHttp500(mg_connection* const conn) {
  mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\n"
                  "Content-Type: text/html\r\n"
                  "Content-Length: 0\r\n\r\n");
}

void ListenHTTP::Handler::sendHttp503(mg_connection* const conn) {
  mg_printf(conn, "HTTP/1.1 503 Service Unavailable\r\n"
                  "Content-Type: text/html\r\n"
                  "Content-Length: 0\r\n\r\n");
}

void ListenHTTP::Handler::setHeaderAttributes(const mg_request_info *req_info, core::FlowFile& flow_file) const {
  // Add filename from "filename" header value (and pattern headers)
  for (int i = 0; i < req_info->num_headers; i++) {
    auto header = &req_info->http_headers[i];

    if (strcmp("filename", header->name) == 0) {
      flow_file.setAttribute("filename", header->value);
    } else if (headers_as_attrs_regex_ && utils::regexMatch(header->name, *headers_as_attrs_regex_)) {
      flow_file.setAttribute(header->name, header->value);
    }
  }

  if (req_info->query_string) {
    flow_file.addAttribute("http.query", req_info->query_string);
  }
}

void ListenHTTP::Handler::enqueueRequest(mg_connection *conn, const mg_request_info *req_info, bool write_body) {
  if (buffer_size_ != 0 && request_buffer_.size() >= buffer_size_) {
    logger_->log_warn("ListenHTTP buffer is full, '{}' request for '{}' uri was dropped", req_info->request_method, req_info->request_uri);
    sendHttp503(conn);
    return;
  } else {
    logger_->log_warn("ListenHTTP buffer is NOT full {}/{}, '{}' request for '{}' uri was dropped", request_buffer_.size() + 1, buffer_size_, req_info->request_method, req_info->request_uri);
  }

  Request req;
  auto req_triggered = req.get_future();

  request_buffer_.enqueue(std::move(req));

  auto req_result = req_triggered.get();
  if (!req_result) {
    sendHttp503(conn);
    req_result.error().second.set_value();
    return;
  }

  auto& [session, req_done] = *req_result;

  auto flow_file = session.get().create();
  if (flow_id_) {
    flow_file->setAttribute(core::SpecialFlowAttribute::FLOW_ID, flow_id_.value());
  }

  if (write_body) {
    session.get().write(flow_file, [&] (auto& out) {
      MgConnectionInputStream mg_body{conn, req_info->content_length};
      return minifi::internal::pipe(mg_body, *out);
    });
  }

  setHeaderAttributes(req_info, *flow_file);
  mg_printf(conn, "HTTP/1.1 200 OK\r\n");
  writeBody(&session.get(), conn, req_info);

  session.get().transfer(flow_file, Success);

  req_done.set_value();
}

bool ListenHTTP::Handler::handlePost(CivetServer* /*server*/, struct mg_connection *conn) {
  auto req_info = mg_get_request_info(conn);
  if (!req_info) {
      logger_->log_error("ListenHTTP handling POST resulted in a null request");
      return false;
  }
  logger_->log_debug("ListenHTTP handling POST request of length {}", req_info->content_length);

  if (!authRequest(conn, req_info)) {
    return true;
  }

  // Always send 100 Continue, as allowed per standard to minimize client delay (https://www.w3.org/Protocols/rfc2616/rfc2616-sec8.html)
  mg_printf(conn, "HTTP/1.1 100 Continue\r\n\r\n");

  enqueueRequest(conn, req_info, true);
  return true;
}

bool ListenHTTP::Handler::authRequest(mg_connection *conn, const mg_request_info *req_info) const {
  // If this is a two-way TLS connection, authorize the peer against the configured pattern
  bool authorized = true;
  if (req_info->is_ssl && req_info->client_cert != nullptr) {
    if (!utils::regexMatch(req_info->client_cert->subject, auth_dn_regex_)) {
      mg_printf(conn, "HTTP/1.1 403 Forbidden\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 0\r\n\r\n");
      logger_->log_warn("ListenHTTP client DN not authorized: {}", req_info->client_cert->subject);
      authorized = false;
    }
  }
  return authorized;
}

bool ListenHTTP::Handler::handleGet(CivetServer* /*server*/, struct mg_connection *conn) {
  auto req_info = mg_get_request_info(conn);
  if (!req_info) {
      logger_->log_error("ListenHTTP handling GET resulted in a null request");
      return false;
  }
  logger_->log_debug("ListenHTTP handling GET request of URI {}", req_info->request_uri);

  if (!authRequest(conn, req_info)) {
    return true;
  }

  enqueueRequest(conn, req_info, false);
  return true;
}

bool ListenHTTP::Handler::handleHead(CivetServer* /*server*/, struct mg_connection *conn) {
  auto req_info = mg_get_request_info(conn);
  if (!req_info) {
    logger_->log_error("ListenHTTP handling HEAD resulted in a null request");
    return false;
  }
  logger_->log_debug("ListenHTTP handling HEAD request of URI {}", req_info->request_uri);

  if (!authRequest(conn, req_info)) {
    return true;
  }

  mg_printf(conn, "HTTP/1.1 200 OK\r\n");
  writeBody(nullptr, conn, req_info);

  return true;
}

bool ListenHTTP::Handler::handlePut(CivetServer* /*server*/, struct mg_connection* conn) {
  mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                  "Content-Type: text/html\r\n"
                  "Content-Length: 0\r\n\r\n");
  return true;
}

bool ListenHTTP::Handler::handleDelete(CivetServer* /*server*/, struct mg_connection* conn) {
  mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n"
                  "Content-Type: text/html\r\n"
                  "Content-Length: 0\r\n\r\n");
  return true;
}

bool ListenHTTP::Handler::setResponseBody(const ResponseBody& response) {
  std::lock_guard<std::mutex> guard(uri_map_mutex_);

  if (response.flow_file->getSize() == 0) {
    logger_->log_info("Unregistering response body for URI '{}'",
                      response.uri);
    response_uri_map_.erase(response.uri);
    return false;
  } else {
    logger_->log_info("Registering response body for URI '{}' of length {}",
                      response.uri,
                      response.flow_file->getSize());
    response_uri_map_[response.uri] = response;
    return true;
  }
}

bool ListenHTTP::Handler::dequeueRequest(Request& req) {
  return request_buffer_.tryDequeue(req);
}

void ListenHTTP::Handler::writeBody(core::ProcessSession* payload_reader, mg_connection *conn, const mg_request_info *req_info) {
  const auto &request_uri_str = std::string(req_info->request_uri);

  if (request_uri_str.size() > base_uri_.size() + 1) {
    ResponseBody response;
    {
      // Attempt to minimize time holding mutex (it would be nice to have a lock-free concurrent map here)
      std::lock_guard<std::mutex> guard(uri_map_mutex_);
      std::string req_uri = request_uri_str.substr(base_uri_.size() + 1);

      if (response_uri_map_.contains(req_uri)) {
        response = response_uri_map_[req_uri];
      }
    }

    if (response.flow_file && response.flow_file->getSize() != 0) {
      logger_->log_debug("Writing response body of {} bytes for URI: {}", response.flow_file->getSize(), req_info->request_uri);
      mg_printf(conn, "Content-type: ");
      mg_printf(conn, "%s", response.mime_type.c_str());
      mg_printf(conn, "\r\n");
      mg_printf(conn, "Content-length: ");
      mg_printf(conn, "%s", std::to_string(response.flow_file->getSize()).c_str());
      mg_printf(conn, "\r\n\r\n");
      if (payload_reader) {
        payload_reader->read(response.flow_file, [&] (auto& content) {
          MgConnectionOutputStream out{conn};
          return minifi::internal::pipe(*content, out);
        });
      }
    } else {
      logger_->log_debug("No response body available for URI: {}", req_info->request_uri);
      mg_printf(conn, "Content-length: 0\r\n\r\n");
    }
  } else {
    logger_->log_debug("No response body available for URI: {}", req_info->request_uri);
    mg_printf(conn, "Content-length: 0\r\n\r\n");
  }
}

bool ListenHTTP::isSecure() const {
  return (listeningPort.length() > 0) && *listeningPort.rbegin() == 's';
}

std::string ListenHTTP::getPort() const {
  if (isSecure()) {
    return listeningPort.substr(0, listeningPort.length() -1);
  }
  return listeningPort;
}

void ListenHTTP::notifyStop() {
  if (handler_) {
    handler_->stop();
  }

  server_.reset();
  handler_.reset();
}

std::set<core::Connectable*> ListenHTTP::getOutGoingConnections(const std::string &relationship) {
  auto result = core::Processor::getOutGoingConnections(relationship);
  if (relationship == Self.getName()) {
    result.insert(this);
  }
  return result;
}

REGISTER_RESOURCE(ListenHTTP, Processor);

}  // namespace org::apache::nifi::minifi::processors
