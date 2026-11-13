// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>

#include "minifi-cpp/core/FlowFile.h"

namespace org::apache::nifi::minifi::extensions::lua {


class LuaScriptFlowFile {
 public:
  explicit LuaScriptFlowFile(std::shared_ptr<core::FlowFile> flow_file);
  std::string getAttribute(const std::string &key);
  void setAttribute(const std::string &key, const std::string &value);
  bool addAttribute(const std::string &key, const std::string &value);
  bool updateAttribute(std::string_view key, const std::string& value);
  bool removeAttribute(std::string_view key);
  std::shared_ptr<core::FlowFile> getFlowFile();
  void releaseFlowFile();

 private:
  std::shared_ptr<core::FlowFile> flow_file_;
};

}  // namespace org::apache::nifi::minifi::extensions::lua
