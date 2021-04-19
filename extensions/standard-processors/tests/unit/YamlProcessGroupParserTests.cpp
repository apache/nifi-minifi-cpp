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
           MaybeProc{"00000000-0000-0000-0000-000000000001", "Proc1"},
           MaybeProc{"00000000-0000-0000-0000-000000000002", "Proc2"}},
      Conn{"Conn2",
           MaybeProc{"00000000-0000-0000-0000-000000000002", "Proc2"},
           MaybeProc{"00000000-0000-0000-0000-000000000005", "Port1"}}
    }).With({
      Proc{"Proc1", "00000000-0000-0000-0000-000000000001"},
      Proc{"Proc2", "00000000-0000-0000-0000-000000000002"}
    }).With({
      RPG{"RPG1", {
        Proc{"Port1", "00000000-0000-0000-0000-000000000005"},
        Proc{"Port2", "00000000-0000-0000-0000-000000000006"}
      }},
      RPG{"RPG2", {}}
    });

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));
  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Nested process group is correctly parsed", "[YamlProcessGroupParser2]") {
  auto pattern = Group("root")
    .With({Conn{"Conn1",
                MaybeProc{"00000000-0000-0000-0000-000000000001", "Proc1"},
                MaybeProc{"00000000-0000-0000-0000-000000000002", "Proc2"}}})
    .With({Proc{"Proc1", "00000000-0000-0000-0000-000000000001"},
           Proc{"Proc2", "00000000-0000-0000-0000-000000000002"}})
    .With({
      Group("Child1")
      .With({Conn{"Child1_Conn1",
                  MaybeProc{"00000000-0000-0000-0000-000000000005", "Port1"},
                  MaybeProc{"00000000-0000-0000-0000-000000000007", "Child1_Proc2"}}})
      .With({Proc{"Child1_Proc1", "00000000-0000-0000-0000-000000000006"},
             Proc{"Child1_Proc2", "00000000-0000-0000-0000-000000000007"}})
      .With({RPG{"Child1_RPG1", {
        Proc{"Port1", "00000000-0000-0000-0000-000000000005"}}}})
    });

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}

TEST_CASE("Cannot connect processors from different groups", "[YamlProcessGroupParser3]") {
  TestController controller;
  LogTestController::getInstance().setTrace<core::YamlConfiguration>();
  Proc Proc1{"Proc1", "00000000-0000-0000-0000-000000000001"};
  Proc Port1{"Port1", "00000000-0000-0000-0000-000000000002"};
  Proc Child1_Proc1{"Child1_Proc1", "00000000-0000-0000-0000-000000000011"};
  Proc Child1_Port1{"Child1_Port1", "00000000-0000-0000-0000-000000000012"};
  Proc Child2_Proc1{"Child2_Proc1", "00000000-0000-0000-0000-000000000021"};
  Proc Child2_Port1{"Child2_Port1", "00000000-0000-0000-0000-000000000022"};

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
    Conn1.source = MaybeProc{Child1_Proc1.id, utils::nullopt};
    Conn1.destination = MaybeProc{Child1_Port1.id, utils::nullopt};

    Child1_Conn1.source = MaybeProc{Proc1.id, utils::nullopt};
    Child1_Conn1.destination = MaybeProc{Port1.id, utils::nullopt};
  }

  SECTION("Connecting processors between their own and their child/parent group") {
    Conn1.source = Proc1;
    Conn1.destination = MaybeProc{Child1_Port1.id, utils::nullopt};

    Child1_Conn1.source = MaybeProc{Port1.id, utils::nullopt};
    Child1_Conn1.destination = Child1_Proc1;
  }

  SECTION("Connecting processors in a sibling group") {
    Conn1.source = Proc1;
    Conn1.destination = Port1;

    Child1_Conn1.source = MaybeProc{Child2_Proc1.id, utils::nullopt};
    Child1_Conn1.destination = MaybeProc{Child2_Port1.id, utils::nullopt};
  }

  auto root = config.getRootFromPayload(pattern.serialize().join("\n"));

  verifyProcessGroup(*root, pattern);
}
