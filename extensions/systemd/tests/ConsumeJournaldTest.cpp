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

#include <cerrno>
#include <memory>
#include <string>
#include <vector>

#include "TestBase.h"
#include "ConsumeJournald.h"
#include "libwrapper/LibWrapper.h"
#include "utils/GeneralUtils.h"
#include "utils/gsl.h"
#include "utils/StringUtils.h"
#include "Utils.h"

namespace minifi = org::apache::nifi::minifi;
namespace utils = minifi::utils;
namespace systemd = minifi::extensions::systemd;
namespace libwrapper = systemd::libwrapper;
using systemd::JournalType;
using systemd::ConsumeJournald;

namespace {
namespace gsl = minifi::gsl;
struct JournalEntry final {
  JournalEntry(const char* const identifier, const char* const message, const int pid = 0, std::vector<std::string> extra_fields = {}, const char* const hostname = "test-pc")
    :fields{std::move(extra_fields)}
  {
    fields.reserve(fields.size() + 4);
    fields.push_back(utils::StringUtils::join_pack("MESSAGE=", message));
    fields.push_back(utils::StringUtils::join_pack("SYSLOG_IDENTIFIER=", identifier));
    if (pid != 0) {
      // The intention of the long expression below is a simple pseudo-random to test both branches equally
      // without having to pull in complex random logic
      const char* const pid_key =
          (int{message[0]} + int{identifier[0]} + static_cast<int>(extra_fields.size()) + pid + int{hostname[0]}) % 2 == 0 ? "_PID" : "SYSLOG_PID";
      fields.push_back(utils::StringUtils::join_pack(pid_key, "=", std::to_string(pid)));
    }
    fields.push_back(utils::StringUtils::join_pack("_HOSTNAME=", hostname));
  }

  std::vector<std::string> fields;  // in KEY=VALUE format, like systemd
};
struct TestJournal final : libwrapper::Journal {
  explicit TestJournal(std::vector<JournalEntry>& journal)
      :journal{&journal}
  { }

  int seekHead() noexcept override {
    cursor = 0;
    consumed = -1;
    field_id = 0;
    return 0;
  }

  int seekTail() noexcept override {
    cursor = journal->size();
    consumed = gsl::narrow<ssize_t>(journal->size() - 1);
    field_id = 0;
    return 0;
  }

  int seekCursor(const char* const cur) noexcept override {
    try {
      cursor = gsl::narrow<size_t>(std::stoll(cur) + 1);
      consumed = gsl::narrow<ssize_t>(cursor - 1);
    } catch (const std::invalid_argument&) {
      return -EINVAL;
    } catch (const std::out_of_range&) {
      return -ERANGE;
    }
    field_id = 0;
    return 0;
  }

  int getCursor(gsl::owner<char*>* const cursor_out) noexcept override {
    *cursor_out = strdup(std::to_string(consumed).c_str());
    return *cursor_out ? 0 : -ENOMEM;
  }

  int next() noexcept override {
    cursor = gsl::narrow<size_t>(consumed + 1);
    field_id = 0;
    if (cursor >= journal->size()) return 0;
    return 1;
  }

  int enumerateData(const void** const data_out, size_t* const size_out) noexcept override {
    if (cursor >= journal->size()) {
      cursor = gsl::narrow<size_t>(consumed + 1);
      return -EADDRNOTAVAIL;
    }
    if (field_id >= (*journal)[cursor].fields.size()) return 0;
    const auto result = gsl::narrow<int>((*journal)[cursor].fields.size() - field_id);
    *data_out = (*journal)[cursor].fields[field_id].c_str();
    *size_out = (*journal)[cursor].fields[field_id].size();
    consumed = gsl::narrow<ssize_t>(cursor);
    ++field_id;
    return result;
  }

  int getRealtimeUsec(uint64_t* const usec_out) noexcept override {
    constexpr auto _20210415171703 = 1618507023000000;
    constexpr auto usec_per_sec = 1000000;
    *usec_out = _20210415171703 + cursor * usec_per_sec + 123456;
    return 0;
  }

  size_t cursor = 0;
  ssize_t consumed = -1;
  size_t field_id = 0;
  gsl::not_null<std::vector<JournalEntry>*> journal;
};
struct TestLibWrapper final : libwrapper::LibWrapper {
  explicit TestLibWrapper(std::vector<JournalEntry> journal)
      :journal{std::move(journal)}
  { }

  std::unique_ptr<libwrapper::Journal> openJournal(JournalType) override {
    return utils::make_unique<TestJournal>(journal);
  }

  std::vector<JournalEntry> journal;
};
}  // namespace

namespace org { namespace apache { namespace nifi { namespace minifi { namespace extensions { namespace systemd {
struct ConsumeJournaldTestAccessor {
  FIELD_ACCESSOR(state_manager_);
};
}}}}}}  // namespace org::apache::nifi::minifi::extensions::systemd
using org::apache::nifi::minifi::extensions::systemd::ConsumeJournaldTestAccessor;

