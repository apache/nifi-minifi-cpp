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
#include "PutUDP.h"

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <netdb.h>
#endif /* WIN32 */
#include <tuple>
#include <utility>

#include "range/v3/view/join.hpp"
#include "range/v3/range/conversion.hpp"

#include "utils/gsl.h"
#include "utils/OptionalUtils.h"
#include "utils/net/DNS.h"
#include "utils/StringUtils.h"
#include "utils/net/Socket.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::processors {

const core::Property PutUDP::Hostname = core::PropertyBuilder::createProperty("Hostname")
    ->withDescription("The ip address or hostname of the destination.")
    ->withDefaultValue("localhost")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->build();

const core::Property PutUDP::Port = core::PropertyBuilder::createProperty("Port")
    ->withDescription("The port on the destination.")
    ->isRequired(true)
    ->supportsExpressionLanguage(true)
    ->build();

const core::Relationship PutUDP::Success{"success", "FlowFiles that are sent to the destination are sent out this relationship."};
const core::Relationship PutUDP::Failure{"failure", "FlowFiles that encountered IO errors are send out this relationship."};

PutUDP::PutUDP(const std::string& name, const utils::Identifier& uuid)
  :Processor(name, uuid), logger_{core::logging::LoggerFactory<PutUDP>::getLogger()}
{ }

PutUDP::~PutUDP() = default;

void PutUDP::initialize() {
  setSupportedProperties({
    Hostname,
    Port
  });
  setSupportedRelationships({
    Success,
    Failure
  });
}

void PutUDP::notifyStop() {}

void PutUDP::onSchedule(core::ProcessContext* const context, core::ProcessSessionFactory*) {
  gsl_Expects(context);

  // if the required properties are missing or empty even before evaluating the EL expression, then we can throw in onSchedule, before we waste any flow files
  if (context->getProperty(Hostname).value_or(std::string{}).empty()) {
    throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "missing hostname"};
  }
  if (context->getProperty(Port).value_or(std::string{}).empty()) {
    throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "missing port"};
  }
}

void PutUDP::onTrigger(core::ProcessContext* context, core::ProcessSession* const session) {
  gsl_Expects(context && session);

  const auto flow_file = session->get();
  if (!flow_file) {
    yield();
    return;
  }

  const auto hostname = context->getProperty(Hostname, flow_file).value_or(std::string{});
  const auto port = context->getProperty(Port, flow_file).value_or(std::string{});
  if (hostname.empty() || port.empty()) {
    logger_->log_error("[%s] invalid target endpoint: hostname: %s, port: %s", flow_file->getUUIDStr(),
        hostname.empty() ? "(empty)" : hostname.c_str(),
        port.empty() ? "(empty)" : port.c_str());
    session->transfer(flow_file, Failure);
    return;
  }

  try {
    const auto names = utils::net::resolveHost(hostname.c_str(), port.c_str(), utils::net::IpProtocol::Udp);
    if (logger_->should_log(core::logging::LOG_LEVEL::debug)) {
      std::vector<std::string> names_vector;
      for (const addrinfo* it = names.get(); it; it = it->ai_next) {
        names_vector.push_back(utils::net::sockaddr_ntop(it->ai_addr));
      }
      logger_->log_debug("resolved \'%s\' to: %s",
          hostname,
          names_vector | ranges::views::join(',') | ranges::to<std::string>());
    }

    auto open_socket_result = utils::net::open_socket(names.get());
    if (!open_socket_result) {
      logger_->log_error("socket: %s", utils::net::get_last_socket_error_message());
      session->transfer(flow_file, Failure);
      return;
    }
    const auto[socket_handle, selected_name] = *std::move(open_socket_result);
    logger_->log_debug("connected to %s", utils::net::sockaddr_ntop(selected_name->ai_addr));

    const auto data = session->readBuffer(flow_file);
    if (data.status < 0) {
      session->transfer(flow_file, Failure);
      return;
    }

#ifdef WIN32
    const char* const buffer_ptr = reinterpret_cast<const char*>(data.buffer.data());
#else
    const void* const buffer_ptr = data.buffer.data();
#endif
    const auto send_result = ::sendto(socket_handle.get(), buffer_ptr, data.buffer.size(), 0, selected_name->ai_addr, selected_name->ai_addrlen);
    if (send_result == utils::net::SocketError) {
      throw Exception{ExceptionType::FILE_OPERATION_EXCEPTION, utils::StringUtils::join_pack("sendto: ", utils::net::get_last_socket_error_message())};
    }
    logger_->log_trace("sendto returned %ld", static_cast<long>(send_result));  // NOLINT: sendto
  } catch (const std::exception& ex) {
    logger_->log_error("[%s] %s", flow_file->getUUIDStr(), ex.what());
    session->transfer(flow_file, Failure);
    return;
  }

  session->transfer(flow_file, Success);
}

REGISTER_RESOURCE(PutUDP, "The PutUDP processor receives a FlowFile and packages the FlowFile content into a single UDP datagram packet which is then transmitted to the configured UDP server. "
                          "The processor doesn't guarantee a successful transfer, even if the flow file is routed to the success relationship.");

}  // namespace org::apache::nifi::minifi::processors

