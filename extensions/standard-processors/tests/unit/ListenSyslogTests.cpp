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


#include "unit/Catch.h"
#include "ListenSyslog.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestUtils.h"
#include "controllers/SSLContextService.h"
#include "range/v3/algorithm/contains.hpp"

using ListenSyslog = org::apache::nifi::minifi::processors::ListenSyslog;

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

constexpr auto local_addresses = {"127.0.0.1", "::ffff:127.0.0.1", "::1"};

struct ValidRFC5424Message {
  constexpr ValidRFC5424Message(std::string_view message,
                                std::string_view priority,
                                std::string_view severity,
                                std::string_view facility,
                                std::string_view version,
                                std::string_view timestamp,
                                std::string_view hostname,
                                std::string_view app_name,
                                std::string_view proc_id,
                                std::string_view msg_id,
                                std::string_view structured_data,
                                std::string_view msg)
      : unparsed_(message),
        priority_(priority),
        severity_(severity),
        facility_(facility),
        version_(version),
        timestamp_(timestamp),
        hostname_(hostname),
        app_name_(app_name),
        proc_id_(proc_id),
        msg_id_(msg_id),
        structured_data_(structured_data),
        msg_(msg) {}

  const std::string_view unparsed_;
  const std::string_view priority_;
  const std::string_view severity_;
  const std::string_view facility_;
  const std::string_view version_;
  const std::string_view timestamp_;
  const std::string_view hostname_;
  const std::string_view app_name_;
  const std::string_view proc_id_;
  const std::string_view msg_id_;
  const std::string_view structured_data_;
  const std::string_view msg_;
};

struct ValidRFC3164Message {
  constexpr ValidRFC3164Message(std::string_view message,
                                std::string_view priority,
                                std::string_view severity,
                                std::string_view facility,
                                std::string_view timestamp,
                                std::string_view hostname,
                                std::string_view msg)
      : unparsed_(message),
        priority_(priority),
        severity_(severity),
        facility_(facility),
        timestamp_(timestamp),
        hostname_(hostname),
        msg_(msg) {}

  const std::string_view unparsed_;
  const std::string_view priority_;
  const std::string_view severity_;
  const std::string_view facility_;
  const std::string_view timestamp_;
  const std::string_view hostname_;
  const std::string_view msg_;
};

// These examples are from the Syslog Protocol RFC5424 documentation
// https://datatracker.ietf.org/doc/html/rfc5424#section-6.5

constexpr ValidRFC5424Message rfc5424_doc_example_1 = ValidRFC5424Message(
    R"(<34>1 2003-10-11T22:14:15.003Z mymachine.example.com su - ID47 - 'su root' failed for lonvick on /dev/pts/8)",
    "34",
    "2",
    "4",
    "1",
    "2003-10-11T22:14:15.003Z",
    "mymachine.example.com",
    "su",
    "-",
    "ID47",
    "-",
    "'su root' failed for lonvick on /dev/pts/8");

constexpr ValidRFC5424Message rfc5424_doc_example_2 = ValidRFC5424Message(
    R"(<165>1 2003-08-24T05:14:15.000003-07:00 192.0.2.1 myproc 8710 - - %% It's time to make the do-nuts.)",
    "165",
    "5",
    "20",
    "1",
    "2003-08-24T05:14:15.000003-07:00",
    "192.0.2.1",
    "myproc",
    "8710",
    "-",
    "-",
    "%% It's time to make the do-nuts.");

constexpr ValidRFC5424Message rfc5424_doc_example_3 = ValidRFC5424Message(
    R"(<165>1 2003-10-11T22:14:15.003Z mymachine.example.com evntslog - ID47 [exampleSDID@32473 iut="3" eventSource="Application" eventID="1011"] An application event log entry...)",
    "165",
    "5",
    "20",
    "1",
    "2003-10-11T22:14:15.003Z",
    "mymachine.example.com",
    "evntslog",
    "-",
    "ID47",
    R"([exampleSDID@32473 iut="3" eventSource="Application" eventID="1011"])",
    "An application event log entry...");

