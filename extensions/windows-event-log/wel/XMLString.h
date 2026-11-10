// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <Windows.h>
#include <winevt.h>

#include <sstream>
#include <string>
#include <regex>

#include "core/Core.h"
#include "core/ProcessorImpl.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/FlowFileRecord.h"
#include "utils/OsUtils.h"

#include "moodycamel/concurrentqueue.h"
#include "pugixml.hpp"


namespace org::apache::nifi::minifi::wel {

class XmlString : public pugi::xml_writer {
 public:
  std::string xml_;

  virtual void write(const void* data, size_t size) {
    xml_.append(static_cast<const char*>(data), size);
  }
};

}  // namespace org::apache::nifi::minifi::wel
