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

#pragma once

#include "TestBase.h"
#include "YamlConfiguration.h"
#include "Utils.h"

struct Lines {
  std::vector<std::string> lines;

  std::string join(const std::string& delim) const {
    std::string result;
    bool first = true;
    for (const auto& line : lines) {
      if (!first) result += delim;
      first = false;
      result += line;
    }
    return result;
  }

  Lines& indentAll() & {
    for (auto& line : lines) {
      line = "  " + std::move(line);
    }
    return *this;
  }

  Lines&& indentAll() && {
    for (auto& line : lines) {
      line = "  " + std::move(line);
    }
    return std::move(*this);
  }

  Lines& append(Lines more_lines) {
    std::move(more_lines.lines.begin(), more_lines.lines.end(), std::back_inserter(lines));
    return *this;
  }

  Lines& emplace_back(std::string line) {
    lines.emplace_back(std::move(line));
    return *this;
  }
};

struct Proc {
  std::string name;
  std::string id;

  Lines serialize() const {
    return {{
      "- id: " + id,
      "  name: " + name,
      "  class: LogAttribute"
    }};
  }
};

struct MaybeProc {
  MaybeProc(const Proc& proc): id(proc.id), name(proc.name) {}
  MaybeProc(std::string id, utils::optional<std::string> name) : id(std::move(id)), name(std::move(name)) {}

  std::string id;
  utils::optional<std::string> name;
};

struct Conn {
  std::string name;
  MaybeProc source;
  MaybeProc destination;

  Lines serialize() const {
    return {{
      "- name: " + name,
      "  source id: " + source.id,
      "  destination id: " + destination.id,
      "  source relationship name: success"
    }};
  }
};

struct RPG {
  std::string name;
  std::vector<Proc> input_ports;

  Lines serialize() const {
    std::vector<std::string> lines;
    lines.emplace_back("- name: " + name);
    if (input_ports.empty()) {
      lines.emplace_back("  Input Ports: []");
    } else {
      lines.emplace_back("  Input Ports:");
      for (const auto& port : input_ports) {
        lines.emplace_back("    - id: " + port.id);
        lines.emplace_back("      name: " + port.name);
      }
    }
    return {lines};
  }
};

struct Group {
  explicit Group(std::string name): name_(std::move(name)) {}
  Group& With(std::vector<Conn> connections) {
    connections_ = std::move(connections);
    return *this;
  }
  Group& With(std::vector<Proc> processors) {
    processors_ = std::move(processors);
    return *this;
  }
  Group& With(std::vector<Group> subgroups) {
    subgroups_ = std::move(subgroups);
    return *this;
  }
  Group& With(std::vector<RPG> rpgs) {
    rpgs_ = std::move(rpgs);
    return *this;
  }
  Lines serialize(bool is_root = true) const {
    Lines body;
    if (processors_.empty()) {
      body.emplace_back("Processors: []");
    } else {
      body.emplace_back("Processors:");
      for (const auto& proc: processors_) {
        body.append(proc.serialize().indentAll());
      }
    }
    if (!connections_.empty()) {
      body.emplace_back("Connections:");
      for (const auto& conn: connections_) {
        body.append(conn.serialize().indentAll());
      }
    }
    if (rpgs_.empty()) {
      body.emplace_back("Remote Process Groups: []");
    } else {
      body.emplace_back("Remote Process Groups:");
      for (const auto& rpg: rpgs_) {
        body.append(rpg.serialize().indentAll());
      }
    }
    if (!subgroups_.empty()) {
      body.emplace_back("Process Groups:");
      for (const auto& subgroup: subgroups_) {
        body.append(subgroup.serialize(false).indentAll());
      }
    }
    Lines lines;
    if (is_root) {
      lines.emplace_back("Flow Controller:");
      lines.emplace_back("  name: " + name_);
      lines.append(std::move(body));
    } else {
      lines.emplace_back("- name: " + name_);
      lines.append(std::move(body).indentAll());
    }
    return lines;
  }

  std::string name_;
  std::vector<Conn> connections_;
  std::vector<Proc> processors_;
  std::vector<Group> subgroups_;
  std::vector<RPG> rpgs_;
};