constexpr ValidRFC5424Message rfc5424_doc_example_4 = ValidRFC5424Message(
    R"(<165>1 2003-10-11T22:14:15.003Z mymachine.example.com evntslog - ID47 [exampleSDID@32473 iut="3" eventSource="Application" eventID="1011"][examplePriority@32473class="high"])",
    "165",
    "5",
    "20",
    "1",
    "2003-10-11T22:14:15.003Z",
    "mymachine.example.com",
    "evntslog",
    "-",
    "ID47",
    R"([exampleSDID@32473 iut="3" eventSource="Application" eventID="1011"][examplePriority@32473class="high"])",
    "");

// These examples are from the Syslog Protocol documentation
// https://datatracker.ietf.org/doc/html/rfc3164#section-5.4
constexpr ValidRFC3164Message rfc3164_doc_example_1 = ValidRFC3164Message(
    R"(<34>Oct 11 22:14:15 mymachine su: 'su root' failed for lonvick on /dev/pts/8)",
    "34",
    "2",
    "4",
    "Oct 11 22:14:15",
    "mymachine",
    "su: 'su root' failed for lonvick on /dev/pts/8");

constexpr ValidRFC3164Message rfc3164_doc_example_2 = ValidRFC3164Message(
    R"(<13>Feb 5 17:32:18 10.0.0.99 Use the BFG!)",
    "13",
    "5",
    "1",
    "Feb 5 17:32:18",
    "10.0.0.99",
    "Use the BFG!");

constexpr ValidRFC3164Message rfc3164_doc_example_3 = ValidRFC3164Message(
    R"(<165>Aug 24 05:34:00 mymachine myproc[10]: %% It's time to make the do-nuts.  %%  Ingredients: Mix=OK, Jelly=OK # Devices: Mixer=OK, Jelly_Injector=OK, Frier=OK # Transport: Conveyer1=OK, Conveyer2=OK # %%)",
    "165",
    "5",
    "20",
    "Aug 24 05:34:00",
    "mymachine",
    R"(myproc[10]: %% It's time to make the do-nuts.  %%  Ingredients: Mix=OK, Jelly=OK # Devices: Mixer=OK, Jelly_Injector=OK, Frier=OK # Transport: Conveyer1=OK, Conveyer2=OK # %%)");

constexpr ValidRFC3164Message rfc3164_doc_example_4 = ValidRFC3164Message(
    R"(<0>Oct 22 10:52:12 scapegoat 1990 Oct 22 10:52:01 TZ-6 scapegoat.dmz.example.org 10.1.2.3 sched[0]: That's All Folks!)",
    "0",
    "0",
    "0",
    "Oct 22 10:52:12",
    "scapegoat",
    R"(1990 Oct 22 10:52:01 TZ-6 scapegoat.dmz.example.org 10.1.2.3 sched[0]: That's All Folks!)");

// These examples are generated by https://man7.org/linux/man-pages/man1/logger.1.html
constexpr std::string_view rfc5424_logger_example_1 = R"(<13>1 2022-03-17T10:10:42.846095+01:00 host-name user - - [timeQuality tzKnown="1" isSynced="0"] log_line")";

constexpr std::string_view invalid_syslog = "not syslog";

void check_for_only_basic_attributes(core::FlowFile& flow_file, uint16_t port, std::string_view protocol) {
  CHECK(std::to_string(port) == flow_file.getAttribute("syslog.port"));
  CHECK(protocol == flow_file.getAttribute("syslog.protocol"));
  CHECK(ranges::contains(local_addresses, flow_file.getAttribute("syslog.sender")));

  CHECK(std::nullopt == flow_file.getAttribute("syslog.valid"));
  CHECK(std::nullopt == flow_file.getAttribute("syslog.priority"));
  CHECK(std::nullopt == flow_file.getAttribute("syslog.version"));
  CHECK(std::nullopt == flow_file.getAttribute("syslog.timestamp"));
  CHECK(std::nullopt == flow_file.getAttribute("syslog.hostname"));
  CHECK(std::nullopt == flow_file.getAttribute("syslog.app_name"));
  CHECK(std::nullopt == flow_file.getAttribute("syslog.proc_id"));
  CHECK(std::nullopt == flow_file.getAttribute("syslog.msg_id"));
  CHECK(std::nullopt == flow_file.getAttribute("syslog.structured_data"));
  CHECK(std::nullopt == flow_file.getAttribute("syslog.msg"));
}