TEST_CASE("ConsumeJournald", "[consumejournald]") {
  TestController test_controller;
  LogTestController::getInstance().setTrace<ConsumeJournald>();
  const auto plan = test_controller.createPlan();
  auto libwrapper = utils::make_unique<TestLibWrapper>(TestLibWrapper{{
      {"kernel", "Linux version 5.10.12-gentoo-x86_64 (root@test-pc.test.local) (x86_64-pc-linux-gnu-gcc (Gentoo 10.2.0-r5 p6) 10.2.0, GNU ld (Gentoo 2.35.2 p1) 2.35.2) #1 SMP Sat Feb 20 03:13:45 CET 2021"},  // NOLINT
      {"kernel", "NX (Execute Disable) protection: active"},
      {"kernel", "ACPI: Local APIC address 0xfee00000"},
      {"kernel", "HugeTLB registered 1.00 GiB page size, pre-allocated 0 pages"},
      {"kernel", "SCSI subsystem initialized"},
      {"systemd", "Starting Rule-based Manager for Device Events and Files...", 1},
  }});
  auto* const libwrapper_observer = libwrapper.get();
  const auto consume_journald = plan->addProcessor(std::make_shared<ConsumeJournald>("ConsumeJournald", utils::Identifier{},
      std::move(libwrapper)), "ConsumeJournald");
  REQUIRE(consume_journald->setProperty(ConsumeJournald::TimestampFormat, "ISO8601"));
  const auto get_cursor_position = [&consume_journald]() -> std::string {
    return ConsumeJournaldTestAccessor::get_state_manager_(dynamic_cast<ConsumeJournald&>(*consume_journald))->get()->at("cursor");
  };

  SECTION("defaults") {
    // first run: seeks to the end, no flow files are created. Yields. Can't check cursor position, because it's only set during message consumption.
    plan->runNextProcessor();
    REQUIRE(nullptr == plan->getFlowFileProducedByCurrentProcessor());  // ConsumeJournald seeks to tail by default, therefore no flow files are produced
    REQUIRE(consume_journald->isYield());

    // add a flow file, check the content
    libwrapper_observer->journal.emplace_back("systemd", "Mounted /boot.", 1);
    plan->runCurrentProcessor();
    REQUIRE("6" == get_cursor_position());
    REQUIRE("2021-04-15T17:17:09.123456+00:00 test-pc systemd[1]: Mounted /boot." == plan->getContent(plan->getCurrentFlowFile()));

    // add two new messages, expect two new flow files
    libwrapper_observer->journal.emplace_back("dbus-daemon", "[system] Successfully activated service 'org.freedesktop.UPower'", 2200);
    libwrapper_observer->journal.emplace_back("NetworkManager", "<info>  [1618507047.7278] manager: (virbr0): new Bridge device (/org/freedesktop/NetworkManager/Devices/5)", 2201);
    plan->runCurrentProcessor();
    REQUIRE(2 == plan->getNumFlowFileProducedByCurrentProcessor());
    REQUIRE("8" == get_cursor_position());
    const auto content = plan->getContent(plan->getCurrentFlowFile());
    REQUIRE(!content.empty());
  }
  SECTION("Raw format, one-by-one") {
    REQUIRE(consume_journald->setProperty(ConsumeJournald::BatchSize, "1"));
    REQUIRE(consume_journald->setProperty(ConsumeJournald::PayloadFormat, "Raw"));
    REQUIRE(consume_journald->setProperty(ConsumeJournald::ProcessOldMessages, "true"));
    {
      plan->runNextProcessor();
      REQUIRE("0" == get_cursor_position());
      REQUIRE(!consume_journald->isYield());
      const auto flowfile = plan->getCurrentFlowFile();
      const auto content = plan->getContent(flowfile);
      REQUIRE("Linux version 5.10.12-gentoo-x86_64 (root@test-pc.test.local) (x86_64-pc-linux-gnu-gcc (Gentoo 10.2.0-r5 p6) 10.2.0, GNU ld (Gentoo 2.35.2 p1) 2.35.2) #1 SMP Sat Feb 20 03:13:45 CET 2021"  // NOLINT
          == content);
      REQUIRE("2021-04-15T17:17:03.123456+00:00" == flowfile->getAttribute("timestamp").value_or("n/a"));
    }
    {
      plan->runCurrentProcessor();
      REQUIRE("1" == get_cursor_position());
      const auto content = plan->getContent(plan->getCurrentFlowFile());
      REQUIRE("NX (Execute Disable) protection: active" == content);
      REQUIRE("2021-04-15T17:17:04.123456+00:00" == plan->getCurrentFlowFile()->getAttribute("timestamp").value_or("n/a"));
    }

    plan->runCurrentProcessor();
    REQUIRE("2" == get_cursor_position());
    REQUIRE("ACPI: Local APIC address 0xfee00000" == plan->getContent(plan->getCurrentFlowFile()));

    plan->runCurrentProcessor();
    REQUIRE("HugeTLB registered 1.00 GiB page size, pre-allocated 0 pages" == plan->getContent(plan->getCurrentFlowFile()));

    plan->runCurrentProcessor();
    REQUIRE("SCSI subsystem initialized" == plan->getContent(plan->getCurrentFlowFile()));

    plan->runCurrentProcessor();
    REQUIRE("Starting Rule-based Manager for Device Events and Files..." == plan->getContent(plan->getCurrentFlowFile()));
    REQUIRE("5" == get_cursor_position());

    plan->runCurrentProcessor();
    REQUIRE(nullptr == plan->getCurrentFlowFile());
    REQUIRE("5" == get_cursor_position());

    {
      // add a flow file, check the content and the timestamp
      libwrapper_observer->journal.emplace_back("systemd", "Mounted /boot.", 1);
      plan->runCurrentProcessor();
      const auto flowfile = plan->getCurrentFlowFile();
      const auto content = plan->getContent(flowfile);
      REQUIRE("6" == get_cursor_position());
      REQUIRE("2021-04-15T17:17:09.123456+00:00" == flowfile->getAttribute("timestamp"));
      REQUIRE("Mounted /boot." == content);
      REQUIRE("test-pc" == flowfile->getAttribute("_HOSTNAME").value_or("n/a"));
      REQUIRE("systemd" == flowfile->getAttribute("SYSLOG_IDENTIFIER").value_or("n/a"));
      const auto pid = (flowfile->getAttribute("_PID") | utils::orElse([&] { return flowfile->getAttribute("SYSLOG_PID"); })).value_or("n/a");
      REQUIRE("1" == pid);
    }

    plan->runCurrentProcessor();
    REQUIRE(nullptr == plan->getCurrentFlowFile());
    REQUIRE("6" == get_cursor_position());
  }
  SECTION("Include Timestamp is honored") {
    REQUIRE(consume_journald->setProperty(ConsumeJournald::BatchSize, "1"));
    REQUIRE(consume_journald->setProperty(ConsumeJournald::IncludeTimestamp, "false"));
    plan->runNextProcessor();  // first run: seeks to the end, no flow files are created. Yields.
    REQUIRE(nullptr == plan->getCurrentFlowFile());

    // add a flow file, ensure that no timestamp is added to the attributes
    libwrapper_observer->journal.emplace_back("systemd", "Mounted /boot.", 1);
    plan->runCurrentProcessor();
    REQUIRE("2021-04-15T17:17:09.123456+00:00 test-pc systemd[1]: Mounted /boot." == plan->getContent(plan->getCurrentFlowFile()));
    REQUIRE(!plan->getCurrentFlowFile()->getAttribute("timestamp").has_value());
  }
  SECTION("Batch Size is honored") {
    REQUIRE(consume_journald->setProperty(ConsumeJournald::BatchSize, "3"));
    REQUIRE(consume_journald->setProperty(ConsumeJournald::ProcessOldMessages, "true"));
    libwrapper_observer->journal.emplace_back("systemd", "Mounted /boot.", 1);
    libwrapper_observer->journal.emplace_back("dbus-daemon", "[system] Successfully activated service 'org.freedesktop.UPower'", 2200);

    plan->runNextProcessor();
    REQUIRE(3 == plan->getNumFlowFileProducedByCurrentProcessor());

    plan->runCurrentProcessor();
    REQUIRE(6 == plan->getNumFlowFileProducedByCurrentProcessor());

    plan->runCurrentProcessor();
    REQUIRE(8 == plan->getNumFlowFileProducedByCurrentProcessor());
    REQUIRE(!consume_journald->isYield());

    plan->runCurrentProcessor();
    REQUIRE(8 == plan->getNumFlowFileProducedByCurrentProcessor());
    REQUIRE(consume_journald->isYield());
  }
  SECTION("throw on invalid batch size") {
    REQUIRE_THROWS(consume_journald->setProperty(ConsumeJournald::BatchSize, "asdf"));
  }
  SECTION("throw on invalid payload format") {
    REQUIRE(consume_journald->setProperty(ConsumeJournald::PayloadFormat, "raw"));  // case-sensitive
    REQUIRE_THROWS(plan->scheduleProcessor(consume_journald));
  }
  SECTION("throw on invalid journal type") {
    REQUIRE(consume_journald->setProperty(ConsumeJournald::JournalType, "hello"));
    REQUIRE_THROWS(plan->scheduleProcessor(consume_journald));
  }
  SECTION("throw on invalid journal type") {
    REQUIRE(consume_journald->setProperty(ConsumeJournald::JournalType, "hello"));
    REQUIRE_THROWS(plan->scheduleProcessor(consume_journald));
  }
}