struct TestAccessor {
  FIELD_ACCESSOR(processors_)
  FIELD_ACCESSOR(connections_)
  FIELD_ACCESSOR(child_process_groups_)
};

template<typename T, typename = void>
struct Resolve;

template<typename T>
struct Resolve<T, typename std::enable_if<!std::is_pointer<T>::value>::type> {
  static auto get(const T& item) -> decltype(item.get()) {
    return item.get();
  }
};

template<typename T>
struct Resolve<T, typename std::enable_if<std::is_pointer<T>::value>::type> {
  static auto get(const T& item) -> T {
    return item;
  }
};

template<typename T>
auto findByName(const std::set<T>& set, const std::string& name) -> decltype(Resolve<T>::get(std::declval<const T&>())) {
  auto it = std::find_if(set.begin(), set.end(), [&](const T& item) {
    return item->getName() == name;
  });
  if (it != set.end()) {
    return Resolve<T>::get(*it);
  }
  return nullptr;
}

void verifyProcessGroup(core::ProcessGroup& group, const Group& pattern) {
  // verify name
  REQUIRE(group.getName() == pattern.name_);
  // verify connections
  std::set<std::shared_ptr<minifi::Connection>>& connections = TestAccessor::get_connections_(group);
  REQUIRE(connections.size() == pattern.connections_.size());
  for (auto& expected : pattern.connections_) {
    auto conn = findByName(connections, expected.name);
    REQUIRE(conn);
    if (!expected.source.name) {
      REQUIRE(conn->getSource() == nullptr);
      REQUIRE(utils::verifyLogLinePresenceInPollTime(
          std::chrono::seconds{1},
          "Cannot find the source processor with id '" + expected.source.id
          + "' for the connection [name = '" + expected.name + "'"
      ));
    } else {
      REQUIRE(conn->getSource()->getName() == expected.source.name);
    }
    if (!expected.destination.name) {
      REQUIRE(conn->getDestination() == nullptr);
      REQUIRE(utils::verifyLogLinePresenceInPollTime(
          std::chrono::seconds{1},
          "Cannot find the destination processor with id '" + expected.destination.id
          + "' for the connection [name = '" + expected.name + "'"
      ));
    } else {
      REQUIRE(conn->getDestination()->getName() == expected.destination.name);
    }
  }

  // verify processors
  std::set<std::shared_ptr<core::Processor>>& processors = TestAccessor::get_processors_(group);
  REQUIRE(processors.size() == pattern.processors_.size());
  for (auto& expected : pattern.processors_) {
    REQUIRE(findByName(processors, expected.name));
  }

  std::set<core::ProcessGroup*> simple_subgroups;
  std::set<core::ProcessGroup*> rpg_subgroups;
  for (auto& subgroup : TestAccessor::get_child_process_groups_(group)) {
    if (subgroup->isRemoteProcessGroup()) {
      rpg_subgroups.insert(subgroup.get());
    } else {
      simple_subgroups.insert(subgroup.get());
    }
  }
  // verify remote process groups
  REQUIRE(rpg_subgroups.size() == pattern.rpgs_.size());
  for (auto& expected : pattern.rpgs_) {
    auto rpg = findByName(rpg_subgroups, expected.name);
    REQUIRE(rpg);
    std::set<std::shared_ptr<core::Processor>>& input_ports = TestAccessor::get_processors_(*rpg);
    REQUIRE(input_ports.size() == expected.input_ports.size());
    for (auto& expected_input_port : expected.input_ports) {
      auto input_port = dynamic_cast<minifi::RemoteProcessorGroupPort*>(findByName(input_ports, expected_input_port.name));
      REQUIRE(input_port);
      REQUIRE(input_port->getName() == expected_input_port.name);
    }
  }

  // verify subgroups
  REQUIRE(simple_subgroups.size() == pattern.subgroups_.size());
  for (auto& expected : pattern.subgroups_) {
    auto subgroup = findByName(simple_subgroups, expected.name_);
    REQUIRE(subgroup);
    verifyProcessGroup(*subgroup, expected);
  }
}
