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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const uint64_t ListenHTTP::DEFAULT_BUFFER_SIZE = 20000;

core::Property ListenHTTP::BasePath(
    core::PropertyBuilder::createProperty("Base Path")
        ->withDescription("Base path for incoming connections")
        ->isRequired(false)
        ->withDefaultValue<std::string>("contentListener")->build());

core::Property ListenHTTP::Port(
    core::PropertyBuilder::createProperty("Listening Port")
        ->withDescription("The Port to listen on for incoming connections. 0 means port is going to be selected randomly.")
        ->isRequired(true)
        ->withDefaultValue<int>(80, core::StandardValidators::get().LISTEN_PORT_VALIDATOR)->build());

core::Property ListenHTTP::AuthorizedDNPattern("Authorized DN Pattern", "A Regular Expression to apply against the Distinguished Name of incoming"
                                               " connections. If the Pattern does not match the DN, the connection will be refused.",
                                               ".*");
core::Property ListenHTTP::SSLCertificate("SSL Certificate", "File containing PEM-formatted file including TLS/SSL certificate and key", "");
core::Property ListenHTTP::SSLCertificateAuthority("SSL Certificate Authority", "File containing trusted PEM-formatted certificates", "");

core::Property ListenHTTP::SSLVerifyPeer(
    core::PropertyBuilder::createProperty("SSL Verify Peer")
        ->withDescription("Whether or not to verify the client's certificate (yes/no)")
        ->isRequired(false)
        ->withAllowableValues<std::string>({"yes", "no"})
        ->withDefaultValue("no")->build());

core::Property ListenHTTP::SSLMinimumVersion(
    core::PropertyBuilder::createProperty("SSL Minimum Version")
        ->withDescription("Minimum TLS/SSL version allowed (TLS1.2)")
        ->isRequired(false)
        ->withAllowableValues<std::string>({"TLS1.2"})
        ->withDefaultValue("TLS1.2")->build());

core::Property ListenHTTP::HeadersAsAttributesRegex("HTTP Headers to receive as Attributes (Regex)", "Specifies the Regular Expression that determines the names of HTTP Headers that"
                                                    " should be passed along as FlowFile attributes",
                                                    "");

core::Property ListenHTTP::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")
        ->withDescription("Maximum number of buffered requests to be processed in a single batch. If set to zero all buffered requests are processed.")
        ->withDefaultValue<uint64_t>(ListenHTTP::DEFAULT_BUFFER_SIZE)->build());

core::Property ListenHTTP::BufferSize(
    core::PropertyBuilder::createProperty("Buffer Size")
        ->withDescription("Maximum number of HTTP Requests allowed to be buffered before processing them when the processor is triggered. "
                          "If the buffer full, the request is refused. If set to zero the buffer is unlimited.")
        ->withDefaultValue<uint64_t>(ListenHTTP::DEFAULT_BUFFER_SIZE)->build());

core::Relationship ListenHTTP::Success("success", "All files are routed to success");