void check_parsed_attributes(const core::FlowFile& flow_file, const ValidRFC5424Message& original_message, uint16_t port, std::string_view protocol) {
  CHECK(std::to_string(port) == flow_file.getAttribute("syslog.port"));
  CHECK(protocol == flow_file.getAttribute("syslog.protocol"));
  CHECK(ranges::contains(local_addresses, flow_file.getAttribute("syslog.sender")));

  CHECK("true" == flow_file.getAttribute("syslog.valid"));
  CHECK(original_message.priority_ == flow_file.getAttribute("syslog.priority"));
  CHECK(original_message.facility_ == flow_file.getAttribute("syslog.facility"));
  CHECK(original_message.severity_ == flow_file.getAttribute("syslog.severity"));
  CHECK(original_message.version_ == flow_file.getAttribute("syslog.version"));
  CHECK(original_message.timestamp_ == flow_file.getAttribute("syslog.timestamp"));
  CHECK(original_message.hostname_ == flow_file.getAttribute("syslog.hostname"));
  CHECK(original_message.app_name_ == flow_file.getAttribute("syslog.app_name"));
  CHECK(original_message.proc_id_ == flow_file.getAttribute("syslog.proc_id"));
  CHECK(original_message.msg_id_ == flow_file.getAttribute("syslog.msg_id"));
  CHECK(original_message.structured_data_ == flow_file.getAttribute("syslog.structured_data"));
  CHECK(original_message.msg_ == flow_file.getAttribute("syslog.msg"));
}

void check_parsed_attributes(const core::FlowFile& flow_file, const ValidRFC3164Message& original_message, uint16_t port, std::string_view protocol) {
  CHECK(std::to_string(port) == flow_file.getAttribute("syslog.port"));
  CHECK(protocol == flow_file.getAttribute("syslog.protocol"));
  CHECK(ranges::contains(local_addresses, flow_file.getAttribute("syslog.sender")));

  CHECK("true" == flow_file.getAttribute("syslog.valid"));
  CHECK(original_message.priority_ == flow_file.getAttribute("syslog.priority"));
  CHECK(original_message.facility_ == flow_file.getAttribute("syslog.facility"));
  CHECK(original_message.severity_ == flow_file.getAttribute("syslog.severity"));
  CHECK(original_message.timestamp_ == flow_file.getAttribute("syslog.timestamp"));
  CHECK(original_message.hostname_ == flow_file.getAttribute("syslog.hostname"));
  CHECK(original_message.msg_ == flow_file.getAttribute("syslog.msg"));
}

