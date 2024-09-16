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

#include "range/v3/range/conversion.hpp"

#include "utils/gsl.h"
#include "utils/expected.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "core/logging/LoggerFactory.h"

#include "asio/ip/udp.hpp"
#include "utils/net/AsioSocketUtils.h"

using asio::ip::udp;

namespace org::apache::nifi::minifi::processors {

PutUDP::PutUDP(std::string_view name, const utils::Identifier& uuid)
    : ProcessorImpl(name, uuid), logger_{core::logging::LoggerFactory<PutUDP>::getLogger(uuid)}
{ }

PutUDP::~PutUDP() = default;

void PutUDP::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutUDP::notifyStop() {}

void PutUDP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  // if the required properties are missing or empty even before evaluating the EL expression, then we can throw in onSchedule, before we waste any flow files
  if (context.getProperty(Hostname).value_or(std::string{}).empty()) {
    throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "missing hostname"};
  }
  if (context.getProperty(Port).value_or(std::string{}).empty()) {
    throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "missing port"};
  }
}

void PutUDP::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  const auto flow_file = session.get();
  if (!flow_file) {
    yield();
    return;
  }

  const auto hostname = context.getProperty(Hostname, flow_file.get()).value_or(std::string{});
  const auto port = context.getProperty(Port, flow_file.get()).value_or(std::string{});
  if (hostname.empty() || port.empty()) {
    logger_->log_error("[{}] invalid target endpoint: hostname: {}, port: {}", flow_file->getUUIDStr(),
        hostname.empty() ? "(empty)" : hostname.c_str(),
        port.empty() ? "(empty)" : port.c_str());
    session.transfer(flow_file, Failure);
    return;
  }

  const auto data = session.readBuffer(flow_file);
  if (io::isError(data.status)) {
    session.transfer(flow_file, Failure);
    return;
  }

  asio::io_context io_context;

  const auto resolve_hostname = [&io_context, &hostname, &port]() -> nonstd::expected<udp::resolver::results_type, std::error_code> {
    udp::resolver resolver(io_context);
    std::error_code error_code;
    auto results = resolver.resolve(hostname, port, error_code);
    if (error_code)
      return nonstd::make_unexpected(error_code);
    return results;
  };

  const auto send_data_to_endpoint = [&io_context, &data, &logger = this->logger_](const udp::resolver::results_type& resolved_query) -> nonstd::expected<void, std::error_code> {
    std::error_code error;
    for (const auto& resolver_entry : resolved_query) {
      error.clear();
      udp::socket socket(io_context);
      socket.open(resolver_entry.endpoint().protocol(), error);
      if (error) {
        logger->log_debug("opening {} socket failed due to {} ", resolver_entry.endpoint().protocol() == udp::v4() ? "IPv4" : "IPv6", error.message());
        continue;
      }
      socket.send_to(asio::buffer(data.buffer), resolver_entry.endpoint(), udp::socket::message_flags{}, error);
      if (error) {
        logger->log_debug("sending to endpoint {} failed due to {}", resolver_entry.endpoint(), error.message());
        continue;
      }
      logger->log_debug("sending to endpoint {} succeeded", resolver_entry.endpoint());
      return {};
    }
    return nonstd::make_unexpected(error);
  };

  const auto transfer_to_success = [&session, &flow_file]() -> void {
    session.transfer(flow_file, Success);
  };

  const auto transfer_to_failure = [&session, &flow_file, &logger = this->logger_](std::error_code ec) -> void {
    gsl_Expects(ec);
    logger->log_error("{}", ec.message());
    session.transfer(flow_file, Failure);
  };

  resolve_hostname()
      | utils::andThen(send_data_to_endpoint)
      | utils::transform(transfer_to_success)
      | utils::orElse(transfer_to_failure);
}

REGISTER_RESOURCE(PutUDP, Processor);

}  // namespace org::apache::nifi::minifi::processors
