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
#include <utility>

#include "range/v3/view/join.hpp"
#include "range/v3/range/conversion.hpp"

#include "utils/gsl.h"
#include "utils/expected.h"
#include "utils/net/DNS.h"
#include "utils/net/Socket.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/PropertyBuilder.h"
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
    ->withDescription("The port on the destination. Can be a service name like ssh or http, as defined in /etc/services.")
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
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
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

  const auto data = session->readBuffer(flow_file);
  if (data.status < 0) {
    session->transfer(flow_file, Failure);
    return;
  }

  const auto nonthrowing_sockaddr_ntop = [](const sockaddr* const sa) -> std::string {
    return utils::try_expression([sa] { return utils::net::sockaddr_ntop(sa); }).value_or("(n/a)");
  };

  const auto debug_log_resolved_names = [&, this](const addrinfo& names) -> decltype(auto) {
    if (logger_->should_log(core::logging::LOG_LEVEL::debug)) {
      std::vector<std::string> names_vector;
      for (const addrinfo* it = &names; it; it = it->ai_next) {
        names_vector.push_back(nonthrowing_sockaddr_ntop(it->ai_addr));
      }
      logger_->log_debug("resolved \'%s\' to: %s",
          hostname,
          names_vector | ranges::views::join(',') | ranges::to<std::string>());
    }
    return names;
  };

  utils::net::resolveHost(hostname.c_str(), port.c_str(), utils::net::IpProtocol::UDP)
      | utils::map(utils::dereference)
      | utils::map(debug_log_resolved_names)
      | utils::flatMap([](const auto& names) { return utils::net::open_socket(names); })
      | utils::flatMap([&, this](utils::net::OpenSocketResult socket_handle_and_selected_name) -> nonstd::expected<void, std::error_code> {
        const auto& [socket_handle, selected_name] = socket_handle_and_selected_name;
        logger_->log_debug("connected to %s", nonthrowing_sockaddr_ntop(selected_name->ai_addr));
#ifdef WIN32
        const char* const buffer_ptr = reinterpret_cast<const char*>(data.buffer.data());
#else
        const void* const buffer_ptr = data.buffer.data();
#endif
        const auto send_result = ::sendto(socket_handle.get(), buffer_ptr, data.buffer.size(), 0, selected_name->ai_addr, selected_name->ai_addrlen);
        logger_->log_trace("sendto returned %ld", static_cast<long>(send_result));  // NOLINT: sendto
        if (send_result == utils::net::SocketError) {
          return nonstd::make_unexpected(utils::net::get_last_socket_error());
        }
        session->transfer(flow_file, Success);
        return {};
      })
      | utils::orElse([&, this](std::error_code ec) {
        gsl_Expects(ec);
        logger_->log_error("%s", ec.message());
        session->transfer(flow_file, Failure);
      });
}

REGISTER_RESOURCE(PutUDP, Processor);

}  // namespace org::apache::nifi::minifi::processors

