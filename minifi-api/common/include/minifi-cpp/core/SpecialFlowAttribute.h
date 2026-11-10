// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

namespace org::apache::nifi::minifi::core {

struct SpecialFlowAttribute {
  static constexpr std::string_view PATH = "path";
  static constexpr std::string_view ABSOLUTE_PATH = "absolute.path";
  static constexpr std::string_view FILENAME = "filename";
  static constexpr std::string_view UUID = "uuid";
  static constexpr std::string_view priority = "priority";
  static constexpr std::string_view MIME_TYPE = "mime.type";
  static constexpr std::string_view DISCARD_REASON = "discard.reason";
  static constexpr std::string_view ALTERNATE_IDENTIFIER = "alternate.identifier";
  static constexpr std::string_view FLOW_ID = "flow.id";

  static constexpr std::array<std::string_view, 9> getSpecialFlowAttributes() {
    return {PATH, ABSOLUTE_PATH, FILENAME, UUID, priority, MIME_TYPE, DISCARD_REASON, ALTERNATE_IDENTIFIER, FLOW_ID};
  }
};

}  // namespace org::apache::nifi::minifi::core
