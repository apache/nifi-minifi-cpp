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

#include "utils/gsl.h"
#include "utils/OptionalUtils.h"
#include "utils/net/DNS.h"
#include "utils/StringUtils.h"
#include "utils/net/Socket.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "range/v3/view/join.hpp"
#include "range/v3/range/conversion.hpp"

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <netdb.h>
#endif /* WIN32 */
#include <tuple>

namespace org::apache::nifi::minifi::processors {

const core::Property PutUDP::Hostname = core::PropertyBuilder::createProperty("Hostname")
    ->withDescription("The ip address or hostname of the destination.")
    ->withDefaultValue("localhost")
    ->isRequired(true)
    ->build();

const core::Property PutUDP::Port = core::PropertyBuilder::createProperty("Port")
    ->withDescription("The port on the destination.")
    ->isRequired(true)
    ->build();

const core::Relationship PutUDP::Success{"success", "FlowFiles that are sent successfully to the destination are sent out this relationship."};
const core::Relationship PutUDP::Failure{"failure", "FlowFiles that failed to send to the destination are sent out this relationship."};

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

  hostname_ = context->getProperty(Hostname).value();
  port_ = (context->getProperty(Port) | utils::orElse([]{ throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "missing port"}; })).value();
}

void PutUDP::onTrigger(core::ProcessContext*, core::ProcessSession* const session) {
  gsl_Expects(session);

  const auto flow_file = session->get();
  if (!flow_file) {
    yield();
    return;
  }

  const auto names = utils::net::resolveHost(hostname_.c_str(), port_.c_str(), utils::net::IpProtocol::Udp);
  if (logger_->should_log(core::logging::LOG_LEVEL::debug)) {
    std::vector<std::string> names_vector;
    for(const addrinfo* it = names.get(); it; it = it->ai_next) {
      names_vector.push_back(utils::net::sockaddr_ntop(it->ai_addr));
    }
    logger_->log_debug("resolved \'%s\' to: %s",
        hostname_,
        names_vector | ranges::views::join(',') | ranges::to<std::string>());
  }
  const auto [ sockfd, selected_name ] = [&names]() -> std::tuple<utils::net::SocketDescriptor, addrinfo*> {
    for(addrinfo* it = names.get(); it; it = it->ai_next) {
      const auto fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
      if (fd != utils::net::InvalidSocket) return std::make_tuple(fd, it);
    }
    return std::make_tuple(utils::net::InvalidSocket, nullptr);
  }();
  if (sockfd == utils::net::InvalidSocket) {
    throw Exception{ExceptionType::PROCESSOR_EXCEPTION, utils::StringUtils::join_pack("socket: ", utils::net::get_last_socket_error_message())};
  }
  const auto closer = gsl::finally([sockfd = sockfd] {
    utils::net::close_socket(sockfd);
  });

  logger_->log_debug("connected to %s", utils::net::sockaddr_ntop(selected_name->ai_addr));

  const auto data = session->readBuffer(flow_file);
  if (data.status < 0) {
    session->transfer(flow_file, Failure);
    return;
  }

  const auto send_result = ::sendto(sockfd, data.buffer.data(), data.buffer.size(), 0, selected_name->ai_addr, selected_name->ai_addrlen);
  if (send_result == utils::net::SocketError) {
    throw Exception{ExceptionType::FILE_OPERATION_EXCEPTION, utils::StringUtils::join_pack("sendto: ", utils::net::get_last_socket_error_message())};
  }

  logger_->log_trace("sendto returned %ld", static_cast<long>(send_result));

  session->transfer(flow_file, Success);
}

REGISTER_RESOURCE(PutUDP, "The PutUDP processor receives a FlowFile and packages the FlowFile content into a single UDP datagram packet which is then transmitted to the configured UDP server. The user must ensure that the FlowFile content being fed to this processor is not larger than the maximum size for the underlying UDP transport. The maximum transport size will vary based on the platform setup but is generally just under 64KB. FlowFiles will be marked as failed if their content is larger than the maximum transport size.");

}  // namespace org::apache::nifi::minifi::processors