void ListenHTTP::initialize() {
  logger_->log_trace("Initializing ListenHTTP");

  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(BasePath);
  properties.insert(Port);
  properties.insert(AuthorizedDNPattern);
  properties.insert(SSLCertificate);
  properties.insert(SSLCertificateAuthority);
  properties.insert(SSLVerifyPeer);
  properties.insert(SSLMinimumVersion);
  properties.insert(HeadersAsAttributesRegex);
  properties.insert(BatchSize);
  properties.insert(BufferSize);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void ListenHTTP::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  std::string basePath;

  if (!context->getProperty(BasePath.getName(), basePath)) {
    logger_->log_info("%s attribute is missing, so default value of %s will be used", BasePath.getName(), BasePath.getValue().to_string());
    basePath = BasePath.getValue().to_string();
  }

  basePath.insert(0, "/");


  if (!context->getProperty(Port.getName(), listeningPort)) {
    logger_->log_error("%s attribute is missing or invalid", Port.getName());
    return;
  }

  bool randomPort = listeningPort == "0";

  std::string authDNPattern;

  if (context->getProperty(AuthorizedDNPattern.getName(), authDNPattern) && !authDNPattern.empty()) {
    logger_->log_debug("ListenHTTP using %s: %s", AuthorizedDNPattern.getName(), authDNPattern);
  }

  std::string sslCertFile;

  if (context->getProperty(SSLCertificate.getName(), sslCertFile) && !sslCertFile.empty()) {
    logger_->log_debug("ListenHTTP using %s: %s", SSLCertificate.getName(), sslCertFile);
  }

  // Read further TLS/SSL options only if TLS/SSL usage is implied by virtue of certificate value being set
  std::string sslCertAuthorityFile;
  std::string sslVerifyPeer;
  std::string sslMinVer;

  if (!sslCertFile.empty()) {
    if (context->getProperty(SSLCertificateAuthority.getName(), sslCertAuthorityFile) && !sslCertAuthorityFile.empty()) {
      logger_->log_debug("ListenHTTP using %s: %s", SSLCertificateAuthority.getName(), sslCertAuthorityFile);
    }

    if (context->getProperty(SSLVerifyPeer.getName(), sslVerifyPeer)) {
      if (sslVerifyPeer.empty() || sslVerifyPeer.compare("no") == 0) {
        logger_->log_debug("ListenHTTP will not verify peers");
      } else {
        logger_->log_debug("ListenHTTP will verify peers");
      }
    } else {
      logger_->log_debug("ListenHTTP will not verify peers");
    }

    if (context->getProperty(SSLMinimumVersion.getName(), sslMinVer)) {
      logger_->log_debug("ListenHTTP using %s: %s", SSLMinimumVersion.getName(), sslMinVer);
    }
  }

  std::string headersAsAttributesPattern;

  if (context->getProperty(HeadersAsAttributesRegex.getName(), headersAsAttributesPattern) && !headersAsAttributesPattern.empty()) {
    logger_->log_debug("ListenHTTP using %s: %s", HeadersAsAttributesRegex.getName(), headersAsAttributesPattern);
  }

  auto numThreads = getMaxConcurrentTasks();

  logger_->log_info("ListenHTTP starting HTTP server on port %s and path %s with %d threads", randomPort ? "random" : listeningPort, basePath, numThreads);

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

  server_.reset(new CivetServer(options, &callbacks_, &logger_));

  context->getProperty(BatchSize.getName(), batch_size_);
  logger_->log_debug("ListenHTTP using %s: %zu", BatchSize.getName(), batch_size_);

  handler_.reset(new Handler(basePath, context, std::move(authDNPattern), std::move(headersAsAttributesPattern)));
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
      logger_->log_info("Listening on port %s", listeningPort);
    }
  }
}

ListenHTTP::~ListenHTTP() = default;

void ListenHTTP::onTrigger(core::ProcessContext* /*context*/, core::ProcessSession *session) {
  logger_->log_debug("OnTrigger ListenHTTP");
  processIncomingFlowFile(session);
  processRequestBuffer(session);
}

void ListenHTTP::processIncomingFlowFile(core::ProcessSession *session) {
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    return;
  }

  std::string type;
  flow_file->getAttribute("http.type", type);

  if (type == "response_body" && handler_) {
    ResponseBody response;
    ResponseBodyReadCallback cb(&response.body);
    flow_file->getAttribute("filename", response.uri);
    flow_file->getAttribute("mime.type", response.mime_type);
    if (response.mime_type.empty()) {
      logger_->log_warn("Using default mime type of application/octet-stream for response body file: %s", response.uri);
      response.mime_type = "application/octet-stream";
    }
    session->read(flow_file, &cb);
    handler_->setResponseBody(std::move(response));
  }

  session->remove(flow_file);
}

void ListenHTTP::processRequestBuffer(core::ProcessSession *session) {
  std::size_t flow_file_count = 0;
  for (; batch_size_ == 0 || batch_size_ > flow_file_count; ++flow_file_count) {
    FlowFileBufferPair flow_file_buffer_pair;
    if (!handler_->dequeueRequest(flow_file_buffer_pair)) {
      break;
    }

    auto flow_file = flow_file_buffer_pair.first;
    session->add(flow_file);

    if (flow_file_buffer_pair.second) {
      WriteCallback callback(std::move(flow_file_buffer_pair.second));
      session->write(flow_file, &callback);
    }

    session->transfer(flow_file, Success);
  }

  logger_->log_debug("ListenHTTP transferred %zu flow files from HTTP request buffer", flow_file_count);
}

