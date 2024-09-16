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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "YamlConfiguration.h"
#include "unit/TestUtils.h"
#include "ProcessGroupTestUtils.h"

static core::YamlConfiguration config{core::ConfigurationContext{
    .flow_file_repo = nullptr,
    .content_repo = nullptr,
    .configuration = std::make_shared<minifi::ConfigureImpl>(),
    .path = "",
    .filesystem = std::make_shared<utils::file::FileSystem>(),
    .sensitive_values_encryptor = utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{utils::crypto::XSalsa20Cipher::generateKey()}}
}};

TEST_CASE("Root process group is correctly parsed", "[YamlProcessGroupParser]") {
  auto pattern = Group("root")
    .With({
      Conn{"Conn1",
           Proc{"00000000-0000-0000-0000-000000000001", "Proc1"},
           Proc{"00000000-0000-0000-0000-000000000002", "Proc2"}},
      Conn{"Conn2",
           Proc{"00000000-0000-0000-0000-000000000002", "Proc2"},
           Proc{"00000000-0000-0000-0000-000000000005", "Port1"}}
    }).With({
      Proc{"00000000-0000-0000-0000-000000000001", "Proc1"},
      Proc{"00000000-0000-0000-0000-000000000002", "Proc2"}
    }).With({
      RPG{"RPG1", {
        Proc{"00000000-0000-0000-0000-000000000005", "Port1"},
        Proc{"00000000-0000-0000-0000-000000000006", "Port2"}
      }},
      RPG{"RPG2", {}}
    });

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));
  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Nested process group is correctly parsed", "[YamlProcessGroupParser]") {
  auto pattern = Group("root")
    .With({Conn{"Conn1",
                Proc{"00000000-0000-0000-0000-000000000001", "Proc1"},
                Proc{"00000000-0000-0000-0000-000000000002", "Proc2"}}})
    .With({Proc{"00000000-0000-0000-0000-000000000001", "Proc1"},
           Proc{"00000000-0000-0000-0000-000000000002", "Proc2"}})
    .With({
      Group("Child1")
      .With({Conn{"Child1_Conn1",
                  Proc{"00000000-0000-0000-0000-000000000005", "Port1"},
                  Proc{"00000000-0000-0000-0000-000000000007", "Child1_Proc2"}}})
      .With({Proc{"00000000-0000-0000-0000-000000000006", "Child1_Proc1"},
             Proc{"00000000-0000-0000-0000-000000000007", "Child1_Proc2"}})
      .With({RPG{"Child1_RPG1", {
        Proc{"00000000-0000-0000-0000-000000000005", "Port1"}}}})
    });

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Cannot connect processors from different groups", "[YamlProcessGroupParser]") {
  TestController controller;
  LogTestController::getInstance().setTrace<core::YamlConfiguration>();
  Proc Proc1{"00000000-0000-0000-0000-000000000001", "Proc1"};
  Proc Port1{"00000000-0000-0000-0000-000000000002", "Port1"};
  Proc Child1_Proc1{"00000000-0000-0000-0000-000000000011", "Child1_Proc1"};
  Proc Child1_Port1{"00000000-0000-0000-0000-000000000012", "Child1_Port1"};
  Proc Child2_Proc1{"00000000-0000-0000-0000-000000000021", "Child2_Proc1"};
  Proc Child2_Port1{"00000000-0000-0000-0000-000000000022", "Child2_Port1"};

  auto pattern = Group("root")
    .With({Proc1})
    .With({Conn{"Conn1", Proc1, Port1}})
    .With({RPG{"RPG1", {Port1}}})
    .With({
      Group("Child1").With({Child1_Proc1})
      .With({Conn{"Child1_Conn1", Child1_Proc1, Child1_Port1}})
      .With({RPG{"Child1_RPG1", {Child1_Port1}}}),
      Group("Child2")
      .With({Child2_Proc1})
      .With({RPG{"Child2_RPG1", {Child2_Port1}}})
    });

  auto& Conn1 = pattern.connections_.at(0);
  auto& Child1_Conn1 = pattern.subgroups_.at(0).connections_.at(0);

  SECTION("Connecting processors in their own groups") {
    // sanity check, everything is resolved as it should
  }

  SECTION("Connecting processors in their child/parent group") {
    Conn1.source = Proc{Child1_Proc1.id, Child1_Proc1.name, ConnectionFailure::UNRESOLVED_SOURCE};
    Conn1.destination = Proc{Child1_Port1.id, Child1_Port1.name, ConnectionFailure::UNRESOLVED_DESTINATION};

    Child1_Conn1.source = Proc{Proc1.id, Proc1.name, ConnectionFailure::UNRESOLVED_SOURCE};
    Child1_Conn1.destination = Proc{Port1.id, Proc1.name, ConnectionFailure::UNRESOLVED_DESTINATION};
  }

  SECTION("Connecting processors between their own and their child/parent group") {
    Conn1.source = Proc1;
    Conn1.destination = Proc{Child1_Port1.id, Child1_Port1.name, ConnectionFailure::UNRESOLVED_DESTINATION};

    Child1_Conn1.source = Proc{Port1.id, Port1.name, ConnectionFailure::UNRESOLVED_SOURCE};
    Child1_Conn1.destination = Child1_Proc1;
  }

  SECTION("Connecting processors in a sibling group") {
    Conn1.source = Proc1;
    Conn1.destination = Port1;

    Child1_Conn1.source = Proc{Child2_Proc1.id, Child2_Proc1.name, ConnectionFailure::UNRESOLVED_SOURCE};
    Child1_Conn1.destination = Proc{Child2_Port1.id, Child2_Port1.name, ConnectionFailure::UNRESOLVED_DESTINATION};
  }

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Processor can communicate with child process group's input port", "[YamlProcessGroupParser]") {
  auto pattern = Group("root")
    .With({Conn{"Conn1",
                Proc{"00000000-0000-0000-0000-000000000001", "Proc1"},
                InputPort{"00000000-0000-0000-0000-000000000002", "Port1"}}})
    .With({Proc{"00000000-0000-0000-0000-000000000001", "Proc1"}})
    .With({
      Group("Child1")
      .With({InputPort{"00000000-0000-0000-0000-000000000002", "Port1"}})
    });

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Child process group can provide input for root processor through output port", "[YamlProcessGroupParser]") {
  auto pattern = Group("root")
    .With({Conn{"Conn1",
                OutputPort{"00000000-0000-0000-0000-000000000002", "Port1"},
                Proc{"00000000-0000-0000-0000-000000000001", "Proc1"}}})
    .With({Proc{"00000000-0000-0000-0000-000000000001", "Proc1"}})
    .With({
      Group("Child1")
      .With({OutputPort{"00000000-0000-0000-0000-000000000002", "Port1"}})
    });

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Child process groups can communicate through ports", "[YamlProcessGroupParser]") {
  auto pattern = Group("root")
    .With({Conn{"Conn1",
                OutputPort{"00000000-0000-0000-0000-000000000002", "Port1"},
                InputPort{"00000000-0000-0000-0000-000000000003", "Port2"}}})
    .With({Proc{"00000000-0000-0000-0000-000000000001", "Proc1"}})
    .With({
      Group("Child1")
      .With({OutputPort{"00000000-0000-0000-0000-000000000002", "Port1"}}),
      Group("Child2")
      .With({InputPort{"00000000-0000-0000-0000-000000000003", "Port2"}})
    });

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Processor cannot communicate with child's nested process group", "[YamlProcessGroupParser]") {
  Proc Proc1{"00000000-0000-0000-0000-000000000001", "Proc1"};
  OutputPort Port1{"00000000-0000-0000-0000-000000000002", "Port1"};
  InputPort Port2{"00000000-0000-0000-0000-000000000003", "Port2", ConnectionFailure::UNRESOLVED_DESTINATION};

  auto pattern = Group("root")
    .With({Conn{"Conn1",
                Proc1,
                Port2}})
    .With({Proc1})
    .With({
      Group("Child1")
      .With({Port1})
      .With({Group("Child2")
            .With({Port2})})
    });

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Input port can be a connection's source and the output port can be a destination inside the process group", "[YamlProcessGroupParser7]") {
  auto pattern = Group("root")
    .With({Conn{"Conn1",
                InputPort{"00000000-0000-0000-0000-000000000001", "Port1"},
                OutputPort{"00000000-0000-0000-0000-000000000002", "Port2"}}})
    .With({InputPort{"00000000-0000-0000-0000-000000000001", "Port1"}})
    .With({OutputPort{"00000000-0000-0000-0000-000000000002", "Port2"}});

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Input port cannot be a connection's destination inside the process group", "[YamlProcessGroupParser]") {
  auto pattern = Group("root")
    .With({Conn{"Conn1",
                Proc{"00000000-0000-0000-0000-000000000002", "Proc1"},
                InputPort{"00000000-0000-0000-0000-000000000001", "Port1", ConnectionFailure::INPUT_CANNOT_BE_DESTINATION}}})
    .With({InputPort{"00000000-0000-0000-0000-000000000001", "Port1"}})
    .With({Proc{"00000000-0000-0000-0000-000000000002", "Proc1"}});

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Output port cannot be a connection's source inside the process group", "[YamlProcessGroupParser]") {
  auto pattern = Group("root")
    .With({Conn{"Conn1",
                OutputPort{"00000000-0000-0000-0000-000000000001", "Port1", ConnectionFailure::OUTPUT_CANNOT_BE_SOURCE},
                Proc{"00000000-0000-0000-0000-000000000002", "Proc1"}}})
    .With({OutputPort{"00000000-0000-0000-0000-000000000001", "Port1"}})
    .With({Proc{"00000000-0000-0000-0000-000000000002", "Proc1"}});

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Input port can be a connection's source and the output port can be a destination inside the process group through processor", "[YamlProcessGroupParser]") {
  auto pattern = Group("root")
    .With({Conn{"Conn1",
                InputPort{"00000000-0000-0000-0000-000000000001", "Port1"},
                Proc{"00000000-0000-0000-0000-000000000003", "Proc1"}},
           Conn{"Conn2",
                Proc{"00000000-0000-0000-0000-000000000003", "Proc1"},
                OutputPort{"00000000-0000-0000-0000-000000000002", "Port2"}}})
    .With({Proc{"00000000-0000-0000-0000-000000000003", "Proc1"}})
    .With({InputPort{"00000000-0000-0000-0000-000000000001", "Port1"}})
    .With({OutputPort{"00000000-0000-0000-0000-000000000002", "Port2"}});

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Processor cannot set connection's destination to child process group's output port", "[YamlProcessGroupParser]") {
  auto pattern = Group("root")
    .With({Conn{"Conn1",
                Proc{"00000000-0000-0000-0000-000000000001", "Proc1"},
                OutputPort{"00000000-0000-0000-0000-000000000002", "Port1", ConnectionFailure::OUTPUT_CANNOT_BE_DESTINATION}}})
    .With({Proc{"00000000-0000-0000-0000-000000000001", "Proc1"}})
    .With({
      Group("Child1")
      .With({OutputPort{"00000000-0000-0000-0000-000000000002", "Port1"}})
    });

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Processor cannot set connection's source to child process group's input port", "[YamlProcessGroupParser]") {
  auto pattern = Group("root")
    .With({Conn{"Conn1",
                InputPort{"00000000-0000-0000-0000-000000000002", "Port1", ConnectionFailure::INPUT_CANNOT_BE_SOURCE},
                Proc{"00000000-0000-0000-0000-000000000001", "Proc1"}}})
    .With({Proc{"00000000-0000-0000-0000-000000000001", "Proc1"}})
    .With({
      Group("Child1")
      .With({InputPort{"00000000-0000-0000-0000-000000000002", "Port1"}})
    });

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}