TEST_CASE("ListenSyslog without parsing test", "[ListenSyslog]") {
  const auto listen_syslog = std::make_shared<ListenSyslog>("ListenSyslog");

  SingleProcessorTestController controller{listen_syslog};
  LogTestController::getInstance().setTrace<ListenSyslog>();
  REQUIRE(listen_syslog->setProperty(ListenSyslog::MaxBatchSize, "2"));
  REQUIRE(listen_syslog->setProperty(ListenSyslog::ParseMessages, "false"));
  std::string protocol;
  uint16_t port = 0;

  SECTION("UDP") {
    REQUIRE(listen_syslog->setProperty(ListenSyslog::ProtocolProperty, "UDP"));
    protocol = "UDP";

    port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_syslog);

    asio::ip::udp::endpoint endpoint;
    SECTION("sending through IPv6", "[IPv6]") {
      if (utils::isIPv6Disabled())
        SKIP("IPv6 is disabled");
      endpoint = asio::ip::udp::endpoint(asio::ip::address_v6::loopback(), port);
    }
    SECTION("sending through IPv4", "[IPv4]") {
      endpoint = asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), port);
    }
    protocol = "UDP";
    CHECK_THAT(utils::sendUdpDatagram(rfc5424_logger_example_1, endpoint), MatchesSuccess());
    CHECK_THAT(utils::sendUdpDatagram(invalid_syslog, endpoint), MatchesSuccess());
  }

  SECTION("TCP") {
    REQUIRE(listen_syslog->setProperty(ListenSyslog::ProtocolProperty, "TCP"));
    protocol = "TCP";

    port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_syslog);

    asio::ip::tcp::endpoint endpoint;
    SECTION("sending through IPv6", "[IPv6]") {
      if (utils::isIPv6Disabled())
        SKIP("IPv6 is disabled");
      endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), port);
    }
    SECTION("sending through IPv4", "[IPv4]") {
      endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
    }
    CHECK_THAT(utils::sendMessagesViaTCP({rfc5424_logger_example_1}, endpoint, "\n"), MatchesSuccess());
    CHECK_THAT(utils::sendMessagesViaTCP({invalid_syslog}, endpoint, "\n"), MatchesSuccess());
  }
  std::unordered_map<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>> result;
  REQUIRE(controller.triggerUntil({{ListenSyslog::Success, 2}}, result, 300ms, 50ms));
  CHECK(controller.plan->getContent(result.at(ListenSyslog::Success)[0]) == rfc5424_logger_example_1);
  CHECK(controller.plan->getContent(result.at(ListenSyslog::Success)[1]) == invalid_syslog);

  check_for_only_basic_attributes(*result.at(ListenSyslog::Success)[0], port, protocol);
  check_for_only_basic_attributes(*result.at(ListenSyslog::Success)[1], port, protocol);
}

