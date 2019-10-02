
/**
 * @file ConsumeWindowsEventLog.h
 * ConsumeWindowsEventLog class declaration
 *
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

#include "core/Core.h"
#include "FlowFileRecord.h"
#include "concurrentqueue.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include <pugixml.hpp>
#include <winevt.h>
#include <sstream>
#include <regex>
#include <codecvt>
#include "utils/OsUtils.h"



namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace wel {

  class XmlString : public pugi::xml_writer
  {
  public:
    std::string xml_;

    virtual void write(const void* data, size_t size)
    {
      xml_.append(static_cast<const char*>(data), size);
    }
  };

} /* namespace wel */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

