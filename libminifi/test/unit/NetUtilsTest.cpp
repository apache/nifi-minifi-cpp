/**
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

#include <string>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/net/DNS.h"
#include "utils/net/AsioSocketUtils.h"
#include "utils/StringUtils.h"
#include "controllers/SSLContextService.h"
#include "unit/TestUtils.h"

namespace utils = org::apache::nifi::minifi::utils;
namespace net = utils::net;

TEST_CASE("net::reverseDnsLookup", "[net][dns][reverseDnsLookup]") {
  SECTION("dns.google IPv6") {
    if (minifi::test::utils::isIPv6Disabled())
      SKIP("IPv6 is disabled");
    auto dns_google_hostname = net::reverseDnsLookup(asio::ip::address::from_string("2001:4860:4860::8888"));
    REQUIRE(dns_google_hostname.has_value());
    CHECK(dns_google_hostname == "dns.google");
  }

  SECTION("dns.google IPv4") {
    auto dns_google_hostname = net::reverseDnsLookup(asio::ip::address::from_string("8.8.8.8"));
    REQUIRE(dns_google_hostname.has_value());
    CHECK(dns_google_hostname == "dns.google");
  }

  SECTION("Unresolvable address IPv6") {
    if (minifi::test::utils::isIPv6Disabled())
      SKIP("IPv6 is disabled");
    auto unresolvable_hostname = net::reverseDnsLookup(asio::ip::address::from_string("2001:db8::"));
    REQUIRE(unresolvable_hostname.has_value());
    CHECK(unresolvable_hostname == "2001:db8::");
  }

  SECTION("Unresolvable address IPv4") {
    auto unresolvable_hostname = net::reverseDnsLookup(asio::ip::address::from_string("192.0.2.0"));
    REQUIRE(unresolvable_hostname.has_value());
    CHECK(unresolvable_hostname == "192.0.2.0");
  }
}

TEST_CASE("utils::net::getSslContext") {
  TestController controller;
  auto plan = controller.createPlan();

  auto ssl_context_node = plan->addController("SSLContextService", "ssl_context_service");
  auto ssl_context_service = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(ssl_context_node->getControllerServiceImplementation());

  const std::filesystem::path cert_dir = minifi::utils::file::FileUtils::get_executable_dir() / "resources";

  REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::CACertificate, (cert_dir / "ca_A.crt").string()));

  SECTION("Secure") {
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::ClientCertificate, (cert_dir / "alice_by_A.pem").string()));
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::PrivateKey, (cert_dir / "alice.key").string()));
  }
  SECTION("Secure empty pass") {
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::ClientCertificate, (cert_dir / "alice_by_A.pem").string()));
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::PrivateKey, (cert_dir / "alice.key").string()));
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::Passphrase, (cert_dir / "empty_pass").string()));
  }
  SECTION("Secure with file pass") {
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::ClientCertificate, (cert_dir / "alice_by_A.pem").string()));
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::PrivateKey, (cert_dir / "alice_encrypted.key").string()));
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::Passphrase, (cert_dir / "alice_encryption_pass").string()));
  }
  SECTION("Secure with pass") {
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::ClientCertificate, (cert_dir / "alice_by_A.pem").string()));
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::PrivateKey, (cert_dir / "alice_encrypted.key").string()));
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::Passphrase, "VsVTmHBzixyA9UfTCttRYXus1oMpIxO6jmDXrNrOp5w"));
  }
  SECTION("Secure with common cert and key file") {
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::ClientCertificate, (cert_dir / "alice_by_A_with_key.pem").string()));
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::CACertificate, (cert_dir / "alice_by_A_with_key.pem").string()));
  }
  REQUIRE_NOTHROW(plan->finalize());
  auto ssl_context = utils::net::getSslContext(*ssl_context_service);
  asio::error_code verification_error;
  ssl_context.set_verify_mode(asio::ssl::verify_peer, verification_error);
  CHECK(!verification_error);
}

TEST_CASE("utils::net::getSslContext passphrase problems") {
  TestController controller;
  auto plan = controller.createPlan();

  auto ssl_context_node = plan->addController("SSLContextService", "ssl_context_service");
  auto ssl_context_service = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(ssl_context_node->getControllerServiceImplementation());

  const std::filesystem::path cert_dir = minifi::utils::file::FileUtils::get_executable_dir() / "resources";

  REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::CACertificate, (cert_dir / "ca_A.crt").string()));
  REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::ClientCertificate, (cert_dir / "alice_by_A.pem").string()));
  REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::PrivateKey, (cert_dir / "alice_encrypted.key").string()));

  SECTION("Missing passphrase") {
    REQUIRE_NOTHROW(plan->finalize());
    REQUIRE_THROWS_WITH(utils::net::getSslContext(*ssl_context_service), "use_private_key_file: bad decrypt (Provider routines)");
  }

  SECTION("Invalid passphrase") {
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::Passphrase, "not_the_correct_passphrase"));
    REQUIRE_NOTHROW(plan->finalize());
    REQUIRE_THROWS_WITH(utils::net::getSslContext(*ssl_context_service), "use_private_key_file: bad decrypt (Provider routines)");
  }

  SECTION("Invalid passphrase file") {
    REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::Passphrase, (cert_dir / "alice_by_B.pem").string()));
    REQUIRE_NOTHROW(plan->finalize());
    REQUIRE_THROWS_WITH(utils::net::getSslContext(*ssl_context_service), "use_private_key_file: bad decrypt (Provider routines)");
  }
}

TEST_CASE("utils::net::getSslContext missing CA") {
  TestController controller;
  auto plan = controller.createPlan();

  auto ssl_context_node = plan->addController("SSLContextService", "ssl_context_service");
  auto ssl_context_service = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(ssl_context_node->getControllerServiceImplementation());

  const std::filesystem::path cert_dir = minifi::utils::file::FileUtils::get_executable_dir() / "resources";

  REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::ClientCertificate, (cert_dir / "alice_by_A.pem").string()));
  REQUIRE(ssl_context_service->setProperty(minifi::controllers::SSLContextServiceImpl::PrivateKey, (cert_dir / "alice.key").string()));

  REQUIRE_NOTHROW(plan->finalize());
  auto ssl_context = utils::net::getSslContext(*ssl_context_service);
  asio::error_code verification_error;
  ssl_context.set_verify_mode(asio::ssl::verify_peer, verification_error);
  CHECK(!verification_error);
}
