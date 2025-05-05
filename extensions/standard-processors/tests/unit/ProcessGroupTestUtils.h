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

#include <utility>
#include <string>
#include <set>
#include <memory>
#include <vector>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"

struct Lines {
  std::vector<std::string> lines;

  std::string join(const std::string& delim) const {
    return utils::string::join(delim, lines);
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

enum class ConnectionFailure {
  UNRESOLVED_SOURCE,
  UNRESOLVED_DESTINATION,
  INPUT_CANNOT_BE_SOURCE,
  OUTPUT_CANNOT_BE_DESTINATION,
  INPUT_CANNOT_BE_DESTINATION,
  OUTPUT_CANNOT_BE_SOURCE
};

struct Proc {
  Proc(std::string id, std::string name, const std::optional<ConnectionFailure>& failure = std::nullopt)
    : id(std::move(id)), name(std::move(name)), failure(failure) {}
  std::string id;
  std::string name;
  std::optional<ConnectionFailure> failure;

  Lines serialize() const {
    return {{
      "- id: " + id,
      "  name: " + name,
      "  class: LogOnDestructionProcessor"
    }};
  }
};

template<typename Tag>
struct Port {
  Port(std::string id, std::string name, const std::optional<ConnectionFailure>& failure = std::nullopt)
    : id(std::move(id)), name(std::move(name)), failure(failure) {}
  std::string id;
  std::string name;
  std::optional<ConnectionFailure> failure;

  Lines serialize() const {
    return {{
      "- id: " + id,
      "  name: " + name
    }};
  }
};

using InputPort = Port<struct InputTag>;
using OutputPort = Port<struct OutputTag>;

struct MaybeProc {
  MaybeProc(const Proc& proc) : id(proc.id), name(proc.name), failure(proc.failure) {}  // NOLINT(runtime/explicit)
  MaybeProc(const InputPort& port) : id(port.id), name(port.name), failure(port.failure) {}  // NOLINT(runtime/explicit)
  MaybeProc(const OutputPort& port) : id(port.id), name(port.name), failure(port.failure) {}  // NOLINT(runtime/explicit)

  std::string id;
  std::string name;
  std::optional<ConnectionFailure> failure;
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
  Group& With(std::vector<InputPort> input_ports) {
    input_ports_ = std::move(input_ports);
    return *this;
  }
  Group& With(std::vector<OutputPort> output_ports) {
    output_ports_ = std::move(output_ports);
    return *this;
  }
  Lines serialize(bool is_root = true) const {
    Lines body;
    if (processors_.empty()) {
      body.emplace_back("Processors: []");
    } else {
      body.emplace_back("Processors:");
      for (const auto& proc : processors_) {
        body.append(proc.serialize().indentAll());
      }
    }
    if (!connections_.empty()) {
      body.emplace_back("Connections:");
      for (const auto& conn : connections_) {
        body.append(conn.serialize().indentAll());
      }
    }
    if (rpgs_.empty()) {
      body.emplace_back("Remote Process Groups: []");
    } else {
      body.emplace_back("Remote Process Groups:");
      for (const auto& rpg : rpgs_) {
        body.append(rpg.serialize().indentAll());
      }
    }
    if (!subgroups_.empty()) {
      body.emplace_back("Process Groups:");
      for (const auto& subgroup : subgroups_) {
        body.append(subgroup.serialize(false).indentAll());
      }
    }
    if (input_ports_.empty()) {
      body.emplace_back("Input Ports: []");
    } else {
      body.emplace_back("Input Ports:");
      for (const auto& port : input_ports_) {
        body.append(port.serialize().indentAll());
      }
    }
    if (output_ports_.empty()) {
      body.emplace_back("Output Ports: []");
    } else {
      body.emplace_back("Output Ports:");
      for (const auto& port : output_ports_) {
        body.append(port.serialize().indentAll());
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
  std::vector<InputPort> input_ports_;
  std::vector<OutputPort> output_ports_;
};

struct ProcessGroupTestAccessor {
  FIELD_ACCESSOR(processors_)
  FIELD_ACCESSOR(connections_)
  FIELD_ACCESSOR(child_process_groups_)
  FIELD_ACCESSOR(ports_)
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

void assertFailure(const Conn& expected, ConnectionFailure failure) {
  auto assertMessage = [](const std::string& message) {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds{1}, message));
  };

  switch (failure) {
    case ConnectionFailure::UNRESOLVED_DESTINATION: {
      assertMessage("Cannot find the destination processor with id '" + expected.destination.id + "' for the connection [name = '" + expected.name + "'");
      break;
    }
    case ConnectionFailure::UNRESOLVED_SOURCE: {
      assertMessage("Cannot find the source processor with id '" + expected.source.id + "' for the connection [name = '" + expected.name + "'");
      break;
    }
    case ConnectionFailure::INPUT_CANNOT_BE_SOURCE: {
      assertMessage("Input port [id = '" + expected.source.id + "'] cannot be a source outside the process group in the connection [name = '" + expected.name + "'");
      break;
    }
    case ConnectionFailure::OUTPUT_CANNOT_BE_DESTINATION: {
      assertMessage("Output port [id = '" + expected.destination.id + "'] cannot be a destination outside the process group in the connection [name = '" + expected.name + "'");
      break;
    }
    case ConnectionFailure::INPUT_CANNOT_BE_DESTINATION: {
      assertMessage("Input port [id = '" + expected.destination.id + "'] cannot be a destination inside the process group in the connection [name = '" + expected.name + "'");
      break;
    }
    case ConnectionFailure::OUTPUT_CANNOT_BE_SOURCE: {
      assertMessage("Output port [id = '" + expected.source.id + "'] cannot be a source inside the process group in the connection [name = '" + expected.name + "'");
      break;
    }
  }
}

void verifyConnectionNode(minifi::Connection* conn, const Conn& expected) {
  if (expected.source.failure) {
    REQUIRE(conn->getSource() == nullptr);
    assertFailure(expected, *expected.source.failure);
  } else {
    REQUIRE(conn->getSource()->getName() == expected.source.name);
  }
  if (expected.destination.failure) {
    REQUIRE(conn->getDestination() == nullptr);
    assertFailure(expected, *expected.destination.failure);
  } else {
    REQUIRE(conn->getDestination()->getName() == expected.destination.name);
  }
}

void verifyProcessGroup(core::ProcessGroup& group, const Group& pattern) {
  // verify name
  REQUIRE(group.getName() == pattern.name_);
  // verify connections
  const auto& connections = ProcessGroupTestAccessor::get_connections_(group);
  REQUIRE(connections.size() == pattern.connections_.size());
  for (auto& expected : pattern.connections_) {
    auto conn = findByName(connections, expected.name);
    REQUIRE(conn);
    verifyConnectionNode(conn, expected);
  }

  // verify processors and ports
  const auto& processors = ProcessGroupTestAccessor::get_processors_(group);
  REQUIRE(processors.size() == pattern.processors_.size() + pattern.input_ports_.size() + pattern.output_ports_.size());
  for (auto& expected : pattern.processors_) {
    REQUIRE(findByName(processors, expected.name));
  }

  for (auto& expected : pattern.input_ports_) {
    REQUIRE(findByName(processors, expected.name));
  }

  for (auto& expected : pattern.output_ports_) {
    REQUIRE(findByName(processors, expected.name));
  }

  const auto& ports = ProcessGroupTestAccessor::get_ports_(group);
  REQUIRE(ports.size() == pattern.input_ports_.size() + pattern.output_ports_.size());
  for (auto& expected : pattern.input_ports_) {
    REQUIRE(findByName(ports, expected.name));
  }

  for (auto& expected : pattern.output_ports_) {
    REQUIRE(findByName(ports, expected.name));
  }

  std::set<core::ProcessGroup*> simple_subgroups;
  std::set<core::ProcessGroup*> rpg_subgroups;
  for (auto& subgroup : ProcessGroupTestAccessor::get_child_process_groups_(group)) {
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
    const auto& input_ports = ProcessGroupTestAccessor::get_processors_(*rpg);
    REQUIRE(input_ports.size() == expected.input_ports.size());
    for (auto& expected_input_port : expected.input_ports) {
      auto input_port = dynamic_cast<minifi::RemoteProcessGroupPort*>(findByName(input_ports, expected_input_port.name));
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