TEST_CASE("ListenSyslog with parsing test", "[ListenSyslog][NetworkListenerProcessor]") {
  const auto listen_syslog = std::make_shared<ListenSyslog>("ListenSyslog");

  SingleProcessorTestController controller{listen_syslog};
  LogTestController::getInstance().setTrace<ListenSyslog>();
  REQUIRE(listen_syslog->setProperty(ListenSyslog::MaxBatchSize, "100"));
  REQUIRE(listen_syslog->setProperty(ListenSyslog::ParseMessages, "true"));

  std::string protocol;
  uint16_t port = 0;
  SECTION("UDP") {
    REQUIRE(listen_syslog->setProperty(ListenSyslog::ProtocolProperty, "UDP"));
    protocol = "UDP";

    port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_syslog);

    asio::ip::udp::endpoint endpoint;
    SECTION("sending through IPv6", "[IPv6]") {
      if (utils::isIPv6Disabled())
        SKIP("IPv6 is disabled");
      endpoint = asio::ip::udp::endpoint(asio::ip::address_v6::loopback(), port);
    }
    SECTION("sending through IPv4", "[IPv4]") {
      endpoint = asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), port);
    }

    CHECK_THAT(utils::sendUdpDatagram(rfc5424_doc_example_1.unparsed_, endpoint), MatchesSuccess());
    CHECK_THAT(utils::sendUdpDatagram(rfc5424_doc_example_2.unparsed_, endpoint), MatchesSuccess());
    CHECK_THAT(utils::sendUdpDatagram(rfc5424_doc_example_3.unparsed_, endpoint), MatchesSuccess());
    CHECK_THAT(utils::sendUdpDatagram(rfc5424_doc_example_4.unparsed_, endpoint), MatchesSuccess());

    CHECK_THAT(utils::sendUdpDatagram(rfc3164_doc_example_1.unparsed_, endpoint), MatchesSuccess());
    CHECK_THAT(utils::sendUdpDatagram(rfc3164_doc_example_2.unparsed_, endpoint), MatchesSuccess());
    CHECK_THAT(utils::sendUdpDatagram(rfc3164_doc_example_3.unparsed_, endpoint), MatchesSuccess());
    CHECK_THAT(utils::sendUdpDatagram(rfc3164_doc_example_4.unparsed_, endpoint), MatchesSuccess());

    CHECK_THAT(utils::sendUdpDatagram(rfc5424_logger_example_1, endpoint), MatchesSuccess());
    CHECK_THAT(utils::sendUdpDatagram(invalid_syslog, endpoint), MatchesSuccess());
  }

  SECTION("TCP") {
    REQUIRE(listen_syslog->setProperty(ListenSyslog::ProtocolProperty, "TCP"));
    protocol = "TCP";

    port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_syslog);

    asio::ip::tcp::endpoint endpoint;
    SECTION("sending through IPv6", "[IPv6]") {
      if (utils::isIPv6Disabled())
        SKIP("IPv6 is disabled");
      endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), port);
    }
    SECTION("sending through IPv4", "[IPv4]") {
      endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
    }

    CHECK_THAT(utils::sendMessagesViaTCP({rfc5424_doc_example_1.unparsed_,
                                          rfc5424_doc_example_2.unparsed_,
                                          rfc5424_doc_example_3.unparsed_,
                                          rfc5424_doc_example_4.unparsed_}, endpoint, "\n"), MatchesSuccess());

    CHECK_THAT(utils::sendMessagesViaTCP({rfc3164_doc_example_1.unparsed_,
                                          rfc3164_doc_example_2.unparsed_,
                                          rfc3164_doc_example_3.unparsed_,
                                          rfc3164_doc_example_4.unparsed_}, endpoint, "\n"), MatchesSuccess());

    CHECK_THAT(utils::sendMessagesViaTCP({rfc5424_logger_example_1}, endpoint, "\n"), MatchesSuccess());
    CHECK_THAT(utils::sendMessagesViaTCP({invalid_syslog}, endpoint, "\n"), MatchesSuccess());
  }

  std::unordered_map<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>> result;
  REQUIRE(controller.triggerUntil({{ListenSyslog::Success, 9},
                                   {ListenSyslog::Invalid, 1}}, result, 300ms, 50ms));
  REQUIRE(result.at(ListenSyslog::Success).size() == 9);
  REQUIRE(result.at(ListenSyslog::Invalid).size() == 1);

  std::unordered_map<std::string, core::FlowFile&> success_flow_files;

  for (auto& flow_file: result.at(ListenSyslog::Success)) {
    success_flow_files.insert({controller.plan->getContent(flow_file), *flow_file});
  }

  REQUIRE(success_flow_files.contains(std::string(rfc5424_doc_example_1.unparsed_)));
  REQUIRE(success_flow_files.contains(std::string(rfc5424_doc_example_2.unparsed_)));
  REQUIRE(success_flow_files.contains(std::string(rfc5424_doc_example_3.unparsed_)));
  REQUIRE(success_flow_files.contains(std::string(rfc5424_doc_example_4.unparsed_)));

  REQUIRE(success_flow_files.contains(std::string(rfc3164_doc_example_1.unparsed_)));
  REQUIRE(success_flow_files.contains(std::string(rfc3164_doc_example_2.unparsed_)));
  REQUIRE(success_flow_files.contains(std::string(rfc3164_doc_example_3.unparsed_)));
  REQUIRE(success_flow_files.contains(std::string(rfc3164_doc_example_4.unparsed_)));

  check_parsed_attributes(success_flow_files.at(std::string(rfc5424_doc_example_1.unparsed_)), rfc5424_doc_example_1, port, protocol);
  check_parsed_attributes(success_flow_files.at(std::string(rfc5424_doc_example_2.unparsed_)), rfc5424_doc_example_2, port, protocol);
  check_parsed_attributes(success_flow_files.at(std::string(rfc5424_doc_example_3.unparsed_)), rfc5424_doc_example_3, port, protocol);
  check_parsed_attributes(success_flow_files.at(std::string(rfc5424_doc_example_4.unparsed_)), rfc5424_doc_example_4, port, protocol);

  check_parsed_attributes(success_flow_files.at(std::string(rfc3164_doc_example_1.unparsed_)), rfc3164_doc_example_1, port, protocol);
  check_parsed_attributes(success_flow_files.at(std::string(rfc3164_doc_example_2.unparsed_)), rfc3164_doc_example_2, port, protocol);
  check_parsed_attributes(success_flow_files.at(std::string(rfc3164_doc_example_3.unparsed_)), rfc3164_doc_example_3, port, protocol);
  check_parsed_attributes(success_flow_files.at(std::string(rfc3164_doc_example_4.unparsed_)), rfc3164_doc_example_4, port, protocol);

  REQUIRE(success_flow_files.contains(std::string(rfc5424_logger_example_1)));
  CHECK(controller.plan->getContent(result.at(ListenSyslog::Invalid)[0]) == invalid_syslog);
}

