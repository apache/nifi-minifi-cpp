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

  handler_ = std::make_unique<Handler>(basePath, &context, std::move(authDNPattern),
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
  const bool incoming_processed = processIncomingFlowFile(session);
  const bool request_processed = processRequestBuffer(session);
  if (!incoming_processed && !request_processed) {
    yield();
  }
}

/// @return Whether there was a flow file processed.
bool ListenHTTP::processIncomingFlowFile(core::ProcessSession &session) {
  std::shared_ptr<core::FlowFile> flow_file = session.get();
  if (!flow_file) {
    return false;
  }

  std::string type;
  flow_file->getAttribute("http.type", type);

  if (type == "response_body" && handler_) {
    ResponseBody response;
    flow_file->getAttribute("filename", response.uri);
    flow_file->getAttribute("mime.type", response.mime_type);
    if (response.mime_type.empty()) {
      logger_->log_warn("Using default mime type of application/octet-stream for response body file: {}", response.uri);
      response.mime_type = "application/octet-stream";
    }
    response.body = session.readBuffer(flow_file).buffer;
    handler_->setResponseBody(response);
  }

  session.remove(flow_file);
  return true;
}

/// @return Whether there was a request processed
bool ListenHTTP::processRequestBuffer(core::ProcessSession& session) {
  gsl_Expects(handler_);
  std::size_t flow_file_count = 0;
  for (; batch_size_ == 0 || batch_size_ > flow_file_count; ++flow_file_count) {
    FlowFileBufferPair flow_file_buffer_pair;
    if (!handler_->dequeueRequest(flow_file_buffer_pair)) {
      break;
    }

    auto flow_file = flow_file_buffer_pair.first;
    session.add(flow_file);

    if (flow_file_buffer_pair.second) {
      session.writeBuffer(flow_file, flow_file_buffer_pair.second->getBuffer());
    }

    session.transfer(flow_file, Success);
  }

  logger_->log_debug("ListenHTTP transferred {} flow files from HTTP request buffer", flow_file_count);
  return flow_file_count > 0;
}

ListenHTTP::Handler::Handler(std::string base_uri, core::ProcessContext *context, std::string &&auth_dn_regex, std::optional<utils::Regex> &&headers_as_attrs_regex)
    : base_uri_(std::move(base_uri)),
      auth_dn_regex_(std::move(auth_dn_regex)),
      headers_as_attrs_regex_(std::move(headers_as_attrs_regex)),
      process_context_(context) {
  context->getProperty(BufferSize, buffer_size_);
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

void ListenHTTP::Handler::enqueueRequest(mg_connection *conn, const mg_request_info *req_info, std::unique_ptr<io::BufferStream> content_buffer) {
  auto flow_file = std::make_shared<FlowFileRecordImpl>();
  auto flow_version = process_context_->getProcessorNode()->getFlowIdentifier();
  if (flow_version != nullptr) {
    flow_file->setAttribute(core::SpecialFlowAttribute::FLOW_ID, flow_version->getFlowId());
  }

  setHeaderAttributes(req_info, *flow_file);

  if (buffer_size_ == 0 || request_buffer_.size() < buffer_size_) {
    request_buffer_.enqueue(std::make_pair(std::move(flow_file), std::move(content_buffer)));
  } else {
    logger_->log_warn("ListenHTTP buffer is full, '{}' request for '{}' uri was dropped", req_info->request_method, req_info->request_uri);
    sendHttp503(conn);
    return;
  }

  mg_printf(conn, "HTTP/1.1 200 OK\r\n");
  writeBody(conn, req_info);
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

  enqueueRequest(conn, req_info, createContentBuffer(conn, req_info));
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

  enqueueRequest(conn, req_info, nullptr);
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
  writeBody(conn, req_info, false /*include_payload*/);

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

void ListenHTTP::Handler::setResponseBody(const ResponseBody& response) {
  std::lock_guard<std::mutex> guard(uri_map_mutex_);

  if (response.body.empty()) {
    logger_->log_info("Unregistering response body for URI '{}'",
                      response.uri);
    response_uri_map_.erase(response.uri);
  } else {
    logger_->log_info("Registering response body for URI '{}' of length {}",
                      response.uri,
                      response.body.size());
    response_uri_map_[response.uri] = response;
  }
}

bool ListenHTTP::Handler::dequeueRequest(FlowFileBufferPair &flow_file_buffer_pair) {
  return request_buffer_.tryDequeue(flow_file_buffer_pair);
}

void ListenHTTP::Handler::writeBody(mg_connection *conn, const mg_request_info *req_info, bool include_payload /*=true*/) {
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

    if (!response.body.empty()) {
      logger_->log_debug("Writing response body of {} bytes for URI: {}", response.body.size(), req_info->request_uri);
      mg_printf(conn, "Content-type: ");
      mg_printf(conn, "%s", response.mime_type.c_str());
      mg_printf(conn, "\r\n");
      mg_printf(conn, "Content-length: ");
      mg_printf(conn, "%s", std::to_string(response.body.size()).c_str());
      mg_printf(conn, "\r\n\r\n");
      if (include_payload) {
        mg_write(conn, reinterpret_cast<char*>(response.body.data()), response.body.size());
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

std::unique_ptr<io::BufferStream> ListenHTTP::Handler::createContentBuffer(struct mg_connection *conn, const struct mg_request_info *req_info) {
  auto content_buffer = std::make_unique<io::BufferStream>();
  size_t nlen = 0;
  int64_t tlen = req_info->content_length;
  std::array<uint8_t, 16384> buf{};

  // if we have no content length we should call mg_read until
  // there is no data left from the stream to be HTTP/1.1 compliant
  while (tlen == -1 || (tlen > 0 && nlen < gsl::narrow<size_t>(tlen))) {
    auto rlen = tlen == -1 ? buf.size() : gsl::narrow<size_t>(tlen) - nlen;
    if (rlen > buf.size()) {
      rlen = buf.size();
    }

    // Read a buffer of data from client
    const auto mg_read_return = mg_read(conn, buf.data(), rlen);
    if (mg_read_return <= 0) {
      break;
    }
    rlen = gsl::narrow<size_t>(mg_read_return);

    // Transfer buffer data to the output stream
    content_buffer->write(buf.data(), rlen);

    nlen += rlen;
  }

  return content_buffer;
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
  server_.reset();
  handler_.reset();
}

REGISTER_RESOURCE(ListenHTTP, Processor);

}  // namespace org::apache::nifi::minifi::processors
