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

#include "utils/net/AsioSocketUtils.h"
#include "controllers/SSLContextService.h"

namespace org::apache::nifi::minifi::utils::net {

template<>
asio::awaitable<std::tuple<std::error_code>> handshake(TcpSocket&, asio::steady_timer::duration) {
  co_return std::error_code();
}

template<>
asio::awaitable<std::tuple<std::error_code>> handshake(SslSocket& socket, asio::steady_timer::duration timeout_duration) {
  co_return co_await asyncOperationWithTimeout(socket.async_handshake(HandshakeType::client, use_nothrow_awaitable), timeout_duration);  // NOLINT
}

asio::ssl::context getClientSslContext(const controllers::SSLContextService& ssl_context_service) {
  asio::ssl::context ssl_context(asio::ssl::context::tls_client);
  ssl_context.set_options(asio::ssl::context::no_tlsv1 | asio::ssl::context::no_tlsv1_1);
  ssl_context.load_verify_file(ssl_context_service.getCACertificate().string());
  ssl_context.set_verify_mode(asio::ssl::verify_peer);
  ssl_context.set_password_callback([password = ssl_context_service.getPassphrase()](std::size_t&, asio::ssl::context_base::password_purpose&) { return password; });
  if (const auto& cert_file = ssl_context_service.getCertificateFile(); !cert_file.empty())
    ssl_context.use_certificate_file(cert_file.string(), asio::ssl::context::pem);
  if (const auto& private_key_file = ssl_context_service.getPrivateKeyFile(); !private_key_file.empty())
    ssl_context.use_private_key_file(private_key_file.string(), asio::ssl::context::pem);
  return ssl_context;
}

}  // namespace org::apache::nifi::minifi::utils::net
