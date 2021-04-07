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

#include "TestBase.h"
#include "YamlConfiguration.h"
#include "Utils.h"
#include "IntegrationTestUtils.h"
#include "ProcessGroupTestUtils.h"

static core::YamlConfiguration config(nullptr, nullptr, nullptr, nullptr, std::make_shared<minifi::Configure>());

TEST_CASE("Root process group is correctly parsed", "[YamlProcessGroupParser1]") {
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

TEST_CASE("Nested process group is correctly parsed", "[YamlProcessGroupParser2]") {
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

TEST_CASE("Cannot connect processors from different groups", "[YamlProcessGroupParser3]") {
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
    Conn1.source = UnresolvedProc{Child1_Proc1.id};
    Conn1.destination = UnresolvedProc{Child1_Port1.id};

    Child1_Conn1.source = UnresolvedProc{Proc1.id};
    Child1_Conn1.destination = UnresolvedProc{Port1.id};
  }

  SECTION("Connecting processors between their own and their child/parent group") {
    Conn1.source = Proc1;
    Conn1.destination = UnresolvedProc{Child1_Port1.id};

    Child1_Conn1.source = UnresolvedProc{Port1.id};
    Child1_Conn1.destination = Child1_Proc1;
  }

  SECTION("Connecting processors in a sibling group") {
    Conn1.source = Proc1;
    Conn1.destination = Port1;

    Child1_Conn1.source = UnresolvedProc{Child2_Proc1.id};
    Child1_Conn1.destination = UnresolvedProc{Child2_Port1.id};
  }

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}