ListenHTTP::Handler::Handler(std::string base_uri, core::ProcessContext *context, std::string &&auth_dn_regex, std::string &&header_as_attrs_regex)
    : base_uri_(std::move(base_uri)),
      auth_dn_regex_(std::move(auth_dn_regex)),
      headers_as_attrs_regex_(std::move(header_as_attrs_regex)),
      process_context_(context),
      logger_(logging::LoggerFactory<ListenHTTP::Handler>::getLogger()) {
  context->getProperty(BufferSize.getName(), buffer_size_);
  logger_->log_debug("ListenHTTP using %s: %zu", BufferSize.getName(), buffer_size_);
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

void ListenHTTP::Handler::setHeaderAttributes(const mg_request_info *req_info, const std::shared_ptr<core::FlowFile> &flow_file) const {
  // Add filename from "filename" header value (and pattern headers)
  for (int i = 0; i < req_info->num_headers; i++) {
    auto header = &req_info->http_headers[i];

    if (strcmp("filename", header->name) == 0) {
      flow_file->setAttribute("filename", header->value);
    } else if (std::regex_match(header->name, headers_as_attrs_regex_)) {
      flow_file->setAttribute(header->name, header->value);
    }
  }

  if (req_info->query_string) {
    flow_file->addAttribute("http.query", req_info->query_string);
  }
}

void ListenHTTP::Handler::enqueueRequest(mg_connection *conn, const mg_request_info *req_info, std::unique_ptr<io::BufferStream> content_buffer) {
  auto flow_file = std::make_shared<FlowFileRecord>();
  auto flow_version = process_context_->getProcessorNode()->getFlowIdentifier();
  if (flow_version != nullptr) {
    flow_file->setAttribute(core::SpecialFlowAttribute::FLOW_ID, flow_version->getFlowId());
  }

  setHeaderAttributes(req_info, flow_file);

  if (buffer_size_ == 0 || request_buffer_.size() < buffer_size_) {
    request_buffer_.enqueue(std::make_pair(std::move(flow_file), std::move(content_buffer)));
  } else {
    logger_->log_warn("ListenHTTP buffer is full, '%s' request for '%s' uri was dropped", req_info->request_method, req_info->request_uri);
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
  logger_->log_debug("ListenHTTP handling POST request of length %lld", req_info->content_length);

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
    if (!std::regex_match(req_info->client_cert->subject, auth_dn_regex_)) {
      mg_printf(conn, "HTTP/1.1 403 Forbidden\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 0\r\n\r\n");
      logger_->log_warn("ListenHTTP client DN not authorized: %s", req_info->client_cert->subject);
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
  logger_->log_debug("ListenHTTP handling GET request of URI %s", req_info->request_uri);

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
  logger_->log_debug("ListenHTTP handling HEAD request of URI %s", req_info->request_uri);

  if (!authRequest(conn, req_info)) {
    return true;
  }

  mg_printf(conn, "HTTP/1.1 200 OK\r\n");
  writeBody(conn, req_info, false /*include_payload*/);

  return true;
}

void ListenHTTP::Handler::setResponseBody(const ResponseBody& response) {
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

      if (response_uri_map_.count(req_uri)) {
        response = response_uri_map_[req_uri];
      }
    }

    if (!response.body.empty()) {
      logger_->log_debug("Writing response body of %lu bytes for URI: %s", response.body.size(), req_info->request_uri);
      mg_printf(conn, "Content-type: ");
      mg_printf(conn, "%s", response.mime_type.c_str());
      mg_printf(conn, "\r\n");
      mg_printf(conn, "Content-length: ");
      mg_printf(conn, "%s", std::to_string(response.body.size()).c_str());
      mg_printf(conn, "\r\n\r\n");
      if (include_payload) {
        mg_printf(conn, "%s", response.body.c_str());
      }
    } else {
      logger_->log_debug("No response body available for URI: %s", req_info->request_uri);
      mg_printf(conn, "Content-length: 0\r\n\r\n");
    }
  } else {
    logger_->log_debug("No response body available for URI: %s", req_info->request_uri);
    mg_printf(conn, "Content-length: 0\r\n\r\n");
  }
}

std::unique_ptr<io::BufferStream> ListenHTTP::Handler::createContentBuffer(struct mg_connection *conn, const struct mg_request_info *req_info) {
  auto content_buffer = std::make_unique<io::BufferStream>();
  size_t nlen = 0;
  int64_t tlen = req_info->content_length;
  uint8_t buf[16384];

  // if we have no content length we should call mg_read until
  // there is no data left from the stream to be HTTP/1.1 compliant
  while (tlen == -1 || (tlen > 0 && nlen < gsl::narrow<size_t>(tlen))) {
    auto rlen = tlen == -1 ? sizeof(buf) : gsl::narrow<size_t>(tlen) - nlen;
    if (rlen > sizeof(buf)) {
      rlen = sizeof(buf);
    }

    // Read a buffer of data from client
    const auto mg_read_return = mg_read(conn, &buf[0], rlen);
    if (mg_read_return <= 0) {
      break;
    }
    rlen = gsl::narrow<size_t>(mg_read_return);

    // Transfer buffer data to the output stream
    content_buffer->write(&buf[0], rlen);

    nlen += rlen;
  }

  return content_buffer;
}

ListenHTTP::WriteCallback::WriteCallback(std::unique_ptr<io::BufferStream> request_content)
    : request_content_(std::move(request_content)) {
}

int64_t ListenHTTP::WriteCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  const auto write_ret = stream->write(request_content_->getBuffer(), request_content_->size());
  return io::isError(write_ret) ? -1 : gsl::narrow<int64_t>(write_ret);
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