TEST_CASE("ListenSyslog can be rescheduled", "[ListenSyslog][NetworkListenerProcessor]") {
  const auto listen_syslog = std::make_shared<ListenSyslog>("ListenSyslog");
  SingleProcessorTestController controller{listen_syslog};
  LogTestController::getInstance().setTrace<ListenSyslog>();
  REQUIRE(listen_syslog->setProperty(ListenSyslog::Port, "0"));
  REQUIRE(listen_syslog->setProperty(ListenSyslog::MaxBatchSize, "100"));
  REQUIRE(listen_syslog->setProperty(ListenSyslog::ParseMessages, "true"));
  SECTION("UDP") {
    REQUIRE(listen_syslog->setProperty(ListenSyslog::ProtocolProperty, "UDP"));
    REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_syslog));
    REQUIRE_NOTHROW(controller.plan->reset(true));
    REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_syslog));
  }

  SECTION("TCP") {
    REQUIRE(listen_syslog->setProperty(ListenSyslog::ProtocolProperty, "TCP"));
    REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_syslog));
    REQUIRE_NOTHROW(controller.plan->reset(true));
    REQUIRE_NOTHROW(controller.plan->scheduleProcessor(listen_syslog));
  }
}

TEST_CASE("ListenSyslog max queue and max batch size test", "[ListenSyslog][NetworkListenerProcessor]") {
  const auto listen_syslog = std::make_shared<ListenSyslog>("ListenSyslog");

  SingleProcessorTestController controller{listen_syslog};
  REQUIRE(listen_syslog->setProperty(ListenSyslog::MaxBatchSize, "10"));
  REQUIRE(listen_syslog->setProperty(ListenSyslog::ParseMessages, "false"));
  REQUIRE(listen_syslog->setProperty(ListenSyslog::MaxQueueSize, "50"));

  LogTestController::getInstance().setWarn<ListenSyslog>();

  uint16_t port = 0;

  SECTION("UDP") {
    REQUIRE(listen_syslog->setProperty(ListenSyslog::ProtocolProperty, "UDP"));
    port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_syslog);

    asio::ip::udp::endpoint endpoint;
    SECTION("sending through IPv6", "[IPv6]") {
      if (utils::isIPv6Disabled())
        SKIP("IPv6 is disabled");
      endpoint = asio::ip::udp::endpoint(asio::ip::address_v6::loopback(), port);
    }
    SECTION("sending through IPv4", "[IPv4]") {
      endpoint = asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), port);
    }
    for (auto i = 0; i < 100; ++i) {
      CHECK_THAT(utils::sendUdpDatagram(rfc5424_doc_example_1.unparsed_, endpoint), MatchesSuccess());
    }
    CHECK(utils::countLogOccurrencesUntil("Queue is full. UDP message ignored.", 50, 300ms, 50ms));
  }

  SECTION("TCP") {
    REQUIRE(listen_syslog->setProperty(ListenSyslog::ProtocolProperty, "TCP"));
    port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_syslog);

    asio::ip::tcp::endpoint endpoint;
    SECTION("sending through IPv6", "[IPv6]") {
      if (utils::isIPv6Disabled())
        SKIP("IPv6 is disabled");
      endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), port);
    }

    SECTION("sending through IPv4", "[IPv4]") {
      endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
    }

    for (auto i = 0; i < 100; ++i) {
      CHECK_THAT(utils::sendMessagesViaTCP({rfc5424_doc_example_1.unparsed_}, endpoint, "\n"), MatchesSuccess());
    }
    CHECK(utils::countLogOccurrencesUntil("Queue is full. TCP message ignored.", 50, 300ms, 50ms));
  }
  CHECK(controller.trigger().at(ListenSyslog::Success).size() == 10);
  CHECK(controller.trigger().at(ListenSyslog::Success).size() == 10);
  CHECK(controller.trigger().at(ListenSyslog::Success).size() == 10);
  CHECK(controller.trigger().at(ListenSyslog::Success).size() == 10);
  CHECK(controller.trigger().at(ListenSyslog::Success).size() == 10);
  CHECK(controller.trigger().at(ListenSyslog::Success).empty());
}

TEST_CASE("Test ListenSyslog via TCP with SSL connection", "[ListenSyslog][NetworkListenerProcessor]") {
  const auto listen_syslog = std::make_shared<ListenSyslog>("ListenSyslog");
  SingleProcessorTestController controller{listen_syslog};

  auto ssl_context_service = controller.plan->addController("SSLContextService", "SSLContextService");
  const auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextServiceImpl::CACertificate, (executable_dir / "resources" / "ca_A.crt").string()));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextServiceImpl::ClientCertificate, (executable_dir / "resources" / "localhost_by_A.pem").string()));
  REQUIRE(controller.plan->setProperty(ssl_context_service, controllers::SSLContextServiceImpl::PrivateKey, (executable_dir / "resources" / "localhost.key").string()));
  ssl_context_service->enable();

  LogTestController::getInstance().setTrace<ListenSyslog>();
  REQUIRE(listen_syslog->setProperty(ListenSyslog::MaxBatchSize, "2"));
  REQUIRE(listen_syslog->setProperty(ListenSyslog::ParseMessages, "false"));
  REQUIRE(listen_syslog->setProperty(ListenSyslog::ProtocolProperty, "TCP"));
  REQUIRE(listen_syslog->setProperty(ListenSyslog::SSLContextService, "SSLContextService"));

  auto port = utils::scheduleProcessorOnRandomPort(controller.plan, listen_syslog);

  asio::ip::tcp::endpoint endpoint;
  SECTION("sending through IPv6", "[IPv6]") {
    if (utils::isIPv6Disabled())
      SKIP("IPv6 is disabled");
    endpoint = asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), port);
  }
  SECTION("sending through IPv4", "[IPv4]") {
    endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
  }

  CHECK_THAT(utils::sendMessagesViaSSL({rfc5424_logger_example_1}, endpoint, executable_dir / "resources" / "ca_A.crt"), MatchesSuccess());
  CHECK_THAT(utils::sendMessagesViaSSL({invalid_syslog}, endpoint, executable_dir / "resources" / "ca_A.crt"), MatchesSuccess());

  std::unordered_map<core::Relationship, std::vector<std::shared_ptr<core::FlowFile>>> result;
  REQUIRE(controller.triggerUntil({{ListenSyslog::Success, 2}}, result, 300ms, 50ms));
  CHECK(controller.plan->getContent(result.at(ListenSyslog::Success)[0]) == rfc5424_logger_example_1);
  CHECK(controller.plan->getContent(result.at(ListenSyslog::Success)[1]) == invalid_syslog);

  check_for_only_basic_attributes(*result.at(ListenSyslog::Success)[0], port, "TCP");
  check_for_only_basic_attributes(*result.at(ListenSyslog::Success)[1], port, "TCP");
}

}  // namespace org::apache::nifi::minifi::test
